#pragma once

#include "doctest/doctest.h"

#include "voxel_map_editor/Grid.hpp"

#include <fstream>
#include <sstream>
#include <string>

// Helpers shared by the voxel map editor's tests.
namespace voxel_map_editor::test {

inline std::string ReadText(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    REQUIRE_MESSAGE(file, path);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

inline std::string ReadFixture(const std::string& name) { return ReadText(std::string(GYO_VOXEL_MAP_FIXTURES) + "/" + name); }

// The grid most cases start from: the default shapes and three blocks, a cube, a slab and a stair.
inline GridDocument TestGrid() {
    GridDocument grid;
    grid.shapes = DefaultShapes();
    grid.blocks = {{"stone", 0, {100, 100, 100, 255}}, {"half", 1, {10, 20, 30, 255}}, {"stairs", 2, {1, 2, 3, 255}}};
    return grid;
}

} // namespace voxel_map_editor::test
