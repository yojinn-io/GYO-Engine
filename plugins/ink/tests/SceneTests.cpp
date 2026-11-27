// The ink Plugin's single-player scene (ink-spike-declaration section 2, C): a
// Product with one World and no window. The author's System advances the
// dialogue each frame and turns its commands into the author's own types: the
// give_item command into a Message another System counts, the play_sound
// command into an Event an observer hears. The Plugin knows none of these.
// Each CHECK carries a tag in brackets ("[ink-scene-...]") for the mutants.

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "adapters/inkcpp/CompileStory.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "plugins/ink/Dialogue.hpp"

// In the Plugin's namespace: a Plugin's tests live in its directory (naming rule N17).
namespace GYO::Plugins::Ink {
namespace {

namespace I = ::GYO::Plugins::Ink;
namespace K = ::GYO::Kernel;
namespace O = ::GYO::Engine::Orchestration;
namespace Base = ::GYO::Engine::Base;

// ---- The author's types

struct Conversation {
    std::optional<I::Story> story;
    std::optional<I::Dialogue> dialogue;
    std::vector<std::size_t> picks; // the player's choices, in turn
    std::size_t nextPick = 0;
    std::vector<std::string> lines; // "speaker: text" as the player would read them
    bool ended = false;
};

struct Inventory {
    std::vector<std::string> items;
};

struct Heard {
    std::vector<std::int32_t> sounds;
};

struct ItemGiven { // message
    std::string item;
    std::int32_t count = 0;
};

struct SoundPlayed { // event
    std::int32_t id = 0;
};

K::SystemResult Failed(const std::string& text) {
    return Base::Err(K::KernelError(K::KernelErrorCode::SystemFailed, text));
}

// One step of the dialogue a frame: a beat, then the next pick.
K::SystemResult Talk(K::ResMut<Conversation> conversation, K::MessageWriter<ItemGiven> items, K::Commands commands) {
    if (conversation->ended) return {};
    auto beat = conversation->dialogue->Advance();
    if (!beat) return Failed(Base::Describe(beat.error()));
    for (const I::Output& output : beat->outputs) {
        if (const auto* line = std::get_if<I::Line>(&output)) {
            conversation->lines.push_back(line->speaker + ": " + line->text);
            continue;
        }
        const auto& command = std::get<I::Command>(output);
        if (command.name == "give_item" && command.args.size() == 2) {
            items.Send(ItemGiven{std::get<std::string>(command.args[0]), std::get<std::int32_t>(command.args[1])});
        } else if (command.name == "play_sound" && command.args.size() == 1) {
            commands.Trigger(SoundPlayed{std::get<std::int32_t>(command.args[0])});
        } else {
            return Failed("the story called an unknown command " + command.name);
        }
    }
    if (beat->ended) {
        conversation->ended = true;
        return {};
    }
    if (conversation->nextPick >= conversation->picks.size()) return Failed("the player ran out of picks");
    auto chosen = conversation->dialogue->Choose(conversation->picks[conversation->nextPick++]);
    if (!chosen) return Failed(Base::Describe(chosen.error()));
    return {};
}

K::SystemResult CountItems(K::MessageReader<ItemGiven> given, K::ResMut<Inventory> inventory) {
    for (const ItemGiven* item : given.Read()) {
        for (std::int32_t i = 0; i < item->count; ++i) inventory->items.push_back(item->item);
    }
    return {};
}

K::SystemResult HearSound(K::On<SoundPlayed> on, K::ResMut<Heard> heard) {
    heard->sounds.push_back(on.Event().id);
    return {};
}

K::SystemOptions In(std::string name, std::string stage) {
    K::SystemOptions options;
    options.name = std::move(name);
    K::Mount mount;
    mount.schedule = "Frame";
    mount.stage = std::move(stage);
    options.mounts.push_back(std::move(mount));
    return options;
}

O::ProductDescription Scene(std::vector<std::size_t> picks) {
    O::WorldDescription world;
    world.name = "Main";
    world.skeleton = O::ScheduleSkeleton::Off;
    world.build = [](O::WorldSetup& setup) {
        K::Registrar& r = setup.Registrar();
        r.Resource<Conversation>({.name = "Conversation"});
        r.Resource<Inventory>({.name = "Inventory"});
        r.Resource<Heard>({.name = "Heard"});
        r.Message<ItemGiven>({.name = "ItemGiven"});
        r.Event<SoundPlayed>({.name = "SoundPlayed"});
        K::ScheduleOptions frame;
        frame.name = "Frame";
        frame.stages = {"Talk", "Count"};
        r.Schedule(std::move(frame));
        r.AddSystem<&Talk>(In("Talk", "Talk"));
        r.AddSystem<&CountItems>(In("CountItems", "Count"));
        r.Observer<SoundPlayed, &HearSound>({.name = "HearSound"});
    };
    world.prepare.push_back([picks = std::move(picks)](K::World& w) -> Base::Result<void, O::OrchestrationError> {
        auto story = I::LoadStory(Test::CompileStory("merchant.ink.json"));
        if (!story) return Base::Err(O::OrchestrationError(O::OrchestrationErrorCode::PrepareFailed, Base::Describe(story.error())));
        I::DialogueOptions options;
        options.seed = 0;
        options.commands = {"give_item", "play_sound"};
        auto dialogue = I::Dialogue::Start(*story, options);
        if (!dialogue) {
            return Base::Err(O::OrchestrationError(O::OrchestrationErrorCode::PrepareFailed, Base::Describe(dialogue.error())));
        }
        Conversation conversation;
        conversation.story = std::move(story).value();
        conversation.dialogue = std::move(dialogue).value();
        conversation.picks = picks;
        w.SetResource<Conversation>(std::move(conversation));
        w.SetResource<Inventory>(Inventory{});
        w.SetResource<Heard>(Heard{});
        return {};
    });
    world.frame = {"Frame"};
    O::ProductDescription description;
    description.name = "ink_scene";
    description.worlds.push_back(std::move(world));
    return description;
}

TEST_CASE("the scene: the author's System runs the dialogue; its commands become the author's Message and Event") {
    auto built = O::Product::Build(Scene({0, 3}), O::RegisteredUnits{}); // buy the sword, then leave
    REQUIRE_MESSAGE(built, (built ? std::string() : built.error().message + "\n" + built.error().detail));
    O::Product& product = **built;
    for (int frame = 0; frame < 10; ++frame) {
        auto ran = product.RunFrame();
        REQUIRE_MESSAGE(ran, (ran ? std::string() : Base::Describe(ran.error())));
        if (product.World("Main").GetWorld().Resource<Conversation>()->ended) break;
    }
    const K::World& world = product.World("Main").GetWorld();
    const Conversation* conversation = world.Resource<Conversation>();
    REQUIRE(conversation != nullptr);
    CHECK_MESSAGE(conversation->ended, "[ink-scene-ended]");
    const std::vector<std::string> lines{"merchant: Welcome to my stall.",
                                         "merchant: Cold wind tonight.",
                                         "merchant: Today's lucky number is 31.",
                                         "merchant: Here you go.",
                                         "merchant: Welcome to my stall.",
                                         "merchant: Fine weather today.",
                                         "merchant: Today's lucky number is 52.",
                                         "merchant: Farewell."};
    const std::vector<std::string> items{"sword", "sword"};
    const std::vector<std::int32_t> sounds{3, 7};
    CHECK_MESSAGE(conversation->lines == lines, "[ink-scene-lines]");
    CHECK_MESSAGE(world.Resource<Inventory>()->items == items, "[ink-scene-message]");
    CHECK_MESSAGE(world.Resource<Heard>()->sounds == sounds, "[ink-scene-event]");
}

} // namespace
} // namespace GYO::Plugins::Ink
