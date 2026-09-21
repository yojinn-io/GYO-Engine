#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "model/backend/ufbx/UfbxModelLoader.hpp"
#include "model/Animation.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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

std::vector<std::byte> MaterialTriangle(
    const std::string_view properties,const std::string_view templateProperties={}) {
    std::string source(kRigidTriangle);
    if(!templateProperties.empty()) {
        source.insert(source.find("Objects:"),
            "Definitions: { Version: 100 Count: 1\n"
            " ObjectType: \"Material\" { Count: 1\n"
            "  PropertyTemplate: \"FbxSurfaceLambert\" { Properties70: {\n"+
            std::string(templateProperties)+"\n } } } }\n");
    }
    source.insert(source.find("\n}\nConnections:"),
        "\n Material: 3, \"Material::Surface\", \"\" {\n"
        "  Version: 102 ShadingModel: \"lambert\" MultiLayer: 0\n"
        "  Properties70: {\n"+std::string(properties)+"\n }\n }\n");
    source.insert(source.rfind('}')," C: \"OO\",3,2\n");
    const auto bytes=std::as_bytes(std::span(source.data(),source.size()));
    return {bytes.begin(),bytes.end()};
}

std::shared_ptr<ModelAsset> LoadModel(const std::vector<std::byte>& bytes) {
    Ufbx::UfbxModelLoader loader;
    auto result=loader.Load({bytes.data(),bytes.size()},{});
    if(!result) throw std::runtime_error(result.error().message);
    return result.value().ShareAs<ModelAsset>();
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
    CHECK(model->materials[0].baseColorLinear==std::array<float,4>{1,1,1,1});
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

TEST_CASE("FBX diffuse colors preserve linear RGB and apply authored factors once") {
    const auto bytes=MaterialTriangle(
        "P: \"DiffuseColor\", \"Color\", \"\", \"A\",0.75,0.5,0.25\n"
        "P: \"DiffuseFactor\", \"Number\", \"\", \"A\",0.8");
    const auto model=LoadModel(bytes);
    REQUIRE(model->materials.size()==1);
    const auto& color=model->materials[0].baseColorLinear;
    CHECK(color[0]==doctest::Approx(0.6));
    CHECK(color[1]==doctest::Approx(0.4));
    CHECK(color[2]==doctest::Approx(0.2));
    CHECK(color[3]==1);
}

TEST_CASE("FBX material templates provide defaults and local factors override them") {
    constexpr std::string_view defaults=
        "P: \"DiffuseColor\", \"Color\", \"\", \"A\",0.75,0.5,0.25\n"
        "P: \"DiffuseFactor\", \"Number\", \"\", \"A\",0.8";
    auto model=LoadModel(MaterialTriangle({},defaults));
    REQUIRE(model->materials.size()==1);
    CHECK(model->materials[0].baseColorLinear[0]==doctest::Approx(0.6));
    model=LoadModel(MaterialTriangle(
        "P: \"DiffuseFactor\", \"Number\", \"\", \"A\",0.4",defaults));
    CHECK(model->materials[0].baseColorLinear[0]==doctest::Approx(0.3));
}

TEST_CASE("FBX missing diffuse color stays white and missing factor preserves color") {
    for(const auto properties:{std::string_view{},std::string_view{
        "P: \"DiffuseFactor\", \"Number\", \"\", \"A\",0.8"}}) {
        const auto model=LoadModel(MaterialTriangle(properties));
        REQUIRE(model->materials.size()==1);
        CHECK(model->materials[0].baseColorLinear==std::array<float,4>{1,1,1,1});
    }
    const auto model=LoadModel(MaterialTriangle(
        "P: \"DiffuseColor\", \"Color\", \"\", \"A\",0.75,0.5,0.25"));
    CHECK(model->materials[0].baseColorLinear==std::array<float,4>{0.75F,0.5F,0.25F,1});
    const auto black=LoadModel(MaterialTriangle(
        "P: \"DiffuseColor\", \"Color\", \"\", \"A\",0,0,0"));
    CHECK(black->materials[0].baseColorLinear==std::array<float,4>{0,0,0,1});
}

TEST_CASE("FBX four-component diffuse color retains straight alpha independently of factor") {
    const auto model=LoadModel(MaterialTriangle(
        "P: \"DiffuseColor\", \"Color\", \"\", \"A\",0.75,0.5,0.25,0.4\n"
        "P: \"DiffuseFactor\", \"Number\", \"\", \"A\",0.8"));
    const auto& color=model->materials[0].baseColorLinear;
    CHECK(color[0]==doctest::Approx(0.6));
    CHECK(color[3]==doctest::Approx(0.4));
}

TEST_CASE("FBX diffuse values outside finite float storage fail with a material diagnostic") {
    const auto bytes=MaterialTriangle(
        "P: \"DiffuseColor\", \"Color\", \"\", \"A\",1e39,0.5,0.25");
    Ufbx::UfbxModelLoader loader;
    const auto loaded=loader.Load({bytes.data(),bytes.size()},{});
    REQUIRE_FALSE(loaded);
    CHECK(loaded.error().message.find("Surface")!=std::string::npos);
    CHECK(loaded.error().message.find("non-finite base color")!=std::string::npos);
}

TEST_CASE("out of range FBX indices fail cleanly") {
    Ufbx::UfbxModelLoader loader;
    std::string invalid(kRigidTriangle);
    constexpr std::string_view indices="a: 0,1,-3";
    invalid.replace(invalid.find(indices),indices.size(),"a: 0,99,-3");
    auto bytes=std::as_bytes(std::span(invalid.data(),invalid.size()));
    CHECK_FALSE(loader.Load({bytes.data(),bytes.size()},{}));
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
