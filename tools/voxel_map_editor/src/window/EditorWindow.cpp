#include "window/EditorWindow.hpp"
#include "window/PreviewScene.hpp"

#include "gyo/voxel_map_editor/Editing.hpp"

#include "gyo/framework/PreviewCamera.hpp"
#include "gyo/framework/PreviewTexture.hpp"
#include "gyo/framework/imgui/ImGuiShell.hpp"

#include "engine/base/Error.hpp"
#include "engine/base/Log.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/render/backend/sdl/SdlRenderer.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

// The window: the editing model (Editing.hpp) under ImGui, and the map on the
// framework's offscreen preview. Everything that changes the grid goes
// through EditSession; this file draws and forwards input.
namespace Gyo::Tools::VoxelMapEditor {
namespace {

namespace Math = Engine::Math;
namespace Sdl = Engine::Render::Backend::Sdl;
namespace Platform = Engine::Platform::Sdl;

enum class Tool : int { Place, Erase, Paint, Pick, BoxFill, BoxClear };
constexpr std::array<const char*, 6> kToolNames{"Place", "Erase", "Paint", "Pick", "Box fill", "Box clear"};
constexpr std::array<const char*, 6> kUpNames{"+Y", "-Y", "+X", "-X", "+Z", "-Z"};

void LogError(const std::string& text) { Engine::Base::Log(Engine::Base::LogLevel::Error, "voxel_map_editor", text); }

// ImGui's text field on a std::string (the buffer grows as the text does).
bool InputString(const char* label, std::string& text) {
    const auto resize = [](ImGuiInputTextCallbackData* data) {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            auto* string = static_cast<std::string*>(data->UserData);
            string->resize(static_cast<std::size_t>(data->BufTextLen));
            data->Buf = string->data();
        }
        return 0;
    };
    return ImGui::InputText(label, text.data(), text.capacity() + 1, ImGuiInputTextFlags_CallbackResize, resize, &text);
}

ImU32 ColorOf(const std::array<std::uint8_t, 4>& rgba) { return IM_COL32(rgba[0], rgba[1], rgba[2], rgba[3]); }

CameraPose PoseOf(const Engine::Render::PerspectiveCamera3D& camera) {
    return {camera.position, camera.rotationRadians, camera.verticalFieldOfViewRadians, camera.nearClip,
            camera.farClip};
}

// The filled cells' bounds; nullopt for a grid without cells.
std::optional<std::pair<Vec3i, Vec3i>> Bounds(const GridDocument& document) {
    std::optional<std::pair<Vec3i, Vec3i>> bounds;
    for (const auto& [at, chunk] : document.chunks) {
        Vec3i low{};
        Vec3i high{};
        for (int axis = 0; axis < 3; ++axis) {
            low[axis] = at[axis] * ChunkSize;
            high[axis] = low[axis] + ChunkSize;
        }
        if (!bounds) {
            bounds = {low, high};
            continue;
        }
        for (int axis = 0; axis < 3; ++axis) {
            bounds->first[axis] = std::min(bounds->first[axis], low[axis]);
            bounds->second[axis] = std::max(bounds->second[axis], high[axis]);
        }
    }
    return bounds;
}

class EditorClient final : public Framework::ImGuiShellClient {
public:
    EditorClient(const Framework::ImGuiShellContext& context, WindowOptions options)
        : platform_(context.platform), renderer_(context.renderer), options_(std::move(options)) {}

    ~EditorClient() override {
        if (texture_.handle) static_cast<void>(renderer_.ReleaseTexture(texture_.handle));
        if (session_ && session_->Modified())
            Engine::Base::Log(Engine::Base::LogLevel::Warning, "voxel_map_editor",
                              "the window closed with unsaved edits of " + options_.gridPath);
    }

    [[nodiscard]] bool Initialize(const Framework::ImGuiShellContext&) override {
        auto opened = OpenEditor(options_.gridPath, options_.meshPath);
        if (!opened) {
            error_ = Engine::Base::Describe(opened.error());
            LogError(error_);
        } else {
            files_ = std::move(opened->files);
            session_.emplace(std::move(opened->document));
            status_ = files_.grid.known ? "opened " + options_.gridPath : "new grid: " + options_.gridPath;
            FrameCamera();
            LoadDrafts();
        }
        // Engine's builtin shaders are built next to the executable (shaders/builtin).
        if (auto base = Platform::BaseDirectory(); !base) {
            previewError_ = "no base directory: " + Engine::Base::Describe(base.error());
        } else if (auto scene = PreviewScene::Create(platform_, base.value() / "shaders" / "builtin", options_.gpuDriver);
                   !scene) {
            previewError_ = Engine::Base::Describe(scene.error());
            LogError(previewError_);
        } else {
            scene_ = std::move(scene.value());
        }
        return true;
    }

    void Draw() override {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("Voxel Map Editor", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        if (!session_) {
            ImGui::TextColored(ImVec4(1, 0.4F, 0.4F, 1), "%s", error_.c_str());
            ImGui::End();
            return;
        }
        Shortcuts();
        if (draftRevision_ != Session().Revision()) ReconcileDrafts();
        ImGui::BeginChild("side", ImVec2(380, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
        DrawFile();
        DrawTools();
        DrawBlocks();
        DrawShapes();
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("view", ImVec2(0, 0));
        DrawPreview();
        ImGui::EndChild();
        DrawClosePrompt();
        ImGui::End();
    }

    [[nodiscard]] bool WantsExit() const override { return wantsExit_; }

    // Unsaved edits: the window asks in the next frame.
    [[nodiscard]] bool CloseRequested() override {
        if (!session_ || !session_->Modified()) return true;
        openClose_ = true;
        return false;
    }

private:
    EditSession& Session() { return *session_; }
    const GridDocument& Document() const { return session_->Document(); }

    void Report(const VoxelMapError& error) {
        error_ = Engine::Base::Describe(error);
        status_.clear();
    }

    void Apply(const Result<void>& result) {
        if (!result) Report(result.error());
        else error_.clear();
    }

    void FrameCamera() {
        const auto& document = Document();
        const float size = document.cellSize;
        Math::Vec3 low{0, 0, 0};
        Math::Vec3 high{16, 4, 16};
        if (const auto bounds = Bounds(document)) {
            low = {static_cast<float>(bounds->first[0]), static_cast<float>(bounds->first[1]),
                   static_cast<float>(bounds->first[2])};
            high = {static_cast<float>(bounds->second[0]), static_cast<float>(bounds->second[1]),
                    static_cast<float>(bounds->second[2])};
        }
        const Math::Vec3 origin{static_cast<float>(document.origin[0]), static_cast<float>(document.origin[1]),
                                static_cast<float>(document.origin[2])};
        camera_.target = (low + high) * 0.5F * size + origin * size;
        camera_.yawRadians = 0.6F;
        camera_.pitchRadians = 0.7F;
        camera_.distance = std::max(Math::Length(high - low) * size * 0.9F, 4.0F * size);
    }

    void Save() {
        if (auto saved = SaveEditor(files_, Document()); !saved) {
            Report(saved.error());
            return;
        }
        Session().MarkSaved();
        error_.clear();
        status_ = "saved " + files_.grid.path + " and " + files_.mesh.path;
    }

    void Shortcuts() {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput || promptOpen_) return;
        const bool command = io.KeyCtrl || io.KeySuper;
        if (command && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            if (io.KeyShift) static_cast<void>(Session().Redo());
            else static_cast<void>(Session().Undo());
        }
        if (command && ImGui::IsKeyPressed(ImGuiKey_Y, false)) static_cast<void>(Session().Redo());
        if (command && ImGui::IsKeyPressed(ImGuiKey_S, false)) Save();
        if (command) return;
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) ++planeY_;
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) --planeY_;
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
            // R turns about the up axis; Shift+R chooses the next up axis.
            orientation_ = io.KeyShift ? (orientation_ + 4) % 24 : orientation_ / 4 * 4 + (orientation_ % 4 + 1) % 4;
        }
        for (int key = 0; key < static_cast<int>(kToolNames.size()); ++key)
            if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + key), false)) tool_ = static_cast<Tool>(key);
    }

    void DrawFile() {
        ImGui::SeparatorText("File");
        ImGui::TextWrapped("grid: %s", files_.grid.path.c_str());
        ImGui::TextWrapped("mesh: %s", files_.mesh.path.c_str());
        if (ImGui::Button("Save (Ctrl+S)")) Save();
        ImGui::SameLine();
        ImGui::BeginDisabled(!Session().CanUndo());
        if (ImGui::Button("Undo")) static_cast<void>(Session().Undo());
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!Session().CanRedo());
        if (ImGui::Button("Redo")) static_cast<void>(Session().Redo());
        ImGui::EndDisabled();
        if (Session().Modified()) ImGui::TextColored(ImVec4(1, 0.8F, 0.3F, 1), "* unsaved");
        if (!status_.empty()) ImGui::TextWrapped("%s", status_.c_str());
        if (!error_.empty()) ImGui::TextColored(ImVec4(1, 0.4F, 0.4F, 1), "%s", error_.c_str());
    }

    void DrawTools() {
        ImGui::SeparatorText("Tool (1-6)");
        for (std::size_t i = 0; i < kToolNames.size(); ++i) {
            if (i % 3 != 0) ImGui::SameLine();
            if (ImGui::RadioButton(kToolNames[i], tool_ == static_cast<Tool>(i))) tool_ = static_cast<Tool>(i);
        }
        ImGui::InputInt("plane y (PgUp/PgDn)", &planeY_);
        planeY_ = std::clamp(planeY_, -CellLimit, CellLimit - 1);
        if (tool_ == Tool::BoxFill || tool_ == Tool::BoxClear) {
            ImGui::InputInt("box height", &boxHeight_);
            boxHeight_ = std::clamp(boxHeight_, 1, 256);
        }
        ImGui::SliderInt("orientation (R, Shift+R)", &orientation_, 0, 23);
        ImGui::Text("up %s, %d quarter turn(s)", kUpNames[static_cast<std::size_t>(orientation_ / 4)], orientation_ % 4);
        ImGui::TextDisabled("left: tool  right drag: orbit  middle drag: pan  wheel: zoom");
    }

    // After a change: indices back in range (an undone "Add shape" leaves one
    // past the end), and a draft reloaded only when what it was loaded from
    // changed, so an unapplied draft survives painting.
    void ReconcileDrafts() {
        const auto& document = Document();
        const bool blockGone = block_ == 0 || block_ > document.blocks.size() ||
                               !(document.blocks[block_ - 1].name == blockSource_.name &&
                                 document.blocks[block_ - 1].shape == blockSource_.shape &&
                                 document.blocks[block_ - 1].color == blockSource_.color);
        const bool shapeGone = shape_ >= document.shapes.size() ||
                               !(document.shapes[shape_].name == shapeSource_.name &&
                                 document.shapes[shape_].boxes == shapeSource_.boxes);
        if (blockGone || shapeGone) LoadDrafts();
        draftRevision_ = Session().Revision();
    }

    // The fields of the selected block and shape, applied as one edit each.
    void LoadDrafts() {
        const auto& document = Document();
        if (block_ == 0 || block_ > document.blocks.size()) block_ = document.blocks.empty() ? 0 : 1;
        if (block_ != 0) {
            blockDraft_ = blockSource_ = document.blocks[block_ - 1];
            for (std::size_t c = 0; c < 4; ++c) blockColor_[c] = static_cast<float>(blockDraft_.color[c]) / 255.0F;
        }
        if (shape_ >= document.shapes.size()) shape_ = 0;
        if (!document.shapes.empty()) shapeDraft_ = shapeSource_ = document.shapes[shape_];
        draftRevision_ = Session().Revision();
    }

    void DrawBlocks() {
        const auto& document = Document();
        ImGui::SeparatorText("Blocks");
        ImGui::BeginChild("blocks", ImVec2(0, 150), ImGuiChildFlags_Borders);
        for (std::size_t i = 0; i < document.blocks.size(); ++i) {
            const auto& block = document.blocks[i];
            const ImVec2 at = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(at, ImVec2(at.x + 14, at.y + 14), ColorOf(block.color));
            ImGui::Dummy(ImVec2(16, 14));
            ImGui::SameLine();
            const std::string label = std::to_string(i + 1) + " " + block.name + " (" +
                                      (block.shape < document.shapes.size() ? document.shapes[block.shape].name : "?") +
                                      ")##block" + std::to_string(i);
            if (ImGui::Selectable(label.c_str(), block_ == i + 1)) {
                block_ = static_cast<std::uint16_t>(i + 1);
                LoadDrafts();
            }
        }
        ImGui::EndChild();
        if (block_ == 0) return;
        InputString("name##block", blockDraft_.name);
        ImGui::ColorEdit4("color", blockColor_.data(), ImGuiColorEditFlags_Uint8);
        const char* shapeName = blockDraft_.shape < document.shapes.size() ? document.shapes[blockDraft_.shape].name.c_str() : "?";
        if (ImGui::BeginCombo("shape##block", shapeName)) {
            for (std::size_t s = 0; s < document.shapes.size(); ++s)
                if (ImGui::Selectable(document.shapes[s].name.c_str(), blockDraft_.shape == s))
                    blockDraft_.shape = static_cast<std::uint32_t>(s);
            ImGui::EndCombo();
        }
        if (ImGui::Button("Apply block")) {
            for (std::size_t c = 0; c < 4; ++c)
                blockDraft_.color[c] = static_cast<std::uint8_t>(std::lround(std::clamp(blockColor_[c], 0.0F, 1.0F) * 255.0F));
            Apply(Session().UpdateBlock(block_ - 1, blockDraft_));
        }
        ImGui::SameLine();
        if (ImGui::Button("Add block")) {
            Block added{FreeName(document.blocks), 0, {160, 160, 160, 255}};
            Apply(Session().AddBlock(std::move(added)));
            if (error_.empty()) block_ = static_cast<std::uint16_t>(Document().blocks.size());
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove block")) Apply(Session().RemoveBlock(block_ - 1));
    }

    template <class Named>
    static std::string FreeName(const std::vector<Named>& items) {
        for (std::size_t n = items.size() + 1;; ++n) {
            const std::string name = "new_" + std::to_string(n);
            if (std::none_of(items.begin(), items.end(), [&](const Named& item) { return item.name == name; })) return name;
        }
    }

    void DrawShapes() {
        const auto& document = Document();
        ImGui::SeparatorText("Shapes (1/16 of a cell)");
        if (ImGui::BeginCombo("shape", document.shapes[shape_].name.c_str())) {
            for (std::size_t s = 0; s < document.shapes.size(); ++s)
                if (ImGui::Selectable(document.shapes[s].name.c_str(), shape_ == s)) {
                    shape_ = s;
                    LoadDrafts();
                }
            ImGui::EndCombo();
        }
        InputString("name##shape", shapeDraft_.name);
        std::optional<std::size_t> removeBox;
        for (std::size_t b = 0; b < shapeDraft_.boxes.size(); ++b) {
            ImGui::PushID(static_cast<int>(b));
            auto& box = shapeDraft_.boxes[b];
            ImGui::SetNextItemWidth(130);
            ImGui::InputInt3("min", box.minimum.data());
            ImGui::SameLine();
            ImGui::SetNextItemWidth(130);
            ImGui::InputInt3("max", box.maximum.data());
            ImGui::SameLine();
            if (ImGui::SmallButton("x")) removeBox = b;
            ImGui::PopID();
        }
        if (removeBox) shapeDraft_.boxes.erase(shapeDraft_.boxes.begin() + static_cast<std::ptrdiff_t>(*removeBox));
        if (ImGui::Button("Add box")) shapeDraft_.boxes.push_back({{0, 0, 0}, {16, 16, 16}});
        ImGui::SameLine();
        if (ImGui::Button("Apply shape")) Apply(Session().UpdateShape(shape_, shapeDraft_));
        if (ImGui::Button("Add shape")) {
            Apply(Session().AddShape({FreeName(document.shapes), {{{0, 0, 0}, {16, 16, 16}}}}));
            if (error_.empty()) shape_ = Document().shapes.size() - 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove shape")) Apply(Session().RemoveShape(shape_));
    }

    // The preview: rebuilt when the grid changed, drawn every frame.
    void DrawPreview() {
        if (!previewError_.empty()) ImGui::TextColored(ImVec4(1, 0.4F, 0.4F, 1), "%s", previewError_.c_str());
        if (!scene_) return;
        if (builtRevision_ != Session().Revision()) {
            builtRevision_ = Session().Revision();
            auto geometry = PreviewGeometry(Document());
            if (!geometry) {
                previewError_ = Engine::Base::Describe(geometry.error());
            } else if (auto shown = scene_->Show(*geometry); !shown) {
                previewError_ = Engine::Base::Describe(shown.error());
            } else {
                previewError_.clear();
            }
        }
        const ImVec2 area = ImGui::GetContentRegionAvail();
        const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
        const auto width = static_cast<std::uint32_t>(std::max(area.x * scale.x, 16.0F));
        const auto height = static_cast<std::uint32_t>(std::max(area.y * scale.y, 16.0F));
        const auto camera = camera_.Camera();
        auto frame = scene_->Render(camera, width, height);
        if (!frame) {
            previewError_ = Engine::Base::Describe(frame.error());
            return;
        }
        if (auto uploaded = Framework::UploadFrame(renderer_, texture_, frame.value()); !uploaded) {
            previewError_ = Engine::Base::Describe(uploaded.error());
            return;
        }
        const auto id = Framework::ImGuiTextureId(renderer_, texture_.handle);
        if (!id) return;
        const ImVec2 size(static_cast<float>(width) / std::max(scale.x, 1e-3F),
                          static_cast<float>(height) / std::max(scale.y, 1e-3F));
        ImGui::Image(static_cast<ImTextureID>(*id), size);
        const ImVec2 imageMin = ImGui::GetItemRectMin();
        const bool hovered = ImGui::IsItemHovered();

        View view{PoseOf(camera), static_cast<int>(width), static_cast<int>(height), imageMin, scale};
        const ImGuiIO& io = ImGui::GetIO();
        hover_.reset();
        planeHover_.reset();
        if (hovered) {
            const float px = (io.MousePos.x - imageMin.x) * scale.x - 0.5F;
            const float py = (io.MousePos.y - imageMin.y) * scale.y - 0.5F;
            const Ray ray = PixelRay(view.pose, view.width, view.height, px, py);
            hover_ = Pick(Document(), ray, planeY_);
            planeHover_ = PlaneCell(ray);
            Navigate(io);
            Edit();
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) EndEdit();
        DrawOverlay(view);
    }

    // Where a ray crosses the edit plane, ignoring the cells (box tools).
    std::optional<Vec3i> PlaneCell(const Ray& ray) const {
        GridDocument plane;
        plane.origin = Document().origin;
        plane.cellSize = Document().cellSize;
        const auto hit = Pick(plane, ray, planeY_);
        if (!hit) return std::nullopt;
        return hit->cell;
    }

    void Navigate(const ImGuiIO& io) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            camera_.yawRadians += io.MouseDelta.x * 0.01F;
            camera_.pitchRadians = std::clamp(camera_.pitchRadians + io.MouseDelta.y * 0.01F, -1.5F, 1.5F);
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            // Along the camera's right and up, at the target's distance.
            const auto camera = camera_.Camera();
            const auto pose = PoseOf(camera);
            const auto forward = Math::Normalize(camera_.target - camera.position);
            const auto right = Math::NormalizeOrZero(Math::Cross({0, 1, 0}, forward));
            const auto up = Math::Cross(forward, right);
            const float perPixel = 2.0F * camera_.distance * std::tan(pose.verticalFieldOfViewRadians * 0.5F) /
                                   std::max(ImGui::GetItemRectSize().y, 1.0F);
            camera_.target = camera_.target - right * (io.MouseDelta.x * perPixel) + up * (io.MouseDelta.y * perPixel);
        }
        if (io.MouseWheel != 0.0F) camera_.distance = std::max(camera_.distance * (1.0F - io.MouseWheel * 0.1F), 0.05F);
    }

    void Edit() {
        const bool pressed = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        const bool held = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        if (!held) return;
        if (pressed) {
            strokeCells_.clear();
            Session().BeginStroke();
            if ((tool_ == Tool::BoxFill || tool_ == Tool::BoxClear) && planeHover_) boxStart_ = planeHover_;
        }
        if (!Session().InStroke()) return;
        if (tool_ == Tool::BoxFill || tool_ == Tool::BoxClear || !hover_) return;
        const Cell brush{block_, block_ == 0 ? std::uint8_t{0} : static_cast<std::uint8_t>(orientation_)};
        switch (tool_) {
        case Tool::Place: {
            // A drag does not climb onto the cells it placed itself.
            if (block_ == 0 || (!hover_->onPlane && strokeCells_.contains(hover_->cell))) return;
            // From inside a filled cell there is no face to place beside.
            if (!hover_->onPlane && hover_->normal == Vec3i{0, 0, 0}) return;
            const Vec3i target = PlacementCell(*hover_);
            if (strokeCells_.contains(target)) return;
            Apply(Session().SetCell(target, brush));
            strokeCells_.insert(target);
            break;
        }
        case Tool::Erase:
            if (!hover_->onPlane) Apply(Session().SetCell(hover_->cell, {}));
            break;
        case Tool::Paint:
            if (!hover_->onPlane && block_ != 0) Apply(Session().PaintCell(hover_->cell, block_));
            break;
        case Tool::Pick:
            if (pressed && !hover_->onPlane) {
                const Cell picked = Document().At(hover_->cell);
                block_ = picked.block;
                orientation_ = picked.orientation;
                LoadDrafts();
            }
            break;
        default: break;
        }
    }

    void EndEdit() {
        if (boxStart_ && !planeHover_ && session_->InStroke())
            status_ = "the box was not applied: release the button over the edit plane";
        if (boxStart_ && planeHover_ && session_->InStroke()) {
            Vec3i other = *planeHover_;
            other[1] = planeY_ + boxHeight_ - 1;
            const Cell value = tool_ == Tool::BoxFill ? Cell{block_, static_cast<std::uint8_t>(block_ == 0 ? 0 : orientation_)}
                                                      : Cell{};
            Apply(Session().FillBox(*boxStart_, other, value));
        }
        boxStart_.reset();
        if (session_->InStroke()) Session().EndStroke();
    }

    struct View final {
        CameraPose pose;
        int width{};
        int height{};
        ImVec2 imageMin;
        ImVec2 scale;
    };

    static std::optional<ImVec2> OnScreen(const View& view, const Math::Vec3 world) {
        const auto pixel = ProjectToPixel(view.pose, view.width, view.height, world);
        if (!pixel) return std::nullopt;
        return ImVec2(view.imageMin.x + (pixel->x + 0.5F) / view.scale.x, view.imageMin.y + (pixel->y + 0.5F) / view.scale.y);
    }

    void Line(const View& view, const Math::Vec3 a, const Math::Vec3 b, const ImU32 color, const float thickness) const {
        const auto from = OnScreen(view, a);
        const auto to = OnScreen(view, b);
        if (from && to) ImGui::GetWindowDrawList()->AddLine(*from, *to, color, thickness);
    }

    void Outline(const View& view, const Vec3i& low, const Vec3i& high, const ImU32 color) const {
        // A box of cells from low to high (both included): its 12 edges.
        const auto& document = Document();
        const Math::Vec3 a = CellCorner(document, low);
        const Math::Vec3 b = CellCorner(document, {high[0] + 1, high[1] + 1, high[2] + 1});
        const std::array<Math::Vec3, 8> c{Math::Vec3{a.x, a.y, a.z}, {b.x, a.y, a.z}, {b.x, a.y, b.z}, {a.x, a.y, b.z},
                                          {a.x, b.y, a.z}, {b.x, b.y, a.z}, {b.x, b.y, b.z}, {a.x, b.y, b.z}};
        constexpr std::array<std::array<int, 2>, 12> edges{{{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                                            {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}}};
        for (const auto& edge : edges) Line(view, c[static_cast<std::size_t>(edge[0])], c[static_cast<std::size_t>(edge[1])], color, 2.0F);
    }

    void DrawOverlay(const View& view) {
        ImDrawList* list = ImGui::GetWindowDrawList();
        list->PushClipRect(view.imageMin, ImVec2(view.imageMin.x + static_cast<float>(view.width) / view.scale.x,
                                                 view.imageMin.y + static_cast<float>(view.height) / view.scale.y));
        // The edit plane's grid, around where the mouse is on it (or the camera's target).
        const auto& document = Document();
        Vec3i centre{};
        if (planeHover_) {
            centre = *planeHover_;
        } else {
            for (int axis = 0; axis < 3; axis += 2) {
                const float target = axis == 0 ? camera_.target.x : camera_.target.z;
                centre[axis] = static_cast<std::int32_t>(std::floor(target / document.cellSize)) - document.origin[axis];
            }
        }
        constexpr int reach = 16;
        const ImU32 grid = IM_COL32(120, 160, 200, 90);
        for (int i = -reach; i <= reach + 1; ++i) {
            Line(view, CellCorner(document, {centre[0] + i, planeY_, centre[2] - reach}),
                 CellCorner(document, {centre[0] + i, planeY_, centre[2] + reach + 1}), grid, 1.0F);
            Line(view, CellCorner(document, {centre[0] - reach, planeY_, centre[2] + i}),
                 CellCorner(document, {centre[0] + reach + 1, planeY_, centre[2] + i}), grid, 1.0F);
        }
        if (boxStart_ && planeHover_) {
            Vec3i other = *planeHover_;
            other[1] = planeY_ + boxHeight_ - 1;
            Vec3i low{};
            Vec3i high{};
            for (int axis = 0; axis < 3; ++axis) {
                low[axis] = std::min((*boxStart_)[axis], other[axis]);
                high[axis] = std::max((*boxStart_)[axis], other[axis]);
            }
            Outline(view, low, high, tool_ == Tool::BoxFill ? IM_COL32(120, 230, 120, 255) : IM_COL32(240, 90, 90, 255));
        } else if ((tool_ == Tool::BoxFill || tool_ == Tool::BoxClear) && planeHover_) {
            Outline(view, *planeHover_, *planeHover_, IM_COL32(230, 230, 230, 200));
        } else if (hover_) {
            const bool placing = tool_ == Tool::Place;
            const Vec3i cell = placing ? PlacementCell(*hover_) : hover_->cell;
            if (placing || !hover_->onPlane)
                Outline(view, cell, cell, placing ? IM_COL32(120, 230, 120, 255) : IM_COL32(240, 200, 90, 255));
        }
        list->PopClipRect();
    }

    void DrawClosePrompt() {
        if (openClose_) {
            if (!ImGui::IsPopupOpen("Unsaved map")) ImGui::OpenPopup("Unsaved map");
            openClose_ = false;
        }
        promptOpen_ = ImGui::BeginPopupModal("Unsaved map", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        if (!promptOpen_) return;
        ImGui::TextUnformatted("The map has changes that are not saved.");
        if (ImGui::Button("Save and close")) {
            Save();
            if (!Session().Modified()) wantsExit_ = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard")) {
            wantsExit_ = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Keep editing")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    Platform::SdlPlatform& platform_;
    Sdl::SdlRenderer& renderer_;
    WindowOptions options_;
    EditorFiles files_;
    std::optional<EditSession> session_;
    std::unique_ptr<PreviewScene> scene_;
    Framework::PreviewTexture texture_;
    Framework::OrbitCamera camera_;
    std::uint64_t builtRevision_{std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t draftRevision_{};
    std::string status_;
    std::string error_;
    std::string previewError_;
    Tool tool_{Tool::Place};
    std::uint16_t block_{1};
    int orientation_{};
    int planeY_{};
    int boxHeight_{1};
    Block blockDraft_;
    Block blockSource_;
    Shape shapeSource_;
    std::array<float, 4> blockColor_{};
    std::size_t shape_{};
    Shape shapeDraft_;
    std::optional<PickHit> hover_;
    std::optional<Vec3i> planeHover_;
    std::optional<Vec3i> boxStart_;
    std::set<Vec3i> strokeCells_;
    bool openClose_{};
    bool promptOpen_{};
    bool wantsExit_{};
};

} // namespace

int RunEditorWindow(const WindowOptions& options) {
    Framework::WindowShellOptions shell;
    shell.platform.title = "GYO Voxel Map Editor";
    shell.platform.width = 1500;
    shell.platform.height = 900;
    shell.platform.resizable = true;
    return Framework::RunImGuiShell(shell, [&options](const Framework::ImGuiShellContext& context) {
        return std::make_unique<EditorClient>(context, options);
    });
}

} // namespace Gyo::Tools::VoxelMapEditor
