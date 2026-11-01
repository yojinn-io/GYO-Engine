// The shells' public headers name only neutral types: this file builds with
// the ImGui binding's public include paths alone, which carry neither ImGui's
// nor SDL's (docs/framework/architecture.md, Design Invariant 2).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "gyo/framework/HeadlessShell.hpp"
#include "gyo/framework/WindowShell.hpp"
#include "gyo/framework/imgui/ImGuiShell.hpp"

#include <type_traits>

TEST_CASE("The shells' public headers build without ImGui's or SDL's headers") {
    CHECK(std::is_abstract_v<Gyo::Tools::Framework::WindowShellClient>);
    CHECK(std::is_abstract_v<Gyo::Tools::Framework::ImGuiShellClient>);
}
