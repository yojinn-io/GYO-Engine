#include <doctest/doctest.h>

#include "RetroFPS/Gameplay/Enemy/EnemyRig.hpp"

#include <cmath>
#include <memory>
#include <stdexcept>

namespace {
using namespace Engine::Model;
using namespace fps;

EnemyRig AttachmentRig() {
    auto character = std::make_shared<ModelAsset>();
    character->nodes = {
        {"root", std::nullopt, {{0.2F, 0.1F, -0.3F}, {0, std::sin(0.15F), 0, std::cos(0.15F)}}},
        {"hand", 0, {{0.3F, 1.2F, 0.4F}, {std::sin(0.2F), 0, 0, std::cos(0.2F)}}}};
    NodeTrack motion;
    motion.nodeIndex = 1;
    motion.translations = {{0, {0.3F, 1.2F, 0.4F}}, {1, {0.5F, 1.5F, 0.7F}}};
    character->clips = {{"aim", 1, {motion}}};
    REQUIRE(ValidateModel(*character));

    auto weapon = std::make_shared<ModelAsset>();
    weapon->nodes = {
        {"import_root", std::nullopt,
         {{0.1F, -0.05F, 0.2F}, {0, 0, std::sin(0.1F), std::cos(0.1F)}, {1.1F, 1.1F, 1.1F}}},
        {"barrel", 0, {{0, 0.03F, 0.1F}}}};
    weapon->materials = {{"metal"}};
    MeshPart rigid;
    rigid.nodeIndex = 1;
    rigid.geometryToNode = ToMatrix({{0.02F, 0, 0.03F}, {std::sin(0.05F), 0, 0, std::cos(0.05F)}});
    rigid.vertices = {{{0, 0.02F, 0.3F}}, {{-0.02F, 0, 0}}, {{0.02F, 0, 0}}};
    rigid.indices = {0, 1, 2};
    auto weighted = rigid;
    weighted.joints = {{1, rigid.geometryToNode}};
    for (auto& vertex : weighted.vertices)
        vertex.weights[0] = 1;
    weapon->meshes = {rigid, weighted};
    REQUIRE(ValidateModel(*weapon));

    Pose rest;
    REQUIRE(MakeDefaultPose(*weapon, rest));
    std::vector<SkinnedVertex> vertices;
    REQUIRE(SkinMesh(*weapon, 0, rest, vertices));
    EnemyRig rig;
    rig.model = character;
    rig.anchor = {0.1F, -0.2F, 0.05F};
    rig.scale = 0.8F;
    EnemyWeaponAttachment attachment;
    attachment.model = weapon;
    attachment.node = 1;
    attachment.localTransform = {{-0.03F, 0.04F, 0.06F},
        {0, std::sin(-0.35F), 0, std::cos(-0.35F)}, {0.7F, 0.7F, 0.7F}};
    // Authoring uses the imported model-space vertex, not its raw mesh coordinate.
    attachment.muzzlePosition = vertices.front().position;
    rig.attackPoint = {attachment.node,
        TransformPoint(ToMatrix(attachment.localTransform), attachment.muzzlePosition)};
    rig.weapon = attachment;
    return rig;
}

void CheckPoseUnchanged(const Pose& actual, const Pose& before) {
    REQUIRE(actual.globalTransforms.size() == before.globalTransforms.size());
    REQUIRE(actual.localTransforms.size() == before.localTransforms.size());
    for (std::size_t node = 0; node < before.globalTransforms.size(); ++node) {
        CHECK(actual.globalTransforms[node].values == before.globalTransforms[node].values);
        CHECK(ToMatrix(actual.localTransforms[node]).values == ToMatrix(before.localTransforms[node]).values);
    }
}
} // namespace

TEST_CASE("v2 weapon muzzle matches rendered vertices through imported and actor transforms") {
    const auto rig = AttachmentRig();
    const auto& weapon = *rig.weapon->model;
    const auto weaponBefore = weapon;
    const auto characterBefore = *rig.model;
    const Float2 position{4.0F, 7.0F};
    const float yaw = 0.9F;
    const auto world = ToMatrix({{position.x, 0, position.z},
        {0, std::sin(yaw * 0.5F), 0, std::cos(yaw * 0.5F)},
        {rig.scale, rig.scale, rig.scale}});

    Pose firstWeaponPose;
    std::vector<Matrix4> firstGlobals;
    for (const double time : {0.15, 0.7}) {
        Pose characterPose;
        REQUIRE(SamplePose(*rig.model, 0, time, PlaybackMode::Clamp, characterPose));
        const auto before = characterPose;
        const auto weaponPose = BuildEnemyWeaponPose(rig, characterPose);
        const auto origin = EnemyBoneWorldPoint(rig, characterPose, rig.attackPoint, position, yaw);
        for (std::size_t mesh = 0; mesh < weapon.meshes.size(); ++mesh) {
            std::vector<SkinnedVertex> vertices;
            REQUIRE(SkinMesh(weapon, mesh, weaponPose, vertices));
            const auto& muzzle = vertices.front().position;
            const auto rendered = TransformPoint(world,
                {muzzle.x - rig.anchor.x, muzzle.y - rig.anchor.y, muzzle.z - rig.anchor.z});
            CHECK(origin.x == doctest::Approx(rendered.x).epsilon(1e-5));
            CHECK(origin.y == doctest::Approx(rendered.y).epsilon(1e-5));
            CHECK(origin.z == doctest::Approx(rendered.z).epsilon(1e-5));
        }
        CheckPoseUnchanged(characterPose, before);
        if (firstGlobals.empty()) {
            firstWeaponPose = weaponPose;
            firstGlobals = firstWeaponPose.globalTransforms;
        } else {
            CHECK(weaponPose.globalTransforms[1].values != firstGlobals[1].values);
            for (std::size_t node = 0; node < firstGlobals.size(); ++node)
                CHECK(firstWeaponPose.globalTransforms[node].values == firstGlobals[node].values);
        }
    }
    for (std::size_t node = 0; node < weapon.nodes.size(); ++node)
        CHECK(ToMatrix(weapon.nodes[node].localTransform).values ==
              ToMatrix(weaponBefore.nodes[node].localTransform).values);
    for (std::size_t node = 0; node < rig.model->nodes.size(); ++node)
        CHECK(ToMatrix(rig.model->nodes[node].localTransform).values ==
              ToMatrix(characterBefore.nodes[node].localTransform).values);
    for (std::size_t mesh = 0; mesh < weapon.meshes.size(); ++mesh) {
        const auto& actual = weapon.meshes[mesh];
        const auto& before = weaponBefore.meshes[mesh];
        CHECK(actual.geometryToNode.values == before.geometryToNode.values);
        CHECK(actual.indices == before.indices);
        for (std::size_t vertex = 0; vertex < actual.vertices.size(); ++vertex) {
            CHECK(actual.vertices[vertex].position.x == before.vertices[vertex].position.x);
            CHECK(actual.vertices[vertex].position.y == before.vertices[vertex].position.y);
            CHECK(actual.vertices[vertex].position.z == before.vertices[vertex].position.z);
            CHECK(actual.vertices[vertex].weights == before.vertices[vertex].weights);
        }
    }
}

TEST_CASE("v2 weapon attachment requires a model and a bone in the authoritative pose") {
    auto rig = AttachmentRig();
    Pose pose;
    REQUIRE(MakeDefaultPose(*rig.model, pose));
    const auto attachment = *rig.weapon;
    rig.weapon.reset();
    CHECK_THROWS_AS(static_cast<void>(BuildEnemyWeaponPose(rig, pose)), std::invalid_argument);
    rig.weapon = attachment;
    rig.weapon->model.reset();
    CHECK_THROWS_AS(static_cast<void>(BuildEnemyWeaponPose(rig, pose)), std::invalid_argument);
    rig.weapon = attachment;
    rig.weapon->node = pose.globalTransforms.size();
    CHECK_THROWS_AS(static_cast<void>(BuildEnemyWeaponPose(rig, pose)), std::invalid_argument);
}
