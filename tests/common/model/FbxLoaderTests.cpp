#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "engine/asset/AssetSnapshot.hpp"
#include "engine/asset/AssetStore.hpp"
#include "engine/model/Animation.hpp"
#include "engine/model/FbxLoader.hpp"
#include "engine/model/ModelData.hpp"

// The "model"/"fbx" Data Contract (model.zh-Hant.md section 3): the settings,
// the rules and their order, the contract example contract/skinned_bar.fbx
// (written by Blender from contract/make_skinned_bar.py: its scene is
// described there), determinism (AS-3) and the way through the asset store.
// Offline: no device, no SDL.

using namespace GYO::Engine;
using Math::Vec3;
using Model::FbxLoader;
using Model::FbxLoaderCode;
using Model::FbxSettings;
using Model::LocalTransform;
using Model::ModelData;
using Model::Wrap;

namespace {

using Bytes = std::vector<std::byte>;

constexpr float kPi = 3.14159265358979323846F;

Bytes ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    const std::vector<char> chars((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    Bytes bytes(chars.size());
    std::transform(chars.begin(), chars.end(), bytes.begin(), [](const char c) { return static_cast<std::byte>(c); });
    return bytes;
}

const Bytes& Bar() {
    static const Bytes bytes = ReadFile(std::filesystem::path(GYO_MODEL_CONTRACT_DIR) / "skinned_bar.fbx");
    return bytes;
}

Base::Result<ModelData, Asset::LoaderFailure<FbxLoaderCode>> Load(const Bytes& bytes, const FbxSettings& settings = {}) {
    const FbxLoader loader;
    Asset::LoadContext context(Asset::AssetId("test.model"), "models/test.fbx", bytes.size());
    return loader.Load(bytes, settings, context);
}

FbxLoaderCode CodeOf(const Bytes& bytes) {
    const auto result = Load(bytes);
    REQUIRE_FALSE(result.has_value());
    return result.error().code;
}

Bytes Text(const std::string& text) {
    Bytes bytes(text.size());
    std::transform(text.begin(), text.end(), bytes.begin(), [](const char c) { return static_cast<std::byte>(c); });
    return bytes;
}

bool Near(const float a, const float b, const float epsilon = 1.0e-4F) { return std::fabs(a - b) <= epsilon; }

std::string Show(const Vec3 v) {
    return "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
}

std::uint32_t NodeNamed(const ModelData& model, const std::string& name) {
    const auto index = model.FindNode(name);
    INFO("node " << name);
    REQUIRE(index.has_value());
    return *index;
}

// The vertex positions of a skinned mesh in model space for a pose
// (model.zh-Hant.md section 2: the sum of weight * global * inverse bind).
std::vector<Vec3> Skinned(const ModelData& model, const Model::Mesh& mesh, const std::vector<LocalTransform>& pose) {
    std::vector<Math::Matrix4> global(model.nodes.size());
    Model::ComputeModelSpace(model.nodes, pose, global);
    std::vector<Vec3> out;
    for (std::size_t v = 0; v < mesh.positions.size(); ++v) {
        Vec3 sum{};
        for (std::size_t k = 0; k < 4; ++k) {
            const float w = mesh.vertexWeights[v][k];
            if (w == 0.0F) continue;
            const std::uint16_t j = mesh.vertexJoints[v][k];
            const Model::Skin& skin = model.skins[mesh.skin];
            const Math::Matrix4 m = Math::Multiply(global[skin.joints[j]], skin.inverseBindMatrices[j]);
            sum = sum + Math::TransformPoint(m, mesh.positions[v]) * w;
        }
        out.push_back(sum);
    }
    return out;
}

const Model::Mesh& MeshNamed(const ModelData& model, const std::string& name) {
    const auto found = std::find_if(model.meshes.begin(), model.meshes.end(),
                                    [&](const Model::Mesh& mesh) { return mesh.name == name; });
    INFO("mesh " << name);
    REQUIRE(found != model.meshes.end());
    return *found;
}

std::vector<LocalTransform> Rest(const ModelData& model) {
    std::vector<LocalTransform> pose(model.nodes.size());
    Model::RestPose(model.nodes, pose);
    return pose;
}

// Every float of the model, and every name and index, in order.
void Append(std::vector<std::uint32_t>& out, const float v) { out.push_back(std::bit_cast<std::uint32_t>(v)); }
void Append(std::vector<std::uint32_t>& out, const Vec3 v) {
    Append(out, v.x);
    Append(out, v.y);
    Append(out, v.z);
}
void Append(std::vector<std::uint32_t>& out, const Math::Quaternion q) {
    Append(out, q.x);
    Append(out, q.y);
    Append(out, q.z);
    Append(out, q.w);
}
void Append(std::vector<std::uint32_t>& out, const std::string& s) {
    out.push_back(static_cast<std::uint32_t>(s.size()));
    for (const char c : s) out.push_back(static_cast<unsigned char>(c));
}
void Append(std::vector<std::uint32_t>& out, const LocalTransform& t) {
    Append(out, t.translation);
    Append(out, t.rotation);
    Append(out, t.scale);
}
template <class T>
void Append(std::vector<std::uint32_t>& out, const Model::Keys<T>& keys) {
    out.push_back(static_cast<std::uint32_t>(keys.times.size()));
    for (const float t : keys.times) Append(out, t);
    for (const T& v : keys.values) Append(out, v);
}
std::vector<std::uint32_t> NodesAndClips(const ModelData& m) {
    std::vector<std::uint32_t> out;
    for (const auto& n : m.nodes) {
        Append(out, n.name);
        out.push_back(n.parent);
        Append(out, n.rest);
    }
    for (const auto& material : m.materials) {
        Append(out, material.name);
        for (const float c : material.baseColor) Append(out, c);
        Append(out, material.baseColorTexture);
    }
    for (const auto& c : m.clips) {
        Append(out, c.name);
        Append(out, c.duration);
        out.push_back(c.nodeCount);
        for (const auto& t : c.tracks) {
            out.push_back(t.node);
            Append(out, t.translation);
            Append(out, t.rotation);
            Append(out, t.scale);
        }
    }
    return out;
}
std::vector<std::uint32_t> Bits(const ModelData& m) {
    std::vector<std::uint32_t> out = NodesAndClips(m);
    for (const auto& skin : m.skins) {
        out.insert(out.end(), skin.joints.begin(), skin.joints.end());
        for (const auto& m4 : skin.inverseBindMatrices) {
            for (const float f : m4.values) Append(out, f);
        }
    }
    for (const auto& mesh : m.meshes) {
        Append(out, mesh.name);
        out.push_back(mesh.node);
        out.push_back(mesh.material);
        for (const Vec3 p : mesh.positions) Append(out, p);
        for (const Vec3 n : mesh.normals) Append(out, n);
        for (const Math::Vec2 uv : mesh.uvs) {
            Append(out, uv.x);
            Append(out, uv.y);
        }
        out.insert(out.end(), mesh.indices.begin(), mesh.indices.end());
        out.push_back(mesh.skin);
        for (const auto& j : mesh.vertexJoints) out.insert(out.end(), j.begin(), j.end());
        for (const auto& w : mesh.vertexWeights) {
            for (const float f : w) Append(out, f);
        }
    }
    return out;
}

} // namespace

TEST_CASE("FbxLoader settings: meshes and nothing else") {
    const FbxLoader loader;
    const auto none = loader.ParseSettings(std::nullopt);
    REQUIRE(none.has_value());
    CHECK(none->meshes);
    const auto off = loader.ParseSettings(R"({"meshes":false})");
    REQUIRE(off.has_value());
    CHECK_FALSE(off->meshes);
    const auto on = loader.ParseSettings(R"({"meshes":true})");
    REQUIRE(on.has_value());
    CHECK(on->meshes);
    const auto empty = loader.ParseSettings("{}");
    REQUIRE(empty.has_value());
    CHECK(empty->meshes);
    for (const std::string bad : {R"({"meshes":1})", R"({"meshes":"false"})", R"({"textures":true})", "[]", "not json"}) {
        CAPTURE(bad);
        const auto result = loader.ParseSettings(bad);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == FbxLoaderCode::BadSettings);
    }
}

TEST_CASE("FbxLoader rule 1: NotFbx") {
    CHECK(CodeOf({}) == FbxLoaderCode::NotFbx);
    CHECK(CodeOf(Text("hello, this is not a model")) == FbxLoaderCode::NotFbx);
    CHECK(CodeOf(Text("\x89PNG\r\n\x1a\n")) == FbxLoaderCode::NotFbx);
    // A file of another format the importer reads (OBJ) is not FBX either.
    CHECK(CodeOf(Text("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n")) == FbxLoaderCode::NotFbx);
}

TEST_CASE("FbxLoader rule 2: Undecodable") {
    // The binary header, then half of the file.
    Bytes half(Bar().begin(), Bar().begin() + static_cast<std::ptrdiff_t>(Bar().size() / 2));
    CHECK(CodeOf(half) == FbxLoaderCode::Undecodable);
    Bytes header(Bar().begin(), Bar().begin() + 27);
    CHECK(CodeOf(header) == FbxLoaderCode::Undecodable);
}

TEST_CASE("FbxLoader reads the contract example: nodes and their rest pose") {
    const auto loaded = Load(Bar());
    REQUIRE(loaded.has_value());
    const ModelData& model = *loaded;
    CHECK(Model::Validate(model).has_value());
    const std::uint32_t rig = NodeNamed(model, "Rig");
    const std::uint32_t lower = NodeNamed(model, "Lower");
    const std::uint32_t upper = NodeNamed(model, "Upper");
    NodeNamed(model, "Bar");
    CHECK(model.nodes[upper].parent == lower);
    CHECK(model.nodes[lower].parent == rig);
    // The first node is the file's root.
    CHECK(model.nodes[0].parent == Model::kNoParent);
    // Upper's joint is 1 above Lower's, along model +Y (metres, +Y up).
    std::vector<Math::Matrix4> global(model.nodes.size());
    Model::ComputeModelSpace(model.nodes, Rest(model), global);
    const Vec3 lowerAt = Math::TransformPoint(global[lower], {});
    const Vec3 upperAt = Math::TransformPoint(global[upper], {});
    INFO("lower " << Show(lowerAt) << " upper " << Show(upperAt));
    CHECK(Near(upperAt.x - lowerAt.x, 0.0F));
    CHECK(Near(upperAt.y - lowerAt.y, 1.0F));
    CHECK(Near(upperAt.z - lowerAt.z, 0.0F));
}

TEST_CASE("FbxLoader reads the contract example: Blender's front view kept with right +X and up +Y and away +Z") {
    // Tag's corners in Blender: (1, 0, 0.5), (1.2, 0, 0.5) and (1, -0.3, 0.5),
    // the last toward the viewer of the front view. Left-handed model space
    // keeps what that view shows: right is +X, up is +Y, away is +Z, so the
    // corner toward the viewer is at z = -0.3. A wrong handedness flips z.
    const auto loaded = Load(Bar());
    REQUIRE(loaded.has_value());
    const ModelData& model = *loaded;
    const auto corners = Skinned(model, MeshNamed(model, "Tag"), Rest(model));
    REQUIRE(corners.size() == 3);
    std::vector<Vec3> expected{{1.0F, 0.5F, 0.0F}, {1.2F, 0.5F, 0.0F}, {1.0F, 0.5F, -0.3F}};
    for (const Vec3 want : expected) {
        INFO("want " << Show(want));
        CHECK(std::any_of(corners.begin(), corners.end(), [&](const Vec3 p) { return Math::Distance(p, want) < 1.0e-4F; }));
    }
}

TEST_CASE("FbxLoader reads the contract example: the material is data and the texture only named") {
    const auto loaded = Load(Bar());
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->materials.size() == 3);
    const auto found = std::find_if(loaded->materials.begin(), loaded->materials.end(),
                                    [](const Model::Material& m) { return m.name == "Paint"; });
    REQUIRE(found != loaded->materials.end());
    const Model::Material& paint = *found;
    CHECK(paint.name == "Paint");
    CHECK(Near(paint.baseColor[0], 0.8F, 1.0e-3F));
    CHECK(Near(paint.baseColor[1], 0.1F, 1.0e-3F));
    CHECK(Near(paint.baseColor[2], 0.1F, 1.0e-3F));
    CHECK(Near(paint.baseColor[3], 1.0F, 1.0e-3F));
    INFO("texture " << paint.baseColorTexture);
    CHECK(paint.baseColorTexture.find("paint.png") != std::string::npos);
}

TEST_CASE("FbxLoader reads the contract example: the skinned mesh") {
    const auto loaded = Load(Bar());
    REQUIRE(loaded.has_value());
    const ModelData& model = *loaded;
    // Bar, Tag, and Pair's two material parts.
    REQUIRE(model.meshes.size() == 4);
    const Model::Mesh& bar = MeshNamed(model, "Bar");
    CHECK(bar.node == NodeNamed(model, "Bar"));
    CHECK(model.materials.at(bar.material).name == "Paint");
    // Ten quads, two triangles each.
    CHECK(bar.indices.size() == 60);
    REQUIRE(bar.uvs.size() == bar.positions.size());
    // Blender writes a cluster for every bone of the armature, so the skin
    // lists all five, each once.
    const std::uint32_t lower = NodeNamed(model, "Lower");
    const std::uint32_t upper = NodeNamed(model, "Upper");
    // Bar, Tag and Pair are moved by one armature, but each FBX mesh has its
    // own skin deformer, so each has its own skin.
    REQUIRE(model.skins.size() == 3);
    REQUIRE(bar.skin < model.skins.size());
    const Model::Skin& barSkin = model.skins[bar.skin];
    REQUIRE(barSkin.joints.size() == 5);
    for (const std::string name : {"Lower", "Upper", "Aux1", "Aux2", "Aux3"}) {
        CAPTURE(name);
        CHECK(std::count(barSkin.joints.begin(), barSkin.joints.end(), NodeNamed(model, name)) == 1);
    }

    // At rest the skin puts the box where Blender had it: 0.2 wide and deep,
    // from 0 to 2 up model +Y, around Lower's joint.
    const auto rest = Skinned(model, bar, Rest(model));
    std::vector<Math::Matrix4> global(model.nodes.size());
    Model::ComputeModelSpace(model.nodes, Rest(model), global);
    const Vec3 base = Math::TransformPoint(global[lower], {});
    float lowY = 1.0e9F, highY = -1.0e9F;
    for (std::size_t v = 0; v < rest.size(); ++v) {
        const Vec3 p = rest[v] - base;
        CAPTURE(v);
        INFO(Show(p));
        CHECK(Near(std::fabs(p.x), 0.1F));
        CHECK(Near(std::fabs(p.z), 0.1F));
        lowY = std::min(lowY, p.y);
        highY = std::max(highY, p.y);
        // Weights by height: below the cut Lower alone, above it Upper alone,
        // at the cut 0.3 each in the file, scaled to sum to 1.
        const auto& w = bar.vertexWeights[v];
        const std::uint32_t first = model.skins[bar.skin].joints[bar.vertexJoints[v][0]];
        if (p.y < 0.5F) {
            CHECK(Near(w[0], 1.0F));
            CHECK(first == lower);
        } else if (p.y > 1.5F) {
            CHECK(Near(w[0], 1.0F));
            CHECK(first == upper);
        } else {
            CHECK(Near(w[0], 0.5F));
            CHECK(Near(w[1], 0.5F));
        }
        CHECK(w[2] == 0.0F);
        CHECK(w[3] == 0.0F);
        // UVs: v was z / 2 in Blender (origin at the bottom); here (0, 0) is
        // the top-left corner, so v is 1 - y / 2.
        CHECK(Near(bar.uvs[v].y, 1.0F - p.y / 2.0F));
    }
    CHECK(Near(lowY, 0.0F));
    CHECK(Near(highY, 2.0F));
}

TEST_CASE("FbxLoader reads the contract example: the four largest weights scaled and no material") {
    const auto loaded = Load(Bar());
    REQUIRE(loaded.has_value());
    const ModelData& model = *loaded;
    const Model::Mesh& tag = MeshNamed(model, "Tag");
    CHECK(tag.material == Model::kNoMaterial);
    CHECK(tag.indices.size() == 3);
    CHECK(tag.uvs.empty());
    REQUIRE(tag.positions.size() == 3);
    const std::uint32_t lower = NodeNamed(model, "Lower");
    const std::uint32_t upper = NodeNamed(model, "Upper");
    const std::uint32_t aux1 = NodeNamed(model, "Aux1");
    const std::uint32_t aux2 = NodeNamed(model, "Aux2");
    const std::uint32_t aux3 = NodeNamed(model, "Aux3");
    // The vertex the file gives five influences: the place of Blender's
    // (1, 0, 0.5), the lowest and nearest to x = 1 of the three.
    std::size_t five = tag.positions.size();
    for (std::size_t v = 0; v < tag.positions.size(); ++v) {
        if (tag.vertexWeights[v][1] > 0.0F) five = v;
    }
    REQUIRE(five < tag.positions.size());
    const auto& joints = tag.vertexJoints[five];
    const auto& weights = tag.vertexWeights[five];
    const auto node = [&](const std::size_t k) { return model.skins[tag.skin].joints[joints[k]]; };
    // 0.4, 0.3, 0.2, 0.2 of 1.1; Aux3's 0.1 is dropped.
    CHECK(node(0) == lower);
    CHECK(node(1) == upper);
    CHECK(Near(weights[0], 0.4F / 1.1F));
    CHECK(Near(weights[1], 0.3F / 1.1F));
    CHECK(Near(weights[2], 0.2F / 1.1F));
    CHECK(Near(weights[3], 0.2F / 1.1F));
    CHECK(((node(2) == aux1 && node(3) == aux2) || (node(2) == aux2 && node(3) == aux1)));
    // Equal weights: the joint listed first in the skin first.
    CHECK(joints[2] < joints[3]);
    for (std::size_t k = 0; k < 4; ++k) CHECK(node(k) != aux3);
    for (std::size_t v = 0; v < tag.positions.size(); ++v) {
        if (v == five) continue;
        CAPTURE(v);
        CHECK(model.skins[tag.skin].joints[tag.vertexJoints[v][0]] == lower);
        CHECK(Near(tag.vertexWeights[v][0], 1.0F));
        CHECK(tag.vertexWeights[v][1] == 0.0F);
    }
}

TEST_CASE("FbxLoader reads the contract example: material parts are meshes that share one skin") {
    const auto loaded = Load(Bar());
    REQUIRE(loaded.has_value());
    const ModelData& model = *loaded;
    std::vector<const Model::Mesh*> pair;
    for (const Model::Mesh& mesh : model.meshes) {
        if (mesh.name == "Pair") pair.push_back(&mesh);
    }
    REQUIRE(pair.size() == 2);
    std::vector<std::string> materials;
    for (const Model::Mesh* mesh : pair) {
        CHECK(mesh->indices.size() == 3);
        materials.push_back(model.materials.at(mesh->material).name);
    }
    std::sort(materials.begin(), materials.end());
    CHECK(materials == std::vector<std::string>{"Left", "Right"});
    REQUIRE(pair[0]->skin != Model::kNoSkin);
    CHECK(pair[0]->skin == pair[1]->skin);
    const Model::Skin& skin = model.skins.at(pair[0]->skin);
    for (const Model::Mesh* mesh : pair) {
        for (std::size_t v = 0; v < mesh->positions.size(); ++v) {
            CHECK(skin.joints[mesh->vertexJoints[v][0]] == NodeNamed(model, "Lower"));
            CHECK(Near(mesh->vertexWeights[v][0], 1.0F));
        }
    }
}

TEST_CASE("FbxLoader reads the contract example: the clips baked to keys") {
    const auto loaded = Load(Bar());
    REQUIRE(loaded.has_value());
    const ModelData& model = *loaded;
    REQUIRE(model.clips.size() == 2);
    std::vector<std::string> names;
    for (const auto& clip : model.clips) names.push_back(clip.name);
    INFO("clips " << names[0] << ", " << names[1]);
    const auto bendIndex = std::find_if(model.clips.begin(), model.clips.end(), [](const Model::AnimationClip& c) {
        return c.name.find("Bend") != std::string::npos;
    });
    const auto holdIndex = std::find_if(model.clips.begin(), model.clips.end(), [](const Model::AnimationClip& c) {
        return c.name.find("Hold") != std::string::npos;
    });
    REQUIRE(bendIndex != model.clips.end());
    REQUIRE(holdIndex != model.clips.end());
    const Model::AnimationClip& bend = *bendIndex;
    const Model::AnimationClip& hold = *holdIndex;
    CHECK(Near(bend.duration, 1.0F, 1.0e-3F));
    CHECK(Near(hold.duration, 0.5F, 1.0e-3F));
    CHECK(bend.nodeCount == model.nodes.size());

    const std::uint32_t upper = NodeNamed(model, "Upper");
    const Model::Mesh& bar = MeshNamed(model, "Bar");
    const auto rest = Skinned(model, bar, Rest(model));
    std::vector<LocalTransform> pose(model.nodes.size());
    std::vector<Math::Matrix4> global(model.nodes.size());

    // Bend: at its end Upper has turned 90 degrees; every vertex above the cut
    // keeps its distance to Upper's joint and has turned 90 degrees about it.
    Model::SampleClip(model.nodes, bend, bend.duration, Wrap::Clamp, pose);
    Model::ComputeModelSpace(model.nodes, pose, global);
    const Vec3 joint = Math::TransformPoint(global[upper], {});
    const auto bent = Skinned(model, bar, pose);
    std::size_t checked = 0;
    for (std::size_t v = 0; v < rest.size(); ++v) {
        if (bar.vertexWeights[v][0] < 0.99F || model.skins[bar.skin].joints[bar.vertexJoints[v][0]] != upper) continue;
        const Vec3 before = rest[v] - joint;
        const Vec3 after = bent[v] - joint;
        CAPTURE(v);
        INFO("before " << Show(before) << " after " << Show(after));
        CHECK(Near(Math::Length(before), Math::Length(after), 1.0e-3F));
        // Only the direction along the bone (model +Y at rest) turns; the
        // vertices at the bone's end are 1 along it.
        if (before.y > 0.9F) {
            const float cosine = Math::Dot(before, after) / (Math::Length(before) * Math::Length(after));
            const float degrees = std::acos(std::clamp(cosine, -1.0F, 1.0F)) * 180.0F / kPi;
            CHECK(degrees == doctest::Approx(90.0F).epsilon(0.02));
        }
        ++checked;
    }
    // The four top corners, each on the top face and two sides: twelve
    // vertices with their own normals.
    CHECK(checked == 12);
    // Half way the turn is about half.
    Model::SampleClip(model.nodes, bend, bend.duration / 2.0F, Wrap::Clamp, pose);
    const auto halfway = Skinned(model, bar, pose);
    for (std::size_t v = 0; v < rest.size(); ++v) {
        if (bar.vertexWeights[v][0] < 0.99F || model.skins[bar.skin].joints[bar.vertexJoints[v][0]] != upper) continue;
        const Vec3 before = rest[v] - joint;
        const Vec3 after = halfway[v] - joint;
        if (before.y <= 0.9F) continue;
        const float cosine = Math::Dot(before, after) / (Math::Length(before) * Math::Length(after));
        CHECK(std::acos(std::clamp(cosine, -1.0F, 1.0F)) * 180.0F / kPi == doctest::Approx(45.0F).epsilon(0.05));
    }

    // Hold: nothing moves.
    for (const float t : {0.0F, 0.25F, 0.5F}) {
        Model::SampleClip(model.nodes, hold, t, Wrap::Clamp, pose);
        const auto held = Skinned(model, bar, pose);
        for (std::size_t v = 0; v < rest.size(); ++v) {
            CAPTURE(v);
            CHECK(Near(Math::Distance(held[v], rest[v]), 0.0F, 1.0e-4F));
        }
    }
}

TEST_CASE("FbxLoader with meshes false: the same nodes and materials and clips but no meshes") {
    const auto full = Load(Bar());
    const auto bare = Load(Bar(), FbxSettings{false});
    REQUIRE(full.has_value());
    REQUIRE(bare.has_value());
    CHECK(bare->meshes.empty());
    CHECK(NodesAndClips(*bare) == NodesAndClips(*full));
}

TEST_CASE("FbxLoader gives the same model for the same bytes on any thread (AS-3)") {
    const auto first = Load(Bar());
    REQUIRE(first.has_value());
    const auto bits = Bits(*first);
    CHECK(Bits(*Load(Bar())) == bits);
    int mismatches[2] = {0, 0};
    std::thread threads[2];
    for (int t = 0; t < 2; ++t) {
        threads[t] = std::thread([&, t] {
            for (int i = 0; i < 20; ++i) {
                const auto loaded = Load(Bar());
                if (!loaded || Bits(*loaded) != bits) ++mismatches[t];
                // A failure in between leaves nothing behind for the next load.
                if (Load(Text("not a model")).has_value()) ++mismatches[t];
            }
        });
    }
    for (std::thread& thread : threads) thread.join();
    CHECK(mismatches[0] == 0);
    CHECK(mismatches[1] == 0);
}

namespace {

// Runs each job at once; a job submitted from a job waits in the queue.
class InlineExecutor final : public Asset::AssetExecutor {
public:
    void Submit(Job job) override {
        queue_.push_back(std::move(job));
        if (running_) return;
        running_ = true;
        while (!queue_.empty()) {
            Job next = std::move(queue_.front());
            queue_.pop_front();
            next();
        }
        running_ = false;
    }

private:
    std::deque<Job> queue_;
    bool running_ = false;
};

// A content root in the temporary directory holding models/bar.fbx.
struct Content final {
    std::filesystem::path root;
    explicit Content(const Bytes& bytes) {
        root = std::filesystem::temp_directory_path() /
               ("gyo_model_fbx_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        std::filesystem::create_directories(root / "models");
        std::ofstream out(root / "models" / "bar.fbx", std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    Content(const Content&) = delete;
    Content& operator=(const Content&) = delete;
    ~Content() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
};

Asset::CatalogEntry Entry(const std::string& id, std::optional<std::string> settings = std::nullopt) {
    Asset::CatalogEntry entry;
    entry.id = Asset::AssetId(id);
    entry.type = "model";
    entry.path = "models/bar.fbx";
    entry.settings = std::move(settings);
    return entry;
}

const Asset::AssetRecord& Record(const Asset::AssetSnapshot& snapshot, const std::string& id) {
    const Asset::AssetRecord* const record = snapshot.Find(Asset::AssetId(id));
    REQUIRE(record != nullptr);
    return *record;
}

} // namespace

TEST_CASE("FbxLoader through the asset store: loaded with its settings and bad settings and the size limit") {
    const Content content(Bar());
    InlineExecutor executor;
    const std::vector<Asset::CatalogEntry> entries{Entry("game.bar"), Entry("game.bar_skeleton", R"({"meshes":false})"),
                                                   Entry("game.bar_bad", R"({"meshes":"no"})")};

    SUBCASE("loaded, with each entry's settings") {
        Asset::AssetStoreOptions options;
        options.contentRoot = content.root.string();
        Asset::AssetStore store(entries, options, &executor);
        REQUIRE(store.Loaders().Register<FbxLoader>().has_value());
        const auto bar = store.Read(store.Request<ModelData>(Asset::AssetId("game.bar")));
        REQUIRE(bar != nullptr);
        CHECK(bar->meshes.size() == 4);
        const auto skeleton = store.Read(store.Request<ModelData>(Asset::AssetId("game.bar_skeleton")));
        REQUIRE(skeleton != nullptr);
        CHECK(skeleton->meshes.empty());
        CHECK(skeleton->clips.size() == 2);
        CHECK(store.Read(store.Request<ModelData>(Asset::AssetId("game.bar_bad"))) == nullptr);
        const Asset::AssetSnapshot snapshot = store.Snapshot();
        const Asset::AssetRecord& bad = Record(snapshot, "game.bar_bad");
        REQUIRE(bad.failure.has_value());
        CHECK(bad.failure->code == Asset::AssetLoadFailureCode::BadSettings);
    }

    SUBCASE("a file above the limit is TooLarge and the loader is not called") {
        Asset::AssetStoreOptions options;
        options.contentRoot = content.root.string();
        options.byteLimits.push_back(Asset::AssetByteLimit{"model", "fbx", Bar().size() - 1});
        Asset::AssetStore store(entries, options, &executor);
        REQUIRE(store.Loaders().Register<FbxLoader>().has_value());
        CHECK(store.Read(store.Request<ModelData>(Asset::AssetId("game.bar"))) == nullptr);
        const Asset::AssetSnapshot snapshot = store.Snapshot();
        const Asset::AssetRecord& record = Record(snapshot, "game.bar");
        REQUIRE(record.failure.has_value());
        CHECK(record.failure->code == Asset::AssetLoadFailureCode::TooLarge);
    }
}

TEST_CASE("FbxLoader through the asset store: a file the loader refuses is LoaderFailed with its code") {
    const Content content(Text("not a model"));
    InlineExecutor executor;
    Asset::AssetStoreOptions options;
    options.contentRoot = content.root.string();
    const std::vector<Asset::CatalogEntry> entries{Entry("game.bar")};
    Asset::AssetStore store(entries, options, &executor);
    REQUIRE(store.Loaders().Register<FbxLoader>().has_value());
    CHECK(store.Read(store.Request<ModelData>(Asset::AssetId("game.bar"))) == nullptr);
    const Asset::AssetSnapshot snapshot = store.Snapshot();
    const Asset::AssetRecord& record = Record(snapshot, "game.bar");
    REQUIRE(record.failure.has_value());
    CHECK(record.failure->code == Asset::AssetLoadFailureCode::LoaderFailed);
    CHECK(record.failure->detail.find("NotFbx") != std::string::npos);
}
