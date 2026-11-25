#include "voxel_map_editor/Grid.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace voxel_map_editor {

std::vector<Box> MergeBoxes(const std::span<const Box> boxes) {
    // Slab by slab along y. Inside a slab, every column x is cut into its
    // longest runs along z (strips), and a strip goes on along x only while
    // the next column has exactly the same strip. A rectangle that goes on
    // unchanged into the next slab extends its box upwards. So a wall along z
    // keeps its corners and a wall along x stops at them, and the boxes are
    // disjoint with the input's union.
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
        const auto emit = [&](const std::size_t x0, const std::size_t x1, const std::size_t z0, const std::size_t z1) {
            const Rectangle r{xs[x0], zs[z0], xs[x1], zs[z1]};
            const auto continued = open.find(r);
            if (continued != open.end()) {
                next.emplace(r, continued->second);
                open.erase(continued);
            } else {
                next.emplace(r, y0);
            }
        };
        std::map<std::pair<std::size_t, std::size_t>, std::size_t> strips; // (z0, z1) -> first column
        for (std::size_t x = 0; x <= nx; ++x) {
            std::map<std::pair<std::size_t, std::size_t>, std::size_t> column;
            if (x < nx) {
                for (std::size_t z = 0; z < nz;) {
                    if (!filled[index(x, z)]) {
                        ++z;
                        continue;
                    }
                    auto end = z + 1;
                    while (end < nz && filled[index(x, end)]) ++end;
                    const auto strip = std::make_pair(z, end);
                    const auto previous = strips.find(strip);
                    column.emplace(strip, previous != strips.end() ? previous->second : x);
                    z = end;
                }
            }
            // A strip the column does not repeat ends here.
            for (const auto& [strip, first] : strips)
                if (!column.contains(strip)) emit(first, x, strip.first, strip.second);
            strips = std::move(column);
        }
        for (const auto& [r, start] : open) close(r, start, y0);
        open = std::move(next);
    }
    if (!ys.empty())
        for (const auto& [r, start] : open) close(r, start, ys.back());
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace voxel_map_editor
