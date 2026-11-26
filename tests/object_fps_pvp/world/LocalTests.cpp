// The game's Local end (world/PvpWorld.hpp, WorldEnd::Local; game-flow declaration 3.3): a single player's World, no
// net and no rollback, its own authority.
#include <doctest/doctest.h>

#include "WorldTestSupport.hpp"

using namespace object_fps_pvp::test;

namespace {

std::unique_ptr<O::Product> BuildLocal(const std::size_t spawn, const std::uint64_t user = 9) {
    O::ProductDescription description;
    description.name = "pvp-solo";
    ReadShippedMaps(description);
    description.worlds.push_back(Stepped(PW::LocalWorld({.content = ShippedContent(), .arena = 0, .localUser = user, .spawn = spawn})));
    auto built = O::Product::Build(std::move(description), O::MakeRegisteredUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    auto product = std::move(built).value();
    Generate(*product);
    return product;
}

} // namespace

TEST_CASE("world: a Local World seats its own player at the spawn it was given, without net, and moves it by its input") {
    const object_fps_pvp::sim::ArenaContent& arena = ShippedContent()->arenas[0];
    REQUIRE(arena.spawns.size() >= 2);
    auto product = BuildLocal(1);
    K::World& world = GameWorld(*product);
    CHECK_FALSE_MESSAGE(world.Types().ResourceIdOf<N::NetWorldLink>().has_value(), "[pvp-local-no-net] no net in the World");
    REQUIRE(static_cast<bool>(product->RunFrame()));
    const K::EntityId player = PlayerOf(world, 9);
    REQUIRE_MESSAGE(player != 0, "[pvp-local-seated] the local user's player is there");
    CHECK_MESSAGE(PW::OwnPlayer(world) == player, "it is the World's own player (it has a PlayerInput)");
    const GYO::Engine::Math::Vec3 start = world.Get<PW::PlayerBody>(player)->feet;
    CHECK_MESSAGE(GYO::Engine::Math::Length(start - arena.spawns[1].position) < 1e-3F, "[pvp-local-spawn] at spawn 1");
    CHECK(world.Resource<PW::MatchInfo>()->arenaId == arena.id);
    // One player only, frame after frame.
    REQUIRE(static_cast<bool>(product->RunFrame()));
    std::size_t players = 0;
    for (const K::EntityId id : world.Entities()) players += world.Get<PW::Owner>(id) != nullptr ? 1 : 0;
    CHECK(players == 1);
    // The input the World's product writes moves it.
    for (int i = 0; i < 32; ++i) {
        PW::PlayerInput input;
        input.present = 1;
        input.ready = 1;  // the Client is ready: the Local World starts (StartMatch) and the input counts
        input.forward = 1.0F;
        input.yaw = world.Get<PW::PlayerBody>(player)->yaw;
        world.Insert(player, input);
        REQUIRE(static_cast<bool>(product->RunFrame()));
    }
    CHECK_MESSAGE(GYO::Engine::Math::Length(world.Get<PW::PlayerBody>(player)->feet - start) > 0.5F, "[pvp-local-moves]");
}

TEST_CASE("world: a Local World's spawn index wraps around the arena's spawns") {
    const object_fps_pvp::sim::ArenaContent& arena = ShippedContent()->arenas[0];
    auto product = BuildLocal(arena.spawns.size());
    REQUIRE(static_cast<bool>(product->RunFrame()));
    K::World& world = GameWorld(*product);
    const K::EntityId player = PlayerOf(world, 9);
    REQUIRE(player != 0);
    CHECK(GYO::Engine::Math::Length(world.Get<PW::PlayerBody>(player)->feet - arena.spawns[0].position) < 1e-3F);
}

TEST_CASE("world: a Local World is its own authority: its dead player respawns, and the death is counted once") {
    auto product = BuildLocal(0);
    K::World& world = GameWorld(*product);
    REQUIRE(static_cast<bool>(product->RunFrame()));
    const K::EntityId player = PlayerOf(world, 9);
    REQUIRE(player != 0);
    // Killed (as JudgeShots leaves a player: to respawn some steps later).
    PW::PlayerCombat dead = *world.Get<PW::PlayerCombat>(player);
    dead.hp = 0;
    dead.life = object_fps_pvp::sim::LifeState::Dead;
    dead.respawnTick = TickOf(world) + 10;
    world.Insert(player, dead);
    for (int i = 0; i < 16; ++i) REQUIRE(static_cast<bool>(product->RunFrame()));
    const PW::PlayerCombat& now = *world.Get<PW::PlayerCombat>(player);
    CHECK_MESSAGE(now.life == object_fps_pvp::sim::LifeState::Alive, "[pvp-local-respawns] RespawnPlayers runs in a Local World");
    CHECK(now.lives == 2);
    CHECK_MESSAGE(world.Resource<PW::LocalDeaths>()->count == 1, "[pvp-local-deaths] the death is counted once");
}
