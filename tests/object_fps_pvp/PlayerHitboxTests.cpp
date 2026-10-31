#include <doctest/doctest.h>

#include "RetroFPS/Pvp/PlayerHitbox.hpp"
#include "RetroFPS/Pvp/PlayerPresentation.hpp"
#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/AssetManager.hpp"
#include "engine/asset/ContentManifest.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/loaders/sdl_image/SdlImageTextureLoader.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "engine/model/backend/ufbx/UfbxModelLoader.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "gyo/AppConfig.hpp"
#include <nlohmann/json.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

// v8-product-hitbox-data D1: the hitbox Data Contract's strict reader (H1)
// and where the product shapes sit on the reference pose (H2).
namespace {
using namespace fps::pvp;

std::filesystem::path ContentRoot() {
    const auto base = Engine::Platform::Sdl::BaseDirectory();
    if (!base) throw std::runtime_error("Cannot locate the PvP test product directory");
    return base.value() / Gyo::AppConfig::Assets;
}

struct Assets final {
    Engine::Asset::AssetCatalog catalog{[] {
        const auto root = ContentRoot();
        const auto manifest = Engine::Asset::ContentManifest::Load(root);
        if (!manifest) throw std::runtime_error(manifest.error().message);
        auto catalog = manifest.value().LoadCatalogs(root);
        if (!catalog) throw std::runtime_error(catalog.error().message);
        return std::move(catalog).value();
    }()};
    Engine::Asset::Loading::LoaderRegistry loaders;
    Engine::Asset::Loading::NativeFileAssetSource source;
    Engine::Asset::Loading::AssetPipeline pipeline{source, loaders};
    Engine::Asset::Core::AssetStorage storage;
    Engine::Asset::Core::AssetLifetime lifetime;
    Engine::Asset::Core::AssetCachePolicy policy{{}};
    Engine::Asset::AssetManager assets{catalog, pipeline, storage, lifetime, policy};
    explicit Assets(bool textures) {
        loaders.Register(std::make_unique<Engine::Asset::Loaders::TextLoader>());
        if (textures) loaders.Register(std::make_unique<Engine::Asset::Loaders::SdlImage::SdlImageTextureLoader>());
        loaders.Register(std::make_unique<Engine::Model::Ufbx::UfbxModelLoader>());
    }
};

const PlayerPresentationDefinition& Definition() {
    static Assets fixture{true};
    static const auto definition = [&] {
        std::string error;
        auto loaded = LoadPlayerPresentationDefinition(fixture.assets, 1.8F, error);
        if (!loaded) throw std::runtime_error(error);
        return loaded;
    }();
    return *definition;
}

nlohmann::json ProductJson() {
    std::ifstream file(ContentRoot() / "characters" / "player" / "hitbox.json");
    REQUIRE(file);
    return nlohmann::json::parse(file);
}

bool Refused(const std::function<void(nlohmann::json&)>& edit) {
    auto json = ProductJson();
    edit(json);
    std::string error;
    const bool refused = !ReadPlayerHitbox(json.dump(), *Definition().character, error);
    CHECK_MESSAGE((!refused || !error.empty()), "a refusal names its reason");
    return refused;
}

// The refusal names the rule, not a later lookup that happened to fail.
std::string Reason(const std::function<void(nlohmann::json&)>& edit) {
    auto json = ProductJson();
    edit(json);
    std::string error;
    static_cast<void>(ReadPlayerHitbox(json.dump(), *Definition().character, error));
    return error;
}

Engine::Math::Vec3 Body(Engine::Math::Vec3 model) {
    const auto& d = Definition();
    return {(model.x - d.anchor.x) * d.scale, (model.y - d.anchor.y) * d.scale, (model.z - d.anchor.z) * d.scale};
}
} // namespace

TEST_CASE("PvP player hitbox loads the product shapes and the Match path reads no texture") {
    std::string error;
    const auto client = ReadPlayerHitbox(ProductJson().dump(), *Definition().character, error);
    REQUIRE_MESSAGE(client, error);
    REQUIRE(client->shapes.size() == 7);
    const auto& model = *Definition().character->model;
    const auto name = [&](std::size_t node) { return model.nodes.at(node).name; };
    CHECK(client->shapes[0].part == HitPart::Head);
    CHECK(client->shapes[0].kind == PlayerHitShape::Kind::Sphere);
    CHECK(name(client->shapes[0].node) == "Head");
    CHECK(client->shapes[0].radius == doctest::Approx(0.184));
    CHECK(name(client->shapes[1].node) == "spine_01");
    CHECK(name(client->shapes[1].nodeTo) == "spine_03");
    CHECK(name(client->shapes[2].node) == "pelvis");
    CHECK(name(client->shapes[2].nodeTo) == "spine_01");
    // Each leg segment on its own bones (H2 review: a crossed, missing,
    // duplicated or knee-skipping leg stays low and would pass H2 alone).
    const std::array<std::pair<const char*, const char*>, 4> legs{{
        {"thigh_l", "calf_l"}, {"calf_l", "foot_l"}, {"thigh_r", "calf_r"}, {"calf_r", "foot_r"}}};
    for (std::size_t i = 0; i < legs.size(); ++i) {
        const auto& shape = client->shapes[3 + i];
        CAPTURE(i);
        CHECK(shape.part == HitPart::Leg);
        CHECK(shape.kind == PlayerHitShape::Kind::Capsule);
        CHECK(name(shape.node) == legs[i].first);
        CHECK(name(shape.nodeTo) == legs[i].second);
    }
    Assets match{false}; // no texture decoder
    const auto loaded = LoadPlayerHitbox(match.assets, error);
    REQUIRE_MESSAGE(loaded, error);
    CHECK(*loaded == *client);
}

TEST_CASE("PvP player hitbox reader refuses every malformed contract") {
    CHECK_FALSE(Refused([](auto&) {}));
    CHECK(Refused([](auto& j) { j["version"] = 2; }));
    CHECK(Refused([](auto& j) { j["extra"] = 1; }));
    CHECK(Refused([](auto& j) { j.erase("shapes"); }));
    CHECK(Refused([](auto& j) { j["shapes"][0]["extra"] = 1; }));
    CHECK(Refused([](auto& j) { j["shapes"][1].erase("to"); }));
    CHECK(Refused([](auto& j) {  // no head part
        j["shapes"].erase(0);
    }));
    CHECK(Refused([](auto& j) { j["shapes"][0]["part"] = "arm"; }));
    CHECK(Refused([](auto& j) { j["shapes"][0]["kind"] = "box"; }));
    CHECK(Refused([](auto& j) { j["shapes"][1]["from"] = "no_such_bone"; }));
    CHECK(Refused([](auto& j) { j["shapes"][1]["to"] = "spine_01"; })); // from == to
    CHECK(Refused([](auto& j) { j["shapes"][1]["radius"] = 0; }));
    CHECK(Refused([](auto& j) { j["shapes"][1]["radius"] = 0.6; }));
    CHECK(Refused([](auto& j) { j["shapes"][1]["radius"] = "NaN"; }));
    CHECK(Refused([](auto& j) { j["shapes"][0]["offset"] = {0, 1}; }));
    CHECK(Refused([](auto& j) { j["shapes"] = nlohmann::json::object(); }));
    CHECK(Reason([](auto& j) { j.erase("shapes"); }).find("lacks 'shapes'") != std::string::npos);
    CHECK(Reason([](auto& j) { j["shapes"][1].erase("to"); }).find("lacks 'to'") != std::string::npos);
    CHECK(Reason([](auto& j) { j["shapes"][0]["offset"] = {0, 1}; }).find("three numbers") != std::string::npos);
}

TEST_CASE("PvP player hitbox shapes sit on their bones in the standing reference pose") {
    const auto& definition = Definition();
    const auto& model = *definition.character->model;
    std::string error;
    const auto hitbox = ReadPlayerHitbox(ProductJson().dump(), *definition.character, error);
    REQUIRE(hitbox);
    PlayerLocomotionState state;
    PlayerPresentationPose pose;
    REQUIRE_MESSAGE(SamplePlayerPresentationPose(definition, state, pose, error), error);
    const auto joint = [&](std::size_t node) {
        return Body(Engine::Math::TransformPoint(pose.body.globalTransforms.at(node), Engine::Math::Vec3{}));
    };
    const auto node = [&](const char* name) { return *model.FindNode(name); };
    const float pelvis = joint(node("pelvis")).y;
    float pelvisRadius = 0;
    for (const auto& shape : hitbox->shapes)
        if (shape.kind == PlayerHitShape::Kind::Capsule && model.nodes.at(shape.node).name == "pelvis") pelvisRadius = shape.radius;
    REQUIRE(pelvisRadius > 0);
    for (const auto& shape : hitbox->shapes) {
        CAPTURE(model.nodes.at(shape.node).name);
        if (shape.kind == PlayerHitShape::Kind::Sphere) {
            const auto centre = Body(Engine::Math::TransformPoint(pose.body.globalTransforms.at(shape.node), shape.offset));
            CHECK(centre.y + shape.radius <= 1.8F + 1e-4F);
            CHECK(centre.y > joint(node("neck_01")).y); // the head sits above the neck
            CHECK(centre.y == doctest::Approx(1.554).epsilon(0.002)); // the survey's head centre
        } else {
            const auto a = joint(shape.node), b = joint(shape.nodeTo);
            CHECK(std::max(a.y, b.y) + shape.radius <= 1.8F + 1e-4F);
            // v8-product-hitbox-data-h2-legs (a): a leg hangs from the pelvis. A
            // thigh starts at the hip, within the pelvis capsule's radius of the
            // pelvis joint; the knee and the foot are below the pelvis joint.
            if (shape.part == HitPart::Leg) {
                const bool thigh = model.nodes.at(shape.node).name.rfind("thigh_", 0) == 0;
                if (thigh) CHECK(Engine::Math::Length(a - joint(node("pelvis"))) <= pelvisRadius);
                else CHECK(a.y < pelvis);
                CHECK(b.y < pelvis);
            }
        }
    }
}
