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

    // Spawn a batch of entities with components {Transform, Velocity, PlayerTag}
    for (int i = 0; i < 256; ++i)
    {
        (void)world.Spawn(Transform{float(i), 0.0f, 0.0f}, Velocity{1.0f, 2.0f, 3.0f}, PlayerTag{});
    }

    // Query: update transforms from velocities
    {
        Query<Write<Transform>, Read<Velocity>> q{world};
        const float dt = 1.0f;
        q.for_chunks([&](const ChunkView& chunk) {
            auto*       t = chunk.write<Transform>();
            const auto* v = chunk.read<Velocity>();
            for (NGIN::UIntSize i = chunk.begin(); i < chunk.end(); ++i)
            {
                t[i].x += v[i].vx * dt;
                t[i].y += v[i].vy * dt;
                t[i].z += v[i].vz * dt;
            }
        });
    }

    // Scheduler: two systems with a barrier in between; the second sees spawns from the first
    Scheduler sched;
    auto spawner = MakeSystem<Write<PlayerTag>>("Spawner", [](World& w, Commands& cmd){
        for (int i = 0; i < 10; ++i)
            cmd.Spawn(PlayerTag{});
    });
    auto counter = MakeSystem<Read<PlayerTag>>("Counter", [](World& w, Commands&){
        Query<Read<PlayerTag>> r{w};
        NGIN::UIntSize total = 0;
        r.for_chunks([&](const ChunkView& ch){ total += (ch.end() - ch.begin()); });
        std::cout << "PlayerTag count: " << static_cast<std::size_t>(total) << "\n";
    });
    sched.Register(spawner);
    sched.Register(counter);
    sched.Build();
    sched.Run(world);

    return 0;
}

