#include "gyo/voxel_map_editor/VoxelMap.hpp"

#include "JsonRules.hpp"

#include "engine/base/Sha256.hpp"
#include "engine/voxel_mesh/VoxelMesh.hpp"

#include <algorithm>
#include <map>
#include <tuple>

namespace Gyo::Tools::VoxelMapEditor {
namespace {

using Quad = std::array<std::int32_t, 7>; // x, y, z, dir, w, h, palette (Engine's order)

// A shape turned to one orientation, and whether it covers each whole side of
// its cell (Engine's face order: +X, -X, +Y, -Y, +Z, -Z).
struct TurnedShape final {
    std::vector<Box> boxes;
    std::array<bool, 6> coversSide{};
};

bool CoversSide(const std::vector<Box>& boxes, const int side) {
    const int axis = side / 2;
    const bool positive = side % 2 == 0;
    const int u = axis == 0 ? 2 : 0; // the two in-plane axes
    const int v = axis == 1 ? 2 : 1;
    std::array<bool, Units * Units> covered{};
    for (const auto& box : boxes) {
        if ((positive ? box.maximum[axis] : box.minimum[axis]) != (positive ? Units : 0)) continue;
        for (int a = box.minimum[u]; a < box.maximum[u]; ++a)
            for (int b = box.minimum[v]; b < box.maximum[v]; ++b) covered[static_cast<std::size_t>(a * Units + b)] = true;
    }
    return std::all_of(covered.begin(), covered.end(), [](const bool c) { return c; });
}

std::map<std::pair<std::uint32_t, std::uint8_t>, TurnedShape> TurnShapes(const GridDocument& document) {
    std::map<std::pair<std::uint32_t, std::uint8_t>, TurnedShape> turned;
    for (const auto& [at, cells] : document.chunks) {
        for (const auto& cell : cells) {
            if (cell.block == 0) continue;
            const auto shape = document.blocks[cell.block - 1u].shape;
            const auto key = std::make_pair(shape, cell.orientation);
            if (turned.contains(key)) continue;
            TurnedShape result;
            for (const auto& box : document.shapes[shape].boxes) result.boxes.push_back(RotateBox(box, cell.orientation));
            for (int side = 0; side < 6; ++side) result.coversSide[static_cast<std::size_t>(side)] = CoversSide(result.boxes, side);
            turned.emplace(key, std::move(result));
        }
    }
    return turned;
}

// The face of a box looking along dir, in Engine's quad form.
Quad FaceQuad(const Box& b, const int dir, const std::int32_t palette) {
    const int axis = dir / 2;
    const bool positive = dir % 2 == 0;
    Quad q{};
    q[0] = b.minimum[0];
    q[1] = b.minimum[1];
    q[2] = b.minimum[2];
    q[static_cast<std::size_t>(axis)] = positive ? b.maximum[axis] : b.minimum[axis];
    q[3] = dir;
    switch (axis) {
    case 0: q[4] = b.maximum[2] - b.minimum[2]; q[5] = b.maximum[1] - b.minimum[1]; break; // (Z, Y)
    case 1: q[4] = b.maximum[0] - b.minimum[0]; q[5] = b.maximum[2] - b.minimum[2]; break; // (X, Z)
    default: q[4] = b.maximum[0] - b.minimum[0]; q[5] = b.maximum[1] - b.minimum[1]; break; // (X, Y)
    }
    q[6] = palette;
    return q;
}

std::uint8_t Shade(const std::uint8_t channel, const std::uint32_t percent) {
    return static_cast<std::uint8_t>((channel * percent + 50u) / 100u);
}

std::string Ints(const std::initializer_list<std::int64_t> values) {
    std::string out = "[";
    bool first = true;
    for (const auto v : values) {
        if (!first) out += ", ";
        first = false;
        out += std::to_string(v);
    }
    return out + "]";
}

} // namespace

std::vector<Box> MergeBoxes(const std::span<const Box> boxes) {
    // Slab by slab along y: the rectangles of the boxes that span a slab are
    // merged greedily (along x, then z), and a rectangle that continues into
    // the next slab unchanged extends its box upwards.
    std::vector<std::int32_t> ys;
    for (const auto& box : boxes) {
        ys.push_back(box.minimum[1]);
        ys.push_back(box.maximum[1]);
    }
    std::sort(ys.begin(), ys.end());
    ys.erase(std::unique(ys.begin(), ys.end()), ys.end());

    using Rectangle = std::array<std::int32_t, 4>; // x0, z0, x1, z1
    std::map<Rectangle, std::int32_t> open;        // rectangle -> y where its box starts
    std::vector<Box> result;
    const auto close = [&result](const Rectangle& r, const std::int32_t y0, const std::int32_t y1) {
        result.push_back({{r[0], y0, r[1]}, {r[2], y1, r[3]}});
    };
    for (std::size_t s = 0; s + 1 < ys.size(); ++s) {
        const auto y0 = ys[s];
        const auto y1 = ys[s + 1];
        std::vector<const Box*> spanning;
        std::vector<std::int32_t> xs, zs;
        for (const auto& box : boxes) {
            if (box.minimum[1] <= y0 && box.maximum[1] >= y1) {
                spanning.push_back(&box);
                xs.insert(xs.end(), {box.minimum[0], box.maximum[0]});
                zs.insert(zs.end(), {box.minimum[2], box.maximum[2]});
            }
        }
        std::sort(xs.begin(), xs.end());
        xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
        std::sort(zs.begin(), zs.end());
        zs.erase(std::unique(zs.begin(), zs.end()), zs.end());
        const auto nx = xs.empty() ? std::size_t{0} : xs.size() - 1;
        const auto nz = zs.empty() ? std::size_t{0} : zs.size() - 1;
        std::vector<char> filled(nx * nz, 0);
        const auto index = [&](const std::size_t x, const std::size_t z) { return z * nx + x; };
        for (const auto* box : spanning) {
            const auto x0 = static_cast<std::size_t>(std::lower_bound(xs.begin(), xs.end(), box->minimum[0]) - xs.begin());
            const auto x1 = static_cast<std::size_t>(std::lower_bound(xs.begin(), xs.end(), box->maximum[0]) - xs.begin());
            const auto z0 = static_cast<std::size_t>(std::lower_bound(zs.begin(), zs.end(), box->minimum[2]) - zs.begin());
            const auto z1 = static_cast<std::size_t>(std::lower_bound(zs.begin(), zs.end(), box->maximum[2]) - zs.begin());
            for (auto z = z0; z < z1; ++z)
                for (auto x = x0; x < x1; ++x) filled[index(x, z)] = 1;
        }
        std::map<Rectangle, std::int32_t> next;
        for (std::size_t z = 0; z < nz; ++z) {
            for (std::size_t x = 0; x < nx; ++x) {
                if (!filled[index(x, z)]) continue;
                auto xEnd = x + 1;
                while (xEnd < nx && filled[index(xEnd, z)]) ++xEnd;
                auto zEnd = z + 1;
                while (zEnd < nz && std::all_of(filled.begin() + static_cast<std::ptrdiff_t>(index(x, zEnd)),
                                                filled.begin() + static_cast<std::ptrdiff_t>(index(xEnd, zEnd)),
                                                [](const char c) { return c != 0; }))
                    ++zEnd;
                for (auto zz = z; zz < zEnd; ++zz)
                    for (auto xx = x; xx < xEnd; ++xx) filled[index(xx, zz)] = 0;
                const Rectangle r{xs[x], zs[z], xs[xEnd], zs[zEnd]};
                const auto continued = open.find(r);
                if (continued != open.end()) {
                    next.emplace(r, continued->second);
                    open.erase(continued);
                } else {
                    next.emplace(r, y0);
                }
            }
        }
        for (const auto& [r, start] : open) close(r, start, y0);
        open = std::move(next);
    }
    if (!ys.empty())
        for (const auto& [r, start] : open) close(r, start, ys.back());
    std::sort(result.begin(), result.end());
    return result;
}

Result<BuildOutput> BuildMesh(const GridDocument& document, const std::span<const std::byte> sourceBytes) {
    const auto turned = TurnShapes(document);

    // Palette entries: the blocks in use, by name.
    std::vector<std::uint32_t> used;
    for (const auto& [at, cells] : document.chunks)
        for (const auto& cell : cells)
            if (cell.block != 0) used.push_back(cell.block - 1u);
    std::sort(used.begin(), used.end());
    used.erase(std::unique(used.begin(), used.end()), used.end());
    std::sort(used.begin(), used.end(), [&](const std::uint32_t a, const std::uint32_t b) {
        return document.blocks[a].name < document.blocks[b].name;
    });
    std::map<std::uint32_t, std::int32_t> paletteOf;
    for (std::size_t i = 0; i < used.size(); ++i) paletteOf[used[i]] = static_cast<std::int32_t>(i);

    std::vector<Quad> quads;
    std::vector<Box> solids;
    BuildStats stats;
    for (const auto& [at, cells] : document.chunks) {
        for (std::int32_t y = 0; y < ChunkSize; ++y)
            for (std::int32_t z = 0; z < ChunkSize; ++z)
                for (std::int32_t x = 0; x < ChunkSize; ++x) {
                    const auto& cell = cells[CellIndex(x, y, z)];
                    if (cell.block == 0) continue;
                    ++stats.cells;
                    const Vec3i position{at[0] * ChunkSize + x, at[1] * ChunkSize + y, at[2] * ChunkSize + z};
                    const auto& block = document.blocks[cell.block - 1u];
                    const auto& shape = turned.at({block.shape, cell.orientation});
                    const auto palette = paletteOf.at(cell.block - 1u);
                    for (const auto& local : shape.boxes) {
                        Box box;
                        for (int axis = 0; axis < 3; ++axis) {
                            box.minimum[axis] = position[axis] * Units + local.minimum[axis];
                            box.maximum[axis] = position[axis] * Units + local.maximum[axis];
                        }
                        solids.push_back(box);
                        for (int dir = 0; dir < 6; ++dir) {
                            const int axis = dir / 2;
                            const bool positive = dir % 2 == 0;
                            const bool onSide = positive ? local.maximum[axis] == Units : local.minimum[axis] == 0;
                            if (onSide) {
                                Vec3i neighbour = position;
                                neighbour[axis] += positive ? 1 : -1;
                                const auto other = document.At(neighbour);
                                if (other.block != 0) {
                                    const auto& otherShape =
                                        turned.at({document.blocks[other.block - 1u].shape, other.orientation});
                                    // The neighbour's side that touches this face (cullface).
                                    if (otherShape.coversSide[static_cast<std::size_t>(dir ^ 1)]) continue;
                                }
                            }
                            quads.push_back(FaceQuad(box, dir, palette));
                        }
                    }
                }
    }
    std::sort(quads.begin(), quads.end());
    quads.erase(std::unique(quads.begin(), quads.end()), quads.end());
    const auto colliders = MergeBoxes(solids);
    stats.quads = quads.size();
    stats.colliders = colliders.size();

    std::string out = "{\n  \"schema\": \"" + std::string(Engine::VoxelMesh::VoxelMeshSchema) + "\",\n";
    out += "  \"version\": " + std::to_string(Engine::VoxelMesh::VoxelMeshVersion) + ",\n";
    out += "  \"source_sha256\": \"" + Engine::Base::Sha256(sourceBytes) + "\",\n";
    out += "  \"build_version\": \"" + std::string(BuildVersion) + "\",\n";
    out += "  \"origin\": " + Ints({document.origin[0], document.origin[1], document.origin[2]}) + ",\n";
    out += "  \"cell_size\": " + JsonRules::FloatText(document.cellSize) + ",\n";
    out += "  \"palette\": [";
    for (std::size_t i = 0; i < used.size(); ++i) {
        const auto& block = document.blocks[used[i]];
        out += i ? ",\n" : "\n";
        out += "    { \"name\": " + JsonRules::Json(block.name).dump() + ", \"faces\": [";
        for (std::size_t dir = 0; dir < 6; ++dir) {
            const auto percent = FaceBrightnessPercent[dir];
            if (dir) out += ", ";
            out += Ints({Shade(block.color[0], percent), Shade(block.color[1], percent), Shade(block.color[2], percent),
                         block.color[3]});
        }
        out += "] }";
    }
    out += used.empty() ? "],\n" : "\n  ],\n";
    out += "  \"quads\": [";
    for (std::size_t i = 0; i < quads.size(); ++i) {
        const auto& q = quads[i];
        out += i ? ",\n    " : "\n    ";
        out += Ints({q[0], q[1], q[2], q[3], q[4], q[5], q[6]});
    }
    out += quads.empty() ? "],\n" : "\n  ],\n";
    out += "  \"colliders\": [";
    for (std::size_t i = 0; i < colliders.size(); ++i) {
        const auto& c = colliders[i];
        out += i ? ",\n    " : "\n    ";
        out += "{ \"min\": " + Ints({c.minimum[0], c.minimum[1], c.minimum[2]}) +
               ", \"max\": " + Ints({c.maximum[0], c.maximum[1], c.maximum[2]}) + " }";
    }
    out += colliders.empty() ? "]\n}\n" : "\n  ]\n}\n";

    // One validator (Design Invariant 2): what Engine does not accept is not kept.
    const auto readBack = Engine::VoxelMesh::ParseVoxelMesh(out);
    if (!readBack) {
        return Engine::Base::Err(VoxelMapError::Make(VoxelMapErrorCode::BuildRefused,
                                                     "Engine refuses the baked text: " + readBack.error().message,
                                                     readBack.error().detail));
    }
    return BuildOutput{std::move(out), stats};
}

} // namespace Gyo::Tools::VoxelMapEditor
