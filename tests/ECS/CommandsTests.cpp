/// @file CommandsTests.cpp
/// @brief Deferred structural changes and packed payload coverage.

#include <boost/ut.hpp>

#include <NGIN/ECS/World.hpp>
#include <NGIN/ECS/Commands.hpp>
#include <NGIN/ECS/Query.hpp>

#include <memory>

using namespace boost::ut;

namespace
{
    struct Position
    {
        int value;
    };

    struct Velocity
    {
        int value;
    };

    struct Tag
    {
    };

    struct MoveOnly
    {
        explicit MoveOnly(int input)
            : Value(std::make_unique<int>(input))
        {
        }

        MoveOnly(MoveOnly&&) noexcept            = default;
        MoveOnly& operator=(MoveOnly&&) noexcept = default;
        MoveOnly(const MoveOnly&)                = delete;
        MoveOnly& operator=(const MoveOnly&)     = delete;

        std::unique_ptr<int> Value;
    };
}

suite<"NGIN::ECS::Commands"> commandsSuite = [] {
  "Deferred_Spawn_Flush_Applies_MoveOnly"_test = [] {
    NGIN::ECS::World    world;
    NGIN::ECS::Commands commands;

    commands.Spawn(Position{1}, MoveOnly{7});
    expect(eq(world.AliveCount(), 0ULL));
    expect(eq(commands.Size(), 1_u));

    commands.Flush(world);

    expect(eq(world.AliveCount(), 1ULL));
    NGIN::UIntSize count = 0;
    NGIN::ECS::Query<NGIN::ECS::Read<Position>, NGIN::ECS::Read<MoveOnly>> query {world};
    query.ForEach([&](const NGIN::ECS::RowView& row) {
      ++count;
      expect(row.Read<Position>().value == 1_i);
      expect(*row.Read<MoveOnly>().Value == 7_i);
    });
    expect(eq(count, 1_u));
  };

  "Mixed_Commands_Preserve_Order"_test = [] {
    NGIN::ECS::World    world;
    NGIN::ECS::Commands commands;

    const auto entity = world.Spawn(Position{1});
    commands.Add<Velocity>(entity, Velocity{2});
    commands.Set<Position>(entity, Position{3});
    commands.Remove<Velocity>(entity);
    commands.Spawn(Tag{});
    commands.Flush(world);

    expect(world.IsAlive(entity));
    expect(world.Get<Position>(entity).value == 3_i);
    expect(!world.Has<Velocity>(entity));

    NGIN::UIntSize tagCount = 0;
    NGIN::ECS::Query<NGIN::ECS::Read<Tag>> query {world};
    query.ForEach([&](const NGIN::ECS::RowView&) { ++tagCount; });
    expect(eq(tagCount, 1_u));
  };

  "ClearWorld_Resets_Then_Later_Commands_Run"_test = [] {
    NGIN::ECS::World    world;
    NGIN::ECS::Commands commands;

    const auto entity = world.Spawn(Position{42});
    commands.ClearWorld();
    commands.Spawn(Tag{});
    commands.Flush(world);

    expect(!world.IsAlive(entity));
    expect(eq(world.AliveCount(), 1ULL));

    NGIN::UIntSize tagCount = 0;
    NGIN::ECS::Query<NGIN::ECS::Read<Tag>> query {world};
    query.ForEach([&](const NGIN::ECS::RowView&) { ++tagCount; });
    expect(eq(tagCount, 1_u));
  };
};
