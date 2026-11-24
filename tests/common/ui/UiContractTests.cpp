// The gyo.ui Data Contract (Design Invariant UI-2): master's codec tests (tests/common/ui/UiDocumentCodecTests.cpp and
// the codec cases of UiTextFieldTests.cpp), master's golden documents, pvp_lobby.json, and the JSON Module's strict
// reading. Every validation rule's reverse case is UiValidationRuleTests.cpp.

#include <doctest/doctest.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "UiContractSupport.hpp"
#include "engine/ui/UiDocumentCodec.hpp"

namespace GYO::Test::Ui {
namespace {

// master engine/ui/src/UiColor.cpp:24-28 (SrgbToLinear) frozen from master 43bccad; the input is formed as at :76-79.
// The volatile offset keeps the comparison at run time so both sides use the platform's std::pow.
volatile unsigned gByteOffset = 0U;

[[nodiscard]] float LegacySrgbToLinear(const float value) noexcept {
    return value <= 0.04045F ? value / 12.92F : std::pow((value + 0.055F) / 1.055F, 2.4F);
}

[[nodiscard]] std::string Document() {
    return ReadContractFile("document_v1.json");
}

[[nodiscard]] std::string TextFieldDocument() {
    return ReadContractFile("golden_v2.json");
}

[[nodiscard]] std::optional<UiFailureCode> Rejected(const std::string& text) {
    auto parsed = UiDocumentCodec::Parse(text, "text-field-test");
    return parsed ? std::optional<UiFailureCode>{} : std::optional<UiFailureCode>{parsed.error().code};
}

// The canonical golden: one line, one trailing newline, a fixed point of Parse -> Serialize, the output for the
// source document, and the same JSON values as master's frozen two-space output.
void CheckGolden(const std::string_view source, const std::string_view canonical, const std::string_view master) {
    const std::string golden = ReadContractFile(canonical);
    REQUIRE(golden.size() > 1U);
    CHECK(golden.back() == '\n');
    CHECK_MESSAGE(golden.find('\n') == golden.size() - 1U, "the canonical form is one line");

    CHECK(SerializeParsed(ReadContractFile(source)) == golden);
    CHECK(SerializeParsed(golden) == golden);
    if (!master.empty()) {
        const std::string frozen = ReadContractFile(master);
        CheckJsonValuesEqual(golden, frozen);
        CHECK_MESSAGE(SerializeParsed(frozen) == golden, "master's frozen output reads back to the same document");
    }
}

} // namespace

TEST_CASE("ui_contract: Parse reads UTF-8 and converts sRGB colours to linear") {
    const UiDocument document = ParseOrFail(Document(), "memory://test-ui");
    CHECK(document.version == 1U);
    REQUIRE(document.canvases.size() == 1U);
    CHECK(document.canvases[0].children.size() == 5U);
    CHECK(document.colors.at("gray").red == doctest::Approx(0.21586F).epsilon(0.0001));
    CHECK(document.colors.at("gray").alpha == 1.0F);
    CHECK(document.canvases[0].children[0].children[0].text.literal == "ゲームスタート");
}

TEST_CASE("ui_contract: Serialize is deterministic, one line, UTF-8 and in master's field order") {
    const UiDocument document = ParseOrFail(Document());
    const std::string first = SerializeOrFail(document);
    const std::string second = SerializeOrFail(document);
    CHECK(first == second);
    CHECK(first.ends_with("\n"));
    CHECK_FALSE(first.ends_with("\n\n"));
    CHECK(first.find('\n') == first.size() - 1U);
    CHECK(first.find("ゲームスタート") != std::string::npos);
    CHECK(first.find("#808080FF") != std::string::npos);
    CHECK(first.starts_with(R"({"schema":"gyo.ui","version":1,"design_canvas":{"size":[100.0,100.0],)"
                            R"("scale_mode":"fit"},"fonts":{"ui":"test.font"},"default_font":"ui","colors":{)"));

    const UiDocument roundTrip = ParseOrFail(first);
    CHECK(roundTrip.canvases[0].focusOrder.size() == 3U);
}

TEST_CASE("ui_contract: unknown properties and an incomplete focus order are rejected") {
    const std::string unknown = Replace(Document(), "\"version\": 1", "\"version\": 1, \"mystery\": true");
    auto unknownResult = UiDocumentCodec::Parse(unknown, "unknown.json");
    REQUIRE_FALSE(unknownResult);
    CHECK(unknownResult.error().code == UiFailureCode::ValidationFailed);
    CHECK(unknownResult.error().source == "unknown.json");
    CHECK(unknownResult.error().jsonPointer == "/mystery");

    const std::string incomplete = Replace(Document(), R"(["button_one", "button_two", "gamma_slider"])",
                                           R"(["button_one", "button_two"])");
    auto incompleteResult = UiDocumentCodec::Parse(incomplete);
    REQUIRE_FALSE(incompleteResult);
    CHECK(incompleteResult.error().code == UiFailureCode::ValidationFailed);
    CHECK(incompleteResult.error().jsonPointer == "/canvases/0/focus_order");
}

TEST_CASE("ui_contract: an interactive list template child is rejected") {
    UiDocument document = ParseOrFail(Document());
    UiElement& list = document.canvases[0].children[3];
    list.itemTemplate[0].type = UiElementType::Button;
    list.itemTemplate[0].action = "one";
    list.itemTemplate[0].background = {"normal", "focused", "pressed"};
    auto validation = UiDocumentCodec::Validate(document);
    REQUIRE_FALSE(validation);
    CHECK(validation.error().code == UiFailureCode::ValidationFailed);
    CHECK(validation.error().jsonPointer == "/canvases/0/children/3/template/0");
    CHECK(validation.error().message.find("forbidden") != std::string::npos);
}

TEST_CASE("ui_contract: slider bindings are typed") {
    UiDocument document = ParseOrFail(Document());
    document.canvases[0].children[2].binding = "rooms";
    auto validation = UiDocumentCodec::Validate(document);
    REQUIRE_FALSE(validation);
    CHECK(validation.error().code == UiFailureCode::BindingTypeMismatch);
    CHECK(validation.error().jsonPointer == "/canvases/0/children/2/binding");
}

TEST_CASE("ui_contract: the sRGB hex codec keeps alpha linear and writes uppercase") {
    auto decoded = DecodeSrgbHexColor("#ff800080");
    REQUIRE(decoded);
    CHECK(decoded.value().red == 1.0F);
    CHECK(decoded.value().green == doctest::Approx(0.21586F).epsilon(0.0001));
    CHECK(decoded.value().alpha == 128.0F / 255.0F);
    CHECK(EncodeSrgbHexColor(decoded.value()) == "#FF800080");

    for (const std::string_view bad : {"", "#", "ff8000", "#ff80", "#ff800", "#ff8000801", "#gg8000", "#ff80008z",
                                       "x#ff8000"}) {
        CAPTURE(bad);
        auto failed = DecodeSrgbHexColor(bad, "colors.json", "/colors/bad");
        REQUIRE_FALSE(failed);
        CHECK(failed.error().code == UiFailureCode::ValidationFailed);
        CHECK(failed.error().source == "colors.json");
        CHECK(failed.error().jsonPointer == "/colors/bad");
    }
}

TEST_CASE("ui_contract: an unsupported schema or version is rejected") {
    auto schemaResult = UiDocumentCodec::Parse(Replace(Document(), R"("schema": "gyo.ui")", R"("schema": "other.ui")"));
    REQUIRE_FALSE(schemaResult);
    CHECK(schemaResult.error().code == UiFailureCode::UnsupportedSchema);
    CHECK(schemaResult.error().jsonPointer == "/schema");

    // Version 2 is supported; 0, 3 and -1 are not.
    for (const char* unsupported : {"\"version\": 0", "\"version\": 3", "\"version\": -1"}) {
        CAPTURE(unsupported);
        auto versionResult = UiDocumentCodec::Parse(Replace(Document(), "\"version\": 1", unsupported));
        REQUIRE_FALSE(versionResult);
        CHECK(versionResult.error().code == UiFailureCode::UnsupportedVersion);
        CHECK(versionResult.error().jsonPointer == "/version");
    }
    CHECK(UiDocumentCodec::Parse(Replace(Document(), "\"version\": 1", "\"version\": 2")).has_value());
}

TEST_CASE("ui_contract: duplicate public ids are rejected") {
    UiDocument document = ParseOrFail(Document());
    const auto expectDuplicate = [&document](const std::string_view pointer) {
        auto validation = UiDocumentCodec::Validate(document);
        REQUIRE_FALSE(validation);
        CHECK(validation.error().code == UiFailureCode::ValidationFailed);
        CHECK(validation.error().jsonPointer == pointer);
    };

    SUBCASE("action") {
        document.actions.push_back(document.actions.front());
        expectDuplicate("/actions/4/id");
    }
    SUBCASE("binding") {
        document.bindings.push_back(document.bindings.front());
        expectDuplicate("/bindings/3/id");
    }
    SUBCASE("canvas") {
        document.canvases.push_back(document.canvases.front());
        expectDuplicate("/canvases/1/id");
    }
    SUBCASE("element") {
        document.canvases[0].children.push_back(document.canvases[0].children[1]);
        expectDuplicate("/canvases/0/children/5/id");
    }
}

TEST_CASE("ui_contract: action and binding ids share one namespace") {
    SUBCASE("in-memory validation reports the conflicting binding id") {
        UiDocument document = ParseOrFail(Document());
        document.bindings[0].id = document.actions[3].id;
        auto validation = UiDocumentCodec::Validate(document);
        REQUIRE_FALSE(validation);
        CHECK(validation.error().code == UiFailureCode::ValidationFailed);
        CHECK(validation.error().jsonPointer == "/bindings/0/id");
        CHECK(validation.error().message.find("globally unique") != std::string::npos);
    }
    SUBCASE("JSON parsing applies the same validation rule") {
        const std::string collision = Replace(Document(), R"("id": "gamma")", R"("id": "set_gamma")");
        auto parsed = UiDocumentCodec::Parse(collision, "collision.json");
        REQUIRE_FALSE(parsed);
        CHECK(parsed.error().code == UiFailureCode::ValidationFailed);
        CHECK(parsed.error().source == "collision.json");
        CHECK(parsed.error().jsonPointer == "/bindings/0/id");
    }
}

TEST_CASE("ui_contract: references and list item fields are typed") {
    UiDocument document = ParseOrFail(Document());

    SUBCASE("a selection needs an enum or boolean binding") {
        document.canvases[0].children[4].text.selectionBinding = "gamma";
        auto validation = UiDocumentCodec::Validate(document);
        REQUIRE_FALSE(validation);
        CHECK(validation.error().code == UiFailureCode::BindingTypeMismatch);
        CHECK(validation.error().jsonPointer == "/canvases/0/children/4/text/binding");
    }
    SUBCASE("list preview item fields are exact") {
        std::get<UiList>(document.bindings[2].preview).items[0].fields.erase("kills");
        auto validation = UiDocumentCodec::Validate(document);
        REQUIRE_FALSE(validation);
        CHECK(validation.error().code == UiFailureCode::ValidationFailed);
        CHECK(validation.error().jsonPointer == "/bindings/2/preview/0");
        CHECK(validation.error().message.find("fields") != std::string::npos);
    }
}

TEST_CASE("ui_contract: fixed_step_list item_step must be finite in float") {
    SUBCASE("non-finite values set in memory are rejected with their location") {
        for (const float bad : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                                -std::numeric_limits<float>::infinity()}) {
            for (int axis = 0; axis < 2; ++axis) {
                UiDocument document = ParseOrFail(Document());
                UiElement* list = FindElementById(document.canvases[0].children, "room_list");
                REQUIRE(list != nullptr);
                (axis == 0 ? list->itemStep.x : list->itemStep.y) = bad;
                const auto validation = UiDocumentCodec::Validate(document);
                REQUIRE_FALSE(validation);
                CHECK(validation.error().code == UiFailureCode::ValidationFailed);
                CHECK(validation.error().jsonPointer == "/canvases/0/children/3/item_step");
            }
        }
    }
    SUBCASE("a finite JSON number beyond the float range is rejected") {
        for (const char* beyond : {"[0, 1e39]", "[-1e39, 0]", "[0, 3.4028236e38]"}) {
            CAPTURE(beyond);
            const auto parsed = UiDocumentCodec::Parse(
                Replace(Document(), "\"item_step\": [0, 9]", std::string("\"item_step\": ") + beyond));
            REQUIRE_FALSE(parsed);
            CHECK(parsed.error().code == UiFailureCode::ValidationFailed);
            CHECK(parsed.error().jsonPointer == "/canvases/0/children/3/item_step");
        }
    }
    SUBCASE("the largest finite float is still accepted, also from a double that rounds to it") {
        for (const char* largest : {"[0, 3.4028234663852886e38]", "[0, 3.4028235e38]"}) {
            CAPTURE(largest);
            const UiDocument document =
                ParseOrFail(Replace(Document(), "\"item_step\": [0, 9]", std::string("\"item_step\": ") + largest));
            CHECK(document.canvases[0].children[3].itemStep.y == std::numeric_limits<float>::max());
        }
    }
}

TEST_CASE("ui_contract: compose rejects unknown, unused and malformed formats") {
    UiDocument document = ParseOrFail(Document());
    UiTextSource& source = document.canvases[0].children[3].itemTemplate[0].text;
    const std::string at = "/canvases/0/children/3/template/0/text";
    const auto expectInvalid = [&document](const std::string& pointer) {
        auto validation = UiDocumentCodec::Validate(document);
        REQUIRE_FALSE(validation);
        CHECK(validation.error().code == UiFailureCode::ValidationFailed);
        CHECK(validation.error().jsonPointer == pointer);
    };

    SUBCASE("unknown") {
        source.composeFormat = "{missing}";
        expectInvalid(at + "/format");
    }
    SUBCASE("invalid name") {
        source.placeholders.emplace("9lives", source.placeholders.at("name"));
        source.composeFormat = "{name} {kills} {9lives}";
        expectInvalid(at + "/format");
    }
    SUBCASE("unused") {
        source.placeholders.emplace("unused", source.placeholders.at("name"));
        expectInvalid(at + "/placeholders");
    }
    SUBCASE("unclosed") {
        source.composeFormat = "{name";
        expectInvalid(at + "/format");
    }
    SUBCASE("unmatched closing brace") {
        source.composeFormat = "{name}} {kills}";
        expectInvalid(at + "/format");
    }
    SUBCASE("escaped braces are text") {
        source.composeFormat = "{{{name}}} {kills}";
        CHECK(UiDocumentCodec::Validate(document));
    }
}

TEST_CASE("ui_contract: the golden of master's test document (v1, UTF-8)") {
    CheckGolden("document_v1.json", "document_v1_canonical.json", "document_v1_master_canonical.json");
}

TEST_CASE("ui_contract: the golden of master's synthetic document (v1)") {
    CheckGolden("golden_v1.json", "golden_v1_canonical.json", "golden_v1_master_canonical.json");

    // Spot checks of what the golden pins (the full comparisons above are authoritative).
    const std::string serialized = SerializeParsed(ReadContractFile("golden_v1.json"));
    CHECK(serialized.find(R"("schema":"gyo.ui")") != std::string::npos);
    CHECK(serialized.find(R"("version":1,)") != std::string::npos);
    CHECK_MESSAGE(serialized.find("0.10000000149011612") != std::string::npos, "0.1 after the float round trip");
    CHECK(serialized.find(R"("#A1B2C3D4")") != std::string::npos);
    CHECK_MESSAGE(serialized.find(R"("#102030FF")") != std::string::npos, "#RRGGBB gains FF");
    CHECK_MESSAGE(serialized.find(R"("preview":-3)") != std::string::npos, "an integer preview stays an integer");
}

TEST_CASE("ui_contract: the golden of master's text field document (v2)") {
    CheckGolden("golden_v2.json", "golden_v2_canonical.json", "");
}

TEST_CASE("ui_contract: sRGB hex colours round-trip every byte") {
    for (unsigned byte = 0; byte < 256U; ++byte) {
        CAPTURE(byte);
        std::array<char, 10> lower{};
        std::array<char, 10> upper{};
        std::snprintf(lower.data(), lower.size(), "#%02x%02x%02x%02x", byte, byte, byte, byte);
        std::snprintf(upper.data(), upper.size(), "#%02X%02X%02X%02X", byte, byte, byte, byte);
        auto decoded = DecodeSrgbHexColor(lower.data());
        REQUIRE(decoded);
        CHECK(EncodeSrgbHexColor(decoded.value()) == upper.data());
        // #RRGGBB defaults alpha to FF.
        auto opaque = DecodeSrgbHexColor(std::string_view(upper.data(), 7));
        REQUIRE(opaque);
        CHECK(EncodeSrgbHexColor(opaque.value()) == std::string(upper.data(), 7) + "FF");
    }

    // Decoded binary32 patterns. The linear segment and alpha are exact constants captured on master 43bccad;
    // everything that goes through std::pow is compared at run time with the decode frozen from master, because
    // libm pow may round differently across platforms.
    struct DecodedBits final {
        unsigned byte;
        std::uint32_t rgb;
        std::uint32_t alpha;
    };
    constexpr std::array<DecodedBits, 4> kDecoded{{
        {0U, 0x00000000U, 0x00000000U},
        {1U, 0x399F22B4U, 0x3B808081U},
        {10U, 0x3B46EB61U, 0x3D20A0A1U},
        {255U, 0x3F800000U, 0x3F800000U},
    }};
    for (const DecodedBits& expected : kDecoded) {
        CAPTURE(expected.byte);
        std::array<char, 10> hex{};
        std::snprintf(hex.data(), hex.size(), "#%02X%02X%02X%02X", expected.byte, expected.byte, expected.byte,
                      expected.byte);
        auto decoded = DecodeSrgbHexColor(hex.data());
        REQUIRE(decoded);
        CHECK(std::bit_cast<std::uint32_t>(decoded.value().red) == expected.rgb);
        CHECK(std::bit_cast<std::uint32_t>(decoded.value().green) == expected.rgb);
        CHECK(std::bit_cast<std::uint32_t>(decoded.value().blue) == expected.rgb);
        CHECK(std::bit_cast<std::uint32_t>(decoded.value().alpha) == expected.alpha);
    }

    for (unsigned byte = 0; byte < 256U; ++byte) {
        CAPTURE(byte);
        const float scaled = static_cast<float>(byte + gByteOffset) * (1.0F / 255.0F);
        const float expected = LegacySrgbToLinear(scaled);
        std::array<char, 10> hex{};
        std::snprintf(hex.data(), hex.size(), "#%02X%02X%02X%02X", byte, byte, byte, byte);
        auto decoded = DecodeSrgbHexColor(hex.data());
        REQUIRE(decoded);
        CHECK(std::bit_cast<std::uint32_t>(decoded.value().red) == std::bit_cast<std::uint32_t>(expected));
        CHECK(std::bit_cast<std::uint32_t>(decoded.value().alpha) == std::bit_cast<std::uint32_t>(scaled));
    }
}

TEST_CASE("ui_contract: Parse maps array positions to fields") {
    const UiDocument document = ParseOrFail(ReadContractFile("golden_v1.json"));

    // design_canvas.size [x, y]
    CHECK(document.designCanvas.size.x == 333.3F);
    CHECK(document.designCanvas.size.y == 187.5F);

    const UiCanvas& canvas = document.canvases.at(0);
    const UiElement& frame = FindChild(canvas.children, "frame");

    // source_uv [x, y, width, height]
    const UiElement& iconUv = FindChild(frame.children, "icon_uv");
    CHECK(iconUv.sourceUv.x == 0.125F);
    CHECK(iconUv.sourceUv.y == 0.25F);
    CHECK(iconUv.sourceUv.width == 0.5F);
    CHECK(iconUv.sourceUv.height == 0.75F);

    // Omitted source_uv keeps the full texture.
    const UiElement& iconFull = FindChild(frame.children, "icon_full");
    CHECK(iconFull.sourceUv.x == 0.0F);
    CHECK(iconFull.sourceUv.y == 0.0F);
    CHECK(iconFull.sourceUv.width == 1.0F);
    CHECK(iconFull.sourceUv.height == 1.0F);

    // rect.* [x, y]
    CHECK(iconUv.rect.anchorMin.x == 0.25F);
    CHECK(iconUv.rect.anchorMin.y == 0.125F);
    CHECK(iconUv.rect.anchorMax.x == 0.25F);
    CHECK(iconUv.rect.anchorMax.y == 0.125F);
    CHECK(iconUv.rect.pivot.x == 0.3F);
    CHECK(iconUv.rect.pivot.y == 0.7F);
    CHECK(iconUv.rect.position.x == 0.1F);
    CHECK(iconUv.rect.position.y == -0.2F);
    CHECK(iconUv.rect.sizeDelta.x == 48.0F);
    CHECK(iconUv.rect.sizeDelta.y == 32.0F);

    CHECK(iconFull.rect.anchorMin.x == 0.75F);
    CHECK(iconFull.rect.anchorMin.y == 0.5F);
    CHECK(iconFull.rect.anchorMax.x == 1.0F);
    CHECK(iconFull.rect.anchorMax.y == 0.75F);
    CHECK(iconFull.rect.pivot.x == 1.0F);
    CHECK(iconFull.rect.pivot.y == 0.0F);
    CHECK(iconFull.rect.position.x == -3.3F);
    CHECK(iconFull.rect.position.y == 4.4F);

    const UiElement& slider = FindChild(canvas.children, "volume_slider");
    CHECK(slider.rect.anchorMin.x == 0.1F);
    CHECK(slider.rect.anchorMin.y == 0.6F);
    CHECK(slider.rect.anchorMax.x == 0.9F);
    CHECK(slider.rect.anchorMax.y == 0.6F);
    CHECK(slider.rect.position.y == 0.3F);
    CHECK(slider.rect.sizeDelta.y == 22.2F);

    const UiElement& farAway = FindChild(canvas.children, "far_away");
    CHECK(farAway.rect.position.x == 1.0e30F);
    CHECK(farAway.rect.position.y == -2.5e-30F);
    CHECK(farAway.rect.sizeDelta.x == 1.0e-30F);
    CHECK(farAway.rect.sizeDelta.y == 3.0e20F);

    // item_step [x, y]
    const UiElement& list = FindChild(canvas.children, "entries_list");
    CHECK(list.itemStep.x == 0.5F);
    CHECK(list.itemStep.y == 10.1F);
    CHECK(list.rect.position.x == 20.2F);
    CHECK(list.rect.position.y == 30.3F);
    CHECK(list.rect.sizeDelta.x == 90.9F);
    CHECK(list.rect.sizeDelta.y == 8.8F);
}

TEST_CASE("ui_contract: version 2 reads, validates and writes text_field") {
    const UiDocument parsed = ParseOrFail(TextFieldDocument(), "text-field-test");
    CHECK(parsed.version == 2U);
    const std::string serialized = SerializeOrFail(parsed);
    const UiDocument reparsed = ParseOrFail(serialized, "round-trip");
    const UiElement& field = reparsed.canvases[0].children[0];
    CHECK(reparsed.version == 2U);
    CHECK(field.type == UiElementType::TextField);
    CHECK(field.binding == "address");
    CHECK(field.action == "submit_address");
    CHECK(field.maxLength == 8U);
    CHECK(field.placeholder == "host:port");
    CHECK(field.placeholderColor == "gray");
    CHECK_MESSAGE(field.textStyle.verticalAlign == UiVerticalAlign::Center, "a text field is centred by default");
    CHECK(field.textStyle.horizontalAlign == UiHorizontalAlign::Left);
    CHECK(field.background.pressed == "editing");
    CHECK(reparsed.actions[0].payload == UiActionPayloadType::String);
    CHECK_MESSAGE(SerializeOrFail(reparsed) == serialized, "the canonical form is stable");

    // The omitted background states default in turn: focused to normal, pressed to focused.
    const UiElement& go = reparsed.canvases[0].children[1];
    CHECK(go.background.normal == "normal");
    CHECK(go.background.focused == "normal");
    CHECK(go.background.pressed == "normal");
    CHECK(go.textStyle.horizontalAlign == UiHorizontalAlign::Center);
    CHECK(go.textStyle.verticalAlign == UiVerticalAlign::Center);
    CHECK(go.textStyle.pointSize == 16.0F);
}

TEST_CASE("ui_contract: a text field outside its contract is rejected") {
    const std::string document = TextFieldDocument();
    const std::string version1 = Replace(document, "\"version\": 2", "\"version\": 1");

    auto stringPayload = UiDocumentCodec::Parse(version1);
    REQUIRE_FALSE(stringPayload);
    CHECK_MESSAGE(stringPayload.error().code == UiFailureCode::UnsupportedVersion,
                  "version 1 has no string payload");
    CHECK(stringPayload.error().jsonPointer == "/actions/0/payload");

    auto textField = UiDocumentCodec::Parse(Replace(version1, R"({ "id": "submit_address", "payload": "string" })",
                                                    R"({ "id": "submit_address", "payload": "none" })"));
    REQUIRE_FALSE(textField);
    CHECK_MESSAGE(textField.error().code == UiFailureCode::UnsupportedVersion,
                  "a text_field in version 1 is a version error before any type mismatch");
    CHECK(textField.error().jsonPointer == "/canvases/0/children/0/type");

    CHECK(Rejected(Replace(document, "\"max_length\": 8", "\"max_length\": 0")) == UiFailureCode::ValidationFailed);
    CHECK(Rejected(Replace(document, R"("action": "submit_address")", R"("action": "go")")) ==
          UiFailureCode::BindingTypeMismatch);
    CHECK(Rejected(Replace(document, R"({ "id": "address", "type": "string", "preview": "" })",
                           R"({ "id": "address", "type": "number", "preview": 0 })")) ==
          UiFailureCode::BindingTypeMismatch);
    CHECK(Rejected(Replace(document, R"("placeholder": "host:port",)", "")) == UiFailureCode::ValidationFailed);
    CHECK_MESSAGE(Rejected(Replace(document, "\"max_length\": 8", R"("max_length": 8, "text": { "literal": "x" })")) ==
                      UiFailureCode::ValidationFailed,
                  "a text field shows its binding, not a text source");
    CHECK(Rejected(Replace(document, R"("focus_order": ["address_field", "go_button"])",
                           R"("focus_order": ["go_button"])")) == UiFailureCode::ValidationFailed);
}

TEST_CASE("ui_contract: master's pvp_lobby.json reads, validates and round-trips") {
    const std::string text = ReadContractFile("pvp_lobby.json");
    const UiDocument document = ParseOrFail(text, "pvp_lobby.json");
    CHECK(document.version == 2U);
    CHECK(document.designCanvas.size.x == 1280.0F);
    CHECK(document.designCanvas.size.y == 720.0F);
    REQUIRE(document.canvases.size() == 1U);
    const UiCanvas& lobby = document.canvases[0];
    CHECK(lobby.id == "lobby");
    CHECK(lobby.focusOrder == std::vector<std::string>{"address", "refresh", "create", "join", "leave", "quit"});
    CHECK(lobby.defaultFocus == std::optional<std::string>{"refresh"});
    CHECK_FALSE(lobby.cancelAction.has_value());

    const std::string serialized = SerializeOrFail(document);
    CHECK(SerializeParsed(serialized) == serialized);
    CheckJsonValuesEqual(SerializeParsed(text), serialized);
}

TEST_CASE("ui_contract: the strict JSON reader's rejections are InvalidJson") {
    const std::string document = Document();
    const auto expectInvalidJson = [](const std::string& text) {
        auto parsed = UiDocumentCodec::Parse(text, "strict.json");
        REQUIRE_FALSE(parsed);
        CHECK(parsed.error().code == UiFailureCode::InvalidJson);
        CHECK(parsed.error().source == "strict.json");
        CHECK(parsed.error().jsonPointer.empty());
        CHECK_FALSE(parsed.error().message.empty());
        return parsed.error().detail;
    };

    SUBCASE("a repeated key, which master's reader let through, names its place") {
        const std::string detail =
            expectInvalidJson(Replace(document, R"("default_font": "ui",)", R"("default_font": "ui", "default_font": "ui",)"));
        CHECK(detail.find("/default_font") != std::string::npos);
    }
    SUBCASE("a byte order mark") {
        expectInvalidJson("\xEF\xBB\xBF" + document);
    }
    SUBCASE("invalid UTF-8") {
        expectInvalidJson(Replace(document, "TWO", "T\xC0\xAFO"));
    }
    SUBCASE("syntax") {
        expectInvalidJson(Replace(document, R"("default_font": "ui",)", R"("default_font": "ui",,)"));
        expectInvalidJson("");
        expectInvalidJson(document + "{}");
    }
    SUBCASE("a number beyond double") {
        expectInvalidJson(Replace(document, "\"item_step\": [0, 9]", "\"item_step\": [0, 1e400]"));
    }
}

TEST_CASE("ui_contract: Serialize refuses a document it cannot write") {
    SUBCASE("one that does not validate, with Validate's failure") {
        UiDocument document = ParseOrFail(Document());
        document.defaultFont = "missing";
        auto serialized = UiDocumentCodec::Serialize(document);
        REQUIRE_FALSE(serialized);
        CHECK(serialized.error().code == UiFailureCode::MissingReference);
        CHECK(serialized.error().jsonPointer == "/default_font");
    }
    SUBCASE("a string that is not UTF-8") {
        UiDocument document = ParseOrFail(Document());
        document.canvases[0].children[1].text.literal = "T\xC0\xAFO";
        auto serialized = UiDocumentCodec::Serialize(document);
        REQUIRE_FALSE(serialized);
        CHECK(serialized.error().code == UiFailureCode::InvalidJson);
    }
    SUBCASE("a preview number that is not finite (master wrote null)") {
        UiDocument document = ParseOrFail(Document());
        document.bindings[0].preview = std::numeric_limits<double>::quiet_NaN();
        auto serialized = UiDocumentCodec::Serialize(document);
        REQUIRE_FALSE(serialized);
        CHECK(serialized.error().code == UiFailureCode::InvalidJson);
    }
}

TEST_CASE("ui_contract: a document built in code stays version 1 unless it needs version 2") {
    UiDocument document = ParseOrFail(Document());
    UiDocument built;
    built.fonts = document.fonts;
    built.defaultFont = document.defaultFont;
    built.colors = document.colors;
    built.actions = document.actions;
    built.bindings = document.bindings;
    built.canvases = document.canvases;
    CHECK(built.version == 1U);
    CHECK(built.designCanvas.size.x == 1280.0F);
    CHECK(built.designCanvas.size.y == 720.0F);
    const std::string serialized = SerializeOrFail(built);
    CHECK(serialized.starts_with(R"({"schema":"gyo.ui","version":1,"design_canvas":{"size":[1280.0,720.0],)"));
    CHECK(SerializeParsed(serialized) == serialized);
}

} // namespace GYO::Test::Ui
