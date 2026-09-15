#pragma once

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

struct TextFileResult final {
    std::optional<std::string> text;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return text.has_value();
    }
};

struct FileOperationResult final {
    bool succeeded{};
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return succeeded; }
};

[[nodiscard]] TextFileResult ReadTextFile(
    const std::filesystem::path& path);

[[nodiscard]] std::optional<FileStamp> ProbeFileStamp(
    const std::filesystem::path& path,
    std::string& error);

[[nodiscard]] FileOperationResult WriteTextFileAtomically(
    const std::filesystem::path& path,
    std::string_view text);

[[nodiscard]] bool IsPathWithin(
    const std::filesystem::path& candidate,
    const std::filesystem::path& root);

} // namespace Gyo::Tools::UiEditor
