#pragma once

// The game in a GYO World, the same registration at the Match and at every Client (the consistency fingerprints and
// the net protocol hashes must agree):
//
//   FixedUpdate/First   ApplyInput:PlayerInput   (net) each Peer's input of the step into its player's PlayerInput
//   FixedUpdate/Update  MovePlayers              both ends: stance, jump, fall, MoveAndSlide against the walls
//                       UseWeapons               both ends: reload, fire (ammo, cooldown, the shot's tick)
//   FixedUpdate/PostUpdate JudgeShots            Match only: the shot's ray, the hit, the damage, death
//                       RespawnPlayers           Match only: the farthest free spawn after the respawn time
//   Main/Update         SeatPlayers              Match only: a player for each Peer that connects, gone when it goes
//                       ControlOwnPlayer         Client only: PeerControls and PlayerInput on the player it owns
//   Main/PreUpdate      FollowMatchArena         Client only: the Match's arena (MatchInfo)
//
// The Client predicts its own player (K3: the rollback strategy) and takes everything else from the Match. A player
// without input in a step (a player the Client does not control) neither moves nor turns.

#include "object_fps_pvp/data/VoxelMeshFile.hpp"
#include "object_fps_pvp/world/Components.hpp"

#include "engine/entity_ids/EntityIdsModule.hpp"
#include "engine/math/geometry/Aabb.hpp"
#include "engine/net/NetService.hpp"
#include "engine/orchestration/Orchestration.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace object_fps_pvp::world {

namespace O = GYO::Engine::Orchestration;
namespace N = GYO::Engine::Net;

// An installed arena: its file, its baked geometry and its walls in world coordinates.
struct InstalledArena final {
    sim::ArenaContent content;
    data::VoxelMeshFile geometry;
    std::vector<Math::Aabb> walls;
    std::uint64_t wallsDigest = 0;
};

// What the game reads before a World is built: every installed arena (maps/arenas.json, in its order), the stance
// rules (rules/movement_rules.json) and the combat rules (the game's defaults).
struct GameContent final {
    std::vector<InstalledArena> arenas;
    sim::StanceRules stance;
    sim::CombatRules combat;

    [[nodiscard]] const InstalledArena* Find(const std::string& id) const;
};

// The content a World was built with (a Client switches arenas with it).
struct ContentRef final {
    std::shared_ptr<const GameContent> content;
};
void HashValue(K::Hasher&, const ContentRef&);

// Why this Client's content cannot play the Match's (empty: it can). Set by FollowMatchArena; the program decides.
struct ContentMismatch final {
    std::string reason;
};
void HashValue(K::Hasher&, const ContentMismatch&);

// Reads the content under root (the game's asset directory). nullopt with error naming the file and what was wrong:
// a file that does not read or parse, an arena whose walls do not build, whose stance rules do not fit, or whose
// spawn is not free of its walls.
[[nodiscard]] std::optional<GameContent> LoadGameContent(const std::string& root, std::string& error);

// The seats of a Match: the admission refuses a client once capacity are taken; SeatPlayers gives a seat back when
// its Peer goes. Shared by the Product's admission and the World (MatchSeats).
struct Seats final {
    std::uint32_t capacity = 4;
    std::atomic<std::uint32_t> taken{0};
};
struct MatchSeats final {
    std::shared_ptr<Seats> seats;
};
void HashValue(K::Hasher&, const MatchSeats&);

// The Match's admission: Refused ("the match is full") when every seat is taken.
[[nodiscard]] N::ServerHost::Admission SeatAdmission(std::shared_ptr<Seats> seats);

// The allotter ranges: the Match's entities and every Client's own (render, sound) ones; kArenaEntity is outside both.
inline constexpr GYO::Engine::EntityIds::EntityIdsOptions kMatchIds{1, 1'000'000};
inline constexpr GYO::Engine::EntityIds::EntityIdsOptions kClientIds{1'000'001, 2'000'000};

struct WorldOptions final {
    std::shared_ptr<const GameContent> content;
    // The Match plays this arena; a Client starts with it and follows the Match's (MatchInfo).
    std::size_t arena = 0;
    // A Client: its user id (NetProductOptions::userId). The Match: 0.
    std::uint64_t localUser = 0;
    // The Match: its seats (Seats); a Client: none.
    std::shared_ptr<Seats> seats;
};

// Registers the game in the World (the table above). The World's description carries NetWorldOptions,
// AuthorityOptions{ServerAuthorityStrategy}, dynamicInsert = false, the id range and, at a Client,
// RollbackOptions{StateRollbackStrategy} (MatchWorld and ClientWorld do that).
void BuildWorld(O::WorldSetup& setup, const WorldOptions& options);

// The World descriptions of the two ends, named "Game".
[[nodiscard]] O::WorldDescription MatchWorld(WorldOptions options);
[[nodiscard]] O::WorldDescription ClientWorld(WorldOptions options);

// The player a Client controls (its own Owner), or 0 before the Match's player has reached it.
[[nodiscard]] K::EntityId OwnPlayer(K::World& world);

} // namespace object_fps_pvp::world
