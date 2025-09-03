#pragma once

#include <NGIN/ECS/Export.hpp>
#include <NGIN/ECS/Entity.hpp>
#include <NGIN/ECS/Archetype.hpp>
#include <NGIN/ECS/TypeRegistry.hpp>
#include <NGIN/Containers/HashMap.hpp>

namespace NGIN::ECS
{
    /// @brief World MVP: entity lifecycle + typed spawn into SoA archetypes.
    class NGIN_ECS_API World
    {
    public:
        World() = default;
        [[nodiscard]] NGIN::UInt64 CurrentEpoch() const noexcept { return m_epoch; }
        void                       NextEpoch() noexcept { ++m_epoch; }

        [[nodiscard]] EntityId Spawn()
        {
            return m_entities.Create();
        }

        template<typename... Cs>
        [[nodiscard]] EntityId Spawn(const Cs&... comps)
        {
            auto id = m_entities.Create();
            auto* a = GetOrCreateArchetypeFor<Cs...>();
            a->Insert(id, m_epoch, comps...);
            return id;
        }

        void Despawn(EntityId id)
        {
            // MVP: only retire entity ID; structural removal handled later by command buffers
            m_entities.Destroy(id);
        }

        [[nodiscard]] bool IsAlive(EntityId id) const noexcept
        {
            return m_entities.IsAlive(id);
        }

        [[nodiscard]] NGIN::UInt64 AliveCount() const noexcept
        {
            return m_entities.AliveCount();
        }

        void Clear()
        {
            // TODO: free archetype/chunk memory as well in later pass
            m_entities.Clear();
        }

        // Internal access for queries (read-only)
        [[nodiscard]] const NGIN::Containers::Vector<Archetype*>& Archetypes() const noexcept
        {
            return m_archetypes;
        }

        // --- Debug/testing helpers ---
        template<typename... Cs>
        [[nodiscard]] NGIN::UIntSize DebugGetChunkCount() const
        {
            ArchetypeSignature sig = BuildSignature<Cs...>();
            if (!m_archIndex.Contains(sig)) return 0;
            const auto idx = m_archIndex[sig];
            return m_archetypes[idx]->ChunkCount();
        }
        template<typename... Cs>
        [[nodiscard]] NGIN::UIntSize DebugGetChunkRowCapacity() const
        {
            ArchetypeSignature sig = BuildSignature<Cs...>();
            if (!m_archIndex.Contains(sig)) return 0;
            const auto idx = m_archIndex[sig];
            // Use formula to get capacity for default chunk size
            return m_archetypes[idx]->ComputeCapacityForChunkBytes(kDefaultChunkBytes);
        }

    private:
        template<typename... Cs>
        static ArchetypeSignature BuildSignature()
        {
            NGIN::Containers::Vector<TypeId> types;
            types.Reserve(sizeof...(Cs));
            (types.EmplaceBack(GetTypeId<Cs>()), ...);
            return ArchetypeSignature::FromUnordered(std::move(types));
        }

        Archetype* GetOrCreateArchetypeForTypes(const NGIN::Containers::Vector<TypeId>& typeList,
                                                const NGIN::Containers::Vector<ComponentInfo>& infos)
        {
            auto sig = ArchetypeSignature::FromUnordered(typeList);
            if (m_archIndex.Contains(sig))
            {
                auto idx = m_archIndex[sig];
                return m_archetypes[idx];
            }
            auto* a       = new Archetype(sig, infos);
            const auto ix = m_archetypes.Size();
            m_archetypes.EmplaceBack(a);
            m_archIndex.Insert(a->Signature(), ix);
            return a;
        }

        template<typename... Cs>
        Archetype* GetOrCreateArchetypeFor()
        {
            auto sig = BuildSignature<Cs...>();
            if (m_archIndex.Contains(sig))
            {
                auto idx = m_archIndex[sig];
                return m_archetypes[idx];
            }
            // Build ComponentInfo list in canonical order (sig.Types)
            NGIN::Containers::Vector<ComponentInfo> comps;
            comps.Reserve(sig.Types.Size());
            for (NGIN::UIntSize i = 0; i < sig.Types.Size(); ++i)
            {
                // Reconstruct ComponentInfo via DescribeComponent<T> by matching TypeId.
                // As we only support types in Cs... for MVP, we can map from pack.
                comps.EmplaceBack(DescribeById<Cs...>(sig.Types[i]));
            }
            auto* a = new Archetype(sig, std::move(comps));
            const auto index = m_archetypes.Size();
            m_archetypes.EmplaceBack(a);
            m_archIndex.Insert(a->Signature(), index);
            return a;
        }

        template<typename T>
        static ComponentInfo DescribeByIdImpl(TypeId id)
        {
            if (GetTypeId<T>() == id)
                return DescribeComponent<T>();
            return ComponentInfo { .id = id, .Size = 0, .Align = 1, .IsPOD = true, .IsEmpty = true };
        }
        template<typename T0, typename T1, typename... Rest>
        static ComponentInfo DescribeByIdImpl(TypeId id)
        {
            if (GetTypeId<T0>() == id)
                return DescribeComponent<T0>();
            return DescribeByIdImpl<T1, Rest...>(id);
        }
        template<typename... Cs>
        static ComponentInfo DescribeById(TypeId id)
        {
            return DescribeByIdImpl<Cs...>(id);
        }

    private:
        EntityAllocator                                             m_entities;
        NGIN::Containers::Vector<Archetype*>                        m_archetypes;
        NGIN::Containers::FlatHashMap<ArchetypeSignature, UIntSize> m_archIndex;
        NGIN::UInt64                                                m_epoch {1};
    };
}// namespace NGIN::ECS
