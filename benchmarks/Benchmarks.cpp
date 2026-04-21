#include <NGIN/ECS/Commands.hpp>
#include <NGIN/ECS/Query.hpp>
#include <NGIN/ECS/World.hpp>
#include <NGIN/Containers/Vector.hpp>

#include <chrono>
#include <cstddef>
#include <iostream>
#include <string_view>

namespace
{
    struct Transform
    {
        float x, y, z;
    };

    struct Velocity
    {
        float x, y, z;
    };

    struct Tag
    {
    };

    template<typename Fn>
    void RunBenchmark(std::string_view name, Fn&& fn)
    {
        const auto begin = std::chrono::steady_clock::now();
        fn();
        const auto end = std::chrono::steady_clock::now();
        const auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
        std::cout << name << ": " << elapsedUs << " us\n";
    }
}

int main()
{
    using namespace NGIN::ECS;

    constexpr int entityCount = 100000;

    RunBenchmark("ngin.spawn", [&] {
      World world;
      for (int index = 0; index < entityCount; ++index)
      {
          (void)world.Spawn(Transform{float(index), 0.0f, 0.0f}, Velocity{1.0f, 2.0f, 3.0f}, Tag{});
      }
    });

    RunBenchmark("ngin.query", [&] {
      World world;
      for (int index = 0; index < entityCount; ++index)
      {
          (void)world.Spawn(Transform{float(index), 0.0f, 0.0f}, Velocity{1.0f, 2.0f, 3.0f});
      }

      Query<Write<Transform>, Read<Velocity>> query {world};
      query.ForEach([&](const RowView& row) {
        auto&       transform = row.Write<Transform>();
        const auto& velocity  = row.Read<Velocity>();
        transform.x += velocity.x;
        transform.y += velocity.y;
        transform.z += velocity.z;
        row.MarkChanged<Transform>();
      });
    });

    RunBenchmark("ngin.commands", [&] {
      World world;
      Commands commands;
      for (int index = 0; index < entityCount; ++index)
      {
          commands.Spawn(Transform{float(index), 0.0f, 0.0f}, Velocity{1.0f, 2.0f, 3.0f}, Tag{});
      }
      commands.Flush(world);
    });

    RunBenchmark("ngin.despawn", [&] {
      World world;
      NGIN::Containers::Vector<EntityId> entities;
      entities.Reserve(entityCount);
      for (int index = 0; index < entityCount; ++index)
      {
          entities.EmplaceBack(world.Spawn(Transform{float(index), 0.0f, 0.0f}, Tag{}));
      }
      for (NGIN::UIntSize index = 0; index < entities.Size(); ++index)
      {
          world.Despawn(entities[index]);
      }
    });

#if defined(NGIN_ECS_BENCHMARK_WITH_ENTT)
    std::cout << "entt benchmark integration enabled\n";
#endif

#if defined(NGIN_ECS_BENCHMARK_WITH_FLECS)
    std::cout << "flecs benchmark integration enabled\n";
#endif

    return 0;
}
