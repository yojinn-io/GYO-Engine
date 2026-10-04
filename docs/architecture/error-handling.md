# GYO error handling

GYO separates failures by who can do something about them:

```text
Assert = Programmer Error
Result = Runtime Error
```

| | Programmer Error | Runtime Error |
|---|---|---|
| Meaning | The program is wrong: a broken precondition or internal invariant, API misuse, an unreachable branch | The program is right, but external data or the environment failed it |
| Mechanism | `GYO_ASSERT` (`engine/base/Assert.hpp`) | `Engine::Base::Result<T, E>` (`engine/base/Result.hpp`) with the module's error type |
| Caller | Does not handle it; the fix is in the code | Must handle it (`[[nodiscard]]`) |
| When it happens | The assertion handler runs, by default printing the condition and aborting, in every build configuration | The function returns the error, carrying the module's own code |
| Shown to people | The handler prints the condition and its location | The error's code, message and detail |
| Tested with | `GYO_CHECK_ASSERTS(expr)` | Assertions on `error().code` |

These are not errors and use neither mechanism:

- **Plain absence** with a single obvious reason (a lookup that finds nothing): `std::optional`.
- **Several problems or warnings** to report at once (document validation, editor diagnostics): diagnostics data.
- **Normal outcomes of game rules** (a rejected action, a miss): the domain's own types.

Exceptions are not a third mechanism. See rule 6.

Status: this contract is being introduced by the [Assert/Result plan](plans/result-unification/README.md). `GYO_ASSERT` and `GYO::Base` exist; Result's final construction syntax, the `CodedError` convention, `Base::Describe` and the remaining migrations land in later batches. Until then existing code may still use the older forms listed in the plan.

## Rules

1. **Classify each failure in this order.**
   1. Does it mean the program is wrong? Use `GYO_ASSERT`.
   2. Can a correct program fail here because of external data or the environment? Return a `Result` at the *first* point that validates that data; later re-checks of already validated data go back to step 1.
   3. Is it only "not there", with one obvious reason? Use `std::optional`.
   4. Are there several problems or warnings to report? Return diagnostics.
   5. Is it a normal outcome of game rules? Use a domain type.
2. **Data-validation APIs (wide contracts) return Result.** Some engine APIs are themselves the first validation point for data that comes from content or callers' input, for example `RenderQueue::Submit`, UI sprite clipping and the model animation API. Bad input there is a Runtime Error and they return a `Result` with an `InvalidArgument`-style code. The plan keeps a list of them.
3. **API misuse is a Programmer Error.** Calling a function in the wrong state (before initialization, without an active canvas), registering a null or duplicate loader, or passing reserved request fields are assertions, not results. The one documented exception is the SDL GPU device: `WrongThread`, `InvalidHandle` and frame-state errors stay `Result`, because they are the GPU backend's external contract and `WrongThread` is detected on another thread.
4. **Propagate within a module unchanged**: return the callee's error as it is.
5. **Translate at module boundaries without losing the code.** The outer module chooses its own code. Its message keeps the inner message verbatim; the inner code name, and the inner detail if any, go into the outer detail as `<InnerCode>: <inner detail>`.
6. **Do not use exceptions to report failures from engine or tool APIs.** A module may throw internally (for example through a JSON library) only if it catches at its public boundary and returns a `Result`. Boundary catches name `std::exception`; never `catch (...)`, which would also swallow a test's `AssertionFailure`. A thread entry point that can meet a Runtime Error catches it itself.
7. **Present Runtime Errors through one formatter** (`Base::Describe`, once it lands) instead of per-module formatting helpers.

## Assert

`GYO_ASSERT(condition)` is defined in `engine/base/include/engine/base/Assert.hpp` (target `GYO::Base`).

- It is evaluated in every build configuration, unlike `<cassert>`'s `assert`, which GYO code outside products does not use. The condition is evaluated exactly once and must have no side effects.
- On failure it calls the installed `AssertionHandler`. `SetAssertionHandler` installs one for the whole program and returns the previous one; `nullptr` selects the default, which prints `GYO_ASSERT failed: <condition>` with the file, line and function to stderr. If a handler returns, the process aborts; a failure inside a handler aborts immediately. Install a handler before starting threads. Product handlers may log; they must not throw.
- A function that contains `GYO_ASSERT` is not `noexcept`, and destructors and thread entry points contain none (the Lakos rule). Tests replace the handler with one that throws, and a throw out of a `noexcept` function terminates instead of reaching the test. Math functions that assert, such as `Clamp` and the functions that forward caller-provided bounds to it, are therefore not `noexcept`.
- In a `constexpr` function a failing `GYO_ASSERT` during constant evaluation is a compile error.
- Conditions may contain commas (`GYO_ASSERT(std::is_same_v<A, B>)`). GYO macros, this one included, assume a standard-conforming preprocessor on every compiler: `build/cmake/GyoBuild.cmake` passes `/Zc:preprocessor` to MSVC for all GYO code (engine, products, tools and tests; not `third_party`), and `Assert.hpp` stops with `#error` if MSVC's traditional preprocessor is active. Macros may therefore forward `__VA_ARGS__` and use `__VA_OPT__`.
- There is no debug-only level yet. `GYO_DEBUG_ASSERT`, for checks too expensive to run in release, will be added together with the first such check.

Tests include `tests/common/support/AssertTestSupport.hpp` (target `gyo_test_support`). `GYO_CHECK_ASSERTS(expr)` installs a throwing handler for the duration of the expression and checks that it failed an assertion; outside it the default handler stays installed, so an unexpected Programmer Error aborts the test executable. `AssertionFailure` deliberately does not derive from `std::exception`. The header also disables the Windows abort dialog. That the default handler really aborts is checked by `gyo_assert_abort_probe` under `cmake -P` (`tests/common/base`).

### Mapping to C++26 contracts

| GYO | C++26 |
|---|---|
| `GYO_ASSERT(cond)` | `contract_assert(cond)` with the enforce semantic |
| `SetAssertionHandler` | the replaceable contract-violation handler |
| `AssertionFailure::expression`, `location` | `contract_violation::comment()`, `location()` |

C++26 has no run-time function to swap the handler and chooses semantics per build rather than per check, so a migration is mechanical for the checks but not for the test helpers.

## Result and errors

`Engine::Base::Result<T, E>` holds either a value or an error. Reading the side that is not held (`value()` on an error, `error()` on a value) is a Programmer Error and asserts. `Engine::Base::Error<Code>` carries an enum `code`, a human-readable `message` and an optional `detail`; each module owns its code enum. GYO has no global error enum, no error chains and no type-erased error categories. Production code that needs to branch on a failure branches on the module's code (for example the VFS read overlay, which tries the next mount on `NotFound`).

Types that cannot be moved are returned as `Result<std::unique_ptr<T>, E>`.
