/// @file ChangeDetectionTests.cpp
/// @brief Tests for Added<> and Changed<> filters with epoch-based clocks.

#include <boost/ut.hpp>

#include <NGIN/ECS/World.hpp>
#include <NGIN/ECS/Query.hpp>

using namespace boost::ut;

namespace
{
    struct Transform { float x, y, z; };
    struct Velocity { float vx, vy, vz; };
    struct Tag {};
}

suite<"NGIN::ECS::ChangeDetection"> changeSuite = [] {
  "Added_Matches_SameEpoch_ThenClears"_test = [] {
    NGIN::ECS::World world;
    (void)world.Spawn(Tag{});

    NGIN::ECS::Query<NGIN::ECS::Added<Tag>> q{world};
    NGIN::UIntSize hits = 0;
    q.for_chunks([&](const NGIN::ECS::ChunkView& ch){ hits += (ch.end() - ch.begin()); });
    expect(hits > 0_u);

    world.NextEpoch();
    hits = 0;
    q.for_chunks([&](const NGIN::ECS::ChunkView& ch){ hits += (ch.end() - ch.begin()); });
    expect(eq(hits, 0_u));
  };

  "Changed_Matches_SameEpoch_ThenClears"_test = [] {
    NGIN::ECS::World world;
    const int N = 128;
    for (int i = 0; i < N; ++i)
    {
        (void)world.Spawn(Transform{float(i), 0.0f, 0.0f}, Velocity{1.0f, 0.0f, 0.0f});
    }

    // Write pass to bump write versions for Transform
    NGIN::ECS::Query<NGIN::ECS::Read<Velocity>, NGIN::ECS::Write<Transform>> qw{world};
    const float dt = 1.0f;
    qw.for_chunks([&](const NGIN::ECS::ChunkView& ch){
      const auto* v = ch.read<Velocity>();
      auto*       t = ch.write<Transform>();
      for (NGIN::UIntSize i = ch.begin(); i < ch.end(); ++i)
      {
        t[i].x += v[i].vx * dt;
      }
    });

    // Now Changed<Transform> should match in this epoch
    NGIN::ECS::Query<NGIN::ECS::Changed<Transform>> qc{world};
    NGIN::UIntSize changedHits = 0;
    qc.for_chunks([&](const NGIN::ECS::ChunkView& ch){ changedHits += (ch.end() - ch.begin()); });
    expect(changedHits > 0_u);

    world.NextEpoch();
    changedHits = 0;
    qc.for_chunks([&](const NGIN::ECS::ChunkView& ch){ changedHits += (ch.end() - ch.begin()); });
    expect(eq(changedHits, 0_u));
  };
};

