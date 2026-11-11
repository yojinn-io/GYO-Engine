# Error handling under the kernel

> Status: target, not implemented

Updated 2026-11-11. Owner: architecture (architect session). Source: `docs/architecture/plans/kernel/spec.zh-Hant.md` §讀法 ("Result 回報"), §E.5, K.3 item 17. Extends the current contract `docs/architecture/error-handling.md`; English because that file is English. The user approved this extension as an Architecture Delta on 2026-11-11; the edit to the current contract is scheduled for the first documentation batch of the migration.

## The added row

The current contract separates Programmer Error (`GYO_ASSERT`, the program is wrong) from Runtime Error (`Result<T, E>`, external data or the environment failed). The kernel adds a third row for a case the two do not cover: a **Programmer Error that can only be detected from runtime data**.

| | Programmer Error detected from runtime data |
|---|---|
| Meaning | The program is wrong, but the violation is visible only when an id arrives at runtime: an untyped `Commands` insert into a zone=C or Exclusive column, a `Despawn` of an entity carrying zone=C columns issued by a zone=B System, an `ExclusiveWorld` write outside zone=B Shared data (spec Z5, F-10, S-13) |
| Mechanism | The Kernel API returns `Result<void, KernelError>`; the error carries the full context (System name, Plan position, command kind, EntityId, ComponentId, violated rule id). Every System returns `Result<void, KernelError>` and wraps the error with its own context (Go's `%w` chain); it does not log and does not swallow. The Executor is the outermost layer and **handles once**: it logs the whole chain once (the log switch lives in the Executor's handler; `Base::Result` stays a pure value), marks the World `Interrupted`, and ends the run |
| In every build | The same in release: nothing is silent. In debug the assertion handler is additionally called at the point the error is recorded, so the failure stops on the spot |
| Final disposition | Still a Programmer Error: the fix is in the code. `Result` is the carrier of the diagnostic chain here, not a sign that the caller can recover |
| Tested with | Tests on the release target must observe the `Err` and the `Interrupted` state (spec J-32b, J-45); debug tests use `GYO_CHECK_ASSERTS` as before |

The user's principle behind this row: no silent handling. The earlier draft's "always-on check macro" was rejected in favour of this shape.

## Design Invariants (candidates; become binding once approved)

1. **A runtime-data Programmer Error is reported as `Result<void, KernelError>` with full context, wrapped layer by layer, logged exactly once by the Executor, and ends in `Interrupted`; no build configuration swallows it.** Guards: spec J-32b, J-45, J-35.
2. **`Base::Result` stays a pure value**: no logging, no handler inside it; the Executor's handler is the only place a kernel error is logged. Guard: review; spec J-9 scan of the kernel directory for log calls outside the Executor.

### Replacement Criteria (candidates)

- If a kernel error path needs to be recoverable (the run continues after an `Err`), this row is wrong for that path: it is a Runtime Error and belongs to the existing `Result` row with a module error type, not to this one.
