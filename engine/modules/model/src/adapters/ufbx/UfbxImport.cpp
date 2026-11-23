#include "UfbxImport.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <ufbx.h>

namespace GYO::Engine::Model::Detail {
namespace {

// What ufbx may allocate for one file, for its work and for the scene.
constexpr std::size_t kMemoryLimit = std::size_t{256} * 1024u * 1024u;

struct SceneDeleter final {
    void operator()(ufbx_scene* scene) const noexcept { ufbx_free_scene(scene); }
};
struct BakeDeleter final {
    void operator()(ufbx_baked_anim* baked) const noexcept { ufbx_free_baked_anim(baked); }
};
using ScenePtr = std::unique_ptr<ufbx_scene, SceneDeleter>;
using BakePtr = std::unique_ptr<ufbx_baked_anim, BakeDeleter>;

Base::Err<ImportFailure> Fail(const ImportProblem problem, const char* message, std::string detail = {}) {
    return Base::Err(ImportFailure::Make(problem, message, std::move(detail)));
}

std::string Text(const ufbx_string value) { return {value.data, value.length}; }

std::string Describe(const ufbx_error& error) {
    std::array<char, 1024> text{};
    ufbx_format_error(text.data(), text.size(), &error);
    return std::string(text.data());
}

Math::Vec3 ToVec3(const ufbx_vec3 v) noexcept {
    return {static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
}
Math::Vec2 ToVec2(const ufbx_vec2 v) noexcept { return {static_cast<float>(v.x), static_cast<float>(v.y)}; }
Math::Quaternion ToQuaternion(const ufbx_quat q) noexcept {
    return {static_cast<float>(q.x), static_cast<float>(q.y), static_cast<float>(q.z), static_cast<float>(q.w)};
}
LocalTransform ToLocal(const ufbx_transform& t) noexcept {
    return {ToVec3(t.translation), ToQuaternion(t.rotation), ToVec3(t.scale)};
}
// ufbx_matrix is four columns of three (the last the translation).
Math::Matrix4 ToMatrix4(const ufbx_matrix& m) noexcept {
    Math::Matrix4 result;
    for (std::size_t c = 0; c < 4; ++c) {
        for (std::size_t r = 0; r < 3; ++r) result.values[c * 4 + r] = static_cast<float>(m.v[c * 3 + r]);
    }
    return result;
}

ufbx_load_opts LoadOptions(const ImportOptions& options) {
    ufbx_load_opts opts{};
    // The engine's model space: metres, +Y up, +Z forward, left-handed. The
    // mirror is explicit (Z), and the conversion goes into the geometry and the
    // transforms, so the root carries none of it.
    opts.target_axes = ufbx_axes_left_handed_y_up;
    opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;
    opts.target_unit_meters = 1.0;
    opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
    // Geometry transforms go into the vertices (helper nodes where they
    // cannot), and inheritance other than parent * local becomes helper nodes,
    // so model space is the plain product down the hierarchy.
    opts.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY;
    opts.inherit_mode_handling = UFBX_INHERIT_MODE_HANDLING_HELPER_NODES;
    opts.generate_missing_normals = true;
    opts.index_error_handling = UFBX_INDEX_ERROR_HANDLING_ABORT_LOADING;
    // Reading files is asset's; textures are named, not loaded.
    opts.load_external_files = false;
    opts.ignore_embedded = true;
    opts.ignore_geometry = !options.meshes;
    // The format is left for ufbx to recognize, so bytes that are no FBX file
    // are told apart from a broken FBX file (a forced FBX format reads any
    // text as a truncated ASCII file); a file it reads as another format is
    // NotFbx too.
    opts.temp_allocator.memory_limit = kMemoryLimit;
    opts.result_allocator.memory_limit = kMemoryLimit;
    return opts;
}

class Importer final {
public:
    Importer(const ufbx_scene& scene, const ImportOptions& options) : scene_(scene), options_(options) {}

    Base::Result<ModelData, ImportFailure> Run() {
        ReadNodes();
        ReadMaterials();
        // With meshes off ufbx loads no geometry (ignore_geometry), so every
        // mesh has no triangles and ReadMesh skips it.
        for (const ufbx_node* node : scene_.nodes) {
            if (auto r = ReadMesh(*node); !r) return Base::Err(std::move(r.error()));
        }
        for (const ufbx_anim_stack* stack : scene_.anim_stacks) {
            if (auto r = ReadClip(*stack); !r) return Base::Err(std::move(r.error()));
        }
        return std::move(model_);
    }

private:
    // Depth first from the root, children in file order: parents precede
    // their children. Iterative, so a deep file cannot grow the stack.
    void ReadNodes() {
        nodeIndex_.assign(scene_.nodes.count, kNoParent);
        std::vector<std::pair<const ufbx_node*, std::uint32_t>> pending;
        pending.emplace_back(scene_.root_node, kNoParent);
        while (!pending.empty()) {
            const auto [node, parent] = pending.back();
            pending.pop_back();
            const auto index = static_cast<std::uint32_t>(model_.nodes.size());
            nodeIndex_[node->typed_id] = index;
            model_.nodes.push_back(Node{Text(node->name), parent, ToLocal(node->local_transform)});
            for (std::size_t i = node->children.count; i > 0; --i) pending.emplace_back(node->children.data[i - 1], index);
        }
    }

    void ReadMaterials() {
        for (const ufbx_material* source : scene_.materials) {
            Material material;
            material.name = Text(source->name);
            const ufbx_material_map& color = source->fbx.diffuse_color;
            if (color.has_value) {
                // ufbx resolves the colour but leaves the diffuse factor apart;
                // it scales RGB, alpha stays straight.
                const ufbx_material_map& factorMap = source->fbx.diffuse_factor;
                const double factor = factorMap.has_value ? factorMap.value_real : 1.0;
                material.baseColor = {static_cast<float>(color.value_vec4.x * factor),
                                      static_cast<float>(color.value_vec4.y * factor),
                                      static_cast<float>(color.value_vec4.z * factor),
                                      static_cast<float>(color.value_vec4.w)};
            }
            if (const ufbx_texture* texture = color.texture) {
                material.baseColorTexture =
                    texture->relative_filename.length > 0 ? Text(texture->relative_filename) : Text(texture->filename);
            }
            model_.materials.push_back(std::move(material));
        }
    }

    Base::Result<void, ImportFailure> ReadMesh(const ufbx_node& node) {
        const ufbx_mesh* mesh = node.mesh;
        if (mesh == nullptr || mesh->num_triangles == 0) return {};
        const std::string where = "mesh at node \"" + Text(node.name) + "\"";
        if (mesh->skin_deformers.count > 1) {
            return Fail(ImportProblem::Unsupported, "a mesh has at most one skin", where);
        }
        const ufbx_skin_deformer* skin = mesh->skin_deformers.count == 1 ? mesh->skin_deformers.data[0] : nullptr;
        Skin skinTemplate;
        if (skin != nullptr) {
            if (skin->clusters.count > std::size_t{std::numeric_limits<std::uint16_t>::max()} + 1) {
                return Fail(ImportProblem::Unsupported, "a skin has at most 65536 joints", where);
            }
            for (const ufbx_skin_cluster* cluster : skin->clusters) {
                if (cluster->bone_node == nullptr) {
                    return Fail(ImportProblem::Undecodable, "a skin cluster has a joint node", where);
                }
                skinTemplate.joints.push_back(nodeIndex_[cluster->bone_node->typed_id]);
                skinTemplate.inverseBindMatrices.push_back(ToMatrix4(cluster->geometry_to_bone));
            }
        }
        if (mesh->material_parts.count > 0) {
            for (const ufbx_mesh_part& part : mesh->material_parts) {
                if (part.num_triangles == 0) continue;
                if (auto r = ReadPart(node, *mesh, skin, skinTemplate, &part, where); !r) return r;
            }
            return {};
        }
        return ReadPart(node, *mesh, skin, skinTemplate, nullptr, where);
    }

    // One Mesh per material part: the corners of its triangles, then
    // identical vertices merged.
    Base::Result<void, ImportFailure> ReadPart(const ufbx_node& node, const ufbx_mesh& mesh,
                                               const ufbx_skin_deformer* skin, const Skin& skinTemplate,
                                               const ufbx_mesh_part* part, const std::string& where) {
        Mesh out;
        out.name = Text(node.name);
        out.node = nodeIndex_[node.typed_id];
        const std::size_t slot = part != nullptr ? part->index : 0;
        if (slot < node.materials.count && node.materials.data[slot] != nullptr) {
            out.material = node.materials.data[slot]->typed_id;
        }
        const bool hasUv = mesh.vertex_uv.exists;
        std::vector<std::uint32_t> triangle(mesh.max_face_triangles * 3);
        const auto addFace = [&](const ufbx_face face) {
            const std::uint32_t count = ufbx_triangulate_face(triangle.data(), triangle.size(), &mesh, face);
            for (std::size_t i = 0; i < std::size_t{count} * 3; ++i) {
                const std::uint32_t corner = triangle[i];
                out.positions.push_back(ToVec3(ufbx_get_vertex_vec3(&mesh.vertex_position, corner)));
                out.normals.push_back(ToVec3(ufbx_get_vertex_vec3(&mesh.vertex_normal, corner)));
                if (hasUv) {
                    // Top-left origin, as an image's first byte (image IM-2).
                    const Math::Vec2 uv = ToVec2(ufbx_get_vertex_vec2(&mesh.vertex_uv, corner));
                    out.uvs.push_back({uv.x, 1.0F - uv.y});
                }
                if (skin != nullptr) AddInfluences(*skin, mesh.vertex_indices.data[corner], out.skin);
            }
        };
        if (part != nullptr) {
            for (const std::uint32_t face : part->face_indices) addFace(mesh.faces.data[face]);
        } else {
            for (const ufbx_face face : mesh.faces) addFace(face);
        }
        if (out.positions.size() >= std::numeric_limits<std::uint32_t>::max()) {
            return Fail(ImportProblem::Unsupported, "a mesh has fewer than 2^32 vertices", where);
        }
        if (auto r = MergeVertices(out); !r) return r;
        if (skin != nullptr) {
            out.skin.joints = skinTemplate.joints;
            out.skin.inverseBindMatrices = skinTemplate.inverseBindMatrices;
        }
        model_.meshes.push_back(std::move(out));
        return {};
    }

    // The four largest weights of the vertex (ties: the lower joint first),
    // scaled to sum to 1; all 0 when the vertex has no weight above 0.
    static void AddInfluences(const ufbx_skin_deformer& skin, const std::uint32_t vertex, Skin& out) {
        std::array<std::uint16_t, 4> joints{};
        std::array<float, 4> weights{};
        if (vertex < skin.vertices.count) {
            const ufbx_skin_vertex& influence = skin.vertices.data[vertex];
            std::vector<std::pair<double, std::uint32_t>> all;
            all.reserve(influence.num_weights);
            for (std::uint32_t k = 0; k < influence.num_weights; ++k) {
                const ufbx_skin_weight& w = skin.weights.data[influence.weight_begin + k];
                if (std::isfinite(w.weight) && w.weight > 0.0) all.emplace_back(w.weight, w.cluster_index);
            }
            std::sort(all.begin(), all.end(), [](const auto& a, const auto& b) {
                return a.first != b.first ? a.first > b.first : a.second < b.second;
            });
            const std::size_t count = std::min<std::size_t>(4, all.size());
            double total = 0.0;
            for (std::size_t k = 0; k < count; ++k) total += all[k].first;
            for (std::size_t k = 0; k < count; ++k) {
                joints[k] = static_cast<std::uint16_t>(all[k].second);
                weights[k] = static_cast<float>(all[k].first / total);
            }
        }
        out.vertexJoints.push_back(joints);
        out.vertexWeights.push_back(weights);
    }

    static Base::Result<void, ImportFailure> MergeVertices(Mesh& mesh) {
        const std::size_t corners = mesh.positions.size();
        std::vector<ufbx_vertex_stream> streams;
        const auto add = [&](auto& values) {
            if (!values.empty()) streams.push_back({values.data(), corners, sizeof(values[0])});
        };
        add(mesh.positions);
        add(mesh.normals);
        add(mesh.uvs);
        add(mesh.skin.vertexJoints);
        add(mesh.skin.vertexWeights);
        mesh.indices.resize(corners);
        ufbx_error error{};
        const std::size_t vertices =
            ufbx_generate_indices(streams.data(), streams.size(), mesh.indices.data(), corners, nullptr, &error);
        if (error.type != UFBX_ERROR_NONE) {
            return Fail(ImportProblem::Undecodable, "the importer could not merge a mesh's vertices", Describe(error));
        }
        mesh.positions.resize(vertices);
        mesh.normals.resize(vertices);
        if (!mesh.uvs.empty()) mesh.uvs.resize(vertices);
        if (!mesh.skin.vertexJoints.empty()) {
            mesh.skin.vertexJoints.resize(vertices);
            mesh.skin.vertexWeights.resize(vertices);
        }
        return {};
    }

    // Keys as float seconds; keys whose times become equal keep the later
    // value (a step baked as two keys).
    template <class Source, class T, class Convert>
    static void CopyKeys(const Source& source, Keys<T>& out, Convert convert, float& lastTime) {
        for (const auto& key : source) {
            const float time = std::max(0.0F, static_cast<float>(key.time));
            if (!out.times.empty() && out.times.back() >= time) {
                out.values.back() = convert(key.value);
                continue;
            }
            out.times.push_back(time);
            out.values.push_back(convert(key.value));
        }
        if (!out.times.empty()) lastTime = std::max(lastTime, out.times.back());
    }

    Base::Result<void, ImportFailure> ReadClip(const ufbx_anim_stack& stack) {
        ufbx_bake_opts opts{};
        opts.trim_start_time = true;
        opts.resample_rate = options_.bakeRate;
        opts.temp_allocator.memory_limit = kMemoryLimit;
        opts.result_allocator.memory_limit = kMemoryLimit;
        ufbx_error error{};
        const BakePtr baked(ufbx_bake_anim(&scene_, stack.anim, &opts, &error));
        if (!baked) {
            return Fail(ImportProblem::Undecodable, "the importer could not bake an animation",
                        "\"" + Text(stack.name) + "\": " + Describe(error));
        }
        AnimationClip clip;
        clip.name = Text(stack.name);
        clip.nodeCount = static_cast<std::uint32_t>(model_.nodes.size());
        float lastTime = 0.0F;
        for (const ufbx_baked_node& source : baked->nodes) {
            Track track;
            track.node = nodeIndex_[source.typed_id];
            CopyKeys(source.translation_keys, track.translation, ToVec3, lastTime);
            CopyKeys(source.rotation_keys, track.rotation, ToQuaternion, lastTime);
            CopyKeys(source.scale_keys, track.scale, ToVec3, lastTime);
            clip.tracks.push_back(std::move(track));
        }
        std::sort(clip.tracks.begin(), clip.tracks.end(),
                  [](const Track& a, const Track& b) { return a.node < b.node; });
        clip.duration = std::max(static_cast<float>(baked->playback_duration), lastTime);
        model_.clips.push_back(std::move(clip));
        return {};
    }

    const ufbx_scene& scene_;
    const ImportOptions& options_;
    ModelData model_;
    // scene node typed_id -> index in model_.nodes
    std::vector<std::uint32_t> nodeIndex_;
};

} // namespace

Base::Result<ModelData, ImportFailure> ImportFbx(const std::span<const std::byte> bytes, const ImportOptions& options) {
    const ufbx_load_opts opts = LoadOptions(options);
    ufbx_error error{};
    const ScenePtr scene(ufbx_load_memory(bytes.data(), bytes.size(), &opts, &error));
    if (!scene) {
        const bool notFbx =
            error.type == UFBX_ERROR_UNRECOGNIZED_FILE_FORMAT || error.type == UFBX_ERROR_EMPTY_FILE;
        if (notFbx) return Fail(ImportProblem::NotFbx, "not an FBX file", Describe(error));
        return Fail(ImportProblem::Undecodable, "the importer could not read the file", Describe(error));
    }
    if (scene->metadata.file_format != UFBX_FILE_FORMAT_FBX) {
        return Fail(ImportProblem::NotFbx, "not an FBX file",
                    "read as file format " + std::to_string(static_cast<int>(scene->metadata.file_format)));
    }
    return Importer(*scene, options).Run();
}

} // namespace GYO::Engine::Model::Detail
