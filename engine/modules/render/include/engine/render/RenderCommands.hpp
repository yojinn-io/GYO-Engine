#pragma once

// What a renderer is asked to draw this frame, as data (docs/architecture/target/v2/render.zh-Hant.md, section 2;
// SDL declaration section 3.3, P27; Design Invariant RD-1). No SDL, no GPU object, no backend type: a backend (today
// sdl_gpu's GPUSubmit) reads it and draws it; a test reads it and compares it. Batch R2a carries one kind of pass,
// the 2D pass (textures, blending, clipping); the 3D scene, cameras and custom shaders come with batch R2b.
//
// Coordinates of a 2D pass are view units with y down: the pass's view rectangle is stretched over the whole
// target, its top-left corner on the target's top-left pixel. Texture coordinate (0, 0) is a texture's top-left
// corner and (1, 1) its bottom-right one, the layout image's ImageData has (row 0 on top; nothing is flipped). The
// one place where up and down turn round is ViewToClip (y-down view units to y-up clip space).
//
// Colours are bytes as they are stored (sRGB-encoded for colour pictures), alpha straight: the 2D pass blends the
// bytes as they are, like Dear ImGui and SDL_Renderer do (render.zh-Hant.md section 2, "colour").

#include <cstdint>
#include <optional>
#include <type_traits>
#include <vector>

#include "engine/kernel/Hash.hpp"
#include "engine/math/geometry/Rect.hpp"
#include "engine/math/linear/Vec2.hpp"
#include "engine/render/RenderTextures.hpp"

namespace GYO::Engine::Render {

// A colour as stored: red, green, blue, alpha, 8 bits each, alpha straight.
struct Color8 final {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    friend bool operator==(const Color8&, const Color8&) = default;
};

// One vertex of a 2D pass. Its byte layout is the shader's vertex input (ShaderAbi.hpp): position at 0, uv at 8,
// colour at 16, 20 bytes in all.
struct Vertex2D final {
    Math::Vec2 position{};
    Math::Vec2 uv{};
    Color8 color{};
};
static_assert(sizeof(Vertex2D) == 20 && alignof(Vertex2D) == 4);
static_assert(std::is_trivially_copyable_v<Vertex2D> && std::is_standard_layout_v<Vertex2D>);

// How a draw's colour is put on the target.
enum class BlendMode : std::uint8_t {
    // Straight alpha: colour = src * src.a + dst * (1 - src.a); alpha = src.a + dst.a * (1 - src.a).
    Alpha = 1,
    // The source replaces the target, alpha included (Bevy's AlphaMode2D::Opaque).
    Opaque,
};

[[nodiscard]] constexpr const char* ToString(const BlendMode mode) noexcept {
    switch (mode) {
    case BlendMode::Alpha: return "Alpha";
    case BlendMode::Opaque: return "Opaque";
    }
    return "Unknown";
}

// One draw of a 2D pass: indexCount indices from indexOffset of the pass's indices, each index plus vertexOffset
// naming a vertex of the pass, as triangles. Draws are drawn in their order; a later draw lies over an earlier one.
struct Draw2D final {
    // TextureId{} is the white texture: the vertex colours alone (shapes without a picture).
    TextureId texture{};
    BlendMode blend = BlendMode::Alpha;
    // Only what lies inside is drawn, in view units; nullopt: the whole target.
    std::optional<Math::Rect> clip;
    std::uint32_t indexOffset = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t vertexOffset = 0;
};

// A 2D pass on one target. The vertices and indices of all its draws are in one list each, so a producer such as
// Dear ImGui hands its draw list over as it is.
struct Pass2D final {
    // The target, in camera's id space (CameraTarget::id): 0 is the main target, the main window when there is one.
    // What another id names is the backend's to map.
    std::uint32_t target = 0;
    // Cleared to this colour before the draws. A target's first pass of the frame with no clear starts from
    // transparent black: a window's picture is undefined when a frame begins.
    std::optional<Color8> clear;
    // The rectangle of view units stretched over the whole target; nullopt: the target's own pixels (view unit =
    // pixel, origin at the top-left pixel).
    std::optional<Math::Rect> view;
    std::vector<Vertex2D> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<Draw2D> draws;
};

// This frame's passes, drawn in their order (render_world's Resource; empty at the start of each frame).
struct RenderCommands final {
    std::vector<Pass2D> passes;
};

// The render World is not decided state: nothing of it goes into a consistency fingerprint (as sdl_window's handle).
inline void HashValue(Kernel::Hasher&, const RenderCommands&) {}

// Clip space of a view: clip = position * scale + offset (x right and y up, -1 to 1 over the target).
struct ViewTransform final {
    Math::Vec2 scale{};
    Math::Vec2 offset{};
};

// The view rectangle's top-left corner goes to clip (-1, 1), its bottom-right corner to (1, -1). A view with zero
// width or height is a Programmer Error.
[[nodiscard]] ViewTransform ViewToClip(Math::Rect view);

// A target's size in pixels.
struct TargetSize final {
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    friend bool operator==(const TargetSize&, const TargetSize&) = default;
};

// The view of a pass on a target of this size: its own view, or the target's pixels.
[[nodiscard]] Math::Rect ViewOf(const Pass2D& pass, TargetSize target);

// A rectangle of whole pixels on a target.
struct PixelRect final {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    friend bool operator==(const PixelRect&, const PixelRect&) = default;
};

// The pixels a draw's clip lets through: those whose centre lies inside the clip, left and top edges included, right
// and bottom edges not (the rasterizer's own rule, so a clip on whole pixels is exact), cut to the target; nullopt
// clip: the whole target. Empty (zero width or height) when nothing is let through.
[[nodiscard]] PixelRect ClipToPixels(const std::optional<Math::Rect>& clip, Math::Rect view, TargetSize target);

} // namespace GYO::Engine::Render
