# Storage Model

This guide explains how `NGIN.ECS` stores data and why the public APIs behave the way they do.

## Archetypes

An archetype is a storage group for one exact component signature.

Examples:

- `{Transform, Velocity}`
- `{Transform, Velocity, PlayerTag}`
- `{Transform}`

If two entities have the same component set, they live in the same archetype.

If you add or remove a component, the entity migrates to a different archetype.

## Chunks

Each archetype owns one or more chunks.

A chunk stores:

- entity ids for each row
- one column per component
- one added-tick column per component
- one changed-tick column per component

This keeps rows dense and iteration predictable.

## Entity Location Table

The world keeps a slot table keyed by entity index.

Each live slot tracks:

- current generation
- alive flag
- archetype index
- chunk index
- row index

That is what makes these operations cheap and correct:

- `TryGet<T>(entity)`
- `Get<T>(entity)`
- `Despawn(entity)`
- swap-remove updates after row movement

## Immediate Despawn

When you call:

```cpp
world.Despawn(entity);
```

the row is removed immediately.

Internally:

1. the row is destroyed
2. the last row in the chunk may move into its place
3. the moved entity’s location table entry is updated
4. the entity allocator bumps generation and recycles the index

## Swap-Remove Behavior

Storage is dense, so row order is not stable.

Do not rely on:

- chunk-local row indices staying fixed
- iteration order remaining the same after structural changes

Do rely on:

- `EntityId` stability while the entity is alive
- direct access through `World`
- row visibility being correct after structural changes

## Component Lifecycle

Each component type is described by `ComponentInfo`, which includes:

- size
- alignment
- empty/tag flag
- bitwise-relocatable flag
- copy construct hook
- move construct hook
- relocate construct hook
- destroy hook

This is why ECS storage can safely host:

- trivial structs
- `std::string`
- `std::vector<T>`
- move-only wrapper types

## Tags

Empty component types are treated as tags.

They still participate in:

- archetype signatures
- `Has<T>()`
- `With<T>` / `Without<T>`
- `Added<T>`

They do not allocate a normal data column.

## Why Writes Are Explicitly Marked

The storage layer tracks actual component ticks, not just “this chunk was visited by a write query”.

That is why dirty marking is explicit:

- `world.Set<T>(...)`
- `world.MarkChanged<T>(...)`
- `row.MarkChanged<T>()`
- `chunk.MarkChanged<T>(...)`

This avoids false positives where touching one row makes the whole chunk appear changed.
