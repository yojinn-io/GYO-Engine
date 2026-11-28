#include "object_fps_pvp/world/PvpWorld.hpp"

#include "object_fps_pvp/match/AdmissionToken.hpp"
#include "object_fps_pvp/sim/PlanarMovement.hpp"
#include "object_fps_pvp/sim/ViewLimits.hpp"

#include "object_fps_pvp/data/ArenaMap.hpp"

#include "engine/asset/AssetStore.hpp"
#include "engine/authority/AuthorityStrategy.hpp"
#include "engine/base/Assert.hpp"
#include "engine/base/Fnv1a.hpp"
#include "engine/authority/Registrations.hpp"
#include "engine/authority/ServerAuthorityStrategy.hpp"
#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/ReadAllText.hpp"
#include "engine/math/scalar/Angle.hpp"
#include "engine/math/scalar/Scalar.hpp"
#include "engine/net/NetModule.hpp"
#include "engine/orchestration/WorldGeneration.hpp"
#include "engine/net/Replication.hpp"
#include "engine/physics_3d/Collider.hpp"
#include "engine/physics_3d/MoveAndSlide.hpp"
#include "engine/physics_3d/Queries.hpp"
#include "engine/physics_3d/Shapes.hpp"
#include "engine/physics_3d/SpatialQuery.hpp"
#include "engine/rollback/Registrations.hpp"
#include "engine/rollback/RollbackStrategy.hpp"
#include "engine/rollback/StateRollbackStrategy.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/voxel_map/VoxelMapLoaders.hpp"
#include "engine/world_asset/AssetRequests.hpp"
#include "engine/world_asset/AssetView.hpp"
#include "engine/world_time/TimeTypes.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace object_fps_pvp::world {

namespace A = GYO::Engine::Authority;
namespace R = GYO::Engine::Rollback;
namespace P = GYO::Engine::Physics3d;
namespace T = GYO::Engine::Transform;
namespace WT = GYO::Engine::WorldTime;
namespace E = GYO::Engine::EntityIds;
namespace Base = GYO::Engine::Base;
namespace AS = GYO::Engine::Asset;
namespace VM = GYO::Engine::VoxelMap;
namespace WA = GYO::Engine::WorldAsset;

// ---- Hashing (the digest and the consistency of decision data)

void HashValue(K::Hasher& h, const PlayerBody& v) {
    HashValue(h, v.feet.x);
    HashValue(h, v.feet.y);
    HashValue(h, v.feet.z);
    HashValue(h, v.verticalVelocity);
    HashValue(h, v.yaw);
    HashValue(h, v.pitch);
    HashValue(h, v.grounded);
    HashValue(h, v.crouched);
}
void HashValue(K::Hasher& h, const PlayerCombat& v) {
    HashValue(h, v.hp);
    HashValue(h, v.ammo);
    HashValue(h, static_cast<std::uint8_t>(v.life));
    HashValue(h, v.lives);
    HashValue(h, v.lifeTick);
    HashValue(h, v.nextShotTick);
    HashValue(h, v.reloadEndTick);
    HashValue(h, v.respawnTick);
    HashValue(h, v.lastShotTick);
    HashValue(h, v.shots);
}
void HashValue(K::Hasher& h, const ShotRecord& v) {
    HashValue(h, v.tick);
    HashValue(h, static_cast<std::uint8_t>(v.kind));
    HashValue(h, v.target);
    HashValue(h, static_cast<std::uint8_t>(v.part));
    HashValue(h, v.damage);
    HashValue(h, v.distance);
}
void HashValue(K::Hasher& h, const HitRecord& v) {
    HashValue(h, v.tick);
    HashValue(h, v.hits);
    HashValue(h, v.attacker);
    HashValue(h, static_cast<std::uint8_t>(v.part));
}
void HashValue(K::Hasher& h, const Owner& v) { HashValue(h, v.userId); }
void HashValue(K::Hasher&, const PlayerMotion&) {}
void HashValue(K::Hasher& h, const PlayerInput& v) {
    HashValue(h, v.forward);
    HashValue(h, v.right);
    HashValue(h, v.yaw);
    HashValue(h, v.pitch);
    HashValue(h, v.present);
    HashValue(h, v.jump);
    HashValue(h, v.crouch);
    HashValue(h, v.fire);
    HashValue(h, v.reload);
    HashValue(h, v.ready);
}
void HashValue(K::Hasher&, const GameRules&) {}
void HashValue(K::Hasher&, const MatchInfo&) {}
void HashValue(K::Hasher&, const LocalUser&) {}
void HashValue(K::Hasher& h, const LocalSpawn& v) { HashValue(h, static_cast<std::uint64_t>(v.index)); }
void HashValue(K::Hasher& h, const MatchStart& v) {
    for (const std::uint64_t user : v.expect) HashValue(h, user);
    HashValue(h, v.firstTick.value_or(0));
    HashValue(h, static_cast<std::uint64_t>(v.deadlineSeconds));
}
void HashValue(K::Hasher& h, const LocalDeaths& v) {
    HashValue(h, v.count);
    HashValue(h, v.respawnSeen);
}

bool CountDeath(const PlayerCombat& combat, std::uint64_t& respawnSeen) noexcept {
    if (combat.life != sim::LifeState::Dead || combat.respawnTick <= respawnSeen) return false;
    respawnSeen = combat.respawnTick;
    return true;
}
void HashValue(K::Hasher&, const MatchSeats&) {}
void HashValue(K::Hasher&, const SeatedPlayers&) {}
void HashValue(K::Hasher&, const ContentMismatch&) {}

namespace {

// ---- Codecs (field by field, no byte copy)

N::Codec<PlayerBody> BodyCodec() {
    return {[](N::NetWriter& w, const PlayerBody& v) {
                w.Value(v.feet.x);
                w.Value(v.feet.y);
                w.Value(v.feet.z);
                w.Value(v.verticalVelocity);
                w.Value(v.yaw);
                w.Value(v.pitch);
                w.Value(v.grounded);
                w.Value(v.crouched);
            },
            [](N::NetReader& r, PlayerBody& v) {
                return r.Value(v.feet.x) && r.Value(v.feet.y) && r.Value(v.feet.z) && r.Value(v.verticalVelocity) &&
                       r.Value(v.yaw) && r.Value(v.pitch) && r.Value(v.grounded) && r.Value(v.crouched);
            },
            1};
}
N::Codec<PlayerCombat> CombatCodec() {
    return {[](N::NetWriter& w, const PlayerCombat& v) {
                w.Value(v.hp);
                w.Value(v.ammo);
                w.Value(v.life);
                w.Value(v.lives);
                w.Value(v.lifeTick);
                w.Value(v.nextShotTick);
                w.Value(v.reloadEndTick);
                w.Value(v.respawnTick);
                w.Value(v.lastShotTick);
                w.Value(v.shots);
            },
            [](N::NetReader& r, PlayerCombat& v) {
                return r.Value(v.hp) && r.Value(v.ammo) && r.Value(v.life) && r.Value(v.lives) && r.Value(v.lifeTick) &&
                       r.Value(v.nextShotTick) && r.Value(v.reloadEndTick) && r.Value(v.respawnTick) && r.Value(v.lastShotTick) &&
                       r.Value(v.shots);
            },
            2};
}
N::Codec<ShotRecord> ShotCodec() {
    return {[](N::NetWriter& w, const ShotRecord& v) {
                w.Value(v.tick);
                w.Value(v.kind);
                w.Value(v.target);
                w.Value(v.part);
                w.Value(v.damage);
                w.Value(v.distance);
            },
            [](N::NetReader& r, ShotRecord& v) {
                return r.Value(v.tick) && r.Value(v.kind) && r.Value(v.target) && r.Value(v.part) && r.Value(v.damage) &&
                       r.Value(v.distance);
            },
            1};
}
N::Codec<HitRecord> HitCodec() {
    return {[](N::NetWriter& w, const HitRecord& v) {
                w.Value(v.tick);
                w.Value(v.hits);
                w.Value(v.attacker);
                w.Value(v.part);
            },
            [](N::NetReader& r, HitRecord& v) { return r.Value(v.tick) && r.Value(v.hits) && r.Value(v.attacker) && r.Value(v.part); },
            1};
}
N::Codec<Owner> OwnerCodec() {
    return {[](N::NetWriter& w, const Owner& v) { w.Value(v.userId); }, [](N::NetReader& r, Owner& v) { return r.Value(v.userId); }, 1};
}
N::InputOptions<PlayerInput> InputOptions() {
    N::InputOptions<PlayerInput> options;
    options.codec = {[](N::NetWriter& w, const PlayerInput& v) {
                         w.Value(v.forward);
                         w.Value(v.right);
                         w.Value(v.yaw);
                         w.Value(v.pitch);
                         w.Value(v.present);
                         w.Value(v.jump);
                         w.Value(v.crouch);
                         w.Value(v.fire);
                         w.Value(v.reload);
                         w.Value(v.ready);
                     },
                     [](N::NetReader& r, PlayerInput& v) {
                         return r.Value(v.forward) && r.Value(v.right) && r.Value(v.yaw) && r.Value(v.pitch) &&
                                r.Value(v.present) && r.Value(v.jump) && r.Value(v.crouch) && r.Value(v.fire) &&
                                r.Value(v.reload) && r.Value(v.ready);
                     },
                     2};
    // A held input is the last one with its presses taken out: a press fires once (C-IN3).
    options.held = [](PlayerInput& v) {
        v.jump = 0;
        v.fire = 0;
        v.reload = 0;
    };
    return options;
}
// An id of at most 64 bytes: its length, then its bytes.
void WriteId(N::NetWriter& w, const std::string& id) {
    w.Value(static_cast<std::uint16_t>(id.size()));
    for (const char c : id) w.Value(static_cast<std::uint8_t>(c));
}
bool ReadId(N::NetReader& r, std::string& id) {
    std::uint16_t size = 0;
    if (!r.Value(size) || size > sim::kMaxContentIdBytes) return false;
    id.clear();
    for (std::uint16_t i = 0; i < size; ++i) {
        std::uint8_t c = 0;
        if (!r.Value(c)) return false;
        id.push_back(static_cast<char>(c));
    }
    return true;
}

N::Codec<MatchInfo> MatchInfoCodec() {
    return {[](N::NetWriter& w, const MatchInfo& v) {
                WriteId(w, v.arenaId);
                w.Value(v.wallsDigest);
                WriteId(w, v.modeId);
                w.Value(v.rulesDigest);
                w.Value(v.characterDigest);
                w.Value(v.arenaDigest);
                w.Value(static_cast<std::uint8_t>(v.phase));
                w.Value(v.startTick);
            },
            [](N::NetReader& r, MatchInfo& v) {
                std::uint8_t phase = 0;
                if (!ReadId(r, v.arenaId) || !r.Value(v.wallsDigest) || !ReadId(r, v.modeId) || !r.Value(v.rulesDigest) ||
                    !r.Value(v.characterDigest) || !r.Value(v.arenaDigest) || !r.Value(phase) || phase > 1 || !r.Value(v.startTick)) {
                    return false;
                }
                v.phase = static_cast<MatchPhase>(phase);
                return true;
            },
            5};
}

// ---- The body's shape and the queries against the walls

using Walls = K::Query<K::Read<P::Collider>, K::Read<T::Transform>, K::Without<K::ChildOf>>;

// The upright capsule of a body of height (feet at the bottom), placed at feet.
P::Shape BodyShape(const float height, const float radius) { return P::CapsuleY(0.5F * height - radius, radius); }
P::Pose BodyPose(const Math::Vec3& feet, const float height) { return P::Pose{feet + Math::Vec3{0.0F, 0.5F * height, 0.0F}, {}}; }

// The ground probe: a body is on the ground when it stands on the arena's floor plane (a rule of the game, when the
// arena has one) or a short sweep down meets a face that faces up (normal y > 0.5).
constexpr float kGroundProbe = 0.002F;
constexpr float kGroundNormalY = 0.5F;

P::MoveAndSlideConfig MoveConfig() { return P::MoveAndSlideConfig{}; }

Base::Result<bool, P::Physics3dError> OnGround(const Walls& walls, const sim::ArenaContent& arena, const Math::Vec3& feet,
                                               const float height, const float radius) {
    if (arena.floor && feet.y <= *arena.floor + kGroundProbe) return true;
    auto hit = P::CastMove(walls, BodyShape(height, radius), BodyPose(feet, height), Math::Vec3{0.0F, -kGroundProbe, 0.0F},
                           MoveConfig().depenetration.skinWidth);
    if (!hit) return Base::Err(std::move(hit).error());
    return hit->has_value() && (*hit)->normal.y > kGroundNormalY;
}

// A body fits where it overlaps no wall deeper than kTouch: standing on a wall's top or against its side is room
// (a spawn on a wall top touches it; the physics' touching counts as a contact).
constexpr float kTouch = 1.0e-4F;

Base::Result<bool, P::Physics3dError> Fits(const Walls& walls, const Math::Vec3& feet, const float height, const float radius) {
    bool blocked = false;
    auto visited = P::ForEachContact(walls, BodyShape(height, radius), BodyPose(feet, height), 0.0F, P::AcceptAll{},
                                     [&blocked](K::EntityId, std::uint32_t, const P::Contact& contact) {
                                         if (contact.distance < -kTouch) blocked = true;
                                         return !blocked;
                                     });
    if (!visited) return Base::Err(std::move(visited).error());
    return !blocked;
}

K::SystemResult PhysicsFailed(const P::Physics3dError& error) {
    return Base::Err(K::KernelError(K::KernelErrorCode::SystemFailed, "object_fps_pvp: a physics query failed",
                                    Base::Describe(error)));
}

// The arena's invisible wall: the feet stay within [radius, width - radius] x [radius, depth - radius] (a move into it
// keeps its other axis: the body slides along it).
Math::Vec3 InsideBounds(const sim::ArenaContent& arena, const float radius, Math::Vec3 feet) {
    feet.x = std::clamp(feet.x, radius, arena.width - radius);
    feet.z = std::clamp(feet.z, radius, arena.depth - radius);
    return feet;
}

// One movement step of a living body with input (present): the view, the stance, the jump, the fall and the slide
// against the walls and inside the arena's bounds; a body without input keeps its view and stance and only falls.
// Gives the downward speed it landed at in this step (0: it did not land).
Base::Result<float, P::Physics3dError> StepBody(const Walls& walls, const GameRules& rules, PlayerBody& body,
                                               const PlayerInput& input, const bool alive, const double seconds) {
    const sim::ArenaContent& arena = rules.arena;
    const sim::BodySize& size = rules.character->bodySize;
    const bool steering = alive && input.present != 0;
    if (steering) {
        body.yaw = Math::WrapRadians(input.yaw);
        body.pitch = std::clamp(input.pitch, -sim::MaximumPitchRadians, sim::MaximumPitchRadians);
    }
    const Math::Vec3 start = body.feet;
    bool crouched = body.crouched != 0;
    if (steering) {
        const bool jumpFromGround = input.jump != 0 && body.grounded != 0;
        if (input.crouch != 0 && !jumpFromGround) {
            crouched = true;
        } else if (crouched) {
            // Standing up needs room for the standing body where the feet are.
            auto room = Fits(walls, start, size.height, size.radius);
            if (!room) return Base::Err(std::move(room).error());
            if (*room) crouched = false;
        }
    }
    body.crouched = crouched ? 1 : 0;
    const float height = sim::BodyHeight(size, crouched);
    const float speed = sim::MoveSpeed(rules.movement, crouched);
    const sim::GroundPoint step = steering ? sim::ComputePlanarDisplacement(std::clamp(input.forward, -1.0F, 1.0F),
                                                                            std::clamp(input.right, -1.0F, 1.0F), body.yaw,
                                                                            speed, static_cast<float>(seconds))
                                           : sim::GroundPoint{};
    if (body.grounded != 0) {
        auto ground = OnGround(walls, arena, start, height, size.radius);
        if (!ground) return Base::Err(std::move(ground).error());
        body.grounded = *ground ? 1 : 0;
    }
    if (steering && input.jump != 0 && body.grounded != 0 && !crouched) {
        body.verticalVelocity = sim::JumpSpeed(rules.movement);
        body.grounded = 0;
    }
    float desiredY = start.y;
    // The downward speed the fall reaches in this step: what it lands at, if it lands.
    float descent = 0.0F;
    const bool airborne = body.grounded == 0;
    if (airborne) {
        const sim::VerticalStep fall = sim::Fall(start.y, body.verticalVelocity, rules.movement.gravity, seconds, arena.floor);
        desiredY = fall.y;
        body.verticalVelocity = fall.velocity;
        descent = std::max(fall.impact, -fall.velocity);
    }
    const Math::Vec3 movement{step.x, desiredY - start.y, step.z};
    Math::Vec3 end = start;
    if (Math::LengthSquared(movement) > 0.0F) {
        P::MoveAndSlideConfig config = MoveConfig();
        if (body.grounded != 0) config.planes.push_back(Math::Vec3{0.0F, 1.0F, 0.0F});
        const float invSeconds = static_cast<float>(1.0 / seconds);
        auto moved = P::MoveAndSlide(walls, BodyShape(height, size.radius), BodyPose(start, height), movement * invSeconds,
                                     static_cast<float>(seconds), config);
        if (!moved) return Base::Err(std::move(moved).error());
        end = InsideBounds(arena, size.radius, moved->position - Math::Vec3{0.0F, 0.5F * height, 0.0F});
        if (arena.floor && end.y < *arena.floor) end.y = *arena.floor;
    }
    body.feet = end;
    if (std::abs(end.y - desiredY) > 0.0001F) body.verticalVelocity = 0.0F;
    if (body.verticalVelocity <= 0.0F) {
        auto ground = OnGround(walls, arena, end, height, size.radius);
        if (!ground) return Base::Err(std::move(ground).error());
        body.grounded = *ground ? 1 : 0;
    } else {
        body.grounded = 0;
    }
    if (body.grounded != 0) body.verticalVelocity = 0.0F;
    return airborne && body.grounded != 0 ? descent : 0.0F;
}

double StepSeconds(const WT::ActiveTime& time) { return static_cast<double>(time.delta) / 1e9; }
std::uint64_t StepNanos(const WT::ActiveTime& time) { return static_cast<std::uint64_t>(time.delta); }

// ---- The Systems of the fixed step

// Before the start the players wait: their input is none (they neither move, turn nor fire). By the step, so a Client
// simulating the steps around the start again gives each the Match's answer.
const PlayerInput& Playable(const PlayerInput& input, const MatchInfo& info, const std::uint64_t tick) {
    static const PlayerInput none{};
    return info.phase == MatchPhase::Playing && tick >= info.startTick ? input : none;
}

// A living player dies at this tick: the respawn waits the rules' time, a reload in progress is dropped.
void Die(PlayerCombat& combat, const sim::CombatRules& rules, const std::uint64_t now, const std::uint64_t stepNanos) {
    combat.life = sim::LifeState::Dead;
    combat.lifeTick = now;
    combat.respawnTick = now + sim::TicksFor(rules.respawnSeconds, stepNanos);
    combat.reloadEndTick = 0;
}

// Both ends (the Client predicts its own player): the body moves (StepBody); a living body that lands from higher than
// the damage rules' safe height loses HP (sim::LandingDamage), and one whose feet are below the kill height dies.
K::SystemResult MovePlayers(K::Query<K::Read<PlayerInput>, K::Write<PlayerCombat>, K::Write<PlayerBody>> players,
                            Walls walls, K::Res<GameRules> rules, K::Res<MatchInfo> info, K::Res<WT::Tick> tick,
                            K::Res<WT::ActiveTime> time) {
    std::optional<P::Physics3dError> failure;
    const double seconds = StepSeconds(*time);
    if (!(seconds > 0.0)) return {};
    const std::uint64_t now = tick->value;
    players.ForEach([&](K::EntityId, const PlayerInput& input, K::Mut<PlayerCombat> combat, K::Mut<PlayerBody> body) {
        if (failure) return;
        PlayerBody next = body.Read();
        const bool alive = combat.Read().life == sim::LifeState::Alive;
        auto stepped = StepBody(walls, *rules, next, Playable(input, *info, now), alive, seconds);
        if (!stepped) {
            failure = std::move(stepped).error();
            return;
        }
        if (!(next == body.Read())) body.Get() = next;
        if (!alive) return;
        const std::uint32_t damage = sim::LandingDamage(rules->movement, rules->damage, *stepped);
        const bool fellOut = sim::BelowKillHeight(rules->arena, next.feet.y);
        if (damage == 0 && !fellOut) return;
        PlayerCombat after = combat.Read();
        after.hp = fellOut ? 0 : after.hp - std::min(damage, after.hp);
        if (after.hp == 0) Die(after, rules->combat, now, StepNanos(*time));
        combat.Get() = after;
    });
    if (failure) return PhysicsFailed(*failure);
    return {};
}

K::SystemResult UseWeapons(K::Query<K::Read<PlayerInput>, K::Write<PlayerCombat>> players, K::Res<GameRules> rules,
                           K::Res<MatchInfo> info, K::Res<WT::Tick> tick, K::Res<WT::ActiveTime> time) {
    const sim::CombatRules& combat = rules->combat;
    const std::uint64_t now = tick->value;
    const std::uint64_t step = StepNanos(*time);
    players.ForEach([&](K::EntityId, const PlayerInput& given, K::Mut<PlayerCombat> cell) {
        const PlayerInput& input = Playable(given, *info, now);
        PlayerCombat next = cell.Read();
        if (next.life != sim::LifeState::Alive) return;
        if (next.reloadEndTick != 0 && now >= next.reloadEndTick) {
            next.ammo = combat.magazineCapacity;
            next.reloadEndTick = 0;
        }
        if (input.present != 0 && next.reloadEndTick == 0) {
            if (input.reload != 0 && next.ammo < combat.magazineCapacity) {
                next.reloadEndTick = now + sim::TicksFor(combat.reloadSeconds, step);
            } else if (input.fire != 0 && next.ammo > 0 && now >= next.nextShotTick) {
                --next.ammo;
                next.nextShotTick = now + sim::TicksFor(combat.cooldownSeconds, step);
                next.lastShotTick = now;
                ++next.shots;
            }
        }
        if (!(next == cell.Read())) cell.Get() = next;
    });
    return {};
}

// The shot's ray from a body's eye along its view.
Math::Ray ShotRay(const GameRules& rules, const PlayerBody& body) {
    const float eye = sim::EyeHeight(rules.character->bodySize, body.crouched != 0);
    return Math::Ray{body.feet + Math::Vec3{0.0F, eye, 0.0F}, sim::ViewDirection(body.yaw, body.pitch)};
}

// The body's stance capsule: where a body stands, for placing a spawn (shots meet the shapes on the bones).
P::Capsule BodyCapsule(const GameRules& rules, const PlayerBody& body) {
    const float height = sim::BodyHeight(rules.character->bodySize, body.crouched != 0);
    const float r = rules.character->bodySize.radius;
    return P::Capsule{body.feet + Math::Vec3{0.0F, r, 0.0F}, body.feet + Math::Vec3{0.0F, height - r, 0.0F}, r};
}

K::SystemResult JudgeShots(K::Query<K::Read<PlayerBody>, K::Write<PlayerCombat>, K::Write<ShotRecord>, K::Write<HitRecord>> players,
                           K::Query<K::Read<PlayerMotion>> motions, Walls walls, K::Res<GameRules> rules, K::Res<WT::Tick> tick, K::Res<WT::ActiveTime> time) {
    const std::uint64_t now = tick->value;
    const sim::CombatRules& combat = rules->combat;
    const float range = combat.shotRange;
    if (!rules->character) return Base::Err(K::KernelError(K::KernelErrorCode::SystemFailed, "object_fps_pvp: no character to judge on"));
    // Ascending EntityId: the order of the shots of one step and of the ties between players.
    for (const K::EntityId shooter : players.Entities()) {
        const PlayerCombat* fired = players.Get<PlayerCombat>(shooter);
        if (fired == nullptr || fired->lastShotTick != now || fired->life != sim::LifeState::Alive) continue;
        const PlayerBody& body = *players.Get<PlayerBody>(shooter);
        const Math::Ray ray = ShotRay(*rules, body);
        std::optional<float> world;
        // The floor plane (when the arena has one): a solid half-space below it.
        if (const auto floor = rules->arena.floor) {
            if (ray.origin.y <= *floor) {
                world = 0.0F;
            } else if (ray.direction.y < 0.0F) {
                world = (ray.origin.y - *floor) / -ray.direction.y;
            }
        }
        auto wall = P::CastRay(walls, ray, range);
        if (!wall) return PhysicsFailed(wall.error());
        if (*wall && (!world || (*wall)->hit.t < *world)) world = (*wall)->hit.t;
        std::vector<sim::PlayerHitCandidate> candidates;
        for (const K::EntityId target : players.Entities()) {
            if (target == shooter) continue;
            const PlayerCombat& targetCombat = *players.Get<PlayerCombat>(target);
            if (targetCombat.life != sim::LifeState::Alive) continue;
            const PlayerBody& targetBody = *players.Get<PlayerBody>(target);
            const PlayerMotion* motion = motions.Get<PlayerMotion>(target);
            if (motion == nullptr) continue;  // not seated by SeatPlayers: not in the match
            // The shapes on the target's bones in this step's pose (port inventory PV2): its nearest shape counts; at
            // one distance the higher part.
            const auto& rig = *rules->character;
            const sim::AnimationInput input =
                AnimationInputOf(targetBody, targetCombat, *players.Get<HitRecord>(target), *rules, now, StepNanos(*time));
            std::vector<GYO::Engine::Model::LocalTransform> local;
            character::ComposeBody(rig, motion->locomotion, sim::ResolveActions(motion->locomotion, input, rig.rules), input.crouched,
                                   input.seconds - input.lifeSeconds, local);
            std::vector<Math::Matrix4> modelSpace;
            character::ModelSpace(rig, local, modelSpace);
            std::optional<sim::PlayerHitCandidate> nearest;
            for (const character::HitShape& shape : character::PlaceHitShapes(rig, modelSpace, targetBody.feet, targetBody.yaw)) {
                const auto hit = P::CastRay(P::Shape{P::Capsule{shape.a, shape.b, shape.radius}}, P::Pose{}, ray, range);
                if (!hit) continue;
                if (!nearest || hit->t < nearest->distance ||
                    (hit->t == nearest->distance && sim::PartRank(shape.part) > sim::PartRank(nearest->part))) {
                    nearest = sim::PlayerHitCandidate{target, hit->t, shape.part};
                }
            }
            if (nearest) candidates.push_back(*nearest);
        }
        const sim::ShotOutcome outcome = sim::ChooseShotOutcome(world, candidates, range);
        ShotRecord record;
        record.tick = now;
        record.kind = outcome.kind;
        record.target = static_cast<K::EntityId>(outcome.player);
        record.part = outcome.part;
        record.distance = outcome.distance;
        if (outcome.kind == sim::ShotHitKind::Player) {
            const K::EntityId target = record.target;
            auto targetCombat = players.GetMut<PlayerCombat>(target);
            record.damage = sim::ShotDamage(combat, outcome.part, targetCombat.Read().hp);
            targetCombat->hp -= record.damage;
            auto hits = players.GetMut<HitRecord>(target);
            hits->tick = now;
            ++hits->hits;
            hits->attacker = shooter;
            hits->part = outcome.part;
            if (targetCombat.Read().hp == 0) Die(targetCombat.Get(), combat, now, StepNanos(*time));
        }
        *players.GetMut<ShotRecord>(shooter) = record;
    }
    return {};
}

// Match: each player's animation moves to this step (its gait and jump), for the hit shapes the next shots meet.
K::SystemResult AnimatePlayers(K::Query<K::Read<PlayerBody>, K::Read<PlayerCombat>, K::Read<HitRecord>, K::Write<PlayerMotion>> players,
                               K::Res<GameRules> rules, K::Res<WT::Tick> tick, K::Res<WT::ActiveTime> time) {
    if (!rules->character) return {};
    const std::uint64_t step = StepNanos(*time);
    players.ForEach([&](K::EntityId, const PlayerBody& body, const PlayerCombat& combat, const HitRecord& hits, K::Mut<PlayerMotion> motion) {
        sim::Advance(motion->locomotion, AnimationInputOf(body, combat, hits, *rules, tick->value, step), rules->character->rules);
    });
    return {};
}

// Whether a standing body fits at a spawn: free of the walls and of every living body.
Base::Result<bool, P::Physics3dError> SpawnFree(const Walls& walls, const GameRules& rules, const sim::SpawnPoint& spawn,
                                                const std::vector<PlayerBody>& living) {
    auto fits = Fits(walls, spawn.position, rules.character->bodySize.height, rules.character->bodySize.radius);
    if (!fits || !*fits) return fits;
    PlayerBody standing;
    standing.feet = spawn.position;
    const P::Capsule mine = BodyCapsule(rules, standing);
    for (const PlayerBody& other : living) {
        auto contact = P::FindContact(P::Shape{mine}, P::Pose{}, P::Shape{BodyCapsule(rules, other)}, P::Pose{}, 0.0F);
        if (!contact) return Base::Err(std::move(contact).error());
        if (contact->has_value()) return false;
    }
    return true;
}

// The spawn a player (re)appears at, or nullopt when none is free (tried again on a later step).
Base::Result<std::optional<std::size_t>, P::Physics3dError> PickSpawn(const Walls& walls, const GameRules& rules,
                                                                       const std::vector<PlayerBody>& living) {
    std::vector<Math::Vec3> enemies;
    for (const PlayerBody& body : living) enemies.push_back(body.feet);
    std::optional<P::Physics3dError> failure;
    const auto chosen = sim::ChooseSpawn(rules.arena.spawns, enemies, [&](const sim::SpawnPoint& spawn) {
        if (failure) return false;
        auto free = SpawnFree(walls, rules, spawn, living);
        if (!free) {
            failure = std::move(free).error();
            return false;
        }
        return *free;
    });
    if (failure) return Base::Err(std::move(*failure));
    return chosen;
}

PlayerBody BodyAt(const sim::SpawnPoint& spawn) {
    PlayerBody body;
    body.feet = spawn.position;
    body.yaw = Math::WrapRadians(spawn.yaw);
    return body;
}

PlayerCombat FreshCombat(const sim::CombatRules& rules, const std::uint32_t lives) {
    PlayerCombat combat;
    combat.hp = rules.maximumHp;
    combat.ammo = rules.magazineCapacity;
    combat.lives = lives;
    return combat;
}

K::SystemResult RespawnPlayers(K::Query<K::Write<PlayerBody>, K::Write<PlayerCombat>, K::Write<HitRecord>> players, Walls walls,
                               K::Res<GameRules> rules, K::Res<WT::Tick> tick) {
    const std::uint64_t now = tick->value;
    // Ascending EntityId: a later respawn sees the bodies placed by earlier ones.
    for (const K::EntityId id : players.Entities()) {
        const PlayerCombat& combat = *players.Get<PlayerCombat>(id);
        if (combat.life != sim::LifeState::Dead || now < combat.respawnTick) continue;
        std::vector<PlayerBody> living;
        for (const K::EntityId other : players.Entities()) {
            if (other != id && players.Get<PlayerCombat>(other)->life == sim::LifeState::Alive) {
                living.push_back(*players.Get<PlayerBody>(other));
            }
        }
        auto spawn = PickSpawn(walls, *rules, living);
        if (!spawn) return PhysicsFailed(spawn.error());
        if (!*spawn) continue;
        const std::uint32_t lives = combat.lives + 1;
        const std::uint64_t lastShot = combat.lastShotTick;
        const std::uint32_t shots = combat.shots;
        *players.GetMut<PlayerBody>(id) = BodyAt(rules->arena.spawns[**spawn]);
        PlayerCombat fresh = FreshCombat(rules->combat, lives);
        fresh.lifeTick = now;
        fresh.lastShotTick = lastShot;
        fresh.shots = shots;
        *players.GetMut<PlayerCombat>(id) = fresh;
        *players.GetMut<HitRecord>(id) = HitRecord{};
    }
    return {};
}

// ---- The Systems of the frame

// Match: a player for each user that connects (at the farthest free spawn). A user's newer connection takes their
// player over (the user's decision of 2026-11-25, v8-arch-gyo2-direct-connect-and-kick-old), on both paths:
//   - from another endpoint, while the older connection is open: the older one is closed here (its PeerDisconnected
//     comes later and removes nothing);
//   - from the same endpoint (a newer handshake: the server ends the older connection and accepts the newer one in the
//     same frame): the player the ended connection leaves is held until the joins of this frame are seen.
// A connection that ends removes the player it controls, unless a newer connection of the user takes it in that frame.
K::SystemResult SeatPlayers(K::MessageReader<N::PeerConnected> connected, K::MessageReader<N::PeerDisconnected> disconnected,
                            K::Query<K::Read<Owner>, K::Read<PlayerBody>, K::Read<PlayerCombat>> players,
                            K::Query<K::Read<N::Peer>> peers, Walls walls, K::Res<GameRules> rules,
                            K::Res<MatchSeats> seats, K::ResMut<N::NetWorldLink> link, K::ResMut<SeatedPlayers> seated,
                            K::Res<WT::Tick> tick, K::Res<O::WorldGeneration> generation,
                            K::Spawn<K::Bundle<PlayerBody, PlayerCombat, ShotRecord, HitRecord, Owner, PlayerInput, PlayerMotion>> spawn,
                            K::Despawn despawn, K::CommandsFor<K::Inserts<N::PeerControls>, K::Removes<N::PeerControls>> controls) {
    // No walls yet: the connections wait, unread. This System runs every frame, so its readers' cursors exist from the
    // first frame and hold the records (a reader that has never run holds none: a mount condition would lose them).
    if (generation->Generating()) return {};
    std::map<K::EntityId, K::EntityId>& byPeer = seated->byPeer;
    // The players of the connections that ended this frame, by user, until the joins of this frame are seen.
    std::map<std::uint64_t, K::EntityId> released;
    for (const N::PeerDisconnected* gone : disconnected.Read()) {
        if (const auto found = byPeer.find(gone->peer); found != byPeer.end()) {
            released[gone->userId] = found->second;
            byPeer.erase(found);
        }
        // Also a connection that ended before its Peer was made (peer 0): its admission took a seat too.
        if (seats->seats) seats->seats->Release(gone->userId);
    }
    std::vector<PlayerBody> living;
    players.ForEach([&](K::EntityId, const Owner&, const PlayerBody& body, const PlayerCombat& combat) {
        if (combat.life == sim::LifeState::Alive) living.push_back(body);
    });
    for (const N::PeerConnected* joined : connected.Read()) {
        // A connection that ended since it came (read late, after the World generated): nothing to seat.
        if (peers.Get<N::Peer>(joined->peer) == nullptr) continue;
        // The user's player, if another connection has it: one that ended this frame, or one still open.
        std::optional<K::EntityId> player;
        if (const auto held = released.find(joined->userId); held != released.end()) {
            player = held->second;
            released.erase(held);
        } else {
            std::optional<std::pair<K::EntityId, std::uint32_t>> older;
            peers.ForEach([&](const K::EntityId id, const N::Peer& peer) {
                if (id != joined->peer && peer.userId == joined->userId && byPeer.contains(id)) older = {id, peer.connectionId};
            });
            if (older) {
                player = byPeer.at(older->first);
                byPeer.erase(older->first);
                controls.Remove<N::PeerControls>(older->first);
                if (link->service != nullptr && link->service->Server() != nullptr) link->service->Server()->Close(older->second);
            }
        }
        if (player) {
            byPeer[joined->peer] = *player;
            controls.Insert(joined->peer, N::PeerControls{*player});
            continue;
        }
        auto chosen = PickSpawn(walls, *rules, living);
        if (!chosen) return PhysicsFailed(chosen.error());
        // No free spawn now: the first one (a spawn is never inside a wall; bodies may overlap until they move).
        const sim::SpawnPoint& point = rules->arena.spawns[chosen->value_or(0)];
        const PlayerBody body = BodyAt(point);
        PlayerCombat combat = FreshCombat(rules->combat, 1);
        combat.lifeTick = tick->value;
        auto id = spawn.New(body, combat, ShotRecord{}, HitRecord{}, Owner{joined->userId}, PlayerInput{}, PlayerMotion{});
        if (!id) return Base::Err(std::move(id).error());
        controls.Insert(joined->peer, N::PeerControls{*id});
        byPeer[joined->peer] = *id;
        living.push_back(body);
    }
    // Nobody took them over: the players leave with their connections.
    for (const auto& [user, player] : released) despawn.Entity(player);
    return {};
}

// The authority (the Match, a Local World): the match starts when every expected user's player says it is ready
// (PlayerInput::ready: the Client has loaded and drawn the game), or once the deadline after the first player came has
// passed (one who does not come does not hold the others). FixedUpdate/Update, before MovePlayers.
K::SystemResult StartMatch(K::Query<K::Read<Owner>, K::Read<PlayerInput>> players, K::ResMut<MatchStart> start,
                           K::ResMut<MatchInfo> info, K::Res<WT::Tick> tick, K::Res<WT::ActiveTime> time) {
    if (info.Read().phase == MatchPhase::Playing) return {};
    const MatchStart& waiting = start.Read();
    // The expected users whose player says ready (each once: a user's players are counted by the user).
    std::vector<std::uint64_t> ready;
    bool anyone = false;
    players.ForEach([&](K::EntityId, const Owner& owner, const PlayerInput& input) {
        anyone = true;
        if (input.ready != 0 && std::find(waiting.expect.begin(), waiting.expect.end(), owner.userId) != waiting.expect.end() &&
            std::find(ready.begin(), ready.end(), owner.userId) == ready.end()) {
            ready.push_back(owner.userId);
        }
    });
    if (anyone && !waiting.firstTick) start->firstTick = tick->value;
    const std::uint64_t step = StepNanos(*time);
    const bool late = waiting.firstTick && waiting.deadlineSeconds > 0 && step > 0 &&
                      tick->value - *waiting.firstTick >= sim::TicksFor(static_cast<float>(waiting.deadlineSeconds), step);
    if (ready.size() >= waiting.expect.size() || late) {
        info->phase = MatchPhase::Playing;
        info->startTick = tick->value;
    }
    return {};
}

// The Match: before the start a connection sees only the player it controls (it is the only rule of relevance so far:
// from the start the PeerVisibility is taken away, which another rule -- distance, teams -- would have to join) (net's PeerVisibility: the others'
// places are not sent, so no Client can show them); from the start it sees everything. Main/Update, after
// SeatPlayers.
K::SystemResult ShowPlayers(K::Query<K::Read<N::PeerControls>> controlling, K::Query<K::Read<N::PeerVisibility>> visibility,
                            K::Query<K::Read<N::Peer>> peers, K::Res<MatchInfo> info,
                            K::CommandsFor<K::Inserts<N::PeerVisibility>, K::Removes<N::PeerVisibility>> commands) {
    const bool playing = info->phase == MatchPhase::Playing;
    peers.ForEach([&](const K::EntityId peer, const N::Peer&) {
        const N::PeerVisibility* now = visibility.Get<N::PeerVisibility>(peer);
        if (playing) {
            if (now != nullptr) commands.Remove<N::PeerVisibility>(peer);
            return;
        }
        N::PeerVisibility own;
        own.everything = false;
        if (const N::PeerControls* controls = controlling.Get<N::PeerControls>(peer)) own.Show(controls->entity);
        if (now == nullptr || now->everything || now->entities != own.entities) commands.Insert(peer, std::move(own));
    });
    return {};
}

// Local (a game without a Match): the local user's player, once, at the spawn LocalSpawn chose (a random one: single
// player's randomness is not decision data two ends must agree on). It holds a PlayerInput the World writes itself.
K::SystemResult SpawnLocalPlayer(K::Query<K::Read<Owner>> players, K::Res<LocalUser> user, K::Res<LocalSpawn> chosen,
                                 K::Res<GameRules> rules, K::Res<WT::Tick> tick,
                                 K::Spawn<K::Bundle<PlayerBody, PlayerCombat, ShotRecord, HitRecord, Owner, PlayerInput, PlayerMotion>> spawn) {
    bool there = false;
    players.ForEach([&](K::EntityId, const Owner& owner) { there = there || owner.userId == user->userId; });
    if (there) return {};
    const sim::SpawnPoint& point = rules->arena.spawns[chosen->index % rules->arena.spawns.size()];
    PlayerCombat combat = FreshCombat(rules->combat, 1);
    combat.lifeTick = tick->value;
    auto id = spawn.New(BodyAt(point), combat, ShotRecord{}, HitRecord{}, Owner{user->userId}, PlayerInput{}, PlayerMotion{});
    if (!id) return Base::Err(std::move(id).error());
    return {};
}

// Local: the deaths of its player (CountDeath; Frame/Last, after the steps of the frame).
K::SystemResult CountLocalDeaths(K::Query<K::Read<Owner>, K::Read<PlayerCombat>> players, K::Res<LocalUser> user,
                                 K::ResMut<LocalDeaths> deaths) {
    players.ForEach([&](K::EntityId, const Owner& owner, const PlayerCombat& combat) {
        if (owner.userId != user->userId) return;
        std::uint64_t seen = deaths.Read().respawnSeen;
        if (CountDeath(combat, seen)) {
            deaths->respawnSeen = seen;
            ++deaths->count;
        }
    });
    return {};
}

// Client: the player it owns gets the Peer's PeerControls and a PlayerInput (the net Module writes the input into it).
K::SystemResult ControlOwnPlayer(K::Query<K::Read<N::Peer>, K::Without<N::PeerControls>> peers,
                                 K::Query<K::Read<Owner>, K::Without<PlayerInput>> players, K::Res<LocalUser> user,
                                 K::CommandsFor<K::Inserts<N::PeerControls, PlayerInput>, K::Removes<>> commands) {
    for (const K::EntityId peer : peers.Entities()) {
        players.ForEach([&](const K::EntityId id, const Owner& owner) {
            if (owner.userId != user->userId) return;
            commands.Insert(peer, N::PeerControls{id});
            commands.Insert(id, PlayerInput{});
        });
    }
    return {};
}

// Client: whether it plays the Match's mode and arena with the same content (MatchInfo), once the Match has said its
// walls. Another mode or arena, or the same one with other rules or walls, is a content mismatch (ContentMismatch): a
// World plays one mode and one arena, so the program builds another World or leaves (big-map declaration 15.3;
// mode-config declaration 3.3). The mode, the rules and another arena are known at once (this World need not read
// its own map first); the same arena's walls are compared once this World has built its own.
K::SystemResult FollowMatchContent(K::Res<MatchInfo> info, K::Res<GameRules> rules, K::Res<ActiveMode> mode, K::Res<MapLoad> load,
                                   K::ResMut<ContentMismatch> mismatch) {
    const MatchInfo& match = *info;
    if (match.arenaId.empty() || match.wallsDigest == 0 || mismatch.Read().kind != MismatchKind::None) return {};
    const GameRules& own = *rules;
    MismatchKind kind = MismatchKind::None;
    std::string reason;
    if (match.modeId != mode->id) {
        kind = MismatchKind::Mode;
        reason = "the Match plays mode '" + match.modeId + "', this game '" + mode->id + "'";
    } else if (match.rulesDigest != mode->rulesDigest) {
        kind = MismatchKind::Rules;
        reason = "mode '" + match.modeId + "' differs from the Match's (rules digest)";
    } else if (match.characterDigest != own.characterDigest) {
        kind = MismatchKind::Character;
        reason = "the character differs from the Match's (character digest: skeleton, clips, hit shapes)";
    } else if (match.arenaId != own.arena.id) {
        kind = MismatchKind::Arena;
        reason = "the Match plays arena '" + match.arenaId + "', this game '" + own.arena.id + "'";
    } else if (match.arenaDigest != own.arenaDigest) {
        kind = MismatchKind::ArenaData;
        reason = "arena '" + match.arenaId + "' differs from the Match's (arena digest: bounds, floor, kill height, spawns)";
    } else if (load->stage == MapStage::Done && match.wallsDigest != own.wallsDigest) {
        kind = MismatchKind::Walls;
        reason = "arena '" + match.arenaId + "' differs from the Match's (walls digest)";
    }
    if (kind == MismatchKind::None) return {};
    ContentMismatch& found = mismatch.Get();
    found.kind = kind;
    found.matchArena = match.arenaId;
    found.matchMode = match.modeId;
    found.reason = std::move(reason);
    return {};
}

// The walls the map's boxes build (BoxSet::Build: S0 measured seconds for a million boxes, so a worker builds them).
using WallsBuilt = Base::Result<P::BoxSet, P::Physics3dError>;

// A World's map while it is read: LoadArena's state. Nothing of it is decision data (the World generates until it is
// done; only the walls it ends with count, and they are in GameRules::wallsDigest).
struct ArenaLoading final {
    // The map's asset id (ArenaContent::map).
    std::string map;
    // The authority (the Match, a Local World): MatchInfo says its arena and walls.
    bool announce = false;
    AS::AssetExecutor* jobs = nullptr;
    AS::Handle<VM::VoxelMapAsset> handle;
    WorkerTask<WallsBuilt> walls;
};
void HashValue(K::Hasher&, const ArenaLoading&) {}

// Why a spawn of the arena is inside its walls, or nothing (whether bodies overlap is decided when they spawn).
std::optional<std::string> SpawnInWall(const sim::ArenaContent& arena, const sim::BodySize& size, const P::BoxSet& walls) {
    for (std::size_t index = 0; index < arena.spawns.size(); ++index) {
        const sim::SpawnPoint& spawn = arena.spawns[index];
        const auto found = P::FindBoxContacts(BodyShape(size.height, size.radius), BodyPose(spawn.position, size.height), walls,
                                              P::Pose{}, 0.0F);
        bool inside = !found;
        if (found) {
            for (const P::BoxContact& contact : *found) inside = inside || contact.contact.distance < -kTouch;
        }
        if (inside) return "spawn " + std::to_string(index) + " of arena '" + arena.id + "' is inside a wall";
    }
    return std::nullopt;
}

// Every end, Main/Update while the World generates (world/ArenaLoad.hpp): the arena's map from the asset store, then
// its walls, built from its boxes on the worker, into kArenaEntity's Collider. The World generates until they are in;
// a map that fails keeps it generating, and MapLoad says why. Only the boxes reach the worker and the handle goes once
// they are copied: the World never keeps the map's faces.
K::SystemResult LoadArena(K::Res<WA::AssetRequests> requests, K::Res<WA::AssetView> view, K::ResMut<ArenaLoading> loading,
                          K::ResMut<MapLoad> load, K::ResMut<GameRules> rules, K::ResMut<MatchInfo> info,
                          K::ResMut<O::WorldGeneration> generation, K::CommandsFor<K::Inserts<P::Collider>, K::Removes<>> walls) {
    const MapStage stage = load.Read().stage;
    if (stage == MapStage::Done) return {};
    // Every frame but the one the map is done in holds the generation.
    const auto hold = [&generation] {
        generation->Hold();
        return K::SystemResult{};
    };
    if (stage == MapStage::Failed) return hold();
    ArenaLoading& arena = loading.Get();
    const auto fail = [&](std::string why) {
        arena.handle = {};
        MapLoad& now = load.Get();
        now.stage = MapStage::Failed;
        now.failure = std::move(why);
    };
    if (stage == MapStage::Reading) {
        if (arena.handle.Id().Empty()) arena.handle = requests->Load<VM::VoxelMapAsset>(AS::AssetId(arena.map));
        const MapReading reading = ReadMap(*view, arena.handle);
        if (!reading.failure.empty()) {
            fail(arena.map + ": " + reading.failure);
            return hold();
        }
        if (load.Read().done != reading.done || load.Read().total != reading.total) {
            load->done = reading.done;
            load->total = reading.total;
        }
        if (!reading.complete) return hold();
        data::ArenaMap boxes = data::CopyMapPart(*reading.index, reading.regions, data::MapPart::Walls);
        arena.handle = {};
        arena.walls = WorkerTask<WallsBuilt>::Start(arena.jobs, [boxes = std::move(boxes)] { return P::BoxSet::Build(data::MapColliders(boxes)); });
        load->stage = MapStage::Building;
        return hold();
    }
    std::optional<WallsBuilt> built = arena.walls.Take();
    if (!built) return hold();
    if (!*built) {
        fail(arena.map + ": the walls do not build: " + Base::Describe(built->error()));
        return hold();
    }
    P::BoxSet boxes = std::move(**built);
    if (auto inside = SpawnInWall(rules.Read().arena, rules.Read().character->bodySize, boxes)) {
        fail(std::move(*inside));
        return hold();
    }
    const std::uint64_t digest = boxes.Digest();
    rules->wallsDigest = digest;
    if (arena.announce) {
        info->arenaId = rules.Read().arena.id;
        info->wallsDigest = digest;
    }
    walls.Insert(kArenaEntity, P::Collider{P::Shape{std::move(boxes)}});
    load->stage = MapStage::Done;
    return {};
}

// onServer: mounted with the net Module's net.OnServer (a System of the authority alone, on a networked end; a Local
// World is the authority itself and mounts it without).
K::SystemOptions Fixed(std::string name, std::string stage, std::vector<std::string> after, const bool onServer) {
    K::SystemOptions options;
    options.name = std::move(name);
    K::Mount mount;
    mount.schedule = std::string(O::DefaultSchedules::fixed);
    mount.stage = std::move(stage);
    mount.after = std::move(after);
    if (onServer) mount.conditions.push_back(N::OnServer());
    options.mounts.push_back(std::move(mount));
    return options;
}

K::SystemOptions Frame(std::string name, std::string stage, K::ConditionRef condition) {
    K::SystemOptions options;
    options.name = std::move(name);
    K::Mount mount;
    mount.schedule = std::string(O::DefaultSchedules::frame);
    mount.stage = std::move(stage);
    mount.conditions.push_back(std::move(condition));
    options.mounts.push_back(std::move(mount));
    return options;
}

std::optional<std::string> ReadText(GYO::Engine::IO::FS::NativeFileSystem& fs, const std::string& path, std::string& error) {
    auto uri = GYO::Engine::IO::FS::NativeFileUri(path);
    if (!uri) {
        error = path + ": " + Base::Describe(uri.error());
        return std::nullopt;
    }
    auto text = GYO::Engine::IO::Helpers::ReadAllTextUtf8(fs, *uri);
    if (!text) {
        error = path + ": " + Base::Describe(text.error());
        return std::nullopt;
    }
    return std::move(*text);
}

} // namespace

bool Blocked(const P::BoxSet& walls, const Math::Vec3& a, const Math::Vec3& b) {
    const float length = Math::Distance(a, b);
    // Two points this close are not apart: nothing stands between them.
    constexpr float kApart = 1.0e-3F;
    if (!(length > kApart)) return false;
    const Math::Ray ray{a, (b - a) * (1.0F / length)};
    // A wall the segment only touches at its far end (a source against a wall) does not stand between.
    const std::optional<P::RayHit> hit = P::CastRay(P::Shape{walls}, P::Pose{}, ray, length);
    return hit.has_value() && hit->t < length - kApart;
}

const sim::ArenaContent* GameContent::Find(const std::string& id) const {
    for (const sim::ArenaContent& arena : arenas) {
        if (arena.id == id) return &arena;
    }
    return nullptr;
}

std::optional<std::size_t> GameContent::FindArena(const std::string_view id) const {
    for (std::size_t i = 0; i < arenas.size(); ++i) {
        if (arenas[i].id == id) return i;
    }
    return std::nullopt;
}

bool GameContent::Plays(const std::size_t mode, const std::size_t arena) const { return modes[mode].Plays(arenas[arena].id); }

std::uint64_t GameContent::ArenaDigest(const std::size_t arena) const {
    K::Hasher h;
    HashValue(h, std::string("gyo.object_fps_pvp.arena/1"));
    HashValue(h, arenas[arena]);
    return Digest64(h);
}

GameRules GameRulesOf(const GameContent& content, const std::size_t mode, const std::size_t arena) {
    GameRules rules;
    rules.arena = content.arenas[arena];
    rules.movement = content.modes[mode].movement;
    rules.damage = content.modes[mode].damage;
    rules.combat = content.combat;
    rules.character = content.character;
    rules.characterDigest = content.characterDigest;
    rules.arenaDigest = content.ArenaDigest(arena);
    return rules;
}

std::optional<std::size_t> GameContent::FindMode(const std::string_view id) const {
    for (std::size_t i = 0; i < modes.size(); ++i) {
        if (modes[i].id == id) return i;
    }
    return std::nullopt;
}

std::optional<GameContent> LoadGameContent(const std::string& root, std::string& error, const std::vector<GameplaySet>& sets) {
    error.clear();
    GYO::Engine::IO::FS::NativeFileSystem fs;
    const std::string maps = root + "/maps/";
    GameContent content;
    content.sets = sets;
    auto listText = ReadText(fs, maps + "arenas.json", error);
    if (!listText) return std::nullopt;
    auto list = sim::ParseArenaList(*listText, error);
    if (!list) {
        error = maps + "arenas.json: " + error;
        return std::nullopt;
    }
    for (const std::string& name : *list) {
        auto arenaText = ReadText(fs, maps + name, error);
        if (!arenaText) return std::nullopt;
        auto arena = sim::ParseArena(*arenaText, error);
        if (!arena) {
            error = maps + name + ": " + error;
            return std::nullopt;
        }
        content.arenas.push_back(std::move(*arena));
    }
    auto rig = character::LoadCharacterRig(root, error);
    if (!rig) return std::nullopt;
    // Every spawn leaves room for the character's body to the arena's bounds.
    for (std::size_t a = 0; a < content.arenas.size(); ++a) {
        const sim::ArenaContent& arena = content.arenas[a];
        for (std::size_t s = 0; s < arena.spawns.size(); ++s) {
            if (!sim::SpawnInBounds(arena, arena.spawns[s], rig->bodySize.radius)) {
                error = maps + (*list)[a] + ": spawn " + std::to_string(s) + " is outside the bounds for the character's radius";
                return std::nullopt;
            }
        }
    }
    content.character = std::make_shared<const character::CharacterRig>(std::move(*rig));
    content.characterDigest = CharacterDigest(*content.character);
    auto modes = LoadModes(root, content.arenas, content.combat, content.sets, error);
    if (!modes) return std::nullopt;
    content.modes = std::move(*modes);
    return content;
}

void ConfigureGame(O::ProductSetup& setup) {
    auto* registry = setup.Buffer().Find<AS::AssetLoaderRegistry>();
    if (registry == nullptr) {
        setup.Report("NoAssetStore", "the asset store is not there (world_asset is not carried): the maps cannot be read");
        return;
    }
    if (auto map = registry->template Register<VM::VoxelMapLoader>(); !map) {
        setup.Report("LoaderRefused", "the voxel_map loader was refused: " + Base::Describe(map.error()));
        return;
    }
    if (auto region = registry->template Register<VM::VoxelRegionLoader>(); !region) {
        setup.Report("LoaderRefused", "the voxel_region loader was refused: " + Base::Describe(region.error()));
    }
}

std::optional<std::string> Seats::Take(const std::uint64_t userId) {
    const std::lock_guard lock(mutex_);
    if (const auto seated = connections_.find(userId); seated != connections_.end()) {
        ++seated->second;
        return std::nullopt;
    }
    if (connections_.size() >= capacity_) return "the match is full";
    connections_.emplace(userId, 1);
    return std::nullopt;
}

void Seats::Release(const std::uint64_t userId) {
    const std::lock_guard lock(mutex_);
    const auto seated = connections_.find(userId);
    if (seated == connections_.end()) return;
    if (--seated->second == 0) connections_.erase(seated);
}

std::uint32_t Seats::Taken() const {
    const std::lock_guard lock(mutex_);
    return static_cast<std::uint32_t>(connections_.size());
}

N::ServerHost::Admission SeatAdmission(std::shared_ptr<Seats> seats) {
    return [seats = std::move(seats)](const N::ConnectRequest& request, N::Endpoint) -> N::NetResult<void> {
        if (auto refused = seats->Take(request.userId)) {
            return Base::Err(N::NetFailure::Make(N::NetFailureCode::MessageRejected, std::move(*refused)));
        }
        return {};
    };
}

N::ServerHost::Admission MatchAdmission(std::shared_ptr<Seats> seats, AdmissionCheck check) {
    return [seats = std::move(seats), check = std::move(check)](const N::ConnectRequest& request, N::Endpoint) -> N::NetResult<void> {
        if (const auto refused = match::VerifyAdmission(check.key, request.token, check.nowUnixSeconds(), check.matchId, request.userId)) {
            return Base::Err(N::NetFailure::Make(N::NetFailureCode::MessageRejected, std::string(match::AdmissionRefusalName(*refused))));
        }
        if (auto refused = seats->Take(request.userId)) {
            return Base::Err(N::NetFailure::Make(N::NetFailureCode::MessageRejected, std::move(*refused)));
        }
        return {};
    };
}

void BuildWorld(O::WorldSetup& setup, const WorldOptions& options, const WorldEnd end) {
    GYO_ASSERT(options.content && options.arena < options.content->arenas.size() && options.mode < options.content->modes.size());
    K::Registrar& r = setup.Registrar();
    A::AuthorityEntry authority(setup);
    authority.Component<PlayerBody>("PlayerBody");
    authority.Component<PlayerCombat>("PlayerCombat");
    authority.Component<ShotRecord>("ShotRecord");
    authority.Component<HitRecord>("HitRecord");
    authority.Component<PlayerInput>("PlayerInput", {.input = true});
    r.Component<Owner>({.name = "Owner"});
    r.Component<PlayerMotion>({.name = "PlayerMotion"});
    r.Resource<GameRules>({.name = "GameRules"});
    r.Resource<MatchInfo>({.name = "MatchInfo"});
    r.Resource<LocalUser>({.name = "LocalUser"});
    r.Resource<MatchSeats>({.name = "MatchSeats"});
    r.Resource<SeatedPlayers>({.name = "SeatedPlayers"});
    r.Resource<MapLoad>({.name = "MapLoad"});
    r.Resource<ArenaLoading>({.name = "ArenaLoading"});
    r.Resource<ContentMismatch>({.name = "ContentMismatch"});
    r.Resource<LocalSpawn>({.name = "LocalSpawn"});
    r.Resource<LocalDeaths>({.name = "LocalDeaths"});
    r.Resource<MatchStart>({.name = "MatchStart"});
    r.Resource<ActiveMode>({.name = "ActiveMode"});
    const bool networked = end != WorldEnd::Local;
    {
        K::SystemOptions load;
        load.name = "LoadArena";
        K::Mount mount;
        mount.schedule = std::string(O::DefaultSchedules::frame);
        mount.stage = std::string(O::DefaultSchedules::update);
        mount.conditions.push_back(O::WhileGenerating());
        load.mounts.push_back(std::move(mount));
        r.AddSystem<&LoadArena>(std::move(load));
    }

    if (networked) {
        N::NetEntry net(setup);
        net.Replicate<PlayerBody>("PlayerBody", BodyCodec());
        net.Replicate<PlayerCombat>("PlayerCombat", CombatCodec());
        net.Replicate<ShotRecord>("ShotRecord", ShotCodec());
        net.Replicate<HitRecord>("HitRecord", HitCodec());
        net.Replicate<Owner>("Owner", OwnerCodec());
        N::NetEntry::ResourceOptions<MatchInfo> info;
        info.codec = MatchInfoCodec();
        net.ReplicateResource<MatchInfo>("MatchInfo", std::move(info));
        net.Input<PlayerInput>("PlayerInput", InputOptions());
        R::RollbackEntry(setup).StepSchedule();
    }

    const std::string update = std::string(O::DefaultSchedules::update);
    const std::string postUpdate = std::string(O::DefaultSchedules::postUpdate);
    // The Client takes the phase from the Match; the authority decides it.
    if (end != WorldEnd::Client) r.AddSystem<&StartMatch>(Fixed("StartMatch", update, {}, false));
    r.AddSystem<&MovePlayers>(Fixed("MovePlayers", update, end != WorldEnd::Client ? std::vector<std::string>{"StartMatch"} : std::vector<std::string>{}, false));
    r.AddSystem<&UseWeapons>(Fixed("UseWeapons", update, {"MovePlayers"}, false));
    r.AddSystem<&AnimatePlayers>(Fixed("AnimatePlayers", postUpdate, {}, networked));
    r.AddSystem<&JudgeShots>(Fixed("JudgeShots", postUpdate, {"AnimatePlayers"}, networked));
    r.AddSystem<&RespawnPlayers>(Fixed("RespawnPlayers", postUpdate, {"JudgeShots"}, networked));
    if (networked) {
        r.AddSystem<&SeatPlayers>(Frame("SeatPlayers", update, N::OnServer()));
        K::SystemOptions show = Frame("ShowPlayers", update, N::OnServer());
        show.mounts.front().after = {"SeatPlayers"};
        r.AddSystem<&ShowPlayers>(std::move(show));
        r.AddSystem<&ControlOwnPlayer>(Frame("ControlOwnPlayer", update, N::OnClient()));
        r.AddSystem<&FollowMatchContent>(Frame("FollowMatchContent", std::string(O::DefaultSchedules::preUpdate), N::OnClient()));
    } else {
        K::SystemOptions local;
        local.name = "SpawnLocalPlayer";
        K::Mount mount;
        mount.schedule = std::string(O::DefaultSchedules::frame);
        mount.stage = update;
        mount.conditions.push_back(O::AfterGenerating());
        local.mounts.push_back(std::move(mount));
        r.AddSystem<&SpawnLocalPlayer>(std::move(local));
        K::SystemOptions deaths;
        deaths.name = "CountLocalDeaths";
        K::Mount last;
        last.schedule = std::string(O::DefaultSchedules::frame);
        last.stage = std::string(O::DefaultSchedules::last);
        deaths.mounts.push_back(std::move(last));
        r.AddSystem<&CountLocalDeaths>(std::move(deaths));
    }

    // The gameplay sets, after every registration of the core (the core's authority numbers and wire ids stay), all of
    // them whatever the mode: each one's two Kernel sets run only when the mode turns it on.
    const std::vector<GameplaySet>& sets = options.content->sets;
    for (std::size_t slot = 0; slot < sets.size(); ++slot) {
        const GameplaySet& set = sets[slot];
        r.Set({.name = GameplaySetFixed(set.name), .schedule = std::string(O::DefaultSchedules::fixed)}, {GameplaySetOn(slot, set.name)});
        r.Set({.name = GameplaySetFrame(set.name), .schedule = std::string(O::DefaultSchedules::frame)}, {GameplaySetOn(slot, set.name)});
        set.build(setup);
    }

    setup.Prepare([options, end](K::World& world) -> Base::Result<void, O::OrchestrationError> {
        const sim::ArenaContent& arena = options.content->arenas[options.arena];
        const ModeContent& mode = options.content->modes[options.mode];
        GameRules rules = GameRulesOf(*options.content, options.mode, options.arena);
        const std::uint64_t arenaDigest = rules.arenaDigest;
        world.SetResource(std::move(rules));
        world.SetResource(LocalUser{options.localUser});
        world.SetResource(MatchSeats{options.seats});
        world.SetResource(SeatedPlayers{});
        world.SetResource(MapLoad{});
        world.SetResource(ArenaLoading{.map = arena.map, .announce = end != WorldEnd::Client, .jobs = options.jobs});
        world.SetResource(ContentMismatch{});
        world.SetResource(LocalSpawn{options.spawn});
        world.SetResource(LocalDeaths{});
        world.SetResource(ActiveMode{mode.id, mode.rulesDigest, mode.on});
        // What the sets the mode turns on read, at every end (the rules digest says both ends read the same).
        for (const auto& [slot, read] : mode.sets) {
            if (read.put) read.put(world);
        }
        // Who the start waits for: the Match's expected users (none: it is played from the first step, a Match
        // started by hand), a Local World's own player.
        MatchStart start;
        start.expect = end == WorldEnd::Local ? std::vector<std::uint64_t>{options.localUser} : options.expectUsers;
        std::sort(start.expect.begin(), start.expect.end());
        start.expect.erase(std::unique(start.expect.begin(), start.expect.end()), start.expect.end());
        start.deadlineSeconds = end == WorldEnd::Local ? 0 : options.startDeadlineSeconds;
        world.SetResource(std::move(start));
        // The Match (and a Local World) says which arena it plays once its walls are built (LoadArena); a Client's
        // MatchInfo comes from the Match.
        MatchInfo info;
        if (end != WorldEnd::Client) {
            info.modeId = mode.id;
            info.rulesDigest = mode.rulesDigest;
            info.arenaDigest = arenaDigest;
            info.characterDigest = options.content->characterDigest;
        }
        if (end == WorldEnd::Match && options.expectUsers.empty()) info.phase = MatchPhase::Playing;
        world.SetResource(std::move(info));
        world.Insert(kArenaEntity, T::Transform{});
        return {};
    });
}

namespace {

// The fixed steps wait for the World to generate (its walls): before that nothing is played.
WT::WorldTimeOptions StepsAfterGenerating() {
    WT::WorldTimeOptions time;
    time.fixedStepConditions.push_back(O::AfterGenerating());
    return time;
}

O::WorldDescription Describe(WorldOptions options, const N::NetRole role) {
    O::WorldDescription world;
    world.name = "Game";
    // The units the game needs, named so a Product that carries more (the Client's render units) does not put them
    // here (an empty list is every unit the Product carries).
    world.units = {"entity_ids", "world_time", "world_asset", "rollback", "authority", "net", "transform", "physics_3d"};
    world.options.Set(StepsAfterGenerating());
    world.options.Set(N::NetWorldOptions{role, std::nullopt});
    world.options.Set(role == N::NetRole::Server ? kMatchIds : kClientIds);
    world.options.Set(A::AuthorityOptions{std::make_shared<A::ServerAuthorityStrategy>()});
    world.dynamicInsert = false;
    if (role == N::NetRole::Client) {
        R::RollbackOptions rollback;
        rollback.strategy = std::make_shared<R::StateRollbackStrategy>();
        world.options.Set(std::move(rollback));
    }
    world.build = [options = std::move(options), role](O::WorldSetup& setup) {
        BuildWorld(setup, options, role == N::NetRole::Server ? WorldEnd::Match : WorldEnd::Client);
    };
    return world;
}

} // namespace

O::WorldDescription MatchWorld(WorldOptions options) { return Describe(std::move(options), N::NetRole::Server); }
O::WorldDescription ClientWorld(WorldOptions options) { return Describe(std::move(options), N::NetRole::Client); }

O::WorldDescription LocalWorld(WorldOptions options) {
    O::WorldDescription world;
    world.name = "Game";
    // No net, and the rollback Module without a strategy (authority needs it there; nothing is predicted): the World is
    // the authority.
    world.units = {"entity_ids", "world_time", "world_asset", "rollback", "authority", "transform", "physics_3d"};
    world.options.Set(StepsAfterGenerating());
    world.options.Set(kMatchIds);
    world.options.Set(A::AuthorityOptions{std::make_shared<A::ServerAuthorityStrategy>()});
    world.dynamicInsert = false;
    world.build = [options = std::move(options)](O::WorldSetup& setup) { BuildWorld(setup, options, WorldEnd::Local); };
    return world;
}

sim::AnimationInput AnimationInputOf(const PlayerBody& body, const PlayerCombat& combat, const HitRecord& hits, const GameRules& rules,
                                     const std::uint64_t tick, const std::uint64_t stepNanoseconds) {
    const double stepSeconds = static_cast<double>(stepNanoseconds) / 1e9;
    sim::AnimationInput input;
    const auto at = [stepSeconds](const std::uint64_t t) { return static_cast<double>(t) * stepSeconds; };
    input.seconds = at(tick);
    input.feet = body.feet;
    input.yaw = body.yaw;
    input.pitch = body.pitch;
    input.grounded = body.grounded != 0;
    input.crouched = body.crouched != 0;
    input.dead = combat.life == sim::LifeState::Dead;
    input.lifeSeconds = at(combat.lifeTick);
    if (combat.lastShotTick != 0) input.shotSeconds = at(combat.lastShotTick);
    if (combat.reloadEndTick != 0) {
        input.reloadEnd = at(combat.reloadEndTick);
        input.reloadStart = *input.reloadEnd - static_cast<double>(sim::TicksFor(rules.combat.reloadSeconds, stepNanoseconds)) * stepSeconds;
    }
    if (hits.hits != 0) {
        input.hitSeconds = at(hits.tick);
        input.hitPart = hits.part;
    }
    return input;
}

K::EntityId OwnPlayer(K::World& world) {
    const LocalUser* user = world.Resource<LocalUser>();
    if (user == nullptr) return 0;
    for (const K::EntityId id : world.Entities()) {
        const Owner* owner = world.Get<Owner>(id);
        if (owner != nullptr && owner->userId == user->userId && world.Get<PlayerInput>(id) != nullptr) return id;
    }
    return 0;
}

} // namespace object_fps_pvp::world
