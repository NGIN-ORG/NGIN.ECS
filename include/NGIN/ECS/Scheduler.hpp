#pragma once

#include <NGIN/ECS/Export.hpp>
#include <NGIN/ECS/World.hpp>
#include <NGIN/ECS/TypeRegistry.hpp>
#include <NGIN/ECS/Query.hpp>
#include <NGIN/ECS/Commands.hpp>
#include <NGIN/Containers/Vector.hpp>
#include <NGIN/Meta/FunctionTraits.hpp>

#include <algorithm>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace NGIN::ECS
{
    class ExclusiveWorld
    {
    public:
        explicit ExclusiveWorld(World& world) noexcept
            : m_world(&world)
        {
        }

        [[nodiscard]] World& Get() const noexcept { return *m_world; }
        [[nodiscard]] World* operator->() const noexcept { return m_world; }
        [[nodiscard]] World& operator*() const noexcept { return *m_world; }

    private:
        World* m_world {nullptr};
    };

    struct SystemDescriptor
    {
        const char*                                      Name {"System"};
        NGIN::Containers::Vector<TypeId>                 Reads;
        NGIN::Containers::Vector<TypeId>                 Writes;
        bool                                             Exclusive {false};
        std::function<void(World&, Commands&, NGIN::UInt64 sinceTick)> Run;
        NGIN::UInt64                                     LastRunTick {0};
    };

    namespace detail
    {
        inline void AppendUnique(NGIN::Containers::Vector<TypeId>& destination,
                                 const NGIN::Containers::Vector<TypeId>& source)
        {
            for (NGIN::UIntSize index = 0; index < source.Size(); ++index)
            {
                destination.EmplaceBack(source[index]);
            }
            SortUnique(destination);
        }

        template<typename Arg>
        struct SystemParamBinder;

        template<typename... Terms>
        struct SystemParamBinder<Query<Terms...>>
        {
            using StorageType = Query<Terms...>;

            static void Describe(SystemDescriptor& descriptor)
            {
                const auto metadata = BuildQueryMetadata<Terms...>();
                AppendUnique(descriptor.Reads, metadata.Reads);
                AppendUnique(descriptor.Writes, metadata.Writes);
            }

            static StorageType Create(World& world, Commands&, NGIN::UInt64 sinceTick)
            {
                return StorageType {world, sinceTick};
            }
        };

        template<typename... Terms>
        struct SystemParamBinder<Query<Terms...>&> : SystemParamBinder<Query<Terms...>>
        {
        };

        template<typename... Terms>
        struct SystemParamBinder<const Query<Terms...>&> : SystemParamBinder<Query<Terms...>>
        {
        };

        template<>
        struct SystemParamBinder<Commands&>
        {
            using StorageType = std::reference_wrapper<Commands>;

            static void Describe(SystemDescriptor& descriptor)
            {
                descriptor.Exclusive = true;
            }

            static StorageType Create(World&, Commands& commands, NGIN::UInt64)
            {
                return std::ref(commands);
            }
        };

        template<>
        struct SystemParamBinder<ExclusiveWorld>
        {
            using StorageType = ExclusiveWorld;

            static void Describe(SystemDescriptor& descriptor)
            {
                descriptor.Exclusive = true;
            }

            static StorageType Create(World& world, Commands&, NGIN::UInt64)
            {
                return ExclusiveWorld {world};
            }
        };

        template<>
        struct SystemParamBinder<ExclusiveWorld&> : SystemParamBinder<ExclusiveWorld>
        {
        };

        template<>
        struct SystemParamBinder<const ExclusiveWorld&> : SystemParamBinder<ExclusiveWorld>
        {
        };

        template<typename Callable, typename Traits, std::size_t... Indices>
        void DescribeSystemArgs(SystemDescriptor& descriptor, std::index_sequence<Indices...>)
        {
            (SystemParamBinder<typename Traits::template ArgNType<Indices>>::Describe(descriptor), ...);
            SortUnique(descriptor.Reads);
            SortUnique(descriptor.Writes);
        }

        template<typename Callable, typename Traits, std::size_t... Indices>
        auto MakeBoundArgs(World& world,
                           Commands& commands,
                           NGIN::UInt64 sinceTick,
                           std::index_sequence<Indices...>)
        {
            return std::tuple<typename SystemParamBinder<typename Traits::template ArgNType<Indices>>::StorageType...> {
                SystemParamBinder<typename Traits::template ArgNType<Indices>>::Create(world, commands, sinceTick)...
            };
        }

        template<typename Callable, typename Traits, std::size_t... Indices>
        void InvokeSystem(Callable& callable,
                          World& world,
                          Commands& commands,
                          NGIN::UInt64 sinceTick,
                          std::index_sequence<Indices...>)
        {
            auto args = MakeBoundArgs<Callable, Traits>(world, commands, sinceTick, std::index_sequence<Indices...> {});
            std::apply([&](auto&&... boundArgs) {
                callable(std::forward<decltype(boundArgs)>(boundArgs)...);
            }, args);
        }

        template<typename Callable>
        [[nodiscard]] SystemDescriptor MakeSystemDescriptor(const char* name, Callable&& callable, bool forceExclusive)
        {
            using Fn     = std::decay_t<Callable>;
            using Traits = NGIN::Meta::FunctionTraits<Fn>;

            SystemDescriptor descriptor {};
            descriptor.Name = name;
            DescribeSystemArgs<Fn, Traits>(descriptor, std::make_index_sequence<Traits::NUM_ARGS> {});
            descriptor.Exclusive = descriptor.Exclusive || forceExclusive;

            descriptor.Run = [fn = std::forward<Callable>(callable)](World& world, Commands& commands, NGIN::UInt64 sinceTick) mutable {
                InvokeSystem<Fn, Traits>(fn,
                                         world,
                                         commands,
                                         sinceTick,
                                         std::make_index_sequence<Traits::NUM_ARGS> {});
            };
            return descriptor;
        }
    }// namespace detail

    template<typename Callable>
    [[nodiscard]] inline SystemDescriptor MakeSystem(const char* name, Callable&& callable)
    {
        return detail::MakeSystemDescriptor(name, std::forward<Callable>(callable), false);
    }

    template<typename Callable>
    [[nodiscard]] inline SystemDescriptor MakeExclusiveSystem(const char* name, Callable&& callable)
    {
        return detail::MakeSystemDescriptor(name, std::forward<Callable>(callable), true);
    }

    class NGIN_ECS_API Scheduler
    {
    public:
        Scheduler() = default;

        NGIN::UInt32 Register(const SystemDescriptor& system)
        {
            const auto id = static_cast<NGIN::UInt32>(m_systems.Size());
            m_systems.EmplaceBack(system);
            return id;
        }

        void Build()
        {
            m_stages.clear();
            m_stageBySystem.clear();
            m_stageBySystem.resize(m_systems.Size(), 0);

            for (NGIN::UIntSize systemIndex = 0; systemIndex < m_systems.Size(); ++systemIndex)
            {
                int stageIndex = 0;
                for (NGIN::UIntSize previousIndex = 0; previousIndex < systemIndex; ++previousIndex)
                {
                    if (Conflicts(m_systems[previousIndex], m_systems[systemIndex]) ||
                        m_systems[previousIndex].Exclusive || m_systems[systemIndex].Exclusive)
                    {
                        stageIndex = std::max(stageIndex, m_stageBySystem[previousIndex] + 1);
                    }
                }

                if (m_stages.size() <= static_cast<std::size_t>(stageIndex))
                {
                    m_stages.resize(static_cast<std::size_t>(stageIndex + 1));
                }

                m_stageBySystem[systemIndex] = stageIndex;
                m_stages[static_cast<std::size_t>(stageIndex)].push_back(static_cast<int>(systemIndex));
            }
        }

        void Run(World& world)
        {
            world.NextEpoch();
            Commands commands;
            for (auto& stage : m_stages)
            {
                for (const int systemIndex : stage)
                {
                    auto& system = m_systems[static_cast<NGIN::UIntSize>(systemIndex)];
                    if (system.Run)
                    {
                        system.Run(world, commands, system.LastRunTick);
                        system.LastRunTick = world.CurrentEpoch();
                    }
                }
                commands.Flush(world);
            }
        }

        [[nodiscard]] NGIN::UIntSize StageCount() const noexcept
        {
            return m_stages.size();
        }

        [[nodiscard]] const std::vector<int>& StageAt(NGIN::UIntSize stageIndex) const
        {
            return m_stages[stageIndex];
        }

    private:
        [[nodiscard]] static bool Intersects(const NGIN::Containers::Vector<TypeId>& left,
                                             const NGIN::Containers::Vector<TypeId>& right)
        {
            for (NGIN::UIntSize leftIndex = 0; leftIndex < left.Size(); ++leftIndex)
            {
                for (NGIN::UIntSize rightIndex = 0; rightIndex < right.Size(); ++rightIndex)
                {
                    if (left[leftIndex] == right[rightIndex])
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        [[nodiscard]] static bool Conflicts(const SystemDescriptor& left, const SystemDescriptor& right)
        {
            return Intersects(left.Writes, right.Writes) ||
                   Intersects(left.Writes, right.Reads) ||
                   Intersects(left.Reads, right.Writes);
        }

    private:
        NGIN::Containers::Vector<SystemDescriptor> m_systems;
        std::vector<int>                           m_stageBySystem;
        std::vector<std::vector<int>>              m_stages;
    };
}
