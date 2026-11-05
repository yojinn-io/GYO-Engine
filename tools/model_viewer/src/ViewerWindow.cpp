#include "ViewerWindow.hpp"

#include "gyo/model_viewer/EventFiles.hpp"
#include "gyo/model_viewer/EventHistory.hpp"
#include "gyo/model_viewer/EventSoundFiles.hpp"
#include "gyo/model_viewer/ModelContent.hpp"
#include "gyo/model_viewer/ModelSession.hpp"
#include "gyo/model_viewer/ModelSummary.hpp"
#include "gyo/model_viewer/Playback.hpp"
#include "gyo/model_viewer/PreviewRenderer.hpp"
#include "gyo/model_viewer/PreviewSounds.hpp"

#include "gyo/framework/imgui/ImGuiShell.hpp"

#include "engine/base/Error.hpp"
#include "engine/base/Log.hpp"
#include "engine/model/Animation.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/render/backend/sdl/SdlRenderer.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
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

void LogError(const std::string& text) { Engine::Base::Log(Engine::Base::LogLevel::Error, "model_viewer", text); }

class ViewerClient final : public Framework::ImGuiShellClient {
public:
    ViewerClient(const Framework::ImGuiShellContext& context, WindowOptions options)
        : platform_(context.platform), renderer_(context.renderer), options_(std::move(options)) {}

    ~ViewerClient() override {
        if (texture_.handle) static_cast<void>(renderer_.ReleaseTexture(texture_.handle));
        // A way out the shell does not ask about (a crash path, a client exit): say what was lost.
        if (AnyUnsaved()) {
            Engine::Base::Log(Engine::Base::LogLevel::Warning, "model_viewer",
                              "the window closed with unsaved edits of the events or their sounds: " +
                                  (eventsPath_.empty() ? std::string("a new events file") : eventsPath_));
        }
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
        if (auto opened = PreviewSounds::Open(); opened) {
            audio_ = std::move(opened.value());
        } else {
            soundsError_ = Engine::Base::Describe(opened.error());
            LogError(soundsError_);
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
                Guarded([this, path = answer.path] { MountContent(path, std::nullopt); });
            } else if (answer.outcome == Platform::PathDialogOutcome::Failed) {
                contentError_ = "the folder dialog failed: " + answer.error;
            }
        }
        const auto now = std::chrono::steady_clock::now();
        const double wall = std::chrono::duration<double>(now - last_).count();
        last_ = now;
        for (auto& flash : flashes_) flash.second -= wall;
        std::erase_if(flashes_, [](const auto& flash) { return flash.second <= 0.0; });
        const auto step = session_.clock.Advance(wall);
        if (step && history_) {
            // A marker lights up when playback passes its moment (Engine's Crossed only).
            if (const auto* track = CurrentTrack()) {
                for (const std::size_t index : CrossedEvents(*track, *step)) flashes_.push_back({index, 0.3});
            }
        }
        // A clip change gives another schedule: a new version.
        if (session_.Clip() != soundsClip_) {
            soundsClip_ = session_.Clip();
            ++soundsVersion_;
        }
        UpdateSounds(step.has_value() && step->currentSeconds >= session_.clock.durationSeconds && session_.clock.loop);
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
        DrawEvents();
        DrawPreview();
        ImGui::EndChild();
        DrawDiscardPrompt();
        ImGui::End();
    }

    [[nodiscard]] bool WantsExit() const override { return wantsExit_; }
    // Unsaved event edits: the window asks in the next frame (v8-framework-close-request).
    [[nodiscard]] bool CloseRequested() override {
        CommitPending(); // a field still being typed in counts as an edit
        if (!AnyUnsaved()) return true;
        Guarded([this] { wantsExit_ = true; });
        return false;
    }

private:
    // Runs action now, or after the user agrees to drop unsaved event edits.
    void Guarded(std::function<void()> action) {
        CommitPending();
        if (AnyUnsaved()) {
            // Opened from the main window's ID stack, where the prompt is drawn.
            pendingAction_ = std::move(action);
            openDiscard_ = true;
            return;
        }
        action();
    }

    void DrawDiscardPrompt() {
        if (openDiscard_) {
            // A second request while it is open keeps it as it is.
            if (!ImGui::IsPopupOpen("Discard unsaved events?")) ImGui::OpenPopup("Discard unsaved events?");
            openDiscard_ = false;
        }
        if (!ImGui::BeginPopupModal("Discard unsaved events?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
        ImGui::TextUnformatted("The events or their sounds have changes that are not saved.");
        if (ImGui::Button("Discard")) {
            auto action = std::move(pendingAction_);
            pendingAction_ = {};
            history_.reset();
            sounds_.reset();
            if (action) action();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Keep editing")) {
            pendingAction_ = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    void MountContent(const std::string& root, const std::optional<std::string>& catalog) {
        CloseEvents();
        ForgetSoundFiles();
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
        contentCatalog_ = catalog;
        contentError_.clear();
        ShowModel();
    }

    // Mounts the same content again (a new events file is in its catalog now),
    // keeping the opened model, and lists the model's events files again.
    void RefreshContent() {
        auto mounted = ModelContent::Mount(PathOf(contentRoot_), contentCatalog_ ? std::optional<std::filesystem::path>(PathOf(*contentCatalog_))
                                                                                 : std::nullopt);
        if (!mounted) {
            eventsError_ = "the new file is written, but the content cannot be read again: " + Engine::Base::Describe(mounted.error());
            return;
        }
        content_ = std::move(mounted.value());
        if (const LoadedModel* model = session_.Model()) {
            if (auto candidates = content_->EventsFor(model->assetId)) candidates_ = std::move(candidates.value());
        }
    }

    void OpenModel(const std::string& id) {
        CloseEvents();
        session_.Open(*content_, id);
        ShowModel();
        if (session_.Model() != nullptr) {
            auto candidates = content_->EventsFor(id);
            if (candidates) candidates_ = std::move(candidates.value());
            else eventsError_ = Engine::Base::Describe(candidates.error());
        }
    }

    void CloseEvents() {
        CloseSounds();
        history_.reset();
        candidates_.clear();
        eventsPath_.clear();
        eventsId_.clear();
        eventsError_.clear();
        eventsStatus_.clear();
        selected_.reset();
        ForgetIndices();
    }

    // Event indices refer to one clip's events in one state: they go with any change.
    void ForgetIndices() {
        // Every edit of the events passes here: the sounds bind to the events as they now are.
        if (sounds_) Rebind();
        selected_.reset();
        dragging_.reset();
        flashes_.clear();
        editingIndex_.reset();
        namePending_ = false;
        secondsPending_ = false;
    }

    void OpenEventsFile(const EventsCandidate& candidate) {
        const LoadedModel* model = session_.Model();
        auto opened = OpenEvents(candidate.path, model->assetId, *model->model);
        if (!opened) {
            eventsError_ = Engine::Base::Describe(opened.error());
            return;
        }
        history_.emplace(opened->document.Set());
        stamp_ = opened->stamp;
        eventsPath_ = candidate.path;
        eventsId_ = candidate.assetId;
        eventsError_.clear();
        eventsStatus_ = "Opened " + candidate.assetId;
        ForgetIndices();
        FindSounds();
    }

    void NewEvents() {
        Engine::Model::AnimationEventSet set;
        set.modelAssetId = session_.Model()->assetId;
        // Never saved: unsaved in every state until it is written as a new file.
        history_.emplace(std::move(set), false);
        history_->Add(CurrentClipName(), {std::min(session_.clock.seconds, session_.clock.durationSeconds), "event"});
        eventsPath_.clear();
        eventsId_.clear();
        eventsStatus_ = "A new events file, not written yet";
        ForgetIndices();
    }

    [[nodiscard]] std::string CurrentClipName() const {
        const LoadedModel* model = session_.Model();
        if (model == nullptr || !session_.Clip()) return {};
        return model->model->clips[*session_.Clip()].name;
    }

    [[nodiscard]] const Engine::Model::AnimationEventTrack* CurrentTrack() const {
        if (!history_) return nullptr;
        return history_->Document().Track(CurrentClipName());
    }

    [[nodiscard]] bool AnyUnsaved() const {
        return (history_ && history_->Unsaved()) || (sounds_ && soundsUnsaved_);
    }

    // The length of a sound asset, through Engine's sound loader, loaded into
    // the preview's mixer on the way (the framework's SoundSeconds).
    [[nodiscard]] Framework::SoundSeconds SoundLengths() {
        return [this](std::string_view id) -> std::optional<double> {
            if (const auto known = soundSeconds_.find(id); known != soundSeconds_.end()) return known->second;
            if (!content_) return std::nullopt;
            auto loaded = content_->LoadSound(id);
            if (!loaded) {
                soundsError_ = Engine::Base::Describe(loaded.error());
                return std::nullopt;
            }
            if (audio_) {
                // The length still binds; only the preview cannot play it (a sample rate the mixer lacks, no slot).
                if (auto put = audio_->Load(loaded.value()); !put) audioWarning_ = Engine::Base::Describe(put.error());
            }
            soundSeconds_.emplace(std::string(id), loaded->Seconds());
            return loaded->Seconds();
        };
    }

    void CloseSounds() {
        if (audio_) playback_.Stop(*audio_);
        ++soundsVersion_;
        sounds_.reset();
        bound_.reset();
        soundCandidates_.clear();
        soundsUnsaved_ = false;
        soundsStatus_.clear();
        soundsError_.clear();
        audioWarning_.clear();
    }

    // The sounds are read again from disk the next time a length is asked for.
    void ForgetSoundFiles() {
        soundSeconds_.clear();
        if (audio_) audio_->Clear();
    }

    // The gyo.event_sounds files of the opened events file; the first one the framework accepts is opened.
    void FindSounds() {
        CloseSounds();
        ForgetSoundFiles();
        if (!content_ || eventsId_.empty()) return;
        auto candidates = content_->EventSoundsFor(eventsId_);
        if (!candidates) {
            soundsError_ = Engine::Base::Describe(candidates.error());
            return;
        }
        soundCandidates_ = std::move(candidates.value());
        for (const auto& candidate : soundCandidates_) {
            if (!candidate.parsed) continue;
            auto opened = OpenEventSounds(candidate.path, eventsId_, history_->Document().Set(), SoundLengths());
            if (!opened) {
                soundsError_ = Engine::Base::Describe(opened.error());
                continue;
            }
            soundsId_ = candidate.assetId;
            sounds_ = std::move(opened.value());
            Rebind();
            soundsStatus_ = "Opened " + soundsId_;
            return;
        }
    }

    // The bound set the schedule reads; none while an edit does not bind (the reason is shown).
    void Rebind() {
        bound_.reset();
        ++soundsVersion_;
        if (!sounds_ || !history_) return;
        auto bound = Framework::BindEventSounds(sounds_->set, history_->Document().Set(), SoundLengths());
        if (!bound) {
            soundsError_ = Engine::Base::Describe(bound.error());
            return;
        }
        bound_ = std::move(bound.value());
        soundsError_.clear();
    }

    void SaveSounds() {
        // Bound to events that are not on disk, the file could name an event the events file does not have.
        if (history_->Unsaved()) {
            soundsError_ = "save the events first: the sounds are checked against the events file as saved";
            return;
        }
        auto saved = SaveEventSounds(sounds_->path, sounds_->stamp, sounds_->set, history_->Document().Set(), SoundLengths());
        if (!saved) {
            soundsError_ = Engine::Base::Describe(saved.error());
            return;
        }
        sounds_->stamp = saved.value();
        soundsUnsaved_ = false;
        soundsError_.clear();
        soundsStatus_ = "Saved " + sounds_->path;
    }

    // The clip's sounds on the mixer as it plays (SoundPlayback, at the times
    // EventSoundOffsetSeconds gives).
    void UpdateSounds(const bool wrapped) {
        if (!audio_) return;
        const auto* track = CurrentTrack();
        std::vector<ScheduledSound> schedule;
        if (bound_ && track != nullptr) {
            schedule = ScheduleSounds(*bound_, *track, session_.clock.durationSeconds, session_.clock.speed);
        }
        const SoundClock clock{session_.clock.playing, session_.clock.loop, session_.clock.speed, session_.clock.seconds,
                               session_.clock.durationSeconds, wrapped};
        // The mixer's default schedule delay: a start this late still sounds on time.
        playback_.Update(*audio_, schedule, soundsVersion_, clock, Engine::Time::Now(), std::chrono::milliseconds(60));
    }

    void DrawSounds() {
        if (!ImGui::TreeNodeEx("Sounds", ImGuiTreeNodeFlags_DefaultOpen)) return;
        if (!audio_) ImGui::TextUnformatted("No audio output: sounds are not heard.");
        if (!sounds_) {
            for (const auto& candidate : soundCandidates_) {
                if (!candidate.parsed) {
                    ImGui::TextColored(ImVec4(1, 0.4F, 0.4F, 1), "%s: %s", candidate.assetId.c_str(), candidate.refusal.c_str());
                }
            }
            if (soundCandidates_.empty()) ImGui::TextUnformatted("These events have no gyo.event_sounds file.");
            if (!eventsId_.empty() && ImGui::Button("New sounds file")) NewSounds();
        } else {
            DrawSoundEntries();
        }
        if (!soundsError_.empty()) ImGui::TextColored(ImVec4(1, 0.4F, 0.4F, 1), "%s", soundsError_.c_str());
        else if (!soundsStatus_.empty()) ImGui::TextUnformatted(soundsStatus_.c_str());
        if (!audioWarning_.empty()) ImGui::TextColored(ImVec4(1, 0.8F, 0.3F, 1), "not heard: %s", audioWarning_.c_str());
        ImGui::TreePop();
    }

    [[nodiscard]] std::vector<std::string> SoundIds() const {
        std::vector<std::string> ids;
        if (!content_) return ids;
        for (const auto& asset : content_->Catalog().Assets()) {
            if (asset.type == kSoundType) ids.push_back(asset.id);
        }
        return ids;
    }

    // An entry for the event name, in the set's name order (the format's order).
    void AddSound(const std::string& event) {
        const auto ids = SoundIds();
        auto& list = sounds_->set.sounds;
        const auto at = std::lower_bound(list.begin(), list.end(), event,
                                         [](const Framework::EventSound& sound, const std::string& name) { return sound.event < name; });
        if (at != list.end() && at->event == event) return;
        list.insert(at, {event, ids.empty() ? std::string() : ids.front(), 0.0, 1.0F});
        soundsUnsaved_ = true;
        Rebind();
    }

    // A set not written yet; it is saved as a new file of the catalog the mount reads.
    void NewSounds() {
        OpenedEventSounds created;
        created.set.animationEventsAssetId = eventsId_;
        sounds_ = std::move(created);
        soundsId_.clear();
        soundsUnsaved_ = true;
        newSoundsCatalog_ = contentCatalog_ ? *contentCatalog_
                            : contentRoot_.empty() ? std::string() : Utf8Path(PathOf(contentRoot_) / "asset_catalog.json");
        if (const auto* track = CurrentTrack(); track != nullptr && !track->events.empty()) {
            AddSound(track->events[selected_.value_or(0) < track->events.size() ? selected_.value_or(0) : 0].name);
        }
        soundsStatus_ = "A new sounds file, not written yet";
    }

    void SaveNewSounds() {
        if (history_->Unsaved()) {
            soundsError_ = "save the events first: the sounds are checked against the events file as saved";
            return;
        }
        NewEventsRequest request{contentRoot_, newSoundsCatalog_, newSoundsId_, newSoundsPath_};
        auto created = CreateEventSounds(request, eventsId_, sounds_->set, history_->Document().Set(), SoundLengths());
        if (!created) {
            soundsError_ = Engine::Base::Describe(created.error());
            return;
        }
        sounds_->path = created->outputPath;
        sounds_->stamp = created->stamp;
        soundsId_ = newSoundsId_;
        soundsUnsaved_ = false;
        soundsError_.clear();
        soundsStatus_ = "Saved " + sounds_->path;
        RefreshContent();
    }

    void DrawSoundEntries() {
        ImGui::Text("%s%s", soundsId_.empty() ? "(new sounds file)" : soundsId_.c_str(), soundsUnsaved_ ? "  * unsaved" : "");
        if (sounds_->path.empty()) {
            InputString("New sounds id", newSoundsId_);
            InputString("New sounds path (in the content root)", newSoundsPath_);
            InputString("Sounds catalog", newSoundsCatalog_);
            if (ImGui::Button("Save sounds as new file")) SaveNewSounds();
        } else if (ImGui::Button("Save sounds")) {
            SaveSounds();
        }
        if (const auto* track = CurrentTrack(); track != nullptr && selected_ && *selected_ < track->events.size()) {
            const auto& name = track->events[*selected_].name;
            const bool has = std::any_of(sounds_->set.sounds.begin(), sounds_->set.sounds.end(),
                                         [&](const Framework::EventSound& sound) { return sound.event == name; });
            if (!has) {
                ImGui::SameLine();
                if (ImGui::Button(("Add a sound for " + name).c_str())) AddSound(name);
            }
        }
        const auto soundIds = SoundIds();
        bool changed = false;
        std::optional<std::size_t> removed;
        for (std::size_t index = 0; index < sounds_->set.sounds.size(); ++index) {
            auto& entry = sounds_->set.sounds[index];
            ImGui::PushID(static_cast<int>(index));
            ImGui::TextUnformatted(entry.event.c_str());
            ImGui::SameLine(160);
            ImGui::SetNextItemWidth(260);
            if (ImGui::BeginCombo("Sound", entry.soundAssetId.c_str())) {
                for (const auto& id : soundIds) {
                    if (ImGui::Selectable(id.c_str(), id == entry.soundAssetId) && id != entry.soundAssetId) {
                        entry.soundAssetId = id;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            // Typed values are not clamped: the framework refuses what it refuses when the file is saved.
            if (ImGui::InputDouble("Sync s", &entry.syncSeconds, 0.0, 0.0, "%.4f")) changed = true;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90);
            if (ImGui::InputFloat("Gain", &entry.gain, 0.0F, 0.0F, "%.3f")) changed = true;
            ImGui::SameLine();
            if (ImGui::Button("Remove")) removed = index;
            ImGui::PopID();
        }
        if (removed) {
            sounds_->set.sounds.erase(sounds_->set.sounds.begin() + static_cast<std::ptrdiff_t>(*removed));
            changed = true;
        }
        if (changed) {
            soundsUnsaved_ = true;
            Rebind();
        }
    }

    void Save() {
        const LoadedModel* model = session_.Model();
        if (eventsPath_.empty()) {
            NewEventsRequest request;
            request.assetRoot = contentRoot_;
            request.catalogPath = newCatalog_;
            request.id = newId_;
            request.path = newPath_;
            auto created = CreateEvents(request, model->assetId, history_->Document().Set(), *model->model);
            if (!created) {
                eventsError_ = Engine::Base::Describe(created.error());
                return;
            }
            eventsPath_ = created->outputPath;
            eventsId_ = newId_;
            stamp_ = created->stamp;
            RefreshContent();
        } else {
            auto saved = SaveEvents(eventsPath_, stamp_, history_->Document().Set(), *model->model);
            if (!saved) {
                eventsError_ = Engine::Base::Describe(saved.error());
                return;
            }
            stamp_ = saved.value();
        }
        history_->MarkSaved();
        eventsError_.clear();
        eventsStatus_ = "Saved " + eventsPath_;
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
            if (ImGui::Selectable(asset->id.c_str(), selected) && !selected) {
                Guarded([this, id = asset->id] { OpenModel(id); });
            }
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
                    if (ImGui::Selectable(clips[index].name.c_str(), index == current) && index != current) {
                        CommitPending();
                        session_.SelectClip(index);
                        ForgetIndices();
                    }
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
            if (ImGui::SliderScalar("Seconds##playhead", ImGuiDataType_Double, &seconds, &zero, &end, "%.3f s")) {
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

    void DrawEvents() {
        const LoadedModel* model = session_.Model();
        if (model == nullptr || !session_.Clip()) return;
        if (!ImGui::CollapsingHeader("Events", ImGuiTreeNodeFlags_DefaultOpen)) return;
        ImGui::PushID("events");
        DrawEventsBody();
        ImGui::PopID();
    }

    void DrawEventsBody() {
        if (!history_) {
            for (const auto& candidate : candidates_) {
                if (!candidate.events) {
                    ImGui::TextColored(ImVec4(1, 0.4F, 0.4F, 1), "%s: %s", candidate.assetId.c_str(), candidate.refusal.c_str());
                } else if (ImGui::Button(("Open " + candidate.assetId).c_str())) {
                    OpenEventsFile(candidate);
                }
            }
            if (candidates_.empty()) ImGui::TextUnformatted("This model has no gyo.anim_events file.");
            if (ImGui::Button("New events file")) {
                // The catalog the mount reads: the one named, or the content root's own.
                newCatalog_ = contentCatalog_ ? *contentCatalog_
                              : contentRoot_.empty() ? std::string() : Utf8Path(PathOf(contentRoot_) / "asset_catalog.json");
                NewEvents();
            }
            if (!eventsError_.empty()) ImGui::TextColored(ImVec4(1, 0.4F, 0.4F, 1), "%s", eventsError_.c_str());
            return;
        }
        DrawEventCommands();
        DrawTimeline();
        DrawSelectedEvent();
        DrawSounds();
        if (!eventsError_.empty()) ImGui::TextColored(ImVec4(1, 0.4F, 0.4F, 1), "%s", eventsError_.c_str());
        else if (!eventsStatus_.empty()) ImGui::TextUnformatted(eventsStatus_.c_str());
    }

    void DrawEventCommands() {
        ImGui::Text("%s%s", eventsId_.empty() ? "(new file)" : eventsId_.c_str(), history_->Unsaved() ? "  * unsaved" : "");
        if (eventsPath_.empty()) {
            InputString("New id", newId_);
            InputString("New path (in the content root)", newPath_);
            InputString("Catalog", newCatalog_);
        }
        // No shortcut while a field is typed in or a marker is held.
        const bool typing = ImGui::GetIO().WantTextInput || ImGui::IsAnyItemActive();
        ImGui::BeginDisabled(!history_->CanUndo());
        if (ImGui::Button("Undo") || (!typing && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z))) Undo();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!history_->CanRedo());
        if (ImGui::Button("Redo") || (!typing && (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z) ||
                                                  ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y)))) {
            Redo();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Add at playhead")) {
            CommitPending();
            const std::size_t added = history_->Add(CurrentClipName(), {session_.clock.seconds, "event"});
            ForgetIndices();
            selected_ = added;
        }
        ImGui::SameLine();
        if (ImGui::Button(eventsPath_.empty() ? "Save as new file" : "Save") ||
            (!typing && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))) {
            CommitPending();
            Save();
        }
        ImGui::SameLine();
        if (ImGui::Button("Close file")) {
            Guarded([this] {
                auto candidates = std::move(candidates_);
                CloseEvents();
                candidates_ = std::move(candidates);
            });
        }
    }

    // A field's typed value pending when an Undo or Redo comes is dropped with the indices.
    void Undo() {
        if (history_->Undo()) ForgetIndices();
    }
    void Redo() {
        if (history_->Redo()) ForgetIndices();
    }

    // A name or seconds typed into the selected event's fields, committed when
    // the selection moves away before the field was left.
    void CommitPending() {
        const auto* track = CurrentTrack();
        if (!history_ || !editingIndex_ || track == nullptr || *editingIndex_ >= track->events.size()) {
            namePending_ = secondsPending_ = false;
            return;
        }
        const std::size_t index = *editingIndex_;
        if (namePending_) history_->Rename(CurrentClipName(), index, editingName_);
        if (secondsPending_) selected_ = history_->Move(CurrentClipName(), index, editingSeconds_);
        namePending_ = secondsPending_ = false;
        flashes_.clear();
    }

    // The clip's line: a marker per event, the playhead, drag to move (stops at
    // the clip's ends), click to select.
    void DrawTimeline() {
        const double duration = session_.clock.durationSeconds;
        const float width = std::max(ImGui::GetContentRegionAvail().x, 50.0F);
        const float height = 34.0F;
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("timeline", ImVec2(width, height));
        const bool hovered = ImGui::IsItemHovered();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), IM_COL32(40, 42, 48, 255));
        const auto toX = [&](double seconds) {
            return origin.x + static_cast<float>(duration > 0.0 ? seconds / duration : 0.0) * width;
        };
        const auto toSeconds = [&](float x) {
            return duration > 0.0 ? static_cast<double>((x - origin.x) / width) * duration : 0.0;
        };
        const float playhead = toX(session_.clock.seconds);
        draw->AddLine(ImVec2(playhead, origin.y), ImVec2(playhead, origin.y + height), IM_COL32(230, 230, 230, 255));
        const auto* track = CurrentTrack();
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        std::optional<std::size_t> under;
        if (track != nullptr) {
            for (std::size_t index = 0; index < track->events.size(); ++index) {
                double seconds = track->events[index].seconds;
                if (dragging_ && dragging_->index == index) seconds = dragging_->seconds;
                const float x = toX(seconds);
                const bool lit = std::any_of(flashes_.begin(), flashes_.end(), [&](const auto& f) { return f.first == index; });
                const bool chosen = selected_ && *selected_ == index;
                const ImU32 colour = lit ? IM_COL32(255, 230, 60, 255) : chosen ? IM_COL32(90, 200, 255, 255) : IM_COL32(200, 120, 60, 255);
                draw->AddTriangleFilled(ImVec2(x, origin.y + 4), ImVec2(x - 6, origin.y + height - 4),
                                        ImVec2(x + 6, origin.y + height - 4), colour);
                if (hovered && std::abs(mouse.x - x) <= 6.0F) under = index;
            }
        }
        if (hovered && under) ImGui::SetTooltip("%s  %.3f s", track->events[*under].name.c_str(), track->events[*under].seconds);
        if (ImGui::IsItemActivated()) {
            if (under) {
                if (selected_ != under) CommitPending();
                selected_ = under;
                dragging_ = Drag{*under, track->events[*under].seconds, track->events[*under].seconds - toSeconds(mouse.x)};
            } else {
                session_.clock.Seek(DragSeconds(toSeconds(mouse.x), duration));
            }
        }
        // A click selects; only a drag past ImGui's threshold moves, keeping where the marker was grabbed.
        if (ImGui::IsItemActive() && dragging_) {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                dragging_->seconds = DragSeconds(toSeconds(mouse.x) + dragging_->grab, duration);
            }
        } else if (ImGui::IsItemActive()) {
            session_.clock.Seek(DragSeconds(toSeconds(mouse.x), duration));
        }
        if (ImGui::IsItemDeactivated() && dragging_) {
            // One undo step per drag, at the moment the marker was let go.
            const Drag drag = *dragging_;
            dragging_.reset();
            if (track != nullptr && drag.index < track->events.size()) {
                const std::size_t moved = history_->Move(CurrentClipName(), drag.index, drag.seconds);
                ForgetIndices();
                selected_ = moved;
            }
        }
    }

    void DrawSelectedEvent() {
        const auto* track = CurrentTrack();
        if (track == nullptr || !selected_ || *selected_ >= track->events.size()) return;
        const std::size_t index = *selected_;
        const auto& event = track->events[index];
        // The fields show the selected event; what was typed for another one is committed first.
        if (editingIndex_ != index) {
            CommitPending();
            const auto* now = CurrentTrack();
            if (now == nullptr || !selected_ || *selected_ >= now->events.size()) return;
            editingName_ = now->events[*selected_].name;
            editingSeconds_ = now->events[*selected_].seconds;
            editingIndex_ = selected_;
            return; // drawn from the next frame on, with the fields of this event
        }
        if (!namePending_) editingName_ = event.name;
        if (!secondsPending_) editingSeconds_ = event.seconds;
        ImGui::PushID(static_cast<int>(index));
        ImGui::SetNextItemWidth(220);
        if (InputString("Name", editingName_)) namePending_ = true;
        if (ImGui::IsItemDeactivatedAfterEdit()) CommitPending();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140);
        // Typed seconds are not clamped: Engine refuses one outside the clip when saving.
        if (ImGui::InputDouble("Seconds", &editingSeconds_, 0.0, 0.0, "%.6f")) secondsPending_ = true;
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            CommitPending();
            editingIndex_ = selected_;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
            namePending_ = secondsPending_ = false;
            history_->Remove(CurrentClipName(), index);
            ForgetIndices();
        }
        ImGui::PopID();
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
    // The events of the opened model (-events).
    struct Drag final {
        std::size_t index{};
        double seconds{};
        double grab{}; // the marker's seconds minus the mouse's when grabbed
    };
    std::vector<EventsCandidate> candidates_;
    std::optional<EventHistory> history_;
    // The opened events' gyo.event_sounds (-event-sounds).
    std::unique_ptr<PreviewSounds> audio_;
    std::vector<EventSoundsCandidate> soundCandidates_;
    std::optional<OpenedEventSounds> sounds_;
    std::optional<Framework::BoundEventSounds> bound_;
    std::map<std::string, double, std::less<>> soundSeconds_;
    std::string soundsId_;
    std::string soundsStatus_;
    std::string soundsError_;
    bool soundsUnsaved_{};
    SoundPlayback playback_;
    std::uint64_t soundsVersion_{};
    std::optional<std::size_t> soundsClip_;
    std::string audioWarning_;
    std::string newSoundsId_;
    std::string newSoundsPath_;
    std::string newSoundsCatalog_;
    Engine::IO::Helpers::FileStamp stamp_{};
    std::string eventsPath_;
    std::string eventsId_;
    std::string eventsError_;
    std::string eventsStatus_;
    std::string newId_;
    std::string newPath_;
    std::string newCatalog_;
    std::optional<std::size_t> selected_;
    std::optional<Drag> dragging_;
    std::vector<std::pair<std::size_t, double>> flashes_; // a lit marker and its seconds left
    std::string editingName_;
    double editingSeconds_{};
    std::optional<std::size_t> editingIndex_;
    bool namePending_{};
    bool secondsPending_{};
    std::optional<std::string> contentCatalog_;
    std::function<void()> pendingAction_;
    bool openDiscard_{};
    bool wantsExit_{};
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
