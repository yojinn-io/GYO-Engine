#pragma once

#include <memory>
#include <string>

#include "engine/io/fs/IFileSystem.hpp"

namespace GYO::Engine::IO::FS {

    /// The file:// backend of IFileSystem (docs/architecture/io.md).
    /// - Every OS call goes to engine/os; this class has no OS code.
    /// - A URI names a native path through the file:// mapping (Path::NativePathFromFileUri).
    /// - Open reads, or writes a file it creates when missing (Write | CreateIfMissing with Truncate or
    ///   Append; Text for the platform's line endings). Exists, Stat, CreateDirectories, Move (one rename on
    ///   one volume that replaces a file already at the target, OS::Files::RenameReplacing), Remove (a file;
    ///   a directory is InvalidPath) and ToNativePathString work.
    /// - It keeps no state, so any number of threads may call it at once (Design Invariant 6).
    /// - An engine/os failure keeps its code name and native detail (Base::CauseDetail).
    class NativeFileSystem final : public IFileSystem {
    public:
        NativeFileSystem() = default;

        const char* Name() const noexcept override { return "NativeFS"; }

        IOResult<std::unique_ptr<Stream::IStream>>
        Open(const Path::Uri& uri, Stream::FileOpenMode mode) override;
        IOResult<bool> Exists(const Path::Uri& uri) override;
        IOResult<FileInfo> Stat(const Path::Uri& uri) override;
        IOResult<std::string> ToNativePathString(const Path::Uri& uri) override;

        IOResult<void> CreateDirectories(const Path::Uri& uri) override;
        IOResult<void> Remove(const Path::Uri& uri) override;
        IOResult<void> Move(const Path::Uri& from, const Path::Uri& to) override;
    };

    /// The file URI of a native path in UTF-8, for this backend. A relative path is made absolute against the
    /// current directory first (OS::Files::MakeAbsolute), as the OS would resolve it.
    IOResult<Path::Uri> NativeFileUri(std::string_view utf8Path);

    /// Whether a file written at the native path a person named lands in root, or is root (docs/architecture/
    /// io.md, "User files"). The candidate's parent is resolved by the OS (OS::Files::MakeAbsolute, then
    /// OS::Files::ResolvedPath: links before '..') and its last name kept as written, since an atomic save
    /// renames over that name, a link or not; root is resolved whole. A trailing separator names the directory
    /// before it. Names are compared with OS::Files::SamePathComponent. A path the OS cannot resolve (no
    /// permission) is compared with '.' and '..' removed as written. It decides containment only and changes no
    /// path; it is not a sandbox: a link named inside root counts as inside, wherever it leads. An empty path is
    /// false; a path that is not UTF-8, or that has a Win32 namespace prefix ("//?/", "//./"), is InvalidPath.
    IOResult<bool> IsNativePathWithin(std::string_view utf8Candidate, std::string_view utf8Root);

} // namespace GYO::Engine::IO::FS
