#pragma once

#include "gyo/ui_editor/EditorError.hpp"

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace Gyo::Tools::UiEditor {

enum class EditorMode {
    Interactive,
    Validate,
    Help,
};

struct CommandLineOptions final {
    EditorMode mode{EditorMode::Interactive};
    std::optional<std::filesystem::path> inputPath;
    std::optional<std::filesystem::path> outputPath;
    std::optional<std::filesystem::path> catalogPath;
    std::optional<std::filesystem::path> assetRoot;
};

[[nodiscard]] Result<CommandLineOptions, CommandLineError> ParseCommandLine(
    std::span<const std::string_view> arguments);

[[nodiscard]] std::string_view CommandLineHelp() noexcept;

} // namespace Gyo::Tools::UiEditor
