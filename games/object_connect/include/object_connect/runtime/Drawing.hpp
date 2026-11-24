#pragma once

#include "object_connect/data/PuzzleData.hpp"
#include "object_connect/game/FinalResults.hpp"
#include "object_connect/game/GameFlow.hpp"
#include "object_connect/math/Color.hpp"
#include "object_connect/math/Vec2.hpp"
#include "object_connect/puzzle/PuzzleBoard.hpp"
#include "object_connect/tentacle/RibbonStrip.hpp"
#include "object_connect/ui/MenuLayout.hpp"

#include "engine/kernel/Hash.hpp"
#include "engine/orchestration/Setup.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTextures.hpp"
#include "engine/text/GlyphAtlas.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The game's picture on render's 2D pass: what the original PuzzleRenderer,
// GameUiRenderer and MouseCursorRenderer drew (Object_Connect ca4482c,
// src/ObjectConnect/Rendering/), as one Pass2D a frame in playfield
// coordinates (the view is the 1280 x 720 playfield, stretched over the
// window). The order is the original's: the level, the UI's rectangles, every
// text, the cursor.
//
// Colours: the original drew to an sRGB target, so its colour values are
// linear and the target encoded them; render's 2D pass takes the bytes as
// stored. ToColor8 encodes them the same way, so the colours look the same.
// Blending then happens on the encoded bytes, not in linear light (render
// page section 2): half-transparent overlays come out a little darker than
// the original's.

namespace object_connect::runtime {

// A linear colour as the sRGB bytes the original's target stored (alpha as is).
[[nodiscard]] GYO::Engine::Render::Color8 ToColor8(Color color) noexcept;

// Appends draws to one 2D pass; consecutive draws of the same texture share one Draw2D.
class Canvas final {
public:
    explicit Canvas(GYO::Engine::Render::Pass2D& pass) noexcept : pass_(&pass) {}

    void Rect(float left, float top, float width, float height, Color color);
    void Triangle(Vec2 a, Vec2 b, Vec2 c, Color color);
    // A fan of segments triangles (the original's 32).
    void Circle(Vec2 center, float radius, Color color, int segments = 32);
    // A triangle strip of RibbonStrip's vertices.
    void Strip(std::span<const RibbonVertex> strip);
    // A texture over a rectangle, times a tint.
    void Image(GYO::Engine::Render::TextureId texture, float left, float top, float width, float height,
               Color tint = {});
    // A part of a texture (uv from the top-left corner) over a rectangle, in this exact colour.
    void Quad(GYO::Engine::Render::TextureId texture, float left, float top, float width, float height, Vec2 uv0,
              Vec2 uv1, GYO::Engine::Render::Color8 color);

private:
    // The draw the next indices go into: the last one when its texture is the same, else a new one.
    GYO::Engine::Render::Draw2D& DrawFor(GYO::Engine::Render::TextureId texture);
    std::uint32_t Push(Vec2 position, Vec2 uv, GYO::Engine::Render::Color8 color);
    void Index(std::uint32_t a, std::uint32_t b, std::uint32_t c);

    GYO::Engine::Render::Pass2D* pass_;
};

enum class TextAlignH : std::uint8_t { Left, Center, Right };
enum class TextAlignV : std::uint8_t { Top, Middle };

// One text to draw, as the original's FontSystem::DrawText took it (TextLayout.cpp:77-97: Top puts the layout's
// top at the anchor, Middle its middle).
struct TextRequest final {
    std::string text;
    Vec2 position{};
    float pixelSize = 16.0f;
    TextAlignH alignH = TextAlignH::Left;
    TextAlignV alignV = TextAlignV::Top;
    Color color{};
};

// The textures the game made for its pictures and the glyph atlas's pages, kept for the World's life.
struct DrawResources final {
    bool made = false;
    // By the path the tables write (nodes.csv, maps/*.csv).
    std::vector<std::pair<std::string, GYO::Engine::Render::TextureId>> nodeTextures;
    // In MouseCursorIcon order.
    std::array<GYO::Engine::Render::TextureId, 3> cursors{};
    GYO::Engine::Text::GlyphAtlas atlas{GYO::Engine::Text::AtlasOptions{}};
    std::uint64_t uploadedRevision = 0;
    std::vector<GYO::Engine::Render::TextureId> atlasPages;

    [[nodiscard]] std::optional<GYO::Engine::Render::TextureId> NodeTexture(std::string_view path) const;
};

inline void HashValue(GYO::Kernel::Hasher&, const DrawResources&) {}

// The level (the original PuzzleRenderer::Draw): background, vessels, dead nodes, halos and pulses, nodes.
// texture(path) gives a node picture's texture, nullopt when there is none (a placeholder is drawn instead).
void DrawPuzzle(Canvas& canvas, const PuzzleDefinition& definition, const PuzzleBoardSnapshot& snapshot,
                float elapsedSeconds,
                const std::function<std::optional<GYO::Engine::Render::TextureId>(std::string_view)>& texture);

// What the UI is drawn from (the original GameUiRenderer::Draw's arguments).
struct UiFrame final {
    GameScreen screen = GameScreen::MainMenu;
    std::size_t selectedItem = 0;
    const PuzzleCatalog* catalog = nullptr;
    std::optional<std::size_t> currentPuzzleIndex;
    const PuzzleBoardSnapshot* board = nullptr;
    const FinalResultsSummary* finalResults = nullptr;
    bool hasNextPuzzle = false;
    bool solvedMenuReady = false;
    std::size_t levelSelectPage = 0;
    std::size_t finalResultsPage = 0;
    std::optional<GYO::Engine::Render::TextureId> organTexture;
};

// The UI's rectangles into the canvas and its texts, in the original's order, into texts. measure(text, pixelSize)
// is the width the text lays out to (the level numbers shrink to fit their backdrop).
void DrawUi(Canvas& canvas, std::vector<TextRequest>& texts, const UiFrame& frame,
            const std::function<float(std::string_view, float)>& measure);

// Registers DrawResources and the System that draws each frame (Main/Last, after generation). Needs render_world in
// the World. Called by BuildRuntime.
void BuildDrawing(GYO::Engine::Orchestration::WorldSetup& setup);

} // namespace object_connect::runtime
