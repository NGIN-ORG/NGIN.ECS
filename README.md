# NGIN.ECS

`NGIN.ECS` is a C++23 library for organizing game, simulation, and tool logic with an entity-component-system model.

If that phrase is new to you, the short version is:

- an **entity** is just an ID
- a **component** is a piece of data, like position, velocity, health, or a tag
- a **system** is code that runs over many matching entities and updates their data

Instead of building behavior around large object hierarchies, you keep the data simple and run logic over groups of
that data.

## Why You Might Want This

This style is useful when your program has a lot of things that share common kinds of state:

- players and enemies in a game
- particles and projectiles
- simulation agents
- runtime objects in an editor or preview tool

It helps when you want:

- data laid out predictably
- one piece of logic to process many objects at once
- clean separation between data and behavior
- explicit control over updates and structural changes

## What NGIN.ECS Tries To Be

`NGIN.ECS` is not trying to hide the ECS model behind a lot of framework code.

It is designed to be:

- explicit
- data-oriented
- friendly to normal C++ component types
- clear about when things change

In practice, that means:

- entities are lightweight handles
- component access is direct
- despawn and clear happen immediately
- systems describe their data needs through typed parameters
- change tracking is row-based instead of broad chunk-level guessing

## Smallest Useful Example

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

    NGIN::ECS::Query<
        NGIN::ECS::Write<Transform>,
        NGIN::ECS::Read<Velocity>
    > query {world};

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

What this means in plain language:

1. create a world
2. spawn one entity with a `Transform` and a `Velocity`
3. ask for every row that has both components
4. update the position from the velocity

## What Makes This Library Practical

### You work with normal C++ component types

Components do not have to be tiny POD-only structs.

`NGIN.ECS` supports normal C++ lifecycle behavior for things like:

- `std::string`
- `std::vector<T>`
- move-only wrappers

### Direct world operations are simple

Typical operations look like this:

```cpp
auto entity = world.Spawn(Transform{0, 0, 0});

world.Add<Velocity>(entity, Velocity{1, 0, 0});
world.Set<Transform>(entity, Transform{5, 0, 0});

const auto& transform = world.Get<Transform>(entity);

world.Despawn(entity);
```

### Systems describe what they need

Instead of passing every system a raw `World&` and hoping it behaves, systems are normally written with typed params:

```cpp
auto moveSystem = NGIN::ECS::MakeSystem("Move", [](
    NGIN::ECS::Query<
        NGIN::ECS::Write<Transform>,
        NGIN::ECS::Read<Velocity>
    >& query
) {
    query.ForEach([&](const NGIN::ECS::RowView& row) {
        auto&       transform = row.Write<Transform>();
        const auto& velocity  = row.Read<Velocity>();
        transform.x += velocity.vx;
        row.MarkChanged<Transform>();
    });
});
```

That gives the scheduler real information about what the system reads and writes.

### Deferred structural changes are available when you need them

If a system needs to queue changes to apply after the current stage, it can use `Commands`:

```cpp
auto spawnSystem = NGIN::ECS::MakeSystem("Spawn", [](
    NGIN::ECS::Commands& commands
) {
    commands.Spawn(Transform{0, 0, 0});
});
```

## If You Are New To ECS

The easiest way to think about it is:

- the **world** owns all ECS data
- each **entity** is one thing in that world
- each **component** is one kind of state
- each **query** says which kinds of state you want
- each **system** runs logic over matching rows

You do not need to understand every storage detail to get started. You can begin with:

- `World`
- `Spawn`
- `Query`
- `RowView`
- `Scheduler`

Then learn the deeper parts as needed.

## Start Here

If you want the guided docs path, use this order:

1. [Documentation Index](docs/README.md)
2. [Quick Start](docs/QuickStart.md)
3. [Entities And World](docs/Entities.md)
4. [Queries](docs/Queries.md)
5. [Systems And Scheduler](docs/Systems.md)
6. [Commands](docs/Commands.md)
7. [Change Detection](docs/ChangeDetection.md)

## Examples

- `examples/QuickStart/`
- `examples/ChangeDetection/`

## Build

```bash
cmake -S . -B build \
  -DNGIN_ECS_BUILD_TESTS=ON \
  -DNGIN_ECS_BUILD_EXAMPLES=ON

cmake --build build -j
ctest --test-dir build --output-on-failure
```

Optional benchmark build:

```bash
cmake -S . -B build \
  -DNGIN_ECS_BUILD_BENCHMARKS=ON

cmake --build build --target ECSBenchmarks -j
```

## Read Next

- [docs/README.md](docs/README.md)
- [docs/QuickStart.md](docs/QuickStart.md)
- [docs/API.md](docs/API.md)
