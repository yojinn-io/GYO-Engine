#pragma once

// The Match's saves (gateway declaration D7, segment 4): a player's save is read from the save service when they
// connect and written back, settled, when they leave. The Match is the authority: no Client writes a save.
//
//   Frame/Last  SyncSaves  PeerConnected -> GET /internal/v1/saves/{user}; deaths counted while the player plays;
//                          PeerDisconnected -> PUT the settled save (match + 1, deaths added) with the version read
//                          and the idempotency key "match-{id}-user-{user}-leave-{n}"
//
// The calls are engine/modules/http HttpCall objects polled once a frame (they never block). A call that got no answer
// is sent again with the same key, at most kAttempts times; a refused one (409: the save was written meanwhile, a save
// of another version) is counted as a failure and not retried. A player who leaves before their save was read is
// written once the read comes back.

#include "object_fps_pvp/match/PlayerSave.hpp"

#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/os/DatagramSocket.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace object_fps_pvp::world {

namespace O = GYO::Engine::Orchestration;

struct MatchSavesOptions final {
    // The save service (its internal protocol), an IPv4 address and port.
    GYO::Engine::OS::DatagramAddress service;
    // The Match's id: the idempotency keys name it, so a key is never another Match's.
    std::uint64_t matchId = 0;
};

// What the saves did, for the Match's log and the tests.
struct MatchSavesReport final {
    std::uint64_t reads = 0;
    std::uint64_t writes = 0;
    std::uint64_t failures = 0;
    std::string lastFailure;
};

// The World's saves: its state is behind a pointer (the calls are not copyable).
struct MatchSaves final {
    struct State;
    std::shared_ptr<State> state;

    [[nodiscard]] MatchSavesReport Report() const;
};
inline void HashValue(GYO::Kernel::Hasher&, const MatchSaves&) {}

// The Match World with its saves: the Resource and SyncSaves join the description's build.
[[nodiscard]] O::WorldDescription WithMatchSaves(O::WorldDescription world, MatchSavesOptions options);

} // namespace object_fps_pvp::world
