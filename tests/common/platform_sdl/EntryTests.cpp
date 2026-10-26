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

TEST_CASE("Destroying an event watch inside a live frame fails its GYO_ASSERT") {
    // A death test (error-handling.md, platform boundary exception).
    CHECK_MESSAGE(ExitCodeOf("watch-in-live-frame") == 43, "destroying an event watch inside a live frame fails a GYO_ASSERT");
}

TEST_CASE("Destroying an event watch inside a watch callback fails its GYO_ASSERT") {
    CHECK_MESSAGE(ExitCodeOf("watch-in-callback") == 43, "destroying an event watch inside a watch callback fails a GYO_ASSERT");
}

TEST_CASE("Destroying an event watch off the main thread fails its GYO_ASSERT") {
    CHECK_MESSAGE(ExitCodeOf("watch-off-main") == 43, "destroying an event watch off the main thread fails a GYO_ASSERT");
}

TEST_CASE("BaseDirectory is SDL's base path as a UTF-8 path") {
    const auto base = Engine::Platform::Sdl::BaseDirectory();
    REQUIRE(base);
    REQUIRE(SDL_GetBasePath() != nullptr);
    const std::u8string back = base.value().u8string();
    CHECK(std::string(back.begin(), back.end()) == SDL_GetBasePath());
    CHECK_FALSE(Engine::Platform::Sdl::PlatformName().empty());
}
