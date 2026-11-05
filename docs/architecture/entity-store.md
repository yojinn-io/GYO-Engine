# GYO entity store

Owner: Engine (proposed: `engine/entity_store` → `GYO::EntityStore`, namespace `Engine::EntityStore`).
Plan, evidence and record: [entity-store](plans/entity-store/README.md) (the reference survey, the Architecture Delta
with its mapping to the first consumer's clauses, and the batches). Implementation is batch `v8-arch-entity-store-impl`.
**Status: proposed, not approved.** The `Design Invariants` section below is an Architecture Delta (AGENTS.md §3);
it takes effect when the user approves it. Nothing in `engine/` exists yet.

Evidence notation. "Survey" is the approved reference survey of Bevy ECS and EnTT in the plan folder; "§2.1 Commands
row" means the row of its table. "E-n" and "RC-n" are the approved clauses of the first consumer's entity model; each
invariant gives its own reason first, and the consumer clauses are evidence only (the Delta's mapping table says
which part of each clause stays with the consumer). Removing that consumer changes no rule here.

## What it is

The container part of an ECS data layout, and nothing else: one typed column per component, kept in strictly
ascending order of an id the caller gives, and a to-do of spawns and destroys that the caller applies, in ascending id,
at a sync point it names. It is not an entity runtime: it runs no system, owns no world and allots no id.

```text
consumer (a product) ────────────→ GYO::EntityStore ──→ GYO::Base
  owns: the column vectors,          header-only:          GYO_ASSERT
  which columns an entity has,       key access, column
  id allotment and ranges, kinds,    functions, deferred
  frozen phases and the sync point,  spawn/destroy to-do
  the one erase entry, digest, wire
```

`GYO::EntityStore` is a leaf directly above `GYO::Base`, like `GYO::Math` ([math](math.md)). A consumer links it
where it keeps columns.

## Components

| Component | What it gives | Status |
|---|---|---|
| `EntityId` | The key type, `std::uint64_t`. No value is reserved (a consumer may give 0 a meaning; the store does not), no generation bits, no range | proposed (batch 3) |
| Key access `EntityKey<T>`, `EntityOf` | `EntityOf(value)` returns `EntityKey<T>::Of(value)`. The primary `EntityKey<T>` reads the member `id`. An element's owner whose key lives elsewhere specializes `EntityKey` (a partial specialization constrained by a concept is allowed); a specialization always wins over the primary, also for a type that has an `id` member too. The specialization must be visible wherever the column is used. No specialization and no member `id`: the column does not compile | proposed (batch 3) |
| Column functions | `FindEntity`, `InsertEntity`, `EraseEntity` over a caller-owned `std::vector<T>`; `FindEntity` and `StrictlyAscending` also over `std::span<const T>` | proposed (batch 3) |
| `DeferredChanges<Spawn>` | The to-do of one sync point: `RecordSpawn(spawn)`, `RecordDestroy(id)`, `Apply(onSpawn, onDestroy)`, `Empty()`. `Spawn` is the consumer's record; its key is read through `EntityOf` | proposed; lands with its first consumer (Delta, open decision on timing) |

Names are proposals; the column functions keep the names their first consumer already uses (Delta §2). Batch 3 may
rename them; the semantics below may not change without a new Delta.

## Contracts

- **Key.** The caller gives every id. The store never allots, recycles, reserves or interprets an id, and has no
  notion of ranges or kinds. Precondition: the caller's allotters never reuse an id while anything may still hold it.
  The store has no generation to detect a reuse.
- **Column.** A `std::vector<T>` owned by the caller, strictly ascending by key, no duplicates. Every function
  requires this. The store does not re-check the whole column on each call (that would make every call O(n));
  a caller asserts it with `StrictlyAscending` where it chooses.
- **Find.** Binary search (`lower_bound` on the key). Returns a pointer to the element with that key, or `nullptr`.
  `noexcept`, no side effect.
- **Insert.** Inserts at the `lower_bound` position, never by appending, even when the key is greater than every key
  present. Inserting a key already present fails a `GYO_ASSERT`. Returns a reference to the inserted element.
- **Erase.** Removes the element with that key and keeps the order of the rest (no swap-and-pop, no tombstone).
  Returns whether it was present; absence is a result, not an error, because an entity need not have every column.
- **Invalidation.** `InsertEntity` and `EraseEntity` invalidate every pointer, reference, iterator and span into that
  column. Pointers from `FindEntity` stay valid until the next `InsertEntity` or `EraseEntity` on the same column.
  This is why a consumer freezes membership while it holds references.
- **Deferred changes.**
  - `RecordSpawn(spawn)` records a spawn whose id the consumer has already allotted, in the consumer's own order.
    Recording a second spawn with an id already recorded fails a `GYO_ASSERT`.
  - `RecordDestroy(id)` records a destroy. Recording an id already recorded has no further effect: two rules may
    name the same entity in one phase; which of them "did it" is the consumer's rule, not the store's.
  - `Apply(onSpawn, onDestroy)` is called by the caller at the sync point it names. It calls `onSpawn` for every
    recorded spawn in ascending id, then `onDestroy` for every recorded destroy in ascending id, then empties both
    lists. The callbacks do the actual insert and erase across the consumer's columns; the store never looks into
    a column.
  - Spawns before destroys: an entity spawned and destroyed at the same sync point is inserted, then erased, so it
    never exists after the sync point; the result is (present ∪ spawned) ∖ destroyed whatever order the rules
    recorded them in. The cost: destroying id X and spawning X again at the same sync point is not possible (see
    the next point). Destroys first would allow that, and would let a same-sync-point spawn-and-destroy survive.
  - A spawn whose id is already in a column fails that callback's `InsertEntity` assertion, a Programmer Error;
    whether a destroyed id must exist is the destroy callback's decision (`EraseEntity` returns false).
  - Recording into, or calling `Apply` on, the same `DeferredChanges` while its `Apply` runs fails a `GYO_ASSERT`;
    a change caused by a spawn or destroy belongs to the next sync point.
  - If a callback throws (for example a test's assertion handler), `Apply` empties both lists and clears its
    running state before the exception leaves it, so the `DeferredChanges` is empty and usable; records not yet
    applied are dropped, and the columns are as the callbacks left them.
- **Failures.** The store has no entry point for external data, so it returns no `Result` and throws nothing
  itself ([error handling](error-handling.md) rule 3). Programmer Errors (`GYO_ASSERT`): inserting a present key,
  recording a spawn id twice, recording or applying while `Apply` runs. A column that is not strictly ascending is a
  Programmer Error the caller detects with `StrictlyAscending`.
- **Not the store's.** When membership is frozen, where the sync point is, which columns an entity has, the one
  erase entry across columns, what a spawn record contains, and how a phase is kept from changing membership.

## Guard

Proposed for batch 3. Tests live in `tests/common/entity_store/` (`gyo_entity_store_tests`, CTest label `cpu`, so L1
runs it) and use synthetic element types only, never a product's (AGENTS.md §7). Mutants go in the plan folder's
`scripts/mutations.json` and must be killed by these common tests alone; a mutant counts as killed only when the
expected test fails. These rule tests are written from this contract (AGENTS.md §14.2), so each one shows its
detection power with the listed mutants.

| Invariant | Rule test | Mutants (must be killed) |
|---|---|---|
| 1 | Ids 0, 2^63 and `UINT64_MAX` insert, find and erase like any other; a spawn's id reaches `onSpawn` unchanged | `FindEntity` treats 0 as absent; `InsertEntity` asserts on 0 as a reserved value |
| 2 | Insert 5, 1, 9, 3 (descending runs included) → 1, 3, 5, 9; erase the middle and the ends → order kept; insert a key above all present | `InsertEntity` appends; `EraseEntity` swaps with the last |
| 3 | `FindEntity` on present, absent, below-first, above-last keys; a span and a vector agree. Comparison budget: with an `EntityKey` specialization that counts key reads, a lookup in 1,024 elements reads at most 2·⌈log2 n⌉ + 2 keys. "No side table" beyond what the row below detects: review | `FindEntity` returns the `lower_bound` element without comparing its key; `FindEntity` scans linearly |
| 4 | Record spawns 7, 3, 5 and destroys 9, 3 in that order → `onSpawn` 3, 5, 7, then `onDestroy` 3, 9; afterwards `Empty()`; a duplicate destroy is called once; recording or applying inside a callback asserts; a throwing callback leaves the object empty and usable | apply in recording order; destroys before spawns; no dedup of destroys; lists not emptied; re-entrancy check removed; running state not cleared on throw |
| 5, 6, 8 | Text scan of the store's headers (comments and literals ignored), with a self-test: no `unordered_`, `std::hash`, `<random>`, `<chrono>`, `<thread>`, `<atomic>`, `<mutex>`; no variable declared `static` or `thread_local` without `constexpr` (`static_cast`, `static_assert`, `static constexpr` and static member functions are not matches). `DeferredChanges` holds only its two lists and its running state: review | add a `std::unordered_map` index to `FindEntity`; add a function-local `static` cache |
| 7 | `GYO_CHECK_ASSERTS` on a present-key insert, a twice-recorded spawn, recording and applying during `Apply`; `EraseEntity` of an absent key returns false without asserting | remove each `GYO_ASSERT`; make absent erase assert |
| 9 | `gyo_expect_links(gyo_entity_store GYO::Base)` at configure time (the helper of `tests/common/time/CMakeLists.txt:22`, deferred as `tests/common/trace/CMakeLists.txt:20` does); the test executable links only `GYO::EntityStore` and `gyo_test_support` (itself `GYO::Base` and doctest, `tests/common/support/CMakeLists.txt:4-6`), so a store header that includes anything but the standard library and `engine/base` does not compile there | link `GYO::Math`; include an `engine/math` header |
| 10 | Through `EntityOf` and the column functions: a type with only `id` is keyed by `id`; a type with an `EntityKey` specialization and no `id` is keyed by it; a type with both an `id` member and a specialization returning another field is keyed by the specialization; a type with neither fails a `requires`-expression check | `EntityOf` reads the member `id` directly instead of going through `EntityKey` |

A consumer keeps its own guards for what stays its own (which columns an entity has, the single erase entry, the
frozen phases); the Delta lists them and the mutants that move here.

## Out of scope

- Id allotment, id ranges and their constants, exhaustion, kinds and the kind-to-columns table.
- The meaning of any id value, tie-breaking rules.
- A type that owns several columns, a multi-column erase, a registry.
- Systems, schedulers, a world, resources, context variables, events, messages, observers, hooks, an app, plugins.
- Queries, views, groups, a merge-join helper, sorting operations.
- Change ticks or change detection, generations, archetypes and tables, sparse sets, paging.
- Snapshots, serialization, reflection, replication and interest, digests.
- Parallel iteration and Jobs (a separate Engine capability).

## Design Invariants

1. **The key is given, never made.** The store's key is `EntityId`, an unsigned 64-bit integer the caller gives. The
   store never allots, recycles or reserves an id value, carries no generation bits, and knows no id range or entity
   kind; it relies on the caller's allotters never reusing an id. Reason: who allots ids, in which ranges and whether
   they may be reused is policy that differs between consumers; a store that allotted would own that policy.
   Evidence: survey §2.1 `Entity` row, §2.2 version-bits row, §4.2 Entity-key row, §5 Entity row (neither reference
   has a container that does not allot, §4.1 row 2); consumer: E-2, E-3, E-4.
2. **Order is an invariant, not an operation.** Every column is strictly ascending by key with no duplicates.
   `InsertEntity` inserts at the `lower_bound` position and `EraseEntity` preserves order; the store has no other
   function that changes membership (no append, swap-and-pop, tombstone, bulk assign or sort). Reason: an order that
   depends on ids alone is the same in every process holding the same ids, whatever happened before; an order that
   depends on insert and erase history is not. Evidence: survey §2.2 deletion-policy and `sort` rows, §3a, §5
   iteration row (both references offer id order only as an operation); consumer: E-5.
3. **Lookup is binary search on the column itself.** No sparse array, index, hash or other side table. Reason: a side
   table is a second copy of membership that can disagree with the column, and without id recycling a sparse array
   grows over dead ids. Evidence: survey §2.1 `SparseSet` row, §2.2 sparse-set row; consumer: E-5.
4. **Structural changes are deferred and applied in id order at the caller's sync point.** `DeferredChanges` records
   spawns with ids the caller already allotted and destroys; `Apply`, called by the caller, applies all spawns in
   ascending id, then all destroys in ascending id, and empties the lists. The store never decides when a sync point
   is, never allots at record time, and stores no closures. Reason: changing membership invalidates references into
   a column, and applying changes in recording order would make the result depend on the order rules ran. Evidence:
   survey §3b, §2.1 `Commands` row, §4.2 Commands row (the purpose of Bevy's Commands adapted, not ported);
   consumer: E-12.
5. **The store runs nothing.** No system, scheduler, world, resource, context variable, event, observer or hook, no
   app or plugin; the only code it calls is the two callbacks passed to `Apply`. Reason: the store is a data
   structure; who runs what and when belongs to its consumer, and the Engine has no umbrella framework
   (`docs/architecture.md:129` as amended by this Delta: umbrella framework, plugin ABI and service locator stay
   forbidden; only "ECS" is narrowed to this store). Evidence: survey §2.1 scheduler, `Resource`/`World`,
   `App`/`Plugin`, `Events`, `Observers` rows; §2.2 registry, context and signal rows.
6. **State authority** (AGENTS.md §3). The columns belong to the caller, and so does every `DeferredChanges`
   instance. The store's only state is what a `DeferredChanges` holds: its two pending lists and whether `Apply` is
   running. It caches nothing to decide on: no index, count, change tick, last-sent record, per-recipient state or
   interest set. Reason: anything cached to decide on is a copy of the consumer's state that can go stale. Evidence:
   survey §3c (change ticks not adopted in the first version, its reasons a to c), §2.1 change-detection row;
   consumer: E-8.
7. **Misuse is a Programmer Error; there are no Runtime Errors.** The store returns no `Result` and throws nothing
   itself; misuse fails a `GYO_ASSERT` (Contracts, Failures). An absent key is an ordinary result (`nullptr`,
   `false`). Reason: every input comes from the caller's own code, none from files, network or the environment.
   Evidence: [error handling](error-handling.md) rules 3 and 6; survey §2.2 error-handling row, §4.2 last row.
8. **Deterministic by construction.** No unordered container, hash, pointer comparison, clock, random source, thread
   or atomic in the store. Keys within each pending list are unique, so `Apply`'s order is a total order on ids.
   Reason: consumers compare results bit for bit across processes and platforms. Evidence: survey §3a, §2.1
   scheduler row; consumer: E-5, E-6, E-10.
9. **A leaf that knows no product.** `GYO::EntityStore` is header-only and links exactly `GYO::Base`; its headers
   include only the standard library and `engine/base`. No product, tool or kind name and no product type appears in
   it. Reason: a module below every product must build, be tested and be reused without any of them. Evidence:
   AGENTS.md §6 and §9; survey §2.1 `bevy_ecs` crate row, §4.2 first row; the `GYO::Math` leaf ([math](math.md)).
10. **The key is read in one way.** The store reads an element's key only through `EntityKey<T>` (via `EntityOf`):
    an owner's specialization if one exists, otherwise the member `id`. It names no other field and does no
    name lookup in a consumer's namespace. Reason: if the store named fields or relied on lookup rules, each
    consumer's naming would become Engine knowledge, and a type with two candidate fields would be ambiguous.
    Evidence: AGENTS.md §6; the consumer's transition field is mapped by that consumer (Delta §2).

### Replacement Criteria

When any of these holds, stop adding to the store and propose to the user what the criterion names instead of
patching. Thresholds come from the first consumer's approved values; each is measured by a consumer on its target
platform, never by the store.

1. A consumer must recycle ids within one allotter generation, or an allotter can no longer guarantee no reuse:
   propose a generation in the key and decide whether that consumer's digests encode it; Invariant 1 changes.
   Mirrors RC-2; survey §4.1 row 1.
2. Two or more consumers need the same allotment policy, or anyone asks the store to allot or to hold a second
   allotment policy: propose a separate Engine allotter beside the store, never inside it. Survey §4.1 row 2.
3. A consumer measures more than 4,000 simultaneous entities, or column maintenance (insert, erase, lookup, merges)
   above 1 ms p95 per tick: propose another structure (paging, an index) that keeps Invariant 2's order. Mirrors
   RC-1; survey §4.1 rows 3 and 12.
4. A consumer measures that its component combinations have become dynamic at run time for the majority of its
   live entities (more than half have a combination outside its fixed table), or that it has more than 16 kinds, or
   it needs archetype-style moves between tables: the column model no longer fits; propose a redesign, not an
   archetype bolted onto the store. Kinds and combinations are the consumer's values; the store does not know them.
   Mirrors RC-3; survey §4.1 row 3.
5. A consumer's RC-4 fires and the cause it reports is intersecting columns, or the same intersection (the same pair
   or tuple of columns joined by ascending id with the same skip rule) is hand-written in more than 3 places in one
   consumer, counted by that consumer: propose a query layer as its own component. Derived from RC-4; survey §4.1
   row 4.
6. A replicated channel becomes a delta against an acknowledged baseline and the consumer's "compare with the last
   sent value" exceeds 1 ms p95 per tick, or a non-replication consumer needs component-level change sets: propose
   change ticks, with a plan for marking every write point (survey §3c reason a). Survey §4.1 row 5.
7. A tick measured over budget needs parallel iteration proved bit-identical to ascending sequential order: use the
   Engine Job capability; propose no scheduler in the store. Survey §4.1 row 6.
8. A second product needs the same world, resource or app/plugin assembly around the store: it is a framework-layer
   question (`docs/architecture.md:129`); the plugin ABI ban must be revisited by the user first. Survey §4.1 rows
   7 and 8.
9. A named rule must read, in the same phase, an entity it just spawned, or must stop seeing one it just destroyed,
   and a fixed sync point cannot serve it: Invariant 4 is wrong; propose a redesign of the deferred changes, not
   events or observers. Survey §4.1 row 9.
10. A tool needs run-time component access that bypasses a data contract: AGENTS.md §8 must change first; no
    reflection is added here. Survey §4.1 row 10.
11. Clients and the authority need different id spaces (an id map): Invariant 1's precondition is gone. Mirrors
    RC-5; survey §4.1 row 11.
12. A column that needs no id order (diagnostics only, not digested, not intersected) makes ordered erase exceed
    criterion 3's threshold: propose a separate unordered container outside the store, not a removal policy flag.
    Survey §4.1 row 12.
13. A consumer needs the store to know kinds, id ranges, phases or which columns an entity has: that knowledge is the
    consumer's (Invariants 1, 5, 9); propose a consumer-side change, or a reviewed Delta if two consumers share it.
14. A test or an incident finds a defect caused by changing a store column without `InsertEntity`/`EraseEntity`
    (`push_back`, `swap`, assignment from an unsorted source): the free-function shape no longer protects
    Invariant 2; propose an owning column type whose membership changes only through the store. A cache the consumer
    rebuilds whole each tick in ascending order, or copies from an ordered column, is not a store column and not a
    bypass.
15. Adding a column type, a consumer or a feature requires changing this section's rule tests or re-validating the
    store's mutants beyond the mutants of the change itself: the store is not a stable leaf; propose rebuild or
    redesign instead of another patch.
