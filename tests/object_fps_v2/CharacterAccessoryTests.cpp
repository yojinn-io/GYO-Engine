#include <doctest/doctest.h>

#include "TestAssets.hpp"
#include "RetroFPS/App/CharacterPresentationDefinition.hpp"
#include "RetroFPS/Gameplay/Enemy/EnemyRig.hpp"

#include <cmath>
#include <memory>
#include <stdexcept>

namespace {
using namespace Engine::Model;
using namespace fps;

template<class A, class B>
void CheckPoint(const A& actual, const B& expected) {
    CHECK(actual.x == doctest::Approx(expected.x).epsilon(1e-5));
    CHECK(actual.y == doctest::Approx(expected.y).epsilon(1e-5));
    CHECK(actual.z == doctest::Approx(expected.z).epsilon(1e-5));
}

void CheckPoseUnchanged(const Pose& actual, const Pose& before) {
    REQUIRE(actual.globalTransforms.size() == before.globalTransforms.size());
    REQUIRE(actual.localTransforms.size() == before.localTransforms.size());
    for (std::size_t node = 0; node < before.globalTransforms.size(); ++node) {
        CHECK(actual.globalTransforms[node].values == before.globalTransforms[node].values);
        CHECK(ToMatrix(actual.localTransforms[node]).values == ToMatrix(before.localTransforms[node]).values);
    }
}

CharacterAccessoryDefinition SyntheticAccessory() {
    auto source = std::make_shared<ModelAsset>();
    source->nodes = {
        {"import_root", std::nullopt, {{0.2F, 0.3F, -0.1F}}},
        {"source_bone", 0, {{0, 1.4F, 0}, {0, std::sin(0.2F), 0, std::cos(0.2F)}}}};
    source->materials = {{"hair"}};
    MeshPart mesh;
    mesh.nodeIndex = 0;
    mesh.vertices = {{{-0.1F, 0, 0}}, {{0.1F, 0, 0}}, {{0, 0.15F, 0.03F}}};
    mesh.indices = {0, 1, 2};
    // The imported geometry is offset from its skin joint's local reference frame.
    mesh.joints = {{1, ToMatrix({{0.02F, 0.04F, -0.03F}})}};
    for (auto& vertex : mesh.vertices) vertex.weights[0] = 1;
    source->meshes = {mesh};
    REQUIRE(ValidateModel(*source));

    auto presentation = std::make_shared<CharacterPresentationDefinition>();
    presentation->model = source;
    CharacterAccessoryDefinition accessory;
    accessory.presentation = presentation;
    accessory.sourceNode = 1;
    accessory.targetNode = 0;
    accessory.placement = {{-0.03F, 0.06F, 0.01F},
        {std::sin(0.15F), 0, 0, std::cos(0.15F)}, {0.8F, 0.8F, 0.8F}};
    REQUIRE(MakeDefaultPose(*source, accessory.referencePose));
    return accessory;
}
} // namespace

TEST_CASE("production male and female hair follows head motion without changing combat geometry") {
    auto& application = fps::tests::ProductionApplication();
    const auto& enemies = application.Content()->Data().enemies.GetDefinitions();
    REQUIRE(enemies.size() == 2);
    for (const auto& enemy : enemies) {
        REQUIRE(enemy.rig);
        const auto& rig = *enemy.rig;
        std::string error;
        const auto character = LoadCharacterPresentationDefinition(application.Assets(), rig.characterAssetId, error);
        REQUIRE_MESSAGE(character, error);
        REQUIRE(character->accessories.size() == 1);
        const auto& accessory = character->accessories.front();
        REQUIRE(accessory.presentation);
        const auto& hair = *accessory.presentation;
        REQUIRE(hair.model);
        REQUIRE_FALSE(hair.model->meshes.empty());
        CHECK_FALSE(hair.animationSet);
        CHECK(hair.model->clips.empty());
        REQUIRE(accessory.targetNode < rig.model->nodes.size());
        REQUIRE(accessory.sourceNode < hair.model->nodes.size());
        CHECK(rig.model->nodes[accessory.targetNode].name == "Head");
        CHECK(hair.model->nodes[accessory.sourceNode].name == "Head");
        for (const auto& mesh : hair.model->meshes) {
            REQUIRE(mesh.materialIndex < hair.materials.size());
            CHECK(hair.materials[mesh.materialIndex].textureAssetId.has_value());
        }

        Pose characterPose;
        REQUIRE(SamplePose(*rig.model, rig.clips[2], 0.25, PlaybackMode::Clamp, characterPose));
        const auto authoritativeBefore = characterPose;
        const auto bindBefore = accessory.referencePose;
        const auto hurtboxesBefore = BuildEnemyHurtboxes(rig, characterPose, {3, 4}, 0.7F);
        const auto first = BuildCharacterAccessoryPose(accessory, characterPose);
        const auto firstBefore = first;
        const auto motion = ToMatrix({{0.4F, 0.2F, -0.3F},
            {0, std::sin(0.35F), 0, std::cos(0.35F)}});
        auto movedCharacter = characterPose;
        movedCharacter.globalTransforms[accessory.targetNode] =
            Multiply(motion, characterPose.globalTransforms[accessory.targetNode]);
        const auto second = BuildCharacterAccessoryPose(accessory, movedCharacter);
        bool visibleMotion = false;
        std::size_t vertexCount = 0;
        for (std::size_t mesh = 0; mesh < hair.model->meshes.size(); ++mesh) {
            std::vector<SkinnedVertex> before, after;
            REQUIRE(SkinMesh(*hair.model, mesh, first, before));
            REQUIRE(SkinMesh(*hair.model, mesh, second, after));
            REQUIRE(before.size() == after.size());
            vertexCount += before.size();
            for (std::size_t vertex = 0; vertex < before.size(); ++vertex) {
                CheckPoint(after[vertex].position, TransformPoint(motion, before[vertex].position));
                visibleMotion |= std::abs(after[vertex].position.x - before[vertex].position.x) +
                    std::abs(after[vertex].position.z - before[vertex].position.z) > 0.01F;
            }
        }
        CHECK(vertexCount > 0);
        CHECK(visibleMotion);
        CheckPoseUnchanged(characterPose, authoritativeBefore);
        CheckPoseUnchanged(accessory.referencePose, bindBefore);
        CheckPoseUnchanged(first, firstBefore);
        const auto hurtboxesAfter = BuildEnemyHurtboxes(rig, characterPose, {3, 4}, 0.7F);
        REQUIRE(hurtboxesAfter.size() == hurtboxesBefore.size());
        for (std::size_t region = 0; region < hurtboxesBefore.size(); ++region) {
            CHECK(hurtboxesAfter[region].region == hurtboxesBefore[region].region);
            CheckPoint(hurtboxesAfter[region].shape.segmentStart, hurtboxesBefore[region].shape.segmentStart);
            CheckPoint(hurtboxesAfter[region].shape.segmentEnd, hurtboxesBefore[region].shape.segmentEnd);
            CHECK(hurtboxesAfter[region].shape.radius == hurtboxesBefore[region].shape.radius);
        }
    }
}

TEST_CASE("character accessory placement acts in the selected target bone frame") {
    const auto accessory = SyntheticAccessory();
    const auto& model = *accessory.presentation->model;
    Pose character;
    character.localTransforms = {{{0.5F, 1.2F, -0.2F},
        {0, std::sin(-0.4F), 0, std::cos(-0.4F)}}};
    character.globalTransforms = {ToMatrix(character.localTransforms.front())};
    const auto pose = BuildCharacterAccessoryPose(accessory, character);
    std::vector<SkinnedVertex> vertices;
    REQUIRE(SkinMesh(model, 0, pose, vertices));
    REQUIRE(vertices.size() == 3);
    const auto& mesh = model.meshes.front();
    for (std::size_t vertex = 0; vertex < vertices.size(); ++vertex) {
        auto expected = TransformPoint(mesh.joints.front().geometryToJoint, mesh.vertices[vertex].position);
        expected = TransformPoint(ToMatrix(accessory.placement), expected);
        expected = TransformPoint(character.globalTransforms.front(), expected);
        CheckPoint(vertices[vertex].position, expected);
    }
}

TEST_CASE("character accessory requires resolved source and target bones") {
    const auto valid = SyntheticAccessory();
    Pose character;
    character.globalTransforms.resize(1);
    auto missingSource = valid;
    missingSource.sourceNode = valid.referencePose.globalTransforms.size();
    CHECK_THROWS_AS(static_cast<void>(BuildCharacterAccessoryPose(missingSource, character)), std::invalid_argument);
    auto missingTarget = valid;
    missingTarget.targetNode = character.globalTransforms.size();
    CHECK_THROWS_AS(static_cast<void>(BuildCharacterAccessoryPose(missingTarget, character)), std::invalid_argument);
    auto missingPresentation = valid;
    missingPresentation.presentation.reset();
    CHECK_THROWS_AS(static_cast<void>(BuildCharacterAccessoryPose(missingPresentation, character)), std::invalid_argument);
}
