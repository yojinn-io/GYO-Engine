#pragma once

#include "gyo/model_viewer/ModelViewerError.hpp"

#include "gyo/framework/ReadOnlyAssetCatalog.hpp"

#include "engine/model/AnimationEvents.hpp"
#include "engine/model/ModelAsset.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// A product's content as the viewer reads it: the framework's read-only
// catalog mount, models through Engine's ufbx loader, text assets through
// engine/io. Nothing here writes (Design Invariant 4).
namespace Gyo::Tools::ModelViewer {

// The catalog type of the models the viewer opens and of the events files.
inline constexpr std::string_view kModelType = "model";
inline constexpr std::string_view kTextType = "text";

struct LoadedModel final {
    std::string assetId;
    std::string path; // the resolved file, UTF-8
    std::shared_ptr<const Engine::Model::ModelAsset> model;
};

// A text asset that names a model as gyo.anim_events. Files that mention the
// schema but Engine refuses are listed too, with Engine's reason, so a broken
// file is not silently missing; whether a file is valid stays Engine's.
struct EventsCandidate final {
    std::string assetId;
    std::string path; // the resolved file, UTF-8
    std::optional<Engine::Model::AnimationEventSet> events;
    std::string refusal; // Engine's reason when events is empty
};

class ModelContent final {
public:
    // A content root (content.json and every catalog it declares) when catalog
    // is empty; otherwise that one catalog, its entries under assetRoot.
    [[nodiscard]] static Result<ModelContent, ModelViewerError> Mount(const std::filesystem::path& assetRoot,
                                                                      const std::optional<std::filesystem::path>& catalog);

    [[nodiscard]] std::vector<const Framework::CatalogAsset*> Models() const;
    [[nodiscard]] const Framework::CatalogAsset* Find(std::string_view id) const noexcept;

    // UnknownAsset for an id that is not a model; ReadFailed, InvalidModel otherwise.
    [[nodiscard]] Result<LoadedModel, ModelViewerError> LoadModel(std::string_view id) const;
    // UnknownAsset for an id that is not a text asset.
    [[nodiscard]] Result<std::string, ModelViewerError> ReadText(std::string_view id) const;
    // The text assets that are, or try to be, gyo.anim_events of modelId, in id order (the mount's order).
    [[nodiscard]] Result<std::vector<EventsCandidate>, ModelViewerError> EventsFor(std::string_view modelId) const;

    [[nodiscard]] const Framework::ReadOnlyAssetCatalog& Catalog() const noexcept { return *catalog_; }

private:
    explicit ModelContent(std::unique_ptr<Framework::ReadOnlyAssetCatalog> catalog) noexcept;

    std::unique_ptr<Framework::ReadOnlyAssetCatalog> catalog_;
};

// The UTF-8 form of a path, as the viewer prints and stores it.
[[nodiscard]] std::string Utf8Path(const std::filesystem::path& path);

} // namespace Gyo::Tools::ModelViewer
