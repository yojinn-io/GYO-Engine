#include "object_fps_pvp/data/VoxelMeshFile.hpp"

#include "data/JsonFields.hpp"

#include <cmath>
#include <stdexcept>

namespace object_fps_pvp::data {

namespace Json = GYO::Engine::Json;
namespace Math = GYO::Engine::Math;

namespace {

Json::ArrayView Array(const Json::Value& value, const std::string_view what) {
    const auto array = value.AsArray();
    if (!array) throw std::runtime_error(std::string{what} + " is not an array");
    return array.value();
}

std::int32_t Int32(const Json::Value& value, const std::string_view what) {
    const std::int64_t number = Integer(value, what);
    // The contract keeps every integer within +-2^30, so sums of two cannot overflow.
    if (number < -(std::int64_t{1} << 30) || number > (std::int64_t{1} << 30)) {
        throw std::runtime_error(std::string{what} + " is outside +-2^30");
    }
    return static_cast<std::int32_t>(number);
}

std::array<std::int32_t, 3> Triple(const Json::Value& value, const std::string_view what) {
    const auto array = Array(value, what);
    if (array.Size() != 3) throw std::runtime_error(std::string{what} + " is not three integers");
    std::array<std::int32_t, 3> triple{};
    std::size_t i = 0;
    for (const auto item : array) triple[i++] = Int32(item, what);
    return triple;
}

VoxelColor Color(const Json::Value& value) {
    const auto array = Array(value, "a colour");
    if (array.Size() != 4) throw std::runtime_error("a colour is four integers");
    VoxelColor color{};
    std::size_t i = 0;
    for (const auto item : array) {
        const std::int64_t component = Integer(item, "a colour component");
        if (component < 0 || component > 255) throw std::runtime_error("a colour component is outside 0 to 255");
        color[i++] = static_cast<std::uint8_t>(component);
    }
    return color;
}

} // namespace

std::optional<VoxelMeshFile> ParseVoxelMeshFile(const std::string_view text, std::string& error) {
    error.clear();
    try {
        const auto document = ParseDocument(text);
        const auto json = AsObject(document.Root(), "the voxel mesh");
        if (String(Member(json, "schema"), "schema") != "gyo.voxel_mesh") throw std::runtime_error("schema is not gyo.voxel_mesh");
        if (Integer(Member(json, "version"), "version") != 1) throw std::runtime_error("unsupported version");
        VoxelMeshFile file;
        file.origin = Triple(Member(json, "origin"), "origin");
        file.cellSize = static_cast<float>(Number(Member(json, "cell_size"), "cell_size"));
        if (!std::isfinite(file.cellSize) || file.cellSize < std::ldexp(1.0F, -16) || file.cellSize > std::ldexp(1.0F, 16)) {
            throw std::runtime_error("cell_size is outside [2^-16, 2^16]");
        }
        for (const auto entry : Array(Member(json, "palette"), "palette")) {
            const auto object = AsObject(entry, "a palette entry");
            const auto faces = Array(Member(object, "faces"), "faces");
            if (faces.Size() != 6) throw std::runtime_error("a palette entry has six colours");
            VoxelPaletteEntry colors{};
            std::size_t i = 0;
            for (const auto face : faces) colors[i++] = Color(face);
            file.palette.push_back(colors);
        }
        if (file.palette.empty()) throw std::runtime_error("the palette is empty");
        for (const auto item : Array(Member(json, "quads"), "quads")) {
            const auto quad = Array(item, "a quad");
            if (quad.Size() != 7) throw std::runtime_error("a quad is seven integers");
            std::array<std::int32_t, 7> v{};
            std::size_t i = 0;
            for (const auto value : quad) v[i++] = Int32(value, "a quad value");
            if (v[3] < 0 || v[3] > 5) throw std::runtime_error("a quad's direction is outside 0 to 5");
            if (v[4] < 1 || v[5] < 1) throw std::runtime_error("a quad's sizes are below 1");
            if (v[6] < 0 || static_cast<std::size_t>(v[6]) >= file.palette.size()) {
                throw std::runtime_error("a quad's palette index is outside the palette");
            }
            file.quads.push_back({{v[0], v[1], v[2]}, static_cast<std::uint8_t>(v[3]), v[4], v[5], static_cast<std::uint32_t>(v[6])});
        }
        for (const auto item : Array(Member(json, "colliders"), "colliders")) {
            const auto object = AsObject(item, "a collider");
            VoxelBox box{Triple(Member(object, "min"), "min"), Triple(Member(object, "max"), "max")};
            for (std::size_t axis = 0; axis < 3; ++axis) {
                if (box.minimum[axis] >= box.maximum[axis]) throw std::runtime_error("a collider's min is not below its max");
            }
            file.colliders.push_back(box);
        }
        return file;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

Math::Vec3 UnitsToWorld(const VoxelMeshFile& file, const std::array<std::int32_t, 3>& units) noexcept {
    const auto axis = [&](const std::size_t i) {
        const std::int64_t sum = std::int64_t{units[i]} + 16 * std::int64_t{file.origin[i]};
        return static_cast<float>(sum) * file.cellSize / 16.0F;
    };
    return {axis(0), axis(1), axis(2)};
}

std::vector<Math::Aabb> ColliderBoxes(const VoxelMeshFile& file) {
    std::vector<Math::Aabb> boxes;
    boxes.reserve(file.colliders.size());
    for (const VoxelBox& box : file.colliders) boxes.push_back({UnitsToWorld(file, box.minimum), UnitsToWorld(file, box.maximum)});
    return boxes;
}

} // namespace object_fps_pvp::data

namespace object_fps_pvp::data {

VoxelSurface ExpandQuads(const VoxelMeshFile& file) {
    VoxelSurface surface;
    const float columns = static_cast<float>(file.palette.size());
    for (const VoxelQuad& quad : file.quads) {
        const std::int32_t x = quad.corner[0], y = quad.corner[1], z = quad.corner[2];
        const std::int32_t w = quad.width, h = quad.height;
        std::array<std::array<std::int32_t, 3>, 4> corners{};
        Math::Vec3 normal{};
        switch (quad.direction) {
        case 0:
        case 1:  // X faces run along Z (w) and Y (h)
            corners = {{{x, y, z}, {x, y, z + w}, {x, y + h, z + w}, {x, y + h, z}}};
            normal = {quad.direction == 0 ? 1.0F : -1.0F, 0.0F, 0.0F};
            break;
        case 2:
        case 3:  // Y faces run along X (w) and Z (h)
            corners = {{{x, y, z}, {x + w, y, z}, {x + w, y, z + h}, {x, y, z + h}}};
            normal = {0.0F, quad.direction == 2 ? 1.0F : -1.0F, 0.0F};
            break;
        default:  // Z faces run along X (w) and Y (h)
            corners = {{{x, y, z}, {x + w, y, z}, {x + w, y + h, z}, {x, y + h, z}}};
            normal = {0.0F, 0.0F, quad.direction == 4 ? 1.0F : -1.0F};
            break;
        }
        const auto base = static_cast<std::uint32_t>(surface.positions.size());
        const std::array<float, 2> uv{(static_cast<float>(quad.palette) + 0.5F) / columns,
                                      (static_cast<float>(quad.direction) + 0.5F) / 6.0F};
        for (const auto& corner : corners) {
            surface.positions.push_back(UnitsToWorld(file, corner));
            surface.normals.push_back(normal);
            surface.uvs.push_back(uv);
        }
        for (const std::uint32_t i : {0U, 1U, 2U, 0U, 2U, 3U}) surface.indices.push_back(base + i);
    }
    return surface;
}

VoxelPaletteImage PaletteImage(const VoxelMeshFile& file) {
    VoxelPaletteImage image;
    image.width = static_cast<std::uint32_t>(file.palette.size());
    image.pixels.resize(static_cast<std::size_t>(image.width) * image.height * 4);
    for (std::uint32_t row = 0; row < image.height; ++row) {
        for (std::uint32_t column = 0; column < image.width; ++column) {
            const VoxelColor& color = file.palette[column][row];
            const std::size_t at = (static_cast<std::size_t>(row) * image.width + column) * 4;
            for (std::size_t c = 0; c < 4; ++c) image.pixels[at + c] = color[c];
        }
    }
    return image;
}

} // namespace object_fps_pvp::data
