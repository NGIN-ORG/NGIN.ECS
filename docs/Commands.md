# Commands

`Commands` is the deferred structural-change API.

Use it when you want to queue changes during system execution and apply them at a safe stage boundary instead of
mutating storage immediately.

## What It Supports

- `Spawn(...)`
- `Despawn(entity)`
- `Add<T>(entity, value)`
- `Remove<T>(entity)`
- `Set<T>(entity, value)`
- `ClearWorld()`
- `Flush(world)`
- `Clear()`
- `Size()`

## Why Use Commands

Direct `World` changes are immediate, which is useful outside system execution.

Inside scheduled systems, deferred commands give you:

- a clean place to queue structural changes
- deterministic application order
- a stage barrier before later systems observe the result

## Example

```cpp
auto spawnEnemies = NGIN::ECS::MakeSystem("SpawnEnemies", [](NGIN::ECS::Commands& commands) {
    commands.Spawn(Transform{0, 0, 0}, EnemyTag{});
    commands.Spawn(Transform{10, 0, 0}, EnemyTag{});
});
```

When the stage ends, the scheduler calls `commands.Flush(world)`.

## Order Guarantees

Commands are applied in submission order.

Example:

```cpp
commands.Add<Velocity>(e, Velocity{1, 0, 0});
commands.Set<Transform>(e, Transform{2, 0, 0});
commands.Remove<Velocity>(e);
```

The final entity state reflects that exact order.

## Manual Usage

You can also use `Commands` directly without the scheduler:

```cpp
NGIN::ECS::Commands commands;
commands.Spawn(Transform{0, 0, 0});
commands.Flush(world);
```

## `ClearWorld()`

This queues a full world clear.

Example:

```cpp
commands.ClearWorld();
commands.Spawn(Tag{});
commands.Flush(world);
```

After flush:

- the old world contents are gone
- later queued commands in the same buffer still run

## Payload Behavior

The command buffer stores typed operations in packed internal storage rather than wrapping each command in
`std::function`.

This matters because it supports:

- lower overhead than one heap-backed callable per op
- move-only payloads
- better control over construction and destruction

## Practical Guidance

Use direct `World` operations when:

- you are outside a scheduler run
- the operation is immediate and local

Use `Commands` when:

- you are in a system
- the operation changes structure
- you want a clear stage boundary before later readers see the result
