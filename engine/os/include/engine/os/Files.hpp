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
// UTF-8) fails before any OS call. These are read-only for now; engine/io's
// native file system is built on them.

namespace Engine::Os::Files {

enum class FileKind : std::uint8_t { None = 0, Regular, Directory, Other };

struct FileStatus final {
    FileKind kind = FileKind::None;
    // Meaningful for regular files.
    std::uint64_t sizeBytes = 0;
    std::chrono::nanoseconds modifiedSinceUnixEpoch{};
};

// A native file open for reading. Move-only; the file is closed when the
// object is destroyed. Opening a directory is InvalidPath.
class ReadFile final {
public:
    [[nodiscard]] static Base::Result<ReadFile, OsError> Open(std::string_view utf8Path);

    ReadFile(ReadFile&& other) noexcept;
    ReadFile& operator=(ReadFile&& other) noexcept;
    ReadFile(const ReadFile&) = delete;
    ReadFile& operator=(const ReadFile&) = delete;
    ~ReadFile();

    // Reads up to into.size() bytes at the current position; 0 at the end of
    // the file. A read may return fewer bytes than asked.
    [[nodiscard]] Base::Result<std::size_t, OsError> Read(std::span<std::byte> into);
    [[nodiscard]] Base::Result<std::uint64_t, OsError> Size() const;
    // Moves the position to offset bytes from the start.
    [[nodiscard]] Base::Result<void, OsError> Seek(std::uint64_t offset);
    [[nodiscard]] Base::Result<std::uint64_t, OsError> Tell() const;

private:
    struct Impl;
    explicit ReadFile(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

// A missing path is NotFound.
[[nodiscard]] Base::Result<FileStatus, OsError> Stat(std::string_view utf8Path);
// A missing path (or a missing parent directory) is false.
[[nodiscard]] Base::Result<bool, OsError> Exists(std::string_view utf8Path);
// The path resolved against the current directory the platform's own way
// (on Windows the current directory of the drive), in '/' form.
[[nodiscard]] Base::Result<std::string, OsError> MakeAbsolute(std::string_view utf8Path);

// A std::filesystem::path as UTF-8 in '/' form, and back. Only the standard
// library is used; no ANSI code page conversion on Windows.
[[nodiscard]] std::string PathToUtf8(const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view utf8);

// The platform default (false on Windows and macOS, true elsewhere); a volume
// may differ.
[[nodiscard]] bool DefaultCaseSensitivePaths() noexcept;

} // namespace Engine::Os::Files
