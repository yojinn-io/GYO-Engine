// The Client's connection liveness (v7.1 outage-resync client-interrupted;
// network-architecture §4 "連線中斷與恢復"), on injected arrival times. The
// network worker's own use of it, with a real socket and a mock Gateway, is in
// build/acceptance/object_fps_pvp/worker_main.cpp (RunOutage).
#include "RetroFPS/Pvp/ClientConnection.hpp"
#include "RetroFPS/Pvp/ConnectionLiveness.hpp"

#include <asio.hpp>
#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <system_error>

namespace {
using namespace fps::pvp;
using namespace std::chrono_literals;
using Event = ConnectionLiveness::Event;
const auto T0 = ConnectionLiveness::Clock::time_point{} + 100s;
} // namespace

TEST_CASE("PvP connection timing matches the shared fixture") {
    std::ifstream input(PVP_CONNECTION_TIMING_PATH);
    REQUIRE(input);
    const auto timing = nlohmann::json::parse(input);
    CHECK(std::chrono::milliseconds{timing.at("interrupted_after_ms").get<std::int64_t>()} == ConnectionInterruptedAfter);
    CHECK(std::chrono::milliseconds{timing.at("timeout_after_ms").get<std::int64_t>()} == ConnectionTimeoutAfter);
}

TEST_CASE("PvP a join times out at T_max before its first Welcome and is never interrupted before a snapshot") {
    ConnectionLiveness liveness;
    liveness.Begin(T0);
    CHECK(liveness.Poll(T0 + 4999ms) == Event::None);
    CHECK_FALSE(liveness.Interrupted());
    CHECK(liveness.NextDeadline() == T0 + ConnectionTimeoutAfter);
    CHECK(liveness.Poll(T0 + 5s) == Event::TimedOut);
}

TEST_CASE("PvP only the first Welcome stands for a snapshot until the first snapshot") {
    ConnectionLiveness liveness;
    liveness.Begin(T0);
    liveness.Welcome(T0 + 1s);
    liveness.Welcome(T0 + 2s); // later Welcomes extend nothing
    liveness.Welcome(T0 + 5s);
    CHECK(liveness.Since() == T0 + 1s);
    CHECK(liveness.Poll(T0 + 5999ms) == Event::None);
    CHECK_FALSE(liveness.Interrupted());
    CHECK(liveness.Poll(T0 + 6s) == Event::TimedOut);
}

TEST_CASE("PvP a playing connection is interrupted once at T_int and times out at T_max from its last snapshot") {
    ConnectionLiveness liveness;
    liveness.Begin(T0);
    liveness.Welcome(T0 + 10ms);
    const auto first = liveness.Snapshot(T0 + 20ms);
    CHECK(first.gap == 0s);
    CHECK_FALSE(first.resumed);
    const auto last = T0 + 40ms;
    CHECK(liveness.Snapshot(last).gap == 20ms);
    CHECK(liveness.NextDeadline() == last + ConnectionInterruptedAfter);
    CHECK(liveness.Poll(last + 499ms) == Event::None);
    CHECK(liveness.Poll(last + 500ms) == Event::Interrupted);
    CHECK(liveness.Interrupted());
    CHECK(liveness.Poll(last + 501ms) == Event::None); // reported once
    liveness.Welcome(last + 1s);                       // Welcomes keep nothing alive while playing
    CHECK(liveness.NextDeadline() == last + ConnectionTimeoutAfter);
    CHECK(liveness.Poll(last + 4999ms) == Event::None);
    CHECK(liveness.Poll(last + 5s) == Event::TimedOut);
}

TEST_CASE("PvP the first valid snapshot after an interruption resumes the connection") {
    ConnectionLiveness liveness;
    liveness.Begin(T0);
    liveness.Welcome(T0);
    static_cast<void>(liveness.Snapshot(T0));
    REQUIRE(liveness.Poll(T0 + 600ms) == Event::Interrupted);
    const auto resumed = liveness.Snapshot(T0 + 2300ms);
    CHECK(resumed.resumed);
    CHECK(resumed.gap == 2300ms);
    CHECK_FALSE(liveness.Interrupted());
    CHECK_FALSE(liveness.Snapshot(T0 + 2310ms).resumed);
    CHECK(liveness.Poll(T0 + 2809ms) == Event::None);
    CHECK(liveness.Poll(T0 + 2810ms) == Event::Interrupted); // a second interruption is reported again
    // A new join starts over.
    liveness.Begin(T0 + 10s);
    CHECK_FALSE(liveness.Interrupted());
    CHECK(liveness.Poll(T0 + 14s) == Event::None);
}

// log-contract §1.1: the worker's connection lines, verbatim.
TEST_CASE("PvP connection interrupted, resumed and summary lines") {
    CHECK(ConnectionInterruptedLine(3, 500ms + 900us, 7) == "connection interrupted version=1 player=3 gap_ms=500 epoch=7");
    CHECK(ConnectionResumedLine(3, 2301ms, 7, 8) ==
          "connection resumed version=1 player=3 gap_ms=2301 epoch_before=7 epoch_after=8");
    CHECK(ConnectionSummaryLine({.player = 3, .interruptions = 2, .maxGap = 2301ms, .transientSocketErrors = 5}) ==
          "connection summary version=1 player=3 interruptions=2 max_gap_ms=2301 transient_socket_errors=5");
}

// D172 D-7: which socket errors only count. Any other error still fails the
// session. The call sites are checked statically (test_diagnostic_isolation.py);
// Windows reports a refused port on an unconnected UDP socket as
// WSAECONNRESET, which the L3 cable and Wi-Fi runs exercise.
TEST_CASE("PvP transient socket errors are the five of a network that may come back") {
    for (const auto error : {std::errc::connection_reset, std::errc::connection_refused, std::errc::network_unreachable,
                             std::errc::host_unreachable, std::errc::network_down})
        CHECK(TransientSocketError(std::make_error_code(error)));
    for (const auto error : {std::errc::bad_file_descriptor, std::errc::message_size, std::errc::permission_denied,
                             std::errc::address_not_available, std::errc::operation_not_permitted})
        CHECK_FALSE(TransientSocketError(std::make_error_code(error)));
    CHECK_FALSE(TransientSocketError({}));
    // The socket's own codes (system category, as asio reports them) too.
    for (const auto error : {asio::error::connection_reset, asio::error::connection_refused, asio::error::network_unreachable,
                             asio::error::host_unreachable, asio::error::network_down})
        CHECK(TransientSocketError(asio::error_code(error)));
    CHECK_FALSE(TransientSocketError(asio::error_code(asio::error::message_size)));
}
