#include "model/Animation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace Engine::Model {
namespace {
using TransferResult = Base::Result<AnimationClip, std::string>;

Quaternion Normalize(const Quaternion q) {
    const float length=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    return {q.x/length,q.y/length,q.z/length,q.w/length};
}
Quaternion Inverse(const Quaternion q) { return {-q.x,-q.y,-q.z,q.w}; }
Quaternion Product(const Quaternion a,const Quaternion b) {
    return Normalize({a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
                      a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
                      a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
                      a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z});
}
bool Finite(const Vec3 v) {
    return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);
}
bool UniformPositive(const Vec3 scale) {
    const float largest=std::max({scale.x,scale.y,scale.z});
    const float smallest=std::min({scale.x,scale.y,scale.z});
    return Finite(scale)&&smallest>0&&largest-smallest<=largest*0.0001F;
}
struct ReferenceFrame final {
    Quaternion rotation{};
    double scale{1};
};
bool ResolveReferenceFrames(const ModelAsset& model,
                            const std::vector<bool>& selected,
                            std::vector<ReferenceFrame>& frames) {
    frames.resize(model.nodes.size());
    for(std::size_t index=0;index<model.nodes.size();++index) {
        if(!selected[index]) continue;
        const auto& node=model.nodes[index];
        if(!UniformPositive(node.localTransform.scale)) return false;
        const auto parent=node.parentIndex?frames[*node.parentIndex]:ReferenceFrame{};
        auto& frame=frames[index];
        frame.rotation=Product(parent.rotation,Normalize(node.localTransform.rotation));
        frame.scale=parent.scale*static_cast<double>(node.localTransform.scale.x);
        if(!std::isfinite(frame.scale)||frame.scale<=0) return false;
    }
    return true;
}
} // namespace

TransferResult TransferCompatibleAnimation(
    const ModelAsset& source,const ModelAsset& target,const std::size_t sourceClipIndex,
    const std::span<const AnimationNodeBinding> bindings,const float translationScale,
    const std::span<const std::size_t> excludedSourceTrackNodes) {
    if(sourceClipIndex>=source.clips.size()||bindings.empty()||
       !std::isfinite(translationScale)||translationScale<=0)
        return TransferResult::Err("Animation transfer requires a clip, bindings and a finite positive translation scale.");
    const auto sourceValidation=ValidateModel(source);
    if(!sourceValidation) return TransferResult::Err("Invalid animation source: "+sourceValidation.error());
    const auto targetValidation=ValidateModel(target);
    if(!targetValidation) return TransferResult::Err("Invalid animation target: "+targetValidation.error());

    constexpr auto unmapped=std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> targets(source.nodes.size(),unmapped);
    std::vector<bool> sourceSelected(source.nodes.size()),targetSelected(target.nodes.size());
    for(const auto binding:bindings) {
        if(binding.sourceNodeIndex>=source.nodes.size()||binding.targetNodeIndex>=target.nodes.size())
            return TransferResult::Err("Animation transfer binding index is out of range.");
        if(sourceSelected[binding.sourceNodeIndex]||targetSelected[binding.targetNodeIndex])
            return TransferResult::Err("Animation transfer bindings must be one-to-one.");
        targets[binding.sourceNodeIndex]=binding.targetNodeIndex;
        sourceSelected[binding.sourceNodeIndex]=targetSelected[binding.targetNodeIndex]=true;
    }
    for(const auto binding:bindings) {
        const auto sourceParent=source.nodes[binding.sourceNodeIndex].parentIndex;
        const auto targetParent=target.nodes[binding.targetNodeIndex].parentIndex;
        if(sourceParent.has_value()!=targetParent.has_value()||
           (sourceParent&&(!sourceSelected[*sourceParent]||targets[*sourceParent]!=*targetParent)))
            return TransferResult::Err("Animation transfer requires matching mapped parent hierarchies, including roots.");
    }

    // Skin joints and their ancestors cannot be silently discarded as geometry
    // tracks. Mapped nodes are protected even when a source has no skin data.
    auto skeletalNodes=sourceSelected;
    std::vector<bool> meshNodes(source.nodes.size());
    for(const auto& mesh:source.meshes) {
        meshNodes[mesh.nodeIndex]=true;
        for(const auto& joint:mesh.joints) {
            std::optional<std::size_t> node=joint.nodeIndex;
            while(node) {
                skeletalNodes[*node]=true;
                node=source.nodes[*node].parentIndex;
            }
        }
    }
    std::unordered_set<std::size_t> excluded;
    for(const auto node:excludedSourceTrackNodes) {
        if(node>=source.nodes.size()||!excluded.insert(node).second||
           !meshNodes[node]||skeletalNodes[node])
            return TransferResult::Err("Animation transfer exclusions must be unique, unmapped, non-skeletal mesh nodes.");
    }
    std::vector<ReferenceFrame> sourceFrames,targetFrames;
    if(!ResolveReferenceFrames(source,sourceSelected,sourceFrames)||
       !ResolveReferenceFrames(target,targetSelected,targetFrames))
        return TransferResult::Err("Animation transfer supports only finite positive uniform reference scales.");

    const auto& original=source.clips[sourceClipIndex];
    AnimationClip output{original.name,original.durationSeconds,{}};
    output.tracks.reserve(original.tracks.size());
    for(const auto& sourceTrack:original.tracks) {
        const auto sourceIndex=sourceTrack.nodeIndex;
        if(targets[sourceIndex]==unmapped) {
            if(excluded.contains(sourceIndex)) continue;
            return TransferResult::Err("Animation source track "+std::to_string(sourceIndex)+" has no binding or explicit exclusion.");
        }
        const auto targetIndex=targets[sourceIndex];
        const auto& sourceNode=source.nodes[sourceIndex];
        const auto& targetNode=target.nodes[targetIndex];
        const auto& sourceRest=sourceNode.localTransform;
        const auto& targetRest=targetNode.localTransform;
        const auto sourceParent=sourceNode.parentIndex?sourceFrames[*sourceNode.parentIndex]:ReferenceFrame{};
        const auto targetParent=targetNode.parentIndex?targetFrames[*targetNode.parentIndex]:ReferenceFrame{};
        const auto correction=Product(Inverse(targetParent.rotation),sourceParent.rotation);
        const auto correctionMatrix=ToMatrix({{},correction,{1,1,1}});
        const double localTranslationScale=static_cast<double>(translationScale)*sourceParent.scale/targetParent.scale;
        if(!std::isfinite(localTranslationScale)||localTranslationScale<=0)
            return TransferResult::Err("Animation transfer translation scale overflowed.");

        NodeTrack track;
        track.nodeIndex=targetIndex;
        track.translations.reserve(sourceTrack.translations.size());
        track.rotations.reserve(sourceTrack.rotations.size());
        track.scales.reserve(sourceTrack.scales.size());
        for(const auto& key:sourceTrack.translations) {
            const Vec3 delta{key.value.x-sourceRest.translation.x,key.value.y-sourceRest.translation.y,
                             key.value.z-sourceRest.translation.z};
            const auto rotated=TransformPoint(correctionMatrix,delta);
            const Vec3 value{
                static_cast<float>(targetRest.translation.x+rotated.x*localTranslationScale),
                static_cast<float>(targetRest.translation.y+rotated.y*localTranslationScale),
                static_cast<float>(targetRest.translation.z+rotated.z*localTranslationScale)};
            if(!Finite(value)) return TransferResult::Err("Animation transfer produced non-finite translation keys.");
            track.translations.push_back({key.timeSeconds,value});
        }
        for(const auto& key:sourceTrack.rotations) {
            const auto delta=Product(Normalize(key.value),Inverse(Normalize(sourceRest.rotation)));
            const auto converted=Product(Product(correction,delta),Inverse(correction));
            track.rotations.push_back({key.timeSeconds,Product(converted,Normalize(targetRest.rotation))});
        }
        for(const auto& key:sourceTrack.scales) {
            if(!UniformPositive(key.value))
                return TransferResult::Err("Animation transfer supports only positive uniform animated scales.");
            const Vec3 value{
                static_cast<float>(static_cast<double>(targetRest.scale.x)*key.value.x/sourceRest.scale.x),
                static_cast<float>(static_cast<double>(targetRest.scale.y)*key.value.y/sourceRest.scale.y),
                static_cast<float>(static_cast<double>(targetRest.scale.z)*key.value.z/sourceRest.scale.z)};
            if(!UniformPositive(value)) return TransferResult::Err("Animation transfer scale keys overflowed or collapsed.");
            track.scales.push_back({key.timeSeconds,value});
        }
        output.tracks.push_back(std::move(track));
    }
    return TransferResult::Ok(std::move(output));
}

} // namespace Engine::Model
