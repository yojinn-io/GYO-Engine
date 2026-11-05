#include "gyo/audio_editor/AudioEditorGui.hpp"

#include "gyo/audio_editor/Conversion.hpp"
#include "gyo/audio_editor/Edit.hpp"
#include "gyo/audio_editor/PreviewPlayer.hpp"
#include "gyo/audio_editor/ProjectFile.hpp"
#include "gyo/audio_editor/SoundExport.hpp"
#include "gyo/audio_editor/WavFile.hpp"
#include "gyo/audio_editor/Waveform.hpp"

#include "FileAccess.hpp"

#include "gyo/framework/imgui/ImGuiShell.hpp"

#include "engine/audio/AudioFormat.hpp"
#include "engine/audio/ClipBank.hpp"
#include "engine/audio/Mixer.hpp"
#include "engine/audio/backend/sdl/SdlAudioOutput.hpp"
#include "engine/base/Error.hpp"
#include "engine/base/Log.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/time/MonotonicClock.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Gyo::Tools::AudioEditor {
namespace {

namespace Audio = Engine::Audio;
namespace Sdl = Engine::Platform::Sdl;

using TextField = std::array<char, 1024>;

void LogError(const std::string_view text) {
    Engine::Base::Log(Engine::Base::LogLevel::Error, "audio_editor", text);
}

void SetField(TextField& field, const std::string& text) {
    field.fill('\0');
    std::copy_n(text.begin(), std::min(text.size(), field.size() - 1), field.begin());
}

std::string Field(const TextField& field) {
    return std::string(field.data());
}

std::string Format(const char* format, const double value) {
    std::array<char, 64> text{};
    std::snprintf(text.data(), text.size(), format, value);
    return text.data();
}

bool EndsWith(const std::string_view text, const std::string_view suffix) {
    if (text.size() < suffix.size()) return false;
    for (std::size_t index = 0; index < suffix.size(); ++index) {
        const char a = text[text.size() - suffix.size() + index];
        const char lower = a >= 'A' && a <= 'Z' ? static_cast<char>(a - 'A' + 'a') : a;
        if (lower != suffix[index]) return false;
    }
    return true;
}

// What a system dialog was shown for.
enum class DialogPurpose { OpenSource, OpenProject, SaveProject, AssetRoot, Catalog };

// The editor's window in the framework's ImGui shell.
class AudioEditorClient final : public Framework::ImGuiShellClient {
public:
    explicit AudioEditorClient(std::string file) : initialFile_(std::move(file)) {}

    [[nodiscard]] bool Initialize(const Framework::ImGuiShellContext& context) override {
        platform_ = &context.platform;
        auto mixer = Audio::Mixer::Create(PreviewMixerConfig(), Audio::ClipBank{});
        if (!mixer) {
            LogError("Preview mixer: " + Engine::Base::Describe(mixer.error()));
            return false;
        }
        auto submitter = mixer->CreateSubmitter();
        if (!submitter) {
            LogError("Preview submitter: " + Engine::Base::Describe(submitter.error()));
            return false;
        }
        // Editing works without a device: plays are then sent and never heard.
        auto output = Audio::Sdl::SdlAudioOutput::Open(std::move(mixer).value());
        if (output) {
            output_.emplace(std::move(output).value());
            player_.emplace(std::move(submitter).value(), &output_->OwnedMixer());
        } else {
            audioProblem_ = "No audio output: " + Engine::Base::Describe(output.error());
            player_.emplace(std::move(submitter).value(), nullptr);
        }
        player_->SetVolume(volume_);
        if (!initialFile_.empty()) {
            if (EndsWith(initialFile_, ".json")) {
                OpenProject(initialFile_);
            } else {
                OpenSource(initialFile_, true);
            }
        }
        return true;
    }

    void BeginFrame() override {
        ApplyDialogAnswers();
        player_->Update(Engine::Time::Now());
    }

    void Draw() override {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar |
                                           ImGuiWindowFlags_NoBringToFrontOnFocus;
        if (ImGui::Begin("Audio Editor", nullptr, flags)) {
            ClampTrim();
            DrawMenu();
            DrawSource();
            DrawWaveform();
            DrawEdit();
            DrawPreview();
            DrawExport();
            ImGui::Separator();
            if (dialog_.has_value()) {
                ImGui::TextUnformatted("Waiting for the system dialog.");
                ImGui::SameLine();
                if (ImGui::SmallButton("Stop waiting")) dialog_.reset();
            }
            ImGui::TextWrapped("%s", status_.c_str());
        }
        ImGui::End();
        // Space plays from the start, unless a text field takes the typing.
        if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space, false)) PlayPreview();
    }

    [[nodiscard]] bool WantsExit() const override { return quit_; }

private:
    // --- files ---------------------------------------------------------------

    // resetSettings false keeps a project's settings for the source it names.
    void OpenSource(const std::string& path, const bool resetSettings) {
        StopPreview();
        project_.request.sourcePath = path;
        source_.reset();
        auto bytes = Detail::ReadAll(path);
        if (!bytes) {
            status_ = "Cannot read the source: " + Engine::Base::Describe(bytes.error());
            return;
        }
        auto decoded = DecodeWav(*bytes);
        if (!decoded) {
            status_ = "Not a WAV the editor reads: " + Engine::Base::Describe(decoded.error());
            return;
        }
        source_ = std::move(decoded).value();
        sourcePeak_ = 0.0;
        for (const auto& plane : source_->planes) {
            for (const double sample : plane) sourcePeak_ = std::max(sourcePeak_, std::abs(sample));
        }
        if (resetSettings) {
            project_.request.settings = {};
        }
        LoadSettingsIntoFields();
        waveform_.clear();
        status_ = "Opened " + path;
        if (const auto valid = ValidateSettings(project_.request.settings, source_->Frames()); !valid) {
            status_ += ". The project's trim or gain does not fit this source: " +
                       Engine::Base::Describe(valid.error());
        }
    }

    void OpenProject(const std::string& path) {
        auto loaded = LoadProject(path);
        if (!loaded) {
            status_ = "Cannot open the project: " + Engine::Base::Describe(loaded.error());
            return;
        }
        project_ = std::move(loaded).value();
        projectPath_ = path;
        SetField(assetRoot_, project_.request.assetRoot);
        SetField(catalog_, project_.request.catalogPath);
        SetField(id_, project_.request.registration.id);
        SetField(clipPath_, project_.request.registration.path);
        OpenSource(project_.request.sourcePath, false);
        // OpenSource's message says whether the source opened and the settings fit it.
        status_ = "Opened project " + path + ". " + status_;
    }

    void SaveProjectTo(const std::string& path) {
        CollectFields();
        if (const auto saved = SaveProject(path, project_); !saved) {
            status_ = "Cannot save the project: " + Engine::Base::Describe(saved.error());
            return;
        }
        projectPath_ = path;
        status_ = "Saved project " + path;
    }

    void Export() {
        CollectFields();
        const auto report = ExportSound(project_.request);
        if (!report) {
            status_ = "Export failed: " + Engine::Base::Describe(report.error());
            return;
        }
        status_ = std::string(report->added ? "Added " : "Updated ") + project_.request.registration.id + ": " +
                  report->outputPath + ", " + std::to_string(report->frames) + " frames, saturated " +
                  std::to_string(report->saturated) + ".";
        if (report->previousPath.has_value()) {
            status_ += " " + *report->previousPath + " is no longer registered; remove it if nothing else uses it.";
        }
    }

    // --- dialogs -------------------------------------------------------------

    void ShowDialog(const DialogPurpose purpose, std::string defaultLocation) {
        Sdl::PathDialogRequest request;
        request.tag = ++lastTag_;
        request.defaultLocation = std::move(defaultLocation);
        switch (purpose) {
        case DialogPurpose::OpenSource:
            request.kind = Sdl::PathDialogKind::OpenFile;
            request.filters = {{"WAV", "wav;wave"}};
            break;
        case DialogPurpose::OpenProject:
            request.kind = Sdl::PathDialogKind::OpenFile;
            request.filters = {{"Audio editor project", "json"}};
            break;
        case DialogPurpose::SaveProject:
            request.kind = Sdl::PathDialogKind::SaveFile;
            request.filters = {{"Audio editor project", "json"}};
            break;
        case DialogPurpose::Catalog:
            request.kind = Sdl::PathDialogKind::OpenFile;
            request.filters = {{"Asset catalog JSON", "json"}};
            break;
        case DialogPurpose::AssetRoot:
            request.kind = Sdl::PathDialogKind::OpenFolder;
            break;
        }
        dialog_ = purpose;
        platform_->ShowPathDialog(std::move(request));
    }

    // Only the latest dialog's answer counts. A Failed answer does not end it,
    // because SDL on Windows may follow one with the fallback dialog's answer
    // (docs/architecture/platform-sdl.md, Design Invariant 10); Stop waiting
    // drops whatever still comes.
    void ApplyDialogAnswers() {
        for (Sdl::PathDialogAnswer& answer : platform_->TakePathDialogAnswers()) {
            if (!dialog_.has_value() || answer.tag != lastTag_) continue;
            if (answer.outcome == Sdl::PathDialogOutcome::Failed) {
                status_ = "The system dialog failed: " + answer.error;
                continue;
            }
            const DialogPurpose purpose = *dialog_;
            dialog_.reset();
            if (answer.outcome != Sdl::PathDialogOutcome::Chosen) continue;
            switch (purpose) {
            case DialogPurpose::OpenSource:
                OpenSource(answer.path, true);
                break;
            case DialogPurpose::OpenProject:
                OpenProject(answer.path);
                break;
            case DialogPurpose::SaveProject:
                SaveProjectTo(answer.path);
                break;
            case DialogPurpose::AssetRoot:
                SetField(assetRoot_, answer.path);
                break;
            case DialogPurpose::Catalog:
                SetField(catalog_, answer.path);
                break;
            }
        }
    }

    // --- fields --------------------------------------------------------------

    void CollectFields() {
        project_.request.assetRoot = Field(assetRoot_);
        project_.request.catalogPath = Field(catalog_);
        project_.request.registration.id = Field(id_);
        project_.request.registration.path = Field(clipPath_);
        // Without a source the fields show nothing of the project's settings,
        // so they are kept as the project has them.
        if (!source_.has_value()) return;
        project_.request.settings.trimStart = static_cast<std::size_t>(trimStart_);
        // The whole source is "no end", so a project keeps following its source.
        const bool toEnd = source_.has_value() && trimEnd_ == source_->Frames();
        project_.request.settings.trimEnd =
            toEnd ? std::nullopt : std::optional<std::size_t>(static_cast<std::size_t>(trimEnd_));
        project_.request.settings.gainDb = gainDb_;
    }

    void LoadSettingsIntoFields() {
        const EditSettings& settings = project_.request.settings;
        const std::size_t frames = source_.has_value() ? source_->Frames() : 0;
        trimStart_ = settings.trimStart;
        trimEnd_ = settings.trimEnd.value_or(frames);
        gainDb_ = settings.gainDb;
    }

    // --- preview -------------------------------------------------------------

    void PlayPreview() {
        if (!source_.has_value()) return;
        CollectFields();
        auto clip = RenderClip(*source_, project_.request.settings, DitherSeed(project_.request.registration.id));
        if (!clip) {
            status_ = "Cannot render: " + Engine::Base::Describe(clip.error());
            return;
        }
        renderedSaturated_ = clip->saturated;
        playedTrimStart_ = project_.request.settings.trimStart;
        playedFrames_ = clip->Frames();
        player_->Play(std::move(clip).value(), Engine::Time::Now());
    }

    void StopPreview() {
        if (player_.has_value()) player_->Stop(Engine::Time::Now());
    }

    // The trim fields always keep at least one frame of the source.
    void ClampTrim() {
        if (!source_.has_value() || source_->Frames() == 0) return;
        const std::uint64_t frames = source_->Frames();
        trimEnd_ = std::clamp<std::uint64_t>(trimEnd_, 1, frames);
        trimStart_ = std::min(trimStart_, trimEnd_ - 1);
    }

    // --- panels --------------------------------------------------------------

    void DrawMenu() {
        if (!ImGui::BeginMenuBar()) return;
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open WAV...")) ShowDialog(DialogPurpose::OpenSource, project_.request.sourcePath);
            if (ImGui::MenuItem("Open Project...")) ShowDialog(DialogPurpose::OpenProject, projectPath_);
            if (ImGui::MenuItem("Save Project", nullptr, false, !projectPath_.empty())) SaveProjectTo(projectPath_);
            if (ImGui::MenuItem("Save Project As...")) ShowDialog(DialogPurpose::SaveProject, projectPath_);
            ImGui::Separator();
            if (ImGui::MenuItem("Quit")) quit_ = true;
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    void DrawSource() {
        ImGui::SeparatorText("Source");
        if (ImGui::Button("Open WAV...")) ShowDialog(DialogPurpose::OpenSource, project_.request.sourcePath);
        ImGui::SameLine();
        ImGui::TextUnformatted(project_.request.sourcePath.empty() ? "(none)" : project_.request.sourcePath.c_str());
        if (!projectPath_.empty()) ImGui::Text("Project: %s", projectPath_.c_str());
        if (!source_.has_value()) return;
        const double seconds = static_cast<double>(source_->Frames()) / source_->sampleRate;
        ImGui::Text("%u Hz, %u-bit %s, %zu ch, %zu frames (%s s), peak %s", source_->sampleRate,
                    static_cast<unsigned>(source_->bits), source_->formatTag == 3 ? "float" : "integer",
                    source_->planes.size(), source_->Frames(), Format("%.3f", seconds).c_str(),
                    Format("%.3f", sourcePeak_).c_str());
    }

    void DrawWaveform() {
        if (!source_.has_value() || source_->Frames() == 0) return;
        const std::size_t frames = source_->Frames();
        const float width = std::max(ImGui::GetContentRegionAvail().x, 64.0f);
        const auto columns = static_cast<std::size_t>(width);
        if (waveform_.size() != source_->planes.size() || waveformColumns_ != columns) {
            waveform_.clear();
            for (const auto& plane : source_->planes) waveform_.push_back(Waveform(plane, columns));
            waveformColumns_ = columns;
        }
        constexpr float kLaneHeight = 110.0f;
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float height = kLaneHeight * static_cast<float>(waveform_.size());
        ImGui::InvisibleButton("waveform", {width, height},
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        // Left click sets where the clip starts, right click where it ends.
        const auto frameAt = [&](const float x) {
            const double t = std::clamp(static_cast<double>(x - origin.x) / width, 0.0, 1.0);
            return static_cast<std::uint64_t>(std::llround(t * static_cast<double>(frames)));
        };
        if (ImGui::IsItemHovered()) {
            const float x = ImGui::GetIO().MousePos.x;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) trimStart_ = std::min(frameAt(x), trimEnd_ - 1);
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) trimEnd_ = std::max(frameAt(x), trimStart_ + 1);
        }
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(origin, {origin.x + width, origin.y + height}, IM_COL32(18, 20, 26, 255));
        const double factor = std::pow(10.0, gainDb_ / 20.0);
        const auto xOf = [&](const std::uint64_t frame) {
            return origin.x + static_cast<float>(static_cast<double>(frame) / static_cast<double>(frames)) * width;
        };
        for (std::size_t lane = 0; lane < waveform_.size(); ++lane) {
            const float top = origin.y + kLaneHeight * static_cast<float>(lane);
            const float middle = top + kLaneHeight * 0.5f;
            const float half = kLaneHeight * 0.45f;
            // Full scale: what passes it is clamped when the clip is quantized.
            draw->AddLine({origin.x, middle - half}, {origin.x + width, middle - half}, IM_COL32(90, 40, 40, 255));
            draw->AddLine({origin.x, middle + half}, {origin.x + width, middle + half}, IM_COL32(90, 40, 40, 255));
            draw->AddLine({origin.x, middle}, {origin.x + width, middle}, IM_COL32(45, 50, 60, 255));
            const std::vector<WaveColumn>& lanes = waveform_[lane];
            for (std::size_t column = 0; column < lanes.size(); ++column) {
                // The waveform shows the gain, so the operator sees where it clips.
                const double high = std::clamp(lanes[column].max * factor, -1.05, 1.05);
                const double low = std::clamp(lanes[column].min * factor, -1.05, 1.05);
                const float x = origin.x + static_cast<float>(column);
                const bool clips = lanes[column].max * factor > 1.0 || lanes[column].min * factor < -1.0;
                draw->AddLine({x, middle - static_cast<float>(high) * half},
                              {x, middle - static_cast<float>(low) * half + 1.0f},
                              clips ? IM_COL32(230, 90, 70, 255) : IM_COL32(110, 190, 240, 255));
            }
        }
        // Outside the trim is dimmed.
        draw->AddRectFilled(origin, {xOf(trimStart_), origin.y + height}, IM_COL32(0, 0, 0, 150));
        draw->AddRectFilled({xOf(trimEnd_), origin.y}, {origin.x + width, origin.y + height}, IM_COL32(0, 0, 0, 150));
        draw->AddLine({xOf(trimStart_), origin.y}, {xOf(trimStart_), origin.y + height}, IM_COL32(120, 230, 120, 255),
                      2.0f);
        draw->AddLine({xOf(trimEnd_), origin.y}, {xOf(trimEnd_), origin.y + height}, IM_COL32(230, 200, 90, 255), 2.0f);
        // The mixer's playhead, for display only (GYO::Audio Design Invariant 4).
        const Audio::Playhead playhead = player_->Position();
        if (playhead.status == Audio::Playhead::Status::Playing) {
            const auto frame = playedTrimStart_ + static_cast<std::uint64_t>(
                static_cast<double>(playhead.clipFrame) * source_->sampleRate / Audio::kSampleRate);
            draw->AddLine({xOf(frame), origin.y}, {xOf(frame), origin.y + height}, IM_COL32(255, 255, 255, 220));
        }
        ImGui::TextDisabled("Left click: trim start. Right click: trim end.");
    }

    void DrawEdit() {
        ImGui::SeparatorText("Edit");
        if (!source_.has_value() || source_->Frames() == 0) {
            ImGui::TextDisabled("Open a WAV to edit it.");
            return;
        }
        const std::uint64_t frames = source_->Frames();
        const double rate = source_->sampleRate;
        const std::uint64_t zero = 0;
        const std::uint64_t lastStart = trimEnd_ - 1;
        const std::uint64_t firstEnd = trimStart_ + 1;
        ImGui::SetNextItemWidth(360.0f);
        ImGui::SliderScalar("Trim start (frames)", ImGuiDataType_U64, &trimStart_, &zero, &lastStart);
        ImGui::SameLine();
        ImGui::Text("%s s", Format("%.3f", static_cast<double>(trimStart_) / rate).c_str());
        ImGui::SetNextItemWidth(360.0f);
        ImGui::SliderScalar("Trim end (frames)", ImGuiDataType_U64, &trimEnd_, &firstEnd, &frames);
        ImGui::SameLine();
        ImGui::Text("%s s", Format("%.3f", static_cast<double>(trimEnd_) / rate).c_str());
        ClampTrim();
        if (ImGui::Button("Whole source")) {
            trimStart_ = 0;
            trimEnd_ = frames;
        }
        const double minDb = kMinGainDb;
        const double maxDb = kMaxGainDb;
        ImGui::SetNextItemWidth(360.0f);
        ImGui::SliderScalar("Gain (dB)", ImGuiDataType_Double, &gainDb_, &minDb, &maxDb, "%.2f dB");
        ImGui::SameLine();
        if (ImGui::Button("0 dB")) gainDb_ = 0.0;
        if (renderedSaturated_.has_value()) {
            ImGui::Text("Last render: %zu samples clamped at full scale", *renderedSaturated_);
        }
    }

    void DrawPreview() {
        ImGui::SeparatorText("Preview");
        ImGui::BeginDisabled(!source_.has_value());
        if (ImGui::Button("Play")) PlayPreview();
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Stop")) StopPreview();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::SliderFloat("Preview volume", &volume_, 0.0f, 1.0f, "%.2f")) player_->SetVolume(volume_);
        ImGui::SameLine();
        ImGui::TextDisabled("(playback only; the gain changes the output)");
        const Audio::Playhead playhead = player_->Position();
        switch (playhead.status) {
        case Audio::Playhead::Status::Playing:
            ImGui::Text("Playing %s / %s s",
                        Format("%.3f", static_cast<double>(playhead.clipFrame) / Audio::kSampleRate).c_str(),
                        Format("%.3f", static_cast<double>(playedFrames_) / Audio::kSampleRate).c_str());
            break;
        case Audio::Playhead::Status::Scheduled:
            ImGui::TextUnformatted("Starting");
            break;
        case Audio::Playhead::Status::NotPlaying:
            // Without an output no slot comes back, so a play may wait for good.
            ImGui::TextUnformatted(!player_->Waiting() ? "Not playing"
                                   : audioProblem_.empty() ? "Waiting for the audio device"
                                                           : "Not playing (no audio output)");
            break;
        }
        if (player_->Problem().has_value()) ImGui::TextColored({1.0f, 0.5f, 0.4f, 1.0f}, "%s", player_->Problem()->c_str());
        if (!audioProblem_.empty()) ImGui::TextColored({1.0f, 0.5f, 0.4f, 1.0f}, "%s", audioProblem_.c_str());
    }

    void DrawExport() {
        ImGui::SeparatorText("Export");
        ImGui::SetNextItemWidth(520.0f);
        ImGui::InputText("Asset root", assetRoot_.data(), assetRoot_.size());
        ImGui::SameLine();
        if (ImGui::Button("Browse...##root")) ShowDialog(DialogPurpose::AssetRoot, Field(assetRoot_));
        ImGui::SetNextItemWidth(520.0f);
        ImGui::InputText("Catalog", catalog_.data(), catalog_.size());
        ImGui::SameLine();
        if (ImGui::Button("Browse...##catalog")) ShowDialog(DialogPurpose::Catalog, Field(catalog_));
        ImGui::SetNextItemWidth(520.0f);
        ImGui::InputText("Id", id_.data(), id_.size());
        ImGui::SetNextItemWidth(520.0f);
        ImGui::InputText("Path in the asset root", clipPath_.data(), clipPath_.size());
        ImGui::BeginDisabled(!source_.has_value());
        if (ImGui::Button("Export and register")) Export();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("Writes the clip and its \"sound\" entry; the source is only read.");
    }

    std::string initialFile_;
    Sdl::SdlPlatform* platform_{};
    // The output owns the mixer the player shows positions from, so it is
    // declared first and destroyed last.
    std::optional<Audio::Sdl::SdlAudioOutput> output_;
    std::optional<PreviewPlayer> player_;
    std::string audioProblem_;

    AudioProject project_;
    std::string projectPath_;
    std::optional<DecodedWav> source_;
    double sourcePeak_{};
    std::vector<std::vector<WaveColumn>> waveform_;
    std::size_t waveformColumns_{};

    TextField assetRoot_{};
    TextField catalog_{};
    TextField id_{};
    TextField clipPath_{};
    std::uint64_t trimStart_{};
    std::uint64_t trimEnd_{};
    double gainDb_{};
    float volume_ = 0.8f;

    std::uint64_t playedTrimStart_{};
    std::size_t playedFrames_{};
    std::optional<std::size_t> renderedSaturated_;

    std::uint64_t lastTag_{};
    std::optional<DialogPurpose> dialog_;
    std::string status_ = "Open a WAV or a project from the File menu.";
    bool quit_{};
};

} // namespace

int RunAudioEditorGui(const std::string& file) {
    Framework::WindowShellOptions shell;
    shell.platform.title = "GYO Audio Editor";
    shell.platform.width = 1200;
    shell.platform.height = 820;
    shell.platform.resizable = true;
    return Framework::RunImGuiShell(shell, [&file](const Framework::ImGuiShellContext&) {
        return std::make_unique<AudioEditorClient>(file);
    });
}

} // namespace Gyo::Tools::AudioEditor
