#pragma once

#include <cstdint>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

namespace Engine::Model {

// Failure classes of the model module. Zero is not a valid code; the numeric
// values are not a data contract.
enum class ModelErrorCode : std::uint8_t {
    // The model data itself is invalid (hierarchy, meshes, skins, clips).
    InvalidModel = 1,
    // A caller argument does not fit the model (clip, time, weight, pose).
    InvalidArgument,
    // Two models cannot share an animation under the given bindings.
    IncompatibleAnimation,
};

[[nodiscard]] constexpr const char* ToString(const ModelErrorCode code) noexcept {
    switch (code) {
    case ModelErrorCode::InvalidModel: return "InvalidModel";
    case ModelErrorCode::InvalidArgument: return "InvalidArgument";
    case ModelErrorCode::IncompatibleAnimation: return "IncompatibleAnimation";
    }
    return "Unknown";
}

using ModelError = Base::Error<ModelErrorCode>;
static_assert(Base::CodedError<ModelError>);

template <class T>
using ModelResult = Base::Result<T, ModelError>;

} // namespace Engine::Model
