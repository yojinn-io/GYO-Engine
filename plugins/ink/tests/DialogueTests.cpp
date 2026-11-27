// The ink Plugin's dialogue, from outside (docs/architecture/target/v2/ink.zh-Hant.md):
// lines, choices and commands; the errors as Results; the seed; save, restore
// and migration; the thread contract; the "C" locale. The stories are
// tests/data/*.ink, compiled to ink.json by inklecate and committed.
// Each CHECK carries a tag in brackets ("[ink-...]") for the mutants.

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "Render.hpp"
#include "adapters/inkcpp/CompileStory.hpp"
#include "plugins/ink/Dialogue.hpp"

// In the Plugin's namespace: a Plugin's tests live in its directory (naming rule N17).
namespace GYO::Plugins::Ink {
namespace {

namespace I = ::GYO::Plugins::Ink;
using Test::CompileStory;
using Test::Render;

// The error of a failed Result, for a message; empty for a value.
template <class R>
std::string Why(const R& result) {
    return result ? std::string() : ::GYO::Engine::Base::Describe(result.error());
}

I::DialogueOptions MerchantOptions(const std::uint32_t seed) {
    I::DialogueOptions options;
    options.seed = seed;
    options.commands = {"give_item", "play_sound"};
    return options;
}

I::Story Load(const std::string& name) {
    auto story = I::LoadStory(CompileStory(name));
    REQUIRE_MESSAGE(story, name << ": " << Why(story));
    return std::move(story).value();
}

I::Dialogue Start(const I::Story& story, I::DialogueOptions options) {
    auto dialogue = I::Dialogue::Start(story, std::move(options));
    REQUIRE_MESSAGE(dialogue, Why(dialogue));
    return std::move(dialogue).value();
}

I::Beat Advance(I::Dialogue& dialogue) {
    auto beat = dialogue.Advance();
    REQUIRE_MESSAGE(beat, Why(beat));
    return std::move(beat).value();
}

void Choose(I::Dialogue& dialogue, const std::size_t index) {
    auto chosen = dialogue.Choose(index);
    REQUIRE_MESSAGE(chosen, Why(chosen));
}

I::Saved TakeSave(const I::Dialogue& dialogue) {
    auto saved = dialogue.Save();
    REQUIRE_MESSAGE(saved, Why(saved));
    return std::move(saved).value();
}

std::vector<std::byte> Save(const I::Dialogue& dialogue) {
    return TakeSave(dialogue).bytes;
}

// Advances, then takes each pick in turn; the beats rendered one after another.
std::string Play(I::Dialogue& dialogue, const std::vector<std::size_t>& picks) {
    std::string out = Render(Advance(dialogue));
    for (const std::size_t pick : picks) {
        Choose(dialogue, pick);
        out += "--\n" + Render(Advance(dialogue));
    }
    return out;
}

std::string ChoiceTexts(const I::Beat& beat) {
    std::string out;
    for (const I::Choice& choice : beat.choices) out += (out.empty() ? "" : " | ") + choice.text;
    return out;
}

// The hub's first visit with seed 0.
const std::string kHubSeed0 =
    "merchant: Welcome to my stall. [hub_welcome]\n"
    "merchant: Cold wind tonight. [hub_weather]\n"
    "merchant: Today's lucky number is 31. [hub_lucky]\n"
    "* Buy a sword [hub_buy]\n"
    "* Haggle [hub_haggle]\n"
    "* Weigh the coins [hub_weigh]\n"
    "* Leave [hub_leave]\n";

// Seed 0: buy the sword, then leave.
const std::string kBuyThenLeaveSeed0 = kHubSeed0 +
    "--\n"
    "> give_item(\"sword\", 2)\n"
    "merchant: Here you go. [hub_sold]\n"
    "> play_sound(3)\n"
    "merchant: Welcome to my stall. [hub_welcome]\n"
    "merchant: Fine weather today. [hub_weather]\n"
    "merchant: Today's lucky number is 52. [hub_lucky]\n"
    "* Buy a sword [hub_buy]\n"
    "* Haggle [hub_haggle]\n"
    "* Weigh the coins [hub_weigh]\n"
    "* Leave [hub_leave]\n"
    "--\n"
    "> play_sound(7)\n"
    "merchant: Farewell. [hub_farewell] #mood:warm\n"
    "(end)\n";

// ---- Lines, choices, commands

TEST_CASE("lines carry their text, speaker, line id and other tags; choices their text and line id") {
    const I::Story story = Load("merchant.ink.json");
    I::Dialogue dialogue = Start(story, MerchantOptions(0));
    CHECK_MESSAGE(Render(Advance(dialogue)) == kHubSeed0, "[ink-lines]");
}

TEST_CASE("Advance while the story waits for a choice gives no outputs and the same choices") {
    const I::Story story = Load("merchant.ink.json");
    I::Dialogue dialogue = Start(story, MerchantOptions(0));
    const I::Beat first = Advance(dialogue);
    const I::Beat again = Advance(dialogue);
    CHECK_MESSAGE(again.outputs.empty(), "[ink-waiting]");
    CHECK_MESSAGE(ChoiceTexts(again) == ChoiceTexts(first), "[ink-waiting]");
    CHECK_MESSAGE(!again.ended, "[ink-waiting]");
}

TEST_CASE("commands come in the order the story ran them, with their arguments in the story's order") {
    const I::Story story = Load("merchant.ink.json");
    I::Dialogue dialogue = Start(story, MerchantOptions(0));
    CHECK_MESSAGE(Play(dialogue, {0, 3}) == kBuyThenLeaveSeed0, "[ink-commands]");
}

TEST_CASE("the end: ended, no choices; Advance after it gives nothing more") {
    const I::Story story = Load("merchant.ink.json");
    I::Dialogue dialogue = Start(story, MerchantOptions(0));
    Play(dialogue, {3});
    const I::Beat after = Advance(dialogue);
    CHECK_MESSAGE((after.ended && after.choices.empty() && after.outputs.empty()), "[ink-end]");
}

TEST_CASE("every line and choice of the merchant story carries a line id (the writing convention, checked on the story)") {
    const I::Story story = Load("merchant.ink.json");
    // Buy, weigh, haggle and insist, leave: every branch of merchant.ink.
    I::Dialogue dialogue = Start(story, MerchantOptions(0));
    std::vector<I::Beat> beats{Advance(dialogue)};
    for (const std::size_t pick : {0U, 2U, 1U, 0U, 3U}) {
        Choose(dialogue, pick);
        beats.push_back(Advance(dialogue));
    }
    REQUIRE(beats.back().ended);
    for (const I::Beat& beat : beats) {
        for (const I::Output& output : beat.outputs) {
            if (const auto* line = std::get_if<I::Line>(&output)) CHECK_MESSAGE(!line->lineId.empty(), line->text);
        }
        for (const I::Choice& choice : beat.choices) CHECK_MESSAGE(!choice.lineId.empty(), choice.text);
    }
}

TEST_CASE("a line without a line id is no error: its lineId is empty") {
    const I::Story story = Load("faults.ink.json");
    I::DialogueOptions options;
    options.lineIdTag = "other"; // no tag of the story has this prefix
    I::Dialogue dialogue = Start(story, options);
    CHECK_MESSAGE(Render(Advance(dialogue)) ==
                      "Before the fault. #line:fault_before\n"
                      "* Call the unbound command #line:fault_unbound\n"
                      "* Paint with a list #line:fault_list\n",
                  "[ink-no-line-id]");
}

// inkcpp d38608d (runner_impl.cpp, Command::CHOICE): a once-only choice already
// taken, or one whose condition is false, is skipped before its tags are
// assigned, so they go to the next choice. Not patched (the spike does not
// patch inkcpp); this case shows it, and fails when an inkcpp update fixes it.
TEST_CASE("inkcpp defect, not patched: the tags of a skipped once-only choice go to the next choice") {
    const I::Story story = Load("once.ink.json");
    I::Dialogue dialogue = Start(story, I::DialogueOptions{});
    Advance(dialogue);
    Choose(dialogue, 0); // the once-only choice is taken
    const I::Beat back = Advance(dialogue);
    REQUIRE(back.choices.size() == 2);
    CHECK(back.choices[0].text == "Again");
    CHECK(back.choices[0].lineId == "once_once");
    CHECK(back.choices[0].tags == std::vector<std::string>{"line:once_again"});
}

// ---- Errors as Results (acceptance 7.2)

TEST_CASE("bytes that are not a compiled story: NotAStory") {
    const std::vector<std::byte> compiled = CompileStory("merchant.ink.json");
    const std::vector<std::byte> text{std::byte{'n'}, std::byte{'o'}, std::byte{'t'}, std::byte{' '},
                                      std::byte{'a'}, std::byte{' '}, std::byte{'s'}, std::byte{'t'}};
    const std::vector<std::byte> cut(compiled.begin(), compiled.begin() + static_cast<std::ptrdiff_t>(compiled.size() / 2));
    // Shorter than inkcpp's header, with the right magic and version at the start.
    const std::vector<std::byte> headOnly(compiled.begin(), compiled.begin() + 16);
    for (const std::vector<std::byte>& bytes : {std::vector<std::byte>{}, text, cut, headOnly}) {
        auto story = I::LoadStory(bytes);
        REQUIRE_MESSAGE(!story, "[ink-not-a-story] " << bytes.size() << " bytes loaded");
        CHECK_MESSAGE(story.error().code == I::DialogueErrorCode::NotAStory, "[ink-not-a-story]");
    }
}

TEST_CASE("a command the dialogue does not bind, with no fallback: ScriptFault, then Stopped") {
    const I::Story story = Load("faults.ink.json");
    I::Dialogue dialogue = Start(story, I::DialogueOptions{});
    Advance(dialogue);
    Choose(dialogue, 0);
    auto fault = dialogue.Advance();
    REQUIRE_MESSAGE(!fault, "[ink-script-fault]");
    CHECK_MESSAGE(fault.error().code == I::DialogueErrorCode::ScriptFault, "[ink-script-fault]");
    CHECK_MESSAGE(!fault.error().detail.empty(), "[ink-script-fault] inkcpp's message is carried");

    auto again = dialogue.Advance();
    REQUIRE_MESSAGE(!again, "[ink-stopped]");
    CHECK_MESSAGE(again.error().code == I::DialogueErrorCode::Stopped, "[ink-stopped]");
    auto choose = dialogue.Choose(0);
    REQUIRE_MESSAGE(!choose, "[ink-stopped]");
    CHECK_MESSAGE(choose.error().code == I::DialogueErrorCode::Stopped, "[ink-stopped]");
    auto save = dialogue.Save();
    REQUIRE_MESSAGE(!save, "[ink-stopped]");
    CHECK_MESSAGE(save.error().code == I::DialogueErrorCode::Stopped, "[ink-stopped]");
}

TEST_CASE("a command given a list: ScriptFault") {
    const I::Story story = Load("faults.ink.json");
    I::DialogueOptions options;
    options.commands = {"paint"};
    I::Dialogue dialogue = Start(story, options);
    Advance(dialogue);
    Choose(dialogue, 1);
    auto fault = dialogue.Advance();
    REQUIRE_MESSAGE(!fault, "[ink-list-argument]");
    CHECK_MESSAGE(fault.error().code == I::DialogueErrorCode::ScriptFault, "[ink-list-argument]");
}

TEST_CASE("a choice index past the choices: ChoiceOutOfRange, and the dialogue goes on") {
    const I::Story story = Load("merchant.ink.json");
    I::Dialogue dialogue = Start(story, MerchantOptions(0));
    Advance(dialogue);
    auto chosen = dialogue.Choose(4);
    REQUIRE_MESSAGE(!chosen, "[ink-choice-range]");
    CHECK_MESSAGE(chosen.error().code == I::DialogueErrorCode::ChoiceOutOfRange, "[ink-choice-range]");
    CHECK_MESSAGE(chosen.error().detail == "index 4, 4 choices", "[ink-choice-range]");
    Choose(dialogue, 3);
    CHECK_MESSAGE(Advance(dialogue).ended, "[ink-choice-range] the dialogue went on");
}

TEST_CASE("bytes that are not a dialogue's save: SnapshotUnreadable") {
    const I::Story story = Load("merchant.ink.json");
    I::Dialogue dialogue = Start(story, MerchantOptions(0));
    Advance(dialogue);
    Choose(dialogue, 0); // a save point this inkcpp restores (the owner page, section 8)
    const std::vector<std::byte> saved = Save(dialogue);
    REQUIRE_MESSAGE(I::Dialogue::Restore(story, saved, MerchantOptions(0)), "[ink-save-unreadable] the save itself");

    std::vector<std::byte> otherVersion = saved;
    otherVersion[0] = std::byte{2};
    const std::vector<std::byte> cut(saved.begin(), saved.begin() + static_cast<std::ptrdiff_t>(saved.size() / 2));
    const std::vector<std::byte> headerOnly(saved.begin(), saved.begin() + 8);
    const std::vector<std::byte> notASave(64, std::byte{0x5A});
    for (const std::vector<std::byte>& bytes : {std::vector<std::byte>{}, notASave, otherVersion, cut, headerOnly}) {
        auto restored = I::Dialogue::Restore(story, bytes, MerchantOptions(0));
        REQUIRE_MESSAGE(!restored, "[ink-save-unreadable] " << bytes.size() << " bytes restored");
        CHECK_MESSAGE(restored.error().code == I::DialogueErrorCode::SnapshotUnreadable, "[ink-save-unreadable]");
    }
}

// ---- The "C" locale (acceptance 7.5)

TEST_CASE("numbers in the text are written with a point (the process is in the \"C\" locale)") {
    const I::Story story = Load("merchant.ink.json");
    I::Dialogue dialogue = Start(story, MerchantOptions(0));
    Advance(dialogue);
    Choose(dialogue, 2);
    const I::Beat weighed = Advance(dialogue);
    REQUIRE(!weighed.outputs.empty());
    const auto* line = std::get_if<I::Line>(&weighed.outputs.front());
    REQUIRE(line != nullptr);
    CHECK_MESSAGE(line->text == "The scale reads 1.5 and 0.25.", "[ink-locale]");
}

// ---- The seed (acceptance 7.1)

TEST_CASE("the same seed gives the same dialogue; another seed another one; neither is the clock") {
    const I::Story story = Load("merchant.ink.json");
    I::Dialogue first = Start(story, MerchantOptions(0));
    I::Dialogue second = Start(story, MerchantOptions(0));
    const std::string a = Play(first, {0, 3});
    CHECK_MESSAGE(a == Play(second, {0, 3}), "[ink-seed]");
    CHECK_MESSAGE(a == kBuyThenLeaveSeed0, "[ink-seed] the pinned text of seed 0, the same on every platform");
    I::Dialogue other = Start(story, MerchantOptions(1));
    CHECK_MESSAGE(Render(Advance(other)) ==
                      "merchant: Welcome to my stall. [hub_welcome]\n"
                      "merchant: Fine weather today. [hub_weather]\n"
                      "merchant: Today's lucky number is 31. [hub_lucky]\n"
                      "* Buy a sword [hub_buy]\n"
                      "* Haggle [hub_haggle]\n"
                      "* Weigh the coins [hub_weigh]\n"
                      "* Leave [hub_leave]\n",
                  "[ink-seed] the pinned text of seed 1");
}

// ---- Save and restore (acceptance 7.3)

TEST_CASE("a save taken after a choice restores the same dialogue: the same text, random numbers and visits") {
    const I::Story story = Load("merchant.ink.json");
    I::Dialogue straight = Start(story, MerchantOptions(0));
    Advance(straight);
    Choose(straight, 0);
    const std::vector<std::byte> saved = Save(straight);
    const std::string expected = Play(straight, {3});

    // Another seed in the options: the random state comes from the save.
    auto restored = I::Dialogue::Restore(story, saved, MerchantOptions(99));
    REQUIRE_MESSAGE(restored, "[ink-restore] " << Why(restored));
    CHECK_MESSAGE(!restored->migrated, "[ink-restore-migrated]");
    CHECK_MESSAGE(Play(restored->dialogue, {3}) == expected, "[ink-restore]");
}

TEST_CASE("a save restored into the story's next version: migrated, old globals kept, new ones at their defaults") {
    const I::Story story = Load("merchant.ink.json");
    const I::Story next = Load("merchant_v2.ink.json");
    I::Dialogue dialogue = Start(story, MerchantOptions(0));
    Advance(dialogue);
    Choose(dialogue, 0); // buy: gold 10 -> 5
    Advance(dialogue);
    Choose(dialogue, 2); // weigh the coins
    const std::vector<std::byte> saved = Save(dialogue);

    auto restored = I::Dialogue::Restore(next, saved, MerchantOptions(0));
    REQUIRE_MESSAGE(restored, "[ink-migrate] " << Why(restored));
    CHECK_MESSAGE(restored->migrated, "[ink-migrate-flag]");
    // gold is 5 (kept; the new version's default is 10), reputation 3 (new, its default).
    CHECK_MESSAGE(Render(Advance(restored->dialogue)) ==
                      "merchant: The scale reads 1.5 and 0.25. [hub_scale]\n"
                      "merchant: Welcome to my stall. [hub_welcome]\n"
                      "merchant: You carry 5 gold, reputation 3. [hub_purse]\n"
                      "merchant: Cold wind tonight. [hub_weather]\n"
                      "merchant: Today's lucky number is 37. [hub_lucky]\n"
                      "* Buy a sword [hub_buy]\n"
                      "* Haggle [hub_haggle]\n"
                      "* Weigh the coins [hub_weigh]\n"
                      "* Leave [hub_leave]\n",
                  "[ink-migrate]");
}

TEST_CASE("a save taken waiting for a choice (inside a tunnel) does not move to the next version: SnapshotNotMigratable") {
    const I::Story story = Load("merchant.ink.json");
    const I::Story next = Load("merchant_v2.ink.json");
    I::Dialogue dialogue = Start(story, MerchantOptions(0));
    Advance(dialogue);
    Choose(dialogue, 1); // haggle: into the tunnel
    REQUIRE(ChoiceTexts(Advance(dialogue)) == "Insist");
    const std::vector<std::byte> saved = Save(dialogue);
    auto restored = I::Dialogue::Restore(next, saved, MerchantOptions(0));
    REQUIRE_MESSAGE(!restored, "[ink-not-migratable]");
    CHECK_MESSAGE(restored.error().code == I::DialogueErrorCode::SnapshotNotMigratable, "[ink-not-migratable]");
}

TEST_CASE("a save restored into a version without the place it was taken in: SnapshotNotMigratable") {
    const I::Story story = Load("merchant.ink.json");
    I::Dialogue dialogue = Start(story, MerchantOptions(0));
    Advance(dialogue);
    Choose(dialogue, 2); // weigh the coins: a save point inkcpp can migrate
    const std::vector<std::byte> saved = Save(dialogue);
    // v3 renames the knot hub; v4 keeps hub but drops the weigh choice the save is in.
    for (const char* name : {"merchant_v3.ink.json", "merchant_v4.ink.json"}) {
        const I::Story next = Load(name);
        for (const std::vector<std::string>& commands : {std::vector<std::string>{}, MerchantOptions(0).commands}) {
            I::DialogueOptions options;
            options.commands = commands;
            auto restored = I::Dialogue::Restore(next, saved, options);
            REQUIRE_MESSAGE(!restored, "[ink-place-gone] " << std::string(name) << " restored with " << commands.size() << " commands");
            CHECK_MESSAGE(restored.error().code == I::DialogueErrorCode::SnapshotNotMigratable,
                          "[ink-place-gone] " << std::string(name) << ": " << ::GYO::Engine::Base::Describe(restored.error()));
        }
    }
}

// inkcpp d38608d defects in its snapshots, not patched (the spike does not
// patch inkcpp). Each case shows one and fails when an inkcpp update fixes it.

// globals_impl.cpp snap_load: a save that cannot migrate (one taken while the
// story waits for a choice) needs every visit-counted container to have a
// named hash, and a sequence ({a|b}, {&a|b}, {!a|b}, {~a|b}) anywhere in the
// story has none.
TEST_CASE("inkcpp defect, not patched: a save taken at a choice cannot be restored, even into the same story, when the story has a sequence") {
    const I::Story story = Load("merchant.ink.json");
    I::Dialogue dialogue = Start(story, MerchantOptions(0));
    Advance(dialogue);
    const std::vector<std::byte> saved = Save(dialogue);
    auto restored = I::Dialogue::Restore(story, saved, MerchantOptions(0));
    REQUIRE(!restored);
    CHECK(restored.error().code == I::DialogueErrorCode::SnapshotUnreadable);
}

TEST_CASE("inkcpp defect, not patched: a save taken before the first Advance restores a dialogue that has ended") {
    const I::Story story = Load("merchant.ink.json");
    const I::Dialogue fresh = Start(story, MerchantOptions(0));
    auto restored = I::Dialogue::Restore(story, Save(fresh), MerchantOptions(0));
    REQUIRE(restored);
    const I::Beat beat = Advance(restored->dialogue);
    CHECK(beat.outputs.empty());
    CHECK(beat.ended);
}

// ---- What Save says Restore can do (the game decides what to keep)

// One save point: where it is taken, and what Save says of it.
enum class At {
    Start,       // before the first Advance
    AfterChoice, // right after the last pick, before the next Advance
    Choices,     // after the picks, waiting for a choice
};

struct SavePoint {
    const char* name;
    const char* story;
    const char* next; // a next version that keeps the save's place, or none
    std::vector<std::size_t> picks;
    At at;
    bool restorable;
    bool migratable;
};

I::Dialogue PlayTo(const I::Story& story, const SavePoint& point) {
    I::Dialogue dialogue = Start(story, MerchantOptions(0));
    if (point.at == At::Start) return dialogue;
    Advance(dialogue);
    for (std::size_t i = 0; i < point.picks.size(); ++i) {
        Choose(dialogue, point.picks[i]);
        if (i + 1 < point.picks.size() || point.at == At::Choices) Advance(dialogue);
    }
    return dialogue;
}

TEST_CASE("Save says whether Restore on the same story gives the dialogue back, and whether it can move it") {
    const char* merchant = "merchant.ink.json";
    const char* next = "merchant_v2.ink.json";
    const std::vector<SavePoint> points = {
        {"right after a choice", merchant, next, {2}, At::AfterChoice, true, true},
        {"right after a choice inside a tunnel (D4)", merchant, next, {1, 0}, At::AfterChoice, true, true},
        {"at a choice inside a tunnel (D2)", merchant, next, {1}, At::Choices, false, false},
        {"at a choice, the story has a sequence (D2)", merchant, next, {}, At::Choices, false, false},
        {"at a choice, the story has no sequence", "once.ink.json", nullptr, {}, At::Choices, true, false},
        {"before the first Advance (D3)", merchant, next, {}, At::Start, false, false},
    };
    for (const SavePoint& point : points) {
        const I::Story story = Load(point.story);
        I::Dialogue dialogue = PlayTo(story, point);
        const I::Saved saved = TakeSave(dialogue);
        CHECK_MESSAGE(saved.restorable == point.restorable, "[ink-save-restorable] " << std::string(point.name));
        CHECK_MESSAGE(saved.migratable == point.migratable, "[ink-save-migratable] " << std::string(point.name));

        // What Save said, checked by restoring: whether inkcpp moves the save to
        // the next version (where it lands there: D4, below); the same text
        // from here on, or not.
        if (point.restorable && point.next != nullptr) {
            auto moved = I::Dialogue::Restore(Load(point.next), saved.bytes, MerchantOptions(0));
            CHECK_MESSAGE(static_cast<bool>(moved) == point.migratable,
                          "[ink-save-migratable] " << std::string(point.name) << ", moved: " << Why(moved));
        }
        const std::string expected = Render(Advance(dialogue));
        auto restored = I::Dialogue::Restore(story, saved.bytes, MerchantOptions(0));
        const bool same = restored && Render(Advance(restored->dialogue)) == expected;
        CHECK_MESSAGE(same == point.restorable, "[ink-save-restorable] " << std::string(point.name) << ", restored: " << Why(restored));
    }
}

TEST_CASE("a save Save says cannot be restored still gives its bytes") {
    const I::Story story = Load("merchant.ink.json");
    I::Dialogue dialogue = Start(story, MerchantOptions(0));
    Advance(dialogue); // at a choice: D2
    const I::Saved saved = TakeSave(dialogue);
    CHECK(!saved.restorable);
    CHECK_MESSAGE(saved.bytes.size() > 8, "[ink-save-bytes] the bytes are the game's to keep or drop");
}

// runner_impl.cpp can_be_migrated does not look at the tunnel's return on the
// stack (snapshot.h says a tunnel in use is not migratable), and migrate_to
// keeps the stack: the tunnel returns to the old version's offset.
TEST_CASE("inkcpp defect, not patched: a save right after a choice inside a tunnel is migratable, and its tunnel returns elsewhere in the next version") {
    const I::Story story = Load("merchant.ink.json");
    I::Dialogue dialogue = Start(story, MerchantOptions(0));
    Advance(dialogue);
    Choose(dialogue, 1); // haggle: into the tunnel
    Advance(dialogue);
    Choose(dialogue, 0); // insist
    const I::Saved saved = TakeSave(dialogue);
    CHECK(saved.migratable);
    auto moved = I::Dialogue::Restore(Load("merchant_v2.ink.json"), saved.bytes, MerchantOptions(0));
    REQUIRE_MESSAGE(moved, Why(moved));
    // The haggle's "No." returns into the sword's branch: a line and a command the player did not choose.
    CHECK(Render(Advance(moved->dialogue)).starts_with("merchant: No. [haggle_no]\n"
                                                       "merchant: Here you go, friend. [hub_sold]\n"
                                                       "> play_sound(3)\n"));
}

// globals_impl.cpp snap_load: a save taken after a choice is flagged
// migratable, so even a restore into the same story takes the migration path,
// which keeps only the visit counts of named containers; a sequence's container
// has no name (JBenda/inkcpp#178).
TEST_CASE("inkcpp defect, not patched: a save taken right after a choice restores with its sequences started over, and Save calls it restorable") {
    const I::Story story = Load("sequence.ink.json");
    I::Dialogue dialogue = Start(story, I::DialogueOptions{});
    Advance(dialogue);
    Choose(dialogue, 0);
    const I::Saved saved = TakeSave(dialogue);
    CHECK(saved.restorable); // the restore succeeds and goes on: Save does not see the sequences
    const std::string original = Render(Advance(dialogue));
    auto restored = I::Dialogue::Restore(story, saved.bytes, I::DialogueOptions{});
    REQUIRE_MESSAGE(restored, Why(restored));
    CHECK(original == "stopping: second [sequence_stopping]\ncycle: two [sequence_cycle]\n* Again [sequence_again]\n");
    CHECK(Render(Advance(restored->dialogue)) ==
          "stopping: first [sequence_stopping]\ncycle: one [sequence_cycle]\n* Again [sequence_again]\n");
}

// runner_impl.cpp Command::SEQUENCE drops the shuffle index the compiler gives
// and picks at random on every visit (JBenda/inkcpp#182).
TEST_CASE("inkcpp defect, not patched: a shuffle shows the same element twice within one pass") {
    const I::Story story = Load("shuffle.ink.json");
    I::Dialogue dialogue = Start(story, I::DialogueOptions{});
    // Two visits of a two-element shuffle once: ink shows y then x.
    CHECK(Play(dialogue, {0}) == "y [shuffle_y]\n* Again [shuffle_again]\n--\ny [shuffle_y]\n* Again [shuffle_again]\n");
}

TEST_CASE("a save taken right after a choice inside a tunnel restores the same dialogue") {
    const I::Story story = Load("merchant.ink.json");
    I::Dialogue straight = Start(story, MerchantOptions(0));
    Advance(straight);
    Choose(straight, 1); // haggle
    Advance(straight);
    Choose(straight, 0); // insist
    const std::vector<std::byte> saved = Save(straight);
    const std::string expected = Play(straight, {3});
    auto restored = I::Dialogue::Restore(story, saved, MerchantOptions(0));
    REQUIRE_MESSAGE(restored, "[ink-restore-tunnel] " << Why(restored));
    CHECK_MESSAGE(Play(restored->dialogue, {3}) == expected, "[ink-restore-tunnel]");
}

// ---- Threads (acceptance 7.4): a Story and its Dialogues are one family, used
// by one thread at a time; separate Stories are separate families.

TEST_CASE("two stories loaded apart run at the same time on two threads, each as on one thread") {
    const std::vector<std::byte> bytes = CompileStory("merchant.ink.json");
    auto run = [&bytes](std::string& out, std::string& error) {
        auto story = I::LoadStory(bytes);
        if (!story) { error = Why(story); return; }
        auto dialogue = I::Dialogue::Start(*story, MerchantOptions(0));
        if (!dialogue) { error = Why(dialogue); return; }
        for (int round = 0; round < 50; ++round) {
            for (const std::size_t pick : {std::size_t{1}, std::size_t{0}, std::size_t{0}, std::size_t{2}}) {
                auto beat = dialogue->Advance();
                if (!beat) { error = Why(beat); return; }
                out += Render(*beat);
                if (beat->ended) break;
                if (auto chosen = dialogue->Choose(pick); !chosen) { error = Why(chosen); return; }
            }
            auto restarted = I::Dialogue::Start(*story, MerchantOptions(static_cast<std::uint32_t>(round)));
            if (!restarted) { error = Why(restarted); return; }
            *dialogue = std::move(restarted).value();
        }
    };
    std::string alone, aloneError;
    run(alone, aloneError);
    REQUIRE_MESSAGE(aloneError.empty(), aloneError);
    std::string a, b, aError, bError;
    std::thread first([&] { run(a, aError); });
    std::thread second([&] { run(b, bError); });
    first.join();
    second.join();
    CHECK_MESSAGE(aError.empty(), aError);
    CHECK_MESSAGE(bError.empty(), bError);
    CHECK_MESSAGE((a == alone && b == alone), "[ink-threads-apart]");
}

TEST_CASE("one story's dialogues used by two threads in turn, with a join between turns, run as on one thread") {
    const I::Story story = Load("merchant.ink.json");
    I::Dialogue straight = Start(story, MerchantOptions(5));
    const std::string expected = Play(straight, {0, 1, 0});

    I::Dialogue dialogue = Start(story, MerchantOptions(5));
    std::string out;
    std::string error;
    const std::vector<std::size_t> picks{0, 1, 0};
    for (std::size_t turn = 0; turn <= picks.size(); ++turn) {
        // A new thread each turn, joined before the next: one thread at a time.
        std::thread worker([&] {
            if (turn > 0) {
                if (auto chosen = dialogue.Choose(picks[turn - 1]); !chosen) { error = Why(chosen); return; }
                out += "--\n";
            }
            auto beat = dialogue.Advance();
            if (!beat) { error = Why(beat); return; }
            out += Render(*beat);
        });
        worker.join();
        REQUIRE_MESSAGE(error.empty(), error);
    }
    CHECK_MESSAGE(out == expected, "[ink-threads-in-turn]");
}

} // namespace
} // namespace GYO::Plugins::Ink
