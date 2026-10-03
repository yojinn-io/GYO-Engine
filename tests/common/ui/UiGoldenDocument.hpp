#pragma once

#include <string_view>

// ASCII-only synthetic gyo.ui v1 document for golden serialization and exact
// layout regression tests. Keep this file ASCII: gyo_ui_tests is compiled
// without /utf-8 on MSVC, so non-ASCII source text would depend on the build
// machine's code page.
//
// Coverage (each item is relied on by a test; keep it when editing):
// - every rect.* array, item_step and design_canvas.size carry distinct,
//   mostly non-binary values (0.1 -> 0.10000000149011612 after the float
//   round trip), plus large and small exponents (far_away / far_panel);
// - an image with asymmetric source_uv and an image without source_uv;
// - panels with a boolean color select and an image tint with an enum select;
// - value text with show_plus, a font override, default text style;
// - a button and a slider whose omitted background states default;
// - a fixed_step_list truncated by max_items with a compose template that
//   escapes braces and mixes item_field and binding placeholders;
// - a canvas with default_focus and cancel_action omitted;
// - lowercase and #RRGGBB colors that canonicalize to uppercase #RRGGBBAA.
//
// kGoldenCanonicalDocument is the frozen gyo.ui v1 Serialize output for
// kGoldenSourceDocument; regenerate it only together with a schema change.
namespace Engine::Ui::Tests {

inline constexpr std::string_view kGoldenSourceDocument = R"json({
  "schema": "gyo.ui",
  "version": 1,
  "design_canvas": { "size": [333.3, 187.5], "scale_mode": "fit" },
  "fonts": { "mono": "golden.font.mono", "body": "golden.font.body" },
  "default_font": "body",
  "colors": {
    "backdrop": "#102030",
    "panel_on": "#a1b2c3d4",
    "panel_off": "#00000000",
    "tint": "#7F7F7F80",
    "text": "#FFFFFF",
    "button": "#203040FF",
    "button_hot": "#405060ff",
    "track": "#010203FF",
    "fill": "#FEFDFCFF",
    "thumb": "#0A0B0CFF"
  },
  "actions": [
    { "id": "confirm", "payload": "none" },
    { "id": "set_volume", "payload": "number" },
    { "id": "close", "payload": "none" }
  ],
  "bindings": [
    { "id": "enabled", "type": "boolean", "preview": true },
    { "id": "mode", "type": "enum", "values": ["easy", "hard", "insane"], "preview": "hard" },
    { "id": "score", "type": "number", "preview": 0.1 },
    { "id": "lives", "type": "integer", "preview": -3 },
    { "id": "player", "type": "string", "preview": "P1" },
    { "id": "volume", "type": "number", "preview": 2.5e-7 },
    {
      "id": "entries",
      "type": "list<object>",
      "item_fields": { "rank": "integer", "label": "string", "ratio": "number", "ready": "boolean" },
      "preview": [
        { "label": "alpha", "rank": 1, "ratio": 0.3, "ready": true },
        { "label": "beta", "rank": 2, "ratio": 1e-3, "ready": false },
        { "label": "gamma", "rank": 3, "ratio": 1.0e+300, "ready": true }
      ]
    }
  ],
  "canvases": [
    {
      "id": "main",
      "backdrop_color": "backdrop",
      "default_focus": "ok_button",
      "cancel_action": "close",
      "focus_order": ["ok_button", "volume_slider"],
      "children": [
        {
          "type": "panel",
          "id": "frame",
          "rect": {
            "anchor_min": [0, 0], "anchor_max": [1, 1], "pivot": [0.5, 0.5],
            "position": [0, 0], "size_delta": [-10.5, -8.25]
          },
          "color": { "select": { "binding": "enabled", "cases": { "true": "panel_on", "false": "panel_off" } } },
          "children": [
            {
              "type": "image",
              "id": "icon_uv",
              "rect": {
                "anchor_min": [0.25, 0.125], "anchor_max": [0.25, 0.125], "pivot": [0.3, 0.7],
                "position": [0.1, -0.2], "size_delta": [48, 32]
              },
              "texture_asset": "golden.texture.atlas",
              "source_uv": [0.125, 0.25, 0.5, 0.75],
              "tint": { "color": "tint" }
            },
            {
              "type": "image",
              "id": "icon_full",
              "rect": {
                "anchor_min": [0.75, 0.5], "anchor_max": [1, 0.75], "pivot": [1, 0],
                "position": [-3.3, 4.4], "size_delta": [0, 0]
              },
              "texture_asset": "golden.texture.full",
              "tint": { "select": { "binding": "mode", "cases": { "easy": "tint", "hard": "text", "insane": "panel_on" } } }
            },
            {
              "type": "text",
              "id": "score_text",
              "rect": {
                "anchor_min": [0, 0], "anchor_max": [0.5, 0], "pivot": [0, 0],
                "position": [2.2, 3.3], "size_delta": [0, 14.7]
              },
              "text": { "binding": "score", "format": { "decimals": 2, "show_plus": true } },
              "font": "mono",
              "point_size": 12.3,
              "text_color": { "color": "text" },
              "horizontal_align": "right",
              "vertical_align": "bottom"
            },
            {
              "type": "text",
              "id": "lives_text",
              "rect": {
                "anchor_min": [0, 1], "anchor_max": [0, 1], "pivot": [0, 1],
                "position": [1.5, -0.75], "size_delta": [60, 16]
              },
              "text": { "binding": "lives" },
              "text_color": { "color": "text" }
            },
            {
              "type": "text",
              "id": "mode_text",
              "rect": {
                "anchor_min": [0.5, 1], "anchor_max": [0.5, 1], "pivot": [0.5, 1],
                "position": [0, 0], "size_delta": [40, 12]
              },
              "text": { "select": { "binding": "mode", "cases": { "easy": "E", "hard": "H", "insane": "I" } } },
              "point_size": 9,
              "text_color": { "select": { "binding": "enabled", "cases": { "true": "text", "false": "panel_off" } } },
              "horizontal_align": "center",
              "vertical_align": "center"
            }
          ]
        },
        {
          "type": "button",
          "id": "ok_button",
          "rect": {
            "anchor_min": [1, 1], "anchor_max": [1, 1], "pivot": [1, 1],
            "position": [-7.7, -6.6], "size_delta": [50.5, 20.25]
          },
          "text": { "literal": "OK" },
          "text_color": { "color": "text" },
          "action": "confirm",
          "background": { "normal": "button" }
        },
        {
          "type": "horizontal_slider",
          "id": "volume_slider",
          "rect": {
            "anchor_min": [0.1, 0.6], "anchor_max": [0.9, 0.6], "pivot": [0.5, 0.5],
            "position": [0, 0.3], "size_delta": [0, 22.2]
          },
          "text": { "literal": "VOLUME" },
          "font": "mono",
          "point_size": 7.5,
          "text_color": { "color": "text" },
          "horizontal_align": "left",
          "vertical_align": "top",
          "action": "set_volume",
          "background": { "normal": "button", "focused": "button_hot" },
          "binding": "volume",
          "minimum": -1.0e+300,
          "maximum": 1.0e+300,
          "step": 0.1,
          "value_format": { "decimals": 3, "show_plus": true },
          "track_color": "track",
          "fill_color": "fill",
          "thumb_color": "thumb"
        },
        {
          "type": "fixed_step_list",
          "id": "entries_list",
          "rect": {
            "anchor_min": [0, 0], "anchor_max": [0, 0], "pivot": [0, 0],
            "position": [20.2, 30.3], "size_delta": [90.9, 8.8]
          },
          "binding": "entries",
          "max_items": 2,
          "item_step": [0.5, 10.1],
          "template": [
            {
              "type": "panel",
              "id": "entry_bar",
              "rect": {
                "anchor_min": [0, 0], "anchor_max": [1, 1], "pivot": [0, 0],
                "position": [0, 0], "size_delta": [0, 0]
              },
              "color": { "color": "button" }
            },
            {
              "type": "text",
              "id": "entry_label",
              "rect": {
                "anchor_min": [0, 0], "anchor_max": [1, 1], "pivot": [0, 0],
                "position": [1, 0.5], "size_delta": [-2, -1]
              },
              "text": {
                "compose": {
                  "format": "{{{label}}} #{rank}: {ratio} {ready} by {player}",
                  "placeholders": {
                    "label": { "item_field": "label" },
                    "rank": { "item_field": "rank", "format": { "decimals": 0, "show_plus": true } },
                    "ratio": { "item_field": "ratio", "format": { "decimals": 1, "show_plus": false } },
                    "ready": { "item_field": "ready" },
                    "player": { "binding": "player" }
                  }
                }
              },
              "point_size": 6.25,
              "text_color": { "color": "text" }
            }
          ]
        },
        {
          "type": "container",
          "id": "far_away",
          "rect": {
            "anchor_min": [0, 0], "anchor_max": [0, 0], "pivot": [0, 0],
            "position": [1.0e+30, -2.5e-30], "size_delta": [1.0e-30, 3.0e+20]
          },
          "children": [
            {
              "type": "panel",
              "id": "far_panel",
              "rect": {
                "anchor_min": [0, 0], "anchor_max": [1, 1], "pivot": [0, 0],
                "position": [0, 0], "size_delta": [0, 0]
              },
              "color": { "color": "fill" }
            }
          ]
        }
      ]
    },
    {
      "id": "minimal",
      "backdrop_color": "panel_off",
      "focus_order": [],
      "children": [
        {
          "type": "container",
          "id": "empty_root",
          "rect": {
            "anchor_min": [0, 0], "anchor_max": [1, 1], "pivot": [0, 0],
            "position": [0, 0], "size_delta": [0, 0]
          }
        }
      ]
    }
  ]
})json";

// Serialize(Parse(kGoldenSourceDocument)) captured on master 43bccad. The
// literal is split into adjacent raw strings (MSVC C2026 caps each piece).
inline constexpr std::string_view kGoldenCanonicalDocument =
    R"json({
  "schema": "gyo.ui",
  "version": 1,
  "design_canvas": {
    "size": [
      333.29998779296875,
      187.5
    ],
    "scale_mode": "fit"
  },
  "fonts": {
    "body": "golden.font.body",
    "mono": "golden.font.mono"
  },
  "default_font": "body",
  "colors": {
    "backdrop": "#102030FF",
    "button": "#203040FF",
    "button_hot": "#405060FF",
    "fill": "#FEFDFCFF",
    "panel_off": "#00000000",
    "panel_on": "#A1B2C3D4",
    "text": "#FFFFFFFF",
    "thumb": "#0A0B0CFF",
    "tint": "#7F7F7F80",
    "track": "#010203FF"
  },
  "actions": [
    {
      "id": "confirm",
      "payload": "none"
    },
    {
      "id": "set_volume",
      "payload": "number"
    },
    {
      "id": "close",
      "payload": "none"
    }
  ],
  "bindings": [
    {
      "id": "enabled",
      "type": "boolean",
      "preview": true
    },
    {
      "id": "mode",
      "type": "enum",
      "values": [
        "easy",
        "hard",
        "insane"
      ],
      "preview": "hard"
    },
    {
      "id": "score",
      "type": "number",
      "preview": 0.1
    },
    {
      "id": "lives",
      "type": "integer",
      "preview": -3
    },
    {
      "id": "player",
      "type": "string",
      "preview": "P1"
    },
    {
      "id": "volume",
      "type": "number",
      "preview": 2.5e-07
    },
    {
      "id": "entries",
      "type": "list<object>",
      "item_fields": {
        "label": "string",
        "rank": "integer",
        "ratio": "number",
        "ready": "boolean"
      },
      "preview": [
        {
          "label": "alpha",
          "rank": 1,
          "ratio": 0.3,
          "ready": true
        },
        {
          "label": "beta",
          "rank": 2,
          "ratio": 0.001,
          "ready": false
        },
        {
          "label": "gamma",
          "rank": 3,
          "ratio": 1e+300,
          "ready": true
        }
      ]
    }
  ],
  "canvases": [
    {
      "id": "main",
      "backdrop_color": "backdrop",
      "default_focus": "ok_button",
      "cancel_action": "close",
      "focus_order": [
        "ok_button",
        "volume_slider"
      ],
      "children": [
        {
          "type": "panel",
          "id": "frame",
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
              -10.5,
              -8.25
            ]
          },
          "color": {
            "select": {
              "binding": "enabled",
              "cases": {
                "false": "panel_off",
                "true": "panel_on"
              }
            }
          },
          "children": [
            {
              "type": "image",
              "id": "icon_uv",
              "rect": {
                "anchor_min": [
                  0.25,
                  0.125
                ],
                "anchor_max": [
                  0.25,
                  0.125
                ],
                "pivot": [
                  0.30000001192092896,
                  0.699999988079071
                ],
                "position": [
                  0.10000000149011612,
                  -0.20000000298023224
                ],
                "size_delta": [
                  48.0,
                  32.0
                ]
              },
              "texture_asset": "golden.texture.atlas",
              "source_uv": [
                0.125,
                0.25,
                0.5,
                0.75
              ],
              "tint": {
                "color": "tint"
              }
            },
            {
              "type": "image",
              "id": "icon_full",
              "rect": {
                "anchor_min": [
                  0.75,
                  0.5
                ],
                "anchor_max": [
                  1.0,
                  0.75
                ],
                "pivot": [
                  1.0,
                  0.0
                ],
                "position": [
                  -3.299999952316284,
                  4.400000095367432
                ],
                "size_delta": [
                  0.0,
                  0.0
                ]
              },
              "texture_asset": "golden.texture.full",
              "source_uv": [
                0.0,
                0.0,
                1.0,
                1.0
              ],
              "tint": {
                "select": {
                  "binding": "mode",
                  "cases": {
                    "easy": "tint",
                    "hard": "text",
                    "insane": "panel_on"
                  }
                }
              }
            },
            {
              "type": "text",
              "id": "score_text",
              "rect": {
                "anchor_min": [
                  0.0,
                  0.0
                ],
                "anchor_max": [
                  0.5,
                  0.0
                ],
                "pivot": [
                  0.0,
                  0.0
                ],
                "position": [
                  2.200000047683716,
                  3.299999952316284
                ],
                "size_delta": [
                  0.0,
                  14.699999809265137
                ]
              },
)json"
    R"json(              "text": {
                "binding": "score",
                "format": {
                  "decimals": 2,
                  "show_plus": true
                }
              },
              "font": "mono",
              "point_size": 12.300000190734863,
              "text_color": {
                "color": "text"
              },
              "horizontal_align": "right",
              "vertical_align": "bottom"
            },
            {
              "type": "text",
              "id": "lives_text",
              "rect": {
                "anchor_min": [
                  0.0,
                  1.0
                ],
                "anchor_max": [
                  0.0,
                  1.0
                ],
                "pivot": [
                  0.0,
                  1.0
                ],
                "position": [
                  1.5,
                  -0.75
                ],
                "size_delta": [
                  60.0,
                  16.0
                ]
              },
              "text": {
                "binding": "lives",
                "format": {
                  "decimals": 0,
                  "show_plus": false
                }
              },
              "point_size": 16.0,
              "text_color": {
                "color": "text"
              },
              "horizontal_align": "left",
              "vertical_align": "top"
            },
            {
              "type": "text",
              "id": "mode_text",
              "rect": {
                "anchor_min": [
                  0.5,
                  1.0
                ],
                "anchor_max": [
                  0.5,
                  1.0
                ],
                "pivot": [
                  0.5,
                  1.0
                ],
                "position": [
                  0.0,
                  0.0
                ],
                "size_delta": [
                  40.0,
                  12.0
                ]
              },
              "text": {
                "select": {
                  "binding": "mode",
                  "cases": {
                    "easy": "E",
                    "hard": "H",
                    "insane": "I"
                  }
                }
              },
              "point_size": 9.0,
              "text_color": {
                "select": {
                  "binding": "enabled",
                  "cases": {
                    "false": "panel_off",
                    "true": "text"
                  }
                }
              },
              "horizontal_align": "center",
              "vertical_align": "center"
            }
          ]
        },
        {
          "type": "button",
          "id": "ok_button",
          "rect": {
            "anchor_min": [
              1.0,
              1.0
            ],
            "anchor_max": [
              1.0,
              1.0
            ],
            "pivot": [
              1.0,
              1.0
            ],
            "position": [
              -7.699999809265137,
              -6.599999904632568
            ],
            "size_delta": [
              50.5,
              20.25
            ]
          },
          "text": {
            "literal": "OK"
          },
          "point_size": 16.0,
          "text_color": {
            "color": "text"
          },
          "horizontal_align": "center",
          "vertical_align": "center",
          "action": "confirm",
          "background": {
            "normal": "button",
            "focused": "button",
            "pressed": "button"
          }
        },
        {
          "type": "horizontal_slider",
          "id": "volume_slider",
          "rect": {
            "anchor_min": [
              0.10000000149011612,
              0.6000000238418579
            ],
            "anchor_max": [
              0.8999999761581421,
              0.6000000238418579
            ],
            "pivot": [
              0.5,
              0.5
            ],
            "position": [
              0.0,
              0.30000001192092896
            ],
            "size_delta": [
              0.0,
              22.200000762939453
            ]
          },
          "text": {
            "literal": "VOLUME"
          },
          "font": "mono",
          "point_size": 7.5,
          "text_color": {
            "color": "text"
          },
          "horizontal_align": "left",
          "vertical_align": "top",
          "action": "set_volume",
          "background": {
            "normal": "button",
            "focused": "button_hot",
            "pressed": "button_hot"
          },
          "binding": "volume",
          "minimum": -1e+300,
          "maximum": 1e+300,
          "step": 0.1,
          "value_format": {
            "decimals": 3,
            "show_plus": true
          },
          "track_color": "track",
          "fill_color": "fill",
          "thumb_color": "thumb"
        },
        {
          "type": "fixed_step_list",
          "id": "entries_list",
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
              20.200000762939453,
              30.299999237060547
            ],
            "size_delta": [
              90.9000015258789,
              8.800000190734863
            ]
          },
)json"
    R"json(          "binding": "entries",
          "max_items": 2,
          "item_step": [
            0.5,
            10.100000381469727
          ],
          "template": [
            {
              "type": "panel",
              "id": "entry_bar",
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
                  0.0,
                  0.0
                ],
                "position": [
                  0.0,
                  0.0
                ],
                "size_delta": [
                  0.0,
                  0.0
                ]
              },
              "color": {
                "color": "button"
              }
            },
            {
              "type": "text",
              "id": "entry_label",
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
                  0.0,
                  0.0
                ],
                "position": [
                  1.0,
                  0.5
                ],
                "size_delta": [
                  -2.0,
                  -1.0
                ]
              },
              "text": {
                "compose": {
                  "format": "{{{label}}} #{rank}: {ratio} {ready} by {player}",
                  "placeholders": {
                    "label": {
                      "item_field": "label",
                      "format": {
                        "decimals": 0,
                        "show_plus": false
                      }
                    },
                    "player": {
                      "binding": "player",
                      "format": {
                        "decimals": 0,
                        "show_plus": false
                      }
                    },
                    "rank": {
                      "item_field": "rank",
                      "format": {
                        "decimals": 0,
                        "show_plus": true
                      }
                    },
                    "ratio": {
                      "item_field": "ratio",
                      "format": {
                        "decimals": 1,
                        "show_plus": false
                      }
                    },
                    "ready": {
                      "item_field": "ready",
                      "format": {
                        "decimals": 0,
                        "show_plus": false
                      }
                    }
                  }
                }
              },
              "point_size": 6.25,
              "text_color": {
                "color": "text"
              },
              "horizontal_align": "left",
              "vertical_align": "top"
            }
          ]
        },
        {
          "type": "container",
          "id": "far_away",
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
              1.0000000150474662e+30,
              -2.500000007927692e-30
            ],
            "size_delta": [
              1.0000000031710769e-30,
              3.000000060122632e+20
            ]
          },
          "children": [
            {
              "type": "panel",
              "id": "far_panel",
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
                  0.0,
                  0.0
                ],
                "position": [
                  0.0,
                  0.0
                ],
                "size_delta": [
                  0.0,
                  0.0
                ]
              },
              "color": {
                "color": "fill"
              }
            }
          ]
        }
      ]
    },
    {
      "id": "minimal",
      "backdrop_color": "panel_off",
      "focus_order": [],
      "children": [
        {
          "type": "container",
          "id": "empty_root",
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
              0.0,
              0.0
            ],
            "position": [
              0.0,
              0.0
            ],
            "size_delta": [
              0.0,
              0.0
            ]
          }
        }
      ]
    }
  ]
}
)json";

} // namespace Engine::Ui::Tests
