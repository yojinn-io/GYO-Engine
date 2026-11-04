#include "gyo/model_viewer/EventDocument.hpp"

#include "JsonText.hpp"

#include "engine/base/Assert.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

namespace Gyo::Tools::ModelViewer {

// A JSON string: quotes, backslashes and control characters escaped, every
// other byte (UTF-8 included) as it is.
void AppendString(std::string& out, const std::string_view text) {
    out += '"';
    for (const char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                std::array<char, 8> escaped{};
                std::snprintf(escaped.data(), escaped.size(), "\\u%04x", static_cast<unsigned>(c));
                out += escaped.data();
            } else {
                out += c;
            }
        }
    }
    out += '"';
}

// The shortest decimal that reads back as the same double (Design Invariant
// 3). Negative zero keeps its sign as "-0.0": "-0" reads back as the integer 0.
// A value that is not finite comes out as text JSON has no number for, so
// Engine's parser refuses it.
void AppendSeconds(std::string& out, const double seconds) {
    if (seconds == 0.0 && std::signbit(seconds)) {
        out += "-0.0";
        return;
    }
    std::array<char, 64> digits{};
    const auto written = std::to_chars(digits.data(), digits.data() + digits.size(), seconds);
    GYO_ASSERT(written.ec == std::errc{});
    out.append(digits.data(), written.ptr);
}

namespace {

namespace Model = Engine::Model;

bool SameSeconds(const double left, const double right) noexcept {
    return std::bit_cast<std::uint64_t>(left) == std::bit_cast<std::uint64_t>(right);
}

std::vector<const Model::AnimationEventTrack*> InNameOrder(const Model::AnimationEventSet& set) {
    std::vector<const Model::AnimationEventTrack*> tracks;
    tracks.reserve(set.tracks.size());
    for (const auto& track : set.tracks) tracks.push_back(&track);
    std::stable_sort(tracks.begin(), tracks.end(),
                     [](const auto* left, const auto* right) { return left->clip < right->clip; });
    return tracks;
}

ModelViewerError InvalidEvents(const Model::ModelError& cause) {
    return ModelViewerError::Make(ModelViewerErrorCode::InvalidEvents, "Engine refuses the events",
                                  Engine::Base::CauseDetail(cause));
}

} // namespace

std::string WriteAnimationEvents(const Model::AnimationEventSet& set) {
    std::string out = "{\n  \"schema\": ";
    AppendString(out, Model::AnimationEventsSchema);
    out += ",\n  \"version\": " + std::to_string(Model::AnimationEventsVersion) + ",\n  \"model_asset_id\": ";
    AppendString(out, set.modelAssetId);
    out += ",\n  \"clips\": {";
    const auto tracks = InNameOrder(set);
    for (std::size_t t = 0; t < tracks.size(); ++t) {
        out += t == 0 ? "\n    " : ",\n    ";
        AppendString(out, tracks[t]->clip);
        out += ": [";
        const auto& events = tracks[t]->events;
        for (std::size_t e = 0; e < events.size(); ++e) {
            out += e == 0 ? "\n      { \"seconds\": " : ",\n      { \"seconds\": ";
            AppendSeconds(out, events[e].seconds);
            out += ", \"event\": ";
            AppendString(out, events[e].name);
            out += " }";
        }
        out += events.empty() ? "]" : "\n    ]";
    }
    out += tracks.empty() ? "}\n}\n" : "\n  }\n}\n";
    return out;
}

bool SameEvents(const Model::AnimationEventSet& left, const Model::AnimationEventSet& right) noexcept {
    if (left.modelAssetId != right.modelAssetId || left.tracks.size() != right.tracks.size()) return false;
    for (std::size_t t = 0; t < left.tracks.size(); ++t) {
        const auto& a = left.tracks[t];
        const auto& b = right.tracks[t];
        if (a.clip != b.clip || a.events.size() != b.events.size()) return false;
        for (std::size_t e = 0; e < a.events.size(); ++e) {
            if (!SameSeconds(a.events[e].seconds, b.events[e].seconds) || a.events[e].name != b.events[e].name) {
                return false;
            }
        }
    }
    return true;
}

Result<std::string, ModelViewerError> CheckedText(const Model::AnimationEventSet& set, const Model::ModelAsset& model) {
    std::string text = WriteAnimationEvents(set);
    auto parsed = Model::ParseAnimationEvents(text);
    if (!parsed) return Engine::Base::Err(InvalidEvents(parsed.error()));
    if (auto bound = Model::BindAnimationEvents(parsed.value(), model); !bound) {
        return Engine::Base::Err(InvalidEvents(bound.error()));
    }
    Model::AnimationEventSet expected;
    expected.modelAssetId = set.modelAssetId;
    for (const auto* track : InNameOrder(set)) expected.tracks.push_back(*track);
    if (!SameEvents(parsed.value(), expected)) {
        // The writer lost something Engine still accepted: never save that.
        return Engine::Base::Err(ModelViewerError::Make(ModelViewerErrorCode::InvalidEvents,
                                                        "the written text does not read back as the events"));
    }
    return text;
}

EventDocument::EventDocument(Model::AnimationEventSet set) : set_(std::move(set)) {}

const Model::AnimationEventTrack* EventDocument::Track(const std::string_view clip) const noexcept {
    const auto found = std::find_if(set_.tracks.begin(), set_.tracks.end(),
                                    [&](const auto& track) { return track.clip == clip; });
    return found == set_.tracks.end() ? nullptr : &*found;
}

Model::AnimationEventTrack& EventDocument::MutableTrack(const std::string_view clip) {
    const auto found = std::find_if(set_.tracks.begin(), set_.tracks.end(),
                                    [&](const auto& track) { return track.clip == clip; });
    GYO_ASSERT(found != set_.tracks.end());
    return *found;
}

std::size_t EventDocument::Add(const std::string_view clip, Model::AnimationEvent event) {
    if (!Track(clip)) set_.tracks.push_back({std::string(clip), {}});
    auto& events = MutableTrack(clip).events;
    const auto at = std::upper_bound(events.begin(), events.end(), event.seconds,
                                     [](const double seconds, const auto& other) { return seconds < other.seconds; });
    const auto index = static_cast<std::size_t>(at - events.begin());
    events.insert(at, std::move(event));
    return index;
}

std::size_t EventDocument::Move(const std::string_view clip, const std::size_t index, const double seconds) {
    auto& events = MutableTrack(clip).events;
    GYO_ASSERT(index < events.size());
    Model::AnimationEvent event = std::move(events[index]);
    events.erase(events.begin() + static_cast<std::ptrdiff_t>(index));
    event.seconds = seconds;
    const auto at = std::upper_bound(events.begin(), events.end(), seconds,
                                     [](const double value, const auto& other) { return value < other.seconds; });
    const auto moved = static_cast<std::size_t>(at - events.begin());
    events.insert(at, std::move(event));
    return moved;
}

void EventDocument::Rename(const std::string_view clip, const std::size_t index, std::string name) {
    auto& events = MutableTrack(clip).events;
    GYO_ASSERT(index < events.size());
    events[index].name = std::move(name);
}

void EventDocument::Remove(const std::string_view clip, const std::size_t index) {
    auto& events = MutableTrack(clip).events;
    GYO_ASSERT(index < events.size());
    events.erase(events.begin() + static_cast<std::ptrdiff_t>(index));
    if (events.empty()) {
        // A copy: clip may view the name of the track being erased.
        const std::string emptied(clip);
        std::erase_if(set_.tracks, [&](const auto& track) { return track.clip == emptied; });
    }
}

} // namespace Gyo::Tools::ModelViewer
