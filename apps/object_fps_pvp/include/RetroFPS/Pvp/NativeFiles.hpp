#pragma once

#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

// The product's native files, through engine/io's native file system, which
// calls engine/os (Design Invariant 2 of docs/architecture/os.md): no fstream
// or std::filesystem operation that touches the file system appears in
// product code. Paths stay std::filesystem::path here; engine/io takes UTF-8.

namespace fps::pvp {

// Why ReadNativeFile returned nothing: the file is larger than allowed, or it
// could not be read (detail: the engine/io error).
struct NativeReadFailure final {
    bool tooLarge = false;
    std::string detail;
};

// The whole file. A file larger than maxBytes is refused before it is read.
[[nodiscard]] std::optional<std::vector<char>> ReadNativeFile(
    const std::filesystem::path& path, NativeReadFailure& failure,
    std::uint64_t maxBytes = (std::numeric_limits<std::uint64_t>::max)());

// Whether the path names anything (a dangling link does not); nullopt when it
// cannot be checked.
[[nodiscard]] std::optional<bool> NativePathExists(const std::filesystem::path& path);

// Creates the directory and its missing parents; an existing directory is
// fine. Throws std::runtime_error naming the directory and the cause.
void CreateNativeDirectories(const std::filesystem::path& directory);

// A text file opened for writing, emptied first, written through a
// std::ostream. Each '\n' becomes the platform's line ending ("\r\n" on
// Windows), as the text-mode std::ofstream it replaces wrote; other processes
// may read the file meanwhile. Bytes reach the file on flush, on Close and
// when the buffer fills.
class NativeTextFile final {
public:
    explicit NativeTextFile(const std::filesystem::path& path);
    ~NativeTextFile();
    NativeTextFile(const NativeTextFile&) = delete;
    NativeTextFile& operator=(const NativeTextFile&) = delete;

    // False when the file did not open, or a write or the close failed (as !std::ofstream).
    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] std::ostream& Stream() noexcept { return stream_; }
    // Flushes and closes the file; false when this or anything before failed.
    [[nodiscard]] bool Close();

private:
    class Buffer;
    std::unique_ptr<Buffer> buffer_;
    std::ostream stream_;
};

} // namespace fps::pvp
