// The file primitives on POSIX systems (macOS, Linux).
#include "engine/os/Files.hpp"

#include "../common/Paths.hpp"

#include <algorithm>
#include <cerrno>
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

OsError FromErrno(const int error, std::string message, const std::string_view path) {
    return MakeOsError(CodeFromErrno(error), std::move(message), {NativeErrorDomain::Errno, error}, path.size());
}

std::chrono::nanoseconds ModifiedTime(const struct stat& status) noexcept {
#if defined(__APPLE__)
    const timespec& time = status.st_mtimespec;
#else
    const timespec& time = status.st_mtim;
#endif
    return std::chrono::seconds(time.tv_sec) + std::chrono::nanoseconds(time.tv_nsec);
}

FileKind KindOf(const mode_t mode) noexcept {
    if (S_ISREG(mode)) return FileKind::Regular;
    if (S_ISDIR(mode)) return FileKind::Directory;
    return FileKind::Other;
}

} // namespace

struct ReadFile::Impl {
    int descriptor = -1;
    std::string path;

    Impl(const int fd, std::string utf8Path) : descriptor(fd), path(std::move(utf8Path)) {}
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    ~Impl() {
        if (descriptor >= 0) ::close(descriptor);
    }
};

ReadFile::ReadFile(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
ReadFile::ReadFile(ReadFile&& other) noexcept = default;
ReadFile& ReadFile::operator=(ReadFile&& other) noexcept = default;
ReadFile::~ReadFile() = default;

Base::Result<ReadFile, OsError> ReadFile::Open(const std::string_view utf8Path) {
    if (auto checked = Detail::CheckPath(utf8Path); !checked) return Base::Err(std::move(checked.error()));
    std::string path(utf8Path);
    int fd = -1;
    do {
        fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) return Base::Err(FromErrno(errno, "cannot open file", utf8Path));
    auto impl = std::make_unique<Impl>(fd, std::move(path));
    struct stat status {};
    if (::fstat(fd, &status) != 0) return Base::Err(FromErrno(errno, "cannot read file status", utf8Path));
    // A directory opens on POSIX; reading it would fail later with EISDIR, so refuse it here like Windows does.
    if (S_ISDIR(status.st_mode)) return Base::Err(FromErrno(EISDIR, "not a regular file", utf8Path));
    return ReadFile(std::move(impl));
}

Base::Result<std::size_t, OsError> ReadFile::Read(const std::span<std::byte> into) {
    const std::size_t asked = std::min(into.size(), kMaximumRead);
    ssize_t got = -1;
    do {
        got = ::read(impl_->descriptor, into.data(), asked);
    } while (got < 0 && errno == EINTR);
    if (got < 0) return Base::Err(FromErrno(errno, "cannot read file", impl_->path));
    return static_cast<std::size_t>(got);
}

Base::Result<std::uint64_t, OsError> ReadFile::Size() const {
    struct stat status {};
    if (::fstat(impl_->descriptor, &status) != 0) {
        return Base::Err(FromErrno(errno, "cannot read file status", impl_->path));
    }
    return static_cast<std::uint64_t>(status.st_size);
}

Base::Result<void, OsError> ReadFile::Seek(const std::uint64_t offset) {
    if (offset > static_cast<std::uint64_t>((std::numeric_limits<off_t>::max)())) {
        return Base::Err(FromErrno(EOVERFLOW, "seek offset is too large", impl_->path));
    }
    if (::lseek(impl_->descriptor, static_cast<off_t>(offset), SEEK_SET) < 0) {
        return Base::Err(FromErrno(errno, "cannot seek in file", impl_->path));
    }
    return {};
}

Base::Result<std::uint64_t, OsError> ReadFile::Tell() const {
    const off_t position = ::lseek(impl_->descriptor, 0, SEEK_CUR);
    if (position < 0) return Base::Err(FromErrno(errno, "cannot read file position", impl_->path));
    return static_cast<std::uint64_t>(position);
}

Base::Result<FileStatus, OsError> Stat(const std::string_view utf8Path) {
    if (auto checked = Detail::CheckPath(utf8Path); !checked) return Base::Err(std::move(checked.error()));
    const std::string path(utf8Path);
    struct stat status {};
    if (::stat(path.c_str(), &status) != 0) return Base::Err(FromErrno(errno, "cannot read file status", utf8Path));
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
        if (errno != ERANGE) return Base::Err(FromErrno(errno, "cannot read the current directory", utf8Path));
        buffer.resize(buffer.size() * 2);
    }
    std::string out(buffer.data());
    if (out.empty() || out.back() != '/') out.push_back('/');
    out.append(utf8Path);
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
