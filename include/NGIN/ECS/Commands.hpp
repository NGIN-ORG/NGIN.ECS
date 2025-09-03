#pragma once

#include <NGIN/ECS/Export.hpp>
#include <NGIN/ECS/World.hpp>
#include <NGIN/Containers/Vector.hpp>

#include <functional>

namespace NGIN::ECS
{
    /// @brief MVP command buffer: defers structural ops; flush applies to World.
    class NGIN_ECS_API Commands
    {
    public:
        Commands() = default;

        template<typename... Cs>
        void Spawn(const Cs&... comps)
        {
            m_ops.EmplaceBack([=](World& w) {
                (void)w.Spawn(comps...);
            });
        }

        void Despawn(EntityId id)
        {
            m_ops.EmplaceBack([=](World& w) {
                w.Despawn(id);
            });
        }

        void Clear() { m_ops.Clear(); }

        void Flush(World& world)
        {
            for (auto& op : m_ops)
            {
                op(world);
            }
            m_ops.Clear();
        }

        [[nodiscard]] NGIN::UIntSize Size() const noexcept { return m_ops.Size(); }

    private:
        NGIN::Containers::Vector<std::function<void(World&)>> m_ops;
    };
}

