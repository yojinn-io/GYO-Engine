#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/model/ModelData.hpp"

// Moving a clip from one node list to another (model.zh-Hant.md section 4.4;
// Design Invariant MD-4). Pure functions: the binding is the author's data;
// which nodes match, which are left out and the translation scale are chosen
// by the caller. BindByName is one way to make a binding, not the only one.

namespace GYO::Engine::Model {

// The first rule broken, in the order the functions check them.
enum class RetargetCode : std::uint8_t {
    // BindByName: a name appears on more than one node of the same list, and
    // a node of the other list has it.
    AmbiguousName = 1,
    // Retarget: translationScale is not finite or not above 0.
    BadTranslationScale = 2,
    // Retarget: a binding names a node out of range, or a node is bound twice.
    BadBinding = 3,
    // Retarget: a bound node's parent chain (in its own list) has a rest scale
    // that is not uniform and above 0, so its rest frame has no single
    // rotation and scale; or a bound source node's own rest scale has a
    // component of 0.
    BadRestScale = 4,
};

[[nodiscard]] constexpr const char* ToString(const RetargetCode code) noexcept {
    switch (code) {
    case RetargetCode::AmbiguousName: return "AmbiguousName";
    case RetargetCode::BadTranslationScale: return "BadTranslationScale";
    case RetargetCode::BadBinding: return "BadBinding";
    case RetargetCode::BadRestScale: return "BadRestScale";
    }
    return "Unknown";
}

using RetargetFailure = Base::Error<RetargetCode>;

// The source node whose animation drives the target node.
struct JointBinding final {
    std::uint32_t source = 0;
    std::uint32_t target = 0;

    [[nodiscard]] friend bool operator==(const JointBinding&, const JointBinding&) = default;
};

struct NameBinding final {
    // Source order.
    std::vector<JointBinding> bindings;
    // The nodes of each list whose name the other list does not have.
    std::vector<std::uint32_t> unmatchedSource;
    std::vector<std::uint32_t> unmatchedTarget;
};

// Binds the nodes whose names are equal (byte for byte). The caller drops
// bindings it does not want before calling Retarget and checks the unmatched
// lists against what it expects.
[[nodiscard]] Base::Result<NameBinding, RetargetFailure> BindByName(std::span<const Node> source,
                                                                    std::span<const Node> target);

struct RetargetSettings final {
    // Multiplies the motion of translations (their difference from the rest
    // translation), measured in model space, for proportions that differ.
    float translationScale = 1.0F;
};

// A clip of the target list made from a clip of the source list (made for
// source.size() nodes; another size is a Programmer Error).
//
// Each bound node's motion relative to its rest pose is carried over, not its
// absolute values, so rest poses may differ in orientation and proportion: a
// source key equal to the source rest gives the target rest. For source node
// s bound to target node t, with S and T the model-space rest rotations of
// their parents (identity for a root):
//
//     C            = conj(T) * S
//     rotation'    = C * rotation * conj(rest_s.rotation) * conj(C) * rest_t.rotation
//     translation' = rest_t.translation + C (translation - rest_s.translation) * k
//     scale'       = rest_t.scale * scale / rest_s.scale   (per component)
//
// where k is translationScale times the source parent's uniform rest scale
// over the target parent's. Tracks of unbound source nodes are dropped;
// target nodes with no bound source keep their rest pose. Key times and the
// duration are copied.
[[nodiscard]] Base::Result<AnimationClip, RetargetFailure> Retarget(std::span<const Node> source,
                                                                    std::span<const Node> target,
                                                                    const AnimationClip& clip,
                                                                    std::span<const JointBinding> bindings,
                                                                    const RetargetSettings& settings = {});

} // namespace GYO::Engine::Model
