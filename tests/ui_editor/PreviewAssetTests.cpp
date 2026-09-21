#include "gyo/ui_editor/AssetPreviewContext.hpp"
#include "gyo/ui_editor/EditorApp.hpp"
#include "gyo/ui_editor/FileService.hpp"
#include "gyo/ui_editor/ReadOnlyAssetCatalog.hpp"
#include "text/ITextRasterizer.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace {
using namespace Gyo::Tools::UiEditor;
int failures{};
void Expect(bool condition, const char* message) {
    if (!condition) { ++failures; std::cerr << "FAILED: " << message << '\n'; }
}

class BitmapRasterizer final : public Engine::Text::ITextRasterizer {
public:
    int calls{};
    Engine::Base::Result<Engine::Text::TextBitmap, Engine::Text::TextError> Rasterize(
        std::span<const std::byte> font,
        const Engine::Text::TextRasterRequest&) override {
        ++calls;
        Expect(!font.empty(), "preview supplies the mounted font bytes");
        Engine::Text::TextBitmap bitmap{4, 4, 16, std::vector<std::byte>(64, std::byte{255})};
        return Engine::Base::Result<Engine::Text::TextBitmap, Engine::Text::TextError>::Ok(std::move(bitmap));
    }
};

struct Fixture final {
    std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("gyo-editor-preview-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    SDL_Surface* surface{};
    SDL_Renderer* renderer{};
    Fixture() {
        std::filesystem::create_directories(root);
        Expect(static_cast<bool>(WriteTextFileAtomically(root / "font.bin", "test-font")), "write owned fake font");
        Expect(static_cast<bool>(WriteTextFileAtomically(root / "catalog.json",
            R"({"version":1,"assets":[{"id":"font.default","type":"font","path":"font.bin"}]})")), "write catalog");
        surface = SDL_CreateSurface(320, 16, SDL_PIXELFORMAT_RGBA32);
        renderer = SDL_CreateSoftwareRenderer(surface);
        Expect(renderer != nullptr, "create windowless software renderer");
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = {320, 16};
        io.DeltaTime = 1.0F / 60.0F;
        Expect(ImGui_ImplSDLRenderer3_Init(renderer), "initialize actual ImGui renderer backend");
    }
    ~Fixture() {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui::DestroyContext();
        SDL_DestroyRenderer(renderer);
        SDL_DestroySurface(surface);
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
    void Begin(AssetPreviewContext& assets) {
        assets.BeginFrame();
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui::NewFrame();
    }
    void Present(AssetPreviewContext& assets) {
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        Expect(SDL_RenderClear(renderer), "clear software surface");
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        Expect(SDL_RenderPresent(renderer), "present queued textures before ending frame");
        assets.EndFrame();
    }
};

void TestQueuedTexturesAndFrameEviction() {
    Fixture fixture;
    ReadOnlyAssetCatalog catalog;
    std::string error;
    Expect(catalog.Mount(fixture.root / "catalog.json", fixture.root, error), "mount fixture");
    auto rasterizer = std::make_unique<BitmapRasterizer>();
    auto* observed = rasterizer.get();
    AssetPreviewContext assets;
    Expect(assets.Initialize(*fixture.renderer, std::move(rasterizer), error), "inject deterministic rasterizer");
    Expect(assets.Mount(catalog, error), "preview consumes parsed catalog");
    // A parsed snapshot must not be read from disk again by the preview.
    Expect(static_cast<bool>(WriteTextFileAtomically(fixture.root / "catalog.json", "broken")), "replace source catalog");
    fixture.Begin(assets);
    for (int index = 0; index < 300; ++index) {
        const auto text = assets.Text("font.default", std::to_string(index), 12, error);
        Expect(text.texture != nullptr, "rasterized texture exists");
        ImGui::GetBackgroundDrawList()->AddImage(
            ImTextureRef{static_cast<ImTextureID>(reinterpret_cast<std::intptr_t>(text.texture))},
            {static_cast<float>(index), 0}, {static_cast<float>(index + 1), 4});
    }
    const auto first = assets.Text("font.default", "0", 12, error);
    Expect(first.texture != nullptr && observed->calls == 300, "all 300 textures remain cached during one frame");
    Expect(!assets.Mount(catalog, error), "mid-frame mount cannot destroy queued textures");
    Expect(!assets.Unmount(), "mid-frame unmount cannot destroy queued textures");
    fixture.Present(assets);
    for (int index = 0; index < 300; ++index) {
        Uint8 red{}, green{}, blue{}, alpha{};
        Expect(SDL_ReadSurfacePixel(fixture.surface, index, 1, &red, &green, &blue, &alpha) &&
            red == 255 && green == 255 && blue == 255, "every queued text actually renders");
    }
    fixture.Begin(assets);
    static_cast<void>(assets.Text("font.default", "0", 12, error));
    Expect(observed->calls == 300, "recent entry survives next-frame trim");
    static_cast<void>(assets.Text("font.default", "1", 12, error));
    Expect(observed->calls == 301, "old entry is evicted only at next frame boundary");
    ImGui::EndFrame(); // Cancel a frame without ever submitting its draw data.
    assets.EndFrame();
    Expect(assets.Unmount(), "cancelled frame permits safe unmount");
    Expect(!assets.IsMounted(), "unmount removes preview state");
    Expect(assets.Mount(catalog, error), "remount reuses the validated snapshot after source file changed");
    fixture.Begin(assets);
    static_cast<void>(assets.Text("font.default", "0", 12, error));
    Expect(observed->calls == 302, "remount does not reuse retired font/text cache");
    fixture.Present(assets);
}

void TestSharedContentAndQueuedMounts() {
    Fixture fixture;
    std::string error;
    Expect(static_cast<bool>(WriteTextFileAtomically(fixture.root / "content.json",
        R"({"version":1,"catalogs":["catalog.json","images.json"],"shader_bundles":[]})")), "write multi-catalog content");
    Expect(static_cast<bool>(WriteTextFileAtomically(fixture.root / "images.json",
        R"({"version":1,"assets":[{"id":"texture.logo","type":"texture","path":"image.png"}]})")), "write separate image catalog");
    // The engine deliberately enables PNG only in SDL_image. Keep one complete
    // pixel in this fixture rather than relying on OS codecs or game assets.
    constexpr unsigned char png[]{
        0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,
        0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
        0x08,0x04,0x00,0x00,0x00,0xb5,0x1c,0x0c,0x02,0x00,0x00,0x00,
        0x0b,0x49,0x44,0x41,0x54,0x78,0xda,0x63,0x64,0xf8,0x0f,0x00,
        0x01,0x05,0x01,0x01,0x27,0x18,0xe3,0x66,0x00,0x00,0x00,0x00,
        0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82};
    Expect(static_cast<bool>(WriteTextFileAtomically(fixture.root / "image.png",
        std::string_view{reinterpret_cast<const char*>(png), sizeof(png)})), "write owned PNG fixture");
    AssetPreviewContext assets;
    auto rasterizer = std::make_unique<BitmapRasterizer>();
    auto* observed = rasterizer.get();
    Expect(assets.Initialize(*fixture.renderer, std::move(rasterizer), error), "initialize multi-catalog preview");
    CommandLineOptions options;
    options.assetRoot = fixture.root;
    EditorApp editor(options, assets);
    Expect(editor.Initialize(error), "GUI root mount consumes both catalogs");
    fixture.Begin(assets);
    auto* image = assets.Texture("texture.logo", error);
    if (!image) std::cerr << error << '\n';
    Expect(image != nullptr, "preview sees image from second catalog");
    const auto text = assets.Text("font.default", "mounted", 12, error);
    Expect(text.texture != nullptr, "preview sees font from first catalog");
    ImGui::GetBackgroundDrawList()->AddImage(
        ImTextureRef{static_cast<ImTextureID>(reinterpret_cast<std::intptr_t>(text.texture))}, {0, 0}, {4, 4});
    editor.RequestAssetUnmount();
    Expect(assets.IsMounted(), "GUI unmount waits while draw commands reference its textures");
    fixture.Present(assets);
    editor.ApplyPendingAssetChanges();
    Expect(!assets.IsMounted(), "GUI applies unmount before next frame");
    editor.RequestAssetMount({}, fixture.root);
    editor.ApplyPendingAssetChanges();
    Expect(assets.IsMounted(), "GUI remount accepts complete content");
    Expect(static_cast<bool>(WriteTextFileAtomically(fixture.root / "images.json", "broken")), "invalidate remount candidate");
    editor.RequestAssetMount({}, fixture.root);
    editor.ApplyPendingAssetChanges();
    Expect(assets.IsMounted(), "failed GUI remount preserves previous preview");
    fixture.Begin(assets);
    image = assets.Texture("texture.logo", error);
    if (!image) std::cerr << error << '\n';
    Expect(image != nullptr, "failed remount keeps the previous image mapping");
    static_cast<void>(assets.Text("font.default", "mounted", 12, error));
    Expect(observed->calls == 2, "successful unmount/remount retires previous text cache");
    ImGui::EndFrame();
    assets.EndFrame();
    editor.RequestAssetUnmount();
    editor.ApplyPendingAssetChanges();
    Expect(!assets.IsMounted(), "cancelled frame supports the same deferred unmount");
}
} // namespace

int main() {
    TestQueuedTexturesAndFrameEviction();
    TestSharedContentAndQueuedMounts();
    return failures == 0 ? 0 : 1;
}
