#include <doctest/doctest.h>

#include "RetroFPS/App/WeaponEventSounds.hpp"
#include "engine/asset/loaders/SoundLoader.hpp"
#include "engine/model/AnimationEvents.hpp"
#include "gyo/framework/EventSounds.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// The product's weapon event sounds as the Client reads them: gyo.event_sounds
// through the framework, bound to the weapon's gyo.anim_events and to the real
// reload sound (v8-product-event-sounds-migrate, E1 to E3).

namespace {
using namespace fps;
namespace Framework = Gyo::Tools::Framework;

constexpr const char* ReloadSoundId = "object_fps_pvp.audio.gun_reload";

std::string ReadText(const char* path) {
    std::ifstream file(path, std::ios::binary);
    REQUIRE(file);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

std::string WeaponEventsId() {
    return nlohmann::json::parse(ReadText(PVP_WEAPON_PRESENTATION_PATH)).at("animation_events_asset_id").get<std::string>();
}

Engine::Model::AnimationEventSet ProductEvents() {
    auto events = Engine::Model::ParseAnimationEvents(ReadText(PVP_WEAPON_ANIM_EVENTS_PATH));
    if (!events) FAIL(Engine::Base::Describe(events.error()));
    return std::move(events).value();
}

WeaponEventSounds Weapon(const std::string& eventSoundsText) {
    std::string error;
    auto sounds = ParseWeaponEventSounds(eventSoundsText, WeaponEventsId(), error);
    if (!sounds) FAIL(error);
    return {ProductEvents(), std::move(*sounds)};
}

// The reload sound's length as Engine loads it.
double ReloadSoundSeconds() {
    const std::string bytes = ReadText(PVP_RELOAD_SOUND_PATH);
    Engine::Asset::Loaders::SoundLoader loader;
    Engine::Asset::Loading::LoadContext context;
    context.resolvedPath = PVP_RELOAD_SOUND_PATH;
    const auto loaded = loader.Load(
        Engine::Base::ConstSpan<std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()), context);
    REQUIRE(loaded.has_value());
    const auto* sound = loaded->As<Engine::Asset::Loaders::SoundAsset>();
    REQUIRE(sound != nullptr);
    REQUIRE(sound->channels > 0);
    REQUIRE(sound->sampleRate > 0);
    return static_cast<double>(sound->pcm16.size() / sound->channels) / sound->sampleRate;
}

std::optional<double> ProductLengths(const std::string_view id) {
    if (id == ReloadSoundId) return ReloadSoundSeconds();
    return std::nullopt;
}

std::optional<Framework::BoundEventSounds> Bind(const WeaponEventSounds& weapon, std::string& error) {
    return BindWeaponEventSounds(weapon, ProductLengths, error);
}
} // namespace

TEST_CASE("E1: the weapon's event sounds bind to its gyo.anim_events and the reload sound") {
    CHECK(nlohmann::json::parse(ReadText(PVP_WEAPON_PRESENTATION_PATH)).at("event_sounds_asset_id") ==
          "object_fps_pvp.event_sounds.nailer_v2.viewmodel");
    const auto weapon = Weapon(ReadText(PVP_WEAPON_EVENT_SOUNDS_PATH));
    CHECK(weapon.sounds.animationEventsAssetId == WeaponEventsId());
    std::string error;
    const auto bound = Bind(weapon, error);
    REQUIRE_MESSAGE(bound, error);
    const auto* magazine = bound->Find("magazine_in");
    REQUIRE(magazine);
    CHECK(magazine->soundAssetId == ReloadSoundId);
    CHECK(magazine->syncSeconds == 0.33);
    CHECK(magazine->gain == 0.8F);
    CHECK_FALSE(bound->Find("no_such_event"));
}

TEST_CASE("E2: the Client refuses event sounds that do not fit its weapon") {
    const auto product = nlohmann::json::parse(ReadText(PVP_WEAPON_EVENT_SOUNDS_PATH));
    std::string error;
    // Written for another anim_events asset: refused before binding.
    CHECK_FALSE(ParseWeaponEventSounds(product.dump(), "object_fps_pvp.anim_events.other", error));
    CHECK(error.find("object_fps_pvp.anim_events.other") != std::string::npos);
    // The framework's own refusal comes through.
    error.clear();
    auto badVersion = product;
    badVersion["version"] = 2;
    CHECK_FALSE(ParseWeaponEventSounds(badVersion.dump(), WeaponEventsId(), error));
    CHECK(error.find("InvalidEventSounds") != std::string::npos);

    const auto refusedBinding = [](const nlohmann::json& changed, const char* path) {
        std::string bindError;
        CHECK_FALSE(Bind(Weapon(changed.dump()), bindError));
        CHECK_MESSAGE(bindError.find(path) != std::string::npos, bindError);
    };
    auto unknownEvent = product;
    unknownEvent["events"]["no_such_event"] = product["events"]["magazine_in"];
    refusedBinding(unknownEvent, "no_such_event");
    auto beyondTheSound = product;
    beyondTheSound["events"]["magazine_in"]["sync_seconds"] = ReloadSoundSeconds() + 0.01;
    refusedBinding(beyondTheSound, "sync_seconds");
    auto unknownSound = product;
    unknownSound["events"]["magazine_in"]["sound_asset_id"] = "object_fps_pvp.audio.no_such_sound";
    refusedBinding(unknownSound, "sound_asset_id");
}

TEST_CASE("E3: the reload sound starts and sounds as before the migration") {
    // sfx.json version 2 played the reload cue (volume 0.8, centred) for
    // magazine_in with its sync point 0.33 s, at event x (action / clip) - sync
    // (v8-tool-assets-anim-events).
    constexpr double OldSync = 0.33;
    constexpr float OldVolume = 0.8F;
    std::string error;
    const auto bound = Bind(Weapon(ReadText(PVP_WEAPON_EVENT_SOUNDS_PATH)), error);
    REQUIRE_MESSAGE(bound, error);
    const auto* magazine = bound->Find("magazine_in");
    REQUIRE(magazine);
    CHECK(magazine->syncSeconds == OldSync);
    CHECK(magazine->gain == OldVolume);
    std::vector<double> moments;
    for (const auto& track : ProductEvents().tracks)
        for (const auto& event : track.events)
            if (event.name == "magazine_in") moments.push_back(event.seconds);
    REQUIRE_FALSE(moments.empty());
    CHECK_FALSE(ScheduleEventSound(*magazine, moments.front(), 0, 1.5, 0)); // a clip without length
    // The clip's length is the model's; several lengths and actions stand for it.
    const double lengths[][2] = {{2.9, 1.5}, {3.5, 1.5}, {2.4, 1.5}, {1.0, 3.0}};
    for (const double moment : moments) {
        for (const auto& [clip, action] : lengths) {
            CAPTURE(moment);
            CAPTURE(clip);
            const auto offset = Framework::EventSoundOffsetSeconds(moment, clip, action, magazine->syncSeconds);
            REQUIRE(offset);
            CHECK(*offset == doctest::Approx(moment * (action / clip) - OldSync));
            // As the Client schedules it: from now, for a reload that started 0.25 s ago, at the file's gain.
            const auto scheduled = ScheduleEventSound(*magazine, moment, clip, action, 0.25);
            REQUIRE(scheduled);
            CHECK(scheduled->secondsFromNow == doctest::Approx(moment * (action / clip) - OldSync - 0.25));
            CHECK(scheduled->gain == OldVolume);
        }
    }
}
