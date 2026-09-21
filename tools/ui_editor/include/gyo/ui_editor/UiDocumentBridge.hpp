#pragma once

#include "gyo/ui_editor/Diagnostic.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Gyo::Tools::UiEditor {

class ReadOnlyAssetCatalog;

struct AssetReference final {
    std::string jsonPath;
    std::string assetId;
    std::string expectedType;
};

struct DocumentParseResult final {
    std::optional<nlohmann::json> document;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] explicit operator bool() const noexcept {
        return document.has_value();
    }
};

// This is the editor's only document seam to the authoritative GYO::Ui codec.
class UiDocumentBridge final {
public:
    [[nodiscard]] static nlohmann::json CreateDefault();
    [[nodiscard]] static DocumentParseResult Parse(std::string_view text);
    [[nodiscard]] static std::string SerializeCanonical(
        const nlohmann::json& document);
    [[nodiscard]] static std::vector<Diagnostic> Validate(
        const nlohmann::json& document);
    [[nodiscard]] static std::vector<AssetReference> AssetReferences(
        const nlohmann::json& document);
};

[[nodiscard]] std::vector<Diagnostic> ValidateCatalogReferences(
    std::span<const AssetReference> references,
    const ReadOnlyAssetCatalog* catalog);

[[nodiscard]] bool HasErrors(std::span<const Diagnostic> diagnostics) noexcept;

} // namespace Gyo::Tools::UiEditor
