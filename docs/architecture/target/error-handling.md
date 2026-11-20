# Error handling under the kernel

> Status: target, not implemented. **Draft changes of `v8-arch-gyo2-error-levels`** (declaration `v2/error-levels-declaration.zh-Hant.md`, approved before work on 2026-11-20, narrowed in revision 7 to the levels, the naming rule, the standard use and D1/D2): the section "Error levels" and Design Invariants 1, 2 and 3 to 7 below; their final text is approved before merge.

Updated 2026-11-11. Owner: architecture (architect session). Source: `docs/architecture/plans/kernel/spec.zh-Hant.md` §讀法 ("以 Err 回傳的 Programmer Error", formerly "Result 回報"), §E.5, K.3 item 17. Extends the current contract `docs/architecture/error-handling.md`; English because that file is English. The user approved this extension as an Architecture Delta on 2026-11-11; the current contract was amended in `v8-arch-kernel-spec-v03` (`docs/architecture/error-handling.md`, section "Programmer Error detected from runtime data"). 2026-11-12 (`v8-arch-kernel-verification-not-assert`): access verification (spec A.6) is not an assertion; its findings are returned as `Err` by the System's trampoline the same way; `GYO_ASSERT` stays always-on for cheap preconditions; the only mechanisms are these two.

## The added row

The current contract separates Programmer Error (`GYO_ASSERT`, the program is wrong) from Runtime Error (`Result<T, E>`, external data or the environment failed). The kernel adds a third row for a case the two do not cover: a **Programmer Error that can only be detected from runtime data**.

| | Programmer Error detected from runtime data |
|---|---|
| Meaning | The program is wrong, but the violation is visible only when an id arrives at runtime: an untyped `Commands` insert into a zone=C or Exclusive column, a `Despawn` of an entity carrying zone=C columns issued by a zone=B System, an `ExclusiveWorld` write outside zone=B Shared data (spec Z5, F-10, S-13) |
| Mechanism | The Kernel API returns `Result<void, KernelError>`; the error carries the full context (System name, Plan position, command kind, EntityId, ComponentId, violated rule id). Every System returns `Result<void, KernelError>` and wraps the error with its own context (Go's `%w` chain); it does not log and does not swallow. The outermost `Run` (or `Trigger`) marks the World `Interrupted`, ends the run and **returns the `Err` once**; the kernel itself logs nothing and has no error handler (error levels D1: once the orchestration layer and a runner sit above it, the Executor is no longer the outermost layer, so logging there as well as returning would handle the error twice). Whoever handles it logs it once (DI 4, 5) |
| In every build | The same in release: nothing is silent, and no build configuration behaves differently. **One event has one nature** (user decision 2026-11-12, `v8-arch-kernel-break-hook`, after Go): an error is either a `GYO_ASSERT` (precondition, always on, terminal) or a Programmer Error returned as `Err`; producing and returning an `Err` never calls the assertion handler or any terminal exit. Stopping on the spot is the startup parameter `breakOnKernelError` in `KernelSettings`: a returning break hook at the point the `KernelError` is constructed (debugger trap by default, no effect without a debugger), after which the `Err` is returned as usual; it is not an assertion handler and is present in release binaries too. References: Go returns error values, and at a unit-of-work boundary (`net/http`) ends that unit rather than the process ("handle once" itself is community practice, not in Go's official documentation; error levels declaration section 3.1); `panic` maps to `GYO_ASSERT`; Go deliberately has no assert and no debug/release split (Go FAQ); `runtime.Breakpoint()` behind a flag is this hook; `GODEBUG` is the shape of the runtime switch. Not copied: `GYO_ASSERT` cannot be recovered (C++ has no panic boundary), and GYO keeps a finer API-misuse / runtime-data split than Go's standard library |
| Final disposition | Still a Programmer Error: the fix is in the code. `Result<void, KernelError>` is the System's **return contract, not an error channel** (user, 2026-11-12): every System returns it on every run, Ok is the normal path, Err carries the diagnostic chain and does not mean the caller can recover |
| Tested with | Tests on the release target must observe the `Err` and the `Interrupted` state (spec J-32b, J-45); debug tests use `GYO_CHECK_ASSERTS` as before |

The user's principle behind this row: no silent handling. The earlier draft's "always-on check macro" was rejected in favour of this shape.

## Error levels (`v8-arch-gyo2-error-levels`; draft)

**Core principle** (user, 2026-11-20): a level only tells the outside what happened and how bad it is; it does not prescribe what the receiver must do. What follows under "Standard use" is the Engine's standard use, a reference for authors: the Engine's own code (the default runner, `WithFailureLog`, the Engine's Modules) follows it and tests and guards check it; an author may do otherwise. "You cannot stop people writing bad code, but the norm is here."

Levels are relative to a unit (the process, a Product, a World, one request). When an error crosses a boundary, the receiver judges it again by "is what broke something I hold?"; its code and content do not change.

| Level | What it means | Can still be trusted | Carrier |
|---|---|---|---|
| **E0** program error | A precondition or an internal invariant broke; the program is wrong. An exception nobody catches (a thread body, a throw out of `noexcept`) ends in `std::terminate` and is E0 too | nothing: the process stops | `GYO_ASSERT` (no type) |
| **E1** fatal | A World broke and the unit holding it (its Product) ends; the process goes on. Sources: a Programmer Error detected from runtime data (the row above), an exception leaving a World's Run, resources exhausted in a Run or a `DropOldFirst` replacement, an empty place after a failed `DropOldFirst`. Precondition: a World's state belongs only to the Product holding it | the process and other Products | returned as a value: the kernel's `Err(KernelError)` from `Run`/`Trigger`; at the orchestration layer an error that carries a `WorldFailure` (`failure` set; Go's `errors.As` shape: the level is read from what the error carries). An exception from author code passes through unchanged after the failure is recorded |
| **E2** failure | This request did not succeed (external data or the environment); everything the requester holds is intact. Startup reports belong here | everything | `Result<T, …Failure>`; startup reports (`StartupReport`, `ProductStartupReport`) |
| **E3** warning | Something was dropped or limited by design (a trimmed message, a rejected spawn, discarded time); the flow does not change | everything | a record (a diagnostics record or a `Message`), not an error value |
| — not an error | No value; an outcome of game rules; a designed precedence | — | `std::optional`, domain types |

Order of classification (extends rule 1 of `../error-handling.md`): a precondition or API misuse visible at the call → E0; a program error visible only from runtime data, a Run that cannot end normally, or an empty World place → E1 for the unit holding that World; external data or the environment failed this request → E2; dropped by design → E3; no value, game rules, designed precedence → not an error. A name that does not exist in a call from outside a World (an orchestration entry point) is API misuse → E0; callers whose names come from data ask first (`HasWorld`). Inside a System the same kind of mistake is E1 by DI-KS 2 (X-12 `UnknownSchedule`).

Levels change only in these ways: a System wrapping an E2 and returning `Err` (E2 → E1: it declares the World cannot go on); any failed step of a `DropOldFirst` replacement (E2 → E1: the old World is already gone); a failure of the author's own fixed input at initialization or registration (may be asserted, → E0, as Go's `Must…`); a World that breaks before it is handed to the Product in a `BuildNewFirst` replacement (E1 → E2 for the Product). Nothing goes from E1 back to E2 for a World the Product holds.

**Type names** (user, 2026-11-20): an error type's name carries its level. `…Error` = E1 (cannot be saved: a World broke, its holder ends); `…Failure` = E2 (this request failed; the caller decides). The rule applies to new types; renaming existing types (`OrchestrationError`, `WorldFailure`, the modules' `…Error`) is a follow-up. GYO's `…Error` is not Java's `Error` (that is GYO's E0, "should not try to catch") and not Go's `XxxError` (a generic error type, mostly GYO's `…Failure`). Neither Java nor Go encodes levels in names; GYO does, and the level is carried by the type as well, not by the name alone. `Result::error()`, `Base::Err` and `…Report` keep their names.

**Standard use** (a reference for authors; the Engine's own code follows it):

| | E0 | E1 | E2 | E3 |
|---|---|---|---|---|
| Producer returns | nothing: the assertion handler, then `abort` | kernel: `Run`/`Trigger` return `Err(KernelError)`, the World is `Interrupted`; orchestration: the entry point's error carries `failure` (a `WorldFailure`) | `Result<T, …Failure>`; a startup report | a diagnostics record or a `Message` |
| Standard handling | none (fix the code) | the call site that receives it stops calling the Product's entry points and ends the runner; reading the World is still allowed (diagnostics, unsaved content). An exception leaving an entry point: at the runner's own boundary, `Failure()` set → as E1, otherwise rethrow | pass it on with context, or handle it here (decide, degrade, take another path, show it) — one of the two | none |
| Logged by | the assertion handler | **one place**: where the runner ends (`WithFailureLog`, or `main`), reading `Failure()` once. The kernel, the orchestration layer and Modules do not log. A failed log write still ends the runner (no retry, no escalation). A normal end is not E1 | only the handler, which may choose not to | the logging Module (subscribing to `Message`s, taking diagnostics) |
| Log level | `Fatal` | `Fatal`, category `world` | the handler's choice (`Error` when it cannot be done and it affects the user; `Warning` or nothing when there is a fallback) | `Warning` (or `Info`) |

**Handle once** (GYO's principle; the community practice of Go's "handle errors once", not an official rule of Go or Java, whose official documents show only "handle it at the boundary, log it once"): an error is either passed on (with context) or handled here, not both. Producers and those who pass it on do not log; the handler logs at most once; an error reaches its handler by one channel (a channel delivers it; a broken unit's recorded cause, read once by its single recorder, is not one); "the same error" is decided by identity, never by content. An E2 whose producer already wrote a diagnostics record by design (`AllottedIdAlive` → `RejectedSpawn`) is not logged again by its handler. With several failures at once, the one that broke the World is the main one and later clean-up failures are attached to it, not logged separately. A cause wrapped into an error is for people (`detail`, like Go's `%v`) unless callers are meant to branch on it; then it is a public field and part of the contract (`WorldError::cause`).

## Relation to "no error chains" in the current contract (`v8-arch-a6-revision`; legacy-impact conflict 1)

The current contract's section on error types ends with "GYO has no global error enum, no error chains and no type-erased error categories". Under the kernel that sentence is **narrowed, not withdrawn**:

- `Engine::Base::Error<Code>` and every module error type carry **no chain** (unchanged): `code`, `message`, `detail`; at a module boundary the inner error's code name and detail go into `detail`.
- `KernelError` is the **one** error type that carries a chain. It has two directions, and the two words are kept apart everywhere (spec §0): the **context chain** is vertical, one error wrapped with one layer of context at each level on the way out (Kernel API, System, Executor); the **sibling list** is horizontal, independent errors of one System call (each access-verification violation, and the System's own `Err`) listed side by side and never wrapped into one another. Each sibling has its own context chain.
- "No global error enum" and "no type-erased error categories" hold for `KernelError` too: how its rule ids are declared is set by the declaration of `v8-arch-kernel-world-exec`.

The legacy file carries a marker on that sentence pointing here; its text is not rewritten until the kernel's migration batch replaces the file.

## Design Invariants

In force: approved by the user on 2026-11-13 (kernel HANDOFF `v8-arch-target-di-approved`). A change to this section is an Architecture Delta (AGENTS section 3) and needs the user's agreement.

Item numbers are unchanged. An item marked "bound by kernel DI-KS n" has its full text only in that clause of the kernel page; the others are specific to this component.

1. **A runtime-data Programmer Error is returned as `Result<void, KernelError>` once by the outermost `Run` (or `Trigger`), and is not silent**: the kernel never logs; it is not silent because the `Err` always reaches the caller (`[[nodiscard]]`) and its handler logs it once (DI 4, 5). As in Go, not handling it is wrong by the norm, not refused by the compiler. Bound by kernel [DI-KS 2](kernel.zh-Hant.md#di-ks-2): **full text and guards are in that clause; read it before changing this component**. *(Draft, `v8-arch-gyo2-error-levels` D1; replaces "logged once by the Executor".)*
2. **`Base::Result` stays a pure value; the kernel has no error handler and no log call.** Guard: review; spec J-9 scan of the kernel directory for log calls (no exemption for the Executor any more). *(Draft, D1.)*
3. **Levels**: E0 to E3 and "not an error" as defined in "Error levels": relative to a unit, the order of classification, and only the listed level changes; E1's precondition (a World's state belongs only to the Product holding it); asserting the author's own fixed input as E0 only at initialization or registration. New error type names follow the suffix rule (`…Error` = E1, `…Failure` = E2), which is not Java's or Go's meaning. *(Draft.)*
4. **Handle once**: producers and those who pass an error on do not log; the handler logs at most once; one channel to the handler; identity, not content, decides "the same error". No de-duplication, "already written" flag or content comparison is used to avoid repeats. *(Draft.)*
5. **E1's single record**: in the Engine's standard use, E1 is logged once where the runner ends, by reading `Failure()`; call sites that receive it only stop; the kernel, the orchestration layer and Modules do not log it; a failed write still ends the runner; a normal end is not E1. *(Draft.)*
6. **E3 is a record, not an error value**: a diagnostics record or a `Message`; it does not change the flow. *(Draft.)*
7. **Levels are information; handling is standard use**: a level tells the outside what happened and how bad it is; the standard handling is a reference that the Engine's own code follows, checked by tests and guards; the Engine does not enforce an author's handling at run time. *(Draft; user, 2026-11-20.)*

### Replacement Criteria

In force: approved by the user on 2026-11-13 (kernel HANDOFF `v8-arch-target-di-approved`).

- If a kernel error path needs to be recoverable (the run continues after an `Err`), this row is wrong for that path: it is a Runtime Error and belongs to the existing `Result` row with a module error type, not to this one.
