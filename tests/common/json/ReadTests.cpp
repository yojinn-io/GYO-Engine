// Reading: strict parsing, repeated keys, kinds, walking, positions, lifetime,
// threads and size (Design Invariants J-2, J-7, J-8, J-9 of
// docs/architecture/target/v2/json-module-declaration.zh-Hant.md).
#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "engine/json/Document.hpp"

using namespace std::string_view_literals;
using GYO::Engine::Json::ArrayView;
using GYO::Engine::Json::Document;
using GYO::Engine::Json::JsonErrorCode;
using GYO::Engine::Json::Kind;
using GYO::Engine::Json::Member;
using GYO::Engine::Json::ObjectView;
using GYO::Engine::Json::Parse;
using GYO::Engine::Json::Value;

namespace {

JsonErrorCode ParseError(const std::string_view text) {
    auto document = Parse(text);
    REQUIRE_FALSE(document.has_value());
    return document.error().code;
}

std::string ParseDetail(const std::string_view text) {
    auto document = Parse(text);
    REQUIRE_FALSE(document.has_value());
    return document.error().detail;
}

Document MustParse(const std::string_view text) {
    auto document = Parse(text);
    REQUIRE(document.has_value());
    return std::move(*document);
}

} // namespace

TEST_CASE("Parse: accepts RFC 8259 text, top-level scalars included") {
    CHECK(Parse(R"({"a":[1,2.5,"x",true,false,null],"b":{}})"sv).has_value());
    CHECK(Parse("[]"sv).has_value());
    CHECK(Parse("\"text\""sv).has_value());
    CHECK(Parse("42"sv).has_value());
    CHECK(Parse(" \t\r\n null \n"sv).has_value());
    CHECK(Parse(R"(["é𝄞/\/"])"sv).has_value());
}

TEST_CASE("Parse: what is not JSON is Syntax") {
    CHECK(ParseError(""sv) == JsonErrorCode::Syntax);
    CHECK(ParseError("   "sv) == JsonErrorCode::Syntax);
    CHECK(ParseError("\xEF\xBB\xBF{}"sv) == JsonErrorCode::Syntax);  // byte order mark
    CHECK(ParseError("{} {}"sv) == JsonErrorCode::Syntax);           // trailing content
    CHECK(ParseError("123\0"sv) == JsonErrorCode::Syntax);           // a number followed by NUL
    CHECK(ParseError("[NaN]"sv) == JsonErrorCode::Syntax);
    CHECK(ParseError("[Infinity]"sv) == JsonErrorCode::Syntax);
    CHECK(ParseError("[1,]"sv) == JsonErrorCode::Syntax);
    CHECK(ParseError("[1] // note"sv) == JsonErrorCode::Syntax);
    CHECK(ParseError("{'a':1}"sv) == JsonErrorCode::Syntax);
    CHECK(ParseError("[01]"sv) == JsonErrorCode::Syntax);
    CHECK(ParseError(R"(["\ud800"])"sv) == JsonErrorCode::Syntax);        // lone high surrogate
    CHECK(ParseError(R"(["\udc00"])"sv) == JsonErrorCode::Syntax);        // lone low surrogate
    CHECK(ParseError(R"(["\ud800A"])"sv) == JsonErrorCode::Syntax);  // high + not low
    CHECK(ParseError("[\"\xC0\xAF\"]"sv) == JsonErrorCode::Syntax);       // overlong
    CHECK(ParseError("[\"\xED\xA0\x80\"]"sv) == JsonErrorCode::Syntax);   // UTF-8 surrogate
    CHECK(ParseError("[\"\xF4\x90\x80\x80\"]"sv) == JsonErrorCode::Syntax); // above U+10FFFF
    CHECK(ParseError("[\"\xE2\x82\"]"sv) == JsonErrorCode::Syntax);       // truncated sequence
    CHECK(ParseError("[\"a\x01\"]"sv) == JsonErrorCode::Syntax);          // raw control character
}

TEST_CASE("Parse: a syntax error names its line and column in bytes") {
    // yyjson reports a trailing comma at the comma itself.
    CHECK(ParseDetail("{\n  \"a\": [1, 2,]\n}"sv) == "line 2, column 13");
    CHECK(ParseDetail("[1] x"sv) == "line 1, column 5");
    CHECK(ParseDetail(""sv) == "line 1, column 1");
}

TEST_CASE("Parse: a key repeated in one object is RepeatedKey, at the second occurrence") {
    CHECK(ParseError(R"({"a":1,"a":2})"sv) == JsonErrorCode::RepeatedKey);
    // Keys compare after unescaping.
    CHECK(ParseError(R"({"a":1,"a":2})"sv) == JsonErrorCode::RepeatedKey);
    CHECK(ParseDetail("{\"x\":[{\"k\":1,\n \"k\":2}]}"sv) == "line 2, column 3; /x/0/k");
    // The same key in different objects is fine.
    CHECK(Parse(R"({"a":{"a":1},"b":{"a":2}})"sv).has_value());
    // A key that differs only after U+0000 is a different key.
    CHECK(Parse(R"({"a\u0000b":1,"a\u0000c":2})"sv).has_value());
}

TEST_CASE("Parse: repeated keys are found in large objects too") {
    std::string text = "{";
    for (int i = 0; i < 1000; ++i) text += "\"k" + std::to_string(i) + "\":0,";
    text += "\"k500\":1}";
    CHECK(ParseError(text) == JsonErrorCode::RepeatedKey);
    CHECK(ParseDetail(text).ends_with("; /k500"));
}

TEST_CASE("Parse: JSON Pointer escapes '~' and '/' in keys") {
    CHECK(ParseDetail(R"({"a/b":{"c~d":{"e":1,"e":2}}})"sv) == "line 1, column 23; /a~1b/c~0d/e");
}

TEST_CASE("Value: kinds and the scalar reads") {
    const Document document = MustParse(R"([null,true,0,"s",[],{}])"sv);
    auto root = document.Root().AsArray();
    REQUIRE(root);
    std::vector<Kind> kinds;
    for (const Value value : *root) kinds.push_back(value.GetKind());
    CHECK(kinds == std::vector<Kind>{Kind::Null, Kind::Bool, Kind::Number, Kind::String, Kind::Array, Kind::Object});

    auto it = root->begin();
    ++it;
    CHECK((*it).AsBool().value() == true);
    ++it;
    ++it;
    CHECK((*it).AsString().value() == "s");
}

TEST_CASE("Value: reading the wrong kind is WrongKind with what was expected and found") {
    const Document document = MustParse(R"({"n":1,"s":"x"})"sv);
    const Value root = document.Root();
    auto asArray = root.AsArray();
    REQUIRE_FALSE(asArray);
    CHECK(asArray.error().code == JsonErrorCode::WrongKind);
    CHECK(asArray.error().detail == "expected Array, found Object");

    auto object = root.AsObject();
    REQUIRE(object);
    const Value s = object->Find("s").value();
    CHECK(s.AsBool().error().code == JsonErrorCode::WrongKind);
    CHECK(s.AsInt64().error().code == JsonErrorCode::WrongKind);
    CHECK(s.AsDouble().error().code == JsonErrorCode::WrongKind);
    CHECK(s.AsNumberText().error().code == JsonErrorCode::WrongKind);
    CHECK(s.AsObject().error().code == JsonErrorCode::WrongKind);
    CHECK(object->Find("n").value().AsString().error().detail == "expected String, found Number");
}

TEST_CASE("ObjectView and ArrayView: document order, Size and Find") {
    const Document document = MustParse(R"({"z":1,"a":[3,2,1],"m":"s"})"sv);
    auto root = document.Root().AsObject();
    REQUIRE(root);
    CHECK(root->Size() == 3);
    std::vector<std::string_view> keys;
    for (const Member& member : *root) keys.push_back(member.key);
    CHECK(keys == std::vector<std::string_view>{"z", "a", "m"});
    CHECK_FALSE(root->Find("missing").has_value());
    auto array = root->Find("a").value().AsArray();
    REQUIRE(array);
    CHECK(array->Size() == 3);
    std::vector<std::string_view> texts;
    for (const Value value : *array) {
        auto text = value.AsNumberText();
        CHECK(text.has_value());
        if (text) texts.push_back(*text);
    }
    CHECK(texts == std::vector<std::string_view>{"3", "2", "1"});

    const Document empty = MustParse("[{},[]]"sv);
    auto outer = empty.Root().AsArray();
    REQUIRE(outer);
    auto first = (*outer->begin()).AsObject();
    REQUIRE(first);
    CHECK(first->Size() == 0);
    CHECK(first->begin() == first->end());
}

TEST_CASE("ObjectView and ArrayView: walking steps over nested containers") {
    // Chosen so that a walk entering the nested values reads wrong members
    // without running past the end.
    const Document object = MustParse(R"({"a":["x","y","z"],"b":"v","c":"w"})"sv);
    std::vector<std::string_view> keys;
    for (const Member& member : object.Root().AsObject().value()) keys.push_back(member.key);
    CHECK(keys == std::vector<std::string_view>{"a", "b", "c"});

    const Document array = MustParse(R"([[1,2],3,4])"sv);
    std::vector<Kind> kinds;
    for (const Value value : array.Root().AsArray().value()) kinds.push_back(value.GetKind());
    CHECK(kinds == std::vector<Kind>{Kind::Array, Kind::Number, Kind::Number});
    std::vector<std::string_view> tail;
    for (const Value value : array.Root().AsArray().value()) {
        if (value.GetKind() == Kind::Number) tail.push_back(value.AsNumberText().value());
    }
    CHECK(tail == std::vector<std::string_view>{"3", "4"});
}

TEST_CASE("Document: does not refer to the parsed text; views survive a move") {
    std::string text = R"({"key":"value","n":12.5})";
    Document document = MustParse(text);
    for (char& c : text) c = '#';
    text.clear();
    text.shrink_to_fit();
    const Value root = document.Root();
    auto object = root.AsObject();
    REQUIRE(object);
    const std::string_view value = object->Find("key").value().AsString().value();
    CHECK((*object->begin()).key == "key");
    CHECK(value == "value");

    Document moved = std::move(document);
    CHECK(value == "value");
    CHECK(object->Find("n").value().AsNumberText().value() == "12.5");
    CHECK(moved.Root().AsObject().value().Size() == 2);
}

TEST_CASE("Parse: several threads parse different documents and read one together") {
    std::vector<std::string> texts;
    for (int t = 0; t < 8; ++t) {
        std::string text = "[";
        for (int i = 0; i < 2000; ++i) text += "{\"t\":" + std::to_string(t) + ",\"i\":" + std::to_string(i) + "},";
        text.back() = ']';
        texts.push_back(std::move(text));
    }
    std::vector<std::size_t> counts(texts.size(), 0);
    std::vector<std::thread> threads;
    for (std::size_t t = 0; t < texts.size(); ++t) {
        threads.emplace_back([&, t] {
            auto document = Parse(texts[t]);
            if (!document) return;
            for (const Value element : document->Root().AsArray().value()) {
                if (element.AsObject().value().Find("t").value().AsNumberText().value() == std::to_string(t)) ++counts[t];
            }
        });
    }
    for (std::thread& thread : threads) thread.join();
    for (const std::size_t count : counts) CHECK(count == 2000);

    const Document shared = MustParse(texts[0]);
    std::vector<std::size_t> seen(8, 0);
    threads.clear();
    for (std::size_t t = 0; t < seen.size(); ++t) {
        threads.emplace_back([&, t] {
            for (const Value element : shared.Root().AsArray().value()) {
                if (element.AsObject().value().Size() == 2) ++seen[t];
            }
        });
    }
    for (std::thread& thread : threads) thread.join();
    for (const std::size_t count : seen) CHECK(count == 2000);
}

TEST_CASE("Parse: deep and wide documents are linear and need no recursion") {
    constexpr std::size_t depth = 1000000;
    std::string deep(depth, '[');
    deep.append(depth, ']');
    const Document document = MustParse(deep);
    std::size_t levels = 0;
    bool allArrays = true;
    Value value = document.Root();
    while (true) {
        auto array = value.AsArray();
        if (!array) {
            allArrays = false;
            break;
        }
        ++levels;
        if (array->Size() == 0) break;
        value = *array->begin();
    }
    CHECK(allArrays);
    CHECK(levels == depth);

    // A repeated key one million levels down still reports, with its path.
    std::string deepKey;
    for (std::size_t i = 0; i < 100000; ++i) deepKey += "{\"a\":";
    deepKey += "{\"k\":1,\"k\":2}";
    deepKey.append(100000, '}');
    CHECK(ParseError(deepKey) == JsonErrorCode::RepeatedKey);

    std::string wideArray = "[";
    for (int i = 0; i < 100000; ++i) wideArray += "{\"i\":1},";
    wideArray.back() = ']';
    CHECK(MustParse(wideArray).Root().AsArray().value().Size() == 100000);

    std::string wideObject = "{";
    for (int i = 0; i < 10000; ++i) wideObject += "\"k" + std::to_string(i) + "\":" + std::to_string(i) + ",";
    wideObject.back() = '}';
    CHECK(MustParse(wideObject).Root().AsObject().value().Size() == 10000);
}
