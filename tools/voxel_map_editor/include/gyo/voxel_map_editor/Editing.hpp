#pragma once

#include "gyo/voxel_map_editor/ChunkedMap.hpp"
#include "gyo/voxel_map_editor/VoxelMap.hpp"

#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/voxel_mesh/VoxelMesh.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

// The editing model of the voxel map editor's window
// (v8-tool-voxel-map-editor-window): edits with undo, picking and saving. No
// ImGui, SDL or render code: the window draws and forwards input only, so all
// of this is tested on the CPU (judgments W1 to W3, W5 to W7).
namespace Gyo::Tools::VoxelMapEditor {

// A grid to start from when the grid file does not exist yet: the default
// shape library and one grey cube block.
[[nodiscard]] GridDocument NewGrid();

// Whether a cell (relative to origin) may hold a block: origin + cell lies in
// [-2^16, 2^16 - 1] on every axis.
[[nodiscard]] bool CellInRange(const GridDocument& document, const Vec3i& cell) noexcept;

// The most cells one box edit may change (a 256 x 16 x 256 box).
inline constexpr std::int64_t MaxBoxCells = std::int64_t{1} << 20;

struct CellChange final {
    Vec3i cell{};
    Cell before{};
    Cell after{};
};

// What changed in a document since it was last asked: the cells a cell edit,
// its undo or its redo set, and whether a palette or shape edit (or its undo
// or redo) replaced the document.
struct DocumentChanges final {
    std::vector<Vec3i> cells;
    bool whole{};
};

// Edits a document. Cell edits between BeginStroke and EndStroke are one undo
// step (one drag); a palette or shape edit is one step of its own. A new edit
// clears what could be redone.
class EditSession final {
public:
    explicit EditSession(GridDocument document);

    [[nodiscard]] const GridDocument& Document() const noexcept { return document_; }
    // Changes since the last MarkSaved (or the start); also true when the
    // saved state cannot be reached by undo or redo any more.
    [[nodiscard]] bool Modified() const noexcept;
    // Ends an open stroke first, so the saved state is the document as written.
    void MarkSaved();
    // Counts every change to the document (edits, undo, redo), so a preview
    // knows when to rebuild.
    [[nodiscard]] std::uint64_t Revision() const noexcept { return revision_; }
    // The changes since the last call (a preview rebakes only what they touch).
    [[nodiscard]] DocumentChanges TakeChanges();

    void BeginStroke();
    void EndStroke();
    // Puts back every cell the open stroke changed; no step is kept (Esc during a drag).
    void CancelStroke();
    [[nodiscard]] bool InStroke() const noexcept { return stroke_.has_value(); }

    // Cell edits; outside a stroke each is a stroke of its own. InvalidGrid
    // (the document unchanged) for a cell out of range, a block that does not
    // exist, an orientation >= 24 or an empty cell with an orientation.
    Result<void> SetCell(const Vec3i& cell, Cell value);
    // Paint: a filled cell takes the block, keeping its orientation; an
    // empty cell stays empty.
    Result<void> PaintCell(const Vec3i& cell, std::uint16_t block);
    // Every cell of the box from one corner to the other (both included); at
    // most MaxBoxCells cells.
    Result<void> FillBox(const Vec3i& corner, const Vec3i& other, Cell value);

    // Palette and shape library; each refused edit leaves the document as it
    // was. The document must still read back through ParseGrid afterwards.
    Result<void> AddBlock(Block block);
    Result<void> UpdateBlock(std::size_t index, Block block);  // blocks[index]
    Result<void> RemoveBlock(std::size_t index);               // refused while a cell uses it
    Result<void> AddShape(Shape shape);
    Result<void> UpdateShape(std::size_t index, Shape shape);
    Result<void> RemoveShape(std::size_t index);               // refused while a block uses it

    [[nodiscard]] bool CanUndo() const noexcept;
    [[nodiscard]] bool CanRedo() const noexcept;
    bool Undo();
    bool Redo();

private:
    struct CellStep final {
        std::vector<CellChange> changes;
        std::map<Vec3i, std::size_t> index;  // cell -> its change, while the stroke is open
    };
    struct DocumentStep final {
        GridDocument before;
        GridDocument after;
    };
    using Step = std::variant<CellStep, DocumentStep>;

    Result<void> ApplyCell(const Vec3i& cell, Cell value);
    Result<void> ApplyDocument(GridDocument next);
    void Push(Step step);

    GridDocument document_;
    std::vector<Step> steps_;
    std::size_t position_{};  // steps_[0, position_) are done
    std::optional<std::size_t> savedPosition_{0};
    std::optional<CellStep> stroke_;
    std::uint64_t revision_{};
    DocumentChanges changes_;
};

// The camera of the preview, in Engine's camera convention
// (Engine::Render::PerspectiveCamera3D): rotation is Z, then Y, then X Euler
// angles, +Z forward. The window fills it from its camera.
struct CameraPose final {
    Engine::Math::Vec3 position{};
    Engine::Math::Vec3 rotationRadians{};
    float verticalFieldOfViewRadians{1.0F};
    float nearClip{0.05F};
    float farClip{100.0F};
};

// Projection * View, as Engine's Renderer composes them.
[[nodiscard]] Engine::Math::Matrix4 ViewProjection(const CameraPose& camera, float aspectRatio) noexcept;

struct Ray final {
    Engine::Math::Vec3 origin{};
    Engine::Math::Vec3 direction{};  // unit length
};

// The ray through a pixel's centre of a width x height image (row 0 on top).
[[nodiscard]] Ray PixelRay(const CameraPose& camera, int width, int height, float pixelX, float pixelY) noexcept;
// Where a world point lands in that image; nullopt behind the camera.
[[nodiscard]] std::optional<Engine::Math::Vec2> ProjectToPixel(const CameraPose& camera, int width, int height,
                                                               Engine::Math::Vec3 world) noexcept;

// World positions of a cell's corner (relative cell coordinates).
[[nodiscard]] Engine::Math::Vec3 CellCorner(const GridDocument& document, const Vec3i& cell) noexcept;

struct PickHit final {
    Vec3i cell{};
    Vec3i normal{};       // the face the ray entered through; zero when it starts inside
    bool onPlane{};       // no filled cell was hit; cell is on the edit plane
    friend bool operator==(const PickHit&, const PickHit&) = default;
};

// The edit plane: the layer of cells whose coordinate on axis (0 x, 1 y, 2 z)
// is at.
struct EditPlane final {
    int axis{1};
    std::int32_t at{};
    friend bool operator==(const EditPlane&, const EditPlane&) = default;
};

// What a pick snaps to (Goxel's snap): the surface of filled cells (volume),
// the edit plane, or both (the first the ray meets).
struct PickOptions final {
    bool volume{true};
    bool plane{true};
};

// Whichever the ray meets first (Goxel's snap): the first filled cell along a
// world ray (DDA; a cell of any shape counts as whole; with volume on), or the
// cell of the edit plane it crosses (with plane on); a filled cell the ray
// enters where it meets the plane (one in the plane's layer) wins. Both within
// maxCells cells.
[[nodiscard]] std::optional<PickHit> Pick(const GridDocument& document, const Ray& ray, const EditPlane& plane,
                                          const PickOptions& options, std::int32_t maxCells = 1024) noexcept;
// The same with the edit plane at relative y == planeY and both snaps on.
[[nodiscard]] std::optional<PickHit> Pick(const GridDocument& document, const Ray& ray, std::int32_t planeY,
                                          std::int32_t maxCells = 1024) noexcept;
// Where placing goes: the plane cell itself, or the cell beside the hit face.
[[nodiscard]] Vec3i PlacementCell(const PickHit& hit) noexcept;

// What the preview draws (Design Invariant 7): the chunks BakeMap and
// RebakeChunks keep (each accepted by Engine's ValidateVoxelChunk), expanded by
// Engine's ExpandVoxelChunk, with ExpandVoxelPalette's image. The same chunks
// are what saving writes. A grid without filled cells has no map and draws
// nothing.
class MapPreview final {
public:
    [[nodiscard]] static Result<MapPreview> Create(const GridDocument& document);

    // Rebakes what the changes touch (everything when whole); on an error the
    // preview is left as it was.
    Result<void> Apply(const GridDocument& document, const DocumentChanges& changes);

    // The baked map, empty when the grid has no filled cell.
    [[nodiscard]] const std::optional<BakedMap>& Map() const noexcept { return map_; }

    // What changed since the last call: every chunk to draw anew (nullopt: no
    // longer drawn), and the palette image when it changed. The first call
    // after Create gives everything.
    struct Changes final {
        std::map<Vec3i, std::optional<Engine::VoxelMesh::VoxelChunkGeometry>> chunks;
        std::optional<Engine::VoxelMesh::VoxelPaletteImage> palette;
    };
    [[nodiscard]] Changes TakeChanges();

private:
    std::optional<BakedMap> map_;
    std::set<Vec3i> dirty_;
    bool paletteDirty_{true};
};

// The files the window edits. A file's bytes as they were when it was opened
// or last saved; nullopt when it did not exist. The map is a folder; its
// map.json stands for it.
struct EditorFile final {
    std::string path;
    std::optional<std::vector<std::byte>> known;
};

struct EditorFiles final {
    EditorFile grid;
    std::string mapDirectory;
    EditorFile mapIndex; // <mapDirectory>/map.json
};

struct OpenedEditor final {
    EditorFiles files;
    GridDocument document;
};

// Reads the grid (NewGrid when the file does not exist) and remembers the
// grid's and the map index's bytes. The window edits the map's geometry only:
// markers belong to the scene editor (decision v8-arch-map-scene-split).
[[nodiscard]] Result<OpenedEditor> OpenEditor(const std::string& gridPath, const std::string& mapDirectory);

enum class SaveMode : std::uint8_t {
    Checked,   // FileChanged, writing nothing, when a file changed since it was opened or saved
    Overwrite, // the user chose to replace what is on disk
};

struct SaveResult final {
    bool mapWritten{}; // false when the grid has no filled cell (a map needs one)
};

// Writes the grid, then the map of those chunks (WriteMap and SaveMap: what
// Engine refuses is not written), each by atomic replace. When the map cannot
// be written after the grid was, the grid's known bytes are updated and the
// error is returned.
[[nodiscard]] Result<SaveResult> SaveEditor(EditorFiles& files, const GridDocument& document,
                                            const MapPreview& preview, SaveMode mode = SaveMode::Checked);

} // namespace Gyo::Tools::VoxelMapEditor
