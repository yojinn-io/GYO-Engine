#pragma once

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

#include <cstdint>

// The audio editor's Runtime Error type (docs/architecture/error-handling.md).
// Codes exist only for distinctions a caller branches on or a test asserts;
// the cause from an engine module is kept in detail (Base::CauseDetail).
namespace Gyo::Tools::AudioEditor {

template <class T, class E>
using Result = Engine::Base::Result<T, E>;

enum class AudioEditorErrorCode : std::uint8_t {
    ReadFailed = 1,      // a file could not be read
    WriteFailed,         // a file could not be written or replaced
    InvalidWav,          // the source is not a RIFF/WAVE file the editor can decode
    UnsupportedFormat,   // a WAV, but a sample format or channel count the editor does not convert
    InvalidCatalog,      // the mounted catalog does not satisfy the content contract
    InvalidRegistration, // the id or path cannot be registered in that catalog
};

[[nodiscard]] constexpr const char* ToString(const AudioEditorErrorCode code) noexcept {
    switch (code) {
    case AudioEditorErrorCode::ReadFailed: return "ReadFailed";
    case AudioEditorErrorCode::WriteFailed: return "WriteFailed";
    case AudioEditorErrorCode::InvalidWav: return "InvalidWav";
    case AudioEditorErrorCode::UnsupportedFormat: return "UnsupportedFormat";
    case AudioEditorErrorCode::InvalidCatalog: return "InvalidCatalog";
    case AudioEditorErrorCode::InvalidRegistration: return "InvalidRegistration";
    }
    return "Unknown";
}

using AudioEditorError = Engine::Base::Error<AudioEditorErrorCode>;
static_assert(Engine::Base::CodedError<AudioEditorError>);

} // namespace Gyo::Tools::AudioEditor
