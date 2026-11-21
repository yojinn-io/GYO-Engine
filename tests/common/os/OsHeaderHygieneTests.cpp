// Design Invariant 4 of docs/architecture/os.md: the public headers of engine/os
// pull in no Windows header, directly or indirectly, and code that includes them
// needs no NOMINMAX. The macros are recorded before doctest is included, since
// doctest's own implementation includes windows.h on Windows. A leak is reported
// as a failing test, not a build error, so a mutant that adds an include is
// killed by its message.
//
// Only Windows is checked by macro. On the POSIX side the boundary guard's text
// scan of engine/os/include covers it: libc++ and libstdc++ define _PTHREAD_H
// from <string> or <filesystem>, so a macro check would fail without a violation.
#include "engine/os/Console.hpp"
#include "engine/os/Files.hpp"
#include "engine/os/OsError.hpp"
#include "engine/os/Process.hpp"
#include "engine/os/Thread.hpp"
#include "engine/os/WakeSignal.hpp"

#if defined(_WINDOWS_) || defined(_INC_WINDOWS) || defined(_WINSOCKAPI_) || defined(_WINSOCK2API_)
constexpr bool kWindowsHeaderReached = true;
#else
constexpr bool kWindowsHeaderReached = false;
#endif

#if defined(min) || defined(max)
constexpr bool kMinMaxMacroDefined = true;
#else
constexpr bool kMinMaxMacroDefined = false;
#endif

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>

namespace {

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::stringstream content;
    content << file.rdbuf();
    return content.str();
}

} // namespace

TEST_CASE("engine/os public headers reach no Windows header") {
    CHECK_MESSAGE(!kWindowsHeaderReached,
                  "an engine/os public header includes windows.h or winsock (directly or through another header)");
}

TEST_CASE("engine/os public headers define no min or max macro") {
    CHECK_MESSAGE(!kMinMaxMacroDefined, "an engine/os public header defines min or max; callers would need NOMINMAX");
}

// The checks above only see the headers this file includes, so every public
// header of engine/os must be included here.
TEST_CASE("every engine/os public header is included by this test") {
    const std::filesystem::path include = GYO_OS_INCLUDE_DIR;
    REQUIRE(std::filesystem::is_directory(include));
    const std::string self = ReadText(GYO_OS_HYGIENE_SOURCE);
    REQUIRE(!self.empty());
    const std::regex directive(R"re(^#include "(engine/os/[^"]+)")re");
    std::set<std::string> included;
    std::istringstream lines(self);
    for (std::string line; std::getline(lines, line);) {
        if (std::smatch match; std::regex_search(line, match, directive)) included.insert(match[1].str());
    }
    std::size_t headers = 0;
    std::string missing;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(include)) {
        if (!entry.is_regular_file()) continue;
        // UTF-8, as the include directives are; generic_string() is the ANSI code page on Windows.
        const auto text = std::filesystem::relative(entry.path(), include).generic_u8string();
        const std::string relative(text.begin(), text.end());
        ++headers;
        if (!included.contains(relative)) missing += relative + "\n";
    }
    CHECK(headers > 0);
    CHECK_MESSAGE(missing.empty(), "engine/os public headers not included by OsHeaderHygieneTests.cpp:\n"
                                       << missing);
}
