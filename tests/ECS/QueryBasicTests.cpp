/// @file QueryBasicTests.cpp
/// @brief Query iteration, optional terms, and entity-aware access.

#include <boost/ut.hpp>

#include <NGIN/ECS/World.hpp>
#include <NGIN/ECS/Query.hpp>

using namespace boost::ut;

namespace
{
    struct Transform
    {
        float x, y, z;
    };

    struct Velocity
    {
        float vx, vy, vz;
    };

    struct PlayerTag
    {
    };

    struct Disabled
    {
    };
}

suite<"NGIN::ECS::Query"> querySuite = [] {
  "Move_Query_Updates_Transforms_And_Exposes_Entity"_test = [] {
    NGIN::ECS::World world;
    NGIN::Containers::Vector<NGIN::ECS::EntityId> entities;

    constexpr NGIN::UIntSize N = 128;
    for (NGIN::UIntSize i = 0; i < N; ++i)
    {
        entities.EmplaceBack(world.Spawn(Transform{float(i), 0.0f, 0.0f}, Velocity{1.0f, 2.0f, 3.0f}));
    }

    NGIN::ECS::Query<NGIN::ECS::Write<Transform>, NGIN::ECS::Read<Velocity>> query {world};
    query.ForEach([&](const NGIN::ECS::RowView& row) {
      auto&       transform = row.Write<Transform>();
      const auto& velocity  = row.Read<Velocity>();
      transform.x += velocity.vx * 2.0f;
      transform.y += velocity.vy * 2.0f;
      transform.z += velocity.vz * 2.0f;
      row.MarkChanged<Transform>();
      expect(world.IsAlive(row.Entity()));
    });

    bool ok = true;
    for (NGIN::UIntSize i = 0; i < entities.Size(); ++i)
    {
        const auto& transform = world.Get<Transform>(entities[i]);
        ok = ok &&
             transform.x == float(i) + 2.0f &&
             transform.y == 4.0f &&
             transform.z == 6.0f;
    }
    expect(ok);
  };

  "Optional_And_WithWithout_Terms_Filter_Correctly"_test = [] {
    NGIN::ECS::World world;

    const auto withVelocity    = world.Spawn(Transform{1, 0, 0}, Velocity{1, 0, 0}, PlayerTag{});
    const auto withoutVelocity = world.Spawn(Transform{2, 0, 0}, PlayerTag{});
    (void)world.Spawn(Transform{3, 0, 0}, Velocity{1, 0, 0}, PlayerTag{}, Disabled{});

    NGIN::UIntSize matched = 0;
    NGIN::UIntSize withOptionalVelocity = 0;
    NGIN::UIntSize withoutOptionalVelocity = 0;

    NGIN::ECS::Query<
        NGIN::ECS::Read<Transform>,
        NGIN::ECS::Opt<Velocity>,
        NGIN::ECS::With<PlayerTag>,
        NGIN::ECS::Without<Disabled>
    > query {world};

    query.ForChunks([&](const NGIN::ECS::ChunkView& chunk) {
      for (NGIN::UIntSize rowIndex = 0; rowIndex < chunk.Count(); ++rowIndex)
      {
        ++matched;
        const auto entity = chunk.EntityAt(rowIndex);
        const auto* velocity = chunk.TryRead<Velocity>(rowIndex);
        if (velocity)
        {
            ++withOptionalVelocity;
            expect(entity == withVelocity);
        }
        else
        {
            ++withoutOptionalVelocity;
            expect(entity == withoutVelocity);
        }
      }
    });

    expect(eq(matched, 2_u));
    expect(eq(withOptionalVelocity, 1_u));
    expect(eq(withoutOptionalVelocity, 1_u));
  };
};
