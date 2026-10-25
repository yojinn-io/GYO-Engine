#include <doctest/doctest.h>

#include "engine/os/Console.hpp"

#include <string>
#include <utility>
#include <vector>

// The one common test directory allowed to look at the OS.
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace Engine::Os;
using namespace Engine::Os::Console;

// Shutdown requests and the command line of a real process are checked through
// a child process (console_check.py); these cases need none.

TEST_CASE("Os console command line is UTF-8, program name first") {
    const char* argv[] = {"program", "--open", "\xE6\x97\xA5\xE6\x9C\xAC.json", nullptr};
    const auto arguments = CommandLineUtf8(3, argv);
    REQUIRE(arguments);
    REQUIRE_FALSE(arguments->empty());
    CHECK_FALSE(arguments->front().empty());
#if !defined(_WIN32)
    // POSIX hands argv on as given; Windows reads the process's own command line.
    CHECK(*arguments == std::vector<std::string>{"program", "--open", "\xE6\x97\xA5\xE6\x9C\xAC.json"});
#endif
}

#if !defined(_WIN32)
TEST_CASE("Os console command line refuses an argument that is not UTF-8") {
    const char* argv[] = {"program", "\xFF\xFE", nullptr};
    const auto arguments = CommandLineUtf8(2, argv);
    REQUIRE_FALSE(arguments);
    CHECK(arguments.error().code == OsErrorCode::InvalidEncoding);
    CHECK(arguments.error().detail == "path_bytes=2");
}

TEST_CASE("Os console command line of no argv is empty") {
    const auto arguments = CommandLineUtf8(0, nullptr);
    REQUIRE(arguments);
    CHECK(arguments->empty());
}
#endif

TEST_CASE("Os console UTF-8 output is applied and restored") {
#if defined(_WIN32)
    const UINT before = ::GetConsoleOutputCP();
    {
        auto output = Utf8ConsoleOutput::Enable();
        const bool applied = output.Applied();
        if (applied) CHECK(::GetConsoleOutputCP() == CP_UTF8);
        else CHECK(before == 0); // no console to apply it to
        // The moved-to object restores; the moved-from one no longer does.
        const auto moved = std::move(output);
        CHECK(moved.Applied() == applied);
    }
    CHECK(::GetConsoleOutputCP() == before);
#else
    // A POSIX console takes bytes: nothing to change, so the request is met.
    const auto output = Utf8ConsoleOutput::Enable();
    CHECK(output.Applied());
#endif
}

TEST_CASE("Os console has no shutdown request until a handler records one") {
    // This test process installs no handler and receives no request.
    CHECK_FALSE(RequestedShutdown());
}

TEST_CASE("Os console shutdown reasons have stable names") {
    CHECK(std::string(ToString(ShutdownReason::Interrupt)) == "interrupt");
    CHECK(std::string(ToString(ShutdownReason::Terminate)) == "terminate");
    CHECK(std::string(ToString(ShutdownReason::Break)) == "break");
    CHECK(std::string(ToString(ShutdownReason::ConsoleClose)) == "console_close");
}
