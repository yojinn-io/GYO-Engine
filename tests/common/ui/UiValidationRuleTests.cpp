// Every rule of the gyo.ui Data Contract with a reverse case: the failure code and JSON Pointer (master's, from
// engine/ui/src/UiValidation.cpp and UiDocumentCodec.cpp) are checked exactly. Validation rules change a parsed
// document in memory; reading rules change the document's text.
//
// The documents (contract/): document_v1.json (canvas "screen", children 0 stretch_parent > button_one, 1 button_two,
// 2 gamma_slider, 3 room_list > room_row, 4 outcome; actions back, one, two, set_gamma; bindings gamma (number),
// outcome (enum win/lose), rooms (list kills/name)) and golden_v2.json (canvas "main", children 0 address_field
// (text_field), 1 go_button; actions submit_address (string), go, back; binding address (string)).

#include <doctest/doctest.h>

#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "UiContractSupport.hpp"
#include "engine/ui/UiDocumentCodec.hpp"

namespace GYO::Test::Ui {
namespace {

struct ValidationRule final {
    const char* name;
    std::function<void(UiDocument&)> change;
    UiFailureCode code;
    const char* pointer;
};

void CheckRules(const std::string_view file, const std::vector<ValidationRule>& rules) {
    const std::string text = ReadContractFile(file);
    for (const ValidationRule& rule : rules) {
        CAPTURE(rule.name);
        UiDocument document = ParseOrFail(text);
        REQUIRE(UiDocumentCodec::Validate(document));
        rule.change(document);
        auto validation = UiDocumentCodec::Validate(document);
        REQUIRE_FALSE(validation);
        CHECK(validation.error().code == rule.code);
        CHECK(validation.error().jsonPointer == rule.pointer);
        CHECK_FALSE(validation.error().message.empty());
        CHECK_MESSAGE(validation.error().source.empty(), "Validate knows no source; Parse adds it");
    }
}

struct TextRule final {
    const char* name;
    const char* from;
    const char* to;
    UiFailureCode code;
    const char* pointer;
};

void CheckTextRules(const std::string_view file, const std::vector<TextRule>& rules) {
    const std::string text = ReadContractFile(file);
    for (const TextRule& rule : rules) {
        CAPTURE(rule.name);
        auto parsed = UiDocumentCodec::Parse(Replace(text, rule.from, rule.to), "rule.json");
        REQUIRE_FALSE(parsed);
        CHECK(parsed.error().code == rule.code);
        CHECK(parsed.error().jsonPointer == rule.pointer);
        CHECK(parsed.error().source == "rule.json");
        CHECK_FALSE(parsed.error().message.empty());
    }
}

constexpr UiFailureCode kInvalid = UiFailureCode::ValidationFailed;
constexpr UiFailureCode kMissing = UiFailureCode::MissingReference;
constexpr UiFailureCode kMismatch = UiFailureCode::BindingTypeMismatch;
constexpr UiFailureCode kVersion = UiFailureCode::UnsupportedVersion;

UiElement& Child(UiDocument& document, const std::size_t index) {
    return document.canvases[0].children[index];
}

UiElement& Template(UiDocument& document) {
    return document.canvases[0].children[3].itemTemplate[0];
}

UiColorSource NamedColor(const std::string& name) {
    UiColorSource color;
    color.color = name;
    return color;
}

} // namespace

TEST_CASE("ui_contract: every document-level validation rule has a reverse case") {
    constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
    CheckRules("document_v1.json", {
        {"version below 1", [](UiDocument& d) { d.version = 0; }, kVersion, "/version"},
        {"version above 2", [](UiDocument& d) { d.version = 3; }, kVersion, "/version"},
        {"design size not positive", [](UiDocument& d) { d.designCanvas.size.y = 0.0F; }, kInvalid,
         "/design_canvas/size"},
        {"design size not finite", [=](UiDocument& d) { d.designCanvas.size.x = kNaN; }, kInvalid,
         "/design_canvas/size"},
        {"no font", [](UiDocument& d) { d.fonts.clear(); }, kInvalid, "/fonts"},
        {"default font undeclared", [](UiDocument& d) { d.defaultFont = "body"; }, kMissing, "/default_font"},
        {"empty font asset", [](UiDocument& d) { d.fonts["ui"] = ""; }, kInvalid, "/fonts"},
        {"empty font alias", [](UiDocument& d) { d.fonts[""] = "x.font"; }, kInvalid, "/fonts"},
        {"colour channel above 1", [](UiDocument& d) { d.colors["white"].red = 1.5F; }, kInvalid, "/colors"},
        {"colour channel not finite", [=](UiDocument& d) { d.colors["white"].alpha = kNaN; }, kInvalid, "/colors"},
        {"empty colour name", [](UiDocument& d) { d.colors[""] = UiColor{}; }, kInvalid, "/colors"},
        {"empty action id", [](UiDocument& d) { d.actions[1].id = ""; }, kInvalid, "/actions/1/id"},
        {"duplicate action id", [](UiDocument& d) { d.actions[2].id = "one"; }, kInvalid, "/actions/2/id"},
        {"string payload in version 1",
         [](UiDocument& d) { d.actions[0].payload = UiActionPayloadType::String; }, kVersion, "/actions/0/payload"},
        {"empty binding id", [](UiDocument& d) { d.bindings[1].id = ""; }, kInvalid, "/bindings/1/id"},
        {"duplicate binding id", [](UiDocument& d) { d.bindings[1].id = "gamma"; }, kInvalid, "/bindings/1/id"},
        {"binding id of an action", [](UiDocument& d) { d.bindings[1].id = "back"; }, kInvalid, "/bindings/1/id"},
        {"preview of another type", [](UiDocument& d) { d.bindings[0].preview = std::int64_t{1}; }, kMismatch,
         "/bindings/0/preview"},
        {"no enum values", [](UiDocument& d) { d.bindings[1].enumValues.clear(); }, kInvalid, "/bindings/1/values"},
        {"empty enum value", [](UiDocument& d) { d.bindings[1].enumValues.push_back(""); }, kInvalid,
         "/bindings/1/values"},
        {"duplicate enum value", [](UiDocument& d) { d.bindings[1].enumValues.push_back("win"); }, kInvalid,
         "/bindings/1/values"},
        {"enum preview not a value", [](UiDocument& d) { d.bindings[1].preview = std::string("draw"); }, kInvalid,
         "/bindings/1/preview"},
        {"no list item fields", [](UiDocument& d) { d.bindings[2].itemFields.clear(); }, kInvalid,
         "/bindings/2/item_fields"},
        {"list preview item with a field too many",
         [](UiDocument& d) { std::get<UiList>(d.bindings[2].preview).items[1].fields["extra"] = true; }, kInvalid,
         "/bindings/2/preview/1"},
        {"list preview field of another type",
         [](UiDocument& d) { std::get<UiList>(d.bindings[2].preview).items[1].fields["kills"] = 5.0; }, kMismatch,
         "/bindings/2/preview/1/kills"},
        {"list preview field renamed",
         [](UiDocument& d) {
             auto& fields = std::get<UiList>(d.bindings[2].preview).items[0].fields;
             fields.erase("name");
             fields["label"] = std::string("A");
         },
         kMismatch, "/bindings/2/preview/0/name"},
        {"no canvas", [](UiDocument& d) { d.canvases.clear(); }, kInvalid, "/canvases"},
        {"empty canvas id", [](UiDocument& d) { d.canvases[0].id = ""; }, kInvalid, "/canvases/0/id"},
        {"unknown backdrop colour", [](UiDocument& d) { d.canvases[0].backdropColor = "nope"; }, kMissing,
         "/canvases/0/backdrop_color"},
        {"unknown cancel action", [](UiDocument& d) { d.canvases[0].cancelAction = "nope"; }, kMissing,
         "/canvases/0/cancel_action"},
        {"cancel action with a payload", [](UiDocument& d) { d.canvases[0].cancelAction = "set_gamma"; }, kMismatch,
         "/canvases/0/cancel_action"},
        {"duplicate in focus order", [](UiDocument& d) { d.canvases[0].focusOrder.push_back("button_two"); },
         kInvalid, "/canvases/0/focus_order"},
        {"focus order names a non-interactive element", [](UiDocument& d) { d.canvases[0].focusOrder[2] = "outcome"; },
         kInvalid, "/canvases/0/focus_order"},
        {"focus order misses an interactive element", [](UiDocument& d) { d.canvases[0].focusOrder.pop_back(); },
         kInvalid, "/canvases/0/focus_order"},
        {"default focus outside the focus order", [](UiDocument& d) { d.canvases[0].defaultFocus = "outcome"; },
         kInvalid, "/canvases/0/default_focus"},
    });
}

TEST_CASE("ui_contract: every element validation rule has a reverse case") {
    constexpr float kInfinity = std::numeric_limits<float>::infinity();
    CheckRules("document_v1.json", {
        // Every element.
        {"empty element id", [](UiDocument& d) { Child(d, 1).id = ""; }, kInvalid, "/canvases/0/children/1/id"},
        {"element id used twice", [](UiDocument& d) { Child(d, 4).id = "button_one"; }, kInvalid,
         "/canvases/0/children/4/id"},
        {"element id used in a template", [](UiDocument& d) { Template(d).id = "outcome"; }, kInvalid,
         "/canvases/0/children/4/id"},
        {"rect not finite", [=](UiDocument& d) { Child(d, 1).rect.sizeDelta.x = kInfinity; }, kInvalid,
         "/canvases/0/children/1/rect"},
        {"anchor outside [0,1]", [](UiDocument& d) { Child(d, 1).rect.anchorMax.y = 1.5F; }, kInvalid,
         "/canvases/0/children/1/rect"},
        {"anchors out of order", [](UiDocument& d) { Child(d, 4).rect.anchorMin.x = 0.6F; }, kInvalid,
         "/canvases/0/children/4/rect"},
        {"pivot outside [0,1]", [](UiDocument& d) { Child(d, 1).rect.pivot.x = -0.1F; }, kInvalid,
         "/canvases/0/children/1/rect/pivot"},
        {"children are validated with their path",
         [](UiDocument& d) { Child(d, 0).children[0].action = "nope"; }, kMissing,
         "/canvases/0/children/0/children/0/action"},

        // panel and image.
        {"panel colour unknown",
         [](UiDocument& d) {
             Child(d, 0).type = UiElementType::Panel;
             Child(d, 0).color = NamedColor("nope");
         },
         kMissing, "/canvases/0/children/0/color"},
        {"image without texture",
         [](UiDocument& d) {
             Child(d, 0).type = UiElementType::Image;
             Child(d, 0).color = NamedColor("white");
         },
         kInvalid, "/canvases/0/children/0/texture_asset"},
        {"image source_uv empty",
         [](UiDocument& d) {
             Child(d, 0).type = UiElementType::Image;
             Child(d, 0).textureAsset = "t";
             Child(d, 0).color = NamedColor("white");
             Child(d, 0).sourceUv = {0.0F, 0.0F, 0.0F, 1.0F};
         },
         kInvalid, "/canvases/0/children/0/source_uv"},
        {"image source_uv beyond the texture",
         [](UiDocument& d) {
             Child(d, 0).type = UiElementType::Image;
             Child(d, 0).textureAsset = "t";
             Child(d, 0).color = NamedColor("white");
             Child(d, 0).sourceUv = {0.5F, 0.0F, 0.75F, 1.0F};
         },
         kInvalid, "/canvases/0/children/0/source_uv"},
        {"image tint unknown",
         [](UiDocument& d) {
             Child(d, 0).type = UiElementType::Image;
             Child(d, 0).textureAsset = "t";
             Child(d, 0).color = NamedColor("nope");
         },
         kMissing, "/canvases/0/children/0/tint"},

        // text: value, compose and select sources and the style.
        {"value reference empty",
         [](UiDocument& d) {
             Child(d, 4).text.kind = UiTextSourceKind::Value;
             Child(d, 4).text.value = {UiValueReferenceKind::Binding, ""};
         },
         kInvalid, "/canvases/0/children/4/text"},
        {"value binding unknown",
         [](UiDocument& d) {
             Child(d, 4).text.kind = UiTextSourceKind::Value;
             Child(d, 4).text.value = {UiValueReferenceKind::Binding, "nope"};
         },
         kMissing, "/canvases/0/children/4/text"},
        {"value of a list binding",
         [](UiDocument& d) {
             Child(d, 4).text.kind = UiTextSourceKind::Value;
             Child(d, 4).text.value = {UiValueReferenceKind::Binding, "rooms"};
         },
         kMismatch, "/canvases/0/children/4/text"},
        {"item_field outside a template",
         [](UiDocument& d) {
             Child(d, 4).text.kind = UiTextSourceKind::Value;
             Child(d, 4).text.value = {UiValueReferenceKind::ItemField, "kills"};
         },
         kInvalid, "/canvases/0/children/4/text"},
        {"item_field unknown in a template",
         [](UiDocument& d) { Template(d).text.placeholders.at("kills").value.id = "deaths"; }, kMissing,
         "/canvases/0/children/3/template/0/text/placeholders/kills"},
        {"compose without format", [](UiDocument& d) { Template(d).text.composeFormat = ""; }, kInvalid,
         "/canvases/0/children/3/template/0/text"},
        {"compose without placeholders", [](UiDocument& d) { Template(d).text.placeholders.clear(); }, kInvalid,
         "/canvases/0/children/3/template/0/text"},
        {"selection binding unknown", [](UiDocument& d) { Child(d, 4).text.selectionBinding = "nope"; }, kMissing,
         "/canvases/0/children/4/text/binding"},
        {"selection cases too many", [](UiDocument& d) { Child(d, 4).text.cases["draw"] = "DRAW"; }, kInvalid,
         "/canvases/0/children/4/text/cases"},
        {"selection case missing",
         [](UiDocument& d) {
             Child(d, 4).text.cases.erase("win");
             Child(d, 4).text.cases["draw"] = "DRAW";
         },
         kInvalid, "/canvases/0/children/4/text/cases"},
        {"selected colour unknown",
         [](UiDocument& d) {
             UiColorSource& color = Child(d, 4).textStyle.color;
             color.kind = UiColorSourceKind::Select;
             color.selectionBinding = "outcome";
             color.cases = {{"lose", "nope"}, {"win", "white"}};
         },
         kMissing, "/canvases/0/children/4/text_color/cases/lose"},
        {"point size not positive", [](UiDocument& d) { Child(d, 4).textStyle.pointSize = 0.0F; }, kInvalid,
         "/canvases/0/children/4/point_size"},
        {"font alias unknown", [](UiDocument& d) { Child(d, 4).textStyle.font = "mono"; }, kMissing,
         "/canvases/0/children/4/font"},
        {"text colour unknown", [](UiDocument& d) { Child(d, 4).textStyle.color = NamedColor("nope"); }, kMissing,
         "/canvases/0/children/4/text_color"},

        // button.
        {"button action unknown", [](UiDocument& d) { Child(d, 1).action = "nope"; }, kMissing,
         "/canvases/0/children/1/action"},
        {"button action with a payload", [](UiDocument& d) { Child(d, 1).action = "set_gamma"; }, kMismatch,
         "/canvases/0/children/1/action"},
        {"button text checked", [](UiDocument& d) { Child(d, 1).textStyle.pointSize = -1.0F; }, kInvalid,
         "/canvases/0/children/1/point_size"},
        {"button normal colour unknown", [](UiDocument& d) { Child(d, 1).background.normal = "nope"; }, kMissing,
         "/canvases/0/children/1/background/normal"},
        {"button focused colour unknown", [](UiDocument& d) { Child(d, 1).background.focused = "nope"; }, kMissing,
         "/canvases/0/children/1/background/focused"},
        {"button pressed colour unknown", [](UiDocument& d) { Child(d, 1).background.pressed = "nope"; }, kMissing,
         "/canvases/0/children/1/background/pressed"},

        // horizontal_slider.
        {"slider in a template",
         [](UiDocument& d) {
             UiElement slider = Child(d, 2);
             slider.id = "row_slider";
             Template(d) = slider;
         },
         kInvalid, "/canvases/0/children/3/template/0"},
        {"slider action unknown", [](UiDocument& d) { Child(d, 2).action = "nope"; }, kMissing,
         "/canvases/0/children/2/action"},
        {"slider action without a number payload", [](UiDocument& d) { Child(d, 2).action = "one"; }, kMismatch,
         "/canvases/0/children/2/action"},
        {"slider binding unknown", [](UiDocument& d) { Child(d, 2).binding = "nope"; }, kMissing,
         "/canvases/0/children/2/binding"},
        {"slider binding not a number", [](UiDocument& d) { Child(d, 2).binding = "outcome"; }, kMismatch,
         "/canvases/0/children/2/binding"},
        {"slider minimum not below maximum", [](UiDocument& d) { Child(d, 2).minimum = 1.5; }, kInvalid,
         "/canvases/0/children/2"},
        {"slider step not positive", [](UiDocument& d) { Child(d, 2).step = 0.0; }, kInvalid,
         "/canvases/0/children/2"},
        {"slider bound not finite",
         [](UiDocument& d) { Child(d, 2).maximum = std::numeric_limits<double>::infinity(); }, kInvalid,
         "/canvases/0/children/2"},
        {"slider pressed colour unknown", [](UiDocument& d) { Child(d, 2).background.pressed = "nope"; }, kMissing,
         "/canvases/0/children/2/background/pressed"},
        {"slider track colour unknown", [](UiDocument& d) { Child(d, 2).trackColor = "nope"; }, kMissing,
         "/canvases/0/children/2/track_color"},
        {"slider fill colour unknown", [](UiDocument& d) { Child(d, 2).fillColor = "nope"; }, kMissing,
         "/canvases/0/children/2/fill_color"},
        {"slider thumb colour unknown", [](UiDocument& d) { Child(d, 2).thumbColor = "nope"; }, kMissing,
         "/canvases/0/children/2/thumb_color"},

        // fixed_step_list.
        {"nested list",
         [](UiDocument& d) {
             UiElement nested = Child(d, 3);
             nested.id = "nested";
             nested.itemTemplate[0].id = "nested_row";
             Template(d) = nested;
         },
         kInvalid, "/canvases/0/children/3/template/0"},
        {"list binding unknown", [](UiDocument& d) { Child(d, 3).binding = "nope"; }, kMissing,
         "/canvases/0/children/3/binding"},
        {"list binding not a list", [](UiDocument& d) { Child(d, 3).binding = "gamma"; }, kMismatch,
         "/canvases/0/children/3/binding"},
        {"list without items", [](UiDocument& d) { Child(d, 3).maxItems = 0; }, kInvalid, "/canvases/0/children/3"},
        {"list without template", [](UiDocument& d) { Child(d, 3).itemTemplate.clear(); }, kInvalid,
         "/canvases/0/children/3"},
    });
}

TEST_CASE("ui_contract: every text_field validation rule has a reverse case") {
    CheckRules("golden_v2.json", {
        {"text_field in version 1",
         [](UiDocument& d) {
             d.version = 1;
             d.actions[0].payload = UiActionPayloadType::None;
         },
         kVersion, "/canvases/0/children/0/type"},
        {"text_field action unknown", [](UiDocument& d) { Child(d, 0).action = "nope"; }, kMissing,
         "/canvases/0/children/0/action"},
        {"text_field action without a string payload", [](UiDocument& d) { Child(d, 0).action = "go"; }, kMismatch,
         "/canvases/0/children/0/action"},
        {"text_field binding unknown", [](UiDocument& d) { Child(d, 0).binding = "nope"; }, kMissing,
         "/canvases/0/children/0/binding"},
        {"text_field binding not a string",
         [](UiDocument& d) {
             d.bindings[0].type = UiBindingType::Number;
             d.bindings[0].preview = 0.0;
         },
         kMismatch, "/canvases/0/children/0/binding"},
        {"text_field without length", [](UiDocument& d) { Child(d, 0).maxLength = 0; }, kInvalid,
         "/canvases/0/children/0/max_length"},
        {"text_field style checked", [](UiDocument& d) { Child(d, 0).textStyle.font = "mono"; }, kMissing,
         "/canvases/0/children/0/font"},
        {"text_field focused colour unknown", [](UiDocument& d) { Child(d, 0).background.focused = "nope"; },
         kMissing, "/canvases/0/children/0/background/focused"},
        {"text_field placeholder colour unknown", [](UiDocument& d) { Child(d, 0).placeholderColor = "nope"; },
         kMissing, "/canvases/0/children/0/placeholder_color"},
        {"text_field placeholder colour missing", [](UiDocument& d) { Child(d, 0).placeholderColor = ""; },
         kMissing, "/canvases/0/children/0/placeholder_color"},
    });

    // In a list template, as the other interactive elements (version 2, so the version rule does not fire first).
    UiDocument document = ParseOrFail(ReadContractFile("document_v1.json"));
    document.version = 2;
    UiElement field = ParseOrFail(ReadContractFile("golden_v2.json")).canvases[0].children[0];
    Template(document) = field;
    auto validation = UiDocumentCodec::Validate(document);
    REQUIRE_FALSE(validation);
    CHECK(validation.error().code == kInvalid);
    CHECK(validation.error().jsonPointer == "/canvases/0/children/3/template/0");
}

TEST_CASE("ui_contract: every reading rule has a reverse case") {
    for (const char* root : {"[]", "\"gyo.ui\"", "1", "null"}) {
        CAPTURE(root);
        auto parsed = UiDocumentCodec::Parse(root, "root.json");
        REQUIRE_FALSE(parsed);
        CHECK(parsed.error().code == kInvalid);
        CHECK(parsed.error().jsonPointer.empty());
        CHECK(parsed.error().message == "expected an object");
    }

    // An integer is an integer literal (master: nlohmann's integer kinds), then range-checked; master's messages
    // tell the two apart.
    const std::string document = ReadContractFile("document_v1.json");
    auto fraction = UiDocumentCodec::Parse(Replace(document, "\"max_items\": 4", "\"max_items\": 4.0"));
    REQUIRE_FALSE(fraction);
    CHECK(fraction.error().message == "expected an integer");
    auto huge = UiDocumentCodec::Parse(Replace(document, "\"max_items\": 4", "\"max_items\": 99999999999999999999"));
    REQUIRE_FALSE(huge);
    CHECK(huge.error().message == "integer is out of range");

    CheckTextRules("document_v1.json", {
        {"unknown root property", R"("default_font": "ui",)", R"("default_font": "ui", "theme": 1,)", kInvalid,
         "/theme"},
        {"missing root property", R"("default_font": "ui",)", "", kInvalid, "/default_font"},
        {"schema not a string", R"("schema": "gyo.ui")", R"("schema": 1)", kInvalid, "/schema"},
        {"version not an integer literal", "\"version\": 1", "\"version\": 1.0", kInvalid, "/version"},
        {"version a string", "\"version\": 1", "\"version\": \"1\"", kInvalid, "/version"},
        {"design canvas key unknown", R"("scale_mode": "fit" })", R"("scale_mode": "fit", "dpi": 2 })", kInvalid,
         "/design_canvas/dpi"},
        {"scale mode other than fit", R"("scale_mode": "fit")", R"("scale_mode": "fill")", kInvalid,
         "/design_canvas/scale_mode"},
        {"size of three numbers", R"("size": [100, 100])", R"("size": [100, 100, 1])", kInvalid,
         "/design_canvas/size"},
        {"size of a string", R"("size": [100, 100])", R"("size": [100, "100"])", kInvalid, "/design_canvas/size/1"},
        {"fonts not an object", R"("fonts": { "ui": "test.font" })", R"("fonts": ["test.font"])", kInvalid, "/fonts"},
        {"font asset not a string", R"("ui": "test.font")", R"("ui": 7)", kInvalid, "/fonts/ui"},
        {"colour not hex", R"("white": "#FFFFFFFF")", R"("white": "white")", kInvalid, "/colors/white"},
        {"colour with a bad digit", R"("white": "#FFFFFFFF")", R"("white": "#FFFFFFFG")", kInvalid, "/colors/white"},
        {"actions not an array", R"("actions": [
    { "id": "back", "payload": "none" },
    { "id": "one", "payload": "none" },
    { "id": "two", "payload": "none" },
    { "id": "set_gamma", "payload": "number" }
  ],)", R"("actions": {},)", kInvalid, "/actions"},
        {"action key unknown", R"({ "id": "back", "payload": "none" })",
         R"({ "id": "back", "payload": "none", "hotkey": "esc" })", kInvalid, "/actions/0/hotkey"},
        {"action payload unknown", R"({ "id": "back", "payload": "none" })", R"({ "id": "back", "payload": "bool" })",
         kInvalid, "/actions/0/payload"},
        {"action without payload", R"({ "id": "back", "payload": "none" })", R"({ "id": "back" })", kInvalid,
         "/actions/0/payload"},
        {"binding type unknown", R"("type": "number", "preview": 1.0)", R"("type": "float", "preview": 1.0)", kInvalid,
         "/bindings/0/type"},
        {"binding key of another type", R"("type": "number", "preview": 1.0)",
         R"("type": "number", "values": ["a"], "preview": 1.0)", kInvalid, "/bindings/0/values"},
        {"number preview a string", R"("type": "number", "preview": 1.0)", R"("type": "number", "preview": "1")",
         kInvalid, "/bindings/0/preview"},
        {"enum values not an array", R"("values": ["win", "lose"])", R"("values": "win")", kInvalid,
         "/bindings/1/values"},
        {"enum value not a string", R"("values": ["win", "lose"])", R"("values": ["win", 2])", kInvalid,
         "/bindings/1/values/1"},
        {"list item enum field", R"("item_fields": { "kills": "integer", "name": "string" })",
         R"("item_fields": { "kills": "integer", "name": "enum" })", kInvalid, "/bindings/2/item_fields/name"},
        {"list item field type unknown", R"("item_fields": { "kills": "integer", "name": "string" })",
         R"("item_fields": { "kills": "int", "name": "string" })", kInvalid, "/bindings/2/item_fields/kills"},
        {"list preview not an array", R"("preview": [
        { "kills": 2, "name": "A" },
        { "kills": 5, "name": "B" }
      ])", R"("preview": {})", kInvalid, "/bindings/2/preview"},
        {"list preview field unknown", R"({ "kills": 2, "name": "A" })", R"({ "kills": 2, "name": "A", "team": 1 })",
         kInvalid, "/bindings/2/preview/0/team"},
        {"list preview field missing", R"({ "kills": 5, "name": "B" })", R"({ "kills": 5 })", kInvalid,
         "/bindings/2/preview/1/name"},
        {"list preview integer field with a fraction", R"({ "kills": 5, "name": "B" })",
         R"({ "kills": 5.5, "name": "B" })", kInvalid, "/bindings/2/preview/1/kills"},
        {"canvas key unknown", R"("id": "screen",)", R"("id": "screen", "title": "x",)", kInvalid, "/canvases/0/title"},
        {"focus order not an array", R"("focus_order": ["button_one", "button_two", "gamma_slider"])",
         R"("focus_order": "button_one")", kInvalid, "/canvases/0/focus_order"},
        {"children not an array", R"("id": "button_two",)", R"("id": "button_two", "children": {},)", kInvalid,
         "/canvases/0/children/1/children"},
        {"element type unknown", R"("type": "container")", R"("type": "group")", kInvalid,
         "/canvases/0/children/0/type"},
        {"element key of another type", R"("id": "button_two",)", R"("id": "button_two", "binding": "gamma",)",
         kInvalid, "/canvases/0/children/1/binding"},
        {"element without rect", R"("id": "button_two",
          "rect": {
            "anchor_min": [0, 0], "anchor_max": [0, 0], "pivot": [0, 0],
            "position": [65, 5], "size_delta": [30, 20]
          },)", R"("id": "button_two",)", kInvalid, "/canvases/0/children/1/rect"},
        {"rect key unknown", R"("position": [65, 5])", R"("pivit": [0, 0], "position": [65, 5])", kInvalid,
         "/canvases/0/children/1/rect/pivit"},
        {"text source with two kinds", R"({ "literal": "TWO" })", R"({ "literal": "TWO", "binding": "gamma" })",
         kInvalid, "/canvases/0/children/1/text/binding"},
        {"value source with binding and item_field", R"({ "literal": "TWO" })",
         R"({ "binding": "gamma", "item_field": "kills" })", kInvalid, "/canvases/0/children/1/text"},
        {"value source with neither", R"({ "literal": "TWO" })", R"({ "format": { "decimals": 0, "show_plus": false } })",
         kInvalid, "/canvases/0/children/1/text"},
        {"text colour without colour or select", R"("text_color": { "color": "white" },
          "action": "two")", R"("text_color": {},
          "action": "two")", kInvalid, "/canvases/0/children/1/text_color/select"},
        {"alignment unknown", R"("horizontal_align": "center",
              "vertical_align": "center",
              "action": "one")", R"("horizontal_align": "middle",
              "vertical_align": "center",
              "action": "one")", kInvalid, "/canvases/0/children/0/children/0/horizontal_align"},
        {"vertical alignment unknown", R"("horizontal_align": "center",
              "vertical_align": "center",
              "action": "one")", R"("horizontal_align": "center",
              "vertical_align": "middle",
              "action": "one")", kInvalid, "/canvases/0/children/0/children/0/vertical_align"},
        {"button without background", R"("action": "two",
          "background": { "normal": "normal", "focused": "focused", "pressed": "pressed" })", R"("action": "two")",
         kInvalid, "/canvases/0/children/1/background"},
        {"background without normal", R"("action": "two",
          "background": { "normal": "normal", "focused": "focused", "pressed": "pressed" })",
         R"("action": "two",
          "background": { "focused": "focused" })", kInvalid, "/canvases/0/children/1/background/normal"},
        {"slider minimum not a number", R"("minimum": 0.75)", R"("minimum": "low")", kInvalid,
         "/canvases/0/children/2/minimum"},
        {"decimals above 9", R"("decimals": 2)", R"("decimals": 10)", kInvalid,
         "/canvases/0/children/2/value_format/decimals"},
        {"decimals negative", R"("decimals": 2)", R"("decimals": -1)", kInvalid,
         "/canvases/0/children/2/value_format/decimals"},
        {"decimals not an integer literal", R"("decimals": 2)", R"("decimals": 2.0)", kInvalid,
         "/canvases/0/children/2/value_format/decimals"},
        {"show_plus not a boolean", R"("decimals": 2, "show_plus": false)", R"("decimals": 2, "show_plus": 0)",
         kInvalid, "/canvases/0/children/2/value_format/show_plus"},
        {"max_items negative", R"("max_items": 4)", R"("max_items": -4)", kInvalid,
         "/canvases/0/children/3/max_items"},
        {"max_items beyond int64", R"("max_items": 4)", R"("max_items": 99999999999999999999)", kInvalid,
         "/canvases/0/children/3/max_items"},
        {"item_step of one number", R"("item_step": [0, 9])", R"("item_step": [9])", kInvalid,
         "/canvases/0/children/3/item_step"},
        // The template's elements move to children (allowed, read first), and template becomes a number.
        {"template not an array", R"("template": [)", R"("template": 1, "children": [)", kInvalid,
         "/canvases/0/children/3/template"},
        {"compose key unknown", R"("format": "{name} {kills}",)", R"("format": "{name} {kills}", "plural": 1,)",
         kInvalid, "/canvases/0/children/3/template/0/text/compose/plural"},
        {"placeholder key unknown", R"("name": { "item_field": "name" })",
         R"("name": { "item_field": "name", "width": 4 })", kInvalid,
         "/canvases/0/children/3/template/0/text/compose/placeholders/name/width"},
        {"select without cases", R"("binding": "outcome",
              "cases": { "lose": "LOSE", "win": "WIN" })", R"("binding": "outcome")", kInvalid,
         "/canvases/0/children/4/text/select/cases"},
        {"select case not a string", R"("lose": "LOSE")", R"("lose": 0)", kInvalid,
         "/canvases/0/children/4/text/select/cases/lose"},
    });

    CheckTextRules("golden_v2.json", {
        {"placeholder colour without placeholder", R"("placeholder": "host:port",)", "", kInvalid,
         "/canvases/0/children/0/placeholder_color"},
        {"placeholder without colour", R"(,
          "placeholder_color": "gray")", "", kInvalid, "/canvases/0/children/0/placeholder_color"},
        {"max_length below 1", R"("max_length": 8)", R"("max_length": 0)", kInvalid,
         "/canvases/0/children/0/max_length"},
        {"max_length missing", R"("max_length": 8,)", "", kInvalid, "/canvases/0/children/0/max_length"},
        {"text_field with a text source", R"("max_length": 8)", R"("max_length": 8, "text": { "literal": "x" })",
         kInvalid, "/canvases/0/children/0/text"},
        {"text_field without binding", R"("binding": "address",
          "max_length")", R"("max_length")", kInvalid, "/canvases/0/children/0/binding"},
    });
}

} // namespace GYO::Test::Ui
