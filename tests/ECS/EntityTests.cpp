/// @file EntityTests.cpp
/// @brief Tests for entity IDs and allocator semantics.

#include <boost/ut.hpp>

#include <NGIN/ECS/Entity.hpp>
#include <NGIN/ECS/World.hpp>

using namespace boost::ut;

suite<"NGIN::ECS::Entity"> entitySuite = [] {
  "PackUnpack"_test = [] {
    const auto id = NGIN::ECS::MakeEntityId(0x1234ABCDULL, static_cast<NGIN::UInt16>(42));
    expect(eq(NGIN::ECS::GetEntityIndex(id), 0x1234ABCDULL)) << "Index mismatch";
    expect(eq(static_cast<NGIN::UInt16>(NGIN::ECS::GetEntityGeneration(id)), static_cast<NGIN::UInt16>(42))) << "Gen mismatch";
  };

  "Allocator_Create_Destroy_Recycle"_test = [] {
    NGIN::ECS::World world;
    const auto e1 = world.Spawn();
    const auto e2 = world.Spawn();
    expect(world.IsAlive(e1));
    expect(world.IsAlive(e2));
    expect(eq(NGIN::ECS::GetEntityGeneration(e1), static_cast<NGIN::UInt16>(1)));
    expect(eq(NGIN::ECS::GetEntityGeneration(e2), static_cast<NGIN::UInt16>(1)));
    expect(eq(NGIN::ECS::GetEntityIndex(e1), 0ULL));
    expect(eq(NGIN::ECS::GetEntityIndex(e2), 1ULL));
    expect(eq(world.AliveCount(), 2ULL));

    world.Despawn(e1);
    expect(!world.IsAlive(e1));
    expect(eq(world.AliveCount(), 1ULL));

    const auto e3 = world.Spawn();
    // Should recycle index 0 with bumped generation
    expect(eq(NGIN::ECS::GetEntityIndex(e3), 0ULL));
    expect(eq(NGIN::ECS::GetEntityGeneration(e3), static_cast<NGIN::UInt16>(2)));
    expect(world.IsAlive(e3));
    expect(!world.IsAlive(e1)); // stale id
    expect(eq(world.AliveCount(), 2ULL));
  };
};

