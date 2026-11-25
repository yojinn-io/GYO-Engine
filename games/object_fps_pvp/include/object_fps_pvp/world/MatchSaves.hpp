#pragma once

// The Match's saves (gateway declaration D7, segment 4): a player's save is read from the save service when they
// join and written back, settled, when they leave. The Match is the authority: no Client writes a save. The rules of
// what is read and written when are match/SaveSync.hpp's (a pure state machine); this is its driver in the World.
//
//   Frame/Last  SyncSaves  PeerDisconnected, then PeerConnected (a reconnect in one frame is no leave) -> SaveSync;
//                          deaths (a respawn tick not seen before) -> SaveSync; each answered HttpCall -> SaveSync;
//                          what SaveSync has due at this tick -> new HttpCall objects (polled each frame, never block)
//
// Known gap: a death in the frame its player disconnects is not counted (SeatPlayers removes the player in Update,
// SyncSaves looks in Last).

#include "object_fps_pvp/match/SaveSync.hpp"

#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/os/DatagramSocket.hpp"

#include <cstdint>
#include <memory>

namespace object_fps_pvp::world {

namespace O = GYO::Engine::Orchestration;

struct MatchSavesOptions final {
    // The save service (its internal protocol), an IPv4 address and port.
    GYO::Engine::OS::DatagramAddress service;
    // The Match's id: the idempotency keys and the saves' lastMatch name it.
    std::uint64_t matchId = 0;
};

// The World's saves: its state is behind a pointer (the calls are not copyable).
struct MatchSaves final {
    struct State;
    std::shared_ptr<State> state;

    [[nodiscard]] match::SaveSyncReport Report() const;
    // A read or write still to finish.
    [[nodiscard]] bool Busy() const;
};
inline void HashValue(GYO::Kernel::Hasher&, const MatchSaves&) {}

// The Match World with its saves: the Resource and SyncSaves join the description's build.
[[nodiscard]] O::WorldDescription WithMatchSaves(O::WorldDescription world, MatchSavesOptions options);

} // namespace object_fps_pvp::world
