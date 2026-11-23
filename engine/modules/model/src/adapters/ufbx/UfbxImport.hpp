#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/model/ModelData.hpp"

// The one adapter of ufbx (Design Invariant MD-1): the rest of the Module sees
// only this function, in its own words.

namespace GYO::Engine::Model::Detail {

enum class ImportProblem : std::uint8_t {
    NotFbx = 1,
    Undecodable = 2,
    Unsupported = 3,
};

[[nodiscard]] constexpr const char* ToString(const ImportProblem problem) noexcept {
    switch (problem) {
    case ImportProblem::NotFbx: return "NotFbx";
    case ImportProblem::Undecodable: return "Undecodable";
    case ImportProblem::Unsupported: return "Unsupported";
    }
    return "Unknown";
}

using ImportFailure = Base::Error<ImportProblem>;

struct ImportOptions final {
    bool meshes = true;
    // Keys per second of the baked animations.
    double bakeRate = 60.0;
};

// Reads a whole FBX file from memory into a ModelData (model.zh-Hant.md
// section 3): the space converted to the engine's, every node in parent-first
// order, materials as data, meshes triangulated with at most four weights a
// vertex, every animation stack baked to linear keys. Opens no file, decodes
// no texture, keeps no state between calls and may run on many threads at
// once. The result is not yet checked against Validate.
[[nodiscard]] Base::Result<ModelData, ImportFailure> ImportFbx(std::span<const std::byte> bytes,
                                                               const ImportOptions& options);

} // namespace GYO::Engine::Model::Detail
