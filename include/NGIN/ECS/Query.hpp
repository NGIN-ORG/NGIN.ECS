#pragma once

// Query markers and execution over chunks

#include <NGIN/ECS/TypeRegistry.hpp>
#include <NGIN/ECS/World.hpp>
#include <algorithm>

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
    template<typename Tag>
    struct With
    {
        using Type = Tag;
    };
    template<typename Tag>
    struct Without
    {
        using Type = Tag;
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
        template<class Vec>
        inline void SortUnique(Vec& v)
        {
            std::sort(v.begin(), v.end());
            auto last    = std::unique(v.begin(), v.end());
            auto newSize = static_cast<NGIN::UIntSize>(last - v.begin());
            while (v.Size() > newSize)
                v.PopBack();
        }

        template<typename Term>
        struct TermApply;

        template<typename T>
        struct TermApply<Read<T>>
        {
            static void Add(NGIN::Containers::Vector<TypeId>& required,
                            NGIN::Containers::Vector<TypeId>& with,
                            NGIN::Containers::Vector<TypeId>& without)
            {
                (void)with; (void)without;
                required.EmplaceBack(GetTypeId<T>());
            }
        };
        template<typename T>
        struct TermApply<Write<T>> : TermApply<Read<T>> { };
        template<typename T>
        struct TermApply<Changed<T>> : TermApply<Read<T>> { };
        template<typename T>
        struct TermApply<Added<T>> : TermApply<Read<T>> { };
        template<typename T>
        struct TermApply<Opt<T>>
        {
            static void Add(NGIN::Containers::Vector<TypeId>& required,
                            NGIN::Containers::Vector<TypeId>& with,
                            NGIN::Containers::Vector<TypeId>& without)
            {
                (void)required; (void)with; (void)without; // optional does not constrain matching in MVP
            }
        };
        template<typename T>
        struct TermApply<With<T>>
        {
            static void Add(NGIN::Containers::Vector<TypeId>& required,
                            NGIN::Containers::Vector<TypeId>& with,
                            NGIN::Containers::Vector<TypeId>& without)
            {
                (void)required; (void)without;
                with.EmplaceBack(GetTypeId<T>());
            }
        };
        template<typename T>
        struct TermApply<Without<T>>
        {
            static void Add(NGIN::Containers::Vector<TypeId>& required,
                            NGIN::Containers::Vector<TypeId>& with,
                            NGIN::Containers::Vector<TypeId>& without)
            {
                (void)required; (void)with;
                without.EmplaceBack(GetTypeId<T>());
            }
        };
    } // namespace detail

    struct ChunkView
    {
        Archetype*      archetype {nullptr};
        Chunk*          chunk {nullptr};
        NGIN::UIntSize  beginIndex {0};
        NGIN::UIntSize  endIndex {0};

        [[nodiscard]] NGIN::UIntSize begin() const noexcept { return beginIndex; }
        [[nodiscard]] NGIN::UIntSize end() const noexcept { return endIndex; }

        template<typename T>
        [[nodiscard]] const T* read() const
        {
            const auto idx = archetype->ColumnIndexOf(GetTypeId<T>());
            return reinterpret_cast<const T*>(chunk->ColumnPtr(idx));
        }
        template<typename T>
        [[nodiscard]] T* write() const
        {
            const auto idx = archetype->ColumnIndexOf(GetTypeId<T>());
            return reinterpret_cast<T*>(chunk->ColumnPtr(idx));
        }
    };

    template<typename... Terms>
    class Query
    {
    public:
        explicit Query(World& world) : m_world(world)
        {
            // Build sets
            (detail::TermApply<Terms>::Add(m_required, m_with, m_without), ...);
            (CollectWrite<Terms>(), ...);
            (CollectChanged<Terms>(), ...);
            (CollectAdded<Terms>(), ...);
            detail::SortUnique(m_required);
            detail::SortUnique(m_with);
            detail::SortUnique(m_without);
            detail::SortUnique(m_writes);
            detail::SortUnique(m_changed);
            detail::SortUnique(m_added);
        }

        template<typename F>
        void for_chunks(F&& func)
        {
            for (auto* arch: m_world.Archetypes())
            {
                if (!arch) continue;
                if (!Matches(*arch)) continue;
                const auto chunkCount = arch->ChunkCount();
                for (NGIN::UIntSize i = 0; i < chunkCount; ++i)
                {
                    auto* ch = arch->GetChunk(i);
                    if (!ch || ch->Count() == 0) continue;
                    if (!PassesChangeFilters(*arch, *ch))
                        continue;
                    // Bump write versions for declared write terms
                    for (NGIN::UIntSize wi = 0; wi < m_writes.Size(); ++wi)
                    {
                        const auto col = arch->ColumnIndexOf(m_writes[wi]);
                        ch->BumpWriteVersion(col, m_world.CurrentEpoch());
                    }
                    ChunkView view { arch, ch, 0, ch->Count() };
                    func(view);
                }
            }
        }

    private:
        template<typename Term>
        struct CollectWriteTerm { static void Add(Query& q) {} };
        template<typename T>
        struct CollectWriteTerm<Write<T>> { static void Add(Query& q) { q.m_writes.EmplaceBack(GetTypeId<T>()); } };
        template<typename Term>
        void CollectWrite() { CollectWriteTerm<Term>::Add(*this); }

        template<typename Term>
        struct CollectChangedTerm { static void Add(Query& q) {} };
        template<typename T>
        struct CollectChangedTerm<Changed<T>> { static void Add(Query& q) { q.m_changed.EmplaceBack(GetTypeId<T>()); } };
        template<typename Term>
        void CollectChanged() { CollectChangedTerm<Term>::Add(*this); }

        template<typename Term>
        struct CollectAddedTerm { static void Add(Query& q) {} };
        template<typename T>
        struct CollectAddedTerm<Added<T>> { static void Add(Query& q) { q.m_added.EmplaceBack(GetTypeId<T>()); } };
        template<typename Term>
        void CollectAdded() { CollectAddedTerm<Term>::Add(*this); }

        [[nodiscard]] bool Matches(const Archetype& arch) const
        {
            const auto& types = arch.Signature().Types;
            auto containsAll = [&](const NGIN::Containers::Vector<TypeId>& need) {
                for (NGIN::UIntSize i = 0; i < need.Size(); ++i)
                {
                    if (!std::binary_search(types.begin(), types.end(), need[i]))
                        return false;
                }
                return true;
            };
            auto containsNone = [&](const NGIN::Containers::Vector<TypeId>& none) {
                for (NGIN::UIntSize i = 0; i < none.Size(); ++i)
                {
                    if (std::binary_search(types.begin(), types.end(), none[i]))
                        return false;
                }
                return true;
            };
            return containsAll(m_required) && containsAll(m_with) && containsNone(m_without);
        }

        [[nodiscard]] bool PassesChangeFilters(const Archetype& arch, const Chunk& ch) const
        {
            const auto epoch = m_world.CurrentEpoch();
            // Changed: require write version equals current epoch
            for (NGIN::UIntSize i = 0; i < m_changed.Size(); ++i)
            {
                const auto col = arch.ColumnIndexOf(m_changed[i]);
                if (ch.WriteVersion(col) != epoch)
                    return false;
            }
            // Added: require add version equals current epoch
            for (NGIN::UIntSize i = 0; i < m_added.Size(); ++i)
            {
                const auto col = arch.ColumnIndexOf(m_added[i]);
                if (ch.AddedVersion(col) != epoch)
                    return false;
            }
            return true;
        }

        World&                                            m_world;
        NGIN::Containers::Vector<TypeId>                  m_required;
        NGIN::Containers::Vector<TypeId>                  m_with;
        NGIN::Containers::Vector<TypeId>                  m_without;
        NGIN::Containers::Vector<TypeId>                  m_writes;
        NGIN::Containers::Vector<TypeId>                  m_changed;
        NGIN::Containers::Vector<TypeId>                  m_added;
    };
}
