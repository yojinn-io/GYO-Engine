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
#include <vector>

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
// A writer shares its file as the C runtime's streams do (_SH_DENYNO): others may read and write, not delete.
constexpr DWORD kShareWriter = FILE_SHARE_READ | FILE_SHARE_WRITE;

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

// A UNC path, \\server\share...: two '\' and then a name, not the Win32 file or
// device namespace form a caller may give (\\?\ or \\.\ at the start, or all of
// the path as \\? or \\.).
bool IsUncPath(const std::wstring_view path) noexcept {
    if (path.size() < 3 || path[0] != L'\\' || path[1] != L'\\' || path[2] == L'\\') return false;
    return !((path[2] == L'?' || path[2] == L'.') && (path.size() == 3 || path[3] == L'\\'));
}

// The shortest limit Win32 puts on a full path (CreateDirectoryW: MAX_PATH less
// room for an 8.3 name). A full path this long or longer takes the long-path form.
constexpr std::size_t kLongPathFrom = MAX_PATH - 12;

// The form of a path handed to a file API (CreateFileW, GetFileAttributesExW).
// Only here may it differ from ToWide, so the long-path form never reaches a
// result such as MakeAbsolute's or an error (Design Invariant 5).
//
// The long-path form (os-subsystem PLAN section 8, P1): a path whose full path is kLongPathFrom
// characters or longer goes to the API as \\?\ plus that full path (for UNC,
// \\?\UNC\ plus the full path after its "\\"), so Win32 does not refuse it for
// its length. The full path comes from GetFullPathNameW, which already applied
// Win32's normalization (dot segments, trailing dots and spaces), so the
// prefix, which turns that normalization off, changes nothing for such a path.
// A shorter path, any other path that starts with \\ (a caller's namespace
// form, \\?\ or \\.\, kept as it is) and a full path that is neither on a
// drive nor UNC (a device such as \\.\NUL) go to the API exactly as before, as
// does a path GetFullPathNameW cannot make full.
Base::Result<std::wstring, OsError> OsCallPath(const std::string_view utf8Path) {
    auto wide = ToWide(utf8Path);
    if (!wide) return wide;
    if (wide.value().starts_with(L"\\\\") && !IsUncPath(wide.value())) return wide;
    // When no full path can be made, the API gets the path as before and reports its own error.
    const auto full = FullWidePath(wide.value(), utf8Path);
    if (!full || full.value().size() < kLongPathFrom) return wide;
    if (IsUncPath(full.value())) return std::wstring(L"\\\\?\\UNC\\") + full.value().substr(2);
    if (!IsDriveAbsolute(full.value())) return wide;
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

struct FileWriter::Impl {
    HANDLE handle = INVALID_HANDLE_VALUE;
    std::string path;
    LineEndings endings;

    Impl(const HANDLE file, std::string utf8Path, const LineEndings lineEndings)
        : handle(file), path(std::move(utf8Path)), endings(lineEndings) {}
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    ~Impl() {
        if (handle != INVALID_HANDLE_VALUE) ::CloseHandle(handle);
    }
};

FileWriter::FileWriter(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
FileWriter::FileWriter(FileWriter&& other) noexcept = default;
FileWriter& FileWriter::operator=(FileWriter&& other) noexcept = default;
FileWriter::~FileWriter() = default;

Base::Result<FileWriter, OsError> FileWriter::Open(const std::string_view utf8Path, const WriteMode mode,
                                                   const LineEndings endings) {
    if (auto checked = Detail::CheckPath(utf8Path); !checked) return Base::Err(std::move(checked.error()));
    auto path = OsCallPath(utf8Path);
    if (!path) return Base::Err(std::move(path.error()));
    // As the C runtime opens a stream for "w" (CREATE_ALWAYS) or "a"; an append-only handle
    // puts every write at the end of the file.
    const DWORD access = mode == WriteMode::Append ? (FILE_APPEND_DATA | SYNCHRONIZE) : GENERIC_WRITE;
    const DWORD creation = mode == WriteMode::Append ? OPEN_ALWAYS : CREATE_ALWAYS;
    const HANDLE file = ::CreateFileW(path.value().c_str(), access, kShareWriter, nullptr, creation,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD error = ::GetLastError();
        // Opening a directory is refused with ERROR_ACCESS_DENIED; report it as POSIX does.
        if (error == ERROR_ACCESS_DENIED && IsDirectory(path.value())) {
            return Base::Err(MakeOsError(OsErrorCode::InvalidPath, "is a directory",
                                         {NativeErrorDomain::Win32, static_cast<std::int64_t>(error)},
                                         utf8Path.size()));
        }
        return Base::Err(FromWin32(error, "cannot open file for writing", utf8Path));
    }
    return FileWriter(std::make_unique<Impl>(file, std::string(utf8Path), endings));
}

namespace {

Base::Result<void, OsError> WriteAll(const HANDLE file, const std::byte* data, std::size_t size,
                                     const std::string_view path) {
    while (size > 0) {
        const DWORD chunk = static_cast<DWORD>((std::min)(size, static_cast<std::size_t>(kMaximumRead)));
        DWORD wrote = 0;
        if (!::WriteFile(file, data, chunk, &wrote, nullptr)) {
            const DWORD error = ::GetLastError();
            return Base::Err(FromWin32(error, "cannot write file", path));
        }
        data += wrote;
        size -= wrote;
    }
    return {};
}

} // namespace

Base::Result<void, OsError> FileWriter::Write(const std::span<const std::byte> bytes) {
    GYO_ASSERT(impl_ != nullptr);
    if (impl_->endings == LineEndings::AsWritten) return WriteAll(impl_->handle, bytes.data(), bytes.size(), impl_->path);
    // As the C runtime's text mode writes: every '\n' becomes "\r\n", whatever precedes it.
    std::vector<std::byte> translated;
    translated.reserve(bytes.size() + bytes.size() / 16 + 1);
    for (const std::byte byte : bytes) {
        if (byte == std::byte{'\n'}) translated.push_back(std::byte{'\r'});
        translated.push_back(byte);
    }
    return WriteAll(impl_->handle, translated.data(), translated.size(), impl_->path);
}

Base::Result<void, OsError> FileWriter::Close() {
    GYO_ASSERT(impl_ != nullptr);
    const HANDLE file = std::exchange(impl_->handle, INVALID_HANDLE_VALUE);
    const std::string path = std::move(impl_->path);
    impl_.reset();
    if (!::CloseHandle(file)) {
        const DWORD error = ::GetLastError();
        return Base::Err(FromWin32(error, "cannot close file", path));
    }
    return {};
}

namespace {

bool IsDriveLetter(const char c) noexcept {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

// The position after the next separator at or after from, or the size.
std::size_t AfterSeparator(const std::string_view path, std::size_t from) noexcept {
    while (from < path.size() && !IsSlash(path[from])) ++from;
    return from < path.size() ? from + 1 : path.size();
}

// The length of the root a path starts with, which CreateDirectories never
// makes: "C:/" or "C:" (drive), "/" (the current drive's root),
// "//server/share/" (UNC), "//?/C:/" or "//?/UNC/server/share/" (long-path
// forms) and "//./device/". 0 for a relative path. Either separator counts.
std::size_t RootLength(const std::string_view path) noexcept {
    if (path.size() >= 2 && IsDriveLetter(path[0]) && path[1] == ':') {
        return path.size() >= 3 && IsSlash(path[2]) ? 3 : 2;
    }
    if (path.empty() || !IsSlash(path[0])) return 0;
    if (path.size() < 2 || !IsSlash(path[1])) return 1;
    std::size_t at = 2;
    if (path.size() >= 4 && (path[2] == '?' || path[2] == '.') && IsSlash(path[3])) {
        at = 4;
        if (path.size() >= at + 2 && IsDriveLetter(path[at]) && path[at + 1] == ':') {
            return path.size() >= at + 3 && IsSlash(path[at + 2]) ? at + 3 : at + 2;
        }
        if (path[2] == '.' || path.size() < at + 4 || path.substr(at, 3) != "UNC" || !IsSlash(path[at + 3])) {
            return AfterSeparator(path, at);
        }
        at += 4;
    }
    // server, then share.
    return AfterSeparator(path, AfterSeparator(path, at));
}

} // namespace

Base::Result<void, OsError> CreateDirectories(const std::string_view utf8Path) {
    if (auto checked = Detail::CheckPath(utf8Path); !checked) return Base::Err(std::move(checked.error()));
    // Every prefix after the root that ends at a separator or at the end, from
    // the outermost: each either is a directory already or is made one, as
    // std::filesystem::create_directories does.
    for (std::size_t end = RootLength(utf8Path) + 1; end <= utf8Path.size(); ++end) {
        if (end != utf8Path.size() && !IsSlash(utf8Path[end])) continue;
        const std::string_view prefix = utf8Path.substr(0, end);
        if (IsSlash(prefix.back())) continue;
        auto native = OsCallPath(prefix);
        if (!native) return Base::Err(std::move(native.error()));
        if (IsDirectory(native.value())) continue;
        if (::CreateDirectoryW(native.value().c_str(), nullptr)) continue;
        const DWORD error = ::GetLastError();
        if (error == ERROR_ALREADY_EXISTS) {
            // Another process may have made it meanwhile.
            if (IsDirectory(native.value())) continue;
            return Base::Err(MakeOsError(OsErrorCode::InvalidPath, "exists and is not a directory",
                                         {NativeErrorDomain::Win32, static_cast<std::int64_t>(error)},
                                         utf8Path.size()));
        }
        return Base::Err(FromWin32(error, "cannot create directory", utf8Path));
    }
    return {};
}

Base::Result<void, OsError> RenameReplacing(const std::string_view utf8From, const std::string_view utf8To) {
    if (auto checked = Detail::CheckPath(utf8From); !checked) return Base::Err(std::move(checked.error()));
    if (auto checked = Detail::CheckPath(utf8To); !checked) return Base::Err(std::move(checked.error()));
    auto from = OsCallPath(utf8From);
    if (!from) return Base::Err(std::move(from.error()));
    auto to = OsCallPath(utf8To);
    if (!to) return Base::Err(std::move(to.error()));
    // The flags of the atomic save this primitive replaced (os-subsystem D56; Design Invariant 9).
    if (!::MoveFileExW(from.value().c_str(), to.value().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD error = ::GetLastError();
        return Base::Err(FromWin32(error, "cannot rename the file over its target", utf8To));
    }
    return {};
}

Base::Result<void, OsError> RemoveFile(const std::string_view utf8Path) {
    if (auto checked = Detail::CheckPath(utf8Path); !checked) return Base::Err(std::move(checked.error()));
    auto path = OsCallPath(utf8Path);
    if (!path) return Base::Err(std::move(path.error()));
    if (!::DeleteFileW(path.value().c_str())) {
        const DWORD error = ::GetLastError();
        if (IsDirectory(path.value())) {
            return Base::Err(MakeOsError(OsErrorCode::InvalidPath, "is a directory, not a file",
                                         {NativeErrorDomain::Win32, static_cast<std::int64_t>(error)},
                                         utf8Path.size()));
        }
        return Base::Err(FromWin32(error, "cannot remove file", utf8Path));
    }
    return {};
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

namespace {

// The errors that mean a component of the path does not exist (or names a file
// where a directory is needed), as the MSVC standard library's
// __std_is_file_not_found reads them (Windows 11 24H2 added the last two):
// ResolvedPath then tries a shorter prefix.
bool IsMissingComponent(const DWORD error) noexcept {
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND || error == ERROR_BAD_NETPATH ||
           error == ERROR_INVALID_NAME || error == ERROR_BAD_PATHNAME || error == ERROR_DIRECTORY ||
           error == ERROR_NETNAME_DELETED;
}

// The final path of an open file or directory (links followed, the case on
// disk), without the \\?\ or \\?\UNC\ form GetFinalPathNameByHandleW gives it.
Base::Result<std::wstring, OsError> FinalPath(const HANDLE handle, const std::string_view utf8Path) {
    constexpr DWORD kFlags = FILE_NAME_NORMALIZED | VOLUME_NAME_DOS;
    DWORD needed = ::GetFinalPathNameByHandleW(handle, nullptr, 0, kFlags);
    while (true) {
        if (needed == 0) {
            const DWORD error = ::GetLastError();
            return Base::Err(FromWin32(error, "cannot resolve the path", utf8Path));
        }
        std::wstring path(needed, L'\0');
        const DWORD written = ::GetFinalPathNameByHandleW(handle, path.data(), needed, kFlags);
        if (written == 0) {
            const DWORD error = ::GetLastError();
            return Base::Err(FromWin32(error, "cannot resolve the path", utf8Path));
        }
        if (written < needed) {
            path.resize(written);
            if (path.starts_with(L"\\\\?\\UNC\\")) return std::wstring(L"\\\\") + path.substr(8);
            if (path.starts_with(L"\\\\?\\")) return path.substr(4);
            return path;
        }
        // The name grew between the two calls; ask again with the new size.
        needed = written;
    }
}

} // namespace

Base::Result<std::string, OsError> ResolvedPath(const std::string_view utf8Path) {
    auto absolute = MakeAbsolute(utf8Path);
    if (!absolute) return absolute;
    const std::string& path = absolute.value();
    const std::size_t root = RootLength(path);
    // The longest prefix that opens, dropping one component at a time from the
    // end: only a missing component shortens it. The root itself is resolved
    // only when it is the whole path, as weakly_canonical keeps root_path() as
    // written when nothing after it exists.
    std::size_t end = path.size();
    while (true) {
        auto native = OsCallPath(std::string_view(path).substr(0, end));
        if (!native) return Base::Err(std::move(native.error()));
        // Only attributes are read, as the standard library's canonical opens
        // it. Backup semantics open a directory too; links are followed.
        const HANDLE handle = ::CreateFileW(native.value().c_str(), FILE_READ_ATTRIBUTES, kShareAll, nullptr,
                                           OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (handle != INVALID_HANDLE_VALUE) {
            auto finalPath = FinalPath(handle, utf8Path);
            ::CloseHandle(handle);
            if (!finalPath) return Base::Err(std::move(finalPath.error()));
            auto resolved = ToUtf8(finalPath.value(), utf8Path);
            if (!resolved) return resolved;
            auto composed = PathFromUtf8(resolved.value() + path.substr(end));
            if (!composed) return Base::Err(std::move(composed.error()));
            return PathToUtf8(composed.value().lexically_normal());
        }
        const DWORD error = ::GetLastError();
        if (!IsMissingComponent(error)) return Base::Err(FromWin32(error, "cannot resolve the path", utf8Path));
        const std::size_t slash = end == 0 ? std::string::npos : path.rfind('/', end - 1);
        if (slash == std::string::npos || slash < root) {
            // Nothing after the root exists: the path as MakeAbsolute gave it.
            auto lexical = PathFromUtf8(path);
            if (!lexical) return Base::Err(std::move(lexical.error()));
            return PathToUtf8(lexical.value().lexically_normal());
        }
        end = slash;
    }
}

Base::Result<bool, OsError> SamePathComponent(const std::string_view utf8Left, const std::string_view utf8Right) {
    for (const std::string_view text : {utf8Left, utf8Right}) {
        if (!Detail::IsValidUtf8(text)) {
            return Base::Err(
                Detail::RefusedArgument(OsErrorCode::InvalidEncoding, "path component is not valid UTF-8", text.size()));
        }
    }
    // ToWide refuses an empty text; two empty components are the same, an empty and a non-empty one are not.
    if (utf8Left.empty() || utf8Right.empty()) return utf8Left.empty() && utf8Right.empty();
    auto left = ToWide(utf8Left);
    if (!left) return Base::Err(std::move(left.error()));
    auto right = ToWide(utf8Right);
    if (!right) return Base::Err(std::move(right.error()));
    const int compared = ::CompareStringOrdinal(left.value().data(), static_cast<int>(left.value().size()),
                                                right.value().data(), static_cast<int>(right.value().size()), TRUE);
    if (compared == 0) {
        const DWORD error = ::GetLastError();
        return Base::Err(FromWin32(error, "cannot compare path components", utf8Left));
    }
    return compared == CSTR_EQUAL;
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

Base::Result<std::vector<std::string>, OsError> FontDirectories() {
    std::vector<std::wstring> candidates;
    const UINT needed = ::GetSystemWindowsDirectoryW(nullptr, 0);
    if (needed == 0) {
        const DWORD error = ::GetLastError();
        return Base::Err(FromWin32(error, "cannot read the Windows directory", {}));
    }
    std::wstring windows(needed, L'\0');
    const UINT written = ::GetSystemWindowsDirectoryW(windows.data(), needed);
    if (written == 0 || written >= needed) {
        const DWORD error = written == 0 ? ::GetLastError() : ERROR_INSUFFICIENT_BUFFER;
        return Base::Err(FromWin32(error, "cannot read the Windows directory", {}));
    }
    windows.resize(written);
    if (windows.back() != L'\\') windows.push_back(L'\\');
    candidates.push_back(windows + L"Fonts");
    // The user's fonts (Windows 10 1809 and later); left out when the variable is not set.
    if (const DWORD size = ::GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0); size > 0) {
        std::wstring local(size, L'\0');
        const DWORD got = ::GetEnvironmentVariableW(L"LOCALAPPDATA", local.data(), size);
        if (got > 0 && got < size) {
            local.resize(got);
            candidates.push_back(local + L"\\Microsoft\\Windows\\Fonts");
        }
    }
    std::vector<std::string> out;
    for (const auto& candidate : candidates) {
        if (!IsDirectory(candidate)) continue;
        if (auto text = ToUtf8(candidate, {})) out.push_back(std::move(text.value()));
    }
    return out;
}

bool DefaultCaseSensitivePaths() noexcept {
    return false;
}

} // namespace Engine::Os::Files
