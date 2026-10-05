#pragma once

#include "engine/base/Error.hpp"

namespace Engine::Render {

// Zero is not a valid code; the numeric values are not a data contract.
enum class RenderErrorCode {
    InvalidArgument = 1,
    InvalidHandle,
    WrongThread,
    BackendUnavailable,
    ShaderCompilationFailed,
    ResourceCreationFailed,
    SubmissionFailed,
    UnsupportedOperation,
};

[[nodiscard]] constexpr const char* ToString(const RenderErrorCode code) noexcept {
    switch (code) {
    case RenderErrorCode::InvalidArgument: return "InvalidArgument";
    case RenderErrorCode::InvalidHandle: return "InvalidHandle";
    case RenderErrorCode::WrongThread: return "WrongThread";
    case RenderErrorCode::BackendUnavailable: return "BackendUnavailable";
    case RenderErrorCode::ShaderCompilationFailed: return "ShaderCompilationFailed";
    case RenderErrorCode::ResourceCreationFailed: return "ResourceCreationFailed";
    case RenderErrorCode::SubmissionFailed: return "SubmissionFailed";
    case RenderErrorCode::UnsupportedOperation: return "UnsupportedOperation";
    }
    return "Unknown";
}

using RenderError = Base::Error<RenderErrorCode>;
static_assert(Base::CodedError<RenderError>);

} // namespace Engine::Render
