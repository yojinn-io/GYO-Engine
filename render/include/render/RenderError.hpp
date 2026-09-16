#pragma once

#include "engine/base/Error.hpp"

namespace Engine::Render {

enum class RenderErrorCode {
    None = 0,
    InvalidArgument,
    InvalidHandle,
    WrongThread,
    BackendUnavailable,
    ShaderCompilationFailed,
    ResourceCreationFailed,
    SubmissionFailed,
    UnsupportedOperation,
};

using RenderError = Base::Error<RenderErrorCode>;

} // namespace Engine::Render
