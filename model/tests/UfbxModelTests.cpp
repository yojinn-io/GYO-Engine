#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "model/backend/ufbx/UfbxModelLoader.hpp"
#include "model/Animation.hpp"
#include <ufbx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <memory>
#include <span>
#include <string_view>
#include <unordered_set>

using namespace Engine::Model;

namespace {
constexpr std::string_view kRigidTriangle = R"fbx(
; FBX 7.4.0 project file
FBXHeaderExtension: { FBXHeaderVersion: 1003 FBXVersion: 7400 }
GlobalSettings: {
    Version: 1000
    Properties70: {
        P: "UpAxis", "int", "Integer", "",1
        P: "UpAxisSign", "int", "Integer", "",1
        P: "FrontAxis", "int", "Integer", "",2
        P: "FrontAxisSign", "int", "Integer", "",1
        P: "CoordAxis", "int", "Integer", "",0
        P: "CoordAxisSign", "int", "Integer", "",1
        P: "UnitScaleFactor", "double", "Number", "",1
    }
}
Objects: {
    Geometry: 1, "Geometry::Triangle", "Mesh" {
        Vertices: *9 { a: 0,0,0,100,0,0,0,100,0 }
        PolygonVertexIndex: *3 { a: 0,1,-3 }
    }
    Model: 2, "Model::Triangle", "Mesh" {
        Version: 232
        Properties70: {
            P: "Lcl Translation", "Lcl Translation", "", "A",50,25,100
            P: "GeometricTranslation", "Vector3D", "Vector", "",0,0,10
        }
    }
}
Connections: { C: "OO",1,2 C: "OO",2,0 }
)fbx";

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

TEST_CASE("FBX errors are typed and file access is not required") {
    Ufbx::UfbxModelLoader loader;
    CHECK(loader.GetType()==Engine::Asset::AssetType::FromString("model"));
    CHECK_FALSE(loader.Load({},{}));
    const std::array<std::byte,8> invalid{};
    auto bad=loader.Load({invalid.data(),invalid.size()},{});
    REQUIRE_FALSE(bad);
    CHECK(bad.error().code==Engine::Asset::AssetErrorCode::DecodeFailed);
}

TEST_CASE("unskinned FBX without materials or UVs keeps rigid geometric transforms") {
    Ufbx::UfbxModelLoader loader;
    const auto bytes=std::as_bytes(std::span(kRigidTriangle.data(),kRigidTriangle.size()));
    auto loaded=loader.Load({bytes.data(),bytes.size()},{});
    if(!loaded) INFO(loaded.error().message);
    REQUIRE(loaded);
    const auto* model=loaded.value().As<ModelAsset>();REQUIRE(model);
    REQUIRE(model->meshes.size()==1);
    REQUIRE(model->materials.size()==1);
    CHECK(model->materials[0].name=="Default");
    CHECK(model->meshes[0].joints.empty());
    CHECK(model->clips.empty());
    Pose pose;REQUIRE(MakeDefaultPose(*model,pose));
    std::vector<SkinnedVertex> vertices;REQUIRE(SkinMesh(*model,0,pose,vertices));
    REQUIRE(vertices.size()==3);
    for(const auto& vertex:vertices) {
        CHECK(vertex.position.z==doctest::Approx(-1.1));
        CHECK(vertex.uv.x==0);
        CHECK(vertex.uv.y==0);
    }
    float minX=vertices[0].position.x,maxX=minX,minY=vertices[0].position.y,maxY=minY;
    for(const auto& v:vertices) {
        minX=(std::min)(minX,v.position.x);maxX=(std::max)(maxX,v.position.x);
        minY=(std::min)(minY,v.position.y);maxY=(std::max)(maxY,v.position.y);
    }
    CHECK(minX==doctest::Approx(0.5));CHECK(maxX==doctest::Approx(1.5));
    CHECK(minY==doctest::Approx(0.25));CHECK(maxY==doctest::Approx(1.25));
}

TEST_CASE("out of range FBX indices and truncated binary input fail cleanly") {
    Ufbx::UfbxModelLoader loader;
    std::string invalid(kRigidTriangle);
    constexpr std::string_view indices="a: 0,1,-3";
    invalid.replace(invalid.find(indices),indices.size(),"a: 0,99,-3");
    auto bytes=std::as_bytes(std::span(invalid.data(),invalid.size()));
    CHECK_FALSE(loader.Load({bytes.data(),bytes.size()},{}));
    const auto binary=ReadModel();
    for(std::size_t size:{std::size_t{28},std::size_t{128},std::size_t{1024}})
        CHECK_FALSE(loader.Load({binary.data(),size},{}));
}

TEST_CASE("deep FBX hierarchies import and evaluate without recursive stack growth") {
    std::string source(kRigidTriangle),nodes,connections="Connections: {\n C: \"OO\",1,2\n C: \"OO\",2,0\n";
    constexpr int depth=3000;
    for(int i=0;i<depth;++i) {
        const std::string id=std::to_string(i+3);
        nodes+="\n Model: "+id+", \"Model::Nested"+std::to_string(i)+"\", \"Null\" { Version: 232 }\n";
        connections+=" C: \"OO\","+id+","+std::to_string(i+2)+"\n";
    }
    connections+="}\n";
    source.insert(source.find("\n}\nConnections:"),nodes);
    const auto connectionStart=source.find("Connections:");
    source.replace(connectionStart,source.size()-connectionStart,connections);
    Ufbx::UfbxModelLoader loader;
    auto bytes=std::as_bytes(std::span(source.data(),source.size()));
    auto loaded=loader.Load({bytes.data(),bytes.size()},{});
    REQUIRE(loaded);
    const auto* model=loaded.value().As<ModelAsset>();REQUIRE(model);
    CHECK(model->FindNode("Nested2999").has_value());
    Pose pose;CHECK(MakeDefaultPose(*model,pose));
}

TEST_CASE("already sampled equivalent Euler rotations do not create intermediate spins") {
    std::string source(kRigidTriangle);
    const std::string animation=R"fbx(
    AnimationStack: 10, "AnimStack::EulerAliases", "" {
        Properties70: {
            P: "LocalStart", "KTime", "Time", "",0
            P: "LocalStop", "KTime", "Time", "",3079077200
        }
    }
    AnimationLayer: 11, "AnimLayer::BaseLayer", "" {}
    AnimationCurveNode: 12, "AnimCurveNode::R", "" {
        Properties70: {
            P: "d|X", "Number", "", "A",10
            P: "d|Y", "Number", "", "A",20
            P: "d|Z", "Number", "", "A",30
        }
    }
)fbx";
    std::string curves;
    const std::array<std::string_view,3> values{"10,190,10","20,160,20","30,210,30"};
    for(std::size_t axis=0;axis<3;++axis) {
        curves+="\n AnimationCurve: "+std::to_string(13+axis)+", \"AnimCurve::\", \"\" {\n"
            " Default: 0 KeyVer: 4008\n"
            " KeyTime: *3 { a: 0,1539538600,3079077200 }\n"
            " KeyValueFloat: *3 { a: "+std::string(values[axis])+" }\n"
            " KeyAttrFlags: *1 { a: 4 }\n"
            " KeyAttrDataFloat: *4 { a: 0,0,0,0 }\n"
            " KeyAttrRefCount: *1 { a: 3 }\n }\n";
    }
    source.insert(source.find("\n}\nConnections:"),animation+curves);
    source.insert(source.rfind('}'),
        " C: \"OO\",11,10 C: \"OO\",12,11 C: \"OP\",12,2,\"Lcl Rotation\""
        " C: \"OP\",13,12,\"d|X\" C: \"OP\",14,12,\"d|Y\" C: \"OP\",15,12,\"d|Z\"\n");
    const auto input=std::as_bytes(std::span(source.data(),source.size()));
    const std::vector<std::byte> bytes(input.begin(),input.end());
    const auto model=LoadModel(bytes);
    const auto clip=model->FindClip("EulerAliases");REQUIRE(clip);
    CHECK(model->clips[*clip].durationSeconds==doctest::Approx(2.0/30));
    Pose pose;REQUIRE(SamplePose(*model,*clip,0,PlaybackMode::Clamp,pose));
    std::vector<SkinnedVertex> start,intermediate;
    REQUIRE(SkinMesh(*model,0,pose,start));
    // (10,20,30) and (190,160,210) are the same XYZ orientation. Neither
    // native samples nor in-between poses may rotate the rigid triangle.
    for(int sample=1;sample<=16;++sample) {
        REQUIRE(SamplePose(*model,*clip,sample/240.0,PlaybackMode::Clamp,pose));
        REQUIRE(SkinMesh(*model,0,pose,intermediate));
        for(std::size_t i=0;i<start.size();++i) {
            CHECK(intermediate[i].position.x==doctest::Approx(start[i].position.x).epsilon(1e-5));
            CHECK(intermediate[i].position.y==doctest::Approx(start[i].position.y).epsilon(1e-5));
            CHECK(intermediate[i].position.z==doctest::Approx(start[i].position.z).epsilon(1e-5));
        }
    }
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
