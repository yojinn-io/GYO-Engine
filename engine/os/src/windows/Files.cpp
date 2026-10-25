// The file primitives on Windows: UTF-8 paths become UTF-16 for the wide APIs.
#include "engine/os/Files.hpp"

#include "../common/Paths.hpp"

#include "engine/base/Assert.hpp"

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

// Every path conversion uses this code page: UTF-8, never the ANSI code page.
constexpr UINT kPathCodePage = CP_UTF8;
// One read asks for at most this much (a DWORD count).
constexpr DWORD kMaximumRead = DWORD{1} << 30;
// FILETIME counts 100 ns intervals since 1601-01-01; the Unix epoch is this many intervals later.
constexpr std::int64_t kUnixEpochInFileTime = 116444736000000000LL;
constexpr DWORD kShareAll = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

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

// The error is passed in, read with GetLastError right after the failing call,
// before any string is built.
OsError FromWin32(const DWORD error, std::string message, const std::string_view path) {
    return MakeOsError(CodeFromWin32(error), std::move(message),
                       {NativeErrorDomain::Win32, static_cast<std::int64_t>(error)}, path.size());
}

// The UTF-16 form of a checked UTF-8 path, with '\' separators.
Base::Result<std::wstring, OsError> ToWide(const std::string_view utf8Path) {
    if (utf8Path.size() > static_cast<std::size_t>(INT_MAX)) {
        return Base::Err(Detail::RefusedArgument(OsErrorCode::TooLarge, "path is too long", utf8Path.size()));
    }
    const int bytes = static_cast<int>(utf8Path.size());
    const int length = ::MultiByteToWideChar(kPathCodePage, MB_ERR_INVALID_CHARS, utf8Path.data(), bytes, nullptr, 0);
    if (length <= 0) {
        const DWORD error = ::GetLastError();
        return Base::Err(FromWin32(error, "cannot convert path to UTF-16", utf8Path));
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    if (::MultiByteToWideChar(kPathCodePage, MB_ERR_INVALID_CHARS, utf8Path.data(), bytes, wide.data(), length) !=
        length) {
        const DWORD error = ::GetLastError();
        return Base::Err(FromWin32(error, "cannot convert path to UTF-16", utf8Path));
    }
    std::replace(wide.begin(), wide.end(), L'/', L'\\');
    return wide;
}

// The full path GetFullPathNameW makes of a UTF-16 path: resolved against the
// current directory, dot segments and trailing dots and spaces removed.
Base::Result<std::wstring, OsError> FullWidePath(const std::wstring& wide, const std::string_view utf8Path) {
    DWORD needed = ::GetFullPathNameW(wide.c_str(), 0, nullptr, nullptr);
    while (true) {
        if (needed == 0) {
            const DWORD error = ::GetLastError();
            return Base::Err(FromWin32(error, "cannot make the path absolute", utf8Path));
        }
        std::wstring full(needed, L'\0');
        const DWORD written = ::GetFullPathNameW(wide.c_str(), needed, full.data(), nullptr);
        if (written == 0) {
            const DWORD error = ::GetLastError();
            return Base::Err(FromWin32(error, "cannot make the path absolute", utf8Path));
        }
        if (written < needed) {
            full.resize(written);
            return full;
        }
        // The current directory changed between the two calls; ask again with the new size.
        needed = written;
    }
}

bool IsDriveAbsolute(const std::wstring_view path) noexcept {
    return path.size() >= 3 && ((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z')) &&
           path[1] == L':' && path[2] == L'\\';
}

// The shortest limit Win32 puts on a full path (CreateDirectoryW: MAX_PATH less
// room for an 8.3 name). A full path this long or longer takes the long-path form.
constexpr std::size_t kLongPathFrom = MAX_PATH - 12;

// The form of a path handed to a file API (CreateFileW, GetFileAttributesExW).
// Only here may it differ from ToWide, so the long-path form never reaches a
// result such as MakeAbsolute's or an error (Design Invariant 5).
//
// The long-path form (os-subsystem PLAN section 8, P1): a path whose full path is kLongPathFrom
// characters or longer goes to the API as \\?\ plus that full path, so Win32
// does not refuse it for its length. The full path comes from GetFullPathNameW,
// which already applied Win32's normalization (dot segments, trailing dots and
// spaces), so the prefix, which turns that normalization off, changes nothing
// for such a path. A shorter path, a path that already starts with \\ (a
// caller's \\?\ or \\.\ form, kept as it is) and a full path that is not on
// a drive (UNC, or a device such as \\.\NUL; UNC roots are not supported,
// io.md) go to the API exactly as before, as does a path GetFullPathNameW
// cannot make full.
Base::Result<std::wstring, OsError> OsCallPath(const std::string_view utf8Path) {
    auto wide = ToWide(utf8Path);
    if (!wide) return wide;
    if (wide.value().starts_with(L"\\\\")) return wide;
    // When no full path can be made, the API gets the path as before and reports its own error.
    const auto full = FullWidePath(wide.value(), utf8Path);
    if (!full || full.value().size() < kLongPathFrom || !IsDriveAbsolute(full.value())) return wide;
    return std::wstring(L"\\\\?\\") + full.value();
}

// UTF-16 to UTF-8 in '/' form; an unpaired surrogate is InvalidEncoding, not a silent U+FFFD.
Base::Result<std::string, OsError> ToUtf8(const std::wstring_view wide, const std::string_view forErrors) {
    if (wide.empty()) return std::string{};
    if (wide.size() > static_cast<std::size_t>(INT_MAX)) {
        return Base::Err(Detail::RefusedArgument(OsErrorCode::TooLarge, "path is too long", forErrors.size()));
    }
    const int length = static_cast<int>(wide.size());
    const int bytes =
        ::WideCharToMultiByte(kPathCodePage, WC_ERR_INVALID_CHARS, wide.data(), length, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) {
        const DWORD error = ::GetLastError();
        return Base::Err(FromWin32(error, "cannot convert path to UTF-8", forErrors));
    }
    std::string utf8(static_cast<std::size_t>(bytes), '\0');
    if (::WideCharToMultiByte(kPathCodePage, WC_ERR_INVALID_CHARS, wide.data(), length, utf8.data(), bytes, nullptr,
                              nullptr) != bytes) {
        const DWORD error = ::GetLastError();
        return Base::Err(FromWin32(error, "cannot convert path to UTF-8", forErrors));
    }
    std::replace(utf8.begin(), utf8.end(), '\\', '/');
    return utf8;
}

// 0 when the file system keeps no time; saturates outside the range of nanoseconds.
std::chrono::nanoseconds SinceUnixEpoch(const FILETIME& time) noexcept {
    ULARGE_INTEGER value{};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    if (value.QuadPart == 0) return std::chrono::nanoseconds{0};
    if (value.QuadPart > static_cast<ULONGLONG>((std::numeric_limits<std::int64_t>::max)())) {
        return (std::chrono::nanoseconds::max)();
    }
    const auto intervals = static_cast<std::int64_t>(value.QuadPart) - kUnixEpochInFileTime;
    constexpr std::int64_t kLimit = (std::numeric_limits<std::int64_t>::max)() / 100;
    if (intervals > kLimit) return (std::chrono::nanoseconds::max)();
    if (intervals < -kLimit) return (std::chrono::nanoseconds::min)();
    return std::chrono::nanoseconds(intervals * 100);
}

// What Stat and Exists report about the file a path names.
struct NativeStatus final {
    DWORD attributes = 0;
    std::uint64_t sizeBytes = 0;
    FILETIME written{};
};

std::uint64_t SizeFrom(const DWORD high, const DWORD low) noexcept {
    return (static_cast<std::uint64_t>(high) << 32) | low;
}

// GetFileAttributesExW describes a symbolic link itself; for a reparse point
// the target is opened and described instead, as POSIX stat does.
Base::Result<NativeStatus, OsError> QueryStatus(const std::wstring& path, const std::string_view utf8Path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        const DWORD error = ::GetLastError();
        return Base::Err(FromWin32(error, "cannot read file status", utf8Path));
    }
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
        return NativeStatus{data.dwFileAttributes, SizeFrom(data.nFileSizeHigh, data.nFileSizeLow),
                            data.ftLastWriteTime};
    }
    const HANDLE target = ::CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, kShareAll, nullptr, OPEN_EXISTING,
                                        FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (target == INVALID_HANDLE_VALUE) {
        const DWORD error = ::GetLastError();
        return Base::Err(FromWin32(error, "cannot read file status", utf8Path));
    }
    BY_HANDLE_FILE_INFORMATION info{};
    const BOOL described = ::GetFileInformationByHandle(target, &info);
    const DWORD error = described ? ERROR_SUCCESS : ::GetLastError();
    ::CloseHandle(target);
    if (!described) return Base::Err(FromWin32(error, "cannot read file status", utf8Path));
    return NativeStatus{info.dwFileAttributes, SizeFrom(info.nFileSizeHigh, info.nFileSizeLow), info.ftLastWriteTime};
}

bool IsDirectory(const std::wstring& path) noexcept {
    const DWORD attributes = ::GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool IsSlash(const char c) noexcept {
    return c == '/' || c == '\\';
}

} // namespace

struct FileReader::Impl {
    HANDLE handle = INVALID_HANDLE_VALUE;
    std::string path;

    Impl(const HANDLE file, std::string utf8Path) : handle(file), path(std::move(utf8Path)) {}
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    ~Impl() {
        if (handle != INVALID_HANDLE_VALUE) ::CloseHandle(handle);
    }
};

FileReader::FileReader(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
FileReader::FileReader(FileReader&& other) noexcept = default;
FileReader& FileReader::operator=(FileReader&& other) noexcept = default;
FileReader::~FileReader() = default;

Base::Result<FileReader, OsError> FileReader::Open(const std::string_view utf8Path) {
    if (auto checked = Detail::CheckPath(utf8Path); !checked) return Base::Err(std::move(checked.error()));
    auto path = OsCallPath(utf8Path);
    if (!path) return Base::Err(std::move(path.error()));
    const HANDLE file = ::CreateFileW(path.value().c_str(), GENERIC_READ, kShareAll, nullptr, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD error = ::GetLastError();
        // Opening a directory is refused with ERROR_ACCESS_DENIED; report it as POSIX does.
        if (error == ERROR_ACCESS_DENIED && IsDirectory(path.value())) {
            return Base::Err(MakeOsError(OsErrorCode::InvalidPath, "not a regular file",
                                         {NativeErrorDomain::Win32, static_cast<std::int64_t>(error)},
                                         utf8Path.size()));
        }
        return Base::Err(FromWin32(error, "cannot open file", utf8Path));
    }
    auto impl = std::make_unique<Impl>(file, std::string(utf8Path));
    // A device (NUL, CON, a pipe) opens too; only disk files are files here, as on POSIX.
    if (::GetFileType(file) != FILE_TYPE_DISK) {
        return Base::Err(Detail::RefusedArgument(OsErrorCode::InvalidPath, "not a regular file", utf8Path.size()));
    }
    return FileReader(std::move(impl));
}

Base::Result<std::size_t, OsError> FileReader::Read(const std::span<std::byte> into) {
    GYO_ASSERT(impl_ != nullptr);
    if (into.empty()) return std::size_t{0};
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

Base::Result<std::uint64_t, OsError> FileReader::Size() const {
    GYO_ASSERT(impl_ != nullptr);
    LARGE_INTEGER size{};
    if (!::GetFileSizeEx(impl_->handle, &size)) {
        const DWORD error = ::GetLastError();
        return Base::Err(FromWin32(error, "cannot read file size", impl_->path));
    }
    return static_cast<std::uint64_t>(size.QuadPart);
}

Base::Result<void, OsError> FileReader::Seek(const std::uint64_t offset) {
    GYO_ASSERT(impl_ != nullptr);
    if (offset > static_cast<std::uint64_t>((std::numeric_limits<LONGLONG>::max)())) {
        return Base::Err(Detail::RefusedArgument(OsErrorCode::TooLarge, "seek offset is too large", impl_->path.size()));
    }
    LARGE_INTEGER distance{};
    distance.QuadPart = static_cast<LONGLONG>(offset);
    if (!::SetFilePointerEx(impl_->handle, distance, nullptr, FILE_BEGIN)) {
        const DWORD error = ::GetLastError();
        return Base::Err(FromWin32(error, "cannot seek in file", impl_->path));
    }
    return {};
}

Base::Result<std::uint64_t, OsError> FileReader::Tell() const {
    GYO_ASSERT(impl_ != nullptr);
    LARGE_INTEGER zero{};
    LARGE_INTEGER position{};
    if (!::SetFilePointerEx(impl_->handle, zero, &position, FILE_CURRENT)) {
        const DWORD error = ::GetLastError();
        return Base::Err(FromWin32(error, "cannot read file position", impl_->path));
    }
    return static_cast<std::uint64_t>(position.QuadPart);
}

Base::Result<FileStatus, OsError> Stat(const std::string_view utf8Path) {
    if (auto checked = Detail::CheckPath(utf8Path); !checked) return Base::Err(std::move(checked.error()));
    auto path = OsCallPath(utf8Path);
    if (!path) return Base::Err(std::move(path.error()));
    auto status = QueryStatus(path.value(), utf8Path);
    if (!status) return Base::Err(std::move(status.error()));
    FileStatus out;
    const DWORD attributes = status.value().attributes;
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) out.kind = FileKind::Directory;
    else if ((attributes & FILE_ATTRIBUTE_DEVICE) != 0) out.kind = FileKind::Other;
    else out.kind = FileKind::Regular;
    if (out.kind == FileKind::Regular) out.sizeBytes = status.value().sizeBytes;
    out.modifiedSinceUnixEpoch = SinceUnixEpoch(status.value().written);
    return out;
}

Base::Result<bool, OsError> Exists(const std::string_view utf8Path) {
    if (auto checked = Detail::CheckPath(utf8Path); !checked) return Base::Err(std::move(checked.error()));
    auto path = OsCallPath(utf8Path);
    if (!path) return Base::Err(std::move(path.error()));
    auto status = QueryStatus(path.value(), utf8Path);
    if (status) return true;
    if (status.error().code == OsErrorCode::NotFound) return false;
    return Base::Err(std::move(status.error()));
}

Base::Result<std::string, OsError> MakeAbsolute(const std::string_view utf8Path) {
    if (auto checked = Detail::CheckPath(utf8Path); !checked) return Base::Err(std::move(checked.error()));
    auto wide = ToWide(utf8Path);
    if (!wide) return Base::Err(std::move(wide.error()));
    auto full = FullWidePath(wide.value(), utf8Path);
    if (!full) return Base::Err(std::move(full.error()));
    return ToUtf8(full.value(), utf8Path);
}

bool IsAbsolutePath(const std::string_view utf8Path) noexcept {
    if (utf8Path.size() >= 2 && IsSlash(utf8Path[0]) && IsSlash(utf8Path[1])) return true;
    const bool letter = utf8Path.size() >= 3 && ((utf8Path[0] >= 'A' && utf8Path[0] <= 'Z') ||
                                                 (utf8Path[0] >= 'a' && utf8Path[0] <= 'z'));
    return letter && utf8Path[1] == ':' && IsSlash(utf8Path[2]);
}

Base::Result<std::string, OsError> PathToUtf8(const std::filesystem::path& path) {
    // From the native UTF-16 directly: the standard library would throw on an unpaired surrogate.
    return ToUtf8(path.native(), {});
}

bool DefaultCaseSensitivePaths() noexcept {
    return false;
}

} // namespace Engine::Os::Files
