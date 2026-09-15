#include <doctest/doctest.h>

#include "render/ColorTransform.hpp"
#include "render/RenderQueue.hpp"

#include <limits>

namespace {

using namespace Engine::Render;

TEST_CASE("sRGB 0.5 survives the exact linear round trip") {
    const float linear = DecodeSrgbComponent(0.5F);
    CHECK(linear == doctest::Approx(0.21404114F).epsilon(0.000001F));
    CHECK(EncodeSrgbComponent(linear) == doctest::Approx(0.5F).epsilon(0.000001F));

    const Color decoded = DecodeSrgbColor({0.5F, 0.5F, 0.5F, 0.375F});
    const Color encoded = EncodeSrgbColor(decoded);
    CHECK(encoded.red == doctest::Approx(0.5F).epsilon(0.000001F));
    CHECK(encoded.alpha == doctest::Approx(0.375F));
}

TEST_CASE("scene exposure and gamma adjustments are monotonic") {
    const Color middle{0.25F, 0.25F, 0.25F, 0.4F};
    const Color darkerExposure = ApplySceneColorTransform(middle, {-1.0F, 1.0F});
    const Color identity = ApplySceneColorTransform(middle, {0.0F, 1.0F});
    const Color brighterExposure = ApplySceneColorTransform(middle, {1.0F, 1.0F});
    CHECK(darkerExposure.red < identity.red);
    CHECK(identity.red < brighterExposure.red);

    const Color lowerGamma = ApplySceneColorTransform(middle, {0.0F, 0.75F});
    const Color higherGamma = ApplySceneColorTransform(middle, {0.0F, 1.5F});
    CHECK(lowerGamma.red < identity.red);
    CHECK(identity.red < higherGamma.red);
    CHECK(higherGamma.alpha == doctest::Approx(middle.alpha));
}

TEST_CASE("scene color transform rejects non-finite and out-of-range values") {
    CHECK(IsValidSceneColorTransform({0.0F, 1.0F}));
    CHECK(IsValidSceneColorTransform({
        kMinimumSceneExposureEv,
        kMinimumSceneGammaAdjustment,
    }));
    CHECK(IsValidSceneColorTransform({
        kMaximumSceneExposureEv,
        kMaximumSceneGammaAdjustment,
    }));

    CHECK_FALSE(IsValidSceneColorTransform({kMinimumSceneExposureEv - 0.01F, 1.0F}));
    CHECK_FALSE(IsValidSceneColorTransform({kMaximumSceneExposureEv + 0.01F, 1.0F}));
    CHECK_FALSE(IsValidSceneColorTransform({0.0F, kMinimumSceneGammaAdjustment - 0.01F}));
    CHECK_FALSE(IsValidSceneColorTransform({0.0F, kMaximumSceneGammaAdjustment + 0.01F}));
    CHECK_FALSE(IsValidSceneColorTransform({
        std::numeric_limits<float>::infinity(),
        1.0F,
    }));
    CHECK_FALSE(IsValidSceneColorTransform({
        0.0F,
        std::numeric_limits<float>::quiet_NaN(),
    }));
}

TEST_CASE("CPU reference keeps Overlay unchanged while Scene is adjusted") {
    constexpr Color sample{0.25F, 0.4F, 0.6F, 0.75F};
    constexpr SceneColorTransform transform{1.0F, 1.2F};

    RenderQueue queue;
    SpriteSubmission scene{};
    scene.destinationPixels = {0.0F, 0.0F, 1.0F, 1.0F};
    scene.tint = sample;
    scene.layer = CompositeLayer::Scene;
    REQUIRE(queue.Submit(scene));

    SpriteSubmission overlay{};
    overlay.destinationPixels = {1.0F, 0.0F, 1.0F, 1.0F};
    overlay.tint = sample;
    overlay.layer = CompositeLayer::Overlay;
    REQUIRE(queue.Submit(overlay));

    REQUIRE(queue.Sprites().size() == 2);
    const auto referenceComposite = [transform](const SpriteSubmission& sprite) {
        return sprite.layer == CompositeLayer::Scene
            ? ApplySceneColorTransform(sprite.tint, transform)
            : sprite.tint;
    };
    const Color displayedScene = referenceComposite(queue.Sprites()[0]);
    const Color displayedOverlay = referenceComposite(queue.Sprites()[1]);

    CHECK(queue.Sprites()[0].layer == CompositeLayer::Scene);
    CHECK(queue.Sprites()[1].layer == CompositeLayer::Overlay);
    CHECK(displayedScene.red != doctest::Approx(sample.red));
    CHECK(displayedScene.green != doctest::Approx(sample.green));
    CHECK(displayedOverlay.red == doctest::Approx(sample.red));
    CHECK(displayedOverlay.green == doctest::Approx(sample.green));
    CHECK(displayedOverlay.blue == doctest::Approx(sample.blue));
    CHECK(displayedOverlay.alpha == doctest::Approx(sample.alpha));
}

} // namespace
