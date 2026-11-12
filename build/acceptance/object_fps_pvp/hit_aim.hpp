#pragma once
// The acceptance probes' aim at one part of a target (v8-product-real-match-aim (a)):
// the target posed by the product's hit model from the shooter's snapshot, the aim
// point the middle of that part's first shape, from the shooter's stance eye.
#include "RetroFPS/Sim/PlayerHitbox.hpp"
#include "RetroFPS/Present/PlayerPresentation.hpp"
#include "RetroFPS/Pvp/PvpMatch.hpp"
#include "RetroFPS/Sim/ShotQuery.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace fps::pvp {

struct HitAim final {
    float yaw{};
    float pitch{};
};

class HitAimModel final {
public:
    // contentRoot: the product content the Match judges with (a folder with content.json).
    HitAimModel(const std::filesystem::path& contentRoot, float bodyHeight) {
        std::string error;
        const auto hitbox = LoadPlayerHitbox(contentRoot, error);
        if (!hitbox) throw std::runtime_error(error);
        const auto pose = LoadPlayerPoseDefinition(contentRoot, bodyHeight, error);
        if (!pose) throw std::runtime_error(error);
        model_ = MakePosedPlayerHitModel(pose, *hitbox);
    }

    [[nodiscard]] HitAim Aim(const Arena& arena, const StanceRules& stance, const WorldSnapshot& snapshot,
                             PlayerId shooterId, PlayerId targetId, HitPart part) const {
        const auto& shooter = Find(snapshot.players, shooterId);
        const auto& target = Find(snapshot.players, targetId);
        const auto& combat = Find(snapshot.combat, targetId);
        const auto& animation = Find(snapshot.animation, targetId);
        std::vector<PlayerWorldHitShape> shapes;
        model_->Shapes({arena, stance, target, combat, animation, snapshot.tick}, shapes);
        const auto shape = std::find_if(shapes.begin(), shapes.end(), [&](const auto& s) { return s.part == part; });
        if (shape == shapes.end()) throw std::runtime_error("the posed hit model has no shape of the aimed part");
        const auto& capsule = std::get<Engine::Math::Capsule>(shape->volume);
        const auto point = (capsule.segmentStart + capsule.segmentEnd) * 0.5F;
        const Engine::Math::Vec3 eye{shooter.position.x, shooter.position.y + StanceEyeHeight(arena, stance, shooter.crouched),
                                     shooter.position.z};
        const auto direction = Engine::Math::Normalize(point - eye);
        return {std::atan2(direction.x, direction.z), -std::asin(direction.y)};
    }

private:
    template <class Item>
    static const Item& Find(const std::vector<Item>& items, PlayerId id) {
        const auto found = std::find_if(items.begin(), items.end(), [&](const auto& item) { return item.playerId == id; });
        if (found == items.end()) throw std::runtime_error("the snapshot lacks the aimed player");
        return *found;
    }
    std::shared_ptr<const PlayerHitModel> model_;
};

} // namespace fps::pvp
