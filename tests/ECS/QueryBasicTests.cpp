/// @file QueryBasicTests.cpp
/// @brief Basic query iteration over chunks: Read<Velocity>, Write<Transform>.

#include <boost/ut.hpp>

#include <NGIN/ECS/World.hpp>
#include <NGIN/ECS/Query.hpp>

using namespace boost::ut;

namespace
{
    struct Transform { float x, y, z; };
    struct Velocity { float vx, vy, vz; };
}

suite<"NGIN::ECS::Query"> querySuite = [] {
  "Move_System_Updates_Transforms"_test = [] {
    NGIN::ECS::World world;
    const std::size_t N = 1024;
    for (std::size_t i = 0; i < N; ++i)
    {
        Transform t{float(i), 0.0f, 0.0f};
        Velocity v{1.0f, 2.0f, 3.0f};
        (void)world.Spawn(t, v);
    }

    NGIN::ECS::Query<NGIN::ECS::Read<Velocity>, NGIN::ECS::Write<Transform>> q{world};
    const float dt = 2.0f;
    q.for_chunks([&](const NGIN::ECS::ChunkView& chunk){
      const auto* v = chunk.read<Velocity>();
      auto*       t = chunk.write<Transform>();
      for (NGIN::UIntSize i = chunk.begin(); i < chunk.end(); ++i)
      {
        t[i].x += v[i].vx * dt;
        t[i].y += v[i].vy * dt;
        t[i].z += v[i].vz * dt;
      }
    });

    // Validate via a read-only pass
    NGIN::ECS::Query<NGIN::ECS::Read<Transform>> qr{world};
    NGIN::UIntSize count = 0;
    bool ok = true;
    qr.for_chunks([&](const NGIN::ECS::ChunkView& chunk){
      const auto* t = chunk.read<Transform>();
      for (NGIN::UIntSize i = chunk.begin(); i < chunk.end(); ++i)
      {
        const auto base = float(count + i);
        ok = ok && (t[i].x == base + 1.0f * dt) && (t[i].y == 0.0f + 2.0f * dt) && (t[i].z == 0.0f + 3.0f * dt);
      }
      count += (chunk.end() - chunk.begin());
    });
    expect(ok) << "Transform updates incorrect";
    expect(eq(count, static_cast<NGIN::UIntSize>(N)));
  };
};

