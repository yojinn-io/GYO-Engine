#pragma once

#include <cstddef>

#include "engine/kernel/World.hpp"

// Entrance ② of section 3 of docs/architecture/target/v2/logging-declaration.zh-Hant.md: the Kernel's own
// diagnostics records are not Messages; the World keeps them and the runner takes them between two Runs
// (event cursor declaration, "the World records, the runner takes"). TakeKernelDiagnostics takes the World's
// DroppedMessages and RejectedSpawn records and writes each as one Warning record of category "kernel"
// carrying the World's name.
//
// Taking empties them: one World has one taker. A runner that wants to look at them too takes them itself
// and writes them with Logging::Write. An interrupted World's records are taken too (question 2 (c),
// approved), marked world_interrupted. Nothing orders them against the line a product's main may write for
// the failure; readers order lines by mono_ns (revision 3, user 2026-11-21).

namespace GYO::Engine::WorldLogging {

// Returns how many records it took. The World is Ready or Interrupted (the Kernel asserts it).
std::size_t TakeKernelDiagnostics(Kernel::World& world);

} // namespace GYO::Engine::WorldLogging
