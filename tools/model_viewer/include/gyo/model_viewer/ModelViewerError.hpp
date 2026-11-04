#pragma once

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

#include <cstdint>

// The model viewer's Runtime Error type (docs/architecture/error-handling.md).
// Codes exist only for distinctions a caller branches on or a test asserts;
// the cause from an engine or framework module is kept in detail
// (Base::CauseDetail).
namespace Gyo::Tools::ModelViewer {

template <class T, class E>
using Result = Engine::Base::Result<T, E>;

enum class ModelViewerErrorCode : std::uint8_t {
    ReadFailed = 1,      // a file could not be read
    WriteFailed,         // a file could not be written or replaced
    InvalidCatalog,      // the content could not be mounted, or the catalog to register in is not a catalog v1
    UnknownAsset,        // no asset with that id, or one of another type
    InvalidModel,        // Engine's model loader refused the model
    InvalidEvents,       // Engine's gyo.anim_events parser or binding refused the events
    OtherModel,          // the events name another model than the one opened
    ChangedOnDisk,       // the file changed since it was opened; saving would overwrite that change
    AlreadyExists,       // a new events file would replace an existing file
    InvalidRegistration, // the id or path cannot be registered in that catalog
};

[[nodiscard]] constexpr const char* ToString(const ModelViewerErrorCode code) noexcept {
    switch (code) {
    case ModelViewerErrorCode::ReadFailed: return "ReadFailed";
    case ModelViewerErrorCode::WriteFailed: return "WriteFailed";
    case ModelViewerErrorCode::InvalidCatalog: return "InvalidCatalog";
    case ModelViewerErrorCode::UnknownAsset: return "UnknownAsset";
    case ModelViewerErrorCode::InvalidModel: return "InvalidModel";
    case ModelViewerErrorCode::InvalidEvents: return "InvalidEvents";
    case ModelViewerErrorCode::OtherModel: return "OtherModel";
    case ModelViewerErrorCode::ChangedOnDisk: return "ChangedOnDisk";
    case ModelViewerErrorCode::AlreadyExists: return "AlreadyExists";
    case ModelViewerErrorCode::InvalidRegistration: return "InvalidRegistration";
    }
    return "Unknown";
}

using ModelViewerError = Engine::Base::Error<ModelViewerErrorCode>;
static_assert(Engine::Base::CodedError<ModelViewerError>);

} // namespace Gyo::Tools::ModelViewer
