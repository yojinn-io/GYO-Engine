#include "gyo/voxel_map_editor/Editing.hpp"

#include "engine/base/Error.hpp"
#include "engine/math/linear/Vec2.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <span>
#include <utility>

namespace Gyo::Tools::VoxelMapEditor {
namespace {

namespace Math = Engine::Math;

Engine::Base::Err<VoxelMapError> Refuse(const VoxelMapErrorCode code, std::string message) {
    return Engine::Base::Err(VoxelMapError::Make(code, std::move(message)));
}

std::string CellText(const Vec3i& cell) {
    return "(" + std::to_string(cell[0]) + ", " + std::to_string(cell[1]) + ", " + std::to_string(cell[2]) + ")";
}

bool AnyCellUses(const GridDocument& document, const std::uint16_t block) {
    for (const auto& [at, chunk] : document.chunks)
        for (const auto& cell : chunk)
            if (cell.block == block) return true;
    return false;
}

// The grid format decides what a document may hold (one validator): an edited
// palette or shape library is kept only when its text reads back.
Result<void> CheckReadsBack(const GridDocument& document) {
    auto parsed = ParseGrid(WriteGrid(document));
    if (!parsed) return Engine::Base::Err(std::move(parsed).error());
    return {};
}

// floor(value) as a cell coordinate; nullopt when it is not finite or not an int32.
std::optional<std::int32_t> FloorCell(const double value) {
    const double floored = std::floor(value);
    if (!std::isfinite(floored) || floored < -2147483648.0 || floored > 2147483647.0) return std::nullopt;
    return static_cast<std::int32_t>(floored);
}

Math::Vec3 Unproject(const Math::Matrix4& inverse, const float x, const float y, const float z) {
    const auto& m = inverse.values;
    const float px = m[0] * x + m[4] * y + m[8] * z + m[12];
    const float py = m[1] * x + m[5] * y + m[9] * z + m[13];
    const float pz = m[2] * x + m[6] * y + m[10] * z + m[14];
    const float pw = m[3] * x + m[7] * y + m[11] * z + m[15];
    return {px / pw, py / pw, pz / pw};
}

} // namespace

GridDocument NewGrid() {
    GridDocument document;
    document.shapes = DefaultShapes();
    document.blocks = {{"block", 0, {160, 160, 160, 255}}};
    return document;
}

bool CellInRange(const GridDocument& document, const Vec3i& cell) noexcept {
    for (int axis = 0; axis < 3; ++axis) {
        const std::int64_t at = std::int64_t{cell[axis]} + document.origin[axis];
        if (at < -CellLimit || at > CellLimit - 1) return false;
    }
    return true;
}

EditSession::EditSession(GridDocument document) : document_(std::move(document)) {}

bool EditSession::Modified() const noexcept {
    return !savedPosition_ || *savedPosition_ != position_ || (stroke_ && !stroke_->changes.empty());
}

void EditSession::MarkSaved() {
    EndStroke();
    savedPosition_ = position_;
}

void EditSession::BeginStroke() {
    if (!stroke_) stroke_.emplace();
}

void EditSession::EndStroke() {
    if (!stroke_) return;
    auto step = std::move(*stroke_);
    stroke_.reset();
    // A cell set and set back in one drag is no change.
    std::erase_if(step.changes, [](const CellChange& change) { return change.before == change.after; });
    step.index.clear();
    if (!step.changes.empty()) Push(std::move(step));
}

void EditSession::Push(Step step) {
    steps_.resize(position_);
    if (savedPosition_ && *savedPosition_ > position_) savedPosition_.reset();
    steps_.push_back(std::move(step));
    position_ = steps_.size();
}

Result<void> EditSession::ApplyCell(const Vec3i& cell, Cell value) {
    if (!CellInRange(document_, cell))
        return Refuse(VoxelMapErrorCode::InvalidGrid, "cell " + CellText(cell) + " lies beyond +-2^16 cells");
    if (value.block > document_.blocks.size())
        return Refuse(VoxelMapErrorCode::InvalidGrid, "block " + std::to_string(value.block) + " does not exist");
    if (value.orientation >= OrientationCount)
        return Refuse(VoxelMapErrorCode::InvalidGrid, "orientation is 0 to 23");
    if (value.block == 0 && value.orientation != 0)
        return Refuse(VoxelMapErrorCode::InvalidGrid, "an empty cell has orientation 0");
    const Cell before = document_.At(cell);
    if (before == value) return {};
    const bool ownStroke = !stroke_;
    if (ownStroke) BeginStroke();
    auto& changes = stroke_->changes;
    if (const auto earlier = stroke_->index.find(cell); earlier != stroke_->index.end()) {
        changes[earlier->second].after = value;
    } else {
        stroke_->index.emplace(cell, changes.size());
        changes.push_back({cell, before, value});
    }
    document_.Set(cell, value);
    ++revision_;
    if (ownStroke) EndStroke();
    return {};
}

Result<void> EditSession::SetCell(const Vec3i& cell, const Cell value) { return ApplyCell(cell, value); }

Result<void> EditSession::PaintCell(const Vec3i& cell, const std::uint16_t block) {
    if (block == 0) return Refuse(VoxelMapErrorCode::InvalidGrid, "paint takes a block, not empty");
    if (!CellInRange(document_, cell))
        return Refuse(VoxelMapErrorCode::InvalidGrid, "cell " + CellText(cell) + " lies beyond +-2^16 cells");
    const Cell current = document_.At(cell);
    if (current.block == 0) return {};
    return ApplyCell(cell, {block, current.orientation});
}

Result<void> EditSession::FillBox(const Vec3i& corner, const Vec3i& other, const Cell value) {
    Vec3i low{};
    Vec3i high{};
    for (int axis = 0; axis < 3; ++axis) {
        low[axis] = std::min(corner[axis], other[axis]);
        high[axis] = std::max(corner[axis], other[axis]);
    }
    if (!CellInRange(document_, low) || !CellInRange(document_, high))
        return Refuse(VoxelMapErrorCode::InvalidGrid, "the box lies beyond +-2^16 cells");
    std::int64_t cells = 1;
    for (int axis = 0; axis < 3; ++axis) cells *= std::int64_t{high[axis]} - low[axis] + 1;
    if (cells > MaxBoxCells)
        return Refuse(VoxelMapErrorCode::InvalidGrid, "a box changes at most 2^20 cells");
    // Checked once, so a refused value changes no cell.
    if (value.block > document_.blocks.size() || value.orientation >= OrientationCount ||
        (value.block == 0 && value.orientation != 0))
        return Refuse(VoxelMapErrorCode::InvalidGrid, "the cell value is not one the grid can hold");
    const bool ownStroke = !stroke_;
    if (ownStroke) BeginStroke();
    for (std::int32_t y = low[1]; y <= high[1]; ++y)
        for (std::int32_t z = low[2]; z <= high[2]; ++z)
            for (std::int32_t x = low[0]; x <= high[0]; ++x) (void)ApplyCell({x, y, z}, value);
    if (ownStroke) EndStroke();
    return {};
}

Result<void> EditSession::ApplyDocument(GridDocument next) {
    if (auto checked = CheckReadsBack(next); !checked) return checked;
    EndStroke();
    DocumentStep step{document_, next};
    document_ = std::move(next);
    ++revision_;
    Push(std::move(step));
    return {};
}

Result<void> EditSession::AddBlock(Block block) {
    if (document_.blocks.size() >= MaxBlocks) return Refuse(VoxelMapErrorCode::InvalidGrid, "blocks lists at most 256");
    auto next = document_;
    next.blocks.push_back(std::move(block));
    return ApplyDocument(std::move(next));
}

Result<void> EditSession::UpdateBlock(const std::size_t index, Block block) {
    if (index >= document_.blocks.size()) return Refuse(VoxelMapErrorCode::InvalidGrid, "no such block");
    auto next = document_;
    next.blocks[index] = std::move(block);
    return ApplyDocument(std::move(next));
}

Result<void> EditSession::RemoveBlock(const std::size_t index) {
    if (index >= document_.blocks.size()) return Refuse(VoxelMapErrorCode::InvalidGrid, "no such block");
    const auto removed = static_cast<std::uint16_t>(index + 1);
    if (AnyCellUses(document_, removed))
        return Refuse(VoxelMapErrorCode::InvalidGrid, "block '" + document_.blocks[index].name + "' is in use");
    auto next = document_;
    next.blocks.erase(next.blocks.begin() + static_cast<std::ptrdiff_t>(index));
    // Later blocks move down by one.
    for (auto& [at, chunk] : next.chunks)
        for (auto& cell : chunk)
            if (cell.block > removed) --cell.block;
    return ApplyDocument(std::move(next));
}

Result<void> EditSession::AddShape(Shape shape) {
    if (document_.shapes.size() >= MaxShapes) return Refuse(VoxelMapErrorCode::InvalidGrid, "shapes lists at most 256");
    auto next = document_;
    next.shapes.push_back(std::move(shape));
    return ApplyDocument(std::move(next));
}

Result<void> EditSession::UpdateShape(const std::size_t index, Shape shape) {
    if (index >= document_.shapes.size()) return Refuse(VoxelMapErrorCode::InvalidGrid, "no such shape");
    auto next = document_;
    next.shapes[index] = std::move(shape);
    return ApplyDocument(std::move(next));
}

Result<void> EditSession::RemoveShape(const std::size_t index) {
    if (index >= document_.shapes.size()) return Refuse(VoxelMapErrorCode::InvalidGrid, "no such shape");
    for (const auto& block : document_.blocks)
        if (block.shape == index)
            return Refuse(VoxelMapErrorCode::InvalidGrid, "shape '" + document_.shapes[index].name + "' is in use");
    auto next = document_;
    next.shapes.erase(next.shapes.begin() + static_cast<std::ptrdiff_t>(index));
    for (auto& block : next.blocks)
        if (block.shape > index) --block.shape;
    return ApplyDocument(std::move(next));
}

bool EditSession::CanUndo() const noexcept { return position_ > 0 || (stroke_ && !stroke_->changes.empty()); }
bool EditSession::CanRedo() const noexcept { return !stroke_ && position_ < steps_.size(); }

bool EditSession::Undo() {
    EndStroke();
    if (position_ == 0) return false;
    const auto& step = steps_[--position_];
    if (const auto* cells = std::get_if<CellStep>(&step)) {
        for (auto change = cells->changes.rbegin(); change != cells->changes.rend(); ++change)
            document_.Set(change->cell, change->before);
    } else {
        document_ = std::get<DocumentStep>(step).before;
    }
    ++revision_;
    return true;
}

bool EditSession::Redo() {
    if (stroke_ || position_ >= steps_.size()) return false;
    const auto& step = steps_[position_++];
    if (const auto* cells = std::get_if<CellStep>(&step)) {
        for (const auto& change : cells->changes) document_.Set(change.cell, change.after);
    } else {
        document_ = std::get<DocumentStep>(step).after;
    }
    ++revision_;
    return true;
}

Math::Matrix4 ViewProjection(const CameraPose& camera, const float aspectRatio) noexcept {
    // The same composition as Engine's Renderer (engine/render/src/Renderer.cpp,
    // ViewMatrix and ProjectionMatrix); W4 checks it against a drawn frame.
    Math::Matrix4 view = Math::MakeTranslation(-camera.position);
    view = Math::Multiply(Math::MakeRotationZ(-camera.rotationRadians.z), view);
    view = Math::Multiply(Math::MakeRotationY(-camera.rotationRadians.y), view);
    view = Math::Multiply(Math::MakeRotationX(-camera.rotationRadians.x), view);
    const auto projection = Math::MakePerspective(camera.verticalFieldOfViewRadians, aspectRatio, camera.nearClip,
                                                  camera.farClip);
    return Math::Multiply(projection, view);
}

Ray PixelRay(const CameraPose& camera, const int width, const int height, const float pixelX,
             const float pixelY) noexcept {
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const auto inverse = Math::Inverse(ViewProjection(camera, aspect)).value_or(Math::Matrix4{});
    const float x = 2.0F * (pixelX + 0.5F) / static_cast<float>(width) - 1.0F;
    const float y = 1.0F - 2.0F * (pixelY + 0.5F) / static_cast<float>(height);
    const auto nearPoint = Unproject(inverse, x, y, 0.0F);
    const auto farPoint = Unproject(inverse, x, y, 1.0F);
    return {nearPoint, Math::NormalizeOrZero(farPoint - nearPoint)};
}

std::optional<Math::Vec2> ProjectToPixel(const CameraPose& camera, const int width, const int height,
                                         const Math::Vec3 world) noexcept {
    const auto m = ViewProjection(camera, static_cast<float>(width) / static_cast<float>(height)).values;
    const float x = m[0] * world.x + m[4] * world.y + m[8] * world.z + m[12];
    const float y = m[1] * world.x + m[5] * world.y + m[9] * world.z + m[13];
    const float w = m[3] * world.x + m[7] * world.y + m[11] * world.z + m[15];
    if (!(w > 0.0F)) return std::nullopt;
    return Math::Vec2{(x / w + 1.0F) * 0.5F * static_cast<float>(width) - 0.5F,
                      (1.0F - y / w) * 0.5F * static_cast<float>(height) - 0.5F};
}

Math::Vec3 CellCorner(const GridDocument& document, const Vec3i& cell) noexcept {
    return {static_cast<float>(cell[0] + document.origin[0]) * document.cellSize,
            static_cast<float>(cell[1] + document.origin[1]) * document.cellSize,
            static_cast<float>(cell[2] + document.origin[2]) * document.cellSize};
}

std::optional<PickHit> Pick(const GridDocument& document, const Ray& ray, const std::int32_t planeY,
                            const std::int32_t maxCells) noexcept {
    // In relative cell coordinates: world / cell_size - origin.
    const double size = document.cellSize;
    const std::array<double, 3> origin{ray.origin.x / size - document.origin[0], ray.origin.y / size - document.origin[1],
                                       ray.origin.z / size - document.origin[2]};
    const std::array<double, 3> direction{ray.direction.x, ray.direction.y, ray.direction.z};
    Vec3i cell{};
    std::array<int, 3> step{};
    std::array<double, 3> next{};   // ray parameter at the next boundary on each axis
    std::array<double, 3> delta{};  // ray parameter across one cell on each axis
    constexpr double infinity = std::numeric_limits<double>::infinity();
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(direction[axis])) return std::nullopt;
        const auto start = FloorCell(origin[axis]);
        // Far beyond any cell (+-2^16), so the walk of maxCells cannot overflow.
        if (!start || *start < -(1 << 30) || *start > (1 << 30)) return std::nullopt;
        cell[axis] = *start;
        if (direction[axis] > 0.0) {
            step[axis] = 1;
            delta[axis] = 1.0 / direction[axis];
            next[axis] = (static_cast<double>(cell[axis]) + 1.0 - origin[axis]) * delta[axis];
        } else if (direction[axis] < 0.0) {
            step[axis] = -1;
            delta[axis] = -1.0 / direction[axis];
            next[axis] = (origin[axis] - static_cast<double>(cell[axis])) * delta[axis];
        } else {
            delta[axis] = infinity;
            next[axis] = infinity;
        }
    }
    Vec3i normal{};
    for (std::int32_t walked = 0; walked <= maxCells; ++walked) {
        if (CellInRange(document, cell) && document.At(cell).block != 0) return PickHit{cell, normal, false};
        int axis = 0;
        if (next[1] < next[axis]) axis = 1;
        if (next[2] < next[axis]) axis = 2;
        if (next[axis] == infinity) break;
        cell[axis] += step[axis];
        next[axis] += delta[axis];
        normal = {0, 0, 0};
        normal[axis] = -step[axis];
    }
    // No filled cell: the edit plane, the cell layer y in [planeY, planeY + 1).
    // The ray meets its top when coming down onto it, its bottom when coming up.
    if (direction[1] == 0.0) return std::nullopt;
    const double face = direction[1] < 0.0 ? planeY + 1.0 : static_cast<double>(planeY);
    const double t = (face - origin[1]) / direction[1];
    // Not behind the ray, and not beyond maxCells along it (a ray near the horizon meets the plane far away).
    if (!(t >= 0.0) || t > static_cast<double>(maxCells)) return std::nullopt;
    const auto x = FloorCell(origin[0] + t * direction[0]);
    const auto z = FloorCell(origin[2] + t * direction[2]);
    if (!x || !z) return std::nullopt;
    const Vec3i planeCell{*x, planeY, *z};
    if (!CellInRange(document, planeCell)) return std::nullopt;
    return PickHit{planeCell, {0, 0, 0}, true};
}

Vec3i PlacementCell(const PickHit& hit) noexcept {
    if (hit.onPlane) return hit.cell;
    return {hit.cell[0] + hit.normal[0], hit.cell[1] + hit.normal[1], hit.cell[2] + hit.normal[2]};
}

Result<Engine::VoxelMesh::VoxelMeshGeometry> PreviewGeometry(const GridDocument& document) {
    if (document.chunks.empty()) return Engine::VoxelMesh::VoxelMeshGeometry{};
    // The source hash does not reach the geometry, so no grid text is written here.
    auto built = BuildMesh(document, {});
    if (!built) return Engine::Base::Err(std::move(built).error());
    auto parsed = Engine::VoxelMesh::ParseVoxelMesh(built->text);
    if (!parsed) return Refuse(VoxelMapErrorCode::BuildRefused, Engine::Base::Describe(parsed.error()));
    return Engine::VoxelMesh::ExpandVoxelMesh(*parsed);
}

namespace {

constexpr std::size_t kMaxRead = 64U * 1024U * 1024U;

// The file's bytes now, or nullopt when it does not exist.
Result<std::optional<std::vector<std::byte>>> Current(const std::string& path) {
    auto exists = FileExists(path);
    if (!exists) return Engine::Base::Err(std::move(exists).error());
    if (!*exists) return std::optional<std::vector<std::byte>>{};
    auto bytes = ReadFile(path, kMaxRead);
    if (!bytes) return Engine::Base::Err(std::move(bytes).error());
    return std::optional<std::vector<std::byte>>{std::move(*bytes)};
}

std::span<const std::byte> AsBytes(const std::string& text) {
    return std::as_bytes(std::span{text.data(), text.size()});
}

} // namespace

Result<OpenedEditor> OpenEditor(const std::string& gridPath, const std::string& meshPath) {
    OpenedEditor opened;
    opened.files.grid.path = gridPath;
    opened.files.mesh.path = meshPath;
    auto grid = Current(gridPath);
    if (!grid) return Engine::Base::Err(std::move(grid).error());
    auto mesh = Current(meshPath);
    if (!mesh) return Engine::Base::Err(std::move(mesh).error());
    if (*grid) {
        const auto& bytes = **grid;
        auto parsed = ParseGrid({reinterpret_cast<const char*>(bytes.data()), bytes.size()});
        if (!parsed) return Engine::Base::Err(std::move(parsed).error());
        opened.document = std::move(*parsed);
    } else {
        opened.document = NewGrid();
    }
    opened.files.grid.known = std::move(*grid);
    opened.files.mesh.known = std::move(*mesh);
    return opened;
}

Result<void> SaveEditor(EditorFiles& files, const GridDocument& document) {
    const std::string gridText = WriteGrid(document);
    auto built = BuildMesh(document, AsBytes(gridText));
    if (!built) return Engine::Base::Err(std::move(built).error());
    for (const EditorFile* file : {&files.grid, &files.mesh}) {
        auto now = Current(file->path);
        if (!now) return Engine::Base::Err(std::move(now).error());
        if (*now != file->known)
            return Refuse(VoxelMapErrorCode::FileChanged, file->path + " changed since it was opened or saved");
    }
    if (auto written = WriteFileReplacing(files.grid.path, gridText); !written) return written;
    const auto gridBytes = AsBytes(gridText);
    files.grid.known = std::vector<std::byte>(gridBytes.begin(), gridBytes.end());
    if (auto written = WriteFileReplacing(files.mesh.path, built->text); !written) {
        return Refuse(VoxelMapErrorCode::WriteFailed,
                      "the grid was saved but its mesh was not: " + Engine::Base::Describe(written.error()));
    }
    const auto meshBytes = AsBytes(built->text);
    files.mesh.known = std::vector<std::byte>(meshBytes.begin(), meshBytes.end());
    return {};
}

} // namespace Gyo::Tools::VoxelMapEditor
