#include "object_fps_pvp/match/PlayerRoster.hpp"

#include "data/JsonFields.hpp"

#include <stdexcept>

namespace object_fps_pvp::match {

namespace J = GYO::Engine::Json;

std::optional<std::map<std::uint64_t, std::string>> ParsePlayerNames(const std::string_view line, std::string& error) {
    try {
        const J::Document document = data::ParseDocument(line);
        const J::ObjectView root = data::AsObject(document.Root(), "the roster");
        const auto players = data::Member(root, "players").AsArray();
        if (!players) throw std::runtime_error("players is not an array");
        std::map<std::uint64_t, std::string> names;
        for (const J::Value player : *players) {
            const J::ObjectView entry = data::AsObject(player, "a player");
            const std::uint64_t user = data::Unsigned(data::Member(entry, "userId"), "userId");
            if (user == 0) throw std::runtime_error("a userId is 0");
            std::string name = data::String(data::Member(entry, "name"), "name");
            if (name.size() > kMaxPlayerNameBytes) throw std::runtime_error("a name is longer than 128 bytes");
            if (!names.emplace(user, std::move(name)).second) throw std::runtime_error("a userId comes twice");
        }
        return names;
    } catch (const std::runtime_error& failure) {
        error = failure.what();
        return std::nullopt;
    }
}

} // namespace object_fps_pvp::match
