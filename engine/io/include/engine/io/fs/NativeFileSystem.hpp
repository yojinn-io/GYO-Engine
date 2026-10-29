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

} // namespace Engine::IO::FS
