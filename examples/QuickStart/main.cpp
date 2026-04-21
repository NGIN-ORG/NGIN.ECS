#include <NGIN/ECS/World.hpp>
#include <NGIN/ECS/Query.hpp>
#include <NGIN/ECS/Scheduler.hpp>
#include <NGIN/ECS/Commands.hpp>

#include <cstddef>
#include <iostream>

namespace Demo
{
    struct Transform { float x, y, z; };
    struct Velocity  { float vx, vy, vz; };
    struct PlayerTag {};
}

int main()
{
    using namespace NGIN::ECS;
    using namespace Demo;

    World world;

    for (int i = 0; i < 256; ++i)
    {
        (void)world.Spawn(Transform{float(i), 0.0f, 0.0f}, Velocity{1.0f, 2.0f, 3.0f}, PlayerTag{});
    }

    Query<Write<Transform>, Read<Velocity>> moveQuery {world};
    moveQuery.ForEach([&](const RowView& row) {
      auto&       transform = row.Write<Transform>();
      const auto& velocity  = row.Read<Velocity>();
      transform.x += velocity.vx;
      transform.y += velocity.vy;
      transform.z += velocity.vz;
      row.MarkChanged<Transform>();
    });

    Scheduler scheduler;
    auto spawner = MakeSystem("Spawner", [](Commands& commands) {
      for (int i = 0; i < 10; ++i)
      {
          commands.Spawn(PlayerTag{});
      }
    });

    auto counter = MakeSystem("Counter", [](Query<Read<PlayerTag>>& query) {
      NGIN::UIntSize total = 0;
      query.ForEach([&](const RowView&) { ++total; });
      std::cout << "PlayerTag count: " << static_cast<std::size_t>(total) << "\n";
    });

    scheduler.Register(spawner);
    scheduler.Register(counter);
    scheduler.Build();
    scheduler.Run(world);

    return 0;
}
