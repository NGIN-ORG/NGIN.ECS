#pragma once

#include <NGIN/ECS/Export.hpp>
#include <NGIN/ECS/World.hpp>
#include <NGIN/Containers/Vector.hpp>

#include <cstddef>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

namespace NGIN::ECS
{
    class NGIN_ECS_API Commands
    {
    public:
        Commands() = default;
        Commands(const Commands&)            = delete;
        Commands& operator=(const Commands&) = delete;
        Commands(Commands&&)                 = delete;
        Commands& operator=(Commands&&)      = delete;

        ~Commands()
        {
            Clear();
        }

        template<typename... Cs>
        void Spawn(Cs&&... components)
        {
            using Operation = SpawnOperation<std::decay_t<Cs>...>;
            StoreOperation<Operation>(std::forward<Cs>(components)...);
        }

        void Despawn(EntityId entityId)
        {
            StoreOperation<DespawnOperation>(entityId);
        }

        template<typename T, typename U>
        void Add(EntityId entityId, U&& value)
        {
            using Operation = AddOperation<T, std::decay_t<U>>;
            StoreOperation<Operation>(entityId, std::forward<U>(value));
        }

        template<typename T>
        void Remove(EntityId entityId)
        {
            using Operation = RemoveOperation<T>;
            StoreOperation<Operation>(entityId);
        }

        template<typename T, typename U>
        void Set(EntityId entityId, U&& value)
        {
            using Operation = SetOperation<T, std::decay_t<U>>;
            StoreOperation<Operation>(entityId, std::forward<U>(value));
        }

        void ClearWorld()
        {
            StoreOperation<ClearWorldOperation>();
        }

        void Clear() noexcept
        {
            for (NGIN::UIntSize index = 0; index < m_records.Size(); ++index)
            {
                m_records[index].Destroy(m_storage.data() + m_records[index].Offset);
            }
            m_records.Clear();
            m_storage.Clear();
        }

        void Flush(World& world)
        {
            for (NGIN::UIntSize index = 0; index < m_records.Size(); ++index)
            {
                auto& record = m_records[index];
                void* payload = m_storage.data() + record.Offset;
                record.Apply(payload, world);
                record.Destroy(payload);
            }
            m_records.Clear();
            m_storage.Clear();
        }

        [[nodiscard]] NGIN::UIntSize Size() const noexcept { return m_records.Size(); }

    private:
        using ApplyFn   = void (*)(void* payload, World& world);
        using DestroyFn = void (*)(void* payload) noexcept;

        struct Record
        {
            NGIN::UIntSize Offset {0};
            ApplyFn        Apply {nullptr};
            DestroyFn      Destroy {nullptr};
        };

        template<typename... Cs>
        struct SpawnOperation
        {
            explicit SpawnOperation(Cs&&... components)
                : Components(std::forward<Cs>(components)...)
            {
            }

            std::tuple<Cs...> Components;
        };

        struct DespawnOperation
        {
            explicit DespawnOperation(EntityId entityId)
                : Entity(entityId)
            {
            }

            EntityId Entity {NullEntityId};
        };

        template<typename T, typename U>
        struct AddOperation
        {
            AddOperation(EntityId entityId, U&& value)
                : Entity(entityId), Value(std::forward<U>(value))
            {
            }

            EntityId Entity {NullEntityId};
            U        Value;
        };

        template<typename T>
        struct RemoveOperation
        {
            explicit RemoveOperation(EntityId entityId)
                : Entity(entityId)
            {
            }

            EntityId Entity {NullEntityId};
        };

        template<typename T, typename U>
        struct SetOperation
        {
            SetOperation(EntityId entityId, U&& value)
                : Entity(entityId), Value(std::forward<U>(value))
            {
            }

            EntityId Entity {NullEntityId};
            U        Value;
        };

        struct ClearWorldOperation
        {
        };

        template<typename Operation>
        struct OperationInvoker;

        template<typename Operation>
        static void DestroyPayload(void* payload) noexcept
        {
            static_cast<Operation*>(payload)->~Operation();
        }

        template<typename Operation, typename... Args>
        void StoreOperation(Args&&... args)
        {
            const auto offset = Allocate(sizeof(Operation), alignof(Operation));
            void* payload = m_storage.data() + offset;
            ::new (payload) Operation(std::forward<Args>(args)...);

            Record record {};
            record.Offset = offset;
            record.Apply = &OperationInvoker<Operation>::Apply;
            record.Destroy = &DestroyPayload<Operation>;
            m_records.EmplaceBack(record);
        }

        [[nodiscard]] NGIN::UIntSize Allocate(NGIN::UIntSize size, NGIN::UIntSize alignment)
        {
            NGIN::UIntSize offset = m_storage.Size();
            const auto mask = alignment - 1;
            if (alignment > 1 && (offset & mask) != 0)
            {
                offset += alignment - (offset & mask);
            }
            while (m_storage.Size() < offset + size)
            {
                m_storage.EmplaceBack(std::byte {});
            }
            return offset;
        }

    private:
        NGIN::Containers::Vector<Record>      m_records;
        NGIN::Containers::Vector<std::byte>   m_storage;
    };

    template<typename... Cs>
    struct Commands::OperationInvoker<Commands::SpawnOperation<Cs...>>
    {
        static void Apply(void* payload, World& world)
        {
            auto& operation = *static_cast<Commands::SpawnOperation<Cs...>*>(payload);
            std::apply([&](auto&... components) {
                (void)world.Spawn(std::move(components)...);
            }, operation.Components);
        }
    };

    template<>
    struct Commands::OperationInvoker<Commands::DespawnOperation>
    {
        static void Apply(void* payload, World& world)
        {
            world.Despawn(static_cast<Commands::DespawnOperation*>(payload)->Entity);
        }
    };

    template<typename T, typename U>
    struct Commands::OperationInvoker<Commands::AddOperation<T, U>>
    {
        static void Apply(void* payload, World& world)
        {
            auto& operation = *static_cast<Commands::AddOperation<T, U>*>(payload);
            world.template Add<T>(operation.Entity, std::move(operation.Value));
        }
    };

    template<typename T>
    struct Commands::OperationInvoker<Commands::RemoveOperation<T>>
    {
        static void Apply(void* payload, World& world)
        {
            (void)world.template Remove<T>(static_cast<Commands::RemoveOperation<T>*>(payload)->Entity);
        }
    };

    template<typename T, typename U>
    struct Commands::OperationInvoker<Commands::SetOperation<T, U>>
    {
        static void Apply(void* payload, World& world)
        {
            auto& operation = *static_cast<Commands::SetOperation<T, U>*>(payload);
            world.template Set<T>(operation.Entity, std::move(operation.Value));
        }
    };

    template<>
    struct Commands::OperationInvoker<Commands::ClearWorldOperation>
    {
        static void Apply(void*, World& world)
        {
            world.Clear();
        }
    };
}
