# Error handling under the kernel

> Status: target, not implemented. **Draft changes of `v8-arch-gyo2-error-levels`** (declaration `v2/error-levels-declaration.zh-Hant.md`, approved before work on 2026-11-20, narrowed in revision 7 to the levels, the naming rule, the standard use and D1/D2): the section "Error levels" and Design Invariants 1 to 5 below; their final text is approved before merge.

Updated 2026-11-11. Owner: architecture (architect session). Source: `docs/architecture/plans/kernel/spec.zh-Hant.md` §讀法 ("以 Err 回傳的 Programmer Error", formerly "Result 回報"), §E.5, K.3 item 17. Extends the current contract `docs/architecture/error-handling.md`; English because that file is English. The user approved this extension as an Architecture Delta on 2026-11-11; the current contract was amended in `v8-arch-kernel-spec-v03` (`docs/architecture/error-handling.md`, section "Programmer Error detected from runtime data"). 2026-11-12 (`v8-arch-kernel-verification-not-assert`): access verification (spec A.6) is not an assertion; its findings are returned as `Err` by the System's trampoline the same way; `GYO_ASSERT` stays always-on for cheap preconditions; the only mechanisms are these two.

## The added row

The current contract separates Programmer Error (`GYO_ASSERT`, the program is wrong) from Runtime Error (`Result<T, E>`, external data or the environment failed). The kernel adds a third row for a case the two do not cover: a **Programmer Error that can only be detected from runtime data**.

| | Programmer Error detected from runtime data |
|---|---|
| Meaning | The program is wrong, but the violation is visible only when an id arrives at runtime: an untyped `Commands` insert into a zone=C or Exclusive column, a `Despawn` of an entity carrying zone=C columns issued by a zone=B System, an `ExclusiveWorld` write outside zone=B Shared data (spec Z5, F-10, S-13) |
| Mechanism | The Kernel API returns `Result<void, KernelError>`; the error carries the full context (System name, Plan position, command kind, EntityId, ComponentId, violated rule id). Every System returns `Result<void, KernelError>` and wraps the error with its own context (Go's `%w` chain); it does not log and does not swallow. The outermost `Run` (or `Trigger`) marks the World `Interrupted`, ends the run and **returns the `Err` once**; the kernel itself logs nothing and has no error handler (error levels D1: once the orchestration layer and a runner sit above it, the Executor is no longer the outermost layer, so logging there as well as returning would handle the error twice). |
| In every build | The same in release: nothing is silent, and no build configuration behaves differently. **One event has one nature** (user decision 2026-11-12, `v8-arch-kernel-break-hook`, after Go): an error is either a `GYO_ASSERT` (precondition, always on, terminal) or a Programmer Error returned as `Err`; producing and returning an `Err` never calls the assertion handler or any terminal exit. Stopping on the spot is the startup parameter `breakOnKernelError` in `KernelSettings`: a returning break hook at the point the `KernelError` is constructed (debugger trap by default, no effect without a debugger), after which the `Err` is returned as usual; it is not an assertion handler and is present in release binaries too. References: Go returns error values, and at a unit-of-work boundary (`net/http`) ends that unit rather than the process ("handle once" itself is community practice, not in Go's official documentation; error levels declaration section 3.1); `panic` maps to `GYO_ASSERT`; Go deliberately has no assert and no debug/release split (Go FAQ); `runtime.Breakpoint()` behind a flag is this hook; `GODEBUG` is the shape of the runtime switch. Not copied: `GYO_ASSERT` cannot be recovered (C++ has no panic boundary), and GYO keeps a finer API-misuse / runtime-data split than Go's standard library |
| Final disposition | Still a Programmer Error: the fix is in the code. `Result<void, KernelError>` is the System's **return contract, not an error channel** (user, 2026-11-12): every System returns it on every run, Ok is the normal path, Err carries the diagnostic chain and does not mean the caller can recover |
| Tested with | Tests on the release target must observe the `Err` and the `Interrupted` state (spec J-32b, J-45); debug tests use `GYO_CHECK_ASSERTS` as before |

The user's principle behind this row: no silent handling. The earlier draft's "always-on check macro" was rejected in favour of this shape.

## Error levels (`v8-arch-gyo2-error-levels`; draft)

**Core principle** (user, 2026-11-20): a level is information. The error that goes back to the caller carries what happened and how bad it is, and goes back the way it always did, through `Result`. The level does not decide or enforce what the caller does; the caller decides.

Levels are relative to a unit (the process, a Product, a World, one request).

| Level | What happened | What is still intact | How it shows |
|---|---|---|---|
| **E0** program error | A precondition or an internal invariant broke: the program is wrong | nothing: the process stops | `GYO_ASSERT` (unchanged) |
| **E1** fatal | A World broke; the Product holding it cannot go on | the process and other Products | the kernel's `Err(KernelError)` from `Run`/`Trigger`; at the orchestration layer an error that carries a `WorldFailure` (`failure` set) |
| **E2** failure | This request did not succeed (external data or the environment) | everything else | `Result<T, …>` with the module's error; startup reports |
| **E3** warning | Something was dropped or limited by design (a trimmed message, a rejected spawn, discarded time) | everything | a record (a diagnostics record or a `Message`), not an error value |
| — not an error | No value; an outcome of game rules | — | `std::optional`, domain types |

Order of classification (extends rule 1 of `../error-handling.md`): a precondition or API misuse visible at the call → E0; a program error visible only from runtime data, or a World's Run that cannot end normally, or an empty World place → E1 for the Product holding that World; external data or the environment failed this request → E2; dropped by design → E3; no value or game rules → not an error.

Where the level of an error changes on its way: a System that returns `Err` declares that its World cannot go on (E2 → E1); a failed step of a `DropOldFirst` replacement leaves the place empty (E2 → E1); a World that breaks before a `BuildNewFirst` replacement hands it to the Product is not the Product's (E1 → E2 for the Product).

**Type names** (user, 2026-11-20): a new error type's name tells its level. `…Error` = E1, `…Failure` = E2. Both are returned to the caller; the suffix only tells how bad it is. Renaming existing types is a follow-up. GYO's `…Error` is not Java's `Error` (that is GYO's E0) and not Go's `XxxError` (a generic error type, mostly GYO's `…Failure`).

**How the Engine's own code uses this** (a description of the Engine, not a requirement on authors): the kernel, the orchestration layer and the Engine's Modules return their errors and write no log line of their own. The kernel's `Run` returns an `Err` once and has no error handler (D1); the orchestration layer returns its errors and does not log them (D2); E3 records are read by the logging Module. The exit flow: the default runner returns `RunnerExit` `Failure` with the reason (for E1 the `WorldFailure` text) and writes nothing. What follows (log it, exit, back to a menu with another `Product`) is decided by the product's own main; the Engine gives no standard main and no example of one; each product writes its main when it moves onto GYO2 (user, 2026-11-21).

## Relation to "no error chains" in the current contract (`v8-arch-a6-revision`; legacy-impact conflict 1)

The current contract's section on error types ends with "GYO has no global error enum, no error chains and no type-erased error categories". Under the kernel that sentence is **narrowed, not withdrawn**:

- `Engine::Base::Error<Code>` and every module error type carry **no chain** (unchanged): `code`, `message`, `detail`; at a module boundary the inner error's code name and detail go into `detail`.
- `KernelError` is the **one** error type that carries a chain. It has two directions, and the two words are kept apart everywhere (spec §0): the **context chain** is vertical, one error wrapped with one layer of context at each level on the way out (Kernel API, System, Executor); the **sibling list** is horizontal, independent errors of one System call (each access-verification violation, and the System's own `Err`) listed side by side and never wrapped into one another. Each sibling has its own context chain.
- "No global error enum" and "no type-erased error categories" hold for `KernelError` too: how its rule ids are declared is set by the declaration of `v8-arch-kernel-world-exec`.

The legacy file carries a marker on that sentence pointing here; its text is not rewritten until the kernel's migration batch replaces the file.

## Design Invariants

In force: approved by the user on 2026-11-13 (kernel HANDOFF `v8-arch-target-di-approved`). A change to this section is an Architecture Delta (AGENTS section 3) and needs the user's agreement.

Item numbers are unchanged. An item marked "bound by kernel DI-KS n" has its full text only in that clause of the kernel page; the others are specific to this component.

1. **A runtime-data Programmer Error is returned as `Result<void, KernelError>` once by the outermost `Run` (or `Trigger`), and is not silent**: the kernel itself writes no log line; the `Err` always goes back to the caller. Bound by kernel [DI-KS 2](kernel.zh-Hant.md#di-ks-2): **full text and guards are in that clause; read it before changing this component**. *(Draft, `v8-arch-gyo2-error-levels` D1; replaces "logged once by the Executor".)*
2. **`Base::Result` stays a pure value; the kernel has no error handler and no log call.** Guard: review; spec J-9 scan of the kernel directory for log calls (no exemption for the Executor any more). *(Draft, D1.)*
3. **Levels are information**: E0 to E3 and "not an error" as defined in "Error levels" tell what happened and how bad it is; they do not decide what the caller does. New error type names tell the level (`…Error` = E1, `…Failure` = E2). *(Draft; user, 2026-11-20.)*
4. **The Engine's own code returns and does not also log**: the kernel, the orchestration layer (its entry points and the default runner) and the Engine's Modules return their errors and write no log line of their own. Guard: the kernel and orchestration stderr probes (`kernel.el-silent`, `orchestration.el-silent`), `[or-runner-default-silent]`; review for the Modules. *(Draft, D1, D2, the exit flow.)*
5. **E3 is a record, not an error value**: a diagnostics record or a `Message`; it does not change the flow. *(Draft.)*

### Replacement Criteria

In force: approved by the user on 2026-11-13 (kernel HANDOFF `v8-arch-target-di-approved`).

- If a kernel error path needs to be recoverable (the run continues after an `Err`), this row is wrong for that path: it is a Runtime Error and belongs to the existing `Result` row with a module error type, not to this one.
