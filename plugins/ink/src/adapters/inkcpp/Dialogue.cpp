// The ink Plugin's adapter: the only file that includes inkcpp. Every entry
// catches inkcpp's exception (std::runtime_error in its STL build) and turns it
// into a DialogueError; std::bad_alloc is left to the caller, as elsewhere.

#include "plugins/ink/Dialogue.hpp"

#include <cstring>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <choice.h>
#include <globals.h>
#include <header.h>
#include <runner.h>
#include <snapshot.h>
#include <story.h>
#include <system.h>

namespace GYO::Plugins::Ink {

namespace Base = Engine::Base;

// inkcpp reads the fixed header at the start of a story or a snapshot before it
// checks the length it was given, so bytes shorter than the header are refused
// here. The story's header is inkcpp's shared one; the snapshot's
// (snapshot_impl.h) is private to inkcpp: two size_t, a hash, a bool and a
// size_t, at most five words.
constexpr std::size_t kStoryHeader = sizeof(ink::internal::header);
constexpr std::size_t kSnapshotHeader = 5 * sizeof(std::size_t);

struct Story::Data {
    std::unique_ptr<ink::runtime::story> story;
    std::uint32_t hash = 0; // inkcpp hashes the whole file on each call: once, here
};

struct Dialogue::State {
    std::shared_ptr<Story::Data> story; // first: destroyed after the runner and globals
    ink::runtime::globals globals;
    ink::runtime::runner runner;
    DialogueOptions options;
    std::vector<Output> outputs; // lines and commands since the last Advance
    bool stopped = false;
};

namespace {

// The save bytes: the format version and the story's hash (little endian), then
// inkcpp's snapshot. The hash tells Restore whether the story changed since.
constexpr std::uint32_t kSaveVersion = 1;
constexpr std::size_t kSaveHeader = 4 + 4;

void PutU32(std::vector<std::byte>& out, const std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) out.push_back(static_cast<std::byte>((value >> shift) & 0xFFu));
}

std::uint32_t GetU32(const std::span<const std::byte> in) {
    std::uint32_t value = 0;
    for (int i = 3; i >= 0; --i) value = (value << 8) | std::to_integer<std::uint32_t>(in[static_cast<std::size_t>(i)]);
    return value;
}

DialogueError MakeError(const DialogueErrorCode code, std::string message, std::string detail = {}) {
    return DialogueError::Make(code, std::move(message), std::move(detail));
}

// A tag "<prefix>:<value>" gives value to slot when the slot is still empty.
bool TakeTag(const std::string& tag, const std::string_view prefix, std::string& slot) {
    if (!slot.empty() || tag.size() <= prefix.size() || !tag.starts_with(prefix) || tag[prefix.size()] != ':') {
        return false;
    }
    slot = tag.substr(prefix.size() + 1);
    return true;
}

Line ReadLine(std::string text, const ink::runtime::runner_interface& runner, const DialogueOptions& options) {
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
    Line line;
    line.text = std::move(text);
    for (std::size_t i = 0; i < runner.num_tags(); ++i) {
        std::string tag = runner.get_tag(i);
        if (!TakeTag(tag, options.lineIdTag, line.lineId) && !TakeTag(tag, options.speakerTag, line.speaker)) {
            line.tags.push_back(std::move(tag));
        }
    }
    return line;
}

std::vector<Choice> ReadChoices(const ink::runtime::runner_interface& runner, const DialogueOptions& options) {
    std::vector<Choice> choices;
    for (const ink::runtime::choice* it = runner.begin(); it != runner.end(); ++it) {
        Choice choice;
        choice.text = it->text();
        for (std::size_t i = 0; i < it->num_tags(); ++i) {
            std::string tag = it->get_tag(i);
            if (!TakeTag(tag, options.lineIdTag, choice.lineId)) choice.tags.push_back(std::move(tag));
        }
        choices.push_back(std::move(choice));
    }
    return choices;
}

Value ReadArgument(const ink::runtime::value& value) {
    using Type = ink::runtime::value::Type;
    switch (value.type) {
    case Type::Bool:   return value.get<Type::Bool>();
    case Type::Int32:  return value.get<Type::Int32>();
    case Type::Float:  return value.get<Type::Float>();
    case Type::String: return std::string(value.get<Type::String>()); // copied: it points into the story's strings
    case Type::Uint32:
    case Type::List:   break;
    }
    // Thrown inside the story's step, which resets the runner and rethrows: a ScriptFault.
    throw ink::ink_exception("a command argument is a list or an unsigned number, which commands do not take");
}

// Binds every command of the options to append a Command to the state's outputs.
void BindCommands(Dialogue::State& state) {
    for (const std::string& name : state.options.commands) {
        Dialogue::State* const target = &state;
        state.runner->bind(
            name.c_str(),
            [target, name](const std::size_t count, const ink::runtime::value* values) {
                Command command;
                command.name = name;
                // inkcpp pops the arguments off its stack: values[0] is the last one.
                for (std::size_t i = count; i-- > 0;) command.args.push_back(ReadArgument(values[i]));
                target->outputs.emplace_back(std::move(command));
            },
            false); // not lookahead safe: run once, when the story reaches it
    }
}

DialogueError Stopped() {
    return MakeError(DialogueErrorCode::Stopped, "the dialogue stopped on an earlier ScriptFault");
}

} // namespace

Story::Story(std::shared_ptr<Data> data) noexcept : data_(std::move(data)) {}

Base::Result<Story, DialogueError> LoadStory(const std::span<const std::byte> compiled) {
    if (compiled.size() < kStoryHeader) {
        return Base::Err(MakeError(DialogueErrorCode::NotAStory, "the bytes are too short for a compiled story",
                                   std::to_string(compiled.size()) + " bytes"));
    }
    auto bytes = std::make_unique<unsigned char[]>(compiled.size());
    std::memcpy(bytes.get(), compiled.data(), compiled.size());
    try {
        auto data = std::make_shared<Story::Data>();
        data->story.reset(ink::runtime::story::from_binary(bytes.get(), compiled.size(), true));
        bytes.release(); // the story frees it
        data->hash = data->story->hash();
        return Story(std::move(data));
    } catch (const ink::ink_exception& e) {
        // Thrown from the story's constructor: the story does not exist and the bytes are still ours.
        return Base::Err(MakeError(DialogueErrorCode::NotAStory, "the bytes are not a compiled story", e.what()));
    }
}

Dialogue::Dialogue(std::unique_ptr<State> state) noexcept : state_(std::move(state)) {}
Dialogue::Dialogue(Dialogue&&) noexcept = default;
Dialogue& Dialogue::operator=(Dialogue&&) noexcept = default;
Dialogue::~Dialogue() = default;

Base::Result<Dialogue, DialogueError> Dialogue::Start(const Story& story, DialogueOptions options) {
    auto state = std::make_unique<State>();
    state->story = story.data_;
    state->options = std::move(options);
    try {
        state->globals = state->story->story->new_globals();
        state->runner = state->story->story->new_runner(state->globals);
        state->runner->set_rng_seed(state->options.seed); // the runner seeds itself from the clock
        BindCommands(*state);
    } catch (const ink::ink_exception& e) {
        return Base::Err(MakeError(DialogueErrorCode::ScriptFault, "the story failed to start", e.what()));
    }
    return Dialogue(std::move(state));
}

Base::Result<Restored, DialogueError> Dialogue::Restore(const Story& story, const std::span<const std::byte> saved,
                                                        DialogueOptions options) {
    if (saved.size() < kSaveHeader) {
        return Base::Err(MakeError(DialogueErrorCode::SnapshotUnreadable, "the bytes are too short for a save"));
    }
    const std::uint32_t version = GetU32(saved.subspan(0, 4));
    if (version != kSaveVersion) {
        return Base::Err(MakeError(DialogueErrorCode::SnapshotUnreadable, "the save has another format version",
                                   "version " + std::to_string(version)));
    }
    const std::uint32_t savedHash = GetU32(saved.subspan(4, 4));
    const std::span<const std::byte> snapshotBytes = saved.subspan(kSaveHeader);

    if (snapshotBytes.size() < kSnapshotHeader) {
        return Base::Err(MakeError(DialogueErrorCode::SnapshotUnreadable, "the bytes are too short for a save"));
    }

    auto state = std::make_unique<State>();
    state->story = story.data_;
    state->options = std::move(options);
    ink::runtime::story& ink = *state->story->story;
    const bool migrated = savedHash != state->story->hash;
    // Another version of the story: what inkcpp cannot load is a save it cannot move there.
    auto failed = [migrated](std::string detail) {
        return migrated ? MakeError(DialogueErrorCode::SnapshotNotMigratable,
                                    "the save cannot move to this version of the story", std::move(detail))
                        : MakeError(DialogueErrorCode::SnapshotUnreadable, "the save could not be read",
                                    std::move(detail));
    };
    try {
        // Not owned: inkcpp reads the bytes only while the dialogue is rebuilt.
        const std::unique_ptr<ink::runtime::snapshot> snapshot(ink::runtime::snapshot::from_binary(
            reinterpret_cast<const unsigned char*>(snapshotBytes.data()), snapshotBytes.size(), false));
        state->globals = ink.new_globals_from_snapshot(*snapshot);
        if (!state->globals) return Base::Err(failed("inkcpp gave no globals for the save"));
        state->runner = ink.new_runner_from_snapshot(*snapshot, state->globals);
        if (!state->runner) return Base::Err(failed("inkcpp found no place in the story to continue the save from"));
        BindCommands(*state);
    } catch (const ink::ink_exception& e) {
        return Base::Err(failed(e.what()));
    }
    return Restored{Dialogue(std::move(state)), migrated};
}

Base::Result<Beat, DialogueError> Dialogue::Advance() {
    State& state = *state_;
    if (state.stopped) return Base::Err(Stopped());
    try {
        while (state.runner->can_continue()) {
            Line line = ReadLine(state.runner->getline(), *state.runner, state.options);
            // inkcpp gives an empty line before a choice point; one without text or tags says nothing.
            if (!line.text.empty() || !line.lineId.empty() || !line.speaker.empty() || !line.tags.empty()) {
                state.outputs.emplace_back(std::move(line));
            }
        }
        Beat beat;
        beat.choices = ReadChoices(*state.runner, state.options);
        beat.ended = beat.choices.empty();
        beat.outputs = std::exchange(state.outputs, {});
        return beat;
    } catch (const ink::ink_exception& e) {
        state.stopped = true;
        state.outputs.clear();
        return Base::Err(MakeError(DialogueErrorCode::ScriptFault, "the story failed while running", e.what()));
    }
}

Base::Result<void, DialogueError> Dialogue::Choose(const std::size_t index) {
    State& state = *state_;
    if (state.stopped) return Base::Err(Stopped());
    const std::size_t count = state.runner->num_choices();
    if (index >= count) {
        return Base::Err(MakeError(DialogueErrorCode::ChoiceOutOfRange, "no choice with this index",
                                   "index " + std::to_string(index) + ", " + std::to_string(count) + " choices"));
    }
    try {
        state.runner->choose(index);
    } catch (const ink::ink_exception& e) {
        state.stopped = true;
        return Base::Err(MakeError(DialogueErrorCode::ScriptFault, "the story failed on the choice", e.what()));
    }
    return {};
}

Base::Result<std::vector<std::byte>, DialogueError> Dialogue::Save() const {
    const State& state = *state_;
    if (state.stopped) return Base::Err(Stopped());
    try {
        const std::unique_ptr<ink::runtime::snapshot> snapshot(state.runner->create_snapshot());
        std::vector<std::byte> bytes;
        bytes.reserve(kSaveHeader + snapshot->get_data_len());
        PutU32(bytes, kSaveVersion);
        PutU32(bytes, state.story->hash);
        const auto* data = reinterpret_cast<const std::byte*>(snapshot->get_data());
        bytes.insert(bytes.end(), data, data + snapshot->get_data_len());
        return bytes;
    } catch (const ink::ink_exception& e) {
        return Base::Err(MakeError(DialogueErrorCode::ScriptFault, "the dialogue could not be saved", e.what()));
    }
}

} // namespace GYO::Plugins::Ink
