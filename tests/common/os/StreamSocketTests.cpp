// StreamSocket (docs/architecture/os.md, Stream; the PvP Gateway port's declaration D8): loopback only, so it runs on
// every L1 runner. The listening end is LoopbackListener.hpp (the test's own native socket).
#include <doctest/doctest.h>

#include "engine/os/StreamSocket.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>

#include "LoopbackListener.hpp"

using namespace GYO::Engine::OS;
using GYO::Test::LoopbackListener;

namespace {

// Polls the socket's state for up to five seconds.
GYO::Engine::Base::Result<StreamState, OSError> Settled(StreamSocket& socket) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    for (;;) {
        auto state = socket.State();
        if (!state || *state == StreamState::Connected || std::chrono::steady_clock::now() > until) return state;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

std::span<const std::byte> AsBytes(const std::string& text) { return std::as_bytes(std::span(text.data(), text.size())); }

// Receives until count bytes or the peer closed (empty optional on the deadline).
std::optional<std::string> ReceiveText(StreamSocket& socket, const std::size_t count, bool& closed) {
    std::string text;
    std::byte buffer[256];
    closed = false;
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < until) {
        auto received = socket.Receive(buffer);
        REQUIRE(received.has_value());
        if (*received) {
            if (**received == 0) {
                closed = true;
                return text;
            }
            text.append(reinterpret_cast<const char*>(buffer), **received);
            if (text.size() >= count) return text;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    return std::nullopt;
}

} // namespace

TEST_CASE("stream socket: a connection carries bytes both ways and sees the peer close") {
    LoopbackListener listener;
    auto connected = StreamSocket::Connect(listener.Address());
    REQUIRE(connected.has_value());
    StreamSocket socket = std::move(connected).value();
    listener.Accept();
    auto state = Settled(socket);
    REQUIRE_MESSAGE(state.has_value(), "the connect to a listening port failed");
    CHECK_MESSAGE(*state == StreamState::Connected, "the connect to a listening port did not finish");

    const std::string request = "GET / HTTP/1.1\r\n\r\n";
    auto sent = socket.Send(AsBytes(request));
    REQUIRE(sent.has_value());
    CHECK(*sent == request.size());
    CHECK(listener.Receive(request.size()) == request);

    listener.Send("HTTP/1.1 200 OK\r\n");
    bool closed = false;
    auto answer = ReceiveText(socket, 17, closed);
    REQUIRE(answer.has_value());
    CHECK(*answer == "HTTP/1.1 200 OK\r\n");
    CHECK_FALSE(closed);

    listener.CloseAccepted();
    auto rest = ReceiveText(socket, 1, closed);
    REQUIRE_MESSAGE(rest.has_value(), "the peer's close was never seen");
    CHECK_MESSAGE(closed, "the peer closed but Receive did not return 0");
}

TEST_CASE("stream socket: nothing waiting is no bytes, and does not block") {
    LoopbackListener listener;
    auto connected = StreamSocket::Connect(listener.Address());
    REQUIRE(connected.has_value());
    listener.Accept();
    REQUIRE(Settled(*connected).has_value());
    std::byte buffer[16];
    auto received = connected->Receive(buffer);
    REQUIRE(received.has_value());
    CHECK_FALSE(received->has_value());
}

TEST_CASE("stream socket: a port nobody listens on is Unreachable") {
    DatagramAddress closedPort{};
    {
        LoopbackListener listener;
        closedPort = listener.Address();
        listener.CloseListening();
    }
    auto connected = StreamSocket::Connect(closedPort);
    REQUIRE(connected.has_value());
    auto state = Settled(*connected);
    REQUIRE_MESSAGE(!state.has_value(), "a connect to a closed port succeeded");
    CHECK(state.error().code == OSErrorCode::Unreachable);
    // It stays failed.
    CHECK_FALSE(connected->State().has_value());
}
