#include "gyo/voxel_map_editor/Tools.hpp"

#include "engine/base/Error.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <limits>
#include <set>

namespace Gyo::Tools::VoxelMapEditor {
namespace {

std::vector<Vec3i> Sorted(std::set<Vec3i> cells) { return {cells.begin(), cells.end()}; }

int ClampSize(const int size) { return std::clamp(size, 1, MaxBrushSize); }

void AddStamp(std::set<Vec3i>& out, const Vec3i& centre, int size, const BrushShape shape) {
    size = ClampSize(size);
    // lo..hi on every axis; twice the stamp's centre is 2 * lo + size (in cell units).
    const Vec3i lo{centre[0] - (size - 1) / 2, centre[1] - (size - 1) / 2, centre[2] - (size - 1) / 2};
    const std::int64_t r2 = std::int64_t{size} * size; // (2 * radius)^2
    for (std::int32_t y = 0; y < size; ++y)
        for (std::int32_t z = 0; z < size; ++z)
            for (std::int32_t x = 0; x < size; ++x) {
                // Twice the offset of the cell's centre from the stamp's centre.
                const std::int64_t dx = 2 * x + 1 - size;
                const std::int64_t dy = 2 * y + 1 - size;
                const std::int64_t dz = 2 * z + 1 - size;
                if (shape == BrushShape::Sphere && dx * dx + dy * dy + dz * dz > r2) continue;
                if (shape == BrushShape::Cylinder && dx * dx + dz * dz > r2) continue;
                out.insert({lo[0] + x, lo[1] + y, lo[2] + z});
            }
}

bool ValidValue(const GridDocument& document, const Cell value) {
    return value.block <= document.blocks.size() && value.orientation < OrientationCount &&
           !(value.block == 0 && value.orientation != 0);
}

} // namespace

std::vector<Vec3i> BrushCells(const Vec3i& centre, const int size, const BrushShape shape) {
    std::set<Vec3i> out;
    AddStamp(out, centre, size, shape);
    return Sorted(std::move(out));
}

std::vector<Vec3i> LineCells(const Vec3i& a, const Vec3i& b) {
    std::vector<Vec3i> out;
    std::array<std::int64_t, 3> d{};
    std::array<int, 3> s{};
    int major = 0;
    for (int axis = 0; axis < 3; ++axis) {
        const std::int64_t delta = std::int64_t{b[axis]} - a[axis];
        d[axis] = std::llabs(delta);
        s[axis] = delta > 0 ? 1 : (delta < 0 ? -1 : 0);
        if (d[axis] > d[major]) major = axis;
    }
    Vec3i p = a;
    out.push_back(p);
    std::array<std::int64_t, 3> error{};
    for (int axis = 0; axis < 3; ++axis) error[axis] = 2 * d[axis] - d[major];
    for (std::int64_t i = 0; i < d[major]; ++i) {
        for (int axis = 0; axis < 3; ++axis) {
            if (axis == major) continue;
            if (error[axis] > 0) {
                p[axis] += s[axis];
                error[axis] -= 2 * d[major];
            }
            error[axis] += 2 * d[axis];
        }
        p[major] += s[major];
        out.push_back(p);
    }
    return out;
}

std::vector<Vec3i> StrokeCells(const Vec3i& from, const Vec3i& to, const int size, const BrushShape shape) {
    std::set<Vec3i> out;
    for (const auto& centre : LineCells(from, to)) AddStamp(out, centre, size, shape);
    return Sorted(std::move(out));
}

std::vector<Vec3i> ShapeCells(const Vec3i& corner, const Vec3i& other, const BrushShape shape) {
    Vec3i lo{};
    Vec3i hi{};
    std::int64_t count = 1;
    for (int axis = 0; axis < 3; ++axis) {
        lo[axis] = std::min(corner[axis], other[axis]);
        hi[axis] = std::max(corner[axis], other[axis]);
        const std::int64_t side = std::int64_t{hi[axis]} - lo[axis] + 1;
        // Checked side by side, so the product cannot overflow.
        if (side > MaxBoxCells || count * side > MaxBoxCells) return {};
        count *= side;
    }
    std::vector<Vec3i> out;
    // Inside the inscribed shape: the cell's centre, scaled to the box, within the unit circle or sphere.
    // In integers: (2 * (x - lo) + 1 - n)^2 / n^2 summed is at most 1.
    const auto term = [&](const int axis, const std::int32_t v, std::int64_t& numerator, std::int64_t& denominator) {
        const std::int64_t n = std::int64_t{hi[axis]} - lo[axis] + 1;
        const std::int64_t d = 2 * (std::int64_t{v} - lo[axis]) + 1 - n;
        numerator = d * d;
        denominator = n * n;
    };
    for (std::int32_t y = lo[1]; y <= hi[1]; ++y)
        for (std::int32_t z = lo[2]; z <= hi[2]; ++z)
            for (std::int32_t x = lo[0]; x <= hi[0]; ++x) {
                if (shape != BrushShape::Cube) {
                    std::int64_t nx{}, dx{}, ny{}, dy{}, nz{}, dz{};
                    term(0, x, nx, dx);
                    term(1, y, ny, dy);
                    term(2, z, nz, dz);
                    // nx/dx + nz/dz (+ ny/dy) <= 1 in integers: the box has at most 2^20 cells, so the
                    // product of the squared sides, and each term below it, stays within 2^40.
                    const bool inside =
                        shape == BrushShape::Sphere ? nx * dy * dz + ny * dx * dz + nz * dx * dy <= dx * dy * dz
                                                    : nx * dz + nz * dx <= dx * dz;
                    if (!inside) continue;
                }
                out.push_back({x, y, z});
            }
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<Vec3i> LaserCells(const GridDocument& document, const Ray& ray, const int size, const std::int32_t maxCells) {
    std::set<Vec3i> out;
    const double cellSize = document.cellSize;
    const std::array<double, 3> origin{ray.origin.x / cellSize - document.origin[0],
                                       ray.origin.y / cellSize - document.origin[1],
                                       ray.origin.z / cellSize - document.origin[2]};
    const std::array<double, 3> direction{ray.direction.x, ray.direction.y, ray.direction.z};
    Vec3i cell{};
    std::array<int, 3> step{};
    std::array<double, 3> next{};
    std::array<double, 3> delta{};
    constexpr double infinity = std::numeric_limits<double>::infinity();
    for (int axis = 0; axis < 3; ++axis) {
        const double start = std::floor(origin[axis]);
        if (!std::isfinite(direction[axis]) || !std::isfinite(start) || start < -(1 << 30) || start > (1 << 30))
            return {};
        cell[axis] = static_cast<std::int32_t>(start);
        if (direction[axis] > 0.0) {
            step[axis] = 1;
            delta[axis] = 1.0 / direction[axis];
            next[axis] = (start + 1.0 - origin[axis]) * delta[axis];
        } else if (direction[axis] < 0.0) {
            step[axis] = -1;
            delta[axis] = -1.0 / direction[axis];
            next[axis] = (origin[axis] - start) * delta[axis];
        } else {
            delta[axis] = infinity;
            next[axis] = infinity;
        }
    }
    for (std::int32_t walked = 0; walked <= maxCells; ++walked) {
        if (CellInRange(document, cell)) AddStamp(out, cell, size, BrushShape::Cube);
        // The same bound as a box: at most MaxBoxCells cells.
        if (out.size() > static_cast<std::size_t>(MaxBoxCells)) break;
        int axis = 0;
        if (next[1] < next[axis]) axis = 1;
        if (next[2] < next[axis]) axis = 2;
        if (next[axis] == infinity) break;
        cell[axis] += step[axis];
        next[axis] += delta[axis];
    }
    return Sorted(std::move(out));
}

std::vector<Vec3i> ExtrudeFace(const GridDocument& document, const Vec3i& cell, const Vec3i& normal) {
    int axis = -1;
    for (int a = 0; a < 3; ++a)
        if (normal[a] != 0) axis = a;
    if (axis < 0 || std::abs(normal[0]) + std::abs(normal[1]) + std::abs(normal[2]) != 1) return {};
    const auto exposed = [&](const Vec3i& c) {
        if (!CellInRange(document, c) || document.At(c).block == 0) return false;
        const Vec3i beside{c[0] + normal[0], c[1] + normal[1], c[2] + normal[2]};
        return !CellInRange(document, beside) || document.At(beside).block == 0;
    };
    if (!exposed(cell)) return {};
    std::set<Vec3i> seen{cell};
    std::deque<Vec3i> open{cell};
    while (!open.empty() && seen.size() < static_cast<std::size_t>(MaxBoxCells)) {
        const Vec3i c = open.front();
        open.pop_front();
        for (int a = 0; a < 3; ++a) {
            if (a == axis) continue;
            for (const int sign : {-1, 1}) {
                Vec3i n = c;
                n[a] += sign;
                if (seen.contains(n) || !exposed(n)) continue;
                seen.insert(n);
                open.push_back(n);
            }
        }
    }
    return Sorted(std::move(seen));
}

Result<void> RedrawStroke(EditSession& session, const std::span<const Vec3i> cells, const EditMode mode,
                          const Cell value) {
    session.CancelStroke();
    session.BeginStroke();
    return ApplyCells(session, cells, mode, value);
}

Result<void> ApplyCells(EditSession& session, const std::span<const Vec3i> cells, const EditMode mode, const Cell value) {
    if (!ValidValue(session.Document(), value) || (mode != EditMode::Sub && value.block == 0))
        return Engine::Base::Err(
            VoxelMapError::Make(VoxelMapErrorCode::InvalidGrid, "the cell value is not one the grid can hold"));
    const bool own = !session.InStroke();
    if (own) session.BeginStroke();
    for (const auto& cell : cells) {
        if (!CellInRange(session.Document(), cell)) continue;
        switch (mode) {
        case EditMode::Add: static_cast<void>(session.SetCell(cell, value)); break;
        case EditMode::Sub: static_cast<void>(session.SetCell(cell, Cell{})); break;
        case EditMode::Paint: static_cast<void>(session.PaintCell(cell, value.block)); break;
        }
    }
    if (own) session.EndStroke();
    return {};
}

Result<void> Extrude(EditSession& session, const std::span<const Vec3i> face, const Vec3i& normal, const int distance) {
    if (std::abs(normal[0]) + std::abs(normal[1]) + std::abs(normal[2]) != 1)
        return Engine::Base::Err(VoxelMapError::Make(VoxelMapErrorCode::InvalidGrid, "extrude takes a unit normal"));
    if (distance == 0 || face.empty()) return {};
    // The same bound as a box: at most MaxBoxCells cells change.
    if (static_cast<std::int64_t>(face.size()) * std::abs(distance) > MaxBoxCells)
        return Engine::Base::Err(VoxelMapError::Make(VoxelMapErrorCode::InvalidGrid, "an extrude changes at most 2^20 cells"));
    const bool own = !session.InStroke();
    if (own) session.BeginStroke();
    // Read every source first: the edit must not see its own writes.
    std::vector<std::pair<Vec3i, Cell>> sources;
    for (const auto& cell : face) sources.emplace_back(cell, session.Document().At(cell));
    for (const auto& [cell, value] : sources) {
        if (distance > 0) {
            for (int k = 1; k <= distance; ++k) {
                const Vec3i to{cell[0] + normal[0] * k, cell[1] + normal[1] * k, cell[2] + normal[2] * k};
                if (CellInRange(session.Document(), to)) static_cast<void>(session.SetCell(to, value));
            }
        } else {
            for (int k = 0; k < -distance; ++k) {
                const Vec3i to{cell[0] - normal[0] * k, cell[1] - normal[1] * k, cell[2] - normal[2] * k};
                if (CellInRange(session.Document(), to)) static_cast<void>(session.SetCell(to, Cell{}));
            }
        }
    }
    if (own) session.EndStroke();
    return {};
}

std::optional<EditPlane> PlaneThrough(const PickHit& hit) noexcept {
    if (hit.onPlane) return std::nullopt;
    for (int axis = 0; axis < 3; ++axis)
        if (hit.normal[axis] != 0) return EditPlane{axis, hit.cell[axis] + hit.normal[axis]};
    return std::nullopt;
}

} // namespace Gyo::Tools::VoxelMapEditor
