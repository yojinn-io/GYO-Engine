#include "engine/io/fs/NativeFileSystem.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "engine/io/path/Uri.hpp"
#include "engine/os/Files.hpp"

namespace Engine::IO::FS {

    namespace {

        // The IoErrorCode for an engine/os failure; operation-specific codes stand for the rest.
        IoErrorCode CodeFrom(const Os::OsErrorCode code, const IoErrorCode otherwise) noexcept {
            switch (code) {
            case Os::OsErrorCode::NotFound:        return IoErrorCode::NotFound;
            case Os::OsErrorCode::AccessDenied:    return IoErrorCode::PermissionDenied;
            case Os::OsErrorCode::InvalidPath:     return IoErrorCode::InvalidPath;
            case Os::OsErrorCode::InvalidEncoding: return IoErrorCode::InvalidPath;
            case Os::OsErrorCode::Unsupported:     return IoErrorCode::NotSupported;
            case Os::OsErrorCode::TooLarge:
            case Os::OsErrorCode::IoFailed:        return otherwise;
            }
            return otherwise;
        }

        // Rule 5 of docs/architecture/error-handling.md: the message stays, the cause goes into the detail.
        IoError FromOs(const Os::OsError& error, const std::string_view nativePath, const IoErrorCode otherwise) {
            return IoError::Make(CodeFrom(error.code, otherwise), error.message,
                                 Base::CauseDetail(error, nativePath));
        }

        IoError Unsupported(const char* operation) {
            return IoError::Make(IoErrorCode::NotSupported, std::string("NativeFS: ") + operation + " is not supported");
        }

        bool WantsWrite(const Stream::FileOpenMode mode) noexcept {
            using Stream::FileOpenMode;
            return Stream::CanWrite(mode) || Stream::Has(mode, FileOpenMode::Append) ||
                   Stream::Has(mode, FileOpenMode::Truncate) || Stream::Has(mode, FileOpenMode::CreateIfMissing);
        }

        // The native path of a file URI, absolute on the platform this runs on: "/x" names the root of the
        // current drive on Windows and "C:/x" a relative path on POSIX, so neither is a file there.
        IoResult<std::string> NativePath(const Path::Uri& uri) {
            auto native = Path::NativePathFromFileUri(uri);
            if (!native) return Base::Err(std::move(native.error()));
            if (!Os::Files::IsAbsolutePath(native.value())) {
                return Base::Err(IoError::Make(IoErrorCode::InvalidPath,
                                               "NativeFS: the path is not absolute on this platform", native.value()));
            }
            return native;
        }

        FileType TypeFrom(const Os::Files::FileKind kind) noexcept {
            switch (kind) {
            case Os::Files::FileKind::None:      return FileType::None;
            case Os::Files::FileKind::Regular:   return FileType::Regular;
            case Os::Files::FileKind::Directory: return FileType::Directory;
            case Os::Files::FileKind::Other:     return FileType::Other;
            }
            return FileType::Other;
        }

        // A read stream over an engine/os file.
        class NativeReadStream final : public Stream::IStream {
        public:
            NativeReadStream(Os::Files::FileReader file, std::string nativePath)
                : file_(std::move(file)), path_(std::move(nativePath)) {}

            Stream::StreamCaps Caps() const noexcept override {
                return {.readable = true, .writable = false, .seekable = true};
            }
            bool IsOpen() const noexcept override { return file_.has_value(); }
            bool IsEof() const noexcept override { return eof_; }

            Base::Result<std::size_t, IoError> Read(void* dst, const std::size_t bytes) override {
                if (!file_) return Base::Err(Closed(IoErrorCode::ReadFailed));
                auto got = file_->Read(std::span(static_cast<std::byte*>(dst), bytes));
                if (!got) return Base::Err(FromOs(got.error(), path_, IoErrorCode::ReadFailed));
                if (got.value() == 0 && bytes != 0) eof_ = true;
                return got.value();
            }

            Base::Result<std::size_t, IoError> Write(const void*, std::size_t) override {
                return Base::Err(IoError::Make(IoErrorCode::NotSupported, "NativeFS: a read stream is not writable"));
            }

            Base::Result<std::uint64_t, IoError> Tell() const override {
                if (!file_) return Base::Err(Closed(IoErrorCode::SeekFailed));
                auto position = file_->Tell();
                if (!position) return Base::Err(FromOs(position.error(), path_, IoErrorCode::SeekFailed));
                return position.value();
            }

            // Like the other read-only streams (MemoryStream, SpanStream), a target outside [0, Size()] fails.
            Base::Result<std::uint64_t, IoError> Seek(const std::int64_t offset,
                                                      const Stream::SeekWhence whence) override {
                if (!file_) return Base::Err(Closed(IoErrorCode::SeekFailed));
                auto size = Size();
                if (!size) return Base::Err(std::move(size.error()));
                std::int64_t base = 0;
                if (whence == Stream::SeekWhence::Current) {
                    auto position = Tell();
                    if (!position) return Base::Err(std::move(position.error()));
                    base = static_cast<std::int64_t>(position.value());
                } else if (whence == Stream::SeekWhence::End) {
                    base = static_cast<std::int64_t>(size.value());
                }
                // base >= 0, so -base never overflows (offset may be INT64_MIN).
                if ((offset < 0 && offset < -base) ||
                    (offset > 0 && base > (std::numeric_limits<std::int64_t>::max)() - offset) ||
                    static_cast<std::uint64_t>(base + offset) > size.value()) {
                    return Base::Err(IoError::Make(IoErrorCode::SeekFailed, "NativeFS: seek outside the file", path_));
                }
                const auto target = static_cast<std::uint64_t>(base + offset);
                if (auto moved = file_->Seek(target); !moved) {
                    return Base::Err(FromOs(moved.error(), path_, IoErrorCode::SeekFailed));
                }
                eof_ = false;
                return target;
            }

            Base::Result<std::uint64_t, IoError> Size() const override {
                if (!file_) return Base::Err(Closed(IoErrorCode::ReadFailed));
                auto size = file_->Size();
                if (!size) return Base::Err(FromOs(size.error(), path_, IoErrorCode::ReadFailed));
                return size.value();
            }

            Base::Result<void, IoError> Flush() override {
                return Base::Err(IoError::Make(IoErrorCode::NotSupported, "NativeFS: a read stream has nothing to flush"));
            }

            Base::Result<void, IoError> Close() override {
                file_.reset();
                return {};
            }

        private:
            IoError Closed(const IoErrorCode code) const {
                return IoError::Make(code, "NativeFS: the stream is closed", path_);
            }

            std::optional<Os::Files::FileReader> file_;
            std::string path_;
            bool eof_ = false;
        };

        // A write stream over an engine/os file: writes only, no position, no buffering (each Write
        // reaches the OS, so Flush has nothing left to do).
        class NativeWriteStream final : public Stream::IStream {
        public:
            NativeWriteStream(Os::Files::FileWriter file, std::string nativePath)
                : file_(std::move(file)), path_(std::move(nativePath)) {}

            Stream::StreamCaps Caps() const noexcept override {
                return {.readable = false, .writable = true, .seekable = false};
            }
            bool IsOpen() const noexcept override { return file_.has_value(); }
            bool IsEof() const noexcept override { return false; }

            Base::Result<std::size_t, IoError> Read(void*, std::size_t) override {
                return Base::Err(IoError::Make(IoErrorCode::NotSupported, "NativeFS: a write stream is not readable"));
            }

            Base::Result<std::size_t, IoError> Write(const void* src, const std::size_t bytes) override {
                if (!file_) return Base::Err(Closed());
                auto wrote = file_->Write(std::span(static_cast<const std::byte*>(src), bytes));
                if (!wrote) return Base::Err(FromOs(wrote.error(), path_, IoErrorCode::WriteFailed));
                return bytes;
            }

            Base::Result<std::uint64_t, IoError> Tell() const override { return Base::Err(NotSeekable()); }
            Base::Result<std::uint64_t, IoError> Seek(std::int64_t, Stream::SeekWhence) override {
                return Base::Err(NotSeekable());
            }
            Base::Result<std::uint64_t, IoError> Size() const override { return Base::Err(NotSeekable()); }

            Base::Result<void, IoError> Flush() override {
                if (!file_) return Base::Err(Closed());
                return {};
            }

            // Reports a failure the destructor could not.
            Base::Result<void, IoError> Close() override {
                if (!file_) return {};
                auto closed = file_->Close();
                file_.reset();
                if (!closed) return Base::Err(FromOs(closed.error(), path_, IoErrorCode::WriteFailed));
                return {};
            }

        private:
            IoError Closed() const { return IoError::Make(IoErrorCode::WriteFailed, "NativeFS: the stream is closed", path_); }
            IoError NotSeekable() const {
                return IoError::Make(IoErrorCode::NotSupported, "NativeFS: a write stream has no position", path_);
            }

            std::optional<Os::Files::FileWriter> file_;
            std::string path_;
        };

        // Writing creates a missing file and either empties the file or appends to it; any other write
        // mode (read and write, overwriting in place, an existing file only) is NotSupported.
        IoResult<std::unique_ptr<Stream::IStream>> OpenForWriting(const std::string& native,
                                                                  const Stream::FileOpenMode mode) {
            using Stream::FileOpenMode;
            const bool append = Stream::Has(mode, FileOpenMode::Append);
            if (Stream::CanRead(mode) || !Stream::Has(mode, FileOpenMode::CreateIfMissing) ||
                (!append && !Stream::Has(mode, FileOpenMode::Truncate))) {
                return Base::Err(Unsupported("this write mode"));
            }
            // Text asks for the platform's line endings ("\r\n" on Windows), as a text-mode C++ stream writes.
            const auto endings = Stream::Has(mode, FileOpenMode::Text) ? Os::Files::LineEndings::Native
                                                                       : Os::Files::LineEndings::AsWritten;
            auto file = Os::Files::FileWriter::Open(
                native, append ? Os::Files::WriteMode::Append : Os::Files::WriteMode::Truncate, endings);
            if (!file) return Base::Err(FromOs(file.error(), native, IoErrorCode::OpenFailed));
            return std::unique_ptr<Stream::IStream>(std::make_unique<NativeWriteStream>(std::move(file.value()), native));
        }

    } // namespace

    IoResult<Path::Uri> NativeFileUri(const std::string_view utf8Path) {
        std::string absolute(utf8Path);
        if (!Os::Files::IsAbsolutePath(absolute)) {
            auto made = Os::Files::MakeAbsolute(absolute);
            if (!made) return Base::Err(FromOs(made.error(), absolute, IoErrorCode::InvalidPath));
            absolute = std::move(made.value());
        }
        return Path::FileUriFromNativePath(absolute);
    }

    IoResult<std::unique_ptr<Stream::IStream>> NativeFileSystem::Open(const Path::Uri& uri,
                                                                      const Stream::FileOpenMode mode) {
        if (!Stream::IsValid(mode)) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath, "NativeFS: invalid FileOpenMode"));
        }
        auto native = NativePath(uri);
        if (!native) return Base::Err(std::move(native.error()));
        if (WantsWrite(mode)) return OpenForWriting(native.value(), mode);
        auto file = Os::Files::FileReader::Open(native.value());
        if (!file) return Base::Err(FromOs(file.error(), native.value(), IoErrorCode::OpenFailed));
        return std::unique_ptr<Stream::IStream>(
            std::make_unique<NativeReadStream>(std::move(file.value()), native.value()));
    }

    IoResult<bool> NativeFileSystem::Exists(const Path::Uri& uri) {
        auto native = NativePath(uri);
        if (!native) return Base::Err(std::move(native.error()));
        auto exists = Os::Files::Exists(native.value());
        if (!exists) return Base::Err(FromOs(exists.error(), native.value(), IoErrorCode::ReadFailed));
        return exists.value();
    }

    IoResult<FileInfo> NativeFileSystem::Stat(const Path::Uri& uri) {
        auto native = NativePath(uri);
        if (!native) return Base::Err(std::move(native.error()));
        auto status = Os::Files::Stat(native.value());
        if (!status) return Base::Err(FromOs(status.error(), native.value(), IoErrorCode::ReadFailed));
        FileInfo info;
        info.type = TypeFrom(status.value().kind);
        info.sizeBytes = status.value().sizeBytes;
        info.mtimeNs = status.value().modifiedSinceUnixEpoch.count();
        info.backend = "file";
        return info;
    }

    IoResult<std::string> NativeFileSystem::ToNativePathString(const Path::Uri& uri) {
        return NativePath(uri);
    }

    FileSystemCapabilities NativeFileSystem::Capabilities() const noexcept {
        // Set every field: most defaults are true, and a VFS must not take this backend for writable.
        FileSystemCapabilities caps;
        caps.canOpenRead = true;
        caps.canOpenWrite = true;
        caps.canList = false;
        caps.canIterate = false;
        caps.canStat = true;
        caps.canCreateDirectories = true;
        caps.canRemove = true;
        caps.canRemoveRecursive = false;
        caps.canMove = true;
        caps.canCopy = false;
        caps.supportsSymlink = false;
        caps.supportsPermissions = false;
        caps.supportsHiddenFlag = false;
        caps.caseSensitivePaths = Os::Files::DefaultCaseSensitivePaths();
        caps.supportsToNativePath = true;
        caps.supportsMtime = true;
        caps.supportsCtime = false;
        caps.supportsAtime = false;
        caps.supportsWatch = false;
        caps.supportsRecursiveWatch = false;
        return caps;
    }

    IoResult<void> NativeFileSystem::CreateDirectories(const Path::Uri& uri) {
        auto native = NativePath(uri);
        if (!native) return Base::Err(std::move(native.error()));
        auto created = Os::Files::CreateDirectories(native.value());
        if (!created) return Base::Err(FromOs(created.error(), native.value(), IoErrorCode::WriteFailed));
        return {};
    }

    IoResult<void> NativeFileSystem::Remove(const Path::Uri& uri, const RemoveOptions& opt) {
        // Files only: a directory, with or without its contents, stays (canRemoveRecursive is false).
        if (opt.recursive) return Base::Err(Unsupported("Remove with recursive"));
        auto native = NativePath(uri);
        if (!native) return Base::Err(std::move(native.error()));
        auto removed = Os::Files::RemoveFile(native.value());
        if (!removed) return Base::Err(FromOs(removed.error(), native.value(), IoErrorCode::WriteFailed));
        return {};
    }

    IoResult<void> NativeFileSystem::Move(const Path::Uri& from, const Path::Uri& to) {
        auto source = NativePath(from);
        if (!source) return Base::Err(std::move(source.error()));
        auto target = NativePath(to);
        if (!target) return Base::Err(std::move(target.error()));
        auto moved = Os::Files::RenameReplacing(source.value(), target.value());
        if (!moved) return Base::Err(FromOs(moved.error(), target.value(), IoErrorCode::WriteFailed));
        return {};
    }

    IoResult<void> NativeFileSystem::Copy(const Path::Uri&, const Path::Uri&) {
        return Base::Err(Unsupported("Copy"));
    }

    IoResult<std::vector<DirectoryEntry>> NativeFileSystem::List(const Path::Uri&, const ListOptions&) {
        return Base::Err(Unsupported("List"));
    }

    IoResult<std::unique_ptr<DirectoryIterator>> NativeFileSystem::Iterate(const Path::Uri&, const ListOptions&) {
        return Base::Err(Unsupported("Iterate"));
    }

    IoResult<std::unique_ptr<IFileWatcher>> NativeFileSystem::CreateWatcher() {
        return Base::Err(Unsupported("CreateWatcher"));
    }

} // namespace Engine::IO::FS
