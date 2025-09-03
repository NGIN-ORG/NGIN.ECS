#pragma once

#include <NGIN/ECS/Export.hpp>
#include <NGIN/Primitives.hpp>
#include <NGIN/Containers/Vector.hpp>

namespace NGIN::ECS
{
    using EntityId = NGIN::UInt64;

    // Layout: [gen:16][index:48]
    inline constexpr NGIN::UInt64 kEntityIndexBits      = 48ULL;
    inline constexpr NGIN::UInt64 kEntityGenerationBits = 16ULL;
    inline constexpr NGIN::UInt64 kEntityIndexMask      = (NGIN::UInt64(1) << kEntityIndexBits) - 1ULL; // low 48
    inline constexpr NGIN::UInt64 kEntityGenerationMask = (NGIN::UInt64(1) << kEntityGenerationBits) - 1ULL; // low 16
    inline constexpr NGIN::UInt64 kEntityGenerationShift = kEntityIndexBits;

    inline constexpr EntityId NullEntityId = 0ULL;

    [[nodiscard]] inline constexpr NGIN::UInt64 GetEntityIndex(EntityId id) noexcept
    {
        return id & kEntityIndexMask;
    }
    [[nodiscard]] inline constexpr NGIN::UInt16 GetEntityGeneration(EntityId id) noexcept
    {
        return static_cast<NGIN::UInt16>((id >> kEntityGenerationShift) & kEntityGenerationMask);
    }
    [[nodiscard]] inline constexpr EntityId MakeEntityId(NGIN::UInt64 index, NGIN::UInt16 generation) noexcept
    {
        return (static_cast<NGIN::UInt64>(generation) << kEntityGenerationShift) | (index & kEntityIndexMask);
    }
    [[nodiscard]] inline constexpr bool IsNull(EntityId id) noexcept { return id == NullEntityId; }

    /// @brief Free-list entity allocator with generation counters.
    class NGIN_ECS_API EntityAllocator
    {
    public:
        EntityAllocator() = default;

        [[nodiscard]] EntityId Create();
        void                    Destroy(EntityId id);
        [[nodiscard]] bool      IsAlive(EntityId id) const noexcept;
        [[nodiscard]] NGIN::UInt64 AliveCount() const noexcept { return m_aliveCount; }

        void Clear() noexcept;

        // Introspection helpers
        [[nodiscard]] NGIN::UInt16 GenerationAtIndex(NGIN::UInt64 index) const noexcept;

    private:
        NGIN::Containers::Vector<NGIN::UInt16> m_generations; // per-index generation
        NGIN::Containers::Vector<NGIN::UInt64> m_freeList;    // stack of free indices
        NGIN::UInt64                            m_aliveCount {0};
    };

}// namespace NGIN::ECS

