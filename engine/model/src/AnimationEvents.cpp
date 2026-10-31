#include "engine/model/AnimationEvents.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <utility>

namespace Engine::Model {
namespace {
using Json = nlohmann::json;

// Thrown inside parsing only, and turned into the module's error at its end.
struct Invalid final {
    std::string message;
    std::string path;
};

[[noreturn]] void Fail(std::string message, std::string path) {
    throw Invalid{std::move(message), std::move(path)};
}

void RequireExactKeys(const Json& object, const std::set<std::string>& keys, const std::string& path) {
    for (const auto& key : keys)
        if (!object.contains(key)) Fail("missing field '" + key + "'", path);
    for (const auto& item : object.items())
        if (!keys.contains(item.key())) Fail("unknown field '" + item.key() + "'", path);
}

bool ValidEventName(const std::string& name) {
    if (name.empty() || name.size() > MaxAnimationEventNameLength) return false;
    return std::all_of(name.begin(), name.end(), [](const char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
    });
}

// The DOM keeps the last of repeated keys, so repeats are caught while parsing.
Json ParseWithoutRepeatedKeys(const std::string_view text) {
    std::vector<std::set<std::string>> open;
    Json::parser_callback_t callback = [&open](int, const Json::parse_event_t event, Json& parsed) {
        if (event == Json::parse_event_t::object_start) {
            open.emplace_back();
        } else if (event == Json::parse_event_t::object_end) {
            open.pop_back();
        } else if (event == Json::parse_event_t::key) {
            const auto key = parsed.get<std::string>();
            if (!open.back().insert(key).second) Fail("repeated key '" + key + "'", "");
        }
        return true;
    };
    try {
        return Json::parse(text.begin(), text.end(), callback);
    } catch (const Json::exception& failure) {
        Fail(std::string("not JSON: ") + failure.what(), "");
    }
}

AnimationEventTrack ParseTrack(const std::string& clip, const Json& events, const std::string& path) {
    if (clip.empty()) Fail("a clip name must not be empty", path);
    if (!events.is_array() || events.empty()) Fail("a clip lists one or more events", path);
    AnimationEventTrack track{clip, {}};
    for (std::size_t i = 0; i < events.size(); ++i) {
        const auto& entry = events[i];
        const auto at = path + "[" + std::to_string(i) + "]";
        if (!entry.is_object()) Fail("an event is an object", at);
        RequireExactKeys(entry, {"seconds", "event"}, at);
        const auto& seconds = entry.at("seconds");
        if (!seconds.is_number()) Fail("seconds is a number", at + ".seconds");
        AnimationEvent event{seconds.get<double>(), {}};
        if (!std::isfinite(event.seconds) || event.seconds < 0)
            Fail("seconds is finite and not negative", at + ".seconds");
        const auto& name = entry.at("event");
        if (!name.is_string() || !ValidEventName(name.get<std::string>()))
            Fail("an event name is 1 to 64 characters of a-z, 0-9, '_' and '.'", at + ".event");
        event.name = name.get<std::string>();
        if (!track.events.empty()) {
            const auto& previous = track.events.back();
            if (event.seconds < previous.seconds) Fail("events are in non-decreasing seconds", at + ".seconds");
            for (auto it = track.events.rbegin(); it != track.events.rend() && it->seconds == event.seconds; ++it)
                if (it->name == event.name) Fail("an event repeats at the same seconds", at);
        }
        track.events.push_back(std::move(event));
    }
    return track;
}

AnimationEventSet ParseSet(const std::string_view text) {
    const auto document = ParseWithoutRepeatedKeys(text);
    if (!document.is_object()) Fail("the document is an object", "");
    RequireExactKeys(document, {"schema", "version", "model_asset_id", "clips"}, "");
    const auto& schema = document.at("schema");
    if (!schema.is_string() || schema.get<std::string>() != AnimationEventsSchema)
        Fail("schema is '" + std::string(AnimationEventsSchema) + "'", "schema");
    const auto& version = document.at("version");
    if (!version.is_number_integer() || version.get<long long>() != AnimationEventsVersion)
        Fail("version is " + std::to_string(AnimationEventsVersion), "version");
    const auto& model = document.at("model_asset_id");
    if (!model.is_string() || model.get<std::string>().empty())
        Fail("model_asset_id is a nonempty string", "model_asset_id");
    const auto& clips = document.at("clips");
    if (!clips.is_object() || clips.empty()) Fail("clips is a nonempty object", "clips");
    AnimationEventSet set{model.get<std::string>(), {}};
    for (const auto& item : clips.items())
        set.tracks.push_back(ParseTrack(item.key(), item.value(), "clips.\"" + item.key() + "\""));
    return set;
}

ModelError MakeError(std::string message, std::string path) {
    return ModelError::Make(ModelErrorCode::InvalidAnimationEvents, "gyo.anim_events: " + std::move(message),
                            std::move(path));
}

} // namespace

ModelResult<AnimationEventSet> ParseAnimationEvents(const std::string_view text) {
    try {
        return ParseSet(text);
    } catch (Invalid& invalid) {
        return Base::Err(MakeError(std::move(invalid.message), std::move(invalid.path)));
    }
}

std::span<const AnimationEvent> BoundAnimationEvents::ForClip(const std::size_t clipIndex) const noexcept {
    if (clipIndex >= byClip.size()) return {};
    return byClip[clipIndex];
}

ModelResult<BoundAnimationEvents> BindAnimationEvents(const AnimationEventSet& events, const ModelAsset& model) {
    BoundAnimationEvents bound;
    bound.byClip.resize(model.clips.size());
    for (const auto& track : events.tracks) {
        const auto path = "clips.\"" + track.clip + "\"";
        const auto clip = model.FindClip(track.clip);
        if (!clip) return Base::Err(MakeError("the model has no clip '" + track.clip + "'", path));
        const double duration = model.clips[*clip].durationSeconds;
        for (std::size_t i = 0; i < track.events.size(); ++i) {
            if (!(track.events[i].seconds <= duration))
                return Base::Err(MakeError("an event lies beyond its clip's " + std::to_string(duration) + " seconds",
                                           path + "[" + std::to_string(i) + "].seconds"));
        }
        bound.byClip[*clip] = track.events;
    }
    return bound;
}

} // namespace Engine::Model
