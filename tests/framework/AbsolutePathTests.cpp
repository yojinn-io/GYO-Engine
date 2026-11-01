#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "gyo/framework/AbsolutePath.hpp"

#include <filesystem>

using Gyo::Tools::Framework::AbsolutePath;

// AbsolutePath (framework-rework D-2): content mounting and ui_editor's remembered paths use it.

TEST_CASE("A relative path becomes absolute and lexically normal") {
    const std::filesystem::path result = AbsolutePath("a/./b/../c.json");
    CHECK(result.is_absolute());
    CHECK(result == result.lexically_normal());
    CHECK(result.filename() == "c.json");
    CHECK(result.parent_path().filename() == "a");
}

TEST_CASE("An absolute path is only normalised") {
    const std::filesystem::path base = AbsolutePath("x");
    const std::filesystem::path root = base.parent_path();
    CHECK(AbsolutePath(root / "y" / ".." / "z") == root / "z");
}

TEST_CASE("An empty path stays empty") {
    CHECK(AbsolutePath("").empty());
}
