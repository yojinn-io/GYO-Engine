#include <doctest/doctest.h>

#include "RetroFPS/Pvp/IpcHost.hpp"
#include "RetroFPS/Pvp/MatchRuntimeHost.hpp"
#include "RetroFPS/Pvp/Wire.hpp"
#include "runtime_v6.pb.h"

#include <asio.hpp>

#include <chrono>
#include <memory>
#include <optional>
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
