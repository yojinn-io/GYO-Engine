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
            return IoError::Make(IoErrorCode::NotSupported,
                                 std::string("NativeFS: ") + operation + " is not supported (read-only backend)");
        }

        bool IsReadOnly(const Stream::FileOpenMode mode) noexcept {
            using Stream::FileOpenMode;
            return Stream::IsValid(mode) && Stream::CanRead(mode) && !Stream::CanWrite(mode) &&
                   !Stream::Has(mode, FileOpenMode::Append) && !Stream::Has(mode, FileOpenMode::Truncate) &&
                   !Stream::Has(mode, FileOpenMode::CreateIfMissing);
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
            NativeReadStream(Os::Files::ReadFile file, std::string nativePath)
                : file_(std::move(file)), path_(std::move(nativePath)) {}

            Stream::StreamCaps Caps() const noexcept override {
                return {.readable = true, .writable = false, .seekable = true};
            }
            bool IsOpen() const noexcept override { return file_.has_value(); }
            bool IsEof() const noexcept override { return eof_; }

            Base::Result<std::size_t, IoError> Read(void* dst, const std::size_t bytes) override {
                if (!file_) return Base::Err(Closed());
                auto got = file_->Read(std::span(static_cast<std::byte*>(dst), bytes));
                if (!got) return Base::Err(FromOs(got.error(), path_, IoErrorCode::ReadFailed));
                if (got.value() == 0 && bytes != 0) eof_ = true;
                return got.value();
            }

            Base::Result<std::size_t, IoError> Write(const void*, std::size_t) override {
                return Base::Err(IoError::Make(IoErrorCode::NotSupported, "NativeFS: a read stream is not writable"));
            }

            Base::Result<std::uint64_t, IoError> Tell() const override {
                if (!file_) return Base::Err(Closed());
                auto position = file_->Tell();
                if (!position) return Base::Err(FromOs(position.error(), path_, IoErrorCode::SeekFailed));
                return position.value();
            }

            Base::Result<std::uint64_t, IoError> Seek(const std::int64_t offset,
                                                      const Stream::SeekWhence whence) override {
                if (!file_) return Base::Err(Closed());
                std::int64_t base = 0;
                if (whence != Stream::SeekWhence::Begin) {
                    auto from = whence == Stream::SeekWhence::Current ? Tell() : Size();
                    if (!from) return Base::Err(std::move(from.error()));
                    base = static_cast<std::int64_t>(from.value());
                }
                if ((offset < 0 && base < -offset) ||
                    (offset > 0 && base > (std::numeric_limits<std::int64_t>::max)() - offset)) {
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
                if (!file_) return Base::Err(Closed());
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
            IoError Closed() const {
                return IoError::Make(IoErrorCode::ReadFailed, "NativeFS: the stream is closed", path_);
            }

            std::optional<Os::Files::ReadFile> file_;
            std::string path_;
            bool eof_ = false;
        };

    } // namespace

    IoResult<std::unique_ptr<Stream::IStream>> NativeFileSystem::Open(const Path::Uri& uri,
                                                                      const Stream::FileOpenMode mode) {
        if (!IsReadOnly(mode)) return Base::Err(Unsupported("opening for writing"));
        auto native = Path::NativePathFromFileUri(uri);
        if (!native) return Base::Err(std::move(native.error()));
        auto file = Os::Files::ReadFile::Open(native.value());
        if (!file) return Base::Err(FromOs(file.error(), native.value(), IoErrorCode::OpenFailed));
        return std::unique_ptr<Stream::IStream>(
            std::make_unique<NativeReadStream>(std::move(file.value()), native.value()));
    }

    IoResult<bool> NativeFileSystem::Exists(const Path::Uri& uri) {
        auto native = Path::NativePathFromFileUri(uri);
        if (!native) return Base::Err(std::move(native.error()));
        auto exists = Os::Files::Exists(native.value());
        if (!exists) return Base::Err(FromOs(exists.error(), native.value(), IoErrorCode::ReadFailed));
        return exists.value();
    }

    IoResult<FileInfo> NativeFileSystem::Stat(const Path::Uri& uri) {
        auto native = Path::NativePathFromFileUri(uri);
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
        return Path::NativePathFromFileUri(uri);
    }

    FileSystemCapabilities NativeFileSystem::Capabilities() const noexcept {
        // Set every field: most defaults are true, and a VFS must not take this backend for writable.
        FileSystemCapabilities caps;
        caps.canOpenRead = true;
        caps.canOpenWrite = false;
        caps.canList = false;
        caps.canIterate = false;
        caps.canStat = true;
        caps.canCreateDirectories = false;
        caps.canRemove = false;
        caps.canRemoveRecursive = false;
        caps.canMove = false;
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

    IoResult<void> NativeFileSystem::CreateDirectories(const Path::Uri&) {
        return Base::Err(Unsupported("CreateDirectories"));
    }

    IoResult<void> NativeFileSystem::Remove(const Path::Uri&, const RemoveOptions&) {
        return Base::Err(Unsupported("Remove"));
    }

    IoResult<void> NativeFileSystem::Move(const Path::Uri&, const Path::Uri&) {
        return Base::Err(Unsupported("Move"));
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
