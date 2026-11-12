# Error handling under the kernel

> Status: target, not implemented

Updated 2026-11-11. Owner: architecture (architect session). Source: `docs/architecture/plans/kernel/spec.zh-Hant.md` §讀法 ("以 Err 回傳的 Programmer Error", formerly "Result 回報"), §E.5, K.3 item 17. Extends the current contract `docs/architecture/error-handling.md`; English because that file is English. The user approved this extension as an Architecture Delta on 2026-11-11; the current contract was amended in `v8-arch-kernel-spec-v03` (`docs/architecture/error-handling.md`, section "Programmer Error detected from runtime data"). 2026-11-12 (`v8-arch-kernel-verification-not-assert`): access verification (spec A.6) is not an assertion; its findings are returned as `Err` by the System's trampoline the same way; `GYO_ASSERT` stays always-on for cheap preconditions; the only mechanisms are these two.

## The added row

The current contract separates Programmer Error (`GYO_ASSERT`, the program is wrong) from Runtime Error (`Result<T, E>`, external data or the environment failed). The kernel adds a third row for a case the two do not cover: a **Programmer Error that can only be detected from runtime data**.

| | Programmer Error detected from runtime data |
|---|---|
| Meaning | The program is wrong, but the violation is visible only when an id arrives at runtime: an untyped `Commands` insert into a zone=C or Exclusive column, a `Despawn` of an entity carrying zone=C columns issued by a zone=B System, an `ExclusiveWorld` write outside zone=B Shared data (spec Z5, F-10, S-13) |
| Mechanism | The Kernel API returns `Result<void, KernelError>`; the error carries the full context (System name, Plan position, command kind, EntityId, ComponentId, violated rule id). Every System returns `Result<void, KernelError>` and wraps the error with its own context (Go's `%w` chain); it does not log and does not swallow. The Executor is the outermost layer and **handles once**: it logs the whole chain once (the log switch lives in the Executor's handler; `Base::Result` stays a pure value), marks the World `Interrupted`, and ends the run |
| In every build | The same in release: nothing is silent, and no build configuration behaves differently. **One event has one nature** (user decision 2026-11-12, `v8-arch-kernel-break-hook`, after Go): an error is either a `GYO_ASSERT` (precondition, always on, terminal) or a Programmer Error returned as `Err`; producing and returning an `Err` never calls the assertion handler or any terminal exit. Stopping on the spot is the startup parameter `breakOnKernelError` in `KernelSettings`: a returning break hook at the point the `KernelError` is constructed (debugger trap by default, no effect without a debugger), after which the `Err` is returned as usual; it is not an assertion handler and is present in release binaries too. References: Go returns error values and handles them once at the outermost layer, ending the unit of work rather than the process; `panic` maps to `GYO_ASSERT`; Go deliberately has no assert and no debug/release split (Go FAQ); `runtime.Breakpoint()` behind a flag is this hook; `GODEBUG` is the shape of the runtime switch. Not copied: `GYO_ASSERT` cannot be recovered (C++ has no panic boundary), and GYO keeps a finer API-misuse / runtime-data split than Go's standard library |
| Final disposition | Still a Programmer Error: the fix is in the code. `Result<void, KernelError>` is the System's **return contract, not an error channel** (user, 2026-11-12): every System returns it on every run, Ok is the normal path, Err carries the diagnostic chain and does not mean the caller can recover |
| Tested with | Tests on the release target must observe the `Err` and the `Interrupted` state (spec J-32b, J-45); debug tests use `GYO_CHECK_ASSERTS` as before |

The user's principle behind this row: no silent handling. The earlier draft's "always-on check macro" was rejected in favour of this shape.

## Design Invariants (candidates; become binding once approved)

1. **A runtime-data Programmer Error is reported as `Result<void, KernelError>` with full context, wrapped layer by layer (verification violations and the System's own `Err` are siblings in one error list, not nested), logged exactly once by the Executor, and ends in `Interrupted`; no build configuration swallows it.** Guards: spec J-32b, J-45, J-35.
2. **`Base::Result` stays a pure value**: no logging, no handler inside it; the Executor's handler is the only place a kernel error is logged. Guard: review; spec J-9 scan of the kernel directory for log calls outside the Executor.

### Replacement Criteria (candidates)

- If a kernel error path needs to be recoverable (the run continues after an `Err`), this row is wrong for that path: it is a Runtime Error and belongs to the existing `Result` row with a module error type, not to this one.
