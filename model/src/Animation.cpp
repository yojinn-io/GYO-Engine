#include "model/Animation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace Engine::Model {
namespace {
using Result = Base::Result<void, std::string>;

bool Finite(const Vec3 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
bool Finite(const Quaternion q) {
    const float lengthSquared=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    return std::isfinite(q.x)&&std::isfinite(q.y)&&std::isfinite(q.z)&&std::isfinite(q.w)
        && std::isfinite(lengthSquared)&&lengthSquared > 1.0e-12F;
}
bool Finite(const Matrix4& m) {
    return std::all_of(m.values.begin(),m.values.end(),[](float f){return std::isfinite(f);});
}
bool Finite(const Transform& t) { return Finite(t.translation)&&Finite(t.rotation)&&Finite(t.scale); }

Vec3 Lerp(const Vec3 a,const Vec3 b,const float t) {
    return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};
}

Quaternion Normalize(Quaternion q) {
    const float length=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    if (length<=1.0e-12F) return {};
    return {q.x/length,q.y/length,q.z/length,q.w/length};
}

Quaternion Slerp(Quaternion a,Quaternion b,const float t) {
    a=Normalize(a); b=Normalize(b);
    float dot=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
    if (dot<0) { b={-b.x,-b.y,-b.z,-b.w}; dot=-dot; }
    dot=std::clamp(dot,-1.0F,1.0F);
    float left=1-t,right=t;
    if (dot<0.9995F) {
        const float angle=std::acos(dot),divisor=std::sin(angle);
        left=std::sin((1-t)*angle)/divisor;
        right=std::sin(t*angle)/divisor;
    }
    return Normalize({a.x*left+b.x*right,a.y*left+b.y*right,
                      a.z*left+b.z*right,a.w*left+b.w*right});
}

template<class T,class Interpolator>
T Sample(const std::vector<Keyframe<T>>& keys,const double time,T fallback,Interpolator interpolate) {
    if(keys.empty()) return fallback;
    if(time<=keys.front().timeSeconds) return keys.front().value;
    if(time>=keys.back().timeSeconds) return keys.back().value;
    auto next=std::upper_bound(keys.begin(),keys.end(),time,
        [](double t,const Keyframe<T>& key){return t<key.timeSeconds;});
    const auto& previous=*(next-1);
    const double length=next->timeSeconds-previous.timeSeconds;
    return interpolate(previous.value,next->value,static_cast<float>((time-previous.timeSeconds)/length));
}

template<class T> bool ValidKeys(const std::vector<Keyframe<T>>& keys) {
    double previous=-std::numeric_limits<double>::infinity();
    for(const auto& key:keys) {
        if(!std::isfinite(key.timeSeconds)||key.timeSeconds<=previous||!Finite(key.value)) return false;
        previous=key.timeSeconds;
    }
    return true;
}

Result ResolveGlobals(const ModelAsset& model,Pose& output) {
    output.globalTransforms.resize(model.nodes.size());
    for(std::size_t i=0;i<model.nodes.size();++i) {
        const auto parent=model.nodes[i].parentIndex;
        if(parent&&*parent>=i) return Result::Err("Model nodes must be ordered parent before child.");
        Matrix4 local=ToMatrix(output.localTransforms[i]);
        output.globalTransforms[i]=parent?Multiply(output.globalTransforms[*parent],local):local;
        if(!Finite(output.globalTransforms[i])) return Result::Err("Model pose contains a non-finite transform.");
    }
    return Result::Ok();
}

Vec3 TransformNormal(const Matrix4& matrix,const Vec3 normal) {
    const auto& m=matrix.values;
    // Cofactor matrix / determinant is the inverse transpose of the 3x3.
    const float c00=m[5]*m[10]-m[9]*m[6],c01=m[9]*m[2]-m[1]*m[10],c02=m[1]*m[6]-m[5]*m[2];
    const float c10=m[8]*m[6]-m[4]*m[10],c11=m[0]*m[10]-m[8]*m[2],c12=m[4]*m[2]-m[0]*m[6];
    const float c20=m[4]*m[9]-m[8]*m[5],c21=m[8]*m[1]-m[0]*m[9],c22=m[0]*m[5]-m[4]*m[1];
    const float determinant=m[0]*c00+m[4]*c01+m[8]*c02;
    if(std::abs(determinant)<1.0e-12F) return normal;
    Vec3 n{(c00*normal.x+c01*normal.y+c02*normal.z)/determinant,
           (c10*normal.x+c11*normal.y+c12*normal.z)/determinant,
           (c20*normal.x+c21*normal.y+c22*normal.z)/determinant};
    const float length=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
    return length>1.0e-12F?Vec3{n.x/length,n.y/length,n.z/length}:normal;
}
} // namespace

Result ValidateModel(const ModelAsset& model) {
    if(model.nodes.empty()) return Result::Err("Model requires at least one node.");
    for(std::size_t i=0;i<model.nodes.size();++i) {
        const auto& node=model.nodes[i];
        if((node.parentIndex&&*node.parentIndex>=i)||!Finite(node.localTransform))
            return Result::Err("Model node has an invalid hierarchy or transform.");
    }
    for(const auto& mesh:model.meshes) {
        if(mesh.nodeIndex>=model.nodes.size()||mesh.materialIndex>=model.materials.size()||
           mesh.vertices.empty()||mesh.indices.empty()||mesh.indices.size()%3!=0||!Finite(mesh.geometryToNode))
            return Result::Err("Model mesh has an invalid node, material, transform or topology.");
        for(const auto index:mesh.indices) if(index>=mesh.vertices.size())
            return Result::Err("Model mesh index exceeds its vertex count.");
        for(const auto& joint:mesh.joints) if(joint.nodeIndex>=model.nodes.size()||!Finite(joint.geometryToJoint))
            return Result::Err("Model skin joint is invalid.");
        for(const auto& v:mesh.vertices) {
            if(!Finite(v.position)||!Finite(v.normal)||!std::isfinite(v.uv.x)||!std::isfinite(v.uv.y))
                return Result::Err("Model vertex contains non-finite attributes.");
            float total=0;
            for(std::size_t k=0;k<4;++k) {
                if(!std::isfinite(v.weights[k])||v.weights[k]<0||
                   (v.weights[k]>0&&v.joints[k]>=mesh.joints.size()))
                    return Result::Err("Model vertex has invalid skin weights or joint indices.");
                total+=v.weights[k];
            }
            if(total>0&&std::abs(total-1.0F)>0.0001F)
                return Result::Err("Model skin weights must sum to one.");
        }
    }
    std::unordered_set<std::string> names;
    for(const auto& clip:model.clips) {
        if(clip.name.empty()||!names.insert(clip.name).second||
           !std::isfinite(clip.durationSeconds)||clip.durationSeconds<0)
            return Result::Err("Model animation has an invalid name or duration.");
        std::unordered_set<std::size_t> trackedNodes;
        for(const auto& track:clip.tracks) {
            if(track.nodeIndex>=model.nodes.size()||!trackedNodes.insert(track.nodeIndex).second||
               !ValidKeys(track.translations)||!ValidKeys(track.rotations)||!ValidKeys(track.scales))
                return Result::Err("Model animation has invalid node tracks or keyframes.");
        }
    }
    return Result::Ok();
}

Result MakeDefaultPose(const ModelAsset& model,Pose& output) {
    output.localTransforms.resize(model.nodes.size());
    for(std::size_t i=0;i<model.nodes.size();++i) output.localTransforms[i]=model.nodes[i].localTransform;
    return ResolveGlobals(model,output);
}

Result SamplePose(const ModelAsset& model,const std::size_t clipIndex,double seconds,
                  const PlaybackMode mode,Pose& output) {
    if(clipIndex>=model.clips.size()||!std::isfinite(seconds))
        return Result::Err("Animation sampling requires a valid clip and finite time.");
    const auto& clip=model.clips[clipIndex];
    if(!std::isfinite(clip.durationSeconds)||clip.durationSeconds<0)
        return Result::Err("Animation duration must be finite and non-negative.");
    if(mode!=PlaybackMode::Clamp&&mode!=PlaybackMode::Loop)
        return Result::Err("Animation playback mode is invalid.");
    if(mode==PlaybackMode::Loop&&clip.durationSeconds>0) {
        seconds=std::fmod(seconds,clip.durationSeconds);
        if(seconds<0) seconds+=clip.durationSeconds;
    } else seconds=std::clamp(seconds,0.0,clip.durationSeconds);
    output.localTransforms.resize(model.nodes.size());
    for(std::size_t i=0;i<model.nodes.size();++i) output.localTransforms[i]=model.nodes[i].localTransform;
    for(const auto& track:clip.tracks) {
        if(track.nodeIndex>=model.nodes.size()) return Result::Err("Animation track node is out of range.");
        auto& t=output.localTransforms[track.nodeIndex];
        t.translation=Sample(track.translations,seconds,t.translation,Lerp);
        t.rotation=Sample(track.rotations,seconds,t.rotation,Slerp);
        t.scale=Sample(track.scales,seconds,t.scale,Lerp);
    }
    return ResolveGlobals(model,output);
}

Result SkinMesh(const ModelAsset& model,const std::size_t meshIndex,const Pose& pose,
                std::vector<SkinnedVertex>& output) {
    if(meshIndex>=model.meshes.size()||pose.globalTransforms.size()!=model.nodes.size())
        return Result::Err("Skinning requires a valid mesh and a complete pose.");
    const auto& mesh=model.meshes[meshIndex];
    if(mesh.nodeIndex>=pose.globalTransforms.size()) return Result::Err("Mesh node is out of range.");
    const Matrix4 rigid=Multiply(pose.globalTransforms[mesh.nodeIndex],mesh.geometryToNode);
    std::vector<Matrix4> palette;
    palette.reserve(mesh.joints.size());
    for(const auto& joint:mesh.joints) {
        if(joint.nodeIndex>=pose.globalTransforms.size()) return Result::Err("Skin joint node is out of range.");
        palette.push_back(Multiply(pose.globalTransforms[joint.nodeIndex],joint.geometryToJoint));
    }
    output.resize(mesh.vertices.size());
    for(std::size_t i=0;i<mesh.vertices.size();++i) {
        const auto& source=mesh.vertices[i];
        Matrix4 blended; blended.values.fill(0);
        float total=0;
        for(std::size_t k=0;k<4;++k) {
            if(source.weights[k]<=0) continue;
            if(source.joints[k]>=palette.size()) return Result::Err("Skin vertex joint is out of range.");
            const auto& matrix=palette[source.joints[k]];
            for(std::size_t j=0;j<16;++j) blended.values[j]+=matrix.values[j]*source.weights[k];
            total+=source.weights[k];
        }
        if(total==0) blended=rigid;
        output[i]={TransformPoint(blended,source.position),TransformNormal(blended,source.normal),source.uv};
        if(!Finite(output[i].position)||!Finite(output[i].normal))
            return Result::Err("Skinned vertex is non-finite.");
    }
    return Result::Ok();
}

} // namespace Engine::Model
