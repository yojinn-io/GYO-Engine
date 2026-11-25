#include "object_fps_pvp/world/PvpWorld.hpp"

#include "object_fps_pvp/match/AdmissionToken.hpp"
#include "object_fps_pvp/sim/PlanarMovement.hpp"
#include "object_fps_pvp/sim/ViewLimits.hpp"

#include "engine/authority/AuthorityStrategy.hpp"
#include "engine/base/Assert.hpp"
#include "engine/authority/Registrations.hpp"
#include "engine/authority/ServerAuthorityStrategy.hpp"
#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/ReadAllText.hpp"
#include "engine/math/scalar/Angle.hpp"
#include "engine/math/scalar/Scalar.hpp"
#include "engine/net/NetModule.hpp"
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
#include "engine/world_time/TimeTypes.hpp"

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
}
void HashValue(K::Hasher&, const GameRules&) {}
void HashValue(K::Hasher&, const MatchInfo&) {}
void HashValue(K::Hasher&, const LocalUser&) {}
void HashValue(K::Hasher&, const MatchSeats&) {}
void HashValue(K::Hasher&, const ContentRef&) {}
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
                     },
                     [](N::NetReader& r, PlayerInput& v) {
                         return r.Value(v.forward) && r.Value(v.right) && r.Value(v.yaw) && r.Value(v.pitch) &&
                                r.Value(v.present) && r.Value(v.jump) && r.Value(v.crouch) && r.Value(v.fire) &&
                                r.Value(v.reload);
                     },
                     1};
    // A held input is the last one with its presses taken out: a press fires once (C-IN3).
    options.held = [](PlayerInput& v) {
        v.jump = 0;
        v.fire = 0;
        v.reload = 0;
    };
    return options;
}
N::Codec<MatchInfo> MatchInfoCodec() {
    return {[](N::NetWriter& w, const MatchInfo& v) {
                w.Value(static_cast<std::uint16_t>(v.arenaId.size()));
                for (const char c : v.arenaId) w.Value(static_cast<std::uint8_t>(c));
                w.Value(v.wallsDigest);
            },
            [](N::NetReader& r, MatchInfo& v) {
                std::uint16_t size = 0;
                if (!r.Value(size) || size > 64) return false;
                v.arenaId.clear();
                for (std::uint16_t i = 0; i < size; ++i) {
                    std::uint8_t c = 0;
                    if (!r.Value(c)) return false;
                    v.arenaId.push_back(static_cast<char>(c));
                }
                return r.Value(v.wallsDigest);
            },
            1};
}

// ---- The body's shape and the queries against the walls

using Walls = K::Query<K::Read<P::Collider>, K::Read<T::Transform>, K::Without<K::ChildOf>>;

// The upright capsule of a body of height (feet at the bottom), placed at feet.
P::Shape BodyShape(const float height, const float radius) { return P::CapsuleY(0.5F * height - radius, radius); }
P::Pose BodyPose(const Math::Vec3& feet, const float height) { return P::Pose{feet + Math::Vec3{0.0F, 0.5F * height, 0.0F}, {}}; }

// The ground probe: a body is on the ground when it stands on the floor (y = 0, a rule of the game) or a short sweep
// down meets a face that faces up (normal y > 0.5).
constexpr float kGroundProbe = 0.002F;
constexpr float kGroundNormalY = 0.5F;

P::MoveAndSlideConfig MoveConfig() { return P::MoveAndSlideConfig{}; }

Base::Result<bool, P::Physics3dError> OnGround(const Walls& walls, const Math::Vec3& feet, const float height,
                                               const float radius) {
    if (feet.y <= kGroundProbe) return true;
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

// One movement step of a living body with input (present): the view, the stance, the jump, the fall and the slide
// against the walls; a body without input keeps its view and stance and only falls.
Base::Result<void, P::Physics3dError> StepBody(const Walls& walls, const GameRules& rules, PlayerBody& body,
                                               const PlayerInput& input, const bool alive, const double seconds) {
    const sim::ArenaContent& arena = rules.arena;
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
            auto room = Fits(walls, start, arena.bodyHeight, arena.radius);
            if (!room) return Base::Err(std::move(room).error());
            if (*room) crouched = false;
        }
    }
    body.crouched = crouched ? 1 : 0;
    const float height = sim::BodyHeight(arena, rules.stance, crouched);
    const float speed = sim::MoveSpeed(arena, rules.stance, crouched);
    const sim::GroundPoint step = steering ? sim::ComputePlanarDisplacement(std::clamp(input.forward, -1.0F, 1.0F),
                                                                            std::clamp(input.right, -1.0F, 1.0F), body.yaw,
                                                                            speed, static_cast<float>(seconds))
                                           : sim::GroundPoint{};
    if (body.grounded != 0) {
        auto ground = OnGround(walls, start, height, arena.radius);
        if (!ground) return Base::Err(std::move(ground).error());
        body.grounded = *ground ? 1 : 0;
    }
    if (steering && input.jump != 0 && body.grounded != 0 && !crouched) {
        body.verticalVelocity = sim::JumpSpeed(arena);
        body.grounded = 0;
    }
    float desiredY = start.y;
    if (body.grounded == 0) {
        const sim::VerticalStep fall = sim::Fall(start.y, body.verticalVelocity, arena.gravity, seconds);
        desiredY = fall.y;
        body.verticalVelocity = fall.velocity;
    }
    const Math::Vec3 movement{step.x, desiredY - start.y, step.z};
    Math::Vec3 end = start;
    if (Math::LengthSquared(movement) > 0.0F) {
        P::MoveAndSlideConfig config = MoveConfig();
        if (body.grounded != 0) config.planes.push_back(Math::Vec3{0.0F, 1.0F, 0.0F});
        const float invSeconds = static_cast<float>(1.0 / seconds);
        auto moved = P::MoveAndSlide(walls, BodyShape(height, arena.radius), BodyPose(start, height), movement * invSeconds,
                                     static_cast<float>(seconds), config);
        if (!moved) return Base::Err(std::move(moved).error());
        end = moved->position - Math::Vec3{0.0F, 0.5F * height, 0.0F};
        // The floor is the plane y = 0.
        if (end.y < 0.0F) end.y = 0.0F;
    }
    body.feet = end;
    if (std::abs(end.y - desiredY) > 0.0001F) body.verticalVelocity = 0.0F;
    if (body.verticalVelocity <= 0.0F) {
        auto ground = OnGround(walls, end, height, arena.radius);
        if (!ground) return Base::Err(std::move(ground).error());
        body.grounded = *ground ? 1 : 0;
    } else {
        body.grounded = 0;
    }
    if (body.grounded != 0) body.verticalVelocity = 0.0F;
    return {};
}

double StepSeconds(const WT::ActiveTime& time) { return static_cast<double>(time.delta) / 1e9; }
std::uint64_t StepNanos(const WT::ActiveTime& time) { return static_cast<std::uint64_t>(time.delta); }

// ---- The Systems of the fixed step

K::SystemResult MovePlayers(K::Query<K::Read<PlayerInput>, K::Read<PlayerCombat>, K::Write<PlayerBody>> players,
                            Walls walls, K::Res<GameRules> rules, K::Res<WT::ActiveTime> time) {
    std::optional<P::Physics3dError> failure;
    const double seconds = StepSeconds(*time);
    if (!(seconds > 0.0)) return {};
    players.ForEach([&](K::EntityId, const PlayerInput& input, const PlayerCombat& combat, K::Mut<PlayerBody> body) {
        if (failure) return;
        PlayerBody next = body.Read();
        auto stepped = StepBody(walls, *rules, next, input, combat.life == sim::LifeState::Alive, seconds);
        if (!stepped) {
            failure = std::move(stepped).error();
            return;
        }
        if (!(next == body.Read())) body.Get() = next;
    });
    if (failure) return PhysicsFailed(*failure);
    return {};
}

K::SystemResult UseWeapons(K::Query<K::Read<PlayerInput>, K::Write<PlayerCombat>> players, K::Res<GameRules> rules,
                           K::Res<WT::Tick> tick, K::Res<WT::ActiveTime> time) {
    const sim::CombatRules& combat = rules->combat;
    const std::uint64_t now = tick->value;
    const std::uint64_t step = StepNanos(*time);
    players.ForEach([&](K::EntityId, const PlayerInput& input, K::Mut<PlayerCombat> cell) {
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
    const float eye = sim::EyeHeight(rules.arena, rules.stance, body.crouched != 0);
    return Math::Ray{body.feet + Math::Vec3{0.0F, eye, 0.0F}, sim::ViewDirection(body.yaw, body.pitch)};
}

// The body's stance capsule: where a body stands, for placing a spawn (shots meet the shapes on the bones).
P::Capsule BodyCapsule(const GameRules& rules, const PlayerBody& body) {
    const float height = sim::BodyHeight(rules.arena, rules.stance, body.crouched != 0);
    const float r = rules.arena.radius;
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
        // The floor: a solid half-space below y = 0.
        if (ray.origin.y <= 0.0F) {
            world = 0.0F;
        } else if (ray.direction.y < 0.0F) {
            world = ray.origin.y / -ray.direction.y;
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
            if (targetCombat.Read().hp == 0) {
                targetCombat->life = sim::LifeState::Dead;
                targetCombat->lifeTick = now;
                targetCombat->respawnTick = now + sim::TicksFor(combat.respawnSeconds, StepNanos(*time));
                targetCombat->reloadEndTick = 0;
            }
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
    auto fits = Fits(walls, spawn.position, rules.arena.bodyHeight, rules.arena.radius);
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

// Match: a player for each Peer that connects (at the farthest free spawn), gone with its Peer.
K::SystemResult SeatPlayers(K::MessageReader<N::PeerConnected> connected, K::MessageReader<N::PeerDisconnected> disconnected,
                            K::Query<K::Read<Owner>, K::Read<PlayerBody>, K::Read<PlayerCombat>> players, Walls walls,
                            K::Res<GameRules> rules, K::Res<MatchSeats> seats,
                            K::Res<WT::Tick> tick,
                            K::Spawn<K::Bundle<PlayerBody, PlayerCombat, ShotRecord, HitRecord, Owner, PlayerInput, PlayerMotion>> spawn,
                            K::Despawn despawn, K::CommandsFor<K::Inserts<N::PeerControls>, K::Removes<>> controls) {
    for (const N::PeerDisconnected* gone : disconnected.Read()) {
        players.ForEach([&](const K::EntityId id, const Owner& owner, const PlayerBody&, const PlayerCombat&) {
            if (owner.userId == gone->userId) despawn.Entity(id);
        });
        if (seats->seats && gone->peer != 0) seats->seats->Release(gone->userId);
    }
    std::vector<PlayerBody> living;
    players.ForEach([&](K::EntityId, const Owner&, const PlayerBody& body, const PlayerCombat& combat) {
        if (combat.life == sim::LifeState::Alive) living.push_back(body);
    });
    for (const N::PeerConnected* joined : connected.Read()) {
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
        living.push_back(body);
    }
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

// Client: the arena the Match plays. An arena this Client does not have, or whose walls differ, is a content
// mismatch (ContentMismatch); the Client's program decides what to do (it disconnects).
K::SystemResult FollowMatchArena(K::ExclusiveWorld<> world) {
    auto info = world.Read<MatchInfo>([](const MatchInfo& m) { return m; });
    if (!info || !info->has_value() || (*info)->arenaId.empty()) return {};
    const MatchInfo& match = **info;
    auto current = world.Read<GameRules>([](const GameRules& r) { return std::pair{r.arena.id, r.wallsDigest}; });
    if (!current || !current->has_value()) return {};
    if ((*current)->first == match.arenaId && (*current)->second == match.wallsDigest) return {};
    auto content = world.Read<ContentRef>([](const ContentRef& c) { return c.content; });
    const InstalledArena* arena = nullptr;
    if (content && content->has_value() && **content) arena = (**content)->Find(match.arenaId);
    if (arena == nullptr || arena->wallsDigest != match.wallsDigest) {
        auto mismatch = world.Read<ContentMismatch>([](const ContentMismatch& m) { return m.reason; });
        if (mismatch && mismatch->has_value() && !(*mismatch)->empty()) return {};
        const std::string reason = arena == nullptr ? "the Match plays arena '" + match.arenaId + "', which this Client does not have"
                                                    : "arena '" + match.arenaId + "' differs from the Match's (walls digest)";
        auto set = world.SetResource(ContentMismatch{reason});
        if (!set) return Base::Err(std::move(set).error());
        return {};
    }
    auto boxes = P::BoxSet::Build(arena->walls);
    if (!boxes) return PhysicsFailed(boxes.error());
    auto inserted = world.Insert(kArenaEntity, P::Collider{P::Shape{std::move(*boxes)}});
    if (!inserted) return Base::Err(std::move(inserted).error());
    GameRules rules = world.Read<GameRules>([](const GameRules& r) { return r; })->value_or(GameRules{});
    rules.arena = arena->content;
    rules.wallsDigest = arena->wallsDigest;
    auto set = world.SetResource(std::move(rules));
    if (!set) return Base::Err(std::move(set).error());
    return {};
}

K::SystemOptions Fixed(std::string name, std::string stage, std::vector<std::string> after, const bool serverOnly) {
    K::SystemOptions options;
    options.name = std::move(name);
    K::Mount mount;
    mount.schedule = std::string(O::DefaultSchedules::fixed);
    mount.stage = std::move(stage);
    mount.after = std::move(after);
    if (serverOnly) mount.conditions.push_back(N::OnServer());
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

const InstalledArena* GameContent::Find(const std::string& id) const {
    for (const InstalledArena& arena : arenas) {
        if (arena.content.id == id) return &arena;
    }
    return nullptr;
}

std::optional<GameContent> LoadGameContent(const std::string& root, std::string& error) {
    error.clear();
    GYO::Engine::IO::FS::NativeFileSystem fs;
    const std::string maps = root + "/maps/";
    GameContent content;
    {
        const std::string path = root + "/rules/movement_rules.json";
        auto text = ReadText(fs, path, error);
        if (!text) return std::nullopt;
        auto stance = sim::ParseStanceRules(*text, error);
        if (!stance) {
            error = path + ": " + error;
            return std::nullopt;
        }
        content.stance = *stance;
    }
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
        if (!sim::StanceFits(*arena, content.stance)) {
            error = maps + name + ": the movement rules do not fit the arena's body";
            return std::nullopt;
        }
        auto meshText = ReadText(fs, maps + arena->geometry, error);
        if (!meshText) return std::nullopt;
        auto mesh = data::ParseVoxelMeshFile(*meshText, error);
        if (!mesh) {
            error = maps + arena->geometry + ": " + error;
            return std::nullopt;
        }
        InstalledArena installed;
        installed.content = std::move(*arena);
        installed.walls = data::ColliderBoxes(*mesh);
        installed.geometry = std::move(*mesh);
        auto boxes = P::BoxSet::Build(installed.walls);
        if (!boxes) {
            error = maps + name + ": the walls do not build: " + Base::Describe(boxes.error());
            return std::nullopt;
        }
        installed.wallsDigest = boxes->Digest();
        // Every spawn is free of the walls (whether bodies overlap is decided when they spawn).
        for (const sim::SpawnPoint& spawn : installed.content.spawns) {
            const float height = installed.content.bodyHeight;
            const auto found = P::FindBoxContacts(BodyShape(height, installed.content.radius), BodyPose(spawn.position, height),
                                                  *boxes, P::Pose{}, 0.0F);
            bool inside = !found;
            if (found) {
                for (const P::BoxContact& contact : *found) inside = inside || contact.contact.distance < -kTouch;
            }
            if (inside) {
                error = maps + name + ": a spawn is inside a wall";
                return std::nullopt;
            }
        }
        content.arenas.push_back(std::move(installed));
    }
    // The character, for the arenas' body (they share one: the character is drawn and judged at that height).
    for (const InstalledArena& arena : content.arenas) {
        if (arena.content.bodyHeight != content.arenas.front().content.bodyHeight) {
            error = maps + arena.content.geometry + ": the arenas do not share one body height";
            return std::nullopt;
        }
    }
    auto rig = character::LoadCharacterRig(root, content.arenas.front().content.bodyHeight, error);
    if (!rig) return std::nullopt;
    content.character = std::make_shared<const character::CharacterRig>(std::move(*rig));
    return content;
}

std::optional<std::string> Seats::Take(const std::uint64_t userId) {
    const std::lock_guard lock(mutex_);
    if (users_.contains(userId)) return "the user is already in the match";
    if (users_.size() >= capacity) return "the match is full";
    users_.insert(userId);
    taken.store(static_cast<std::uint32_t>(users_.size()));
    return std::nullopt;
}

void Seats::Release(const std::uint64_t userId) {
    const std::lock_guard lock(mutex_);
    users_.erase(userId);
    taken.store(static_cast<std::uint32_t>(users_.size()));
}

N::ServerHost::Admission SeatAdmission(std::shared_ptr<Seats> seats) {
    return [seats = std::move(seats)](const N::ConnectRequest& request, N::Endpoint) -> N::NetResult<void> {
        if (auto refused = seats->Take(request.userId)) {
            return Base::Err(N::NetFailure::Make(N::NetFailureCode::MessageRejected, std::move(*refused)));
        }
        return {};
    };
}

N::ServerHost::Admission MatchAdmission(std::shared_ptr<Seats> seats, AdmissionKey key) {
    return [seats = std::move(seats), key = std::move(key)](const N::ConnectRequest& request, N::Endpoint) -> N::NetResult<void> {
        if (const auto refused = match::VerifyAdmission(key.key, request.token, key.nowUnixSeconds(), key.matchId, request.userId)) {
            return Base::Err(N::NetFailure::Make(N::NetFailureCode::MessageRejected, std::string(match::AdmissionRefusalName(*refused))));
        }
        if (auto refused = seats->Take(request.userId)) {
            return Base::Err(N::NetFailure::Make(N::NetFailureCode::MessageRejected, std::move(*refused)));
        }
        return {};
    };
}

void BuildWorld(O::WorldSetup& setup, const WorldOptions& options) {
    GYO_ASSERT(options.content && options.arena < options.content->arenas.size());
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
    r.Resource<ContentRef>({.name = "ContentRef"});
    r.Resource<ContentMismatch>({.name = "ContentMismatch"});

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

    const std::string update = std::string(O::DefaultSchedules::update);
    const std::string postUpdate = std::string(O::DefaultSchedules::postUpdate);
    r.AddSystem<&MovePlayers>(Fixed("MovePlayers", update, {}, false));
    r.AddSystem<&UseWeapons>(Fixed("UseWeapons", update, {"MovePlayers"}, false));
    r.AddSystem<&AnimatePlayers>(Fixed("AnimatePlayers", postUpdate, {}, true));
    r.AddSystem<&JudgeShots>(Fixed("JudgeShots", postUpdate, {"AnimatePlayers"}, true));
    r.AddSystem<&RespawnPlayers>(Fixed("RespawnPlayers", postUpdate, {"JudgeShots"}, true));
    r.AddSystem<&SeatPlayers>(Frame("SeatPlayers", update, N::OnServer()));
    r.AddSystem<&ControlOwnPlayer>(Frame("ControlOwnPlayer", update, N::OnClient()));
    r.AddSystem<&FollowMatchArena>(Frame("FollowMatchArena", std::string(O::DefaultSchedules::preUpdate), N::OnClient()));

    setup.Prepare([options](K::World& world) -> Base::Result<void, O::OrchestrationError> {
        const InstalledArena& arena = options.content->arenas[options.arena];
        GameRules rules;
        rules.arena = arena.content;
        rules.stance = options.content->stance;
        rules.combat = options.content->combat;
        rules.wallsDigest = arena.wallsDigest;
        rules.character = options.content->character;
        world.SetResource(std::move(rules));
        world.SetResource(LocalUser{options.localUser});
        world.SetResource(MatchSeats{options.seats});
        world.SetResource(ContentRef{options.content});
        world.SetResource(ContentMismatch{});
        // The Match says which arena it plays; a Client's MatchInfo comes from the Match.
        world.SetResource(options.localUser == 0 ? MatchInfo{arena.content.id, arena.wallsDigest} : MatchInfo{});
        auto boxes = P::BoxSet::Build(arena.walls);
        GYO_ASSERT(static_cast<bool>(boxes) && "LoadGameContent built these walls");
        world.Insert(kArenaEntity, P::Collider{P::Shape{std::move(*boxes)}});
        world.Insert(kArenaEntity, T::Transform{});
        return {};
    });
}

namespace {

O::WorldDescription Describe(WorldOptions options, const N::NetRole role) {
    O::WorldDescription world;
    world.name = "Game";
    // The units the game needs, named so a Product that carries more (the Client's render units) does not put them
    // here (an empty list is every unit the Product carries).
    world.units = {"entity_ids", "world_time", "rollback", "authority", "net", "transform", "physics_3d"};
    world.options.Set(N::NetWorldOptions{role, std::nullopt});
    world.options.Set(role == N::NetRole::Server ? kMatchIds : kClientIds);
    world.options.Set(A::AuthorityOptions{std::make_shared<A::ServerAuthorityStrategy>()});
    world.dynamicInsert = false;
    if (role == N::NetRole::Client) {
        R::RollbackOptions rollback;
        rollback.strategy = std::make_shared<R::StateRollbackStrategy>();
        world.options.Set(std::move(rollback));
    }
    world.build = [options = std::move(options)](O::WorldSetup& setup) { BuildWorld(setup, options); };
    return world;
}

} // namespace

O::WorldDescription MatchWorld(WorldOptions options) { return Describe(std::move(options), N::NetRole::Server); }
O::WorldDescription ClientWorld(WorldOptions options) { return Describe(std::move(options), N::NetRole::Client); }

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
