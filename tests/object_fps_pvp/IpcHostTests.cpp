#include <doctest/doctest.h>

#include "RetroFPS/Pvp/IpcHost.hpp"
#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"
#include "RetroFPS/Pvp/Movement.hpp"
#include "RetroFPS/Pvp/Wire.hpp"
#include "runtime_v7.pb.h"

#include <asio.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <set>
#include <streambuf>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace fps::pvp;
using namespace std::chrono_literals;
using asio::ip::tcp;
namespace pb = object_fps_pvp::runtime::v7;
// Test bodies sit outside this namespace, where protobuf's own ::pb makes pb ambiguous.
using Envelope = pb::RuntimeEnvelope;
using WireDecision = pb::ShotDecision;
constexpr auto Expired = pb::REJECTION_EXPIRED;

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
    // Closes gracefully: no more frames, then read until the Match closes, so
    // the Match sees end of stream and nothing it sent is left unread (which
    // would reset the connection instead).
    void Finish() {
        socket_.shutdown(tcp::socket::shutdown_send);
        for (int frames = 0; frames < 600 && !closed_; ++frames) static_cast<void>(Receive(200ms));
        REQUIRE(closed_);
    }

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

pb::RuntimeEnvelope InputWindow(PlayerId player, std::uint64_t first, std::uint64_t last) {
    pb::RuntimeEnvelope message;
    message.set_protocol_version(wire::ProtocolVersion);
    auto* input = message.mutable_input();
    input->set_player_id(player);
    input->set_movement_epoch(1);
    input->set_life_generation(1);
    for (auto sequence = first; sequence <= last; ++sequence) input->add_commands()->set_sequence(sequence);
    return message;
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

// Captures what the Match writes to std::clog (its event log). The IPC thread
// writes through std::osyncstream, so the buffer locks its own reads and writes.
class ClogCapture final : public std::streambuf {
public:
    ClogCapture() : previous_(std::clog.rdbuf(this)) {}
    ~ClogCapture() override { std::clog.rdbuf(previous_); }
    ClogCapture(const ClogCapture&) = delete;
    ClogCapture& operator=(const ClogCapture&) = delete;
    // True once the log holds text past offset from, within the deadline.
    [[nodiscard]] bool Shows(const std::string& text, std::chrono::milliseconds timeout = 3000ms, std::size_t from = 0) {
        const auto giveUp = std::chrono::steady_clock::now() + timeout;
        for (;;) {
            {
                std::scoped_lock lock(mutex_);
                if (text_.find(text, from) != std::string::npos) return true;
            }
            if (std::chrono::steady_clock::now() >= giveUp) return false;
            std::this_thread::sleep_for(5ms);
        }
    }

    // True once the log holds text at least `times` times, within the deadline.
    [[nodiscard]] bool ShowsTimes(const std::string& text, std::size_t times, std::chrono::milliseconds timeout = 3000ms) {
        const auto giveUp = std::chrono::steady_clock::now() + timeout;
        for (;;) {
            {
                std::scoped_lock lock(mutex_);
                std::size_t found = 0;
                for (auto at = text_.find(text); at != std::string::npos; at = text_.find(text, at + text.size())) ++found;
                if (found >= times) return true;
            }
            if (std::chrono::steady_clock::now() >= giveUp) return false;
            std::this_thread::sleep_for(1ms);
        }
    }
    [[nodiscard]] std::string Text() {
        std::scoped_lock lock(mutex_);
        return text_;
    }
    // The offset of what is logged next.
    [[nodiscard]] std::size_t Size() {
        std::scoped_lock lock(mutex_);
        return text_.size();
    }

protected:
    std::streamsize xsputn(const char* data, std::streamsize count) override {
        std::scoped_lock lock(mutex_);
        text_.append(data, static_cast<std::size_t>(count));
        return count;
    }
    int_type overflow(int_type value) override {
        if (traits_type::eq_int_type(value, traits_type::eof())) return traits_type::not_eof(value);
        std::scoped_lock lock(mutex_);
        text_.push_back(traits_type::to_char_type(value));
        return value;
    }

private:
    std::streambuf* previous_;
    std::mutex mutex_;
    std::string text_;
};

// One frame from a test Gateway that is closed, and the reason the Match logs.
bool ClosesWith(ClogCapture& log, const std::string& address, const std::vector<std::uint8_t>& frame,
                const std::string& reason) {
    const auto from = log.Size(); // only this session's close counts
    Gateway gateway(address);
    REQUIRE(gateway.Receive());
    gateway.SendRaw(frame);
    for (int frames = 0; frames < 600 && !gateway.Closed(); ++frames) static_cast<void>(gateway.Receive(200ms));
    return gateway.Closed() && log.Shows("ipc closed reason=" + reason, 3000ms, from);
}

std::vector<std::uint8_t> Framed(std::string payload) { return wire::Frame(payload); }

// pv7 contract §1: the version is read before the content, a mismatch names both
// versions, and a parse failure, an unknown field and a message sent the wrong
// way are told apart.
TEST_CASE("PvP IPC names each protocol violation") {
    ClogCapture log;
    Server server;
    const auto mismatch = [](std::uint32_t version) {
        return "protocol version mismatch: expected " + std::to_string(wire::ProtocolVersion) + ", received " +
               std::to_string(version);
    };
    // A Match that never greets fails this case once, not once per violation.
    REQUIRE(ClosesWith(log, server.address, Framed(std::string(1, '\xff')), "frame parse"));
    // Another Gateway's version notice (only field 1), every older version and
    // the next one.
    for (std::uint32_t version = 1; version <= wire::ProtocolVersion + 1; ++version) {
        if (version == wire::ProtocolVersion) continue;
        CAPTURE(version);
        Envelope notice;
        notice.set_protocol_version(version);
        CHECK(ClosesWith(log, server.address, Framed(notice.SerializeAsString()), mismatch(version)));
    }
    // The version is judged before the content: a message of another version
    // that this version would also refuse is named by its version.
    Envelope other = Join(1);
    other.set_protocol_version(wire::ProtocolVersion + 1);
    CHECK(ClosesWith(log, server.address, Framed(other.SerializeAsString()), mismatch(wire::ProtocolVersion + 1)));
    other.mutable_ready();
    CHECK(ClosesWith(log, server.address, Framed(other.SerializeAsString()), mismatch(wire::ProtocolVersion + 1)));
    CHECK(ClosesWith(log, server.address, Framed(std::string(1, '\xff')), "frame parse"));
    Envelope empty;
    empty.set_protocol_version(wire::ProtocolVersion);
    std::string unknown = empty.SerializeAsString();
    unknown += std::string{static_cast<char>((99 << 3) & 0x7f | 0x80), static_cast<char>(99 >> 4), 1}; // field 99, varint 1
    CHECK(ClosesWith(log, server.address, Framed(unknown), "unknown message fields=99"));
    CHECK(ClosesWith(log, server.address, Framed(empty.SerializeAsString()), "unknown message: empty"));
    Envelope wrongWay = empty;
    wrongWay.mutable_ready();
    CHECK(ClosesWith(log, server.address, Framed(wrongWay.SerializeAsString()),
                     "unknown message field=" + std::to_string(Envelope::kReadyFieldNumber)));
}

// A Gateway of another version refuses Ready and closes without a frame.
TEST_CASE("PvP IPC hints at another protocol version when the Gateway closes before any frame") {
    ClogCapture log;
    Server server;
    {
        Gateway silent(server.address);
        REQUIRE(silent.Receive());
        silent.Finish();
    }
    CHECK(log.Shows("ipc closed reason=eof"));
    CHECK(log.Shows("ipc hint: the Gateway closed before sending any frame"));
}

TEST_CASE("PvP IPC gives no version hint once the Gateway sent a frame") {
    ClogCapture log;
    Server server;
    {
        Gateway gateway(server.address);
        REQUIRE(gateway.Receive());
        gateway.Send(Join(1));
        gateway.Finish();
    }
    // The next Ready follows the reset, so the first session's close is logged.
    Gateway next(server.address);
    const auto ready = next.Receive();
    REQUIRE(ready);
    CHECK(ready->has_ready());
    CHECK(log.Shows("ipc closed reason=eof"));
    CHECK_FALSE(log.Shows("ipc hint", 0ms));
}

// Stopping the Match while a Gateway is connected ends that session through
// the same log as any other end: its last clock window and the close reason
// (no reset: the Match is going away). Without it a log cannot tell how its
// last connection ended (an acceptance runner that stops the Match first).
TEST_CASE("PvP IPC logs the close of a connected session when the Match stops") {
    ClogCapture log;
    Server server;
    Gateway gateway(server.address);
    REQUIRE(gateway.Receive());
    server.ipc.Stop();
    CHECK(log.Shows("ipc closed reason=stopped"));
    CHECK(log.Shows("clock sync"));
    CHECK(log.Shows("final=1"));
    CHECK_FALSE(log.Shows("ipc closed reason=eof", 0ms));
    server.ipc.Stop(); // a second stop logs nothing more
    const auto text = log.Text();
    CHECK(text.find("ipc closed") == text.rfind("ipc closed"));
}

TEST_CASE("PvP IPC refuses an arena with fewer spawns than the room holds") {
    Arena small = IpcArena();
    small.spawns.resize(MaxPlayers - 1);
    MatchRuntimeHost host{IpcArena()};
    const auto refusal = "Arena " + small.id + " has " + std::to_string(MaxPlayers - 1) + " spawns; a room of " +
                         std::to_string(MaxPlayers) + " players needs at least that many";
    CHECK_THROWS_WITH_AS(IpcHost(host, small), refusal.c_str(), std::runtime_error);
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

// The action lane (Match -> Gateway over TCP) resends every unacknowledged
// decision at most once per action interval until the ACK retires it, and
// sends a retirement once when it advances or when asked (pv7 §2a). These are
// real-time tests, so they assert counts and order, never exact times.
TEST_CASE("PvP IPC resends an unacknowledged decision at most once per action interval until it is retired") {
    Server server;
    Gateway gateway(server.address);
    JoinPlayer(gateway, 5);
    gateway.Send(Actions(5, {1}, 0));
    // Before the ACK the decision stays in the Match and travels again each
    // interval: 400 ms hold at most ceil(400 ms / I) + 1 = 13 frames. The
    // lower bound only catches a lane that stopped resending.
    const auto beforeAck = ReceiveUntil(gateway, Clock::now() + 400ms);
    const auto decisionFrames = std::count_if(beforeAck.begin(), beforeAck.end(),
        [](const auto& frame) { return Carries(frame.message, 1); });
    MESSAGE("frames carrying the unacknowledged decision in 400 ms: " << decisionFrames);
    CHECK(decisionFrames >= 4);
    CHECK(decisionFrames <= 13);
    CHECK(static_cast<std::size_t>(decisionFrames) == CountResults(beforeAck));
    gateway.Send(Actions(5, {}, 1));
    const auto retired = ReceiveFirst(gateway, [](const auto& message) {
        return message.has_action_results() && message.action_results().retired_through() == 1;
    });
    CHECK(retired.action_results().decisions_size() == 0);
    // Nothing unacknowledged: no retired-only repeats.
    const auto resultsWhileIdle = CountResults(ReceiveUntil(gateway, Clock::now() + 500ms));
    CHECK(resultsWhileIdle == 0);
}

// Reads whole results frames for 500 ms once `decisions` reloads (ids 1..n)
// are all decided and none acknowledged.
std::vector<std::vector<ActionId>> UnacknowledgedFrames(ActionId decisions) {
    Server server;
    Gateway gateway(server.address);
    JoinPlayer(gateway, 5);
    for (ActionId first = 1; first <= decisions; first += MaxActionBatch) {
        std::vector<ActionId> ids;
        for (auto id = first; id <= std::min<ActionId>(decisions, first + MaxActionBatch - 1); ++id) ids.push_back(id);
        gateway.Send(Actions(5, ids, 0));
    }
    static_cast<void>(ReceiveFirst(gateway, [&](const auto& message) { return Carries(message, decisions); }));
    std::vector<std::vector<ActionId>> frames;
    for (const auto& frame : ReceiveUntil(gateway, Clock::now() + 500ms)) {
        if (!frame.message.has_action_results()) continue;
        std::vector<ActionId> ids;
        for (const auto& decision : frame.message.action_results().decisions()) ids.push_back(decision.action_id());
        frames.push_back(std::move(ids));
    }
    return frames;
}

TEST_CASE("PvP IPC puts the smallest unacknowledged ids in every results frame and cycles the rest") {
    // Twelve cycle evenly through four slots; ten wrap inside a frame.
    for (const ActionId decisions : {ActionId{16}, ActionId{14}}) {
        CAPTURE(decisions);
        const auto frames = UnacknowledgedFrames(decisions);
        REQUIRE(frames.size() >= 4);
        for (const auto& ids : frames) {
            REQUIRE(ids.size() == MaxActionBatch);
            // Strictly increasing: the Client refuses a frame repeating an id.
            CHECK(std::adjacent_find(ids.begin(), ids.end(), std::greater_equal<>()) == ids.end());
            // The Client acknowledges contiguously: the smallest never starve.
            CHECK(std::vector<ActionId>(ids.begin(), ids.begin() + 4) == std::vector<ActionId>{1, 2, 3, 4});
        }
        // Four cycling slots: any three frames in a row carry all the rest.
        for (std::size_t first = 0; first + 3 <= frames.size(); ++first) {
            std::set<ActionId> cycled;
            for (std::size_t k = first; k < first + 3; ++k) cycled.insert(frames[k].begin() + 4, frames[k].end());
            CAPTURE(first);
            CHECK(cycled.size() == decisions - 4);
        }
    }
}

TEST_CASE("PvP IPC sends the retirement once more when an ACK-only batch shows it was missed") {
    Server server;
    Gateway gateway(server.address);
    JoinPlayer(gateway, 5);
    ActAndRetire(gateway, 5, 1);
    CHECK(CountResults(ReceiveUntil(gateway, Clock::now() + 300ms)) == 0);
    // An ACK no further than the retirement: the Client did not see it.
    gateway.Send(Actions(5, {}, 1));
    const auto again = ReceiveUntil(gateway, Clock::now() + 400ms);
    REQUIRE(CountResults(again) == 1);
    const auto repeat = std::find_if(again.begin(), again.end(), [](const auto& frame) { return frame.message.has_action_results(); });
    CHECK(repeat->message.action_results().retired_through() == 1);
    CHECK(repeat->message.action_results().decisions_size() == 0);
}

TEST_CASE("PvP IPC keeps a lane asked again and again to one results frame per action interval") {
    Server server;
    Gateway gateway(server.address);
    JoinPlayer(gateway, 5);
    ActAndRetire(gateway, 5, 1);
    static_cast<void>(ReceiveUntil(gateway, Clock::now() + 100ms));
    // A refused batch every 5 ms for 300 ms: at most ceil(300 ms / I) + 1 = 10
    // frames, read for 50 ms more so a late tail counts too.
    std::size_t frames = 0;
    const auto end = Clock::now() + 300ms;
    while (Clock::now() < end) {
        gateway.Send(Actions(5, {}, 99));
        frames += CountResults(ReceiveUntil(gateway, Clock::now() + 5ms));
    }
    frames += CountResults(ReceiveUntil(gateway, Clock::now() + 50ms));
    MESSAGE("results frames for 60 refused batches in 300 ms: " << frames);
    CHECK(frames <= 11);
    CHECK(frames >= 3);
}

TEST_CASE("PvP IPC answers a refused action batch with one results frame") {
    Server server;
    Gateway gateway(server.address);
    JoinPlayer(gateway, 5);
    ActAndRetire(gateway, 5, 1);
    CHECK(CountResults(ReceiveUntil(gateway, Clock::now() + 300ms)) == 0);
    auto malformed = Actions(5, {2}, 1);
    malformed.mutable_actions()->mutable_shots(0)->set_life_generation(0);
    std::vector<ActionId> over(MaxActionBatch + 1);
    std::iota(over.begin(), over.end(), ActionId{2});
    // The host refuses this one: it acknowledges what was never decided.
    const auto refusals = {malformed, Actions(5, over, 1), Actions(5, {}, 99)};
    // pv7 §5 (D-6): each answer counts the refusal and names its reason.
    const auto reasons = {object_fps_pvp::runtime::v7::ACTION_REFUSAL_MALFORMED, object_fps_pvp::runtime::v7::ACTION_REFUSAL_OVER_BATCH, object_fps_pvp::runtime::v7::ACTION_REFUSAL_INVALID_BATCH};
    auto reason = reasons.begin();
    std::uint64_t refusedSoFar = 0;
    for (const auto& refused : refusals) {
        CAPTURE(refused.DebugString());
        gateway.Send(refused);
        const auto answer = ReceiveUntil(gateway, Clock::now() + 300ms);
        REQUIRE(CountResults(answer) == 1);
        const auto frame = std::find_if(answer.begin(), answer.end(), [](const auto& f) { return f.message.has_action_results(); });
        CHECK(frame->message.action_results().retired_through() == 1);
        CHECK(frame->message.action_results().decisions_size() == 0);
        CHECK(frame->message.action_results().refused_batches() == ++refusedSoFar);
        CHECK(frame->message.action_results().last_refusal() == *reason++);
    }
    // An accepted batch counts nothing more; the latest reason stays.
    gateway.Send(Actions(5, {2}, 1));
    const auto accepted = ReceiveFirst(gateway, [](const auto& message) { return Carries(message, 2); });
    CHECK(accepted.action_results().refused_batches() == 3);
    CHECK(accepted.action_results().last_refusal() == object_fps_pvp::runtime::v7::ACTION_REFUSAL_INVALID_BATCH);
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

// Arms the lane timer: decision `id` was written at t1 and is due again at
// t1 + 1/30 s (a resend, or its retirement one step later). Returns once the
// snapshot of the retiring step has been read, with t1.
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

TEST_CASE("PvP IPC hands every malformed input and action batch to the host's ingress counts") {
    Server server;
    Gateway gateway(server.address);
    JoinPlayer(gateway, 5);
    static_cast<void>(server.host.TakeIngressStatistics());
    gateway.Send(InputWindow(5, 1, 0));                     // no command
    gateway.Send(InputWindow(5, 1, MaxPendingCommands + 1)); // one over the window
    gateway.Send(InputWindow(99, 1, 1));                    // a player the Match does not hold
    auto overBatch = Actions(5, {}, 0);
    for (ActionId id = 1; id <= MaxActionBatch + 1; ++id) {
        auto* shot = overBatch.mutable_actions()->add_shots();
        shot->set_action_id(id);
        shot->set_kind(object_fps_pvp::runtime::v7::ACTION_RELOAD);
        shot->set_life_generation(1);
    }
    gateway.Send(overBatch);
    auto noYaw = Actions(5, {}, 0);
    auto* aimless = noYaw.mutable_actions()->add_shots();
    aimless->set_action_id(1);
    aimless->set_kind(object_fps_pvp::runtime::v7::ACTION_SHOT);
    aimless->set_life_generation(1);
    aimless->set_pitch(0);
    gateway.Send(noYaw);
    gateway.Send(Actions(5, {1}, 0));
    auto conflicting = Actions(5, {}, 0);
    auto* shot = conflicting.mutable_actions()->add_shots();
    shot->set_action_id(1);
    shot->set_kind(object_fps_pvp::runtime::v7::ACTION_SHOT);
    shot->set_life_generation(1);
    shot->set_yaw(0.5F);
    shot->set_pitch(0);
    gateway.Send(conflicting);
    // Frames are handled in order: once this join is answered, all of the above was counted.
    gateway.Send(Join(6));
    const auto joined = ReceiveFirst(gateway, [](const auto& message) { return message.has_join_result(); });
    REQUIRE(joined.join_result().player_id() == 6);

    const auto taken = server.host.TakeIngressStatistics();
    REQUIRE(taken.contains(5));
    const auto& counts = taken.at(5);
    const auto ipcMalformedInputs = counts.rejected[IngressIndex(IngressInputRejection::Malformed)].inputs;
    CHECK(ipcMalformedInputs == 2);
    const auto ipcReceivedCommands = counts.received.commands;
    CHECK(ipcReceivedCommands == 13);
    const auto ipcOverBatchShots = counts.rejectedActions[IngressIndex(IngressActionRejection::OverBatch)].shots;
    CHECK(ipcOverBatchShots == MaxActionBatch + 1);
    const auto ipcMalformedActions = counts.rejectedActions[IngressIndex(IngressActionRejection::Malformed)].batches;
    CHECK(ipcMalformedActions == 1);
    const auto actionConflicts = counts.rejectedActions[IngressIndex(IngressActionRejection::Conflict)].batches;
    CHECK(actionConflicts == 1);
    CHECK(counts.acceptedActions.batches == 1);
    CHECK(counts.receivedActions.batches == 4);
    const auto& unknown = taken.at(0);
    const auto ipcUnknownPlayerCommands = unknown.rejected[IngressIndex(IngressInputRejection::UnknownPlayer)].commands;
    CHECK(ipcUnknownPlayerCommands == 1);
    CHECK(unknown.received.commands == 1);
}

namespace {
pb::RuntimeEnvelope Heartbeat(std::uint64_t transmit, std::optional<std::uint64_t> origin = {},
                              std::optional<std::uint64_t> receive = {}) {
    pb::RuntimeEnvelope message;
    message.set_protocol_version(wire::ProtocolVersion);
    auto* clock = message.mutable_clock();
    clock->set_transmit_us(transmit);
    if (origin) clock->set_origin_us(*origin);
    if (receive) clock->set_receive_us(*receive);
    return message;
}

pb::RuntimeEnvelope DropReport(std::uint64_t player, std::uint64_t sequence, std::vector<std::pair<int, std::uint64_t>> counts,
                               std::vector<int> samples = {}, bool final = false) {
    pb::RuntimeEnvelope message;
    message.set_protocol_version(wire::ProtocolVersion);
    auto* report = message.mutable_drop_report();
    report->set_player_id(player);
    report->set_report_sequence(sequence);
    report->set_final(final);
    for (const auto& [reason, total] : counts) {
        auto* count = report->add_counts();
        count->set_reason(static_cast<pb::DropReason>(reason));
        count->set_total(total);
    }
    for (const auto reason : samples) {
        auto* sample = report->add_samples();
        sample->set_reason(static_cast<pb::DropReason>(reason));
        sample->set_item_count(1);
    }
    return message;
}

bool DropReasonValid(int value) { return pb::DropReason_IsValid(value); }
std::string DropReasonName(int value) { return pb::DropReason_Name(static_cast<pb::DropReason>(value)); }

// The sum of `key` over the logged clock windows.
std::uint64_t LoggedClockCount(ClogCapture& log, const std::string& key) {
    std::uint64_t total = 0;
    const auto text = log.Text();
    for (auto at = text.find("clock sync "); at != std::string::npos; at = text.find("clock sync ", at + 1)) {
        const auto line = text.substr(at, text.find('\n', at) - at);
        const auto field = line.find(" " + key + "=");
        if (field != std::string::npos) total += std::stoull(line.substr(field + key.size() + 2));
    }
    return total;
}
} // namespace

// pv7 §3: the Match answers a heartbeat with one clock reply that echoes it,
// stamped when written; a heartbeat echoing that reply is measured, and the
// window is logged when the link closes.
TEST_CASE("PvP IPC answers a heartbeat and measures the echoed reply") {
    ClogCapture log;
    Server server;
    {
        Gateway gateway(server.address);
        REQUIRE(gateway.Receive());
        gateway.Send(Heartbeat(100));
        const auto first = ReceiveFirst(gateway, [](const auto& message) { return message.has_clock(); });
        CHECK(first.clock().origin_us() == 100);
        REQUIRE(first.clock().has_receive_us());
        CHECK(first.clock().transmit_us() >= first.clock().receive_us());
        gateway.Send(Heartbeat(200, first.clock().transmit_us(), 199));
        const auto second = ReceiveFirst(gateway, [](const auto& message) { return message.has_clock(); });
        CHECK(second.clock().origin_us() == 200);
        CHECK(second.clock().transmit_us() > first.clock().transmit_us());
        gateway.Finish();
    }
    CHECK(log.Shows("clock sync"));
    CHECK(log.Shows("samples=1 missing=0 invalid=0 negative_delay=0"));
    CHECK(log.Shows("final=1"));
    CHECK_FALSE(log.Shows("ipc hint", 0ms)); // a heartbeat is a frame
}

// Replies never enter the controls (which close the link over 64): a burst of
// heartbeats leaves the link open, each answered at most once, newest wins.
TEST_CASE("PvP IPC keeps heartbeat replies out of the controls") {
    ClogCapture log;
    Server server;
    Gateway gateway(server.address);
    REQUIRE(gateway.Receive());
    constexpr std::uint64_t Heartbeats = 300;
    for (std::uint64_t transmit = 1; transmit <= Heartbeats; ++transmit) gateway.Send(Heartbeat(transmit));
    gateway.Send(Join(5));
    std::uint64_t replies = 0, lastOrigin = 0;
    bool joined = false;
    for (int frames = 0; frames < 2000 && !(joined && lastOrigin == Heartbeats); ++frames) {
        const auto message = gateway.Receive();
        REQUIRE(message);
        if (message->has_clock()) {
            ++replies;
            CHECK(message->clock().origin_us() > lastOrigin);
            lastOrigin = message->clock().origin_us();
        }
        joined = joined || message->has_join_result();
    }
    CHECK_FALSE(gateway.Closed());
    CHECK(joined);
    CHECK(lastOrigin == Heartbeats);
    // No silent exit (D57): every heartbeat was answered or its reply replaced, counted.
    gateway.Finish();
    REQUIRE(log.Shows("final=1"));
    CHECK(replies + LoggedClockCount(log, "replaced") == Heartbeats);
}

namespace {
// What the Match decided in one run of the scenario below.
struct DecisionOutcome {
    std::map<ActionId, std::string> decisions; // by action id, as serialized
    std::vector<std::string> joins;            // join results, in order
    std::string snapshot;                      // the last one written
    std::uint64_t clockReplies{};
    std::uint64_t clockSamples{}; // measured by the Match, from its logged windows
    std::uint64_t copies{};       // commands the host classified as copies
    std::uint64_t otherLife{};    // inputs the host refused for another life
    std::vector<std::int64_t> holds; // every written snapshot's match_hold_us, -1 when absent
    std::string log;                 // the Match's log of the run
};

pb::RuntimeEnvelope MovementInput(PlayerId player, std::uint64_t sequence, float forward, std::uint64_t observedTick) {
    pb::RuntimeEnvelope message;
    message.set_protocol_version(wire::ProtocolVersion);
    auto* input = message.mutable_input();
    input->set_player_id(player);
    input->set_observed_authority_tick(observedTick); // the reference age the host samples
    input->set_movement_epoch(1);
    input->set_life_generation(1);
    auto* command = input->add_commands();
    command->set_sequence(sequence);
    command->set_move_forward(forward);
    return message;
}

pb::RuntimeEnvelope Shot(PlayerId player, ActionId id, std::uint64_t observedTick, bool reload = false) {
    pb::RuntimeEnvelope message;
    message.set_protocol_version(wire::ProtocolVersion);
    auto* actions = message.mutable_actions();
    actions->set_player_id(player);
    auto* shot = actions->add_shots();
    shot->set_action_id(id);
    shot->set_life_generation(1);
    shot->set_observed_authority_tick(observedTick);
    if (reload) {
        shot->set_kind(pb::ACTION_RELOAD);
    } else {
        shot->set_kind(pb::ACTION_SHOT);
        shot->set_yaw(0);
        shot->set_pitch(0);
    }
    return message;
}

// The same frames, with or without clock samples. The host steps on a fake
// clock the test drives; each step's frames are fenced by a Leave of an
// absent player, whose log line shows the IPC thread handed them to the host
// before the step. Clock samples carry extreme values: other-domain times
// 10 s apart, jumps, repeats, unmatched origins and a receive after transmit.
// copies: every input window also arrives again, with a window that repeats
// the previous (resolved) command, and one of a future life: what a Gateway
// forwarding each Client datagram as it came hands the Match (pv7 §2a).
// causes: every input and action batch carries extreme cause records (pv7
// §5): a dwell at the 10 s clamp, beyond it or absent, and Client sequences
// that jump, repeat and are zero; the fences' Leaves carry every reason.
DecisionOutcome RunDecisionScenario(bool clockSamples, bool copies = false, bool causes = false) {
    ClogCapture log;
    auto now = Clock::time_point{} + 1000s;
    MatchRuntimeHost host{IpcArena(), [&now] { return now; }};
    IpcHost ipc{host, IpcArena()};
    const auto address = "127.0.0.1:" + std::to_string(FreePort());
    std::string error;
    REQUIRE(ipc.Start(address, error));
    const auto tick = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(MovementTickSeconds));
    const auto step = [&] {
        static_cast<void>(host.Advance(MovementTickSeconds));
        now += tick;
    };
    DecisionOutcome outcome;
    std::optional<std::uint64_t> lastReply;
    const auto take = [&](const pb::RuntimeEnvelope& message) {
        if (message.has_action_results())
            for (const auto& decision : message.action_results().decisions())
                outcome.decisions.emplace(decision.action_id(), decision.SerializeAsString());
        if (message.has_join_result()) outcome.joins.push_back(message.join_result().SerializeAsString());
        if (message.has_snapshot()) {
            // The hold is a time, not a decision: compared without it.
            auto snapshot = message.snapshot();
            outcome.holds.push_back(snapshot.has_match_hold_us() ? std::int64_t{snapshot.match_hold_us()} : -1);
            snapshot.clear_match_hold_us();
            outcome.snapshot = snapshot.SerializeAsString();
        }
        if (message.has_clock()) {
            ++outcome.clockReplies;
            lastReply = message.clock().transmit_us();
        }
    };
    Gateway gateway(address);
    // Reads what is there now; a Match that never falls quiet (an unpaced
    // lane) fails here instead of hanging the test.
    const auto drain = [&] {
        const auto giveUp = Clock::now() + 2s;
        while (const auto message = gateway.Receive(1ms)) {
            take(*message);
            REQUIRE_MESSAGE(Clock::now() < giveUp, "the Match wrote without pause for 2 s");
        }
    };
    // The accept's reset completes on a step (the Match restarts at tick 0),
    // then Ready comes; no step runs between the reset and Ready.
    std::optional<pb::RuntimeEnvelope> ready;
    for (int attempt = 0; attempt < 4 && !ready; ++attempt) {
        step();
        ready = gateway.Receive(1000ms);
    }
    REQUIRE(ready);
    REQUIRE(ready->has_ready());
    std::size_t fences = 0;
    std::uint64_t transmit = 1;
    constexpr int Steps = 60;
    for (int k = 0; k <= Steps; ++k) {
        if (clockSamples) {
            const auto base = static_cast<std::uint64_t>(k + 1) * 10'000'000;
            gateway.Send(Heartbeat(transmit += 10'000'000));                        // 10 s apart
            gateway.Send(Heartbeat(transmit));                                      // a repeat
            transmit += 3;
            gateway.Send(Heartbeat(transmit, base + 777, base)); // unmatched origin
            transmit += 3;
            gateway.Send(Heartbeat(transmit, 5, transmit + 10)); // receive after transmit
            // Drop reports with extreme values, violations included (pv7 §4).
            gateway.Send(DropReport(static_cast<std::uint64_t>(k % 3), static_cast<std::uint64_t>(k + 1),
                                    {{k % 3 ? 14 : 45, 1'000'000'000ull * static_cast<std::uint64_t>(k + 1)}}, {k % 3 ? 14 : 45}, k == Steps));
            if (lastReply) { // measured, with a jump every 7th step
                transmit += k % 7 ? 3 : 900'000'000;
                gateway.Send(Heartbeat(transmit, *lastReply, transmit - 1));
            }
        }
        std::vector<pb::RuntimeEnvelope> frames;
        if (k == 0) frames = {Join(1), Join(2)};
        else {
            const auto observed = static_cast<std::uint64_t>(k);
            // Player 1 alternates its stride, so a lost or replaced command moves it elsewhere.
            const auto stride = [](std::uint64_t sequence) { return sequence % 2 ? 1.0f : 0.5f; };
            frames = {MovementInput(1, observed, stride(observed), observed - 1), MovementInput(2, observed, 0.0f, observed - 1)};
            if (copies) {
                for (const auto player : {PlayerId{1}, PlayerId{2}}) {
                    const auto forward = [&](std::uint64_t sequence) { return player == 1 ? stride(sequence) : 0.0f; };
                    frames.push_back(MovementInput(player, observed, forward(observed), observed - 1));
                    auto pair = MovementInput(player, observed, forward(observed), observed - 1);
                    if (k > 1) {
                        auto* previous = pair.mutable_input()->mutable_commands()->Add();
                        previous->set_sequence(observed - 1);
                        previous->set_move_forward(forward(observed - 1));
                        std::swap(*pair.mutable_input()->mutable_commands(0), *pair.mutable_input()->mutable_commands(1));
                    }
                    frames.push_back(pair);
                    // Another life, other content: refused, it must change nothing.
                    auto future = MovementInput(player, observed, -1.0f, observed - 1);
                    future.mutable_input()->set_life_generation(2);
                    frames.push_back(future);
                }
            }
            if (k == 20 || k == 30 || k == 40) frames.push_back(Shot(2, static_cast<ActionId>(k / 10 - 1), observed));
            if (k == 45) frames.push_back(Shot(2, 4, 1)); // an old reference: expired
            if (k == 50) frames.push_back(Shot(2, 5, observed, true));
        }
        std::uint32_t frameIndex = 0;
        for (auto& frame : frames) {
            if (causes) {
                const auto variant = (static_cast<std::uint32_t>(k) + frameIndex++) % 4;
                const std::optional<std::uint32_t> dwell = variant == 0 ? std::optional<std::uint32_t>(10'000'000)
                    : variant == 1 ? std::optional<std::uint32_t>(4'000'000'000u) : variant == 2 ? std::optional<std::uint32_t>(0) : std::nullopt;
                const std::uint32_t sequence = variant == 3 ? 0 : 1'000'000'000u - static_cast<std::uint32_t>(k) * 7u * (variant % 2 ? 1u : 0u)
                    + static_cast<std::uint32_t>(k) * 13u;
                if (frame.has_input()) {
                    if (dwell) frame.mutable_input()->set_gateway_dwell_us(*dwell);
                    frame.mutable_input()->set_client_transport_sequence(sequence);
                } else if (frame.has_actions()) {
                    if (dwell) frame.mutable_actions()->set_gateway_dwell_us(*dwell);
                    frame.mutable_actions()->set_client_transport_sequence(sequence);
                }
            }
            gateway.Send(frame);
        }
        auto fence = Leave(99);
        if (causes) fence.mutable_leave()->set_reason(static_cast<pb::LeaveReason>(k % 5)); // 4 is unknown
        gateway.Send(fence);
        REQUIRE(log.ShowsTimes("leave player=99", ++fences));
        step();
        drain();
    }
    for (int k = 0; k < 30; ++k) {
        step();
        drain();
    }
    // No more steps: what is left arrives, the last snapshot last. Bounded,
    // as results resent until acknowledged would never fall quiet.
    for (const auto giveUp = Clock::now() + 3s; Clock::now() < giveUp;) {
        const auto message = gateway.Receive(300ms);
        if (!message) break;
        take(*message);
    }
    gateway.Finish(); // the Match logs its last clock window
    REQUIRE(log.Shows("final=1"));
    outcome.clockSamples = LoggedClockCount(log, "samples");
    ipc.Stop();
    outcome.log = log.Text();
    for (const auto& [player, counts] : host.TakeIngressStatistics(true)) {
        static_cast<void>(player);
        for (const auto copy : {IngressCommandClass::PendingCopy, IngressCommandClass::LateCopy, IngressCommandClass::ResolvedCopy})
            outcome.copies += counts.classified[IngressIndex(copy)];
        outcome.otherLife += counts.rejected[IngressIndex(IngressInputRejection::LifeFuture)].inputs;
    }
    return outcome;
}
} // namespace

// pv7 §2a (edge-input-passthrough): the Match keeps the first arrival of each
// command and refuses what does not fit, so copies, resolved commands and a
// future life change no decision, join result or final snapshot.
TEST_CASE("PvP IPC decisions do not depend on input copies") {
    const auto once = RunDecisionScenario(false);
    const auto copied = RunDecisionScenario(false, true);
    REQUIRE(once.decisions.size() == 5);
    // The copies reached the host and were classified, not lost on the way.
    CHECK(once.copies == 0);
    CHECK(copied.copies > 100);
    CHECK(copied.otherLife > 100);
    CHECK(once.joins == copied.joins);
    CHECK(once.snapshot == copied.snapshot);
    CHECK(once.decisions == copied.decisions);
}

// D32 (pv7 §2): clock samples never reach a decision. The same frames give
// the same decisions, join results and final snapshot (positions, combat,
// slack sample) bit for bit, with no samples and with extreme ones.
// D32 (pv7 §2, §5): the cause records never reach a decision. Extreme dwell
// and Client sequences and every Leave reason give the same decisions, join
// results and final snapshot bit for bit; the Match still measured them.
TEST_CASE("PvP IPC decisions do not depend on cause records") {
    const auto without = RunDecisionScenario(false);
    const auto with = RunDecisionScenario(false, false, true);
    REQUIRE(without.decisions.size() == 5);
    CHECK(without.joins == with.joins);
    CHECK(without.snapshot == with.snapshot);
    CHECK(without.decisions == with.decisions);
    // The records were read, not lost on the way.
    CHECK(with.log.find("dwell_range=0") == std::string::npos);
    CHECK(with.log.find("sequence_missing=0") == std::string::npos);
    CHECK(with.log.find("leave player=99 reason=session_timeout") != std::string::npos);
    CHECK(without.log.find("dwell_samples=0") != std::string::npos);
}

// pv7 §5 (D-4): every written snapshot carries the Match hold, measured on the
// host's (here fake) clock from the publication: at most a few ticks, and a
// whole tick once the test advanced the clock before the IPC thread chose the
// frame. Another clock (the IPC's real one) would clamp it at 10 s.
TEST_CASE("PvP IPC snapshots carry the Match hold from the publication on the host's clock") {
    const auto outcome = RunDecisionScenario(false);
    REQUIRE(outcome.holds.size() > 60);
    const auto tick = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<double>(MovementTickSeconds)).count();
    CHECK(std::all_of(outcome.holds.begin(), outcome.holds.end(), [](auto hold) { return hold >= 0 && hold < 1'000'000; }));
    CHECK(std::any_of(outcome.holds.begin(), outcome.holds.end(), [&](auto hold) { return hold >= tick - 1; }));
}

TEST_CASE("PvP IPC decisions do not depend on clock samples") {
    const auto without = RunDecisionScenario(false);
    const auto with = RunDecisionScenario(true);
    REQUIRE(without.decisions.size() == 5);
    // The reference age decides one of them: the shot of tick 1, seen at tick 45.
    WireDecision expired;
    REQUIRE(expired.ParseFromString(without.decisions.at(4)));
    CHECK(expired.rejection() == Expired);
    CHECK(with.clockReplies > 0);
    CHECK(with.clockSamples > 0); // the Match measured, not only answered
    CHECK(without.clockSamples == 0);
    CHECK(without.joins == with.joins);
    CHECK(without.snapshot == with.snapshot);
    CHECK(without.decisions == with.decisions);
}

// The ClockSample envelope's bytes, shared with the Gateway's
// TestClockSampleBytesMatchTheMatchBinding: both bindings agree.
TEST_CASE("PvP IPC clock samples have the agreed bytes") {
    const std::string full("\x08\x07\xa2\x01\x06\x08\x01\x10\x02\x18\x03", 11);
    const std::string transmitOnly("\x08\x07\xa2\x01\x02\x18\x03", 7);
    Envelope message;
    message.set_protocol_version(7);
    message.mutable_clock()->set_origin_us(1);
    message.mutable_clock()->set_receive_us(2);
    message.mutable_clock()->set_transmit_us(3);
    CHECK(message.SerializeAsString() == full);
    message.mutable_clock()->clear_origin_us();
    message.mutable_clock()->clear_receive_us();
    CHECK(message.SerializeAsString() == transmitOnly);
    Envelope back;
    REQUIRE(back.ParseFromString(full));
    CHECK(back.clock().has_origin_us());
    CHECK(back.clock().origin_us() == 1);
    CHECK(back.clock().receive_us() == 2);
    CHECK(back.clock().transmit_us() == 3);
    REQUIRE(back.ParseFromString(transmitOnly));
    CHECK_FALSE(back.clock().has_origin_us());
    CHECK_FALSE(back.clock().has_receive_us());
}

// pv7 §4: each report is logged as read, with the body the Gateway logs as
// written; a violation is logged and counted, never closing the link (D-5).
TEST_CASE("PvP IPC logs the Gateway's drop reports and flags violations without closing") {
    ClogCapture log;
    Server server;
    Gateway gateway(server.address);
    REQUIRE(gateway.Receive());
    gateway.Send(DropReport(3, 1, {{14, 2}}, {14}));
    gateway.Send(DropReport(3, 2, {{14, 1}}, {13}));              // a total going back; a sample of no count
    gateway.Send(DropReport(3, 4, {{14, 1}}));                    // a gap
    gateway.Send(DropReport(0, 1, {{14, 1}}));                    // a player reason on the Gateway stream
    gateway.Send(DropReport(5, 1, {{1, 1}}, {}, true));           // a final
    gateway.Send(DropReport(5, 2, {{1, 1}}));                     // after it
    gateway.Send(Join(7));
    const auto joined = ReceiveFirst(gateway, [](const auto& message) { return message.has_join_result(); });
    CHECK(joined.join_result().accepted());
    CHECK_FALSE(gateway.Closed());
    CHECK(log.Shows("gw_drop report player=3 report_sequence=1 final=0 samples=1 samples_omitted=0 merged_streams=0 "
                    "drop_inactive_input_packets=2\n"));
    CHECK(log.Shows("gw_drop violation player=3 report_sequence=2 problem=counts_regress"));
    CHECK(log.Shows("gw_drop violation player=3 report_sequence=2 problem=sample_reason"));
    CHECK(log.Shows("gw_drop violation player=3 report_sequence=4 problem=sequence"));
    CHECK(log.Shows("gw_drop violation player=0 report_sequence=1 problem=reason_range"));
    CHECK(log.Shows("gw_drop violation player=5 report_sequence=2 problem=after_final"));
    CHECK_FALSE(log.Shows("player=3 report_sequence=1 problem", 0ms));
}

// Every violation class is logged and the link stays open (D-5).
TEST_CASE("PvP IPC flags every drop report violation class") {
    ClogCapture log;
    Server server;
    Gateway gateway(server.address);
    REQUIRE(gateway.Receive());
    auto merged = DropReport(9, 1, {{1, 1}});
    merged.mutable_drop_report()->set_merged_streams(1); // only the Gateway stream merges
    gateway.Send(merged);
    auto omitted = DropReport(9, 2, {{1, 1}});
    omitted.mutable_drop_report()->set_samples_omitted(5);
    gateway.Send(omitted);
    auto back = DropReport(9, 3, {{1, 1}});
    back.mutable_drop_report()->set_samples_omitted(4); // going back
    gateway.Send(back);
    gateway.Send(DropReport(9, 4, {{1, 1}}, {1, 1, 1, 1, 1}));     // five samples
    gateway.Send(DropReport(9, 5, {{2, 1}, {1, 1}}));              // out of order
    gateway.Send(DropReport(9, 6, {{1, 1}, {1, 1}}));              // twice
    gateway.Send(DropReport(9, 7, {{1, 0}}));                      // a zero total
    gateway.Send(DropReport(9, 8, {{3, 1}}));                      // reason 1 vanished
    auto dwell = DropReport(9, 9, {{3, 1}}, {3});
    dwell.mutable_drop_report()->mutable_samples(0)->set_dwell_us(10'000'001);
    gateway.Send(dwell);
    auto gatewayStream = DropReport(0, 1, {{45, 1}});
    gatewayStream.mutable_drop_report()->set_merged_streams(3);
    gateway.Send(gatewayStream);
    auto gatewayBack = DropReport(0, 2, {{45, 1}});
    gatewayBack.mutable_drop_report()->set_merged_streams(2); // going back
    gateway.Send(gatewayBack);
    gateway.Send(Join(7));
    static_cast<void>(ReceiveFirst(gateway, [](const auto& message) { return message.has_join_result(); }));
    CHECK_FALSE(gateway.Closed());
    CHECK(log.Shows("gw_drop violation player=9 report_sequence=1 problem=merged_streams"));
    CHECK(log.Shows("gw_drop violation player=9 report_sequence=3 problem=samples_omitted"));
    CHECK(log.Shows("gw_drop violation player=9 report_sequence=4 problem=samples_over_4"));
    CHECK(log.Shows("gw_drop violation player=9 report_sequence=5 problem=counts_order"));
    CHECK(log.Shows("gw_drop violation player=9 report_sequence=6 problem=counts_order"));
    CHECK(log.Shows("gw_drop violation player=9 report_sequence=7 problem=counts_order"));
    CHECK(log.Shows("gw_drop violation player=9 report_sequence=8 problem=counts_regress"));
    CHECK(log.Shows("gw_drop violation player=9 report_sequence=9 problem=dwell_range"));
    CHECK(log.Shows("gw_drop violation player=0 report_sequence=2 problem=merged_streams"));
    for (const auto* clean : {"player=9 report_sequence=2 problem", "player=0 report_sequence=1 problem"}) {
        CAPTURE(clean);
        CHECK_FALSE(log.Shows(std::string("gw_drop violation ") + clean, 0ms));
    }
}

// The DropReason golden shared with Go (TestDropReasonsMatchTheGoldenAndTheKeys)
// and Python (test_drop_reasons.py): the same values and names.
TEST_CASE("PvP IPC drop reasons match the shared golden") {
    std::ifstream file(PVP_DROP_REASONS_PATH);
    REQUIRE(file);
    const auto golden = nlohmann::json::parse(file);
    std::set<int> values;
    for (const auto& entry : golden) {
        const int value = entry.at("value").get<int>();
        values.insert(value);
        REQUIRE(DropReasonValid(value));
        CHECK(DropReasonName(value) == entry.at("name").get<std::string>());
    }
    for (int value = 1; value < 128; ++value) {
        CAPTURE(value);
        CHECK(DropReasonValid(value) == values.contains(value));
    }
}

// pv7 §5: the Leave reason is logged; a missing or unknown one is counted as a
// violation, and every Leave removes the player the same way.
TEST_CASE("PvP IPC logs each Leave reason and removes the player for any") {
    ClogCapture log;
    Server server;
    Gateway gateway(server.address);
    JoinPlayer(gateway, 7);
    for (const PlayerId player : {PlayerId{8}, PlayerId{9}}) {
        gateway.Send(Join(player));
        static_cast<void>(ReceiveFirst(gateway, [&](const auto& message) {
            return message.has_join_result() && message.join_result().player_id() == player;
        }));
    }
    const std::vector<std::pair<PlayerId, int>> leaves{{7, object_fps_pvp::runtime::v7::LEAVE_REASON_CLIENT_REQUEST}, {8, 0}, {9, 77}};
    for (const auto& [player, reason] : leaves) {
        auto leave = Leave(player);
        leave.mutable_leave()->set_reason(static_cast<object_fps_pvp::runtime::v7::LeaveReason>(reason));
        gateway.Send(leave);
    }
    CHECK(log.Shows("leave player=7 reason=client_request"));
    CHECK(log.Shows("leave player=8 reason=unspecified"));
    CHECK(log.Shows("leave violation player=8 value=0 count=1"));
    CHECK(log.Shows("leave player=9 reason=unspecified"));
    CHECK(log.Shows("leave violation player=9 value=77 count=2"));
    CHECK_FALSE(log.Text().find("leave violation player=7") != std::string::npos);
    // All three are gone from the snapshots that follow.
    static_cast<void>(ReceiveFirst(gateway, [](const auto& message) {
        return message.has_snapshot() && !SnapshotHas(message, 7) && !SnapshotHas(message, 8) && !SnapshotHas(message, 9);
    }));
}

// pv7 §2, §5: the Client sequence and the dwell each forwarded message carries
// are counted per player and logged at the session end; out-of-range and
// missing values are counted and ignored, the session stays open.
TEST_CASE("PvP IPC counts the Client sequence gaps and the dwell it was handed") {
    ClogCapture log;
    Server server;
    Gateway gateway(server.address);
    JoinPlayer(gateway, 3);
    struct Sent { std::optional<std::uint32_t> sequence, dwell; bool actions; };
    const std::vector<Sent> sent{
        {1, 100, false}, {2, 300, true}, {5, 10'000'000, false}, {4, 10'000'001, false}, {std::nullopt, std::nullopt, true},
        {0, 50, false}, {9, 200, false}};
    std::uint64_t command = 1;
    for (const auto& s : sent) {
        auto message = s.actions ? Actions(3, {}, 0) : InputWindow(3, command, command);
        if (!s.actions) ++command;
        auto set = [&](auto* value) {
            if (s.sequence) value->set_client_transport_sequence(*s.sequence);
            if (s.dwell) value->set_gateway_dwell_us(*s.dwell);
        };
        if (s.actions) set(message.mutable_actions()); else set(message.mutable_input());
        gateway.Send(message);
    }
    gateway.Finish();
    // Gaps: 2 to 5 skips 2, 5 to 9 skips 3; 4 is late; 0 and none are missing.
    CHECK(log.Shows("gw_transport player=3 window_ms="));
    CHECK(log.Shows("final=1 messages=7 sequence_missing=2 sequence_gaps=5 sequence_late=1 largest_sequence=9 "
                    "dwell_samples=5 dwell_missing=1 dwell_range=1 dwell_max_us=10000000 dwell_mean_us=2000130"));
    CHECK_FALSE(log.Text().find("ipc closed reason=eof") == std::string::npos);
}

// pv7 §5 (D-6): the refusal names are the host's, value for value.
TEST_CASE("PvP IPC refusal names are the host's rejection names") {
    for (std::size_t index = 0; index < IngressIndex(IngressActionRejection::Count); ++index) {
        const auto rejection = static_cast<IngressActionRejection>(index);
        std::string name(IngressName(rejection));
        for (auto& c : name) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        CHECK(object_fps_pvp::runtime::v7::ActionRefusal_Name(static_cast<object_fps_pvp::runtime::v7::ActionRefusal>(index + 1)) == "ACTION_REFUSAL_" + name);
    }
}
