#include "engine/model/Animation.hpp"

#include "engine/math/scalar/Scalar.hpp"

#include <cmath>
#include <limits>
#include <unordered_set>

namespace Engine::Model {
using Math::Quaternion;
using Math::Vec3;
namespace {
using TransferResult = ModelResult<AnimationClip>;

// Normalized rotation composition: Product(a, b) applies b first, then a.
Quaternion Product(const Quaternion a,const Quaternion b) { return Math::Normalize(Math::Multiply(a,b)); }
bool UniformPositive(const Vec3 scale) {
    const float largest=Math::Max(Math::Max(scale.x,scale.y),scale.z);
    const float smallest=Math::Min(Math::Min(scale.x,scale.y),scale.z);
    return Math::IsFinite(scale)&&smallest>0&&largest-smallest<=largest*0.0001F;
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
        frame.rotation=Product(parent.rotation,Math::Normalize(node.localTransform.rotation));
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
        return Base::Err(ModelError::Make(ModelErrorCode::InvalidArgument, "Animation transfer requires a clip, bindings and a finite positive translation scale."));
    const auto sourceValidation=ValidateModel(source);
    if(!sourceValidation) return Base::Err(ModelError::Make(sourceValidation.error().code, "Invalid animation source: "+sourceValidation.error().message, sourceValidation.error().detail));
    const auto targetValidation=ValidateModel(target);
    if(!targetValidation) return Base::Err(ModelError::Make(targetValidation.error().code, "Invalid animation target: "+targetValidation.error().message, targetValidation.error().detail));

    constexpr auto unmapped=std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> targets(source.nodes.size(),unmapped);
    std::vector<bool> sourceSelected(source.nodes.size()),targetSelected(target.nodes.size());
    for(const auto binding:bindings) {
        if(binding.sourceNodeIndex>=source.nodes.size()||binding.targetNodeIndex>=target.nodes.size())
            return Base::Err(ModelError::Make(ModelErrorCode::IncompatibleAnimation, "Animation transfer binding index is out of range."));
        if(sourceSelected[binding.sourceNodeIndex]||targetSelected[binding.targetNodeIndex])
            return Base::Err(ModelError::Make(ModelErrorCode::IncompatibleAnimation, "Animation transfer bindings must be one-to-one."));
        targets[binding.sourceNodeIndex]=binding.targetNodeIndex;
        sourceSelected[binding.sourceNodeIndex]=targetSelected[binding.targetNodeIndex]=true;
    }
    for(const auto binding:bindings) {
        const auto sourceParent=source.nodes[binding.sourceNodeIndex].parentIndex;
        const auto targetParent=target.nodes[binding.targetNodeIndex].parentIndex;
        if(sourceParent.has_value()!=targetParent.has_value()||
           (sourceParent&&(!sourceSelected[*sourceParent]||targets[*sourceParent]!=*targetParent)))
            return Base::Err(ModelError::Make(ModelErrorCode::IncompatibleAnimation, "Animation transfer requires matching mapped parent hierarchies, including roots."));
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
            return Base::Err(ModelError::Make(ModelErrorCode::IncompatibleAnimation, "Animation transfer exclusions must be unique, unmapped, non-skeletal mesh nodes."));
    }
    std::vector<ReferenceFrame> sourceFrames,targetFrames;
    if(!ResolveReferenceFrames(source,sourceSelected,sourceFrames)||
       !ResolveReferenceFrames(target,targetSelected,targetFrames))
        return Base::Err(ModelError::Make(ModelErrorCode::IncompatibleAnimation, "Animation transfer supports only finite positive uniform reference scales."));

    const auto& original=source.clips[sourceClipIndex];
    AnimationClip output{original.name,original.durationSeconds,{}};
    output.tracks.reserve(original.tracks.size());
    for(const auto& sourceTrack:original.tracks) {
        const auto sourceIndex=sourceTrack.nodeIndex;
        if(targets[sourceIndex]==unmapped) {
            if(excluded.contains(sourceIndex)) continue;
            return Base::Err(ModelError::Make(ModelErrorCode::IncompatibleAnimation, "Animation source track "+std::to_string(sourceIndex)+" has no binding or explicit exclusion."));
        }
        const auto targetIndex=targets[sourceIndex];
        const auto& sourceNode=source.nodes[sourceIndex];
        const auto& targetNode=target.nodes[targetIndex];
        const auto& sourceRest=sourceNode.localTransform;
        const auto& targetRest=targetNode.localTransform;
        const auto sourceParent=sourceNode.parentIndex?sourceFrames[*sourceNode.parentIndex]:ReferenceFrame{};
        const auto targetParent=targetNode.parentIndex?targetFrames[*targetNode.parentIndex]:ReferenceFrame{};
        const auto correction=Product(Math::Conjugate(targetParent.rotation),sourceParent.rotation);
        const auto correctionMatrix=ToMatrix({{},correction,{1,1,1}});
        const double localTranslationScale=static_cast<double>(translationScale)*sourceParent.scale/targetParent.scale;
        if(!std::isfinite(localTranslationScale)||localTranslationScale<=0)
            return Base::Err(ModelError::Make(ModelErrorCode::IncompatibleAnimation, "Animation transfer translation scale overflowed."));

        NodeTrack track;
        track.nodeIndex=targetIndex;
        track.translations.reserve(sourceTrack.translations.size());
        track.rotations.reserve(sourceTrack.rotations.size());
        track.scales.reserve(sourceTrack.scales.size());
        for(const auto& key:sourceTrack.translations) {
            const Vec3 delta=key.value-sourceRest.translation;
            const auto rotated=TransformPoint(correctionMatrix,delta);
            const Vec3 value{
                static_cast<float>(targetRest.translation.x+rotated.x*localTranslationScale),
                static_cast<float>(targetRest.translation.y+rotated.y*localTranslationScale),
                static_cast<float>(targetRest.translation.z+rotated.z*localTranslationScale)};
            if(!Math::IsFinite(value)) return Base::Err(ModelError::Make(ModelErrorCode::IncompatibleAnimation, "Animation transfer produced non-finite translation keys."));
            track.translations.push_back({key.timeSeconds,value});
        }
        for(const auto& key:sourceTrack.rotations) {
            const auto delta=Product(Math::Normalize(key.value),Math::Conjugate(Math::Normalize(sourceRest.rotation)));
            const auto converted=Product(Product(correction,delta),Math::Conjugate(correction));
            track.rotations.push_back({key.timeSeconds,Product(converted,Math::Normalize(targetRest.rotation))});
        }
        for(const auto& key:sourceTrack.scales) {
            if(!UniformPositive(key.value))
                return Base::Err(ModelError::Make(ModelErrorCode::IncompatibleAnimation, "Animation transfer supports only positive uniform animated scales."));
            const Vec3 value{
                static_cast<float>(static_cast<double>(targetRest.scale.x)*key.value.x/sourceRest.scale.x),
                static_cast<float>(static_cast<double>(targetRest.scale.y)*key.value.y/sourceRest.scale.y),
                static_cast<float>(static_cast<double>(targetRest.scale.z)*key.value.z/sourceRest.scale.z)};
            if(!UniformPositive(value)) return Base::Err(ModelError::Make(ModelErrorCode::IncompatibleAnimation, "Animation transfer scale keys overflowed or collapsed."));
            track.scales.push_back({key.timeSeconds,value});
        }
        output.tracks.push_back(std::move(track));
    }
    return std::move(output);
}

} // namespace Engine::Model
