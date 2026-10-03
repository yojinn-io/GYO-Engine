#include <doctest/doctest.h>

#include "UiGoldenDocument.hpp"
#include "UiTestDocument.hpp"
#include "ui/UiDocumentCodec.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ostream>
#include <string>
#include <string_view>

namespace Engine::Ui::Tests {
namespace {
// engine/ui/src/UiColor.cpp:24-28 (SrgbToLinear) frozen from master 43bccad;
// the input is formed as at :76-79. The volatile offset keeps the comparison at
// run time so both sides use the platform's std::pow.
volatile unsigned gByteOffset = 0U;

[[nodiscard]] float LegacySrgbToLinear(float value) noexcept {
    return value <= 0.04045F
        ? value / 12.92F
        : std::pow((value + 0.055F) / 1.055F, 2.4F);
}


// Frozen gyo.ui v1 Serialize output for kDocument, captured on master 43bccad
// (math-foundation B5 prerequisite). The literal is kept ASCII because
// gyo_ui_tests is compiled without /utf-8 on MSVC: the one non-ASCII run
// (the button_one literal, UTF-8 for katakana "game start") is spelled with
// \x escapes, which yield the same bytes under every source and execution
// character set. kDocument itself (UiTestDocument.hpp) still embeds that run
// as raw UTF-8, as the pre-existing tests in this file already assume.
inline constexpr std::string_view kDocumentCanonicalGolden =
    R"json({
  "schema": "gyo.ui",
  "version": 1,
  "design_canvas": {
    "size": [
      100.0,
      100.0
    ],
    "scale_mode": "fit"
  },
  "fonts": {
    "ui": "test.font"
  },
  "default_font": "ui",
  "colors": {
    "background": "#000000FF",
    "fill": "#E08020FF",
    "focused": "#804000FF",
    "gray": "#808080FF",
    "normal": "#202020FF",
    "pressed": "#C06000FF",
    "thumb": "#FFFFFFFF",
    "track": "#303030FF",
    "white": "#FFFFFFFF"
  },
  "actions": [
    {
      "id": "back",
      "payload": "none"
    },
    {
      "id": "one",
      "payload": "none"
    },
    {
      "id": "two",
      "payload": "none"
    },
    {
      "id": "set_gamma",
      "payload": "number"
    }
  ],
  "bindings": [
    {
      "id": "gamma",
      "type": "number",
      "preview": 1.0
    },
    {
      "id": "outcome",
      "type": "enum",
      "values": [
        "win",
        "lose"
      ],
      "preview": "win"
    },
    {
      "id": "rooms",
      "type": "list<object>",
      "item_fields": {
        "kills": "integer",
        "name": "string"
      },
      "preview": [
        {
          "kills": 2,
          "name": "A"
        },
        {
          "kills": 5,
          "name": "B"
        }
      ]
    }
  ],
  "canvases": [
    {
      "id": "screen",
      "backdrop_color": "background",
      "default_focus": "button_one",
      "cancel_action": "back",
      "focus_order": [
        "button_one",
        "button_two",
        "gamma_slider"
      ],
      "children": [
        {
          "type": "container",
          "id": "stretch_parent",
          "rect": {
            "anchor_min": [
              0.0,
              0.0
            ],
            "anchor_max": [
              1.0,
              1.0
            ],
            "pivot": [
              0.5,
              0.5
            ],
            "position": [
              0.0,
              0.0
            ],
            "size_delta": [
              -20.0,
              -20.0
            ]
          },
          "children": [
            {
              "type": "button",
              "id": "button_one",
              "rect": {
                "anchor_min": [
                  0.0,
                  0.0
                ],
                "anchor_max": [
                  1.0,
                  1.0
                ],
                "pivot": [
                  0.5,
                  0.5
                ],
                "position": [
                  0.0,
                  0.0
                ],
                "size_delta": [
                  40.0,
                  -40.0
                ]
              },
              "text": {
                "literal": ")json"
    "\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0\xE3\x82\xB9\xE3\x82\xBF\xE3\x83\xBC\xE3\x83\x88"
    R"json("
              },
              "point_size": 10.0,
              "text_color": {
                "color": "white"
              },
              "horizontal_align": "center",
              "vertical_align": "center",
              "action": "one",
              "background": {
                "normal": "normal",
                "focused": "focused",
                "pressed": "pressed"
              }
            }
          ]
        },
        {
          "type": "button",
          "id": "button_two",
          "rect": {
            "anchor_min": [
              0.0,
              0.0
            ],
            "anchor_max": [
              0.0,
              0.0
            ],
            "pivot": [
              0.0,
              0.0
            ],
            "position": [
              65.0,
              5.0
            ],
            "size_delta": [
              30.0,
              20.0
            ]
          },
          "text": {
            "literal": "TWO"
          },
          "point_size": 10.0,
          "text_color": {
            "color": "white"
          },
          "horizontal_align": "center",
          "vertical_align": "center",
          "action": "two",
          "background": {
            "normal": "normal",
            "focused": "focused",
            "pressed": "pressed"
          }
        },
        {
          "type": "horizontal_slider",
          "id": "gamma_slider",
          "rect": {
            "anchor_min": [
              0.0,
              0.0
            ],
            "anchor_max": [
              0.0,
              0.0
            ],
            "pivot": [
              0.0,
              0.0
            ],
            "position": [
              10.0,
              75.0
            ],
            "size_delta": [
              80.0,
              20.0
            ]
          },
          "text": {
            "literal": "GAMMA"
          },
          "point_size": 8.0,
          "text_color": {
            "color": "white"
          },
          "horizontal_align": "left",
          "vertical_align": "top",
          "action": "set_gamma",
          "background": {
            "normal": "normal",
            "focused": "focused",
            "pressed": "pressed"
          },
          "binding": "gamma",
          "minimum": 0.75,
          "maximum": 1.5,
          "step": 0.05,
          "value_format": {
            "decimals": 2,
            "show_plus": false
          },
          "track_color": "track",
          "fill_color": "fill",
          "thumb_color": "thumb"
        },
        {
          "type": "fixed_step_list",
          "id": "room_list",
          "rect": {
            "anchor_min": [
              0.0,
              0.0
            ],
            "anchor_max": [
              0.0,
              0.0
            ],
            "pivot": [
              0.0,
              0.0
            ],
            "position": [
              5.0,
              2.0
            ],
            "size_delta": [
              40.0,
              20.0
            ]
          },
          "binding": "rooms",
          "max_items": 4,
          "item_step": [
            0.0,
            9.0
          ],
          "template": [
            {
              "type": "text",
              "id": "room_row",
              "rect": {
                "anchor_min": [
                  0.0,
                  0.0
                ],
                "anchor_max": [
                  1.0,
                  0.0
                ],
                "pivot": [
                  0.0,
                  0.0
                ],
                "position": [
                  0.0,
                  0.0
                ],
                "size_delta": [
                  0.0,
                  8.0
                ]
              },
              "text": {
                "compose": {
                  "format": "{name} {kills}",
                  "placeholders": {
                    "kills": {
                      "item_field": "kills",
                      "format": {
                        "decimals": 0,
                        "show_plus": false
                      }
                    },
                    "name": {
                      "item_field": "name",
                      "format": {
                        "decimals": 0,
                        "show_plus": false
                      }
                    }
                  }
                }
              },
              "point_size": 7.0,
              "text_color": {
                "color": "gray"
              },
              "horizontal_align": "left",
              "vertical_align": "top"
            }
          ]
        },
        {
          "type": "text",
          "id": "outcome",
          "rect": {
            "anchor_min": [
              0.5,
              0.5
            ],
            "anchor_max": [
              0.5,
              0.5
            ],
            "pivot": [
              0.5,
              0.5
            ],
            "position": [
              0.0,
              0.0
            ],
            "size_delta": [
              30.0,
              10.0
            ]
          },
          "text": {
            "select": {
              "binding": "outcome",
              "cases": {
                "lose": "LOSE",
                "win": "WIN"
              }
            }
          },
          "point_size": 8.0,
          "text_color": {
            "color": "white"
          },
          "horizontal_align": "center",
          "vertical_align": "center"
        }
      ]
    }
  ]
}
)json";

[[nodiscard]] bool IsAscii(std::string_view text) noexcept {
    for (const char value : text) {
        if (static_cast<unsigned char>(value) >= 0x80U) return false;
    }
    return true;
}

[[nodiscard]] std::string SerializeParsed(std::string_view json) {
    auto parsed = UiDocumentCodec::Parse(json, "golden");
    if (!parsed) {
        FAIL_CHECK(parsed.error().message << " at " << parsed.error().jsonPointer);
        return {};
    }
    auto serialized = UiDocumentCodec::Serialize(parsed.value());
    if (!serialized) {
        FAIL_CHECK(serialized.error().message);
        return {};
    }
    return std::move(serialized).value();
}

[[nodiscard]] const UiElement& FindChild(
    const std::vector<UiElement>& elements,
    std::string_view id) {
    for (const UiElement& element : elements) {
        if (element.id == id) return element;
    }
    FAIL("missing element " << id);
    return elements.front();
}

} // namespace

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

TEST_CASE("Serialize matches the frozen gyo.ui v1 golden for kDocument") {
    const std::string serialized = SerializeParsed(kDocument);
    CHECK(serialized == kDocumentCanonicalGolden);
    // The golden is a fixed point of Parse -> Serialize.
    CHECK(SerializeParsed(kDocumentCanonicalGolden) == kDocumentCanonicalGolden);
}

TEST_CASE("Serialize matches the frozen gyo.ui v1 golden for the synthetic document") {
    REQUIRE(IsAscii(kGoldenSourceDocument));
    REQUIRE(IsAscii(kGoldenCanonicalDocument));
    const std::string serialized = SerializeParsed(kGoldenSourceDocument);
    CHECK(serialized == kGoldenCanonicalDocument);
    CHECK(SerializeParsed(kGoldenCanonicalDocument) == kGoldenCanonicalDocument);

    // Spot checks that document what the golden pins (the full comparison
    // above is authoritative).
    CHECK(serialized.find("\"schema\": \"gyo.ui\"") != std::string::npos);
    CHECK(serialized.find("\"version\": 1,") != std::string::npos);
    CHECK(serialized.find("0.10000000149011612") != std::string::npos);
    CHECK(serialized.find("1.0000000150474662e+30") != std::string::npos);
    CHECK(serialized.find("1.0000000031710769e-30") != std::string::npos);
    CHECK(serialized.find("\"#A1B2C3D4\"") != std::string::npos);
    CHECK(serialized.find("\"#102030FF\"") != std::string::npos);
}

TEST_CASE("sRGB hex colours round-trip every byte") {
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
        CHECK(EncodeSrgbHexColor(opaque.value()) ==
              std::string(upper.data(), 7) + "FF");
    }

    // Decoded binary32 patterns. The linear segment and alpha are exact
    // constants captured on master 43bccad; everything that goes through
    // std::pow is compared at run time with the decode frozen from master,
    // because libm pow may round differently across platforms.
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
        std::snprintf(hex.data(), hex.size(), "#%02X%02X%02X%02X",
            expected.byte, expected.byte, expected.byte, expected.byte);
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

TEST_CASE("Parse maps array positions to fields") {
    auto parsed = UiDocumentCodec::Parse(kGoldenSourceDocument);
    REQUIRE(parsed);
    const UiDocument& document = parsed.value();

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

} // namespace Engine::Ui::Tests
