#include "gyo/voxel_map_editor/VoxelMap.hpp"

#include "JsonRules.hpp"

#include <cmath>
#include <limits>

namespace Gyo::Tools::VoxelMapEditor {
namespace {

using namespace JsonRules;

// Imports fill at most this many cells, so a mistaken box cannot run away.
constexpr std::int64_t MaxImportedCells = std::int64_t{1} << 24;

std::int64_t WholeCells(const Json& value, const std::string& path) {
    if (!value.is_number()) Fail("a coordinate is a number", path);
    const double number = value.get<double>();
    if (!std::isfinite(number) || std::floor(number) != number || std::fabs(number) > CellLimit)
        Fail("a coordinate is a whole number of cells within +-2^16", path);
    return static_cast<std::int64_t>(number);
}

GridDocument Import(const std::string_view text, const std::string_view pointer, const std::array<std::uint8_t, 4>& color) {
    if (text.size() > MaxGridTextBytes) Fail("the text exceeds 16 MiB", "");
    const auto document = ParseWithoutRepeatedKeys(text);
    // A pointer that is not one, or that leads nowhere (also an array index
    // nlohmann cannot take), is an input error, never an escaping exception.
    const Json* found = nullptr;
    try {
        const Json::json_pointer at{std::string(pointer)};
        if (document.contains(at)) found = &document.at(at);
    } catch (const Json::exception&) {
        Fail("not a JSON pointer that leads to a value", std::string(pointer));
    }
    if (!found) Fail("nothing at the pointer", std::string(pointer));
    const auto& boxes = *found;
    if (!boxes.is_array() || boxes.empty()) Fail("the pointer names a nonempty array of boxes", std::string(pointer));

    std::vector<std::array<std::int64_t, 6>> read;
    std::array<std::int64_t, 3> lowest{std::numeric_limits<std::int64_t>::max(), 0, std::numeric_limits<std::int64_t>::max()};
    std::int64_t cells = 0;
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        const auto path = std::string(pointer) + "/" + std::to_string(i);
        const auto& entry = boxes[i];
        if (!entry.is_object() || !entry.contains("min") || !entry.contains("max"))
            Fail("a box has min and max", path);
        std::array<std::int64_t, 6> box{};
        for (int axis = 0; axis < 3; ++axis) {
            const auto& minimum = entry.at("min");
            const auto& maximum = entry.at("max");
            if (!minimum.is_array() || minimum.size() != 3) Fail("min is three numbers", path + ".min");
            if (!maximum.is_array() || maximum.size() != 3) Fail("max is three numbers", path + ".max");
            box[static_cast<std::size_t>(axis)] = WholeCells(minimum[static_cast<std::size_t>(axis)], At(path + ".min", static_cast<std::size_t>(axis)));
            box[static_cast<std::size_t>(axis + 3)] = WholeCells(maximum[static_cast<std::size_t>(axis)], At(path + ".max", static_cast<std::size_t>(axis)));
            if (!(box[static_cast<std::size_t>(axis)] < box[static_cast<std::size_t>(axis + 3)])) Fail("min is below max on every axis", path);
        }
        if (box[1] < 0) Fail("a box lies on or above the floor (y >= 0)", path + ".min");
        lowest[0] = std::min(lowest[0], box[0]);
        lowest[2] = std::min(lowest[2], box[2]);
        cells += (box[3] - box[0]) * (box[4] - box[1]) * (box[5] - box[2]);
        if (cells > MaxImportedCells) Fail("the boxes fill more than 2^24 cells", path);
        read.push_back(box);
    }

    GridDocument grid;
    grid.origin = {static_cast<std::int32_t>(lowest[0]), 0, static_cast<std::int32_t>(lowest[2])};
    grid.cellSize = 1.0F;
    grid.shapes = DefaultShapes();
    grid.blocks = {{"block", 0, color}};
    for (const auto& box : read) {
        for (auto y = box[1]; y < box[4]; ++y)
            for (auto z = box[2]; z < box[5]; ++z)
                for (auto x = box[0]; x < box[3]; ++x) {
                    const Vec3i cell{static_cast<std::int32_t>(x - lowest[0]), static_cast<std::int32_t>(y),
                                     static_cast<std::int32_t>(z - lowest[2])};
                    if (cell[0] >= CellLimit || cell[1] >= CellLimit || cell[2] >= CellLimit)
                        Fail("the boxes span more than 2^16 cells", std::string(pointer));
                    grid.Set(cell, {1, 0});
                }
    }
    // What the tool writes, it reads back (as the build does with Engine's parser).
    if (auto readBack = ParseGrid(WriteGrid(grid)); !readBack)
        Fail("the imported grid does not read back: " + readBack.error().message, readBack.error().detail);
    return grid;
}

} // namespace

Result<GridDocument> ImportBoxes(const std::string_view text, const std::string_view pointer,
                                 const std::array<std::uint8_t, 4>& color) {
    try {
        return Import(text, pointer, color);
    } catch (Invalid& invalid) {
        return Engine::Base::Err(VoxelMapError::Make(VoxelMapErrorCode::InvalidBoxes,
                                                     "box import: " + std::move(invalid.message),
                                                     std::move(invalid.path)));
    }
}

} // namespace Gyo::Tools::VoxelMapEditor
