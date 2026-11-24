#include "object_connect/runtime/Drawing.hpp"

#include "object_connect/game/MouseCursorPolicy.hpp"
#include "object_connect/runtime/RuntimeWorld.hpp"
#include "object_connect/world/GameScreenState.hpp"
#include "object_connect/world/ObjectConnectWorld.hpp"
#include "object_connect/world/Resources.hpp"

#include "engine/base/Result.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"
#include "engine/orchestration/OrchestrationError.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/WorldGeneration.hpp"
#include "engine/text/TextLayout.hpp"
#include "engine/text/TextStyle.hpp"
#include "engine/world_asset/AssetView.hpp"
#include "engine/world_time/TimeTypes.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace object_connect::runtime {
namespace {

namespace K = GYO::Kernel;
namespace M = GYO::Engine::Math;
namespace O = GYO::Engine::Orchestration;
namespace R = GYO::Engine::Render;
namespace TX = GYO::Engine::Text;
namespace WA = GYO::Engine::WorldAsset;
namespace WT = GYO::Engine::WorldTime;

constexpr float kCursorSize = 32.0f;

[[nodiscard]] std::uint8_t ToByte(const float value) noexcept {
    const float clamped = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
    return static_cast<std::uint8_t>(std::lround(clamped * 255.0f));
}

[[nodiscard]] float EncodeSrgb(const float linear) noexcept {
    const float c = std::isfinite(linear) ? std::clamp(linear, 0.0f, 1.0f) : 0.0f;
    return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

[[nodiscard]] M::Vec2 ToMath(const Vec2 value) noexcept { return {value.x, value.y}; }

// The cursor picture's anchor (MouseCursorRenderer.cpp:44-53): the pointer's tip at the top for Interactive.
[[nodiscard]] Vec2 CursorAnchor(const MouseCursorIcon icon) noexcept {
    return icon == MouseCursorIcon::Interactive ? Vec2{0.5f, 1.0f / kCursorSize} : Vec2{0.5f, 0.5f};
}

[[nodiscard]] std::vector<std::uint8_t> ToRgba(const TX::AtlasPageView& page, const TX::AtlasRect rect) {
    std::vector<std::uint8_t> pixels;
    pixels.reserve(static_cast<std::size_t>(rect.width) * rect.height * 4u);
    for (std::uint32_t y = rect.y; y < rect.y + rect.height; ++y) {
        for (std::uint32_t x = rect.x; x < rect.x + rect.width; ++x) {
            pixels.insert(pixels.end(), {255, 255, 255, page.pixels[static_cast<std::size_t>(y) * page.rowPitch + x]});
        }
    }
    return pixels;
}

// One glyph to draw from the atlas.
struct GlyphQuad final {
    std::uint32_t page = 0;
    float left = 0.0f;
    float top = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    Vec2 uv0{};
    Vec2 uv1{};
    R::Color8 color{};
};

// The original's line height was the font's ascent minus descent plus line gap (FontSystem.cpp:774): 1.0 of the size
// for this font (ascent 1802, descent 246, gap 0, 2048 units per em); the Engine's default 1.2 would put a Top-aligned
// text 0.1 of its size lower.
[[nodiscard]] TX::TextLayout LayOut(UiText& text, const std::string_view utf8, const float pixelSize) {
    const TX::FontId fonts[] = {*text.font};
    const TX::TextSpan spans[] = {
        {0, static_cast<std::uint32_t>(utf8.size()), {fonts, pixelSize, TX::LineHeight::Relative(1.0f)}}};
    const TX::ShapedText shaped = text.context.Shape(utf8, spans, {.language = "ja"});
    return text.context.Layout(shaped, {});
}

// The texts' glyphs into the atlas, the atlas's new pages and changes into textures, the glyphs into the canvas.
void PaintTexts(Canvas& canvas, UiText& text, DrawResources& resources, R::RenderTextures& textures,
                const std::vector<TextRequest>& requests) {
    if (!text.font) {
        return;
    }
    std::vector<GlyphQuad> quads;
    for (const TextRequest& request : requests) {
        if (request.text.empty()) {
            continue;
        }
        const TX::TextLayout layout = LayOut(text, request.text, request.pixelSize);
        const TX::RasterResult raster = text.context.Rasterize(layout, resources.atlas);
        // Each line is aligned by its own width (TextLayout.cpp:64-75).
        const auto lineLeft = [&](const std::uint32_t line) {
            const float width = line < layout.lines.size() ? layout.lines[line].width : layout.width;
            float left = request.position.x;
            if (request.alignH == TextAlignH::Center) left -= width * 0.5f;
            if (request.alignH == TextAlignH::Right) left -= width;
            return std::round(left);
        };
        float top = request.position.y;
        if (request.alignV == TextAlignV::Middle) top -= layout.height * 0.5f;
        top = std::round(top);
        const R::Color8 color = ToColor8(request.color);
        for (std::size_t index = 0; index < layout.glyphs.size() && index < raster.placements.size(); ++index) {
            const TX::PositionedGlyph& glyph = layout.glyphs[index];
            const TX::GlyphPlacement& placement = raster.placements[index];
            if (placement.status != TX::GlyphRasterStatus::Placed) {
                continue;
            }
            const TX::AtlasPageView page = resources.atlas.Page(placement.page);
            const auto pageWidth = static_cast<float>(page.width);
            const auto pageHeight = static_cast<float>(page.height);
            const TX::AtlasRect& r = placement.rect;
            const float lineX = glyph.line < layout.lines.size() ? layout.lines[glyph.line].x : 0.0f;
            quads.push_back({placement.page,
                             lineLeft(glyph.line) + std::round(glyph.x - lineX) + static_cast<float>(placement.left),
                             top + std::round(glyph.y) - static_cast<float>(placement.top),
                             static_cast<float>(r.width), static_cast<float>(r.height),
                             {static_cast<float>(r.x) / pageWidth, static_cast<float>(r.y) / pageHeight},
                             {static_cast<float>(r.x + r.width) / pageWidth,
                              static_cast<float>(r.y + r.height) / pageHeight},
                             color});
        }
    }

    // New pages are made whole; older pages get the rectangles changed since the last upload.
    const std::size_t uploadedPages = resources.atlasPages.size();
    for (std::size_t page = uploadedPages; page < resources.atlas.PageCount(); ++page) {
        const TX::AtlasPageView view = resources.atlas.Page(page);
        resources.atlasPages.push_back(textures.Create({view.width, view.height, R::TextureFilter::Linear},
                                                       ToRgba(view, {0, 0, view.width, view.height})));
    }
    const TX::AtlasChanges changes = resources.atlas.ChangesSince(resources.uploadedRevision);
    for (const TX::AtlasChange& change : changes.changes) {
        if (change.page >= uploadedPages || change.rect.width == 0 || change.rect.height == 0) {
            continue;
        }
        const TX::AtlasPageView view = resources.atlas.Page(change.page);
        textures.Update(resources.atlasPages[change.page],
                        {change.rect.x, change.rect.y, change.rect.width, change.rect.height},
                        ToRgba(view, change.rect));
    }
    resources.uploadedRevision = changes.revision;

    for (const GlyphQuad& quad : quads) {
        canvas.Quad(resources.atlasPages[quad.page], quad.left, quad.top, quad.width, quad.height, quad.uv0, quad.uv1,
                    quad.color);
    }
}

[[nodiscard]] R::TextureId MakeTexture(R::RenderTextures& textures, const GYO::Engine::Image::ImageData& image) {
    return textures.Create({image.Width(), image.Height(), R::TextureFilter::Linear},
                           std::vector<std::uint8_t>(image.Pixels().begin(), image.Pixels().end()));
}

// The pictures as textures, once the content is there: the node pictures the tables name, the cursors.
void MakeTextures(DrawResources& resources, R::RenderTextures& textures, const GameContent& content,
                  const WA::AssetView& view) {
    const auto add = [&](const NamedFile<GYO::Engine::Image::ImageData>& file) {
        if (resources.NodeTexture(file.path)) return;
        if (const auto* image = view.Get(file.asset); image != nullptr && image->Width() > 0 && image->Height() > 0) {
            resources.nodeTextures.emplace_back(file.path, MakeTexture(textures, *image));
        }
    };
    if (const NodeTable* nodes = view.Get(content.nodes)) {
        for (const auto& file : nodes->textures) add(file);
    }
    if (const LevelTable* levels = view.Get(content.levels)) {
        for (const NamedFile<MapTable>& map : levels->maps) {
            if (const MapTable* table = view.Get(map.asset)) {
                for (const auto& file : table->textures) add(file);
            }
        }
    }
    for (std::size_t index = 0; index < content.cursors.size(); ++index) {
        if (const auto* image = view.Get(content.cursors[index]);
            image != nullptr && image->Width() > 0 && image->Height() > 0) {
            resources.cursors[index] = MakeTexture(textures, *image);
        }
    }
    resources.made = true;
}

// One frame's picture (the original Game::Impl::Draw): the level, the UI, the texts, the cursor.
K::SystemResult DrawFrame(K::Res<world::ScreenState> screen, K::Res<world::ScreenRequest> request,
                          K::Res<world::MenuSelection> selection, K::Res<world::Catalog> catalog,
                          K::Res<world::Session> session, K::Res<world::RunResults> results,
                          K::Res<MenuPagesState> pages, K::Res<CursorAppearance> cursor,
                          K::Res<world::FrameInput> input, K::Res<WT::ActiveTime> time, K::Res<GameContent> content,
                          K::Res<WA::AssetView> view, K::ResMut<UiText> text, K::ResMut<DrawResources> resources,
                          K::ResMut<R::RenderCommands> commands, K::ResMut<R::RenderTextures> textures) {
    DrawResources& res = resources.Get();
    if (!res.made) {
        MakeTextures(res, textures.Get(), *content, *view);
    }
    R::Pass2D pass;
    pass.clear = R::Color8{0, 0, 0, 255};
    pass.view = M::Rect{0.0f, 0.0f, ui::kUiWidth, ui::kUiHeight};
    Canvas canvas(pass);

    const GameScreen shown = world::EffectiveScreen(*screen, *request);
    std::optional<PuzzleBoardSnapshot> snapshot;
    if (session->board && shown != GameScreen::FinalResults) {
        snapshot = session->board->MakeSnapshot();
        const auto texture = [&res](const std::string_view path) { return res.NodeTexture(path); };
        DrawPuzzle(canvas, session->board->GetDefinition(), *snapshot,
                   static_cast<float>(static_cast<double>(time->elapsed) / 1e9), texture);
    }

    UiFrame frame;
    frame.screen = shown;
    frame.selectedItem = selection->item;
    frame.catalog = &catalog->puzzles;
    frame.currentPuzzleIndex = session->puzzleIndex;
    frame.board = snapshot ? &*snapshot : nullptr;
    frame.finalResults = shown == GameScreen::FinalResults ? &results->summary : nullptr;
    frame.hasNextPuzzle = world::HasNextPuzzle(*catalog, *session);
    frame.solvedMenuReady = session->solvedElapsedSeconds >= world::kSolvedMenuDelaySeconds;
    frame.levelSelectPage = pages->pages.LevelSelectPage();
    frame.finalResultsPage = pages->pages.FinalResultsPage();
    frame.organTexture = res.NodeTexture("assets/textures/node/organ.png");
    std::vector<TextRequest> texts;
    UiText& ui = text.Get();
    const auto measure = [&ui](const std::string_view utf8, const float pixelSize) {
        return ui.font ? LayOut(ui, utf8, pixelSize).width : 0.0f;
    };
    DrawUi(canvas, texts, frame, measure);
    PaintTexts(canvas, ui, res, textures.Get(), texts);

    if (cursor->custom) {
        const auto icon = static_cast<std::size_t>(cursor->icon);
        if (res.cursors[icon].value != 0) {
            const Vec2 anchor = CursorAnchor(cursor->icon);
            canvas.Image(res.cursors[icon], input->pointer.position.x - anchor.x * kCursorSize,
                         input->pointer.position.y - anchor.y * kCursorSize, kCursorSize, kCursorSize);
        }
    }
    commands->passes.push_back(std::move(pass));
    return {};
}

} // namespace

R::Color8 ToColor8(const Color color) noexcept {
    return {ToByte(EncodeSrgb(color.r)), ToByte(EncodeSrgb(color.g)), ToByte(EncodeSrgb(color.b)), ToByte(color.a)};
}

std::optional<R::TextureId> DrawResources::NodeTexture(const std::string_view path) const {
    for (const auto& [name, id] : nodeTextures) {
        if (name == path) return id;
    }
    return std::nullopt;
}

R::Draw2D& Canvas::DrawFor(const R::TextureId texture) {
    if (pass_->draws.empty() || pass_->draws.back().texture != texture) {
        R::Draw2D draw;
        draw.texture = texture;
        draw.indexOffset = static_cast<std::uint32_t>(pass_->indices.size());
        pass_->draws.push_back(draw);
    }
    return pass_->draws.back();
}

std::uint32_t Canvas::Push(const Vec2 position, const Vec2 uv, const R::Color8 color) {
    pass_->vertices.push_back({ToMath(position), ToMath(uv), color});
    return static_cast<std::uint32_t>(pass_->vertices.size() - 1);
}

void Canvas::Index(const std::uint32_t a, const std::uint32_t b, const std::uint32_t c) {
    pass_->indices.insert(pass_->indices.end(), {a, b, c});
    pass_->draws.back().indexCount += 3;
}

void Canvas::Rect(const float left, const float top, const float width, const float height, const Color color) {
    Quad(R::TextureId{}, left, top, width, height, {0.0f, 0.0f}, {1.0f, 1.0f}, ToColor8(color));
}

void Canvas::Triangle(const Vec2 a, const Vec2 b, const Vec2 c, const Color color) {
    DrawFor(R::TextureId{});
    const R::Color8 c8 = ToColor8(color);
    const std::uint32_t first = Push(a, {}, c8);
    Push(b, {}, c8);
    Push(c, {}, c8);
    Index(first, first + 1, first + 2);
}

void Canvas::Circle(const Vec2 center, const float radius, const Color color, const int segments) {
    constexpr float kTau = 6.2831853071795864769f;
    DrawFor(R::TextureId{});
    const R::Color8 c8 = ToColor8(color);
    const std::uint32_t middle = Push(center, {}, c8);
    for (int index = 0; index <= segments; ++index) {
        const float angle = kTau * static_cast<float>(index) / static_cast<float>(segments);
        Push({center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius}, {}, c8);
    }
    for (int index = 0; index < segments; ++index) {
        Index(middle, middle + 1 + static_cast<std::uint32_t>(index), middle + 2 + static_cast<std::uint32_t>(index));
    }
}

void Canvas::Strip(const std::span<const RibbonVertex> strip) {
    if (strip.size() < 3) {
        return;
    }
    DrawFor(R::TextureId{});
    const auto first = static_cast<std::uint32_t>(pass_->vertices.size());
    for (const RibbonVertex& vertex : strip) Push(vertex.position, {}, ToColor8(vertex.color));
    for (std::uint32_t index = 0; index + 2 < strip.size(); ++index) {
        Index(first + index, first + index + 1, first + index + 2);
    }
}

void Canvas::Image(const R::TextureId texture, const float left, const float top, const float width,
                   const float height, const Color tint) {
    Quad(texture, left, top, width, height, {0.0f, 0.0f}, {1.0f, 1.0f}, ToColor8(tint));
}

void Canvas::Quad(const R::TextureId texture, const float left, const float top, const float width,
                  const float height, const Vec2 uv0, const Vec2 uv1, const R::Color8 color) {
    DrawFor(texture);
    const std::uint32_t first = Push({left, top}, uv0, color);
    Push({left + width, top}, {uv1.x, uv0.y}, color);
    Push({left, top + height}, {uv0.x, uv1.y}, color);
    Push({left + width, top + height}, uv1, color);
    Index(first, first + 1, first + 2);
    Index(first + 2, first + 1, first + 3);
}

void BuildDrawing(O::WorldSetup& setup) {
    K::Registrar& r = setup.Registrar();
    r.Resource<DrawResources>({.name = "object_connect.DrawResources"});
    K::SystemOptions options;
    options.name = "object_connect.DrawFrame";
    K::Mount mount;
    mount.schedule = std::string(O::DefaultSchedules::frame);
    mount.stage = std::string(O::DefaultSchedules::last);
    mount.conditions = {O::AfterGenerating()};
    options.mounts.push_back(std::move(mount));
    r.AddSystem<&DrawFrame>(std::move(options));
    setup.Prepare([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(DrawResources{});
        return {};
    });
}

} // namespace object_connect::runtime
