#pragma once

#include "gyo/voxel_map_editor/Editing.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

// The editing tools of the window (v8-tool-voxel-window-1): Goxel's functions
// and interaction (modes, brush, shape, laser, line, extrude, plane, picker),
// written for this editor's model of blocks, shapes and orientations. Only
// cell sets and their application; no ImGui, so every tool is tested on the
// CPU (judgment W1).
namespace Gyo::Tools::VoxelMapEditor {

// What an edit does to a cell (Goxel's modes): Add sets the cell to the chosen
// block and orientation, filled or not; Sub empties it; Paint gives a filled
// cell the chosen block and keeps its orientation, and leaves empty cells.
enum class EditMode : std::uint8_t { Add, Sub, Paint };

enum class BrushShape : std::uint8_t { Cube, Sphere, Cylinder };

inline constexpr int MaxBrushSize = 16;

// The cells of one brush stamp of size 1 to MaxBrushSize around centre. A cube
// is size^3 cells; a sphere and a (vertical) cylinder are the cells whose
// centres lie within the stamp's radius size / 2 of its centre (both inside
// that cube). An odd size is centred on the cell, an even size on its +X +Y +Z
// corner. Sorted, no repeats.
[[nodiscard]] std::vector<Vec3i> BrushCells(const Vec3i& centre, int size, BrushShape shape);

// A stroke from one stamp to the next: stamps along the straight line between
// the two centres (LineCells), so a fast drag leaves no gap. Sorted, no repeats.
[[nodiscard]] std::vector<Vec3i> StrokeCells(const Vec3i& from, const Vec3i& to, int size, BrushShape shape);

// The cells of a straight line from a to b, both included (3D Bresenham).
[[nodiscard]] std::vector<Vec3i> LineCells(const Vec3i& a, const Vec3i& b);

// The cells of a shape spanning the box from one corner cell to the other (both
// included): the box itself, or the sphere (ellipsoid) or vertical cylinder
// inscribed in it. Sorted, no repeats. Empty when the box has more than
// MaxBoxCells cells.
[[nodiscard]] std::vector<Vec3i> ShapeCells(const Vec3i& corner, const Vec3i& other, BrushShape shape);

// The laser: the cells within the brush (cube of size) around each cell the
// ray walks through, up to maxCells along it, and stopping once it holds more
// than MaxBoxCells cells. Sorted, no repeats.
[[nodiscard]] std::vector<Vec3i> LaserCells(const GridDocument& document, const Ray& ray, int size,
                                            std::int32_t maxCells = 1024);

// The face Extrude drags: from a filled cell and the side its face looks to
// (a unit axis normal), every filled cell in the same layer along that axis,
// connected to it through edges in the layer, whose face on that side is
// uncovered (the cell beside it is empty). At most MaxBoxCells cells; sorted.
[[nodiscard]] std::vector<Vec3i> ExtrudeFace(const GridDocument& document, const Vec3i& cell, const Vec3i& normal);

// Applies a set of cells in a mode, as one stroke when none is open. Value is
// the chosen block and orientation (Add, and the block of Paint). Cells out of
// range are skipped. InvalidGrid for a value the grid cannot hold.
Result<void> ApplyCells(EditSession& session, std::span<const Vec3i> cells, EditMode mode, Cell value);
// A drag that follows the pointer (Line, ve-feel-1 F5): puts back what the open stroke changed and applies the
// cells instead, in a new open stroke, so only the latest cells stay; releasing (EndStroke) keeps them as one
// step, CancelStroke puts them back.
Result<void> RedrawStroke(EditSession& session, std::span<const Vec3i> cells, EditMode mode, Cell value);

// Extrude by distance along normal: outward (distance > 0) each face cell is
// copied distance times beyond its face (block and orientation); inward
// (distance < 0) the face cells and the -distance - 1 layers behind them are
// emptied. One stroke when none is open. InvalidGrid when it would change more
// than MaxBoxCells cells.
Result<void> Extrude(EditSession& session, std::span<const Vec3i> face, const Vec3i& normal, int distance);

// The edit plane through a picked face: the layer of the cell beside the face
// (where a block placed on it would go), on the face's axis. nullopt for a hit
// on the plane itself or from inside a cell.
[[nodiscard]] std::optional<EditPlane> PlaneThrough(const PickHit& hit) noexcept;

} // namespace Gyo::Tools::VoxelMapEditor
