#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "model/backend/ufbx/UfbxModelLoader.hpp"
#include "model/Animation.hpp"
#include <ufbx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace Engine::Model;

namespace {
std::vector<std::byte> ReadModel() {
    std::ifstream input(GYO_MARK23_FBX,std::ios::binary|std::ios::ate);
    if(!input) throw std::runtime_error("Mark23 regression fixture cannot be opened");
    std::vector<std::byte> bytes(static_cast<std::size_t>(input.tellg()));
    input.seekg(0);input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    return bytes;
}

std::shared_ptr<ModelAsset> LoadModel(const std::vector<std::byte>& bytes) {
    Ufbx::UfbxModelLoader loader;
    auto result=loader.Load({bytes.data(),bytes.size()},{});
    if(!result) throw std::runtime_error(result.error().message);
    return result.value().ShareAs<ModelAsset>();
}

using Scene=std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)>;

Scene ReferenceScene(const std::vector<std::byte>& bytes) {
    ufbx_load_opts options{};
    options.target_axes=ufbx_axes_left_handed_y_up;
    options.handedness_conversion_axis=UFBX_MIRROR_AXIS_Z;
    options.target_unit_meters=1.0;
    options.space_conversion=UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
    options.inherit_mode_handling=UFBX_INHERIT_MODE_HANDLING_HELPER_NODES;
    options.generate_missing_normals=true;
    ufbx_error error{};
    Scene result(ufbx_load_memory(bytes.data(),bytes.size(),&options,&error),ufbx_free_scene);
    if(!result) throw std::runtime_error("ufbx reference load failed");
    return result;
}

std::vector<std::uint32_t> OrderedCorners(const ufbx_mesh* mesh,const ufbx_mesh_part& part) {
    std::vector<std::uint32_t> corners,triangles;
    std::unordered_set<std::uint32_t> seen;
    for(auto faceIndex:part.face_indices) {
        auto face=mesh->faces[faceIndex];
        if(face.num_indices<3) continue;
        triangles.resize((face.num_indices-2)*3);
        const auto count=ufbx_triangulate_face(triangles.data(),triangles.size(),mesh,face);
        for(std::size_t i=0;i<count*3;++i) if(seen.insert(triangles[i]).second) corners.push_back(triangles[i]);
    }
    return corners;
}
}

TEST_CASE("truncated Mark23 binary input fails cleanly") {
    Ufbx::UfbxModelLoader loader;
    const auto binary=ReadModel();
    REQUIRE(binary.size()>=1024);
    for(std::size_t size:{std::size_t{28},std::size_t{128},std::size_t{1024}})
        CHECK_FALSE(loader.Load({binary.data(),size},{}));
}

TEST_CASE("Mark23 retains five clips, material splits and normalized four-weight skinning") {
    const auto bytes=ReadModel();
    const auto model=LoadModel(bytes);
    REQUIRE(model);
    REQUIRE(ValidateModel(*model));
    REQUIRE(model->meshes.size()==5);
    REQUIRE(model->materials.size()==3);
    REQUIRE(model->clips.size()==5);
    const std::array<std::pair<const char*,double>,5> expected{{
        {"Reload",112.0/30},{"Shoot",10.0/30},{"Hide",11.0/30},{"Draw",25.0/30},{"Idle",1.0/30}}};
    for(const auto& [name,duration]:expected) {
        const auto clip=model->FindClip(name);REQUIRE(clip);
        CHECK(model->clips[*clip].durationSeconds==doctest::Approx(duration).epsilon(1e-8));
    }
    std::size_t triangles=0;
    for(const auto& mesh:model->meshes) {
        triangles+=mesh.indices.size()/3;
        CHECK_FALSE(mesh.joints.empty());
        for(const auto& vertex:mesh.vertices) {
            float weight=0;for(float w:vertex.weights) weight+=w;
            CHECK(weight==doctest::Approx(1).epsilon(1e-5));
        }
    }
    CHECK(triangles==7762);
    Pose idle;REQUIRE(SamplePose(*model,*model->FindClip("Idle"),0,PlaybackMode::Clamp,idle));
    const auto pivot=model->FindNode("main_j");REQUIRE(pivot);
    const auto position=TransformPoint(idle.globalTransforms[*pivot],{});
    CHECK(std::abs(position.x)==doctest::Approx(0.111353).epsilon(1e-4));
    CHECK(position.y==doctest::Approx(1.429741).epsilon(1e-4));
}

TEST_CASE("Mark23 preserves tiled UV coordinates for repeat sampling") {
    const auto bytes=ReadModel();
    const auto model=LoadModel(bytes);
    auto reference=ReferenceScene(bytes);
    bool hasNegativeV=false,hasUAboveOne=false;
    for(const auto& mesh:model->meshes) {
        CAPTURE(mesh.name);
        const auto nodeName=mesh.name.substr(0,mesh.name.find('/'));
        const auto* source=ufbx_find_node(reference.get(),nodeName.c_str());REQUIRE(source);
        const auto* geometry=source->mesh;REQUIRE(geometry);
        const auto& materialName=model->materials[mesh.materialIndex].name;
        std::size_t slot=0;
        while(slot<source->materials.count&&materialName!=source->materials[slot]->name.data) ++slot;
        REQUIRE(slot<source->materials.count);
        const auto* diffuse=source->materials[slot]->fbx.diffuse_color.texture;REQUIRE(diffuse);
        CHECK(diffuse->wrap_u==UFBX_WRAP_REPEAT);
        CHECK(diffuse->wrap_v==UFBX_WRAP_REPEAT);
        CHECK_FALSE(diffuse->has_uv_transform);
        const auto corners=OrderedCorners(geometry,geometry->material_parts[slot]);
        REQUIRE(corners.size()==mesh.vertices.size());
        double maximumError=0;
        float maxV=mesh.vertices.front().uv.y;
        for(std::size_t i=0;i<mesh.vertices.size();++i) {
            const auto expected=ufbx_get_vertex_vec2(&geometry->vertex_uv,corners[i]);
            const auto actual=mesh.vertices[i].uv;
            maximumError=(std::max)(maximumError,std::abs(actual.x-expected.x));
            maximumError=(std::max)(maximumError,std::abs(actual.y-(1-expected.y)));
            hasNegativeV|=actual.y<0;
            hasUAboveOne|=actual.x>1;
            maxV=(std::max)(maxV,actual.y);
        }
        CHECK(maximumError<1e-6);
        // Every gun V is outside the first tile. Clamp would sample only the
        // atlas edge, so this asset also verifies that negative UVs survive.
        if(materialName=="Mark23_D") CHECK(maxV<0);
    }
    CHECK(hasNegativeV);
    CHECK(hasUAboveOne);
}

TEST_CASE("baked neutral sampling and CPU skinning retain every native FBX frame") {
    const auto bytes=ReadModel();
    const auto model=LoadModel(bytes);
    auto reference=ReferenceScene(bytes);
    Pose pose;
    std::vector<SkinnedVertex> output;
    Vec3 reloadMagazineStart{},reloadMagazineMiddle{};
    for(std::size_t clipIndex=0;clipIndex<model->clips.size();++clipIndex) {
        const auto& clip=model->clips[clipIndex];
        CAPTURE(clip.name);
        const auto* stack=ufbx_find_anim_stack(reference.get(),clip.name.c_str());REQUIRE(stack);
        const auto nativeFrames=static_cast<std::size_t>(std::llround(clip.durationSeconds*30));
        for(std::size_t frame=0;frame<=nativeFrames;++frame) {
            const double fraction=nativeFrames?static_cast<double>(frame)/nativeFrames:0;
            CAPTURE(fraction);
            const double seconds=clip.durationSeconds*fraction;
            REQUIRE(SamplePose(*model,clipIndex,seconds,PlaybackMode::Clamp,pose));
            ufbx_evaluate_opts options{};options.evaluate_skinning=true;
            ufbx_error error{};
            Scene evaluated(ufbx_evaluate_scene(reference.get(),stack->anim,stack->time_begin+seconds,&options,&error),ufbx_free_scene);
            REQUIRE(evaluated);
            for(std::size_t meshIndex=0;meshIndex<model->meshes.size();++meshIndex) {
                const auto& mesh=model->meshes[meshIndex];CAPTURE(mesh.name);
                const auto slash=mesh.name.find('/');
                const auto nodeName=mesh.name.substr(0,slash);
                const auto* source=ufbx_find_node(evaluated.get(),nodeName.c_str());REQUIRE(source);
                const auto* geometry=source->mesh;REQUIRE(geometry);
                const auto materialName=model->materials[mesh.materialIndex].name;
                std::size_t slot=0;
                while(slot<source->materials.count&&materialName!=source->materials[slot]->name.data) ++slot;
                REQUIRE(slot<source->materials.count);
                const auto corners=OrderedCorners(geometry,geometry->material_parts[slot]);
                REQUIRE(corners.size()==mesh.vertices.size());
                REQUIRE(SkinMesh(*model,meshIndex,pose,output));
                double maximumError=0;
                for(std::size_t i=0;i<output.size();++i) {
                    auto expected=ufbx_get_vertex_vec3(&geometry->skinned_position,corners[i]);
                    if(geometry->skinned_is_local) expected=ufbx_transform_position(&source->geometry_to_world,expected);
                    const auto actual=output[i].position;
                    const double dx=actual.x-expected.x,dy=actual.y-expected.y,dz=actual.z-expected.z;
                    maximumError=(std::max)(maximumError,std::sqrt(dx*dx+dy*dy+dz*dz));
                    const auto uv=ufbx_get_vertex_vec2(&geometry->vertex_uv,corners[i]);
                    if(i==0) {
                        CHECK(output[i].uv.x==doctest::Approx(uv.x));
                        CHECK(output[i].uv.y==doctest::Approx(1-uv.y));
                    }
                }
                CAPTURE(maximumError);
                CHECK(maximumError<0.00001); // within 0.01 mm at every authored 30 Hz frame
                if(clip.name=="Reload"&&nodeName=="mag") {
                    if(fraction==0) reloadMagazineStart=output[0].position;
                    if(fraction==0.5) reloadMagazineMiddle=output[0].position;
                }
            }
        }
    }
    const auto a=reloadMagazineStart,b=reloadMagazineMiddle;
    CHECK(std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z)>0.05F);
}

TEST_CASE("Reload wrist subframes follow native quaternion poses through Euler branch changes") {
    const auto bytes=ReadModel();
    const auto model=LoadModel(bytes);
    auto reference=ReferenceScene(bytes);
    const auto clip=model->FindClip("Reload");REQUIRE(clip);
    const auto* stack=ufbx_find_anim_stack(reference.get(),"Reload");REQUIRE(stack);
    Pose pose;
    for(const char* name:{"LeftHand1","RightHand1"}) {
        CAPTURE(name);
        const auto node=model->FindNode(name);REQUIRE(node);
        const auto* source=ufbx_find_node(reference.get(),name);REQUIRE(source);
        // Cover the complete clip, including the visible 1.4/1.5 s and
        // 2.6-2.7 s wrist spins caused by inserting Euler-interpolated keys.
        for(int frame=0;frame<112;++frame) {
            const auto a=ufbx_evaluate_transform(stack->anim,source,stack->time_begin+frame/30.0);
            const auto b=ufbx_evaluate_transform(stack->anim,source,stack->time_begin+(frame+1)/30.0);
            for(double fraction:{0.25,0.5,0.75}) {
                const double time=(frame+fraction)/30;
                CAPTURE(time);
                REQUIRE(SamplePose(*model,*clip,time,PlaybackMode::Clamp,pose));
                const auto expected=ufbx_quat_slerp(a.rotation,b.rotation,fraction);
                const auto actual=pose.localTransforms[*node].rotation;
                const double dot=actual.x*expected.x+actual.y*expected.y+
                                 actual.z*expected.z+actual.w*expected.w;
                CHECK(std::abs(dot)>1-1e-6);
            }
        }
    }
}
