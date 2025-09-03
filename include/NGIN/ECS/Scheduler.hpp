#pragma once

#include <NGIN/ECS/Export.hpp>
#include <NGIN/ECS/World.hpp>
#include <NGIN/ECS/TypeRegistry.hpp>
#include <NGIN/ECS/Query.hpp>
#include <NGIN/ECS/Commands.hpp>
#include <NGIN/Containers/Vector.hpp>

#include <functional>

namespace NGIN::ECS
{
    struct SystemDescriptor
    {
        const char*                                       Name {"System"};
        NGIN::Containers::Vector<TypeId>                  Reads;
        NGIN::Containers::Vector<TypeId>                  Writes;
        std::function<void(World&, Commands&)>            Run;
    };

    namespace detail
    {
        template<typename Term>
        struct CollectRW
        {
            static void Add(SystemDescriptor&) {}
        };
        template<typename T>
        struct CollectRW<Read<T>>
        {
            static void Add(SystemDescriptor& d) { d.Reads.EmplaceBack(GetTypeId<T>()); }
        };
        template<typename T>
        struct CollectRW<Write<T>>
        {
            static void Add(SystemDescriptor& d) { d.Writes.EmplaceBack(GetTypeId<T>()); }
        };
    } // namespace detail

    template<typename... Terms, typename Fn>
    [[nodiscard]] inline SystemDescriptor MakeSystem(const char* name, Fn&& fn)
    {
        SystemDescriptor d {};
        d.Name = name;
        (detail::CollectRW<Terms>::Add(d), ...);
        d.Run = std::forward<Fn>(fn);
        return d;
    }

    class NGIN_ECS_API Scheduler
    {
    public:
        Scheduler() = default;

        NGIN::UInt32 Register(const SystemDescriptor& sys)
        {
            const auto id = static_cast<NGIN::UInt32>(m_systems.Size());
            m_systems.EmplaceBack(sys);
            return id;
        }

        void Build()
        {
            const auto N = m_systems.Size();
            m_edges.clear();
            m_edges.resize(N);
            m_inDegree.clear();
            m_inDegree.resize(N, 0);

            auto conflicts = [&](const SystemDescriptor& a, const SystemDescriptor& b) {
                // Returns true if a must precede b due to write->read or write->write
                // Edge direction: writer -> other
                auto hasWriteOn = [&](TypeId t, const NGIN::Containers::Vector<TypeId>& set) {
                    for (NGIN::UIntSize i = 0; i < set.Size(); ++i)
                        if (set[i] == t) return true;
                    return false;
                };
                // a writes vs b reads/writes
                for (NGIN::UIntSize i = 0; i < a.Writes.Size(); ++i)
                {
                    const auto t = a.Writes[i];
                    if (hasWriteOn(t, b.Writes)) return true;
                    if (hasWriteOn(t, b.Reads)) return true;
                }
                return false;
            };

            for (NGIN::UIntSize i = 0; i < N; ++i)
            {
                for (NGIN::UIntSize j = 0; j < N; ++j)
                {
                    if (i == j) continue;
                    const auto& A = m_systems[i];
                    const auto& B = m_systems[j];
                    if (conflicts(A, B))
                    {
                        // A -> B
                        m_edges[i].push_back(static_cast<int>(j));
                        ++m_inDegree[j];
                    }
                    else if (conflicts(B, A))
                    {
                        // B -> A (we'll add when visiting j,i)
                    }
                    else
                    {
                        // No direct dependency
                    }
                }
            }

            // Kahn's algorithm; also collect stages (systems whose in-degree becomes zero together form a stage)
            m_stages.clear();
            NGIN::Containers::Vector<int> zero;
            for (NGIN::UIntSize i = 0; i < N; ++i)
                if (m_inDegree[i] == 0)
                    zero.EmplaceBack(static_cast<int>(i));

            NGIN::Containers::Vector<int> current;
            NGIN::Containers::Vector<int> next;
            while (zero.Size() > 0)
            {
                // Stage is the current set of zero in-degree nodes, preserving registration order
                current = zero;
                zero.Clear();
                // Record stage
                m_stages.emplace_back();
                m_stages.back().assign(current.begin(), current.end());

                // Decrement neighbors
                for (NGIN::UIntSize idx = 0; idx < current.Size(); ++idx)
                {
                    int u = current[idx];
                    for (int v: m_edges[u])
                    {
                        if (--m_inDegree[v] == 0)
                            zero.EmplaceBack(v);
                    }
                }
            }
            // Note: cycles (e.g., write-write mutual) will keep some nodes with in-degree > 0.
            // We conservatively place remaining into final serial stage by registration order.
            NGIN::Containers::Vector<int> remaining;
            for (NGIN::UIntSize i = 0; i < N; ++i)
                if (m_inDegree[i] > 0)
                    remaining.EmplaceBack(static_cast<int>(i));
            if (remaining.Size() > 0)
            {
                m_stages.emplace_back();
                m_stages.back().assign(remaining.begin(), remaining.end());
            }
        }

        void Run(World& world)
        {
            Commands commands;
            for (auto& stage: m_stages)
            {
                for (int idx: stage)
                {
                    auto& sys = m_systems[static_cast<NGIN::UIntSize>(idx)];
                    if (sys.Run)
                        sys.Run(world, commands);
                }
                // Barrier: flush commands after stage
                commands.Flush(world);
            }
        }

        [[nodiscard]] NGIN::UIntSize StageCount() const noexcept { return m_stages.size(); }
        [[nodiscard]] const std::vector<int>& StageAt(NGIN::UIntSize i) const { return m_stages[i]; }

    private:
        NGIN::Containers::Vector<SystemDescriptor> m_systems;
        std::vector<std::vector<int>>              m_edges;
        std::vector<int>                           m_inDegree;
        std::vector<std::vector<int>>              m_stages;
    };
}
