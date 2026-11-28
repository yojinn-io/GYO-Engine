// The doors set (world/Doors.hpp; interact-door declaration section 3), from outside, with the shipped content: the
// sandbox mode turns on the interaction and doors sets, and pvp_arena's scene has one door (id 1): its hinge at
// (14, 0, 4), facing +Z, its leaf 1 m along +X, 2.1 m high, 0.1 m thick; open, turned a quarter about the hinge, it runs
// from the hinge along -Z. A player stands in front of it at z = 3.5 looking along +Z (0.45 m from the shut leaf; it
// looks along -X at the open one).
#include <doctest/doctest.h>

#include "WorldTestSupport.hpp"

#include "object_fps_pvp/world/Doors.hpp"
#include "object_fps_pvp/world/Interaction.hpp"
#include "object_fps_pvp/world/Scene.hpp"

#include "engine/net/Replication.hpp"
#include "engine/physics_3d/Collider.hpp"
#include "engine/physics_3d/Shapes.hpp"
#include "engine/transform/Transform.hpp"

#include <numbers>

using namespace object_fps_pvp::test;
namespace sim = object_fps_pvp::sim;

namespace {

namespace P = GYO::Engine::Physics3d;
namespace T = GYO::Engine::Transform;

constexpr K::EntityId kDoor = PW::SceneEntity(1);
constexpr K::EntityId kScreen = 6'000'002;
constexpr Math::Vec3 kFront{14.5F, 0.0F, 3.5F};
constexpr float kAtOpenLeaf = -std::numbers::pi_v<float> / 2;

std::size_t Sandbox() {
    const auto mode = ShippedContent()->FindMode("sandbox");
    REQUIRE(mode.has_value());
    return *mode;
}

std::size_t CorePvp() {
    const auto mode = ShippedContent()->FindMode("core_pvp");
    REQUIRE(mode.has_value());
    return *mode;
}

struct DoorMatch : PlayedMatch {
    explicit DoorMatch(const std::size_t mode = Sandbox()) : PlayedMatch(ShippedContent(), mode) {}
    bool Open() {
        const PW::DoorState* state = World().Get<PW::DoorState>(kDoor);
        REQUIRE(state != nullptr);
        return state->open != 0;
    }
};

PW::PlayerInput Walk(const float forward) {
    PW::PlayerInput input;
    input.present = 1;
    input.forward = forward;
    return input;
}

} // namespace

TEST_CASE("world: doors: the shipped sandbox plays pvp_arena with its door, shut, standing as its shape says") {
    DoorMatch match;
    REQUIRE_MESSAGE(match.World().Get<PW::DoorShape>(kDoor) != nullptr, "[pvp-door-put] the scene's door is at its id's entity");
    CHECK_FALSE(match.Open());
    CHECK(match.World().Get<PW::Interactable>(kDoor) != nullptr);
    CHECK(match.World().Get<P::Collider>(kDoor) != nullptr);
    CHECK_MESSAGE(match.World().Get<GYO::Engine::Net::Placed>(kDoor) != nullptr, "[pvp-door-placed] with a net, placed at both ends");
    const PW::DoorShape& shape = *match.World().Get<PW::DoorShape>(kDoor);
    // Shut: the leaf's centre half its width along +X from the hinge; open: along -Z.
    const T::Transform shut = PW::DoorPose(shape, false);
    CHECK_MESSAGE(shut.translation.x == doctest::Approx(14.5F), "[pvp-door-pose] shut: along the facing's right");
    CHECK(shut.translation.y == doctest::Approx(1.05F));
    CHECK(shut.translation.z == doctest::Approx(4.0F));
    const T::Transform open = PW::DoorPose(shape, true);
    CHECK_MESSAGE(open.translation.x == doctest::Approx(14.0F).epsilon(1e-4), "[pvp-door-pose] open: turned about the hinge");
    CHECK(open.translation.z == doctest::Approx(3.5F).epsilon(1e-4));
    const T::Transform* now = match.World().Get<T::Transform>(kDoor);
    REQUIRE(now != nullptr);
    CHECK(*now == shut);
}

TEST_CASE("world: doors: E at a door within reach turns it, and it stands open from the next step; E again shuts it") {
    DoorMatch match;
    const K::EntityId player = match.AddPlayer(kFront);
    match.Run(2);
    match.Tap(player);
    CHECK_MESSAGE(match.Open(), "[pvp-door-toggle] E opens the door looked at");
    const PW::DoorShape& shape = *match.World().Get<PW::DoorShape>(kDoor);
    CHECK_MESSAGE(*match.World().Get<T::Transform>(kDoor) == PW::DoorPose(shape, true), "[pvp-door-pose-follows] it stands open");
    match.Tap(player);
    CHECK_MESSAGE(match.Open(), "the open leaf is not in front of it any more: E does nothing");
    match.Tap(player, PW::kPressInteract, kAtOpenLeaf);
    CHECK_MESSAGE(!match.Open(), "[pvp-door-toggle] E at the open leaf shuts it");
    CHECK(*match.World().Get<T::Transform>(kDoor) == PW::DoorPose(shape, false));
}

TEST_CASE("world: doors: a door beyond reach, or behind another collider, does not turn") {
    SUBCASE("beyond reach") {
        DoorMatch match;
        const K::EntityId player = match.AddPlayer({14.5F, 0.0F, 2.3F});
        match.Tap(player);
        CHECK_MESSAGE(!match.Open(), "[pvp-door-reach] 1.65 m away, beyond the reach of 1.5 m");
    }
    SUBCASE("behind another collider") {
        DoorMatch match;
        const K::EntityId player = match.AddPlayer(kFront);
        const float eye = sim::EyeHeight(match.World().Resource<PW::GameRules>()->body, false);
        match.World().Insert(kScreen, P::Collider{P::Shape{P::Cuboid{{0.3F, 0.3F, 0.05F}}}});
        match.World().Insert(kScreen, T::Transform{Math::Vec3{kFront.x, eye, 3.75F}});
        match.Tap(player);
        CHECK_MESSAGE(!match.Open(), "[pvp-door-wall-between] a collider in between stops the view");
    }
}

TEST_CASE("world: doors: shut, a door stops a body and a shot; open, the doorway is free") {
    SUBCASE("walking") {
        DoorMatch shut;
        DoorMatch opened;
        const K::EntityId a = shut.AddPlayer(kFront);
        const K::EntityId b = opened.AddPlayer(kFront);
        opened.Tap(b);
        REQUIRE(opened.Open());
        shut.Steer(a, Walk(1.0F));
        opened.Steer(b, Walk(1.0F));
        shut.Run(64);
        opened.Run(64);
        CHECK_MESSAGE(shut.Body(a).feet.z < 3.8F, "[pvp-door-blocks] the shut leaf stops the body: " << shut.Body(a).feet.z);
        CHECK_MESSAGE(opened.Body(b).feet.z > 5.0F, "[pvp-door-open-passes] the open doorway lets it through: " << opened.Body(b).feet.z);
    }
    SUBCASE("a shot") {
        const auto hpAfterShot = [](const bool open) {
            DoorMatch match;
            const K::EntityId shooter = match.AddPlayer(kFront);
            const K::EntityId target = match.AddPlayer({14.5F, 0.0F, 6.0F});
            if (open) {
                match.Tap(shooter);
                REQUIRE(match.Open());
            }
            match.Tap(shooter, PW::kPressFire);
            return match.World().Get<PW::PlayerCombat>(target)->hp;
        };
        const std::uint32_t full = sim::CombatRules{}.maximumHp;
        CHECK_MESSAGE(hpAfterShot(false) == full, "[pvp-door-blocks] the shut leaf stops the shot");
        CHECK_MESSAGE(hpAfterShot(true) < full, "[pvp-door-open-passes] through the open doorway the shot hits");
    }
}

TEST_CASE("world: doors: a door whose turn would meet a living body does not turn") {
    DoorMatch match;
    const K::EntityId player = match.AddPlayer(kFront);
    // Where the open leaf would be (x = 14, z 3 to 4), clear of the shut one and of the player's view.
    const K::EntityId inTheWay = match.AddPlayer({13.8F, 0.0F, 3.4F});
    match.Tap(player);
    CHECK_MESSAGE(!match.Open(), "[pvp-door-body-in-way] the open leaf would meet a body: it stays shut");
    // Dead, the body is not in the way.
    match.World().GetMut<PW::PlayerCombat>(inTheWay)->life = sim::LifeState::Dead;
    match.World().GetMut<PW::PlayerCombat>(inTheWay)->respawnTick = ~std::uint64_t{0};
    match.Tap(player);
    CHECK_MESSAGE(match.Open(), "[pvp-door-body-in-way] a dead body is not in the way");
}

TEST_CASE("world: doors: the core PvP mode leaves them off: pvp_arena's scene puts no door") {
    DoorMatch match(CorePvp());
    CHECK_MESSAGE(match.World().Get<PW::DoorState>(kDoor) == nullptr, "[pvp-interact-off] no door in core PvP");
    CHECK(match.World().Get<PW::DoorShape>(kDoor) == nullptr);
    CHECK(match.World().Get<P::Collider>(kDoor) == nullptr);
}

TEST_CASE("world: doors: a mode that turns doors on without interaction does not load; a door's data is checked") {
    ModeFolder folder;
    folder.Modes({{"doors.json", ModeText("doors_only", kShippedRules, R"({"doors": {}})")}});
    std::string error;
    const auto& content = *ShippedContent();
    CHECK_FALSE_MESSAGE(PW::LoadModes(folder.root.string(), content.arenas, content.combat, PW::GameSets(), error).has_value(),
                        "[pvp-door-needs-interaction]");
    CHECK_MESSAGE(error.find("the set 'doors' needs the set 'interaction'") != std::string::npos, error);
    const auto door = [](const std::string& fields) {
        return R"({"version": 1, "id": "s", "objects": [{"id": 1, "set": "doors", "data": {"hinge": [1, 0, 1], "yaw": 0, )" + fields +
               R"(, "open": false, "color": [0.5, 0.5, 0.5]}}]})";
    };
    CHECK(PW::ParseScene(door(R"("width": 1, "height": 2, "thickness": 0.1, "swing": 1.5)"), PW::GameSets(), error).has_value());
    const auto refused = [&](const std::string& fields, const std::string& what) {
        CHECK_FALSE(PW::ParseScene(door(fields), PW::GameSets(), error).has_value());
        CHECK_MESSAGE(error.find(what) != std::string::npos, "[pvp-door-read] expected '" << what << "', read: " << error);
    };
    refused(R"("width": 0, "height": 2, "thickness": 0.1, "swing": 1.5)", "width");
    refused(R"("width": 1, "height": 2, "thickness": 0.1, "swing": 0)", "swing is not 0 < |swing| <= pi");
    refused(R"("width": 1, "height": 2, "thickness": 0.1, "swing": 3.2)", "swing is not 0 < |swing| <= pi");
    refused(R"("width": 1, "height": 2, "thickness": 0.1, "swing": 1.5, "lock": 1)", "a door takes no 'lock'");
}

TEST_CASE("world: doors: a Match and two Clients: the door is attached before the start, its state reaches both and stands there") {
    // The Match waits for users 1 and 2 (not ready): before the start, each Client sees its player and the door.
    Session session(2, 4, 0, ShippedContent(), {1, 2}, 60, std::nullopt, nullptr, Sandbox());
    session.Frames(90);
    REQUIRE(session.Match().Resource<PW::MatchInfo>()->phase == PW::MatchPhase::Waiting);
    for (std::size_t i = 0; i < 2; ++i) {
        K::World& client = session.Client(i);
        CAPTURE(i);
        CHECK(client.Resource<PW::ContentMismatch>()->kind == PW::MismatchKind::None);
        REQUIRE_MESSAGE(client.Get<PW::DoorState>(kDoor) != nullptr, "[pvp-door-match-client] the Client has the door's state");
        CHECK_MESSAGE(client.Get<GYO::Engine::Net::NotReceiving>(kDoor) == nullptr, "[pvp-door-match-client] attached before the start");
        CHECK(client.Resource<GYO::Engine::Net::NetReplicaState>()->attaches >= 1);
    }
    // The Match turns it (as OperateDoors does): both Clients are sent the state and stand the door open.
    session.Match().GetMut<PW::DoorState>(kDoor)->open = 1;
    session.Frames(30);
    const PW::DoorShape& shape = *session.Match().Get<PW::DoorShape>(kDoor);
    for (std::size_t i = 0; i < 2; ++i) {
        K::World& client = session.Client(i);
        CAPTURE(i);
        CHECK_MESSAGE(client.Get<PW::DoorState>(kDoor)->open == 1, "[pvp-door-match-client] the state reaches the Client");
        CHECK_MESSAGE(*client.Get<T::Transform>(kDoor) == PW::DoorPose(shape, true), "[pvp-door-match-client] the Client's door stands open");
    }
}
