#pragma once

// What an author puts in the source World to be drawn (docs/architecture/target/v2/render.zh-Hant.md, section 7; SDL
// declaration section 6.4, R2b). world_render registers these in the source World; the render World reads them
// through Extract (camera_render, sprite_render) and turns them into RenderCommands. After Bevy: Sprite
// (bevy_sprite/src/sprite.rs:17), ClearColorConfig and ClearColor (bevy_camera/src/clear_color.rs:13, :52), RenderLayers
// (bevy_camera's visibility). Shape2d is the 2D shape whose vertices the author gives every frame (a ribbon: a
// triangle strip from a chain of particles; SDL declaration section 6.4, "(1)").
//
// Coordinates are world units of the entity's GlobalTransform, y up (as Bevy 2D and the camera Module); a larger z is
// drawn in front (the 2D camera's default, the user's choice (b), X1). Texture coordinate (0, 0) is a picture's
// top-left corner, as image's ImageData has it.
//
// None of these is decided state: the hashes write nothing (as GlobalTransform's).

#include <cstdint>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "engine/asset/Handle.hpp"
#include "engine/image/ImageData.hpp"
#include "engine/kernel/Hash.hpp"
#include "engine/math/geometry/Aabb.hpp"
#include "engine/math/geometry/Rect.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Vec2.hpp"
#include "engine/model/ModelData.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTextures.hpp"

namespace GYO::Engine::Render {

// The picture a drawable shows: none (the white texture: the vertex colours alone), an image asset (drawn once it is
// loaded; until then the drawable is skipped, as Bevy skips a sprite whose image is not ready), or an image the author
// holds (a glyph atlas the game rasterizes into; a new ImageData is a new picture, the old one is let go when no
// drawable shows it any more).
using ImageRef = std::variant<std::monostate, Asset::Handle<Image::ImageData>, std::shared_ptr<const Image::ImageData>>;

// Which cameras draw an entity: a camera draws it when their masks share a bit. No RenderLayers: layer 0 (mask 1),
// as Bevy's default. 32 layers (Bevy's is unbounded); a HUD is a second camera on another layer.
struct RenderLayers final {
    std::uint32_t mask = 1;
};

// A picture on a rectangle (Bevy's Sprite). Its size in world units is customSize, else the size of rect, else the
// picture's size in pixels (1 x 1 for no picture); anchor (-0.5 to 0.5 on each axis, 0 the centre, as Bevy's Anchor)
// is the point at the entity's position.
struct Sprite final {
    ImageRef image;
    // Multiplies the picture's colours (straight alpha).
    Color8 color{255, 255, 255, 255};
    bool flipX = false;
    bool flipY = false;
    std::optional<Math::Vec2> customSize;
    // A region of the picture in its pixels (origin top-left); none: the whole picture.
    std::optional<Math::Rect> rect;
    Math::Vec2 anchor{0.0F, 0.0F};
    BlendMode blend = BlendMode::Alpha;
    TextureFilter filter = TextureFilter::Linear;
};

enum class Topology : std::uint8_t {
    // Every three indices are a triangle.
    TriangleList = 1,
    // Every index after the first two makes a triangle with the two before it (a ribbon).
    TriangleStrip,
};

// A 2D shape the author gives vertex by vertex, as often as it changes (every frame for a moving ribbon). Positions
// are in the entity's local units (GlobalTransform places them); uv picks the picture (white: none). No indices: the
// vertices in their order.
struct Shape2d final {
    std::vector<Vertex2D> vertices;
    std::vector<std::uint32_t> indices;
    Topology topology = Topology::TriangleList;
    ImageRef image;
    BlendMode blend = BlendMode::Alpha;
    TextureFilter filter = TextureFilter::Linear;
};

// What a camera clears its target to before it draws (Bevy's ClearColorConfig on Camera). No component: Default.
struct ClearColorConfig final {
    enum class Mode : std::uint8_t {
        // The source World's ClearColor.
        Default = 1,
        // color.
        Custom,
        // Nothing: the camera draws over what earlier cameras drew (a HUD camera).
        None,
    };
    Mode mode = Mode::Default;
    Color8 color{0, 0, 0, 255};
};

// The source World's default clear colour (Bevy's ClearColor; opaque black here, where Bevy's is a dark grey: the
// letterbox bars of a fixed-resolution game are black).
struct ClearColor final {
    Color8 color{0, 0, 0, 255};
};

// One mesh of a model asset, drawn by 3D cameras (Bevy's Mesh3d with its MeshMaterial3d, bevy_mesh / bevy_pbr; render
// batch r2b-3d). A mesh without a skin sits at its node (its rest pose, or the entity's SkinPose when it has one); a
// skinned mesh's vertices are placed by its skin and the entity's SkinPose (model.zh-Hant.md section 2.1), on the CPU
// (render.zh-Hant.md section 8.4). The entity's GlobalTransform places the model. The material is the author's:
// color times image (none: white); the model's own material is data the author reads (model.zh-Hant.md: Material).
struct Mesh3d final {
    Asset::Handle<Model::ModelData> model;
    // An index of ModelData::meshes; past the end: nothing is drawn.
    std::uint32_t mesh = 0;
    Color8 color{255, 255, 255, 255};
    ImageRef image;
    BlendMode blend = BlendMode::Opaque;
    // false: drawn over whatever is nearer (an overlay in the world).
    bool depthTest = true;
    // None: written when opaque, not when blended (Bevy's AlphaMode::Blend writes no depth, so crossing translucent
    // meshes do not hide each other).
    std::optional<bool> depthWrite;
    CullMode cull = CullMode::None;
    TextureFilter filter = TextureFilter::Linear;
};

// A 3D camera's depth (Bevy's Camera3d::depth_load_op): clear: each camera starts with a cleared depth (the default:
// a viewmodel camera draws over the world); false: it keeps the depth the cameras before it on the target left (a
// second camera that draws into the same scene). No component: cleared.
struct Camera3dDepth final {
    bool clear = true;
};

// The model-space matrix of every node of the entity's model this frame (Model::ComputeModelSpace of the pose the game
// sampled; the game keeps the animation, as the user's layer 2 decision leaves it, v8-arch-gyo2-skeletal-animation-
// layers). Sizes other than the model's node count: the rest pose is used.
struct SkinPose final {
    std::vector<Math::Matrix4> nodes;
};

// Visibility (big-map declaration section 13; after bevy_camera's visibility: Aabb in primitives.rs, NoFrustumCulling
// and VisibleEntities in visibility/mod.rs:318 and :344, VisibilityRange in visibility/range.rs:80). The visibility
// Module's CheckVisibility decides, per camera, which entities with an Aabb or a VisibilityRange it sees and writes
// them into the camera's VisibleEntities; Queue3d leaves out the others. An entity with neither is always drawn, and
// without the visibility Module no camera has a list, so everything is drawn.

// The entity's bounds in its own space (Bevy's Aabb): the frustum test puts its eight corners through the entity's
// GlobalTransform and tests the box around them.
struct Aabb final {
    Math::Aabb local{};
};

// The entity is not tested against the frustum (it still is against its VisibilityRange).
struct NoFrustumCulling final {};

// A half-open range of distances [start, end) (Bevy's Range<f32>).
struct DistanceRange final {
    float start = 0.0F;
    float end = 0.0F;
};

// Seen only from distances in [startMargin.start, endMargin.end) (Bevy's VisibilityRange and is_visible_at_all): the
// distance from the camera's position to the entity's (or, with useAabb, to its Aabb's centre). The margins are where
// Bevy fades one level of detail into the next; that needs a shader (the render pipeline line), so both levels are
// drawn there for now: zero-width margins switch at once.
struct VisibilityRange final {
    DistanceRange startMargin{};
    DistanceRange endMargin{};
    bool useAabb = false;
};

// On a camera, written by CheckVisibility: the entities with an Aabb or a VisibilityRange this camera sees, in
// ascending EntityId. A camera without it (no visibility Module, or not checked yet) culls nothing.
struct VisibleEntities final {
    std::vector<std::uint64_t> entities;

    friend bool operator==(const VisibleEntities&, const VisibleEntities&) = default;
};

inline void HashValue(Kernel::Hasher&, const Aabb&) {}
inline void HashValue(Kernel::Hasher&, const NoFrustumCulling&) {}
inline void HashValue(Kernel::Hasher&, const VisibilityRange&) {}
inline void HashValue(Kernel::Hasher&, const VisibleEntities&) {}
inline void HashValue(Kernel::Hasher&, const RenderLayers&) {}
inline void HashValue(Kernel::Hasher&, const Mesh3d&) {}
inline void HashValue(Kernel::Hasher&, const SkinPose&) {}
inline void HashValue(Kernel::Hasher&, const Camera3dDepth&) {}
inline void HashValue(Kernel::Hasher&, const Sprite&) {}
inline void HashValue(Kernel::Hasher&, const Shape2d&) {}
inline void HashValue(Kernel::Hasher&, const ClearColorConfig&) {}
inline void HashValue(Kernel::Hasher&, const ClearColor&) {}

} // namespace GYO::Engine::Render
