#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/model/FbxLoader.hpp"
#include "engine/model/ModelData.hpp"

// The one adapter of ufbx (Design Invariant MD-1): the rest of the Module sees
// only this function, in its own words.

namespace GYO::Engine::Model::Detail {

// The adapter's failures are the loader's codes (NotFbx, Undecodable,
// Unsupported): FbxLoader adds the entry's id and path.
using ImportFailure = Base::Error<FbxLoaderCode>;

struct ImportOptions final {
    bool meshes = true;
    // Keys per second of the baked animations.
    double bakeRate = 60.0;
    // What ufbx may allocate, for its work and for the scene each.
    std::size_t memoryLimit = std::size_t{256} * 1024u * 1024u;
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
