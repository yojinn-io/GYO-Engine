// The file primitives on POSIX systems (macOS, Linux).
#include "engine/os/Files.hpp"

#include "../common/Paths.hpp"

#include "engine/base/Assert.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace Engine::Os::Files {

namespace {

// One read asks for at most this much: macOS refuses reads above INT_MAX bytes.
constexpr std::size_t kMaximumRead = std::size_t{1} << 30;
// The whole seconds std::chrono::nanoseconds (64-bit) can hold with room for the fraction.
constexpr std::int64_t kMaximumSeconds = (std::numeric_limits<std::int64_t>::max)() / 1'000'000'000 - 1;

OsErrorCode CodeFromErrno(const int error) noexcept {
    switch (error) {
    case ENOENT:
    case ENOTDIR:
        return OsErrorCode::NotFound;
    case EACCES:
    case EPERM:
        return OsErrorCode::AccessDenied;
    case EISDIR:
    case ENAMETOOLONG:
    case ELOOP:
    case EINVAL:
        return OsErrorCode::InvalidPath;
    case EFBIG:
    case EOVERFLOW:
        return OsErrorCode::TooLarge;
    default:
        return OsErrorCode::IoFailed;
    }
}

// The error is passed in, read from errno right after the failing call, before
// any string is built.
OsError FromErrno(const int error, std::string message, const std::string_view path) {
    return MakeOsError(CodeFromErrno(error), std::move(message), {NativeErrorDomain::Errno, error}, path.size());
}

std::chrono::nanoseconds ModifiedTime(const struct stat& status) noexcept {
#if defined(__APPLE__)
    const timespec& time = status.st_mtimespec;
#else
    const timespec& time = status.st_mtim;
#endif
    const auto seconds = static_cast<std::int64_t>(time.tv_sec);
    if (seconds > kMaximumSeconds) return (std::chrono::nanoseconds::max)();
    if (seconds < -kMaximumSeconds) return (std::chrono::nanoseconds::min)();
    return std::chrono::seconds(seconds) + std::chrono::nanoseconds(time.tv_nsec);
}

FileKind KindOf(const mode_t mode) noexcept {
    if (S_ISREG(mode)) return FileKind::Regular;
    if (S_ISDIR(mode)) return FileKind::Directory;
    return FileKind::Other;
}

} // namespace

struct FileReader::Impl {
    int descriptor = -1;
    std::string path;

    Impl(const int fd, std::string utf8Path) : descriptor(fd), path(std::move(utf8Path)) {}
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    ~Impl() {
        if (descriptor >= 0) ::close(descriptor);
    }
};

FileReader::FileReader(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
FileReader::FileReader(FileReader&& other) noexcept = default;
FileReader& FileReader::operator=(FileReader&& other) noexcept = default;
FileReader::~FileReader() = default;

Base::Result<FileReader, OsError> FileReader::Open(const std::string_view utf8Path) {
    if (auto checked = Detail::CheckPath(utf8Path); !checked) return Base::Err(std::move(checked.error()));
    std::string path(utf8Path);
    int fd = -1;
    // O_NONBLOCK so that opening a FIFO does not wait for a writer; it does not change reads of regular files.
    do {
        fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) {
        const int error = errno;
        return Base::Err(FromErrno(error, "cannot open file", utf8Path));
    }
    auto impl = std::make_unique<Impl>(fd, std::move(path));
    struct stat status {};
    if (::fstat(fd, &status) != 0) {
        const int error = errno;
        return Base::Err(FromErrno(error, "cannot read file status", utf8Path));
    }
    // A directory, FIFO or device opens on POSIX; only regular files are files here, as on Windows.
    if (!S_ISREG(status.st_mode)) {
        return Base::Err(Detail::RefusedArgument(OsErrorCode::InvalidPath, "not a regular file", utf8Path.size()));
    }
    return FileReader(std::move(impl));
}

Base::Result<std::size_t, OsError> FileReader::Read(const std::span<std::byte> into) {
    GYO_ASSERT(impl_ != nullptr);
    if (into.empty()) return std::size_t{0};
    const std::size_t asked = std::min(into.size(), kMaximumRead);
    ssize_t got = -1;
    do {
        got = ::read(impl_->descriptor, into.data(), asked);
    } while (got < 0 && errno == EINTR);
    if (got < 0) {
        const int error = errno;
        return Base::Err(FromErrno(error, "cannot read file", impl_->path));
    }
    return static_cast<std::size_t>(got);
}

Base::Result<std::uint64_t, OsError> FileReader::Size() const {
    GYO_ASSERT(impl_ != nullptr);
    struct stat status {};
    if (::fstat(impl_->descriptor, &status) != 0) {
        const int error = errno;
        return Base::Err(FromErrno(error, "cannot read file status", impl_->path));
    }
    return static_cast<std::uint64_t>(status.st_size);
}

Base::Result<void, OsError> FileReader::Seek(const std::uint64_t offset) {
    GYO_ASSERT(impl_ != nullptr);
    if (offset > static_cast<std::uint64_t>((std::numeric_limits<off_t>::max)())) {
        return Base::Err(Detail::RefusedArgument(OsErrorCode::TooLarge, "seek offset is too large", impl_->path.size()));
    }
    if (::lseek(impl_->descriptor, static_cast<off_t>(offset), SEEK_SET) < 0) {
        const int error = errno;
        return Base::Err(FromErrno(error, "cannot seek in file", impl_->path));
    }
    return {};
}

Base::Result<std::uint64_t, OsError> FileReader::Tell() const {
    GYO_ASSERT(impl_ != nullptr);
    const off_t position = ::lseek(impl_->descriptor, 0, SEEK_CUR);
    if (position < 0) {
        const int error = errno;
        return Base::Err(FromErrno(error, "cannot read file position", impl_->path));
    }
    return static_cast<std::uint64_t>(position);
}

struct FileWriter::Impl {
    int descriptor = -1;
    std::string path;

    Impl(const int fd, std::string utf8Path) : descriptor(fd), path(std::move(utf8Path)) {}
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    ~Impl() {
        if (descriptor >= 0) ::close(descriptor);
    }
};

FileWriter::FileWriter(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
FileWriter::FileWriter(FileWriter&& other) noexcept = default;
FileWriter& FileWriter::operator=(FileWriter&& other) noexcept = default;
FileWriter::~FileWriter() = default;

// '\n' is the line ending here, so Native writes the bytes as given.
Base::Result<FileWriter, OsError> FileWriter::Open(const std::string_view utf8Path, const WriteMode mode,
                                                   LineEndings /*endings*/) {
    if (auto checked = Detail::CheckPath(utf8Path); !checked) return Base::Err(std::move(checked.error()));
    std::string path(utf8Path);
    // As a C++ file stream opens it (fopen "w" or "a"): created with 0666 less the umask.
    const int flags = O_WRONLY | O_CREAT | O_CLOEXEC | (mode == WriteMode::Append ? O_APPEND : O_TRUNC);
    int fd = -1;
    do {
        fd = ::open(path.c_str(), flags, 0666);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) {
        const int error = errno;
        return Base::Err(FromErrno(error, "cannot open file for writing", utf8Path));
    }
    return FileWriter(std::make_unique<Impl>(fd, std::move(path)));
}

Base::Result<void, OsError> FileWriter::Write(const std::span<const std::byte> bytes) {
    GYO_ASSERT(impl_ != nullptr);
    std::size_t done = 0;
    while (done < bytes.size()) {
        const std::size_t chunk = (std::min)(bytes.size() - done, kMaximumRead);
        const ssize_t wrote = ::write(impl_->descriptor, bytes.data() + done, chunk);
        if (wrote < 0) {
            const int error = errno;
            if (error == EINTR) continue;
            return Base::Err(FromErrno(error, "cannot write file", impl_->path));
        }
        done += static_cast<std::size_t>(wrote);
    }
    return {};
}

Base::Result<void, OsError> FileWriter::Close() {
    GYO_ASSERT(impl_ != nullptr);
    const int fd = std::exchange(impl_->descriptor, -1);
    const std::string path = std::move(impl_->path);
    impl_.reset();
    // EINTR still closed the descriptor (Linux, macOS); retrying could close another one.
    if (::close(fd) != 0 && errno != EINTR) {
        const int error = errno;
        return Base::Err(FromErrno(error, "cannot close file", path));
    }
    return {};
}

Base::Result<void, OsError> CreateDirectories(const std::string_view utf8Path) {
    if (auto checked = Detail::CheckPath(utf8Path); !checked) return Base::Err(std::move(checked.error()));
    const std::string path(utf8Path);
    // Every prefix that ends at a '/' or at the end, from the outermost: each
    // either is a directory already or is made one.
    for (std::size_t end = 1; end <= path.size(); ++end) {
        if (end != path.size() && path[end] != '/') continue;
        const std::string prefix = path.substr(0, end);
        if (prefix.back() == '/') continue;
        if (::mkdir(prefix.c_str(), 0777) == 0) continue;
        const int error = errno;
        if (error == EEXIST) {
            struct stat status {};
            if (::stat(prefix.c_str(), &status) == 0 && S_ISDIR(status.st_mode)) continue;
            return Base::Err(MakeOsError(OsErrorCode::InvalidPath, "exists and is not a directory",
                                         {NativeErrorDomain::Errno, error}, utf8Path.size()));
        }
        return Base::Err(FromErrno(error, "cannot create directory", utf8Path));
    }
    return {};
}

Base::Result<FileStatus, OsError> Stat(const std::string_view utf8Path) {
    if (auto checked = Detail::CheckPath(utf8Path); !checked) return Base::Err(std::move(checked.error()));
    const std::string path(utf8Path);
    struct stat status {};
    if (::stat(path.c_str(), &status) != 0) {
        const int error = errno;
        return Base::Err(FromErrno(error, "cannot read file status", utf8Path));
    }
    FileStatus out;
    out.kind = KindOf(status.st_mode);
    out.sizeBytes = out.kind == FileKind::Regular ? static_cast<std::uint64_t>(status.st_size) : 0;
    out.modifiedSinceUnixEpoch = ModifiedTime(status);
    return out;
}

Base::Result<bool, OsError> Exists(const std::string_view utf8Path) {
    if (auto checked = Detail::CheckPath(utf8Path); !checked) return Base::Err(std::move(checked.error()));
    const std::string path(utf8Path);
    struct stat status {};
    if (::stat(path.c_str(), &status) == 0) return true;
    const int error = errno;
    if (error == ENOENT || error == ENOTDIR) return false;
    return Base::Err(FromErrno(error, "cannot read file status", utf8Path));
}

Base::Result<std::string, OsError> MakeAbsolute(const std::string_view utf8Path) {
    if (auto checked = Detail::CheckPath(utf8Path); !checked) return Base::Err(std::move(checked.error()));
    if (utf8Path.front() == '/') return std::string(utf8Path);
    std::vector<char> buffer(256);
    while (::getcwd(buffer.data(), buffer.size()) == nullptr) {
        const int error = errno;
        if (error != ERANGE) return Base::Err(FromErrno(error, "cannot read the current directory", utf8Path));
        buffer.resize(buffer.size() * 2);
    }
    std::string out(buffer.data());
    // The current directory is bytes; a result that is not UTF-8 would not be a UTF-8 path (D43).
    if (!Detail::IsValidUtf8(out)) {
        return Base::Err(Detail::RefusedArgument(OsErrorCode::InvalidEncoding,
                                                 "the current directory is not valid UTF-8", utf8Path.size()));
    }
    if (out.empty() || out.back() != '/') out.push_back('/');
    out.append(utf8Path);
    return out;
}

bool IsAbsolutePath(const std::string_view utf8Path) noexcept {
    return !utf8Path.empty() && utf8Path.front() == '/';
}

Base::Result<std::string, OsError> PathToUtf8(const std::filesystem::path& path) {
    // POSIX paths are bytes; they are UTF-8 only if the bytes are.
    const std::u8string text = path.generic_u8string();
    std::string out(text.begin(), text.end());
    if (!Detail::IsValidUtf8(out)) {
        return Base::Err(Detail::RefusedArgument(OsErrorCode::InvalidEncoding, "path is not valid UTF-8", out.size()));
    }
    return out;
}

bool DefaultCaseSensitivePaths() noexcept {
#if defined(__APPLE__)
    return false;
#else
    return true;
#endif
}

} // namespace Engine::Os::Files
