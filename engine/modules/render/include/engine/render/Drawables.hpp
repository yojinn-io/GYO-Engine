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
#include <mutex>
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
#include "engine/render/TonemappingMethods.h"

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

// When a camera on a render target draws (render-pipeline declaration 14.11; after Godot's SubViewport
// render_target_update_mode, scene/main/viewport.h:917-921, which is the viewport's: here it is the camera's and the
// decision is the target's). No component: Always. A Disabled camera waits while its target has a picture; a target
// none of whose cameras draws this frame (waiting, inactive, not valid) and that has a picture is retained
// (RenderCommands::retained: its last picture stands for this frame's); if any camera draws it, its waiting cameras
// draw too. A target with no picture (made, or its size changed, and not drawn on since) is drawn by its cameras. A
// camera on the main target always draws (the window's picture is not kept from frame to frame).
struct TargetUpdate final {
    enum class Mode : std::uint8_t {
        // Every frame (Godot's UPDATE_ALWAYS).
        Always = 1,
        // In the frame it is seen, then Disabled (Godot's UPDATE_ONCE): world_render sets Disabled at the start of the
        // next frame.
        Once,
        // Not again until its target has no picture (Godot's UPDATE_DISABLED, which this one leaves on a resize).
        Disabled,
    };
    Mode mode = Mode::Always;
};

// On a Camera3d: the camera draws its phases up to PostProcess on a picture of its own, linear light past 1 kept
// (a Rgba16Float render target of the render World's), which its tonemapping pass, after its PostProcess phase, puts
// through a curve, encodes to sRGB bytes and puts on its target (Bevy's Hdr, bevy_camera/src/components.rs:91; opt-in).
// Not looked at on a camera that is not a Camera3d: the 2D passes blend bytes (RP-4). Render-pipeline declaration 16.
struct Hdr final {};

// How a Hdr camera's tonemapping pass maps its picture's light into 0 to 1 before it encodes it (Bevy's Tonemapping,
// bevy_render/src/view/mod.rs:291; the curves of bevy_core_pipeline/src/tonemapping.wesl that need no lookup table). No
// component: the default method. The values are TonemappingMethods.h's (the shader's too).
struct Tonemapping final {
    enum class Method : std::uint8_t {
        // No curve: the light clamped to 0 to 1 (Bevy's None, the pass bypassed).
        None = GYO_TONEMAP_NONE,
        // c / (1 + c) per channel (tonemapping.wesl:230).
        Reinhard = GYO_TONEMAP_REINHARD,
        // Reinhard on the Rec. 709 luminance, the colour scaled to it (tonemapping.wesl:245); black stays black.
        ReinhardLuminance = GYO_TONEMAP_REINHARD_LUMINANCE,
        // The ACES fit of Stephen Hill (Godot's ACES; tonemapping.wesl:106-140).
        AcesFitted = GYO_TONEMAP_ACES_FITTED,
        // Tomasz Stachowiak's SomewhatBoringDisplayTransform (tonemapping.wesl:39-80).
        SomewhatBoringDisplayTransform = GYO_TONEMAP_SOMEWHAT_BORING_DISPLAY_TRANSFORM,
        // Khronos PBR Neutral (tonemapping.wesl:258-301).
        KhronosPbrNeutral = GYO_TONEMAP_KHRONOS_PBR_NEUTRAL,
    };
    // No curve by default, as Godot's (scene/resources/environment.h:116, TONE_MAPPER_LINEAR): whether and how to
    // compress the highlights is the author's art rule, and with no curve a camera looks the same with Hdr as without.
    // Bevy defaults to a curve (TonyMcMapface) because its default is PBR lighting; GYO's comes with B4 (render-pipeline
    // declaration 16.9, RC-A4-1).
    Method method = Method::None;
};

// The source World's default clear colour (Bevy's ClearColor; opaque black here, where Bevy's is a dark grey: the
// letterbox bars of a fixed-resolution game are black).
struct ClearColor final {
    Color8 color{0, 0, 0, 255};
};

// A model the author hands to the render World (Bevy's RenderAssetUsages::RENDER_WORLD with Mesh::take_gpu_data:
// bevy_asset/src/render_asset.rs:32, bevy_render/src/render_asset.rs:348-358, bevy_mesh/src/mesh.rs:2722). Once the
// render World has made the GPU mesh of every mesh without a skin, it takes their vertices and indices: the model
// that stays has its nodes (where its meshes are placed), its skinned meshes whole (skinned on the CPU every frame)
// and the other meshes empty. As in Bevy, what was taken cannot be drawn again: a GPU mesh let go (no Mesh3d drew the
// model in a frame) is not made again. The author keeps only this object; Model() shows what is left.
class HandedModel final {
public:
    explicit HandedModel(std::shared_ptr<const Model::ModelData> model) : model_(std::move(model)) {}

    // The model now: whole until its vertices are taken, then what stays.
    [[nodiscard]] std::shared_ptr<const Model::ModelData> Model() const {
        const std::lock_guard lock(mutex_);
        return model_;
    }
    [[nodiscard]] bool Taken() const {
        const std::lock_guard lock(mutex_);
        return taken_;
    }
    // mesh_render's, once the GPU meshes are made: the model without the vertices and indices of its meshes that have
    // no skin. The whole model goes when nothing else holds it.
    void TakeVertices();

private:
    mutable std::mutex mutex_;
    std::shared_ptr<const Model::ModelData> model_;
    bool taken_ = false;
};

// The model a Mesh3d draws: a model asset (drawn once it is loaded), a model the author holds (geometry the game
// makes at run time, such as a map's region; as ImageRef's held image: a new ModelData is a new model, the old one's
// GPU meshes are let go when no Mesh3d draws it any more), or one the author hands to the render World (HandedModel:
// its vertices are then only on the GPU). Bevy adds such a mesh to Assets<Mesh> (Assets::add); gyo2's asset has no
// run-time add, so the held forms follow ImageRef instead.
using ModelRef = std::variant<Asset::Handle<Model::ModelData>, std::shared_ptr<const Model::ModelData>, std::shared_ptr<HandedModel>>;

// One mesh of a model (an asset or a held one, ModelRef), drawn by 3D cameras (Bevy's Mesh3d with its MeshMaterial3d, bevy_mesh / bevy_pbr; render
// batch r2b-3d). A mesh without a skin sits at its node (its rest pose, or the entity's SkinPose when it has one); a
// skinned mesh's vertices are placed by its skin and the entity's SkinPose (model.zh-Hant.md section 2.1), on the CPU
// (render.zh-Hant.md section 8.4). The entity's GlobalTransform places the model. The material is the author's:
// color times image (none: white); the model's own material is data the author reads (model.zh-Hant.md: Material).
struct Mesh3d final {
    ModelRef model;
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
// in visibility/mod.rs:318, VisibilityRange in visibility/range.rs:80). The visibility Module's CheckVisibility decides,
// per camera, which entities with an Aabb or a VisibilityRange it does not see and writes them into the camera's
// CulledEntities; Queue3d leaves those out. Bevy keeps the positive list (VisibleEntities, :344) and lets the queue
// read it per visibility class; the negative list keeps "which entities are culled at all" in the visibility Module
// alone, so a renderer only asks "is it in the list". An entity with neither component is never culled, and without
// the visibility Module no camera has a list, so everything is drawn.

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

// On a camera, written by CheckVisibility: the entities with an Aabb or a VisibilityRange this camera does not see, in
// ascending EntityId. A camera without it (no visibility Module, or not checked yet) culls nothing.
struct CulledEntities final {
    std::vector<std::uint64_t> entities;

    friend bool operator==(const CulledEntities&, const CulledEntities&) = default;
};

inline void HashValue(Kernel::Hasher&, const Aabb&) {}
inline void HashValue(Kernel::Hasher&, const NoFrustumCulling&) {}
inline void HashValue(Kernel::Hasher&, const VisibilityRange&) {}
inline void HashValue(Kernel::Hasher&, const CulledEntities&) {}
inline void HashValue(Kernel::Hasher&, const RenderLayers&) {}
inline void HashValue(Kernel::Hasher&, const Mesh3d&) {}
inline void HashValue(Kernel::Hasher&, const SkinPose&) {}
inline void HashValue(Kernel::Hasher&, const Camera3dDepth&) {}
inline void HashValue(Kernel::Hasher&, const Sprite&) {}
inline void HashValue(Kernel::Hasher&, const Shape2d&) {}
inline void HashValue(Kernel::Hasher&, const ClearColorConfig&) {}
inline void HashValue(Kernel::Hasher&, const TargetUpdate&) {}
inline void HashValue(Kernel::Hasher&, const Hdr&) {}
inline void HashValue(Kernel::Hasher&, const Tonemapping&) {}
inline void HashValue(Kernel::Hasher&, const ClearColor&) {}

} // namespace GYO::Engine::Render
