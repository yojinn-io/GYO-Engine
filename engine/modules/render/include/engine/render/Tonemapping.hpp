#pragma once

// High dynamic range and tonemapping (docs/architecture/target/v2/render.zh-Hant.md, section 2 "colour" and section
// 8.2; render-pipeline declaration section 16, v8-arch-gyo2-render-a4). A 3D camera with Hdr draws its scene into a
// linear Rgba16Float picture (render target) of the render World; its tonemapping pass, after its PostProcess phase,
// puts the picture through a curve and encodes it to sRGB bytes on the camera's own target (8-bit, as the 2D passes
// and the UI blend bytes on it: RP-4). After Bevy: Hdr (bevy_camera/src/components.rs:91), Tonemapping
// (bevy_render/src/view/mod.rs:291), Exposure (bevy_camera/src/camera.rs:236), the curves of
// bevy_core_pipeline/src/tonemapping.wesl. world_render registers the components in the source World; camera_render
// extracts them. Like the other camera components of render (TargetUpdate), they are the author's settings, not
// decided state.
//
// The curves here are the CPU side of the shader's (shaders/Tonemapping.hlsli, Design Invariant RD-4): the same
// formulas, so a test compares what a GPU drew with them.

#include <cstdint>
#include <optional>
#include <vector>

#include "engine/kernel/Hash.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTextures.hpp"

namespace GYO::Engine::Render {

// On a Camera3d: the camera draws into a linear Rgba16Float picture, tonemapped onto its target after its PostProcess
// phase (Bevy's Hdr, opt-in). Not looked at on a 2D camera: the 2D passes blend bytes (RP-4).
struct Hdr final {};

// How a Hdr camera's tonemapping pass maps the picture's light into 0 to 1 before it encodes it (Bevy's Tonemapping;
// the curves need no lookup table). No component: the default method.
struct Tonemapping final {
    enum class Method : std::uint8_t {
        // No curve: the light clamped to 0 to 1 (Bevy's None, the pass bypassed).
        None = 1,
        // c / (1 + c) per channel (tonemapping.wesl:230).
        Reinhard,
        // Reinhard on the Rec. 709 luminance, the colour scaled to it (tonemapping.wesl:245). The default: Bevy's
        // before its lookup-table curves (#7594, 912fb5886); Bevy's TonyMcMapface needs a 3D lookup table.
        ReinhardLuminance,
        // The ACES fit of Stephen Hill (Godot's ACES; tonemapping.wesl:106-140).
        AcesFitted,
        // Tomasz Stachowiak's SomewhatBoringDisplayTransform (tonemapping.wesl:39-80).
        SomewhatBoringDisplayTransform,
        // Khronos PBR Neutral (tonemapping.wesl:258-301).
        KhronosPbrNeutral,
    };
    Method method = Method::ReinhardLuminance;
};

// How much of the incoming light a 3D camera takes (Bevy's Exposure, in EV100). Lit materials multiply their lighting
// by Scale() (Bevy's view.exposure, bevy_pbr/src/render/pbr_functions.wesl:850); the unlit material and the
// tonemapping pass do not (as Bevy's). No component: Blender's default.
struct Exposure final {
    // Bevy's Exposure::EV100_BLENDER (camera.rs).
    static constexpr float kBlender = 9.7F;
    float ev100 = kBlender;

    // exp2(-ev100) / 1.2 (Bevy's Exposure::exposure, after Filament's physically based camera).
    [[nodiscard]] float Scale() const noexcept;
};

inline void HashValue(Kernel::Hasher&, const Hdr&) {}
inline void HashValue(Kernel::Hasher&, const Tonemapping&) {}
inline void HashValue(Kernel::Hasher&, const Exposure&) {}

// The sRGB transfer function (IEC 61966-2-1), exact (not a power of 2.2): an encoded value 0 to 1 to linear light, and
// linear light, clamped to 0 to 1, to its encoded value.
[[nodiscard]] float SrgbToLinear(float encoded) noexcept;
[[nodiscard]] float LinearToSrgb(float linear) noexcept;

// The method's curve on linear light, after Bevy's tone_mapping (tonemapping.wesl:373-428) with no colour grading:
// negative channels are taken as 0 first. Not clamped (a curve's result may pass 1; encoding clamps).
[[nodiscard]] Math::Vec3 Tonemap(Tonemapping::Method method, Math::Vec3 linear) noexcept;

// What a tonemapping pass writes for a pixel of the picture: Tonemap, then each channel encoded (LinearToSrgb) to the
// nearest byte; alpha (0 to 1) to the nearest byte as it is.
[[nodiscard]] Color8 TonemapToBytes(Tonemapping::Method method, Math::Vec3 linear, float alpha) noexcept;

// A Hdr camera's tonemapping pass: the built-in full-screen shader (ShaderAbi.hpp, kTonemappingFragmentSource) over
// viewport of output, sampling picture, the method in its parameters.
[[nodiscard]] PassFullscreen TonemappingPass(TextureId picture, std::optional<TextureId> output, std::optional<PixelRect> viewport,
                                             Tonemapping::Method method);

// The render World's HDR pictures (render's Resource, camera_render): one per camera target that Hdr cameras draw on
// this frame, shared by them (Bevy's main textures, keyed by the camera's target, bevy_render/src/view/mod.rs:1385), a
// render target of format Rgba16Float that follows that target's size. A camera that draws over another
// (ClearColorConfig::None, Camera3dDepth{false}) draws on the same picture.
struct HdrTargets final {
    struct Entry {
        // The camera target (nullopt: the main target) and its picture.
        std::optional<TextureId> output;
        TextureId picture{};
    };
    std::vector<Entry> entries;

    // The picture for the camera target, or nullptr.
    [[nodiscard]] const TextureId* Find(const std::optional<TextureId>& output) const noexcept;
};

inline void HashValue(Kernel::Hasher&, const HdrTargets&) {}

// Keeps one picture for each camera target of outputs (made the first time, kept while it is in outputs; a target may
// be in it more than once) and lets go of the others: a target no Hdr camera draws on this frame has none.
void SyncHdrTargets(const std::vector<std::optional<TextureId>>& outputs, RenderTextures& textures, HdrTargets& pictures);

} // namespace GYO::Engine::Render
