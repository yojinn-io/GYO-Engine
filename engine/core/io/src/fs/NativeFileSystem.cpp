#include "engine/io/fs/NativeFileSystem.hpp"

#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "engine/io/path/Uri.hpp"
#include "engine/os/Files.hpp"

namespace GYO::Engine::IO::FS {

    namespace {

        // The IOErrorCode for an engine/os failure; operation-specific codes stand for the rest.
        IOErrorCode CodeFrom(const OS::OSErrorCode code, const IOErrorCode otherwise) noexcept {
            switch (code) {
            case OS::OSErrorCode::NotFound:        return IOErrorCode::NotFound;
            case OS::OSErrorCode::AccessDenied:    return IOErrorCode::PermissionDenied;
            case OS::OSErrorCode::InvalidPath:     return IOErrorCode::InvalidPath;
            case OS::OSErrorCode::InvalidEncoding: return IOErrorCode::InvalidPath;
            case OS::OSErrorCode::Unsupported:     return IOErrorCode::NotSupported;
            case OS::OSErrorCode::TooLarge:
            case OS::OSErrorCode::IOFailed:        return otherwise;
            }
            return otherwise;
        }

        // Rule 5 of docs/architecture/error-handling.md: the message stays, the cause goes into the detail.
        IOError FromOS(const OS::OSError& error, const std::string_view nativePath, const IOErrorCode otherwise) {
            return IOError::Make(CodeFrom(error.code, otherwise), error.message,
                                 Base::CauseDetail(error, nativePath));
        }

        IOError Unsupported(const char* operation) {
            return IOError::Make(IOErrorCode::NotSupported, std::string("NativeFS: ") + operation + " is not supported");
        }

        bool WantsWrite(const Stream::FileOpenMode mode) noexcept {
            using Stream::FileOpenMode;
            return Stream::CanWrite(mode) || Stream::Has(mode, FileOpenMode::Append) ||
                   Stream::Has(mode, FileOpenMode::Truncate) || Stream::Has(mode, FileOpenMode::CreateIfMissing);
        }

        // The native path of a file URI, absolute on the platform this runs on: "/x" names the root of the
        // current drive on Windows and "C:/x" a relative path on POSIX, so neither is a file there.
        IOResult<std::string> NativePath(const Path::Uri& uri) {
            auto native = Path::NativePathFromFileUri(uri);
            if (!native) return Base::Err(std::move(native.error()));
            if (!OS::Files::IsAbsolutePath(native.value())) {
                return Base::Err(IOError::Make(IOErrorCode::InvalidPath,
                                               "NativeFS: the path is not absolute on this platform", native.value()));
            }
            return native;
        }

        FileType TypeFrom(const OS::Files::FileKind kind) noexcept {
            switch (kind) {
            case OS::Files::FileKind::None:      return FileType::None;
            case OS::Files::FileKind::Regular:   return FileType::Regular;
            case OS::Files::FileKind::Directory: return FileType::Directory;
            case OS::Files::FileKind::Other:     return FileType::Other;
            }
            return FileType::Other;
        }

        // A read stream over an engine/os file.
        class NativeReadStream final : public Stream::IStream {
        public:
            NativeReadStream(OS::Files::FileReader file, std::string nativePath)
                : file_(std::move(file)), path_(std::move(nativePath)) {}

            Stream::StreamCaps Caps() const noexcept override {
                return {.readable = true, .writable = false, .seekable = true};
            }
            bool IsOpen() const noexcept override { return file_.has_value(); }
            bool IsEof() const noexcept override { return eof_; }

            Base::Result<std::size_t, IOError> Read(void* dst, const std::size_t bytes) override {
                if (!file_) return Base::Err(Closed(IOErrorCode::ReadFailed));
                auto got = file_->Read(std::span(static_cast<std::byte*>(dst), bytes));
                if (!got) return Base::Err(FromOS(got.error(), path_, IOErrorCode::ReadFailed));
                if (got.value() == 0 && bytes != 0) eof_ = true;
                return got.value();
            }

            Base::Result<std::size_t, IOError> Write(const void*, std::size_t) override {
                return Base::Err(IOError::Make(IOErrorCode::NotSupported, "NativeFS: a read stream is not writable"));
            }

            Base::Result<std::uint64_t, IOError> Tell() const override {
                if (!file_) return Base::Err(Closed(IOErrorCode::SeekFailed));
                auto position = file_->Tell();
                if (!position) return Base::Err(FromOS(position.error(), path_, IOErrorCode::SeekFailed));
                return position.value();
            }

            // A target outside [0, Size()] fails.
            Base::Result<std::uint64_t, IOError> Seek(const std::int64_t offset,
                                                      const Stream::SeekWhence whence) override {
                if (!file_) return Base::Err(Closed(IOErrorCode::SeekFailed));
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
                    return Base::Err(IOError::Make(IOErrorCode::SeekFailed, "NativeFS: seek outside the file", path_));
                }
                const auto target = static_cast<std::uint64_t>(base + offset);
                if (auto moved = file_->Seek(target); !moved) {
                    return Base::Err(FromOS(moved.error(), path_, IOErrorCode::SeekFailed));
                }
                eof_ = false;
                return target;
            }

            Base::Result<std::uint64_t, IOError> Size() const override {
                if (!file_) return Base::Err(Closed(IOErrorCode::ReadFailed));
                auto size = file_->Size();
                if (!size) return Base::Err(FromOS(size.error(), path_, IOErrorCode::ReadFailed));
                return size.value();
            }

            Base::Result<void, IOError> Flush() override {
                return Base::Err(IOError::Make(IOErrorCode::NotSupported, "NativeFS: a read stream has nothing to flush"));
            }

            Base::Result<void, IOError> Close() override {
                file_.reset();
                return {};
            }

        private:
            IOError Closed(const IOErrorCode code) const {
                return IOError::Make(code, "NativeFS: the stream is closed", path_);
            }

            std::optional<OS::Files::FileReader> file_;
            std::string path_;
            bool eof_ = false;
        };

        // A write stream over an engine/os file: writes only, no position, no buffering (each Write
        // reaches the OS, so Flush has nothing left to do).
        class NativeWriteStream final : public Stream::IStream {
        public:
            NativeWriteStream(OS::Files::FileWriter file, std::string nativePath)
                : file_(std::move(file)), path_(std::move(nativePath)) {}

            Stream::StreamCaps Caps() const noexcept override {
                return {.readable = false, .writable = true, .seekable = false};
            }
            bool IsOpen() const noexcept override { return file_.has_value(); }
            bool IsEof() const noexcept override { return false; }

            Base::Result<std::size_t, IOError> Read(void*, std::size_t) override {
                return Base::Err(IOError::Make(IOErrorCode::NotSupported, "NativeFS: a write stream is not readable"));
            }

            Base::Result<std::size_t, IOError> Write(const void* src, const std::size_t bytes) override {
                if (!file_) return Base::Err(Closed());
                auto wrote = file_->Write(std::span(static_cast<const std::byte*>(src), bytes));
                if (!wrote) return Base::Err(FromOS(wrote.error(), path_, IOErrorCode::WriteFailed));
                return bytes;
            }

            Base::Result<std::uint64_t, IOError> Tell() const override { return Base::Err(NotSeekable()); }
            Base::Result<std::uint64_t, IOError> Seek(std::int64_t, Stream::SeekWhence) override {
                return Base::Err(NotSeekable());
            }
            Base::Result<std::uint64_t, IOError> Size() const override { return Base::Err(NotSeekable()); }

            Base::Result<void, IOError> Flush() override {
                if (!file_) return Base::Err(Closed());
                return {};
            }

            // Reports a failure the destructor could not.
            Base::Result<void, IOError> Close() override {
                if (!file_) return {};
                auto closed = file_->Close();
                file_.reset();
                if (!closed) return Base::Err(FromOS(closed.error(), path_, IOErrorCode::WriteFailed));
                return {};
            }

        private:
            IOError Closed() const { return IOError::Make(IOErrorCode::WriteFailed, "NativeFS: the stream is closed", path_); }
            IOError NotSeekable() const {
                return IOError::Make(IOErrorCode::NotSupported, "NativeFS: a write stream has no position", path_);
            }

            std::optional<OS::Files::FileWriter> file_;
            std::string path_;
        };

        // Writing creates a missing file and either empties the file or appends to it; any other write
        // mode (read and write, overwriting in place, an existing file only) is NotSupported.
        IOResult<std::unique_ptr<Stream::IStream>> OpenForWriting(const std::string& native,
                                                                  const Stream::FileOpenMode mode) {
            using Stream::FileOpenMode;
            const bool append = Stream::Has(mode, FileOpenMode::Append);
            if (Stream::CanRead(mode) || !Stream::Has(mode, FileOpenMode::CreateIfMissing) ||
                (!append && !Stream::Has(mode, FileOpenMode::Truncate))) {
                return Base::Err(Unsupported("this write mode"));
            }
            // Text asks for the platform's line endings ("\r\n" on Windows), as a text-mode C++ stream writes.
            const auto endings = Stream::Has(mode, FileOpenMode::Text) ? OS::Files::LineEndings::Native
                                                                       : OS::Files::LineEndings::AsWritten;
            auto file = OS::Files::FileWriter::Open(
                native, append ? OS::Files::WriteMode::Append : OS::Files::WriteMode::Truncate, endings);
            if (!file) return Base::Err(FromOS(file.error(), native, IOErrorCode::OpenFailed));
            return std::unique_ptr<Stream::IStream>(std::make_unique<NativeWriteStream>(std::move(file.value()), native));
        }

    } // namespace

    namespace {

        // "a/b/" names the directory "a/b": its empty last name goes; a volume's root ("/", "C:/") stays.
        std::filesystem::path WithoutTrailingSeparator(std::filesystem::path path) {
            if (!path.has_filename() && path.has_relative_path()) path = path.parent_path();
            return path;
        }

        // Absolute as the OS makes it (OS::Files::MakeAbsolute), nothing removed lexically; as given when the
        // OS cannot.
        std::filesystem::path Absolute(const std::filesystem::path& given) {
            if (auto text = OS::Files::PathToUtf8(given); text && !text.value().empty()) {
                if (auto made = OS::Files::MakeAbsolute(text.value())) {
                    if (auto path = OS::Files::PathFromUtf8(made.value())) return std::move(path.value());
                }
            }
            return given;
        }

        // Links in the existing part followed by the OS before its '..' (OS::Files::ResolvedPath); removed
        // lexically when the OS cannot resolve the path.
        std::filesystem::path Resolved(const std::filesystem::path& absolute) {
            if (auto text = OS::Files::PathToUtf8(absolute)) {
                if (auto resolved = OS::Files::ResolvedPath(text.value())) {
                    if (auto path = OS::Files::PathFromUtf8(resolved.value())) return std::move(path.value());
                }
            }
            return absolute.lexically_normal();
        }

        // Where a file written at the path lands: the parent as the OS resolves it, then the last name as
        // written (an atomic save renames over the name, a link or not).
        std::filesystem::path CandidatePath(const std::filesystem::path& given) {
            const std::filesystem::path absolute = WithoutTrailingSeparator(Absolute(given));
            const std::filesystem::path name = absolute.filename();
            if (name == "." || name == "..") return WithoutTrailingSeparator(Resolved(absolute));
            return WithoutTrailingSeparator((Resolved(absolute.parent_path()) / name).lexically_normal());
        }

        // The directory the root is on disk: resolved whole, the last name too.
        std::filesystem::path RootPath(const std::filesystem::path& given) {
            return WithoutTrailingSeparator(Resolved(WithoutTrailingSeparator(Absolute(given))));
        }

        bool SameComponent(const std::filesystem::path& left, const std::filesystem::path& right) {
            auto leftText = OS::Files::PathToUtf8(left);
            auto rightText = OS::Files::PathToUtf8(right);
            if (!leftText || !rightText) return false;
            auto same = OS::Files::SamePathComponent(leftText.value(), rightText.value());
            return same && same.value();
        }

        // The Win32 namespace prefixes the file mapping refuses too (Path::FileUriFromNativePath).
        bool HasNamespacePrefix(const std::string_view utf8) noexcept {
            return utf8.starts_with("//?/") || utf8.starts_with("//./") || utf8.starts_with("\\\\?\\") ||
                   utf8.starts_with("\\\\.\\");
        }

    } // namespace

    IOResult<bool> IsNativePathWithin(const std::string_view utf8Candidate, const std::string_view utf8Root) {
        if (utf8Candidate.empty() || utf8Root.empty()) return false;
        for (const std::string_view text : {utf8Candidate, utf8Root}) {
            if (HasNamespacePrefix(text)) {
                return Base::Err(IOError::Make(IOErrorCode::InvalidPath, "NativeFS: a namespace prefix is not a path",
                                               std::string(text)));
            }
        }
        auto candidatePath = OS::Files::PathFromUtf8(utf8Candidate);
        if (!candidatePath) return Base::Err(FromOS(candidatePath.error(), {}, IOErrorCode::InvalidPath));
        auto rootPath = OS::Files::PathFromUtf8(utf8Root);
        if (!rootPath) return Base::Err(FromOS(rootPath.error(), {}, IOErrorCode::InvalidPath));
        const std::filesystem::path candidate = CandidatePath(candidatePath.value());
        const std::filesystem::path root = RootPath(rootPath.value());
        auto candidatePart = candidate.begin();
        for (auto rootPart = root.begin(); rootPart != root.end(); ++rootPart, ++candidatePart) {
            if (candidatePart == candidate.end() || !SameComponent(*candidatePart, *rootPart)) return false;
        }
        return true;
    }

    IOResult<Path::Uri> NativeFileUri(const std::string_view utf8Path) {
        std::string absolute(utf8Path);
        if (!OS::Files::IsAbsolutePath(absolute)) {
            auto made = OS::Files::MakeAbsolute(absolute);
            if (!made) return Base::Err(FromOS(made.error(), absolute, IOErrorCode::InvalidPath));
            absolute = std::move(made.value());
        }
        return Path::FileUriFromNativePath(absolute);
    }

    IOResult<std::unique_ptr<Stream::IStream>> NativeFileSystem::Open(const Path::Uri& uri,
                                                                      const Stream::FileOpenMode mode) {
        if (!Stream::IsValid(mode)) {
            return Base::Err(IOError::Make(IOErrorCode::InvalidPath, "NativeFS: invalid FileOpenMode"));
        }
        auto native = NativePath(uri);
        if (!native) return Base::Err(std::move(native.error()));
        if (WantsWrite(mode)) return OpenForWriting(native.value(), mode);
        auto file = OS::Files::FileReader::Open(native.value());
        if (!file) return Base::Err(FromOS(file.error(), native.value(), IOErrorCode::OpenFailed));
        return std::unique_ptr<Stream::IStream>(
            std::make_unique<NativeReadStream>(std::move(file.value()), native.value()));
    }

    IOResult<bool> NativeFileSystem::Exists(const Path::Uri& uri) {
        auto native = NativePath(uri);
        if (!native) return Base::Err(std::move(native.error()));
        auto exists = OS::Files::Exists(native.value());
        if (!exists) return Base::Err(FromOS(exists.error(), native.value(), IOErrorCode::ReadFailed));
        return exists.value();
    }

    IOResult<FileInfo> NativeFileSystem::Stat(const Path::Uri& uri) {
        auto native = NativePath(uri);
        if (!native) return Base::Err(std::move(native.error()));
        auto status = OS::Files::Stat(native.value());
        if (!status) return Base::Err(FromOS(status.error(), native.value(), IOErrorCode::ReadFailed));
        FileInfo info;
        info.type = TypeFrom(status.value().kind);
        info.sizeBytes = status.value().sizeBytes;
        info.mtimeNs = status.value().modifiedSinceUnixEpoch.count();
        info.backend = "file";
        return info;
    }

    IOResult<std::string> NativeFileSystem::ToNativePathString(const Path::Uri& uri) {
        return NativePath(uri);
    }


    IOResult<void> NativeFileSystem::CreateDirectories(const Path::Uri& uri) {
        auto native = NativePath(uri);
        if (!native) return Base::Err(std::move(native.error()));
        auto created = OS::Files::CreateDirectories(native.value());
        if (!created) return Base::Err(FromOS(created.error(), native.value(), IOErrorCode::WriteFailed));
        return {};
    }

    IOResult<void> NativeFileSystem::Remove(const Path::Uri& uri) {
        // Files only: a directory, with or without its contents, stays.
        auto native = NativePath(uri);
        if (!native) return Base::Err(std::move(native.error()));
        auto removed = OS::Files::RemoveFile(native.value());
        if (!removed) return Base::Err(FromOS(removed.error(), native.value(), IOErrorCode::WriteFailed));
        return {};
    }

    IOResult<void> NativeFileSystem::Move(const Path::Uri& from, const Path::Uri& to) {
        auto source = NativePath(from);
        if (!source) return Base::Err(std::move(source.error()));
        auto target = NativePath(to);
        if (!target) return Base::Err(std::move(target.error()));
        auto moved = OS::Files::RenameReplacing(source.value(), target.value());
        if (!moved) return Base::Err(FromOS(moved.error(), target.value(), IOErrorCode::WriteFailed));
        return {};
    }

} // namespace GYO::Engine::IO::FS
