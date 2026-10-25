# GYO file abstraction

Owner: Engine (`engine/io`, compiled into `GYO::Engine`).
Plan and record: [os-subsystem](plans/os-subsystem/README.md), batches io-contracts and os-files-read (D41–D46).

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
  | `file://server/share/x`, `file:////server/share/x` | `//server/share/x`; **not supported in this round** (`NotSupported`, D46) |
  | `file://C:/x` | rejected (`InvalidPath`): the drive letter is not an authority |
  | `file://`, `file://localhost` | rejected (`InvalidPath`): no path |

  - A file URI always names an absolute path. Dot segments resolve inside the root; escaping the root is
    `PathEscapesRoot`.
  - The path bytes are UTF-8 as they are: no percent-encoding or decoding, and `?` and `#` belong to the path (a
    file URI has no query or fragment).
  - Backslashes in the path are separators, as `/`; so a POSIX file name that contains a backslash cannot be
    written as a file URI.
  - Windows namespace prefixes (`\\?\`, `\\.\`, `\??\`, in either slash form) are rejected (`InvalidPath`).
  - A POSIX path whose first segment looks like a drive (`/C:/x`) has no file URI: its URI would name the drive
    path. Parsing a URI that normalizes to such a path (`file:///./C:/x`) is `InvalidPath`.
  - `Uri::ToString` prints the canonical form `file:///` followed by the path without its leading `/` (a UNC
    authority, if one was set by hand, is printed so that parsing the text reports it). Parsing the canonical form
    gives the same path: the round trip is a fixed point, whatever spelling the first parse came from.
- **Native paths stay in `/` form.** In engine/io a native path uses `/` separators (`C:/a`, `/a`). Backslashes,
  UTF-16 and long-path prefixes (`\\?\`, `\\?\UNC\`) belong to engine/os. `FileUriFromNativePath` turns
  backslashes into `/` and rejects relative paths, the drive-relative forms `C:x`, `C:` and `\x`, POSIX paths that
  look like a drive (`/C:/x`) and namespace prefixes. The native `C:` is rejected while the URI `file:///C:` names
  `C:/`: a URI path is always absolute.
- **`NativePathFromFileUri`** returns the path of a file URI only in the mapping's normalized form: a `Uri` built by
  hand with another authority (`NotSupported`), a query or fragment, or a path that does not normalize to itself
  (`InvalidPath`) has no native path.
- **The loose parser.** `ParseUriLoose` gives a `file://` URI (the scheme name in any case) that `ParseUri` accepts
  the same path. A `file://` URI that `ParseUri` rejects gets no scheme from the loose parser, so it never names a
  native path. Other schemes keep the loose meaning: leading slashes are dropped and the rest is a logical path
  (`res:///a` is `a`).
- **Mounts stay inside their root.** `MountTable::Resolve` rejects a request with a `..` segment
  (`PathEscapesRoot`), so a mount never names a path outside its root.
- **The native backend** (`FS::NativeFileSystem`, os-files-read, os-files-write) serves `file://` URIs through
  engine/os. `Open` reads, or writes a file it creates when missing: `Write | CreateIfMissing` with `Truncate` or
  `Append`, and `Text` for the platform's line endings (`\r\n` on Windows, as a text-mode C++ stream writes;
  without it the bytes go as given). Its write stream has no position and no buffer (each `Write` reaches the OS,
  `Flush` has nothing left to do), and `Close` reports a failure. Read and write together, overwriting in place
  and writing an existing file only are `NotSupported`. `Exists`, `Stat`, `CreateDirectories` and
  `ToNativePathString` work; every other operation is `NotSupported`, and `Capabilities()` sets every field to
  say exactly that (the defaults of `FileSystemCapabilities` are mostly true).
  - `FS::NativeFileUri` gives the file URI of a native UTF-8 path for this backend, making a relative path
    absolute against the current directory first, as the OS would. A missing file is `NotFound`, so a VFS read overlay falls through to
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

## Design Invariants

1. **engine/io contains no OS code.** No OS header, no OS macro and no native file access (fstream, `fopen`,
   `std::filesystem` operations that touch the file system) in engine/io; native access goes through engine/os
   only. Checked by the OS boundary guard (R1–R4 cover engine/io; mutant os-g4).
2. **Every path string is UTF-8.** No engine/io API takes or returns a path in another encoding. engine/io does
   not transcode: path bytes pass through unchanged (`UriTests`, the UTF-8 case); an invalid sequence is refused
   by engine/os with `InvalidEncoding` before any OS call (`tests/common/os/FilesTests.cpp`).
3. **One `file://` mapping on every platform.** Parsing, printing and the native-path helpers follow the rules above
   as pure, locale-independent string rules, so the same input gives the same result on Windows, Linux and macOS;
   the round trip through the canonical form is a fixed point (`tests/common/core/io/UriTests.cpp`).
4. **Long-path prefixes never appear in engine/io strings.** `ParseUri` and `FileUriFromNativePath` reject them
   (`UriTests`); engine/os makes them just before the OS call.
5. **A mount never names a path outside its root** (`UriTests`, the mount cases).

### Replacement Criteria

1. A second non-native file source (a pack or memory) has to be written twice, once as an `IFileSystem` and once as
   an `IAssetSource`: decide whether `IAssetSource` stays, is renamed or is removed (os-subsystem open decision 4)
   before writing it.
2. A consumer needs percent-encoding, a query or a fragment in a file URI, or a path that is not UTF-8: revisit the
   mapping as a Data Contract change instead of adding exceptions.
3. A consumer needs UNC or other network roots: implement that row, including keeping the leading `//` through
   `NormalizeSlashes` and `Path` normalization (`PathUtils.cpp`) and the asset resolver, which today collapse it.
4. A scheme other than `file://` needs RFC 3986 rules the logical-path `Uri` does not have: split the URI type
   instead of growing this one.
