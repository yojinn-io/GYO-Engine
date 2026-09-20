#include "model/backend/ufbx/UfbxModelLoader.hpp"
#include "model/Animation.hpp"

#include <ufbx.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace Engine::Model::Ufbx {
namespace {
using Result = Base::Result<Asset::Core::AnyAsset, Asset::Loading::AssetError>;
using ScenePtr = std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)>;
using BakePtr = std::unique_ptr<ufbx_baked_anim, decltype(&ufbx_free_baked_anim)>;

std::string String(const ufbx_string value) { return {value.data,value.length}; }
Vec3 Vector(const ufbx_vec3 v) { return {static_cast<float>(v.x),static_cast<float>(v.y),static_cast<float>(v.z)}; }
Quaternion Rotation(const ufbx_quat q) {
    return {static_cast<float>(q.x),static_cast<float>(q.y),static_cast<float>(q.z),static_cast<float>(q.w)};
}
Transform TransformOf(const ufbx_transform& t) { return {Vector(t.translation),Rotation(t.rotation),Vector(t.scale)}; }
Matrix4 Matrix(const ufbx_matrix& source) {
    Matrix4 result;
    for(std::size_t c=0;c<4;++c) for(std::size_t r=0;r<3;++r)
        result.values[c*4+r]=static_cast<float>(source.v[c*3+r]);
    return result;
}

Result Error(const Asset::Loading::LoadContext& context,std::string message) {
    return Result::Err(Asset::Loading::AssetError::Make(
        Asset::AssetErrorCode::DecodeFailed,"FBX model: "+std::move(message),context.resolvedPath));
}

std::string ErrorMessage(const ufbx_error& error) {
    char message[2048]{};
    ufbx_format_error(message,sizeof(message),&error);
    return message;
}

template<class Source,class Destination,class Convert>
void CopyKeys(const Source& source,std::vector<Keyframe<Destination>>& destination,Convert convert) {
    destination.reserve(source.count);
    for(const auto& key:source) {
        Keyframe<Destination> next{key.time,convert(key.value)};
        // Identical-time steps select the right-hand value at the boundary.
        if(!destination.empty()&&destination.back().timeSeconds==next.timeSeconds)
            destination.back()=next;
        else destination.push_back(next);
    }
}

std::shared_ptr<ModelAsset> Import(const ufbx_scene& scene) {
    auto model=std::make_shared<ModelAsset>();
    std::vector<std::size_t> nodeIndices(scene.nodes.count,std::numeric_limits<std::size_t>::max());
    // Source files can contain arbitrarily deep hierarchies. Keep traversal
    // iterative so the parser's bounded allocation cannot become stack growth.
    std::vector<std::pair<const ufbx_node*,std::optional<std::size_t>>> pending;
    pending.emplace_back(scene.root_node,std::nullopt);
    while(!pending.empty()) {
        const auto [node,parent]=pending.back();
        pending.pop_back();
        if(nodeIndices.at(node->typed_id)!=std::numeric_limits<std::size_t>::max())
            throw std::runtime_error("node hierarchy contains a repeated node");
        const std::size_t index=model->nodes.size();
        nodeIndices.at(node->typed_id)=index;
        model->nodes.push_back({String(node->name),parent,TransformOf(node->local_transform)});
        for(std::size_t i=node->children.count;i>0;--i)
            pending.emplace_back(node->children[i-1],index);
    }

    for(const auto* material:scene.materials) {
        Material imported{String(material->name)};
        const auto& color=material->fbx.diffuse_color;
        if(color.has_value) {
            // ufbx resolves authored/template properties but does not multiply
            // the diffuse factor into the color. Apply it to RGB once; alpha
            // remains straight, and a missing factor is the identity.
            const auto& diffuseFactor=material->fbx.diffuse_factor;
            const double factor=diffuseFactor.has_value?diffuseFactor.value_real:1.0;
            const auto value=color.value_vec4;
            imported.baseColorLinear={static_cast<float>(value.x*factor),
                static_cast<float>(value.y*factor),static_cast<float>(value.z*factor),
                static_cast<float>(value.w)};
        }
        model->materials.push_back(std::move(imported));
    }
    const std::size_t defaultMaterial=model->materials.size();
    // A fallback material exists only if a mesh actually needs it.
    bool usedDefault=false;

    for(const auto* node:scene.nodes) {
        const auto* mesh=node->mesh;
        if(!mesh||mesh->num_triangles==0) continue;
        if(mesh->skin_deformers.count>1||mesh->blend_deformers.count||mesh->cache_deformers.count)
            throw std::runtime_error("only a single linear/rigid skin deformer is supported per mesh");
        const ufbx_skin_deformer* skin=mesh->skin_deformers.count?mesh->skin_deformers[0]:nullptr;
        if(skin&&skin->skinning_method!=UFBX_SKINNING_METHOD_LINEAR&&skin->skinning_method!=UFBX_SKINNING_METHOD_RIGID)
            throw std::runtime_error("dual-quaternion skinning is not supported");

        std::vector<SkinJoint> joints;
        if(skin) for(const auto* cluster:skin->clusters) {
            if(!cluster->bone_node) throw std::runtime_error("skin cluster has no joint node");
            joints.push_back({nodeIndices.at(cluster->bone_node->typed_id),Matrix(cluster->geometry_to_bone)});
        }

        const auto addPart=[&](const ufbx_mesh_part* materialPart) {
            MeshPart part;
            part.name=String(node->name);
            part.nodeIndex=nodeIndices.at(node->typed_id);
            part.geometryToNode=Matrix(node->geometry_to_node);
            part.joints=joints;
            const std::size_t slot=materialPart?materialPart->index:0;
            if(slot<node->materials.count&&node->materials[slot]) {
                part.materialIndex=node->materials[slot]->typed_id;
                part.name+="/"+model->materials.at(part.materialIndex).name;
            } else {part.materialIndex=defaultMaterial;usedDefault=true;}

            std::unordered_map<std::uint32_t,std::uint32_t> corners;
            std::vector<std::uint32_t> triangleIndices;
            const auto addFace=[&](const std::uint32_t faceIndex) {
                const auto face=mesh->faces[faceIndex];
                if(face.num_indices<3) return;
                triangleIndices.resize((face.num_indices-2)*3);
                const auto triangles=ufbx_triangulate_face(triangleIndices.data(),triangleIndices.size(),mesh,face);
                for(std::size_t i=0;i<static_cast<std::size_t>(triangles)*3;++i) {
                    const auto corner=triangleIndices[i];
                    auto found=corners.find(corner);
                    if(found!=corners.end()) {part.indices.push_back(found->second);continue;}
                    SkinVertex vertex;
                    vertex.position=Vector(ufbx_get_vertex_vec3(&mesh->vertex_position,corner));
                    vertex.normal=Vector(ufbx_get_vertex_vec3(&mesh->vertex_normal,corner));
                    if(mesh->vertex_uv.exists) {
                        const auto uv=ufbx_get_vertex_vec2(&mesh->vertex_uv,corner);
                        // GYO decoded textures use a top-left origin. Keep tile
                        // coordinates unbounded: wrapping individual corners
                        // changes interpolation across UV seams.
                        vertex.uv={static_cast<float>(uv.x),1.0F-static_cast<float>(uv.y)};
                    }
                    if(skin) {
                        const auto sourceVertex=mesh->vertex_indices[corner];
                        const auto influence=skin->vertices[sourceVertex];
                        const auto count=(std::min)(std::size_t{4},static_cast<std::size_t>(influence.num_weights));
                        double total=0;
                        for(std::size_t k=0;k<count;++k) {
                            const auto weight=skin->weights[influence.weight_begin+k];
                            if(!std::isfinite(weight.weight)||weight.weight<0||weight.cluster_index>=joints.size())
                                throw std::runtime_error("invalid skin influence");
                            vertex.joints[k]=weight.cluster_index;
                            vertex.weights[k]=static_cast<float>(weight.weight);
                            total+=weight.weight;
                        }
                        if(total>0) for(float& weight:vertex.weights) weight=static_cast<float>(weight/total);
                    }
                    if(part.vertices.size()>=std::numeric_limits<std::uint32_t>::max())
                        throw std::runtime_error("mesh exceeds 32-bit index capacity");
                    const auto index=static_cast<std::uint32_t>(part.vertices.size());
                    corners.emplace(corner,index);
                    part.vertices.push_back(vertex);
                    part.indices.push_back(index);
                }
            };
            if(materialPart) for(const auto face:materialPart->face_indices) addFace(face);
            else for(std::size_t i=0;i<mesh->faces.count;++i) addFace(static_cast<std::uint32_t>(i));
            if(!part.indices.empty()) model->meshes.push_back(std::move(part));
        };
        if(mesh->material_parts.count) {
            for(const auto& part:mesh->material_parts) if(part.num_triangles) addPart(&part);
        } else addPart(nullptr);
    }
    if(usedDefault) model->materials.push_back({"Default"});

    for(const auto* stack:scene.anim_stacks) {
        ufbx_bake_opts options{};
        options.trim_start_time=true;
        options.resample_rate=60.0;
        // Keep ufbx's default minimum sample rate (19.5 Hz), which preserves
        // animation already baked at 24/30 Hz. Inserting Euler-interpolated
        // subframes between equivalent Euler branches creates false spins;
        // the owning quaternion tracks interpolate the authored poses instead.
        options.temp_allocator.memory_limit=256*1024*1024;
        options.result_allocator.memory_limit=256*1024*1024;
        ufbx_error error{};
        BakePtr baked(ufbx_bake_anim(&scene,stack->anim,&options,&error),ufbx_free_baked_anim);
        if(!baked) throw std::runtime_error("cannot bake "+String(stack->name)+": "+ErrorMessage(error));
        AnimationClip clip;
        clip.name=String(stack->name);
        clip.durationSeconds=baked->playback_duration;
        for(const auto& node:baked->nodes) {
            NodeTrack track;
            track.nodeIndex=nodeIndices.at(node.typed_id);
            CopyKeys(node.translation_keys,track.translations,Vector);
            CopyKeys(node.rotation_keys,track.rotations,Rotation);
            CopyKeys(node.scale_keys,track.scales,Vector);
            clip.tracks.push_back(std::move(track));
        }
        model->clips.push_back(std::move(clip));
    }
    if(model->meshes.empty()) throw std::runtime_error("file contains no triangle meshes");
    const auto valid=ValidateModel(*model);
    if(!valid) throw std::runtime_error(valid.error());
    return model;
}
} // namespace

Asset::AssetType UfbxModelLoader::GetType() const noexcept { return Asset::AssetType::FromString("model"); }

Result UfbxModelLoader::Load(const Base::ConstSpan<std::byte> bytes,
                           const Asset::Loading::LoadContext& context) {
    if(bytes.empty()) return Error(context,"input is empty");
    ufbx_load_opts options{};
    options.target_axes=ufbx_axes_left_handed_y_up;
    // Explicit reflection avoids distributing a negative scale through FBX
    // scale-compensating bones and keeps imported model-space +Y upright.
    options.handedness_conversion_axis=UFBX_MIRROR_AXIS_Z;
    options.target_unit_meters=1.0;
    options.space_conversion=UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
    options.geometry_transform_handling=UFBX_GEOMETRY_TRANSFORM_HANDLING_PRESERVE;
    options.inherit_mode_handling=UFBX_INHERIT_MODE_HANDLING_HELPER_NODES;
    options.generate_missing_normals=true;
    options.index_error_handling=UFBX_INDEX_ERROR_HANDLING_ABORT_LOADING;
    options.load_external_files=false;
    options.ignore_embedded=true;
    options.file_format=UFBX_FILE_FORMAT_FBX;
    options.temp_allocator.memory_limit=256*1024*1024;
    options.result_allocator.memory_limit=256*1024*1024;
    ufbx_error error{};
    ScenePtr scene(ufbx_load_memory(bytes.data(),bytes.size(),&options,&error),ufbx_free_scene);
    if(!scene) return Error(context,ErrorMessage(error));
    try {
        return Result::Ok(Asset::Core::AnyAsset::FromShared<ModelAsset>(Import(*scene)));
    } catch(const std::exception& exception) {
        return Error(context,exception.what());
    }
}

} // namespace Engine::Model::Ufbx
