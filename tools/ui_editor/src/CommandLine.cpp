#include "gyo/ui_editor/CommandLine.hpp"

#include <utility>

namespace Gyo::Tools::UiEditor {
namespace {

[[nodiscard]] bool NeedsValue(const std::string_view option) noexcept {
    return option == "--open" || option == "--output" ||
           option == "--asset-catalog" || option == "--asset-root" ||
           option == "--validate";
}

} // namespace

CommandLineResult ParseCommandLine(
    const std::span<const std::string_view> arguments) {
    CommandLineOptions options;
    bool sawValidate = false;

    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const std::string_view argument = arguments[index];
        if (argument == "--help" || argument == "-h") {
            if (arguments.size() != 1U) {
                return {std::nullopt, "--help cannot be combined with other options"};
            }
            options.mode = EditorMode::Help;
            return {std::move(options), {}};
        }

        if (!NeedsValue(argument)) {
            return {
                std::nullopt,
                argument.starts_with('-')
                    ? "unknown option: " + std::string{argument}
                    : "positional arguments are not supported: " +
                          std::string{argument},
            };
        }
        if (index + 1U >= arguments.size() || arguments[index + 1U].empty() ||
            arguments[index + 1U].starts_with('-')) {
            return {
                std::nullopt,
                "option requires a path value: " + std::string{argument},
            };
        }

        const std::filesystem::path value{arguments[++index]};
        if (argument == "--open") {
            if (options.inputPath.has_value()) {
                return {std::nullopt, "--open may be specified only once"};
            }
            options.inputPath = value;
        } else if (argument == "--output") {
            if (options.outputPath.has_value()) {
                return {std::nullopt, "--output may be specified only once"};
            }
            options.outputPath = value;
        } else if (argument == "--asset-catalog") {
            if (options.catalogPath.has_value()) {
                return {
                    std::nullopt,
                    "--asset-catalog may be specified only once",
                };
            }
            options.catalogPath = value;
        } else if (argument == "--asset-root") {
            if (options.assetRoot.has_value()) {
                return {
                    std::nullopt,
                    "--asset-root may be specified only once",
                };
            }
            options.assetRoot = value;
        } else {
            if (sawValidate || options.inputPath.has_value()) {
                return {
                    std::nullopt,
                    "--validate cannot be combined with --open or repeated",
                };
            }
            sawValidate = true;
            options.mode = EditorMode::Validate;
            options.inputPath = value;
        }
    }

    if (sawValidate && options.outputPath.has_value()) {
        return {std::nullopt, "--validate cannot be combined with --output"};
    }
    if (options.catalogPath.has_value() && !options.assetRoot.has_value()) {
        options.assetRoot = options.catalogPath->parent_path();
        if (options.assetRoot->empty()) {
            options.assetRoot = std::filesystem::path{"."};
        }
    }

    return {std::move(options), {}};
}

std::string_view CommandLineHelp() noexcept {
    return R"(GYO UI Editor

Usage:
  gyo_ui_editor [--open <working.json>] [--output <export.json>]
                [--asset-catalog <catalog.json>] [--asset-root <directory>]
  gyo_ui_editor --validate <document.json>
                [--asset-catalog <catalog.json>] [--asset-root <directory>]
  gyo_ui_editor --help

--asset-root alone loads content.json and every declared catalog, without a
fallback. Explicit --asset-catalog selects only that catalog; --asset-root then
sets its asset directory (otherwise the catalog's parent directory is used).
Content is mounted read-only for AssetId pickers and preview. Export never
copies assets, edits a catalog, or publishes into the mounted asset root.

Exit status: 0 success/valid, 2 command-line usage, 3 file or catalog I/O,
and 4 invalid GYO UI JSON.
)";
}

} // namespace Gyo::Tools::UiEditor
