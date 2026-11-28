#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "engine/model/Animation.hpp"
#include "engine/model/ModelData.hpp"
#include "engine/model/Retarget.hpp"

#include "AssertTestSupport.hpp"

// The model data rules (model.zh-Hant.md section 2.2), skeletal animation's
// first layer (section 4: RestPose, SampleClip, Blend, ApplyAdditive,
// ComputeModelSpace, CrossedEvents; section 4.7: SolveTwoBoneIK) and
// retargeting (section 4.4: BindByName, Retarget), on node lists and clips
// written here, so each case shows the values it expects.

using namespace GYO::Engine;
using Math::Quaternion;
using Math::Vec3;
using Model::AnimationClip;
using Model::JointBinding;
using Model::Keys;
using Model::LocalTransform;
using Model::ModelData;
using Model::ModelDataCode;
using Model::Node;
using Model::RetargetCode;
using Model::Track;
using Model::Wrap;

namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kEpsilon = 1.0e-5F;

Quaternion AboutY(const float degrees) { return Math::MakeQuaternionFromAxisAngle({0, 1, 0}, degrees * kPi / 180.0F); }
Quaternion AboutX(const float degrees) { return Math::MakeQuaternionFromAxisAngle({1, 0, 0}, degrees * kPi / 180.0F); }
Quaternion AboutZ(const float degrees) { return Math::MakeQuaternionFromAxisAngle({0, 0, 1}, degrees * kPi / 180.0F); }

bool Near(const float a, const float b, const float epsilon = kEpsilon) { return std::fabs(a - b) <= epsilon; }
bool Near(const Vec3 a, const Vec3 b, const float epsilon = kEpsilon) {
    return Near(a.x, b.x, epsilon) && Near(a.y, b.y, epsilon) && Near(a.z, b.z, epsilon);
}
// The same rotation (q and -q are one rotation).
bool SameRotation(const Quaternion a, const Quaternion b, const float epsilon = kEpsilon) {
    return Near(std::fabs(Math::Dot(Math::Normalize(a), Math::Normalize(b))), 1.0F, epsilon);
}

bool SameBits(const float a, const float b) { return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b); }
bool SameBits(const LocalTransform& a, const LocalTransform& b) {
    return SameBits(a.translation.x, b.translation.x) && SameBits(a.translation.y, b.translation.y) &&
           SameBits(a.translation.z, b.translation.z) && SameBits(a.rotation.x, b.rotation.x) &&
           SameBits(a.rotation.y, b.rotation.y) && SameBits(a.rotation.z, b.rotation.z) &&
           SameBits(a.rotation.w, b.rotation.w) && SameBits(a.scale.x, b.scale.x) && SameBits(a.scale.y, b.scale.y) &&
           SameBits(a.scale.z, b.scale.z);
}

std::string Show(const Vec3 v) {
    return "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
}
std::string Show(const Quaternion q) {
    return "(" + std::to_string(q.x) + ", " + std::to_string(q.y) + ", " + std::to_string(q.z) + ", " +
           std::to_string(q.w) + ")";
}

Node MakeNode(std::string name, const std::uint32_t parent, const LocalTransform rest = {}) {
    return Node{std::move(name), parent, rest};
}

// root (0) -> arm (1) -> hand (2); arm rests 1 above root, hand 1 above arm.
std::vector<Node> Chain() {
    return {MakeNode("root", Model::kNoParent), MakeNode("arm", 0, {{0, 1, 0}, {}, {1, 1, 1}}),
            MakeNode("hand", 1, {{0, 1, 0}, {}, {1, 1, 1}})};
}

// Duration 2. arm: translation keys at 0, 1, 2 on y = 1, 3, 5; rotation keys
// at 0 and 2 from identity to 90 degrees about Y. hand: no track. root: a
// scale-only track would be on 0; there is none.
AnimationClip ArmClip() {
    AnimationClip clip;
    clip.name = "wave";
    clip.duration = 2.0F;
    clip.nodeCount = 3;
    Track arm;
    arm.node = 1;
    arm.translation = Keys<Vec3>{{0.0F, 1.0F, 2.0F}, {{0, 1, 0}, {0, 3, 0}, {0, 5, 0}}};
    arm.rotation = Keys<Quaternion>{{0.0F, 2.0F}, {Quaternion{}, AboutY(90)}};
    clip.tracks.push_back(arm);
    return clip;
}

std::vector<LocalTransform> Sample(const std::vector<Node>& nodes, const AnimationClip& clip, const float seconds,
                                   const Wrap wrap) {
    std::vector<LocalTransform> out(nodes.size());
    Model::SampleClip(nodes, clip, seconds, wrap, out);
    return out;
}

ModelData ValidModel() {
    ModelData model;
    model.nodes = Chain();
    model.materials.push_back(Model::Material{"skin", {1, 1, 1, 1}, "", {}});
    Model::Mesh mesh;
    mesh.name = "body";
    mesh.node = 0;
    mesh.material = 0;
    mesh.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    mesh.normals = {{0, 0, -1}, {0, 0, -1}, {0, 0, -1}};
    mesh.uvs = {{0, 0}, {1, 0}, {0, 1}};
    mesh.indices = {0, 1, 2};
    model.skins.push_back(Model::Skin{{1, 2}, {Math::Matrix4{}, Math::Matrix4{}}});
    mesh.skin = 0;
    mesh.vertexJoints = {{0, 0, 0, 0}, {0, 1, 0, 0}, {1, 0, 0, 0}};
    mesh.vertexWeights = {{1, 0, 0, 0}, {0.75F, 0.25F, 0, 0}, {0, 0, 0, 0}};
    model.meshes.push_back(mesh);
    model.clips.push_back(ArmClip());
    return model;
}

ModelDataCode CodeOf(const ModelData& model) {
    const auto result = Model::Validate(model);
    REQUIRE_FALSE(result.has_value());
    return result.error().code;
}

} // namespace

TEST_CASE("Validate accepts a model that keeps every rule") {
    const auto result = Model::Validate(ValidModel());
    CHECK(result.has_value());
    ModelData empty;
    CHECK(Model::Validate(empty).has_value());
}

TEST_CASE("Validate rule 1: BadNode") {
    SUBCASE("a parent after its child") {
        ModelData model = ValidModel();
        model.nodes[1].parent = 2;
        CHECK(CodeOf(model) == ModelDataCode::BadNode);
    }
    SUBCASE("a node its own parent") {
        ModelData model = ValidModel();
        model.nodes[1].parent = 1;
        CHECK(CodeOf(model) == ModelDataCode::BadNode);
    }
    SUBCASE("a rest value not finite") {
        ModelData model = ValidModel();
        model.nodes[2].rest.rotation.w = std::numeric_limits<float>::quiet_NaN();
        CHECK(CodeOf(model) == ModelDataCode::BadNode);
    }
}

TEST_CASE("Validate rule 2: BadMesh") {
    ModelData model = ValidModel();
    SUBCASE("node out of range") { model.meshes[0].node = 3; }
    SUBCASE("material out of range") { model.meshes[0].material = 1; }
    SUBCASE("normals not one per position") { model.meshes[0].normals.pop_back(); }
    SUBCASE("uvs neither none nor one per position") { model.meshes[0].uvs.pop_back(); }
    SUBCASE("indices not three per triangle") { model.meshes[0].indices.push_back(0); }
    SUBCASE("an index out of range") { model.meshes[0].indices[2] = 3; }
    SUBCASE("a position not finite") { model.meshes[0].positions[1].x = std::numeric_limits<float>::infinity(); }
    SUBCASE("a uv not finite") { model.meshes[0].uvs[1].y = std::numeric_limits<float>::quiet_NaN(); }
    CHECK(CodeOf(model) == ModelDataCode::BadMesh);
}

TEST_CASE("Validate: a mesh with no material and no uvs is valid") {
    ModelData model = ValidModel();
    model.meshes[0].material = Model::kNoMaterial;
    model.meshes[0].uvs.clear();
    CHECK(Model::Validate(model).has_value());
}

TEST_CASE("Validate rule 3: BadSkin") {
    ModelData model = ValidModel();
    SUBCASE("a joint out of range") { model.skins[0].joints[1] = 3; }
    SUBCASE("inverse bind matrices not one per joint") { model.skins[0].inverseBindMatrices.pop_back(); }
    SUBCASE("an inverse bind matrix not finite") {
        model.skins[0].inverseBindMatrices[0].values[5] = std::numeric_limits<float>::quiet_NaN();
    }
    SUBCASE("joints not one per vertex") { model.meshes[0].vertexJoints.pop_back(); }
    SUBCASE("weights not one per vertex") { model.meshes[0].vertexWeights.pop_back(); }
    SUBCASE("a vertex names a joint the skin does not have") { model.meshes[0].vertexJoints[1][1] = 2; }
    SUBCASE("a negative weight") { model.meshes[0].vertexWeights[1] = {1.25F, -0.25F, 0, 0}; }
    SUBCASE("a weight not finite") {
        model.meshes[0].vertexWeights[0][0] = std::numeric_limits<float>::quiet_NaN();
    }
    SUBCASE("weights summing to neither 1 nor 0") { model.meshes[0].vertexWeights[1] = {0.5F, 0.25F, 0, 0}; }
    SUBCASE("a mesh's skin out of range") { model.meshes[0].skin = 1; }
    SUBCASE("joints and weights on a mesh without a skin") {
        model.meshes[0].skin = Model::kNoSkin;
    }
    CHECK(CodeOf(model) == ModelDataCode::BadSkin);
}

TEST_CASE("Validate rule 4: BadClip") {
    ModelData model = ValidModel();
    SUBCASE("node count not the node list's") { model.clips[0].nodeCount = 2; }
    SUBCASE("a track's node out of range") { model.clips[0].tracks[0].node = 3; }
    SUBCASE("two tracks on one node") { model.clips[0].tracks.push_back(model.clips[0].tracks[0]); }
    SUBCASE("times and values differ in count") { model.clips[0].tracks[0].rotation.values.pop_back(); }
    SUBCASE("times not strictly increasing") { model.clips[0].tracks[0].translation.times[1] = 0.0F; }
    SUBCASE("a time after the duration") { model.clips[0].tracks[0].translation.times[2] = 2.5F; }
    SUBCASE("a negative time") { model.clips[0].tracks[0].translation.times[0] = -0.5F; }
    SUBCASE("a value not finite") {
        model.clips[0].tracks[0].translation.values[1].y = std::numeric_limits<float>::infinity();
    }
    SUBCASE("a scale value not finite") {
        model.clips[0].tracks[0].scale = Keys<Vec3>{{1.0F}, {{1, std::numeric_limits<float>::quiet_NaN(), 1}}};
    }
    SUBCASE("a negative duration") {
        model.clips[0].duration = -1.0F;
        model.clips[0].tracks.clear();
    }
    SUBCASE("a duration not finite") { model.clips[0].duration = std::numeric_limits<float>::infinity(); }
    CHECK(CodeOf(model) == ModelDataCode::BadClip);
}

TEST_CASE("Validate checks in order: nodes then each mesh then the clips") {
    ModelData model = ValidModel();
    model.clips[0].nodeCount = 2;
    model.meshes[0].vertexWeights[1] = {0.5F, 0.25F, 0, 0};
    CHECK(CodeOf(model) == ModelDataCode::BadSkin);
    model.meshes[0].indices.push_back(0);
    CHECK(CodeOf(model) == ModelDataCode::BadMesh);
    model.nodes[2].parent = 2;
    CHECK(CodeOf(model) == ModelDataCode::BadNode);
}

TEST_CASE("FindNode and FindClip give the first with the name") {
    ModelData model = ValidModel();
    model.nodes.push_back(MakeNode("arm", 0));
    CHECK(model.FindNode("arm") == std::optional<std::uint32_t>{1});
    CHECK(model.FindNode("leg") == std::nullopt);
    CHECK(model.FindClip("wave") == std::optional<std::uint32_t>{0});
    CHECK(model.FindClip("run") == std::nullopt);
}

TEST_CASE("RestPose is every node's rest") {
    const auto nodes = Chain();
    std::vector<LocalTransform> out(3, LocalTransform{{9, 9, 9}, AboutX(30), {2, 2, 2}});
    Model::RestPose(nodes, out);
    for (std::size_t i = 0; i < nodes.size(); ++i) CHECK(SameBits(out[i], nodes[i].rest));
}

TEST_CASE("SampleClip interpolates between keys: translation linearly and rotation by slerp") {
    const auto nodes = Chain();
    const auto pose = Sample(nodes, ArmClip(), 0.5F, Wrap::Clamp);
    INFO("translation " << Show(pose[1].translation) << " rotation " << Show(pose[1].rotation));
    CHECK(Near(pose[1].translation, {0, 2, 0}));
    CHECK(SameRotation(pose[1].rotation, AboutY(22.5F)));
    const auto later = Sample(nodes, ArmClip(), 1.5F, Wrap::Clamp);
    CHECK(Near(later[1].translation, {0, 4, 0}));
    CHECK(SameRotation(later[1].rotation, AboutY(67.5F)));
}

TEST_CASE("SampleClip at a key is the key's value") {
    const auto nodes = Chain();
    CHECK(Near(Sample(nodes, ArmClip(), 1.0F, Wrap::Clamp)[1].translation, {0, 3, 0}));
    CHECK(Near(Sample(nodes, ArmClip(), 0.0F, Wrap::Clamp)[1].translation, {0, 1, 0}));
    CHECK(Near(Sample(nodes, ArmClip(), 2.0F, Wrap::Clamp)[1].translation, {0, 5, 0}));
}

TEST_CASE("SampleClip leaves nodes without a track and channels without keys at rest") {
    const auto nodes = Chain();
    const auto pose = Sample(nodes, ArmClip(), 0.5F, Wrap::Clamp);
    CHECK(SameBits(pose[0], nodes[0].rest));
    CHECK(SameBits(pose[2], nodes[2].rest));
    CHECK(Near(pose[1].scale, nodes[1].rest.scale, 0.0F));
}

TEST_CASE("SampleClip outside a channel's keys gives the first or the last key") {
    auto nodes = Chain();
    AnimationClip clip = ArmClip();
    // Keys only between 0.5 and 1.5 of the 2 seconds.
    clip.tracks[0].translation = Keys<Vec3>{{0.5F, 1.5F}, {{0, 7, 0}, {0, 9, 0}}};
    CHECK(Near(Sample(nodes, clip, 0.25F, Wrap::Clamp)[1].translation, {0, 7, 0}));
    CHECK(Near(Sample(nodes, clip, 1.75F, Wrap::Clamp)[1].translation, {0, 9, 0}));
    CHECK(Near(Sample(nodes, clip, 1.0F, Wrap::Clamp)[1].translation, {0, 8, 0}));
}

TEST_CASE("Wrap::Clamp holds the ends") {
    const auto nodes = Chain();
    CHECK(Near(Sample(nodes, ArmClip(), -0.5F, Wrap::Clamp)[1].translation, {0, 1, 0}));
    CHECK(Near(Sample(nodes, ArmClip(), 5.0F, Wrap::Clamp)[1].translation, {0, 5, 0}));
    CHECK(SameRotation(Sample(nodes, ArmClip(), 5.0F, Wrap::Clamp)[1].rotation, AboutY(90)));
}

TEST_CASE("Wrap::Loop repeats and negative times count back from the end") {
    const auto nodes = Chain();
    CHECK(Near(Sample(nodes, ArmClip(), 2.5F, Wrap::Loop)[1].translation, {0, 2, 0}));
    CHECK(Near(Sample(nodes, ArmClip(), 4.5F, Wrap::Loop)[1].translation, {0, 2, 0}));
    CHECK(Near(Sample(nodes, ArmClip(), -0.5F, Wrap::Loop)[1].translation, {0, 4, 0}));
    // The end of a loop is its start.
    CHECK(Near(Sample(nodes, ArmClip(), 2.0F, Wrap::Loop)[1].translation, {0, 1, 0}));
    CHECK(Near(Sample(nodes, ArmClip(), 1.5F, Wrap::Loop)[1].translation, {0, 4, 0}));
    // Just before 0 rounds to the end of the loop, which is its start.
    CHECK(Near(Sample(nodes, ArmClip(), -1.0e-8F, Wrap::Loop)[1].translation, {0, 1, 0}));
}

TEST_CASE("Wrap::Loop of a clip of duration 0 is always at 0") {
    auto nodes = Chain();
    AnimationClip clip;
    clip.nodeCount = 3;
    Track arm;
    arm.node = 1;
    arm.translation = Keys<Vec3>{{0.0F}, {{0, 6, 0}}};
    clip.tracks.push_back(arm);
    CHECK(Near(Sample(nodes, clip, 3.25F, Wrap::Loop)[1].translation, {0, 6, 0}));
    CHECK(Near(Sample(nodes, clip, 3.25F, Wrap::Clamp)[1].translation, {0, 6, 0}));
}

TEST_CASE("SampleClip rotation keys take the shortest path") {
    auto nodes = Chain();
    AnimationClip clip = ArmClip();
    // 170 and -170 degrees are 20 degrees apart through 180.
    clip.tracks[0].rotation = Keys<Quaternion>{{0.0F, 2.0F}, {AboutY(170), AboutY(-170)}};
    CHECK(SameRotation(Sample(nodes, clip, 1.0F, Wrap::Clamp)[1].rotation, AboutY(180)));
}

TEST_CASE("SampleClip and RestPose: an output or a clip of another size is a Programmer Error") {
    const auto nodes = Chain();
    std::vector<LocalTransform> small(2);
    GYO_CHECK_ASSERTS(Model::SampleClip(nodes, ArmClip(), 0.0F, Wrap::Clamp, small));
    GYO_CHECK_ASSERTS(Model::RestPose(nodes, small));
    std::vector<LocalTransform> out(3);
    AnimationClip other = ArmClip();
    other.nodeCount = 4;
    GYO_CHECK_ASSERTS(Model::SampleClip(nodes, other, 0.0F, Wrap::Clamp, out));
    GYO_CHECK_ASSERTS(Model::SampleClip(nodes, ArmClip(), std::numeric_limits<float>::quiet_NaN(), Wrap::Loop, out));
}

TEST_CASE("Blend mixes translation and scale linearly and rotation by slerp") {
    const std::vector<LocalTransform> a{{{0, 0, 0}, Quaternion{}, {1, 1, 1}}, {{2, 0, 0}, AboutX(10), {1, 1, 1}}};
    const std::vector<LocalTransform> b{{{4, 0, 0}, AboutZ(90), {3, 3, 3}}, {{2, 0, 8}, AboutX(50), {1, 2, 1}}};
    std::vector<LocalTransform> out(2);
    Model::Blend(a, b, 0.25F, out);
    CHECK(Near(out[0].translation, {1, 0, 0}));
    CHECK(SameRotation(out[0].rotation, AboutZ(22.5F)));
    CHECK(Near(out[0].scale, {1.5F, 1.5F, 1.5F}));
    CHECK(Near(out[1].translation, {2, 0, 2}));
    CHECK(SameRotation(out[1].rotation, AboutX(20)));
    CHECK(Near(out[1].scale, {1, 1.25F, 1}));
}

TEST_CASE("Blend weight 0 is a and weight 1 is b and out may be an input") {
    const std::vector<LocalTransform> a{{{1, 2, 3}, AboutY(30), {1, 1, 1}}};
    const std::vector<LocalTransform> b{{{5, 6, 7}, AboutY(60), {2, 2, 2}}};
    std::vector<LocalTransform> out(1);
    Model::Blend(a, b, 0.0F, out);
    CHECK(Near(out[0].translation, a[0].translation));
    CHECK(SameRotation(out[0].rotation, a[0].rotation));
    Model::Blend(a, b, 1.0F, out);
    CHECK(Near(out[0].translation, b[0].translation));
    CHECK(SameRotation(out[0].rotation, b[0].rotation));
    CHECK(Near(out[0].scale, b[0].scale));
    std::vector<LocalTransform> inPlace = a;
    Model::Blend(inPlace, b, 0.5F, inPlace);
    CHECK(Near(inPlace[0].translation, {3, 4, 5}));
    CHECK(SameRotation(inPlace[0].rotation, AboutY(45)));
    std::vector<LocalTransform> intoB = b;
    Model::Blend(a, intoB, 0.5F, intoB);
    CHECK(Near(intoB[0].translation, {3, 4, 5}));
}

TEST_CASE("Blend: spans of different sizes are a Programmer Error") {
    const std::vector<LocalTransform> a(2);
    const std::vector<LocalTransform> b(3);
    std::vector<LocalTransform> out(2);
    GYO_CHECK_ASSERTS(Model::Blend(a, b, 0.5F, out));
    std::vector<LocalTransform> wide(3);
    GYO_CHECK_ASSERTS(Model::Blend(a, a, 0.5F, wide));
}

TEST_CASE("Blend with a weight per node mixes each node by its own weight") {
    const std::vector<LocalTransform> a{{{0, 0, 0}, Quaternion{}, {1, 1, 1}},
                                        {{0, 0, 0}, Quaternion{}, {1, 1, 1}},
                                        {{0, 0, 0}, Quaternion{}, {1, 1, 1}}};
    const std::vector<LocalTransform> b{{{4, 0, 0}, AboutZ(90), {3, 3, 3}},
                                        {{4, 0, 0}, AboutZ(90), {3, 3, 3}},
                                        {{4, 0, 0}, AboutZ(90), {3, 3, 3}}};
    const std::vector<float> weights{0.0F, 1.0F, 0.25F};
    std::vector<LocalTransform> out(3);
    Model::Blend(a, b, weights, out);
    CHECK(Near(out[0].translation, {0, 0, 0}));
    CHECK(SameRotation(out[0].rotation, Quaternion{}));
    CHECK(Near(out[0].scale, {1, 1, 1}));
    CHECK(Near(out[1].translation, {4, 0, 0}));
    CHECK(SameRotation(out[1].rotation, AboutZ(90)));
    CHECK(Near(out[1].scale, {3, 3, 3}));
    CHECK(Near(out[2].translation, {1, 0, 0}));
    CHECK(SameRotation(out[2].rotation, AboutZ(22.5F)));
    CHECK(Near(out[2].scale, {1.5F, 1.5F, 1.5F}));
}

TEST_CASE("Blend with a weight per node gives the single-weight Blend when every weight is the same") {
    const std::vector<LocalTransform> a{{{1, 2, 3}, AboutY(30), {1, 1, 1}}, {{2, 0, 0}, AboutX(10), {1, 2, 1}}};
    const std::vector<LocalTransform> b{{{5, 6, 7}, AboutY(80), {2, 2, 2}}, {{2, 0, 8}, AboutX(50), {1, 1, 3}}};
    const std::vector<float> weights{0.375F, 0.375F};
    std::vector<LocalTransform> perNode(2);
    std::vector<LocalTransform> single(2);
    Model::Blend(a, b, weights, perNode);
    Model::Blend(a, b, 0.375F, single);
    CHECK(SameBits(perNode[0], single[0]));
    CHECK(SameBits(perNode[1], single[1]));
}

TEST_CASE("Blend with a weight per node may write into an input") {
    const std::vector<LocalTransform> a{{{1, 2, 3}, AboutY(30), {1, 1, 1}}};
    const std::vector<LocalTransform> b{{{5, 6, 7}, AboutY(60), {2, 2, 2}}};
    const std::vector<float> half{0.5F};
    std::vector<LocalTransform> intoA = a;
    Model::Blend(intoA, b, half, intoA);
    CHECK(Near(intoA[0].translation, {3, 4, 5}));
    CHECK(SameRotation(intoA[0].rotation, AboutY(45)));
    std::vector<LocalTransform> intoB = b;
    Model::Blend(a, intoB, half, intoB);
    CHECK(Near(intoB[0].translation, {3, 4, 5}));
    CHECK(Near(intoB[0].scale, {1.5F, 1.5F, 1.5F}));
}

TEST_CASE("Blend with a weight per node: weights of another size are a Programmer Error") {
    const std::vector<LocalTransform> a(2);
    std::vector<LocalTransform> out(2);
    const std::vector<float> one{0.5F};
    GYO_CHECK_ASSERTS(Model::Blend(a, a, one, out));
    const std::vector<float> two{0.5F, 0.5F};
    std::vector<LocalTransform> wide(3);
    GYO_CHECK_ASSERTS(Model::Blend(a, a, two, wide));
}

TEST_CASE("ApplyAdditive with base equal to reference and weight 1 gives motion") {
    const std::vector<LocalTransform> reference{{{1, 0, 0}, AboutX(10), {1, 1, 1}}};
    const std::vector<LocalTransform> motion{{{3, 2, 0}, AboutX(70), {2, 1, 1}}};
    std::vector<LocalTransform> out(1);
    Model::ApplyAdditive(reference, motion, reference, 1.0F, out);
    CHECK(Near(out[0].translation, motion[0].translation));
    CHECK(SameRotation(out[0].rotation, motion[0].rotation));
    CHECK(Near(out[0].scale, motion[0].scale));
}

TEST_CASE("ApplyAdditive with motion equal to reference leaves base") {
    const std::vector<LocalTransform> base{{{5, 6, 7}, AboutY(40), {2, 2, 2}}};
    const std::vector<LocalTransform> reference{{{1, 0, 0}, AboutX(10), {1, 3, 1}}};
    std::vector<LocalTransform> out(1);
    for (const float weight : {0.0F, 0.5F, 1.0F}) {
        Model::ApplyAdditive(base, reference, reference, weight, out);
        CHECK(Near(out[0].translation, base[0].translation));
        CHECK(SameRotation(out[0].rotation, base[0].rotation));
        CHECK(Near(out[0].scale, base[0].scale));
    }
}

TEST_CASE("ApplyAdditive turns the base by the change in the node's own frame") {
    // base: 90 degrees about Y; the change of motion from reference: 90
    // degrees about X. In the node's own frame the result is base * change
    // (about Y, then about the turned X); in the parent's frame it would be
    // change * base, another rotation.
    const std::vector<LocalTransform> base{{{0, 0, 0}, AboutY(90), {1, 1, 1}}};
    const std::vector<LocalTransform> reference{{{0, 0, 0}, AboutZ(30), {1, 1, 1}}};
    const std::vector<LocalTransform> motion{{{0, 0, 0}, Math::Multiply(AboutZ(30), AboutX(90)), {1, 1, 1}}};
    std::vector<LocalTransform> out(1);
    Model::ApplyAdditive(base, motion, reference, 1.0F, out);
    const Quaternion own = Math::Multiply(AboutY(90), AboutX(90));
    const Quaternion parent = Math::Multiply(AboutX(90), AboutY(90));
    INFO("out " << Show(out[0].rotation));
    REQUIRE_FALSE(SameRotation(own, parent));
    CHECK(SameRotation(out[0].rotation, own));
}

TEST_CASE("ApplyAdditive weight 0.5 adds half of the change") {
    const std::vector<LocalTransform> base{{{1, 1, 1}, AboutY(10), {1, 1, 1}}};
    const std::vector<LocalTransform> reference{{{0, 0, 0}, Quaternion{}, {1, 1, 1}}};
    const std::vector<LocalTransform> motion{{{4, 0, 2}, AboutY(60), {3, 1, 2}}};
    std::vector<LocalTransform> out(1);
    Model::ApplyAdditive(base, motion, reference, 0.5F, out);
    CHECK(Near(out[0].translation, {3, 1, 2}));
    CHECK(SameRotation(out[0].rotation, AboutY(40)));
    CHECK(Near(out[0].scale, {2, 1, 1.5F}));
    Model::ApplyAdditive(base, motion, reference, 0.0F, out);
    CHECK(Near(out[0].translation, base[0].translation));
    CHECK(SameRotation(out[0].rotation, base[0].rotation));
    CHECK(Near(out[0].scale, base[0].scale));
}

TEST_CASE("ApplyAdditive may write into any of its inputs") {
    const std::vector<LocalTransform> base{{{1, 1, 1}, AboutY(10), {1, 1, 1}}};
    const std::vector<LocalTransform> reference{{{0, 0, 0}, Quaternion{}, {1, 1, 1}}};
    const std::vector<LocalTransform> motion{{{4, 0, 2}, AboutY(60), {3, 1, 2}}};
    std::vector<LocalTransform> expected(1);
    Model::ApplyAdditive(base, motion, reference, 1.0F, expected);
    CHECK(Near(expected[0].translation, {5, 1, 3}));
    CHECK(SameRotation(expected[0].rotation, AboutY(70)));
    std::vector<LocalTransform> intoBase = base;
    Model::ApplyAdditive(intoBase, motion, reference, 1.0F, intoBase);
    std::vector<LocalTransform> intoMotion = motion;
    Model::ApplyAdditive(base, intoMotion, reference, 1.0F, intoMotion);
    std::vector<LocalTransform> intoReference = reference;
    Model::ApplyAdditive(base, motion, intoReference, 1.0F, intoReference);
    CHECK(SameBits(intoBase[0], expected[0]));
    CHECK(SameBits(intoMotion[0], expected[0]));
    CHECK(SameBits(intoReference[0], expected[0]));
}

TEST_CASE("ApplyAdditive: spans of other sizes are a Programmer Error") {
    const std::vector<LocalTransform> base(1);
    const std::vector<LocalTransform> reference(1);
    const std::vector<LocalTransform> motion(1);
    const std::vector<LocalTransform> two(2);
    std::vector<LocalTransform> out(1);
    GYO_CHECK_ASSERTS(Model::ApplyAdditive(base, two, reference, 1.0F, out));
    GYO_CHECK_ASSERTS(Model::ApplyAdditive(base, motion, two, 1.0F, out));
    std::vector<LocalTransform> wide(2);
    GYO_CHECK_ASSERTS(Model::ApplyAdditive(base, motion, reference, 1.0F, wide));
}

namespace {

// Instants of a clip of duration 2: at the start, inside, and at the end.
const std::vector<float> kInstants{0.0F, 0.5F, 1.0F, 1.5F, 2.0F};

std::vector<std::uint32_t> Crossed(const float from, const float to, const Wrap wrap,
                                   const std::vector<float>& times = kInstants, const float duration = 2.0F) {
    std::vector<std::uint32_t> out(times.size());
    out.resize(Model::CrossedEvents(times, from, to, duration, wrap, out));
    return out;
}

using Indices = std::vector<std::uint32_t>;

} // namespace

TEST_CASE("CrossedEvents forward crosses from up to but not including to") {
    CHECK(Crossed(0.5F, 1.5F, Wrap::Clamp) == Indices{1, 2});
    CHECK(Crossed(0.5F, 1.5F, Wrap::Loop) == Indices{1, 2});
    CHECK(Crossed(0.0F, 0.25F, Wrap::Clamp) == Indices{0});
    CHECK(Crossed(0.6F, 0.9F, Wrap::Clamp).empty());
}

TEST_CASE("CrossedEvents of consecutive intervals cross each instant once") {
    const std::vector<float> cuts{0.0F, 0.25F, 0.5F, 1.25F, 1.5F, 1.75F};
    for (const Wrap wrap : {Wrap::Clamp, Wrap::Loop}) {
        Indices all;
        for (std::size_t i = 0; i + 1 < cuts.size(); ++i) {
            const Indices part = Crossed(cuts[i], cuts[i + 1], wrap);
            all.insert(all.end(), part.begin(), part.end());
        }
        CHECK(all == Indices{0, 1, 2, 3});
    }
}

TEST_CASE("CrossedEvents backward crosses from down to but not including to in descending order") {
    CHECK(Crossed(1.5F, 0.5F, Wrap::Clamp) == Indices{3, 2});
    CHECK(Crossed(1.5F, 0.5F, Wrap::Loop) == Indices{3, 2});
    CHECK(Crossed(1.4F, 1.1F, Wrap::Clamp).empty());
}

TEST_CASE("CrossedEvents with Wrap::Loop crosses the end into the start on times not wrapped") {
    const std::vector<float> times{0.25F, 1.0F, 1.75F};
    // 1.5 -> 2.5 is 1.5 -> 2 then 0 -> 0.5.
    CHECK(Crossed(1.5F, 2.5F, Wrap::Loop, times) == Indices{2, 0});
    // The same instants six loops on, and before 0.
    CHECK(Crossed(13.5F, 14.5F, Wrap::Loop, times) == Indices{2, 0});
    CHECK(Crossed(-0.5F, 0.5F, Wrap::Loop, times) == Indices{2, 0});
}

TEST_CASE("CrossedEvents with Wrap::Loop crosses the instant on the end when an interval stops on it") {
    // Steps that land on the end: the one reaching it crosses the instant on
    // the end, the next one, leaving the start, the instant on 0.
    const std::vector<float> cuts{0.0F, 0.5F, 1.0F, 1.5F, 2.0F, 2.5F, 3.0F};
    Indices all;
    for (std::size_t i = 0; i + 1 < cuts.size(); ++i) {
        const Indices part = Crossed(cuts[i], cuts[i + 1], Wrap::Loop);
        all.insert(all.end(), part.begin(), part.end());
    }
    CHECK(all == Indices{0, 1, 2, 3, 4, 0, 1});
    CHECK(Crossed(1.5F, 2.0F, Wrap::Loop) == Indices{3, 4});
    CHECK(Crossed(2.0F, 2.5F, Wrap::Loop) == Indices{0});
}

TEST_CASE("CrossedEvents backward with Wrap::Loop crosses the instant on the start when an interval stops on it") {
    // The mirror of forward: the step reaching 0 crosses the instant on 0,
    // the next one, leaving it, the instant on the end.
    const std::vector<float> cuts{3.0F, 2.5F, 2.0F, 1.5F, 1.0F, 0.5F, 0.0F, -0.5F};
    Indices all;
    for (std::size_t i = 0; i + 1 < cuts.size(); ++i) {
        const Indices part = Crossed(cuts[i], cuts[i + 1], Wrap::Loop);
        all.insert(all.end(), part.begin(), part.end());
    }
    CHECK(all == Indices{2, 1, 0, 4, 3, 2, 1, 0, 4});
    CHECK(Crossed(0.5F, 0.0F, Wrap::Loop) == Indices{1, 0});
    CHECK(Crossed(0.0F, -0.5F, Wrap::Loop) == Indices{4});
    CHECK(Crossed(2.0F, 0.0F, Wrap::Loop) == Indices{4, 3, 2, 1, 0});
}

TEST_CASE("CrossedEvents backward with Wrap::Loop crosses the start into the end") {
    const std::vector<float> times{0.25F, 1.0F, 1.75F};
    // 0.5 -> -0.5 is 0.5 -> 0 then 2 -> 1.5.
    CHECK(Crossed(0.5F, -0.5F, Wrap::Loop, times) == Indices{0, 2});
    CHECK(Crossed(4.5F, 3.5F, Wrap::Loop, times) == Indices{0, 2});
}

TEST_CASE("CrossedEvents with Wrap::Loop over a whole duration or more crosses each instant once from where it starts") {
    const std::vector<float> times{0.25F, 1.0F, 1.75F};
    CHECK(Crossed(1.5F, 3.5F, Wrap::Loop, times) == Indices{2, 0, 1});
    CHECK(Crossed(1.5F, 9.0F, Wrap::Loop, times) == Indices{2, 0, 1});
    CHECK(Crossed(1.5F, -0.5F, Wrap::Loop, times) == Indices{1, 0, 2});
    CHECK(Crossed(1.5F, -7.0F, Wrap::Loop, times) == Indices{1, 0, 2});
}

TEST_CASE("CrossedEvents with Wrap::Loop crosses the instants at 0 and at the duration both when it loops") {
    CHECK(Crossed(1.75F, 2.25F, Wrap::Loop) == Indices{4, 0});
    CHECK(Crossed(2.25F, 1.75F, Wrap::Loop) == Indices{0, 4});
}

TEST_CASE("CrossedEvents with Wrap::Clamp crosses the end once when the time reaches it") {
    // Reaching the end exactly, or passing it, crosses the instant on it.
    CHECK(Crossed(1.75F, 2.0F, Wrap::Clamp) == Indices{4});
    CHECK(Crossed(1.75F, 3.0F, Wrap::Clamp) == Indices{4});
    // After the end nothing more, whether the time stays or goes on.
    CHECK(Crossed(2.0F, 3.0F, Wrap::Clamp).empty());
    CHECK(Crossed(3.0F, 4.0F, Wrap::Clamp).empty());
    // Before the start nothing; reaching it from before crosses the instant at 0.
    CHECK(Crossed(-2.0F, -1.0F, Wrap::Clamp).empty());
    CHECK(Crossed(-1.0F, 0.25F, Wrap::Clamp) == Indices{0});
}

TEST_CASE("CrossedEvents backward with Wrap::Clamp crosses the start once when the time reaches it") {
    CHECK(Crossed(0.25F, 0.0F, Wrap::Clamp) == Indices{0});
    CHECK(Crossed(0.25F, -1.0F, Wrap::Clamp) == Indices{0});
    CHECK(Crossed(0.0F, -1.0F, Wrap::Clamp).empty());
    CHECK(Crossed(3.0F, 1.75F, Wrap::Clamp) == Indices{4});
}

TEST_CASE("CrossedEvents with Wrap::Clamp turning back at an end crosses the instant on it again") {
    // As at any turn: the interval leaving the instant the time stands on crosses it.
    CHECK(Crossed(1.75F, 2.0F, Wrap::Clamp) == Indices{4});
    CHECK(Crossed(2.0F, 1.75F, Wrap::Clamp) == Indices{4});
    CHECK(Crossed(0.25F, 0.0F, Wrap::Clamp) == Indices{0});
    CHECK(Crossed(0.0F, 0.25F, Wrap::Clamp) == Indices{0});
    // A turn inside: once.
    CHECK(Crossed(0.25F, 0.5F, Wrap::Clamp).empty());
    CHECK(Crossed(0.5F, 0.25F, Wrap::Clamp) == Indices{1});
}

TEST_CASE("CrossedEvents crosses nothing when the time stays or the clip has no length") {
    CHECK(Crossed(0.5F, 0.5F, Wrap::Clamp).empty());
    CHECK(Crossed(0.5F, 0.5F, Wrap::Loop).empty());
    const std::vector<float> atZero{0.0F};
    CHECK(Crossed(-1.0F, 1.0F, Wrap::Clamp, atZero, 0.0F).empty());
    CHECK(Crossed(-1.0F, 1.0F, Wrap::Loop, atZero, 0.0F).empty());
}

TEST_CASE("CrossedEvents: a short out or a time that is not finite or a negative duration is a Programmer Error") {
    std::vector<std::uint32_t> small(kInstants.size() - 1);
    GYO_CHECK_ASSERTS(static_cast<void>(Model::CrossedEvents(kInstants, 0.0F, 1.0F, 2.0F, Wrap::Clamp, small)));
    std::vector<std::uint32_t> out(kInstants.size());
    const float nan = std::numeric_limits<float>::quiet_NaN();
    GYO_CHECK_ASSERTS(static_cast<void>(Model::CrossedEvents(kInstants, nan, 1.0F, 2.0F, Wrap::Loop, out)));
    GYO_CHECK_ASSERTS(static_cast<void>(Model::CrossedEvents(kInstants, 0.0F, nan, 2.0F, Wrap::Loop, out)));
    GYO_CHECK_ASSERTS(static_cast<void>(Model::CrossedEvents(kInstants, 0.0F, 1.0F, -1.0F, Wrap::Clamp, out)));
}

namespace {

using Model::TwoBoneChain;

// arm (0) -> forearm (1) -> hand (2) -> finger (3), along +X: the arm 1 long,
// the forearm `forearm` long, the finger off the hand.
std::vector<Node> Arm(const float forearm = 1.0F) {
    return {MakeNode("arm", Model::kNoParent), MakeNode("forearm", 0, {{1, 0, 0}, {}, {1, 1, 1}}),
            MakeNode("hand", 1, {{forearm, 0, 0}, {}, {1, 1, 1}}), MakeNode("finger", 2, {{0.25F, 0, 0}, {}, {1, 1, 1}})};
}
constexpr TwoBoneChain kArm{0, 1, 2};

std::vector<LocalTransform> RestOf(const std::vector<Node>& nodes) {
    std::vector<LocalTransform> pose(nodes.size());
    Model::RestPose(nodes, pose);
    return pose;
}

std::vector<Vec3> Positions(const std::vector<Node>& nodes, const std::vector<LocalTransform>& pose) {
    std::vector<Math::Matrix4> model(nodes.size());
    Model::ComputeModelSpace(nodes, pose, model);
    std::vector<Vec3> out;
    for (const Math::Matrix4& m : model) out.push_back(Math::TransformPoint(m, {0, 0, 0}));
    return out;
}

constexpr float kPositionTolerance = 1.0e-4F;

} // namespace

TEST_CASE("SolveTwoBoneIK puts the tip on a target in reach and keeps the bones and the rest of the pose") {
    const auto nodes = Arm();
    auto pose = RestOf(nodes);
    pose[2].rotation = AboutY(30);
    const auto before = pose;
    Model::SolveTwoBoneIK(nodes, kArm, {1, 1, 0}, {0, 0, 1}, 1.0F, pose);
    const auto at = Positions(nodes, pose);
    INFO("mid " << Show(at[1]) << " tip " << Show(at[2]));
    CHECK(Near(at[2], {1, 1, 0}, kPositionTolerance));
    CHECK(Near(Math::Distance(at[0], at[1]), 1.0F, kPositionTolerance));
    CHECK(Near(Math::Distance(at[1], at[2]), 1.0F, kPositionTolerance));
    // Only the rotations of root and mid change.
    for (std::size_t i = 0; i < pose.size(); ++i) {
        LocalTransform unturned = pose[i];
        unturned.rotation = before[i].rotation;
        CHECK(SameBits(unturned, before[i]));
        if (i >= 2) CHECK(SameBits(pose[i], before[i]));
    }
}

TEST_CASE("SolveTwoBoneIK bends toward the pole in the plane of root and target and pole") {
    const auto nodes = Arm();
    for (const float side : {1.0F, -1.0F}) {
        auto pose = RestOf(nodes);
        Model::SolveTwoBoneIK(nodes, kArm, {1.5F, 0, 0}, {0.75F, 0, side}, 1.0F, pose);
        const auto at = Positions(nodes, pose);
        INFO("side " << side << " mid " << Show(at[1]) << " tip " << Show(at[2]));
        CHECK(Near(at[2], {1.5F, 0, 0}, kPositionTolerance));
        CHECK(at[1].z * side > 0.5F);
        CHECK(Near(at[1].y, 0.0F, kPositionTolerance));
    }
}

TEST_CASE("SolveTwoBoneIK straightens the chain toward a target out of reach") {
    const auto nodes = Arm();
    auto pose = RestOf(nodes);
    Model::SolveTwoBoneIK(nodes, kArm, {0, 4, 3}, {0, 0, 1}, 1.0F, pose);
    const auto at = Positions(nodes, pose);
    INFO("mid " << Show(at[1]) << " tip " << Show(at[2]));
    CHECK(Near(at[1], {0, 0.8F, 0.6F}, kPositionTolerance));
    CHECK(Near(at[2], {0, 1.6F, 1.2F}, kPositionTolerance));
}

TEST_CASE("SolveTwoBoneIK at the edge of reach meets the straightened chain") {
    const auto nodes = Arm();
    auto edge = RestOf(nodes);
    Model::SolveTwoBoneIK(nodes, kArm, {0, 2, 0}, {0, 1, 1}, 1.0F, edge);
    auto inside = RestOf(nodes);
    Model::SolveTwoBoneIK(nodes, kArm, {0, 1.9999F, 0}, {0, 1, 1}, 1.0F, inside);
    const auto atEdge = Positions(nodes, edge);
    const auto atInside = Positions(nodes, inside);
    INFO("edge mid " << Show(atEdge[1]) << " inside mid " << Show(atInside[1]));
    CHECK(Near(atEdge[1], {0, 1, 0}, kPositionTolerance));
    CHECK(Near(atEdge[2], {0, 2, 0}, kPositionTolerance));
    CHECK(Near(atInside[1], atEdge[1], 2.0e-2F));
    CHECK(Near(atInside[2], {0, 1.9999F, 0}, kPositionTolerance));
}

TEST_CASE("SolveTwoBoneIK folds the chain toward a target inside the body nearer than the bones' difference") {
    // arm 1, forearm 0.5: nothing nearer the root than 0.5 can be reached.
    const auto nodes = Arm(0.5F);
    auto pose = RestOf(nodes);
    Model::SolveTwoBoneIK(nodes, kArm, {0, 0.2F, 0}, {0, 0, 1}, 1.0F, pose);
    const auto at = Positions(nodes, pose);
    INFO("mid " << Show(at[1]) << " tip " << Show(at[2]));
    CHECK(Near(at[1], {0, 1, 0}, kPositionTolerance));
    CHECK(Near(at[2], {0, 0.5F, 0}, kPositionTolerance));
    // A straight arm folding back onto itself: the forearm turns half a turn.
    auto back = RestOf(nodes);
    Model::SolveTwoBoneIK(nodes, kArm, {0.2F, 0, 0}, {0, 0, 1}, 1.0F, back);
    const auto atBack = Positions(nodes, back);
    INFO("back mid " << Show(atBack[1]) << " tip " << Show(atBack[2]));
    CHECK(Near(atBack[1], {1, 0, 0}, kPositionTolerance));
    CHECK(Near(atBack[2], {0.5F, 0, 0}, kPositionTolerance));
    // Equal bones reach a target close to the root.
    const auto equal = Arm();
    auto near = RestOf(equal);
    Model::SolveTwoBoneIK(equal, kArm, {0.05F, 0.05F, 0}, {0, 0, 1}, 1.0F, near);
    const auto atNear = Positions(equal, near);
    INFO("near tip " << Show(atNear[2]));
    CHECK(Near(atNear[2], {0.05F, 0.05F, 0}, kPositionTolerance));
    CHECK(Near(Math::Distance(atNear[0], atNear[1]), 1.0F, kPositionTolerance));
}

TEST_CASE("SolveTwoBoneIK leaves the pose for a target on the root or a bone of length 0") {
    const auto nodes = Arm();
    auto pose = RestOf(nodes);
    const auto before = pose;
    Model::SolveTwoBoneIK(nodes, kArm, {0, 0, 0}, {0, 0, 1}, 1.0F, pose);
    for (std::size_t i = 0; i < pose.size(); ++i) CHECK(SameBits(pose[i], before[i]));
    const auto folded = Arm(0.0F);
    auto zero = RestOf(folded);
    const auto zeroBefore = zero;
    Model::SolveTwoBoneIK(folded, kArm, {0.5F, 0.5F, 0}, {0, 0, 1}, 1.0F, zero);
    for (std::size_t i = 0; i < zero.size(); ++i) CHECK(SameBits(zero[i], zeroBefore[i]));
}

TEST_CASE("SolveTwoBoneIK weight 0 leaves the pose bit for bit") {
    const auto nodes = Arm();
    auto pose = RestOf(nodes);
    pose[0].rotation = AboutZ(10);
    // Not of unit length: weight 0 does not even normalize it.
    pose[1].rotation = Quaternion{0.1F, 0, 0, 1};
    const auto before = pose;
    Model::SolveTwoBoneIK(nodes, kArm, {1, 1, 0}, {0, 0, 1}, 0.0F, pose);
    for (std::size_t i = 0; i < pose.size(); ++i) CHECK(SameBits(pose[i], before[i]));
}

TEST_CASE("SolveTwoBoneIK weight 0.5 slerps each rotation halfway to the solved one") {
    const auto nodes = Arm();
    auto original = RestOf(nodes);
    original[1].rotation = AboutY(40);
    auto solved = original;
    Model::SolveTwoBoneIK(nodes, kArm, {1, 1, 0}, {0, 0, 1}, 1.0F, solved);
    auto half = original;
    Model::SolveTwoBoneIK(nodes, kArm, {1, 1, 0}, {0, 0, 1}, 0.5F, half);
    for (const std::uint32_t node : {kArm.root, kArm.mid}) {
        const Quaternion expected = Math::Slerp(original[node].rotation, solved[node].rotation, 0.5F);
        CHECK(SameBits(half[node].rotation.x, expected.x));
        CHECK(SameBits(half[node].rotation.y, expected.y));
        CHECK(SameBits(half[node].rotation.z, expected.z));
        CHECK(SameBits(half[node].rotation.w, expected.w));
    }
    REQUIRE_FALSE(SameRotation(solved[kArm.root].rotation, original[kArm.root].rotation));
}

TEST_CASE("SolveTwoBoneIK with a pole on the line to the target bends to the side the mid is on") {
    // The forearm turned 90 degrees: mid at (1, 0, 0), tip at (1, 1, 0).
    const auto nodes = Arm();
    auto pose = RestOf(nodes);
    pose[1].rotation = AboutZ(90);
    const Vec3 target{1.2F, 0.6F, 0};
    // The mid's side of the line from root to target, before.
    const Vec3 u = Math::Normalize(target);
    const Vec3 side = Vec3{1, 0, 0} - u * Math::Dot(Vec3{1, 0, 0}, u);
    Model::SolveTwoBoneIK(nodes, kArm, target, target * 2.0F, 1.0F, pose);
    const auto at = Positions(nodes, pose);
    INFO("mid " << Show(at[1]) << " tip " << Show(at[2]));
    CHECK(Near(at[2], target, kPositionTolerance));
    CHECK(Math::Dot(at[1] - u * Math::Dot(at[1], u), side) > 0.0F);
    // A straight chain with the pole on the line still reaches, to some side.
    auto straight = RestOf(nodes);
    Model::SolveTwoBoneIK(nodes, kArm, {1, 0, 0}, {3, 0, 0}, 1.0F, straight);
    const auto atStraight = Positions(nodes, straight);
    INFO("straight mid " << Show(atStraight[1]) << " tip " << Show(atStraight[2]));
    CHECK(Near(atStraight[2], {1, 0, 0}, kPositionTolerance));
    CHECK(Near(Math::Distance(atStraight[0], atStraight[1]), 1.0F, kPositionTolerance));
    CHECK(Near(Math::Distance(atStraight[1], atStraight[2]), 1.0F, kPositionTolerance));
}

TEST_CASE("SolveTwoBoneIK with a pole a rounding away from the line still reaches the target") {
    // Far from the origin, with the pole just off the line from root to
    // target: its side of the line is mostly rounding, and the mid must stay
    // on the bones' spheres all the same.
    std::vector<Node> nodes = Arm();
    nodes[0].rest.translation = {100, 50, -30};
    const Vec3 root{100, 50, -30};
    const Vec3 toward = Math::Normalize(Vec3{0.3F, 0.7F, 0.2F});
    // The larger ones are just past what counts as on the line, where the side
    // is still mostly rounding.
    for (const float off : {0.0F, 1.0e-6F, 1.0e-4F, 1.0e-2F, 2.0e-2F, 5.0e-2F}) {
        auto pose = RestOf(nodes);
        const Vec3 target = root + toward * 1.3F;
        const Vec3 pole = root + toward * 40.0F + Vec3{0, 0, off};
        Model::SolveTwoBoneIK(nodes, kArm, target, pole, 1.0F, pose);
        const auto at = Positions(nodes, pose);
        INFO("off " << off << " tip " << Show(at[2]) << " target " << Show(target));
        CHECK(Near(at[2], target, 1.0e-3F));
        CHECK(Near(Math::Distance(at[0], at[1]), 1.0F, 1.0e-3F));
        if (off == 0.0F) {
            // On the line: to the side the mid is on (+X, the arm at rest).
            const Vec3 mid = at[1] - root;
            const Vec3 x{1, 0, 0};
            CHECK(Math::Dot(mid - toward * Math::Dot(mid, toward), x - toward * Math::Dot(x, toward)) > 0.0F);
        }
    }
}

TEST_CASE("SolveTwoBoneIK near the origin with a pole just off the line still reaches the target") {
    // Small coordinates: a side just past rounding still carries a part along
    // the line that would take the tip off the target.
    const auto nodes = Arm();
    const Vec3 toward = Math::Normalize(Vec3{0.3F, 0.7F, 0.2F});
    const Vec3 target = toward * 1.3F;
    for (const float along : {0.1F, 3.0F, 40.0F}) {
        for (const float off : {3.0e-7F, 8.0e-6F, 1.25e-4F}) {
            auto pose = RestOf(nodes);
            Model::SolveTwoBoneIK(nodes, kArm, target, toward * along + Vec3{0, 0, off}, 1.0F, pose);
            const auto at = Positions(nodes, pose);
            INFO("along " << along << " off " << off << " tip " << Show(at[2]));
            CHECK(Near(at[2], target, kPositionTolerance));
        }
    }
}

TEST_CASE("SolveTwoBoneIK with a pole on the line near the root or far out bends to the side the mid is on") {
    // A pole on the line close to the root, or one far along it, with the
    // whole arm far from the origin: the point's own rounding is larger
    // than its side, and the chain still bends to the mid's side.
    std::vector<Node> nodes = Arm();
    nodes[0].rest.translation = {50, 50, 50};
    const Vec3 root{50, 50, 50};
    const Vec3 toward = Math::Normalize(Vec3{0.3F, 0.7F, 0.2F});
    const Vec3 target = root + toward * 1.3F;
    for (const float along : {0.01F, 0.1F, 3.0F, 40.0F}) {
        auto pose = RestOf(nodes);
        Model::SolveTwoBoneIK(nodes, kArm, target, root + (target - root) * along, 1.0F, pose);
        const auto at = Positions(nodes, pose);
        INFO("along " << along << " mid " << Show(at[1]) << " tip " << Show(at[2]));
        CHECK(Near(at[2], target, 1.0e-3F));
        const Vec3 mid = at[1] - root;
        const Vec3 x{1, 0, 0};
        CHECK(Math::Dot(mid - toward * Math::Dot(mid, toward), x - toward * Math::Dot(x, toward)) > 0.0F);
    }
}

TEST_CASE("SolveTwoBoneIK works in model space under turned and scaled ancestors and a node between root and mid") {
    // spine (0) up 1, turned 90 degrees about Y, scale 2 -> upperarm (1)
    // -> twist (2) -> forearm (3) -> hand (4); the chain is upperarm, forearm, hand.
    const std::vector<Node> nodes{MakeNode("spine", Model::kNoParent, {{0, 1, 0}, AboutY(90), {2, 2, 2}}),
                                  MakeNode("upperarm", 0, {{0.5F, 0, 0}, AboutZ(20), {1, 1, 1}}),
                                  MakeNode("twist", 1, {{0.5F, 0, 0}, AboutX(30), {1, 1, 1}}),
                                  MakeNode("forearm", 2, {{0.5F, 0, 0}, {}, {1, 1, 1}}),
                                  MakeNode("hand", 3, {{1, 0, 0}, {}, {1, 1, 1}})};
    auto pose = RestOf(nodes);
    const Vec3 root = Positions(nodes, pose)[1];
    const Vec3 target = root + Vec3{0, 2.5F, 1.5F};
    Model::SolveTwoBoneIK(nodes, {1, 3, 4}, target, root + Vec3{1, 0, 0}, 1.0F, pose);
    const auto at = Positions(nodes, pose);
    INFO("tip " << Show(at[4]) << " target " << Show(target));
    CHECK(Near(at[4], target, kPositionTolerance));
    CHECK(Near(Math::Distance(at[1], at[3]), 2.0F, kPositionTolerance));
    CHECK(Near(Math::Distance(at[3], at[4]), 2.0F, kPositionTolerance));
}

TEST_CASE("SolveTwoBoneIK: a chain or a pose that does not fit or a value that is not finite is a Programmer Error") {
    const auto nodes = Arm();
    auto pose = RestOf(nodes);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    GYO_CHECK_ASSERTS(Model::SolveTwoBoneIK(nodes, {0, 1, 9}, {1, 1, 0}, {0, 0, 1}, 1.0F, pose));
    GYO_CHECK_ASSERTS(Model::SolveTwoBoneIK(nodes, {1, 0, 2}, {1, 1, 0}, {0, 0, 1}, 1.0F, pose));
    GYO_CHECK_ASSERTS(Model::SolveTwoBoneIK(nodes, {0, 2, 1}, {1, 1, 0}, {0, 0, 1}, 1.0F, pose));
    GYO_CHECK_ASSERTS(Model::SolveTwoBoneIK(nodes, {0, 1, 1}, {1, 1, 0}, {0, 0, 1}, 1.0F, pose));
    std::vector<LocalTransform> small(2);
    GYO_CHECK_ASSERTS(Model::SolveTwoBoneIK(nodes, kArm, {1, 1, 0}, {0, 0, 1}, 1.0F, small));
    GYO_CHECK_ASSERTS(Model::SolveTwoBoneIK(nodes, kArm, {nan, 1, 0}, {0, 0, 1}, 1.0F, pose));
    GYO_CHECK_ASSERTS(Model::SolveTwoBoneIK(nodes, kArm, {1, 1, 0}, {0, nan, 1}, 1.0F, pose));
    GYO_CHECK_ASSERTS(Model::SolveTwoBoneIK(nodes, kArm, {1, 1, 0}, {0, 0, 1}, nan, pose));
}

TEST_CASE("ComputeModelSpace multiplies down the hierarchy") {
    // root at (1, 0, 0) turned 90 degrees about Y; arm 1 along the root's +Z;
    // hand 2 along the arm's +Y. A positive turn about +Y takes +Z to +X.
    const std::vector<Node> nodes{MakeNode("root", Model::kNoParent), MakeNode("arm", 0), MakeNode("hand", 1)};
    const std::vector<LocalTransform> local{
        {{1, 0, 0}, AboutY(90), {1, 1, 1}}, {{0, 0, 1}, Quaternion{}, {2, 2, 2}}, {{0, 1, 0}, Quaternion{}, {1, 1, 1}}};
    std::vector<Math::Matrix4> out(3);
    Model::ComputeModelSpace(nodes, local, out);
    const Vec3 root = Math::TransformPoint(out[0], {0, 0, 0});
    const Vec3 arm = Math::TransformPoint(out[1], {0, 0, 0});
    const Vec3 hand = Math::TransformPoint(out[2], {0, 0, 0});
    INFO("root " << Show(root) << " arm " << Show(arm) << " hand " << Show(hand));
    CHECK(Near(root, {1, 0, 0}));
    CHECK(Near(arm, {2, 0, 0}));
    // The arm's scale of 2 doubles the hand's offset.
    CHECK(Near(hand, {2, 2, 0}));
}

TEST_CASE("ComputeModelSpace: spans of other sizes are a Programmer Error") {
    const auto nodes = Chain();
    const std::vector<LocalTransform> local(3);
    std::vector<Math::Matrix4> small(2);
    GYO_CHECK_ASSERTS(Model::ComputeModelSpace(nodes, local, small));
    const std::vector<LocalTransform> shortLocal(2);
    std::vector<Math::Matrix4> out(3);
    GYO_CHECK_ASSERTS(Model::ComputeModelSpace(nodes, shortLocal, out));
}

// --- Retargeting --------------------------------------------------------------

namespace {

Model::NameBinding Bind(const std::vector<Node>& source, const std::vector<Node>& target) {
    auto result = Model::BindByName(source, target);
    REQUIRE(result.has_value());
    return *result;
}

AnimationClip Retargeted(const std::vector<Node>& source, const std::vector<Node>& target, const AnimationClip& clip,
                         const float translationScale = 1.0F) {
    const auto binding = Bind(source, target);
    auto result = Model::Retarget(source, target, clip, binding.bindings, Model::RetargetSettings{translationScale});
    REQUIRE(result.has_value());
    return *result;
}

RetargetCode RetargetCodeOf(const std::vector<Node>& source, const std::vector<Node>& target,
                            const std::vector<JointBinding>& bindings, const float translationScale = 1.0F) {
    const auto result = Model::Retarget(source, target, ArmClip(), bindings, Model::RetargetSettings{translationScale});
    REQUIRE_FALSE(result.has_value());
    return result.error().code;
}

// The model-space rotation of node i with the given local rotations.
Quaternion GlobalRotation(const std::vector<Node>& nodes, const std::vector<LocalTransform>& local, std::uint32_t i) {
    Quaternion q = Math::Normalize(local[i].rotation);
    while (nodes[i].parent != Model::kNoParent) {
        i = nodes[i].parent;
        q = Math::Multiply(Math::Normalize(local[i].rotation), q);
    }
    return q;
}

} // namespace

TEST_CASE("BindByName binds equal names and lists the rest") {
    const std::vector<Node> source{MakeNode("root", Model::kNoParent), MakeNode("hips", 0), MakeNode("tail", 1),
                                   MakeNode("head", 1)};
    const std::vector<Node> target{MakeNode("Root", Model::kNoParent), MakeNode("head", 0), MakeNode("hips", 0),
                                   MakeNode("prop", 2)};
    const auto binding = Bind(source, target);
    CHECK(binding.bindings == std::vector<JointBinding>{{1, 2}, {3, 1}});
    CHECK(binding.unmatchedSource == std::vector<std::uint32_t>{0, 2});
    CHECK(binding.unmatchedTarget == std::vector<std::uint32_t>{0, 3});
}

TEST_CASE("BindByName: a name on two nodes of one list is AmbiguousName when the other list has it") {
    const std::vector<Node> single{MakeNode("root", Model::kNoParent), MakeNode("bone", 0)};
    const std::vector<Node> twice{MakeNode("root", Model::kNoParent), MakeNode("bone", 0), MakeNode("bone", 0)};
    const auto inTarget = Model::BindByName(single, twice);
    REQUIRE_FALSE(inTarget.has_value());
    CHECK(inTarget.error().code == RetargetCode::AmbiguousName);
    const auto inSource = Model::BindByName(twice, single);
    REQUIRE_FALSE(inSource.has_value());
    CHECK(inSource.error().code == RetargetCode::AmbiguousName);
    // A repeated name the other list lacks is only unmatched.
    const std::vector<Node> other{MakeNode("root", Model::kNoParent)};
    const auto unmatched = Model::BindByName(twice, other);
    REQUIRE(unmatched.has_value());
    CHECK(unmatched->unmatchedSource == std::vector<std::uint32_t>{1, 2});
}

TEST_CASE("Retarget between equal node lists gives the same keys") {
    const auto nodes = Chain();
    const AnimationClip out = Retargeted(nodes, nodes, ArmClip());
    REQUIRE(out.tracks.size() == 1);
    const Track& track = out.tracks[0];
    CHECK(track.node == 1);
    CHECK(out.duration == 2.0F);
    CHECK(out.nodeCount == 3);
    CHECK(out.name == "wave");
    CHECK(track.translation.times == ArmClip().tracks[0].translation.times);
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK(Near(track.translation.values[i], ArmClip().tracks[0].translation.values[i]));
    }
    CHECK(SameRotation(track.rotation.values[1], AboutY(90)));
    CHECK(Model::ValidateClip(nodes, out).has_value());
}

TEST_CASE("Retarget carries motion relative to rest: a source rest key gives the target rest") {
    // The source arm rests turned 30 degrees about X; the target arm rests
    // turned -45 degrees about Z, 2 above its root.
    std::vector<Node> source = Chain();
    source[1].rest.rotation = AboutX(30);
    std::vector<Node> target = Chain();
    target[1].rest = {{0, 2, 0}, AboutZ(-45), {1, 1, 1}};
    AnimationClip clip = ArmClip();
    clip.tracks[0].rotation = Keys<Quaternion>{{0.0F, 2.0F}, {AboutX(30), Math::Multiply(AboutY(40), AboutX(30))}};
    clip.tracks[0].translation = Keys<Vec3>{{0.0F}, {{0, 1, 0}}};
    const AnimationClip out = Retargeted(source, target, clip);
    REQUIRE(out.tracks.size() == 1);
    CHECK(SameRotation(out.tracks[0].rotation.values[0], AboutZ(-45)));
    CHECK(Near(out.tracks[0].translation.values[0], {0, 2, 0}));
    // The 40 degrees about Y (applied over the rest, in the parent's frame)
    // carries over.
    CHECK(SameRotation(out.tracks[0].rotation.values[1], Math::Multiply(AboutY(40), AboutZ(-45))));
}

TEST_CASE("Retarget keeps the model-space change of rotation when parent rests differ") {
    // The source hips rest turned 90 degrees about Y, the target's do not.
    // The animated spine turns 30 degrees about X in its parent's frame; in
    // model space that is a turn about the source parent's X, which the
    // target must reproduce.
    const std::vector<Node> source{MakeNode("hips", Model::kNoParent, {{}, AboutY(90), {1, 1, 1}}),
                                   MakeNode("spine", 0, {{0, 1, 0}, AboutZ(10), {1, 1, 1}})};
    const std::vector<Node> target{MakeNode("hips", Model::kNoParent),
                                   MakeNode("spine", 0, {{0, 1, 0}, AboutX(-20), {1, 1, 1}})};
    AnimationClip clip;
    clip.duration = 1.0F;
    clip.nodeCount = 2;
    Track spine;
    spine.node = 1;
    spine.rotation = Keys<Quaternion>{{1.0F}, {Math::Multiply(AboutX(30), AboutZ(10))}};
    clip.tracks.push_back(spine);
    const AnimationClip out = Retargeted(source, target, clip);

    std::vector<LocalTransform> sourceRest(2), sourcePose(2), targetRest(2), targetPose(2);
    Model::RestPose(source, sourceRest);
    Model::RestPose(target, targetRest);
    Model::SampleClip(source, clip, 1.0F, Wrap::Clamp, sourcePose);
    Model::SampleClip(target, out, 1.0F, Wrap::Clamp, targetPose);
    const Quaternion sourceChange =
        Math::Multiply(GlobalRotation(source, sourcePose, 1), Math::Conjugate(GlobalRotation(source, sourceRest, 1)));
    const Quaternion targetChange =
        Math::Multiply(GlobalRotation(target, targetPose, 1), Math::Conjugate(GlobalRotation(target, targetRest, 1)));
    INFO("source " << Show(sourceChange) << " target " << Show(targetChange));
    CHECK(SameRotation(sourceChange, targetChange));
    // Not the same local value: the parent frames differ.
    CHECK_FALSE(SameRotation(targetPose[1].rotation, sourcePose[1].rotation));
}

TEST_CASE("Retarget scales translation motion by translationScale and the parents' rest scales") {
    // The source root's parent rests at scale 2, the target's at scale 1.
    const std::vector<Node> source{MakeNode("armature", Model::kNoParent, {{}, {}, {2, 2, 2}}),
                                   MakeNode("hips", 0, {{0, 1, 0}, {}, {1, 1, 1}})};
    const std::vector<Node> target{MakeNode("armature", Model::kNoParent),
                                   MakeNode("hips", 0, {{0, 3, 0}, {}, {1, 1, 1}})};
    AnimationClip clip;
    clip.duration = 1.0F;
    clip.nodeCount = 2;
    Track hips;
    hips.node = 1;
    hips.translation = Keys<Vec3>{{0.0F, 1.0F}, {{0, 1, 0}, {0, 1, 2}}};
    clip.tracks.push_back(hips);
    const AnimationClip out = Retargeted(source, target, clip, 0.25F);
    REQUIRE(out.tracks.size() == 1);
    CHECK(Near(out.tracks[0].translation.values[0], {0, 3, 0}));
    // 2 in the source's frame is 4 in model space; times 0.25 is 1 in the
    // target's model space and its frame.
    INFO(Show(out.tracks[0].translation.values[1]));
    CHECK(Near(out.tracks[0].translation.values[1], {0, 3, 1}));
}

TEST_CASE("Retarget turns translation motion into the target parent's frame") {
    const std::vector<Node> source{MakeNode("hips", Model::kNoParent, {{}, AboutY(90), {1, 1, 1}}),
                                   MakeNode("spine", 0, {{0, 1, 0}, {}, {1, 1, 1}})};
    const std::vector<Node> target{MakeNode("hips", Model::kNoParent), MakeNode("spine", 0, {{0, 1, 0}, {}, {1, 1, 1}})};
    AnimationClip clip;
    clip.duration = 1.0F;
    clip.nodeCount = 2;
    Track spine;
    spine.node = 1;
    // One along the source hips' +Z, which is model +X.
    spine.translation = Keys<Vec3>{{1.0F}, {{0, 1, 1}}};
    clip.tracks.push_back(spine);
    const AnimationClip out = Retargeted(source, target, clip);
    INFO(Show(out.tracks[0].translation.values[0]));
    CHECK(Near(out.tracks[0].translation.values[0], {1, 1, 0}));
}

TEST_CASE("Retarget carries scale as a ratio to rest") {
    std::vector<Node> source = Chain();
    source[1].rest.scale = {2, 2, 2};
    std::vector<Node> target = Chain();
    target[1].rest.scale = {1, 3, 1};
    AnimationClip clip = ArmClip();
    clip.tracks[0].scale = Keys<Vec3>{{1.0F}, {{4, 2, 6}}};
    // Only the arm: the target arm's non-uniform rest is above the hand.
    const std::vector<JointBinding> armOnly{{1, 1}};
    const auto out = Model::Retarget(source, target, clip, armOnly);
    REQUIRE(out.has_value());
    CHECK(Near(out->tracks[0].scale.values[0], {2, 3, 3}));
}

TEST_CASE("Retarget drops tracks of unbound nodes and leaves unbound targets without a track") {
    std::vector<Node> source = Chain();
    AnimationClip clip = ArmClip();
    Track hand;
    hand.node = 2;
    hand.rotation = Keys<Quaternion>{{0.0F}, {AboutX(15)}};
    clip.tracks.push_back(hand);
    Track root;
    root.node = 0;
    root.translation = Keys<Vec3>{{0.0F}, {{0, 0, 1}}};
    clip.tracks.insert(clip.tracks.begin(), root);
    // The target has root and hand, in another order, and no arm.
    const std::vector<Node> target{MakeNode("root", Model::kNoParent), MakeNode("tip", 0), MakeNode("hand", 0)};
    const AnimationClip out = Retargeted(source, target, clip);
    REQUIRE(out.tracks.size() == 2);
    CHECK(out.tracks[0].node == 0);
    CHECK(out.tracks[1].node == 2);
    CHECK(out.nodeCount == 3);
    CHECK(Model::ValidateClip(target, out).has_value());
    // A binding the caller chose: only the hand.
    const std::vector<JointBinding> handOnly{{2, 2}};
    const auto chosen = Model::Retarget(source, target, clip, handOnly);
    REQUIRE(chosen.has_value());
    REQUIRE(chosen->tracks.size() == 1);
    CHECK(chosen->tracks[0].node == 2);
}

TEST_CASE("Retarget orders the tracks by the target's nodes") {
    // The target lists the chain the other way up.
    const std::vector<Node> target{MakeNode("hand", Model::kNoParent), MakeNode("arm", 0, {{0, 1, 0}, {}, {1, 1, 1}}),
                                   MakeNode("root", 1)};
    AnimationClip clip = ArmClip();
    Track root;
    root.node = 0;
    root.translation = Keys<Vec3>{{0.0F}, {{0, 0, 0}}};
    Track hand;
    hand.node = 2;
    hand.translation = Keys<Vec3>{{0.0F}, {{0, 1, 0}}};
    clip.tracks.insert(clip.tracks.begin(), root);
    clip.tracks.push_back(hand);
    const AnimationClip out = Retargeted(Chain(), target, clip);
    REQUIRE(out.tracks.size() == 3);
    CHECK(out.tracks[0].node == 0);
    CHECK(out.tracks[1].node == 1);
    CHECK(out.tracks[2].node == 2);
}

TEST_CASE("Retarget measures motion from the reference poses the caller gives") {
    // Rest poses equal, but the caller says the source's reference holds the
    // arm turned 30 degrees about X and 2 above the root, the target's turned
    // -45 degrees about Z: a key equal to the source reference gives the
    // target reference, and a turn over it carries over.
    const auto nodes = Chain();
    std::vector<LocalTransform> sourceReference(3), targetReference(3);
    Model::RestPose(nodes, sourceReference);
    Model::RestPose(nodes, targetReference);
    sourceReference[1] = {{0, 2, 0}, AboutX(30), {1, 1, 1}};
    targetReference[1] = {{0, 1, 0}, AboutZ(-45), {1, 1, 1}};
    AnimationClip clip = ArmClip();
    clip.tracks[0].rotation = Keys<Quaternion>{{0.0F, 2.0F}, {AboutX(30), Math::Multiply(AboutY(40), AboutX(30))}};
    clip.tracks[0].translation = Keys<Vec3>{{0.0F}, {{0, 2, 0}}};
    const std::vector<JointBinding> bindings{{1, 1}};
    const auto out = Model::Retarget(nodes, nodes, clip, bindings, {}, sourceReference, targetReference);
    REQUIRE(out.has_value());
    CHECK(SameRotation(out->tracks[0].rotation.values[0], AboutZ(-45)));
    CHECK(SameRotation(out->tracks[0].rotation.values[1], Math::Multiply(AboutY(40), AboutZ(-45))));
    CHECK(Near(out->tracks[0].translation.values[0], {0, 1, 0}));
    // Without them the rest is the reference, and the same keys move the arm.
    const auto byRest = Model::Retarget(nodes, nodes, clip, bindings);
    REQUIRE(byRest.has_value());
    CHECK(SameRotation(byRest->tracks[0].rotation.values[0], AboutX(30)));
    // A reference pose of another size is a Programmer Error.
    const std::vector<LocalTransform> wrong(2);
    GYO_CHECK_ASSERTS((void)Model::Retarget(nodes, nodes, clip, bindings, {}, wrong));
}

TEST_CASE("Retarget rule 1: BadTranslationScale") {
    const auto nodes = Chain();
    const std::vector<JointBinding> bindings{{1, 1}};
    CHECK(RetargetCodeOf(nodes, nodes, bindings, 0.0F) == RetargetCode::BadTranslationScale);
    CHECK(RetargetCodeOf(nodes, nodes, bindings, -1.0F) == RetargetCode::BadTranslationScale);
    CHECK(RetargetCodeOf(nodes, nodes, bindings, std::numeric_limits<float>::quiet_NaN()) ==
          RetargetCode::BadTranslationScale);
    CHECK(RetargetCodeOf(nodes, nodes, bindings, std::numeric_limits<float>::infinity()) ==
          RetargetCode::BadTranslationScale);
}

TEST_CASE("Retarget rule 2: BadBinding") {
    const auto nodes = Chain();
    CHECK(RetargetCodeOf(nodes, nodes, {{3, 1}}) == RetargetCode::BadBinding);
    CHECK(RetargetCodeOf(nodes, nodes, {{1, 3}}) == RetargetCode::BadBinding);
    CHECK(RetargetCodeOf(nodes, nodes, {{1, 1}, {1, 2}}) == RetargetCode::BadBinding);
    CHECK(RetargetCodeOf(nodes, nodes, {{1, 1}, {2, 1}}) == RetargetCode::BadBinding);
}

TEST_CASE("Retarget rule 3: BadRestScale") {
    SUBCASE("a non-uniform scale above a bound source node") {
        std::vector<Node> source = Chain();
        source[0].rest.scale = {1, 2, 1};
        CHECK(RetargetCodeOf(source, Chain(), {{2, 2}}) == RetargetCode::BadRestScale);
    }
    SUBCASE("a scale of 0 above a bound target node") {
        std::vector<Node> target = Chain();
        target[1].rest.scale = {0, 0, 0};
        CHECK(RetargetCodeOf(Chain(), target, {{2, 2}}) == RetargetCode::BadRestScale);
    }
    SUBCASE("a negative uniform scale above a bound node") {
        std::vector<Node> source = Chain();
        source[0].rest.scale = {-1, -1, -1};
        CHECK(RetargetCodeOf(source, Chain(), {{1, 1}}) == RetargetCode::BadRestScale);
    }
    SUBCASE("a bound source node's own scale with a component of 0") {
        std::vector<Node> source = Chain();
        source[1].rest.scale = {1, 0, 1};
        CHECK(RetargetCodeOf(source, Chain(), {{1, 1}}) == RetargetCode::BadRestScale);
    }
}

TEST_CASE("Retarget: a non-uniform scale on nodes that are not above a bound node is fine") {
    std::vector<Node> source = Chain();
    source[2].rest.scale = {1, 2, 3};
    std::vector<Node> target = Chain();
    target[1].rest.scale = {3, 1, 1};
    const std::vector<JointBinding> bindings{{1, 1}};
    CHECK(Model::Retarget(source, target, ArmClip(), bindings).has_value());
}

TEST_CASE("Retarget checks in order: translation scale then bindings then rest scales") {
    std::vector<Node> source = Chain();
    source[0].rest.scale = {1, 2, 1};
    CHECK(RetargetCodeOf(source, Chain(), {{9, 9}}, 0.0F) == RetargetCode::BadTranslationScale);
    CHECK(RetargetCodeOf(source, Chain(), {{1, 1}, {9, 9}}) == RetargetCode::BadBinding);
    CHECK(RetargetCodeOf(source, Chain(), {{1, 1}}) == RetargetCode::BadRestScale);
}

TEST_CASE("Retarget: a clip made for another node count is a Programmer Error") {
    std::vector<Node> source = Chain();
    source.push_back(MakeNode("extra", 0));
    const std::vector<JointBinding> bindings{{1, 1}};
    GYO_CHECK_ASSERTS((void)Model::Retarget(source, Chain(), ArmClip(), bindings));
}
