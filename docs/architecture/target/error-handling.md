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

## Relation to "no error chains" in the current contract (`v8-arch-a6-revision`; legacy-impact conflict 1)

The current contract's section on error types ends with "GYO has no global error enum, no error chains and no type-erased error categories". Under the kernel that sentence is **narrowed, not withdrawn**:

- `Engine::Base::Error<Code>` and every module error type carry **no chain** (unchanged): `code`, `message`, `detail`; at a module boundary the inner error's code name and detail go into `detail`.
- `KernelError` is the **one** error type that carries a chain. It has two directions, and the two words are kept apart everywhere (spec §0): the **context chain** is vertical, one error wrapped with one layer of context at each level on the way out (Kernel API, System, Executor); the **sibling list** is horizontal, independent errors of one System call (each access-verification violation, and the System's own `Err`) listed side by side and never wrapped into one another. Each sibling has its own context chain.
- "No global error enum" and "no type-erased error categories" hold for `KernelError` too: how its rule ids are declared is set by the declaration of `v8-arch-kernel-world-exec`.

The legacy file carries a marker on that sentence pointing here; its text is not rewritten until the kernel's migration batch replaces the file.

## Design Invariants

In force: approved by the user on 2026-11-13 (kernel HANDOFF `v8-arch-target-di-approved`). A change to this section is an Architecture Delta (AGENTS section 3) and needs the user's agreement.

Item numbers are unchanged. An item marked "bound by kernel DI-KS n" has its full text only in that clause of the kernel page; the others are specific to this component.

1. **A runtime-data Programmer Error is returned as `Result<void, KernelError>`, logged once by the Executor, and never silent**: bound by kernel [DI-KS 2](kernel.zh-Hant.md#di-ks-2): **full text and guards are in that clause; read it before changing this component**.
2. **`Base::Result` stays a pure value**: no logging, no handler inside it; the Executor's handler is the only place a kernel error is logged. Guard: review; spec J-9 scan of the kernel directory for log calls outside the Executor.

### Replacement Criteria

In force: approved by the user on 2026-11-13 (kernel HANDOFF `v8-arch-target-di-approved`).

- If a kernel error path needs to be recoverable (the run continues after an `Err`), this row is wrong for that path: it is a Runtime Error and belongs to the existing `Result` row with a module error type, not to this one.
