# NGIN.ECS API Guide

This is a compact public-surface guide for `include/NGIN/ECS`.

## Main Headers

Use the focused headers that match your need:

- `#include <NGIN/ECS/Entity.hpp>`
- `#include <NGIN/ECS/World.hpp>`
- `#include <NGIN/ECS/Query.hpp>`
- `#include <NGIN/ECS/Commands.hpp>`
- `#include <NGIN/ECS/Scheduler.hpp>`
- `#include <NGIN/ECS/TypeRegistry.hpp>`

## `Entity.hpp`

### Types and constants

- `using EntityId = NGIN::UInt64`
- `NullEntityId`

### Helpers

- `GetEntityIndex(EntityId)`
- `GetEntityGeneration(EntityId)`
- `MakeEntityId(index, generation)`
- `IsNull(EntityId)`

## `World.hpp`

### Frame and lifecycle

- `CurrentEpoch()`
- `PreviousEpoch()`
- `NextEpoch()`
- `AliveCount()`
- `Clear()`
- `IsAlive(entity)`

### Entity creation and destruction

- `Spawn()`
- `Spawn(Cs&&...)`
- `Despawn(entity)`

### Direct component access

- `Has<T>(entity)`
- `TryGet<T>(entity)`
- `TryGetMut<T>(entity)`
- `Get<T>(entity)`
- `GetMut<T>(entity)`

### Structural changes

- `Add<T>(entity, value)`
- `Remove<T>(entity)`
- `Set<T>(entity, value)`
- `MarkChanged<T>(entity)`

## `Query.hpp`

### Terms

- `Read<T>`
- `Write<T>`
- `Opt<T>`
- `With<T>`
- `Without<T>`
- `Added<T>`
- `Changed<T>`

### Query

- `Query<Terms...>(world)`
- `Query<Terms...>(world, sinceTick)`
- `SinceTick()`
- `Metadata()`
- `ForEach(fn)`
- `ForChunks(fn)`
- lowercase aliases `each(...)` and `for_chunks(...)`

### `RowView`

- `Entity()`
- `Read<T>()`
- `Write<T>()`
- `TryRead<T>()`
- `TryWrite<T>()`
- `MarkChanged<T>()`

### `ChunkView`

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

## `Commands.hpp`

### Queueing operations

- `Spawn(Cs&&...)`
- `Despawn(entity)`
- `Add<T>(entity, value)`
- `Remove<T>(entity)`
- `Set<T>(entity, value)`
- `ClearWorld()`

### Buffer management

- `Flush(world)`
- `Clear()`
- `Size()`

## `Scheduler.hpp`

### Builders

- `MakeSystem(name, callable)`
- `MakeExclusiveSystem(name, callable)`

### Param wrapper

- `ExclusiveWorld`

### Scheduler

- `Register(system)`
- `Build()`
- `Run(world)`
- `StageCount()`
- `StageAt(i)`

## `TypeRegistry.hpp`

### Type ids

- `TypeId`
- `GetTypeId<T>()`

### Metadata

- `ComponentInfo`
- `DescribeComponent<T>()`

## Current Caveats

- `Write<T>` does not automatically mark `T` changed; call `MarkChanged<T>()` after mutating.
- `World::Add<T>` and `World::Set<T>` currently expect the provided value type to match `T` exactly.
- Systems that take `Commands&` are currently staged as exclusive so command-producing systems always get a barrier.
- `ECS.hpp` still only exposes the legacy `ParseInt` and `LibraryName` helpers; it is not the umbrella ECS entry point yet.
