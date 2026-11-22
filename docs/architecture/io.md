# GYO file abstraction

Owner: Engine (`engine/core/io` → `GYO::IO`; core, not a Module).
Plan and record: [os-subsystem](plans/os-subsystem/README.md), batches io-contracts, os-files-read and io-native-paths
(D41–D46, D54). Re-reviewed for GYO2 in [threads-io-review](target/v2/threads-io-review-declaration.zh-Hant.md)
(`v8-arch-gyo2-threads-io-review`, 2026-11-22): the parts with no user and no named Feature were removed (the file
watcher, directory listing, copying, the capability table, the stream classes other than `IStream`, non-atomic
whole-file writes, `Vfs`, `MountTable`, `MountPoint` and the loose URI parser), whole-file reads over the limit
became `TooLarge`, and Design Invariant 5 (mounts) was dropped with the mounts. The text before it:
`master@08566260:docs/architecture/io.md`.

## What it is

engine/io is the Engine's OS-neutral file abstraction: `IFileSystem` (one file backend; native files today),
`Uri` and `Path` (file URIs and paths), the `IStream` a backend opens, and whole-file helpers. It contains no OS
code. Its native backend implements `IFileSystem` for `file://` and calls the file primitives of engine/os for every
OS operation:

```text
asset, tools, a game's save files ──→ engine/core/io (IFileSystem, Uri, helpers) ──→ engine/core/os (Files) ──→ engine/core/base
```

Users: asset reads content files from threads outside the World (asset 5.12); tools read and write the files a
person names (user files, below); a game reads and writes its save files with `ReadAllBytes` and
`WriteFileAtomically` (restore-entry 19.6; the save format is the author's).

## Path contract

- **UTF-8.** Every path string in engine/io is UTF-8: `Uri` and `Path` and the paths handed to engine/os. A caller that holds a `std::filesystem::path` converts it
  with the UTF-8 helpers of engine/os, never with `path::string()` (the ANSI code page on Windows).
- **One `file://` mapping**, as pure strings, the same on every platform:

  | URI | Native path in engine/io |
  |---|---|
  | `file:///Users/x` | `/Users/x` |
  | `file:///C:/x`, `file:///c:/x` | `C:/x`, `c:/x` (the drive letter is the first path segment, as in RFC 8089) |
  | `file:///C:` | `C:/` |
  | `file://localhost/...` (any case) | as an empty authority |
  | `file:///a/./b/../c`, `file:///a//b/` | `/a/./b/../c`, `/a//b/`: kept as written |
  | `file://server/share/x`, `file:////server/share/x`, `file://///server/share/x` | `//server/share/x` (UNC, D54) |
  | `file:////localhost/C$/x` | `//localhost/C$/x`: a UNC host named localhost, since `file://localhost/` is the local machine |
  | `file://C:/x` | rejected (`InvalidPath`): the drive letter is not an authority |
  | `file://`, `file://localhost` | rejected (`InvalidPath`): no path |

  - A file URI always names an absolute path, **kept as written** (io-native-paths, D54): dot segments, repeated
    and trailing slashes reach the OS, which resolves them by its own rules when the file is opened. On POSIX
    `link/..` is the parent of the link's target; on Windows `GetFullPathNameW` removes the `..` lexically first.
    The mapping never resolves a path. Lexical rules only refuse: the drive lookalike check below.
  - UNC: after an empty or localhost authority, two or more leading slashes start a UNC path (`file:////host`,
    `file://///host`, RFC 8089 appendix E); any other authority is a UNC host. A host alone or a host and share
    (`file://server`, `file://server/share`) maps as it is, and the OS decides what it names. A host with a `?` or
    `:` is rejected (`InvalidPath`) in every spelling: no Windows server has one, and it would read as a namespace
    prefix (the host `??`), a drive (`//C:/x`) or a port. On POSIX the OS reads `//host/x` as the local `/host/x`,
    so there `file://host/x` opens a local file.
  - The path bytes are UTF-8 as they are: no percent-encoding or decoding, and `?` and `#` belong to the path (a
    file URI has no query or fragment).
  - Backslashes in the path are separators, as `/`; so a POSIX file name that contains a backslash cannot be
    written as a file URI (the POSIX path `/\x` maps to the UNC path `//x`).
  - Windows namespace prefixes (`\\?\`, `\\.\`, `\??\`, in either slash form) are rejected (`InvalidPath`).
  - A POSIX path whose first segment looks like a drive, as written or once dot segments and extra leading slashes
    are removed (`/C:/x`, `/C:/../x`, `/./C:/x`, `///C:/x`), has no file URI: its URI would name the drive path.
    The check is lexical and only refuses; the path stays as written. Parsing a URI that names such a path
    (`file:///./C:/x`) is `InvalidPath`.
  - `Uri::ToString` prints the canonical form: `file:///` followed by a local path without its leading `/`;
    `file://host/...` for a UNC path, `file:////localhost/...` when the host is named localhost; a host set by hand
    in the authority is printed there. Parsing the canonical form gives the same path: the round trip is a fixed
    point, whatever spelling the first parse came from.
- **Native paths stay in `/` form.** In engine/io a native path uses `/` separators (`C:/a`, `/a`, `//server/a`).
  Backslashes, UTF-16 and long-path prefixes (`\\?\`, `\\?\UNC\`) belong to engine/os. `FileUriFromNativePath` keeps
  the path as written, turning backslashes into `/` only. One leading `/` is a local path, two a UNC path
  (`//host/...`; rejected only when there is no host), three or more the root `/`, as POSIX reads them. On POSIX a
  `//host/x` path is accepted and the OS gives it its own meaning (Linux and macOS read it as `/host/x`). It
  rejects relative paths, the drive-relative forms `C:x`, `C:` and `\x`, POSIX paths that look like a drive
  (`/C:/x`), refused UNC hosts and namespace prefixes, also once three or more leading slashes are the root. The native `C:` is rejected while the URI `file:///C:` names `C:/`: a URI path
  is always absolute.
- **`NativePathFromFileUri`** returns the path of a file URI only in the mapping's canonical form: a host set by
  hand in the authority names the UNC path `//host` + path; a query or fragment, a backslash, a POSIX path that
  looks like a drive, or a path the mapping would write otherwise (`///a`, `C:`) has no native path (`InvalidPath`).
- **The native backend** (`FS::NativeFileSystem`, os-files-read, os-files-write, os-tool-files) serves `file://` URIs through
  engine/os. `Open` reads, or writes a file it creates when missing: `Write | CreateIfMissing` with `Truncate` or
  `Append`, and `Text` for the platform's line endings (`\r\n` on Windows, as a text-mode C++ stream writes;
  without it the bytes go as given). Its write stream has no position and no buffer (each `Write` reaches the OS,
  `Flush` has nothing left to do), and `Close` reports a failure. Read and write together, overwriting in place
  and writing an existing file only are `NotSupported`. `Exists`, `Stat`, `CreateDirectories`, `Move`, `Remove`
  and `ToNativePathString` work. `Move` is one rename on one volume that replaces a file already at the target
  (`OS::Files::RenameReplacing`); `Remove` removes a file (a directory is `InvalidPath` on every platform, D56).
  It keeps no state (Design Invariant 6).
  - `Helpers::WriteFileAtomically` is the one atomic write (D56; the editor's save, and IP-5's settings later): the
    bytes go to `<name>.tmp.<time>.<random>` beside the target, which is closed and then moved over it, so a reader
    sees the old file or all of the new bytes. When a step fails the temporary file is removed and the target keeps
    its old file. It works on any `IFileSystem` whose `Move` replaces in one step; the parent directory must exist
    unless the caller asks for it to be created (User files, below).
  - `FS::NativeFileUri` gives the file URI of a native UTF-8 path for this backend, making a relative path
    absolute against the current directory first, as the OS would (`OS::Files::MakeAbsolute`). So a relative path
    keeps its dot segments on POSIX, while on Windows `GetFullPathNameW` has already removed them. A current
    directory with no UTF-8 form is `InvalidPath` with the engine/os cause `InvalidEncoding` in the detail (D43). A
    missing file is `NotFound`.
  - The path must also be absolute on the platform the backend runs on (`OS::Files::IsAbsolutePath`): `/x` names
    the root of the current drive on Windows and `C:/x` is relative on POSIX, so the backend refuses them
    (`InvalidPath`) instead of letting the OS reinterpret them.
  - Its read stream seeks within `[0, Size()]`; a target outside is `SeekFailed`; a read of zero bytes is not the
    end of the file.
- **Errors.** Failures are `IOError`. When a backend converts an engine/os failure, it chooses the `IOErrorCode`,
  keeps the inner message, and records the cause with `Base::CauseDetail(osError, outerDetail)`, for example
  `C:/x/y.dxil; NotFound: win32=3; path_bytes=271`. The native error code is never dropped.
- **User files** (v8-engine-io-user-files). Helpers for native paths a person names, in UTF-8, through
  `IFileSystem`. `Helpers::ProbeFileStamp` reports a missing file as
  `exists == false`, refuses anything that is not a regular file from its status before opening it (InvalidPath),
  and reads the file whole for its hash, up to the caller's `ReadAllOptions::maxBytes`.
  `Helpers::WriteFileAtomically` with `createParentDirectories` creates the missing directories above the file
  first; a file right under a volume's root needs none. `FS::IsNativePathWithin` decides whether a file written at
  a native path lands in a root: the candidate's parent is resolved by the OS (`OS::Files::MakeAbsolute`, then
  `OS::Files::ResolvedPath`, links before '..') and its last name kept as written, since an atomic save renames
  over that name; the root is resolved whole; a trailing separator names the directory before it; names are
  compared with `OS::Files::SamePathComponent`; a path the OS cannot resolve is compared with '.' and '..' removed
  as written. It changes no path and is not a sandbox: a link named inside the root counts as inside. A namespace
  prefix is InvalidPath, as in the file mapping.

- **Whole-file reads.** `Helpers::ReadAllBytes` and `Helpers::ReadAllTextUtf8` read a file to its end, up to the
  caller's `ReadAllOptions::maxBytes`; more is `TooLarge` (from the size the stream reports before reading, or while
  reading when it reports none), so a caller tells a file that is too big from one it could not read.
- **Threads** (Design Invariant 6). Any thread may call an `IFileSystem` at the same time as others; a stream that
  `Open` returns is used by one thread at a time. The native backend keeps no state and engine/os's file calls keep
  no shared mutable state (os.md, Files).

## Design Invariants

1. **engine/io contains no OS code.** No OS header, no OS macro and no native file access (fstream, `fopen`,
   `std::filesystem` operations that touch the file system) in engine/io; native access goes through engine/os
   only. Checked by the OS boundary guard (R1–R4 cover engine/io; mutant os-g4-io-opens).
2. **Every path string is UTF-8.** No engine/io API takes or returns a path in another encoding. engine/io does
   not transcode: path bytes pass through unchanged (`UriTests`, the UTF-8 case); an invalid sequence is refused
   by engine/os with `InvalidEncoding` before any OS call (`tests/common/os/FilesTests.cpp`).
3. **One `file://` mapping on every platform, and it never resolves a path.** Parsing, printing and the
   native-path helpers follow the rules above as pure, locale-independent string rules, so the same input gives the
   same result on Windows, Linux and macOS; the round trip through the canonical form is a fixed point
   (`tests/common/core/io/UriTests.cpp`). The path is kept as written: dot segments and repeated slashes reach the
   OS, which resolves them its own way when the file is opened (`NativeFileSystemTests.cpp`, the symbolic link
   case); a lexical rule only refuses (D54). Every spelling the tests generate keeps the round trip (`UriTests`,
   the generated spellings).
4. **Long-path prefixes never appear in engine/io strings.** `ParseUri` and `FileUriFromNativePath` reject them
   (`UriTests`); engine/os makes them just before the OS call.
5. (Dropped with `MountTable` in v8-arch-gyo2-threads-io-review; it was "a mount never names a path outside its
   root". A content root and its relative paths are asset's, asset 5.3.)
6. **Many threads may read at once.** An `IFileSystem` implementation takes concurrent calls from any thread; a
   stream that `Open` returns is used by one thread at a time. The native backend keeps no members and engine/os's
   file calls keep no shared mutable state. No guard (review): no change to a backend makes a test fail every
   time; `NativeFileSystemTests.cpp` "NativeFileSystem serves whole-file reads from several threads at once" is a
   smoke test, not a verdict.

### Replacement Criteria

1. A consumer needs to list a directory, watch files, copy, or a second file source (a pack, memory): add that one
   operation to `IFileSystem` with its tests; do not bring the capability table back in its place. Layering sources
   (a mod over the base content) is a component of its own, not priorities inside an `IFileSystem` backend.
2. A consumer needs percent-encoding, a query or a fragment in a file URI, or a path that is not UTF-8: revisit the
   mapping as a Data Contract change instead of adding exceptions.
3. A consumer needs the mapping to resolve a path (one name for two spellings of a file, a cache key that follows
   links): add a resolving helper over engine/os beside the mapping instead of normalizing inside it.
   `FS::IsNativePathWithin` is such a helper; it only decides containment and changes no path.
4. A consumer needs URIs of a scheme other than `file://` with rules of their own: give them a type of their own
   instead of growing `Uri`, whose other schemes are only kept by name (`UriScheme::Unknown`).
