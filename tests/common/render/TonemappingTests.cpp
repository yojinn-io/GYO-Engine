// The tonemapping curves, the sRGB transfer function, Exposure and the tonemapping pass's data (render-pipeline
// declaration section 16, v8-arch-gyo2-render-a4; Tonemapping.hpp). The expected values are Bevy's formulas
// (bevy_core_pipeline/src/tonemapping.wesl at 6a911653f) evaluated apart from this code, in double precision, at five
// stimuli: grey past 1, a dark colour, a bright saturated one, a near-black one (the PBR Neutral toe) and one with a
// negative channel (taken as 0 first, tone_mapping's max). Tags "[tm-...]" are the mutants' expectations.
#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <cstring>
#include <optional>
#include <string>
#include <variant>

#include "engine/render/ShaderAbi.hpp"
#include "engine/render/Tonemapping.hpp"

namespace M = GYO::Engine::Math;
namespace R = GYO::Engine::Render;
namespace Abi = GYO::Engine::Render::ShaderAbi;

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
        const M::Vec3 got = R::Tonemap(method, kStimuli[i]);
        CHECK_MESSAGE(Near(got, expected[i]), tag, " stimulus ", i, ": ", got.x, ", ", got.y, ", ", got.z);
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
    const M::Vec3 black = R::Tonemap(Method::ReinhardLuminance, {0.0F, 0.0F, 0.0F});
    CHECK_MESSAGE((black.x == 0.0F && black.y == 0.0F && black.z == 0.0F), "[tm-luminance-black] no 0 / 0 at black");
}

TEST_CASE("render: the default tonemapping, the sRGB transfer function and Exposure") {
    CHECK_MESSAGE(R::Tonemapping{}.method == Method::ReinhardLuminance, "[tm-default] Bevy's default before its lookup-table curves");
    // IEC 61966-2-1: 0.5 linear is 187.5 of 255 encoded; 0.04045 encoded is the toe's end, 0.0031308 linear.
    CHECK_MESSAGE(std::fabs(R::LinearToSrgb(0.5F) * 255.0F - 187.516F) <= 1e-2F, "[tm-encode] the exact curve, not a power of 2.2");
    CHECK_MESSAGE(std::fabs(R::LinearToSrgb(0.001F) - 0.01292F) <= 1e-6F, "[tm-encode-toe] the linear toe below 0.0031308");
    CHECK_MESSAGE((std::fabs(R::LinearToSrgb(4.0F) - 1.0F) <= 1e-6F && R::LinearToSrgb(-1.0F) == 0.0F), "[tm-encode-clamped] clamped to 0 to 1");
    CHECK_MESSAGE(std::fabs(R::SrgbToLinear(188.0F / 255.0F) - 0.5029F) <= 1e-4F, "[tm-decode] 188 of 255 is 0.503 linear");
    CHECK_MESSAGE(std::fabs(R::SrgbToLinear(0.04F) - 0.04F / 12.92F) <= 1e-7F, "[tm-decode-toe] the linear toe");
    for (int byte = 0; byte <= 255; ++byte) {
        const float encoded = static_cast<float>(byte) / 255.0F;
        const long back = std::lround(R::LinearToSrgb(R::SrgbToLinear(encoded)) * 255.0F);
        CHECK_MESSAGE(back == byte, "[tm-round-trip] byte ", byte, " decodes and encodes back to itself");
    }
    // Bevy's Exposure::exposure: exp2(-ev100) / 1.2; Exposure::BLENDER (9.7) by default.
    CHECK_MESSAGE(R::Exposure{}.ev100 == 9.7F, "[tm-exposure-default] Blender's EV100");
    CHECK_MESSAGE(std::fabs(R::Exposure{}.Scale() - 0.00100190789F) <= 1e-9F, "[tm-exposure-scale] exp2(-9.7) / 1.2");
    CHECK_MESSAGE(std::fabs(R::Exposure{0.0F}.Scale() - 1.0F / 1.2F) <= 1e-7F, "[tm-exposure-zero] EV100 0 is 1 / 1.2");
}

TEST_CASE("render: HDR pictures: one per camera target, following its size, kept while named, let go after") {
    R::RenderTextures textures;
    textures.SetMainSize(R::TargetSize{8, 8});
    const R::TextureId monitor = textures.CreateTarget(R::RenderTargetDesc{R::TargetSize{4, 4}});
    R::HdrTargets pictures;
    R::SyncHdrTargets({std::nullopt, monitor, std::nullopt}, textures, pictures);
    CHECK_MESSAGE(pictures.entries.size() == 2, "[tm-sync-one-per-target] two cameras on the main target share one picture");
    const R::TextureId* mainPicture = pictures.Find(std::nullopt);
    const R::TextureId* monitorPicture = pictures.Find(monitor);
    REQUIRE((mainPicture != nullptr && monitorPicture != nullptr));
    const R::RenderTargetDesc* desc = textures.FindTarget(*monitorPicture);
    REQUIRE(desc != nullptr);
    const auto* follows = std::get_if<R::FollowTarget>(&desc->size);
    CHECK_MESSAGE((follows != nullptr && follows->target == monitor && textures.SizeOf(*monitorPicture) == R::TargetSize{4, 4} &&
                   textures.SizeOf(*mainPicture) == R::TargetSize{8, 8}),
                  "[tm-sync-follows] each picture follows its camera target's size");
    CHECK_MESSAGE((desc->format == R::RenderTargetFormat::Rgba16Float && desc->filter == R::TextureFilter::Nearest),
                  "[tm-sync-format] linear light, read at its own pixels");
    const R::TextureId keptId = *monitorPicture;
    const R::TextureId goneId = *mainPicture;
    R::SyncHdrTargets({monitor}, textures, pictures);
    CHECK_MESSAGE((pictures.Find(monitor) != nullptr && *pictures.Find(monitor) == keptId), "[tm-sync-kept] the same picture while named");
    CHECK_MESSAGE((pictures.Find(std::nullopt) == nullptr && textures.FindTarget(goneId) == nullptr),
                  "[tm-sync-let-go] a target no camera names any more has its picture let go");
}

TEST_CASE("render: what a tonemapping pass writes for a pixel, and the pass's data") {
    // 4.0 grey through Reinhard: 0.8 linear, 231 encoded; the alpha byte as it is.
    CHECK_MESSAGE((R::TonemapToBytes(Method::Reinhard, {4.0F, 4.0F, 4.0F}, 1.0F) == R::Color8{231, 231, 231, 255}),
                  "[tm-bytes] the curve, then the encoding, to the nearest byte");
    CHECK_MESSAGE((R::TonemapToBytes(Method::None, {4.0F, 0.5F, 0.0F}, 0.5F) == R::Color8{255, 188, 0, 128}), "[tm-bytes-none] clamped and encoded");
    const R::PassFullscreen pass = R::TonemappingPass(R::TextureId{7}, R::TextureId{3}, R::PixelRect{1, 2, 3, 4}, Method::AcesFitted);
    CHECK_MESSAGE((pass.target == R::TextureId{3} && pass.viewport == R::PixelRect{1, 2, 3, 4}), "[tm-pass-on] on the camera's target, over its viewport");
    CHECK_MESSAGE(pass.shader == "tonemapping.frag", "[tm-pass-shader] the built-in tonemapping shader by its name");
    CHECK_MESSAGE((pass.inputs.size() == 1 && pass.inputs[0] == R::TextureId{7}), "[tm-pass-input] it samples the picture");
    REQUIRE(pass.parameters.size() == sizeof(Abi::TonemappingParameters));
    float lanes[4];
    std::memcpy(lanes, pass.parameters.data(), sizeof(lanes));
    CHECK_MESSAGE((lanes[0] == 4.0F && lanes[1] == 0.0F && lanes[2] == 0.0F && lanes[3] == 0.0F), "[tm-pass-method] lane 0 is the method's value");
}
