#include "window/EditorWindow.hpp"
#include "window/PreviewScene.hpp"

#include "gyo/voxel_map_editor/Editing.hpp"
#include "gyo/voxel_map_editor/FlyCamera.hpp"
#include "gyo/voxel_map_editor/Tools.hpp"

#include "gyo/framework/PreviewCamera.hpp"
#include "gyo/framework/PreviewTexture.hpp"
#include "gyo/framework/imgui/ImGuiShell.hpp"

#include "engine/base/Error.hpp"
#include "engine/base/Log.hpp"
#include "engine/math/linear/Matrix4.hpp"
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
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

// The window: the editing model (Editing.hpp) under ImGui, and the map on the
// framework's offscreen preview. Everything that changes the grid goes
// through EditSession; this file draws and forwards input.
namespace Gyo::Tools::VoxelMapEditor {
namespace {

namespace Math = Engine::Math;
namespace Sdl = Engine::Render::Backend::Sdl;
namespace Platform = Engine::Platform::Sdl;

// Goxel's tools and modes (v8-tool-voxel-window-1), with their keys.
// Markers are not placed here: the map editor edits geometry, the scene editor places what stands on it
// (decision v8-arch-map-scene-split).
enum class Tool : int { Brush, Shape, Laser, Line, Extrude, Plane, Picker };
constexpr std::array<const char*, 7> kToolNames{"Brush", "Shape", "Laser", "Line", "Extrude", "Plane", "Picker"};
constexpr std::array<ImGuiKey, 7> kToolKeys{ImGuiKey_B, ImGuiKey_S, ImGuiKey_L, ImGuiKey_N,
                                            ImGuiKey_F, ImGuiKey_P, ImGuiKey_C};
constexpr std::array<const char*, 7> kToolKeyNames{"B", "S", "L", "N", "F", "P", "C"};
constexpr std::array<const char*, 3> kModeNames{"Add (T)", "Sub (R)", "Paint (G)"};
constexpr std::array<const char*, 3> kShapeNames{"Cube", "Sphere", "Cylinder"};
constexpr std::array<const char*, 3> kAxisNames{"X", "Y", "Z"};
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
        if (!Open()) LogError(error_);
        else FrameCamera();
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
        // Four groups (ve-feel-1 F2): what each part of the panel is for, the orientation next to the block.
        if (ImGui::CollapsingHeader("File", ImGuiTreeNodeFlags_DefaultOpen)) DrawFile();
        if (ImGui::CollapsingHeader("Tool and mode", ImGuiTreeNodeFlags_DefaultOpen)) DrawTools();
        if (ImGui::CollapsingHeader("Block and orientation", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawOrientation();
            DrawBlocks();
            DrawShapes();
        }
        if (ImGui::CollapsingHeader("View and camera", ImGuiTreeNodeFlags_DefaultOpen)) DrawView();
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("view", ImVec2(0, 0));
        DrawPreview();
        ImGui::EndChild();
        DrawClosePrompt();
        DrawChangedPrompt();
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

    // Reads the files; the session, the preview and the drafts start again.
    bool Open() {
        if (session_) CancelDrag();
        auto opened = OpenEditor(options_.gridPath, options_.mapDirectory);
        if (!opened) {
            error_ = Engine::Base::Describe(opened.error());
            return false;
        }
        auto preview = MapPreview::Create(opened->document);
        if (!preview) {
            error_ = Engine::Base::Describe(preview.error());
            return false;
        }
        files_ = std::move(opened->files);
        session_.emplace(std::move(opened->document));
        preview_ = std::move(*preview);
        if (scene_) scene_->Clear();
        status_ = files_.grid.known ? "opened " + options_.gridPath : "new grid: " + options_.gridPath;
        error_.clear();
        LoadDrafts();
        return true;
    }
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
        const auto bounds = ChunkBounds(document);
        if (bounds) {
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
        camera_.distance = FrameDistance(bounds, size);
    }

    // Brings the preview's map up to the document (saving writes its chunks).
    bool SyncPreview() {
        const auto changes = Session().TakeChanges();
        if (changes.cells.empty() && !changes.whole) return true;
        if (auto applied = preview_->Apply(Document(), changes); !applied) {
            // Keep the changes for the next try: the whole map is baked again then.
            previewError_ = Engine::Base::Describe(applied.error());
            previewStale_ = true;
            return false;
        }
        return true;
    }

    void Save(const SaveMode mode = SaveMode::Checked) {
        // A drag in progress is put back first: a save keeps what was released, not half a drag.
        CancelDrag();
        if (previewStale_) {
            if (auto rebuilt = preview_->Apply(Document(), {{}, true}); !rebuilt) {
                Report(rebuilt.error());
                return;
            }
            previewStale_ = false;
        }
        if (!SyncPreview()) {
            error_ = previewError_;
            return;
        }
        auto saved = SaveEditor(files_, Document(), *preview_, mode);
        if (!saved) {
            if (saved.error().code == VoxelMapErrorCode::FileChanged) {
                changedFile_ = saved.error().message;
                openChanged_ = true;
            }
            Report(saved.error());
            return;
        }
        Session().MarkSaved();
        error_.clear();
        status_ = "saved " + files_.grid.path + " and " + files_.mapDirectory;
    }

    void Shortcuts() {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput || promptOpen_) return;
        const bool command = io.KeyCtrl || io.KeySuper;
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) CancelDrag();
        // During a drag, undo puts the drag back and does nothing else.
        if (command && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            if (drag_) CancelDrag();
            else if (io.KeyShift) static_cast<void>(Session().Redo());
            else static_cast<void>(Session().Undo());
        }
        if (command && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
            if (drag_) CancelDrag();
            else static_cast<void>(Session().Redo());
        }
        if (command && ImGui::IsKeyPressed(ImGuiKey_S, false)) Save();
        if (command) return;
        // Tab: the orbit camera or the free one (ve-free-camera).
        if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) ToggleCamera();
        // Flying (the right button held in the free camera), W, A, S, D, Q and E move the camera only.
        const bool flying = KeysFor(freeCamera_, ImGui::IsMouseDown(ImGuiMouseButton_Right)) == KeyUse::Camera;
        const auto editKey = [flying](const ImGuiKey key) { return !(flying && IsFlyKey(key)) && ImGui::IsKeyPressed(key, false); };
        // Modes and tools (Goxel's keys).
        if (ImGui::IsKeyPressed(ImGuiKey_T, false)) mode_ = EditMode::Add;
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) mode_ = EditMode::Sub;
        if (ImGui::IsKeyPressed(ImGuiKey_G, false)) mode_ = EditMode::Paint;
        for (std::size_t t = 0; t < kToolKeys.size(); ++t)
            if (editKey(kToolKeys[t])) SelectTool(static_cast<Tool>(t));
        // Orientation: Q and E turn about the up axis; with Shift they choose the up axis.
        if (editKey(ImGuiKey_E))
            orientation_ = io.KeyShift ? (orientation_ + 4) % 24 : orientation_ / 4 * 4 + (orientation_ % 4 + 1) % 4;
        if (editKey(ImGuiKey_Q))
            orientation_ = io.KeyShift ? (orientation_ + 20) % 24 : orientation_ / 4 * 4 + (orientation_ % 4 + 3) % 4;
        // Brush size and the edit plane.
        if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) brushSize_ = std::max(brushSize_ - 1, 1);
        if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) brushSize_ = std::min(brushSize_ + 1, MaxBrushSize);
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) plane_.at = std::min(plane_.at + 1, CellLimit - 1);
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) plane_.at = std::max(plane_.at - 1, -CellLimit);
        // The camera: arrows turn it; 1, 3, 7 and 0 (or the keypad's) look from the front, the right, the top, or as at the start.
        const float turn = 0.05F;
        if (freeCamera_) {
            if (ImGui::IsKeyDown(ImGuiKey_LeftArrow)) FlyLook(fly_, -turn, 0.0F);
            if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) FlyLook(fly_, turn, 0.0F);
            if (ImGui::IsKeyDown(ImGuiKey_UpArrow)) FlyLook(fly_, 0.0F, turn);
            if (ImGui::IsKeyDown(ImGuiKey_DownArrow)) FlyLook(fly_, 0.0F, -turn);
        } else {
        if (ImGui::IsKeyDown(ImGuiKey_LeftArrow)) camera_.yawRadians -= turn;
        if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) camera_.yawRadians += turn;
        if (ImGui::IsKeyDown(ImGuiKey_UpArrow)) camera_.pitchRadians = std::clamp(camera_.pitchRadians + turn, -1.5F, 1.5F);
        if (ImGui::IsKeyDown(ImGuiKey_DownArrow)) camera_.pitchRadians = std::clamp(camera_.pitchRadians - turn, -1.5F, 1.5F);
        }
        const auto view = [](const ImGuiKey a, const ImGuiKey b) { return ImGui::IsKeyPressed(a, false) || ImGui::IsKeyPressed(b, false); };
        if (view(ImGuiKey_1, ImGuiKey_Keypad1)) SetView(0.0F, 0.0F);
        if (view(ImGuiKey_3, ImGuiKey_Keypad3)) SetView(-1.5707964F, 0.0F);
        if (view(ImGuiKey_7, ImGuiKey_Keypad7)) SetView(0.0F, 1.5F);
        if (view(ImGuiKey_0, ImGuiKey_Keypad0)) {
            if (freeCamera_) ToggleCamera(); // framing the map is the orbit camera's
            FrameCamera();
        }
    }

    // The keys flying takes over (the free camera with the right button held).
    static bool IsFlyKey(const ImGuiKey key) {
        return key == ImGuiKey_W || key == ImGuiKey_A || key == ImGuiKey_S || key == ImGuiKey_D || key == ImGuiKey_Q ||
               key == ImGuiKey_E;
    }

    void SetView(const float yaw, const float pitch) {
        if (freeCamera_) {
            fly_.yawRadians = yaw;
            fly_.pitchRadians = pitch;
            return;
        }
        camera_.yawRadians = yaw;
        camera_.pitchRadians = pitch;
    }

    // Orbit to free and back without the view jumping (ve-free-camera FC2).
    void ToggleCamera() {
        CancelDrag();
        if (!freeCamera_) {
            fly_ = FlyFromPose(PoseOf(camera_.Camera()), fly_.speedCells);
            freeCamera_ = true;
            return;
        }
        const OrbitPlacement placed = OrbitFromFly(fly_, Document());
        camera_.target = placed.target;
        camera_.yawRadians = fly_.yawRadians;
        camera_.pitchRadians = fly_.pitchRadians;
        camera_.distance = placed.distance;
        freeCamera_ = false;
    }

    // The camera the preview draws with and picks from.
    Engine::Render::PerspectiveCamera3D ViewCamera() const {
        if (!freeCamera_) return camera_.Camera();
        const CameraPose pose = FlyPose(fly_, camera_.verticalFieldOfViewRadians, Document().cellSize);
        return {pose.position, pose.rotationRadians, pose.verticalFieldOfViewRadians, pose.nearClip, pose.farClip};
    }

    void SelectTool(const Tool tool) {
        if (tool == tool_) return;
        CancelDrag();
        tool_ = tool;
    }

    void DrawFile() {
        ImGui::SeparatorText("File");
        ImGui::TextWrapped("grid: %s", files_.grid.path.c_str());
        ImGui::TextWrapped("map: %s", files_.mapDirectory.c_str());
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
        ImGui::SeparatorText("Mode");
        for (std::size_t m = 0; m < kModeNames.size(); ++m) {
            if (m != 0) ImGui::SameLine();
            if (ImGui::RadioButton(kModeNames[m], mode_ == static_cast<EditMode>(m))) mode_ = static_cast<EditMode>(m);
        }
        ImGui::SeparatorText("Tool");
        for (std::size_t t = 0; t < kToolNames.size(); ++t) {
            if (t % 4 != 0) ImGui::SameLine();
            const std::string label = std::string(kToolNames[t]) + " (" + kToolKeyNames[t] + ")";
            if (ImGui::RadioButton(label.c_str(), tool_ == static_cast<Tool>(t))) SelectTool(static_cast<Tool>(t));
        }
        if (tool_ == Tool::Brush || tool_ == Tool::Shape || tool_ == Tool::Line || tool_ == Tool::Laser) {
            int shape = static_cast<int>(brushShape_);
            if (tool_ != Tool::Laser && ImGui::Combo("shape", &shape, kShapeNames.data(), static_cast<int>(kShapeNames.size())))
                brushShape_ = static_cast<BrushShape>(shape);
            if (tool_ != Tool::Shape) ImGui::SliderInt("size ([ ])", &brushSize_, 1, MaxBrushSize);
            if (tool_ == Tool::Shape) ImGui::Checkbox("stay on the start's plane", &shapePlanar_);
        }
        ImGui::SeparatorText("Snap");
        ImGui::Checkbox("volume", &snapVolume_);
        ImGui::SameLine();
        ImGui::Checkbox("plane", &snapPlane_);
        int axis = plane_.axis;
        if (ImGui::Combo("plane axis", &axis, kAxisNames.data(), 3)) plane_.axis = axis;
        ImGui::InputInt("plane at (PgUp/PgDn)", &plane_.at);
        plane_.at = std::clamp(plane_.at, -CellLimit, CellLimit - 1);
    }

    void DrawOrientation() {
        ImGui::SeparatorText("Orientation");
        ImGui::SliderInt("orientation (Q/E, Shift)", &orientation_, 0, 23);
        ImGui::Text("up %s, %d quarter turn(s)", kUpNames[static_cast<std::size_t>(orientation_ / 4)], orientation_ % 4);
    }

    void DrawView() {
        // The camera (ve-free-camera): orbit or free, Tab switches.
        bool free = freeCamera_;
        if (ImGui::RadioButton("orbit", !free) && free) ToggleCamera();
        ImGui::SameLine();
        if (ImGui::RadioButton("free (Tab)", free) && !free) ToggleCamera();
        if (freeCamera_) {
            ImGui::TextDisabled("hold right: look; with W/A/S/D fly, E/Q up/down, Shift fast; wheel: speed");
            ImGui::SliderFloat("speed (cells/s)", &fly_.speedCells, FlyMinSpeedCells, FlyMaxSpeedCells, "%.0f",
                               ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
        } else {
            ImGui::TextDisabled("middle drag: rotate  right drag: pan  wheel: zoom");
        }
        ImGui::TextDisabled("arrows: rotate  1/3/7: front/right/top  0: frame the map");
        // Chunks farther than this from the orbit target (in x and z) are not drawn (Visibility.hpp).
        ImGui::SliderFloat("draw distance (cells)", &drawDistanceCells_, MinDrawDistanceCells, MaxDrawDistanceCells,
                           "%.0f", ImGuiSliderFlags_AlwaysClamp);
        // Zoom (ve-feel-1 F1): a slider over the range, logarithmic, and a lock for the wheel (the orbit camera's).
        if (!freeCamera_) {
        const ZoomRange zoom = Zoom();
        const float size = Document().cellSize;
        float distanceCells = camera_.distance / size;
        if (ImGui::SliderFloat("zoom (cells)", &distanceCells, zoom.minimum / size, zoom.maximum / size, "%.0f",
                               ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp))
            camera_.distance = ClampZoom(distanceCells * size, zoom);
        ImGui::Checkbox("lock zoom (wheel off)", &zoomLocked_);
        }
        if (scene_) {
            ImGui::Text("drawn %zu / %zu chunks", scene_->DrawnCount(), scene_->ChunkCount());
            if (scene_->Capped())
                ImGui::TextColored(ImVec4(1, 0.8F, 0.3F, 1), "at most %zu chunks: the farthest are not drawn",
                                   MaxDrawnChunks);
        }
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
        if (previewStale_) {
            if (preview_->Apply(Document(), {{}, true})) previewStale_ = false;
        }
        if (SyncPreview() && !previewStale_) {
            if (auto shown = scene_->Apply(preview_->TakeChanges()); !shown) {
                // The changes are used up: everything is baked and uploaded again next frame.
                previewError_ = Engine::Base::Describe(shown.error());
                previewStale_ = true;
            } else {
                previewError_.clear();
            }
        }
        const ImVec2 area = ImGui::GetContentRegionAvail();
        const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
        const auto width = static_cast<std::uint32_t>(std::max(area.x * scale.x, 16.0F));
        const auto height = static_cast<std::uint32_t>(std::max(area.y * scale.y, 16.0F));
        // The map may have grown or shrunk since the last frame; not during a drag, so the pick ray stays put.
        if (!drag_ && !freeCamera_) camera_.distance = ClampZoom(camera_.distance, Zoom());
        const auto camera = ViewCamera();
        // Zoomed out, farther than the panel's distance is drawn (ve-feel-1 F1).
        // The free camera draws around itself, to the panel's distance (as the game draws around its camera).
        const float panel = drawDistanceCells_ * Document().cellSize;
        auto frame = scene_->Render(camera, freeCamera_ ? fly_.position : camera_.target,
                                    freeCamera_ ? panel : DrawDistanceFor(panel, camera_.distance), width, height);
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
        if (hovered || drag_) {
            const float px = (io.MousePos.x - imageMin.x) * scale.x - 0.5F;
            const float py = (io.MousePos.y - imageMin.y) * scale.y - 0.5F;
            ray_ = PixelRay(view.pose, view.width, view.height, px, py);
            // During a drag the pick looks at the map as it was when the drag began, so a
            // drag does not climb onto the cells it made (Goxel's snapping to the original).
            hover_ = Pick(drag_ ? drag_->base : Document(), ray_, plane_, PickOptions{snapVolume_, snapPlane_});
            if (hovered) Navigate(io);
            Edit(io, view);
        }
        if (drag_ && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) EndDrag();
        DrawOverlay(view);
        DrawHint(imageMin);
    }

    struct View final {
        CameraPose pose;
        int width{};
        int height{};
        ImVec2 imageMin;
        ImVec2 scale;
    };

    void Navigate(const ImGuiIO& io) {
        if (freeCamera_) {
            // The right button held: the mouse looks, W/A/S/D/E/Q fly, Shift is faster; the wheel sets the speed.
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                FlyLook(fly_, io.MouseDelta.x * 0.005F, io.MouseDelta.y * 0.005F);
                const FlyInput input{.forward = ImGui::IsKeyDown(ImGuiKey_W),
                                     .back = ImGui::IsKeyDown(ImGuiKey_S),
                                     .left = ImGui::IsKeyDown(ImGuiKey_A),
                                     .right = ImGui::IsKeyDown(ImGuiKey_D),
                                     .up = ImGui::IsKeyDown(ImGuiKey_E),
                                     .down = ImGui::IsKeyDown(ImGuiKey_Q),
                                     .fast = io.KeyShift};
                FlyMove(fly_, input, io.DeltaTime, Document().cellSize);
            }
            if (io.MouseWheel != 0.0F) FlyScrollSpeed(fly_, io.MouseWheel);
            return;
        }
        // Goxel's mouse: the middle button rotates, the right one pans, the wheel zooms.
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            camera_.yawRadians += io.MouseDelta.x * 0.01F;
            camera_.pitchRadians = std::clamp(camera_.pitchRadians + io.MouseDelta.y * 0.01F, -1.5F, 1.5F);
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
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
        // Between two cells and framing the whole map (ve-feel-1 F1); locked, the wheel leaves it.
        if (io.MouseWheel != 0.0F && !zoomLocked_)
            camera_.distance = ClampZoom(camera_.distance * (1.0F - io.MouseWheel * 0.1F), Zoom());
    }

    // The zoom range, kept until the document changes (walking every chunk each frame costs on a km^2 map).
    ZoomRange Zoom() {
        if (!zoomRevision_ || *zoomRevision_ != Session().Revision()) {
            zoomRange_ = ZoomLimits(ChunkBounds(Document()), Document().cellSize);
            zoomRevision_ = Session().Revision();
        }
        return zoomRange_;
    }

    Cell Brush() const {
        return {block_, block_ == 0 ? std::uint8_t{0} : static_cast<std::uint8_t>(orientation_)};
    }

    // The cell an edit at the hover acts on: Add goes beside the face it points
    // at (or onto the plane), Sub and Paint act on the cell itself.
    std::optional<Vec3i> Target() const {
        if (!hover_) return std::nullopt;
        if (mode_ == EditMode::Add) {
            if (!hover_->onPlane && hover_->normal == Vec3i{0, 0, 0}) return std::nullopt; // inside a cell
            return PlacementCell(*hover_);
        }
        return hover_->cell;
    }

    // A drag: its changes go into one open stroke and are shown as they are made;
    // releasing keeps them as one undo step, Esc puts them back.
    struct Drag final {
        Tool tool{};
        GridDocument base;           // the map when the drag began (what picks look at)
        Vec3i start{};
        std::optional<Vec3i> last;   // the brush's last stamp
        Vec3i normal{};              // extrude: the face's side; shape: the start face's side
        std::vector<Vec3i> face;     // extrude: the face
        ImVec2 mouseStart{};
        std::optional<Vec3i> shown;  // shape: the corner last applied; line: the end last applied
        std::optional<int> distance; // extrude: the distance last applied
        ImVec2 laserAt{-1e9F, -1e9F}; // laser: where the mouse was when it last cut
    };

    void Edit(const ImGuiIO& io, const View& view) {
        const bool pressed = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered();
        // Ctrl+click picks the block and orientation in any tool, as the picker does.
        if (pressed && (io.KeyCtrl || tool_ == Tool::Picker)) {
            if (hover_ && !hover_->onPlane) {
                const Cell picked = Document().At(hover_->cell);
                block_ = picked.block;
                orientation_ = picked.orientation;
                LoadDrafts();
            }
            return;
        }
        if (pressed) BeginDrag(io);
        if (drag_ && ImGui::IsMouseDown(ImGuiMouseButton_Left)) UpdateDrag(io, view);
    }

    void BeginDrag(const ImGuiIO& io) {
        const auto target = Target();
        switch (tool_) {
        case Tool::Plane:
            if (hover_) {
                if (const auto plane = PlaneThrough(*hover_)) plane_ = *plane;
            }
            return;
        case Tool::Extrude:
            if (!hover_ || hover_->onPlane || hover_->normal == Vec3i{0, 0, 0}) return;
            break;
        case Tool::Laser: break;
        default:
            if (!target) return;
        }
        Drag drag;
        drag.tool = tool_;
        drag.base = Document();
        drag.mouseStart = io.MousePos;
        if (tool_ == Tool::Extrude) {
            drag.start = hover_->cell;
            drag.normal = hover_->normal;
            drag.face = ExtrudeFace(Document(), hover_->cell, hover_->normal);
        } else if (target) {
            drag.start = *target;
            drag.normal = hover_ ? hover_->normal : Vec3i{};
        }
        Session().BeginStroke();
        // Shift+click with the brush: a line from the last stamp of the previous stroke.
        if (tool_ == Tool::Brush && io.KeyShift && lastStamp_ && target)
            Apply(ApplyCells(Session(), StrokeCells(*lastStamp_, *target, brushSize_, brushShape_), mode_, Brush()));
        drag_ = std::move(drag);
    }

    // The layer a planar shape stays on: across the start face's side, or the edit plane's axis.
    EditPlane StartPlane(const Drag& drag) const {
        for (int axis = 0; axis < 3; ++axis)
            if (drag.normal[axis] != 0) return EditPlane{axis, drag.start[axis]};
        return EditPlane{plane_.axis, drag.start[static_cast<std::size_t>(plane_.axis)]};
    }

    void UpdateDrag(const ImGuiIO& io, const View& view) {
        auto& drag = *drag_;
        const auto target = Target();
        switch (drag.tool) {
        case Tool::Brush: {
            if (!target || (drag.last && *drag.last == *target)) return;
            const Vec3i from = drag.last ? *drag.last : *target;
            Apply(ApplyCells(Session(), StrokeCells(from, *target, brushSize_, brushShape_), mode_, Brush()));
            drag.last = target;
            break;
        }
        case Tool::Shape: {
            std::optional<Vec3i> corner = target;
            if (shapePlanar_) {
                // Staying on the start's plane: the corner is where the ray meets that layer.
                const auto planar = Pick(drag.base, ray_, StartPlane(drag), PickOptions{false, true});
                corner = planar ? std::optional<Vec3i>(planar->cell) : std::nullopt;
            }
            if (!corner || (drag.shown && *drag.shown == *corner)) return;
            // The shape follows the mouse: put back the last one, make the new one.
            Session().CancelStroke();
            Session().BeginStroke();
            Apply(ApplyCells(Session(), ShapeCells(drag.start, *corner, brushShape_), mode_, Brush()));
            drag.shown = corner;
            break;
        }
        case Tool::Line: {
            // Goxel's line (ve-feel-1 F5): from where the drag began to the pointer; the line follows the pointer.
            if (!target || (drag.shown && *drag.shown == *target)) return;
            Apply(RedrawStroke(Session(), StrokeCells(drag.start, *target, brushSize_, brushShape_), mode_, Brush()));
            drag.shown = target;
            break;
        }
        case Tool::Laser:
            // Always taking away (Laser in Add or Paint acts as Sub), once per position of the mouse.
            if (io.MousePos.x == drag.laserAt.x && io.MousePos.y == drag.laserAt.y) return;
            drag.laserAt = io.MousePos;
            Apply(ApplyCells(Session(), LaserCells(Document(), ray_, brushSize_, 256), EditMode::Sub, {}));
            break;
        case Tool::Extrude: {
            // How far: the mouse's travel along the face's side as it looks on screen, in cells.
            const auto& document = Document();
            const Vec3i next{drag.start[0] + drag.normal[0], drag.start[1] + drag.normal[1], drag.start[2] + drag.normal[2]};
            const auto a = OnScreen(view, CellCorner(document, drag.start));
            const auto b = OnScreen(view, CellCorner(document, next));
            if (!a || !b) return;
            const float dx = b->x - a->x;
            const float dy = b->y - a->y;
            const float length2 = dx * dx + dy * dy;
            if (length2 < 1.0F) return;
            const float travelled = ((io.MousePos.x - drag.mouseStart.x) * dx + (io.MousePos.y - drag.mouseStart.y) * dy) / length2;
            const int distance = std::clamp(static_cast<int>(std::lround(travelled)), -64, 64);
            if (drag.distance && *drag.distance == distance) return;
            drag.distance = distance;
            Session().CancelStroke();
            Session().BeginStroke();
            Apply(Extrude(Session(), drag.face, drag.normal, distance));
            break;
        }
        default: break;
        }
    }

    void EndDrag() {
        if (!drag_) return;
        if (drag_->tool == Tool::Brush && drag_->last) lastStamp_ = drag_->last;
        drag_.reset();
        if (Session().InStroke()) Session().EndStroke();
    }

    void CancelDrag() {
        if (!drag_) return;
        drag_.reset();
        Session().CancelStroke();
    }

    // One line of what the left button does now (Goxel's hints, in our words).
    void DrawHint(const ImVec2 at) const {
        std::string hint;
        switch (tool_) {
        case Tool::Brush: hint = "drag to draw; Shift+click: a line from the last point"; break;
        case Tool::Shape: hint = "drag from corner to corner"; break;
        case Tool::Laser: hint = "drag to cut along the view"; break;
        case Tool::Line: hint = "drag from the start to the end of the line (Esc: cancel)"; break;
        case Tool::Extrude: hint = "drag a face to push it out or in"; break;
        case Tool::Plane: hint = "click a face to put the edit plane on it"; break;
        case Tool::Picker: hint = "click a block to take it"; break;
        }
        if (tool_ != Tool::Picker && tool_ != Tool::Plane) hint += "; Ctrl+click: take a block";
        if (drag_) hint += "; Esc: cancel";
        ImGui::GetWindowDrawList()->AddText(ImVec2(at.x + 8, at.y + 6), IM_COL32(230, 230, 230, 220), hint.c_str());
    }

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
        const auto& document = Document();
        // The edit plane's grid, around where the mouse is on it (or the camera's target).
        const auto u = static_cast<std::size_t>((plane_.axis + 1) % 3);
        const auto v = static_cast<std::size_t>((plane_.axis + 2) % 3);
        const auto w = static_cast<std::size_t>(plane_.axis);
        Vec3i centre{};
        if (hover_ && hover_->onPlane) {
            centre = hover_->cell;
        } else {
            const Math::Vec3 at = freeCamera_ ? fly_.position : camera_.target;
            const std::array<float, 3> target{at.x, at.y, at.z};
            for (std::size_t axis = 0; axis < 3; ++axis)
                centre[axis] = static_cast<std::int32_t>(std::floor(target[axis] / document.cellSize)) - document.origin[axis];
        }
        centre[w] = plane_.at;
        // The grid lies on the side of the layer the camera looks at.
        const Math::Vec3 eyeAt = ViewCamera().position;
        const std::array<float, 3> eye{eyeAt.x, eyeAt.y, eyeAt.z};
        const float layer = (static_cast<float>(plane_.at + document.origin[w]) + 0.5F) * document.cellSize;
        const std::int32_t side = eye[w] > layer ? plane_.at + 1 : plane_.at;
        const auto corner = [&](const std::int32_t a, const std::int32_t b) {
            Vec3i cell{};
            cell[u] = a;
            cell[v] = b;
            cell[w] = side;
            return CellCorner(document, cell);
        };
        constexpr int reach = 16;
        const ImU32 grid = IM_COL32(120, 160, 200, 90);
        for (int i = -reach; i <= reach + 1; ++i) {
            Line(view, corner(centre[u] + i, centre[v] - reach), corner(centre[u] + i, centre[v] + reach + 1), grid, 1.0F);
            Line(view, corner(centre[u] - reach, centre[v] + i), corner(centre[u] + reach + 1, centre[v] + i), grid, 1.0F);
        }
        // What the left button would touch.
        const ImU32 colour = mode_ == EditMode::Add ? IM_COL32(120, 230, 120, 255)
                             : mode_ == EditMode::Sub ? IM_COL32(240, 90, 90, 255) : IM_COL32(240, 200, 90, 255);
        const auto bounds = [](const std::vector<Vec3i>& cells) -> std::optional<std::pair<Vec3i, Vec3i>> {
            if (cells.empty()) return std::nullopt;
            Vec3i low = cells.front(), high = cells.front();
            for (const auto& c : cells)
                for (int axis = 0; axis < 3; ++axis) {
                    low[axis] = std::min(low[axis], c[axis]);
                    high[axis] = std::max(high[axis], c[axis]);
                }
            return std::make_pair(low, high);
        };
        const auto target = Target();
        if (tool_ == Tool::Extrude && hover_ && !hover_->onPlane && !drag_) {
            // The face is found again only when the pointer's face or the map changed.
            const auto key = std::make_tuple(hover_->cell, hover_->normal, Session().Revision());
            if (faceKey_ != key) {
                faceKey_ = key;
                faceBounds_ = bounds(ExtrudeFace(document, hover_->cell, hover_->normal));
            }
            if (faceBounds_) Outline(view, faceBounds_->first, faceBounds_->second, IM_COL32(230, 230, 230, 220));
        } else if ((tool_ == Tool::Brush || tool_ == Tool::Line) && target && !drag_) {
            if (const auto box = bounds(BrushCells(*target, brushSize_, brushShape_))) Outline(view, box->first, box->second, colour);
        } else if ((tool_ == Tool::Shape || tool_ == Tool::Picker || tool_ == Tool::Plane) && hover_ && !drag_) {
            Outline(view, hover_->cell, hover_->cell, IM_COL32(230, 230, 230, 200));
        } else if (tool_ == Tool::Shape && drag_ && target) {
            Vec3i low{}, high{};
            for (int axis = 0; axis < 3; ++axis) {
                low[axis] = std::min(drag_->start[axis], (*target)[axis]);
                high[axis] = std::max(drag_->start[axis], (*target)[axis]);
            }
            Outline(view, low, high, colour);
        }
        // The block's orientation where it would go (ve-feel-1 F3): its up and its front (+Z) as arrows.
        const bool places = tool_ == Tool::Brush || tool_ == Tool::Shape || tool_ == Tool::Line;
        // Paint keeps a cell's orientation, so in Paint the arrows show the cell's own (the user, 2026-11-07).
        if (places && mode_ != EditMode::Sub && target)
            DrawOrientationArrows(view, *target,
                                  mode_ == EditMode::Paint ? Document().At(*target).orientation : orientation_);
        DrawAxes(view);
        list->PopClipRect();
    }

    void DrawOrientationArrows(const View& view, const Vec3i& cell, const int orientation) const {
        const auto& document = Document();
        const auto& m = OrientationMatrix(static_cast<std::size_t>(orientation));
        const auto turned = [&m](const Vec3i local) {
            std::array<float, 3> r{};
            for (std::size_t row = 0; row < 3; ++row)
                for (std::size_t col = 0; col < 3; ++col) r[row] += static_cast<float>(m[row][col] * local[col]);
            return Math::Vec3{r[0], r[1], r[2]};
        };
        const Math::Vec3 centre = CellCorner(document, cell) + Math::Vec3{0.5F, 0.5F, 0.5F} * document.cellSize;
        const float length = 0.9F * document.cellSize;
        const auto arrow = [&](const Math::Vec3 direction, const ImU32 color) {
            const Math::Vec3 tip = centre + direction * length;
            Line(view, centre, tip, color, 3.0F);
            if (const auto at = OnScreen(view, tip)) ImGui::GetWindowDrawList()->AddCircleFilled(*at, 4.0F, color);
        };
        arrow(turned({0, 1, 0}), IM_COL32(250, 220, 60, 255));  // up: yellow
        arrow(turned({0, 0, 1}), IM_COL32(60, 210, 250, 255));  // front (+Z): cyan
    }

    // The camera's orientation (ve-feel-1 F4, as Goxel's corner axes): world X, Y and Z turned into the view.
    void DrawAxes(const View& view) const {
        // The rotation part of Engine's view matrix: X, then Y, then Z, each undone.
        const Math::Matrix4 toView = Math::Multiply(
            Math::MakeRotationX(-view.pose.rotationRadians.x),
            Math::Multiply(Math::MakeRotationY(-view.pose.rotationRadians.y),
                           Math::MakeRotationZ(-view.pose.rotationRadians.z)));
        const ImVec2 origin(view.imageMin.x + 50.0F, view.imageMin.y + static_cast<float>(view.height) / view.scale.y - 50.0F);
        constexpr float size = 36.0F;
        struct Axis final {
            Math::Vec3 world;
            ImU32 color;
            const char* name;
        };
        const std::array<Axis, 3> axes{{{{1, 0, 0}, IM_COL32(235, 70, 70, 255), "X"},
                                        {{0, 1, 0}, IM_COL32(80, 220, 80, 255), "Y"},
                                        {{0, 0, 1}, IM_COL32(80, 140, 250, 255), "Z"}}};
        ImDrawList* list = ImGui::GetWindowDrawList();
        for (const auto& axis : axes) {
            const Math::Vec3 v = Math::TransformVector(toView, axis.world);
            const ImVec2 tip(origin.x + v.x * size, origin.y - v.y * size);
            list->AddLine(origin, tip, axis.color, 2.5F);
            list->AddText(ImVec2(tip.x + 2.0F, tip.y - 7.0F), axis.color, axis.name);
        }
    }

    // A file changed on disk since it was opened or saved (the save wrote nothing).
    void DrawChangedPrompt() {
        if (openChanged_) {
            if (!ImGui::IsPopupOpen("File changed")) ImGui::OpenPopup("File changed");
            openChanged_ = false;
        }
        const bool open = ImGui::BeginPopupModal("File changed", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        promptOpen_ = promptOpen_ || open;
        if (!open) return;
        ImGui::TextUnformatted(changedFile_.c_str());
        if (ImGui::Button("Reload from disk")) {
            // The edits are dropped; what is on disk is opened again.
            if (Open()) FrameCamera();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Overwrite")) {
            Save(SaveMode::Overwrite);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
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
    bool freeCamera_{};
    FlyCamera fly_{};
    std::optional<MapPreview> preview_;
    bool previewStale_{};
    bool openChanged_{};
    std::string changedFile_;
    std::uint64_t draftRevision_{};
    std::string status_;
    std::string error_;
    std::string previewError_;
    Tool tool_{Tool::Brush};
    EditMode mode_{EditMode::Add};
    BrushShape brushShape_{BrushShape::Cube};
    int brushSize_{1};
    float drawDistanceCells_{DefaultDrawDistanceCells};
    bool zoomLocked_{};
    std::optional<std::uint64_t> zoomRevision_;
    ZoomRange zoomRange_{};
    bool snapVolume_{true};
    bool snapPlane_{true};
    EditPlane plane_{};
    std::uint16_t block_{1};
    int orientation_{};
    Block blockDraft_;
    Block blockSource_;
    Shape shapeSource_;
    std::array<float, 4> blockColor_{};
    std::size_t shape_{};
    Shape shapeDraft_;
    std::optional<PickHit> hover_;
    Ray ray_{};
    std::optional<Drag> drag_;
    std::optional<Vec3i> lastStamp_;
    bool shapePlanar_{};
    std::optional<std::tuple<Vec3i, Vec3i, std::uint64_t>> faceKey_;
    std::optional<std::pair<Vec3i, Vec3i>> faceBounds_;
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
