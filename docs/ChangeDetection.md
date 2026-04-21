# Change Detection

`NGIN.ECS` tracks add and change events per row and per component column.

The library does not use chunk-wide dirty flags. Queries can distinguish mixed old and new rows in the same chunk.

## The Tick Model

The world stores:

- `CurrentEpoch()`
- `PreviousEpoch()`

You advance time with:

```cpp
world.NextEpoch();
```

The scheduler also advances the world tick automatically at the start of `Scheduler::Run(world)`.

## `Added<T>`

`Added<T>` matches rows where component `T` was added after the query baseline.

This happens when:

- an entity is spawned with `T`
- `world.Add<T>(entity, value)` migrates an entity into an archetype that now includes `T`

Example:

```cpp
NGIN::ECS::Query<NGIN::ECS::Added<PlayerTag>> addedPlayers {world};
```

## `Changed<T>`

`Changed<T>` matches rows where component `T` was marked changed after the query baseline.

This happens when:

- `world.Set<T>(entity, value)` runs
- `world.MarkChanged<T>(entity)` runs
- `row.MarkChanged<T>()` or `chunk.MarkChanged<T>(i)` runs during query iteration

## Important Rule: Mutable Access Is Not Automatically Dirty

This is deliberate.

Example:

```cpp
query.ForEach([&](const NGIN::ECS::RowView& row) {
    auto& transform = row.Write<Transform>();
    transform.x += 1.0f;
    row.MarkChanged<Transform>();
});
```

If you mutate through `Write<T>()` and do not mark the row changed, `Changed<T>` queries will not see that write.

## Query Baselines

### Direct queries

Direct `Query<...> query {world};` uses `world.PreviousEpoch()` as its baseline.

### Explicit baseline

You can override the baseline:

```cpp
NGIN::ECS::Query<NGIN::ECS::Changed<Transform>> sinceStart {world, 0};
```

### Scheduled systems

The scheduler passes each system its own `sinceTick`, based on that system’s previous run.

This means a scheduled query param naturally answers:

> “What changed since the last time this system ran?”

## Typical Patterns

### Detect newly spawned rows this frame

```cpp
NGIN::ECS::Query<NGIN::ECS::Added<Tag>> added {world};
```

### Detect rows changed by a system

```cpp
auto moveSystem = NGIN::ECS::MakeSystem("Move", [](
    NGIN::ECS::Query<NGIN::ECS::Write<Transform>, NGIN::ECS::Read<Velocity>>& query
) {
    query.ForEach([&](const NGIN::ECS::RowView& row) {
        auto&       transform = row.Write<Transform>();
        const auto& velocity  = row.Read<Velocity>();
        transform.x += velocity.vx;
        row.MarkChanged<Transform>();
    });
});
```

### Check the next frame

```cpp
world.NextEpoch();
NGIN::ECS::Query<NGIN::ECS::Changed<Transform>> changed {world};
```

If nothing marked `Transform` changed in the new epoch, the query is empty.
