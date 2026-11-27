#pragma once

// The game's gameplay sets (mode-config declaration 2.4, section 4): a part of the game (doors, weapon switching, a
// hit's consequences...) that a mode turns on or leaves off. Every set is registered in every World, whatever the mode
// (its components, Replicate, authority and Systems: the net protocol hash and the consistency fingerprint do not depend
// on the mode, so both ends connect and the mode is compared in MatchInfo). Its Systems are members of its two Kernel
// sets, pvp.<name>.fixed (FixedUpdate) and pvp.<name>.frame (Main), whose run condition pvp.mode.<name> is true only
// when this World's mode turns the set on (ActiveMode). A set that is off runs nothing and puts its components on no
// entity: nothing of it is replicated, nothing of it is in the authority digest.
//
// A set's registration is Bevy's Fn(&mut App) plugin (bevy_app/src/plugin.rs:96-99); the condition its Kernel sets
// carry is Bevy's configure_sets(...).run_if (bevy_ecs/src/schedule/config.rs). The sets are registered after the
// core's registrations, in the table's order, so the core's authority registration numbers and wire ids do not move.

#include "engine/json/Document.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace object_fps_pvp::world {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;

// At most this many gameplay sets (each set's run condition is a function of its slot).
inline constexpr std::size_t kMaxGameplaySets = 32;

// What a set read from a mode file or an arena file: the digest of the values it read (into the rules digest), and
// what it puts into a World that plays it (its own Resource, before the first frame).
struct SetData final {
    std::uint64_t digest = 0;
    std::function<void(K::World&)> put;
};

// Reads a set's block (an object). nullopt with error naming what was wrong.
using SetReader = std::function<std::optional<SetData>(const GYO::Engine::Json::ObjectView& block, std::string& error)>;

struct GameplaySet final {
    // Its name (ASCII, as the game's names): the key in a mode file's sets and an arena file's sets.
    std::string name;
    // Its registration: components, net, authority, Systems (each in GameplaySetFixed or GameplaySetFrame).
    std::function<void(O::WorldSetup&)> build;
    // Its settings in a mode file (none: it takes {} only).
    SetReader readMode;
    // Its block in an arena file (none: an arena may not have one for it).
    SetReader readArena;
};

// The game's own sets, in registration order (the slot of each is its index). A test may give its own table to
// LoadGameContent.
[[nodiscard]] const std::vector<GameplaySet>& GameSets();

// The Kernel sets a gameplay set's Systems are members of (Mount::sets).
[[nodiscard]] std::string GameplaySetFixed(std::string_view set);
[[nodiscard]] std::string GameplaySetFrame(std::string_view set);

// Which sets this World's mode turns on, by slot (ActiveMode::on).
using SetsOn = std::bitset<kMaxGameplaySets>;

// The mode this World plays (WorldOptions::mode): put before the first frame, never written after. Not decision data,
// and not GameRules (LoadArena writes that while the World generates: a condition reading it would conflict with it).
struct ActiveMode final {
    std::string id;
    std::uint64_t rulesDigest = 0;
    SetsOn on;
};
void HashValue(K::Hasher&, const ActiveMode&);

// What a mode file or an arena file says for the sets: by slot (the set's index in the table), what each read.
using SetBlocks = std::vector<std::pair<std::size_t, SetData>>;

// Reads the blocks of an object of sets (set name -> block) with each set's reader (readMode, or readArena when arena
// is true). nullopt with error: a name the table does not have, a block that is not an object, one for a set without a
// reader of that kind that is not {}, or what the set's reader says.
[[nodiscard]] std::optional<SetBlocks> ReadSetBlocks(const GYO::Engine::Json::ObjectView& blocks, const std::vector<GameplaySet>& sets,
                                                     bool arena, std::string& error);

// The sets block of an arena file (its "sets" member; none: no block). nullopt with error as ReadSetBlocks.
[[nodiscard]] std::optional<SetBlocks> ReadArenaSets(std::string_view arenaText, const std::vector<GameplaySet>& sets, std::string& error);

// The run condition of the slot's set (pvp.mode.<name>): true when the World's ActiveMode turns it on.
[[nodiscard]] K::ConditionRef GameplaySetOn(std::size_t slot, const std::string& name);

} // namespace object_fps_pvp::world
