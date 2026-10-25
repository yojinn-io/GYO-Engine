# GYO OS layer

Owner: Engine (`engine/os` → `GYO::Os`).
Plan and record: [os-subsystem](plans/os-subsystem/README.md), batch os-skeleton onwards (D40–D46).

## What it is

The only Engine code that knows which operating system it runs on. Every other Engine module, every product and
every tool reaches the operating system through neutral APIs, so none of them branches on the OS.

```text
products / tools ──┬──→ engine/asset ──→ engine/io ──→ engine/os ──→ engine/base
                   ├──→ engine/trace ──→ engine/threads ──→ engine/time ──→ engine/os
                   ├──→ engine/platform/sdl ──→ engine/os, SDL
                   └──→ engine/os (process facts, console)
```

`GYO::Os` is a leaf directly above `GYO::Base`. Modules keep their own abstractions and call it: engine/io keeps
`IFileSystem`, `Vfs`, `Uri` and streams, and its native file-system backend calls the file primitives here;
engine/time's `Waiter` and engine/threads' `RoleThread` keep their public APIs and use the wait and thread
primitives here.

## Components

| Component | What it gives | Status |
|---|---|---|
| `OsError` | The layer's error; the detail keeps the native error code | os-skeleton (landed) |
| Files | Read primitives with UTF-8 paths (`ReadFile`, `Stat`, `Exists`, `MakeAbsolute`, `PathToUtf8`/`PathFromUtf8`); later write, create, atomic replace | read: os-files-read (landed); write: os-files-write |
| System | System font directories | os-tool-files |
| Process | Executable path, process and thread CPU time, local calendar time | os-process-info |
| Console | UTF-8 command line, UTF-8 console output, shutdown requests | os-console-shutdown |
| Thread | Thread name and priority hint for the current thread | os-threads-backend |
| Wait | The wake primitive behind `Time::Waiter` | os-wait-backend |
| Net (conditional) | Neutral classification of socket error codes | os-net-errors |

Only the components whose batch has landed exist; the others are planned in the os-subsystem plan.

## Contracts

- **Errors.** `Base::Result<T, OsError>`; `OsErrorCode` starts at 1. The detail is `errno=<n>` or `win32=<n>`,
  followed by `; path_bytes=<n>` when the failure concerns a path (`Os::NativeErrorDetail`). An argument the layer
  refuses itself, before or instead of an OS call (an empty path, a NUL, invalid UTF-8, an offset or length out of
  range, a file that is not a regular file), has no native error: its detail is `path_bytes=<n>` alone; no native
  code is made up. An outer module keeps the detail with `Base::CauseDetail` (error-handling.md rule 5).
- **Files.**
  - A missing path is `NotFound`, and `Exists` is false (also for a missing parent and a dangling link).
  - Only regular files open: a directory, a FIFO or a device is `InvalidPath` on every platform. POSIX opens with
    `O_NONBLOCK`, so a FIFO is refused at once instead of waiting for a writer.
  - `Stat` and `Exists` follow symbolic links on every platform. On Windows a reparse point is opened and its
    target described, as POSIX `stat` does.
  - Reads may return fewer bytes than asked; 0 for a non-empty span is the end of the file; an empty span returns 0
    without an OS call. Seeking past the end is allowed and reads nothing.
  - A moved-from `ReadFile` may only be destroyed or assigned to; anything else is a Programmer Error
    (`GYO_ASSERT`).
  - `MakeAbsolute` resolves against the current directory the platform's own way: `GetFullPathNameW` on Windows
    also removes dot segments and trailing dots and spaces; POSIX joins the current directory as it is. It answers
    in `/` form and never returns a long-path prefix.
  - `IsAbsolutePath` answers per platform as a pure string rule: `/x` on POSIX; `C:/x`, `C:\x` or a `\\` or `//`
    prefix on Windows.
  - `PathToUtf8` and `PathFromUtf8` return `InvalidEncoding` for a path with no UTF-8 form (an unpaired surrogate
    on Windows, bytes that are not UTF-8 on POSIX) and for text that is not UTF-8 (D47); they never throw.
  - Write times are 0 when the file system keeps none and saturate outside the range of `std::chrono::nanoseconds`.
- **Paths and text.** UTF-8 in `std::string_view`, UTF-8 out in `std::string`. Invalid UTF-8 is `InvalidEncoding`.
  On Windows the layer converts to UTF-16 and calls the wide APIs. Long-path forms (`\\?\`, `\\?\UNC\`) are made
  inside the layer just before the OS call; they are never returned or written into an error.
- **Data and time.** Bytes are `std::span<std::byte>`; durations and time points are `std::chrono`.
- **Policy stays with the caller.** Which socket errors are transient, what to do on a shutdown request and how to
  format a time stamp are the caller's decisions.

## Guard

`tests/common/os/os_boundary_guard.py` (CTest `os.boundary_guard`, self-test `os.boundary_guard_selftest`) checks
Invariants 1–3 as text, so the result is the same on every platform:

- **R1**: an OS header in an `#include` line (Windows SDK and CRT headers, POSIX headers, `sys/`, `mach/`,
  `mach-o/` and similar).
- **R2**: an OS macro in a preprocessor condition (`_WIN32`, `__APPLE__`, `__linux__`, `TARGET_OS_*`,
  `SDL_PLATFORM_*`, `__MINGW32__`, `__CYGWIN__` and others), including `__has_include(<OS header>)`. Compiler and
  architecture macros (`_MSC_VER`, `__clang__`, `_M_X64`, `__aarch64__`) are not OS branches.
- **R3**: a listed OS or CRT-split API (`localtime_r`, `localtime_s`, `std::signal`, `pthread_*`, `kqueue`,
  `GetLastError`, `SetThreadPriority` and others).
- **R4**: in runtime code (engine outside engine/os and the host shader tool; later apps and tools), fstream,
  `fopen` and every `std::filesystem` operation that touches the file system. The `path` type and pure path
  operations are allowed.
- **R5**: in engine/os, an include of anything but the standard library, OS headers, `engine/base` and
  `engine/os`.

engine/os/src is scanned for R5 only; engine/os/include is scanned for everything. Comments, string literals and
character literals are ignored; `#include` lines are matched as written. The scan covers engine and tests/common
today; os-guard-wide extends it to apps, tools, build/acceptance and the product test directories.

Existing sites are frozen in ratchet allowlists named `os_boundary_allowlist.txt`. A line is
`path rule count  # reason`. The count must equal the file's findings for that rule: more fails (a new site), fewer
fails and asks for the entry to shrink, a missing file fails. The Engine allowlist (`tests/common/os`) covers
engine/ and tests/common/; any other allowlist covers only files under its own directory, so deleting a product
deletes its allowlist and no common file changes. A configure-time check keeps `gyo_os` linked to exactly
`GYO::Base`. The configure-time check that no non-os target gets an OS-identifying compile definition comes with
os-guard-wide; no such definition exists today.

Known blind spots, left to review:

- Headers included indirectly: asio's interface definitions bring `windows.h` into product translation units.
- OS semantics without a macro: text-mode line endings, socket option meanings, which socket errors a platform
  reports.
- Third-party APIs that open files by path (ImGui's `AddFontFromFileTTF`, SDL's `SDL_SaveBMP` and
  `SDL_IOFromFile`): the third party absorbs the OS difference, so R4 does not cover them; callers pass UTF-8 paths.
- Code that brings a namespace into scope (`using namespace std::filesystem`) and calls the operations unqualified.

## Out of scope

- Windows, input, clipboard, GPU and the SDL clock domain stay with engine/platform/sdl and its own Design
  Invariants; engine/os does not link SDL or call `SDL_Init`.
- Sockets themselves stay with asio in the products that use them.
- The build layer: CMake's `WIN32`/`APPLE`/`UNIX`, MSVC flags, CI workflows, Python acceptance scripts and the
  platform columns of `engine/config`. Exception: Invariant 1's ban on OS-identifying macros also applies to
  compile definitions a non-os target gets from CMake.
- The Go relay service.

## Design Invariants

1. **OS knowledge lives only in engine/os.** C/C++ sources outside engine/os/src (engine, apps, tools,
   build/acceptance and tests, except tests/common/os) include no OS header (R1), branch on no OS macro in a
   preprocessor condition (R2) and call no listed OS or CRT-split API (R3). A non-os target gets no
   OS-identifying compile definition from CMake. Compiler and architecture macros are not OS branches. Exceptions
   live only in an owner's ratchet allowlist.
2. **Native file access goes through engine/os only** (R4). Runtime code in engine (outside engine/os and the host
   shader tool, engine/io included), apps and tools uses no fstream, `fopen` or `_wfopen` and no
   `std::filesystem` operation that touches the file system; only the `path` type and pure path operations. Files
   go through engine/io's abstraction, whose native backend calls engine/os; process facts come from engine/os
   directly. Tests and build/acceptance are not bound by R4: they only write temporary paths the developer
   controls.
3. **engine/os is the lowest layer for OS differences** (R5 and the link check). `GYO::Os` links only `GYO::Base`
   and OS system libraries (`Threads::Threads` once the condition-variable wait backend moves in). engine/os
   includes only engine/base, the standard library and OS headers. Its API has no type of a module above it: no
   `Uri`, `IFileSystem` or `IStream`.
4. **Public headers do not leak the OS.** engine/os/include includes only the standard library and engine/base. A
   class that holds a native handle keeps it opaque (pimpl or opaque storage); no `HANDLE` or file descriptor type
   appears in a header. Code that includes these headers needs no `NOMINMAX`.
5. **Neutral types and UTF-8.** Paths are UTF-8 `std::string_view` (UTF-8 `std::string` out), data are byte
   spans, time is `std::chrono`. Invalid UTF-8 is `InvalidEncoding`. On Windows every path is converted to UTF-16
   for the wide APIs; the ANSI code page APIs are never used. Long-path forms are made only inside engine/os just
   before the OS call; they are never returned or written into an error.
6. **Failures are Runtime Errors.** Functions return `Base::Result<_, OsError>`; the detail keeps the native error
   in the fixed form of the Contracts section. No exception crosses the public boundary or an OS callback (signal
   handler, console control handler). error-handling.md's "Exception at the platform boundary" extends to
   engine/os when os-console-shutdown lands (with consent).
7. **No product knowledge, no policy.** No product or tool name, file name, schema or log field appears in
   engine/os. Which socket errors are transient, what follows a shutdown request and how a time stamp is formatted
   are decided by the caller.
8. **State authority** (AGENTS.md §3). engine/os owns only its own OS resources and the shutdown-request flag.
   Queries (executable path, CPU time, file status) read the OS every time; nothing is cached to decide on. The
   Waiter's latch belongs to Time: engine/os reads it through a reference, under the condition variable's mutex in
   the fallback backend, and keeps no copy.
9. **Moving a site does not change observable behaviour.** File bytes (CRLF on Windows, append semantics),
   `WaitBackend` strings, name truncation, priority mapping, timeout conversion (100 ns rounding and the
   millisecond fallback on Windows, `NOTE_NSECONDS` on kqueue) and exception specifications stay as they are. The
   only allowed difference is a change the batch declares beforehand (marked ★ in the plan).
10. **Every capability has an explicit result on every OS.** A request that needs no OS call is met as asked and
    counts as applied (for example the Normal priority, which `RoleThread` reports as applied today). A request
    that needs an OS call the layer does not make reports not-applied or `Unsupported`; it never reports success
    silently. When OSes cannot do the same thing, the difference is written in the type or the documentation.

### Replacement Criteria

1. A consumer needs engine/os to hand out a native handle (socket, `HANDLE`, file descriptor), or must know which
   OS it is on to use a neutral function correctly: stop adding parameters; propose a native-handle layer or a
   redesign of that component.
2. Changing any component of engine/os makes it necessary to re-run the Wait timing L2: propose splitting Wait into
   a component of its own with its own guard and criteria.
3. Implementing a capability needs engine/os to depend on engine/io, Time, Threads, Trace, SDL, asio or
   `GYO::Engine`: the layering is wrong; stop and let the user decide instead of adding the edge.
4. A consumer needs asynchronous or event-driven file I/O, directory watching or large directory listings, and the
   narrow primitives of Files would have to grow into a second `IFileSystem`: stop and redraw the split between
   engine/os primitives and engine/io backends.
5. Keeping a caller's bytes (Invariant 9, for example CRLF) needs engine/io or engine/os to maintain OS-specific
   stream wrappers: propose deciding line-ending semantics as a Data Contract instead of converting silently in a
   backend.
6. An allowlist grows instead of shrinking for two batches in a row, more than two permanent entries are needed
   outside tests, or false positives need repeated rule changes: rewrite the rules or narrow Invariant 1 instead of
   adding exceptions.
7. One capability on one OS needs branches across two or more engine/os files, the per-OS files are mostly
   duplicates, or a platform whose process and file model differs from both POSIX and Win32 is added: propose a
   backend interface with a selection table.
