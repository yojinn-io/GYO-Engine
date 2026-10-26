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
// UTF-8, a value out of range) fails before any OS call. engine/io's native
// file system is built on them.

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
// device is InvalidPath. A moved-from FileReader may only be destroyed or
// assigned to; calling anything else on it is a Programmer Error.
class FileReader final {
public:
    [[nodiscard]] static Base::Result<FileReader, OsError> Open(std::string_view utf8Path);

    FileReader(FileReader&& other) noexcept;
    FileReader& operator=(FileReader&& other) noexcept;
    FileReader(const FileReader&) = delete;
    FileReader& operator=(const FileReader&) = delete;
    ~FileReader();

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
    explicit FileReader(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

// How a FileWriter opens its file; both create a missing file.
enum class WriteMode : std::uint8_t {
    Truncate = 1, // an existing file starts empty
    Append,       // every write goes to the end of the file
};

// What a FileWriter does with '\n'.
enum class LineEndings : std::uint8_t {
    AsWritten = 1, // the bytes as given, on every platform
    Native,        // each '\n' becomes the platform's line ending ("\r\n" on
                   // Windows, '\n' elsewhere), as a C++ text-mode stream writes it
};

// A native file open for writing. Move-only; the file is closed when the
// object is destroyed (Close reports a failure the destructor cannot). Other
// processes may read and write the file while it is open. Nothing is
// buffered: each Write hands its bytes to the OS (not to the disk). A
// moved-from or closed FileWriter may only be destroyed or assigned to;
// calling anything else on it is a Programmer Error.
class FileWriter final {
public:
    [[nodiscard]] static Base::Result<FileWriter, OsError> Open(std::string_view utf8Path, WriteMode mode,
                                                                LineEndings endings);

    FileWriter(FileWriter&& other) noexcept;
    FileWriter& operator=(FileWriter&& other) noexcept;
    FileWriter(const FileWriter&) = delete;
    FileWriter& operator=(const FileWriter&) = delete;
    ~FileWriter();

    // Writes all the bytes, or fails. An empty span calls no OS.
    [[nodiscard]] Base::Result<void, OsError> Write(std::span<const std::byte> bytes);
    // Closes the file; the writer is closed afterwards even when it fails.
    [[nodiscard]] Base::Result<void, OsError> Close();

private:
    struct Impl;
    explicit FileWriter(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

// Creates the directory and its missing parents, as
// std::filesystem::create_directories does; an existing directory is fine. A
// path (or a parent) that exists as anything else is InvalidPath.
[[nodiscard]] Base::Result<void, OsError> CreateDirectories(std::string_view utf8Path);

// Both follow symbolic links, so they describe the file a FileReader opens.
// A missing path is NotFound.
[[nodiscard]] Base::Result<FileStatus, OsError> Stat(std::string_view utf8Path);
// A missing path (or a missing parent directory, or a dangling link) is false.
[[nodiscard]] Base::Result<bool, OsError> Exists(std::string_view utf8Path);

// The path resolved against the current directory the platform's own way, in
// '/' form. On Windows GetFullPathNameW also removes dot segments and trailing
// dots and spaces; on POSIX the current directory is joined as it is and an
// absolute path comes back unchanged. A relative path under a current directory
// with no UTF-8 form (bytes that are not UTF-8 on POSIX) is InvalidEncoding.
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
