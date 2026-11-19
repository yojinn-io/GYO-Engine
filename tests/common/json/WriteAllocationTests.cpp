// The Writer allocates nothing when the string has the capacity and the depth
// stays within its inline levels (Design Invariant J-5), and an allocation
// failure is OutOfMemory from Finish, not an exception. This file replaces the
// global operator new and delete for the whole test executable: they count,
// and fail on request, on the calling thread only.
#include <doctest/doctest.h>

#include <cstddef>
#include <cstdlib>
#include <new>
#include <string>
#include <string_view>

#include "AssertTestSupport.hpp"
#include "engine/json/Document.hpp"
#include "engine/json/Writer.hpp"

namespace {

thread_local std::size_t allocations = 0;
thread_local bool failNext = false;

void* Allocate(const std::size_t size) {
    if (failNext) {
        failNext = false;
        throw std::bad_alloc();
    }
    ++allocations;
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}

} // namespace

void* operator new(const std::size_t size) { return Allocate(size); }
void* operator new[](const std::size_t size) { return Allocate(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

using GYO::Engine::Json::JsonErrorCode;
using GYO::Engine::Json::Writer;

namespace {

// A trace-like record of about 420 bytes, nested 32 levels at its deepest: the
// Contract's number (J-5), written out so that a smaller kInlineDepth fails here.
void WriteRecord(Writer& writer) {
    writer.BeginObject();
    writer.Key("schema_version");
    writer.Uint(3);
    writer.Key("kind");
    writer.String("input_applied");
    for (int i = 0; i < 12; ++i) {
        writer.Key("field_with_a_longer_name");
        writer.Double(1234.5678 + i);
    }
    writer.Key("note");
    writer.String("escaped \"quote\" and a\ttab");
    writer.Key("deep");
    for (int i = 1; i < 32; ++i) writer.BeginArray();
    writer.Int(-7);
    for (int i = 1; i < 32; ++i) writer.EndArray();
    writer.EndObject();
}

} // namespace

TEST_CASE("Writer: no allocation when the string has the capacity and the depth is inline") {
    std::string line;
    line.reserve(2048);
    const char* data = line.data();
    allocations = 0;
    {
        Writer writer(line);
        WriteRecord(writer);
        CHECK(writer.Finish().has_value());
    }
    CHECK(allocations == 0);
    CHECK(line.data() == data);
    CHECK(line.size() > 400);

    // The same string, cleared, serves the next line without allocating.
    line.clear();
    allocations = 0;
    {
        Writer writer(line);
        WriteRecord(writer);
        CHECK(writer.Finish().has_value());
    }
    CHECK(allocations == 0);
}

TEST_CASE("Writer: an allocation failure is OutOfMemory and the string is cut back") {
    std::string line = "kept";
    line.shrink_to_fit();
    Writer writer(line);
    writer.BeginObject();
    writer.Key("a");
    const std::string big(4096, 'x');
    failNext = true;  // the next allocation, the string's growth, fails
    writer.String(big);
    failNext = false;
    writer.EndObject();
    auto done = writer.Finish();
    REQUIRE_FALSE(done.has_value());
    CHECK(done.error().code == JsonErrorCode::OutOfMemory);
    CHECK(line == "kept");
}

// Added after the adversarial check of the mutation list (Fable/high, 2026-11-20;
// build/evidence/gyo2-json-module/mutation-check-fable/).

using GYO::Engine::Json::Parse;
using GYO::Engine::Json::WriteValue;

TEST_CASE("Parse: an allocation failure in the adapter's own walk is OutOfMemory, not an exception") {
    const std::string_view text = R"({"a":[1,2,{"b":3}]})";
    bool threw = false;
    bool has = true;
    JsonErrorCode code = JsonErrorCode::Syntax;
    failNext = true;  // the first operator new after this: the validation walk's stack
    try {
        auto document = Parse(text);
        has = document.has_value();
        if (!has) code = document.error().code;
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    failNext = false;
    CHECK_FALSE(threw);
    CHECK_FALSE(has);
    CHECK(code == JsonErrorCode::OutOfMemory);
}

TEST_CASE("WriteValue: an allocation failure does not escape as std::bad_alloc") {
    auto document = Parse("[[1]]");
    REQUIRE(document.has_value());
    std::string out;
    out.reserve(64);
    Writer writer(out);
    bool threw = false;
    failNext = true;
    try {
        WriteValue(writer, document->Root());
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    failNext = false;
    CHECK_FALSE(threw);
}

TEST_CASE("Writer: an allocation failure when nesting past the inline levels is OutOfMemory from Finish") {
    std::string out;
    out.reserve(4096);
    Writer writer(out);
    for (std::size_t i = 0; i < Writer::kInlineDepth; ++i) writer.BeginArray();
    failNext = true;  // the 33rd level needs the heap
    writer.BeginArray();
    failNext = false;
    bool asserted = false;
    bool finished = false;
    JsonErrorCode code = JsonErrorCode::Syntax;
    {
        ::Engine::Test::ScopedAssertionHandler scope;
        try {
            for (std::size_t i = 0; i < Writer::kInlineDepth + 1; ++i) writer.EndArray();
            auto done = writer.Finish();
            finished = true;
            if (!done) code = done.error().code;
        } catch (const ::Engine::Base::AssertionFailure&) {
            asserted = true;
        }
    }
    CHECK_FALSE(asserted);
    CHECK(finished);
    CHECK(code == JsonErrorCode::OutOfMemory);
}

TEST_CASE("Value: building a WrongKind error under allocation failure does not escape as std::bad_alloc") {
    auto document = Parse("{}");
    REQUIRE(document.has_value());
    bool threw = false;
    failNext = true;
    try {
        auto array = document->Root().AsArray();
        CHECK_FALSE(array.has_value());
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    failNext = false;
    CHECK_FALSE(threw);
}

TEST_CASE("Document: does not refer to the parsed text (buffer with slack, overwritten, kept alive)") {
    std::string buffer = R"({"key":"value","n":12.5})";
    const std::size_t length = buffer.size();
    buffer.append(8, '\0');  // slack: an in-place parse would stay inside this buffer
    auto document = Parse(std::string_view(buffer.data(), length));
    REQUIRE(document.has_value());
    for (char& c : buffer) c = '#';
    auto object = document->Root().AsObject();
    REQUIRE(object.has_value());
    const GYO::Engine::Json::Member first = *object->begin();
    CHECK(first.key == "key");
    CHECK(first.value.AsString().value() == "value");
}

TEST_CASE("Document: a moved-from Document is empty (Root is a Programmer Error)") {
    using GYO::Engine::Json::Document;
    auto parsed = Parse("[1]");
    REQUIRE(parsed.has_value());
    auto* source = new Document(std::move(*parsed));
    Document moved = std::move(*source);
    bool asserted = false;
    {
        ::Engine::Test::ScopedAssertionHandler scope;
        try {
            static_cast<void>(source->Root());
        } catch (const ::Engine::Base::AssertionFailure&) {
            asserted = true;
        }
    }
    CHECK_MESSAGE(asserted, "the moved-from Document still has a tree");
    // Deleted only when empty: a move that left the tree behind fails above, not in a double free.
    if (asserted) delete source;
    CHECK(moved.Root().AsArray().value().Size() == 1);

    auto otherParsed = Parse("{\"k\":true}");
    REQUIRE(otherParsed.has_value());
    auto* other = new Document(std::move(*otherParsed));
    moved = std::move(*other);
    CHECK_MESSAGE(moved.Root().GetKind() == GYO::Engine::Json::Kind::Object, "move assignment did not take the tree");
    bool otherAsserted = false;
    {
        ::Engine::Test::ScopedAssertionHandler scope;
        try {
            static_cast<void>(other->Root());
        } catch (const ::Engine::Base::AssertionFailure&) {
            otherAsserted = true;
        }
    }
    CHECK_MESSAGE(otherAsserted, "the move-assigned-from Document still has a tree");
    if (otherAsserted) delete other;
}

namespace {

// Runs prefix, fills the string to its capacity so that the next byte needs
// the heap, fails that allocation inside op, then finishes with suffix.
template <class Prefix, class Op, class Suffix>
void ExpectOutOfMemory(const char* name, Prefix prefix, Op op, Suffix suffix) {
    INFO("allocation failure in " << std::string(name));
    std::string out = "kept";
    Writer writer(out);
    prefix(writer);
    out.append(out.capacity() - out.size(), ' ');  // whitespace between tokens; cut back anyway
    bool threw = false;
    failNext = true;
    try {
        op(writer);
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    const bool consumed = !failNext;
    failNext = false;
    CHECK_FALSE(threw);
    CHECK(consumed);  // the call did reach the heap
    if (threw) return;
    suffix(writer);
    auto done = writer.Finish();
    REQUIRE_FALSE(done.has_value());
    CHECK(done.error().code == JsonErrorCode::OutOfMemory);
    CHECK(out == "kept");
}

} // namespace

TEST_CASE("Writer: an allocation failure in any call is OutOfMemory from Finish") {
    const auto nothing = [](Writer&) {};
    const auto inArray = [](Writer& w) { w.BeginArray(); };
    const auto endArray = [](Writer& w) { w.EndArray(); };
    const auto inObject = [](Writer& w) { w.BeginObject(); };
    ExpectOutOfMemory("BeginArray", nothing, [](Writer& w) { w.BeginArray(); }, endArray);
    ExpectOutOfMemory("EndArray", inArray, endArray, nothing);
    ExpectOutOfMemory("Key", inObject, [](Writer& w) { w.Key("k"); }, [](Writer& w) { w.Null(); w.EndObject(); });
    ExpectOutOfMemory("String", inArray, [](Writer& w) { w.String("s"); }, endArray);
    ExpectOutOfMemory("Int", inArray, [](Writer& w) { w.Int(-1); }, endArray);
    ExpectOutOfMemory("Uint", inArray, [](Writer& w) { w.Uint(1); }, endArray);
    ExpectOutOfMemory("Double", inArray, [](Writer& w) { w.Double(1.5); }, endArray);
    ExpectOutOfMemory("Bool", inArray, [](Writer& w) { w.Bool(true); }, endArray);
    ExpectOutOfMemory("Null", inArray, [](Writer& w) { w.Null(); }, endArray);
    auto number = Parse("12");
    REQUIRE(number.has_value());
    const GYO::Engine::Json::Value root = number->Root();
    ExpectOutOfMemory("WriteValue of a number", inArray, [root](Writer& w) { WriteValue(w, root); }, endArray);
}
