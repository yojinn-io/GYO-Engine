# GYO error handling

GYO separates failures by who can do something about them:

```text
Assert = Programmer Error
Result = Runtime Error
```

| | Programmer Error | Runtime Error |
|---|---|---|
| Meaning | The program is wrong: a broken precondition or internal invariant, API misuse, an unreachable branch | The program is right, but external data or the environment failed it |
| Mechanism | `GYO_ASSERT`, `GYO_UNREACHABLE` (`engine/base/Assert.hpp`) | `Engine::Base::Result<T, E>` (`engine/base/Result.hpp`) with the module's error type |
| Caller | Does not handle it; the fix is in the code | Must handle it (`[[nodiscard]]`) |
| When it happens | The assertion handler runs, by default printing the condition and aborting, in every build configuration | The function returns the error, carrying the module's own code |
| Shown to people | The handler prints the condition and its location | The error's code, message and detail |
| Tested with | `GYO_CHECK_ASSERTS(expr)` | Assertions on `error().code` |

These are not errors and use neither mechanism:

- **Plain absence** with a single obvious reason (a lookup that finds nothing): `std::optional`.
- **Several problems or warnings** to report at once (document validation, editor diagnostics): diagnostics data.
- **Normal outcomes of game rules** (a rejected action, a miss): the domain's own types.

Exceptions are not a third mechanism. See rule 6.

Status: this contract is being introduced by the [Assert/Result plan](plans/result-unification/README.md). The engine follows it; the UI editor's migration lands in a later batch. Until then the editor may still use the older forms listed in the plan.

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
5. **Translate at module boundaries without losing the code.** The outer module chooses its own code. Its message keeps the inner message verbatim (it may add its own context around it); the outer detail records the cause with `Base::CauseDetail(inner, outerDetail)`, which yields `<InnerCode>` or `<InnerCode>: <inner detail>`, after the outer module's own detail (such as a path) when there is one.
6. **Do not use exceptions to report failures from engine or tool APIs.** A module may throw internally (for example through a JSON library) only if it catches at its public boundary and returns a `Result`. Boundary catches name `std::exception`; never `catch (...)`, which would also swallow a test's `AssertionFailure`. A thread entry point that can meet a Runtime Error catches it itself.
7. **Present Runtime Errors through one formatter**, `Base::Describe`, instead of per-module formatting helpers.

## Assert

`GYO_ASSERT(condition)` is defined in `engine/base/include/engine/base/Assert.hpp` (target `GYO::Base`).

- It is evaluated in every build configuration, unlike `<cassert>`'s `assert`, which GYO code outside products does not use. The condition is evaluated exactly once and must have no side effects.
- On failure it calls the installed `AssertionHandler`. `SetAssertionHandler` installs one for the whole program and returns the previous one; `nullptr` selects the default, which prints `GYO_ASSERT failed: <condition>` with the file, line and function to stderr. If a handler returns, the process aborts; a failure inside a handler aborts immediately. Install a handler before starting threads. Product handlers may log; they must not throw.
- A function that contains `GYO_ASSERT` is not `noexcept`, and destructors and thread entry points contain none (the Lakos rule). Tests replace the handler with one that throws, and a throw out of a `noexcept` function terminates instead of reaching the test. Math functions that assert, such as `Clamp` and the functions that forward caller-provided bounds to it, are therefore not `noexcept`.
- In a `constexpr` function a failing `GYO_ASSERT` during constant evaluation is a compile error.
- Conditions may contain commas (`GYO_ASSERT(std::is_same_v<A, B>)`). GYO macros, this one included, assume a standard-conforming preprocessor on every compiler: `build/cmake/GyoBuild.cmake` passes `/Zc:preprocessor` to MSVC for all GYO code (engine, products, tools and tests; not `third_party`), and `Assert.hpp` stops with `#error` if MSVC's traditional preprocessor is active. Macros may therefore forward `__VA_ARGS__` and use `__VA_OPT__`.
- `GYO_UNREACHABLE()` marks a branch a correct program never reaches, such as the end of a switch that handles every enumerator; reaching it is reported like a failed assertion.
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

`Engine::Base::Result<T, E>` holds either a value or an error; `E` must satisfy `CodedError` (below), so a string or another uncoded type does not compile. Its interface is a subset of C++23 `std::expected`, with `Base::Err` in the role of `std::unexpected`:

```cpp
Base::Result<Path, IoError> Parse(std::string_view raw) {
    auto normalized = Normalize(raw);
    if (!normalized) return Base::Err(std::move(normalized).error());  // propagate
    return Path::FromNormalized(std::move(*normalized));               // success
}
Base::Result<void, IoError> Close() { /* ... */ return {}; }           // void success
```

- **Success**: return the value; it converts implicitly when `T` does. A braced value names its type (`return T{...};`); a non-void `Result` has no default constructor, so `return {};` cannot silently succeed with a default `T`.
- **Failure**: `return Base::Err(error);`. `Err<G>` converts to `Result<T, E>` whenever `E` is constructible from `G`. A bare `E` does not convert, so a forgotten `Err` is a compile error. A braced error names its type (`Base::Err(UiError{...})`).
- **Queries**: `has_value()` or `explicit operator bool` (success, also for `Result<bool, E>`), `value()`, `operator*`, `operator->`, `error()`. Reading the side that is not held is a Programmer Error and asserts. Unlike `std::expected`, nothing throws.
- `T` and `E` must differ and must not be references. The class is `[[nodiscard]]`.
- No monadic operations (`and_then`, `transform`) and no `value_or` until code needs them. Every error type `E` used with `Result` satisfies the `Base::CodedError` concept (`engine/base/Error.hpp`), checked by a `static_assert` next to its declaration:

- `code` is an enum owned by the module. Zero is not a valid code: each enum starts at 1, and `Error::Make` asserts against zero. The numeric values are not a data contract. A `ToString(code)` in the enum's namespace names each code.
- `message` is for people (logs, diagnostics) and is never parsed by code; tests compare `code`.
- `detail` is optional context, such as a path or, at a module boundary, the inner error's code name and detail.
- There is no default constructor, so an error value always describes a failure. A state that may or may not hold an error uses `std::optional<E>` (for example `AssetManager::GetError`).

`Engine::Base::Error<Code>` is the common error type: public `code`, `message` and `detail`, constructed with `Error<Code>::Make(code, message, detail)`. A module may define its own type when it needs more context; `Ui::UiError` adds the document `source` and `jsonPointer`. Each module declares its error type, and IO also `IoResult<T>`, exactly once next to its code enum. `Base::Describe(error)` formats any `CodedError` as `<CodeName>: <message>`, followed by ` (<detail>)` when there is a detail. GYO has no global error enum, no error chains and no type-erased error categories. Production code that needs to branch on a failure branches on the module's code (for example the VFS read overlay, which tries the next mount on `NotFound`).

Types that cannot be moved are returned as `Result<std::unique_ptr<T>, E>`.
