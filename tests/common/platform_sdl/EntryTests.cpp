// IP-3 ip3e: the entry point, base directory and platform name.
#include <doctest/doctest.h>

#include "engine/platform/sdl/SdlPlatform.hpp"

#include <SDL3/SDL.h>

#include <cstdlib>
#include <string>
#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace {

// The exit code of a child program (std::system reports a wait status on POSIX).
int ExitCodeOf(const std::string& arguments) {
    const std::string command = std::string("\"") + GYO_ENTRY_PROBE + "\" " + arguments;
#if defined(_WIN32)
    return std::system(("\"" + command + "\"").c_str());
#else
    const int status = std::system(command.c_str());
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

} // namespace

TEST_CASE("The entry point returns what GyoMain returns") {
    CHECK_MESSAGE(ExitCodeOf("7") == 7, "the exit code is GyoMain's return value");
    CHECK_MESSAGE(ExitCodeOf("0") == 0, "the exit code is GyoMain's return value");
}

TEST_CASE("The entry point turns an exception from GyoMain into exit code 1") {
    CHECK_MESSAGE(ExitCodeOf("throw") == 1, "an exception from GyoMain exits with code 1");
}

TEST_CASE("A replaced log bridge fails its GYO_ASSERT and leaves the other output") {
    // A death test: the bridge's destructor is noexcept, so its assertion ends
    // the probe (error-handling.md, platform boundary exception).
    const int code = ExitCodeOf("replaced-bridge");
    CHECK_MESSAGE(code != 0, "destroying a replaced bridge fails a GYO_ASSERT");
    CHECK_MESSAGE(code == 41, "a replaced bridge leaves the other output in place");
}

TEST_CASE("BaseDirectory is SDL's base path as a UTF-8 path") {
    const auto base = Engine::Platform::Sdl::BaseDirectory();
    REQUIRE(base);
    REQUIRE(SDL_GetBasePath() != nullptr);
    CHECK(base.value() == Engine::Platform::Sdl::PathFromUtf8(SDL_GetBasePath()));
    CHECK_FALSE(Engine::Platform::Sdl::PlatformName().empty());
}

TEST_CASE("PathFromUtf8 keeps non-ASCII text") {
    const std::string utf8 = "caf\xC3\xA9-\xE6\x97\xA5\xE6\x9C\xAC";  // café-日本
    const auto path = Engine::Platform::Sdl::PathFromUtf8(utf8);
    const std::u8string back = path.u8string();
    CHECK_MESSAGE(std::string(back.begin(), back.end()) == utf8, "a UTF-8 path round-trips byte for byte");
}
