# Queries

Queries are the main way to iterate ECS data.

A query describes a component shape and optional filters, then yields live rows that match.

## Query Terms

Available terms:

- `Read<T>`
- `Write<T>`
- `Opt<T>`
- `With<T>`
- `Without<T>`
- `Added<T>`
- `Changed<T>`

## Basic Read / Write Query

```cpp
NGIN::ECS::Query<
    NGIN::ECS::Write<Transform>,
    NGIN::ECS::Read<Velocity>
> query {world};
```

This matches rows that have both `Transform` and `Velocity`.

## Iteration Styles

### Row iteration

Use this for the normal case.

```cpp
query.ForEach([&](const NGIN::ECS::RowView& row) {
    auto&       transform = row.Write<Transform>();
    const auto& velocity  = row.Read<Velocity>();
    transform.x += velocity.vx;
    row.MarkChanged<Transform>();
});
```

### Chunk iteration

Use this when you want per-chunk grouping or want to inspect optional column presence once per chunk.

```cpp
query.ForChunks([&](const NGIN::ECS::ChunkView& chunk) {
    for (NGIN::UIntSize i = 0; i < chunk.Count(); ++i)
    {
        auto entity = chunk.EntityAt(i);
        auto& transform = chunk.Write<Transform>(i);
        (void)entity;
        (void)transform;
    }
});
```

## `RowView`

Main API:

- `Entity()`
- `Read<T>()`
- `Write<T>()`
- `TryRead<T>()`
- `TryWrite<T>()`
- `MarkChanged<T>()`

There are lowercase aliases too:

- `entity()`
- `read<T>()`
- `write<T>()`
- `try_read<T>()`
- `try_write<T>()`
- `mark_changed<T>()`

## `ChunkView`

Main API:

- `Count()`
- `Row(i)`
- `EntityAt(i)`
- `Has<T>()`
- `Read<T>(i)`
- `Write<T>(i)`
- `TryRead<T>(i)`
- `TryWrite<T>(i)`
- `MarkChanged<T>(i)`
- `AddedTick<T>(i)`
- `ChangedTick<T>(i)`

## Optional Terms

`Opt<T>` does not constrain matching. It means:

> “If the archetype has `T`, let me read it. If not, give me null.”

Example:

```cpp
NGIN::ECS::Query<
    NGIN::ECS::Read<Transform>,
    NGIN::ECS::Opt<Velocity>
> query {world};

query.ForEach([&](const NGIN::ECS::RowView& row) {
    if (const auto* velocity = row.TryRead<Velocity>())
    {
        // row has Velocity
    }
    else
    {
        // row does not have Velocity
    }
});
```

## `With<T>` And `Without<T>`

These constrain matching but do not expose access by themselves.

Example:

```cpp
NGIN::ECS::Query<
    NGIN::ECS::Read<Transform>,
    NGIN::ECS::With<PlayerTag>,
    NGIN::ECS::Without<Disabled>
> activePlayers {world};
```

## Change Filters

`Added<T>` and `Changed<T>` are row-level filters.

Example:

```cpp
NGIN::ECS::Query<NGIN::ECS::Added<Tag>> addedThisFrame {world};
NGIN::ECS::Query<NGIN::ECS::Changed<Transform>> changedThisFrame {world};
```

The query baseline defaults to `world.PreviousEpoch()`.

You can override it:

```cpp
NGIN::ECS::Query<NGIN::ECS::Changed<Transform>> sinceStart {world, 0};
```

## Matching Rules

### `Read<T>`

- requires `T`
- counted as a read dependency for scheduled systems

### `Write<T>`

- requires `T`
- counted as a write dependency for scheduled systems
- grants mutable access
- does not automatically mark the row changed

### `Opt<T>`

- does not require `T`
- counted as a read dependency for scheduled systems

### `Added<T>`

- requires `T`
- only matches rows where `T` was added after the query baseline

### `Changed<T>`

- requires `T`
- only matches rows where `T` was marked changed after the query baseline

## Practical Guidance

Use `RowView` when:

- most logic is row-by-row
- you need entity ids alongside component access
- you want the simplest and clearest code

Use `ChunkView` when:

- you want chunk-level bookkeeping
- you want to branch once on `Has<T>()` and then iterate rows
- you are writing tooling or diagnostics around storage behavior
