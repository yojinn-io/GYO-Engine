#include "object_fps_pvp/match/PlayerSave.hpp"

#include "data/JsonFields.hpp"

#include "engine/json/Writer.hpp"

#include <stdexcept>

namespace object_fps_pvp::match {

namespace J = GYO::Engine::Json;

PlayerSave Settled(PlayerSave save, const std::uint64_t deaths) noexcept {
    ++save.matches;
    save.deaths += deaths;
    return save;
}

std::string EncodePlayerSave(const PlayerSave& save) {
    std::string out;
    J::Writer writer(out);
    writer.BeginObject();
    writer.Key("saveVersion");
    writer.Uint(kPlayerSaveVersion);
    writer.Key("matches");
    writer.Uint(save.matches);
    writer.Key("deaths");
    writer.Uint(save.deaths);
    writer.EndObject();
    (void)writer.Finish();
    return out;
}

std::optional<PlayerSave> DecodePlayerSave(const std::string_view body, std::string& error) {
    try {
        const J::Document document = data::ParseDocument(body);
        if (document.Root().GetKind() == J::Kind::Null) return PlayerSave{};
        const J::ObjectView object = data::AsObject(document.Root(), "the save");
        const std::uint64_t version = data::Unsigned(data::Member(object, "saveVersion"), "saveVersion");
        if (version != kPlayerSaveVersion) {
            error = "the save's saveVersion is " + std::to_string(version) + ", this Match reads " + std::to_string(kPlayerSaveVersion);
            return std::nullopt;
        }
        PlayerSave save;
        save.matches = data::Unsigned(data::Member(object, "matches"), "matches");
        save.deaths = data::Unsigned(data::Member(object, "deaths"), "deaths");
        return save;
    } catch (const std::runtime_error& failure) {
        error = failure.what();
        return std::nullopt;
    }
}

} // namespace object_fps_pvp::match
