#include <doctest/doctest.h>

#include "UiTestDocument.hpp"
#include "ui/UiDocumentCodec.hpp"

#include <string>

namespace Engine::Ui::Tests {

TEST_CASE("UiDocumentCodec parses UTF-8 and converts sRGB colors to linear") {
    auto parsed = UiDocumentCodec::Parse(kDocument, "memory://test-ui");
    REQUIRE(parsed);
    CHECK(parsed.value().canvases.size() == 1);
    CHECK(parsed.value().canvases[0].children.size() == 5);
    CHECK(parsed.value().colors.at("gray").red == doctest::Approx(0.21586F).epsilon(0.0001));
    CHECK(parsed.value().colors.at("gray").alpha == doctest::Approx(1.0F));
}

TEST_CASE("UiDocumentCodec emits deterministic canonical UTF-8 JSON") {
    auto parsed = UiDocumentCodec::Parse(kDocument);
    REQUIRE(parsed);
    auto first = UiDocumentCodec::Serialize(parsed.value());
    REQUIRE(first);
    auto second = UiDocumentCodec::Serialize(parsed.value());
    REQUIRE(second);
    CHECK(first.value() == second.value());
    CHECK(first.value().ends_with("\n"));
    CHECK(first.value().find("ゲームスタート") != std::string::npos);
    CHECK(first.value().find("#808080FF") != std::string::npos);
    CHECK(first.value().find("\"schema\"") < first.value().find("\"version\""));

    auto roundTrip = UiDocumentCodec::Parse(first.value());
    REQUIRE(roundTrip);
    CHECK(roundTrip.value().canvases[0].focusOrder.size() == 3);
}

TEST_CASE("UiDocumentCodec rejects unknown properties and incomplete focus order") {
    std::string unknown(kDocument);
    unknown.replace(unknown.find("\"version\": 1"), 12, "\"version\": 1, \"mystery\": true");
    auto unknownResult = UiDocumentCodec::Parse(unknown, "unknown.json");
    REQUIRE_FALSE(unknownResult);
    CHECK(unknownResult.error().source == "unknown.json");
    CHECK(unknownResult.error().jsonPointer == "/mystery");

    std::string incomplete(kDocument);
    incomplete.replace(
        incomplete.find("[\"button_one\", \"button_two\", \"gamma_slider\"]"),
        std::string("[\"button_one\", \"button_two\", \"gamma_slider\"]").size(),
        "[\"button_one\", \"button_two\"]");
    auto incompleteResult = UiDocumentCodec::Parse(incomplete);
    REQUIRE_FALSE(incompleteResult);
    CHECK(incompleteResult.error().jsonPointer.find("focus_order") != std::string::npos);
}

TEST_CASE("UiDocumentCodec rejects interactive list template children") {
    auto parsed = UiDocumentCodec::Parse(kDocument);
    REQUIRE(parsed);
    UiElement& list = parsed.value().canvases[0].children[3];
    list.itemTemplate[0].type = UiElementType::Button;
    list.itemTemplate[0].action = "one";
    list.itemTemplate[0].background = {"normal", "focused", "pressed"};
    auto validation = UiDocumentCodec::Validate(parsed.value());
    REQUIRE_FALSE(validation);
    CHECK(validation.error().message.find("forbidden") != std::string::npos);
}

TEST_CASE("UiDocumentCodec enforces typed slider bindings") {
    auto parsed = UiDocumentCodec::Parse(kDocument);
    REQUIRE(parsed);
    parsed.value().canvases[0].children[2].binding = "rooms";
    auto validation = UiDocumentCodec::Validate(parsed.value());
    REQUIRE_FALSE(validation);
    CHECK(validation.error().code == UiErrorCode::BindingTypeMismatch);
}

TEST_CASE("sRGB hex codec keeps alpha linear and canonicalizes case") {
    auto decoded = DecodeSrgbHexColor("#ff800080");
    REQUIRE(decoded);
    CHECK(decoded.value().red == doctest::Approx(1.0F));
    CHECK(decoded.value().green == doctest::Approx(0.21586F).epsilon(0.0001));
    CHECK(decoded.value().alpha == doctest::Approx(128.0F / 255.0F));
    CHECK(EncodeSrgbHexColor(decoded.value()) == "#FF800080");
}

TEST_CASE("UiDocumentCodec rejects unsupported schema and version") {
    std::string schema(kDocument);
    schema.replace(
        schema.find("\"schema\": \"gyo.ui\""),
        std::string("\"schema\": \"gyo.ui\"").size(),
        "\"schema\": \"other.ui\"");
    auto schemaResult = UiDocumentCodec::Parse(schema);
    REQUIRE_FALSE(schemaResult);
    CHECK(schemaResult.error().code == UiErrorCode::UnsupportedSchema);

    std::string version(kDocument);
    version.replace(
        version.find("\"version\": 1"),
        std::string("\"version\": 1").size(),
        "\"version\": 2");
    auto versionResult = UiDocumentCodec::Parse(version);
    REQUIRE_FALSE(versionResult);
    CHECK(versionResult.error().code == UiErrorCode::UnsupportedVersion);
}

TEST_CASE("UiDocument validation rejects duplicate public ids") {
    auto parsed = UiDocumentCodec::Parse(kDocument);
    REQUIRE(parsed);

    SUBCASE("action") {
        parsed.value().actions.push_back(parsed.value().actions.front());
        CHECK_FALSE(UiDocumentCodec::Validate(parsed.value()));
    }
    SUBCASE("binding") {
        parsed.value().bindings.push_back(parsed.value().bindings.front());
        CHECK_FALSE(UiDocumentCodec::Validate(parsed.value()));
    }
    SUBCASE("canvas") {
        parsed.value().canvases.push_back(parsed.value().canvases.front());
        CHECK_FALSE(UiDocumentCodec::Validate(parsed.value()));
    }
    SUBCASE("element") {
        parsed.value().canvases[0].children.push_back(
            parsed.value().canvases[0].children[1]);
        CHECK_FALSE(UiDocumentCodec::Validate(parsed.value()));
    }
}

TEST_CASE("UiDocumentCodec rejects action and binding id collisions") {
    SUBCASE("in-memory validation reports the conflicting binding id") {
        auto parsed = UiDocumentCodec::Parse(kDocument);
        REQUIRE(parsed);
        parsed.value().bindings[0].id = parsed.value().actions[3].id;

        auto validation = UiDocumentCodec::Validate(parsed.value());
        REQUIRE_FALSE(validation);
        CHECK(validation.error().code == UiErrorCode::ValidationFailed);
        CHECK(validation.error().jsonPointer == "/bindings/0/id");
        CHECK(validation.error().message.find("globally unique") != std::string::npos);
    }

    SUBCASE("JSON parsing applies the same validation rule") {
        std::string collision(kDocument);
        collision.replace(
            collision.find("\"id\": \"gamma\""),
            std::string("\"id\": \"gamma\"").size(),
            "\"id\": \"set_gamma\"");

        auto parsed = UiDocumentCodec::Parse(collision, "collision.json");
        REQUIRE_FALSE(parsed);
        CHECK(parsed.error().code == UiErrorCode::ValidationFailed);
        CHECK(parsed.error().source == "collision.json");
        CHECK(parsed.error().jsonPointer == "/bindings/0/id");
    }
}

TEST_CASE("UiDocument validation enforces typed references and list item fields") {
    auto parsed = UiDocumentCodec::Parse(kDocument);
    REQUIRE(parsed);

    SUBCASE("selection requires enum or boolean") {
        parsed.value().canvases[0].children[4].text.selectionBinding = "gamma";
        auto validation = UiDocumentCodec::Validate(parsed.value());
        REQUIRE_FALSE(validation);
        CHECK(validation.error().code == UiErrorCode::BindingTypeMismatch);
    }
    SUBCASE("list preview item fields are exact") {
        UiBindingDeclaration& rooms = parsed.value().bindings[2];
        std::get<UiList>(rooms.preview).items[0].fields.erase("kills");
        auto validation = UiDocumentCodec::Validate(parsed.value());
        REQUIRE_FALSE(validation);
        CHECK(validation.error().message.find("fields") != std::string::npos);
    }
}

TEST_CASE("named placeholder compose rejects unknown, unused, and malformed formats") {
    auto parsed = UiDocumentCodec::Parse(kDocument);
    REQUIRE(parsed);
    UiTextSource& source =
        parsed.value().canvases[0].children[3].itemTemplate[0].text;

    SUBCASE("unknown") {
        source.composeFormat = "{missing}";
        CHECK_FALSE(UiDocumentCodec::Validate(parsed.value()));
    }
    SUBCASE("unused") {
        source.placeholders.emplace("unused", source.placeholders.at("name"));
        CHECK_FALSE(UiDocumentCodec::Validate(parsed.value()));
    }
    SUBCASE("unclosed") {
        source.composeFormat = "{name";
        CHECK_FALSE(UiDocumentCodec::Validate(parsed.value()));
    }
    SUBCASE("unmatched closing brace") {
        source.composeFormat = "{name}} {kills}";
        CHECK_FALSE(UiDocumentCodec::Validate(parsed.value()));
    }
}

} // namespace Engine::Ui::Tests
