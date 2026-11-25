#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/json/Document.hpp"
#include "engine/json/JsonError.hpp"

// Writing JSON as a stream: each call appends to the caller's string, in call
// order, with no document built in between. See Design Invariant J-5 of
// docs/architecture/target/v2/json-module-declaration.zh-Hant.md.
//
//     std::string line;                 // reused: clear() keeps the capacity
//     Json::Writer writer(line);
//     writer.BeginObject();
//     writer.Key("tick"); writer.Uint(1207);
//     writer.Key("speed"); writer.Double(speed);
//     writer.EndObject();
//     if (auto done = writer.Finish(); !done) { ... line is back to its old length ... }
//
// Output is strict JSON that Parse reads back to the same values
// (doubles bit for bit, strings byte for byte, keys in order), provided the
// caller does not write one key twice in an object: the Writer does not look
// (that would need memory per object), and Parse rejects the result.
// Misuse of the structure (a value in an object without a key, a key outside
// an object, unbalanced ends, a second top-level value, Finish with open
// containers, any call after Finish) is a Programmer Error. Data JSON cannot
// represent (a non-finite number, invalid UTF-8) and allocation failure are
// Runtime Errors that Finish reports. With enough capacity in the string and
// no more than kInlineDepth levels, writing allocates nothing.
//
// The layout is the caller's choice (Layout): compact on one line by default,
// or indented. Either way the bytes depend only on the calls and the Layout.

namespace GYO::Engine::Json {

enum class IndentCharacter : char { Space = ' ', Tab = '\t' };

// How the Writer lays out the text. With indent 0 (the default) everything is
// on one line with no spaces. With indent n > 0 every member and element starts
// a line indented n characters per level, a key is followed by ": ", the
// closing bracket of a non-empty container has its own line, and an empty
// container stays "{}" or "[]".
struct Layout final {
    std::size_t indent = 0;
    IndentCharacter character = IndentCharacter::Space;
};

class Writer final {
public:
    static constexpr std::size_t kInlineDepth = 32;

    explicit Writer(std::string& out, Layout layout = {}) noexcept;
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    void BeginObject();
    void EndObject();
    void BeginArray();
    void EndArray();
    void Key(std::string_view key);

    void String(std::string_view text);
    void Int(std::int64_t value);
    void Uint(std::uint64_t value);
    // The shortest text that reads back to the same double, keeping ".0" on
    // integral values (100000.0, 1e21, -0.0). A non-finite value is
    // NotRepresentable.
    void Double(double value);
    void Bool(bool value);
    void Null();

    // Ends the document. On failure the string is cut back to the length it
    // had when the Writer was made, and the first error is returned
    // (NotRepresentable or OutOfMemory).
    [[nodiscard]] JsonResult<void> Finish();

private:
    friend void WriteValue(Writer& writer, Value value);

    enum class Container : std::uint8_t { Object = 1, Array };
    struct Frame {
        Container container;
        bool first;
        bool awaitingValue;
    };

    void BeforeValue();
    // Before a member or an element: the comma after the previous one and, when
    // indenting, the new line.
    void Separate(Frame& frame);
    void NewLine(std::size_t level);
    void BeginContainer(Container container, char open);
    void EndContainer(Container container, char close);
    // The text of a number already known to be valid JSON (WriteValue).
    void NumberText(std::string_view text);
    void Fail(JsonErrorCode code, std::string message);
    Frame& Top();
    void Push(Frame frame);
    void Pop();

    std::string& out_;
    Layout layout_;
    std::size_t startLength_;
    std::array<Frame, kInlineDepth> inline_{};
    std::vector<Frame> deeper_;
    std::size_t depth_ = 0;
    // Levels opened after a level's state could not be stored (allocation
    // failure past kInlineDepth): only counted, so that the caller can still
    // close them and reach Finish, which then reports OutOfMemory.
    std::size_t lost_ = 0;
    std::size_t values_ = 0;
    bool topWritten_ = false;
    bool finished_ = false;
    std::optional<JsonError> error_;
};

// Writes a parsed value with the Writer: members in document order, numbers
// as their original text (AsNumberText), strings escaped anew. No recursion.
void WriteValue(Writer& writer, Value value);

} // namespace GYO::Engine::Json
