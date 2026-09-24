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

TEST_CASE("animation instances are explicitly clocked and independent") {
    auto model=std::make_shared<const ModelAsset>(MakeModel());
    AnimationInstance first(model),second(model);
    CHECK_FALSE(first.Advance(0.1));
    REQUIRE(first.Play(0,PlaybackMode::Clamp));
    REQUIRE(second.Play(0,PlaybackMode::Loop));
    auto interval=first.Advance(1.25);
    REQUIRE(interval);
    CHECK(interval.value().Crossed(1.0));
    CHECK_FALSE(interval.value().Crossed(1.5));
    CHECK(first.CurrentPose().localTransforms[1].translation.y==doctest::Approx(2.25));
    CHECK(second.CurrentPose().localTransforms[1].translation.y==1);
    REQUIRE(first.Advance(5));
    CHECK(first.IsFinished());
    CHECK(first.TimeSeconds()==2);
    interval=first.Advance(1);
    REQUIRE(interval);
    CHECK_FALSE(interval.value().Crossed(2));
    interval=second.Advance(4.25);
    REQUIRE(interval);
    CHECK(interval.value().Crossed(1));
    CHECK(interval.value().Crossed(0));
    CHECK(second.CurrentPose().localTransforms[1].translation.y==doctest::Approx(1.25));
    CHECK_FALSE(second.IsFinished());
    const auto pose=second.CurrentPose();
    CHECK_FALSE(second.Advance(-1));
    CHECK_FALSE(second.Advance(std::numeric_limits<double>::infinity()));
    CHECK(second.TimeSeconds()==4.25);
    CHECK(second.CurrentPose().localTransforms[1].translation.y==pose.localTransforms[1].translation.y);
}

TEST_CASE("interrupted transitions preserve the current pose and can be copied for event sampling") {
    auto model=std::make_shared<ModelAsset>(MakeModel());
    model->clips.push_back({"high",1,{{1,{{0,{0,10,0}}},{},{}}}});
    model->clips.push_back({"low",1,{{1,{{0,{0,-2,0}}},{},{}}}});
    AnimationInstance instance(model);
    REQUIRE(instance.Play(0,PlaybackMode::Clamp));
    REQUIRE(instance.Advance(1));
    REQUIRE(instance.Play(1,PlaybackMode::Clamp,1));
    CHECK(instance.CurrentPose().localTransforms[1].translation.y==2);
    REQUIRE(instance.Advance(0.25));
    CHECK(instance.CurrentPose().localTransforms[1].translation.y==4);
    REQUIRE(instance.Play(2,PlaybackMode::Clamp,0.5));
    CHECK(instance.CurrentPose().localTransforms[1].translation.y==4);
    auto eventSampler=instance;
    REQUIRE(eventSampler.Advance(0.25));
    CHECK(eventSampler.CurrentPose().localTransforms[1].translation.y==1);
    CHECK(instance.TimeSeconds()==0);
    CHECK(instance.CurrentPose().localTransforms[1].translation.y==4);
    REQUIRE(instance.Advance(0.5));
    CHECK(instance.CurrentPose().localTransforms[1].translation.y==-2);
    CHECK(TransformPoint(instance.CurrentPose().globalTransforms[1],{}).x==2);
}

TEST_CASE("pose blends retain hierarchy and normalize shortest-arc rotations") {
    const auto model=MakeModel();
    Pose from,to,output;
    REQUIRE(MakeDefaultPose(model,from));
    to=from;
    to.localTransforms[0].translation={4,0,0};
    const float a=std::sqrt(0.5F);
    from.localTransforms[1].rotation={0,0,a,a};
    to.localTransforms[1].rotation={0,0,-a,-a};
    REQUIRE(BlendPoses(model,from,to,0.5F,output));
    const auto point=TransformPoint(output.globalTransforms[1],{1,0,0});
    CHECK(point.x==doctest::Approx(3));
    CHECK(point.y==doctest::Approx(2));
    REQUIRE(BlendPoses(model,from,to,1,to));
    CHECK(to.localTransforms[0].translation.x==4);
    CHECK_FALSE(BlendPoses(model,from,to,-0.1F,output));
    to.localTransforms.clear();
    CHECK_FALSE(BlendPoses(model,from,to,0.5F,output));
}

TEST_CASE("playback intervals report an event once across exact and wrapped boundaries") {
    CHECK(PlaybackInterval{0,0.5,2,PlaybackMode::Clamp}.Crossed(0.5));
    CHECK_FALSE(PlaybackInterval{0.5,0.6,2,PlaybackMode::Clamp}.Crossed(0.5));
    CHECK(PlaybackInterval{1.8,2.2,2,PlaybackMode::Loop}.Crossed(0.1));
    CHECK_FALSE(PlaybackInterval{1.8,2.2,2,PlaybackMode::Loop}.Crossed(0.3));
    CHECK_FALSE(PlaybackInterval{0,1,2,PlaybackMode::Clamp}.Crossed(0));
    AnimationInstance empty;
    CHECK_FALSE(empty.Play(0,PlaybackMode::Loop));
    CHECK_FALSE(empty.Advance(0));
}

TEST_CASE("compatible animation transfer preserves proportions and converts rest frames and units") {
    const float half=std::sqrt(0.5F);
    ModelAsset source,target;
    source.nodes={{"source root",std::nullopt,{{},{},{2,2,2}}},
                  {"source joint",0,{{0,1,0},{0,half,0,half},{1,1,1}}},
                  {"source child",1,{{0,1,0},{},{1,1,1}}}};
    NodeTrack motion;
    motion.nodeIndex=1;
    motion.translations={{0,{0,1,0}},{2,{1,1,0}}};
    motion.rotations={{0,{0,half,0,half}},{2,{0.5F,0.5F,0.5F,0.5F}}};
    motion.scales={{0,{1,1,1}},{2,{2,2,2}}};
    source.clips.push_back({"motion",2,{motion}});
    target.nodes={{"unmapped root",std::nullopt,{{7,8,9},{},{1,1,1}}},
                  {"target root",std::nullopt,{{10,0,0},{0,0,half,half},{4,4,4}}},
                  {"unmapped child",1,{{0,0,7},{},{1,1,1}}},
                  {"target joint",1,{{2,0,0},{0.5F,0.5F,-0.5F,0.5F},{1,1,1}}},
                  {"target child",3,{{0,3,0},{},{1,1,1}}}};
    // Different names and reordered siblings require only the caller's indices.
    const std::array<AnimationNodeBinding,3> bindings{{{1,3},{0,1},{2,4}}};
    auto result=TransferCompatibleAnimation(source,target,0,bindings,1.5F);
    REQUIRE_MESSAGE(result,(result?"":result.error()));
    CHECK(result.value().name=="motion");
    CHECK(result.value().durationSeconds==2);
    REQUIRE(result.value().tracks.size()==1);
    CHECK(result.value().tracks[0].nodeIndex==3);
    target.clips.push_back(result.value());
    Pose rest,animated;
    REQUIRE(SamplePose(target,0,0,PlaybackMode::Clamp,rest));
    REQUIRE(SamplePose(target,0,2,PlaybackMode::Clamp,animated));
    // Reference keys preserve the target's own rest offsets and orientation.
    CHECK(rest.localTransforms[3].translation.x==2);
    CHECK(rest.localTransforms[3].translation.y==0);
    const auto restOrigin=TransformPoint(rest.globalTransforms[3],{});
    const auto restTip=TransformPoint(rest.globalTransforms[3],{1,0,0});
    CHECK(restTip.x-restOrigin.x==doctest::Approx(0).epsilon(1e-5));
    CHECK(restTip.y-restOrigin.y==doctest::Approx(0).epsilon(1e-5));
    CHECK(restTip.z-restOrigin.z==doctest::Approx(-4));
    // Source parent scale 2 / target parent scale 4 converts the requested
    // model-space distance multiplier back to the target's local coordinates.
    CHECK(animated.localTransforms[3].translation.x==doctest::Approx(2));
    CHECK(animated.localTransforms[3].translation.y==doctest::Approx(-0.75));
    const auto origin=TransformPoint(animated.globalTransforms[3],{});
    const auto tip=TransformPoint(animated.globalTransforms[3],{1,0,0});
    CHECK(origin.x-restOrigin.x==doctest::Approx(3));
    CHECK(origin.y-restOrigin.y==doctest::Approx(0).epsilon(1e-5));
    CHECK(tip.x-origin.x==doctest::Approx(0).epsilon(1e-5));
    CHECK(tip.y-origin.y==doctest::Approx(8));
    CHECK(tip.z-origin.z==doctest::Approx(0).epsilon(1e-5));
    CHECK(animated.localTransforms[4].translation.y==3);
    CHECK(animated.localTransforms[0].translation.y==8);
    CHECK(animated.localTransforms[2].translation.z==7);
    // Returned data is fully owning, and transfer did not change either input.
    CHECK(source.clips[0].tracks[0].translations[1].value.x==1);
    CHECK(source.nodes[1].localTransform.translation.y==1);
    CHECK(target.nodes[3].localTransform.translation.y==0);
    result.value().tracks[0].translations[1].value.x=999;
    CHECK(source.clips[0].tracks[0].translations[1].value.x==1);
    CHECK(target.clips[0].tracks[0].translations[1].value.x==doctest::Approx(2));
}

TEST_CASE("compatible transfer keeps absent channels and separate targets independent") {
    const auto source=MakeModel();
    auto first=source,second=source;
    first.clips.clear();second.clips.clear();
    first.nodes[1].localTransform.translation.y=4;
    second.nodes[1].localTransform.translation.y=8;
    const std::array<AnimationNodeBinding,2> bindings{{{0,0},{1,1}}};
    auto firstClip=TransferCompatibleAnimation(source,first,0,bindings,0.5F);
    auto secondClip=TransferCompatibleAnimation(source,second,0,bindings,2);
    REQUIRE(firstClip);REQUIRE(secondClip);
    CHECK(firstClip.value().tracks[0].rotations.empty());
    CHECK(firstClip.value().tracks[0].scales.empty());
    first.clips.push_back(std::move(firstClip).value());
    second.clips.push_back(std::move(secondClip).value());
    Pose a,b;
    REQUIRE(SamplePose(first,0,1,PlaybackMode::Clamp,a));
    REQUIRE(SamplePose(second,0,1,PlaybackMode::Clamp,b));
    CHECK(a.localTransforms[1].translation.y==doctest::Approx(4.5));
    CHECK(b.localTransforms[1].translation.y==doctest::Approx(10));
    CHECK(source.clips[0].tracks[0].translations[1].value.y==3);
}

TEST_CASE("compatible transfer validates complete one-to-one parent bindings") {
    const auto source=MakeModel();
    const auto target=source;
    const std::array<AnimationNodeBinding,2> valid{{{0,0},{1,1}}};
    CHECK_FALSE(TransferCompatibleAnimation(source,target,99,valid,1));
    CHECK_FALSE(TransferCompatibleAnimation(source,target,0,{},1));
    const std::array<AnimationNodeBinding,2> duplicateSource{{{0,0},{0,1}}};
    const std::array<AnimationNodeBinding,2> duplicateTarget{{{0,0},{1,0}}};
    const std::array<AnimationNodeBinding,2> outOfRange{{{0,0},{1,99}}};
    const std::array<AnimationNodeBinding,2> wrongParents{{{0,1},{1,0}}};
    const std::array<AnimationNodeBinding,1> missingParent{{{1,1}}};
    const std::array<AnimationNodeBinding,1> missingTrack{{{0,0}}};
    for(const auto& invalid:{duplicateSource,duplicateTarget,outOfRange,wrongParents})
        CHECK_FALSE(TransferCompatibleAnimation(source,target,0,invalid,1));
    CHECK_FALSE(TransferCompatibleAnimation(source,target,0,missingParent,1));
    const auto missing=TransferCompatibleAnimation(source,target,0,missingTrack,1);
    REQUIRE_FALSE(missing);
    CHECK(missing.error().find("no binding")!=std::string::npos);
    auto invalidHierarchy=target;
    invalidHierarchy.nodes[0].parentIndex=1;
    CHECK_FALSE(TransferCompatibleAnimation(source,invalidHierarchy,0,valid,1));
}

TEST_CASE("compatible transfer only excludes explicitly named non-skeletal mesh indices") {
    auto source=MakeModel(),target=source;
    source.nodes.push_back({"geometry",0,{}});
    source.meshes[0].nodeIndex=2;
    source.clips[0].tracks.push_back({2,{{0,{1,2,3}}},{},{}});
    const std::array<AnimationNodeBinding,2> bindings{{{0,0},{1,1}}};
    CHECK_FALSE(TransferCompatibleAnimation(source,target,0,bindings,1));
    const std::array<std::size_t,1> excluded{{2}};
    const auto valid=TransferCompatibleAnimation(source,target,0,bindings,1,excluded);
    REQUIRE(valid);
    REQUIRE(valid.value().tracks.size()==1);
    CHECK(valid.value().tracks[0].nodeIndex==1);
    const std::array<std::size_t,2> duplicate{{2,2}};
    CHECK_FALSE(TransferCompatibleAnimation(source,target,0,bindings,1,duplicate));
    for(const auto invalid:std::array<std::size_t,3>{0,1,99}) {
        const std::array<std::size_t,1> bad{{invalid}};
        CHECK_FALSE(TransferCompatibleAnimation(source,target,0,bindings,1,bad));
    }
    // A mesh node may itself be a skin joint; that is never discardable.
    source.meshes[0].joints.push_back({2,{}});
    CHECK_FALSE(TransferCompatibleAnimation(source,target,0,bindings,1,excluded));
}

TEST_CASE("compatible transfer rejects unsupported scales and non-finite inputs or output") {
    const auto source=MakeModel(),target=source;
    const std::array<AnimationNodeBinding,2> bindings{{{0,0},{1,1}}};
    for(const float invalid:{0.0F,-1.0F,std::numeric_limits<float>::infinity(),
                             std::numeric_limits<float>::quiet_NaN()})
        CHECK_FALSE(TransferCompatibleAnimation(source,target,0,bindings,invalid));
    for(const Vec3 invalid:{Vec3{1,2,1},Vec3{-1,-1,-1},Vec3{0,0,0},
                            Vec3{std::numeric_limits<float>::quiet_NaN(),1,1}}) {
        auto changedSource=source,changedTarget=target;
        changedSource.nodes[0].localTransform.scale=invalid;
        changedTarget.nodes[1].localTransform.scale=invalid;
        CHECK_FALSE(TransferCompatibleAnimation(changedSource,target,0,bindings,1));
        CHECK_FALSE(TransferCompatibleAnimation(source,changedTarget,0,bindings,1));
        changedSource=source;
        changedSource.clips[0].tracks[0].scales={{0,invalid}};
        CHECK_FALSE(TransferCompatibleAnimation(changedSource,target,0,bindings,1));
    }
    auto invalid=source;
    invalid.clips[0].tracks[0].translations[0].value.x=std::numeric_limits<float>::infinity();
    CHECK_FALSE(TransferCompatibleAnimation(invalid,target,0,bindings,1));
    invalid=source;
    invalid.clips[0].tracks[0].rotations={{0,{0,0,0,0}}};
    CHECK_FALSE(TransferCompatibleAnimation(invalid,target,0,bindings,1));
    invalid=source;
    invalid.clips[0].tracks[0].translations[0].timeSeconds=std::numeric_limits<double>::quiet_NaN();
    CHECK_FALSE(TransferCompatibleAnimation(invalid,target,0,bindings,1));
    invalid=source;
    invalid.clips[0].tracks[0].translations[1].value.y=std::numeric_limits<float>::max();
    CHECK_FALSE(TransferCompatibleAnimation(invalid,target,0,bindings,2));
}
