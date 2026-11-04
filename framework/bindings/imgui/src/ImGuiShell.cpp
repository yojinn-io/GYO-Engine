#include "gyo/framework/imgui/ImGuiShell.hpp"

#include <array>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include "engine/base/Assert.hpp"
#include "engine/base/Error.hpp"
#include "engine/base/Log.hpp"
#include "engine/input/PhysicalInputFrame.hpp"
#include "engine/input/backend/sdl/SdlInput.hpp"
#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/os/Files.hpp"
#include "engine/platform/sdl/SdlPlatformNative.hpp"
#include "engine/render/backend/sdl/SdlRendererNative.hpp"

namespace Gyo::Tools::Framework {
namespace {

using Engine::Base::LogLevel;
using Engine::Platform::Sdl::PlatformEvent;
using Engine::Platform::Sdl::SdlPlatform;
using Engine::Render::Backend::Sdl::Color;
using Engine::Render::Backend::Sdl::SdlRenderer;
using Engine::Runtime::FrameContext;
using Engine::Runtime::RuntimeControl;

constexpr const char* kLogCategory = "framework.imgui";
constexpr float kChromeFontSize = 16.0F;

// The font files the chrome may use, by priority, relative to a font
// directory from engine/os. One list serves every platform: each file exists
// only where its platform installs it (Windows, macOS, then Linux names;
// os-subsystem D56).
constexpr std::array kPrimaryFontNames{
    std::string_view{"msjh.ttc"},
    std::string_view{"msyh.ttc"},
    std::string_view{"simsun.ttc"},
    std::string_view{"PingFang.ttc"},
    std::string_view{"STHeiti Medium.ttc"},
    std::string_view{"opentype/noto/NotoSansCJK-Regular.ttc"},
    std::string_view{"truetype/noto/NotoSansCJK-Regular.ttc"},
    std::string_view{"truetype/wqy/wqy-zenhei.ttc"},
};
constexpr std::array kJapaneseFontNames{
    std::string_view{"YuGothM.ttc"},
    std::string_view{"meiryo.ttc"},
    std::string_view{"msgothic.ttc"},
    std::string_view{"ヒラギノ角ゴシック W3.ttc"},
    std::string_view{"ヒラギノ丸ゴ ProN W4.ttc"},
};

void Log(const LogLevel level, const std::string& title, const std::string& text) noexcept {
    try {
        Engine::Base::Log(level, kLogCategory, title + ": " + text);
    } catch (...) {
        // A log line is never worth a failure.
    }
}

// The first name that is a regular file, trying every name in a directory
// before the next directory (system directories come first). UTF-8, as ImGui
// opens it.
[[nodiscard]] std::string FirstExistingFont(const std::span<const std::string> directories,
                                            const std::span<const std::string_view> fileNames) {
    Engine::IO::FS::NativeFileSystem files;
    for (const std::string& directory : directories) {
        for (const std::string_view fileName : fileNames) {
            std::string candidate = directory + "/" + std::string(fileName);
            const auto uri = Engine::IO::FS::NativeFileUri(candidate);
            if (!uri) {
                continue;
            }
            const auto info = files.Stat(uri.value());
            if (info && info.value().IsFile()) {
                return candidate;
            }
        }
    }
    return {};
}

// The chrome is operating-system UI, not game content: it uses a system CJK
// font and never reaches into an asset root; content a client previews brings
// its own fonts.
void ConfigureChromeFont(ImGuiIO& io, const std::string& title) {
    std::vector<std::string> directories;
    if (auto found = Engine::Os::Files::FontDirectories()) {
        directories = std::move(found.value());
    } else {
        Log(LogLevel::Warning, title, "cannot list the font directories: " + Engine::Base::Describe(found.error()));
    }
    const std::string primary = FirstExistingFont(directories, kPrimaryFontNames);
    const std::string japaneseFallback = FirstExistingFont(directories, kJapaneseFontNames);

    ImFont* chromeFont = nullptr;
    if (!primary.empty()) {
        chromeFont = io.Fonts->AddFontFromFileTTF(primary.c_str(), kChromeFontSize);
        if (chromeFont != nullptr) {
            io.FontDefault = chromeFont;
            Log(LogLevel::Info, title, "chrome font: " + primary);
        }
    }
    if (chromeFont != nullptr && !japaneseFallback.empty() && japaneseFallback != primary) {
        ImFontConfig merge;
        merge.MergeMode = true;
        if (io.Fonts->AddFontFromFileTTF(japaneseFallback.c_str(), kChromeFontSize, &merge) != nullptr) {
            Log(LogLevel::Info, title, "Japanese font fallback: " + japaneseFallback);
        }
    }
    if (chromeFont == nullptr) {
        io.Fonts->AddFontDefault();
        Log(LogLevel::Warning, title, "no system CJK font was found; the chrome uses ImGui's Latin-only font");
    }
}

// The byte offset of a code point index in UTF-8 text (clamped to its end).
[[nodiscard]] std::size_t ByteOffset(const std::string_view text, const std::int32_t codePoints) noexcept {
    std::size_t offset = 0;
    for (std::int32_t count = 0; offset < text.size() && count < codePoints; ++count) {
        ++offset;
        while (offset < text.size() && (static_cast<unsigned char>(text[offset]) & 0xC0U) == 0x80U) {
            ++offset;
        }
    }
    return offset;
}

// The input method's composition, drawn at ImGui's text cursor
// (v8-tool-ui-editor-ime-enter). ImGui's SDL3 backend ignores the composition
// and ImGui draws none, so where the program has to draw it (macOS always;
// Windows only when SdlPlatformOptions::imeComposition asks, which the shell
// leaves off) typed text stayed invisible until it was committed. The text
// comes from Engine's input backend, the one reading of SDL's text events
// (Design Invariant 2); the place is the one ImGui reports to the platform.
// Display only: it follows the events and decides nothing (AGENTS.md
// section 3).
class ImeComposition final {
public:
    ImeComposition() = default;
    ImeComposition(const ImeComposition&) = delete;
    ImeComposition& operator=(const ImeComposition&) = delete;

    // After ImGui's SDL3 backend installed its handler, which it clears again
    // when it shuts down.
    void Install() {
        ImGuiPlatformIO& platformIo = ImGui::GetPlatformIO();
        backend_ = platformIo.Platform_SetImeDataFn;
        platformIo.Platform_ImeUserData = this;
        platformIo.Platform_SetImeDataFn = &ImeComposition::SetImeData;
    }

    // One frame of the input backend: the latest composition wins; committed
    // text, a composition that ended, focus loss or ImGui no longer asking for
    // the input method clear it.
    void Take(const Engine::Input::PhysicalInputFrame& frame) {
        for (const Engine::Input::TextEvent& event : frame.text) {
            if (event.kind == Engine::Input::TextEvent::Kind::Composition) {
                text_ = event.text;
                cursor_ = event.cursor;
                selectionLength_ = event.selectionLength;
            } else if (event.kind == Engine::Input::TextEvent::Kind::Text) {
                text_.clear();
            }
        }
        for (const Engine::Input::InputEvent& event : frame.events) {
            if (event.kind == Engine::Input::InputEventKind::WindowFocusLost) {
                text_.clear();
            }
        }
        if (!reported_.WantVisible) {
            text_.clear();
        }
    }

    // On the foreground list, over the field: the text on an opaque ground,
    // underlined, the selected span thicker, the cursor where it is.
    void Draw() const {
        if (text_.empty()) {
            return;
        }
        ImDrawList* list = ImGui::GetForegroundDrawList();
        ImFont* font = ImGui::GetFont();
        const float size = reported_.InputLineHeight > 0.0F ? reported_.InputLineHeight : ImGui::GetFontSize();
        // ImGui reports one pixel left of its cursor (imgui_widgets.cpp).
        const ImVec2 origin{reported_.InputPos.x + 1.0F, reported_.InputPos.y};
        const auto widthTo = [&](const std::size_t offset) {
            return font->CalcTextSizeA(size, FLT_MAX, 0.0F, text_.data(), text_.data() + offset).x;
        };
        const float width = widthTo(text_.size());
        const float bottom = origin.y + size;
        list->AddRectFilled(origin, {origin.x + width, bottom}, ImGui::GetColorU32(ImGuiCol_PopupBg));
        list->AddText(font, size, origin, ImGui::GetColorU32(ImGuiCol_Text), text_.data(), text_.data() + text_.size());
        const ImU32 line = ImGui::GetColorU32(ImGuiCol_Text);
        list->AddLine({origin.x, bottom - 1.0F}, {origin.x + width, bottom - 1.0F}, line, 1.0F);
        if (cursor_ >= 0 && selectionLength_ > 0) {
            const float from = widthTo(ByteOffset(text_, cursor_));
            const float to = widthTo(ByteOffset(text_, cursor_ + selectionLength_));
            list->AddLine({origin.x + from, bottom - 1.0F}, {origin.x + to, bottom - 1.0F}, line, 3.0F);
        } else if (cursor_ >= 0) {
            const float at = origin.x + widthTo(ByteOffset(text_, cursor_));
            list->AddLine({at, origin.y}, {at, bottom - 1.0F}, ImGui::GetColorU32(ImGuiCol_InputTextCursor), 1.0F);
        }
    }

    // What ImGui last told its SDL3 backend.
    [[nodiscard]] const ImGuiPlatformImeData& Reported() const noexcept { return reported_; }

    // Tells the backend again what ImGui last told it, so that it places text
    // input for ImGui's field after the client had it.
    void Report() {
        if (backend_ != nullptr) {
            backend_(ImGui::GetCurrentContext(), ImGui::GetMainViewport(), &reported_);
        }
    }

private:
    using SetImeDataFn = void (*)(ImGuiContext*, ImGuiViewport*, ImGuiPlatformImeData*);

    static void SetImeData(ImGuiContext* context, ImGuiViewport* viewport, ImGuiPlatformImeData* data) {
        auto* self = static_cast<ImeComposition*>(ImGui::GetPlatformIO().Platform_ImeUserData);
        self->reported_ = *data;
        if (self->backend_ != nullptr) {
            self->backend_(context, viewport, data);
        }
    }

    SetImeDataFn backend_{};
    ImGuiPlatformImeData reported_{};
    std::string text_;
    std::int32_t cursor_{-1};
    std::int32_t selectionLength_{-1};
};

// ImGui's context and its two SDL backends, shut down in reverse when it goes.
class ImGuiScope final {
public:
    ImGuiScope() = default;
    ImGuiScope(const ImGuiScope&) = delete;
    ImGuiScope& operator=(const ImGuiScope&) = delete;
    ~ImGuiScope() {
        if (rendererBackend_) {
            ImGui_ImplSDLRenderer3_Shutdown();
        }
        if (platformBackend_) {
            ImGui_ImplSDL3_Shutdown();
        }
        if (context_) {
            ImGui::DestroyContext();
        }
    }

    [[nodiscard]] bool Start(const SdlPlatform& platform, const SdlRenderer& renderer, const std::string& title) {
        // One ImGui shell at a time per process: the backends keep global state.
        GYO_ASSERT(ImGui::GetCurrentContext() == nullptr);
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        context_ = true;

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        // The binding writes no side file (Design Invariant 4): no window
        // state next to the program, no ImGui log.
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        ConfigureChromeFont(io, title);
        ImGui::StyleColorsDark();

        auto* const window = Engine::Platform::Sdl::NativeWindow(platform);
        auto* const nativeRenderer = Engine::Render::Backend::Sdl::NativeRenderer(renderer);
        if (!ImGui_ImplSDL3_InitForSDLRenderer(window, nativeRenderer)) {
            Log(LogLevel::Error, title, "ImGui's SDL3 platform backend could not start");
            return false;
        }
        platformBackend_ = true;
        composition_.Install();
        if (!ImGui_ImplSDLRenderer3_Init(nativeRenderer)) {
            Log(LogLevel::Error, title, "ImGui's SDL renderer backend could not start");
            return false;
        }
        rendererBackend_ = true;
        return true;
    }

    [[nodiscard]] ImeComposition& Composition() noexcept { return composition_; }

private:
    // Outlives the context: the members go after the destructor's shutdown.
    ImeComposition composition_;
    bool context_{};
    bool platformBackend_{};
    bool rendererBackend_{};
};

// The client's side of the window's input (ImGuiShellInput,
// v8-input-ui-adapter). Text input has one owner at a time: the client while
// it asks for it, otherwise ImGui's SDL3 backend for ImGui's own fields. The
// client's request is applied after ImGui's frame ended, when the backend has
// done what ImGui asked, so the client's wins within a frame.
class ShellInput final : public ImGuiShellInput {
public:
    ShellInput(Engine::Input::Backend::Sdl::SdlInput& input, const std::string& title) : input_(input), title_(title) {}

    [[nodiscard]] const Engine::Input::PhysicalInputFrame& Frame() const override { return frame_; }

    void RequestTextInput(const Engine::Platform::Sdl::Rect& area, const int cursorPixels) override {
        requested_ = Request{area, cursorPixels};
    }

    // Between the backend's EndFrame and its next BeginFrame, which clears it.
    void Take() {
        frame_ = input_.Snapshot();
        requested_.reset();
    }

    // After ImGui's frame ended. While the client asks, its request is applied
    // every frame: ImGui's backend also starts, stops and places text input,
    // in its NewFrame too (on keyboard focus changes) without telling ImGui's
    // platform hook, so no copy of SDL's state here could say a call is not
    // needed. Starting text input that is on does nothing in SDL.
    void Apply(ImeComposition& imgui) {
        if (requested_) {
            Check(input_.SetTextInput(true), "turn text input on");
            Check(input_.SetTextInputArea(requested_->area, requested_->cursorPixels), "set the text input area");
            owned_ = true;
            return;
        }
        if (!owned_) {
            return;
        }
        owned_ = false;
        const ImGuiPlatformImeData& reported = imgui.Reported();
        if (reported.WantVisible || reported.WantTextInput) {
            // Back to ImGui's field: its backend places text input again.
            imgui.Report();
        } else {
            Check(input_.SetTextInput(false), "turn text input off");
        }
    }

private:
    struct Request {
        Engine::Platform::Sdl::Rect area;
        int cursorPixels{};
    };

    void Check(const Engine::Base::Result<void, Engine::Input::Backend::Sdl::SdlInputError>& result,
               const char* what) const {
        if (!result) {
            Log(LogLevel::Warning, title_, std::string{"could not "} + what + ": " +
                                               Engine::Base::Describe(result.error()));
        }
    }

    Engine::Input::Backend::Sdl::SdlInput& input_;
    const std::string& title_;
    Engine::Input::PhysicalInputFrame frame_;
    std::optional<Request> requested_;
    // The client asked last frame: text input goes back to ImGui when it stops.
    bool owned_{};
};

// The windowed shell's client for an ImGui window. Members are destroyed in
// reverse: ImGui, then the client, then the renderer.
class ImGuiWindowClient final : public WindowShellClient {
public:
    ImGuiWindowClient(SdlPlatform& platform, std::string title)
        : platform_(platform), title_(std::move(title)), input_(platform), shellInput_(input_, title_) {
        input_.BeginFrame();
    }

    [[nodiscard]] bool Start(const ImGuiShellClientFactory& factory) {
        auto created = SdlRenderer::Create(platform_);
        if (!created) {
            Log(LogLevel::Error, title_, "renderer initialization failed: " + Engine::Base::Describe(created.error()));
            return false;
        }
        renderer_ = std::move(created).value();
        context_.emplace(ImGuiShellContext{platform_, *renderer_, shellInput_});
        client_ = factory(*context_);
        if (client_ == nullptr) {
            Log(LogLevel::Error, title_, "there is no client to run");
            return false;
        }
        if (!imgui_.Start(platform_, *renderer_, title_)) {
            return false;
        }
        return client_->Initialize(*context_);
    }

    void OnEvent(const PlatformEvent& event) override {
        input_.HandleEvent(event);
        ImGui_ImplSDL3_ProcessEvent(&Engine::Platform::Sdl::NativeEvent(event));
    }

    RuntimeControl Update(const FrameContext&) override {
        input_.EndFrame();
        shellInput_.Take();
        imgui_.Composition().Take(shellInput_.Frame());
        input_.BeginFrame();
        client_->BeginFrame();
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        client_->Draw();
        imgui_.Composition().Draw();
        if (client_->WantsExit()) {
            ImGui::EndFrame();
            client_->EndFrame();
            return RuntimeControl::Stop;
        }
        return RuntimeControl::Continue;
    }

    RuntimeControl Render(const FrameContext&) override {
        ImGui::Render();
        shellInput_.Apply(imgui_.Composition());
        auto cleared = renderer_->Clear(Color{12, 14, 18, 255});
        if (!cleared) {
            client_->EndFrame();
            Log(LogLevel::Error, title_, "clear failed: " + Engine::Base::Describe(cleared.error()));
            exitCode_ = 1;
            return RuntimeControl::Stop;
        }
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(),
                                              Engine::Render::Backend::Sdl::NativeRenderer(*renderer_));
        auto presented = renderer_->Present();
        client_->EndFrame();
        if (!presented) {
            Log(LogLevel::Error, title_, "present failed: " + Engine::Base::Describe(presented.error()));
            exitCode_ = 1;
            return RuntimeControl::Stop;
        }
        return RuntimeControl::Continue;
    }

    [[nodiscard]] bool CloseRequested() override { return client_ == nullptr || client_->CloseRequested(); }

    [[nodiscard]] int ExitCode() const noexcept override { return exitCode_; }

private:
    SdlPlatform& platform_;
    std::string title_;
    // The one reading of SDL's input events: ImGui's composition overlay reads
    // its text, the client the whole frame.
    Engine::Input::Backend::Sdl::SdlInput input_;
    ShellInput shellInput_;
    std::unique_ptr<SdlRenderer> renderer_;
    std::optional<ImGuiShellContext> context_;
    std::unique_ptr<ImGuiShellClient> client_;
    ImGuiScope imgui_;
    int exitCode_{};
};

} // namespace

// ImGui's SDL renderer backend reads an ImTextureID as the native texture pointer.
static_assert(sizeof(ImTextureID) == sizeof(std::uint64_t) && sizeof(void*) <= sizeof(ImTextureID));

std::optional<std::uint64_t> ImGuiTextureId(const Engine::Render::Backend::Sdl::SdlRenderer& renderer,
                                            Engine::Render::Backend::Sdl::SdlTextureHandle texture) {
    auto* native = Engine::Render::Backend::Sdl::NativeTexture(renderer, texture);
    if (native == nullptr) {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(native));
}

int RunImGuiShell(const WindowShellOptions& options, const ImGuiShellClientFactory& factory) {
    GYO_ASSERT(factory);
    return RunWindowShell(options, [&](SdlPlatform& platform) -> std::unique_ptr<WindowShellClient> {
        auto client = std::make_unique<ImGuiWindowClient>(platform, options.platform.title);
        if (!client->Start(factory)) {
            return nullptr;
        }
        return client;
    });
}

} // namespace Gyo::Tools::Framework
