#pragma once

#include <NGIN/ECS/Export.hpp>
#include <NGIN/ECS/Entity.hpp>
#include <NGIN/ECS/Archetype.hpp>
#include <NGIN/ECS/TypeRegistry.hpp>
#include <NGIN/Containers/HashMap.hpp>
#include <NGIN/Containers/Vector.hpp>

#include <stdexcept>
#include <type_traits>
#include <utility>

namespace NGIN::ECS
{
    class NGIN_ECS_API World
    {
    public:
        World() = default;
        World(const World&)            = delete;
        World& operator=(const World&) = delete;
        World(World&&)                 = delete;
        World& operator=(World&&)      = delete;

        [[nodiscard]] NGIN::UInt64 CurrentEpoch() const noexcept { return m_currentEpoch; }
        [[nodiscard]] NGIN::UInt64 PreviousEpoch() const noexcept { return m_previousEpoch; }

        void NextEpoch() noexcept
        {
            m_previousEpoch = m_currentEpoch;
            ++m_currentEpoch;
        }

        [[nodiscard]] EntityId Spawn()
        {
            NGIN::Containers::Vector<ComponentPayload> payloads;
            return SpawnWithPayloads(payloads);
        }

        template<typename... Cs>
        [[nodiscard]] EntityId Spawn(Cs&&... components)
        {
            NGIN::Containers::Vector<ComponentPayload> payloads;
            payloads.Reserve(sizeof...(Cs));
            (payloads.EmplaceBack(CapturePayload(std::forward<Cs>(components))), ...);
            return SpawnWithPayloads(payloads);
        }

        void Despawn(EntityId entityId)
        {
            if (!IsAlive(entityId))
            {
                return;
            }

            const auto entityIndex = GetEntityIndex(entityId);
            auto&      slot        = m_slots[entityIndex];
            auto       location    = slot.Location;
            auto*      archetype   = m_archetypes[location.ArchetypeIndex].Get();
            if (archetype)
            {
                archetype->RemoveRow(location.ChunkIndex, location.RowIndex, [&](EntityId movedEntity,
                                                                                 NGIN::UIntSize chunkIndex,
                                                                                 NGIN::UIntSize rowIndex) {
                    auto& movedSlot             = m_slots[GetEntityIndex(movedEntity)];
                    movedSlot.Location.ChunkIndex = chunkIndex;
                    movedSlot.Location.RowIndex   = rowIndex;
                });
            }

            m_entities.Destroy(entityId);
            slot.Generation = m_entities.GenerationAtIndex(entityIndex);
            slot.Alive      = false;
            slot.Location   = {};
        }

        [[nodiscard]] bool IsAlive(EntityId entityId) const noexcept
        {
            return m_entities.IsAlive(entityId);
        }

        [[nodiscard]] NGIN::UInt64 AliveCount() const noexcept
        {
            return m_entities.AliveCount();
        }

        void Clear()
        {
            m_archetypes.Clear();
            m_archIndex.Clear();
            m_entities.Clear();
            m_slots.Clear();
        }

        template<typename T>
        [[nodiscard]] bool Has(EntityId entityId) const noexcept
        {
            if (!IsAlive(entityId))
            {
                return false;
            }

            const auto& slot = m_slots[GetEntityIndex(entityId)];
            if (!slot.Location.IsValid())
            {
                return false;
            }

            const auto* archetype = m_archetypes[slot.Location.ArchetypeIndex].Get();
            return archetype->HasComponent(GetTypeId<T>());
        }

        template<typename T>
        [[nodiscard]] const T* TryGet(EntityId entityId) const noexcept
        {
            if (!IsAlive(entityId))
            {
                return nullptr;
            }

            const auto& slot = m_slots[GetEntityIndex(entityId)];
            if (!slot.Location.IsValid())
            {
                return nullptr;
            }

            const auto* archetype = m_archetypes[slot.Location.ArchetypeIndex].Get();
            const auto  column    = archetype->FindColumnIndex(GetTypeId<T>());
            if (column == kInvalidIndex)
            {
                return nullptr;
            }

            if constexpr (std::is_empty_v<T>)
            {
                static T instance {};
                return &instance;
            }

            const auto* chunk = archetype->GetChunk(slot.Location.ChunkIndex);
            return static_cast<const T*>(chunk->ComponentPtr(column, slot.Location.RowIndex));
        }

        template<typename T>
        [[nodiscard]] T* TryGetMut(EntityId entityId) noexcept
        {
            if (!IsAlive(entityId))
            {
                return nullptr;
            }

            const auto& slot = m_slots[GetEntityIndex(entityId)];
            if (!slot.Location.IsValid())
            {
                return nullptr;
            }

            auto* archetype = m_archetypes[slot.Location.ArchetypeIndex].Get();
            const auto column = archetype->FindColumnIndex(GetTypeId<T>());
            if (column == kInvalidIndex)
            {
                return nullptr;
            }

            if constexpr (std::is_empty_v<T>)
            {
                static T instance {};
                return &instance;
            }

            auto* chunk = archetype->GetChunk(slot.Location.ChunkIndex);
            return static_cast<T*>(chunk->ComponentPtr(column, slot.Location.RowIndex));
        }

        template<typename T>
        [[nodiscard]] const T& Get(EntityId entityId) const
        {
            if (const auto* component = TryGet<T>(entityId))
            {
                return *component;
            }
            throw std::out_of_range("Component is not present on entity.");
        }

        template<typename T>
        [[nodiscard]] T& GetMut(EntityId entityId)
        {
            if (auto* component = TryGetMut<T>(entityId))
            {
                return *component;
            }
            throw std::out_of_range("Component is not present on entity.");
        }

        template<typename T, typename U>
        void Add(EntityId entityId, U&& value)
        {
            ValidateAlive(entityId);
            if (Has<T>(entityId))
            {
                throw std::invalid_argument("Component already exists on entity.");
            }

            NGIN::Containers::Vector<ComponentPayload> payloads;
            payloads.EmplaceBack(CaptureTypedPayload<T>(std::forward<U>(value)));
            MoveEntityToSignature(entityId, BuildSignatureWithAdded<T>(entityId), payloads);
        }

        template<typename T>
        [[nodiscard]] bool Remove(EntityId entityId)
        {
            ValidateAlive(entityId);
            if (!Has<T>(entityId))
            {
                return false;
            }

            MoveEntityToSignature(entityId, BuildSignatureWithRemoved<T>(entityId), {});
            return true;
        }

        template<typename T, typename U>
        void Set(EntityId entityId, U&& value)
        {
            ValidateAlive(entityId);
            auto* component = TryGetMut<T>(entityId);
            if (!component)
            {
                throw std::out_of_range("Component is not present on entity.");
            }

            auto*       archetype = m_archetypes[m_slots[GetEntityIndex(entityId)].Location.ArchetypeIndex].Get();
            const auto  column    = archetype->ColumnIndexOf(GetTypeId<T>());
            auto*       chunk     = archetype->GetChunk(m_slots[GetEntityIndex(entityId)].Location.ChunkIndex);
            const auto  row       = m_slots[GetEntityIndex(entityId)].Location.RowIndex;
            const auto& info      = archetype->ComponentAt(column);

            if (!info.IsEmpty)
            {
                info.Destroy(component);
                ConstructFromPayload(info,
                                     chunk->ComponentPtr(column, row),
                                     CaptureTypedPayload<T>(std::forward<U>(value)));
            }
            chunk->SetChangedTick(column, row, m_currentEpoch);
        }

        template<typename T>
        void MarkChanged(EntityId entityId)
        {
            ValidateAlive(entityId);
            auto* archetype = m_archetypes[m_slots[GetEntityIndex(entityId)].Location.ArchetypeIndex].Get();
            const auto column = archetype->ColumnIndexOf(GetTypeId<T>());
            auto* chunk = archetype->GetChunk(m_slots[GetEntityIndex(entityId)].Location.ChunkIndex);
            chunk->SetChangedTick(column, m_slots[GetEntityIndex(entityId)].Location.RowIndex, m_currentEpoch);
        }

        [[nodiscard]] const NGIN::Containers::Vector<NGIN::Memory::Scoped<Archetype>>& Archetypes() const noexcept
        {
            return m_archetypes;
        }

        template<typename... Cs>
        [[nodiscard]] NGIN::UIntSize DebugGetChunkCount() const
        {
            auto signature = BuildSignature<Cs...>();
            const auto* index = m_archIndex.GetPtr(signature);
            if (!index)
            {
                return 0;
            }
            return m_archetypes[*index]->ChunkCount();
        }

        template<typename... Cs>
        [[nodiscard]] NGIN::UIntSize DebugGetChunkRowCapacity() const
        {
            auto signature = BuildSignature<Cs...>();
            const auto* index = m_archIndex.GetPtr(signature);
            if (!index)
            {
                return 0;
            }
            return m_archetypes[*index]->ComputeCapacityForChunkBytes(kDefaultChunkBytes);
        }

    private:
        struct EntityLocation
        {
            NGIN::UIntSize ArchetypeIndex {kInvalidIndex};
            NGIN::UIntSize ChunkIndex {kInvalidIndex};
            NGIN::UIntSize RowIndex {kInvalidIndex};

            [[nodiscard]] bool IsValid() const noexcept
            {
                return ArchetypeIndex != kInvalidIndex && ChunkIndex != kInvalidIndex && RowIndex != kInvalidIndex;
            }
        };

        struct EntitySlot
        {
            NGIN::UInt16   Generation {0};
            bool           Alive {false};
            EntityLocation Location {};
        };

        template<typename T>
        const ComponentInfo& RegisterComponent()
        {
            const auto typeId = GetTypeId<T>();
            if (const auto* existing = m_componentRegistry.GetPtr(typeId))
            {
                return *existing;
            }
            m_componentRegistry.Insert(typeId, DescribeComponent<T>());
            return *m_componentRegistry.GetPtr(typeId);
        }

        const ComponentInfo& RequireComponentInfo(TypeId typeId) const
        {
            const auto* info = m_componentRegistry.GetPtr(typeId);
            if (!info)
            {
                throw std::out_of_range("Component type is not registered in this world.");
            }
            return *info;
        }

        template<typename T>
        [[nodiscard]] ComponentPayload CapturePayload(T&& value)
        {
            using Component = std::remove_cvref_t<T>;
            const auto& info = RegisterComponent<Component>();
            return ComponentPayload {
                .id = info.id,
                .Info = info,
                .Data = std::addressof(value),
                .MoveConstruct = std::is_rvalue_reference_v<T&&>,
            };
        }

        template<typename T, typename U>
        [[nodiscard]] ComponentPayload CaptureTypedPayload(U&& value)
        {
            using Component = std::remove_cvref_t<T>;
            static_assert(std::is_same_v<std::remove_cvref_t<U>, Component>,
                          "World::Add/Set currently require the value type to match the component type exactly.");
            return CapturePayload(std::forward<U>(value));
        }

        template<typename... Cs>
        static ArchetypeSignature BuildSignature()
        {
            NGIN::Containers::Vector<TypeId> types;
            types.Reserve(sizeof...(Cs));
            (types.EmplaceBack(GetTypeId<Cs>()), ...);
            return ArchetypeSignature::FromUnordered(std::move(types));
        }

        [[nodiscard]] EntityId SpawnWithPayloads(const NGIN::Containers::Vector<ComponentPayload>& payloads)
        {
            const auto entityId = m_entities.Create();
            EnsureEntitySlot(entityId);

            const auto archetypeIndex = GetOrCreateArchetypeIndex(BuildSignatureFromPayloads(payloads));
            const auto rowAddress     = InsertEntityIntoArchetype(entityId, archetypeIndex, payloads);

            auto& slot              = m_slots[GetEntityIndex(entityId)];
            slot.Generation         = GetEntityGeneration(entityId);
            slot.Alive              = true;
            slot.Location.ArchetypeIndex = archetypeIndex;
            slot.Location.ChunkIndex     = rowAddress.ChunkIndex;
            slot.Location.RowIndex       = rowAddress.RowIndex;
            return entityId;
        }

        [[nodiscard]] ArchetypeSignature BuildSignatureFromPayloads(const NGIN::Containers::Vector<ComponentPayload>& payloads)
        {
            NGIN::Containers::Vector<TypeId> types;
            types.Reserve(payloads.Size());
            for (NGIN::UIntSize index = 0; index < payloads.Size(); ++index)
            {
                types.EmplaceBack(payloads[index].id);
            }
            return ArchetypeSignature::FromUnordered(std::move(types));
        }

        void EnsureEntitySlot(EntityId entityId)
        {
            const auto entityIndex = GetEntityIndex(entityId);
            while (m_slots.Size() <= entityIndex)
            {
                m_slots.EmplaceBack();
            }
        }

        void ValidateAlive(EntityId entityId) const
        {
            if (!IsAlive(entityId))
            {
                throw std::out_of_range("Entity is stale or not alive.");
            }
        }

        [[nodiscard]] NGIN::UIntSize GetOrCreateArchetypeIndex(const ArchetypeSignature& signature)
        {
            if (const auto* existing = m_archIndex.GetPtr(signature))
            {
                return *existing;
            }

            NGIN::Containers::Vector<ComponentInfo> components;
            components.Reserve(signature.Types.Size());
            for (NGIN::UIntSize index = 0; index < signature.Types.Size(); ++index)
            {
                components.EmplaceBack(RequireComponentInfo(signature.Types[index]));
            }

            const auto archetypeIndex = m_archetypes.Size();
            m_archetypes.EmplaceBack(NGIN::Memory::MakeScoped<Archetype>(signature, std::move(components)));
            m_archIndex.Insert(signature, archetypeIndex);
            return archetypeIndex;
        }

        [[nodiscard]] const ComponentPayload* FindPayload(const NGIN::Containers::Vector<ComponentPayload>& payloads, TypeId typeId) const noexcept
        {
            for (NGIN::UIntSize index = 0; index < payloads.Size(); ++index)
            {
                if (payloads[index].id == typeId)
                {
                    return &payloads[index];
                }
            }
            return nullptr;
        }

        void ConstructFromPayload(const ComponentInfo& info, void* destination, const ComponentPayload& payload)
        {
            if (info.IsEmpty)
            {
                return;
            }

            if (payload.MoveConstruct)
            {
                if (!info.MoveConstruct)
                {
                    throw std::invalid_argument("Component is not move-constructible.");
                }
                info.MoveConstruct(destination, const_cast<void*>(payload.Data));
            }
            else
            {
                if (!info.CopyConstruct)
                {
                    throw std::invalid_argument("Component is not copy-constructible.");
                }
                info.CopyConstruct(destination, payload.Data);
            }
        }

        [[nodiscard]] ArchetypeRowAddress InsertEntityIntoArchetype(EntityId entityId,
                                                                    NGIN::UIntSize archetypeIndex,
                                                                    const NGIN::Containers::Vector<ComponentPayload>& payloads)
        {
            auto* archetype = m_archetypes[archetypeIndex].Get();
            return archetype->EmplaceRow(entityId,
                                         [&](Chunk& chunk,
                                             NGIN::UIntSize,
                                             NGIN::UIntSize row,
                                             NGIN::UIntSize columnIndex,
                                             const ComponentInfo& info) {
                const auto* payload = FindPayload(payloads, info.id);
                if (info.IsEmpty)
                {
                    chunk.SetAddedTick(columnIndex, row, m_currentEpoch);
                    chunk.SetChangedTick(columnIndex, row, 0);
                    return;
                }
                if (!payload)
                {
                    throw std::invalid_argument("Missing payload for archetype column.");
                }
                ConstructFromPayload(info, chunk.ComponentPtr(columnIndex, row), *payload);
                chunk.SetAddedTick(columnIndex, row, m_currentEpoch);
                chunk.SetChangedTick(columnIndex, row, 0);
            });
        }

        template<typename T>
        [[nodiscard]] ArchetypeSignature BuildSignatureWithAdded(EntityId entityId)
        {
            RegisterComponent<T>();
            auto types = m_archetypes[m_slots[GetEntityIndex(entityId)].Location.ArchetypeIndex]->Signature().Types;
            types.EmplaceBack(GetTypeId<T>());
            return ArchetypeSignature::FromUnordered(std::move(types));
        }

        template<typename T>
        [[nodiscard]] ArchetypeSignature BuildSignatureWithRemoved(EntityId entityId)
        {
            auto types = m_archetypes[m_slots[GetEntityIndex(entityId)].Location.ArchetypeIndex]->Signature().Types;
            const auto typeId = GetTypeId<T>();
            NGIN::Containers::Vector<TypeId> filtered;
            filtered.Reserve(types.Size());
            for (NGIN::UIntSize index = 0; index < types.Size(); ++index)
            {
                if (types[index] != typeId)
                {
                    filtered.EmplaceBack(types[index]);
                }
            }
            return ArchetypeSignature::FromUnordered(std::move(filtered));
        }

        void MoveEntityToSignature(EntityId entityId,
                                   const ArchetypeSignature& destinationSignature,
                                   const NGIN::Containers::Vector<ComponentPayload>& payloads)
        {
            const auto entityIndex        = GetEntityIndex(entityId);
            const auto sourceLocation     = m_slots[entityIndex].Location;
            const auto destinationIndex   = GetOrCreateArchetypeIndex(destinationSignature);
            auto*      sourceArchetype    = m_archetypes[sourceLocation.ArchetypeIndex].Get();
            auto*      destinationArchetype = m_archetypes[destinationIndex].Get();
            auto*      sourceChunk        = sourceArchetype->GetChunk(sourceLocation.ChunkIndex);

            const auto destinationAddress = destinationArchetype->EmplaceRow(
                entityId,
                [&](Chunk& destinationChunk,
                    NGIN::UIntSize,
                    NGIN::UIntSize destinationRow,
                    NGIN::UIntSize destinationColumn,
                    const ComponentInfo& destinationInfo) {
                    const auto sourceColumn = sourceArchetype->FindColumnIndex(destinationInfo.id);
                    if (sourceColumn != kInvalidIndex)
                    {
                        if (!destinationInfo.IsEmpty)
                        {
                            auto* destinationPtr = destinationChunk.ComponentPtr(destinationColumn, destinationRow);
                            auto* sourcePtr      = const_cast<void*>(sourceChunk->ComponentPtr(sourceColumn, sourceLocation.RowIndex));
                            if (destinationInfo.MoveConstruct)
                            {
                                destinationInfo.MoveConstruct(destinationPtr, sourcePtr);
                            }
                            else if (destinationInfo.CopyConstruct)
                            {
                                destinationInfo.CopyConstruct(destinationPtr, sourcePtr);
                            }
                            else
                            {
                                throw std::runtime_error("Component cannot be migrated.");
                            }
                        }

                        destinationChunk.SetAddedTick(destinationColumn,
                                                      destinationRow,
                                                      sourceChunk->AddedTick(sourceColumn, sourceLocation.RowIndex));
                        destinationChunk.SetChangedTick(destinationColumn,
                                                        destinationRow,
                                                        sourceChunk->ChangedTick(sourceColumn, sourceLocation.RowIndex));
                        return;
                    }

                    const auto* payload = FindPayload(payloads, destinationInfo.id);
                    if (!payload)
                    {
                        throw std::invalid_argument("Missing payload for migrated component.");
                    }
                    ConstructFromPayload(destinationInfo,
                                         destinationChunk.ComponentPtr(destinationColumn, destinationRow),
                                         *payload);
                    destinationChunk.SetAddedTick(destinationColumn, destinationRow, m_currentEpoch);
                    destinationChunk.SetChangedTick(destinationColumn, destinationRow, 0);
                }
            );

            m_slots[entityIndex].Location.ArchetypeIndex = destinationIndex;
            m_slots[entityIndex].Location.ChunkIndex     = destinationAddress.ChunkIndex;
            m_slots[entityIndex].Location.RowIndex       = destinationAddress.RowIndex;

            sourceArchetype->RemoveRow(sourceLocation.ChunkIndex,
                                       sourceLocation.RowIndex,
                                       [&](EntityId movedEntity, NGIN::UIntSize chunkIndex, NGIN::UIntSize rowIndex) {
                auto& movedSlot = m_slots[GetEntityIndex(movedEntity)];
                if (movedEntity == entityId)
                {
                    return;
                }
                movedSlot.Location.ChunkIndex = chunkIndex;
                movedSlot.Location.RowIndex   = rowIndex;
            });
        }

    private:
        EntityAllocator                                              m_entities;
        NGIN::Containers::Vector<EntitySlot>                         m_slots;
        NGIN::Containers::Vector<NGIN::Memory::Scoped<Archetype>>    m_archetypes;
        NGIN::Containers::FlatHashMap<ArchetypeSignature, UIntSize>  m_archIndex;
        NGIN::Containers::FlatHashMap<TypeId, ComponentInfo>         m_componentRegistry;
        NGIN::UInt64                                                 m_currentEpoch {1};
        NGIN::UInt64                                                 m_previousEpoch {0};
    };
}// namespace NGIN::ECS
