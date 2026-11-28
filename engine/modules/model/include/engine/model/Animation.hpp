#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "engine/math/linear/Matrix4.hpp"
#include "engine/model/ModelData.hpp"

// Skeletal animation, layer 1 (model.zh-Hant.md section 4; Design Invariant
// MD-3): pure functions of their arguments. No clock, no World, no state kept
// between calls; which clip plays, at what time and with which weights is the
// caller's (layer 2, the per-entity player, is not here; skinning for display
// is the renderer's).
//
// Inputs come from a valid ModelData (Validate) or a clip ValidateClip
// accepts. Output spans are the caller's storage, one element per node; a
// size that does not match, or a clip made for another node count, is a
// Programmer Error.
//
// The same inputs give the same output on one build. Results may differ in
// the last bits between platforms and compilers (sin, acos and the order of
// float operations are not fixed): use them where each machine decides for
// itself, not where two machines must agree bit for bit.

namespace GYO::Engine::Model {

// What a time outside [0, duration] means.
enum class Wrap : std::uint8_t {
    // Holds the ends: before 0 is the pose at 0, after the duration the pose
    // at the duration.
    Clamp = 1,
    // Repeats: the time modulo the duration (negative times count back from
    // the end). A clip of duration 0 is always at 0.
    Loop = 2,
};

// Every node at its rest pose.
void RestPose(std::span<const Node> nodes, std::span<LocalTransform> out);

// The pose of the clip at the time, in seconds. A node without a track, or a
// channel without keys, takes its rest value. Before a channel's first key is
// that key's value, after its last key the last key's value; between two keys
// translation and scale interpolate linearly and rotation by shortest-path
// slerp.
void SampleClip(std::span<const Node> nodes, const AnimationClip& clip, float seconds, Wrap wrap,
                std::span<LocalTransform> out);

// out[i] = a[i] mixed toward b[i] by weight: translation and scale linearly,
// rotation by shortest-path slerp. weight 0 gives a, 1 gives b; other values
// are not clamped. out may be a or b.
void Blend(std::span<const LocalTransform> a, std::span<const LocalTransform> b, float weight,
           std::span<LocalTransform> out);

// The same per node with each node's own weight: out[i] = a[i] mixed toward
// b[i] by weights[i]. A mask is weights of 0 and 1. out may be a or b.
void Blend(std::span<const LocalTransform> a, std::span<const LocalTransform> b, std::span<const float> weights,
           std::span<LocalTransform> out);

// out[i] = base[i] plus weight times the motion of motion[i] relative to
// reference[i]: translation and scale add weight * (motion - reference);
// rotation is base * slerp(identity, inverse(reference) * motion, weight),
// the change in the node's own frame. weight 1 with base equal to reference
// gives motion; weight 0 or motion equal to reference gives base. weight is
// not clamped. out may be any of the inputs.
void ApplyAdditive(std::span<const LocalTransform> base, std::span<const LocalTransform> motion,
                   std::span<const LocalTransform> reference, float weight, std::span<LocalTransform> out);

// The instants of a clip that its time crosses moving from `from` to `to`,
// in seconds as SampleClip takes them (Wrap::Loop times need not be wrapped).
// times are the instants, ascending, each within [0, duration]. Writes their
// indices to out in the order they are crossed and returns how many.
//
// Forward (from < to) crosses [from, to); backward (to < from) crosses
// (to, from]: consecutive intervals cross each instant once. Wrap::Clamp
// holds the time within [0, duration], and the interval that reaches the end
// (or, backward, the start) also crosses an instant there. Wrap::Loop crosses
// the end into the start (or back); an interval of a whole duration or more
// crosses each instant once, from where it starts. from == to, or a clip of
// duration 0, crosses nothing. out must hold times.size() indices.
std::size_t CrossedEvents(std::span<const float> times, float from, float to, float duration, Wrap wrap,
                          std::span<std::uint32_t> out);

// Each node's transform in model space: the parent's times the node's own
// (Math::Multiply(parent, local)), a root's own alone.
void ComputeModelSpace(std::span<const Node> nodes, std::span<const LocalTransform> local,
                       std::span<Math::Matrix4> out);

} // namespace GYO::Engine::Model
