#pragma once

// World generation (world-gen declaration docs/architecture/target/v2/world-gen-declaration.zh-Hant.md, sections
// 5.1 and 5.2; O-14). Every World is generating from the moment it is built (startup and each replacement, before
// its prepare functions). While it generates its frame list runs as usual; the author's generation Systems sit in a
// set with the run condition WhileGenerating(), the game's logic in a set with AfterGenerating(). A System that is
// not done yet calls Hold(); the first frame nobody holds ends generation. The orchestration layer owns this
// Resource and writes it between two Runs; it knows nothing of what the Systems generate (no Loading, no save).
//
// The state lives here only: Product::Generating reads it, and a World lent to another thread takes it along.

#include "engine/kernel/Hash.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"

namespace GYO::Engine::Orchestration {

struct WorldGeneration {
    // Whether this World is generating: {true, false} when the World is built and before each generating frame,
    // {false, false} when generation ends (both written by the orchestration layer).
    [[nodiscard]] bool Generating() const noexcept { return generating; }
    // This frame is not the last generating one. No effect after generation has ended.
    void Hold() noexcept { held = generating; }

    bool generating = false;
    bool held = false;
};

// Field by field (World::Summary, DI-KS 27). How many frames generation took is not in the World, so a World
// generated in one frame and one generated in three compare equal.
inline void HashValue(Kernel::Hasher& h, const WorldGeneration& value) {
    HashValue(h, value.generating);
    HashValue(h, value.held);
}

namespace Detail {
bool IsGenerating(Kernel::Res<WorldGeneration> generation);
bool HasGenerated(Kernel::Res<WorldGeneration> generation);
} // namespace Detail

// Run conditions with fixed names ("WhileGenerating", "AfterGenerating"), for an author's set or a Module's
// condition option (e.g. WorldTimeOptions::fixedStepConditions). The names are fixed so that both ends of a
// connection have the same ones (a condition's name is in the authority Module's consistency fingerprint).
[[nodiscard]] Kernel::ConditionRef WhileGenerating();
[[nodiscard]] Kernel::ConditionRef AfterGenerating();

} // namespace GYO::Engine::Orchestration
