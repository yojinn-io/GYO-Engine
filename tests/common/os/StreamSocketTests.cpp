// StreamSocket (docs/architecture/os.md, Stream): loopback only, so it runs on every L1 runner. The listening end is
// the test library gyo_test_loopback_listener.
#include <doctest/doctest.h>

#include "engine/os/StreamSocket.hpp"

#include "gyo_test/LoopbackListener.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <vector>

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

// Receives until count bytes or the peer closed (nullopt on the deadline).
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

// A connection to the listener, connected.
StreamSocket Connected(LoopbackListener& listener) {
    REQUIRE(listener.Ok());
    auto connected = StreamSocket::Connect(listener.Address());
    REQUIRE(connected.has_value());
    REQUIRE(listener.Accept());
    auto state = Settled(*connected);
    REQUIRE_MESSAGE(state.has_value(), "the connect to a listening port failed");
    REQUIRE_MESSAGE(*state == StreamState::Connected, "the connect to a listening port did not finish");
    return std::move(connected).value();
}

} // namespace

TEST_CASE("stream socket: a connection carries bytes both ways and sees the peer close") {
    LoopbackListener listener;
    StreamSocket socket = Connected(listener);

    const std::string request = "GET / HTTP/1.1\r\n\r\n";
    auto sent = socket.Send(AsBytes(request));
    REQUIRE(sent.has_value());
    CHECK(*sent == request.size());
    CHECK(listener.Receive(request.size()).value_or("") == request);

    REQUIRE(listener.Send("HTTP/1.1 200 OK\r\n"));
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
    StreamSocket socket = Connected(listener);
    std::byte buffer[16];
    auto received = socket.Receive(buffer);
    REQUIRE(received.has_value());
    CHECK_FALSE(received->has_value());
}

TEST_CASE("stream socket: before the connect finishes, Send takes nothing and Receive has nothing, on every platform") {
    LoopbackListener listener;
    REQUIRE(listener.Ok());
    auto connected = StreamSocket::Connect(listener.Address());
    REQUIRE(connected.has_value());
    // Not accepted, and State not asked: as far as the socket knows it may still be connecting.
    const std::string text = "early";
    auto sent = connected->Send(AsBytes(text));
    auto state = connected->State();
    REQUIRE(state.has_value());
    if (*state == StreamState::Connecting) {
        REQUIRE_MESSAGE(sent.has_value(), "a Send while connecting failed: " << sent.error().message);
    }
    std::byte buffer[16];
    auto received = connected->Receive(buffer);
    REQUIRE_MESSAGE(received.has_value(), "a Receive before any byte came failed");
    CHECK_FALSE(received->has_value());
}

TEST_CASE("stream socket: when the OS has no room, Send takes 0 bytes") {
    LoopbackListener listener;
    StreamSocket socket = Connected(listener);
    // The listener reads nothing: the OS buffers fill, then a Send takes nothing (it does not block).
    const std::vector<std::byte> chunk(64 * 1024, std::byte{0x5a});
    bool full = false;
    for (int i = 0; i < 4096 && !full; ++i) {
        auto sent = socket.Send(chunk);
        REQUIRE(sent.has_value());
        full = *sent == 0;
    }
    CHECK_MESSAGE(full, "256 MiB went out without the OS ever running out of room");
}

TEST_CASE("stream socket: a Send after the peer reset fails, it does not end the process") {
    LoopbackListener listener;
    StreamSocket socket = Connected(listener);
    listener.CloseAccepted();
    // The first sends may still be taken (the peer's reset comes back with them); a later one fails. A SIGPIPE here
    // would end the test process instead.
    const std::string text(1024, 'x');
    bool failed = false;
    for (int i = 0; i < 2000 && !failed; ++i) {
        auto sent = socket.Send(AsBytes(text));
        failed = !sent.has_value();
        if (!failed) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK_MESSAGE(failed, "sending to a closed peer never failed");
    // The first failure may be the reset itself (ECONNRESET, no signal); the sends after it are the ones a SIGPIPE
    // would come with (EPIPE). Each fails, and the process goes on.
    for (int i = 0; i < 3; ++i) CHECK_FALSE(socket.Send(AsBytes(text)).has_value());
}

TEST_CASE("stream socket: a port nobody listens on is Unreachable") {
    DatagramAddress closedPort{};
    {
        LoopbackListener listener;
        REQUIRE(listener.Ok());
        closedPort = listener.Address();
        listener.CloseListening();
    }
    auto connected = StreamSocket::Connect(closedPort);
    REQUIRE(connected.has_value());
    auto state = Settled(*connected);
    REQUIRE_MESSAGE(!state.has_value(), "a connect to a closed port succeeded");
    CHECK(state.error().code == OSErrorCode::Unreachable);
    // It stays failed, and Send and Receive say so too.
    CHECK_FALSE(connected->State().has_value());
    CHECK_FALSE(connected->Send(AsBytes(std::string("x"))).has_value());
    std::byte buffer[4];
    CHECK_FALSE(connected->Receive(buffer).has_value());
}
