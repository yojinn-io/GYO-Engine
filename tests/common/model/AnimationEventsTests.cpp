#include <doctest/doctest.h>

#include "engine/model/AnimationEvents.hpp"

#include <string>

using namespace Engine::Model;

// Rules of gyo.anim_events version 1 (docs/architecture/animation-events.md).

namespace {
std::string Document(const std::string& clips, const std::string& head = R"("schema": "gyo.anim_events", "version": 1, "model_asset_id": "test.model")") {
    return "{" + head + ", \"clips\": " + clips + "}";
}

std::string Rejected(const std::string& text) {
    const auto parsed = ParseAnimationEvents(text);
    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().code == ModelErrorCode::InvalidAnimationEvents);
    return parsed.error().detail;
}

ModelAsset TwoClipModel() {
    ModelAsset model;
    model.nodes.push_back({"root", std::nullopt, {}});
    AnimationClip walk;
    walk.name = "walk";
    walk.durationSeconds = 2;
    AnimationClip reload;
    reload.name = "Armature|Reload";
    reload.durationSeconds = 1.25;
    model.clips = {walk, reload};
    return model;
}
} // namespace

TEST_CASE("gyo.anim_events reads a valid document, clips ordered by name") {
    const auto parsed = ParseAnimationEvents(Document(R"({
        "walk": [ { "seconds": 0, "event": "foot.left" }, { "seconds": 1, "event": "foot_right" } ],
        "Armature|Reload": [ { "seconds": 0.5, "event": "a" }, { "seconds": 0.5, "event": "b" }, { "seconds": 1.25, "event": "magazine_in2" } ]
    })"));
    REQUIRE(parsed);
    CHECK(parsed.value().modelAssetId == "test.model");
    REQUIRE(parsed.value().tracks.size() == 2);
    const auto& reload = parsed.value().tracks[0];
    CHECK(reload.clip == "Armature|Reload");
    REQUIRE(reload.events.size() == 3);
    CHECK(reload.events[0].seconds == 0.5);
    CHECK(reload.events[1].name == "b");
    CHECK(reload.events[2].name == "magazine_in2");
    CHECK(parsed.value().tracks[1].events[0].name == "foot.left");
}

TEST_CASE("gyo.anim_events rejects another schema or version") {
    const std::string clips = R"({ "c": [ { "seconds": 0, "event": "e" } ] })";
    CHECK(Rejected(Document(clips, R"("schema": "gyo.anim_event", "version": 1, "model_asset_id": "m")")) == "schema");
    CHECK(Rejected(Document(clips, R"("schema": "gyo.anim_events", "version": 2, "model_asset_id": "m")")) == "version");
    CHECK(Rejected(Document(clips, R"("schema": "gyo.anim_events", "version": "1", "model_asset_id": "m")")) == "version");
    CHECK(Rejected(Document(clips, R"("schema": "gyo.anim_events", "version": 1.5, "model_asset_id": "m")")) == "version");
    CHECK(Rejected(Document(clips, R"("schema": "gyo.anim_events", "version": 1, "model_asset_id": "")")) == "model_asset_id");
}

TEST_CASE("gyo.anim_events rejects missing, unknown and repeated fields") {
    Rejected(R"({ "schema": "gyo.anim_events", "version": 1, "clips": { "c": [ { "seconds": 0, "event": "e" } ] } })");
    Rejected(Document(R"({ "c": [ { "seconds": 0, "event": "e" } ] }, "loop": true)"));
    CHECK(Rejected(Document(R"({ "c": [ { "seconds": 0, "event": "e", "volume": 1 } ] })")) == "clips.\"c\"[0]");
    CHECK(Rejected(Document(R"({ "c": [ { "seconds": 0 } ] })")) == "clips.\"c\"[0]");
    Rejected(Document(R"({ "c": [ { "seconds": 0, "event": "e" } ], "c": [ { "seconds": 1, "event": "f" } ] })"));
    Rejected(Document(R"({ "c": [ { "seconds": 0, "event": "e", "seconds": 1 } ] })"));
    Rejected(R"({ "schema": "gyo.anim_events", "schema": "gyo.anim_events", "version": 1, "model_asset_id": "m", "clips": { "c": [ { "seconds": 0, "event": "e" } ] } })");
}

TEST_CASE("gyo.anim_events rejects documents that are not of its shape") {
    Rejected("{ not json");
    Rejected("[]");
    CHECK(Rejected(Document("{}")) == "clips");
    CHECK(Rejected(Document("[]")) == "clips");
    CHECK(Rejected(Document(R"({ "c": [] })")) == "clips.\"c\"");
    CHECK(Rejected(Document(R"({ "": [ { "seconds": 0, "event": "e" } ] })")) == "clips.\"\"");
    CHECK(Rejected(Document(R"({ "c": [ 1 ] })")) == "clips.\"c\"[0]");
}

TEST_CASE("gyo.anim_events seconds are finite, not negative and non-decreasing in a clip") {
    CHECK(Rejected(Document(R"({ "c": [ { "seconds": -0.001, "event": "e" } ] })")) == "clips.\"c\"[0].seconds");
    CHECK(Rejected(Document(R"({ "c": [ { "seconds": "0", "event": "e" } ] })")) == "clips.\"c\"[0].seconds");
    Rejected(Document(R"({ "c": [ { "seconds": 1e999, "event": "e" } ] })"));
    CHECK(Rejected(Document(R"({ "c": [ { "seconds": 1, "event": "e" }, { "seconds": 0.5, "event": "f" } ] })")) ==
          "clips.\"c\"[1].seconds");
}

TEST_CASE("gyo.anim_events rejects the same event twice at the same seconds") {
    CHECK(Rejected(Document(R"({ "c": [ { "seconds": 1, "event": "e" }, { "seconds": 1, "event": "f" }, { "seconds": 1, "event": "e" } ] })")) ==
          "clips.\"c\"[2]");
    CHECK(ParseAnimationEvents(Document(R"({ "c": [ { "seconds": 1, "event": "e" }, { "seconds": 2, "event": "e" } ] })")));
}

TEST_CASE("gyo.anim_events names are 1 to 64 characters of a-z, 0-9, '_' and '.'") {
    const auto withName = [](const std::string& name) {
        return Document(R"({ "c": [ { "seconds": 0, "event": ")" + name + R"(" } ] })");
    };
    CHECK(ParseAnimationEvents(withName("magazine_in.2")));
    CHECK(ParseAnimationEvents(withName(std::string(64, 'a'))));
    for (const auto* name : {"", "Magazine", "magazine in", "magazine-in", "é"})
        CHECK(Rejected(withName(name)) == "clips.\"c\"[0].event");
    CHECK(Rejected(withName(std::string(65, 'a'))) == "clips.\"c\"[0].event");
    CHECK(Rejected(Document(R"({ "c": [ { "seconds": 0, "event": 7 } ] })")) == "clips.\"c\"[0].event");
}

TEST_CASE("binding resolves clips by exact name, up to the clip's duration") {
    const auto model = TwoClipModel();
    const auto parsed = ParseAnimationEvents(Document(R"({ "Armature|Reload": [ { "seconds": 1.25, "event": "end" } ] })"));
    REQUIRE(parsed);
    const auto bound = BindAnimationEvents(parsed.value(), model);
    REQUIRE(bound);
    CHECK(bound.value().ForClip(0).empty());
    REQUIRE(bound.value().ForClip(1).size() == 1);
    CHECK(bound.value().ForClip(1)[0].name == "end");
    CHECK(bound.value().ForClip(2).empty());
}

TEST_CASE("binding fails as a whole for a missing clip or an event beyond its clip") {
    const auto model = TwoClipModel();
    const auto missing = ParseAnimationEvents(Document(R"({ "walk": [ { "seconds": 0, "event": "e" } ], "Reload": [ { "seconds": 0, "event": "e" } ] })"));
    REQUIRE(missing);
    const auto noClip = BindAnimationEvents(missing.value(), model);
    REQUIRE_FALSE(noClip);
    CHECK(noClip.error().code == ModelErrorCode::InvalidAnimationEvents);
    CHECK(noClip.error().detail == "clips.\"Reload\"");

    const auto beyond = ParseAnimationEvents(Document(R"({ "Armature|Reload": [ { "seconds": 0.5, "event": "e" }, { "seconds": 1.2501, "event": "f" } ] })"));
    REQUIRE(beyond);
    const auto late = BindAnimationEvents(beyond.value(), model);
    REQUIRE_FALSE(late);
    CHECK(late.error().detail == "clips.\"Armature|Reload\"[1].seconds");
}
