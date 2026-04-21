#pragma once

#include <NGIN/ECS/TypeRegistry.hpp>
#include <NGIN/ECS/World.hpp>

#include <algorithm>
#include <limits>
#include <type_traits>
#include <utility>

namespace NGIN::ECS
{
    template<typename T>
    struct Read
    {
        using Type = T;
    };

    template<typename T>
    struct Write
    {
        using Type = T;
    };

    template<typename T>
    struct Opt
    {
        using Type = T;
    };

    template<typename T>
    struct With
    {
        using Type = T;
    };

    template<typename T>
    struct Without
    {
        using Type = T;
    };

    template<typename T>
    struct Changed
    {
        using Type = T;
    };

    template<typename T>
    struct Added
    {
        using Type = T;
    };

    namespace detail
    {
        template<typename T>
        [[nodiscard]] inline T* EmptyComponentInstance() noexcept
        {
            static T instance {};
            return &instance;
        }

        template<class Vec>
        inline void SortUnique(Vec& values)
        {
            std::sort(values.begin(), values.end());
            auto last    = std::unique(values.begin(), values.end());
            auto newSize = static_cast<NGIN::UIntSize>(last - values.begin());
            while (values.Size() > newSize)
            {
                values.PopBack();
            }
        }

        struct QueryTermMetadata
        {
            NGIN::Containers::Vector<TypeId> Required;
            NGIN::Containers::Vector<TypeId> Optional;
            NGIN::Containers::Vector<TypeId> With;
            NGIN::Containers::Vector<TypeId> Without;
            NGIN::Containers::Vector<TypeId> Reads;
            NGIN::Containers::Vector<TypeId> Writes;
            NGIN::Containers::Vector<TypeId> Changed;
            NGIN::Containers::Vector<TypeId> Added;
        };

        template<typename Term>
        struct TermMetadataCollector;

        template<typename T>
        struct TermMetadataCollector<Read<T>>
        {
            static void Collect(QueryTermMetadata& metadata)
            {
                metadata.Required.EmplaceBack(GetTypeId<T>());
                metadata.Reads.EmplaceBack(GetTypeId<T>());
            }
        };

        template<typename T>
        struct TermMetadataCollector<Write<T>>
        {
            static void Collect(QueryTermMetadata& metadata)
            {
                metadata.Required.EmplaceBack(GetTypeId<T>());
                metadata.Writes.EmplaceBack(GetTypeId<T>());
            }
        };

        template<typename T>
        struct TermMetadataCollector<Opt<T>>
        {
            static void Collect(QueryTermMetadata& metadata)
            {
                metadata.Optional.EmplaceBack(GetTypeId<T>());
                metadata.Reads.EmplaceBack(GetTypeId<T>());
            }
        };

        template<typename T>
        struct TermMetadataCollector<With<T>>
        {
            static void Collect(QueryTermMetadata& metadata)
            {
                metadata.With.EmplaceBack(GetTypeId<T>());
            }
        };

        template<typename T>
        struct TermMetadataCollector<Without<T>>
        {
            static void Collect(QueryTermMetadata& metadata)
            {
                metadata.Without.EmplaceBack(GetTypeId<T>());
            }
        };

        template<typename T>
        struct TermMetadataCollector<Changed<T>>
        {
            static void Collect(QueryTermMetadata& metadata)
            {
                metadata.Required.EmplaceBack(GetTypeId<T>());
                metadata.Reads.EmplaceBack(GetTypeId<T>());
                metadata.Changed.EmplaceBack(GetTypeId<T>());
            }
        };

        template<typename T>
        struct TermMetadataCollector<Added<T>>
        {
            static void Collect(QueryTermMetadata& metadata)
            {
                metadata.Required.EmplaceBack(GetTypeId<T>());
                metadata.Reads.EmplaceBack(GetTypeId<T>());
                metadata.Added.EmplaceBack(GetTypeId<T>());
            }
        };

        template<typename... Terms>
        [[nodiscard]] inline QueryTermMetadata BuildQueryMetadata()
        {
            QueryTermMetadata metadata {};
            (TermMetadataCollector<Terms>::Collect(metadata), ...);
            SortUnique(metadata.Required);
            SortUnique(metadata.Optional);
            SortUnique(metadata.With);
            SortUnique(metadata.Without);
            SortUnique(metadata.Reads);
            SortUnique(metadata.Writes);
            SortUnique(metadata.Changed);
            SortUnique(metadata.Added);
            return metadata;
        }
    }// namespace detail

    class ChunkView;

    class RowView
    {
    public:
        RowView(const ChunkView* chunkView, NGIN::UIntSize logicalIndex)
            : m_chunkView(chunkView), m_logicalIndex(logicalIndex)
        {
        }

        [[nodiscard]] EntityId Entity() const noexcept;
        [[nodiscard]] EntityId entity() const noexcept { return Entity(); }

        template<typename T>
        [[nodiscard]] const T& Read() const;

        template<typename T>
        [[nodiscard]] const T& read() const
        {
            return Read<T>();
        }

        template<typename T>
        [[nodiscard]] T& Write() const;

        template<typename T>
        [[nodiscard]] T& write() const
        {
            return Write<T>();
        }

        template<typename T>
        [[nodiscard]] const T* TryRead() const;

        template<typename T>
        [[nodiscard]] const T* try_read() const
        {
            return TryRead<T>();
        }

        template<typename T>
        [[nodiscard]] T* TryWrite() const;

        template<typename T>
        [[nodiscard]] T* try_write() const
        {
            return TryWrite<T>();
        }

        template<typename T>
        void MarkChanged() const;

        template<typename T>
        void mark_changed() const
        {
            MarkChanged<T>();
        }

    private:
        const ChunkView*   m_chunkView {nullptr};
        NGIN::UIntSize     m_logicalIndex {0};
    };

    class ChunkView
    {
    public:
        ChunkView(Archetype* archetype,
                  Chunk* chunk,
                  const NGIN::Containers::Vector<NGIN::UIntSize>* rows,
                  NGIN::UInt64 markTick)
            : m_archetype(archetype), m_chunk(chunk), m_rows(rows), m_markTick(markTick)
        {
        }

        [[nodiscard]] NGIN::UIntSize Count() const noexcept { return m_rows ? m_rows->Size() : 0; }
        [[nodiscard]] NGIN::UIntSize count() const noexcept { return Count(); }

        [[nodiscard]] RowView Row(NGIN::UIntSize logicalIndex) const
        {
            return RowView(this, logicalIndex);
        }

        [[nodiscard]] RowView row(NGIN::UIntSize logicalIndex) const
        {
            return Row(logicalIndex);
        }

        [[nodiscard]] EntityId EntityAt(NGIN::UIntSize logicalIndex) const noexcept
        {
            return m_chunk->EntityAt(PhysicalRow(logicalIndex));
        }

        [[nodiscard]] EntityId entity(NGIN::UIntSize logicalIndex) const noexcept
        {
            return EntityAt(logicalIndex);
        }

        template<typename T>
        [[nodiscard]] bool Has() const noexcept
        {
            return m_archetype->HasComponent(GetTypeId<T>());
        }

        template<typename T>
        [[nodiscard]] bool has() const noexcept
        {
            return Has<T>();
        }

        template<typename T>
        [[nodiscard]] const T* TryRead(NGIN::UIntSize logicalIndex) const
        {
            const auto columnIndex = m_archetype->FindColumnIndex(GetTypeId<T>());
            if (columnIndex == kInvalidIndex)
            {
                return nullptr;
            }
            if constexpr (std::is_empty_v<T>)
            {
                return detail::EmptyComponentInstance<T>();
            }
            return static_cast<const T*>(m_chunk->ComponentPtr(columnIndex, PhysicalRow(logicalIndex)));
        }

        template<typename T>
        [[nodiscard]] const T* try_read(NGIN::UIntSize logicalIndex) const
        {
            return TryRead<T>(logicalIndex);
        }

        template<typename T>
        [[nodiscard]] T* TryWrite(NGIN::UIntSize logicalIndex) const
        {
            const auto columnIndex = m_archetype->FindColumnIndex(GetTypeId<T>());
            if (columnIndex == kInvalidIndex)
            {
                return nullptr;
            }
            if constexpr (std::is_empty_v<T>)
            {
                return detail::EmptyComponentInstance<T>();
            }
            return static_cast<T*>(m_chunk->ComponentPtr(columnIndex, PhysicalRow(logicalIndex)));
        }

        template<typename T>
        [[nodiscard]] T* try_write(NGIN::UIntSize logicalIndex) const
        {
            return TryWrite<T>(logicalIndex);
        }

        template<typename T>
        [[nodiscard]] const T& Read(NGIN::UIntSize logicalIndex) const
        {
            const auto* component = TryRead<T>(logicalIndex);
            if (!component)
            {
                throw std::out_of_range("Component is not present in query row.");
            }
            return *component;
        }

        template<typename T>
        [[nodiscard]] const T& read(NGIN::UIntSize logicalIndex) const
        {
            return Read<T>(logicalIndex);
        }

        template<typename T>
        [[nodiscard]] T& Write(NGIN::UIntSize logicalIndex) const
        {
            auto* component = TryWrite<T>(logicalIndex);
            if (!component)
            {
                throw std::out_of_range("Component is not present in query row.");
            }
            return *component;
        }

        template<typename T>
        [[nodiscard]] T& write(NGIN::UIntSize logicalIndex) const
        {
            return Write<T>(logicalIndex);
        }

        template<typename T>
        void MarkChanged(NGIN::UIntSize logicalIndex) const
        {
            const auto columnIndex = m_archetype->ColumnIndexOf(GetTypeId<T>());
            m_chunk->SetChangedTick(columnIndex, PhysicalRow(logicalIndex), m_markTick);
        }

        template<typename T>
        void mark_changed(NGIN::UIntSize logicalIndex) const
        {
            MarkChanged<T>(logicalIndex);
        }

        template<typename T>
        [[nodiscard]] NGIN::UInt64 AddedTick(NGIN::UIntSize logicalIndex) const
        {
            const auto columnIndex = m_archetype->ColumnIndexOf(GetTypeId<T>());
            return m_chunk->AddedTick(columnIndex, PhysicalRow(logicalIndex));
        }

        template<typename T>
        [[nodiscard]] NGIN::UInt64 ChangedTick(NGIN::UIntSize logicalIndex) const
        {
            const auto columnIndex = m_archetype->ColumnIndexOf(GetTypeId<T>());
            return m_chunk->ChangedTick(columnIndex, PhysicalRow(logicalIndex));
        }

    private:
        [[nodiscard]] NGIN::UIntSize PhysicalRow(NGIN::UIntSize logicalIndex) const noexcept
        {
            return (*m_rows)[logicalIndex];
        }

    private:
        Archetype*                                   m_archetype {nullptr};
        Chunk*                                       m_chunk {nullptr};
        const NGIN::Containers::Vector<NGIN::UIntSize>* m_rows {nullptr};
        NGIN::UInt64                                 m_markTick {0};

        friend class RowView;
    };

    inline EntityId RowView::Entity() const noexcept
    {
        return m_chunkView->EntityAt(m_logicalIndex);
    }

    template<typename T>
    inline const T& RowView::Read() const
    {
        return m_chunkView->template Read<T>(m_logicalIndex);
    }

    template<typename T>
    inline T& RowView::Write() const
    {
        return m_chunkView->template Write<T>(m_logicalIndex);
    }

    template<typename T>
    inline const T* RowView::TryRead() const
    {
        return m_chunkView->template TryRead<T>(m_logicalIndex);
    }

    template<typename T>
    inline T* RowView::TryWrite() const
    {
        return m_chunkView->template TryWrite<T>(m_logicalIndex);
    }

    template<typename T>
    inline void RowView::MarkChanged() const
    {
        m_chunkView->template MarkChanged<T>(m_logicalIndex);
    }

    template<typename... Terms>
    class Query
    {
    public:
        static inline constexpr NGIN::UInt64 kAutoSinceTick = (std::numeric_limits<NGIN::UInt64>::max)();

        explicit Query(World& world, NGIN::UInt64 sinceTick = kAutoSinceTick)
            : m_world(world),
              m_sinceTick(sinceTick == kAutoSinceTick ? world.PreviousEpoch() : sinceTick),
              m_metadata(detail::BuildQueryMetadata<Terms...>())
        {
        }

        [[nodiscard]] NGIN::UInt64 SinceTick() const noexcept { return m_sinceTick; }
        [[nodiscard]] const detail::QueryTermMetadata& Metadata() const noexcept { return m_metadata; }

        template<typename F>
        void ForChunks(F&& function)
        {
            for (NGIN::UIntSize archetypeIndex = 0; archetypeIndex < m_world.Archetypes().Size(); ++archetypeIndex)
            {
                auto* archetype = m_world.Archetypes()[archetypeIndex].Get();
                if (!archetype || !Matches(*archetype))
                {
                    continue;
                }

                for (NGIN::UIntSize chunkIndex = 0; chunkIndex < archetype->ChunkCount(); ++chunkIndex)
                {
                    auto* chunk = archetype->GetChunk(chunkIndex);
                    if (!chunk || chunk->Count() == 0)
                    {
                        continue;
                    }

                    m_rowScratch.Clear();
                    for (NGIN::UIntSize row = 0; row < chunk->Count(); ++row)
                    {
                        if (PassesFilters(*archetype, *chunk, row))
                        {
                            m_rowScratch.EmplaceBack(row);
                        }
                    }

                    if (m_rowScratch.Size() == 0)
                    {
                        continue;
                    }

                    ChunkView view {archetype, chunk, &m_rowScratch, m_world.CurrentEpoch()};
                    function(view);
                }
            }
        }

        template<typename F>
        void for_chunks(F&& function)
        {
            ForChunks(std::forward<F>(function));
        }

        template<typename F>
        void ForEach(F&& function)
        {
            ForChunks([&](const ChunkView& chunkView) {
                for (NGIN::UIntSize logicalIndex = 0; logicalIndex < chunkView.Count(); ++logicalIndex)
                {
                    function(chunkView.Row(logicalIndex));
                }
            });
        }

        template<typename F>
        void each(F&& function)
        {
            ForEach(std::forward<F>(function));
        }

    private:
        [[nodiscard]] bool Matches(const Archetype& archetype) const
        {
            const auto& types = archetype.Signature().Types;
            auto containsAll = [&](const NGIN::Containers::Vector<TypeId>& required) {
                for (NGIN::UIntSize index = 0; index < required.Size(); ++index)
                {
                    if (!std::binary_search(types.begin(), types.end(), required[index]))
                    {
                        return false;
                    }
                }
                return true;
            };

            auto containsNone = [&](const NGIN::Containers::Vector<TypeId>& disallowed) {
                for (NGIN::UIntSize index = 0; index < disallowed.Size(); ++index)
                {
                    if (std::binary_search(types.begin(), types.end(), disallowed[index]))
                    {
                        return false;
                    }
                }
                return true;
            };

            return containsAll(m_metadata.Required) &&
                   containsAll(m_metadata.With) &&
                   containsNone(m_metadata.Without);
        }

        [[nodiscard]] bool PassesFilters(const Archetype& archetype, const Chunk& chunk, NGIN::UIntSize row) const
        {
            for (NGIN::UIntSize index = 0; index < m_metadata.Changed.Size(); ++index)
            {
                const auto columnIndex = archetype.ColumnIndexOf(m_metadata.Changed[index]);
                if (chunk.ChangedTick(columnIndex, row) <= m_sinceTick)
                {
                    return false;
                }
            }

            for (NGIN::UIntSize index = 0; index < m_metadata.Added.Size(); ++index)
            {
                const auto columnIndex = archetype.ColumnIndexOf(m_metadata.Added[index]);
                if (chunk.AddedTick(columnIndex, row) <= m_sinceTick)
                {
                    return false;
                }
            }

            return true;
        }

    private:
        World&                                           m_world;
        NGIN::UInt64                                     m_sinceTick {0};
        detail::QueryTermMetadata                        m_metadata;
        NGIN::Containers::Vector<NGIN::UIntSize>         m_rowScratch;
    };
}
