#include "RetroFPS/Sim/PlayerHitbox.hpp"
#include "RetroFPS/Sim/PlayerPose.hpp"

#include "engine/math/linear/Matrix4.hpp"

#include <stdexcept>
#include <string>
#include <utility>

// The posed hit model (v8-product-hitbox-shot S2): the judged pose is the
// presented pose (hitbox-plan §3), so every step is the Client's own function.
namespace fps::pvp {
inline namespace sim {
namespace {
class PosedHitModel final : public PlayerHitModel {
public:
    PosedHitModel(std::shared_ptr<const PlayerPresentationDefinition> definition, PlayerHitbox hitbox)
        : definition_(std::move(definition)), hitbox_(std::move(hitbox)) {
        if (!definition_ || !definition_->character || !definition_->character->model)
            throw std::invalid_argument("posed hit model requires a resolved character");
        const auto nodes = definition_->character->model->nodes.size();
        if (hitbox_.shapes.empty()) throw std::invalid_argument("posed hit model requires shapes");
        for (const auto& shape : hitbox_.shapes)
            if (shape.node >= nodes || (shape.kind == PlayerHitShape::Kind::Capsule && shape.nodeTo >= nodes))
                throw std::invalid_argument("posed hit model shapes name bones outside the character");
    }

    void Shapes(const PlayerHitInput& input, std::vector<PlayerWorldHitShape>& output) const override {
        const auto& definition = *definition_;
        // The Client samples this tick's snapshot at alpha 0: both ends are its value.
        const auto frame = PlayerFrameFromAuthority(input.player, input.combat,
            InterpolatePlayerAnimation(input.animation, input.animation, 0), static_cast<double>(input.tick));
        std::string error;
        PlayerLocomotionState state;
        if (!AdvancePlayerLocomotion(state, frame, definition, error)) throw std::runtime_error(error);
        PlayerActionPose actions;
        if (!ResolvePlayerActions(definition, state, frame, actions, error)) throw std::runtime_error(error);
        Engine::Model::Pose pose;
        if (!SamplePlayerBodyPose(definition, state, actions, pose, error)) throw std::runtime_error(error);
        const auto world = [&](std::size_t node, const Engine::Math::Vec3& local) {
            return PlayerBodyPointToWorld(definition, input.player.position, input.player.yaw,
                Engine::Math::TransformPoint(pose.globalTransforms.at(node), local));
        };
        for (const auto& shape : hitbox_.shapes) {
            if (shape.kind == PlayerHitShape::Kind::Sphere) {
                const auto centre = world(shape.node, shape.offset);
                output.push_back({shape.part, Engine::Math::Capsule{centre, centre, shape.radius}});
            } else {
                output.push_back({shape.part, Engine::Math::Capsule{world(shape.node, {}), world(shape.nodeTo, {}),
                    shape.radius}});
            }
        }
    }

private:
    std::shared_ptr<const PlayerPresentationDefinition> definition_;
    PlayerHitbox hitbox_;
};
} // namespace

std::shared_ptr<const PlayerHitModel> MakePosedPlayerHitModel(
    std::shared_ptr<const PlayerPresentationDefinition> definition, PlayerHitbox hitbox) {
    return std::make_shared<const PosedHitModel>(std::move(definition), std::move(hitbox));
}
} // namespace sim
} // namespace fps::pvp
