#include "gyo/ui_editor/UiDocumentBridge.hpp"

#include "gyo/ui_editor/ReadOnlyAssetCatalog.hpp"

#include "ui/UiDocumentCodec.hpp"

#include <algorithm>
#include <ranges>
#include <string>
#include <utility>

namespace Gyo::Tools::UiEditor {
namespace {

using Json = nlohmann::json;

[[nodiscard]] Diagnostic CodecDiagnostic(const Engine::Ui::UiError& error) {
    return {
        DiagnosticSeverity::Error,
        error.jsonPointer.empty() ? "/" : error.jsonPointer,
        error.message,
    };
}

void CollectAssetReferences(
    const Json& value,
    const std::string& path,
    std::vector<AssetReference>& references) {
    if (value.is_object()) {
        for (auto member = value.begin(); member != value.end(); ++member) {
            const std::string childPath = path + "/" + member.key();
            if (member.value().is_string()) {
                std::string expectedType;
                // Element `font` values are aliases into the root fonts table,
                // not AssetIds. Only table values cross the asset boundary.
                if (path == "/fonts") {
                    expectedType = "font";
                } else if (member.key() == "texture_asset") {
                    expectedType = "texture";
                }
                if (!expectedType.empty()) {
                    references.push_back({
                        childPath,
                        member.value().get<std::string>(),
                        std::move(expectedType),
                    });
                }
            }
            CollectAssetReferences(member.value(), childPath, references);
        }
    } else if (value.is_array()) {
        for (std::size_t index = 0; index < value.size(); ++index) {
            CollectAssetReferences(
                value[index], path + "/" + std::to_string(index), references);
        }
    }
}

} // namespace

nlohmann::json UiDocumentBridge::CreateDefault() {
    return Json{
        {"schema", "gyo.ui"},
        {"version", 1},
        {"design_canvas",
         {{"size", {1280.0, 720.0}}, {"scale_mode", "fit"}}},
        {"fonts", {{"default", "font.default"}}},
        {"default_font", "default"},
        {"colors",
         {{"transparent", "#00000000"},
          {"white", "#FFFFFFFF"},
          {"panel", "#303746EE"},
          {"button", "#344054FF"},
          {"button_focused", "#3979A8FF"},
          {"button_pressed", "#245878FF"},
          {"track", "#465064FF"},
          {"accent", "#43A7D5FF"}}},
        {"actions", Json::array()},
        {"bindings", Json::array()},
        {"canvases",
         Json::array({
             {
                 {"id", "main"},
                 {"backdrop_color", "transparent"},
                 {"focus_order", Json::array()},
                 {"children", Json::array()},
             },
         })},
    };
}

DocumentParseResult UiDocumentBridge::Parse(const std::string_view text) {
    auto parsed = Engine::Ui::UiDocumentCodec::Parse(text, "ui-editor");
    if (!parsed) {
        return {std::nullopt, {CodecDiagnostic(parsed.error())}};
    }
    auto canonical = Engine::Ui::UiDocumentCodec::Serialize(parsed.value());
    if (!canonical) {
        return {std::nullopt, {CodecDiagnostic(canonical.error())}};
    }
    try {
        return {Json::parse(canonical.value()), {}};
    } catch (const Json::exception& error) {
        return {
            std::nullopt,
            {{DiagnosticSeverity::Error, "/", error.what()}},
        };
    }
}

std::string UiDocumentBridge::SerializeCanonical(const Json& document) {
    auto parsed = Engine::Ui::UiDocumentCodec::Parse(
        document.dump(), "ui-editor");
    if (parsed) {
        auto canonical = Engine::Ui::UiDocumentCodec::Serialize(parsed.value());
        if (canonical) return std::move(canonical).value();
    }
    // Invalid in-progress authoring states still need deterministic snapshots
    // for transaction cancel/undo. SaveTo validates before calling this path.
    return document.dump(2) + '\n';
}

std::vector<Diagnostic> UiDocumentBridge::Validate(const Json& document) {
    auto parsed = Engine::Ui::UiDocumentCodec::Parse(
        document.dump(), "ui-editor");
    if (parsed) return {};
    return {CodecDiagnostic(parsed.error())};
}

std::vector<AssetReference> UiDocumentBridge::AssetReferences(
    const Json& document) {
    std::vector<AssetReference> references;
    CollectAssetReferences(document, "", references);
    return references;
}

std::vector<Diagnostic> ValidateCatalogReferences(
    const std::span<const AssetReference> references,
    const ReadOnlyAssetCatalog* catalog) {
    std::vector<Diagnostic> diagnostics;
    for (const AssetReference& reference : references) {
        if (reference.assetId.empty()) {
            diagnostics.push_back({
                DiagnosticSeverity::Error,
                reference.jsonPath,
                "AssetId cannot be empty",
            });
            continue;
        }
        if (catalog == nullptr || !catalog->IsMounted()) {
            diagnostics.push_back({
                DiagnosticSeverity::Warning,
                reference.jsonPath,
                "AssetId '" + reference.assetId +
                    "' is unresolved because no catalog is mounted",
            });
            continue;
        }
        const CatalogAsset* asset = catalog->Find(reference.assetId);
        if (asset == nullptr) {
            diagnostics.push_back({
                DiagnosticSeverity::Error,
                reference.jsonPath,
                "AssetId '" + reference.assetId +
                    "' does not exist in the mounted catalog",
            });
        } else if (asset->type != reference.expectedType) {
            diagnostics.push_back({
                DiagnosticSeverity::Error,
                reference.jsonPath,
                "AssetId '" + reference.assetId + "' has type '" + asset->type +
                    "'; expected '" + reference.expectedType + "'",
            });
        }
    }
    return diagnostics;
}

bool HasErrors(const std::span<const Diagnostic> diagnostics) noexcept {
    return std::ranges::any_of(diagnostics, [](const Diagnostic& diagnostic) {
        return diagnostic.severity == DiagnosticSeverity::Error;
    });
}

} // namespace Gyo::Tools::UiEditor
