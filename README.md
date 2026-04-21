# NGIN.ECS

NGIN.ECS is a C++23 library for organizing game or simulation logic using an **entity-component-system (ECS)** model.

If you haven’t used an ECS before, the idea is simple:

* **Entities** are just IDs
* **Components** are plain data (position, velocity, health, etc.)
* **Systems** run over many entities at once and update that data

Instead of attaching behavior to objects, you write logic that operates on groups of data.

---

## A minimal example

```cpp
struct Transform { float x, y, z; };
struct Velocity  { float vx, vy, vz; };

World world;

world.Spawn(
    Transform{0, 0, 0},
    Velocity{1, 0, 0}
);

Query<Write<Transform>, Read<Velocity>> query {world};

query.ForEach([&](const RowView& row) {
    auto& t = row.Write<Transform>();
    const auto& v = row.Read<Velocity>();

    t.x += v.vx;
});
```

This updates every entity that has both `Transform` and `Velocity`.

---

## How to think about it

NGIN.ECS stores entities in tables (called **archetypes**) based on the components they have.

A query like:

```cpp
Query<Write<Transform>, Read<Velocity>>
```

means:

> “Give me all rows where both of these components exist.”

Each iteration gives you direct access to that row’s data — no lookups, no indirection.

---

## Why this style is useful

This approach makes it easy to:

* process large numbers of entities efficiently
* keep data layout predictable
* separate data from behavior cleanly

It’s commonly used in:

* games
* simulations
* real-time systems

---

## What NGIN.ECS focuses on

Instead of trying to hide how ECS works, NGIN.ECS keeps things explicit:

### Direct data access

You work with references to components directly:

```cpp
auto& transform = row.Write<Transform>();
```

There’s no wrapper or proxy layer.

---

### Normal C++ types

Components can be anything:

* `std::string`
* `std::vector<T>`
* move-only types

They behave exactly as expected (construct, move, destroy).

---

### Immediate changes

When you remove something, it’s gone:

```cpp
world.Despawn(entity);
```

There’s no delayed cleanup step to keep track of.

---

### Systems describe their inputs

```cpp
MakeSystem("Move", [](Query<
    Write<Transform>,
    Read<Velocity>
>& query) {
    ...
});
```

The types in the function signature describe what the system reads and writes.

This is used to organize execution safely.

---

## Common operations

```cpp
auto e = world.Spawn(Transform{...});

world.Get<Transform>(e);
world.GetMut<Transform>(e);

world.Add<Velocity>(e, {...});
world.Remove<Velocity>(e);

world.Despawn(e);
```

---

## Change detection

You can track what changed between frames:

```cpp
world.MarkChanged<Transform>(entity);

Query<Changed<Transform>> query {world};
```

Use `world.NextEpoch()` to advance the frame.

---

## Running systems

```cpp
Scheduler scheduler;

scheduler.Register(moveSystem);
scheduler.Build();
scheduler.Run(world);
```

Systems are grouped and executed in a safe order based on what they access.

---

## Examples

* `examples/QuickStart/`
* `examples/ChangeDetection/`

---

## Build

```bash
cmake -S . -B build \
  -DNGIN_ECS_BUILD_TESTS=ON \
  -DNGIN_ECS_BUILD_EXAMPLES=ON

cmake --build build -j
ctest --test-dir build --output-on-failure
```

---

## In short

NGIN.ECS gives you:

* a way to organize data as components
* a way to process that data in batches
* full control over how and when things change

Without hiding how it works.
