#pragma once

#include "engine/base/Result.hpp"
#include "engine/os/OsError.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>

// The native file primitives of the OS layer (docs/architecture/os.md). Paths
// are UTF-8: on Windows they become UTF-16 for the wide APIs, never the ANSI
// code page. An argument the layer refuses itself (empty, a NUL, invalid
// UTF-8, a value out of range) fails before any OS call. These are read-only
// for now; engine/io's native file system is built on them.

namespace Engine::Os::Files {

enum class FileKind : std::uint8_t { None = 0, Regular, Directory, Other };

struct FileStatus final {
    FileKind kind = FileKind::None;
    // Meaningful for regular files; 0 otherwise.
    std::uint64_t sizeBytes = 0;
    // Saturates outside the range std::chrono::nanoseconds can hold; 0 when
    // the file system keeps no write time.
    std::chrono::nanoseconds modifiedSinceUnixEpoch{};
};

// A native file open for reading. Move-only; the file is closed when the
// object is destroyed. Only regular files open: a directory, a FIFO or a
// device is InvalidPath. A moved-from ReadFile may only be destroyed or
// assigned to; calling anything else on it is a Programmer Error.
class ReadFile final {
public:
    [[nodiscard]] static Base::Result<ReadFile, OsError> Open(std::string_view utf8Path);

    ReadFile(ReadFile&& other) noexcept;
    ReadFile& operator=(ReadFile&& other) noexcept;
    ReadFile(const ReadFile&) = delete;
    ReadFile& operator=(const ReadFile&) = delete;
    ~ReadFile();

    // Reads up to into.size() bytes at the current position. A read may
    // return fewer bytes than asked; 0 for a non-empty span is the end of the
    // file. An empty span returns 0 without calling the OS.
    [[nodiscard]] Base::Result<std::size_t, OsError> Read(std::span<std::byte> into);
    [[nodiscard]] Base::Result<std::uint64_t, OsError> Size() const;
    // Moves the position to offset bytes from the start; past the end is
    // allowed (a later read returns 0).
    [[nodiscard]] Base::Result<void, OsError> Seek(std::uint64_t offset);
    [[nodiscard]] Base::Result<std::uint64_t, OsError> Tell() const;

private:
    struct Impl;
    explicit ReadFile(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

// Both follow symbolic links, so they describe the file a ReadFile opens.
// A missing path is NotFound.
[[nodiscard]] Base::Result<FileStatus, OsError> Stat(std::string_view utf8Path);
// A missing path (or a missing parent directory, or a dangling link) is false.
[[nodiscard]] Base::Result<bool, OsError> Exists(std::string_view utf8Path);

// The path resolved against the current directory the platform's own way, in
// '/' form. On Windows GetFullPathNameW also removes dot segments and trailing
// dots and spaces; on POSIX the current directory is joined as it is and an
// absolute path comes back unchanged.
[[nodiscard]] Base::Result<std::string, OsError> MakeAbsolute(std::string_view utf8Path);

// Whether the path is absolute on this platform: "/x" on POSIX; "C:/x",
// "C:\x" or a "\\" or "//" prefix on Windows. Pure string rules, no OS call.
[[nodiscard]] bool IsAbsolutePath(std::string_view utf8Path) noexcept;

// A std::filesystem::path as UTF-8 in '/' form, and back, without the ANSI
// code page. A path that has no UTF-8 form (an unpaired surrogate on Windows,
// bytes that are not UTF-8 on POSIX) and text that is not UTF-8 are
// InvalidEncoding.
[[nodiscard]] Base::Result<std::string, OsError> PathToUtf8(const std::filesystem::path& path);
[[nodiscard]] Base::Result<std::filesystem::path, OsError> PathFromUtf8(std::string_view utf8);

// The platform default (false on Windows and macOS, true elsewhere); a volume
// may differ.
[[nodiscard]] bool DefaultCaseSensitivePaths() noexcept;

} // namespace Engine::Os::Files
