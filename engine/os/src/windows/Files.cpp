// The file primitives on Windows: UTF-8 paths become UTF-16 for the wide APIs.
#include "engine/os/Files.hpp"

#include "../common/Paths.hpp"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Engine::Os::Files {

namespace {

// One read asks for at most this much (a DWORD count).
constexpr DWORD kMaximumRead = DWORD{1} << 30;
// FILETIME counts 100 ns intervals since 1601-01-01; the Unix epoch is this many intervals later.
constexpr std::int64_t kUnixEpochInFileTime = 116444736000000000LL;

OsErrorCode CodeFromWin32(const DWORD error) noexcept {
    switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
    case ERROR_BAD_NETPATH:
    case ERROR_BAD_NET_NAME:
        return OsErrorCode::NotFound;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
        return OsErrorCode::AccessDenied;
    case ERROR_INVALID_NAME:
    case ERROR_BAD_PATHNAME:
    case ERROR_FILENAME_EXCED_RANGE:
    case ERROR_DIRECTORY:
        return OsErrorCode::InvalidPath;
    case ERROR_NO_UNICODE_TRANSLATION:
        return OsErrorCode::InvalidEncoding;
    default:
        return OsErrorCode::IoFailed;
    }
}

OsError FromWin32(const DWORD error, std::string message, const std::string_view path) {
    return MakeOsError(CodeFromWin32(error), std::move(message),
                       {NativeErrorDomain::Win32, static_cast<std::int64_t>(error)}, path.size());
}

bool IsNotFound(const DWORD error) noexcept {
    return CodeFromWin32(error) == OsErrorCode::NotFound;
}

// The UTF-16 form of a checked UTF-8 path, with '\' separators.
Base::Result<std::wstring, OsError> WidePath(const std::string_view utf8Path) {
    if (utf8Path.size() > static_cast<std::size_t>(INT_MAX)) {
        return Base::Err(FromWin32(ERROR_FILENAME_EXCED_RANGE, "path is too long", utf8Path));
    }
    const int bytes = static_cast<int>(utf8Path.size());
    const int length = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8Path.data(), bytes, nullptr, 0);
    if (length <= 0) return Base::Err(FromWin32(::GetLastError(), "cannot convert path to UTF-16", utf8Path));
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8Path.data(), bytes, wide.data(), length) != length) {
        return Base::Err(FromWin32(::GetLastError(), "cannot convert path to UTF-16", utf8Path));
    }
    std::replace(wide.begin(), wide.end(), L'/', L'\\');
    return wide;
}

Base::Result<std::string, OsError> Utf8Path(const std::wstring_view wide, const std::string_view forErrors) {
    if (wide.empty()) return std::string{};
    const int length = static_cast<int>(wide.size());
    const int bytes = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), length, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) return Base::Err(FromWin32(::GetLastError(), "cannot convert path to UTF-8", forErrors));
    std::string utf8(static_cast<std::size_t>(bytes), '\0');
    if (::WideCharToMultiByte(CP_UTF8, 0, wide.data(), length, utf8.data(), bytes, nullptr, nullptr) != bytes) {
        return Base::Err(FromWin32(::GetLastError(), "cannot convert path to UTF-8", forErrors));
    }
    std::replace(utf8.begin(), utf8.end(), '\\', '/');
    return utf8;
}

std::chrono::nanoseconds SinceUnixEpoch(const FILETIME& time) noexcept {
    ULARGE_INTEGER value{};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    const auto intervals = static_cast<std::int64_t>(value.QuadPart) - kUnixEpochInFileTime;
    return std::chrono::nanoseconds(intervals * 100);
}

bool IsDirectory(const std::wstring& wide) noexcept {
    const DWORD attributes = ::GetFileAttributesW(wide.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

} // namespace

struct ReadFile::Impl {
    HANDLE handle = INVALID_HANDLE_VALUE;
    std::string path;

    Impl(const HANDLE file, std::string utf8Path) : handle(file), path(std::move(utf8Path)) {}
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    ~Impl() {
        if (handle != INVALID_HANDLE_VALUE) ::CloseHandle(handle);
    }
};

ReadFile::ReadFile(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
ReadFile::ReadFile(ReadFile&& other) noexcept = default;
ReadFile& ReadFile::operator=(ReadFile&& other) noexcept = default;
ReadFile::~ReadFile() = default;

Base::Result<ReadFile, OsError> ReadFile::Open(const std::string_view utf8Path) {
    if (auto checked = Detail::CheckPath(utf8Path); !checked) return Base::Err(std::move(checked.error()));
    auto wide = WidePath(utf8Path);
    if (!wide) return Base::Err(std::move(wide.error()));
    const HANDLE file = ::CreateFileW(wide.value().c_str(), GENERIC_READ,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD error = ::GetLastError();
        // Opening a directory is refused with ERROR_ACCESS_DENIED; report it as POSIX does.
        if (error == ERROR_ACCESS_DENIED && IsDirectory(wide.value())) {
            return Base::Err(MakeOsError(OsErrorCode::InvalidPath, "not a regular file",
                                         {NativeErrorDomain::Win32, static_cast<std::int64_t>(error)},
                                         utf8Path.size()));
        }
        return Base::Err(FromWin32(error, "cannot open file", utf8Path));
    }
    return ReadFile(std::make_unique<Impl>(file, std::string(utf8Path)));
}

Base::Result<std::size_t, OsError> ReadFile::Read(const std::span<std::byte> into) {
    const DWORD asked = static_cast<DWORD>((std::min)(into.size(), static_cast<std::size_t>(kMaximumRead)));
    DWORD got = 0;
    // The Win32 function, not this class.
    if (!::ReadFile(impl_->handle, into.data(), asked, &got, nullptr)) {
        const DWORD error = ::GetLastError();
        if (error == ERROR_HANDLE_EOF) return std::size_t{0};
        return Base::Err(FromWin32(error, "cannot read file", impl_->path));
    }
    return static_cast<std::size_t>(got);
}

Base::Result<std::uint64_t, OsError> ReadFile::Size() const {
    LARGE_INTEGER size{};
    if (!::GetFileSizeEx(impl_->handle, &size)) {
        return Base::Err(FromWin32(::GetLastError(), "cannot read file size", impl_->path));
    }
    return static_cast<std::uint64_t>(size.QuadPart);
}

Base::Result<void, OsError> ReadFile::Seek(const std::uint64_t offset) {
    if (offset > static_cast<std::uint64_t>((std::numeric_limits<LONGLONG>::max)())) {
        return Base::Err(MakeOsError(OsErrorCode::TooLarge, "seek offset is too large",
                                     {NativeErrorDomain::Win32, ERROR_NEGATIVE_SEEK}, impl_->path.size()));
    }
    LARGE_INTEGER distance{};
    distance.QuadPart = static_cast<LONGLONG>(offset);
    if (!::SetFilePointerEx(impl_->handle, distance, nullptr, FILE_BEGIN)) {
        return Base::Err(FromWin32(::GetLastError(), "cannot seek in file", impl_->path));
    }
    return {};
}

Base::Result<std::uint64_t, OsError> ReadFile::Tell() const {
    LARGE_INTEGER zero{};
    LARGE_INTEGER position{};
    if (!::SetFilePointerEx(impl_->handle, zero, &position, FILE_CURRENT)) {
        return Base::Err(FromWin32(::GetLastError(), "cannot read file position", impl_->path));
    }
    return static_cast<std::uint64_t>(position.QuadPart);
}

Base::Result<FileStatus, OsError> Stat(const std::string_view utf8Path) {
    if (auto checked = Detail::CheckPath(utf8Path); !checked) return Base::Err(std::move(checked.error()));
    auto wide = WidePath(utf8Path);
    if (!wide) return Base::Err(std::move(wide.error()));
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!::GetFileAttributesExW(wide.value().c_str(), GetFileExInfoStandard, &data)) {
        return Base::Err(FromWin32(::GetLastError(), "cannot read file status", utf8Path));
    }
    FileStatus out;
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) out.kind = FileKind::Directory;
    else if ((data.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) != 0) out.kind = FileKind::Other;
    else out.kind = FileKind::Regular;
    if (out.kind == FileKind::Regular) {
        out.sizeBytes = (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    }
    out.modifiedSinceUnixEpoch = SinceUnixEpoch(data.ftLastWriteTime);
    return out;
}

Base::Result<bool, OsError> Exists(const std::string_view utf8Path) {
    if (auto checked = Detail::CheckPath(utf8Path); !checked) return Base::Err(std::move(checked.error()));
    auto wide = WidePath(utf8Path);
    if (!wide) return Base::Err(std::move(wide.error()));
    if (::GetFileAttributesW(wide.value().c_str()) != INVALID_FILE_ATTRIBUTES) return true;
    const DWORD error = ::GetLastError();
    if (IsNotFound(error)) return false;
    return Base::Err(FromWin32(error, "cannot read file status", utf8Path));
}

Base::Result<std::string, OsError> MakeAbsolute(const std::string_view utf8Path) {
    if (auto checked = Detail::CheckPath(utf8Path); !checked) return Base::Err(std::move(checked.error()));
    auto wide = WidePath(utf8Path);
    if (!wide) return Base::Err(std::move(wide.error()));
    DWORD needed = ::GetFullPathNameW(wide.value().c_str(), 0, nullptr, nullptr);
    while (true) {
        if (needed == 0) return Base::Err(FromWin32(::GetLastError(), "cannot make the path absolute", utf8Path));
        std::wstring full(needed, L'\0');
        const DWORD written = ::GetFullPathNameW(wide.value().c_str(), needed, full.data(), nullptr);
        if (written == 0) return Base::Err(FromWin32(::GetLastError(), "cannot make the path absolute", utf8Path));
        if (written < needed) {
            full.resize(written);
            return Utf8Path(full, utf8Path);
        }
        // The current directory changed between the two calls; ask again with the new size.
        needed = written;
    }
}

bool DefaultCaseSensitivePaths() noexcept {
    return false;
}

} // namespace Engine::Os::Files
