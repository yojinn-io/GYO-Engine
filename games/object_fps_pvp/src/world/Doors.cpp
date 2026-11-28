#include "object_fps_pvp/world/Doors.hpp"

#include "world/Sight.hpp"

#include "data/JsonFields.hpp"
#include "object_fps_pvp/world/ContentHash.hpp"
#include "object_fps_pvp/world/Interaction.hpp"

#include "engine/authority/Registrations.hpp"
#include "engine/math/linear/Quaternion.hpp"
#include "engine/net/NetModule.hpp"
#include "engine/net/Replication.hpp"
#include "engine/physics_3d/Collider.hpp"
#include "engine/physics_3d/Queries.hpp"
#include "engine/physics_3d/Shapes.hpp"

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>

namespace object_fps_pvp::world {

namespace A = GYO::Engine::Authority;
namespace Base = GYO::Engine::Base;
namespace Json = GYO::Engine::Json;
namespace N = GYO::Engine::Net;
namespace P = GYO::Engine::Physics3d;
namespace T = GYO::Engine::Transform;

void HashValue(K::Hasher& h, const DoorState& v) { HashValue(h, v.open); }

void HashValue(K::Hasher& h, const DoorShape& v) {
    for (const float value : {v.hinge.x, v.hinge.y, v.hinge.z, v.yaw, v.width, v.height, v.thickness, v.swing, v.color.x, v.color.y, v.color.z}) {
        HashValue(h, value);
    }
}

T::Transform DoorPose(const DoorShape& shape, const bool open) {
    // A turn about +Y by the yaw takes +Z to the facing (sim::ViewDirection) and +X to the body's right
    // (sim::ComputePlanarInput's right axis).
    const float angle = shape.yaw + (open ? shape.swing : 0.0F);
    const Math::Quaternion turn = Math::MakeQuaternionFromAxisAngle(Math::Vec3{0.0F, 1.0F, 0.0F}, angle);
    T::Transform pose;
    pose.translation = shape.hinge + Math::Rotate(turn, Math::Vec3{0.5F * shape.width, 0.5F * shape.height, 0.0F});
    pose.rotation = turn;
    return pose;
}

namespace {

P::Collider LeafOf(const DoorShape& shape) {
    return P::Collider{P::Shape{P::Cuboid{{0.5F * shape.width, 0.5F * shape.height, 0.5F * shape.thickness}}}};
}

N::Codec<DoorState> DoorStateCodec() {
    return {[](N::NetWriter& w, const DoorState& v) { w.Value(v.open); },
            [](N::NetReader& r, DoorState& v) { return r.Value(v.open) && v.open <= 1; }, 1};
}

// Both ends, FixedUpdate/First: each door stands as its state says (a Client's door it receives nothing for keeps the
// pose it had).
K::SystemResult PoseDoors(K::Query<K::Read<DoorState>, K::Read<DoorShape>, K::Write<T::Transform>> doors) {
    doors.ForEach([](K::EntityId, const DoorState& state, const DoorShape& shape, K::Mut<T::Transform> transform) {
        const T::Transform pose = DoorPose(shape, state.open != 0);
        if (!(transform.Read() == pose)) transform.Get() = pose;
    });
    return {};
}

// The authority, FixedUpdate/PostUpdate after JudgeInteractions: each Interacted whose target is a door turns it, in
// the messages' order (two players at one door in one step turn it twice), unless the door's new pose would meet a
// living player's standing body (as a spawn is free, SpawnFree).
K::SystemResult OperateDoors(K::MessageReader<Interacted> interacted, K::Query<K::Write<DoorState>, K::Read<DoorShape>> doors,
                             K::Query<K::Read<PlayerBody>, K::Read<PlayerCombat>> players, K::Res<GameRules> rules) {
    for (const Interacted* used : interacted.Read()) {
        if (!doors.Contains(used->target)) continue;
        auto state = doors.GetMut<DoorState>(used->target);
        const DoorShape& shape = *doors.Get<DoorShape>(used->target);
        const bool open = state->open == 0;
        const T::Transform turned = DoorPose(shape, open);
        const P::Shape leaf = LeafOf(shape).shape;
        bool blocked = false;
        for (const K::EntityId id : players.Entities()) {
            if (players.Get<PlayerCombat>(id)->life != sim::LifeState::Alive) continue;
            auto contact = P::FindContact(P::Shape{BodyCapsule(*rules, *players.Get<PlayerBody>(id))}, P::Pose{}, leaf,
                                          P::Pose{turned.translation, turned.rotation}, 0.0F);
            if (!contact) {
                return Base::Err(K::KernelError(K::KernelErrorCode::SystemFailed, "object_fps_pvp: a door's turn failed: " + contact.error().message));
            }
            if ((blocked = contact->has_value())) break;
        }
        if (!blocked) state->open = open ? 1 : 0;
    }
    return {};
}

float Finite(const Json::Value& value, const std::string_view what) {
    const auto number = static_cast<float>(data::Number(value, what));
    if (!std::isfinite(number)) throw std::runtime_error(std::string(what) + " is not finite");
    return number;
}

Math::Vec3 Three(const Json::Value& value, const std::string_view what) {
    const auto array = value.AsArray();
    if (!array || array.value().Size() != 3) throw std::runtime_error(std::string(what) + " is three numbers");
    float v[3]{};
    std::size_t i = 0;
    for (const auto item : array.value()) v[i++] = Finite(item, what);
    return {v[0], v[1], v[2]};
}

// A door's data: hinge, yaw, width, height, thickness, swing, open, color.
std::optional<SetData> ReadDoor(const K::EntityId entity, const Json::ObjectView& data, std::string& error) {
    DoorShape shape;
    DoorState state;
    try {
        for (const auto member : data) {
            const std::string_view key = member.key;
            if (key != "hinge" && key != "yaw" && key != "width" && key != "height" && key != "thickness" && key != "swing" &&
                key != "open" && key != "color") {
                throw std::runtime_error("a door takes no '" + std::string(key) + "'");
            }
        }
        shape.hinge = Three(data::Member(data, "hinge"), "hinge");
        shape.yaw = Finite(data::Member(data, "yaw"), "yaw");
        shape.width = data::Positive(data::Member(data, "width"), "width");
        shape.height = data::Positive(data::Member(data, "height"), "height");
        shape.thickness = data::Positive(data::Member(data, "thickness"), "thickness");
        shape.swing = Finite(data::Member(data, "swing"), "swing");
        if (shape.swing == 0.0F || std::abs(shape.swing) > std::numbers::pi_v<float>) throw std::runtime_error("swing is not 0 < |swing| <= pi");
        state.open = data::Boolean(data::Member(data, "open"), "open") ? 1 : 0;
        shape.color = Three(data::Member(data, "color"), "color");
        for (const float channel : {shape.color.x, shape.color.y, shape.color.z}) {
            if (channel < 0.0F || channel > 1.0F) throw std::runtime_error("a color channel is not 0 to 1");
        }
    } catch (const std::runtime_error& failure) {
        error = failure.what();
        return std::nullopt;
    }
    K::Hasher h;
    HashValue(h, std::string("gyo.object_fps_pvp.door/1"));
    HashValue(h, shape);
    HashValue(h, state);
    return SetData{Digest64(h), [entity, shape, state](K::World& world) {
                       world.Insert(entity, state);
                       world.Insert(entity, shape);
                       world.Insert(entity, Interactable{});
                       world.Insert(entity, LeafOf(shape));
                       world.Insert(entity, DoorPose(shape, state.open != 0));
                   }};
}

} // namespace

GameplaySet DoorsSet() {
    GameplaySet set;
    set.name = kDoorsSet;
    set.needs = {kInteractionSet};
    set.build = [](O::WorldSetup& setup) {
        K::Registrar& r = setup.Registrar();
        r.Component<DoorShape>({.name = "DoorShape"});
        A::AuthorityEntry(setup).Component<DoorState>("DoorState");
        const bool networked = setup.Options<N::NetWorldOptions>() != nullptr;
        if (networked) N::NetEntry(setup).Replicate<DoorState>("DoorState", DoorStateCodec());
        K::SystemOptions pose;
        pose.name = "PoseDoors";
        K::Mount first;
        first.schedule = std::string(O::DefaultSchedules::fixed);
        first.stage = std::string(O::DefaultSchedules::first);
        first.sets = {GameplaySetFixed(kDoorsSet)};
        pose.mounts.push_back(std::move(first));
        r.AddSystem<&PoseDoors>(std::move(pose));
        K::SystemOptions operate;
        operate.name = "OperateDoors";
        K::Mount post;
        post.schedule = std::string(O::DefaultSchedules::fixed);
        post.stage = std::string(O::DefaultSchedules::postUpdate);
        post.after = {kJudgeInteractions};
        post.sets = {GameplaySetFixed(kDoorsSet)};
        if (networked) post.conditions.push_back(N::OnServer());
        operate.mounts.push_back(std::move(post));
        r.AddSystem<&OperateDoors>(std::move(operate));
    };
    set.readScene = &ReadDoor;
    return set;
}

} // namespace object_fps_pvp::world
