# GYO file abstraction

Owner: Engine (`engine/io`, compiled into `GYO::Engine`).
Plan and record: [os-subsystem](plans/os-subsystem/README.md), batches io-contracts, os-files-read and io-native-paths (D41–D46, D54).

## What it is

engine/io is the Engine's OS-neutral file abstraction: `IFileSystem` (one backend: native files, a pack, memory),
`Vfs` and `MountTable` (schemes routed to backends, read overlays), `Uri` and `Path` (logical and file paths),
streams and read/write helpers. It contains no OS code. Its native backend (os-files-read) implements
`IFileSystem` for `file://` and calls the file primitives of engine/os for every OS operation:

```text
engine/asset ──→ engine/io (IFileSystem, Vfs, Uri) ──→ engine/os (Files) ──→ engine/base
```

## Path contract

- **UTF-8.** Every path string in engine/io is UTF-8: `Uri` and `Path`, resolver roots, the paths an
  `IAssetSource` reads, and the paths handed to engine/os. A caller that holds a `std::filesystem::path` converts it
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
    The mapping never resolves a path. Lexical rules only refuse or keep a request inside a root: the drive
    lookalike check below, the asset resolver's cleaning of a catalog path, `MountTable`'s refusal of `..`.
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
- **The loose parser.** `ParseUriLoose` gives a `file://` URI (the scheme name in any case) that `ParseUri` accepts
  the same path. A `file://` URI that `ParseUri` rejects gets no scheme from the loose parser, so it never names a
  native path. Other schemes keep the loose meaning: leading slashes are dropped and the rest is a logical path
  (`res:///a` is `a`).
- **Mounts stay inside their root.** `MountTable::Resolve` rejects a request with a `..` segment
  (`PathEscapesRoot`), so a mount never names a path outside its root. A UNC root works as any other file root.
- **The native backend** (`FS::NativeFileSystem`, os-files-read, os-files-write, os-tool-files) serves `file://` URIs through
  engine/os. `Open` reads, or writes a file it creates when missing: `Write | CreateIfMissing` with `Truncate` or
  `Append`, and `Text` for the platform's line endings (`\r\n` on Windows, as a text-mode C++ stream writes;
  without it the bytes go as given). Its write stream has no position and no buffer (each `Write` reaches the OS,
  `Flush` has nothing left to do), and `Close` reports a failure. Read and write together, overwriting in place
  and writing an existing file only are `NotSupported`. `Exists`, `Stat`, `CreateDirectories`, `Move`, `Remove`
  and `ToNativePathString` work; every other operation is `NotSupported`, and `Capabilities()` sets every field to
  say exactly that (the defaults of `FileSystemCapabilities` are mostly true). `Move` is one rename on one volume
  that replaces a file already at the target (`Os::Files::RenameReplacing`); `Remove` removes a file (a directory
  is `InvalidPath` on every platform), and a recursive request is `NotSupported` (D56).
  - `Helpers::WriteFileAtomically` is the one atomic write (D56; the editor's save, and IP-5's settings later): the
    bytes go to `<name>.tmp.<time>.<random>` beside the target, which is closed and then moved over it, so a reader
    sees the old file or all of the new bytes. When a step fails the temporary file is removed and the target keeps
    its old file. It works on any `IFileSystem` whose `Move` replaces in one step; the parent directory must exist.
  - `FS::NativeFileUri` gives the file URI of a native UTF-8 path for this backend, making a relative path
    absolute against the current directory first, as the OS would (`Os::Files::MakeAbsolute`). So a relative path
    keeps its dot segments on POSIX, while on Windows `GetFullPathNameW` has already removed them. A current
    directory with no UTF-8 form is `InvalidPath` with the engine/os cause `InvalidEncoding` in the detail (D43). A
    missing file is `NotFound`, so a VFS read overlay falls through to
  the next mount. `caseSensitivePaths` is the platform default from engine/os; a volume may differ.
  - The path must also be absolute on the platform the backend runs on (`Os::Files::IsAbsolutePath`): `/x` names
    the root of the current drive on Windows and `C:/x` is relative on POSIX, so the backend refuses them
    (`InvalidPath`) instead of letting the OS reinterpret them.
  - Its read stream seeks like the other read-only streams (`MemoryStream`, `SpanStream`): a target outside
    `[0, Size()]` is `SeekFailed`; a read of zero bytes is not the end of the file.
- **Errors.** Failures are `IoError`. When a backend converts an engine/os failure, it chooses the `IoErrorCode`,
  keeps the inner message, and records the cause with `Base::CauseDetail(osError, outerDetail)`, for example
  `C:/x/y.dxil; NotFound: win32=3; path_bytes=271`. The native error code is never dropped.

Behaviour that changed with this contract (io-contracts):

- A resolver that allows schemes used to treat `file:///x` and `file://localhost/x` in a catalog as the relative
  paths `x` and `localhost/x` under its assets root; it now sees the absolute path `/x` and rejects it, like any
  absolute path (`AssetPathResolverTests`). The asset manifest and catalog resolvers do not allow schemes; the
  shader-bundle resolvers do (the default), but no shipped catalog or manifest has a `file://` entry.
- `MountTable::Resolve` used to pass a `..` segment through unresolved; it now refuses it.

Behaviour that changed with io-native-paths (D54):

- A native path or file URI kept its dot segments only until the mapping removed them by a lexical rule; they now
  reach the OS. On POSIX, `/a/link/../x` used to name `/a/x` and now names the `x` beside the link's target, the
  file the OS itself opens for that path. Windows reads the same file as before (`GetFullPathNameW` is lexical).
  Reads the lexical rule used to rescue now fail by the OS's rules: on POSIX every segment before a `..` must be
  an existing directory (`missing/../x`, `file.txt/../x` are `NotFound`), and a trailing `/` or `/.` after a file
  name is an error.
- UNC paths are supported (D46's limit is lifted): `file://server/share/x` and the native `//server/share/x` map to
  each other instead of `NotSupported` and `InvalidPath`; on Windows a long UNC path reaches the OS as `\\?\UNC\`
  (os.md). A POSIX native path that starts with `//` is accepted instead of being refused as UNC. A UNC host with
  a `?` or `:` and every spelling of a POSIX path that looks like a drive stay refused.
- The asset resolver keeps its root as written, `\` turned into `/` only: a root's `..` and a UNC root's leading
  `//` reach the OS, and a root whose `..` climbs above its start (`../assets`) is no longer `PathEscapesRoot`.
  The catalog path is still cleaned and checked inside the root by a lexical rule. An escape that
  `allowEscapeAssetsRoot` permits keeps its `..` (`assets/game/../shared/a.ppm`, was `assets/shared/a.ppm`);
  `allowAbsolutePath` still removes dot segments lexically (outside D54; no product sets either option).

## Design Invariants

1. **engine/io contains no OS code.** No OS header, no OS macro and no native file access (fstream, `fopen`,
   `std::filesystem` operations that touch the file system) in engine/io; native access goes through engine/os
   only. Checked by the OS boundary guard (R1–R4 cover engine/io; mutant os-g4).
2. **Every path string is UTF-8.** No engine/io API takes or returns a path in another encoding. engine/io does
   not transcode: path bytes pass through unchanged (`UriTests`, the UTF-8 case); an invalid sequence is refused
   by engine/os with `InvalidEncoding` before any OS call (`tests/common/os/FilesTests.cpp`).
3. **One `file://` mapping on every platform, and it never resolves a path.** Parsing, printing and the
   native-path helpers follow the rules above as pure, locale-independent string rules, so the same input gives the
   same result on Windows, Linux and macOS; the round trip through the canonical form is a fixed point
   (`tests/common/core/io/UriTests.cpp`). The path is kept as written: dot segments and repeated slashes reach the
   OS, which resolves them its own way when the file is opened (`NativeFileSystemTests.cpp`, the symbolic link
   case); a lexical rule only refuses or keeps a request inside a root (D54). Every spelling the tests generate
   keeps the round trip (`UriTests`, the generated spellings).
4. **Long-path prefixes never appear in engine/io strings.** `ParseUri` and `FileUriFromNativePath` reject them
   (`UriTests`); engine/os makes them just before the OS call.
5. **A mount never names a path outside its root** (`UriTests`, the mount cases).

### Replacement Criteria

1. A second non-native file source (a pack or memory) has to be written twice, once as an `IFileSystem` and once as
   an `IAssetSource`: decide whether `IAssetSource` stays, is renamed or is removed (os-subsystem open decision 4)
   before writing it.
2. A consumer needs percent-encoding, a query or a fragment in a file URI, or a path that is not UTF-8: revisit the
   mapping as a Data Contract change instead of adding exceptions.
3. A consumer needs the mapping to resolve a path (one name for two spellings of a file, a cache key that follows
   links): add a resolving helper over engine/os beside the mapping instead of normalizing inside it. One site still
   normalizes a native path lexically: the asset resolver's `allowAbsolutePath` removes the dot segments of an
   absolute catalog path and collapses a UNC path's leading `//` (no product sets it; os-subsystem HANDOFF, open
   items, D55).
4. A scheme other than `file://` needs RFC 3986 rules the logical-path `Uri` does not have: split the URI type
   instead of growing this one.
