#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "model/Animation.hpp"

#include <cmath>
#include <limits>

using namespace Engine::Model;

namespace {
ModelAsset MakeModel() {
    ModelAsset model;
    model.nodes.push_back({"root",std::nullopt,{{2,0,0},{},{1,1,1}}});
    model.nodes.push_back({"joint",0,{{0,1,0},{},{1,1,1}}});
    model.materials.push_back({"diffuse"});
    MeshPart part;
    part.name="triangle";
    part.joints.push_back({1,{}});
    part.vertices={{{0,0,0},{0,1,0},{0,0},{0,0,0,0},{1,0,0,0}},
                   {{1,0,0},{0,1,0},{1,0},{0,0,0,0},{1,0,0,0}},
                   {{0,0,1},{0,1,0},{0,1},{0,0,0,0},{1,0,0,0}}};
    part.indices={0,1,2};
    model.meshes.push_back(part);
    AnimationClip clip;
    clip.name="move";clip.durationSeconds=2;
    NodeTrack track;track.nodeIndex=1;
    track.translations={{{0},{0,1,0}},{{2},{0,3,0}}};
    clip.tracks.push_back(track);
    model.clips.push_back(clip);
    return model;
}
}

TEST_CASE("owning model samples hierarchy and skins in model space") {
    auto model=MakeModel();
    REQUIRE(ValidateModel(model));
    CHECK(model.FindNode("joint")==1);
    CHECK(model.FindClip("move")==0);
    CHECK_FALSE(model.FindClip("missing"));
    Pose pose;
    REQUIRE(SamplePose(model,0,1,PlaybackMode::Clamp,pose));
    std::vector<SkinnedVertex> vertices;
    REQUIRE(SkinMesh(model,0,pose,vertices));
    CHECK(vertices[0].position.x==doctest::Approx(2));
    CHECK(vertices[0].position.y==doctest::Approx(2));
    CHECK(vertices[1].uv.x==1);
    CHECK(vertices[0].normal.y==doctest::Approx(1));
    const auto* storage=vertices.data();
    REQUIRE(SamplePose(model,0,3,PlaybackMode::Clamp,pose));
    REQUIRE(SkinMesh(model,0,pose,vertices));
    CHECK(vertices[0].position.y==doctest::Approx(3));
    CHECK(vertices.data()==storage);
    REQUIRE(SamplePose(model,0,3,PlaybackMode::Loop,pose));
    CHECK(pose.localTransforms[1].translation.y==doctest::Approx(2));
    REQUIRE(SamplePose(model,0,-1,PlaybackMode::Loop,pose));
    CHECK(pose.localTransforms[1].translation.y==doctest::Approx(2));
}

TEST_CASE("spherical rotation interpolation handles equivalent signed quaternions") {
    auto model=MakeModel();
    auto& track=model.clips[0].tracks[0];
    const float a=std::sqrt(0.5F);
    track.rotations={{0,{0,0,a,a}},{2,{0,0,-a,-a}}};
    Pose pose;
    REQUIRE(SamplePose(model,0,1,PlaybackMode::Clamp,pose));
    const Vec3 point=TransformPoint(ToMatrix(pose.localTransforms[1]),{1,0,0});
    CHECK(point.x==doctest::Approx(0).epsilon(1e-5));
    CHECK(point.y==doctest::Approx(3));
}

TEST_CASE("empty tracks and zero-duration clips retain defaults") {
    auto model=MakeModel();
    model.clips[0].durationSeconds=0;
    model.clips[0].tracks.clear();
    Pose pose;
    REQUIRE(SamplePose(model,0,100,PlaybackMode::Loop,pose));
    CHECK(pose.localTransforms[1].translation.y==1);
    REQUIRE(MakeDefaultPose(model,pose));
    CHECK(TransformPoint(pose.globalTransforms[1],{}).x==2);
}

TEST_CASE("rigid vertices preserve geometric transforms and normals") {
    auto model=MakeModel();
    auto& mesh=model.meshes[0];
    mesh.joints.clear();
    for(auto& vertex:mesh.vertices) vertex.weights={};
    mesh.geometryToNode=ToMatrix({{0,0,4},{},{2,3,4}});
    Pose pose;REQUIRE(MakeDefaultPose(model,pose));
    std::vector<SkinnedVertex> vertices;REQUIRE(SkinMesh(model,0,pose,vertices));
    CHECK(vertices[1].position.x==4);
    CHECK(vertices[1].position.z==4);
    CHECK(vertices[1].normal.y==doctest::Approx(1));
}

TEST_CASE("model materials retain renderer-neutral linear colors and reject non-finite components") {
    auto model=MakeModel();
    CHECK(model.materials[0].baseColorLinear==std::array<float,4>{1,1,1,1});
    model.materials[0].baseColorLinear={0.25F,0.5F,0.75F,0.4F};
    REQUIRE(ValidateModel(model));
    for(std::size_t component=0;component<4;++component) {
        for(const float invalid:{std::numeric_limits<float>::infinity(),
                                 std::numeric_limits<float>::quiet_NaN()}) {
            auto invalidModel=model;
            invalidModel.materials[0].baseColorLinear[component]=invalid;
            const auto validation=ValidateModel(invalidModel);
            REQUIRE_FALSE(validation);
            CHECK(validation.error().find("diffuse")!=std::string::npos);
            CHECK(validation.error().find("non-finite base color")!=std::string::npos);
        }
    }
}

TEST_CASE("invalid hierarchy, animation and skin data fail with diagnostics") {
    auto model=MakeModel();
    model.nodes[0].parentIndex=1;
    CHECK_FALSE(ValidateModel(model));
    model=MakeModel();model.meshes[0].vertices[0].joints[0]=99;
    CHECK_FALSE(ValidateModel(model));
    model=MakeModel();model.meshes[0].vertices[0].weights[0]=0.2F;
    CHECK_FALSE(ValidateModel(model));
    model=MakeModel();model.clips[0].tracks[0].translations[1].timeSeconds=0;
    CHECK_FALSE(ValidateModel(model));
    model=MakeModel();
    Pose pose;
    CHECK_FALSE(SamplePose(model,100,0,PlaybackMode::Clamp,pose));
    CHECK_FALSE(SamplePose(model,0,std::numeric_limits<double>::infinity(),PlaybackMode::Clamp,pose));
    std::vector<SkinnedVertex> vertices;
    CHECK_FALSE(SkinMesh(model,0,pose,vertices));
    model.clips[0].durationSeconds=-1;
    CHECK_FALSE(SamplePose(model,0,0,PlaybackMode::Clamp,pose));
    model=MakeModel();
    model.nodes[0].localTransform.rotation.x=std::numeric_limits<float>::max();
    CHECK_FALSE(ValidateModel(model));
}
