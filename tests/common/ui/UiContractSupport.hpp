#pragma once

// Shared helpers of the gyo.ui Data Contract tests (Design Invariant UI-2).
//
// The documents in contract/ (read at run time from GYO_UI_CONTRACT_DIR):
// - document_v1.json: master's kDocument (tests/common/ui/UiTestDocument.hpp), version 1 with UTF-8 text.
// - document_v1_master_canonical.json: master's Serialize output for it, frozen on master 43bccad
//   (kDocumentCanonicalGolden of tests/common/ui/UiDocumentCodecTests.cpp), two-space indentation.
// - golden_v1.json, golden_v1_master_canonical.json: master's kGoldenSourceDocument and kGoldenCanonicalDocument
//   (tests/common/ui/UiGoldenDocument.hpp), ASCII only.
// - golden_v2.json: master's kTextFieldDocument (tests/common/ui/UiTextFieldTests.cpp), version 2 with a text_field.
// - *_canonical.json without "master": what Serialize writes now, one line (the JSON Module's one format, J-5). The
//   tests check that they hold the same JSON values as master's frozen output (JsonValuesEqual).
// - pvp_lobby.json: master's assets/object_fps_pvp/ui/pvp_lobby.json.
// contract/.gitattributes keeps them byte for byte (no line ending conversion).

#include <doctest/doctest.h>

#include <cstddef>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "engine/json/Document.hpp"
#include "engine/ui/UiDocumentCodec.hpp"

namespace GYO::Test::Ui {

using namespace GYO::Engine::Ui;
namespace Json = GYO::Engine::Json;

[[nodiscard]] inline std::string ReadContractFile(const std::string_view name) {
    const std::string path = std::string(GYO_UI_CONTRACT_DIR) + "/" + std::string(name);
    std::ifstream file(path, std::ios::binary);
    REQUIRE_MESSAGE(file.good(), "cannot open " << path);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// text with its first occurrence of from replaced; from must occur.
[[nodiscard]] inline std::string Replace(std::string text, const std::string_view from, const std::string_view to) {
    const std::size_t at = text.find(from);
    REQUIRE_MESSAGE(at != std::string::npos, "missing \"" << std::string(from) << "\"");
    text.replace(at, from.size(), to);
    return text;
}

[[nodiscard]] inline UiDocument ParseOrFail(const std::string_view json, const std::string_view source = "contract") {
    auto parsed = UiDocumentCodec::Parse(json, source);
    REQUIRE_MESSAGE(parsed.has_value(), (parsed ? std::string() : parsed.error().message + " at " +
                                                                     parsed.error().jsonPointer));
    return std::move(parsed).value();
}

[[nodiscard]] inline std::string SerializeOrFail(const UiDocument& document) {
    auto serialized = UiDocumentCodec::Serialize(document);
    REQUIRE_MESSAGE(serialized.has_value(), (serialized ? std::string() : serialized.error().message + " at " +
                                                                              serialized.error().jsonPointer));
    return std::move(serialized).value();
}

[[nodiscard]] inline std::string SerializeParsed(const std::string_view json) {
    return SerializeOrFail(ParseOrFail(json));
}

[[nodiscard]] inline UiElement* FindElementById(std::vector<UiElement>& elements, const std::string_view id) {
    for (UiElement& element : elements) {
        if (element.id == id) return &element;
        if (UiElement* found = FindElementById(element.children, id)) return found;
        if (UiElement* found = FindElementById(element.itemTemplate, id)) return found;
    }
    return nullptr;
}

[[nodiscard]] inline const UiElement& FindChild(const std::vector<UiElement>& elements, const std::string_view id) {
    for (const UiElement& element : elements) {
        if (element.id == id) return element;
    }
    FAIL("missing element " << std::string(id));
    return elements.front();
}

namespace Detail {

// Two parsed values are the same JSON: same kinds, numbers equal as doubles and both integer literals or both not,
// strings byte for byte, object members in the same order. The texts of numbers may differ (1e+30 and 1e30).
[[nodiscard]] inline bool SameValue(const Json::Value a, const Json::Value b, const std::string& pointer,
                                    std::string& where) {
    where = pointer;
    if (a.GetKind() != b.GetKind()) return false;
    switch (a.GetKind()) {
    case Json::Kind::Null:
        return true;
    case Json::Kind::Bool:
        return *a.AsBool() == *b.AsBool();
    case Json::Kind::Number:
        return a.IsIntegerLiteral() == b.IsIntegerLiteral() && *a.AsDouble() == *b.AsDouble();
    case Json::Kind::String:
        return *a.AsString() == *b.AsString();
    case Json::Kind::Array: {
        const Json::ArrayView left = *a.AsArray();
        const Json::ArrayView right = *b.AsArray();
        if (left.Size() != right.Size()) return false;
        auto r = right.begin();
        std::size_t index = 0;
        for (const Json::Value l : left) {
            if (!SameValue(l, *r, pointer + "/" + std::to_string(index++), where)) return false;
            ++r;
        }
        return true;
    }
    case Json::Kind::Object: {
        const Json::ObjectView left = *a.AsObject();
        const Json::ObjectView right = *b.AsObject();
        if (left.Size() != right.Size()) return false;
        auto r = right.begin();
        for (const Json::Member l : left) {
            const Json::Member rm = *r;
            if (l.key != rm.key) {
                where = pointer + "/" + std::string(l.key);
                return false;
            }
            if (!SameValue(l.value, rm.value, pointer + "/" + std::string(l.key), where)) return false;
            ++r;
        }
        return true;
    }
    }
    return false;
}

} // namespace Detail

// Both texts parse and hold the same JSON values in the same order (see Detail::SameValue).
inline void CheckJsonValuesEqual(const std::string_view a, const std::string_view b) {
    auto left = Json::Parse(a);
    auto right = Json::Parse(b);
    REQUIRE(left.has_value());
    REQUIRE(right.has_value());
    std::string where;
    const bool same = Detail::SameValue(left->Root(), right->Root(), "", where);
    CHECK_MESSAGE(same, "the documents differ at \"" << where << "\"");
}

} // namespace GYO::Test::Ui
