#pragma once

// Helpers of the ui tests (docs/architecture/target/v2/ui.zh-Hant.md, section 6): a real font (the text Module's
// BIZ UDPGothic subset, which has ASCII letters and がっこう), documents read from text, and draw list queries.

#include <doctest/doctest.h>

#include <cstddef>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "engine/text/FontData.hpp"
#include "engine/ui/UiDocumentCodec.hpp"
#include "engine/ui/UiRuntime.hpp"
#include "engine/ui/UiText.hpp"

namespace GYO::Test::UiSupport {

namespace Ui = GYO::Engine::Ui;

inline std::string ReadFile(const char* path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), path);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

inline GYO::Engine::Text::FontData Font() {
    const std::string bytes = ReadFile(GYO_UI_TEST_FONT);
    std::vector<std::byte> data(bytes.size());
    for (std::size_t i = 0; i < bytes.size(); ++i) data[i] = static_cast<std::byte>(bytes[i]);
    auto font = GYO::Engine::Text::FontData::FromBytes(std::move(data));
    REQUIRE(font);
    return std::move(font).value();
}

// A UiText with the test font added under id.
inline Ui::UiText TextWithFont(const std::string& id = "test.font") {
    Ui::UiText text;
    REQUIRE(text.AddFont(id, Font()));
    return text;
}

inline std::shared_ptr<const Ui::UiDocument> Parse(std::string_view json) {
    auto parsed = Ui::UiDocumentCodec::Parse(json, "test");
    REQUIRE_MESSAGE(static_cast<bool>(parsed), (parsed ? std::string() : parsed.error().message + " " + parsed.error().jsonPointer));
    return std::make_shared<const Ui::UiDocument>(std::move(parsed).value());
}

inline Ui::UiRuntime Runtime(std::string_view json, std::string_view canvas) {
    Ui::UiRuntime runtime;
    REQUIRE(runtime.Initialize(Parse(json)));
    REQUIRE(runtime.ActivateCanvas(canvas));
    return runtime;
}

template <class T>
std::vector<T> All(const Ui::UiDrawList& list) {
    std::vector<T> out;
    for (const Ui::UiDrawCommand& command : list.commands) {
        if (const auto* draw = std::get_if<T>(&command)) out.push_back(*draw);
    }
    return out;
}

} // namespace GYO::Test::UiSupport
