#include "object_fps_pvp/world/Modes.hpp"

#include "data/JsonFields.hpp"

#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/ReadAllText.hpp"
#include "engine/kernel/Hash.hpp"

#include <algorithm>
#include <stdexcept>

namespace object_fps_pvp::world {

namespace Json = GYO::Engine::Json;
namespace K = GYO::Kernel;

namespace {

std::string PlainName(const Json::Value& value, const std::string_view what) {
    std::string name = data::String(value, what);
    if (name.empty() || name == "." || name == ".." || name.find_first_of("/\\:") != std::string::npos) {
        throw std::runtime_error(std::string{what} + " is not a plain file name: " + name);
    }
    return name;
}

std::vector<std::string> Names(const Json::Value& value, const std::string_view what) {
    const auto array = value.AsArray();
    if (!array) throw std::runtime_error(std::string{what} + " is not an array");
    std::vector<std::string> names;
    for (const auto item : array.value()) {
        std::string name = data::String(item, what);
        if (name.empty()) throw std::runtime_error(std::string{what} + " names an empty id");
        if (std::find(names.begin(), names.end(), name) != names.end()) throw std::runtime_error(std::string{what} + " names '" + name + "' twice");
        names.push_back(std::move(name));
    }
    return names;
}

} // namespace

std::optional<std::vector<std::string>> ParseModeList(const std::string_view text, std::string& error) {
    error.clear();
    try {
        const auto document = data::ParseDocument(text);
        const auto json = data::AsObject(document.Root(), "the mode list");
        if (data::Integer(data::Member(json, "version"), "version") != 1) throw std::runtime_error("unsupported version");
        const auto modes = data::Member(json, "modes").AsArray();
        if (!modes) throw std::runtime_error("modes is not an array");
        std::vector<std::string> names;
        for (const auto item : modes.value()) names.push_back(PlainName(item, "a mode file"));
        if (names.empty()) throw std::runtime_error("the list names no mode");
        return names;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

std::optional<ModeFile> ParseMode(const std::string_view text, const std::vector<GameplaySet>& sets, std::string& error) {
    error.clear();
    try {
        const auto document = data::ParseDocument(text);
        const auto json = data::AsObject(document.Root(), "the mode");
        if (data::Integer(data::Member(json, "version"), "version") != 1) throw std::runtime_error("unsupported mode version");
        ModeFile mode;
        mode.id = data::String(data::Member(json, "id"), "id");
        if (mode.id.empty() || mode.id.size() > sim::kMaxContentIdBytes) throw std::runtime_error("id is 1 to 64 characters");
        mode.name = data::String(data::Member(json, "name"), "name");
        for (const auto rule : data::AsObject(data::Member(json, "rules"), "rules")) {
            std::string kind(rule.key);
            if (std::any_of(mode.rules.begin(), mode.rules.end(), [&](const auto& named) { return named.first == kind; })) {
                throw std::runtime_error("rules names the kind '" + kind + "' twice");
            }
            mode.rules.emplace_back(std::move(kind), data::ContentPath(rule.value, "a rule file"));
        }
        std::string why;
        auto blocks = ReadSetBlocks(data::AsObject(data::Member(json, "sets"), "sets"), sets, why);
        if (!blocks) throw std::runtime_error(why);
        mode.sets = std::move(*blocks);
        mode.arenas = Names(data::Member(json, "arenas"), "arenas");
        return mode;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

bool ModeContent::Plays(const std::string_view arenaId) const {
    return arenas.empty() || std::find(arenas.begin(), arenas.end(), arenaId) != arenas.end();
}

namespace {

std::optional<std::string> ReadText(GYO::Engine::IO::FS::NativeFileSystem& fs, const std::string& path, std::string& error) {
    auto uri = GYO::Engine::IO::FS::NativeFileUri(path);
    if (!uri) {
        error = path + ": " + GYO::Engine::Base::Describe(uri.error());
        return std::nullopt;
    }
    auto text = GYO::Engine::IO::Helpers::ReadAllTextUtf8(fs, *uri);
    if (!text) {
        error = path + ": " + GYO::Engine::Base::Describe(text.error());
        return std::nullopt;
    }
    return std::move(*text);
}

} // namespace

std::optional<std::vector<ModeContent>> LoadModes(const std::string& root, const std::vector<sim::ArenaContent>& arenas,
                                                  const sim::CombatRules& combat, const std::vector<GameplaySet>& sets,
                                                  std::string& error) {
    error.clear();
    GYO::Engine::IO::FS::NativeFileSystem fs;
    const std::string folder = root + "/modes/";
    auto listText = ReadText(fs, folder + "modes.json", error);
    if (!listText) return std::nullopt;
    auto list = ParseModeList(*listText, error);
    if (!list) {
        error = folder + "modes.json: " + error;
        return std::nullopt;
    }
    std::vector<ModeContent> modes;
    for (const std::string& name : *list) {
        const std::string path = folder + name;
        const auto fail = [&](std::string why) {
            error = path + ": " + std::move(why);
            return std::nullopt;
        };
        auto text = ReadText(fs, path, error);
        if (!text) return std::nullopt;
        auto file = ParseMode(*text, sets, error);
        if (!file) return fail(error);
        if (std::any_of(modes.begin(), modes.end(), [&](const ModeContent& mode) { return mode.id == file->id; })) {
            return fail("another installed mode has the id '" + file->id + "'");
        }
        ModeContent mode;
        mode.id = file->id;
        mode.name = file->name;
        mode.arenas = file->arenas;
        bool movement = false;
        for (const auto& [kind, rulePath] : file->rules) {
            if (kind != "movement" && kind != "damage") return fail("the game knows no rule kind '" + kind + "'");
            auto ruleText = ReadText(fs, root + "/" + rulePath, error);
            if (!ruleText) return std::nullopt;
            bool read = false;
            if (kind == "movement") {
                const auto rules = sim::ParseMovementRules(*ruleText, error);
                if ((read = rules.has_value())) mode.movement = *rules;
                movement = read;
            } else {
                const auto rules = sim::ParseDamageRules(*ruleText, error);
                if ((read = rules.has_value())) mode.damage = *rules;
            }
            if (!read) {
                error = root + "/" + rulePath + ": " + error;
                return std::nullopt;
            }
        }
        if (!movement) return fail("rules names no movement rules");
        for (const std::string& id : mode.arenas) {
            if (std::none_of(arenas.begin(), arenas.end(), [&](const sim::ArenaContent& arena) { return arena.id == id; })) {
                return fail("no installed arena has the id '" + id + "'");
            }
        }
        for (const auto& [slot, read] : file->sets) mode.on.Add(slot);
        mode.rulesDigest = RulesDigest(*file, sets, mode.movement, mode.damage, combat);
        mode.sets = std::move(file->sets);
        modes.push_back(std::move(mode));
    }
    return modes;
}

std::uint64_t RulesDigest(const ModeFile& mode, const std::vector<GameplaySet>& table, const sim::MovementRules& movement,
                          const sim::DamageRules& damage, const sim::CombatRules& combat) {
    K::Hasher h;
    // The mode: what decides the match (its display name does not).
    HashValue(h, std::string("gyo.object_fps_pvp.rules/1"));
    HashValue(h, mode.id);
    // The sets it turns on, by name, each with the digest of its settings.
    std::vector<std::pair<std::string, std::uint64_t>> sets;
    for (const auto& [slot, read] : mode.sets) sets.emplace_back(table[slot].name, read.digest);
    std::sort(sets.begin(), sets.end());
    HashValue(h, sets);
    std::vector<std::string> arenas = mode.arenas;
    std::sort(arenas.begin(), arenas.end());
    HashValue(h, arenas);
    // The rule tables, by kind (movement, damage), and the combat rules (the game's defaults until a rule table carries
    // them).
    HashValue(h, movement);
    HashValue(h, damage);
    HashValue(h, combat);
    return Digest64(h);
}

} // namespace object_fps_pvp::world
