#include "object_fps_pvp/world/Interaction.hpp"

#include "object_fps_pvp/world/Sight.hpp"

#include "data/JsonFields.hpp"
#include "object_fps_pvp/world/ContentHash.hpp"

#include "engine/authority/Registrations.hpp"
#include "engine/net/NetModule.hpp"
#include "engine/net/Replication.hpp"
#include "engine/physics_3d/SpatialQuery.hpp"
#include "engine/world_time/TimeTypes.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace object_fps_pvp::world {

namespace A = GYO::Engine::Authority;
namespace Base = GYO::Engine::Base;
namespace N = GYO::Engine::Net;
namespace P = GYO::Engine::Physics3d;
namespace WT = GYO::Engine::WorldTime;

void HashValue(K::Hasher&, const Interactable&) {}
void HashValue(K::Hasher& h, const InteractionRules& v) { HashValue(h, v.reach); }

namespace {

// Match and Local World, FixedUpdate/PostUpdate after RespawnPlayers: each living player that pressed E at this
// playable step, in ascending EntityId order, uses the first collider its view meets within reach when that one is
// Interactable; the floor plane in between stops the view (as it stops a shot, JudgeShots).
K::SystemResult JudgeInteractions(K::Query<K::Read<PlayerBody>, K::Read<PlayerCombat>, K::Read<PlayerInput>> players,
                                  K::Query<K::Read<Interactable>> targets, Walls walls, K::Res<GameRules> rules, K::Res<MatchInfo> info,
                                  K::Res<InteractionRules> reach, K::Res<WT::Tick> tick, K::MessageWriter<Interacted> interacted) {
    const std::uint64_t now = tick->value;
    for (const K::EntityId actor : players.Entities()) {
        if (!Playable(*players.Get<PlayerInput>(actor), *info, now).Pressed(kPressInteract)) continue;
        if (players.Get<PlayerCombat>(actor)->life != sim::LifeState::Alive) continue;
        const Math::Ray ray = ShotRay(*rules, *players.Get<PlayerBody>(actor));
        auto hit = P::CastRay(walls, ray, reach->reach);
        if (!hit) {
            return Base::Err(K::KernelError(K::KernelErrorCode::SystemFailed, "object_fps_pvp: an interaction's ray failed: " + hit.error().message));
        }
        if (!*hit) continue;
        const std::optional<float> floor = FloorAlong(*rules, ray);
        if (floor && *floor < (*hit)->hit.t) continue;
        if (!targets.Contains((*hit)->entity)) continue;
        interacted.Send(Interacted{actor, (*hit)->entity, now});
    }
    return {};
}

// The mode's block: {} or {"reach": metres}. Put at every end of a World that plays it (a set that is off has no
// InteractionRules anywhere).
std::optional<SetData> ReadInteraction(const GYO::Engine::Json::ObjectView& block, std::string& error) {
    InteractionRules rules;
    try {
        if (data::HasMember(block, "reach")) rules.reach = static_cast<float>(data::Number(data::Member(block, "reach"), "reach"));
    } catch (const std::runtime_error& failure) {
        error = failure.what();
        return std::nullopt;
    }
    if (!std::isfinite(rules.reach) || rules.reach <= 0.0F) {
        error = "reach is not a positive number of metres";
        return std::nullopt;
    }
    for (const auto member : block) {
        if (member.key != "reach") {
            error = "the interaction set takes no '" + std::string(member.key) + "'";
            return std::nullopt;
        }
    }
    K::Hasher h;
    HashValue(h, std::string("gyo.object_fps_pvp.interaction/1"));
    HashValue(h, rules);
    return SetData{Digest64(h), [rules](K::World& world) { world.SetResource(rules); }};
}

} // namespace

GameplaySet InteractionSet() {
    GameplaySet set;
    set.name = kInteractionSet;
    set.build = [](O::WorldSetup& setup) {
        K::Registrar& r = setup.Registrar();
        r.Component<Interactable>({.name = "Interactable"});
        r.Resource<InteractionRules>({.name = "InteractionRules"});
        A::AuthorityEntry(setup).Message<Interacted>("Interacted");
        K::SystemOptions judge;
        judge.name = kJudgeInteractions;
        K::Mount mount;
        mount.schedule = std::string(O::DefaultSchedules::fixed);
        mount.stage = std::string(O::DefaultSchedules::postUpdate);
        mount.after = {"RespawnPlayers"};
        mount.sets = {GameplaySetFixed(kInteractionSet)};
        if (setup.Options<N::NetWorldOptions>() != nullptr) mount.conditions.push_back(N::OnServer());
        judge.mounts.push_back(std::move(mount));
        r.AddSystem<&JudgeInteractions>(std::move(judge));
    };
    set.readMode = &ReadInteraction;
    return set;
}

} // namespace object_fps_pvp::world
