// doctest main of the sdl Plugin's tests.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "UnattendedAssertions.hpp"

// SDL assertions end this executable instead of opening a dialog (UnattendedAssertions.hpp).
TEST_CASE("build: a failed SDL assertion ends the run without a dialog") {
    CHECK_MESSAGE(GYO::Plugins::SDL::UnattendedAssertionsInstalled(), "[sdl-assert-unattended] the unattended handler is installed before main");
}
