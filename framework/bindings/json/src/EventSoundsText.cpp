#include "gyo/framework/json/EventSoundsText.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace Gyo::Tools::Framework {
namespace {

// A sorted object, so events come out in ascending name order (std::string's).
using Json = nlohmann::json;

// Thrown inside parsing only, and turned into the binding's error at its end.
struct Invalid final {
    std::string message;
    std::string path;
};

[[noreturn]] void Fail(std::string message, std::string path) {
    throw Invalid{std::move(message), std::move(path)};
}

// The path of a member: event names are quoted, as in events."magazine_in".
std::string MemberPath(const std::string& object, const std::string& key) {
    if (object.empty()) return key;
    if (object == "events") return object + ".\"" + key + "\"";
    return object + "." + key;
}

void RequireExactKeys(const Json& object, const std::set<std::string>& keys, const std::string& path) {
    for (const auto& key : keys)
        if (!object.contains(key)) Fail("missing field '" + key + "'", path);
    for (const auto& item : object.items())
        if (!keys.contains(item.key())) Fail("unknown field '" + item.key() + "'", MemberPath(path, item.key()));
}

// The DOM keeps the last of repeated keys, so repeats are caught while parsing.
Json ParseWithoutRepeatedKeys(const std::string_view text) {
    struct Open final {
        std::set<std::string> keys;
        std::string path;
        std::string last; // the latest key, the parent of a nested object
    };
    std::vector<Open> open;
    Json::parser_callback_t callback = [&open](int, const Json::parse_event_t event, Json& parsed) {
        if (event == Json::parse_event_t::object_start) {
            open.push_back({{}, open.empty() ? std::string() : MemberPath(open.back().path, open.back().last), {}});
        } else if (event == Json::parse_event_t::object_end) {
            open.pop_back();
        } else if (event == Json::parse_event_t::key) {
            auto key = parsed.get<std::string>();
            if (!open.back().keys.insert(key).second)
                Fail("repeated key '" + key + "'", MemberPath(open.back().path, key));
            open.back().last = std::move(key);
        }
        return true;
    };
    try {
        return Json::parse(text.begin(), text.end(), callback);
    } catch (const Json::exception& failure) {
        Fail(std::string("not JSON: ") + failure.what(), "");
    }
}

std::string String(const Json& value, const char* what, const std::string& path) {
    if (!value.is_string()) Fail(std::string(what) + " is a string", path);
    return value.get<std::string>();
}

double Number(const Json& value, const char* what, const std::string& path) {
    if (!value.is_number()) Fail(std::string(what) + " is a number", path);
    return value.get<double>();
}

EventSoundSet ParseSet(const std::string_view text) {
    const auto document = ParseWithoutRepeatedKeys(text);
    if (!document.is_object()) Fail("the document is an object", "");
    RequireExactKeys(document, {"schema", "version", "animation_events_asset_id", "events"}, "");
    const auto& schema = document.at("schema");
    if (!schema.is_string() || schema.get<std::string>() != EventSoundsSchema)
        Fail("schema is '" + std::string(EventSoundsSchema) + "'", "schema");
    const auto& version = document.at("version");
    if (!version.is_number_integer() || version.get<long long>() != EventSoundsVersion)
        Fail("version is " + std::to_string(EventSoundsVersion), "version");
    EventSoundSet set;
    set.animationEventsAssetId =
        String(document.at("animation_events_asset_id"), "animation_events_asset_id", "animation_events_asset_id");
    const auto& events = document.at("events");
    if (!events.is_object()) Fail("events is an object", "events");
    for (const auto& item : events.items()) {
        const auto path = MemberPath("events", item.key());
        const auto& entry = item.value();
        if (!entry.is_object()) Fail("an event's sound is an object", path);
        RequireExactKeys(entry, {"sound_asset_id", "sync_seconds", "gain"}, path);
        EventSound sound;
        sound.event = item.key();
        sound.soundAssetId = String(entry.at("sound_asset_id"), "sound_asset_id", path + ".sound_asset_id");
        sound.syncSeconds = Number(entry.at("sync_seconds"), "sync_seconds", path + ".sync_seconds");
        const double gain = Number(entry.at("gain"), "gain", path + ".gain");
        // A number a float cannot hold is no gain; the range is Engine's (CheckEventSoundSet).
        if (!(std::fabs(gain) <= std::numeric_limits<float>::max())) Fail("gain is a GYO::Audio gain", path + ".gain");
        sound.gain = static_cast<float>(gain);
        set.sounds.push_back(std::move(sound));
    }
    return set;
}

} // namespace

Result<EventSoundSet, EventSoundsError> ParseEventSounds(const std::string_view text) {
    EventSoundSet set;
    try {
        set = ParseSet(text);
    } catch (Invalid& invalid) {
        return Engine::Base::Err(EventSoundsError::Make(EventSoundsErrorCode::InvalidEventSounds,
            "gyo.event_sounds: " + std::move(invalid.message), std::move(invalid.path)));
    }
    if (auto checked = CheckEventSoundSet(set); !checked) return Engine::Base::Err(std::move(checked).error());
    return set;
}

} // namespace Gyo::Tools::Framework
