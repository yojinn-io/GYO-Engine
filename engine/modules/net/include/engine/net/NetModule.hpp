#pragma once

// The net Module's registration unit (declaration D-3, D-10; Design Invariants draft in
// docs/architecture/target/v2/net.zh-Hant.md). Carried by default (config/modules.csv) and free when unused:
//   - the Product: with NetProductOptions in ProductDescription::options, Configure makes the NetService (the
//     Link and the connection layer) in the Product's buffer; without them nothing is made, no socket opened;
//   - a World: with NetWorldOptions in WorldDescription::options (its role), Build registers the Peer component,
//     the two messages, the NetWorldLink Resource and two apply-point Systems, ReceiveNet in Main/First and
//     SendNet in Main/Last (orchestration O-12); without them it registers nothing (G-1).
// In a World with an authority strategy the two Systems are marked authority.OneEndOnly; in a World without
// one (K1) nothing goes through the authority Module (AU-E2 otherwise).

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "engine/kernel/Hash.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/net/Handshake.hpp"
#include "engine/net/NetService.hpp"
#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::Net {

// WorldDescription::options: the World that takes part in the network, and its role. idRange: the allotter
// range to compare, when the author replaced entity_ids' RangeAllotter (none: read from RangeAllotter).
struct NetWorldOptions final {
    NetRole role = NetRole::Server;
    std::optional<IdRange> idRange;
};

// One connection, as an entity (P12: a Peer is not a Player; the product maps userId to its own player). A
// server World has one per accepted client; a client World one for its connection to the server. The network's
// own state: not decision data, not in the authority registrations or the digest, rebuilt from the connection.
struct Peer final {
    std::uint32_t connectionId = 0;
    std::uint64_t userId = 0;
    // The connection's drop counts by DropReason (D-12), refreshed every frame by SendNet.
    std::array<std::uint64_t, kDropReasonCount> drops{};
};
// Hashes nothing: a Peer is the network's state, not World state two runs must agree on (its connection id is
// random).
inline void HashValue(Kernel::Hasher&, const Peer&) {}

// A retained message: a connection was made (peer is its entity).
struct PeerConnected final {
    Kernel::EntityId peer = 0;
    std::uint64_t userId = 0;
};
// A retained message: a connection ended or was never made. peer is 0 when no Peer entity was made (a client
// whose handshake was rejected or timed out).
struct PeerDisconnected final {
    Kernel::EntityId peer = 0;
    std::uint64_t userId = 0;
    DisconnectReason reason = DisconnectReason::Closed;
    std::string detail;
};
inline void HashValue(Kernel::Hasher& h, const PeerConnected& v) {
    HashValue(h, v.peer);
    HashValue(h, v.userId);
}
inline void HashValue(Kernel::Hasher& h, const PeerDisconnected& v) {
    HashValue(h, v.peer);
    HashValue(h, v.userId);
    HashValue(h, v.reason);
    HashValue(h, v.detail);
}

// The World's way to the Product's NetService (O-15: a System reaches a Product-level object through a Resource
// the owning Module puts in the prepare function), and the World side of the connections.
struct NetWorldLink final {
    NetService* service = nullptr;
    NetRole role = NetRole::Server;
    std::optional<std::string> fingerprint;
    std::optional<IdRange> idRange;
    // Server: connection id -> Peer entity. Client: its one Peer entity (0: none yet) and whether its end was
    // reported.
    std::map<std::uint32_t, Kernel::EntityId> peers;
    Kernel::EntityId clientPeer = 0;
    bool clientEndReported = false;
};
inline void HashValue(Kernel::Hasher&, const NetWorldLink&) {}

// The hash of the net Module's registrations (declaration D-5's protocol hash): step 1b has none, so it is the
// hash of the empty list.
[[nodiscard]] std::uint64_t NetProtocolHash();

class NetModule final : public Orchestration::Module {
public:
    NetModule() = default;

    void Configure(Orchestration::ProductSetup& setup) override;
    void Build(Orchestration::WorldSetup& setup) override;
    void Check(const Kernel::FrozenRegistry& registry, Orchestration::WorldCheck& context) override;
};

} // namespace GYO::Engine::Net
