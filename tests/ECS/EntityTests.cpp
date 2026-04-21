/// @file EntityTests.cpp
/// @brief Tests for entity IDs, liveness, and immediate storage removal.

#include <boost/ut.hpp>

#include <NGIN/ECS/Entity.hpp>
#include <NGIN/ECS/World.hpp>
#include <NGIN/ECS/Query.hpp>

using namespace boost::ut;

namespace
{
    struct Transform
    {
        int value;
    };

    static_assert(!std::is_copy_constructible_v<NGIN::ECS::World>);
    static_assert(!std::is_copy_assignable_v<NGIN::ECS::World>);
}

suite<"NGIN::ECS::Entity"> entitySuite = [] {
  "PackUnpack"_test = [] {
    const auto id = NGIN::ECS::MakeEntityId(0x1234ABCDULL, static_cast<NGIN::UInt16>(42));
    expect(eq(NGIN::ECS::GetEntityIndex(id), 0x1234ABCDULL));
    expect(eq(static_cast<NGIN::UInt16>(NGIN::ECS::GetEntityGeneration(id)), static_cast<NGIN::UInt16>(42)));
  };

  "Allocator_Create_Destroy_Recycle"_test = [] {
    NGIN::ECS::World world;

    const auto e1 = world.Spawn();
    const auto e2 = world.Spawn();
    expect(world.IsAlive(e1));
    expect(world.IsAlive(e2));
    expect(eq(world.AliveCount(), 2ULL));

    world.Despawn(e1);
    expect(!world.IsAlive(e1));
    expect(eq(world.AliveCount(), 1ULL));

    const auto e3 = world.Spawn();
    expect(eq(NGIN::ECS::GetEntityIndex(e3), 0ULL));
    expect(eq(NGIN::ECS::GetEntityGeneration(e3), static_cast<NGIN::UInt16>(2)));
    expect(world.IsAlive(e3));
    expect(!world.IsAlive(e1));
  };

  "Despawn_Removes_Row_Immediately"_test = [] {
    NGIN::ECS::World world;

    const auto dead  = world.Spawn(Transform{1});
    const auto alive = world.Spawn(Transform{2});
    world.Despawn(dead);

    NGIN::UIntSize count = 0;
    bool sawAlive = false;
    bool sawDead  = false;
    NGIN::ECS::Query<NGIN::ECS::Read<Transform>> query {world};
    query.ForEach([&](const NGIN::ECS::RowView& row) {
      ++count;
      sawAlive = sawAlive || row.Entity() == alive;
      sawDead  = sawDead || row.Entity() == dead;
    });

    expect(eq(count, 1_u));
    expect(sawAlive);
    expect(!sawDead);
    expect(!world.TryGet<Transform>(dead));
    expect(world.TryGet<Transform>(alive) != nullptr);
  };

  "Clear_Removes_All_Storage"_test = [] {
    NGIN::ECS::World world;

    (void)world.Spawn(Transform{1});
    (void)world.Spawn(Transform{2});
    expect(world.DebugGetChunkCount<Transform>() > 0_u);

    world.Clear();

    NGIN::UIntSize count = 0;
    NGIN::ECS::Query<NGIN::ECS::Read<Transform>> query {world};
    query.ForEach([&](const NGIN::ECS::RowView&) { ++count; });

    expect(eq(world.AliveCount(), 0ULL));
    expect(eq(world.DebugGetChunkCount<Transform>(), 0_u));
    expect(eq(count, 0_u));
  };
};
