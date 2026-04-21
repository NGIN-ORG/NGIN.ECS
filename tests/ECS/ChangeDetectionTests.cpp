/// @file ChangeDetectionTests.cpp
/// @brief Row-granular Added<> and Changed<> query coverage.

#include <boost/ut.hpp>

#include <NGIN/ECS/World.hpp>
#include <NGIN/ECS/Query.hpp>

using namespace boost::ut;

namespace
{
    struct Tag
    {
    };

    struct Transform
    {
        int value;
    };
}

suite<"NGIN::ECS::ChangeDetection"> changeSuite = [] {
  "Added_Is_RowGranular_For_Mixed_Old_And_New_Rows"_test = [] {
    NGIN::ECS::World world;

    for (int i = 0; i < 8; ++i)
    {
        (void)world.Spawn(Tag{});
    }

    world.NextEpoch();
    for (int i = 0; i < 3; ++i)
    {
        (void)world.Spawn(Tag{});
    }

    NGIN::UIntSize recentAdds = 0;
    NGIN::ECS::Query<NGIN::ECS::Added<Tag>> recentQuery {world};
    recentQuery.ForEach([&](const NGIN::ECS::RowView&) { ++recentAdds; });
    expect(eq(recentAdds, 3_u));

    NGIN::UIntSize allAdds = 0;
    NGIN::ECS::Query<NGIN::ECS::Added<Tag>> sinceStartQuery {world, 0};
    sinceStartQuery.ForEach([&](const NGIN::ECS::RowView&) { ++allAdds; });
    expect(eq(allAdds, 11_u));
  };

  "Added_Tracks_Component_Add_Migration_Per_Entity"_test = [] {
    NGIN::ECS::World world;

    const auto untouched = world.Spawn(Transform{1});
    const auto tagged    = world.Spawn(Transform{2});

    world.NextEpoch();
    world.Add<Tag>(tagged, Tag{});

    NGIN::UIntSize addedCount = 0;
    bool sawTagged = false;
    bool sawUntouched = false;
    NGIN::ECS::Query<NGIN::ECS::Added<Tag>> query {world};
    query.ForEach([&](const NGIN::ECS::RowView& row) {
      ++addedCount;
      sawTagged = sawTagged || row.Entity() == tagged;
      sawUntouched = sawUntouched || row.Entity() == untouched;
    });

    expect(eq(addedCount, 1_u));
    expect(sawTagged);
    expect(!sawUntouched);
  };

  "Changed_Is_RowGranular_And_Explicit"_test = [] {
    NGIN::ECS::World world;
    NGIN::Containers::Vector<NGIN::ECS::EntityId> entities;

    for (int i = 0; i < 6; ++i)
    {
        entities.EmplaceBack(world.Spawn(Transform{i}));
    }

    world.NextEpoch();
    world.GetMut<Transform>(entities[1]).value += 10;
    world.MarkChanged<Transform>(entities[1]);
    world.GetMut<Transform>(entities[4]).value += 20;
    world.MarkChanged<Transform>(entities[4]);

    NGIN::UIntSize changedCount = 0;
    bool sawOne = false;
    bool sawFour = false;
    NGIN::ECS::Query<NGIN::ECS::Changed<Transform>> query {world};
    query.ForEach([&](const NGIN::ECS::RowView& row) {
      ++changedCount;
      sawOne  = sawOne  || row.Entity() == entities[1];
      sawFour = sawFour || row.Entity() == entities[4];
    });

    expect(eq(changedCount, 2_u));
    expect(sawOne);
    expect(sawFour);

    world.NextEpoch();
    changedCount = 0;
    NGIN::ECS::Query<NGIN::ECS::Changed<Transform>> nextFrameQuery {world};
    nextFrameQuery.ForEach([&](const NGIN::ECS::RowView&) { ++changedCount; });
    expect(eq(changedCount, 0_u));
  };
};
