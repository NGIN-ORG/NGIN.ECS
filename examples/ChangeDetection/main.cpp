#include <NGIN/ECS/World.hpp>
#include <NGIN/ECS/Query.hpp>

#include <iostream>

namespace Demo
{
    struct Tag {};
    struct Transform { float x, y, z; };
}

int main()
{
    using namespace NGIN::ECS;
    using namespace Demo;

    World world;

    for (int i = 0; i < 5; ++i)
    {
        (void)world.Spawn(Tag{});
    }

    Query<Added<Tag>> addedThisFrame {world};
    NGIN::UIntSize addedCount = 0;
    addedThisFrame.ForEach([&](const RowView&) { ++addedCount; });
    std::cout << "Added<Tag> this frame: " << static_cast<std::size_t>(addedCount) << "\n";

    world.NextEpoch();
    const auto entity = world.Spawn(Transform{1.0f, 0.0f, 0.0f});
    auto& transform = world.GetMut<Transform>(entity);
    transform.x += 2.0f;
    world.MarkChanged<Transform>(entity);

    Query<Changed<Transform>> changedThisFrame {world};
    NGIN::UIntSize changedCount = 0;
    changedThisFrame.ForEach([&](const RowView&) { ++changedCount; });
    std::cout << "Changed<Transform> this frame: " << static_cast<std::size_t>(changedCount) << "\n";

    world.NextEpoch();
    Query<Changed<Transform>> changedNextFrame {world};
    changedCount = 0;
    changedNextFrame.ForEach([&](const RowView&) { ++changedCount; });
    std::cout << "Changed<Transform> next frame: " << static_cast<std::size_t>(changedCount) << "\n";

    return 0;
}
