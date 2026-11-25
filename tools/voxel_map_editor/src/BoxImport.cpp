#include "voxel_map_editor/Grid.hpp"

#include "JsonRules.hpp"

#include <cmath>
#include <limits>

namespace voxel_map_editor {
namespace {

using namespace JsonRules;

// Imports fill at most this many cells, so a mistaken box cannot run away.
constexpr std::int64_t kMaxImportedCells = std::int64_t{1} << 24;

Check WholeCells(const Json::Value& value, const std::string& path, std::int64_t& out) {
    const auto number = value.AsDouble();
    if (!number) return Broken("a coordinate is a number", path);
    if (!std::isfinite(*number) || std::floor(*number) != *number || std::fabs(*number) > kCellLimit)
        return Broken("a coordinate is a whole number of cells within +-2^16", path);
    out = static_cast<std::int64_t>(*number);
    return std::nullopt;
}

// One reference token of a JSON pointer (RFC 6901): "~1" is '/', "~0" is '~', any other '~' is not a pointer.
std::optional<std::string> Unescaped(const std::string_view token) {
    std::string out;
    for (std::size_t i = 0; i < token.size(); ++i) {
        if (token[i] != '~') {
            out += token[i];
            continue;
        }
        if (i + 1 >= token.size() || (token[i + 1] != '0' && token[i + 1] != '1')) return std::nullopt;
        out += token[++i] == '0' ? '~' : '/';
    }
    return out;
}

// The value a JSON pointer names; Broken when the text is not a pointer or it leads nowhere.
Check Follow(const Json::Value& root, const std::string_view pointer, std::optional<Json::Value>& out) {
    const std::string path(pointer);
    if (!pointer.empty() && pointer.front() != '/') return Broken("not a JSON pointer that leads to a value", path);
    Json::Value at = root;
    std::size_t start = 1;
    while (start <= pointer.size() && !pointer.empty()) {
        const auto end = std::min(pointer.find('/', start), pointer.size());
        const auto token = Unescaped(pointer.substr(start, end - start));
        if (!token) return Broken("not a JSON pointer that leads to a value", path);
        if (const auto object = at.AsObject()) {
            const auto member = object->Find(*token);
            if (!member) return Broken("nothing at the pointer", path);
            at = *member;
        } else if (const auto array = at.AsArray()) {
            // An array index is "0" or digits without a leading zero, below the array's size.
            const bool digits = !token->empty() && token->size() <= 9 &&
                                token->find_first_not_of("0123456789") == std::string::npos &&
                                (token->size() == 1 || token->front() != '0');
            if (!digits) return Broken("nothing at the pointer", path);
            const auto index = std::stoul(*token);
            if (index >= array->Size()) return Broken("nothing at the pointer", path);
            auto item = array->begin();
            for (std::size_t i = 0; i < index; ++i) ++item;
            at = *item;
        } else {
            return Broken("nothing at the pointer", path);
        }
        start = end + 1;
    }
    out = at;
    return std::nullopt;
}

Check Import(const std::string_view text, const std::string_view pointer, const std::array<std::uint8_t, 4>& color,
             GridDocument& grid) {
    if (text.size() > kMaxGridTextBytes) return Broken("the text exceeds 16 MiB", "");
    auto document = Json::Parse(text);
    if (!document) return Broken("not JSON: " + GYO::Engine::Base::Describe(document.error()), "");
    std::optional<Json::Value> found;
    if (Check broken = Follow(document->Root(), pointer, found)) return broken;
    const auto boxes = found->AsArray();
    if (!boxes || boxes->Size() == 0) return Broken("the pointer names a nonempty array of boxes", std::string(pointer));

    std::vector<std::array<std::int64_t, 6>> read;
    std::array<std::int64_t, 3> lowest{std::numeric_limits<std::int64_t>::max(), 0, std::numeric_limits<std::int64_t>::max()};
    std::int64_t cells = 0;
    std::size_t i = 0;
    for (const Json::Value entry : *boxes) {
        const auto path = std::string(pointer) + "/" + std::to_string(i++);
        const auto object = entry.AsObject();
        if (!object || !object->Find("min") || !object->Find("max")) return Broken("a box has min and max", path);
        const auto minimum = Get(*object, "min").AsArray();
        const auto maximum = Get(*object, "max").AsArray();
        if (!minimum || minimum->Size() != 3) return Broken("min is three numbers", path + ".min");
        if (!maximum || maximum->Size() != 3) return Broken("max is three numbers", path + ".max");
        std::array<std::int64_t, 6> box{};
        std::size_t axis = 0;
        for (const Json::Value value : *minimum) {
            if (Check broken = WholeCells(value, At(path + ".min", axis), box[axis])) return broken;
            ++axis;
        }
        axis = 0;
        for (const Json::Value value : *maximum) {
            if (Check broken = WholeCells(value, At(path + ".max", axis), box[axis + 3])) return broken;
            ++axis;
        }
        for (axis = 0; axis < 3; ++axis)
            if (!(box[axis] < box[axis + 3])) return Broken("min is below max on every axis", path);
        if (box[1] < 0) return Broken("a box lies on or above the floor (y >= 0)", path + ".min");
        lowest[0] = std::min(lowest[0], box[0]);
        lowest[2] = std::min(lowest[2], box[2]);
        cells += (box[3] - box[0]) * (box[4] - box[1]) * (box[5] - box[2]);
        if (cells > kMaxImportedCells) return Broken("the boxes fill more than 2^24 cells", path);
        read.push_back(box);
    }

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
                    if (cell[0] >= kCellLimit || cell[1] >= kCellLimit || cell[2] >= kCellLimit)
                        return Broken("the boxes span more than 2^16 cells", std::string(pointer));
                    grid.Set(cell, {1, 0});
                }
    }
    // What the tool writes, it reads back (as the bake does with the Module's parser).
    if (auto readBack = ParseGrid(WriteGrid(grid)); !readBack)
        return Broken("the imported grid does not read back: " + readBack.error().message, readBack.error().detail);
    return std::nullopt;
}

} // namespace

Result<GridDocument> ImportBoxes(const std::string_view text, const std::string_view pointer,
                                 const std::array<std::uint8_t, 4>& color) {
    GridDocument grid;
    if (Check broken = Import(text, pointer, color, grid))
        return GYO::Engine::Base::Err(
            Error::Make(ErrorCode::InvalidBoxes, "box import: " + std::move(broken->message), std::move(broken->path)));
    return grid;
}

} // namespace voxel_map_editor
