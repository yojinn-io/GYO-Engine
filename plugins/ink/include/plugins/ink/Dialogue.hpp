#pragma once

// Ink stories for a game: a compiled story, and a dialogue on it that gives
// lines, choices and commands (docs/architecture/target/v2/ink.zh-Hant.md).
//
// The words are the game's: nothing here names inkcpp. What the game does with
// a line or a command, where a save goes and what to do on an error is the
// game's; an error only says what happened.
//
// Threads: a Story and every Dialogue started or restored from it (and every
// copy of the Story) are one family. A family is used by one thread at a time;
// handing it to another thread needs a synchronisation between the two (a
// mutex, a join, the Kernel running one System after another). Stories loaded
// separately, even from the same bytes, are separate families.
//
// Numbers in the text are formatted through the C library: the process'
// LC_NUMERIC locale must be "C", which it is unless something calls setlocale.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

namespace GYO::Plugins::Ink {

enum class DialogueErrorCode : std::uint16_t {
    // LoadStory: the bytes are not a compiled story this runtime reads (another
    // format, another byte order, another compiler version, cut short).
    NotAStory = 1,
    // inkcpp failed: a command the story calls that is not in
    // DialogueOptions::commands and has no fallback, a command argument that is
    // a list, a broken instruction. From Advance or Choose the dialogue has
    // stopped; from Start there is no dialogue; from Save it is unchanged.
    ScriptFault = 2,
    // A call on a dialogue that stopped on a ScriptFault.
    Stopped = 3,
    // Choose(index) with an index past the choices; the dialogue is unchanged.
    ChoiceOutOfRange = 4,
    // Restore, the same story: the bytes are not a save inkcpp restores (another
    // format, cut short, another snapshot version, or a save taken where this
    // inkcpp cannot restore one; docs/architecture/target/v2/ink.zh-Hant.md).
    SnapshotUnreadable = 5,
    // Restore, another version of the story: inkcpp could not move the save
    // there (taken inside a tunnel or a thread, or its place in the story gone).
    SnapshotNotMigratable = 6,
};

[[nodiscard]] constexpr const char* ToString(const DialogueErrorCode code) noexcept {
    switch (code) {
    case DialogueErrorCode::NotAStory:             return "NotAStory";
    case DialogueErrorCode::ScriptFault:           return "ScriptFault";
    case DialogueErrorCode::Stopped:               return "Stopped";
    case DialogueErrorCode::ChoiceOutOfRange:      return "ChoiceOutOfRange";
    case DialogueErrorCode::SnapshotUnreadable:    return "SnapshotUnreadable";
    case DialogueErrorCode::SnapshotNotMigratable: return "SnapshotNotMigratable";
    }
    return "Unknown";
}

using DialogueError = Engine::Base::Error<DialogueErrorCode>;
static_assert(Engine::Base::CodedError<DialogueError>);

// An argument a story passes to a command.
using Value = std::variant<bool, std::int32_t, float, std::string>;

// One line of text. The tags "<lineIdTag>:<id>" and "<speakerTag>:<id>" are
// taken out of the tags; a line without them has an empty lineId or speaker.
struct Line {
    std::string text; // without the line's end
    std::string lineId;
    std::string speaker;
    std::vector<std::string> tags; // the other tags, in the story's order
};

// One choice the story offers.
struct Choice {
    std::string text;
    std::string lineId;
    std::vector<std::string> tags; // the tags other than the line id
};

// A command the story calls (an EXTERNAL function named in
// DialogueOptions::commands). It returns nothing to the story.
struct Command {
    std::string name;
    std::vector<Value> args; // in the order the story writes them
};

using Output = std::variant<Line, Command>;

// What one Advance gives.
struct Beat {
    std::vector<Output> outputs;  // lines and commands, in the order the story ran them
    std::vector<Choice> choices;  // empty when there is nothing to choose
    bool ended = false;           // the story reached its end
};

struct DialogueOptions {
    // The seed of the story's random numbers (shuffles, RANDOM). A fixed value
    // by default, never the clock. Restore keeps the saved random state instead.
    std::uint32_t seed = 0;
    // The EXTERNAL functions of the story that are commands.
    std::vector<std::string> commands;
    std::string lineIdTag = "line";
    std::string speakerTag = "speaker";
};

// A compiled story, read once and never changed. Copies share it (and are one
// family for threads, see above).
class Story {
public:
    struct Data;

private:
    explicit Story(std::shared_ptr<Data> data) noexcept;
    std::shared_ptr<Data> data_;

    friend class Dialogue;
    friend Engine::Base::Result<Story, DialogueError> LoadStory(std::span<const std::byte> compiled);
};

// Reads the bytes of a story compiled by inkcpp's compiler (ink.json to binary).
[[nodiscard]] Engine::Base::Result<Story, DialogueError> LoadStory(std::span<const std::byte> compiled);

struct Restored;

// One dialogue in progress on a story. It keeps its story alive.
class Dialogue {
public:
    [[nodiscard]] static Engine::Base::Result<Dialogue, DialogueError> Start(const Story& story,
                                                                           DialogueOptions options);
    // Restores a dialogue from Save's bytes. The story may be another version of
    // the one saved (see Restored). options.seed is not used: the random state
    // comes from the save. Where a save can be taken and restored is inkcpp's
    // (docs/architecture/target/v2/ink.zh-Hant.md): right after a choice it can.
    [[nodiscard]] static Engine::Base::Result<Restored, DialogueError> Restore(const Story& story,
                                                                             std::span<const std::byte> saved,
                                                                             DialogueOptions options);

    Dialogue(Dialogue&&) noexcept;
    Dialogue& operator=(Dialogue&&) noexcept;
    Dialogue(const Dialogue&) = delete;
    Dialogue& operator=(const Dialogue&) = delete;
    ~Dialogue();

    // Runs the story until it offers choices or ends. Called again while it
    // waits for a choice, it gives no outputs and the same choices.
    [[nodiscard]] Engine::Base::Result<Beat, DialogueError> Advance();
    // Takes the choice at index (of the last Beat's choices).
    [[nodiscard]] Engine::Base::Result<void, DialogueError> Choose(std::size_t index);
    // The dialogue's state as bytes, for Restore.
    [[nodiscard]] Engine::Base::Result<std::vector<std::byte>, DialogueError> Save() const;

    struct State;

private:
    explicit Dialogue(std::unique_ptr<State> state) noexcept;
    std::unique_ptr<State> state_;
};

struct Restored {
    Dialogue dialogue;
    // The save came from another version of the story: global variables that
    // still exist kept their values and new ones start at their defaults; the
    // dialogue goes on from the knot it was in.
    bool migrated = false;
};

} // namespace GYO::Plugins::Ink
