#include <doctest/doctest.h>

#include "../TestAssets.hpp"
#include "RenderDeviceStub.hpp"
#include "RetroFPS/App/ObjectFpsRuntimeClient.hpp"
#include "input/backend/sdl/SdlInput.hpp"
#include "render/Renderer.hpp"
#include "text/ITextRasterizer.hpp"

namespace {
using namespace fps;

class DebugRuntimeDevice final : public Gyo::Tests::RenderDeviceStub {
    std::uint32_t nextHandle{};
public:
    Engine::Base::Result<Engine::Render::MeshHandle, Engine::Render::RenderError>
    CreateMesh(const Engine::Render::MeshView&) override {
        return Engine::Base::Result<Engine::Render::MeshHandle, Engine::Render::RenderError>::Ok(
            Engine::Render::MeshHandle::FromParts(++nextHandle, 1));
    }
    Engine::Base::Result<Engine::Render::TextureHandle, Engine::Render::RenderError>
    CreateTexture(const Engine::Render::ImageView&) override {
        return Engine::Base::Result<Engine::Render::TextureHandle, Engine::Render::RenderError>::Ok(
            Engine::Render::TextureHandle::FromParts(++nextHandle, 1));
    }
    Engine::Base::Result<void, Engine::Render::RenderError>
    ReleaseMesh(Engine::Render::MeshHandle) override {
        return Engine::Base::Result<void, Engine::Render::RenderError>::Ok();
    }
    Engine::Base::Result<void, Engine::Render::RenderError>
    ReleaseTexture(Engine::Render::TextureHandle) override {
        return Engine::Base::Result<void, Engine::Render::RenderError>::Ok();
    }
    Engine::Base::Result<void, Engine::Render::RenderError> UpdateMeshVertices(
        Engine::Render::MeshHandle, std::span<const Engine::Render::Vertex3D>) override {
        return Engine::Base::Result<void, Engine::Render::RenderError>::Ok();
    }
};

class UnusedTextRasterizer final : public Engine::Text::ITextRasterizer {
public:
    Engine::Base::Result<Engine::Text::TextBitmap, Engine::Text::TextError> Rasterize(
        std::span<const std::byte>, const Engine::Text::TextRasterRequest&) override {
        FAIL("Input-only test must not request text rasterization");
        return Engine::Base::Result<Engine::Text::TextBitmap, Engine::Text::TextError>::Ok({});
    }
};

TEST_CASE("F3 toggles once per focused press and leaves paused simulation unchanged") {
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create();
    REQUIRE(platform);
    Engine::Input::Backend::Sdl::SdlInput input(*platform.value());
    const SDL_WindowID window = SDL_GetWindowID(platform.value()->NativeWindow());
    // Establish the fixture's initial focus before RuntimeClient records it;
    // losing focus after construction correctly requests an automatic pause.
    SDL_Event initialFocus{};
    initialFocus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    initialFocus.window.windowID = window;
    input.HandleEvent(initialFocus);
    auto& app = fps::tests::ProductionApplication();
    DebugRuntimeDevice device;
    Engine::Render::Renderer renderer;
    UnusedTextRasterizer rasterizer;
    ObjectFpsPresentation presentation;
    std::string error;
    REQUIRE_MESSAGE(presentation.Initialize(device, renderer, rasterizer,
        app.Assets(), app.Content(), {}, error), error);
    ObjectFpsRuntimeClient runtime(*platform.value(), input, app.Assets(), presentation);
    GameSessionConfig config;
    config.fadeInSeconds = 0.001F;
    config.fadeOutSeconds = 0.001F;
    REQUIRE_MESSAGE(runtime.Initialize(app.Content(), config, error), error);
    CHECK_FALSE(runtime.DisplaySettings().showCollisionVolumes);

    const auto focus = [&](bool gained) {
        SDL_Event event{};
        event.type = gained ? SDL_EVENT_WINDOW_FOCUS_GAINED : SDL_EVENT_WINDOW_FOCUS_LOST;
        event.window.windowID = window;
        input.HandleEvent(event);
    };
    const auto key = [&](bool down, bool repeat = false) {
        SDL_Event event{};
        event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        event.key.windowID = window;
        event.key.scancode = SDL_SCANCODE_F3;
        event.key.repeat = repeat;
        input.HandleEvent(event);
    };
    std::uint64_t frameIndex{};
    const auto update = [&] {
        REQUIRE_MESSAGE(runtime.Update({frameIndex++, 0.016}) ==
            Engine::Runtime::RuntimeControl::Continue, runtime.LastError());
    };

    // Enter a stage without requesting a mouse capture from the dummy driver.
    input.BeginFrame(); focus(false);
    runtime.Submit(StartCampaignCommand{});
    update();
    for (int index = 0; index < 5; ++index) { input.BeginFrame(); update(); }
    REQUIRE(runtime.Query().screen == GameScreen::Playing);
    REQUIRE_FALSE(runtime.Query().enemies.empty());
    // F3 is consumed while Playing, independently of a simultaneous pause.
    input.BeginFrame(); focus(true); key(true);
    runtime.Submit(PauseCommand{});
    update();
    CHECK(runtime.DisplaySettings().showCollisionVolumes);
    REQUIRE(runtime.Query().screen == GameScreen::Paused);
    const auto paused = runtime.Query();
    input.BeginFrame(); key(true, true); update();
    CHECK(runtime.DisplaySettings().showCollisionVolumes);
    input.BeginFrame(); update();
    CHECK(runtime.DisplaySettings().showCollisionVolumes);
    input.BeginFrame(); key(false); update();
    CHECK(runtime.DisplaySettings().showCollisionVolumes);
    input.BeginFrame(); key(true); update();
    CHECK_FALSE(runtime.DisplaySettings().showCollisionVolumes);
    input.BeginFrame(); key(false); update();
    input.BeginFrame(); focus(false); key(true); update();
    CHECK_FALSE(runtime.DisplaySettings().showCollisionVolumes);
    input.BeginFrame(); focus(true); update();
    CHECK_FALSE(runtime.DisplaySettings().showCollisionVolumes); // Held at focus gain is not a press.
    input.BeginFrame(); key(false); update();
    input.BeginFrame(); key(true); update();
    CHECK(runtime.DisplaySettings().showCollisionVolumes);

    const auto& after = runtime.Query();
    REQUIRE(after.screen == GameScreen::Paused);
    REQUIRE(after.player);
    CHECK(after.player->health == paused.player->health);
    CHECK(after.player->position.x == paused.player->position.x);
    CHECK(after.player->position.z == paused.player->position.z);
    CHECK(after.weapon.magazineAmmo == paused.weapon.magazineAmmo);
    REQUIRE(after.enemies.size() == paused.enemies.size());
    for (std::size_t index = 0; index < after.enemies.size(); ++index) {
        const auto& actual = after.enemies[index];
        const auto& expected = paused.enemies[index];
        CHECK(actual.id == expected.id);
        CHECK(actual.position.x == expected.position.x);
        CHECK(actual.position.z == expected.position.z);
        REQUIRE(actual.pose.globalTransforms.size() == expected.pose.globalTransforms.size());
        for (std::size_t node = 0; node < actual.pose.globalTransforms.size(); ++node)
            CHECK(actual.pose.globalTransforms[node].values == expected.pose.globalTransforms[node].values);
    }

    input.BeginFrame(); key(false);
    runtime.Submit(ReturnToMainMenuCommand{}); update();
    for (int index = 0; index < 5; ++index) { input.BeginFrame(); update(); }
    REQUIRE(runtime.Query().screen == GameScreen::MainMenu);
    CHECK(runtime.DisplaySettings().showCollisionVolumes);
}
} // namespace
