#include "engine/ui_render/UiRenderModule.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "engine/base/Result.hpp"
#include "engine/camera_render/CameraRenderModule.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"
#include "engine/math/scalar/ColorSpace.hpp"
#include "engine/render/Compose.hpp"
#include "engine/render/Pipeline2d.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render_world/RenderWorldModule.hpp"
#include "engine/ui/UiRoot.hpp"
#include "engine/ui_render/UiPass.hpp"
#include "engine/world_asset/AssetView.hpp"

namespace GYO::Engine::UiRender {

namespace {

using SystemResult = Base::Result<void, Kernel::KernelError>;

const UiRenderOptions& OptionsOf(const UiRenderOptions* given) {
    static const UiRenderOptions defaults{};
    return given != nullptr ? *given : defaults;
}

// The atlas pixels of a rectangle as RGBA: white, the coverage as alpha.
std::vector<std::uint8_t> CoverageToRgba(const Text::AtlasPageView& page, const Text::AtlasRect& rect) {
    std::vector<std::uint8_t> out(static_cast<std::size_t>(rect.width) * rect.height * 4U);
    std::size_t o = 0;
    for (std::uint32_t y = 0; y < rect.height; ++y) {
        const std::size_t row = static_cast<std::size_t>(rect.y + y) * page.rowPitch + rect.x;
        for (std::uint32_t x = 0; x < rect.width; ++x) {
            out[o++] = 255;
            out[o++] = 255;
            out[o++] = 255;
            out[o++] = page.pixels[row + x];
        }
    }
    return out;
}

// Uploads what changed in the atlas since the revision last uploaded: a new page is made whole, a known page gets the
// changed rectangles (render.zh-Hant.md section 3: the data side gives the ids).
void UploadAtlas(const Ui::UiText& text, UiAtlasTextures& state, Render::RenderTextures& textures) {
    if (text.Id() != state.atlas) {
        for (const Render::TextureId page : state.pages) textures.Release(page);
        state = UiAtlasTextures{};
        state.atlas = text.Id();
    }
    const Text::GlyphAtlas& atlas = text.Atlas();
    if (atlas.Revision() == state.revision) return;
    const Text::AtlasChanges changes = atlas.ChangesSince(state.revision);
    const std::size_t known = state.pages.size();
    for (std::size_t page = known; page < atlas.PageCount(); ++page) {
        const Text::AtlasPageView view = atlas.Page(page);
        const Render::TextureDesc desc{view.width, view.height, Render::TextureFilter::Nearest};
        state.pages.push_back(textures.Create(desc, CoverageToRgba(view, {0, 0, view.width, view.height})));
        state.sizes.push_back(desc);
    }
    for (const Text::AtlasChange& change : changes.changes) {
        // A page made above already has its whole present content.
        if (change.page >= known || change.rect.width == 0 || change.rect.height == 0) continue;
        const Text::AtlasPageView view = atlas.Page(change.page);
        textures.Update(state.pages[change.page], {change.rect.x, change.rect.y, change.rect.width, change.rect.height},
                        CoverageToRgba(view, change.rect));
    }
    state.revision = changes.revision;
}

SystemResult ExtractUi(Kernel::Extract<Kernel::Read<Ui::UiRoot>> roots, Kernel::Extract<Kernel::Read<Ui::UiRootState>> states,
                       Kernel::ExtractRes<Ui::UiText> text, Kernel::ExtractRes<WorldAsset::AssetView> assets,
                       Kernel::ResMut<ExtractedUiRoots> out, Kernel::ResMut<UiAtlasTextures> atlas,
                       Kernel::ResMut<Render::RenderImages> images, Kernel::ResMut<Render::RenderTextures> textures) {
    std::vector<ExtractedUiRoot> list;
    if (text.Present()) UploadAtlas(*text, atlas.Get(), textures.Get());
    const WorldAsset::AssetView* view = assets.Present() ? &*assets : nullptr;
    roots.ForEach([&](const Kernel::EntityId id, const Ui::UiRoot& root) {
        const Ui::UiRootState* state = states.Get(id);
        if (state == nullptr || !state->camera || state->failure || state->drawList.commands.empty()) return;
        ExtractedUiRoot extracted;
        extracted.entity = id;
        extracted.camera = *state->camera;
        extracted.order = root.order;
        extracted.drawList = state->drawList;
        for (const Ui::UiDrawCommand& command : state->drawList.commands) {
            const auto* image = std::get_if<Ui::UiImageDraw>(&command);
            if (image == nullptr) continue;
            Render::TextureId texture{};
            if (const Asset::Handle<Image::ImageData>* handle = state->Texture(image->textureAsset)) {
                Render::ResolvedImage resolved =
                    Render::ResolveImage(Render::ImageRef{*handle}, Render::TextureFilter::Linear, view);
                Math::Vec2 size{};
                const std::optional<Render::TextureId> made =
                    images.Get().Use(resolved.key, resolved.data, std::move(resolved.holder), textures.Get(), size);
                if (made) texture = *made;
            }
            extracted.images.push_back(texture);
        }
        list.push_back(std::move(extracted));
    });
    std::stable_sort(list.begin(), list.end(), [](const ExtractedUiRoot& a, const ExtractedUiRoot& b) {
        return a.order != b.order ? a.order < b.order : a.entity < b.entity;
    });
    out.Get().roots = std::move(list);
    return {};
}

SystemResult QueueUiPasses(Kernel::Res<Render::ExtractedCameras> cameras, Kernel::Res<ExtractedUiRoots> roots,
                           Kernel::Res<UiAtlasTextures> atlas, Kernel::ResMut<Render::CameraPasses> passes) {
    Render::CameraPasses& slots = passes.Get();
    for (std::size_t i = 0; i < cameras->cameras.size() && i < slots.byCamera.size(); ++i) {
        std::optional<Render::Pass2D> pass = QueueUi(cameras->cameras[i], roots->roots, *atlas);
        if (pass) slots.byCamera[i][Render::CameraPhase::Overlay].push_back(std::move(*pass));
    }
    return {};
}

template <class T>
void PutIfAbsent(Kernel::World& world) {
    if (world.Resource<T>() == nullptr) world.SetResource(T{});
}

} // namespace

Render::Color8 ToColor8(const Ui::UiColor& color) noexcept {
    const auto byte = [](const float value) {
        const float clamped = std::isnan(value) ? 0.0F : std::clamp(value, 0.0F, 1.0F);
        return static_cast<std::uint8_t>(std::lround(clamped * 255.0F));
    };
    return {byte(Math::EncodeSrgb(color.red)), byte(Math::EncodeSrgb(color.green)), byte(Math::EncodeSrgb(color.blue)),
            byte(color.alpha)};
}

void AppendUiDrawList(const Ui::UiDrawList& drawList, const std::vector<Render::TextureId>& images,
                      const UiAtlasTextures& atlas, Render::Pass2D& pass) {
    std::size_t image = 0;
    for (const Ui::UiDrawCommand& command : drawList.commands) {
        std::visit(
            [&](const auto& draw) {
                using T = std::decay_t<decltype(draw)>;
                if constexpr (std::is_same_v<T, Ui::UiQuadDraw>) {
                    AppendUiRect(pass, draw.rect, {0.0F, 0.0F, 1.0F, 1.0F}, ToColor8(draw.color), Render::TextureId{},
                                 draw.clip);
                } else if constexpr (std::is_same_v<T, Ui::UiImageDraw>) {
                    const Render::TextureId texture = image < images.size() ? images[image] : Render::TextureId{};
                    ++image;
                    if (texture == Render::TextureId{}) return;
                    AppendUiRect(pass, draw.rect, draw.sourceUv, ToColor8(draw.tint), texture, draw.clip);
                } else {
                    if (draw.page >= atlas.pages.size()) return;
                    const Render::TextureDesc& size = atlas.sizes[draw.page];
                    const float w = static_cast<float>(size.width);
                    const float h = static_cast<float>(size.height);
                    const Math::Rect uv{static_cast<float>(draw.atlasX) / w, static_cast<float>(draw.atlasY) / h,
                                        draw.rect.width / w, draw.rect.height / h};
                    AppendUiRect(pass, draw.rect, uv, ToColor8(draw.color), atlas.pages[draw.page], draw.clip);
                }
            },
            command);
    }
}

void AppendUiRect(Render::Pass2D& pass, const Math::Rect rect, const Math::Rect uv, const Render::Color8 color,
                  const Render::TextureId texture, const Math::Rect clip) {
    if (!(rect.width > 0.0F) || !(rect.height > 0.0F)) return;
    std::optional<Math::Rect> drawClip;
    if (clip.width < Ui::kNoClip.width || clip.height < Ui::kNoClip.height) {
        if (!(clip.width > 0.0F) || !(clip.height > 0.0F)) return;
        drawClip = clip;
    }
    const auto first = static_cast<std::uint32_t>(pass.vertices.size());
    pass.vertices.push_back({{rect.x, rect.y}, {uv.x, uv.y}, color});
    pass.vertices.push_back({{rect.x + rect.width, rect.y}, {uv.x + uv.width, uv.y}, color});
    pass.vertices.push_back({{rect.x + rect.width, rect.y + rect.height}, {uv.x + uv.width, uv.y + uv.height}, color});
    pass.vertices.push_back({{rect.x, rect.y + rect.height}, {uv.x, uv.y + uv.height}, color});
    const auto indexOffset = static_cast<std::uint32_t>(pass.indices.size());
    for (const std::uint32_t corner : {0U, 1U, 2U, 0U, 2U, 3U}) pass.indices.push_back(first + corner);
    const auto sameClip = [&drawClip](const std::optional<Math::Rect>& other) {
        if (drawClip.has_value() != other.has_value()) return false;
        if (!drawClip) return true;
        return drawClip->x == other->x && drawClip->y == other->y && drawClip->width == other->width &&
               drawClip->height == other->height;
    };
    if (!pass.draws.empty()) {
        Render::Draw2D& last = pass.draws.back();
        if (last.texture == texture && last.blend == Render::BlendMode::Alpha && sameClip(last.clip) &&
            last.indexOffset + last.indexCount == indexOffset && last.vertexOffset == 0) {
            last.indexCount += 6;
            return;
        }
    }
    Render::Draw2D draw;
    draw.texture = texture;
    draw.blend = Render::BlendMode::Alpha;
    draw.clip = drawClip;
    draw.indexOffset = indexOffset;
    draw.indexCount = 6;
    draw.vertexOffset = 0;
    pass.draws.push_back(draw);
}

std::optional<Render::Pass2D> QueueUi(const Render::ExtractedCamera& camera, const std::vector<ExtractedUiRoot>& roots,
                                      const UiAtlasTextures& atlas) {
    Render::Pass2D pass;
    pass.target = Render::PhaseTarget(camera, Render::CameraPhase::Overlay);
    const Camera::Viewport& viewport = camera.computed.viewport;
    pass.viewport = Render::PixelRect{static_cast<std::uint32_t>(std::max(0, viewport.position.x)),
                                      static_cast<std::uint32_t>(std::max(0, viewport.position.y)),
                                      static_cast<std::uint32_t>(std::max(0, viewport.size.x)),
                                      static_cast<std::uint32_t>(std::max(0, viewport.size.y))};
    for (const ExtractedUiRoot& root : roots) {
        if (root.camera != camera.entity) continue;
        AppendUiDrawList(root.drawList, root.images, atlas, pass);
    }
    if (pass.draws.empty()) return std::nullopt;
    return pass;
}

void UiRenderModule::Build(Orchestration::WorldSetup& setup) {
    const UiRenderOptions& options = OptionsOf(setup.Options<UiRenderOptions>());
    Kernel::Registrar& r = setup.Registrar();
    r.Resource<ExtractedUiRoots>({.name = "ExtractedUiRoots"});
    r.Resource<UiAtlasTextures>({.name = "UiAtlasTextures"});

    Kernel::SystemOptions extract;
    extract.name = "ExtractUi";
    Kernel::Mount extractMount;
    extractMount.schedule = std::string(RenderWorld::kExtractSchedule);
    extractMount.stage = std::string(RenderWorld::kExtractSchedule);
    extract.mounts.push_back(std::move(extractMount));
    r.AddSystem<&ExtractUi>(std::move(extract));

    Kernel::SystemOptions queue;
    queue.name = "QueueUiPasses";
    Kernel::Mount queueMount;
    queueMount.schedule = options.schedule;
    queueMount.stage = options.stage;
    queueMount.before.push_back(std::string(CameraRender::kComposeCameraPasses));
    queue.mounts.push_back(std::move(queueMount));
    r.AddSystem<&QueueUiPasses>(std::move(queue));

    setup.Prepare([](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
        PutIfAbsent<ExtractedUiRoots>(world);
        PutIfAbsent<UiAtlasTextures>(world);
        return {};
    });
}

} // namespace GYO::Engine::UiRender
