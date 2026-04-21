# Quick Start

This guide shows the normal first workflow with `NGIN.ECS`:

1. define component types
2. spawn entities into a `World`
3. iterate matching rows with a `Query`
4. group logic into systems and run them with a `Scheduler`

## The Smallest Loop

```cpp
#include <NGIN/ECS/World.hpp>
#include <NGIN/ECS/Query.hpp>

struct Transform { float x, y, z; };
struct Velocity  { float vx, vy, vz; };

int main()
{
    NGIN::ECS::World world;

    (void)world.Spawn(
        Transform{0.0f, 0.0f, 0.0f},
        Velocity{1.0f, 0.0f, 0.0f}
    );

    NGIN::ECS::Query<NGIN::ECS::Write<Transform>, NGIN::ECS::Read<Velocity>> query {world};
    query.ForEach([&](const NGIN::ECS::RowView& row) {
        auto&       transform = row.Write<Transform>();
        const auto& velocity  = row.Read<Velocity>();

        transform.x += velocity.vx;
        transform.y += velocity.vy;
        transform.z += velocity.vz;

        row.MarkChanged<Transform>();
    });
}
```

What to notice:

- `Spawn(...)` creates an entity and places it in storage based on its component set
- `Query<Write<Transform>, Read<Velocity>>` only matches rows that have both components
- iteration is row-based, not object-based
- writes are explicit, and change tracking is explicit too

## A Slightly More Realistic Example

```cpp
#include <NGIN/ECS/World.hpp>
#include <NGIN/ECS/Query.hpp>
#include <NGIN/ECS/Scheduler.hpp>

struct Transform { float x, y, z; };
struct Velocity  { float vx, vy, vz; };
struct PlayerTag {};

int main()
{
    using namespace NGIN::ECS;

    World world;

    for (int i = 0; i < 128; ++i)
    {
        (void)world.Spawn(
            Transform{float(i), 0.0f, 0.0f},
            Velocity{1.0f, 2.0f, 3.0f},
            PlayerTag{}
        );
    }

    auto moveSystem = MakeSystem("Move", [](Query<Write<Transform>, Read<Velocity>>& query) {
        query.ForEach([&](const RowView& row) {
            auto&       transform = row.Write<Transform>();
            const auto& velocity  = row.Read<Velocity>();
            transform.x += velocity.vx;
            transform.y += velocity.vy;
            transform.z += velocity.vz;
            row.MarkChanged<Transform>();
        });
    });

    Scheduler scheduler;
    scheduler.Register(moveSystem);
    scheduler.Build();
    scheduler.Run(world);
}
```

## First Concepts To Keep In Mind

### Entities are handles, not objects

An `EntityId` identifies a row in ECS storage. It does not own behavior and it is not a class hierarchy node.

### Components are data

Keep components focused on state. `NGIN.ECS` supports normal C++ types, including:

- trivial structs
- `std::string`
- `std::vector<T>`
- move-only wrappers

### Queries define the data shape

A query is how you say:

> “Give me every live row that looks like this.”

### Systems define execution shape

A scheduler system is just a callable whose parameter types describe what it needs:

- `Query<...>&`
- `Commands&`
- `ExclusiveWorld`

## What To Read Next

- [Entities.md](Entities.md) for `World` and entity operations
- [Queries.md](Queries.md) for terms and iteration APIs
- [Systems.md](Systems.md) for scheduler usage
