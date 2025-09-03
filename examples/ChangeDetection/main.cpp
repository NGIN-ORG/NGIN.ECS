#include <NGIN/ECS/World.hpp>
#include <NGIN/ECS/Query.hpp>
#include <iostream>

namespace Demo
{
    struct Tag {};
    struct Transform { float x, y, z; };
    struct Velocity  { float vx, vy, vz; };
}

int main()
{
    using namespace NGIN::ECS;
    using namespace Demo;

    World world;

    // Added<T> matches in the same epoch as spawn
    for (int i = 0; i < 5; ++i)
        (void)world.Spawn(Tag{});

    Query<Added<Tag>> qAdded{world};
    NGIN::UIntSize addedCount = 0;
    qAdded.for_chunks([&](const ChunkView& ch) { addedCount += (ch.end() - ch.begin()); });
    std::cout << "Added<Tag> in current epoch: " << static_cast<std::size_t>(addedCount) << "\n";

    // Advance epoch: Added<T> no longer matches
    world.NextEpoch();
    addedCount = 0;
    qAdded.for_chunks([&](const ChunkView& ch) { addedCount += (ch.end() - ch.begin()); });
    std::cout << "Added<Tag> next epoch: " << static_cast<std::size_t>(addedCount) << "\n";

    // Spawn Transform+Velocity, then perform a write pass that bumps write version
    for (int i = 0; i < 8; ++i)
        (void)world.Spawn(Transform{float(i), 0, 0}, Velocity{1, 0, 0});

    // Advance epoch so that write happens in a fresh epoch
    world.NextEpoch();
    Query<Write<Transform>, Read<Velocity>> move{world};
    const float dt = 1.0f;
    move.for_chunks([&](const ChunkView& ch){
        auto*       t = ch.write<Transform>();
        const auto* v = ch.read<Velocity>();
        for (NGIN::UIntSize i = ch.begin(); i < ch.end(); ++i)
            t[i].x += v[i].vx * dt;
    });

    // Changed<Transform> should match in the same epoch the write occurred
    Query<Changed<Transform>> qChanged{world};
    NGIN::UIntSize changedCount = 0;
    qChanged.for_chunks([&](const ChunkView& ch) { changedCount += (ch.end() - ch.begin()); });
    std::cout << "Changed<Transform> in write epoch: " << static_cast<std::size_t>(changedCount) << "\n";

    world.NextEpoch();
    changedCount = 0;
    qChanged.for_chunks([&](const ChunkView& ch) { changedCount += (ch.end() - ch.begin()); });
    std::cout << "Changed<Transform> next epoch: " << static_cast<std::size_t>(changedCount) << "\n";

    return 0;
}
