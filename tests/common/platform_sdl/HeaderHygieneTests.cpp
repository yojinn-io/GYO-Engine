#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

// G1: the non-native public headers of the SDL backends pull in no SDL header,
// directly or indirectly. Every SDL header includes SDL_stdinc.h, except the
// platform headers, which define their own guard (docs/architecture/platform-sdl.md,
// Design Invariant 1). A leak is reported as a failing test, not a build error,
// so a mutant that reintroduces an include is killed by its message.
#include "engine/platform/sdl/EntryPoint.hpp"
#include "engine/platform/sdl/SdlLogBridge.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#if defined(GYO_HYGIENE_SDL_INPUT)
#include "engine/input/backend/sdl/SdlInput.hpp"
#endif
#if defined(GYO_HYGIENE_SDL_RENDERER)
#include "engine/render/backend/sdl/SdlRenderer.hpp"
#endif
#if defined(GYO_HYGIENE_SDL_GPU)
#include "engine/render/backend/sdl_gpu/SdlGpuRenderDevice.hpp"
#endif

#if defined(SDL_stdinc_h_) || defined(SDL_h_) || defined(SDL_platform_defines_h_)
constexpr bool kSdlHeaderReached = true;
#else
constexpr bool kSdlHeaderReached = false;
#endif

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Removes // and /* */ comments; string and character literals are kept as
// they are (an SDL name inside one still counts).
std::string StripComments(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        if (text.compare(i, 2, "//") == 0) {
            while (i < text.size() && text[i] != '\n') ++i;
        } else if (text.compare(i, 2, "/*") == 0) {
            const auto end = text.find("*/", i + 2);
            for (const auto stop = end == std::string::npos ? text.size() : end + 2; i < stop; ++i)
                if (text[i] == '\n') out += '\n';  // keeps line numbers
        } else {
            out += text[i++];
        }
    }
    return out;
}

bool IsPublicHeader(const std::filesystem::path& path) {
    const auto extension = path.extension().string();
    if (extension != ".hpp" && extension != ".h" && extension != ".inl") return false;
    bool underInclude = false;
    for (const auto& part : path) {
        if (part == "native") return false;  // the *Native layer may name SDL
        if (part == "include") underInclude = true;
    }
    return underInclude;
}

} // namespace

TEST_CASE("SDL backend public headers do not reach any SDL header") {
    CHECK_MESSAGE(!kSdlHeaderReached,
                  "a non-native SDL backend header includes SDL (directly or through another header)");
}

// G2: a scan of every public header under engine/ outside the native layer.
// The allowlist is empty and may only shrink (Design Invariant 1).
TEST_CASE("Engine public headers outside the native layer name no SDL symbol") {
    const std::filesystem::path engine = std::filesystem::path(GYO_SOURCE_DIR) / "engine";
    REQUIRE(std::filesystem::is_directory(engine));
    const std::regex symbol(R"(\bSDL_\w+|\bSDLK_\w+|\bSDLCALL\b|<SDL3/)");
    std::vector<std::string> leaks;
    std::size_t scanned = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(engine)) {
        if (!entry.is_regular_file()) continue;
        const auto relative = std::filesystem::relative(entry.path(), engine);
        if (!IsPublicHeader(relative)) continue;
        ++scanned;
        std::ifstream file(entry.path(), std::ios::binary);
        std::stringstream content;
        content << file.rdbuf();
        std::istringstream lines(StripComments(content.str()));
        std::string line;
        for (int number = 1; std::getline(lines, line); ++number) {
            if (std::smatch match; std::regex_search(line, match, symbol))
                leaks.push_back("engine/" + relative.generic_string() + ":" + std::to_string(number) + " " +
                                match.str());
        }
    }
    CHECK(scanned > 0);
    std::string report;
    for (const auto& leak : leaks) report += leak + "\n";
    CHECK_MESSAGE(leaks.empty(), "SDL symbols in engine public headers outside native/:\n" << report);
}
