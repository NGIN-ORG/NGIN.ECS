#include <NGIN/ECS/Entity.hpp>

namespace NGIN::ECS
{
    EntityId EntityAllocator::Create()
    {
        // Recycle if possible
        if (m_freeList.Size() > 0)
        {
            const auto freeIdx = m_freeList[m_freeList.Size() - 1];
            m_freeList.PopBack();
            const auto gen = m_generations[freeIdx];
            ++m_aliveCount;
            return MakeEntityId(freeIdx, gen);
        }
        // New index
        const NGIN::UInt64 index = m_generations.Size();
        // Start generation at 1 to avoid colliding with NullEntityId (0)
        m_generations.PushBack(static_cast<NGIN::UInt16>(1));
        ++m_aliveCount;
        return MakeEntityId(index, 1);
    }

    void EntityAllocator::Destroy(EntityId id)
    {
        if (IsNull(id))
            return;
        const auto index = GetEntityIndex(id);
        if (index >= m_generations.Size())
            return; // out of range -> ignore
        const auto currentGen = m_generations[index];
        if (currentGen != GetEntityGeneration(id))
            return; // stale id -> ignore

        // Bump generation (wrap permitted)
        m_generations[index] = static_cast<NGIN::UInt16>(currentGen + 1);
        m_freeList.PushBack(index);
        if (m_aliveCount > 0)
            --m_aliveCount;
    }

    bool EntityAllocator::IsAlive(EntityId id) const noexcept
    {
        if (IsNull(id))
            return false;
        const auto index = GetEntityIndex(id);
        if (index >= m_generations.Size())
            return false;
        return m_generations[index] == GetEntityGeneration(id);
    }

    void EntityAllocator::Clear() noexcept
    {
        m_generations.Clear();
        m_freeList.Clear();
        m_aliveCount = 0;
    }

    NGIN::UInt16 EntityAllocator::GenerationAtIndex(NGIN::UInt64 index) const noexcept
    {
        if (index >= m_generations.Size())
            return 0;
        return m_generations[index];
    }

} // namespace NGIN::ECS

