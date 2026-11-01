// The headless shell (docs/framework/architecture.md, Design Invariant 6).
#include <doctest/doctest.h>

#include "gyo/framework/HeadlessShell.hpp"

#include "engine/os/Console.hpp"

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using Gyo::Tools::Framework::RunHeadlessShell;

TEST_CASE("H1: the headless shell hands its body the command line as Engine converts it") {
    // On Windows Engine reads GetCommandLineW, which differs from these
    // arguments, so a shell that passed argv on would fail here too.
    std::array<char*, 3> argv{const_cast<char*>("prog"), const_cast<char*>("a"),
                              const_cast<char*>("\xE6\x97\xA5\xE6\x9C\xAC")};
    const auto expected = Engine::Os::Console::CommandLineUtf8(static_cast<int>(argv.size()), argv.data());
    REQUIRE(expected.has_value());

    std::vector<std::string> received;
    int bodyCalls = 0;
    int failureCalls = 0;
    const int code = RunHeadlessShell(
        static_cast<int>(argv.size()), argv.data(),
        [&](const std::span<const std::string> arguments) {
            ++bodyCalls;
            received.assign(arguments.begin(), arguments.end());
            return 0;
        },
        [&](std::string_view) {
            ++failureCalls;
            return 2;
        });
    CHECK(code == 0);
    CHECK(bodyCalls == 1);
    CHECK(failureCalls == 0);
    CHECK(received == expected.value());
}

TEST_CASE("H2: the headless shell returns the body's value unchanged") {
    std::array<char*, 1> argv{const_cast<char*>("prog")};
    int bodyCalls = 0;
    const int code = RunHeadlessShell(
        1, argv.data(),
        [&](std::span<const std::string>) {
            ++bodyCalls;
            return 7;
        },
        [](std::string_view) { return 2; });
    CHECK(code == 7);
    CHECK(bodyCalls == 1);
}

TEST_CASE("H3: a command line with no UTF-8 form goes to the failure function, whose value is returned") {
    std::array<char*, 2> argv{const_cast<char*>("prog"), const_cast<char*>("\xFF")};
    if (Engine::Os::Console::CommandLineUtf8(static_cast<int>(argv.size()), argv.data())) {
        // Engine reads this platform's command line elsewhere (Windows), so these
        // arguments never reach it; H1 covers the shell there.
        MESSAGE("Engine does not read argv on this platform; H1 covers the shell");
        return;
    }
    int bodyCalls = 0;
    int failureCalls = 0;
    std::string reason;
    const int code = RunHeadlessShell(
        2, argv.data(),
        [&](std::span<const std::string>) {
            ++bodyCalls;
            return 0;
        },
        [&](const std::string_view text) {
            ++failureCalls;
            reason = text;
            return 9;
        });
    CHECK(code == 9);
    CHECK(bodyCalls == 0);
    CHECK(failureCalls == 1);
    CHECK_FALSE(reason.empty());
}
