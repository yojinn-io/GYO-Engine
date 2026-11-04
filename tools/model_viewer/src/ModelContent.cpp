#include "gyo/model_viewer/ModelContent.hpp"

#include "gyo/framework/EventSounds.hpp"
#include "gyo/framework/json/EventSoundsText.hpp"

#include "engine/asset/loaders/SoundLoader.hpp"

#include "FileAccess.hpp"

#include "engine/asset/loading/LoadContext.hpp"
#include "engine/model/backend/ufbx/UfbxModelLoader.hpp"

#include <utility>

namespace Gyo::Tools::ModelViewer {
namespace {

// How a text names the format (animation-events.md, "Asset"). Used only to
// list a file Engine refuses, never to decide that a file is valid.
constexpr std::string_view kSchemaMention = "\"gyo.anim_events\"";

ModelViewerError Unknown(const std::string_view id, const std::string_view type) {
    return ModelViewerError::Make(ModelViewerErrorCode::UnknownAsset,
                                  "no asset of type " + std::string(type) + " with this id", std::string(id));
}

} // namespace

std::string Utf8Path(const std::filesystem::path& path) {
    const auto text = path.generic_u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

ModelContent::ModelContent(std::unique_ptr<Framework::ReadOnlyAssetCatalog> catalog) noexcept
    : catalog_(std::move(catalog)) {}

Result<ModelContent, ModelViewerError> ModelContent::Mount(const std::filesystem::path& assetRoot,
                                                           const std::optional<std::filesystem::path>& catalog) {
    auto mounted = std::make_unique<Framework::ReadOnlyAssetCatalog>();
    auto result = catalog ? mounted->Mount(*catalog, assetRoot) : mounted->MountRoot(assetRoot);
    if (!result) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::InvalidCatalog, "cannot mount the content",
                                                        Engine::Base::CauseDetail(result.error())));
    }
    return ModelContent(std::move(mounted));
}

std::vector<const Framework::CatalogAsset*> ModelContent::Models() const {
    std::vector<const Framework::CatalogAsset*> models;
    for (const auto& asset : catalog_->Assets()) {
        if (asset.type == kModelType) models.push_back(&asset);
    }
    return models;
}

const Framework::CatalogAsset* ModelContent::Find(const std::string_view id) const noexcept {
    return catalog_->Find(id);
}

Result<LoadedModel, ModelViewerError> ModelContent::LoadModel(const std::string_view id) const {
    const auto* asset = catalog_->Find(id);
    if (!asset || asset->type != kModelType) return Engine::Base::Err(Unknown(id, kModelType));
    LoadedModel loaded{std::string(id), Utf8Path(asset->resolvedPath), {}};
    auto bytes = ReadAll(loaded.path, kMaxModelBytes);
    if (!bytes) return Engine::Base::Err(std::move(bytes).error());
    Engine::Asset::Loading::LoadContext context;
    context.resolvedPath = loaded.path;
    Engine::Model::Ufbx::UfbxModelLoader loader;
    auto loadedAsset = loader.Load(Engine::Base::ConstSpan<std::byte>(bytes->data(), bytes->size()), context);
    if (!loadedAsset) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::InvalidModel, "Engine refuses the model",
                                                        Engine::Base::CauseDetail(loadedAsset.error(), loaded.path)));
    }
    loaded.model = loadedAsset.value().ShareAs<Engine::Model::ModelAsset>();
    if (!loaded.model) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::InvalidModel,
                                                        "the loader gave no model", loaded.path));
    }
    return loaded;
}

Result<std::string, ModelViewerError> ModelContent::ReadText(const std::string_view id) const {
    const auto* asset = catalog_->Find(id);
    if (!asset || asset->type != kTextType) return Engine::Base::Err(Unknown(id, kTextType));
    auto bytes = ReadAll(Utf8Path(asset->resolvedPath), kMaxTextBytes);
    if (!bytes) return Engine::Base::Err(std::move(bytes).error());
    return std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size());
}

Result<std::vector<EventSoundsCandidate>, ModelViewerError> ModelContent::EventSoundsFor(
    const std::string_view eventsAssetId) const {
    std::vector<EventSoundsCandidate> candidates;
    for (const auto& asset : catalog_->Assets()) {
        if (asset.type != kTextType) continue;
        auto text = ReadText(asset.id);
        if (!text) return Engine::Base::Err(std::move(text).error());
        if (text->find(Framework::EventSoundsSchema) == std::string::npos) continue;
        EventSoundsCandidate candidate{asset.id, Utf8Path(asset.resolvedPath), false, {}};
        auto parsed = Framework::ParseEventSounds(*text);
        if (!parsed) {
            candidate.refusal = Engine::Base::Describe(parsed.error());
        } else if (parsed->animationEventsAssetId == eventsAssetId) {
            candidate.parsed = true;
        } else {
            continue;
        }
        candidates.push_back(std::move(candidate));
    }
    return candidates;
}

Result<LoadedSound, ModelViewerError> ModelContent::LoadSound(const std::string_view id) const {
    const auto* asset = catalog_->Find(id);
    if (!asset || asset->type != kSoundType) return Engine::Base::Err(Unknown(id, kSoundType));
    const std::string path = Utf8Path(asset->resolvedPath);
    auto bytes = ReadAll(path, kMaxModelBytes);
    if (!bytes) return Engine::Base::Err(std::move(bytes).error());
    Engine::Asset::Loading::LoadContext context;
    context.resolvedPath = path;
    Engine::Asset::Loaders::SoundLoader loader;
    auto loadedAsset = loader.Load(Engine::Base::ConstSpan<std::byte>(bytes->data(), bytes->size()), context);
    if (!loadedAsset) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::InvalidSound, "Engine refuses the sound",
                                                        Engine::Base::CauseDetail(loadedAsset.error(), path)));
    }
    auto sound = loadedAsset.value().ShareAs<Engine::Asset::Loaders::SoundAsset>();
    if (!sound) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::InvalidSound, "the loader gave no sound", path));
    }
    return LoadedSound{std::string(id), sound->sampleRate, sound->channels, sound->pcm16};
}

double LoadedSound::Seconds() const noexcept {
    if (sampleRate == 0 || channels == 0) return 0.0;
    return static_cast<double>(pcm16.size() / channels) / static_cast<double>(sampleRate);
}

Result<std::vector<EventsCandidate>, ModelViewerError> ModelContent::EventsFor(const std::string_view modelId) const {
    std::vector<EventsCandidate> candidates;
    for (const auto& asset : catalog_->Assets()) {
        if (asset.type != kTextType) continue;
        auto text = ReadText(asset.id);
        if (!text) return Engine::Base::Err(std::move(text).error());
        if (text->find(kSchemaMention) == std::string::npos) continue;
        EventsCandidate candidate{asset.id, Utf8Path(asset.resolvedPath), {}, {}};
        auto parsed = Engine::Model::ParseAnimationEvents(*text);
        if (!parsed) {
            candidate.refusal = Engine::Base::Describe(parsed.error());
        } else if (parsed->modelAssetId == modelId) {
            candidate.events = std::move(parsed).value();
        } else {
            continue;
        }
        candidates.push_back(std::move(candidate));
    }
    return candidates;
}

} // namespace Gyo::Tools::ModelViewer
