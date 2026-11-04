#include "ViewerWindow.hpp"

#include "gyo/model_viewer/ModelContent.hpp"
#include "gyo/model_viewer/ModelSession.hpp"
#include "gyo/model_viewer/ModelSummary.hpp"
#include "gyo/model_viewer/Playback.hpp"
#include "gyo/model_viewer/PreviewRenderer.hpp"

#include "gyo/framework/imgui/ImGuiShell.hpp"

#include "engine/base/Error.hpp"
#include "engine/base/Log.hpp"
#include "engine/model/Animation.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/render/backend/sdl/SdlRenderer.hpp"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace Gyo::Tools::ModelViewer {
namespace {

namespace Sdl = Engine::Render::Backend::Sdl;
namespace Platform = Engine::Platform::Sdl;

constexpr std::uint64_t kContentDialog = 1;

std::filesystem::path PathOf(const std::string& utf8) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

void LogError(const std::string& text) { Engine::Base::Log(Engine::Base::LogLevel::Error, "model_viewer", text); }

class ViewerClient final : public Framework::ImGuiShellClient {
public:
    ViewerClient(const Framework::ImGuiShellContext& context, WindowOptions options)
        : platform_(context.platform), renderer_(context.renderer), options_(std::move(options)) {}

    ~ViewerClient() override {
        if (texture_.handle) static_cast<void>(renderer_.ReleaseTexture(texture_.handle));
    }

    [[nodiscard]] bool Initialize(const Framework::ImGuiShellContext&) override {
        // Engine's builtin shaders are built next to the executable (shaders/builtin).
        auto base = Platform::BaseDirectory();
        if (!base) {
            previewError_ = "no base directory: " + Engine::Base::Describe(base.error());
        } else {
            PreviewOptions preview;
            preview.shaderBundle = base.value() / "shaders" / "builtin";
            preview.width = size_[0];
            preview.height = size_[1];
            preview.driver = options_.gpuDriver;
            auto created = PreviewRenderer::Create(platform_, preview);
            if (created) {
                preview_ = std::move(created.value());
            } else {
                previewError_ = Engine::Base::Describe(created.error());
                LogError(previewError_);
            }
        }
        if (options_.assetRoot) {
            MountContent(*options_.assetRoot, options_.assetCatalog);
        }
        last_ = std::chrono::steady_clock::now();
        return true;
    }

    void BeginFrame() override {
        for (auto& answer : platform_.TakePathDialogAnswers()) {
            if (answer.tag != kContentDialog) continue;
            if (answer.outcome == Platform::PathDialogOutcome::Chosen) {
                MountContent(answer.path, std::nullopt);
            } else if (answer.outcome == Platform::PathDialogOutcome::Failed) {
                contentError_ = "the folder dialog failed: " + answer.error;
            }
        }
        const auto now = std::chrono::steady_clock::now();
        session_.clock.Advance(std::chrono::duration<double>(now - last_).count());
        last_ = now;
        // ImGui draws a frame's images after Draw: the texture changes size only here.
        if (preview_ && (wanted_[0] != size_[0] || wanted_[1] != size_[1])) {
            if (texture_.handle) static_cast<void>(renderer_.ReleaseTexture(texture_.handle));
            texture_ = {};
            if (auto resized = preview_->Resize(wanted_[0], wanted_[1]); resized) {
                size_[0] = wanted_[0];
                size_[1] = wanted_[1];
            } else {
                previewError_ = Engine::Base::Describe(resized.error());
            }
        }
    }

    void Draw() override {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("Model Viewer", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        ImGui::BeginChild("side", ImVec2(360, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
        DrawContent();
        DrawModelInfo();
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("view", ImVec2(0, 0));
        DrawPlayback();
        DrawPreview();
        ImGui::EndChild();
        ImGui::End();
    }

    [[nodiscard]] bool WantsExit() const override { return false; }

private:
    void MountContent(const std::string& root, const std::optional<std::string>& catalog) {
        session_.Close();
        content_.reset();
        auto mounted = ModelContent::Mount(PathOf(root),
                                           catalog ? std::optional<std::filesystem::path>(PathOf(*catalog)) : std::nullopt);
        if (!mounted) {
            contentError_ = Engine::Base::Describe(mounted.error());
            ShowModel();
            return;
        }
        content_ = std::move(mounted.value());
        contentRoot_ = root;
        contentError_.clear();
        ShowModel();
    }

    void OpenModel(const std::string& id) {
        session_.Open(*content_, id);
        ShowModel();
    }

    // The preview shows the session's model or none, never the one before a failed open.
    void ShowModel() {
        summary_.clear();
        const LoadedModel* model = session_.Model();
        if (model == nullptr) {
            if (preview_) static_cast<void>(preview_->Show(nullptr, pose_));
            return;
        }
        summary_ = DescribeModel(*model->model);
        if (auto posed = PreviewPose(*model->model, session_.Clip(), session_.clock, pose_); !posed) {
            previewError_ = Engine::Base::Describe(posed.error());
            // Nothing of the model before stays in the preview.
            if (preview_) static_cast<void>(preview_->Show(nullptr, pose_));
            return;
        }
        if (preview_) {
            if (auto shown = preview_->Show(model->model, pose_); !shown) {
                previewError_ = Engine::Base::Describe(shown.error());
            } else {
                previewError_.clear();
            }
            camera_ = FrameModel(*model->model, pose_, camera_);
        }
    }

    void DrawContent() {
        ImGui::SeparatorText("Content");
        ImGui::TextWrapped("%s", contentRoot_.empty() ? "(none)" : contentRoot_.c_str());
        if (ImGui::Button("Open content folder...")) {
            Platform::PathDialogRequest request;
            request.tag = kContentDialog;
            request.kind = Platform::PathDialogKind::OpenFolder;
            request.defaultLocation = contentRoot_;
            platform_.ShowPathDialog(std::move(request));
        }
        if (!contentError_.empty()) ImGui::TextColored(ImVec4(1, 0.4F, 0.4F, 1), "%s", contentError_.c_str());
        if (!content_) return;
        ImGui::SeparatorText("Models");
        ImGui::BeginChild("models", ImVec2(0, 180), ImGuiChildFlags_Borders);
        for (const auto* asset : content_->Models()) {
            const bool selected = session_.Model() != nullptr && session_.Model()->assetId == asset->id;
            if (ImGui::Selectable(asset->id.c_str(), selected)) OpenModel(asset->id);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", asset->sourcePath.c_str());
        }
        ImGui::EndChild();
    }

    void DrawModelInfo() {
        if (!session_.Error().empty()) {
            ImGui::TextColored(ImVec4(1, 0.4F, 0.4F, 1), "%s", session_.Error().c_str());
        }
        if (summary_.empty()) return;
        ImGui::SeparatorText("Engine's view");
        ImGui::BeginChild("summary", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::TextUnformatted(summary_.c_str());
        ImGui::EndChild();
    }

    void DrawPlayback() {
        const LoadedModel* model = session_.Model();
        if (model == nullptr) return;
        const auto& clips = model->model->clips;
        if (clips.empty()) {
            ImGui::TextUnformatted("No clips: the default pose.");
        } else {
            const auto current = session_.Clip().value_or(0);
            if (ImGui::BeginCombo("Clip", clips[current].name.c_str())) {
                for (std::size_t index = 0; index < clips.size(); ++index) {
                    if (ImGui::Selectable(clips[index].name.c_str(), index == current)) session_.SelectClip(index);
                }
                ImGui::EndCombo();
            }
            if (ImGui::Button(session_.clock.playing ? "Pause" : "Play")) {
                if (!session_.clock.playing && !session_.clock.loop &&
                    session_.clock.seconds >= session_.clock.durationSeconds) {
                    session_.clock.Seek(0.0);
                }
                session_.clock.playing = !session_.clock.playing;
            }
            ImGui::SameLine();
            ImGui::Checkbox("Loop", &session_.clock.loop);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(160);
            float speed = static_cast<float>(session_.clock.speed);
            if (ImGui::SliderFloat("Speed", &speed, 0.05F, 4.0F, "%.2fx", ImGuiSliderFlags_Logarithmic)) {
                session_.clock.speed = speed;
            }
            double seconds = session_.clock.seconds;
            const double zero = 0.0;
            const double end = session_.clock.durationSeconds;
            if (ImGui::SliderScalar("Seconds", ImGuiDataType_Double, &seconds, &zero, &end, "%.3f s")) {
                session_.clock.Seek(seconds);
            }
        }
        if (preview_) {
            bool skeleton = skeleton_;
            if (ImGui::Checkbox("Skeleton", &skeleton)) {
                skeleton_ = skeleton;
                preview_->SetSkeleton(skeleton);
            }
            ImGui::SameLine();
            if (ImGui::Button("Frame model")) camera_ = FrameModel(*model->model, pose_, camera_);
        }
    }

    void DrawPreview() {
        if (!previewError_.empty()) ImGui::TextColored(ImVec4(1, 0.4F, 0.4F, 1), "%s", previewError_.c_str());
        if (!preview_) return;
        // The frame in pixels, the image in ImGui's units (sharp on a high-density display).
        const ImVec2 area = ImGui::GetContentRegionAvail();
        const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
        wanted_[0] = static_cast<std::uint32_t>(std::max(area.x * scale.x, 16.0F));
        wanted_[1] = static_cast<std::uint32_t>(std::max(area.y * scale.y, 16.0F));
        if (const LoadedModel* model = session_.Model()) {
            if (auto posed = PreviewPose(*model->model, session_.Clip(), session_.clock, pose_); !posed) {
                previewError_ = Engine::Base::Describe(posed.error());
                return;
            }
        }
        auto frame = preview_->Render(pose_, camera_);
        if (!frame) {
            previewError_ = Engine::Base::Describe(frame.error());
            return;
        }
        const auto* image = frame.value();
        if (auto uploaded = UploadFrame(renderer_, texture_, *image); !uploaded) {
            previewError_ = Engine::Base::Describe(uploaded.error());
            return;
        }
        previewError_.clear();
        const auto id = Framework::ImGuiTextureId(renderer_, texture_.handle);
        if (!id) return;
        ImGui::Image(static_cast<ImTextureID>(*id), ImVec2(static_cast<float>(image->width) / std::max(scale.x, 1e-3F),
                                                           static_cast<float>(image->height) / std::max(scale.y, 1e-3F)));
        if (ImGui::IsItemHovered()) {
            const ImGuiIO& io = ImGui::GetIO();
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                camera_.yawRadians += io.MouseDelta.x * 0.01F;
                camera_.pitchRadians = std::clamp(camera_.pitchRadians + io.MouseDelta.y * 0.01F, -1.5F, 1.5F);
            }
            if (io.MouseWheel != 0.0F) camera_.distance = std::max(camera_.distance * (1.0F - io.MouseWheel * 0.1F), 0.01F);
        }
    }

    Platform::SdlPlatform& platform_;
    Sdl::SdlRenderer& renderer_;
    WindowOptions options_;
    std::optional<ModelContent> content_;
    std::string contentRoot_;
    std::string contentError_;
    ModelSession session_;
    std::string summary_;
    Engine::Model::Pose pose_;
    std::unique_ptr<PreviewRenderer> preview_;
    std::string previewError_;
    OrbitCamera camera_;
    bool skeleton_{true};
    PreviewTexture texture_;
    std::uint32_t size_[2]{640, 480};
    std::uint32_t wanted_[2]{640, 480};
    std::chrono::steady_clock::time_point last_;
};

} // namespace

int RunViewerWindow(const WindowOptions& options) {
    Framework::WindowShellOptions shell;
    shell.platform.title = "GYO Model Viewer";
    shell.platform.width = 1400;
    shell.platform.height = 860;
    shell.platform.resizable = true;
    return Framework::RunImGuiShell(shell, [&options](const Framework::ImGuiShellContext& context) {
        return std::make_unique<ViewerClient>(context, options);
    });
}

} // namespace Gyo::Tools::ModelViewer
