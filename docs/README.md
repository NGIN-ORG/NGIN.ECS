# NGIN.ECS Docs

This is the user-facing documentation entry point for `NGIN.ECS`.

If you are new to the library, start with the practical guides first. Read the storage and API reference pages after
you have the mental model in place.

## Start Here

Choose the path that matches what you need right now.

### I want to get something running fast

Read [QuickStart.md](QuickStart.md).

Use it for:

- first entity and component types
- the normal `Spawn -> Query -> Scheduler` loop
- the smallest working examples

### I want to understand entity handles and the `World` API

Read [Entities.md](Entities.md).

Use it for:

- `EntityId`
- liveness and generations
- `World`
- `Spawn`, `Despawn`, `Add`, `Remove`, `Set`, `Get`, `TryGet`

### I want to iterate data with queries

Read [Queries.md](Queries.md).

Use it for:

- `Query<Terms...>`
- `RowView`
- `ChunkView`
- `Read`, `Write`, `Opt`, `With`, `Without`, `Added`, `Changed`

### I want to build systems and run them through the scheduler

Read [Systems.md](Systems.md).

Use it for:

- `MakeSystem`
- `MakeExclusiveSystem`
- typed system params
- `Scheduler`
- execution stages and conflict rules

### I want deferred structural changes

Read [Commands.md](Commands.md).

Use it for:

- `Commands`
- deferred spawn, despawn, add, remove, set, clear
- stage barriers and flush behavior

### I want to understand frame-to-frame change tracking

Read [ChangeDetection.md](ChangeDetection.md).

Use it for:

- `world.NextEpoch()`
- `Added<T>`
- `Changed<T>`
- baseline ticks for direct queries and scheduled systems

### I want the storage and performance model

Read [Storage.md](Storage.md).

Use it for:

- archetypes and chunks
- row movement
- swap-remove behavior
- component lifecycle hooks

### I want a compact API checklist

Read [API.md](API.md).

Use it for:

- public headers
- top-level types
- method inventory
- current caveats and constraints

## Recommended Reading Order

For a first pass:

1. [../README.md](../README.md)
2. [QuickStart.md](QuickStart.md)
3. [Entities.md](Entities.md)
4. [Queries.md](Queries.md)
5. [Systems.md](Systems.md)
6. whichever focused guide you need next
