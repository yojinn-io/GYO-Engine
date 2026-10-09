#include <doctest/doctest.h>

#include "RetroFPS/Pvp/IpcHost.hpp"
#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"
#include "RetroFPS/Pvp/Wire.hpp"
#include "runtime_v6.pb.h"

#include <asio.hpp>

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace fps::pvp;
using namespace std::chrono_literals;
using asio::ip::tcp;
namespace pb = object_fps_pvp::runtime::v6;

Arena IpcArena() {
    Arena arena;
    arena.id = "synthetic_ipc_arena";
    arena.width = arena.depth = 16;
    arena.spawns = {{{2, 0, 2}, 0}, {{13, 0, 2}, 0}, {{2, 0, 13}, 0}, {{13, 0, 13}, 0}};
    return arena;
}

unsigned short FreePort() {
    asio::io_context io;
    tcp::acceptor probe(io, tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
    return probe.local_endpoint().port();
}

// The Match and its IPC on a free loopback port, started in production order:
// the IpcHost registers its publish listener before the host's role starts.
struct Server final {
    MatchRuntimeHost host{IpcArena()};
    IpcHost ipc{host, IpcArena()};
    std::string address = "127.0.0.1:" + std::to_string(FreePort());
    Server() {
        std::string error;
        REQUIRE(host.Start(error));
        REQUIRE(ipc.Start(address, error));
    }
    ~Server() {
        ipc.Stop();
        host.Stop();
    }
};

// A blocking test Gateway: length-prefixed RuntimeEnvelope frames.
class Gateway final {
public:
    explicit Gateway(const std::string& address) {
        const auto separator = address.rfind(':');
        const tcp::endpoint endpoint(asio::ip::make_address(address.substr(0, separator)),
                                     static_cast<unsigned short>(std::stoi(address.substr(separator + 1))));
        const auto giveUp = std::chrono::steady_clock::now() + 5s;
        for (;;) {
            asio::error_code error;
            socket_.connect(endpoint, error);
            if (!error) break;
            socket_.close(error);
            REQUIRE(std::chrono::steady_clock::now() < giveUp);
            std::this_thread::sleep_for(10ms);
        }
        socket_.non_blocking(true);
    }
    void Send(const pb::RuntimeEnvelope& message) {
        const auto frame = wire::Frame(message.SerializeAsString());
        socket_.non_blocking(false);
        asio::write(socket_, asio::buffer(frame));
        socket_.non_blocking(true);
    }
    void SendRaw(const std::vector<std::uint8_t>& bytes) {
        socket_.non_blocking(false);
        asio::write(socket_, asio::buffer(bytes));
        socket_.non_blocking(true);
    }
    // The next frame, or nullopt when the deadline passes; closed() tells EOF.
    std::optional<pb::RuntimeEnvelope> Receive(std::chrono::milliseconds timeout = 2000ms) {
        const auto giveUp = std::chrono::steady_clock::now() + timeout;
        for (;;) {
            if (input_.size() >= 4) {
                const auto length = static_cast<std::size_t>(wire::Read(std::span(input_).first(4)));
                if (input_.size() >= length + 4) {
                    pb::RuntimeEnvelope message;
                    REQUIRE(message.ParseFromArray(input_.data() + 4, static_cast<int>(length)));
                    input_.erase(input_.begin(), input_.begin() + static_cast<std::ptrdiff_t>(length + 4));
                    return message;
                }
            }
            std::array<std::uint8_t, 4096> buffer{};
            asio::error_code error;
            const auto received = socket_.read_some(asio::buffer(buffer), error);
            if (error == asio::error::eof) {
                closed_ = true;
                return std::nullopt;
            }
            if (error && error != asio::error::would_block && error != asio::error::try_again) {
                closed_ = true;
                return std::nullopt;
            }
            input_.insert(input_.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(received));
            if (!received) {
                if (std::chrono::steady_clock::now() >= giveUp) return std::nullopt;
                std::this_thread::sleep_for(1ms);
            }
        }
    }
    [[nodiscard]] bool Closed() const noexcept { return closed_; }

private:
    asio::io_context io_;
    tcp::socket socket_{io_};
    std::vector<std::uint8_t> input_;
    bool closed_{};
};

pb::RuntimeEnvelope Join(PlayerId player) {
    pb::RuntimeEnvelope message;
    message.set_protocol_version(wire::ProtocolVersion);
    message.mutable_join()->set_player_id(player);
    return message;
}

bool SnapshotHas(const pb::RuntimeEnvelope& message, PlayerId player) {
    if (!message.has_snapshot()) return false;
    for (const auto& state : message.snapshot().players())
        if (state.player_id() == player) return true;
    return false;
}

using Clock = std::chrono::steady_clock;

pb::RuntimeEnvelope Leave(PlayerId player) {
    pb::RuntimeEnvelope message;
    message.set_protocol_version(wire::ProtocolVersion);
    message.mutable_leave()->set_player_id(player);
    return message;
}

// Reloads with no observed tick: each still gets a terminal decision
// (InvalidReference), which is all the lane tests need. No ids: an ACK only.
pb::RuntimeEnvelope Actions(PlayerId player, const std::vector<ActionId>& ids, ActionId acknowledged) {
    pb::RuntimeEnvelope message;
    message.set_protocol_version(wire::ProtocolVersion);
    auto* actions = message.mutable_actions();
    actions->set_player_id(player);
    actions->set_acknowledged_through(acknowledged);
    for (const auto id : ids) {
        auto* shot = actions->add_shots();
        shot->set_action_id(id);
        shot->set_kind(pb::ACTION_RELOAD);
        shot->set_life_generation(1);
    }
    return message;
}

bool Carries(const pb::RuntimeEnvelope& message, ActionId id) {
    if (!message.has_action_results()) return false;
    for (const auto& decision : message.action_results().decisions())
        if (decision.action_id() == id) return true;
    return false;
}

struct Received final {
    pb::RuntimeEnvelope message;
    Clock::time_point at;
};

// Frames received until `until`; the last one may be read just after it.
std::vector<Received> ReceiveUntil(Gateway& gateway, Clock::time_point until) {
    std::vector<Received> frames;
    for (;;) {
        const auto left = std::chrono::ceil<std::chrono::milliseconds>(until - Clock::now());
        if (left <= 0ms) return frames;
        auto message = gateway.Receive(left);
        if (!message) return frames;
        frames.push_back({std::move(*message), Clock::now()});
    }
}

std::size_t CountResults(const std::vector<Received>& frames) {
    return static_cast<std::size_t>(std::count_if(frames.begin(), frames.end(),
        [](const auto& frame) { return frame.message.has_action_results(); }));
}

// Reads frames until one satisfies `match`; fails the test after 2 s.
template <typename Match>
pb::RuntimeEnvelope ReceiveFirst(Gateway& gateway, Match match) {
    const auto giveUp = Clock::now() + 2s;
    while (Clock::now() < giveUp) {
        auto message = gateway.Receive(100ms);
        if (message && match(*message)) return std::move(*message);
    }
    FAIL("the expected frame did not arrive within 2 s");
    return {};
}

// Reads Ready, joins `player` and returns once the join is accepted.
void JoinPlayer(Gateway& gateway, PlayerId player) {
    const auto ready = gateway.Receive();
    REQUIRE(ready);
    REQUIRE(ready->has_ready());
    gateway.Send(Join(player));
    const auto joined = ReceiveFirst(gateway, [](const auto& message) { return message.has_join_result(); });
    REQUIRE(joined.join_result().player_id() == player);
    REQUIRE(joined.join_result().accepted());
}

// One action through its decision, then its ACK through the retirement.
void ActAndRetire(Gateway& gateway, PlayerId player, ActionId id) {
    gateway.Send(Actions(player, {id}, id - 1));
    static_cast<void>(ReceiveFirst(gateway, [&](const auto& message) { return Carries(message, id); }));
    gateway.Send(Actions(player, {}, id));
    static_cast<void>(ReceiveFirst(gateway, [&](const auto& message) {
        return message.has_action_results() && message.action_results().retired_through() == id;
    }));
}

} // namespace

TEST_CASE("PvP IPC greets with Ready and sends a join's result before any snapshot showing the player") {
    Server server;
    Gateway gateway(server.address);
    const auto ready = gateway.Receive();
    REQUIRE(ready);
    REQUIRE(ready->has_ready());
    CHECK(ready->ready().arena_id() == "synthetic_ipc_arena");
    CHECK(ready->ready().arena_digest() != 0);
    gateway.Send(Join(7));
    bool joined = false;
    for (int frames = 0; frames < 600 && !joined; ++frames) {
        const auto message = gateway.Receive();
        REQUIRE(message);
        // Controls go before snapshots: the snapshot of the join's tick waits.
        CHECK_FALSE(SnapshotHas(*message, 7));
        if (message->has_join_result()) {
            CHECK(message->join_result().player_id() == 7);
            CHECK(message->join_result().accepted());
            joined = true;
        }
    }
    REQUIRE(joined);
    bool seen = false;
    for (int frames = 0; frames < 600 && !seen; ++frames) {
        const auto message = gateway.Receive();
        REQUIRE(message);
        seen = SnapshotHas(*message, 7);
    }
    CHECK(seen);
}

TEST_CASE("PvP IPC resets the Match between connections") {
    Server server;
    {
        Gateway first(server.address);
        REQUIRE(first.Receive());
        first.Send(Join(3));
        bool present = false;
        for (int frames = 0; frames < 600 && !present; ++frames) {
            const auto message = first.Receive();
            REQUIRE(message);
            present = SnapshotHas(*message, 3);
        }
        REQUIRE(present);
    } // closes the connection
    // The session's end resets the Match at once, before any next Gateway:
    // nothing takes snapshots between connections, so the test reads them.
    bool cleared = false;
    const auto giveUp = std::chrono::steady_clock::now() + 2s;
    while (!cleared && std::chrono::steady_clock::now() < giveUp) {
        if (const auto snapshot = server.host.TakeSnapshot()) cleared = snapshot->players.empty();
        else std::this_thread::sleep_for(5ms);
    }
    CHECK(cleared);
    Gateway second(server.address);
    const auto ready = second.Receive();
    REQUIRE(ready);
    CHECK(ready->has_ready());
    // The next connection sees a fresh Match: snapshots without the old player.
    int snapshots = 0;
    for (int frames = 0; frames < 600 && snapshots < 10; ++frames) {
        const auto message = second.Receive();
        REQUIRE(message);
        if (!message->has_snapshot()) continue;
        ++snapshots;
        CHECK_FALSE(SnapshotHas(*message, 3));
    }
    CHECK(snapshots == 10);
}

TEST_CASE("PvP IPC ends a session on a protocol violation and accepts the next Gateway") {
    Server server;
    {
        Gateway bad(server.address);
        REQUIRE(bad.Receive());
        bad.SendRaw({0, 0, 0, 0}); // a zero-length frame
        for (int frames = 0; frames < 600 && !bad.Closed(); ++frames) static_cast<void>(bad.Receive(200ms));
        CHECK(bad.Closed());
    }
    Gateway next(server.address);
    const auto ready = next.Receive();
    REQUIRE(ready);
    CHECK(ready->has_ready());
}

TEST_CASE("PvP IPC stops promptly while waiting for and while serving a Gateway") {
    {
        Server idle;
        const auto started = std::chrono::steady_clock::now();
        idle.ipc.Stop();
        CHECK(std::chrono::steady_clock::now() - started < 1s);
    }
    Server busy;
    Gateway gateway(busy.address);
    REQUIRE(gateway.Receive());
    gateway.Send(Join(1));
    std::this_thread::sleep_for(50ms);
    const auto started = std::chrono::steady_clock::now();
    busy.ipc.Stop();
    busy.host.Stop();
    CHECK(std::chrono::steady_clock::now() - started < 1s);
    CHECK_FALSE(busy.host.Error());
}

// The action lane (Match -> Gateway over TCP) writes only new content, once:
// a decision it has not sent, or a retirement beyond the one it sent. These are
// real-time tests, so they assert counts and order, never exact times.
TEST_CASE("PvP IPC sends each action decision and each retirement once, then writes no results") {
    Server server;
    Gateway gateway(server.address);
    JoinPlayer(gateway, 5);
    gateway.Send(Actions(5, {1}, 0));
    // Before the ACK the decision stays in the Match; it still travels once.
    const auto beforeAck = ReceiveUntil(gateway, Clock::now() + 400ms);
    const auto decisionFrames = std::count_if(beforeAck.begin(), beforeAck.end(),
        [](const auto& frame) { return Carries(frame.message, 1); });
    CHECK(decisionFrames == 1);
    CHECK(CountResults(beforeAck) == 1);
    gateway.Send(Actions(5, {}, 1));
    const auto retired = ReceiveFirst(gateway, [](const auto& message) { return message.has_action_results(); });
    CHECK(retired.action_results().retired_through() == 1);
    CHECK(retired.action_results().decisions_size() == 0);
    // Nothing new: no retired-only repeats.
    const auto resultsWhileIdle = CountResults(ReceiveUntil(gateway, Clock::now() + 500ms));
    CHECK(resultsWhileIdle == 0);
}

TEST_CASE("PvP IPC writes a decision before the snapshot of its tick when the lane was idle") {
    Server server;
    Gateway gateway(server.address);
    JoinPlayer(gateway, 5);
    for (ActionId id = 1; id <= 3; ++id) {
        // Idle well past 1/30 s; the extra milliseconds vary the tick phase.
        static_cast<void>(ReceiveUntil(gateway, Clock::now() + 100ms + 7ms * id));
        gateway.Send(Actions(5, {id}, id - 1));
        // The decision and the snapshot of its tick are published by one step;
        // an idle lane is due at once and lanes go before snapshots.
        std::uint64_t latestSnapshot = 0;
        const auto decision = ReceiveFirst(gateway, [&](const auto& message) {
            if (message.has_snapshot()) latestSnapshot = std::max<std::uint64_t>(latestSnapshot, message.snapshot().tick());
            return Carries(message, id);
        });
        const auto resolvedTick = decision.action_results().decisions(0).resolved_tick();
        CHECK(latestSnapshot < resolvedTick);
        gateway.Send(Actions(5, {}, id));
        static_cast<void>(ReceiveFirst(gateway, [&](const auto& message) {
            return message.has_action_results() && message.action_results().retired_through() == id;
        }));
    }
}

TEST_CASE("PvP IPC writes a lane at most once per action interval while it always has new content") {
    Server server;
    Gateway gateway(server.address);
    JoinPlayer(gateway, 5);
    // A new action each tick, with the ACK of every decision received so far.
    // The first choice follows the first action and choices are at least
    // 1/30 s apart, so 3 s receive at most ceil(3 s / I) + 1 = 91 frames (an
    // unpaced lane writes about 180). Receive delays only lower the count.
    std::set<ActionId> decided;
    ActionId next = 1, acknowledged = 0;
    long frames = 0;
    std::optional<Clock::time_point> previous;
    auto shortest = Clock::duration::max();
    const auto start = Clock::now();
    const auto end = start + 3s;
    auto sendAt = start;
    while (Clock::now() < end) {
        gateway.Send(Actions(5, {next++}, acknowledged));
        sendAt += std::chrono::microseconds(16667);
        for (const auto& frame : ReceiveUntil(gateway, std::min(sendAt, end))) {
            if (frame.message.has_action_results()) {
                for (const auto& decision : frame.message.action_results().decisions()) decided.insert(decision.action_id());
                while (decided.contains(acknowledged + 1)) ++acknowledged;
            }
            if (!frame.message.has_action_results() || frame.at >= end) continue;
            ++frames;
            if (previous) shortest = std::min(shortest, frame.at - *previous);
            previous = frame.at;
        }
    }
    // Recorded only: two frames can arrive in one read of the polling receiver.
    MESSAGE("action results frames in 3 s: " << frames << ", shortest receive interval "
            << std::chrono::duration<double, std::milli>(shortest).count() << " ms");
    CHECK(frames <= 91);
    CHECK(frames >= 30);
}

TEST_CASE("PvP IPC does not spin while its action lanes are idle") {
    Server server;
    Gateway gateway(server.address);
    JoinPlayer(gateway, 5);
    ActAndRetire(gateway, 5, 1);
    static_cast<void>(ReceiveUntil(gateway, Clock::now() + 100ms));
    const auto before = server.ipc.Iterations();
    static_cast<void>(ReceiveUntil(gateway, Clock::now() + 500ms));
    const auto idleIterations = server.ipc.Iterations() - before;
    MESSAGE("IPC handler passes in 500 idle ms: " << idleIterations);
    // Calibrated before batch 08b on this test: 146-150 in 34 runs, retired-only
    // repeats included. The bound only catches a zero-wait spin.
    CHECK(idleIterations < 600);
}

// Arms the lane timer: decision `id` was written at t1 and its retirement
// becomes new content one step later, before t1 + 1/30 s. Returns once the
// snapshot of that step has been read, with t1.
Clock::time_point ArmLaneTimer(Gateway& gateway, PlayerId player, ActionId id) {
    gateway.Send(Actions(player, {id}, id - 1));
    const auto decision = ReceiveFirst(gateway, [&](const auto& message) { return Carries(message, id); });
    const auto written = Clock::now();
    gateway.Send(Actions(player, {}, id));
    const auto resolvedTick = decision.action_results().decisions(0).resolved_tick();
    static_cast<void>(ReceiveFirst(gateway, [&](const auto& message) {
        return message.has_snapshot() && message.snapshot().tick() > resolvedTick;
    }));
    return written;
}

TEST_CASE("PvP IPC drops a lane whose timer waits when the player leaves, and stops promptly while one waits") {
    {
        Server server;
        Gateway gateway(server.address);
        JoinPlayer(gateway, 5);
        const auto written = ArmLaneTimer(gateway, 5, 1);
        gateway.Send(Leave(5));
        const bool beforeDeadline = Clock::now() - written < 25ms;
        const auto after = ReceiveUntil(gateway, Clock::now() + 300ms);
        // A stalled test may read the snapshot after the retirement was due.
        if (beforeDeadline) CHECK(CountResults(after) == 0);
        else CHECK(CountResults(after) <= 1);
        CHECK(std::any_of(after.begin(), after.end(), [](const auto& frame) { return frame.message.has_snapshot(); }));
        // The session still serves lanes.
        gateway.Send(Join(6));
        static_cast<void>(ReceiveFirst(gateway, [](const auto& message) {
            return message.has_join_result() && message.join_result().player_id() == 6 && message.join_result().accepted();
        }));
        ActAndRetire(gateway, 6, 1);
    }
    Server server;
    Gateway gateway(server.address);
    JoinPlayer(gateway, 5);
    static_cast<void>(ArmLaneTimer(gateway, 5, 1));
    const auto started = Clock::now();
    server.ipc.Stop();
    server.host.Stop();
    CHECK(Clock::now() - started < 1s);
    CHECK_FALSE(server.host.Error());
}

TEST_CASE("PvP IPC starts each connection with empty action lanes") {
    Server server;
    {
        Gateway first(server.address);
        JoinPlayer(first, 5);
        first.Send(Actions(5, {1}, 0));
        static_cast<void>(ReceiveFirst(first, [](const auto& message) { return Carries(message, 1); }));
    } // closes the connection; the Match resets
    Gateway second(server.address);
    JoinPlayer(second, 5);
    // The same id in the new Match is new content for the new lane.
    second.Send(Actions(5, {1}, 0));
    const auto decision = ReceiveFirst(second, [](const auto& message) { return Carries(message, 1); });
    CHECK(decision.action_results().player_id() == 5);
}
