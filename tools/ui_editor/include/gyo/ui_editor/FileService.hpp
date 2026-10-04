#pragma once

#include "gyo/ui_editor/EditorError.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace Gyo::Tools::UiEditor {

struct FileStamp final {
    bool exists{};
    std::uintmax_t size{};
    std::filesystem::file_time_type writeTime{};
    std::uint64_t contentHash{};

    friend bool operator==(const FileStamp&, const FileStamp&) noexcept = default;
};

[[nodiscard]] Result<std::string, FileError> ReadTextFile(
    const std::filesystem::path& path);

// A path that does not exist is a success with exists == false.
[[nodiscard]] Result<FileStamp, FileError> ProbeFileStamp(
    const std::filesystem::path& path);

[[nodiscard]] Result<void, FileError> WriteTextFileAtomically(
    const std::filesystem::path& path,
    std::string_view text);

// The absolute, lexically normal form of path. Unlike std::filesystem::absolute
// it never throws: if the current directory is unavailable the path is kept as
// given, and the file operation that follows reports the failure.
[[nodiscard]] std::filesystem::path AbsolutePath(const std::filesystem::path& path);

[[nodiscard]] bool IsPathWithin(
    const std::filesystem::path& candidate,
    const std::filesystem::path& root);

} // namespace Gyo::Tools::UiEditor
