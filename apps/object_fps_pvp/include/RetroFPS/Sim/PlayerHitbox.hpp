#pragma once

#include "RetroFPS/App/CharacterPresentationDefinition.hpp"
#include "RetroFPS/Sim/Combat.hpp"
#include "RetroFPS/Sim/PlayerHitModel.hpp"
#include "engine/math/linear/Vec3.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::Asset { class AssetManager; }

namespace fps::pvp {
inline namespace sim {
struct PlayerPresentationDefinition;

// One hit shape of the player hitbox Data Contract (docs/object_fps_pvp/
// hitbox-shapes.zh-Hant.md): a sphere carried by a bone, or a capsule between
// two bone origins. Arms have no shape: a ray passes through them (pv9 §2).
struct PlayerHitShape final {
    enum class Kind : std::uint8_t { Sphere, Capsule };
    HitPart part{HitPart::Body};
    Kind kind{Kind::Capsule};
    std::size_t node{};   // the sphere's bone, or the capsule's from bone
    std::size_t nodeTo{}; // the capsule's to bone
    // The sphere centre in its bone's local frame (the skeleton's units).
    Engine::Math::Vec3 offset{};
    float radius{}; // metres in the presented body (model minus anchor, times scale)
    [[nodiscard]] bool operator==(const PlayerHitShape& other) const noexcept {
        return part == other.part && kind == other.kind && node == other.node && nodeTo == other.nodeTo &&
               offset.x == other.offset.x && offset.y == other.offset.y && offset.z == other.offset.z &&
               radius == other.radius;
    }
};

struct PlayerHitbox final {
    std::vector<PlayerHitShape> shapes;
    bool operator==(const PlayerHitbox&) const = default;
};

// The strict reader of the contract against a character's skeleton: version
// 1, no unknown or missing key, the three parts, each bone exactly once in the
// skeleton, radii in [0.01, 0.5] m.
[[nodiscard]] std::optional<PlayerHitbox> ReadPlayerHitbox(std::string_view text,
    const CharacterPresentationDefinition& character, std::string& error);

// The Match's path: the product hitbox with its character's model, no texture.
[[nodiscard]] std::optional<PlayerHitbox> LoadPlayerHitbox(Engine::Asset::AssetManager& assets, std::string& error);
[[nodiscard]] std::optional<PlayerHitbox> LoadPlayerHitbox(const std::filesystem::path& contentRoot, std::string& error);

// The product's hit model (v8-product-hitbox-shot): the body the Client
// presents for the same authority values at the tick (PlayerFrameFromAuthority,
// AdvancePlayerLocomotion from a fresh state, ResolvePlayerActions,
// SamplePlayerBodyPose), the shapes placed on its bones and taken to the world
// as the renderer draws the body (PlayerBodyPointToWorld). The hitbox must be
// read against the definition's character (both from one content root): the
// shapes keep bone indices only, so this checks that they are inside the
// skeleton, not that the skeleton is the one they were read against.
[[nodiscard]] std::shared_ptr<const PlayerHitModel> MakePosedPlayerHitModel(
    std::shared_ptr<const PlayerPresentationDefinition> definition, PlayerHitbox hitbox);

} // namespace sim
} // namespace fps::pvp
