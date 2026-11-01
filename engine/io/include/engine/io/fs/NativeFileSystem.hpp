#pragma once

#include <memory>
#include <string>
#include <vector>

#include "engine/io/fs/IFileSystem.hpp"

namespace Engine::IO::FS {

    /// The file:// backend of IFileSystem (docs/architecture/io.md).
    /// - Every OS call goes to engine/os; this class has no OS code.
    /// - A URI names a native path through the file:// mapping (Path::NativePathFromFileUri).
    /// - Open reads, or writes a file it creates when missing (Write | CreateIfMissing with Truncate or
    ///   Append; Text for the platform's line endings). Exists, Stat, CreateDirectories, Move (one rename on
    ///   one volume that replaces a file already at the target, Os::Files::RenameReplacing), Remove (a file,
    ///   not recursive) and ToNativePathString work; every other operation is NotSupported, as
    ///   Capabilities() says.
    /// - An engine/os failure keeps its code name and native detail (Base::CauseDetail).
    class NativeFileSystem final : public IFileSystem {
    public:
        NativeFileSystem() = default;

        const char* Name() const noexcept override { return "NativeFS"; }

        IoResult<std::unique_ptr<Engine::IO::Stream::IStream>>
        Open(const Engine::IO::Path::Uri& uri, Engine::IO::Stream::FileOpenMode mode) override;
        IoResult<bool> Exists(const Engine::IO::Path::Uri& uri) override;
        IoResult<FileInfo> Stat(const Engine::IO::Path::Uri& uri) override;
        IoResult<std::string> ToNativePathString(const Engine::IO::Path::Uri& uri) override;
        FileSystemCapabilities Capabilities() const noexcept override;

        IoResult<void> CreateDirectories(const Engine::IO::Path::Uri& uri) override;
        IoResult<void> Remove(const Engine::IO::Path::Uri& uri, const RemoveOptions& opt) override;
        IoResult<void> Move(const Engine::IO::Path::Uri& from, const Engine::IO::Path::Uri& to) override;
        IoResult<void> Copy(const Engine::IO::Path::Uri& from, const Engine::IO::Path::Uri& to) override;
        IoResult<std::vector<DirectoryEntry>> List(const Engine::IO::Path::Uri& uri, const ListOptions& opt) override;
        IoResult<std::unique_ptr<DirectoryIterator>> Iterate(const Engine::IO::Path::Uri& uri,
                                                             const ListOptions& opt) override;
        IoResult<std::unique_ptr<IFileWatcher>> CreateWatcher() override;
    };

    /// The file URI of a native path in UTF-8, for this backend. A relative path is made absolute against the
    /// current directory first (Os::Files::MakeAbsolute), as the OS would resolve it.
    IoResult<Engine::IO::Path::Uri> NativeFileUri(std::string_view utf8Path);

    /// Whether a file written at the native path a person named lands in root, or is root (docs/architecture/
    /// io.md, "User files"). The candidate's parent is resolved by the OS (Os::Files::MakeAbsolute, then
    /// Os::Files::ResolvedPath: links before '..') and its last name kept as written, since an atomic save
    /// renames over that name, a link or not; root is resolved whole. A trailing separator names the directory
    /// before it. Names are compared with Os::Files::SamePathComponent. A path the OS cannot resolve (no
    /// permission) is compared with '.' and '..' removed as written. It decides containment only and changes no
    /// path; it is not a sandbox: a link named inside root counts as inside, wherever it leads. An empty path is
    /// false; a path that is not UTF-8, or that has a Win32 namespace prefix ("//?/", "//./"), is InvalidPath.
    IoResult<bool> IsNativePathWithin(std::string_view utf8Candidate, std::string_view utf8Root);

} // namespace Engine::IO::FS
