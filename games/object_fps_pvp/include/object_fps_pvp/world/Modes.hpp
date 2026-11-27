#pragma once

// The game's modes (mode-config declaration, section 3): one executable, and the mode's data decides which rule tables
// a match plays with (and, from the gameplay sets, which of the game's Systems run). A mode file names its rule tables
// by kind; the World of a match plays one mode (another mode is another World, as another arena).
//
//   modes/modes.json   version 1: the mode files, plain names next to it, in order; the first is the default
//   modes/<id>.json    version 1: { id, name, rules: { kind: file }, sets: { set: settings }, arenas: [id...] }
//
// The rule kinds the game knows: movement (sim/MovementRules.hpp, a file under the content root). The combat rules are
// still the game's defaults (sim/CombatRules.hpp): they go into the rules digest all the same.

#include "object_fps_pvp/sim/Arena.hpp"
#include "object_fps_pvp/sim/CombatRules.hpp"
#include "object_fps_pvp/sim/MovementRules.hpp"
#include "object_fps_pvp/world/GameplaySets.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace object_fps_pvp::character {
struct CharacterRig;
struct HitShapeDefinition;
// What the rules digest takes of a hit shape (the character's own data has no Kernel; its hash is here, with the
// digest's other parts).
void HashValue(GYO::Kernel::Hasher& h, const HitShapeDefinition& v);
} // namespace object_fps_pvp::character

namespace object_fps_pvp::sim {
// The rule tables' values, field by field, for the rules digest (the sim library has no Kernel).
void HashValue(GYO::Kernel::Hasher& h, const StanceRules& v);
void HashValue(GYO::Kernel::Hasher& h, const CombatRules& v);
} // namespace object_fps_pvp::sim

namespace object_fps_pvp::world {

// What a mode file says.
struct ModeFile final {
    std::string id;
    // What the lobby and the rooms show.
    std::string name;
    // Rule kind -> file (relative to the content root), in the file's order.
    std::vector<std::pair<std::string, std::string>> rules;
    // The gameplay sets it turns on, each with what the set read of its settings (SetBlocks: by slot).
    SetBlocks sets;
    // The arenas it plays, by id (empty: every installed arena).
    std::vector<std::string> arenas;
};

// Reads modes/modes.json. nullopt with error: not version 1, not a list of plain file names, or empty.
[[nodiscard]] std::optional<std::vector<std::string>> ParseModeList(std::string_view text, std::string& error);

// Reads a mode file; its sets with the table's readers (ReadSetBlocks). nullopt with error: a member missing or of the
// wrong kind, not version 1, an id not of 1 to 64 characters (as an arena's), a rule kind twice or a rule file that is not a relative path inside the
// content root, an arena named twice, or what ReadSetBlocks says. Whether the rule kinds and the arenas are known is
// LoadModes' to say.
[[nodiscard]] std::optional<ModeFile> ParseMode(std::string_view text, const std::vector<GameplaySet>& sets, std::string& error);

// One installed mode, read.
struct ModeContent final {
    std::string id;
    std::string name;
    sim::StanceRules stance;
    // The arenas it plays (empty: all).
    std::vector<std::string> arenas;
    // The gameplay sets it turns on (by slot), and what each read of its settings.
    SetsOn on;
    SetBlocks sets;
    // What a Client compares with the Match's (MatchInfo::rulesDigest): the mode (its id, sets and arenas) and the
    // rules a match of it plays with (its rule tables, the combat rules and the character's hit shapes), as values.
    std::uint64_t rulesDigest = 0;

    [[nodiscard]] bool Plays(std::string_view arenaId) const;
};

// Reads the installed modes under root (the game's asset directory): modes/modes.json and the files it lists, and each
// mode's rule tables. nullopt with error naming the file and what was wrong: a file that does not read or parse, two
// modes with one id, a rule kind the game does not know, no movement rules, a set the table does not have (or its
// settings do not read), an arena that is not installed, or one whose body the movement rules do not fit.
[[nodiscard]] std::optional<std::vector<ModeContent>> LoadModes(const std::string& root, const std::vector<sim::ArenaContent>& arenas,
                                                                const sim::CombatRules& combat, const character::CharacterRig& character,
                                                                const std::vector<GameplaySet>& sets, std::string& error);

// The rules digest of a mode (ModeContent::rulesDigest): the parsed values through the Kernel's Hasher, never the
// files' bytes (a line ending, a space or the order of an object's members does not change it).
// The sets are hashed by name (from the table) with the digest of what each read.
[[nodiscard]] std::uint64_t RulesDigest(const ModeFile& mode, const std::vector<GameplaySet>& sets, const sim::StanceRules& stance,
                                        const sim::CombatRules& combat, const character::CharacterRig& character);

} // namespace object_fps_pvp::world
