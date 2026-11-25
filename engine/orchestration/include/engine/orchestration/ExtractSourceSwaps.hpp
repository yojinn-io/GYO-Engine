#pragma once

// The notice that a World's extract source was replaced (O-8, O-12; declaration
// docs/architecture/target/v2/sdl-extract-replace-declaration.zh-Hant.md, section 3.2). The orchestration layer
// registers it in every World with an extractFrom and adds one to count each time ReplaceWorld puts a new source
// under that World (the reader stays; its Registry reads the new source, Kernel DI-KS 38). The new source's
// entities may reuse the old one's ids, so whatever the reader keeps across frames keyed by source entities is
// stale: the System owning it reads Res<ExtractSourceSwaps>::Changed() and drops it whole before using it (every
// System using it looks, or the owner clears it in the earliest schedule that uses it). The orchestration layer
// only tells; it knows nothing of such tables. Written between two Runs, as WorldGeneration.

#include "engine/kernel/Hash.hpp"

#include <cstdint>

namespace GYO::Engine::Orchestration {

struct ExtractSourceSwaps {
    // How many times this World's source was replaced while this World lived (0 when it was built).
    std::uint64_t count = 0;
};

inline void HashValue(Kernel::Hasher& h, const ExtractSourceSwaps& value) {
    HashValue(h, value.count);
}

} // namespace GYO::Engine::Orchestration
