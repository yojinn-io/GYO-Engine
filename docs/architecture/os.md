# GYO OS layer

Owner: Engine (`engine/core/os` → `GYO::OS`).
Plan and record: [os-subsystem](plans/os-subsystem/README.md), batch os-skeleton onwards (D40–D46).
Proposed direction (not yet in this contract): [build-baseline](plans/build-baseline/README.md) — platform and toolchain
tiers, and Windows processes fixing their code page to UTF-8 through the manifest's `activeCodePage` (decided
2026-10-27, not implemented).

## What it is

The only Engine code that knows which operating system it runs on. Every other Engine module, every product and
every tool reaches the operating system through neutral APIs, so none of them branches on the OS.

Inside, the sources are laid out by platform (v8-arch-gyo2-os-src-platform): `src/common` holds what does not depend
on the OS; everything that does lives under `src/sys`. `src/sys/<platform>` (`windows`, `macos`, `linux`) holds each
platform's own sources, and the build selects one (`linux` also for any platform that is neither Windows nor
Apple). `src/sys/Files.cpp`, `Process.cpp`, `Machine.cpp` and `Console.cpp` are the implementation macOS and Linux share, and
`src/sys/UserData.cpp` is shared by all three (v8-arch-gyo2-os-user-data); where the platforms differ, the shared
implementation calls the abstract `Detail::Platform` (`src/sys/Platform.hpp`), which each `src/sys/<platform>`
implements. Windows implements the file, process and console primitives itself, so it leaves their steps empty and
implements only `UserDataBase`. No source under `src/sys` branches on the OS with the preprocessor.

```text
products / tools ──┬──→ engine/modules/* ──→ engine/core/* (logging: engine/core/os, threads, time)
                   ├──→ engine/core/io ──→ engine/core/os ──→ engine/core/base
                   ├──→ engine/core/threads ──→ engine/core/time ──→ engine/core/os (the wait primitive)
                   │    engine/core/threads ──→ engine/core/os (thread name and priority)
                   └──→ engine/core/os (process facts, console)
```

`GYO::OS` is a leaf directly above `GYO::Base`. Modules keep their own abstractions and call it: engine/core/io keeps
`IFileSystem`, `Uri` and `IStream`, and its native file-system backend calls the file primitives here;
engine/core/time's `Waiter` and engine/core/threads' `RoleThread` keep their public APIs and use the wait and thread
primitives here.

## Components

| Component | What it gives | Status |
|---|---|---|
| `OSError` | The layer's error; the detail keeps the native error code | os-skeleton (landed) |
| Files | File primitives with UTF-8 paths (`FileReader`, `FileWriter`, `CreateDirectories`, `RenameReplacing`, `RemoveFile`, `Stat`, `Exists`, `MakeAbsolute`, `ResolvedPath`, `SamePathComponent`, `PathToUtf8`/`PathFromUtf8`), message text for a path (`PathForMessage`), the font directories (`FontDirectories`) and this user's data directory for an application (`UserDataDirectory`) | read: os-files-read; write: os-files-write; rename, resolve, compare, fonts: os-tool-files; message text: os-path-utf8-sweep; user data directory: v8-arch-gyo2-os-user-data (landed; brought in by the user's owner decision of 2026-11-22 for players' saves) |
| Process | Executable path, process and thread CPU time, local calendar time, the process ANSI code page (`ExecutablePath`, `ProcessCpuTime`, `CurrentThreadCpuTime`, `ToLocalTime`, `AnsiCodePage`) | os-process-info; code page: os-utf8-codepage (landed) |
| Machine | The machine's host name as the OS reports it (`HostName`) | v8-arch-gyo2-display-name (landed; the observed pressure of a Feature, D78: the player's display name, whose default is the host name, the user's decision of 2026-11-28; declaration `docs/architecture/target/v2/display-name-declaration.zh-Hant.md`) |
| Process code page | The manifest that makes every Windows executable of the repository run with UTF-8 as its ANSI code page, and the CMake module that embeds it in one executable (`engine/core/os/cmake`); the build layer chooses the executables (`build/cmake/GyoCodePage.cmake`) | os-utf8-codepage (landed) |
| Console | UTF-8 command line, UTF-8 console output, shutdown requests (`CommandLineUtf8`, `Utf8ConsoleOutput`, `InstallShutdownRequests`, `RequestedShutdown`) | os-console-shutdown (landed) |
| Thread | Thread name and priority hint for the calling thread (`SetCurrentName`, `SetCurrentPriority`) | os-threads-backend (landed) |
| Wait | The wake primitive behind `Time::Waiter` (`WakeSignal`: `Create`, `Signal`, `Block`, `Mechanism`) | v8-arch-gyo2-os-wait (landed; the user's owner decision of 2026-11-21 lifted the D78 freeze, and the batch was accepted by CI L1, the guards and mutants instead of the Wait timing L2) |
| Datagram | A non-blocking IPv4 UDP socket (`DatagramSocket`: `Open`, `LocalAddress`, `Send`, `Receive`) and the address both sockets take (`SocketAddress`, its own header: `Loopback`, `Any`, `Parse`, `ToString`), failures in neutral codes | v8-arch-gyo2-net-module step 0; the address renamed after the socket, not the transport, when the stream came to share it: v8-arch-gyo2-rename-address (the user approved the net Module declaration on 2026-11-22, its Q5: the foreseen pressure of the net Module, which comes before the products, replaces D78's "no observed pressure"; which errors are transient stays with the caller, Invariant 7) |
| Stream | A non-blocking IPv4 TCP client connection on the same address type, `SocketAddress` (`StreamSocket`: `Connect`, `State`, `Send`, `Receive`), failures in neutral codes; no listener (no product needs one) | v8-product-pvp-gateway segment 3 (the observed pressure of a Feature, D78: the user put the PvP Gateway in the PvP port on 2026-11-25, `v8-arch-gyo2-gateway-in-pvp-port`, and its Client and Match talk HTTP to the Gateway's services; declaration D8 in `games/object_fps_pvp/docs/gateway-declaration.zh-Hant.md`) |

Only the components whose batch has landed exist. A frozen or not-started component comes back only with observed
pressure, a Feature that needs it, or the user's owner decision (Replacement Criterion 9).

## Contracts

- **Errors.** `Base::Result<T, OSError>`; `OSErrorCode` starts at 1. The detail is `errno=<n>` or `win32=<n>`,
  followed by `; path_bytes=<n>` when the failure concerns a path (`OS::NativeErrorDetail`). An argument the layer
  refuses itself, before or instead of an OS call (an empty path, a NUL, invalid UTF-8, an offset or length out of
  range, a file that is not a regular file), has no native error: its detail is `path_bytes=<n>` alone; no native
  code is made up. An outer module keeps the detail with `Base::CauseDetail` (error-handling.md rule 5).
- **Files.**
  - The file calls keep no shared mutable state, so any number of threads may call them at once; a `FileReader` or
    `FileWriter` is used by one thread at a time. engine/core/io's Design Invariant 6 rests on this
    (v8-arch-gyo2-threads-io-review).
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
    caller already gives in a `\\?\` or `\\.\` form is passed on unchanged (engine/core/io refuses such paths, io.md
    Invariant 4; engine/core/os keeps Win32's meaning for them). Errors keep the caller's path length (`path_bytes`), not
    the prefixed form.
  - `IsAbsolutePath` answers per platform as a pure string rule: `/x` on POSIX; `C:/x`, `C:\x` or a `\\` or `//`
    prefix on Windows.
  - `PathToUtf8` and `PathFromUtf8` return `InvalidEncoding` for a path with no UTF-8 form (an unpaired surrogate
    on Windows, bytes that are not UTF-8 on POSIX) and for text that is not UTF-8 (D47); they never throw.
  - `PathForMessage` gives a path as UTF-8 text for messages and logs (os-path-utf8-sweep, D63). The separators
    stay as the path holds them (`\` on Windows when it has them), so an ASCII path prints the bytes
    `path::string()` printed (D48). It never fails, never throws and never uses the ANSI code page: what has no
    UTF-8 form becomes U+FFFD (an unpaired surrogate on Windows; on POSIX each byte that starts no well-formed
    sequence). The text is lossy, so it is not for naming a file again: that is `PathToUtf8`.
  - Write times are 0 when the file system keeps none and saturate outside the range of `std::chrono::nanoseconds`.
  - `FileWriter` opens a file it creates when missing, either emptied (`WriteMode::Truncate`) or appended to
    (`WriteMode::Append`, every write at the end, also after another process's writes). Truncate opens as a C++
    file stream does (POSIX `open` with 0666 less the umask; Windows `GENERIC_WRITE` with `CREATE_ALWAYS`).
    Append is `O_APPEND` on POSIX, as there; on Windows it is an append-only handle (`FILE_APPEND_DATA`,
    `OPEN_ALWAYS`), atomic against other appenders, where the C runtime seeks to the end before each write; the
    batch that moved an appending caller (os-trace-writers, D58) declared that difference. Other processes may read and write
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
  - `UserDataDirectory(organization, application)` (v8-arch-gyo2-os-user-data; declaration
    `target/v2/os-user-data-declaration.zh-Hant.md`, C1–C10) answers `<base>/<organization>/<application>`, or
    `<base>/<application>` when the organization is empty, and makes the directory and its missing parents. Only
    the base differs between platforms, and each `Detail::Platform` names it (`UserDataBase`); the names are
    checked, the path is joined and the directories are made once in `src/sys/UserData.cpp`, which every platform
    compiles.
    - Base: macOS `$HOME/Library/Application Support` (as Godot; not Foundation, which would be a new link);
      Windows the Roaming AppData known folder from `SHGetFolderPathW(CSIDL_APPDATA | CSIDL_FLAG_CREATE)` in
      shell32 (it wraps `SHGetKnownFolderPath`, which would need ole32 for `CoTaskMemFree`); elsewhere
      `$XDG_DATA_HOME` when it is an absolute path (a relative or empty one is ignored, as the XDG specification
      says), else `$HOME/.local/share`. The trailing `/` of `HOME` and of the base are dropped, so a base of `/`
      gives `/<organization>/<application>`.
    - Each name is one path component (the user's decision Q1 (b)): an empty application, `.`, `..` or a name
      with `/` or `\` is `InvalidPath` before any directory is made (detail `path_bytes=<n>` of the name). A NUL
      or text that is not UTF-8 is refused by `CreateDirectories`' own check of the whole path. Anything else
      about a name is left to the OS and not defined here: on Windows a name ending in `.` or a space, a reserved
      name (`CON`) or `:` goes to `CreateDirectories` as it is.
    - Directories it makes are 0700 on POSIX (the user's decision Q2 (a), as SDL and the XDG specification);
      levels that exist keep their access, and the public `CreateDirectories` keeps 0777 less the umask. On
      Windows a new directory inherits its parent's ACL.
    - No base is `IOFailed`: POSIX without an absolute `HOME` (no native error, no detail; it never falls back to
      the current directory as Godot does), Windows when `SHGetFolderPathW` fails (`win32=<n>` when the HRESULT
      is a Win32 one). A base with no UTF-8 form is `InvalidEncoding`; a directory that cannot be made reports
      `CreateDirectories`' error. As with `ExecutablePath`, no caller branches on the code.
    - It answers an absolute UTF-8 path in `/` form without a trailing separator, reading the OS and the
      environment on every call (Invariant 8). What goes in the directory is the caller's (Invariant 7).
- **Process code page** (os-utf8-codepage, D64–D66).
  - `engine/core/os/cmake/utf8-codepage.manifest` declares `<activeCodePage>UTF-8</activeCodePage>` and nothing else.
    `engine/core/os/cmake/GyoProcessCodePage.cmake` embeds it in one executable. With MSVC, clang-cl and Clang on the
    MSVC ABI it is a `.manifest` source, which the linker merges into the manifest it embeds as resource 1 (the
    linker's own is `requestedExecutionLevel asInvoker`). With MinGW it is the `RT_MANIFEST` resource 1 of a
    generated `.rc`, with an explicit language and a dependency on the manifest file; whether it wins over
    MinGW-w64's default manifest is to be verified on a MinGW machine (open decision 23). The way is decided when
    the module is included, in the scope that enabled the compilers.
  - The build layer (`build/cmake/GyoCodePage.cmake`, included by `GyoBuild.cmake`) chooses the executables. It
    records each directory it adds (engine, every selected Plugin, every game and tool, their tests and acceptance
    programs, tests/common, examples) by its binary directory, since a standalone tool wrapper reuses the tool's source
    directory, and once every target exists applies the manifest to every executable target there and below whose
    source directory is under a root of `build/cmake/GyoSourceRoots.txt` (engine, plugins, games, tools, examples,
    build/acceptance and tests: the repository's own code, one list that the checks and guards below read too,
    `v8-arch-gyo2-build-cleanup`). It marks each with `GYO_PROCESS_CODE_PAGE`
    on every platform. No product is named: a new, cloned or removed product needs no common change. A target
    that must run without it sets `GYO_PROCESS_CODE_PAGE_EXEMPT` to the reason (there is none today). Third-party
    programs that FetchContent adds (protoc, and SDL's own) are outside the roots and keep their own manifests; Go services are
    not CMake executables. sdl_gpu's separately configured shader tool (plugins/sdl_gpu/shader_tool) includes the
    engine/core/os module and applies it itself.
  - Windows 10 1903 and later honour the declaration; earlier versions ignore it and the process takes the system
    setting, which engine/core/os does not depend on (Invariant 5). Whether Proton and Wine honour it is not verified
    (open decision 20); `longPathAware` is not declared (open decision 21), since it takes effect only where the
    machine's registry enables it.
  - ★ (declared before the batch, D66): third-party and CRT narrow text becomes UTF-8 in our processes, for
    example `std::system_category().message()` (`FormatMessageA`) and MSVC's `path::string()`, which no longer
    throws for characters outside the old code page (it still throws for an unpaired surrogate); narrow `argv`
    (the host shader tool, the acceptance probes) arrives as UTF-8.
  - ★ (found in the batch's review, accepted in D67): the OEM code page most likely becomes 65001 too (not verified on Windows).
    The console's code page does not change, so a program that does not enable `Utf8ConsoleOutput` (the host
    shader tool, tests, probes) and prints non-ASCII narrow text to a console set to 950 or 932 now shows it as
    UTF-8 bytes where it showed it in the old code page.
- **Process.**
  - `AnsiCodePage` is `GetACP()` on Windows: 65001 (UTF-8) in every executable of the repository (Invariant 11),
    the system's setting in an executable built elsewhere or on Windows before 10 1903. It is for diagnostics, such
    as a start line; engine/core/os never converts through it. `Unsupported` on POSIX, which has no ANSI code page
    (narrow text is bytes).
  - `ExecutablePath` is the running image as an absolute UTF-8 path, read from the OS on every call:
    `GetModuleFileNameW` on Windows (a `\\?\` or `\\?\UNC\` prefix is removed, Invariant 5), `/proc/self/exe` on
    Linux, `_NSGetExecutablePath` with links resolved (`realpath`) on macOS; `Unsupported` elsewhere. It answers in
    `/` form like every engine/core/os path. It is not the resource directory of an application: a macOS app bundle
    keeps those apart, and the SDL adapter's `BaseDirectory` stays the authority for SDL's resource base
    (os-subsystem open decision 8; SDL is not in gyo2 yet and returns with the SDL batch).
  - `ProcessCpuTime` and `CurrentThreadCpuTime` are user plus kernel time in `std::chrono::nanoseconds`:
    `GetProcessTimes`/`GetThreadTimes` on Windows (scheduler ticks, about 15.6 ms), `getrusage(RUSAGE_SELF)` and
    `clock_gettime(CLOCK_THREAD_CPUTIME_ID)` on POSIX. A refused query is `Unsupported` with the native error.
  - `ToLocalTime` converts the whole seconds of a `system_clock` time point (`to_time_t`) with `localtime_r` or
    `localtime_s`, in the time zone the C runtime last read; it does not call `tzset`. It returns the calendar
    fields (month 1–12) and the offset from UTC in seconds, computed from those fields, so a caller printing the
    offset as a sign (`+` for zero), hours and minutes gets the bytes strftime's `%z` gave on every platform. A time
    point the platform cannot convert is `Unsupported` with the native error (Windows: after the year 3000).
  - An OS failure of `ExecutablePath` is `IOFailed` (the native error stays in the detail; no caller branches on
    the code), a path longer than Windows allows is `TooLarge`, and a path with no UTF-8 form is `InvalidEncoding`.
- **Machine.**
  - `HostName` is the machine's name on the network as the OS reports it, asked on every call: `gethostname` on
    macOS and Linux (macOS may end it with `.local`, or answer with a name the network handed it),
    `GetComputerNameExW(ComputerNamePhysicalDnsHostname)` on Windows (retried with the size `ERROR_MORE_DATA` gives,
    as Go's `os.Hostname` does). It is not trimmed, case-folded or shortened: what a caller makes of it (a player's
    name without `.local`) is the caller's. A refused query is `IOFailed` with the native error; a name with no UTF-8
    form is `InvalidEncoding`.
- **Console.**
  - `CommandLineUtf8` gives the process's arguments in UTF-8, the program name first. Windows reads
    `GetCommandLineW` and splits it with `CommandLineToArgvW` (shell32), whatever the ANSI code page is; its rules
    for quotes inside quotes can differ from the C runtime's narrow `argv` in corner cases. POSIX hands `argv` on
    as given, and an argument that is not UTF-8 is `InvalidEncoding` (D50).
  - `Utf8ConsoleOutput` sets the Windows console's output code page to UTF-8 while it lives and restores the
    previous one when destroyed; it is not applied without a console. POSIX needs no change and counts as applied.
    Output redirected to a file or a pipe is bytes either way.
  - `InstallShutdownRequests` installs the process's handlers once (all or nothing); `RequestedShutdown` reads the
    first request's reason from any thread. The flag is the one process state engine/core/os owns (Invariant 8); what a
    program does on a request is its policy.
    - POSIX: SIGINT and SIGTERM with `sigaction`, carrying the `sa_flags` `std::signal` sets on the platform's C
      library (`SA_RESTART`, the handler kept): later signals only repeat the request.
    - Windows: Ctrl+C, Ctrl+Break and closing the console with `SetConsoleCtrlHandler`. After a request, another
      Ctrl+C or Ctrl+Break gets the default handling and ends the process at once, as a second Ctrl+C did when
      the C runtime's SIGINT handling reset itself. On a close the handler waits up to 4 s, since the OS ends the
      process when it returns (about 5 s after the close); the program's own exit ends the wait sooner. Log off
      and system shutdown keep the default handling. `TerminateProcess` never reaches a handler.
- **Thread** (os-threads-backend, D69). The calls engine/core/threads' `RoleThread` makes on a new role's thread before its
  body runs, moved from it line by line; what a role is and its lifetime stay with engine/core/threads. One source per
  platform (`src/sys/<platform>/Thread.cpp`; it was one source with per-OS branches until
  v8-arch-gyo2-os-src-platform). Each answers whether the platform took the request and is never an error
  (Invariant 10).
  - `SetCurrentName` names the calling thread with a UTF-8 name: `pthread_setname_np` on macOS; on Linux with the
    first 15 bytes (the platform's limit); on Windows `SetThreadDescription` (Windows 10 1607 and later, looked up
    at run time) with the name converted to UTF-16. ★ (D69): Windows used to widen each byte, which garbled a
    name beyond ASCII; a name that is not UTF-8 is now not applied. Other platforms have no call, so `false`.
  - `SetCurrentPriority` applies a hint: macOS QoS classes (Background as utility, Normal as default, Interactive
    as user-interactive), Windows thread priorities (below normal, normal, above normal), Linux only Background as
    nice 10, which fails if the process already runs nicer. Normal on Linux and other platforms makes no call and
    counts as applied; Interactive on Linux needs privileges and is not tried.
- **Wait.** `WakeSignal` wakes one blocking thread when another signals it or a relative timeout passes; what a
  wake means (the latch, the single waiter, deadlines, the re-check of the time base) is engine/core/time's.
  - `Signal` (any thread, `noexcept`) leaves one pending signal, kept by the primitive itself; several pending
    signals end one block. `Block` (the single waiting thread) returns when signalled, when the relative timeout
    has passed (none: only a signal ends it) or spuriously. One pending signal ends at most one block; one that
    arrives together with a timeout may be taken by that block or by the next. The caller sets its own state
    before it signals and re-checks it after every block. Timeouts up to one hour are supported (the Waiter never
    asks for more).
  - macOS: a kqueue with an `EVFILT_USER` event registered with `EV_CLEAR`, and an `EVFILT_TIMER` one-shot timer
    armed per block with `NOTE_NSECONDS | NOTE_CRITICAL`. Windows: an auto-reset event and a waitable timer created
    with `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`, else a plain one (`WaitableTimer`); the timeout is set in 100 ns
    units rounded up, with a millisecond fallback when the timer cannot be set. Other platforms (Linux): a mutex, a
    condition variable and a pending flag, set and cleared under the mutex. `Mechanism` reports which.
  - A primitive the OS refuses is `IOFailed` with the native error. `Block` lets the fallback's
    `std::system_error` pass through (Invariant 6). A moved-from signal ignores `Signal`; `Block` and `Mechanism`
    on it are Programmer Errors.
- **Datagram** (v8-arch-gyo2-net-module step 0). `DatagramSocket` sends and receives one IPv4 UDP datagram at a time
  and never blocks.
  - `Open(local)` binds (port 0: the OS picks one) and sets the socket non-blocking and not inherited by child
    processes. A local address another socket holds is `AddressInUse`; an address that is not local is
    `InvalidAddress`; a refused one is `AccessDenied`; anything else is `IOFailed`.
  - `Send` is one `sendto`: no room now is `WouldBlock`, a datagram the OS will not send is `TooLarge`, an
    unreachable peer or network is `Unreachable`. A datagram the OS accepted may still be lost.
  - `Receive(buffer)` takes one waiting datagram or returns none. A datagram longer than the buffer is dropped and
    reported as `TooLarge` (POSIX: `MSG_TRUNC` in `recvmsg`'s flags; Windows: `WSAEMSGSIZE`).
  - An ICMP reply to an earlier send never fails a receive: Windows turns off `SIO_UDP_CONNRESET` and
    `SIO_UDP_NETRESET` at `Open`; macOS and Linux do not report it on an unconnected socket.
  - macOS and Linux share `src/sys/DatagramSocket.cpp` (BSD sockets, `fcntl`); Windows (`src/sys/windows/`) holds
    one `WSAStartup` reference per socket and links `ws2_32`. `SocketAddress::Parse` (four decimal octets) has no
    OS call and lives in `src/common`.
  - IPv6 is not here: no named Feature needs it (the net Module declaration, step 0).
- **Stream** (v8-product-pvp-gateway segment 3). `StreamSocket` is one IPv4 TCP client connection and never blocks;
  the caller asks again on its next frame.
  - `Connect(to)` starts connecting (non-blocking, not inherited by child processes, `TCP_NODELAY`). The OS refusing
    to start it is `InvalidAddress`, `AccessDenied` or `IOFailed`; everything after that is reported by `State`.
  - `State` is `Connecting` or `Connected`; a connect that failed is `Unreachable` (refused, no route, timed out) or
    `IOFailed`, and it stays failed. The OS's own connect timeout (tens of seconds; it differs by platform) is the
    only timeout here: a caller that wants a shorter one drops the socket.
  - `Send` returns the bytes the OS took: 0 when it has no room or the connect has not finished. `Receive` returns
    the bytes taken, 0 when the peer closed its side (an empty buffer also reads 0), none when nothing is waiting or
    the connect has not finished. Both check the connection's own state first, so the three platforms answer alike
    before `Connected` and after a failed connect (State's error); a reset connection is `IOFailed`.
  - A send to a reset connection never raises a signal: `MSG_NOSIGNAL` on each send where the C library has it (Linux,
    macOS SDKs since 2023), else `SO_NOSIGPIPE` on the socket (older macOS SDKs); the feature macros choose one, no OS
    macro. macOS and Linux share `src/sys/StreamSocket.cpp` (`poll` and `SO_ERROR`);
    Windows (`src/sys/windows/StreamSocket.cpp`) uses `select` (WSAPoll does not report a failed connect before
    Windows 10 2004) and holds one `WSAStartup` reference per socket.
  - The address is `SocketAddress` (an IPv4 address and port). A connect to `0.0.0.0` is the OS's to answer
    (POSIX reaches the local host, Windows refuses it). In Linux, `EADDRNOTAVAIL` from a connect can mean the local
    ports ran out; it is reported as `InvalidAddress` like the address that is not local.
  - No listener: no product serves TCP from C++ (a game's servers are its own services). The tests' listener is
    the test library `gyo_test_loopback_listener` in tests/common/os.
- **Paths and text.** UTF-8 in `std::string_view`, UTF-8 out in `std::string`. Invalid UTF-8 is `InvalidEncoding`.
  On Windows the layer converts to UTF-16 and calls the wide APIs. Long-path forms (`\\?\`, `\\?\UNC\`) are made
  inside the layer just before the OS call; they are never returned or written into an error.
- **Data and time.** Bytes are `std::span<std::byte>`; durations and time points are `std::chrono`.
- **Names.** engine/core/os names never reuse a Win32 function name or a `windows.h` A/W macro name (`ReadFile`,
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
- **R4**: in runtime code (engine outside engine/core/os, plugins, games and tools), fstream, `fopen`
  and every `std::filesystem` operation that touches the file system. The `path` type and pure path
  operations are allowed.
- **R5**: in engine/core/os, an include of anything but the standard library, OS headers, `engine/core/base` and
  `engine/core/os`.
- **R6**: in engine/core/os, an ANSI code page constant (`CP_ACP`, `CP_THREAD_ACP`, `CP_OEMCP`, `CP_MACCP`) or an ANSI
  Win32 entry point (a name ending in a capital `A` after a lower-case letter or digit, called unqualified or with
  `::`: `CreateFileA(`, `GetModuleFileNameA(`). Invariants 5 and 11: engine/core/os converts with `CP_UTF8` through the
  wide entry points and does not rely on the process code page. `gyo_os` is compiled with `UNICODE`, so a Win32
  name without its suffix resolves to the wide entry point.

engine/core/os/src is scanned for R5 and R6 only; engine/core/os/include is scanned for everything. Comments, string literals and
character literals are ignored; `#include` lines are matched as written. The scan covers the roots of
`build/cmake/GyoSourceRoots.txt`: engine, plugins, games, tools, examples, build/acceptance and tests (except
tests/common/os, third_party and build/target). engine and tests must yield
at least one source; the other roots are skipped, and reported as skipped, while they hold none (removing the SDL
Plugins may leave plugins/ empty, Design Invariant SDL-2 of `target/v2/sdl.zh-Hant.md`). The roots name no product or
Plugin, so adding or removing one changes no guard file.

Existing sites are frozen in ratchet allowlists named `os_boundary_allowlist.txt`. A line is
`path rule count  # reason`. The count must equal the file's findings for that rule: more fails (a new site), fewer
fails and asks for the entry to shrink, a missing file fails. The Engine allowlist (`tests/common/os`) covers
engine/ and tests/common/; any other allowlist covers only files under its own directory, so deleting a product
deletes its allowlist and no common file changes.

Three configure-time checks run once the whole project is configured (`tests/common/os/CMakeLists.txt`): `gyo_os`
links exactly `GYO::Base` and system libraries Invariant 3 allows: on Windows `shell32` for `CommandLineToArgvW`
(D49, D50) and `SHGetFolderPathW` (v8-arch-gyo2-os-user-data), elsewhere `Threads::Threads` for
`pthread_setname_np` (D69) and the wait fallback's mutex and condition variable (v8-arch-gyo2-os-wait); and
no target or directory under the scanned roots outside engine/core/os and
tests/common/os has an OS-identifying compile definition (Invariant 1). A definition is OS-identifying when its
name contains an OS word (`WIN32`, `WINDOWS`, `APPLE`, `DARWIN`, `LINUX`, `POSIX`, `UNIX`, `ANDROID` and others,
so `GYO_WINDOWS` and `WIN32_LEAN_AND_MEAN` are refused) or when it is conditioned on `$<PLATFORM_ID:...>`. The
value is not read, since paths may contain any word. `COMPILE_DEFINITIONS`, `INTERFACE_COMPILE_DEFINITIONS` and
the `-D`/`/D` entries of `COMPILE_OPTIONS` are checked, on targets and directories alike. The check is a module
(`tests/common/os/OsDefinitionCheck.cmake`), so `cmake.ProcessCodePage` also runs it on its synthetic tree, where a
target under plugins/ is refused on every OS before any real Plugin carries one (`v8-arch-gyo2-sdl-p`); like the
code page walks it follows each directory once along its ancestry. The third
(`tests/common/os/ProcessCodePageCheck.cmake`) checks Invariant 11: every executable target under the scanned roots
carries the `GYO_PROCESS_CODE_PAGE` mark the build layer sets, or an exemption with its reason, which is listed. It
is written apart from the walk that applies the manifest, walks from the top directory by source path as well as
from the recorded directories, and runs on every platform, so a target the walk misses fails anywhere. It also
refuses a directory inside the repository that is under no root of `build/cmake/GyoSourceRoots.txt`, nor under
third_party/ or the build tree (where FetchContent populates): such a directory is the repository's own code that
the list does not name yet, and would get neither the manifest nor the check (plugins/ and examples/ were missed that
way; `v8-arch-gyo2-build-cleanup-b1-b2`).
`cmake.ProcessCodePage` runs the build layer and the check on a synthetic tree: a plain build, a standalone tool
wrapper, a directory the build layer did not record (refused), a directory under no root (refused), a dependency
populated in a build tree inside the repository (not refused) and an exemption (listed). The walk, the check and the
OS definition check read the same roots (`build/cmake/GyoSourceRoots.txt`); the walk and the check stay apart in
everything else. `cmake.ProcessCodePage` pins that list, so a root removed from it fails there instead of silently
narrowing every reader.

`OSHeaderHygieneTests` (CTest `os.header_hygiene`) checks Invariant 4 on Windows: after the engine/core/os public
headers, no `windows.h` or winsock guard macro and no `min` or `max` macro is defined. It is recorded before
doctest is included and fails if a public header is not included by the test. The POSIX side is the text scan of
engine/core/os/include, since libc++ and libstdc++ define `_PTHREAD_H` from `<string>`.

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
- Paths taken from narrow `argv` strings in programs that do not use `OS::Console::CommandLineUtf8` (today the
  acceptance programs and the host shader tool): on Windows 10 1903 and later they are UTF-8 through the manifest
  (Invariant 11); on earlier Windows they are in the system's ANSI code page.
- R6 reads names, not values: a code page given as a number (`MultiByteToWideChar(0, ...)`, 0 being `CP_ACP`), a
  conversion through the value `GetACP()` returns, or an ANSI entry point reached through a macro or a function
  pointer is not seen; nor are names that start in lower case (`lstrlenA`) or are all capitals
  (`MAKEINTRESOURCEA`). A call written `p-> NameA(` (a space after the arrow) is a false positive.
- Invariant 11 is checked on the build graph and in one executable (`gyo_os_tests`), not in every built file: the
  host shader tool's call is not checked, an exemption is only listed, and the configure-time check exists only
  when `BUILD_TESTING` is on.
- R3's bare `signal(` also matches a variable or declaration named `signal`. Such a false positive is listed in the
  owner's allowlist with its reason; the pattern is not loosened.

## Out of scope

- Windows, input, clipboard, GPU and the SDL clock domain stay with the SDL adapter and its own Design
  Invariants (not in gyo2 yet; it returns with the SDL batch, without `engine/platform`); engine/core/os does not
  link SDL or call `SDL_Init`.
- The build layer: CMake's `WIN32`/`APPLE`/`UNIX`, MSVC flags, CI workflows, Python acceptance scripts and the
  platform columns of `config/`. Exception: Invariant 1's ban on OS-identifying macros also applies to
  compile definitions a non-os target gets from CMake.
- The Go relay service.

## Design Invariants

1. **OS knowledge lives only in engine/core/os.** C/C++ sources outside engine/core/os/src (engine, plugins, games,
   tools, examples, build/acceptance and tests, except tests/common/os) include no OS header (R1), branch on no OS macro in a
   preprocessor condition (R2) and call no listed OS or CRT-split API (R3). A non-os target gets no
   OS-identifying compile definition from CMake. Compiler and architecture macros are not OS branches. Exceptions
   live only in an owner's ratchet allowlist.
2. **Native file access goes through engine/core/os only** (R4). Runtime code in engine (outside engine/core/os,
   engine/core/io included), plugins, games and tools uses no fstream, `fopen` or `_wfopen` and no
   `std::filesystem` operation that touches the file system; only the `path` type and pure path operations. The
   shader tool has no exception (it had one while it lived in master's engine/render): sdl_gpu's build-time shader
   tool is runtime code under plugins/ and reads and writes files through SDL (v8-arch-gyo2-sdl-r2a). Files
   go through engine/core/io's abstraction, whose native backend calls engine/core/os; process facts come from engine/core/os
   directly. (master's engine/trace wrote its files through engine/core/os directly, os-subsystem open decision 2,
   D58; gyo2's trace Module writes through logging and has no such exception.) Tests and build/acceptance are not bound by R4: they only write temporary paths the developer
   controls. A Plugin's own tests (plugins/<name>/tests/) are bound by it like the rest of plugins/: there is no
   exclusion inside a runtime root (`v8-arch-gyo2-sdl-p`).
3. **engine/core/os is the lowest layer for OS differences** (R5 and the link check). `GYO::OS` links only `GYO::Base`
   and OS system libraries (`shell32` and, for `DatagramSocket` and `StreamSocket`, `ws2_32` on Windows (v8-arch-gyo2-net-module step 0, v8-product-pvp-gateway segment 3); `Threads::Threads` elsewhere, since the thread naming moved in,
   os-threads-backend, D69, and for the wait fallback, v8-arch-gyo2-os-wait). engine/core/os
   includes only engine/core/base, the standard library and OS headers. Its API has no type of a module above it: no
   `Uri`, `IFileSystem` or `IStream`.
4. **Public headers do not leak the OS.** engine/core/os/include includes only the standard library and engine/core/base. A
   class that holds a native handle keeps it opaque (pimpl or opaque storage); no `HANDLE` or file descriptor type
   appears in a header. Code that includes these headers needs no `NOMINMAX`.
5. **Neutral types and UTF-8.** Paths are UTF-8 `std::string_view` (UTF-8 `std::string` out), data are byte
   spans, time is `std::chrono`. Invalid UTF-8 is `InvalidEncoding`. On Windows every path is converted to UTF-16
   for the wide APIs; the ANSI code page APIs are never used. Long-path forms are made only inside engine/core/os just
   before the OS call; they are never returned or written into an error.
6. **Failures are Runtime Errors.** Functions return `Base::Result<_, OSError>`; the detail keeps the native error
   in the fixed form of the Contracts section. A request that only reports whether the platform applied it returns
   `bool` instead (the thread name and priority, `Utf8ConsoleOutput::Applied()`): not being applied is a result,
   not a failure (Invariant 10; D70). No exception crosses the public boundary or an OS callback (signal
   handler, console control handler), with one exception: `WakeSignal::Block` lets the standard library's
   `std::system_error` from the fallback's mutex and condition variable pass through, as engine/core/time's `Waiter`
   did before the move (as D72 approved for the earlier move). The OS callbacks only write a lock-free atomic (and, on a console close,
   wait) and are `noexcept`, so engine/core/os needs none of error-handling.md's "Exception at the platform boundary"
   rules (no `catch (...)`, no destructor checking global state); that section's scope stays the SDL adapter
   (D49; not in gyo2 yet).
7. **No product knowledge, no policy.** No product or tool name, file name, schema or log field appears in
   engine/core/os. Which socket errors are transient, what follows a shutdown request and how a time stamp is formatted
   are decided by the caller.
8. **State authority** (AGENTS.md §3). engine/core/os owns only its own OS resources and the shutdown-request flag.
   Queries (executable path, CPU time, file status) read the OS every time; nothing is cached to decide on. The
   Waiter's latch belongs to Time; engine/core/os never reads it. A pending signal is the wake primitive's own state on
   every platform and ends at most one block (kqueue's user event with `EV_CLEAR`, the Windows auto-reset event, the
   fallback's flag set and cleared under its mutex). Guards: `WakeSignal`'s API takes no latch; the WakeSignalTests
   cases "a signal sent before the block stays pending", "a consumed signal does not end the next block", "a signal
   that ended a block does not end the next one", "a block without a timeout ends only on a signal" and "loses no
   signal sent while the waiter checks its flag", with mutants ow-x1, ow-x3, ow-x11, ow-x12, ow-x13 and ow-x16 to
   ow-x22.
9. **Moving a site does not change observable behaviour.** File bytes (CRLF on Windows, append semantics),
   `WaitBackend` strings, name truncation, priority mapping, timeout conversion (100 ns rounding and the
   millisecond fallback on Windows, `NOTE_NSECONDS` on kqueue) and exception specifications stay as they are. The
   only allowed difference is a change the batch declares beforehand (marked ★ in the plan).
10. **Every capability has an explicit result on every OS.** A request that needs no OS call is met as asked and
    counts as applied (for example the Normal priority, which `RoleThread` reports as applied today). A request
    that needs an OS call the layer does not make reports not-applied or `Unsupported`; it never reports success
    silently. When OSes cannot do the same thing, the difference is written in the type or the documentation.
11. **Windows processes run with UTF-8 as their ANSI code page** (os-utf8-codepage, D64–D66). Every Windows
    executable that CMake builds from this repository's own C/C++ sources (products, tools, tests, probes, the host
    shader tool) embeds the manifest that declares `activeCodePage` UTF-8, so `GetACP()` is 65001 in the process
    whatever the system
    setting (Windows 10 1903 and later). It is the second line: engine/core/os still converts with `CP_UTF8` and calls
    only wide APIs (Invariant 5), so its behaviour does not depend on the manifest. Guards: the configure-time check
    that every executable target carries the manifest or a reasoned exemption; `gyo_os_tests` reads its own manifest
    resource and checks `GetACP()` ("OS files run with UTF-8 as the process ANSI code page"); the source guard
    refuses the ANSI code page constants and ANSI Win32 entry points in engine/core/os (R6); `cmake.ProcessCodePage`.
    Mutants pu-x8 to pu-x11, pu-x14 to pu-x16, os-f4, os-c6, pu-x5, gdir-code-page-walk-skips-games and
    gdir-code-page-check-skips-games (the games/ root, `v8-arch-gyo2-build-games-dir`), sdlp-code-page-walk-skips-plugins
    and sdlp-code-page-check-skips-plugins (the plugins/ root, `v8-arch-gyo2-sdl-p`), bcl-code-page-walk-skips-examples,
    bcl-code-page-check-skips-examples, the two that drop a root from `build/cmake/GyoSourceRoots.txt`,
    bcl-source-roots-drop-examples and bcl-source-roots-drop-build-acceptance, and the three of the directory under no
    root, bcl-check-misses-unlisted, bcl-check-unlisted-third-party and bcl-check-unlisted-build-tree
    (`v8-arch-gyo2-build-cleanup`).

### Replacement Criteria

1. A consumer needs engine/core/os to hand out a native handle (socket, `HANDLE`, file descriptor), or must know which
   OS it is on to use a neutral function correctly: stop adding parameters; propose a native-handle layer or a
   redesign of that component.
2. A change to the Wait backends changes timing behaviour (another mechanism, another timeout conversion), or a
   Feature needs a sub-millisecond verdict that CI L1 cannot give: propose splitting Wait into a component of its
   own with its own timing acceptance, guard and criteria.
3. Implementing a capability needs engine/core/os to depend on engine/core/io, Time, Threads, Trace, SDL, asio or
   `GYO::Engine`: the layering is wrong; stop and let the user decide instead of adding the edge.
4. A consumer needs asynchronous or event-driven file I/O, directory watching or large directory listings, and the
   narrow primitives of Files would have to grow into a second `IFileSystem`: stop and redraw the split between
   engine/core/os primitives and engine/core/io backends.
5. Keeping a caller's bytes (Invariant 9, for example CRLF) needs engine/core/io or engine/core/os to maintain OS-specific
   stream wrappers: propose deciding line-ending semantics as a Data Contract instead of converting silently in a
   backend.
6. An allowlist grows instead of shrinking for two batches in a row, more than two permanent entries are needed
   outside tests, or false positives need repeated rule changes: rewrite the rules or narrow Invariant 1 instead of
   adding exceptions.
7. One capability on one OS needs branches across two or more engine/core/os files, the per-OS files are mostly
   duplicates, or a platform whose process and file model differs from both POSIX and Win32 is added: propose a
   backend interface with a selection table.
8. (os-utf8-codepage, D67) A product or tool needs manifest settings of its own (DPI awareness, the
   UAC level, `longPathAware`) or a code page other than UTF-8: stop editing engine/core/os's manifest for it; decide how
   an owner declares manifest parts and how they merge (MSVC merges `.manifest` sources, a `.rc` holds one
   resource), or move the manifest out of engine/core/os.
9. (os-closeout, D78) engine/core/os stays the only place for OS differences, and grows only with observed pressure
   (the user's owner decision D78 for engine/core/os, stricter than AGENTS.md §2, which lets foundation architecture be designed
   ahead from foreseen pressure). A batch that moves a site into engine/core/os is accepted
   by CI L1 and the guards alone. If it needs a real-machine L2 or L3, a ★ declaration or a changed test verdict
   (AGENTS.md §14.4) to be accepted, it is not a pure move: stop, keep the move behaviour-preserving, and hand the
   behaviour change to the product version whose Feature needs it. A component or batch that no observed pressure
   and no Feature needs is not started; OS code outside engine/core/os moves in only when pressure is observed there.
   The user's owner decision can also bring a frozen component back; such a batch is accepted as its own declaration
   says, and the declaration names its ★ items and its test verdict changes (AGENTS.md §14.4). Wait came back this
   way (v8-arch-gyo2-os-wait, 2026-11-21).
