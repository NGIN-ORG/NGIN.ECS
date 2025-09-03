#pragma once

#include <NGIN/Primitives.hpp>
#include <NGIN/Containers/Vector.hpp>
#include <NGIN/Hashing/FNV.hpp>
#include <NGIN/Memory/SystemAllocator.hpp>
#include <NGIN/ECS/Entity.hpp>
#include <NGIN/ECS/TypeRegistry.hpp>

#include <algorithm>
#include <cstring>

namespace NGIN::ECS
{
    inline constexpr NGIN::UIntSize kDefaultChunkBytes = 64 * 1024;

    struct ArchetypeSignature
    {
        NGIN::Containers::Vector<TypeId> Types; // canonical sorted order
        NGIN::UInt64                      Hash {0};

        [[nodiscard]] bool operator==(const ArchetypeSignature& other) const noexcept
        {
            if (Hash != other.Hash)
                return false; // fast path, tolerate collision via Types compare next
            if (Types.Size() != other.Types.Size())
                return false;
            for (NGIN::UIntSize i = 0; i < Types.Size(); ++i)
                if (Types[i] != other.Types[i])
                    return false;
            return true;
        }

        [[nodiscard]] static ArchetypeSignature FromUnordered(NGIN::Containers::Vector<TypeId> values)
        {
            // Sort and deduplicate
            std::sort(values.begin(), values.end());
            auto last    = std::unique(values.begin(), values.end());
            auto newSize = static_cast<NGIN::UIntSize>(last - values.begin());
            while (values.Size() > newSize)
                values.PopBack();
            // Hash all type ids
            NGIN::UInt64 h = 1469598103934665603ULL; // base seed
            for (auto& v: values)
            {
                const auto* p = reinterpret_cast<const char*>(&v);
                const auto hv = NGIN::Hashing::FNV1a64(p, sizeof(TypeId));
                // 64-bit mix (boost style)
                h ^= hv + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            }
            ArchetypeSignature sig {};
            sig.Types = std::move(values);
            sig.Hash  = h;
            return sig;
        }
    };

    struct ColumnLayout
    {
        ComponentInfo Info {};
        NGIN::UIntSize Stride {0}; // bytes per element (equals Info.Size for POD/direct storage)
    };

    struct ComponentPayload
    {
        TypeId                   id {0};
        const void*              data {nullptr};
        NGIN::UIntSize           size {0};
        NGIN::UIntSize           align {1};
    };

    class Chunk
    {
    public:
        Chunk() = default;
        Chunk(const NGIN::Containers::Vector<ColumnLayout>& columns, NGIN::UIntSize capacity)
            : m_capacity(capacity)
        {
            m_columnsData.Reserve(columns.Size());
            for (NGIN::UIntSize i = 0; i < columns.Size(); ++i)
            {
                if (columns[i].Info.IsEmpty || columns[i].Stride == 0)
                {
                    m_columnsData.EmplaceBack(nullptr);
                }
                else
                {
                    void* mem = m_alloc.Allocate(columns[i].Stride * capacity, columns[i].Info.Align);
                    if (!mem) throw std::bad_alloc();
                    m_columnsData.EmplaceBack(mem);
                }
            }
            m_entities.Reserve(capacity);
            // Initialize version clocks per column
            m_writeVersion.Reserve(columns.Size());
            m_addVersion.Reserve(columns.Size());
            for (NGIN::UIntSize i = 0; i < columns.Size(); ++i)
            {
                m_writeVersion.EmplaceBack(0);
                m_addVersion.EmplaceBack(0);
            }
        }

        ~Chunk()
        {
            for (NGIN::UIntSize i = 0; i < m_columnsData.Size(); ++i)
            {
                if (m_columnsData[i])
                {
                    // Note: no dtor calls because MVP supports POD-first direct columns
                    // Non-POD to be handled via blob store in later phases
                    m_alloc.Deallocate(m_columnsData[i], 0, 0);
                }
            }
        }

        [[nodiscard]] NGIN::UIntSize Capacity() const noexcept { return m_capacity; }
        [[nodiscard]] NGIN::UIntSize Count() const noexcept { return m_count; }
        [[nodiscard]] bool           HasRoom() const noexcept { return m_count < m_capacity; }
        [[nodiscard]] void*          ColumnPtr(NGIN::UIntSize i) noexcept { return m_columnsData[i]; }
        [[nodiscard]] const void*    ColumnPtr(NGIN::UIntSize i) const noexcept { return m_columnsData[i]; }
        [[nodiscard]] NGIN::UInt64   WriteVersion(NGIN::UIntSize col) const noexcept { return m_writeVersion[col]; }
        [[nodiscard]] NGIN::UInt64   AddedVersion(NGIN::UIntSize col) const noexcept { return m_addVersion[col]; }
        void                         BumpWriteVersion(NGIN::UIntSize col, NGIN::UInt64 epoch) noexcept { m_writeVersion[col] = epoch; }
        void                         BumpAddVersion(NGIN::UIntSize col, NGIN::UInt64 epoch) noexcept { m_addVersion[col] = epoch; }

        void AddRow(EntityId id,
                    const NGIN::Containers::Vector<ColumnLayout>& columns,
                    const void* const*                             values,
                    NGIN::UInt64                                   epoch)
        {
            const auto row = m_count;
            if (!(row < m_capacity)) throw std::out_of_range("Chunk full");
            m_entities.EmplaceBack(id);
            for (NGIN::UIntSize c = 0; c < columns.Size(); ++c)
            {
                if (!(columns[c].Info.IsEmpty || columns[c].Stride == 0))
                {
                    const auto* src = values[c];
                    if (!src)
                        throw std::invalid_argument("Missing component value for column");
                    auto* base = static_cast<char*>(m_columnsData[c]);
                    std::memcpy(base + row * columns[c].Stride, src, columns[c].Stride);
                }
                // Mark as added at this epoch for all columns (including tags)
                m_addVersion[c] = epoch;
            }
            ++m_count;
        }

    private:
        NGIN::Memory::SystemAllocator                m_alloc {};
        NGIN::Containers::Vector<void*>              m_columnsData; // size = columns
        NGIN::Containers::Vector<EntityId>           m_entities;
        NGIN::Containers::Vector<NGIN::UInt64>       m_writeVersion; // per column
        NGIN::Containers::Vector<NGIN::UInt64>       m_addVersion;   // per column
        NGIN::UIntSize                               m_count {0};
        NGIN::UIntSize                               m_capacity {0};
    };

    class Archetype
    {
    public:
        explicit Archetype(ArchetypeSignature sig, NGIN::Containers::Vector<ComponentInfo> components)
            : m_signature(std::move(sig)), m_components(std::move(components))
        {
            m_columns.Reserve(m_components.Size());
            for (NGIN::UIntSize i = 0; i < m_components.Size(); ++i)
            {
                ColumnLayout cl {};
                cl.Info   = m_components[i];
                cl.Stride = cl.Info.IsEmpty ? 0 : cl.Info.Size;
                m_columns.EmplaceBack(cl);
            }
            m_rowStride = ComputeRowStrideBytes();
        }

        [[nodiscard]] const ArchetypeSignature& Signature() const noexcept { return m_signature; }
        [[nodiscard]] NGIN::UIntSize             ComponentCount() const noexcept { return m_components.Size(); }
        [[nodiscard]] const ColumnLayout&        ColumnAt(NGIN::UIntSize i) const noexcept { return m_columns[i]; }
        [[nodiscard]] NGIN::UIntSize             RowStrideBytes() const noexcept { return m_rowStride; }
        [[nodiscard]] NGIN::UIntSize             ColumnIndexOf(TypeId id) const
        {
            for (NGIN::UIntSize i = 0; i < m_columns.Size(); ++i)
                if (m_columns[i].Info.id == id)
                    return i;
            throw std::out_of_range("Component not in archetype");
        }

        [[nodiscard]] NGIN::UIntSize ComputeCapacityForChunkBytes(NGIN::UIntSize chunkBytes) const noexcept
        {
            auto stride = m_rowStride + sizeof(EntityId); // include entity id per row
            if (stride == 0) return chunkBytes; // degenerate
            auto cap = static_cast<NGIN::UIntSize>(chunkBytes / stride);
            return cap == 0 ? 1 : cap;
        }

        Chunk* GetChunkWithRoom()
        {
            if (m_chunks.Size() == 0 || !m_chunks[m_chunks.Size() - 1]->HasRoom())
            {
                const auto cap = ComputeCapacityForChunkBytes(kDefaultChunkBytes);
                auto*      ch  = new Chunk(m_columns, cap);
                m_chunks.EmplaceBack(ch);
            }
            return m_chunks[m_chunks.Size() - 1];
        }

        [[nodiscard]] Chunk* GetChunk(NGIN::UIntSize i) const { return m_chunks[i]; }

        template<typename... Cs>
        void Insert(EntityId id, NGIN::UInt64 epoch, const Cs&... cs)
        {
            auto* chunk = GetChunkWithRoom();
            // Build aligned pointer list matching column order
            m_valueScratch.Reserve(m_columns.Size());
            m_valueScratch.Clear();
            for (NGIN::UIntSize c = 0; c < m_columns.Size(); ++c)
            {
                if (m_columns[c].Info.IsEmpty || m_columns[c].Stride == 0)
                {
                    m_valueScratch.EmplaceBack(nullptr);
                }
                else
                {
                    m_valueScratch.EmplaceBack(FindValuePtr(m_columns[c].Info.id, cs...));
                }
            }
            chunk->AddRow(id, m_columns, m_valueScratch.data(), epoch);
        }

        void InsertDynamic(EntityId id, NGIN::UInt64 epoch, const NGIN::Containers::Vector<ComponentPayload>& values)
        {
            auto* chunk = GetChunkWithRoom();
            m_valueScratch.Reserve(m_columns.Size());
            m_valueScratch.Clear();
            for (NGIN::UIntSize c = 0; c < m_columns.Size(); ++c)
            {
                if (m_columns[c].Info.IsEmpty || m_columns[c].Stride == 0)
                {
                    m_valueScratch.EmplaceBack(nullptr);
                    continue;
                }
                const TypeId need = m_columns[c].Info.id;
                const void*  ptr  = nullptr;
                for (NGIN::UIntSize i = 0; i < values.Size(); ++i)
                {
                    if (values[i].id == need)
                    {
                        ptr = values[i].data;
                        break;
                    }
                }
                if (!ptr)
                    throw std::invalid_argument("Missing component value for column");
                m_valueScratch.EmplaceBack(ptr);
            }
            chunk->AddRow(id, m_columns, m_valueScratch.data(), epoch);
        }

        [[nodiscard]] NGIN::UIntSize ChunkCount() const noexcept { return m_chunks.Size(); }
        [[nodiscard]] NGIN::UIntSize LastChunkCapacity() const noexcept
        {
            if (m_chunks.Size() == 0) return 0;
            return m_chunks[m_chunks.Size() - 1]->Capacity();
        }

    private:
        [[nodiscard]] NGIN::UIntSize ComputeRowStrideBytes() const noexcept
        {
            NGIN::UIntSize bytes = 0;
            for (NGIN::UIntSize i = 0; i < m_components.Size(); ++i)
            {
                if (!m_components[i].IsEmpty)
                    bytes += m_components[i].Size;
            }
            return bytes;
        }

        template<typename T0>
        static const void* FindValuePtr(TypeId id, const T0& v0)
        {
            if (GetTypeId<T0>() == id) return &v0;
            return nullptr;
        }
        template<typename T0, typename T1, typename... Rest>
        static const void* FindValuePtr(TypeId id, const T0& v0, const T1& v1, const Rest&... rest)
        {
            if (GetTypeId<T0>() == id) return &v0;
            return FindValuePtr(id, v1, rest...);
        }

        ArchetypeSignature                        m_signature;
        NGIN::Containers::Vector<ComponentInfo>   m_components; // canonical order
        NGIN::Containers::Vector<ColumnLayout>    m_columns;
        NGIN::Containers::Vector<Chunk*>          m_chunks;
        NGIN::UIntSize                            m_rowStride {0};

        // Scratch buffer for value pointers during insertion
        NGIN::Containers::Vector<const void*>     m_valueScratch;
    };
}

namespace std
{
    template<>
    struct hash<NGIN::ECS::ArchetypeSignature>
    {
        size_t operator()(const NGIN::ECS::ArchetypeSignature& s) const noexcept
        {
            return static_cast<size_t>(s.Hash);
        }
    };
}
