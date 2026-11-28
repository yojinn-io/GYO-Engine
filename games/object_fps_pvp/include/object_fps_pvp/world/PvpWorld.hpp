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
//   Main/Update         LoadArena                every end, while the World generates: its arena's map from the asset
//                                                store, then the walls on a worker (world/ArenaLoad.hpp; MapLoad)
//   Main/PreUpdate      FollowMatchContent       Client only: whether it plays the Match's mode and arena with the
//                                                same content (MatchInfo); if not, ContentMismatch (a World plays one
//                                                mode and one arena: another one is another World)
//   Main/Update         SpawnLocalPlayer         Local only: the local user's player at LocalSpawn
//                       ShowPlayers              Match only: before the start a connection sees only its player
//   FixedUpdate/Update  StartMatch               the authority: Waiting -> Playing (MatchInfo::phase) when every
//                                                expected player is ready, or at the deadline; before it MovePlayers
//                                                and UseWeapons take no input
//
// The fixed steps start when the World has generated (WorldTimeOptions::fixedStepConditions: AfterGenerating), so
// nothing moves before the walls are there; SeatPlayers keeps the connections that came while it generated.
//
// Three ends: the Match (the server), a Client (predicts its own player) and Local (single player: no net; the rollback
// Module is there without a strategy, as authority needs it; the World is the authority, "Match only" runs there too,
// and the World's own product writes the player's PlayerInput). A Local World also counts its player's deaths
// (LocalDeaths, Frame/Last CountLocalDeaths), by the same rule as the Match's saves (CountDeath).
//
// The Client predicts its own player (K3: the rollback strategy) and takes everything else from the Match. A player
// without input in a step (a player the Client does not control) neither moves nor turns.

#include "object_fps_pvp/character/CharacterRig.hpp"
#include "object_fps_pvp/world/ArenaLoad.hpp"
#include "object_fps_pvp/world/Components.hpp"
#include "object_fps_pvp/world/Modes.hpp"

#include "engine/asset/AssetExecutor.hpp"

#include "engine/entity_ids/EntityIdsModule.hpp"
#include "engine/math/geometry/Aabb.hpp"
#include "engine/physics_3d/Shapes.hpp"
#include "engine/net/NetService.hpp"
#include "engine/orchestration/Orchestration.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace object_fps_pvp::world {

namespace O = GYO::Engine::Orchestration;
namespace N = GYO::Engine::Net;

// What the game reads before a World is built: every installed arena's file (maps/arenas.json, in its order), every
// installed mode (modes/modes.json, in its order; world/Modes.hpp: its rule tables and gameplay sets), the gameplay
// sets' table (world/GameplaySets.hpp; every World registers all of them) with each arena's set blocks (arenaSets, by
// arena), the combat rules (the game's defaults) and the character (its skeleton, clips and hit shapes; the arenas
// share one body height). An arena's map is not here: the World that plays the arena reads it (LoadArena, big-map
// declaration 15.6).
struct GameContent final {
    std::vector<sim::ArenaContent> arenas;
    std::vector<ModeContent> modes;
    std::vector<GameplaySet> sets;
    std::vector<SetBlocks> arenaSets;
    // CharacterDigest of the character, read once (MatchInfo::characterDigest).
    std::uint64_t characterDigest = 0;
    sim::CombatRules combat;
    std::shared_ptr<const character::CharacterRig> character;

    [[nodiscard]] const sim::ArenaContent* Find(const std::string& id) const;
    // The index of the installed arena with this id.
    [[nodiscard]] std::optional<std::size_t> FindArena(std::string_view id) const;
    // The index of the installed mode with this id.
    [[nodiscard]] std::optional<std::size_t> FindMode(std::string_view id) const;
    // Whether the mode (an index into modes) plays the arena (an index into arenas): the one place every choice asks.
    [[nodiscard]] bool Plays(std::size_t mode, std::size_t arena) const;
    // The digest of the blocks the arena carries for the sets the mode turns on (MatchInfo::arenaDigest; one formula,
    // also when there is none). The mode's own rules are ModeContent::rulesDigest: kept apart, so another arena is
    // known as another arena (built again on it), not as other rules (design review 2, S1).
    [[nodiscard]] std::uint64_t ArenaDigest(std::size_t mode, std::size_t arena) const;
};

// Whether the walls stand between two points: they cut the straight segment from a to b. A sound's occlusion asks it
// (the Client hears a cue behind the map at a lower gain; present/SoundCues.hpp HeardGains).
[[nodiscard]] bool Blocked(const GYO::Engine::Physics3d::BoxSet& walls, const Math::Vec3& a, const Math::Vec3& b);

// What differs from the Match's (FollowMatchContent compares in this order): another mode, the same mode with other
// rules (the rules digest), another character (the character digest), another arena, the same arena with other blocks
// for the sets the mode turns on (the arena digest), the same arena with other walls (the walls digest). Mode and Arena
// are another match this Client may have; the others are other content.
enum class MismatchKind : std::uint8_t { None = 0, Mode, Rules, Character, Arena, ArenaData, Walls };

// Why this Client cannot play the Match's match (kind None: it can). Set by FollowMatchContent; it only says what
// differs, the program decides (it may build its game again on matchMode and matchArena when it has them -- kind Mode
// or Arena -- or leave).
struct ContentMismatch final {
    MismatchKind kind = MismatchKind::None;
    // In the player's words: what differs, with both ends' values.
    std::string reason;
    // The arena and the mode the Match plays.
    std::string matchArena;
    std::string matchMode;
};
void HashValue(K::Hasher&, const ContentMismatch&);

// The rules a World of this mode on this arena plays with (both ends: GameRules is the same at the Match and every
// Client); its walls digest is put once its map is built (LoadArena).
[[nodiscard]] GameRules GameRulesOf(const GameContent& content, std::size_t mode, std::size_t arena);

// Reads the content under root (the game's asset directory), with the gameplay sets' table (by default the game's).
// nullopt with error naming the file and what was wrong: a file that does not read or parse, an arena whose stance
// rules do not fit, a set block a set does not read. Whether its map reads, and its spawns
// are free of its walls, is known when a World reads the map (MapLoad).
[[nodiscard]] std::optional<GameContent> LoadGameContent(const std::string& root, std::string& error,
                                                         const std::vector<GameplaySet>& sets = GameSets());

// What a Product playing the game needs besides its Worlds: the map's loaders (voxel_map's) in its asset store
// (world_asset, with AssetOptions naming the content root and an executor). A Product's configure calls it.
void ConfigureGame(O::ProductSetup& setup);

// The seats of a Match: the admission takes one per user (Take), refusing a full Match. A user who connects again
// while their connection is still open (from any endpoint: the Client crashed and came back, or reconnected) is let
// in, and the newer connection replaces the older one (SeatPlayers: the user's decision of 2026-11-25,
// v8-arch-gyo2-direct-connect-and-kick-old). The seat is counted per connection, so the old connection's release does
// not free the new one's seat. The admission runs in the World's net System, and the Match's main loop reads Taken
// (the World may run on its own thread, so the state is behind a lock).
class Seats final {
public:
    explicit Seats(std::uint32_t capacity = 4) : capacity_(capacity) {}

    // nullopt: seated; else why not ("the match is full").
    [[nodiscard]] std::optional<std::string> Take(std::uint64_t userId);
    void Release(std::uint64_t userId);
    // The users seated.
    [[nodiscard]] std::uint32_t Taken() const;

private:
    std::uint32_t capacity_;
    mutable std::mutex mutex_;
    // Each seated user's open connections.
    std::map<std::uint64_t, std::uint32_t> connections_;
};

// Match: which player each connection (its Peer entity) controls. A newer connection of the same user takes the
// player over from the older one; a connection that ends removes only the player it still controls.
struct SeatedPlayers final {
    std::map<K::EntityId, K::EntityId> byPeer;
};
void HashValue(K::Hasher&, const SeatedPlayers&);

struct MatchSeats final {
    std::shared_ptr<Seats> seats;
};
void HashValue(K::Hasher&, const MatchSeats&);

// The Match: the names of the room's members by user (WorldOptions::playerNames), which NamePlayers gives their
// players. Not decision data.
struct PlayerNames final {
    std::map<std::uint64_t, std::string> byUser;
};
void HashValue(K::Hasher&, const PlayerNames&);

// The Match's admission without a key (a Match started by hand, --connect, the tests): the seats only.
[[nodiscard]] N::ServerHost::Admission SeatAdmission(std::shared_ptr<Seats> seats);

// What a Match the matches service started checks first (gateway declaration D5): its own key and id, and the wall
// clock in Unix seconds (an admission is not decision data, net D-9).
struct AdmissionCheck final {
    std::vector<std::byte> key;
    std::uint64_t matchId = 0;
    std::function<std::int64_t()> nowUnixSeconds;
};

// The token (match/AdmissionToken.hpp: Refused with the failed field as the detail), then the seats. Security is not
// in this line (the user, 2026-11-25: "現在不用考慮資安"): no TLS, no rate limit.
[[nodiscard]] N::ServerHost::Admission MatchAdmission(std::shared_ptr<Seats> seats, AdmissionCheck check);

// The allotter ranges: the Match's entities and every Client's own (render, sound) ones; kArenaEntity is outside both.
inline constexpr GYO::Engine::EntityIds::EntityIdsOptions kMatchIds{1, 1'000'000};
inline constexpr GYO::Engine::EntityIds::EntityIdsOptions kClientIds{1'000'001, 2'000'000};

struct WorldOptions final {
    std::shared_ptr<const GameContent> content;
    // The arena the World plays (an index into content->arenas): its map is read while the World generates.
    std::size_t arena = 0;
    // The mode it plays (an index into content->modes): its rule tables.
    std::size_t mode = 0;
    // A Client: its user id (NetProductOptions::userId). The Match: 0.
    std::uint64_t localUser = 0;
    // The Match: its seats (Seats); a Client: none.
    std::shared_ptr<Seats> seats;
    // Local: the spawn index of the local player (taken modulo the arena's spawns; the product picks it at random).
    std::size_t spawn = 0;
    // The Match: the users the start waits for (the room's members; none: played from the first step), and how long
    // after the first player came it waits at most (0: no limit).
    std::vector<std::uint64_t> expectUsers;
    std::uint32_t startDeadlineSeconds = 60;
    // The Match: the room's members' names by user (match::ParsePlayerNames; none: every player's name is empty).
    std::map<std::uint64_t, std::string> playerNames;
    // Where the map's walls are built (the Product's worker; not owned, it outlives the World). None: in the System,
    // on the World's thread.
    GYO::Engine::Asset::AssetExecutor* jobs = nullptr;
};

// Which end a World is.
enum class WorldEnd : std::uint8_t { Match, Client, Local };

// Local: where its player first stands (WorldOptions::spawn).
struct LocalSpawn final {
    std::size_t index = 0;
};
void HashValue(K::Hasher&, const LocalSpawn&);

// The authority's start (StartMatch): the users it waits for, when the first player came, the deadline after that.
struct MatchStart final {
    std::vector<std::uint64_t> expect;
    std::optional<std::uint64_t> firstTick;
    std::uint32_t deadlineSeconds = 0;
};
void HashValue(K::Hasher&, const MatchStart&);

// A death of a player as a save counts it: a dead player whose respawn tick (set when it dies) was not seen before.
// respawnSeen: the last one seen for that player.
[[nodiscard]] bool CountDeath(const PlayerCombat& combat, std::uint64_t& respawnSeen) noexcept;

// Local: the deaths of its player so far (CountDeath).
struct LocalDeaths final {
    std::uint64_t count = 0;
    std::uint64_t respawnSeen = 0;
};
void HashValue(K::Hasher&, const LocalDeaths&);

// Registers the game in the World (the table above). The World's description carries AuthorityOptions
// {ServerAuthorityStrategy}, dynamicInsert = false, the id range and, at the networked ends, NetWorldOptions and, at a
// Client, RollbackOptions{StateRollbackStrategy} (MatchWorld, ClientWorld and LocalWorld do that).
void BuildWorld(O::WorldSetup& setup, const WorldOptions& options, WorldEnd end);

// The World descriptions of the three ends, named "Game".
[[nodiscard]] O::WorldDescription MatchWorld(WorldOptions options);
[[nodiscard]] O::WorldDescription ClientWorld(WorldOptions options);
[[nodiscard]] O::WorldDescription LocalWorld(WorldOptions options);

// What one player is doing for its animation, from its state at tick (seconds are the World's steps of
// stepNanoseconds).
[[nodiscard]] sim::AnimationInput AnimationInputOf(const PlayerBody& body, const PlayerCombat& combat, const HitRecord& hits,
                                                   const GameRules& rules, std::uint64_t tick, std::uint64_t stepNanoseconds);

// The player a Client controls (its own Owner), or 0 before the Match's player has reached it.
[[nodiscard]] K::EntityId OwnPlayer(K::World& world);

} // namespace object_fps_pvp::world
