#pragma once

#include <memory>
#include <string>

#include "engine/io/IOError.hpp"
#include "engine/io/fs/FileInfo.hpp"
#include "engine/io/path/Uri.hpp"
#include "engine/io/stream/FileOpenMode.hpp"
#include "engine/io/stream/IStream.hpp"

namespace GYO::Engine::IO::FS {

    /// One file backend (native files today; a pack or memory later) behind the URIs it serves
    /// (docs/architecture/io.md).
    /// - An operation the backend cannot do returns NotSupported.
    /// - Design Invariant 6: an implementation takes concurrent calls from any thread; a stream that Open
    ///   returns is used by one thread at a time.
    class IFileSystem {
    public:
        virtual ~IFileSystem() = default;

        IFileSystem(const IFileSystem&) = delete;
        IFileSystem& operator=(const IFileSystem&) = delete;

        /// The backend's name, for messages ("NativeFS").
        virtual const char* Name() const noexcept = 0;

        /// Opens a stream on uri; what it can do follows mode and the stream's Caps().
        virtual IOResult<std::unique_ptr<Stream::IStream>> Open(const Path::Uri& uri, Stream::FileOpenMode mode) = 0;

        virtual IOResult<bool> Exists(const Path::Uri& uri) = 0;

        /// A missing entry is NotFound.
        virtual IOResult<FileInfo> Stat(const Path::Uri& uri) = 0;

        /// Creates the directory and its missing parents.
        virtual IOResult<void> CreateDirectories(const Path::Uri& uri) = 0;

        /// Removes a file.
        virtual IOResult<void> Remove(const Path::Uri& uri) = 0;

        /// Moves within this backend, replacing a file already at the target.
        virtual IOResult<void> Move(const Path::Uri& from, const Path::Uri& to) = 0;

        /// The native path string of uri, when the backend has one.
        virtual IOResult<std::string> ToNativePathString(const Path::Uri& uri) = 0;

    protected:
        IFileSystem() = default;
    };

} // namespace GYO::Engine::IO::FS
