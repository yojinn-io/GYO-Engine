#pragma once
// The GUI probes' aim at the peer's torso (v8-product-hit-gui-aim (a)): the
// predeclared hits stay Body (25000) under the part judgement. Included after
// gui_input.hpp; reads the connection state only.
#include "hit_aim.hpp"

#include <filesystem>
#include <algorithm>
#include <memory>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>

class GuiTorsoAim final {
public:
    explicit GuiTorsoAim(std::filesystem::path assetRoot) : assetRoot_(std::move(assetRoot)) {}
    // The torso aim at the living peer, or none before the world, rules or a
    // living peer exist. The room's arena must be the deployed pvp_arena.json.
    std::optional<fps::pvp::HitAim> Aim(const fps::pvp::ClientConnectionState& state, fps::pvp::PlayerId peer) {
        if (!state.snapshot || !state.movementRules || !state.playerId || !peer) return std::nullopt;
        const auto target = std::find_if(state.snapshot->players.begin(), state.snapshot->players.end(),
                                         [&](const auto& p) { return p.playerId == peer; });
        if (target == state.snapshot->players.end() || target->lifeState != fps::pvp::LifeState::Alive) return std::nullopt;
        if (!arena_) {
            std::string error;
            arena_ = fps::pvp::Arena::Load(assetRoot_ / "pvp_arena.json", error);
            if (!arena_) throw std::runtime_error(error);
            model_ = std::make_unique<fps::pvp::HitAimModel>(assetRoot_, arena_->bodyHeight);
        }
        if (state.arenaId != arena_->id)
            throw std::runtime_error("GUI torso aim needs the room's arena " + state.arenaId + ", not " + arena_->id);
        return model_->Aim(*arena_, state.movementRules->stance, *state.snapshot, state.playerId, peer,
                           fps::pvp::HitPart::Body);
    }

private:
    std::filesystem::path assetRoot_;
    std::optional<fps::pvp::Arena> arena_;
    std::unique_ptr<fps::pvp::HitAimModel> model_;
};
