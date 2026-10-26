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
| Files | File primitives with UTF-8 paths (`FileReader`, `FileWriter`, `CreateDirectories`, `RenameReplacing`, `RemoveFile`, `Stat`, `Exists`, `MakeAbsolute`, `ResolvedPath`, `SamePathComponent`, `PathToUtf8`/`PathFromUtf8`) and the font directories (`FontDirectories`) | read: os-files-read; write: os-files-write; rename, resolve, compare, fonts: os-tool-files (landed) |
| Process | Executable path, process and thread CPU time, local calendar time (`ExecutablePath`, `ProcessCpuTime`, `CurrentThreadCpuTime`, `ToLocalTime`) | os-process-info (landed) |
| Console | UTF-8 command line, UTF-8 console output, shutdown requests (`CommandLineUtf8`, `Utf8ConsoleOutput`, `InstallShutdownRequests`, `RequestedShutdown`) | os-console-shutdown (landed) |
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
  - A moved-from `FileReader` may only be destroyed or assigned to; anything else is a Programmer Error
    (`GYO_ASSERT`).
  - `MakeAbsolute` resolves against the current directory the platform's own way: `GetFullPathNameW` on Windows
    also removes dot segments and trailing dots and spaces; POSIX joins the current directory as it is. It answers
    in `/` form and never returns a long-path prefix. A relative path under a current directory that has no UTF-8
    form (POSIX bytes that are not UTF-8) is `InvalidEncoding` (D43, D54): the answer would not be a UTF-8 path.
  - Long paths on Windows (os-subsystem PLAN section 8, P1): `FileReader::Open`, `Stat` and `Exists` read a file whose
    full path (`GetFullPathNameW`) is 248 characters (`MAX_PATH - 12`) or longer by handing the API `\\?\` plus
    that full path. The full path already carries Win32's normalization (dot segments, trailing dots and spaces),
    so turning normalization off with the prefix changes nothing for it; this is the change declared beforehand
    under Design Invariant 9. A UNC full path (`\\server\share\...`) that long goes as `\\?\UNC\` plus the full
    path after its `\\` (D54, io-native-paths). A shorter path, a full path that is neither on a drive letter nor
    UNC (a device such as `\\.\NUL`) and a path `GetFullPathNameW` refuses go to the API as before. A path the
    caller already gives in a `\\?\` or `\\.\` form is passed on unchanged (engine/io refuses such paths, io.md
    Invariant 4; engine/os keeps Win32's meaning for them). Errors keep the caller's path length (`path_bytes`), not
    the prefixed form.
  - `IsAbsolutePath` answers per platform as a pure string rule: `/x` on POSIX; `C:/x`, `C:\x` or a `\\` or `//`
    prefix on Windows.
  - `PathToUtf8` and `PathFromUtf8` return `InvalidEncoding` for a path with no UTF-8 form (an unpaired surrogate
    on Windows, bytes that are not UTF-8 on POSIX) and for text that is not UTF-8 (D47); they never throw.
  - Write times are 0 when the file system keeps none and saturate outside the range of `std::chrono::nanoseconds`.
  - `FileWriter` opens a file it creates when missing, either emptied (`WriteMode::Truncate`) or appended to
    (`WriteMode::Append`, every write at the end, also after another process's writes). Truncate opens as a C++
    file stream does (POSIX `open` with 0666 less the umask; Windows `GENERIC_WRITE` with `CREATE_ALWAYS`).
    Append is `O_APPEND` on POSIX, as there; on Windows it is an append-only handle (`FILE_APPEND_DATA`,
    `OPEN_ALWAYS`), atomic against other appenders, where the C runtime seeks to the end before each write; the
    batch that moves an appending caller (os-trace-writers) declares that difference. Other processes may read and write
    the file meanwhile, not delete it (the C runtime's `_SH_DENYNO` on Windows). Nothing is buffered: each `Write`
    hands all its bytes to the OS, not to the disk. A directory is `InvalidPath`; `Close` reports a failure the
    destructor cannot. Long paths on Windows go through the same `\\?\` form as reads.
  - Line endings (D52): `LineEndings::AsWritten` writes the bytes as given; `LineEndings::Native` turns each `\n`
    into `\r\n` on Windows, whatever precedes it, and writes the bytes unchanged elsewhere, which is what a
    text-mode C++ stream writes (Invariant 9). Replacement Criterion 5 was met here and put to the user, who kept
    the bytes; making line endings a Data Contract is a later decision.
  - `CreateDirectories` makes the directory and its missing parents, as `std::filesystem::create_directories`
    does; an existing directory is fine, and a path or parent that exists as anything else is `InvalidPath`.
  - `RenameReplacing` renames a file over another in one step (os-tool-files, D56; named so because `ReplaceFile`
    is a Win32 function): POSIX `rename`; Windows `MoveFileExW` with `MOVEFILE_REPLACE_EXISTING` and
    `MOVEFILE_WRITE_THROUGH`, the flags the editor's atomic save used (Invariant 9). Both paths are on one volume;
    a missing source is `NotFound`; a directory at the target is an error; on failure the target is unchanged; an
    error's `path_bytes` is the target's. `RemoveFile` removes a file, never a directory (`InvalidPath` on every
    platform); a missing file is `NotFound`. Both take long paths as reads do.
  - `ResolvedPath` is what `std::filesystem::weakly_canonical` gives, in `/` form: `MakeAbsolute`, then links
    resolved in the longest prefix that exists, the rest appended as written and dot segments removed lexically. A
    root with nothing existing after it stays as written, as `weakly_canonical` keeps `root_path()`. POSIX finds
    the prefix with `stat`, as the standard library's existence check does, and resolves it with `realpath`.
    Windows opens it and asks `GetFinalPathNameByHandleW`, which also gives the prefix its case on disk and the
    directory a `subst` drive names; its `\\?\` and `\\?\UNC\` forms are removed. The errors that count as a
    missing component are those of the MSVC library's `__std_is_file_not_found`. Two declared differences from the
    MSVC library (D57): a volume with no drive letter is an error here, where the library answers with a
    `\\?\GLOBALROOT\...` path (Invariant 5 keeps namespace forms out of results), and the rest after the prefix
    has `MakeAbsolute`'s normalization (Windows drops trailing dots and spaces). The guard test compares the result
    with `weakly_canonical` for the same inputs, including a root-only path, a `subst`-style drive and UNC.
  - `SamePathComponent` compares two names (D56 ★): on Windows as NTFS does, ignoring case by the OS's uppercase
    table (`CompareStringOrdinal`), where the editor used the C runtime's `towlower`, whose result for letters
    outside ASCII depends on the locale; elsewhere byte for byte, macOS included, although `DefaultCaseSensitivePaths`
    is false there (a declared difference, kept as the editor compared before).
  - Moving the editor's file service changed, as declared after its review (D57): a relative empty path made
    absolute is empty on POSIX too (Windows already); reading a directory or a FIFO on POSIX fails (Windows already;
    a FIFO no longer blocks); a Windows name the OS refuses fails when probed rather than when written; error
    messages keep their words with the engine cause in the detail.
  - `FontDirectories` lists the font directories that exist, the system's first, then the user's: Windows `Fonts`
    in the system Windows directory and `%LOCALAPPDATA%\Microsoft\Windows\Fonts`; macOS `/System/Library/Fonts`,
    `/Library/Fonts`, `~/Library/Fonts`; other POSIX `/usr/share/fonts`, `/usr/local/share/fonts` and the user's
    data directory's `fonts`. Which file to use is the caller's (Invariant 7); the editor searches one list of names
    in each directory, system directories first (D56 ★: it searched only the system directory before, so only a
    font that is missing there can be found elsewhere now; D57: on Linux a Windows or macOS font in the same
    directory now comes before Noto, and Windows reads the system Windows directory instead of `WINDIR`).
- **Process.**
  - `ExecutablePath` is the running image as an absolute UTF-8 path, read from the OS on every call:
    `GetModuleFileNameW` on Windows (a `\\?\` or `\\?\UNC\` prefix is removed, Invariant 5), `/proc/self/exe` on
    Linux, `_NSGetExecutablePath` with links resolved (`realpath`) on macOS; `Unsupported` elsewhere. It answers in
    `/` form like every engine/os path. It is not the resource directory of an application: a macOS app bundle
    keeps those apart, and engine/platform/sdl's `BaseDirectory` stays the authority for SDL's resource base
    (os-subsystem open decision 8).
  - `ProcessCpuTime` and `CurrentThreadCpuTime` are user plus kernel time in `std::chrono::nanoseconds`:
    `GetProcessTimes`/`GetThreadTimes` on Windows (scheduler ticks, about 15.6 ms), `getrusage(RUSAGE_SELF)` and
    `clock_gettime(CLOCK_THREAD_CPUTIME_ID)` on POSIX. A refused query is `Unsupported` with the native error.
  - `ToLocalTime` converts the whole seconds of a `system_clock` time point (`to_time_t`) with `localtime_r` or
    `localtime_s`, in the time zone the C runtime last read; it does not call `tzset`. It returns the calendar
    fields (month 1–12) and the offset from UTC in seconds, computed from those fields, so a caller printing the
    offset as a sign (`+` for zero), hours and minutes gets the bytes strftime's `%z` gave on every platform. A time
    point the platform cannot convert is `Unsupported` with the native error (Windows: after the year 3000).
  - An OS failure of `ExecutablePath` is `IoFailed` (the native error stays in the detail; no caller branches on
    the code), a path longer than Windows allows is `TooLarge`, and a path with no UTF-8 form is `InvalidEncoding`.
- **Console.**
  - `CommandLineUtf8` gives the process's arguments in UTF-8, the program name first. Windows reads
    `GetCommandLineW` and splits it with `CommandLineToArgvW` (shell32), whatever the ANSI code page is; its rules
    for quotes inside quotes can differ from the C runtime's narrow `argv` in corner cases. POSIX hands `argv` on
    as given, and an argument that is not UTF-8 is `InvalidEncoding` (D50).
  - `Utf8ConsoleOutput` sets the Windows console's output code page to UTF-8 while it lives and restores the
    previous one when destroyed; it is not applied without a console. POSIX needs no change and counts as applied.
    Output redirected to a file or a pipe is bytes either way.
  - `InstallShutdownRequests` installs the process's handlers once (all or nothing); `RequestedShutdown` reads the
    first request's reason from any thread. The flag is the one process state engine/os owns (Invariant 8); what a
    program does on a request is its policy.
    - POSIX: SIGINT and SIGTERM with `sigaction`, carrying the `sa_flags` `std::signal` sets on the platform's C
      library (`SA_RESTART`, the handler kept): later signals only repeat the request.
    - Windows: Ctrl+C, Ctrl+Break and closing the console with `SetConsoleCtrlHandler`. After a request, another
      Ctrl+C or Ctrl+Break gets the default handling and ends the process at once, as a second Ctrl+C did when
      the C runtime's SIGINT handling reset itself. On a close the handler waits up to 4 s, since the OS ends the
      process when it returns (about 5 s after the close); the program's own exit ends the wait sooner. Log off
      and system shutdown keep the default handling. `TerminateProcess` never reaches a handler.
- **Paths and text.** UTF-8 in `std::string_view`, UTF-8 out in `std::string`. Invalid UTF-8 is `InvalidEncoding`.
  On Windows the layer converts to UTF-16 and calls the wide APIs. Long-path forms (`\\?\`, `\\?\UNC\`) are made
  inside the layer just before the OS call; they are never returned or written into an error.
- **Data and time.** Bytes are `std::span<std::byte>`; durations and time points are `std::chrono`.
- **Names.** engine/os names never reuse a Win32 function name or a `windows.h` A/W macro name (`ReadFile`,
  `WriteFile`, `CreateDirectory`, `DeleteFile`, `MoveFile`, `CopyFile`, `GetFileAttributes` and the like): product
  translation units see `windows.h` indirectly (through asio), and a name that collides there is ambiguous or
  renamed by the preprocessor. The read primitive was renamed from `ReadFile` to `FileReader` when windows-x64
  showed the ambiguity (os-files-read).
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
- **R4**: in runtime code (engine outside engine/os and the host shader tool, apps and tools), fstream, `fopen`
  and every `std::filesystem` operation that touches the file system. The `path` type and pure path
  operations are allowed.
- **R5**: in engine/os, an include of anything but the standard library, OS headers, `engine/base` and
  `engine/os`.

engine/os/src is scanned for R5 only; engine/os/include is scanned for everything. Comments, string literals and
character literals are ignored; `#include` lines are matched as written. The scan covers engine, apps, tools,
build/acceptance and tests (except tests/common/os, third_party and build/target), and every existing root must
yield at least one source. The roots name no product, so adding or removing a product changes no guard file.

Existing sites are frozen in ratchet allowlists named `os_boundary_allowlist.txt`. A line is
`path rule count  # reason`. The count must equal the file's findings for that rule: more fails (a new site), fewer
fails and asks for the entry to shrink, a missing file fails. The Engine allowlist (`tests/common/os`) covers
engine/ and tests/common/; any other allowlist covers only files under its own directory, so deleting a product
deletes its allowlist and no common file changes.

Two configure-time checks run once the whole project is configured (`tests/common/os/CMakeLists.txt`): `gyo_os`
links exactly `GYO::Base` (and, on Windows, `shell32` for `CommandLineToArgvW`, a system library Invariant 3
allows; D49, D50), and no target or directory under the scanned roots outside engine/os and
tests/common/os has an OS-identifying compile definition (Invariant 1). A definition is OS-identifying when its
name contains an OS word (`WIN32`, `WINDOWS`, `APPLE`, `DARWIN`, `LINUX`, `POSIX`, `UNIX`, `ANDROID` and others,
so `GYO_WINDOWS` and `WIN32_LEAN_AND_MEAN` are refused) or when it is conditioned on `$<PLATFORM_ID:...>`. The
value is not read, since paths may contain any word. `COMPILE_DEFINITIONS`, `INTERFACE_COMPILE_DEFINITIONS` and
the `-D`/`/D` entries of `COMPILE_OPTIONS` are checked, on targets and directories alike.

`OsHeaderHygieneTests` (CTest `os.header_hygiene`) checks Invariant 4 on Windows: after the engine/os public
headers, no `windows.h` or winsock guard macro and no `min` or `max` macro is defined. It is recorded before
doctest is included and fails if a public header is not included by the test. The POSIX side is the text scan of
engine/os/include, since libc++ and libstdc++ define `_PTHREAD_H` from `<string>`.

Known blind spots, left to review:

- Headers included indirectly: asio's interface definitions bring `windows.h` into product translation units.
- OS semantics without a macro: text-mode line endings, socket option meanings, which socket errors a platform
  reports.
- Third-party APIs that open files by path (ImGui's `AddFontFromFileTTF`, SDL's `SDL_SaveBMP` and
  `SDL_IOFromFile`): the third party absorbs the OS difference, so R4 does not cover them; callers pass UTF-8 paths.
- Code that brings a namespace into scope (`using namespace std::filesystem`) and calls the operations unqualified.
- A compile definition whose name identifies no OS but which CMake sets on one OS only (`NOMINMAX` under
  `if(WIN32)`): it configures a header and is not identification, so it is allowed. `-D` flags in
  `CMAKE_<LANG>_FLAGS` or inside a generator expression in `COMPILE_OPTIONS`, and third_party's interface
  definitions (asio's `_WIN32_WINNT` and `WIN32_LEAN_AND_MEAN`), are not checked.
- Paths taken from narrow `argv` strings in programs that do not use `Os::Console::CommandLineUtf8` (today the
  acceptance programs): on Windows they are in the ANSI code page, not UTF-8, without any OS call in the caller.
- R3's bare `signal(` also matches a variable or declaration named `signal`. Such a false positive is listed in the
  owner's allowlist with its reason; the pattern is not loosened.

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
   handler, console control handler). The OS callbacks only write a lock-free atomic (and, on a console close,
   wait) and are `noexcept`, so engine/os needs none of error-handling.md's "Exception at the platform boundary"
   rules (no `catch (...)`, no destructor checking global state); that section's scope stays engine/platform/sdl
   (D49).
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
