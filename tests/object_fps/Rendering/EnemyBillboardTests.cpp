#include "../TestSupport.hpp"

#include "RetroFPS/Rendering/EnemyRenderSettings.hpp"

#include <numbers>
#include <string>

namespace fps::tests {
namespace {

[[nodiscard]] EnemyDefinition MakeDefinition() {
    EnemyDefinition definition;
    definition.id = "melee_basic";
    definition.kind = EnemyKind::Melee;
    definition.hitboxRadius = 0.2f;
    definition.hitboxHeight = 3.5f;
    definition.renderWidth = 0.973913f;
    definition.renderHeight = 0.8f;
    definition.frameWidthPixels = 560;
    definition.frameHeightPixels = 460;
    definition.animations.idle = {0, 0, 3, 0.1f, std::nullopt, std::nullopt};
    definition.animations.moving = {0, 460, 4, 0.1f, std::nullopt, std::nullopt};
    definition.animations.attacking = {0, 920, 6, 0.05f, 3, std::nullopt};
    definition.animations.dead = {0, 1380, 4, 0.1f, std::nullopt, std::nullopt};
    return definition;
}

void TestBillboardPose(TestContext& context) {
    EnemyDefinition definition = MakeDefinition();
    const EnemyBillboardPose forward = ResolveEnemyBillboardPose(
        definition, {1.0f, 1.0f}, {1.0f, 2.0f});
    const EnemyBillboardPose right = ResolveEnemyBillboardPose(
        definition, {1.0f, 1.0f}, {2.0f, 1.0f});

    context.Expect(
        NearlyEqual(forward.width, definition.renderWidth) &&
            NearlyEqual(forward.height, definition.renderHeight),
        "billboard pose uses definition render size");
    context.Expect(
        !NearlyEqual(forward.height, definition.hitboxHeight),
        "billboard pose is independent from hitbox height");
    context.Expect(
        NearlyEqual(forward.centerY, definition.renderHeight * 0.5f),
        "billboard center keeps its rendered bottom edge on the ground");
    context.Expect(
        NearlyEqual(forward.yawRadians, 0.0f),
        "viewer on +Z uses the GYO procedural quad's +Z face");
    context.Expect(
        NearlyEqual(right.yawRadians, std::numbers::pi_v<float> * 0.5f),
        "viewer on +X rotates the GYO procedural quad toward the viewer");

    const EnemyBillboardPose coincident = ResolveEnemyBillboardPose(
        definition, {3.0f, 4.0f}, {3.0f, 4.0f}, 0.75f);
    context.Expect(
        NearlyEqual(coincident.yawRadians, 0.75f),
        "coincident viewer preserves the previous billboard yaw");
}

void TestAnimationSelection(TestContext& context) {
    const EnemyDefinition definition = MakeDefinition();
    context.Expect(
        &GetEnemyAnimationClip(definition, EnemyState::Idle) ==
            &definition.animations.idle,
        "idle state selects the idle atlas row");
    context.Expect(
        &GetEnemyAnimationClip(definition, EnemyState::Moving) ==
            &definition.animations.moving,
        "moving state selects the moving atlas row");
    context.Expect(
        &GetEnemyAnimationClip(definition, EnemyState::Attacking) ==
            &definition.animations.attacking,
        "attacking state selects the attacking atlas row");
    context.Expect(
        &GetEnemyAnimationClip(definition, EnemyState::Dead) ==
            &definition.animations.dead,
        "dead state selects the dead atlas row");

    EnemyAnimationClipDefinition emptyClip{};
    context.Expect(
        !ResolveEnemyAnimationFrame(emptyClip, EnemyState::Idle, 1.0f).has_value(),
        "empty animation clip has no frame");

    context.Expect(
        ResolveEnemyAnimationFrame(
            definition.animations.idle, EnemyState::Idle, 0.0f) == 0,
        "animation starts on frame zero");
    context.Expect(
        ResolveEnemyAnimationFrame(
            definition.animations.idle, EnemyState::Idle, 0.35f) == 0,
        "idle animation loops");
    context.Expect(
        ResolveEnemyAnimationFrame(
            definition.animations.moving, EnemyState::Moving, 0.45f) == 0,
        "moving animation loops");
    context.Expect(
        ResolveEnemyAnimationFrame(
            definition.animations.attacking, EnemyState::Attacking, 5.0f) == 5,
        "attack animation holds its final frame");
    context.Expect(
        ResolveEnemyAnimationFrame(
            definition.animations.dead, EnemyState::Dead, 5.0f) == 3,
        "dead animation holds its final frame");
}

void TestStateToAtlasRowIntegration(TestContext& context) {
    const EnemyDefinition definition = MakeDefinition();
    struct ExpectedSample final {
        EnemyState state;
        float elapsedSeconds;
        std::uint32_t originYpx;
        std::size_t frameIndex;
    };
    constexpr ExpectedSample samples[] = {
        {EnemyState::Idle, 0.201f, 0, 2},
        {EnemyState::Moving, 0.301f, 460, 3},
        {EnemyState::Attacking, 0.151f, 920, 3},
        {EnemyState::Dead, 0.301f, 1380, 3},
    };

    for (const ExpectedSample& sample : samples) {
        const EnemyAnimationClipDefinition& clip =
            GetEnemyAnimationClip(definition, sample.state);
        const std::optional<std::size_t> frame = ResolveEnemyAnimationFrame(
            clip, sample.state, sample.elapsedSeconds);
        context.Expect(
            clip.originYpx == sample.originYpx && frame == sample.frameIndex,
            "runtime enemy state resolves to its matching atlas row and frame");
        if (!frame.has_value()) {
            continue;
        }
        const std::optional<EnemyAtlasUvTransform> uv = ResolveEnemyAtlasUv(
            clip,
            definition.frameWidthPixels,
            definition.frameHeightPixels,
            *frame,
            3360,
            1840);
        const float expectedTopCenter =
            (static_cast<float>(sample.originYpx) + 0.5f) / 1840.0f;
        context.Expect(
            uv.has_value() && NearlyEqual(uv->offsetY, expectedTopCenter),
            "resolved atlas material preserves the selected state's vertical row offset");
    }
}

void TestAtlasUvTransforms(TestContext& context) {
    const EnemyDefinition definition = MakeDefinition();
    const EnemyAnimationClipDefinition& attack = definition.animations.attacking;
    const std::optional<EnemyAtlasUvTransform> first = ResolveEnemyAtlasUv(
        attack, 560, 460, 0, 3360, 1840);
    const std::optional<EnemyAtlasUvTransform> last = ResolveEnemyAtlasUv(
        attack, 560, 460, 5, 3360, 1840);

    context.Expect(first.has_value() && last.has_value(), "valid atlas endpoints resolve");
    if (first.has_value() && last.has_value()) {
        context.Expect(
            NearlyEqual(first->offsetX, 0.5f / 3360.0f) &&
                NearlyEqual(first->offsetY, 920.5f / 1840.0f),
            "first attack frame starts at inset centers in GYO quad UV space");
        context.Expect(
            NearlyEqual(last->offsetX, 2800.5f / 3360.0f) &&
                NearlyEqual(last->offsetY, first->offsetY),
            "last attack frame uses the final horizontal atlas cell");
        context.Expect(
            NearlyEqual(first->scaleX, 559.0f / 3360.0f) &&
                NearlyEqual(first->scaleY, 459.0f / 1840.0f),
            "atlas UV scale applies a half-texel inset at both endpoints");
    }

    EnemyAnimationClipDefinition rangedDead;
    rangedDead.originYpx = 2730;
    rangedDead.frameCount = 4;
    rangedDead.secondsPerFrame = 0.1f;
    const std::optional<EnemyAtlasUvTransform> rangedLast = ResolveEnemyAtlasUv(
        rangedDead, 700, 910, 3, 3500, 3640);
    context.Expect(
        rangedLast.has_value() &&
            NearlyEqual(rangedLast->offsetX, 2100.5f / 3500.0f) &&
            NearlyEqual(rangedLast->offsetY, 2730.5f / 3640.0f) &&
            NearlyEqual(rangedLast->scaleY, 909.0f / 3640.0f),
        "ranged final death frame resolves against its sheet dimensions");

    context.Expect(
        !ResolveEnemyAtlasUv(attack, 560, 460, 6, 3360, 1840).has_value(),
        "frame index beyond the clip is rejected");
    context.Expect(
        !ResolveEnemyAtlasUv(attack, 560, 460, 5, 3359, 1840).has_value(),
        "frame rectangle beyond the loaded sheet is rejected");
}

} // namespace

void RunEnemyBillboardTests(TestContext& context) {
    TestBillboardPose(context);
    TestAnimationSelection(context);
    TestStateToAtlasRowIntegration(context);
    TestAtlasUvTransforms(context);
}

} // namespace fps::tests
