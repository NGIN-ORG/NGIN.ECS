/// @file CommandsTests.cpp
/// @brief Tests for deferred spawns via Commands.

#include <boost/ut.hpp>

#include <NGIN/ECS/World.hpp>
#include <NGIN/ECS/Commands.hpp>

using namespace boost::ut;

namespace
{
    struct T { int a; };
    struct U { float b; };
}

suite<"NGIN::ECS::Commands"> commandsSuite = [] {
  "Deferred_Spawn_Flush_Applies"_test = [] {
    NGIN::ECS::World    world;
    NGIN::ECS::Commands cmd;

    const int N = 100;
    for (int i = 0; i < N; ++i)
    {
        cmd.Spawn(T{i}, U{float(i) * 0.5f});
    }
    expect(eq(world.AliveCount(), 0_u));
    expect(eq(cmd.Size(), static_cast<NGIN::UIntSize>(N)));

    cmd.Flush(world);
    expect(eq(world.AliveCount(), static_cast<NGIN::UInt64>(N)));
    expect(eq(cmd.Size(), 0_u));

    // flush again should be no-op
    cmd.Flush(world);
    expect(eq(world.AliveCount(), static_cast<NGIN::UInt64>(N)));
  };
};

