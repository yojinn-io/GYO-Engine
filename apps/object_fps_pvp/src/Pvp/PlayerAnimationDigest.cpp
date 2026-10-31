#include "RetroFPS/Pvp/PlayerPresentation.hpp"

#include "engine/base/Sha256.hpp"

#include <bit>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>

// AnimationContentDigest (protocol-v9 §4.6; the byte order of the
// v8-product-hitbox-data declaration, v8-product-hitbox-digest-scope (a)).
// Every value is written in a fixed order, numbers as their IEEE 754 or
// integer bits in little-endian, strings as a u32 length and their bytes.
namespace fps::pvp {
namespace {
class Writer final {
public:
    void U32(std::uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) bytes_.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
    }
    void U64(std::uint64_t value) {
        for (int shift = 0; shift < 64; shift += 8) bytes_.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
    }
    void F32(float value) { U32(std::bit_cast<std::uint32_t>(value)); }
    void F64(double value) { U64(std::bit_cast<std::uint64_t>(value)); }
    void Size(std::size_t value) {
        if (value > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("animation content digest: a count exceeds 32 bits");
        U32(static_cast<std::uint32_t>(value));
    }
    void Text(const std::string& value) {
        Size(value.size());
        for (const char c : value) bytes_.push_back(static_cast<std::byte>(c));
    }
    void Vec3(const Engine::Math::Vec3& value) { F32(value.x); F32(value.y); F32(value.z); }
    void Rotation(const Engine::Math::Quaternion& value) { F32(value.x); F32(value.y); F32(value.z); F32(value.w); }
    [[nodiscard]] const std::vector<std::byte>& Bytes() const noexcept { return bytes_; }

private:
    std::vector<std::byte> bytes_;
};

} // namespace

std::uint64_t AnimationDigestFromSha256(const std::string& hex) {
    if (hex.size() < 16) throw std::invalid_argument("animation content digest: short SHA-256");
    std::uint64_t value{};
    for (std::size_t byte = 0; byte < 8; ++byte)
        value |= std::stoull(hex.substr(byte * 2, 2), nullptr, 16) << (8 * byte);
    return value ? value : 1;
}

std::uint64_t AnimationContentDigest(const PlayerPresentationDefinition& definition) {
    if (!definition.character || !definition.character->model)
        throw std::invalid_argument("animation content digest requires the character model");
    const auto& model = *definition.character->model;
    Writer out;
    out.Text("object_fps_pvp.animation_digest.v1");

    // presentation.json: every value the pose or the Match's animation rules read.
    out.F32(definition.referenceSpeed);
    out.F64(definition.walkNativeSpeed);
    out.F64(definition.jogNativeSpeed);
    out.F64(definition.transitionSeconds);
    out.F64(definition.maxFrameDeltaSeconds);
    out.F64(definition.shotSeconds);
    out.F64(definition.jumpStartSeconds);
    out.F64(definition.jumpLandSeconds);
    out.F64(definition.hitSeconds);
    out.F64(definition.aimUpRadians);
    out.F64(definition.aimDownRadians);
    out.Text(model.nodes.at(definition.upperBodyRoot).name);

    // The clips the pose samples, in the definition's fixed order.
    for (const std::size_t index : {definition.idleClip, definition.walkClip, definition.jogClip,
             definition.shootClip, definition.reloadClip, definition.jumpStartClip, definition.jumpLoopClip,
             definition.jumpLandClip, definition.deathClip, definition.aimUpClip, definition.aimNeutralClip,
             definition.aimDownClip, definition.hitClip, definition.crouchIdleClip, definition.crouchMoveClip}) {
        const auto& clip = model.clips.at(index);
        out.Text(clip.name);
        out.F64(clip.durationSeconds);
        out.Size(clip.tracks.size());
        for (const auto& track : clip.tracks) {
            out.Text(model.nodes.at(track.nodeIndex).name);
            out.Size(track.translations.size());
            for (const auto& key : track.translations) { out.F64(key.timeSeconds); out.Vec3(key.value); }
            out.Size(track.rotations.size());
            for (const auto& key : track.rotations) { out.F64(key.timeSeconds); out.Rotation(key.value); }
            out.Size(track.scales.size());
            for (const auto& key : track.scales) { out.F64(key.timeSeconds); out.Vec3(key.value); }
        }
    }

    // The skeleton: names, parents (all ones for a root) and reference pose.
    out.Size(model.nodes.size());
    for (const auto& node : model.nodes) {
        out.Text(node.name);
        out.U32(node.parentIndex ? static_cast<std::uint32_t>(*node.parentIndex) : 0xffffffffU);
        out.Vec3(node.localTransform.translation);
        out.Rotation(node.localTransform.rotation);
        out.Vec3(node.localTransform.scale);
    }

    return AnimationDigestFromSha256(Engine::Base::Sha256(out.Bytes()));
}
} // namespace fps::pvp
