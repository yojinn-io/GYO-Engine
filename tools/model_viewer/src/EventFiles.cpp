#include "gyo/model_viewer/EventFiles.hpp"

#include "gyo/model_viewer/ModelContent.hpp"

#include "FileAccess.hpp"

#include "gyo/framework/json/CatalogText.hpp"

#include "engine/base/Fnv1a.hpp"

#include <span>
#include <utility>

namespace Gyo::Tools::ModelViewer {
namespace {

std::span<const std::byte> Bytes(const std::string& text) {
    return std::as_bytes(std::span(text.data(), text.size()));
}

std::string Join(std::string root, const std::string_view relative) {
    if (!root.empty() && root.back() != '/' && root.back() != '\\') root += '/';
    root += relative;
    return root;
}

// The framework's refusal as the viewer's own code.
ModelViewerError RegistrationFailed(const Framework::CatalogError& error) {
    const ModelViewerErrorCode code = error.code == Framework::CatalogErrorCode::InvalidCatalog
                                          ? ModelViewerErrorCode::InvalidCatalog
                                          : ModelViewerErrorCode::InvalidRegistration;
    return ModelViewerError::Make(code, error.message, error.detail);
}

// The stamp of what was just written. Another writer that replaced the file
// right after would hand back its stamp, and the next save would overwrite its
// change unseen: ChangedOnDisk instead. The file was written either way.
Result<Engine::IO::Helpers::FileStamp, ModelViewerError> StampOfWritten(const std::string& path,
                                                                        const std::string& text) {
    auto stamp = Probe(path);
    if (!stamp) {
        auto error = std::move(stamp).error();
        error.detail += "; the file was written";
        return Engine::Base::Err(std::move(error));
    }
    if (!stamp->exists || stamp->contentHash != Engine::Base::Fnv1a64(text)) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::ChangedOnDisk,
                                                        "the file changed right after it was written", path));
    }
    return stamp.value();
}

} // namespace

Result<OpenedEvents, ModelViewerError> OpenEvents(const std::string& path, const std::string_view modelAssetId,
                                                  const Engine::Model::ModelAsset& model) {
    auto stamp = Probe(path);
    if (!stamp) return Engine::Base::Err(std::move(stamp).error());
    auto bytes = ReadAll(path, kMaxTextBytes);
    if (!bytes) return Engine::Base::Err(std::move(bytes).error());
    const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
    // The stamp is of what is parsed: a change between the probe and the read
    // would otherwise be saved over unseen.
    if (Engine::Base::Fnv1a64(text) != stamp->contentHash) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::ChangedOnDisk,
                                                        "the file changed while it was read", path));
    }
    auto parsed = Engine::Model::ParseAnimationEvents(text);
    if (!parsed) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::InvalidEvents, "Engine refuses the events",
                                                        Engine::Base::CauseDetail(parsed.error(), path)));
    }
    if (parsed->modelAssetId != modelAssetId) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::OtherModel,
                                                        "the events name another model",
                                                        path + "; model_asset_id " + parsed->modelAssetId));
    }
    if (auto bound = Engine::Model::BindAnimationEvents(parsed.value(), model); !bound) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::InvalidEvents, "Engine refuses the events",
                                                        Engine::Base::CauseDetail(bound.error(), path)));
    }
    return OpenedEvents{path, stamp.value(), EventDocument(std::move(parsed).value())};
}

Result<Engine::IO::Helpers::FileStamp, ModelViewerError> SaveEvents(const std::string& path,
                                                                    const Engine::IO::Helpers::FileStamp& opened,
                                                                    const Engine::Model::AnimationEventSet& events,
                                                                    const Engine::Model::ModelAsset& model) {
    auto text = CheckedText(events, model);
    if (!text) return Engine::Base::Err(std::move(text).error());
    auto current = Probe(path);
    if (!current) return Engine::Base::Err(std::move(current).error());
    if (current.value() != opened) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::ChangedOnDisk,
                                                        "the file changed since it was opened", path));
    }
    if (auto written = WriteReplacing(path, Bytes(*text)); !written) return Engine::Base::Err(std::move(written).error());
    return StampOfWritten(path, *text);
}

Result<NewEventsReport, ModelViewerError> CreateEvents(const NewEventsRequest& request, const std::string_view modelAssetId,
                                                       const Engine::Model::AnimationEventSet& events,
                                                       const Engine::Model::ModelAsset& model) {
    if (events.modelAssetId != modelAssetId) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::OtherModel,
                                                        "the events name another model than the one opened",
                                                        events.modelAssetId));
    }
    auto catalogBytes = ReadAll(request.catalogPath, kMaxTextBytes);
    if (!catalogBytes) return Engine::Base::Err(std::move(catalogBytes).error());
    auto update = Framework::RegisterAsset(
        std::string_view(reinterpret_cast<const char*>(catalogBytes->data()), catalogBytes->size()),
        {.id = request.id, .type = std::string(kTextType), .path = request.path});
    if (!update) return Engine::Base::Err(RegistrationFailed(update.error()));
    if (!update->added) {
        // Registering would move an existing entry to the new file.
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::InvalidRegistration,
                                                        "the id is already registered", request.id));
    }
    NewEventsReport report{Join(request.assetRoot, request.path), {}};
    auto existing = Probe(report.outputPath);
    if (!existing) return Engine::Base::Err(std::move(existing).error());
    if (existing->exists) {
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::AlreadyExists,
                                                        "a file is already at the path", report.outputPath));
    }
    auto text = CheckedText(events, model);
    if (!text) return Engine::Base::Err(std::move(text).error());
    if (auto written = WriteReplacing(report.outputPath, Bytes(*text)); !written) {
        return Engine::Base::Err(std::move(written).error());
    }
    if (auto written = WriteReplacing(request.catalogPath, Bytes(update->text)); !written) {
        auto error = std::move(written).error();
        error.detail += "; written but not registered: " + report.outputPath;
        return Engine::Base::Err(std::move(error));
    }
    auto stamp = StampOfWritten(report.outputPath, *text);
    if (!stamp) return Engine::Base::Err(std::move(stamp).error());
    report.stamp = stamp.value();
    return report;
}

} // namespace Gyo::Tools::ModelViewer
