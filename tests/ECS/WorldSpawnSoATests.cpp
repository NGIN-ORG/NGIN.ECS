/// @file WorldSpawnSoATests.cpp
/// @brief Tests for typed spawn into SoA archetypes and chunking.

#include <boost/ut.hpp>

#include <NGIN/ECS/World.hpp>

using namespace boost::ut;

namespace
{
    struct Transform { float x, y, z; };
    struct Velocity { float vx, vy, vz; };
    struct PlayerTag {};
}

suite<"NGIN::ECS::WorldSpawn"> spawnSuite = [] {
  "Spawn_Typed_Allocates_Chunks"_test = [] {
    NGIN::ECS::World world;
    // Create archetype by spawning one entity
    {
        Transform t{0.0f, 0.0f, 0.0f};
        Velocity v{1.0f, 0.0f, 0.0f};
        (void)world.Spawn(t, v, PlayerTag{});
    }
    const auto perChunk = world.DebugGetChunkRowCapacity<Transform, Velocity, PlayerTag>();
    expect(perChunk > 0_u);

    const auto N = perChunk * 2 + 3; // force at least 3 chunks eventually
    for (std::size_t i = 0; i < N; ++i)
    {
        Transform t{float(i), 0.0f, 0.0f};
        Velocity v{1.0f, 0.0f, 0.0f};
        auto e = world.Spawn(t, v, PlayerTag{});
        expect(world.IsAlive(e));
    }

    const auto chunks = world.DebugGetChunkCount<Transform, Velocity, PlayerTag>();
    expect(chunks >= 2_u) << "Expected at least 2 chunks";
  };
};
