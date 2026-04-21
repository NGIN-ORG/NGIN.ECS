#pragma once

#include <NGIN/Primitives.hpp>
#include <NGIN/Containers/Vector.hpp>
#include <NGIN/Hashing/FNV.hpp>
#include <NGIN/Memory/SystemAllocator.hpp>
#include <NGIN/Memory/SmartPointers.hpp>
#include <NGIN/ECS/Entity.hpp>
#include <NGIN/ECS/TypeRegistry.hpp>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace NGIN::ECS
{
    inline constexpr NGIN::UIntSize kDefaultChunkBytes = 64 * 1024;
    inline constexpr NGIN::UIntSize kInvalidIndex      = (std::numeric_limits<NGIN::UIntSize>::max)();

    struct ArchetypeSignature
    {
        NGIN::Containers::Vector<TypeId> Types;
        NGIN::UInt64                     Hash {0};

        [[nodiscard]] bool operator==(const ArchetypeSignature& other) const noexcept
        {
            if (Hash != other.Hash || Types.Size() != other.Types.Size())
            {
                return false;
            }
            for (NGIN::UIntSize i = 0; i < Types.Size(); ++i)
            {
                if (Types[i] != other.Types[i])
                {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] static ArchetypeSignature FromUnordered(NGIN::Containers::Vector<TypeId> values)
        {
            std::sort(values.begin(), values.end());
            auto last    = std::unique(values.begin(), values.end());
            auto newSize = static_cast<NGIN::UIntSize>(last - values.begin());
            while (values.Size() > newSize)
            {
                values.PopBack();
            }

            NGIN::UInt64 hash = 1469598103934665603ULL;
            for (const auto typeId : values)
            {
                const auto componentHash = NGIN::Hashing::FNV1a64(
                    reinterpret_cast<const char*>(&typeId),
                    sizeof(TypeId)
                );
                hash ^= componentHash + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
            }

            ArchetypeSignature signature {};
            signature.Types = std::move(values);
            signature.Hash  = hash;
            return signature;
        }
    };

    struct ComponentPayload
    {
        TypeId           id {0};
        ComponentInfo    Info {};
        const void*      Data {nullptr};
        bool             MoveConstruct {false};
    };

    struct ArchetypeRowAddress
    {
        NGIN::UIntSize ChunkIndex {kInvalidIndex};
        NGIN::UIntSize RowIndex {kInvalidIndex};
    };

    class Chunk
    {
    public:
        explicit Chunk(const NGIN::Containers::Vector<ComponentInfo>& components, NGIN::UIntSize capacity)
            : m_capacity(capacity)
        {
            m_columns.Reserve(components.Size());
            m_entities.Reserve(capacity);
            for (NGIN::UIntSize columnIndex = 0; columnIndex < components.Size(); ++columnIndex)
            {
                Column column {};
                column.Info = components[columnIndex];

                if (!column.Info.IsEmpty)
                {
                    column.Data = m_allocator.Allocate(column.Info.Size * capacity, column.Info.Align);
                    if (!column.Data)
                    {
                        throw std::bad_alloc();
                    }
                }

                column.AddedTicks = static_cast<NGIN::UInt64*>(
                    m_allocator.Allocate(sizeof(NGIN::UInt64) * capacity, alignof(NGIN::UInt64))
                );
                column.ChangedTicks = static_cast<NGIN::UInt64*>(
                    m_allocator.Allocate(sizeof(NGIN::UInt64) * capacity, alignof(NGIN::UInt64))
                );
                if (!column.AddedTicks || !column.ChangedTicks)
                {
                    throw std::bad_alloc();
                }

                std::memset(column.AddedTicks, 0, sizeof(NGIN::UInt64) * capacity);
                std::memset(column.ChangedTicks, 0, sizeof(NGIN::UInt64) * capacity);
                m_columns.EmplaceBack(column);
            }
        }

        Chunk(const Chunk&)            = delete;
        Chunk& operator=(const Chunk&) = delete;
        Chunk(Chunk&&)                 = delete;
        Chunk& operator=(Chunk&&)      = delete;

        ~Chunk()
        {
            Reset();
            for (NGIN::UIntSize columnIndex = 0; columnIndex < m_columns.Size(); ++columnIndex)
            {
                auto& column = m_columns[columnIndex];
                if (column.Data)
                {
                    m_allocator.Deallocate(column.Data, column.Info.Size * m_capacity, column.Info.Align);
                }
                if (column.AddedTicks)
                {
                    m_allocator.Deallocate(column.AddedTicks, sizeof(NGIN::UInt64) * m_capacity, alignof(NGIN::UInt64));
                }
                if (column.ChangedTicks)
                {
                    m_allocator.Deallocate(column.ChangedTicks,
                                           sizeof(NGIN::UInt64) * m_capacity,
                                           alignof(NGIN::UInt64));
                }
            }
        }

        [[nodiscard]] NGIN::UIntSize Capacity() const noexcept { return m_capacity; }
        [[nodiscard]] NGIN::UIntSize Count() const noexcept { return m_count; }
        [[nodiscard]] bool HasRoom() const noexcept { return m_count < m_capacity; }

        [[nodiscard]] EntityId EntityAt(NGIN::UIntSize row) const noexcept { return m_entities[row]; }
        [[nodiscard]] const EntityId* Entities() const noexcept { return m_entities.data(); }

        [[nodiscard]] bool HasColumn(NGIN::UIntSize columnIndex) const noexcept
        {
            return columnIndex < m_columns.Size();
        }

        [[nodiscard]] const ComponentInfo& ColumnInfo(NGIN::UIntSize columnIndex) const noexcept
        {
            return m_columns[columnIndex].Info;
        }

        [[nodiscard]] void* ComponentPtr(NGIN::UIntSize columnIndex, NGIN::UIntSize row) noexcept
        {
            auto& column = m_columns[columnIndex];
            if (column.Info.IsEmpty)
            {
                return nullptr;
            }
            return static_cast<std::byte*>(column.Data) + (row * column.Info.Size);
        }

        [[nodiscard]] const void* ComponentPtr(NGIN::UIntSize columnIndex, NGIN::UIntSize row) const noexcept
        {
            const auto& column = m_columns[columnIndex];
            if (column.Info.IsEmpty)
            {
                return nullptr;
            }
            return static_cast<const std::byte*>(column.Data) + (row * column.Info.Size);
        }

        [[nodiscard]] NGIN::UInt64 AddedTick(NGIN::UIntSize columnIndex, NGIN::UIntSize row) const noexcept
        {
            return m_columns[columnIndex].AddedTicks[row];
        }

        [[nodiscard]] NGIN::UInt64 ChangedTick(NGIN::UIntSize columnIndex, NGIN::UIntSize row) const noexcept
        {
            return m_columns[columnIndex].ChangedTicks[row];
        }

        void SetAddedTick(NGIN::UIntSize columnIndex, NGIN::UIntSize row, NGIN::UInt64 tick) noexcept
        {
            m_columns[columnIndex].AddedTicks[row] = tick;
        }

        void SetChangedTick(NGIN::UIntSize columnIndex, NGIN::UIntSize row, NGIN::UInt64 tick) noexcept
        {
            m_columns[columnIndex].ChangedTicks[row] = tick;
        }

        [[nodiscard]] NGIN::UIntSize BeginRow(EntityId entityId)
        {
            if (!HasRoom())
            {
                throw std::out_of_range("Chunk is full.");
            }
            const auto row = m_count;
            m_entities.EmplaceBack(entityId);
            ++m_count;
            return row;
        }

        void RollbackNewRow(NGIN::UIntSize row, NGIN::UIntSize constructedColumns) noexcept
        {
            for (NGIN::UIntSize columnIndex = 0; columnIndex < constructedColumns; ++columnIndex)
            {
                DestroyElement(columnIndex, row);
            }
            if (m_entities.Size() > 0)
            {
                m_entities.PopBack();
            }
            if (m_count > 0)
            {
                --m_count;
            }
        }

        void DestroyRow(NGIN::UIntSize row) noexcept
        {
            for (NGIN::UIntSize columnIndex = 0; columnIndex < m_columns.Size(); ++columnIndex)
            {
                DestroyElement(columnIndex, row);
            }
        }

        struct RemoveRowResult
        {
            bool               HadMovedEntity {false};
            EntityId           MovedEntity {NullEntityId};
            NGIN::UIntSize     NewRowIndex {kInvalidIndex};
        };

        [[nodiscard]] RemoveRowResult SwapRemoveRow(NGIN::UIntSize row)
        {
            if (row >= m_count)
            {
                throw std::out_of_range("Row index out of range.");
            }

            RemoveRowResult result {};
            const auto lastRow = m_count - 1;

            DestroyRow(row);
            if (row != lastRow)
            {
                result.HadMovedEntity = true;
                result.MovedEntity    = m_entities[lastRow];
                result.NewRowIndex    = row;

                for (NGIN::UIntSize columnIndex = 0; columnIndex < m_columns.Size(); ++columnIndex)
                {
                    MoveElement(columnIndex, lastRow, row);
                    m_columns[columnIndex].AddedTicks[row]   = m_columns[columnIndex].AddedTicks[lastRow];
                    m_columns[columnIndex].ChangedTicks[row] = m_columns[columnIndex].ChangedTicks[lastRow];
                }
                m_entities[row] = result.MovedEntity;
            }

            if (m_entities.Size() > 0)
            {
                m_entities.PopBack();
            }
            --m_count;
            return result;
        }

        void Reset() noexcept
        {
            for (NGIN::UIntSize row = 0; row < m_count; ++row)
            {
                DestroyRow(row);
            }
            m_entities.Clear();
            m_count = 0;
        }

    private:
        struct Column
        {
            ComponentInfo  Info {};
            void*          Data {nullptr};
            NGIN::UInt64*  AddedTicks {nullptr};
            NGIN::UInt64*  ChangedTicks {nullptr};
        };

        void DestroyElement(NGIN::UIntSize columnIndex, NGIN::UIntSize row) noexcept
        {
            auto& column = m_columns[columnIndex];
            if (column.Info.IsEmpty || !column.Info.Destroy || row >= m_count)
            {
                return;
            }
            column.Info.Destroy(ComponentPtr(columnIndex, row));
        }

        void MoveElement(NGIN::UIntSize columnIndex, NGIN::UIntSize sourceRow, NGIN::UIntSize destinationRow)
        {
            auto& column = m_columns[columnIndex];
            if (column.Info.IsEmpty)
            {
                return;
            }

            void* destination = ComponentPtr(columnIndex, destinationRow);
            void* source      = ComponentPtr(columnIndex, sourceRow);
            if (!destination || !source)
            {
                return;
            }

            if (column.Info.RelocateConstruct)
            {
                column.Info.RelocateConstruct(destination, source);
            }
            else if (column.Info.MoveConstruct)
            {
                column.Info.MoveConstruct(destination, source);
            }
            else if (column.Info.CopyConstruct)
            {
                column.Info.CopyConstruct(destination, source);
            }
            else
            {
                throw std::runtime_error("Component is not movable.");
            }
        }

    private:
        NGIN::Memory::SystemAllocator          m_allocator {};
        NGIN::Containers::Vector<Column>       m_columns;
        NGIN::Containers::Vector<EntityId>     m_entities;
        NGIN::UIntSize                         m_count {0};
        NGIN::UIntSize                         m_capacity {0};
    };

    class Archetype
    {
    public:
        explicit Archetype(ArchetypeSignature signature, NGIN::Containers::Vector<ComponentInfo> components)
            : m_signature(std::move(signature)), m_components(std::move(components))
        {
        }

        Archetype(const Archetype&)            = delete;
        Archetype& operator=(const Archetype&) = delete;
        Archetype(Archetype&&)                 = delete;
        Archetype& operator=(Archetype&&)      = delete;

        [[nodiscard]] const ArchetypeSignature& Signature() const noexcept { return m_signature; }
        [[nodiscard]] NGIN::UIntSize ComponentCount() const noexcept { return m_components.Size(); }
        [[nodiscard]] const ComponentInfo& ComponentAt(NGIN::UIntSize index) const noexcept { return m_components[index]; }
        [[nodiscard]] NGIN::UIntSize ChunkCount() const noexcept { return m_chunks.Size(); }

        [[nodiscard]] Chunk* GetChunk(NGIN::UIntSize index) const noexcept
        {
            return m_chunks[index].Get();
        }

        [[nodiscard]] bool HasComponent(TypeId typeId) const noexcept
        {
            return FindColumnIndex(typeId) != kInvalidIndex;
        }

        [[nodiscard]] NGIN::UIntSize FindColumnIndex(TypeId typeId) const noexcept
        {
            for (NGIN::UIntSize index = 0; index < m_components.Size(); ++index)
            {
                if (m_components[index].id == typeId)
                {
                    return index;
                }
            }
            return kInvalidIndex;
        }

        [[nodiscard]] NGIN::UIntSize ColumnIndexOf(TypeId typeId) const
        {
            const auto index = FindColumnIndex(typeId);
            if (index == kInvalidIndex)
            {
                throw std::out_of_range("Component is not present in archetype.");
            }
            return index;
        }

        [[nodiscard]] NGIN::UIntSize ComputeCapacityForChunkBytes(NGIN::UIntSize chunkBytes) const noexcept
        {
            NGIN::UIntSize rowBytes = sizeof(EntityId);
            for (NGIN::UIntSize index = 0; index < m_components.Size(); ++index)
            {
                rowBytes += (2 * sizeof(NGIN::UInt64));
                if (!m_components[index].IsEmpty)
                {
                    rowBytes += m_components[index].Size;
                }
            }
            if (rowBytes == 0)
            {
                return 1;
            }
            const auto capacity = chunkBytes / rowBytes;
            return capacity == 0 ? 1 : capacity;
        }

        template<typename Initializer>
        [[nodiscard]] ArchetypeRowAddress EmplaceRow(EntityId entityId, Initializer&& initializer)
        {
            auto [chunkIndex, chunk] = EnsureChunkWithRoom();
            const auto row = chunk->BeginRow(entityId);

            NGIN::UIntSize completedColumns = 0;
            try
            {
                for (; completedColumns < m_components.Size(); ++completedColumns)
                {
                    initializer(*chunk, chunkIndex, row, completedColumns, m_components[completedColumns]);
                }
            } catch (...)
            {
                chunk->RollbackNewRow(row, completedColumns);
                throw;
            }

            return ArchetypeRowAddress {chunkIndex, row};
        }

        template<typename RelocatedEntityFn>
        void RemoveRow(NGIN::UIntSize chunkIndex, NGIN::UIntSize rowIndex, RelocatedEntityFn&& relocatedEntityFn)
        {
            auto* chunk = GetChunk(chunkIndex);
            if (!chunk)
            {
                throw std::out_of_range("Chunk index out of range.");
            }

            const auto rowResult = chunk->SwapRemoveRow(rowIndex);
            if (rowResult.HadMovedEntity)
            {
                relocatedEntityFn(rowResult.MovedEntity, chunkIndex, rowResult.NewRowIndex);
            }

            if (chunk->Count() == 0)
            {
                const auto lastChunkIndex = m_chunks.Size() - 1;
                if (chunkIndex != lastChunkIndex)
                {
                    m_chunks[chunkIndex] = std::move(m_chunks[lastChunkIndex]);
                    auto* movedChunk = m_chunks[chunkIndex].Get();
                    for (NGIN::UIntSize row = 0; row < movedChunk->Count(); ++row)
                    {
                        relocatedEntityFn(movedChunk->EntityAt(row), chunkIndex, row);
                    }
                }
                m_chunks.PopBack();
            }
        }

    private:
        [[nodiscard]] std::pair<NGIN::UIntSize, Chunk*> EnsureChunkWithRoom()
        {
            if (m_chunks.Size() == 0 || !m_chunks[m_chunks.Size() - 1]->HasRoom())
            {
                auto chunk = NGIN::Memory::MakeScoped<Chunk>(
                    m_components,
                    ComputeCapacityForChunkBytes(kDefaultChunkBytes)
                );
                m_chunks.EmplaceBack(std::move(chunk));
            }
            return {m_chunks.Size() - 1, m_chunks[m_chunks.Size() - 1].Get()};
        }

    private:
        ArchetypeSignature                                      m_signature;
        NGIN::Containers::Vector<ComponentInfo>                 m_components;
        NGIN::Containers::Vector<NGIN::Memory::Scoped<Chunk>>   m_chunks;
    };
}

namespace std
{
    template<>
    struct hash<NGIN::ECS::ArchetypeSignature>
    {
        size_t operator()(const NGIN::ECS::ArchetypeSignature& signature) const noexcept
        {
            return static_cast<size_t>(signature.Hash);
        }
    };
}
