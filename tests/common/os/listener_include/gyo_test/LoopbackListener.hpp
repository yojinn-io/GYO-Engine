#pragma once

// A loopback TCP listener with one accepted connection at a time, for tests of engine/core/os StreamSocket and what is
// built on it (the http Module, a game's calls to its services). engine/core/os has no listener (nothing but tests
// would use one, os.md Constraints 9). This header is neutral: the native socket code is in LoopbackListener.cpp, the
// test library gyo_test_loopback_listener (tests/common/os, where OS headers are allowed). Nothing here blocks past
// the time it is given, and nothing aborts a test: a server thread can report a failure and end.

#include "engine/os/DatagramSocket.hpp"

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace GYO::Test {

class LoopbackListener final {
public:
    // Listens on 127.0.0.1, a port the OS picks. Ok() says whether it does.
    LoopbackListener();
    ~LoopbackListener();
    LoopbackListener(const LoopbackListener&) = delete;
    LoopbackListener& operator=(const LoopbackListener&) = delete;

    [[nodiscard]] bool Ok() const noexcept;
    [[nodiscard]] Engine::OS::DatagramAddress Address() const;

    // Accepts the next connection (closing the one before); false when none came within the time.
    [[nodiscard]] bool Accept(std::chrono::milliseconds within = std::chrono::seconds(10));
    // Sends all of text on the accepted connection.
    [[nodiscard]] bool Send(std::string_view text);
    // Exactly count bytes; nullopt when the connection ended or nothing came for ten seconds.
    [[nodiscard]] std::optional<std::string> Receive(std::size_t count);
    // One HTTP request: its head and the Content-Length bytes of its body.
    [[nodiscard]] std::optional<std::string> ReceiveRequest();

    void CloseAccepted();
    void CloseListening();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace GYO::Test
