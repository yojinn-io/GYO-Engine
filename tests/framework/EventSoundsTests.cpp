// gyo.event_sounds version 1 (docs/framework/event-sounds.md): the rules of
// the core (CheckEventSoundSet, BindEventSounds, EventSoundOffsetSeconds) and
// of the JSON binding (ParseEventSounds), one test case per rule group
// (docs/architecture/plans/event-sounds/contract-declaration.md, ES0-ES9).
// Synthetic data only; no product's assets (AGENTS.md section 7).
#include <doctest/doctest.h>

#include "gyo/framework/EventSounds.hpp"
#include "gyo/framework/json/EventSoundsText.hpp"

#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace Gyo::Tools::Framework;

namespace {

constexpr const char* kHead = R"("schema": "gyo.event_sounds", "version": 1, "animation_events_asset_id": "test.anim_events")";

std::string Document(const std::string& events, const std::string& head = kHead) {
    return "{" + head + ", \"events\": " + events + "}";
}

std::string Entry(const std::string& sound = "test.audio.a", const std::string& sync = "0.25",
                  const std::string& gain = "0.8") {
    return R"({ "sound_asset_id": ")" + sound + R"(", "sync_seconds": )" + sync + R"(, "gain": )" + gain + " }";
}

struct Refusal final {
    EventSoundsErrorCode code{};
    std::string detail;
};

Refusal Rejected(const std::string& text) {
    const auto parsed = ParseEventSounds(text);
    REQUIRE_FALSE(parsed);
    return {parsed.error().code, parsed.error().detail};
}

// The path of a text rule's refusal; the code is always InvalidEventSounds.
std::string Invalid(const std::string& text) {
    const auto refusal = Rejected(text);
    CHECK(refusal.code == EventSoundsErrorCode::InvalidEventSounds);
    return refusal.detail;
}

std::string Invalid(const EventSoundSet& set) {
    const auto checked = CheckEventSoundSet(set);
    REQUIRE_FALSE(checked);
    CHECK(checked.error().code == EventSoundsErrorCode::InvalidEventSounds);
    return checked.error().detail;
}

// Two clips; "b" is only in the second, "shared" in both.
Engine::Model::AnimationEventSet Events() {
    Engine::Model::AnimationEventSet events;
    events.modelAssetId = "test.model";
    events.tracks.push_back({"clip_one", {{0.1, "a"}, {0.5, "shared"}}});
    events.tracks.push_back({"clip_two", {{0.2, "b"}, {0.9, "shared"}, {1.0, "unsounded"}}});
    return events;
}

SoundSeconds Lengths(std::map<std::string, double> lengths, std::vector<std::string>* asked = nullptr) {
    return [lengths = std::move(lengths), asked](const std::string_view id) -> std::optional<double> {
        if (asked) asked->emplace_back(id);
        const auto found = lengths.find(std::string(id));
        if (found == lengths.end()) return std::nullopt;
        return found->second;
    };
}

EventSoundSet Parsed(const std::string& text) {
    auto parsed = ParseEventSounds(text);
    REQUIRE(parsed);
    return std::move(parsed).value();
}

double Offset(const double event, const double clip, const double action, const double sync) {
    const auto offset = EventSoundOffsetSeconds(event, clip, action, sync);
    REQUIRE(offset.has_value());
    return *offset;
}

Refusal Unbound(const EventSoundSet& set, const SoundSeconds& lengths) {
    const auto bound = BindEventSounds(set, Events(), lengths);
    REQUIRE_FALSE(bound);
    return {bound.error().code, bound.error().detail};
}

} // namespace

TEST_CASE("ES0: a valid document reads and binds, one event or several") {
    // One sound per action (the format's shape) and one stitched clip on one event (its special case).
    const auto several = Parsed(Document(R"({
        "shared": { "sound_asset_id": "test.audio.shared", "sync_seconds": 0, "gain": 0 },
        "a":      { "sound_asset_id": "test.audio.a", "sync_seconds": 0.25, "gain": 0.8 },
        "b":      { "sound_asset_id": "test.audio.b", "sync_seconds": 1.5, "gain": 8 }
    })"));
    CHECK(several.animationEventsAssetId == "test.anim_events");
    REQUIRE(several.sounds.size() == 3);
    CHECK(several.sounds[0].event == "a");
    CHECK(several.sounds[0].soundAssetId == "test.audio.a");
    CHECK(several.sounds[0].syncSeconds == 0.25);
    CHECK(several.sounds[0].gain == 0.8F);
    CHECK(several.sounds[1].event == "b");
    CHECK(several.sounds[1].syncSeconds == 1.5);
    CHECK(several.sounds[1].gain == 8.0F);
    CHECK(several.sounds[2].event == "shared");
    CHECK(several.sounds[2].gain == 0.0F);

    const auto bound = BindEventSounds(several, Events(),
        Lengths({{"test.audio.a", 1.0}, {"test.audio.b", 2.0}, {"test.audio.shared", 0.5}}));
    REQUIRE(bound);
    REQUIRE(bound->Find("b") != nullptr);
    CHECK(bound->Find("b")->soundAssetId == "test.audio.b");
    CHECK(bound->Find("shared")->syncSeconds == 0);
    CHECK(bound->Find("a")->gain == 0.8F);
    CHECK(bound->Find("unsounded") == nullptr); // an event without an entry is silent
    CHECK(bound->Find("missing") == nullptr);
    CHECK(bound->Find("") == nullptr);

    const auto one = Parsed(Document("{ \"shared\": " + Entry("test.audio.stitched", "0.33", "1") + " }"));
    REQUIRE(one.sounds.size() == 1);
    const auto boundOne = BindEventSounds(one, Events(), Lengths({{"test.audio.stitched", 3.0}}));
    REQUIRE(boundOne);
    CHECK(boundOne->Find("shared")->soundAssetId == "test.audio.stitched");
}

TEST_CASE("ES1: fields are exact and no key repeats") {
    const std::string events = "{ \"a\": " + Entry() + " }";
    CHECK(Invalid(Document(events, std::string(kHead) + R"(, "extra": 1)")) == "extra");
    CHECK(Invalid(R"({"schema": "gyo.event_sounds", "version": 1, "events": )" + events + "}") == "");
    CHECK(Invalid(Document(R"({ "a": { "sound_asset_id": "s", "sync_seconds": 0, "gain": 1, "pitch": 1 } })")) ==
          "events.\"a\".pitch");
    CHECK(Invalid(Document(R"({ "a": { "sound_asset_id": "s", "sync_seconds": 0 } })")) == "events.\"a\"");
    CHECK(Invalid(Document(events, std::string(kHead) + R"(, "version": 1)")) == "version");
    CHECK(Invalid(Document("{ \"a\": " + Entry() + ", \"a\": " + Entry("test.audio.b") + " }")) == "events.\"a\"");
    CHECK(Invalid(Document(R"({ "a": { "sound_asset_id": "s", "sync_seconds": 0, "gain": 1, "gain": 2 } })")) ==
          "events.\"a\".gain");
    CHECK(Invalid("[]") == "");
    CHECK(Invalid("{ not json") == "");
}

TEST_CASE("ES2: schema and version") {
    const std::string events = "{ \"a\": " + Entry() + " }";
    const auto head = [](const std::string& schema, const std::string& version) {
        return "\"schema\": " + schema + ", \"version\": " + version + R"(, "animation_events_asset_id": "x")";
    };
    CHECK(Invalid(Document(events, head(R"("gyo.event_sound")", "1"))) == "schema");
    CHECK(Invalid(Document(events, head(R"("gyo.anim_events")", "1"))) == "schema");
    CHECK(Invalid(Document(events, head(R"("gyo.event_sounds")", "2"))) == "version");
    CHECK(Invalid(Document(events, head(R"("gyo.event_sounds")", R"("1")"))) == "version");
    CHECK(Invalid(Document(events, head(R"("gyo.event_sounds")", "1.5"))) == "version");
    CHECK(Invalid(Document(events, head(R"("gyo.event_sounds")", "1.0"))) == "version");
    CHECK(ParseEventSounds(Document(events, head(R"("gyo.event_sounds")", "1"))));
}

TEST_CASE("ES3: animation_events_asset_id is a nonempty string") {
    const std::string events = "{ \"a\": " + Entry() + " }";
    const std::string head = R"("schema": "gyo.event_sounds", "version": 1, "animation_events_asset_id": )";
    CHECK(Invalid(Document(events, head + R"("")")) == "animation_events_asset_id");
    CHECK(Invalid(Document(events, head + "7")) == "animation_events_asset_id");
}

TEST_CASE("ES4: events is a nonempty object of objects with nonempty names") {
    CHECK(Invalid(Document("{}")) == "events");
    CHECK(Invalid(Document("[]")) == "events");
    CHECK(Invalid(Document(R"("a")")) == "events");
    CHECK(Invalid(Document("{ \"\": " + Entry() + " }")) == "events.\"\"");
    CHECK(Invalid(Document(R"({ "a": 1 })")) == "events.\"a\"");
    CHECK(Invalid(Document(R"({ "a": [] })")) == "events.\"a\"");
    // The characters of a name are Engine's rule, met through binding (ES7, D-es-1).
    CHECK(ParseEventSounds(Document("{ \"Some Event\": " + Entry() + " }")));
    // Structure: one entry per name, in ascending order.
    EventSoundSet set{"test.anim_events", {{"b", "s", 0, 1}, {"a", "s", 0, 1}}};
    CHECK(Invalid(set) == "events.\"a\"");
    set.sounds = {{"a", "s", 0, 1}, {"a", "t", 0, 1}};
    CHECK(Invalid(set) == "events.\"a\"");
}

TEST_CASE("ES5: sound_asset_id is a nonempty string") {
    CHECK(Invalid(Document("{ \"a\": " + Entry("") + " }")) == "events.\"a\".sound_asset_id");
    CHECK(Invalid(Document(R"({ "a": { "sound_asset_id": 3, "sync_seconds": 0, "gain": 1 } })")) ==
          "events.\"a\".sound_asset_id");
}

TEST_CASE("ES6: sync_seconds is finite and not negative and gain is Engine's") {
    CHECK(Invalid(Document("{ \"a\": " + Entry("s", "-0.01") + " }")) == "events.\"a\".sync_seconds");
    CHECK(Invalid(Document("{ \"a\": " + Entry("s", "\"0\"") + " }")) == "events.\"a\".sync_seconds");
    CHECK(ParseEventSounds(Document("{ \"a\": " + Entry("s", "0") + " }")));
    CHECK(Invalid(Document("{ \"a\": " + Entry("s", "0", "-0.1") + " }")) == "events.\"a\".gain");
    CHECK(Invalid(Document("{ \"a\": " + Entry("s", "0", "8.0001") + " }")) == "events.\"a\".gain");
    CHECK(Invalid(Document("{ \"a\": " + Entry("s", "0", "1e39") + " }")) == "events.\"a\".gain");
    CHECK(Invalid(Document("{ \"a\": " + Entry("s", "0", "\"1\"") + " }")) == "events.\"a\".gain");
    CHECK(ParseEventSounds(Document("{ \"a\": " + Entry("s", "0", "0") + " }")));
    CHECK(ParseEventSounds(Document("{ \"a\": " + Entry("s", "0", "8") + " }")));
    // Structure, where JSON cannot reach: non-finite values.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    CHECK(Invalid(EventSoundSet{"x", {{"a", "s", nan, 1}}}) == "events.\"a\".sync_seconds");
    CHECK(Invalid(EventSoundSet{"x", {{"a", "s", inf, 1}}}) == "events.\"a\".sync_seconds");
    CHECK(Invalid(EventSoundSet{"x", {{"a", "s", 0, std::numeric_limits<float>::quiet_NaN()}}}) == "events.\"a\".gain");
    CHECK(Invalid(EventSoundSet{"", {{"a", "s", 0, 1}}}) == "animation_events_asset_id");
}

TEST_CASE("ES7: binding needs every name in some clip of the anim_events set") {
    const auto lengths = Lengths({{"s", 1.0}});
    CHECK(BindEventSounds(EventSoundSet{"x", {{"b", "s", 0, 1}}}, Events(), lengths)); // only in the second clip
    const auto missing = Unbound(EventSoundSet{"x", {{"a", "s", 0, 1}, {"c", "s", 0, 1}}}, lengths);
    CHECK(missing.code == EventSoundsErrorCode::UnboundEventSounds);
    CHECK(missing.detail == "events.\"c\"");
    const auto upper = Unbound(EventSoundSet{"x", {{"B", "s", 0, 1}}}, lengths);
    CHECK(upper.code == EventSoundsErrorCode::UnboundEventSounds);
    CHECK(upper.detail == "events.\"B\"");
    CHECK(Unbound(Parsed(Document("{ \"Some Event\": " + Entry("s", "0") + " }")), lengths).detail ==
          "events.\"Some Event\"");
    // A set the rules refuse gives that refusal, not a binding error.
    const auto invalid = Unbound(EventSoundSet{"x", {}}, lengths);
    CHECK(invalid.code == EventSoundsErrorCode::InvalidEventSounds);
}

TEST_CASE("ES8: binding needs every sound's length and a sync point within it, as a whole") {
    std::vector<std::string> asked;
    CHECK(BindEventSounds(EventSoundSet{"x", {{"a", "test.audio.one", 1.0, 1}}}, Events(),
                          Lengths({{"test.audio.one", 1.0}}, &asked)));
    CHECK(asked == std::vector<std::string>{"test.audio.one"}); // asked by the entry's sound asset id
    const auto beyond = Unbound(EventSoundSet{"x", {{"a", "s", 1.0001, 1}}}, Lengths({{"s", 1.0}}));
    CHECK(beyond.code == EventSoundsErrorCode::UnboundEventSounds);
    CHECK(beyond.detail == "events.\"a\".sync_seconds");
    const auto unanswered = Unbound(EventSoundSet{"x", {{"a", "s", 0, 1}}}, Lengths({}));
    CHECK(unanswered.code == EventSoundsErrorCode::UnboundEventSounds);
    CHECK(unanswered.detail == "events.\"a\".sound_asset_id");
    CHECK(Unbound(EventSoundSet{"x", {{"a", "s", 0, 1}}}, Lengths({{"s", std::nan("")}})).detail ==
          "events.\"a\".sound_asset_id");
    CHECK(Unbound(EventSoundSet{"x", {{"a", "s", 0, 1}}}, Lengths({{"s", -1}})).detail ==
          "events.\"a\".sound_asset_id");
    CHECK(Unbound(EventSoundSet{"x", {{"a", "s", 0, 1}}}, SoundSeconds{}).detail == "events.\"a\".sound_asset_id");
    // One entry fails, so nothing is bound.
    const auto partial = BindEventSounds(EventSoundSet{"x", {{"a", "good", 0, 1}, {"b", "bad", 0, 1}}}, Events(),
                                         Lengths({{"good", 1.0}}));
    REQUIRE_FALSE(partial);
    CHECK(partial.error().detail == "events.\"b\".sound_asset_id");
}

TEST_CASE("ES9: when an event's sound starts") {
    CHECK(Offset(2, 3, 1.5, 0) == doctest::Approx(1));
    CHECK(Offset(2, 3, 1.5, 0.25) == doctest::Approx(0.75));
    CHECK(Offset(2, 1, 4, 0) == doctest::Approx(8)); // a slowed clip
    CHECK(Offset(0.1, 1, 1, 0.3) == doctest::Approx(-0.2)); // the sound leads the action
    CHECK_FALSE(EventSoundOffsetSeconds(1, 0, 1.5, 0));
    CHECK_FALSE(EventSoundOffsetSeconds(1, -3, 1.5, 0));
    CHECK_FALSE(EventSoundOffsetSeconds(1, 3, 0, 0));
    CHECK_FALSE(EventSoundOffsetSeconds(1, 3, -1.5, 0));
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    CHECK_FALSE(EventSoundOffsetSeconds(nan, 3, 1.5, 0));
    CHECK_FALSE(EventSoundOffsetSeconds(1, inf, 1.5, 0));
    CHECK_FALSE(EventSoundOffsetSeconds(1, 3, inf, 0));
    CHECK_FALSE(EventSoundOffsetSeconds(1, 3, 1.5, inf));
    CHECK_FALSE(EventSoundOffsetSeconds(inf, 3, 1.5, 0));
    CHECK_FALSE(EventSoundOffsetSeconds(1, nan, 1.5, 0));
    CHECK_FALSE(EventSoundOffsetSeconds(1, 3, nan, 0));
    CHECK_FALSE(EventSoundOffsetSeconds(1, 3, 1.5, nan));
    // Finite arguments whose result overflows (user, 2026-11-04: tightened from PvP's version).
    CHECK_FALSE(EventSoundOffsetSeconds(1e300, 1e-10, 1e10, 0));
    CHECK_FALSE(EventSoundOffsetSeconds(1, 1e-300, 1e300, 0));
}
