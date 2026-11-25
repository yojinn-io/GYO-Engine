// The game in a World (world/PvpWorld.hpp), from outside: a lone Match whose players the test puts in and steers
// (movement against the shipped arena's walls, the jump, the stance, the weapon, the judgement, death and respawn),
// and a Match with Clients over memory links (connect, seat, control, prediction, the Match's judgement reaching
// both Clients, the full room). Port declaration, section 3.
#include <doctest/doctest.h>

#include <map>

#include "WorldTestSupport.hpp"

#include "engine/math/scalar/Constants.hpp"
#include "engine/physics_3d/Collider.hpp"
#include "engine/net/NetService.hpp"
#include "engine/physics_3d/Shapes.hpp"
#include "object_fps_pvp/character/CharacterRig.hpp"

#include <cmath>
#include <numbers>

using namespace object_fps_pvp::test;
namespace sim = object_fps_pvp::sim;

namespace {

// A lone Match: players are put in by the test outside every allotter range, their input written directly.
struct LoneMatch {
    std::pair<N::MemoryLink, N::MemoryLink> links = N::MemoryLink::Pair({});
    std::shared_ptr<N::NetTime> clock = std::make_shared<N::NetTime>(0);
    std::unique_ptr<O::Product> product;
    K::EntityId next = 5'000'001;

    explicit LoneMatch(std::shared_ptr<const PW::GameContent> content = ShippedContent()) {
        product = BuildMatch(std::move(content), std::make_shared<N::MemoryLink>(links.first), std::make_shared<PW::Seats>(),
                             [c = clock] { return *c; });
        Run(1);
    }
    K::World& World() { return GameWorld(*product); }
    // A seated player as SeatPlayers leaves it (its PlayerMotion carries the pose the shots meet).
    K::EntityId AddPlayer(const Math::Vec3 feet, const float yaw = 0.0F) {
        const K::EntityId id = next++;
        PW::PlayerBody body;
        body.feet = feet;
        body.yaw = yaw;
        const sim::CombatRules rules;
        PW::PlayerCombat combat;
        combat.hp = rules.maximumHp;
        combat.ammo = rules.magazineCapacity;
        World().Insert(id, body);
        World().Insert(id, combat);
        World().Insert(id, PW::ShotRecord{});
        World().Insert(id, PW::HitRecord{});
        World().Insert(id, PW::Owner{id});
        World().Insert(id, PW::PlayerInput{});
        World().Insert(id, PW::PlayerMotion{});
        return id;
    }
    void Steer(const K::EntityId id, const PW::PlayerInput& input) { *World().GetMut<PW::PlayerInput>(id) = input; }
    void Run(const int frames) {
        for (int i = 0; i < frames; ++i) {
            REQUIRE(static_cast<bool>(product->RunFrame()));
            *clock += Ms(16);
        }
    }
    const PW::PlayerBody& Body(const K::EntityId id) { return *World().Get<PW::PlayerBody>(id); }
    const PW::PlayerCombat& Combat(const K::EntityId id) { return *World().Get<PW::PlayerCombat>(id); }
};

PW::PlayerInput Input(const float forward, const float right, const float yaw, const float pitch = 0.0F) {
    PW::PlayerInput input;
    input.present = 1;
    input.forward = forward;
    input.right = right;
    input.yaw = yaw;
    input.pitch = pitch;
    return input;
}

constexpr float kHalfPi = std::numbers::pi_v<float> / 2;

} // namespace

TEST_CASE("world: a player walks at the arena's speed and stops at the wall, at its radius and skin") {
    LoneMatch match;
    const K::EntityId walker = match.AddPlayer({5, 0, 5});
    match.Steer(walker, Input(1, 0, 0));
    match.Run(64);
    // One second at 3 units a second along +Z.
    CHECK(match.Body(walker).feet.z == doctest::Approx(8.0).epsilon(1e-3));
    CHECK(match.Body(walker).feet.x == doctest::Approx(5.0));
    CHECK(match.Body(walker).feet.y == 0.0F);
    CHECK(match.Body(walker).grounded == 1);
    match.Run(64 * 6);
    // The +Z wall starts at z = 20: the capsule stops at its radius (0.25) and the skin (0.01).
    CHECK(match.Body(walker).feet.z == doctest::Approx(20.0 - 0.25 - 0.01).epsilon(1e-4));
    // Sideways into the pillar (x in [9, 11], z in [9, 11]).
    const K::EntityId side = match.AddPlayer({5, 0, 10});
    match.Steer(side, Input(0, 1, 0));
    match.Run(64 * 3);
    CHECK(match.Body(side).feet.x == doctest::Approx(9.0 - 0.25 - 0.01).epsilon(1e-4));
    CHECK(match.Body(side).feet.z == doctest::Approx(10.0));
}

TEST_CASE("world: a player without input neither moves nor turns") {
    LoneMatch match;
    const K::EntityId idle = match.AddPlayer({5, 0, 5}, 1.0F);
    PW::PlayerInput absent = Input(1, 1, 2.0F);
    absent.present = 0;
    match.Steer(idle, absent);
    match.Run(32);
    CHECK(match.Body(idle).feet.x == 5.0F);
    CHECK(match.Body(idle).feet.z == 5.0F);
    CHECK(match.Body(idle).yaw == 1.0F);
}

TEST_CASE("world: a jump rises to the arena's height and lands") {
    LoneMatch match;
    const K::EntityId jumper = match.AddPlayer({5, 0, 5});
    PW::PlayerInput jump = Input(0, 0, 0);
    jump.jump = 1;
    match.Steer(jumper, jump);
    match.Run(1);
    match.Steer(jumper, Input(0, 0, 0));
    float peak = 0;
    bool left = false;
    for (int i = 0; i < 64; ++i) {
        match.Run(1);
        peak = std::max(peak, match.Body(jumper).feet.y);
        left = left || match.Body(jumper).grounded == 0;
    }
    CHECK(left);
    CHECK(peak <= 0.6F + 1e-4F);
    CHECK(peak > 0.59F);
    CHECK(match.Body(jumper).feet.y == 0.0F);
    CHECK(match.Body(jumper).grounded == 1);
}

TEST_CASE("world: a crouched player under a low roof cannot stand up until it leaves it") {
    // An arena of the shipped one's rules with a roof from y 1.5 to 3 over x in [8, 12] (no pillar).
    auto content = std::make_shared<PW::GameContent>(*ShippedContent());
    PW::InstalledArena& arena = content->arenas.front();
    arena.walls = {{{8.0F, 1.5F, 0.0F}, {12.0F, 3.0F, 20.0F}}};
    auto boxes = GYO::Engine::Physics3d::BoxSet::Build(arena.walls);
    REQUIRE(static_cast<bool>(boxes));
    arena.wallsDigest = boxes->Digest();
    LoneMatch match(content);
    const K::EntityId player = match.AddPlayer({6, 0, 5});
    PW::PlayerInput crouch = Input(0, 1, 0);
    crouch.crouch = 1;
    match.Steer(player, crouch);
    match.Run(64);  // 1.5 units a second: from x 6 to 7.5
    match.Run(64);  // under the roof: x 9
    REQUIRE(match.Body(player).crouched == 1);
    REQUIRE(match.Body(player).feet.x > 8.5F);
    // Let go of crouch under the roof: still crouched, still at the crouched speed.
    match.Steer(player, Input(0, 1, 0));
    match.Run(16);
    CHECK(match.Body(player).crouched == 1);
    // Out from under the roof (x > 12 + radius): it stands.
    match.Run(64 * 3);
    CHECK(match.Body(player).feet.x > 12.3F);
    CHECK(match.Body(player).crouched == 0);
}

TEST_CASE("world: a shot hits the player in front, the cooldown holds the next, four body hits kill, the respawn is the farthest spawn") {
    LoneMatch match;
    const sim::CombatRules rules;
    // A at (5, 0, 3) looking +Z at B at (5, 0, 8); nothing between them (the pillar is at x 9 to 11).
    const K::EntityId a = match.AddPlayer({5, 0, 3}, 0.0F);
    const K::EntityId b = match.AddPlayer({5, 0, 8}, std::numbers::pi_v<float>);
    // From the eye (1.6) at B's chest (1.2): the bones' body shape.
    const float chest = -std::atan2(1.2F - 1.6F, 5.0F);
    PW::PlayerInput fire = Input(0, 0, 0, chest);
    fire.fire = 1;
    match.Steer(a, fire);
    match.Run(1);
    const std::uint64_t firstShot = TickOf(match.World());
    CHECK(match.Combat(a).ammo == rules.magazineCapacity - 1);
    CHECK(match.Combat(a).lastShotTick == firstShot);
    const PW::ShotRecord& shot = *match.World().Get<PW::ShotRecord>(a);
    CHECK(shot.kind == sim::ShotHitKind::Player);
    CHECK(shot.target == b);
    CHECK(shot.part == sim::HitPart::Body);
    CHECK(shot.damage == 25'000);
    // On B's front, short of its feet's line (5 ahead) by less than a body's depth.
    CHECK(shot.distance > 4.5F);
    CHECK(shot.distance < 5.0F);
    CHECK(match.Combat(b).hp == rules.maximumHp - 25'000);
    const PW::HitRecord& hits = *match.World().Get<PW::HitRecord>(b);
    CHECK(hits.hits == 1);
    CHECK(hits.attacker == a);
    CHECK(hits.tick == firstShot);
    // A press in the cooldown (11 steps at 64 Hz) fires nothing.
    match.Run(1);
    CHECK(match.Combat(a).ammo == rules.magazineCapacity - 1);
    match.Steer(a, Input(0, 0, 0, chest));
    match.Run(12);
    for (int shotIndex = 0; shotIndex < 3; ++shotIndex) {
        match.Steer(a, fire);
        match.Run(1);
        match.Steer(a, Input(0, 0, 0, chest));
        match.Run(11);
    }
    CHECK(match.Combat(a).ammo == rules.magazineCapacity - 4);
    CHECK(match.Combat(b).hp == 0);
    CHECK(match.Combat(b).life == sim::LifeState::Dead);
    const std::uint64_t respawnTick = match.Combat(b).respawnTick;
    // Three seconds after the fourth hit (192 steps).
    CHECK(respawnTick == match.World().Get<PW::HitRecord>(b)->tick + 192);
    // A dead player is not hit.
    match.Steer(a, fire);
    match.Run(1);
    CHECK(match.World().Get<PW::ShotRecord>(a)->kind != sim::ShotHitKind::Player);
    match.Steer(a, Input(0, 0, 0, chest));
    while (TickOf(match.World()) < respawnTick) match.Run(1);
    CHECK(match.Combat(b).life == sim::LifeState::Alive);
    CHECK(match.Combat(b).hp == rules.maximumHp);
    CHECK(match.Combat(b).lives == 2);
    CHECK(match.World().Get<PW::HitRecord>(b)->hits == 0);
    // The spawn farthest from A (at (5, 0, 3)): (5, 0, 12).
    CHECK(match.Body(b).feet.z == 12.0F);
    CHECK(match.Body(b).feet.x == 5.0F);
}

TEST_CASE("world: the pillar stops a shot") {
    LoneMatch match;
    // A at (7, 0, 10) looking +X at B at (13, 0, 10): the pillar (x 9 to 11) is between them.
    const K::EntityId a = match.AddPlayer({7, 0, 10}, kHalfPi);
    const K::EntityId b = match.AddPlayer({13, 0, 10});
    PW::PlayerInput fire = Input(0, 0, kHalfPi);
    fire.fire = 1;
    match.Steer(a, fire);
    match.Run(1);
    const PW::ShotRecord& shot = *match.World().Get<PW::ShotRecord>(a);
    CHECK(shot.kind == sim::ShotHitKind::World);
    CHECK(shot.distance == doctest::Approx(2.0).epsilon(1e-3));
    CHECK(match.Combat(b).hp == sim::CombatRules{}.maximumHp);
}

TEST_CASE("world: an empty magazine fires nothing; a reload fills it after the reload time") {
    LoneMatch match;
    const sim::CombatRules rules;
    const K::EntityId a = match.AddPlayer({5, 0, 3});
    PW::PlayerInput fire = Input(0, 0, kHalfPi);
    fire.fire = 1;
    for (std::uint32_t i = 0; i < rules.magazineCapacity; ++i) {
        match.Steer(a, fire);
        match.Run(1);
        match.Steer(a, Input(0, 0, kHalfPi));
        match.Run(11);
    }
    REQUIRE(match.Combat(a).ammo == 0);
    const std::uint32_t shots = match.Combat(a).shots;
    match.Steer(a, fire);
    match.Run(1);
    CHECK(match.Combat(a).shots == shots);
    PW::PlayerInput reload = Input(0, 0, kHalfPi);
    reload.reload = 1;
    match.Steer(a, reload);
    match.Run(1);
    const std::uint64_t started = TickOf(match.World());
    CHECK(match.Combat(a).reloadEndTick == started + 96);
    match.Steer(a, fire);
    match.Run(1);
    CHECK(match.Combat(a).shots == shots);  // no shot while reloading
    match.Steer(a, Input(0, 0, kHalfPi));
    while (TickOf(match.World()) < started + 96) match.Run(1);
    CHECK(match.Combat(a).ammo == rules.magazineCapacity);
    CHECK(match.Combat(a).reloadEndTick == 0);
}

// ---- The Match with Clients

namespace {

struct Session {
    std::shared_ptr<N::NetTime> clock = std::make_shared<N::NetTime>(0);
    std::vector<std::pair<N::MemoryLink, N::MemoryLink>> links;
    std::shared_ptr<PW::Seats> seats = std::make_shared<PW::Seats>();
    std::unique_ptr<O::Product> match;
    std::vector<std::unique_ptr<O::Product>> clients;

    // expectUsers, startDeadlineSeconds: the Match's start (none: played from the first step).
    Session(const int clientCount, const std::uint32_t capacity, const std::size_t matchArena = 0,
            std::shared_ptr<const PW::GameContent> clientContent = ShippedContent(), std::vector<std::uint64_t> expectUsers = {},
            const std::uint32_t startDeadlineSeconds = 60) {
        seats = std::make_shared<PW::Seats>(capacity);
        std::vector<N::MemoryLink> serverEnds;
        for (int i = 0; i < clientCount; ++i) {
            N::MemoryLinkSettings settings;
            settings.seed = static_cast<std::uint64_t>(60 + i);
            settings.aToB.fixedDelaySteps = 2;
            settings.bToA.fixedDelaySteps = 2;
            links.push_back(N::MemoryLink::Pair(settings));
            serverEnds.push_back(links.back().first);
        }
        auto reader = [c = clock] { return *c; };
        match = BuildMatch(ShippedContent(), std::make_shared<HubLink>(serverEnds), seats, reader, matchArena, std::move(expectUsers),
                           startDeadlineSeconds);
        for (int i = 0; i < clientCount; ++i) {
            clients.push_back(BuildClient(clientContent, std::make_shared<N::MemoryLink>(links[static_cast<std::size_t>(i)].second),
                                          static_cast<std::uint64_t>(i + 1), reader));
        }
    }
    K::World& Match() { return GameWorld(*match); }
    K::World& Client(const std::size_t i) { return GameWorld(*clients[i]); }
    // One frame everywhere; inputs[i] (when given) is client i's input for its next step.
    void Frame(const std::vector<std::optional<PW::PlayerInput>>& inputs = {}) {
        for (std::size_t i = 0; i < clients.size(); ++i) {
            if (i < inputs.size() && inputs[i]) {
                if (auto* local = N::LocalInput<PW::PlayerInput>(Client(i))) {
                    const auto target = static_cast<std::uint32_t>(TickOf(Client(i)) + 1);
                    local->Set(target, *inputs[i], N::ViewTime{target, 0});
                }
            }
        }
        REQUIRE(static_cast<bool>(match->RunFrame()));
        for (auto& client : clients) REQUIRE(static_cast<bool>(client->RunFrame()));
        for (auto& link : links) link.first.Advance();
        *clock += Ms(16);
    }
    void Frames(const int n, const std::vector<std::optional<PW::PlayerInput>>& inputs = {}) {
        for (int i = 0; i < n; ++i) Frame(inputs);
    }
};

std::size_t PeerCount(K::World& world) {
    std::size_t count = 0;
    for (const K::EntityId id : world.Entities()) count += world.Get<N::Peer>(id) != nullptr ? 1 : 0;
    return count;
}

} // namespace

TEST_CASE("world: two Clients connect, each gets a player, controls its own and sees the other") {
    Session session(2, 4);
    session.Frames(90);
    REQUIRE(PeerCount(session.Match()) == 2);
    const K::EntityId one = PlayerOf(session.Match(), 1);
    const K::EntityId two = PlayerOf(session.Match(), 2);
    REQUIRE(one != 0);
    REQUIRE(two != 0);
    // Seated at two different spawns.
    CHECK(session.Match().Get<PW::PlayerBody>(one)->feet.z != session.Match().Get<PW::PlayerBody>(two)->feet.z);
    for (std::size_t i = 0; i < 2; ++i) {
        CAPTURE(i);
        K::World& client = session.Client(i);
        CHECK(PlayerOf(client, 1) == one);
        CHECK(PlayerOf(client, 2) == two);
        CHECK(PW::OwnPlayer(client) == (i == 0 ? one : two));
        CHECK(client.Resource<PW::MatchInfo>()->arenaId == "pvp_training_v1");
        CHECK(client.Resource<PW::ContentMismatch>()->reason.empty());
    }
}

TEST_CASE("world: a Client walks; the Match moves its player and both Clients end at the Match's value") {
    Session session(2, 4);
    session.Frames(90);
    const K::EntityId one = PlayerOf(session.Match(), 1);
    REQUIRE(one != 0);
    REQUIRE(PW::OwnPlayer(session.Client(0)) == one);
    const Math::Vec3 start = session.Match().Get<PW::PlayerBody>(one)->feet;
    const float yaw = session.Match().Get<PW::PlayerBody>(one)->yaw;
    // Forward for 40 steps, then nothing (input present, no movement) until everything has arrived.
    session.Frames(20, {Input(1, 0, yaw)});
    // Mid-walk the Client's own player is ahead of the Match's (it predicts; it does not wait for the Match).
    const float predicted = Math::Length(session.Client(0).Get<PW::PlayerBody>(one)->feet - start);
    const float atMatchNow = Math::Length(session.Match().Get<PW::PlayerBody>(one)->feet - start);
    CHECK_MESSAGE(predicted > atMatchNow + 0.05F, "the Client predicts its own player: " << predicted << " vs the Match's " << atMatchNow);
    session.Frames(20, {Input(1, 0, yaw)});
    session.Frames(60, {Input(0, 0, yaw)});
    const PW::PlayerBody& atMatch = *session.Match().Get<PW::PlayerBody>(one);
    CHECK(Math::Length(atMatch.feet - start) > 1.5F);
    for (std::size_t i = 0; i < 2; ++i) {
        CAPTURE(i);
        const PW::PlayerBody* atClient = session.Client(i).Get<PW::PlayerBody>(one);
        REQUIRE(atClient != nullptr);
        CHECK(atClient->feet.x == atMatch.feet.x);
        CHECK(atClient->feet.z == atMatch.feet.z);
    }
}

TEST_CASE("world: a Client shoots the other; the Match judges and both Clients see the hit") {
    Session session(2, 4);
    session.Frames(90);
    const K::EntityId one = PlayerOf(session.Match(), 1);
    const K::EntityId two = PlayerOf(session.Match(), 2);
    REQUIRE(one != 0);
    REQUIRE(two != 0);
    const Math::Vec3 from = session.Match().Get<PW::PlayerBody>(one)->feet;
    const Math::Vec3 to = session.Match().Get<PW::PlayerBody>(two)->feet;
    // Aim at the other's chest height from the eye.
    const Math::Vec3 aim = to + Math::Vec3{0, 1.2F, 0} - (from + Math::Vec3{0, 1.6F, 0});
    const float yaw = std::atan2(aim.x, aim.z);
    const float pitch = -std::atan2(aim.y, std::sqrt(aim.x * aim.x + aim.z * aim.z));
    session.Frames(10, {Input(0, 0, yaw, pitch)});
    PW::PlayerInput fire = Input(0, 0, yaw, pitch);
    fire.fire = 1;
    session.Frame({fire});
    session.Frames(30, {Input(0, 0, yaw, pitch)});
    const sim::CombatRules rules;
    CHECK(session.Match().Get<PW::PlayerCombat>(two)->hp == rules.maximumHp - 25'000);
    CHECK(session.Match().Get<PW::PlayerCombat>(one)->ammo == rules.magazineCapacity - 1);
    for (std::size_t i = 0; i < 2; ++i) {
        CAPTURE(i);
        K::World& client = session.Client(i);
        CHECK(client.Get<PW::PlayerCombat>(two)->hp == rules.maximumHp - 25'000);
        CHECK(client.Get<PW::HitRecord>(two)->hits == 1);
        CHECK(client.Get<PW::HitRecord>(two)->attacker == one);
        CHECK(client.Get<PW::ShotRecord>(one)->kind == sim::ShotHitKind::Player);
        CHECK(client.Get<PW::PlayerCombat>(one)->ammo == rules.magazineCapacity - 1);
    }
}

TEST_CASE("world: a Client past the seats is refused and gets no player") {
    Session session(3, 2);
    session.Frames(120);
    CHECK(PeerCount(session.Match()) == 2);
    CHECK(PlayerOf(session.Match(), 3) == 0);
    CHECK(PeerCount(session.Client(2)) == 0);
    CHECK(session.seats->Taken() == 2);
}

TEST_CASE("world: a Client that leaves takes its player with it and gives its seat back") {
    Session session(2, 2);
    session.Frames(90);
    REQUIRE(PlayerOf(session.Match(), 2) != 0);
    REQUIRE(session.seats->Taken() == 2);
    auto* net = session.clients[1]->Buffer().Find<N::NetService>();
    REQUIRE(net != nullptr);
    net->Close();
    session.Frames(30);
    CHECK(PlayerOf(session.Match(), 2) == 0);
    CHECK(PlayerOf(session.Match(), 1) != 0);
    CHECK(session.seats->Taken() == 1);
    CHECK(PlayerOf(session.Client(0), 2) == 0);
}

TEST_CASE("world: a Client follows the Match's arena; one it does not have is a content mismatch") {
    {
        // The Match plays the second installed arena (pvp_corners); the Client starts with the first.
        Session session(1, 4, 1);
        session.Frames(90);
        K::World& client = session.Client(0);
        CHECK(client.Resource<PW::GameRules>()->arena.id == "pvp_corners_v1");
        CHECK(client.Resource<PW::GameRules>()->wallsDigest == ShippedContent()->arenas[1].wallsDigest);
        const auto* walls = client.Get<GYO::Engine::Physics3d::Collider>(PW::kArenaEntity);
        REQUIRE(walls != nullptr);
        const auto* boxes = std::get_if<GYO::Engine::Physics3d::BoxSet>(&walls->shape);
        REQUIRE(boxes != nullptr);
        CHECK(boxes->Digest() == ShippedContent()->arenas[1].wallsDigest);
        CHECK(client.Resource<PW::ContentMismatch>()->reason.empty());
    }
    {
        auto onlyFirst = std::make_shared<PW::GameContent>(*ShippedContent());
        onlyFirst->arenas.resize(1);
        Session session(1, 4, 1, onlyFirst);
        session.Frames(90);
        const std::string& reason = session.Client(0).Resource<PW::ContentMismatch>()->reason;
        CHECK_MESSAGE(reason.find("pvp_corners_v1") != std::string::npos, reason);
        CHECK(session.Client(0).Resource<PW::GameRules>()->arena.id == "pvp_training_v1");
    }
}

TEST_CASE("world: with the bones' hit shapes a head shot, a body shot and a leg shot each deal their part's damage") {
    LoneMatch match;
    REQUIRE(ShippedContent()->character != nullptr);
    const sim::CombatRules rules;
    // B stands at (5, 0, 8) facing A; A at (5, 0, 3) aims from its eye (1.6) at a height on B.
    const K::EntityId b = match.AddPlayer({5, 0, 8}, std::numbers::pi_v<float>);
    const auto shootAt = [&](const float height, const float side = 0.0F) {
        const K::EntityId a = match.AddPlayer({5, 0, 3}, 0.0F);
        const float pitch = -std::atan2(height - 1.6F, 5.0F);
        PW::PlayerInput aim = Input(0, 0, std::atan2(side, 5.0F), pitch);
        match.Steer(a, aim);
        match.Run(2);
        PW::PlayerInput fire = aim;
        fire.fire = 1;
        match.Steer(a, fire);
        match.Run(1);
        const PW::ShotRecord shot = *match.World().Get<PW::ShotRecord>(a);
        match.World().Despawn(a);
        return shot;
    };
    match.Run(4);
    const PW::ShotRecord head = shootAt(1.68F);
    CHECK(head.kind == sim::ShotHitKind::Player);
    CHECK(head.part == sim::HitPart::Head);
    CHECK(head.damage == 50'000);
    const PW::ShotRecord body = shootAt(1.2F);
    CHECK(body.part == sim::HitPart::Body);
    CHECK(body.damage == 25'000);
    const PW::ShotRecord leg = shootAt(0.7F, 0.18F);  // a thigh (the legs stand apart: the middle passes between them)
    CHECK(leg.part == sim::HitPart::Leg);
    CHECK(leg.damage == 18'750);
    CHECK(match.Combat(b).hp == rules.maximumHp - 50'000 - 25'000 - 18'750);
    // Over the head: no part of the body.
    CHECK(shootAt(2.3F).kind != sim::ShotHitKind::Player);
}

TEST_CASE("world: a shot a stride ahead of a walking player meets its front leg in some step of the gait") {
    LoneMatch match;
    // B walks across A's view (facing +X, from x 2); A keeps aiming 0.45 ahead of B's feet at 0.65 high and fires
    // every cooldown: the standing pose has nothing there, the front leg of the gait swings through it. The Match
    // judges on the pose of the step (AnimatePlayers).
    const K::EntityId a = match.AddPlayer({5, 0, 3}, 0.0F);
    const K::EntityId b = match.AddPlayer({2, 0, 8}, kHalfPi);
    std::map<sim::HitPart, int> parts;
    int misses = 0;
    for (int frame = 0; frame < 120; ++frame) {
        match.Steer(b, Input(1, 0, kHalfPi));
        const Math::Vec3 to = match.Body(b).feet + Math::Vec3{0.45F, 0.0F, 0.0F} - match.Body(a).feet;
        const float distance = std::sqrt(to.x * to.x + to.z * to.z);
        PW::PlayerInput aim = Input(0, 0, std::atan2(to.x, to.z), -std::atan2(0.65F - 1.6F, distance));
        aim.fire = frame % 12 == 11 ? 1 : 0;
        match.Steer(a, aim);
        match.Run(1);
        if (aim.fire == 0) continue;
        const PW::ShotRecord& shot = *match.World().Get<PW::ShotRecord>(a);
        if (shot.kind == sim::ShotHitKind::Player) {
            ++parts[shot.part];
        } else {
            ++misses;
        }
    }
    INFO("legs " << parts[sim::HitPart::Leg] << ", body " << parts[sim::HitPart::Body] << ", misses " << misses);
    CHECK(parts[sim::HitPart::Leg] > 0);
}

TEST_CASE("world: the character loads with its skeleton, sixteen clips, seven hit shapes and the arenas' body height") {
    const auto& rig = *ShippedContent()->character;
    CHECK(rig.body->nodes.size() == 77);
    for (const auto& clip : rig.clips) CHECK(clip.duration > 0.0F);
    CHECK(rig.hitShapes.size() == 7);
    // 1.8 units tall from a model about 0.55 tall.
    CHECK(rig.scale == doctest::Approx(1.8 / 0.554).epsilon(0.02));
    // The idle pose's head above the hips, both within the body.
    std::vector<GYO::Engine::Model::LocalTransform> local;
    object_fps_pvp::character::ComposeBody(rig, sim::Locomotion{}, sim::ActionPose{}, false, 0.0, local);
    std::vector<Math::Matrix4> modelSpace;
    object_fps_pvp::character::ModelSpace(rig, local, modelSpace);
    const auto shapes = object_fps_pvp::character::PlaceHitShapes(rig, modelSpace, {0, 0, 0}, 0.0F);
    REQUIRE(shapes.size() == 7);
    CHECK(shapes[0].part == sim::HitPart::Head);
    CHECK(shapes[0].a.y > 1.4F);
    CHECK(shapes[0].a.y < 1.9F);
    // Crouched (the clip lowers the hips a long way: the retarget's translation scale carries it to this body).
    object_fps_pvp::character::ComposeBody(rig, sim::Locomotion{}, sim::ActionPose{}, true, 0.0, local);
    object_fps_pvp::character::ModelSpace(rig, local, modelSpace);
    const auto crouched = object_fps_pvp::character::PlaceHitShapes(rig, modelSpace, {0, 0, 0}, 0.0F);
    CHECK(crouched[0].a.y < shapes[0].a.y - 0.2F);
    CHECK(crouched[0].a.y > 0.7F);
    CHECK(crouched[2].a.y > 0.2F);
    for (const auto& shape : shapes) {
        CHECK(shape.a.y >= -0.1F);
        CHECK(std::abs(shape.a.x) < 0.5F);
    }
}

TEST_CASE("world: the upper body follows the aim and the lower body the gait") {
    const auto& rig = *ShippedContent()->character;
    const auto headAt = [&](const sim::ActionPose& actions, const sim::Locomotion& locomotion) {
        std::vector<GYO::Engine::Model::LocalTransform> local;
        object_fps_pvp::character::ComposeBody(rig, locomotion, actions, false, 0.0, local);
        std::vector<Math::Matrix4> modelSpace;
        object_fps_pvp::character::ModelSpace(rig, local, modelSpace);
        return object_fps_pvp::character::PlaceHitShapes(rig, modelSpace, {0, 0, 0}, 0.0F);
    };
    sim::ActionPose up;
    up.aim = -1.0F;
    sim::ActionPose down;
    down.aim = 1.0F;
    const auto lookingUp = headAt(up, sim::Locomotion{});
    const auto lookingDown = headAt(down, sim::Locomotion{});
    // The head moves with the aim; the legs (the lower body) do not.
    CHECK_MESSAGE(Math::Length(lookingUp[0].a - lookingDown[0].a) > 0.05F, "[pvp-aim-upper] the head follows the aim");
    CHECK(Math::Length(lookingUp[4].b - lookingDown[4].b) < 1e-4F);
    // Shooting (held fire keeps the Shoot clip on): the action adds to the aim, the head still follows it.
    up.upper = sim::UpperClip::Shoot;
    up.upperProgress = 0.5;
    down.upper = sim::UpperClip::Shoot;
    down.upperProgress = 0.5;
    CHECK_MESSAGE(Math::Length(headAt(up, sim::Locomotion{})[0].a - headAt(down, sim::Locomotion{})[0].a) > 0.05F,
                  "[pvp-aim-shooting] the head follows the aim while shooting");
    // Half a gait cycle on: the legs have moved.
    sim::Locomotion walking;
    walking.moveWeight = 1.0;
    walking.cycles = 0.25;
    sim::Locomotion later = walking;
    later.cycles = 0.75;
    CHECK(Math::Length(headAt({}, walking)[4].b - headAt({}, later)[4].b) > 0.05F);
}


namespace {

PW::PlayerInput Ready(PW::PlayerInput input) {
    input.ready = 1;
    return input;
}

} // namespace

TEST_CASE("world: before the start each Client sees only its own player and nobody moves; ready together, the match starts") {
    Session session(2, 4, 0, ShippedContent(), {1, 2});
    // Never sent, not once: each frame of the wait.
    bool everSeen = false;
    for (int i = 0; i < 90; ++i) {
        session.Frame();
        everSeen = everSeen || PlayerOf(session.Client(0), 2) != 0 || PlayerOf(session.Client(1), 1) != 0;
    }
    CHECK_MESSAGE(!everSeen, "[pvp-start-hidden] the other player's place is not sent before the start, in no frame");
    const K::EntityId one = PlayerOf(session.Match(), 1);
    const K::EntityId two = PlayerOf(session.Match(), 2);
    REQUIRE(one != 0);
    REQUIRE(two != 0);
    CHECK(session.Match().Resource<PW::MatchInfo>()->phase == PW::MatchPhase::Waiting);
    for (std::size_t i = 0; i < 2; ++i) {
        CAPTURE(i);
        CHECK(PW::OwnPlayer(session.Client(i)) == (i == 0 ? one : two));
        CHECK_MESSAGE(PlayerOf(session.Client(i), i == 0 ? 2 : 1) == 0, "[pvp-start-hidden] the other player's place is not sent before the start");
    }
    // One ready Client walking: nothing moves yet (the other is not ready).
    const Math::Vec3 start = session.Match().Get<PW::PlayerBody>(one)->feet;
    const float yaw = session.Match().Get<PW::PlayerBody>(one)->yaw;
    PW::PlayerInput fire = Ready(Input(1, 0, yaw + 0.5F));
    fire.fire = 1;
    session.Frames(40, {fire});
    CHECK_MESSAGE(Math::Length(session.Match().Get<PW::PlayerBody>(one)->feet - start) < 1e-4F, "[pvp-start-frozen] nobody moves before the start");
    CHECK_MESSAGE(Math::Length(session.Client(0).Get<PW::PlayerBody>(one)->feet - start) < 1e-4F, "[pvp-start-frozen] ... not in the Client's prediction either");
    CHECK(session.Match().Get<PW::PlayerBody>(one)->yaw == yaw);
    CHECK_MESSAGE(session.Match().Get<PW::PlayerCombat>(one)->shots == 0, "[pvp-start-no-shot] nobody fires before the start");
    CHECK(session.Match().Resource<PW::MatchInfo>()->phase == PW::MatchPhase::Waiting);
    // Both ready: the match starts; everybody sees everybody, and the walk moves.
    session.Frames(30, {Ready(Input(0, 0, yaw)), Ready(Input(0, 0, 0))});
    CHECK_MESSAGE(session.Match().Resource<PW::MatchInfo>()->phase == PW::MatchPhase::Playing, "[pvp-start-ready] every expected player ready: started");
    for (std::size_t i = 0; i < 2; ++i) {
        CAPTURE(i);
        CHECK(session.Client(i).Resource<PW::MatchInfo>()->phase == PW::MatchPhase::Playing);
        CHECK_MESSAGE(PlayerOf(session.Client(i), i == 0 ? 2 : 1) != 0, "[pvp-start-shown] after the start the other player is sent");
    }
    session.Frames(40, {Ready(Input(1, 0, yaw)), Ready(Input(0, 0, 0))});
    CHECK(Math::Length(session.Match().Get<PW::PlayerBody>(one)->feet - start) > 0.5F);
}

TEST_CASE("world: an expected player who never comes does not hold the start past its deadline") {
    Session session(1, 4, 0, ShippedContent(), {1, 2}, 1);
    session.Frames(30, {Ready(Input(0, 0, 0))});
    CHECK(session.Match().Resource<PW::MatchInfo>()->phase == PW::MatchPhase::Waiting);
    // A second (64 steps) after the first player came.
    session.Frames(80, {Ready(Input(0, 0, 0))});
    CHECK_MESSAGE(session.Match().Resource<PW::MatchInfo>()->phase == PW::MatchPhase::Playing, "[pvp-start-deadline]");
}
