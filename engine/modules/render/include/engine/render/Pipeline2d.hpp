#pragma once

// The 2D pipeline as data and functions (docs/architecture/target/v2/render.zh-Hant.md, section 7; SDL declaration
// section 6.4, R2b): what the render World extracts from the source World each frame and how it becomes Pass2Ds. The
// Systems that call these are camera_render's and sprite_render's; everything here runs without a World, so a test
// compares the passes directly.
//
// Each extracted thing is data for this frame only, rebuilt by every extract (SDL declaration section 6.1 point 8,
// judged kind by kind in render.zh-Hant.md section 7.3): cameras (Bevy syncs a render entity per camera; here a camera
// keeps no state of its own in the render World), sprites and shapes (Bevy's ExtractedSprites is rebuilt each frame
// too, bevy_sprite_render/src/render/mod.rs:353). What stays from frame to frame is keyed by the picture, not by the
// entity: the GPU texture of an image (RenderImages).

#include <cstdint>
#include <map>
#include <optional>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "engine/camera/Camera.hpp"
#include "engine/kernel/Hash.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/render/Drawables.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTextures.hpp"
#include "engine/world_asset/AssetView.hpp"

namespace GYO::Engine::Render {

// One camera of the source World as this frame's extract saw it.
struct ExtractedCamera final {
    // The source entity (Kernel's EntityId).
    std::uint64_t entity = 0;
    std::int32_t order = 0;
    std::uint32_t target = 0;
    Camera::ComputedCamera computed;
    // The camera entity's GlobalTransform: the view point.
    Math::Matrix4 view{};
    // A Camera2d draws the 2D items; a Camera3d the 3D ones (batch r2b-3d).
    bool is2d = false;
    bool is3d = false;
    // The colour its first pass clears the target to; none: it draws over what is there.
    std::optional<Color8> clear;
    std::uint32_t layers = 1;
};

// render's Resource in the render World (camera_render): the active, valid cameras of this frame, by order, then by
// source entity (Bevy sorts by order and target, bevy_render/src/camera.rs:836; the entity makes ties certain).
struct ExtractedCameras final {
    std::vector<ExtractedCamera> cameras;
};

// One 2D thing to draw this frame (a sprite or a shape), in triangles of its own vertices in local units.
struct Extracted2dItem final {
    std::uint64_t entity = 0;
    // The entity's GlobalTransform.
    Math::Matrix4 world{};
    std::uint32_t layers = 1;
    TextureId texture{};
    BlendMode blend = BlendMode::Alpha;
    std::vector<Vertex2D> vertices;
    // A triangle list into vertices.
    std::vector<std::uint32_t> indices;
};

// render's Resource in the render World (sprite_render): this frame's 2D items, in source entity order (sprites
// first, then shapes; the order a pass draws them in is decided by z, Queue2d).
struct Extracted2dItems final {
    std::vector<Extracted2dItem> items;
};

// The key of an image's texture: an asset (its id and life: a reloaded asset is a new picture) or an image the author
// holds (its address, the image kept alive by RenderImages while it has a texture), with the filter.
struct ImageKey final {
    std::string asset;
    std::uint64_t life = 0;
    const Image::ImageData* held = nullptr;
    TextureFilter filter = TextureFilter::Linear;

    friend auto operator<=>(const ImageKey&, const ImageKey&) = default;
};

// A picture as this frame shows it: its key, its pixels (null: an asset not loaded yet, or no source asset view), and
// for an image the author holds, the holder that keeps it alive. No picture: an empty key, null data.
struct ResolvedImage final {
    ImageKey key;
    const Image::ImageData* data = nullptr;
    std::shared_ptr<const Image::ImageData> holder;
};
// view: the source World's AssetView (read through ExtractRes), or null when the source has none.
[[nodiscard]] ResolvedImage ResolveImage(const ImageRef& image, TextureFilter filter, const WorldAsset::AssetView* view);

// render's Resource in the render World (sprite_render): the textures made from images, made the first time a
// drawable shows the image (Bevy's RenderAssets<GpuImage>, made in prepare_assets, bevy_render/src/render_asset.rs:393).
// An asset's texture stays while the render World lives; an image the author holds is let go after a frame in which no
// drawable showed it.
class RenderImages final {
public:
    // The texture of image for this frame, made from its pixels when there is none (asked on textures); the white
    // texture for no picture. Null image data (an asset not loaded, a null pointer): nullopt, the drawable is skipped.
    // The size of the picture in pixels in size (1 x 1 for none).
    std::optional<TextureId> Use(const ImageKey& key, const Image::ImageData* data, std::shared_ptr<const Image::ImageData> holder,
                                 RenderTextures& textures, Math::Vec2& size);
    // After a frame's extract: lets go of the textures of held images no drawable showed this frame.
    void EndFrame(RenderTextures& textures);
    [[nodiscard]] std::size_t Count() const noexcept { return entries_.size(); }

private:
    struct Entry {
        TextureId texture;
        Math::Vec2 size;
        std::shared_ptr<const Image::ImageData> holder;
        bool used = false;
    };
    std::map<ImageKey, Entry> entries_;
};

inline void HashValue(Kernel::Hasher&, const ExtractedCameras&) {}
inline void HashValue(Kernel::Hasher&, const Extracted2dItems&) {}
inline void HashValue(Kernel::Hasher&, const RenderImages&) {}

// The vertices and indices of a sprite of this size (world units), uv over region (0..1 of the picture), as two
// triangles.
void SpriteGeometry(const Sprite& sprite, Math::Vec2 pictureSize, std::vector<Vertex2D>& vertices, std::vector<std::uint32_t>& indices);

// A shape's triangles as a list: a list as it is (a trailing partial triangle dropped), a strip unrolled (each
// triangle keeps the strip's facing order: odd triangles swap their first two corners, as GPUs do); no indices: the
// vertices in order.
[[nodiscard]] std::vector<std::uint32_t> TriangleListOf(const Shape2d& shape);

// The 2D pass one camera draws: its viewport, its clear, and the items whose layers it draws, back to front by the
// z of their position (smaller z first, as Bevy's Transparent2d sort key, bevy_sprite_render/src/render/mod.rs:504;
// ties by source entity), each vertex taken through the item's world matrix and the camera's clip-from-world matrix
// to pixels of the viewport. An item whose position lies before the near or past the far plane is not drawn.
// Consecutive items with the same texture and blend become one draw (Bevy batches such sprites). The clear: the first
// camera drawing on a target this frame (firstOnTarget) clears the whole target (Pass2D::clear: the letterbox bars);
// a later one clears only its viewport (an opaque rectangle of the clear colour drawn first), as Bevy clears a window
// once a frame and puts each camera's picture within its viewport. nullopt: the camera is not valid (no target size)
// or not a 2D camera.
[[nodiscard]] std::optional<Pass2D> Queue2d(const ExtractedCamera& camera, std::span<const Extracted2dItem> items,
                                            bool firstOnTarget = true);

} // namespace GYO::Engine::Render
