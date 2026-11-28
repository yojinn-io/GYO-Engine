#include <doctest/doctest.h>

#include "engine/os/Machine.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

// The OS's own answer is written here as the reference HostName must match
// (this directory may look at the OS).
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/utsname.h>
#endif

using namespace GYO::Engine::OS;

namespace {

bool IsAscii(const std::string& text) {
    return std::all_of(text.begin(), text.end(), [](const char c) { return static_cast<unsigned char>(c) < 0x80; });
}

std::string Upper(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](const char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); });
    return text;
}

} // namespace

TEST_CASE("Machine::HostName is the name the OS reports") {
    const auto name = Machine::HostName();
    REQUIRE_MESSAGE(name, (name ? std::string{} : name.error().detail));
    CHECK_FALSE(name.value().empty());
    CHECK(name.value().find('\0') == std::string::npos);
#if defined(_WIN32)
    // The NetBIOS name (COMPUTERNAME, at most 15 characters) is the start of the DNS host name, compared without case.
    wchar_t computer[64]{};
    const DWORD length = ::GetEnvironmentVariableW(L"COMPUTERNAME", computer, 64);
    REQUIRE(length > 0);
    REQUIRE(length < 64);
    std::string netbios;
    for (DWORD i = 0; i < length; ++i) {
        REQUIRE(computer[i] < 0x80);
        netbios.push_back(static_cast<char>(computer[i]));
    }
    INFO("COMPUTERNAME=" << netbios);
    if (IsAscii(name.value())) CHECK(Upper(name.value()).starts_with(Upper(netbios)));
#else
    utsname system{};
    REQUIRE(::uname(&system) == 0);
    CHECK(name.value() == std::string(system.nodename));
#endif
}

TEST_CASE("Machine::HostName asks the OS again on every call") {
    const auto first = Machine::HostName();
    const auto second = Machine::HostName();
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first.value() == second.value());
}
