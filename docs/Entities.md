# Entities And World

This guide covers the basic ownership and direct-access model in `NGIN.ECS`.

## `EntityId`

`EntityId` is a 64-bit generational handle.

Layout:

- high 16 bits: generation
- low 48 bits: entity index

Helpers in `Entity.hpp`:

- `GetEntityIndex(id)`
- `GetEntityGeneration(id)`
- `MakeEntityId(index, generation)`
- `IsNull(id)`
- `NullEntityId`

Why generations matter:

- stale ids are rejected after despawn
- reused indices get a higher generation
- clearing the world invalidates old ids before indices are reused

## `World`

`World` is the main API surface. It owns archetypes, chunks, and entity-location metadata.

It is intentionally non-copyable and non-movable.

### Lifecycle And Frame State

- `CurrentEpoch()`
- `PreviousEpoch()`
- `NextEpoch()`
- `AliveCount()`
- `Clear()`

`NextEpoch()` advances the world tick used by change detection.

## Spawning Entities

### Empty entity

```cpp
NGIN::ECS::EntityId e = world.Spawn();
```

### Entity with components

```cpp
auto e = world.Spawn(
    Transform{0, 0, 0},
    Velocity{1, 0, 0}
);
```

The component set determines which archetype the row is stored in.

## Despawning

```cpp
world.Despawn(e);
```

Important behavior:

- removal is immediate
- the row disappears from future queries right away
- storage uses swap-remove internally, so another entity may move into the freed row
- stale handles fail `IsAlive(...)`

## Liveness

```cpp
if (world.IsAlive(e))
{
    // safe to use direct world APIs
}
```

## Reading Components Directly

### Non-throwing access

- `TryGet<T>(entity) -> const T*`
- `TryGetMut<T>(entity) -> T*`
- `Has<T>(entity) -> bool`

```cpp
if (const auto* velocity = world.TryGet<Velocity>(e))
{
    // use *velocity
}
```

### Throwing access

- `Get<T>(entity) -> const T&`
- `GetMut<T>(entity) -> T&`

Use these when missing components are a bug, not a normal condition.

## Structural Changes

### Add a component

```cpp
world.Add<Velocity>(e, Velocity{1, 0, 0});
```

This migrates the entity to a different archetype.

### Remove a component

```cpp
bool removed = world.Remove<Velocity>(e);
```

Returns `false` if the component was not present.

### Replace a component value

```cpp
world.Set<Velocity>(e, Velocity{2, 0, 0});
```

This updates the stored value in place and marks the row changed for that component in the current epoch.

## Marking Changes Explicitly

When you mutate a component through `GetMut<T>` or query row access, call:

```cpp
world.MarkChanged<Transform>(e);
```

or

```cpp
row.MarkChanged<Transform>();
```

`Write<T>` grants mutable access. It does not automatically set the changed tick.

## Tag Components

Empty types are supported:

```cpp
struct PlayerTag {};

auto e = world.Spawn(PlayerTag{});
bool hasTag = world.Has<PlayerTag>(e);
```

Tags participate in:

- archetype selection
- `With<T>` / `Without<T>` query filtering
- `Added<T>` query filtering

## Current Constraint

`World::Add<T>` and `World::Set<T>` currently require the provided value type to match `T` exactly.

Good:

```cpp
world.Set<Velocity>(e, Velocity{1, 0, 0});
```

Not documented as supported:

```cpp
world.Set<Velocity>(e, SomeVelocityLikeType{...});
```

## Typical Pattern

Use direct world access when:

- you are working with one specific entity
- you need structural changes outside the scheduler
- you are bridging ECS with non-ECS code

Use queries when:

- you are processing many entities at once
- the operation is shaped by a component set
- you want the scheduler to reason about access
