#pragma once

// The UI's 2D pass as functions (docs/architecture/target/v2/ui.zh-Hant.md, section 4.7): what QueueUiPasses calls,
// usable without a World so a test compares the pass directly (as render's Queue2d).

#include <optional>
#include <vector>

#include "engine/math/geometry/Rect.hpp"
#include "engine/render/Pipeline2d.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/ui/UiTypes.hpp"
#include "engine/ui_render/UiRenderModule.hpp"

namespace GYO::Engine::UiRender {

// A linear colour as sRGB-encoded bytes, alpha straight (scaled, not encoded).
[[nodiscard]] Render::Color8 ToColor8(const Ui::UiColor& color) noexcept;

// Appends a rectangle (pixels of the pass's viewport) with its uv region, colour and texture, clipped to clip
// (ui's kNoClip: none). Nothing for an empty rectangle or clip. A rectangle with the same texture and clip as the
// draw before it extends that draw.
void AppendUiRect(Render::Pass2D& pass, Math::Rect rect, Math::Rect uv, Render::Color8 color, Render::TextureId texture,
                  Math::Rect clip);

// Appends a root's draw list: quads with the white texture, images with their textures (images: one per image
// command, in order; TextureId{} skips it), glyphs with their atlas page's texture.
void AppendUiDrawList(const Ui::UiDrawList& drawList, const std::vector<Render::TextureId>& images,
                      const UiAtlasTextures& atlas, Render::Pass2D& pass);

// The UI pass of one camera: the roots drawn over it (roots already in drawing order) on its viewport, the view its
// own pixels. nullopt: nothing to draw.
[[nodiscard]] std::optional<Render::Pass2D> QueueUi(const Render::ExtractedCamera& camera,
                                                    const std::vector<ExtractedUiRoot>& roots,
                                                    const UiAtlasTextures& atlas);

} // namespace GYO::Engine::UiRender
