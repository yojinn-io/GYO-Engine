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
// One World has one taker (Design Invariant LG-1; O-12; kernel-diagnostics-exchange-declaration.zh-Hant.md):
//   registered        ForwardKernelDiagnostics, in the World's build, registers an exchange function "out"
//                     that takes them after each of the World's rounds in RunFrame (a frame later when an
//                     earlier "out" fails). Records of RunGameInit and RunSchedule stay in the World until the
//                     next "out". The runner (or the product's main) takes only what no "out" will take: once
//                     a World is broken (the Product has ended: no exchange function runs on any World), or
//                     before it replaces that World or destroys the Product.
//   not registered    the runner takes them itself, outside any Run.
// Not guaranteed: records nobody took before the World went (made after the last "out" and before the World
// is replaced or the Product destroyed); records of a new World ReplaceWorld did not put in place (nobody can
// reach it). Inside RunFrame an exception from writing a record leaves RunFrame as O-12 says.
//
// Taking empties them. A runner that wants to look at them itself does not register ForwardKernelDiagnostics:
// it takes them and writes them with Logging::Write. An interrupted World's records are taken too (question 2 (c),
// approved), marked world_interrupted. Nothing orders them against the line a product's main may write for
// the failure; readers order lines by mono_ns (revision 3, user 2026-11-21).

namespace GYO::Engine::WorldLogging {

// Returns how many records it took. The World is Ready or Interrupted (the Kernel asserts it).
std::size_t TakeKernelDiagnostics(Kernel::World& world);

// Registers, on the World being built, an exchange function "out" that calls TakeKernelDiagnostics and always
// succeeds (O-12). Call it once per World, in the World's build (a unit's Build or the product's own build);
// it is registered again whenever the build runs again (ReplaceWorld). With it the "out" is the World's taker;
// the runner takes only what no "out" will take (see above).
void ForwardKernelDiagnostics(Orchestration::WorldSetup& setup);

} // namespace GYO::Engine::WorldLogging
