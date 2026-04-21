# Systems And Scheduler

`NGIN.ECS` systems are built from typed params.

The scheduler reads those param types, infers access, and builds a serial stage plan that is trustworthy enough to
parallelize later.

## The Two Constructors

- `MakeSystem(name, callable)`
- `MakeExclusiveSystem(name, callable)`

Both return a `SystemDescriptor`.

## Normal Typed Systems

Example:

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

From that signature, the scheduler learns:

- this system reads `Velocity`
- this system writes `Transform`

## Supported System Params

### `Query<Terms...>`

This is the normal way to express component access.

Accepted forms:

- `Query<Terms...>`
- `Query<Terms...>&`
- `const Query<Terms...>&`

The scheduler uses query terms to infer reads and writes.

### `Commands&`

Use this for deferred structural changes.

Example:

```cpp
auto spawnSystem = NGIN::ECS::MakeSystem("Spawn", [](NGIN::ECS::Commands& commands) {
    commands.Spawn(PlayerTag{});
});
```

Current behavior:

- a system that takes `Commands&` is treated as exclusive for stage planning
- this guarantees a barrier before later readers observe flushed changes

### `ExclusiveWorld`

Use this when the system needs unrestricted direct `World` access.

Example:

```cpp
auto clearSystem = NGIN::ECS::MakeExclusiveSystem("Clear", [](NGIN::ECS::ExclusiveWorld world) {
    world->Clear();
});
```

An exclusive system always runs in its own stage.

## Building And Running

```cpp
NGIN::ECS::Scheduler scheduler;
scheduler.Register(moveSystem);
scheduler.Register(spawnSystem);
scheduler.Build();
scheduler.Run(world);
```

## What `Scheduler::Run` Does

When you call `Run(world)`:

1. the world advances to the next epoch
2. each stage runs in order
3. one shared `Commands` buffer is flushed after each stage
4. each system’s last-run tick is updated

This matters for:

- `Added<T>`
- `Changed<T>`
- direct query baseline behavior

## Conflict Rules

Two systems conflict if:

- both write the same component
- one writes a component the other reads

The current scheduler is serial, but it still uses those rules to produce a correct stage order.

## Inspecting The Stage Plan

Helpers:

- `StageCount()`
- `StageAt(i)`

These are useful in tests and diagnostics.

## When To Use Which Style

Use a normal typed system when:

- your logic is mostly query-driven
- you want the scheduler to infer real access

Use `Commands&` when:

- you need deferred structural changes
- the system should not mutate world structure directly during query iteration

Use `ExclusiveWorld` when:

- the operation is inherently whole-world
- you need direct `World` methods that are not modeled as query/commands params

## Recommended Pattern

Keep most systems shaped like:

- one or more query params
- optional `Commands&`

Reserve `ExclusiveWorld` for rare whole-world operations.
