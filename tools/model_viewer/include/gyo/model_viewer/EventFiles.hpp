#pragma once

#include "gyo/model_viewer/EventDocument.hpp"
#include "gyo/model_viewer/ModelViewerError.hpp"

#include "engine/io/helpers/UserFiles.hpp"
#include "engine/model/ModelAsset.hpp"

#include <string>
#include <string_view>

// Opening, saving and creating gyo.anim_events files (Design Invariants 2 and 4):
// a file is read whole, saved by atomic replace through engine/io only when
// CheckedText accepts the events and the file is still what was opened, and a
// new file is registered in one catalog with type "text".
namespace Gyo::Tools::ModelViewer {

struct OpenedEvents final {
    std::string path;
    Engine::IO::Helpers::FileStamp stamp; // what the file was when read
    EventDocument document;
};

// Reads path and has Engine parse it and bind it to model. OtherModel when the
// file names another model than modelAssetId (the module does not know ids).
[[nodiscard]] Result<OpenedEvents, ModelViewerError> OpenEvents(const std::string& path, std::string_view modelAssetId,
                                                                const Engine::Model::ModelAsset& model);

// Replaces the file at path with the events once CheckedText accepts them and
// the file is still opened (ChangedOnDisk otherwise: nothing is written).
// Returns the stamp of what was written; ChangedOnDisk when the file is
// already something else when probed right after the write. The probe and the
// write are two steps: a change in between is seen by the next save, not this one.
[[nodiscard]] Result<Engine::IO::Helpers::FileStamp, ModelViewerError> SaveEvents(
    const std::string& path, const Engine::IO::Helpers::FileStamp& opened,
    const Engine::Model::AnimationEventSet& events, const Engine::Model::ModelAsset& model);

struct NewEventsRequest final {
    std::string assetRoot;
    std::string catalogPath; // the catalog that gets the entry
    std::string id;          // the new text asset's id
    std::string path;        // inside the asset root
};

struct NewEventsReport final {
    std::string outputPath;
    Engine::IO::Helpers::FileStamp stamp;
};

// Writes a new events file and registers it (type "text"). Nothing is written
// when the events name another model than modelAssetId (OtherModel), the
// catalog refuses the entry (InvalidCatalog, InvalidRegistration), a file is
// already at the path (AlreadyExists) or CheckedText refuses the events. The
// catalog is read once at the start and replaced at the end without a stamp
// check: the user names a catalog that nothing else edits meanwhile. The file
// is written first and the catalog last; when the catalog cannot be written
// the new file stays, unregistered, and the error names it.
// A product's own definitions that refer to the file are the user's to edit.
[[nodiscard]] Result<NewEventsReport, ModelViewerError> CreateEvents(const NewEventsRequest& request,
                                                                     std::string_view modelAssetId,
                                                                     const Engine::Model::AnimationEventSet& events,
                                                                     const Engine::Model::ModelAsset& model);

} // namespace Gyo::Tools::ModelViewer
