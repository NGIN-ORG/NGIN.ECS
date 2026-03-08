# NGIN.ECS

NGIN.ECS is a C++23 entity-component-system library in the NGIN family.

It is built around a data-oriented model with archetypes, chunked storage, and explicit system access patterns.

The short version is:

- entities are lightweight handles
- components are plain data
- systems operate through queries
- storage is cache-friendly
- scheduling is intended to remain deterministic and explicit

It is designed to be useful as a standalone ECS library, not only inside the full NGIN platform.

## What NGIN.ECS Is For

NGIN.ECS is for applications that want:

- ECS-style composition
- data-oriented storage
- explicit read/write access patterns
- deterministic system execution
- a path toward safe scheduling and deferred structural changes

Typical uses include:

- gameplay/runtime simulation
- tool-side world state and previews
- data-oriented application subsystems

## The Core Model

### Entity

A lightweight identifier for a thing.

### Component

Plain data attached to entities.

Examples:

- `Transform`
- `Velocity`
- `Health`

### Query

A way to iterate over entities that match a component shape.

### System

Logic that reads and writes components through queries.

### Commands

A deferred way to perform structural changes such as spawning or despawning.

## Quick Example

```cpp
#include <NGIN/ECS/World.hpp>
#include <NGIN/ECS/Query.hpp>

struct Transform { float x, y, z; };
struct Velocity  { float vx, vy, vz; };

NGIN::ECS::World world;
for (int i = 0; i < 1000; ++i)
{
    (void)world.Spawn(Transform{float(i), 0, 0}, Velocity{1, 2, 3});
}

NGIN::ECS::Query<NGIN::ECS::Write<Transform>, NGIN::ECS::Read<Velocity>> query{world};
query.for_chunks([&](const NGIN::ECS::ChunkView& chunk) {
    auto* transforms = chunk.write<Transform>();
    const auto* velocities = chunk.read<Velocity>();
    for (auto i = chunk.begin(); i < chunk.end(); ++i)
    {
        transforms[i].x += velocities[i].vx;
    }
});
```

## What It Provides

NGIN.ECS provides:

- entity IDs
- component-oriented world storage
- archetypes and chunked columnar layout
- queries over component sets
- deferred structural commands
- scheduler-oriented access declarations
- change detection support through epochs

## Build Targets

- `NGIN::ECS`

## Build Options

Main CMake options:

- `NGIN_ECS_BUILD_TESTS` default `ON`
- `NGIN_ECS_BUILD_EXAMPLES` default `OFF`

## Typical Local Build

```bash
cmake -S . -B build \
  -DNGIN_ECS_BUILD_TESTS=ON \
  -DNGIN_ECS_BUILD_EXAMPLES=ON

cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Where It Fits

Within the NGIN platform, NGIN.ECS is a domain/runtime package that can be pulled into targets which need ECS-style world composition.

Outside the platform, it is simply a standalone ECS library built on top of `NGIN.Base`.

## Read Next

- `include/NGIN/ECS/`
- `examples/`
