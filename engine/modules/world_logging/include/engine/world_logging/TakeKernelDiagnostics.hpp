#pragma once

#include <cstddef>

#include "engine/kernel/World.hpp"
#include "engine/orchestration/Setup.hpp"

// Entrance ② of section 3 of docs/architecture/target/v2/logging-declaration.zh-Hant.md: the Kernel's own
// diagnostics records are not Messages; the World keeps them and they are taken outside any Run (event cursor
// declaration, "the World records, the runner takes"). TakeKernelDiagnostics takes the World's DroppedMessages
// and RejectedSpawn records and writes each as one Warning record of category "kernel" carrying the World's
// name.
//
// Two ways in (Design Invariants LG-1 and O-12; kernel-diagnostics-exchange-declaration.zh-Hant.md):
//   a healthy World   ForwardKernelDiagnostics, in the World's build, registers an exchange function "out"
//                     that takes them after each of the World's rounds in RunFrame. Records of RunGameInit and
//                     RunSchedule stay in the World until the next RunFrame's "out".
//   a broken World    RunFrame calls no exchange function on it; the runner (or the product's main) calls
//                     TakeKernelDiagnostics itself to take the records of the frame it broke in.
// Not guaranteed: records of a RunSchedule after the last RunFrame are lost with the World if ReplaceWorld
// replaces it next; take them before replacing.
//
// Taking empties them: one World has one taker. A runner that wants to look at them too takes them itself
// and writes them with Logging::Write. An interrupted World's records are taken too (question 2 (c),
// approved), marked world_interrupted. Nothing orders them against the line a product's main may write for
// the failure; readers order lines by mono_ns (revision 3, user 2026-11-21).

namespace GYO::Engine::WorldLogging {

// Returns how many records it took. The World is Ready or Interrupted (the Kernel asserts it).
std::size_t TakeKernelDiagnostics(Kernel::World& world);

// Registers, on the World being built, an exchange function "out" that calls TakeKernelDiagnostics and always
// succeeds (O-12). Call it once per World, in the World's build (a unit's Build or the product's own build);
// it is registered again whenever the build runs again (ReplaceWorld). With it the World has its taker:
// the runner takes only once the World is broken.
void ForwardKernelDiagnostics(Orchestration::WorldSetup& setup);

} // namespace GYO::Engine::WorldLogging
