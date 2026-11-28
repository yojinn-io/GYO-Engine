// The interaction set (interact-door declaration section 2), from outside: a lone Match plays a mode that turns on the
// interaction set and the test's own kind ("tap": its System reads Interacted, as a kind of target does). A player
// looks along +Z at a test target (Interactable, a box collider) and presses E (kPressInteract). The core PvP mode
// (as shipped) leaves both off.
#include <doctest/doctest.h>

#include "WorldTestSupport.hpp"

#include "object_fps_pvp/world/GameplaySets.hpp"
#include "object_fps_pvp/world/Interaction.hpp"

#include "engine/authority/Consistency.hpp"
#include "engine/authority/Registrations.hpp"
#include "engine/physics_3d/Collider.hpp"
#include "engine/physics_3d/Shapes.hpp"
#include "engine/transform/Transform.hpp"

#include <vector>

using namespace object_fps_pvp::test;
namespace sim = object_fps_pvp::sim;

namespace {

namespace A = GYO::Engine::Authority;
namespace P = GYO::Engine::Physics3d;
namespace T = GYO::Engine::Transform;

// What the test's kind saw (the test's own bookkeeping, put whatever the mode).
struct Taps final {
    std::vector<PW::Interacted> seen;
};
void HashValue(K::Hasher&, const Taps&) {}

K::SystemResult CountTaps(K::MessageReader<PW::Interacted> interacted, K::ResMut<Taps> taps) {
    for (const PW::Interacted* one : interacted.Read()) taps->seen.push_back(*one);
    return {};
}

PW::GameplaySet TapSet() {
    PW::GameplaySet set;
    set.name = "tap";
    set.needs = {PW::kInteractionSet};
    set.build = [](O::WorldSetup& setup) {
        setup.Registrar().Resource<Taps>({.name = "Taps"});
        K::SystemOptions count;
        count.name = "CountTaps";
        K::Mount mount;
        mount.schedule = std::string(O::DefaultSchedules::fixed);
        mount.stage = std::string(O::DefaultSchedules::postUpdate);
        mount.after = {PW::kJudgeInteractions};
        mount.sets = {PW::GameplaySetFixed("tap")};
        count.mounts.push_back(std::move(mount));
        setup.Registrar().AddSystem<&CountTaps>(std::move(count));
        setup.Prepare([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            world.SetResource(Taps{});
            return {};
        });
    };
    return set;
}

std::string Mode(const std::string& id, const std::string& sets) { return ModeText(id, R"({"movement": "rules/movement_rules.json"})", sets); }

// The shipped content with the interaction set and the tap kind; modes core_pvp (no set) and tap_on (reach 2 m).
std::shared_ptr<const PW::GameContent> WithTap() {
    static const std::shared_ptr<const PW::GameContent> content = [] {
        auto copy = std::make_shared<PW::GameContent>(*ShippedContent());
        copy->sets = {PW::InteractionSet(), TapSet()};
        ModeFolder folder;
        folder.Modes({{"core.json", Mode("core_pvp", "{}")}, {"tap.json", Mode("tap_on", R"({"interaction": {"reach": 2.0}, "tap": {}})")}});
        std::string error;
        auto modes = PW::LoadModes(folder.root.string(), copy->arenas, copy->combat, copy->sets, error);
        REQUIRE_MESSAGE(modes.has_value(), error);
        copy->modes = std::move(*modes);
        return copy;
    }();
    return content;
}

constexpr K::EntityId kTarget = 6'000'001;
constexpr K::EntityId kScreen = 6'000'002;

// A lone Match in a mode of WithTap(): players are put in by the test, their input written directly.
struct Lone {
    std::pair<N::MemoryLink, N::MemoryLink> links = N::MemoryLink::Pair({});
    std::shared_ptr<N::NetTime> clock = std::make_shared<N::NetTime>(0);
    std::unique_ptr<O::Product> product;
    K::EntityId next = 5'000'001;

    explicit Lone(const std::size_t mode, std::shared_ptr<const PW::GameContent> content = WithTap()) {
        product = BuildMatch(std::move(content), std::make_shared<N::MemoryLink>(links.first), std::make_shared<PW::Seats>(),
                             [c = clock] { return *c; }, 0, {}, 60, mode);
        Run(1);
    }
    K::World& World() { return GameWorld(*product); }
    K::EntityId AddPlayer(const Math::Vec3 feet) {
        const K::EntityId id = next++;
        PW::PlayerBody body;
        body.feet = feet;
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
    // A box at the eye's height of a player standing at feet, `ahead` metres along +Z.
    void Box(const K::EntityId id, const Math::Vec3 feet, const float ahead, const bool interactable) {
        const PW::GameRules& rules = *World().Resource<PW::GameRules>();
        const float eye = sim::EyeHeight(rules.body, false);
        World().Insert(id, P::Collider{P::Shape{P::Cuboid{{0.3F, 0.3F, 0.05F}}}});
        World().Insert(id, T::Transform{Math::Vec3{feet.x, feet.y + eye, feet.z + ahead}});
        if (interactable) World().Insert(id, PW::Interactable{});
    }
    void Press(const K::EntityId id, const std::uint8_t key) {
        PW::PlayerInput input;
        input.present = 1;
        input.Press(key, true);
        *World().GetMut<PW::PlayerInput>(id) = input;
    }
    void Release(const K::EntityId id) { *World().GetMut<PW::PlayerInput>(id) = PW::PlayerInput{.present = 1}; }
    void Run(const int frames) {
        for (int i = 0; i < frames; ++i) {
            REQUIRE(static_cast<bool>(product->RunFrame()));
            *clock += Ms(16);
        }
    }
    // Presses key for one step's frames, then releases it and runs on a little.
    void Tap(const K::EntityId id, const std::uint8_t key = PW::kPressInteract) {
        Press(id, key);
        Run(4);
        Release(id);
        Run(8);
    }
    const std::vector<PW::Interacted>& Seen() { return World().Resource<Taps>()->seen; }
};

constexpr Math::Vec3 kFeet{5.0F, 0.0F, 5.0F};

} // namespace

TEST_CASE("world: interaction: E at a target within reach is one Interacted for that player and target") {
    Lone lone(1);
    const K::EntityId player = lone.AddPlayer(kFeet);
    lone.Box(kTarget, kFeet, 1.0F, true);
    lone.Run(2);
    REQUIRE(lone.Seen().empty());
    lone.Tap(player);
    REQUIRE_MESSAGE(!lone.Seen().empty(), "[pvp-interact-hit] looking at a target within reach, E interacts");
    CHECK_MESSAGE(lone.Seen().front().actor == player, "[pvp-interact-hit] the actor is the player");
    CHECK_MESSAGE(lone.Seen().front().target == kTarget, "[pvp-interact-hit] the target is the one looked at");
    CHECK(lone.Seen().front().tick > 0);
    REQUIRE(lone.World().Resource<PW::InteractionRules>() != nullptr);
    CHECK_MESSAGE(lone.World().Resource<PW::InteractionRules>()->reach == doctest::Approx(2.0F), "the mode's reach is put");
}

TEST_CASE("world: interaction: a target beyond reach, behind a collider that is not Interactable, or not pressed, is not used") {
    SUBCASE("beyond reach") {
        Lone lone(1);
        const K::EntityId player = lone.AddPlayer(kFeet);
        lone.Box(kTarget, kFeet, 2.6F, true);
        lone.Tap(player);
        CHECK_MESSAGE(lone.Seen().empty(), "[pvp-interact-reach] a target beyond the mode's reach is not used");
    }
    SUBCASE("behind another collider") {
        Lone lone(1);
        const K::EntityId player = lone.AddPlayer(kFeet);
        lone.Box(kScreen, kFeet, 0.6F, false);
        lone.Box(kTarget, kFeet, 1.2F, true);
        lone.Tap(player);
        CHECK_MESSAGE(lone.Seen().empty(), "[pvp-interact-wall] the first collider the view meets is not Interactable: nothing");
    }
    SUBCASE("another key") {
        Lone lone(1);
        const K::EntityId player = lone.AddPlayer(kFeet);
        lone.Box(kTarget, kFeet, 1.0F, true);
        lone.Tap(player, PW::kPressReload);
        lone.Tap(player, PW::kPressFire);
        lone.Tap(player, PW::kPressJump);
        CHECK_MESSAGE(lone.Seen().empty(), "[pvp-interact-key] reload, fire and jump do not interact");
    }
}

TEST_CASE("world: interaction: a dead player does not interact") {
    Lone lone(1);
    const K::EntityId player = lone.AddPlayer(kFeet);
    lone.Box(kTarget, kFeet, 1.0F, true);
    lone.World().GetMut<PW::PlayerCombat>(player)->life = sim::LifeState::Dead;
    lone.World().GetMut<PW::PlayerCombat>(player)->respawnTick = ~std::uint64_t{0};
    lone.Tap(player);
    CHECK_MESSAGE(lone.Seen().empty(), "[pvp-interact-dead] a dead player does not interact");
}

TEST_CASE("world: interaction: before the start E does nothing") {
    // A Match that waits for user 1, who never comes: the match is not started.
    auto content = WithTap();
    std::pair<N::MemoryLink, N::MemoryLink> links = N::MemoryLink::Pair({});
    auto clock = std::make_shared<N::NetTime>(0);
    auto product = BuildMatch(content, std::make_shared<N::MemoryLink>(links.first), std::make_shared<PW::Seats>(),
                              [c = clock] { return *c; }, 0, {1}, 60, 1);
    K::World& world = GameWorld(*product);
    REQUIRE(world.Resource<PW::MatchInfo>()->phase != PW::MatchPhase::Playing);
    const K::EntityId player = 5'000'001;
    PW::PlayerBody body;
    body.feet = kFeet;
    world.Insert(player, body);
    PW::PlayerCombat combat;
    combat.hp = sim::CombatRules{}.maximumHp;
    world.Insert(player, combat);
    PW::PlayerInput input;
    input.present = 1;
    input.Press(PW::kPressInteract, true);
    world.Insert(player, input);
    const PW::GameRules& rules = *world.Resource<PW::GameRules>();
    world.Insert(kTarget, P::Collider{P::Shape{P::Cuboid{{0.3F, 0.3F, 0.05F}}}});
    world.Insert(kTarget, T::Transform{Math::Vec3{kFeet.x, sim::EyeHeight(rules.body, false), kFeet.z + 1.0F}});
    world.Insert(kTarget, PW::Interactable{});
    for (int i = 0; i < 12; ++i) {
        REQUIRE(static_cast<bool>(product->RunFrame()));
        *clock += Ms(16);
    }
    CHECK_MESSAGE(world.Resource<Taps>()->seen.empty(), "[pvp-interact-before-start] before the start E does nothing");
}

TEST_CASE("world: interaction: the core PvP mode leaves it off: E does nothing, and the step is the same as without E") {
    // The same steps with E pressed and without, in core_pvp: every decision datum but the input is the same.
    Lone pressed(0);
    Lone plain(0);
    const K::EntityId a = pressed.AddPlayer(kFeet);
    const K::EntityId b = plain.AddPlayer(kFeet);
    REQUIRE(a == b);
    pressed.Box(kTarget, kFeet, 1.0F, true);
    plain.Box(kTarget, kFeet, 1.0F, true);
    const auto rest = [](K::World& world) {
        const auto digest = A::Digest(world);
        REQUIRE(static_cast<bool>(digest));
        std::uint64_t input = 0;
        for (const A::DataRecord& record : world.Resource<A::AuthorityRegistry>()->data) {
            if (record.key == K::KeyOf<PW::PlayerInput>()) input += record.ops->digest(world, record.seq);
        }
        return *digest - input;
    };
    pressed.Press(a, PW::kPressInteract);
    plain.Release(b);
    for (int i = 0; i < 30; ++i) {
        pressed.Run(1);
        plain.Run(1);
        CAPTURE(i);
        REQUIRE_MESSAGE(rest(pressed.World()) == rest(plain.World()), "[pvp-interact-off] E changes no decision datum but the input");
    }
    CHECK_MESSAGE(pressed.Seen().empty(), "[pvp-interact-off] no Interacted in core PvP");
    CHECK_MESSAGE(pressed.World().Resource<PW::InteractionRules>() == nullptr, "[pvp-interact-off] the set's rules are not put");
}

TEST_CASE("world: interaction: a mode that turns on a kind without the interaction set does not load") {
    ModeFolder folder;
    folder.Modes({{"a.json", Mode("a", R"({"tap": {}})")}});
    const auto& shipped = *ShippedContent();
    const std::vector<PW::GameplaySet> sets{PW::InteractionSet(), TapSet()};
    std::string error;
    CHECK_FALSE(PW::LoadModes(folder.root.string(), shipped.arenas, shipped.combat, sets, error).has_value());
    CHECK_MESSAGE(error.find("needs the set 'interaction'") != std::string::npos, "[pvp-door-needs-interaction] " << error);
    // Its reach: positive metres, and nothing else.
    for (const std::string& bad : {std::string(R"({"interaction": {"reach": 0}})"), std::string(R"({"interaction": {"reach": "far"}})"),
                                   std::string(R"({"interaction": {"range": 1}})")}) {
        CAPTURE(bad);
        ModeFolder other;
        other.Modes({{"a.json", Mode("a", bad)}});
        CHECK_FALSE(PW::LoadModes(other.root.string(), shipped.arenas, shipped.combat, sets, error).has_value());
    }
}
