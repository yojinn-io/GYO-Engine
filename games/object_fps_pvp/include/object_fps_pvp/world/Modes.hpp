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

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace object_fps_pvp::character {
struct CharacterRig;
}

namespace object_fps_pvp::world {

// What a mode file says.
struct ModeFile final {
    std::string id;
    // What the lobby and the rooms show.
    std::string name;
    // Rule kind -> file (relative to the content root), in the file's order.
    std::vector<std::pair<std::string, std::string>> rules;
    // The gameplay sets it turns on, by name, each with its settings (the set reads them); in the file's order.
    std::vector<std::string> sets;
    // The arenas it plays, by id (empty: every installed arena).
    std::vector<std::string> arenas;
};

// Reads modes/modes.json. nullopt with error: not version 1, not a list of plain file names, or empty.
[[nodiscard]] std::optional<std::vector<std::string>> ParseModeList(std::string_view text, std::string& error);

// Reads a mode file. nullopt with error: a member missing or of the wrong kind, not version 1, an empty id, a rule kind
// twice or a rule file that is not a relative path inside the content root, a set or an arena named twice.
// Whether the rule kinds, the sets and the arenas are known is LoadModes' to say.
[[nodiscard]] std::optional<ModeFile> ParseMode(std::string_view text, std::string& error);

// One installed mode, read.
struct ModeContent final {
    std::string id;
    std::string name;
    sim::StanceRules stance;
    // The arenas it plays (empty: all).
    std::vector<std::string> arenas;
    // What a Client compares with the Match's (MatchInfo::rulesDigest): the mode (its id, sets and arenas) and the
    // rules a match of it plays with (its rule tables, the combat rules and the character's hit shapes), as values.
    std::uint64_t rulesDigest = 0;

    [[nodiscard]] bool Plays(std::string_view arenaId) const;
};

// Reads the installed modes under root (the game's asset directory): modes/modes.json and the files it lists, and each
// mode's rule tables. nullopt with error naming the file and what was wrong: a file that does not read or parse, two
// modes with one id, a rule kind the game does not know, no movement rules, a set the game does not have, an arena
// that is not installed, or one whose body the movement rules do not fit.
[[nodiscard]] std::optional<std::vector<ModeContent>> LoadModes(const std::string& root, const std::vector<sim::ArenaContent>& arenas,
                                                                const sim::CombatRules& combat, const character::CharacterRig& character,
                                                                std::string& error);

// The rules digest of a mode (ModeContent::rulesDigest): the parsed values through the Kernel's Hasher, never the
// files' bytes (a line ending, a space or the order of an object's members does not change it).
[[nodiscard]] std::uint64_t RulesDigest(const ModeFile& mode, const sim::StanceRules& stance, const sim::CombatRules& combat,
                                        const character::CharacterRig& character);

} // namespace object_fps_pvp::world
