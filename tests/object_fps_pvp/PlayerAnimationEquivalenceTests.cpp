#include <doctest/doctest.h>

#include "PresentationLegacyLocomotion.hpp"
#include "RetroFPS/Pvp/PlayerAnimation.hpp"
#include "RetroFPS/Pvp/PlayerAnimationContent.hpp"
#include "RetroFPS/Pvp/PlayerPresentation.hpp"
#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/AssetManager.hpp"
#include "engine/asset/ContentManifest.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/loaders/sdl_image/SdlImageTextureLoader.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "engine/model/backend/ufbx/UfbxModelLoader.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include <nlohmann/json.hpp>
#include "gyo/AppConfig.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// v8-product-anim-authority A1: the Match's integer state machine against the
// Client's float one it replaced (E1; frozen in PresentationLegacyLocomotion.hpp), and the Match's texture-free rules
// against the Client's (v8-product-anim-rules-source-texture-free).
namespace {
using namespace fps::pvp;

Engine::Asset::AssetCatalog LoadCatalog() {
    const auto base = Engine::Platform::Sdl::BaseDirectory();
    if (!base) throw std::runtime_error("Cannot locate the PvP test product directory");
    const auto root = base.value() / Gyo::AppConfig::Assets;
    const auto manifest = Engine::Asset::ContentManifest::Load(root);
    if (!manifest) throw std::runtime_error(manifest.error().message + " " + manifest.error().detail);
    auto catalog = manifest.value().LoadCatalogs(root);
    if (!catalog) throw std::runtime_error(catalog.error().message + " " + catalog.error().detail);
    return std::move(catalog).value();
}

// The product content with definition overrides in memory.
class OverrideSource final : public Engine::Asset::Loading::IAssetSource {
public:
    std::unordered_map<std::string, std::string> overrides;
    Engine::Base::Result<Engine::Asset::Loading::ByteBuffer, Engine::Asset::AssetError>
    ReadAll(std::string_view path) override {
        const auto found = overrides.find(std::string(path));
        if (found != overrides.end()) {
            const auto bytes = std::as_bytes(std::span(found->second.data(), found->second.size()));
            return Engine::Asset::Loading::ByteBuffer(bytes.begin(), bytes.end());
        }
        Engine::Asset::Loading::NativeFileAssetSource native;
        return native.ReadAll(path);
    }
};

// The product content, with or without a texture decoder registered.
struct ContentAssets final {
    Engine::Asset::AssetCatalog catalog{LoadCatalog()};
    Engine::Asset::Loading::LoaderRegistry loaders;
    OverrideSource source;
    Engine::Asset::Loading::AssetPipeline pipeline{source, loaders};
    Engine::Asset::Core::AssetStorage storage;
    Engine::Asset::Core::AssetLifetime lifetime;
    Engine::Asset::Core::AssetCachePolicy policy{{}};
    Engine::Asset::AssetManager assets{catalog, pipeline, storage, lifetime, policy};
    explicit ContentAssets(bool textures) {
        loaders.Register(std::make_unique<Engine::Asset::Loaders::TextLoader>());
        if (textures) loaders.Register(std::make_unique<Engine::Asset::Loaders::SdlImage::SdlImageTextureLoader>());
        loaders.Register(std::make_unique<Engine::Model::Ufbx::UfbxModelLoader>());
    }
    template<class Edit>
    void Override(std::string_view id, Edit edit) {
        const auto* entry = catalog.Find(Engine::Asset::AssetId::FromString(id));
        if (!entry) throw std::runtime_error("Missing PvP definition " + std::string(id));
        const auto bytes = source.ReadAll(entry->resolvedPath);
        if (!bytes) throw std::runtime_error(bytes.error().message);
        auto json = nlohmann::json::parse(std::string_view(
            reinterpret_cast<const char*>(bytes.value().data()), bytes.value().size()));
        edit(json);
        source.overrides[entry->resolvedPath] = json.dump();
    }
};

const PlayerPresentationDefinition& ClientDefinition() {
    static ContentAssets fixture{true};
    static const auto definition = [&] {
        std::string error;
        auto loaded = LoadPlayerPresentationDefinition(fixture.assets, 1.8F, error);
        if (!loaded) throw std::runtime_error(error);
        return loaded;
    }();
    return *definition;
}

// One authority state per tick from tick 1, as the Match produces them.
struct Sample final {
    Engine::Math::Vec3 position{};
    float yaw{};
    bool grounded{true};
    float verticalVelocity{};
    bool crouched{};
    std::uint64_t movementEpoch{1};
};

constexpr int RouteTicks = 600; // 10 s

// Integrates a planar velocity (m/s along x and z) into positions.
std::vector<Sample> Planar(const std::function<Engine::Math::Vec3(int)>& velocity, float yaw = 0) {
    std::vector<Sample> route;
    Engine::Math::Vec3 position{};
    for (int tick = 1; tick <= RouteTicks; ++tick) {
        const auto v = velocity(tick);
        position.x += v.x / AuthorityTickRate;
        position.z += v.z / AuthorityTickRate;
        route.push_back({position, yaw});
    }
    return route;
}

// Every `period` ticks a jump of `air` ticks: rising at liftoff, falling after.
void Jumps(std::vector<Sample>& route, int period, int air, bool rising = true) {
    for (int tick = 1; tick <= RouteTicks; ++tick) {
        const int in = tick % period;
        auto& sample = route[static_cast<std::size_t>(tick - 1)];
        if (in >= 1 && in <= air) {
            sample.grounded = false;
            sample.verticalVelocity = rising && in <= air / 2 ? 4.0F : -4.0F;
        }
    }
}

struct Route final {
    const char* name;
    std::vector<Sample> samples;
};

std::vector<Route> Routes() {
    using V = Engine::Math::Vec3;
    std::vector<Route> routes;
    routes.push_back({"walk", Planar([](int) { return V{0, 0, 1.5F}; })});
    routes.push_back({"jog", Planar([](int) { return V{3.2F, 0, 3.83F}; }, 0.6962F)}); // along a 0.7 rad facing
    routes.push_back({"accelerate", Planar([](int t) { return V{0, 0, std::min(6.0F, t * 6.0F / 300)}; })});
    routes.push_back({"decelerate", Planar([](int t) { return V{0, 0, std::max(0.0F, 6.0F - t * 6.0F / 300)}; })});
    routes.push_back({"stop-go", Planar([](int t) { return V{0, 0, (t / 60) % 3 == 2 ? 0.0F : 4.0F}; })});
    routes.push_back({"backward", Planar([](int) { return V{0, 0, -3}; })});
    routes.push_back({"strafe", Planar([](int) { return V{3, 0, 0}; })});
    auto standing = Planar([](int) { return V{}; });
    Jumps(standing, 90, 36);
    routes.push_back({"standing-jump", std::move(standing)});
    auto running = Planar([](int) { return V{0, 0, 5}; });
    Jumps(running, 75, 30);
    routes.push_back({"running-jump", std::move(running)});
    auto ledge = Planar([](int) { return V{0, 0, 2}; });
    Jumps(ledge, 120, 24, false);
    routes.push_back({"ledge-drop", std::move(ledge)});
    auto crouchWalk = Planar([](int) { return V{0, 0, 1.5F}; });
    for (auto& sample : crouchWalk) sample.crouched = true;
    routes.push_back({"crouch-walk", std::move(crouchWalk)});
    auto crouchStop = Planar([](int t) { return V{0, 0, t < 200 ? 1.5F : 0.0F}; });
    for (auto& sample : crouchStop) sample.crouched = true;
    routes.push_back({"crouch-stop", std::move(crouchStop)});
    return routes;
}

JumpPhase FromPresentation(PlayerJumpPhase phase) {
    switch (phase) {
    case PlayerJumpPhase::Grounded: return JumpPhase::Grounded;
    case PlayerJumpPhase::Start: return JumpPhase::Start;
    case PlayerJumpPhase::Airborne: return JumpPhase::Airborne;
    case PlayerJumpPhase::Land: return JumpPhase::Land;
    }
    return JumpPhase::Unspecified;
}

// E1's jump judgement (v8-product-anim-authority-e1-jump): the phase and its
// zero point agree, except on the one tick where the Client's double
// comparison now - zero >= span missed a span of exactly whole ticks; there
// the Match is already in the next phase, from zero + span, and the Client
// must catch up on the next tick (which is judged again as Same).
enum class JumpAgreement { Same, ClientFloatLag, Differs };

JumpAgreement JudgeJump(JumpPhase match, std::uint64_t matchZero, JumpPhase client, double clientZeroSeconds,
                        std::uint64_t tick, const PlayerAnimationRules& rules, double startSeconds,
                        double landSeconds) {
    const auto clientZero = static_cast<std::uint64_t>(std::llround(clientZeroSeconds * AuthorityTickRate));
    if (match == client && matchZero == clientZero) return JumpAgreement::Same;
    const double now = static_cast<double>(tick) / AuthorityTickRate;
    const auto lag = [&](JumpPhase from, JumpPhase to, std::uint32_t ticks, double seconds) {
        return client == from && match == to && tick == clientZero + ticks && matchZero == clientZero + ticks &&
               !(now - clientZeroSeconds >= seconds);
    };
    if (lag(JumpPhase::Start, JumpPhase::Airborne, rules.jumpStartTicks, startSeconds) ||
        lag(JumpPhase::Land, JumpPhase::Grounded, rules.jumpLandTicks, landSeconds))
        return JumpAgreement::ClientFloatLag;
    return JumpAgreement::Differs;
}

// The largest error over its bound seen on any route (<= 1 passes).
struct Worst final {
    double phase{}, move{}, jog{};
    std::vector<std::uint64_t> lags; // ticks judged ClientFloatLag, over all routes
};

void Compare(const Route& route, const PlayerPresentationDefinition& definition, Worst& worst) {
    const std::string name = route.name;
    CAPTURE(name);
    const auto& rules = definition.animationRules;
    PresentationLegacyLocomotion::LocomotionState old;
    std::optional<PlayerAnimation> value;
    std::optional<PlayerState> before;
    double cycles = 0; // the integer phase, unwrapped by its signed differences
    std::uint64_t sinceReset = 0;
    for (std::size_t index = 0; index < route.samples.size(); ++index) {
        const auto& sample = route.samples[index];
        const auto tick = static_cast<std::uint64_t>(index + 1);
        PlayerState now;
        now.playerId = 1;
        now.position = sample.position;
        now.yaw = sample.yaw;
        now.grounded = sample.grounded;
        now.verticalVelocity = sample.verticalVelocity;
        now.crouched = sample.crouched;
        now.movementEpoch = sample.movementEpoch;
        double planarSpeed = 0;
        if (before) {
            const double dx = static_cast<double>(now.position.x) - before->position.x;
            const double dz = static_cast<double>(now.position.z) - before->position.z;
            planarSpeed = std::sqrt(dx * dx + dz * dz) * AuthorityTickRate;
        }
        PresentationLegacyLocomotion::Frame frame;
        frame.playerId = 1;
        frame.movementEpoch = now.movementEpoch;
        frame.position = now.position;
        frame.yaw = now.yaw;
        frame.presentationSeconds = static_cast<double>(tick) / AuthorityTickRate;
        frame.deltaSeconds = 1.0 / AuthorityTickRate;
        frame.planarSpeed = planarSpeed;
        frame.grounded = now.grounded;
        frame.verticalVelocity = now.verticalVelocity;
        frame.crouched = now.crouched;
        std::string error;
        REQUIRE_MESSAGE(PresentationLegacyLocomotion::AdvanceLocomotion(old, frame, definition, error), error);
        const auto previousPhase = value ? std::optional{value->gaitPhaseQ16} : std::nullopt;
        value = AdvancePlayerAnimation(rules, value, before ? &*before : nullptr, now, tick);
        before = now;
        CAPTURE(tick);
        if (old.phaseReset) {
            CHECK(value->gaitPhaseQ16 == 0);
            CHECK(value->moveWeightPermille == 0);
            CHECK(value->jumpPhaseTick == tick);
            cycles = 0;
            sinceReset = 0;
        } else {
            ++sinceReset;
            cycles += static_cast<std::int32_t>(value->gaitPhaseQ16 - *previousPhase) / 65536.0;
        }
        const double phaseBound = static_cast<double>(sinceReset) / 131072 + 0.000001;
        const double phaseError = std::abs(cycles - old.unwrappedPhaseCycles);
        const double moveError = std::abs(value->moveWeightPermille / 1000.0 - old.moveWeight);
        const double jogError = std::abs(value->jogWeightPermille / 1000.0 - old.jogWeight);
        worst.phase = std::max(worst.phase, phaseError / phaseBound);
        worst.move = std::max(worst.move, moveError / 0.001001);
        worst.jog = std::max(worst.jog, jogError / 0.001000001);
        CHECK(phaseError <= phaseBound);
        CHECK(moveError <= 0.001 + 0.000001);
        CHECK(jogError <= 0.001 + 0.000000001);
        const auto jump = JudgeJump(value->jumpPhase, value->jumpPhaseTick, FromPresentation(old.jumpPhase),
            old.jumpPhaseSeconds, tick, rules, definition.jumpStartSeconds, definition.jumpLandSeconds);
        CHECK(jump != JumpAgreement::Differs);
        if (jump == JumpAgreement::ClientFloatLag) worst.lags.push_back(tick);
    }
}
} // namespace

TEST_CASE("PvP E1 jump judgement admits only the Client's one-tick float lag at a whole-tick span") {
    PlayerAnimationRules rules;
    rules.jumpStartTicks = rules.jumpLandTicks = 6;
    const auto judge = [&](JumpPhase match, std::uint64_t matchZero, JumpPhase client, double clientZeroSeconds,
                           std::uint64_t tick) {
        return JudgeJump(match, matchZero, client, clientZeroSeconds, tick, rules, 0.1, 0.1);
    };
    const double s271 = 271.0 / 60; // 277/60 - 271/60 = 0.09999999999999964 (trial 1)
    REQUIRE_FALSE(277.0 / 60 - s271 >= 0.1);
    const double s60 = 60.0 / 60; // 66/60 - 60/60 = 0.10000000000000009: no lag possible
    REQUIRE(66.0 / 60 - s60 >= 0.1);
    CHECK(judge(JumpPhase::Start, 271, JumpPhase::Start, s271, 275) == JumpAgreement::Same);
    CHECK(judge(JumpPhase::Airborne, 277, JumpPhase::Start, s271, 277) == JumpAgreement::ClientFloatLag);
    CHECK(judge(JumpPhase::Grounded, 277, JumpPhase::Land, s271, 277) == JumpAgreement::ClientFloatLag);
    // Not the float lag: the comparison held, so the Client should have switched.
    CHECK(judge(JumpPhase::Airborne, 66, JumpPhase::Start, s60, 66) == JumpAgreement::Differs);
    // One tick later, a wrong zero point, a different pair or the Match behind.
    CHECK(judge(JumpPhase::Airborne, 277, JumpPhase::Start, s271, 278) == JumpAgreement::Differs);
    // The Match a tick early: inside the span the float comparison also fails.
    CHECK(judge(JumpPhase::Airborne, 277, JumpPhase::Start, s271, 276) == JumpAgreement::Differs);
    CHECK(judge(JumpPhase::Airborne, 276, JumpPhase::Start, s271, 277) == JumpAgreement::Differs);
    CHECK(judge(JumpPhase::Airborne, 278, JumpPhase::Start, s271, 277) == JumpAgreement::Differs);
    CHECK(judge(JumpPhase::Grounded, 277, JumpPhase::Start, s271, 277) == JumpAgreement::Differs);
    CHECK(judge(JumpPhase::Airborne, 277, JumpPhase::Land, s271, 277) == JumpAgreement::Differs);
    CHECK(judge(JumpPhase::Start, 271, JumpPhase::Airborne, s271 + 0.1, 277) == JumpAgreement::Differs);
    CHECK(judge(JumpPhase::Land, 271, JumpPhase::Start, s271, 273) == JumpAgreement::Differs);
}

TEST_CASE("PvP Match animation rules load without textures and equal the Client's") {
    ContentAssets match{false}; // no texture decoder: a texture read would fail the load
    // The player character binds a texture here, as the hair accessory does, so
    // the full presentation load would decode it; the Match's path must not.
    const auto slot = ClientDefinition().character->model->materials.at(0).name;
    match.Override("object_fps_pvp.player.character", [&](auto& json) {
        json["material_overrides"][slot] = {{"texture_asset_id", "object_fps_pvp.texture.character.female.hair"}};
    });
    std::string error;
    {
        ContentAssets check{false};
        check.Override("object_fps_pvp.player.character", [&](auto& json) {
            json["material_overrides"][slot] = {{"texture_asset_id", "object_fps_pvp.texture.character.female.hair"}};
        });
        REQUIRE_FALSE(fps::LoadCharacterPresentationDefinition(check.assets,
            Engine::Asset::AssetId::FromString("object_fps_pvp.player.character"), error)); // the full path does read it
    }
    const auto rules = LoadPlayerAnimationRules(match.assets, error);
    REQUIRE_MESSAGE(rules, error);
    CHECK(*rules == ClientDefinition().animationRules);
    CHECK(rules->transitionTicks == 6);
    CHECK(rules->jumpStartTicks == 6);
    CHECK(rules->jumpLandTicks == 6);
    // The same cycle lengths the Client's speed blend uses.
    CHECK(rules->walkCycleDistance == PlayerCycleDistance(ClientDefinition(), 0.0));
    CHECK(rules->jogCycleDistance == PlayerCycleDistance(ClientDefinition(), 1.0));
}

// E1 (v8-product-anim-authority-declaration.md): bounds from the quantization
// alone, declared before the comparison was run.
TEST_CASE("PvP Match animation equals the Client locomotion it replaces within its quantization") {
    Worst worst;
    const auto routes = Routes();
    REQUIRE(routes.size() == 12);
    for (const auto& route : routes) Compare(route, ClientDefinition(), worst);
    MESSAGE("E1 worst error / bound: phase " << worst.phase << ", move " << worst.move << ", jog " << worst.jog
            << "; client float lag ticks " << worst.lags.size());
    // Declared before the rerun: exactly trial 1's 19 ticks (457 on two routes),
    // evidence/anim-authority-e1-run1.txt.
    std::sort(worst.lags.begin(), worst.lags.end());
    CHECK(worst.lags == std::vector<std::uint64_t>{37, 43, 262, 271, 277, 307, 337, 367, 382, 391, 412, 457, 457,
                                                   487, 493, 532, 547, 562, 583});
}

// v8-product-hitbox-data D2 (H4): the Match's body pose (no texture, no
// weapon, no accessory) is bit for bit the Client's presented body for the
// same authority inputs: the judged pose is the presented pose.
namespace {
const PlayerPresentationDefinition& MatchDefinition() {
    static ContentAssets fixture{false};
    static const auto definition = [&] {
        std::string error;
        auto loaded = LoadPlayerPoseDefinition(fixture.assets, 1.8F, error);
        if (!loaded) throw std::runtime_error(error);
        return loaded;
    }();
    return *definition;
}

std::size_t BodyDifferences(const PlayerLocomotionState& state, const PlayerPresentationFrame& frame) {
    std::string error;
    PlayerActionPose actions;
    REQUIRE_MESSAGE(ResolvePlayerActions(ClientDefinition(), state, frame, actions, error), error);
    PlayerPresentationPose client;
    REQUIRE_MESSAGE(SamplePlayerPresentationPose(ClientDefinition(), state, actions, client, error), error);
    Engine::Model::Pose match;
    REQUIRE_MESSAGE(SamplePlayerBodyPose(MatchDefinition(), state, actions, match, error), error);
    REQUIRE(match.globalTransforms.size() == client.body.globalTransforms.size());
    std::size_t differences = 0;
    for (std::size_t node = 0; node < match.globalTransforms.size(); ++node)
        for (std::size_t i = 0; i < match.globalTransforms[node].values.size(); ++i)
            differences += std::bit_cast<std::uint32_t>(match.globalTransforms[node].values[i]) !=
                           std::bit_cast<std::uint32_t>(client.body.globalTransforms[node].values[i]);
    return differences;
}
} // namespace

TEST_CASE("PvP Match body pose equals the presented body bit for bit") {
    REQUIRE_FALSE(MatchDefinition().weapon); // the Match loads no weapon
    // The body's world placement is the same too: shapes go to the world by
    // (model - anchor) x scale (hitbox-shapes contract).
    CHECK(std::bit_cast<std::uint32_t>(MatchDefinition().scale) == std::bit_cast<std::uint32_t>(ClientDefinition().scale));
    CHECK(std::bit_cast<std::uint32_t>(MatchDefinition().anchor.y) == std::bit_cast<std::uint32_t>(ClientDefinition().anchor.y));
    CHECK(std::bit_cast<std::uint32_t>(MatchDefinition().anchor.x) == std::bit_cast<std::uint32_t>(ClientDefinition().anchor.x));
    CHECK(std::bit_cast<std::uint32_t>(MatchDefinition().anchor.z) == std::bit_cast<std::uint32_t>(ClientDefinition().anchor.z));
    std::size_t poses = 0;
    for (const auto& route : Routes()) {
        const std::string name = route.name;
        CAPTURE(name);
        const auto& rules = MatchDefinition().animationRules;
        std::optional<PlayerAnimation> value;
        std::optional<PlayerState> before;
        PlayerLocomotionState state;
        for (std::size_t index = 0; index < route.samples.size(); ++index) {
            const auto& sample = route.samples[index];
            const auto tick = static_cast<std::uint64_t>(index + 1);
            PlayerState now;
            now.playerId = 1;
            now.position = sample.position;
            now.yaw = sample.yaw;
            now.grounded = sample.grounded;
            now.verticalVelocity = sample.verticalVelocity;
            now.crouched = sample.crouched;
            value = AdvancePlayerAnimation(rules, value, before ? &*before : nullptr, now, tick);
            before = now;
            PlayerPresentationFrame frame;
            frame.playerId = 1;
            frame.movementEpoch = 1;
            frame.position = now.position;
            frame.yaw = now.yaw;
            frame.presentationSeconds = static_cast<double>(tick) / AuthorityTickRate;
            frame.deltaSeconds = 1.0 / AuthorityTickRate;
            frame.grounded = now.grounded;
            frame.crouched = now.crouched;
            frame.animation = InterpolatePlayerAnimation(*value, *value, 0);
            std::string error;
            REQUIRE_MESSAGE(AdvancePlayerLocomotion(state, frame, ClientDefinition(), error), error);
            if (tick % 10 != 0) continue;
            CAPTURE(tick);
            CHECK(BodyDifferences(state, frame) == 0);
            ++poses;
            // The actions and pitches the judged pose includes (pv9-upper-body-in-hitbox (a)).
            for (const float pitch : {-1.0F, 0.0F, 1.0F}) {
                auto pitched = frame;
                pitched.pitch = pitch;
                CHECK(BodyDifferences(state, pitched) == 0);
                ++poses;
            }
            auto shot = frame;
            shot.shotActionId = 1;
            shot.shotSeconds = frame.presentationSeconds - 0.05;
            CHECK(BodyDifferences(state, shot) == 0);
            auto reload = frame;
            reload.reloadActionId = 2;
            reload.reloadStartSeconds = frame.presentationSeconds - 0.3;
            reload.reloadEndSeconds = frame.presentationSeconds + 1.2;
            CHECK(BodyDifferences(state, reload) == 0);
            auto hit = frame;
            hit.damageCount = 1;
            hit.damageSeconds = frame.presentationSeconds - 0.1;
            CHECK(BodyDifferences(state, hit) == 0);
            auto dead = frame;
            dead.dead = true;
            dead.lifeStateSeconds = frame.presentationSeconds - 0.4;
            CHECK(BodyDifferences(state, dead) == 0);
            poses += 4;
        }
    }
    MESSAGE("H4 compared poses: " << poses);
    CHECK(poses == 12 * 60 * 8);
}
