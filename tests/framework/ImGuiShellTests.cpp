// The ImGui binding (docs/framework/architecture.md, Design Invariants 4 and
// 6), on SDL's dummy video driver and software renderer, in a working
// directory of its own.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "gyo/framework/imgui/ImGuiShell.hpp"

#include "engine/platform/sdl/SdlPlatformNative.hpp"
#include "engine/render/backend/sdl/SdlRendererNative.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace Gyo::Tools::Framework;

namespace {

WindowShellOptions SmallWindow() {
    WindowShellOptions options;
    options.platform.title = "imgui shell test";
    options.platform.width = 640;
    options.platform.height = 480;
    return options;
}

struct Counts {
    int initialize{};
    int beginFrame{};
    int draw{};
    int endFrame{};
    bool contextAtInitialize{};
    bool contextInDestructor{true};
    bool rendererInDestructor{};
    bool destroyed{};
};

// Draws one window per frame and wants to exit after exitAfter frames
// (never when 0). Hooks let a case act at a given point.
class Client : public ImGuiShellClient {
public:
    Client(const ImGuiShellContext& context, Counts& counts, int exitAfter)
        : context_(context), counts_(counts), exitAfter_(exitAfter) {}
    ~Client() override {
        counts_.contextInDestructor = ImGui::GetCurrentContext() != nullptr;
        counts_.rendererInDestructor =
            SDL_GetRenderer(Engine::Platform::Sdl::NativeWindow(context_.platform)) != nullptr;
        counts_.destroyed = true;
    }
    bool Initialize(const ImGuiShellContext&) override {
        ++counts_.initialize;
        counts_.contextAtInitialize = ImGui::GetCurrentContext() != nullptr;
        return initializeResult;
    }
    void BeginFrame() override {
        ++counts_.beginFrame;
        if (onBeginFrame) {
            onBeginFrame(counts_.beginFrame);
        }
    }
    void Draw() override {
        ++counts_.draw;
        ImGui::Begin("shell test");
        ImGui::TextUnformatted("text");
        ImGui::End();
        if (onDraw) {
            onDraw(counts_.draw);
        }
    }
    bool WantsExit() const override { return exitAfter_ != 0 && counts_.draw >= exitAfter_; }
    void EndFrame() override { ++counts_.endFrame; }

    bool initializeResult{true};
    std::function<void(int)> onBeginFrame;
    std::function<void(int)> onDraw;

protected:
    ImGuiShellContext context_;
    Counts& counts_;
    int exitAfter_;
};

void PushInjected(const ImGuiShellContext& context, SDL_Event& event) {
    REQUIRE(Engine::Platform::Sdl::PushInjectedEvent(context.platform, event));
}

SDL_WindowID OwnWindow(const ImGuiShellContext& context) {
    return SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(context.platform));
}

} // namespace

TEST_CASE("I1: three frames, then the client's exit request returns 0; every BeginFrame has its EndFrame") {
    Counts counts;
    const int code = RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
        return std::make_unique<Client>(context, counts, 3);
    });
    CHECK(code == 0);
    CHECK(counts.initialize == 1);
    CHECK(counts.beginFrame == 3);
    CHECK(counts.draw == 3);
    CHECK(counts.endFrame == 3);
}

TEST_CASE("I2: the binding writes no imgui.ini and no ImGui log") {
    std::filesystem::remove("imgui.ini");
    std::filesystem::remove("imgui_log.txt");
    Counts counts;
    bool noIni = false;
    bool noLog = false;
    const int code = RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
        auto client = std::make_unique<Client>(context, counts, 3);
        client->onDraw = [&](int) {
            noIni = ImGui::GetIO().IniFilename == nullptr;
            noLog = ImGui::GetIO().LogFilename == nullptr;
            ImGui::LogToFile();
            ImGui::LogText("logged");
            ImGui::LogFinish();
        };
        return client;
    });
    CHECK(code == 0);
    CHECK(noIni);
    CHECK(noLog);
    CHECK_FALSE(std::filesystem::exists("imgui.ini"));
    CHECK_FALSE(std::filesystem::exists("imgui_log.txt"));
}

TEST_CASE("I3: ImGui is shut down before the client goes, and the renderer is still there") {
    Counts counts;
    const int code = RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
        return std::make_unique<Client>(context, counts, 1);
    });
    CHECK(code == 0);
    CHECK(counts.destroyed);
    CHECK_FALSE(counts.contextInDestructor);
    CHECK(counts.rendererInDestructor);
}

TEST_CASE("I4: the factory runs before ImGui, Initialize after it; no client or a failed Initialize returns 1") {
    bool contextAtFactory = true;
    Counts counts;
    CHECK(RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
              contextAtFactory = ImGui::GetCurrentContext() != nullptr;
              return std::make_unique<Client>(context, counts, 1);
          }) == 0);
    CHECK_FALSE(contextAtFactory);
    CHECK(counts.contextAtInitialize);

    CHECK(RunImGuiShell(SmallWindow(), [](const ImGuiShellContext&) { return nullptr; }) == 1);

    Counts failed;
    CHECK(RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
              auto client = std::make_unique<Client>(context, failed, 1);
              client->initializeResult = false;
              return client;
          }) == 1);
    CHECK(failed.initialize == 1);
    CHECK(failed.beginFrame == 0);
}

TEST_CASE("I5: the window's events reach ImGui") {
    Counts counts;
    ImVec2 seen{};
    const int code = RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
        auto client = std::make_unique<Client>(context, counts, 2);
        client->onBeginFrame = [&context](int frame) {
            if (frame != 1) {
                return;
            }
            SDL_Event motion{};
            motion.type = SDL_EVENT_MOUSE_MOTION;
            motion.motion.windowID = OwnWindow(context);
            motion.motion.x = 37.0F;
            motion.motion.y = 41.0F;
            PushInjected(context, motion);
        };
        client->onDraw = [&](int frame) {
            if (frame == 2) {
                seen = ImGui::GetIO().MousePos;
            }
        };
        return client;
    });
    CHECK(code == 0);
    CHECK(seen.x == doctest::Approx(37.0));
    CHECK(seen.y == doctest::Approx(41.0));
}

TEST_CASE("I6: the frame is drawn: the font atlas reaches the renderer before the next frame") {
    Counts counts;
    int statusAtFrame2 = -1;
    int statusAtFrame3 = -1;
    const int code = RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
        auto client = std::make_unique<Client>(context, counts, 3);
        client->onBeginFrame = [&](int frame) {
            ImTextureData* atlas = ImGui::GetIO().Fonts->TexData;
            const int status = atlas == nullptr ? -2 : static_cast<int>(atlas->Status);
            if (frame == 2) {
                statusAtFrame2 = status;
            } else if (frame == 3) {
                statusAtFrame3 = status;
            }
        };
        return client;
    });
    CHECK(code == 0);
    CHECK(statusAtFrame2 == static_cast<int>(ImTextureStatus_OK));
    CHECK(statusAtFrame3 == static_cast<int>(ImTextureStatus_OK));
}

TEST_CASE("I7: closing the window returns 0") {
    Counts counts;
    const int code = RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
        auto client = std::make_unique<Client>(context, counts, 0);
        client->onBeginFrame = [&context](int frame) {
            if (frame != 2) {
                return;
            }
            SDL_Event close{};
            close.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
            close.window.windowID = OwnWindow(context);
            PushInjected(context, close);
        };
        return client;
    });
    CHECK(code == 0);
    CHECK(counts.beginFrame == 2);
    CHECK(counts.endFrame == 2);
}

TEST_CASE("I9: the options reach the platform as given") {
    Counts counts;
    Engine::Platform::Sdl::Rect rect{};
    const int code = RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
        rect = context.platform.QueryWindow().rect;
        return std::make_unique<Client>(context, counts, 1);
    });
    CHECK(code == 0);
    CHECK(rect.width == 640);
    CHECK(rect.height == 480);
}

namespace {

// What the foreground list held at the start of each frame (the previous
// frame's drawing), and where the edited field was.
struct CompositionRun {
    std::vector<int> vertices;
    std::vector<ImVec2> lowest;
    std::vector<ImVec2> highest;
    ImVec2 fieldMin{};
    ImVec2 fieldMax{};
};

// v8-tool-ui-editor-ime-enter: an InputText is edited from frame 2 on (until
// stopEditingAt, when it is no longer drawn); each entry of events is pushed
// at the start of its frame and reaches ImGui before the next frame.
CompositionRun RunComposition(const std::vector<std::pair<int, std::function<void(SDL_Event&, SDL_WindowID)>>>& events,
                              const int frames, const int stopEditingAt = 0) {
    Counts counts;
    CompositionRun run;
    std::array<char, 64> buffer{};
    const int code = RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
        auto client = std::make_unique<Client>(context, counts, frames);
        client->onBeginFrame = [&](const int frame) {
            const ImDrawList* foreground = ImGui::GetForegroundDrawList();
            run.vertices.push_back(foreground->VtxBuffer.Size);
            ImVec2 low{1.0e9F, 1.0e9F};
            ImVec2 high{-1.0e9F, -1.0e9F};
            for (const ImDrawVert& vertex : foreground->VtxBuffer) {
                low = {std::min(low.x, vertex.pos.x), std::min(low.y, vertex.pos.y)};
                high = {std::max(high.x, vertex.pos.x), std::max(high.y, vertex.pos.y)};
            }
            run.lowest.push_back(low);
            run.highest.push_back(high);
            for (const auto& [at, make] : events) {
                if (at == frame) {
                    SDL_Event event{};
                    make(event, OwnWindow(context));
                    PushInjected(context, event);
                }
            }
        };
        client->onDraw = [&](const int frame) {
            if (stopEditingAt != 0 && frame >= stopEditingAt) {
                return;
            }
            ImGui::Begin("ime field");
            if (frame == 1) {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::InputText("field", buffer.data(), buffer.size());
            run.fieldMin = ImGui::GetItemRectMin();
            run.fieldMax = ImGui::GetItemRectMax();
            ImGui::End();
        };
        return client;
    });
    CHECK(code == 0);
    return run;
}

void Composing(SDL_Event& event, const SDL_WindowID window) {
    event.type = SDL_EVENT_TEXT_EDITING;
    event.edit.windowID = window;
    event.edit.text = "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"; // 日本語
    event.edit.start = 0;
    event.edit.length = 3;
}

} // namespace

TEST_CASE("I10: the input method's composition is drawn at the edited field until it ends") {
    const auto committed = [](SDL_Event& event, const SDL_WindowID window) {
        event.type = SDL_EVENT_TEXT_INPUT;
        event.text.windowID = window;
        event.text.text = "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e";
    };
    const auto ended = [](SDL_Event& event, const SDL_WindowID window) {
        event.type = SDL_EVENT_TEXT_EDITING;
        event.edit.windowID = window;
        event.edit.text = "";
    };
    const auto focusLost = [](SDL_Event& event, const SDL_WindowID window) {
        event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        event.window.windowID = window;
    };

    SUBCASE("drawn inside the field, then gone once the text is committed") {
        const CompositionRun run = RunComposition({{5, &Composing}, {8, committed}}, 11);
        // run.vertices[i] is what frame i drew (read at the start of frame i + 1).
        CHECK(run.vertices[4] == 0);
        REQUIRE(run.vertices[6] > 0);
        CHECK(run.lowest[6].x >= run.fieldMin.x);
        CHECK(run.highest[6].x <= run.fieldMax.x);
        CHECK(run.lowest[6].y >= run.fieldMin.y - 1.0F);
        CHECK(run.highest[6].y <= run.fieldMax.y + 1.0F);
        CHECK(run.vertices[9] == 0);
    }
    SUBCASE("gone when the composition ends without text") {
        const CompositionRun run = RunComposition({{5, &Composing}, {8, ended}}, 11);
        REQUIRE(run.vertices[6] > 0);
        CHECK(run.vertices[9] == 0);
    }
    SUBCASE("gone when the window loses focus") {
        const CompositionRun run = RunComposition({{5, &Composing}, {8, focusLost}}, 11);
        REQUIRE(run.vertices[6] > 0);
        CHECK(run.vertices[9] == 0);
    }
    SUBCASE("gone when ImGui stops editing") {
        const CompositionRun run = RunComposition({{5, &Composing}}, 11, 8);
        REQUIRE(run.vertices[6] > 0);
        CHECK(run.vertices[9] == 0);
    }
}

// Run only by gyo_framework.imgui_shell_no_renderer, where SDL_RENDER_DRIVER
// names no driver.
TEST_CASE("I8: a renderer that cannot start returns 1 without calling the factory" * doctest::skip()) {
    int factoryCalls = 0;
    const int code = RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext&) {
        ++factoryCalls;
        return nullptr;
    });
    CHECK(code == 1);
    CHECK(factoryCalls == 0);
}

namespace {

// v8-input-ui-adapter: what the client saw of the window's input and of SDL's
// text input at each BeginFrame. Index i is read at frame i + 1's BeginFrame:
// SDL's text input as frame i left it, and frame i + 1's input.
struct InputRun {
    std::vector<bool> active;
    std::vector<SDL_Rect> area;
    std::vector<std::vector<std::string>> compositions;
    bool sameInDraw{true};
    std::size_t seenText{};
};

// The client asks for text input over clientArea in the frames of clientFrames;
// an ImGui InputText is edited in the frames of imguiFrames. Each entry of events
// is pushed at the start of its frame.
InputRun RunInput(const std::vector<int>& clientFrames, const std::vector<int>& imguiFrames,
                  const std::vector<std::pair<int, std::function<void(SDL_Event&, SDL_WindowID)>>>& events,
                  const int frames,
                  const std::vector<std::pair<int, std::function<void(SDL_Window*)>>>& windowActions = {}) {
    Counts counts;
    InputRun run;
    std::array<char, 64> buffer{};
    const auto in = [](const std::vector<int>& list, const int frame) {
        return std::find(list.begin(), list.end(), frame) != list.end();
    };
    const int code = RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
        auto client = std::make_unique<Client>(context, counts, frames);
        client->onBeginFrame = [&, context](const int frame) {
            SDL_Window* window = Engine::Platform::Sdl::NativeWindow(context.platform);
            run.active.push_back(SDL_TextInputActive(window));
            SDL_Rect area{};
            int cursor = 0;
            SDL_GetTextInputArea(window, &area, &cursor);
            run.area.push_back(area);
            std::vector<std::string> compositions;
            for (const auto& entry : context.input.Frame().text) {
                if (entry.kind == Engine::Input::TextEvent::Kind::Composition) {
                    compositions.push_back(entry.text);
                }
            }
            run.compositions.push_back(std::move(compositions));
            run.seenText = context.input.Frame().text.size();
            for (const auto& [at, act] : windowActions) {
                if (at == frame) {
                    act(window);
                }
            }
            for (const auto& [at, make] : events) {
                if (at == frame) {
                    SDL_Event event{};
                    make(event, OwnWindow(context));
                    PushInjected(context, event);
                }
            }
        };
        client->onDraw = [&, context](const int frame) {
            if (context.input.Frame().text.size() != run.seenText) {
                run.sameInDraw = false;
            }
            if (in(clientFrames, frame)) {
                context.input.RequestTextInput({100, 200, 50, 20}, 3);
            }
            if (in(imguiFrames, frame)) {
                ImGui::Begin("input field");
                if (!in(imguiFrames, frame - 1)) {
                    ImGui::SetKeyboardFocusHere();
                }
                ImGui::InputText("field", buffer.data(), buffer.size());
                ImGui::End();
            }
        };
        return client;
    });
    CHECK(code == 0);
    return run;
}

std::vector<int> Frames(const int from, const int to) {
    std::vector<int> frames;
    for (int frame = from; frame <= to; ++frame) {
        frames.push_back(frame);
    }
    return frames;
}

} // namespace

TEST_CASE("I11: the client's request owns text input while it asks, and gets the frame's composition") {
    SUBCASE("on while asked, over the client's area; off once it stops, with no ImGui field") {
        const InputRun run = RunInput(Frames(2, 4), {}, {{3, &Composing}}, 7);
        CHECK_FALSE(run.active[0]);
        CHECK(run.active[2]);
        CHECK(run.area[2].x == 100);
        CHECK(run.area[2].y == 200);
        CHECK(run.area[2].w == 50);
        CHECK(run.area[2].h == 20);
        // Pushed at the start of frame 3, read by the client in frame 4.
        REQUIRE(run.compositions[3].size() == 1);
        CHECK(run.compositions[3][0] == "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e");
        CHECK(run.compositions[4].empty());
        CHECK(run.sameInDraw);
        CHECK_FALSE(run.active[5]);
    }
    SUBCASE("a client that never asks leaves text input alone") {
        const InputRun run = RunInput({}, {}, {}, 4);
        for (const bool active : run.active) {
            CHECK_FALSE(active);
        }
    }
    SUBCASE("the client wins over an ImGui field in the same frame, and gives it back to the field") {
        // The field is focused in frame 2 and edited from frame 3 on.
        const InputRun run = RunInput(Frames(4, 5), Frames(2, 8), {}, 9);
        CHECK(run.active[3]);
        CHECK(run.area[3].x != 100);
        CHECK(run.active[4]);
        CHECK(run.area[4].x == 100);
        CHECK(run.area[4].y == 200);
        // Back to the field ImGui is still editing.
        CHECK(run.active[6]);
        CHECK(run.area[6].x == run.area[3].x);
        CHECK(run.area[6].y == run.area[3].y);
        CHECK(run.area[6].w == run.area[3].w);
        CHECK(run.area[6].h == run.area[3].h);
        CHECK(run.area[3].h > 0);
    }
    SUBCASE("moving from an ImGui field to the client's in one frame keeps text input on") {
        const InputRun run = RunInput(Frames(4, 6), Frames(2, 3), {}, 8);
        CHECK(run.active[3]);
        CHECK(run.active[4]);
        CHECK(run.area[4].x == 100);
        CHECK(run.active[5]);
        CHECK_FALSE(run.active[7]);
    }
    SUBCASE("an ImGui field that stops while the client asks leaves text input on for the client") {
        // Both ask in frames 3 and 4; ImGui's backend stops text input when its field goes in frame 5.
        const InputRun run = RunInput(Frames(3, 6), Frames(2, 4), {}, 8);
        CHECK(run.active[4]);
        CHECK(run.area[4].x == 100);
        CHECK(run.active[5]);
        CHECK(run.area[5].x == 100);
        CHECK_FALSE(run.active[7]);
    }
    SUBCASE("while both ask, the window losing and regaining keyboard focus leaves the client's area") {
        // ImGui's backend stops text input when the focus goes and starts it over its own field when the focus
        // comes back, in its NewFrame, without telling ImGui's platform hook.
        const auto hide = [](SDL_Window* window) { SDL_HideWindow(window); };
        const auto show = [](SDL_Window* window) { SDL_ShowWindow(window); };
        const InputRun run = RunInput(Frames(4, 11), Frames(2, 11), {}, 12, {{6, hide}, {8, show}});
        CHECK(run.area[5].x == 100);
        for (const int index : {9, 10, 11}) {
            CAPTURE(index);
            CHECK(run.active[index]);
            CHECK(run.area[index].x == 100);
            CHECK(run.area[index].y == 200);
        }
    }
}

TEST_CASE("T2: a texture's ImGui id is its own while it lives, reaches ImGui's draw list, and goes with it") {
    using Engine::Render::Backend::Sdl::SdlTextureHandle;
    Counts counts;
    SdlTextureHandle texture;
    std::vector<std::optional<std::uint64_t>> ids;
    std::vector<bool> drawn;
    std::uint64_t native{};
    bool releasedHasNoId{};
    bool noHandleHasNoId{};
    const int code = RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
        auto client = std::make_unique<Client>(context, counts, 4);
        auto& renderer = context.renderer;
        client->onBeginFrame = [&](int frame) {
            if (frame == 4) {
                // Released before ImGui's frame: the last frame that drew it is rendered.
                REQUIRE(renderer.ReleaseTexture(texture));
                releasedHasNoId = !ImGuiTextureId(renderer, texture).has_value();
                noHandleHasNoId = !ImGuiTextureId(renderer, {}).has_value();
                return;
            }
            if (frame != 1) {
                return;
            }
            auto created = renderer.CreateStreamingTexture(4, 4);
            REQUIRE(created);
            texture = created.value();
            native = reinterpret_cast<std::uintptr_t>(Engine::Render::Backend::Sdl::NativeTexture(renderer, texture));
            const std::vector<std::byte> pixels(64, std::byte{200});
            REQUIRE(renderer.UploadTexture(texture, pixels, 16));
        };
        client->onDraw = [&](int frame) {
            if (frame <= 3) {
                const auto id = ImGuiTextureId(renderer, texture);
                ids.push_back(id);
                bool found = false;
                if (id) {
                    ImGui::Begin("texture");
                    ImGui::Image(static_cast<ImTextureID>(*id), ImVec2(4, 4));
                    for (const ImDrawCmd& command : ImGui::GetWindowDrawList()->CmdBuffer) {
                        found = found || command.GetTexID() == static_cast<ImTextureID>(*id);
                    }
                    ImGui::End();
                }
                drawn.push_back(found);
            }
        };
        return client;
    });
    CHECK(code == 0);
    REQUIRE(ids.size() == 3);
    for (std::size_t frame = 0; frame < ids.size(); ++frame) {
        CAPTURE(frame);
        REQUIRE(ids[frame].has_value());
        CHECK(*ids[frame] == native);
        CHECK(drawn[frame]);
    }
    CHECK_MESSAGE(releasedHasNoId, "a released texture still has an ImGui id");
    CHECK(noHandleHasNoId);
}
