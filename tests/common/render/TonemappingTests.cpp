// The tonemapping curves (the tests' reference, TonemappingReference.hpp), the colour rule of a target, the Hdr
// cameras' targets (SyncHdrTargets) and the tonemapping pass's data (render-pipeline declaration section 16, revision
// 16.8, v8-arch-gyo2-render-a4). The curves' expected values are Bevy's formulas (bevy_core_pipeline/src/tonemapping.wesl
// at 6a911653f) worked out apart from this code, in double precision, at five stimuli: grey past 1, a dark colour, a
// bright saturated one, a near-black one (the PBR Neutral toe) and one with a negative channel (taken as 0 first,
// tone_mapping's max). Tags "[tm-...]" are the mutants' expectations.
#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <cstring>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "engine/render/Compose.hpp"
#include "engine/render/Drawables.hpp"
#include "engine/render/RenderTargets.hpp"
#include "engine/render/RenderTextures.hpp"
#include "engine/render/ShaderAbi.hpp"
#include "engine/render/TonemappingMethods.h"

#include "TonemappingReference.hpp"

namespace M = GYO::Engine::Math;
namespace R = GYO::Engine::Render;
namespace Abi = GYO::Engine::Render::ShaderAbi;
namespace TM = GYO::Test::Tonemapping;

namespace {

using Method = R::Tonemapping::Method;

// Float against the double-precision values: 2e-5 (a float's rounding through the curves, well under a byte's 1/255).
constexpr float kTolerance = 2e-5F;

bool Near(const M::Vec3 a, const std::array<float, 3> b) {
    return std::fabs(a.x - b[0]) <= kTolerance && std::fabs(a.y - b[1]) <= kTolerance && std::fabs(a.z - b[2]) <= kTolerance;
}

constexpr std::array<M::Vec3, 5> kStimuli{{{4.0F, 4.0F, 4.0F}, {0.5F, 0.25F, 0.125F}, {4.0F, 1.0F, 0.25F}, {0.02F, 0.05F, 0.01F}, {-1.0F, 2.0F, 0.05F}}};

void CheckCurve(const Method method, const std::array<std::array<float, 3>, 5>& expected, const char* tag) {
    for (std::size_t i = 0; i < kStimuli.size(); ++i) {
        const M::Vec3 got = TM::Tonemap(method, kStimuli[i]);
        CHECK_MESSAGE(Near(got, expected[i]), std::string(tag) << " stimulus " << i << ": " << got.x << ", " << got.y << ", " << got.z);
    }
}

} // namespace

TEST_CASE("render: each tonemapping curve is Bevy's formula") {
    CheckCurve(Method::None, {{{4.0F, 4.0F, 4.0F}, {0.5F, 0.25F, 0.125F}, {4.0F, 1.0F, 0.25F}, {0.02F, 0.05F, 0.01F}, {0.0F, 2.0F, 0.05F}}},
               "[tm-none] no curve: the light as it is, negatives at 0");
    CheckCurve(Method::Reinhard,
               {{{0.8F, 0.8F, 0.8F}, {0.3333333F, 0.2F, 0.1111111F}, {0.8F, 0.5F, 0.2F}, {0.0196078F, 0.0476190F, 0.0099010F},
                 {0.0F, 0.6666667F, 0.0476190F}}},
               "[tm-reinhard] c / (1 + c), tonemapping.wesl:230");
    CheckCurve(Method::ReinhardLuminance,
               {{{0.8F, 0.8F, 0.8F}, {0.3863614F, 0.1931807F, 0.0965904F}, {1.5481973F, 0.3870493F, 0.0967623F},
                 {0.0192172F, 0.0480430F, 0.0096086F}, {0.0F, 0.8216893F, 0.0205422F}}},
               "[tm-reinhard-luminance] Reinhard on the luminance, tonemapping.wesl:245");
    CheckCurve(Method::AcesFitted,
               {{{0.9090138F, 0.9090138F, 0.9090047F}, {0.3667057F, 0.1729550F, 0.0741484F}, {0.9948911F, 0.6633806F, 0.3166845F},
                 {0.0030782F, 0.0133197F, 0.0014262F}, {0.3725712F, 0.8153648F, 0.1798016F}}},
               "[tm-aces] the ACES fit, tonemapping.wesl:106-140");
    CheckCurve(Method::SomewhatBoringDisplayTransform,
               {{{0.9522338F, 0.9522338F, 0.9522338F}, {0.4162913F, 0.2105428F, 0.1059479F}, {0.9837841F, 0.6190136F, 0.2595816F},
                 {0.0190107F, 0.0475249F, 0.0095055F}, {0.0444465F, 0.8583215F, 0.0856697F}}},
               "[tm-boring] SomewhatBoringDisplayTransform, tonemapping.wesl:39-80");
    CheckCurve(Method::KhronosPbrNeutral,
               {{{0.9832558F, 0.9832558F, 0.9832558F}, {0.46F, 0.21F, 0.085F}, {0.9832558F, 0.4682992F, 0.33956F},
                 {0.010625F, 0.040625F, 0.000625F}, {0.1295794F, 0.9610811F, 0.150367F}}},
               "[tm-pbr-neutral] Khronos PBR Neutral, tonemapping.wesl:258-301");
}

TEST_CASE("render: Reinhard on the luminance keeps black black") {
    const M::Vec3 black = TM::Tonemap(Method::ReinhardLuminance, {0.0F, 0.0F, 0.0F});
    CHECK_MESSAGE((black.x == 0.0F && black.y == 0.0F && black.z == 0.0F), "[tm-luminance-black] no 0 / 0 at black");
}

TEST_CASE("render: the default tonemapping and the methods' values") {
    CHECK_MESSAGE(R::Tonemapping{}.method == Method::ReinhardLuminance, "[tm-default] Bevy's default before its lookup-table curves");
    // The C++ enum takes its values from the one list the shader includes too.
    CHECK_MESSAGE((static_cast<int>(Method::None) == GYO_TONEMAP_NONE && static_cast<int>(Method::Reinhard) == GYO_TONEMAP_REINHARD &&
                   static_cast<int>(Method::KhronosPbrNeutral) == GYO_TONEMAP_KHRONOS_PBR_NEUTRAL),
                  "[tm-method-values] the methods' values are TonemappingMethods.h's");
}

TEST_CASE("render: what the colours on a target stand for: bytes on an 8-bit target, linear light on a float one") {
    CHECK_MESSAGE((R::ColourSpaceOf(R::RenderTargetFormat::Rgba8Unorm) == R::ColourSpace::Encoded &&
                   R::ColourSpaceOf(R::RenderTargetFormat::Rgba16Float) == R::ColourSpace::Linear),
                  "[tm-colour-space] a Rgba16Float target's colours are linear light, a Rgba8Unorm one's bytes");
    CHECK_MESSAGE((R::SampledAsLinear(R::TextureFormat::Rgba8UnormSrgb) && !R::SampledAsLinear(R::TextureFormat::Rgba8Unorm) &&
                   R::SampledAsLinear(R::RenderTargetFormat::Rgba16Float) && !R::SampledAsLinear(R::RenderTargetFormat::Rgba8Unorm)),
                  "[tm-sampled-linear] an sRGB texture and a float target sample as linear light (never decoded again)");
}

TEST_CASE("render: Hdr cameras' targets: a picture each, one depth per camera target, kept while drawn, let go after") {
    R::RenderTextures textures;
    textures.SetMainSize(R::TargetSize{8, 8});
    const R::TextureId monitor = textures.CreateTarget(R::RenderTargetDesc{R::TargetSize{4, 4}});
    R::HdrTargets targets;
    const std::vector<R::HdrCamera> first{{1, std::nullopt}, {2, std::nullopt}, {3, monitor}};
    R::SyncHdrTargets(first, textures, targets);
    REQUIRE((targets.PictureOf(1) && targets.PictureOf(2) && targets.PictureOf(3)));
    CHECK_MESSAGE((targets.pictures.size() == 3 && *targets.PictureOf(1) != *targets.PictureOf(2)),
                  "[tm-sync-picture-per-camera] two cameras on the main target have a picture each");
    CHECK_MESSAGE((targets.depths.size() == 2 && targets.DepthOf(std::nullopt) && targets.DepthOf(monitor)),
                  "[tm-sync-depth-per-target] one depth per camera target");
    const R::RenderTargetDesc* picture = textures.FindTarget(*targets.PictureOf(3));
    const R::RenderTargetDesc* depth = textures.FindTarget(*targets.DepthOf(monitor));
    REQUIRE((picture != nullptr && depth != nullptr));
    const auto* follows = std::get_if<R::FollowTarget>(&picture->size);
    const auto* depthFollows = std::get_if<R::FollowTarget>(&depth->size);
    CHECK_MESSAGE((follows != nullptr && follows->target == monitor && depthFollows != nullptr && depthFollows->target == monitor &&
                   textures.SizeOf(*targets.PictureOf(3)) == R::TargetSize{4, 4} && textures.SizeOf(*targets.PictureOf(1)) == R::TargetSize{8, 8}),
                  "[tm-sync-follows] each follows its camera target's size");
    CHECK_MESSAGE((picture->format == R::RenderTargetFormat::Rgba16Float && picture->filter == R::TextureFilter::Nearest &&
                   depth->format == R::RenderTargetFormat::Depth),
                  "[tm-sync-format] the picture is linear light read at its own texels, the depth a Depth target");
    const R::TextureId kept = *targets.PictureOf(3);
    const R::TextureId keptDepth = *targets.DepthOf(monitor);
    const R::TextureId gone = *targets.PictureOf(1);
    const R::TextureId goneDepth = *targets.DepthOf(std::nullopt);
    const std::vector<R::HdrCamera> next{{3, monitor}};
    R::SyncHdrTargets(next, textures, targets);
    CHECK_MESSAGE((targets.PictureOf(3) == kept && targets.DepthOf(monitor) == keptDepth), "[tm-sync-kept] the same targets while drawn");
    CHECK_MESSAGE((!targets.PictureOf(1) && !targets.DepthOf(std::nullopt) && textures.FindTarget(gone) == nullptr &&
                   textures.FindTarget(goneDepth) == nullptr),
                  "[tm-sync-let-go] a camera that does not draw, a target no Hdr camera draws on, have theirs let go");
    const std::vector<R::HdrCamera> moved{{3, std::nullopt}};
    R::SyncHdrTargets(moved, textures, targets);
    CHECK_MESSAGE((targets.PictureOf(3) && *targets.PictureOf(3) != kept && textures.FindTarget(kept) == nullptr),
                  "[tm-sync-moved] a camera on another target has a picture of that target's size");
}

TEST_CASE("render: what a tonemapping pass writes for a pixel, and the pass's data") {
    // 4.0 grey through Reinhard: 0.8 linear, 231 encoded; the alpha byte as it is.
    CHECK_MESSAGE((TM::TonemapToBytes(Method::Reinhard, {4.0F, 4.0F, 4.0F}, 1.0F) == R::Color8{231, 231, 231, 255}),
                  "[tm-bytes] the curve, then the encoding, to the nearest byte");
    CHECK_MESSAGE((TM::TonemapToBytes(Method::None, {4.0F, 0.5F, 0.0F}, 0.5F) == R::Color8{255, 188, 0, 128}),
                  "[tm-bytes-none] clamped and encoded");
    const R::PassFullscreen pass =
        R::TonemappingPass(R::TextureId{7}, R::TextureId{3}, R::PixelRect{1, 2, 3, 4}, Method::AcesFitted, R::BlendMode::Alpha);
    CHECK_MESSAGE((pass.target == R::TextureId{3} && pass.viewport == R::PixelRect{1, 2, 3, 4}), "[tm-pass-on] on the camera's target, over its viewport");
    CHECK_MESSAGE(pass.shader == "tonemapping.frag", "[tm-pass-shader] the built-in tonemapping shader by its name");
    CHECK_MESSAGE((pass.inputs.size() == 1 && pass.inputs[0] == R::TextureId{7}), "[tm-pass-input] it samples the picture");
    CHECK_MESSAGE(pass.blend == R::BlendMode::Alpha, "[tm-pass-blend] put on the target by the blend given");
    REQUIRE(pass.parameters.size() == sizeof(Abi::TonemappingParameters));
    float lanes[4];
    std::memcpy(lanes, pass.parameters.data(), sizeof(lanes));
    CHECK_MESSAGE((lanes[0] == 4.0F && lanes[1] == 0.0F && lanes[2] == 0.0F && lanes[3] == 0.0F), "[tm-pass-method] lane 0 is the method's value");
}
