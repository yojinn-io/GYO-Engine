#pragma once

#include <string_view>

namespace Engine::Ui::Tests {

inline constexpr std::string_view kDocument = R"json({
  "schema": "gyo.ui",
  "version": 1,
  "design_canvas": { "size": [100, 100], "scale_mode": "fit" },
  "fonts": { "ui": "test.font" },
  "default_font": "ui",
  "colors": {
    "background": "#000000FF",
    "white": "#FFFFFFFF",
    "gray": "#808080FF",
    "normal": "#202020FF",
    "focused": "#804000FF",
    "pressed": "#C06000FF",
    "track": "#303030FF",
    "fill": "#E08020FF",
    "thumb": "#FFFFFFFF"
  },
  "actions": [
    { "id": "back", "payload": "none" },
    { "id": "one", "payload": "none" },
    { "id": "two", "payload": "none" },
    { "id": "set_gamma", "payload": "number" }
  ],
  "bindings": [
    { "id": "gamma", "type": "number", "preview": 1.0 },
    { "id": "outcome", "type": "enum", "values": ["win", "lose"], "preview": "win" },
    {
      "id": "rooms",
      "type": "list<object>",
      "item_fields": { "kills": "integer", "name": "string" },
      "preview": [
        { "kills": 2, "name": "A" },
        { "kills": 5, "name": "B" }
      ]
    }
  ],
  "canvases": [
    {
      "id": "screen",
      "backdrop_color": "background",
      "default_focus": "button_one",
      "cancel_action": "back",
      "focus_order": ["button_one", "button_two", "gamma_slider"],
      "children": [
        {
          "type": "container",
          "id": "stretch_parent",
          "rect": {
            "anchor_min": [0, 0], "anchor_max": [1, 1], "pivot": [0.5, 0.5],
            "position": [0, 0], "size_delta": [-20, -20]
          },
          "children": [
            {
              "type": "button",
              "id": "button_one",
              "rect": {
                "anchor_min": [0, 0], "anchor_max": [1, 1], "pivot": [0.5, 0.5],
                "position": [0, 0], "size_delta": [40, -40]
              },
              "text": { "literal": "ゲームスタート" },
              "point_size": 10,
              "text_color": { "color": "white" },
              "horizontal_align": "center",
              "vertical_align": "center",
              "action": "one",
              "background": { "normal": "normal", "focused": "focused", "pressed": "pressed" }
            }
          ]
        },
        {
          "type": "button",
          "id": "button_two",
          "rect": {
            "anchor_min": [0, 0], "anchor_max": [0, 0], "pivot": [0, 0],
            "position": [65, 5], "size_delta": [30, 20]
          },
          "text": { "literal": "TWO" },
          "point_size": 10,
          "text_color": { "color": "white" },
          "action": "two",
          "background": { "normal": "normal", "focused": "focused", "pressed": "pressed" }
        },
        {
          "type": "horizontal_slider",
          "id": "gamma_slider",
          "rect": {
            "anchor_min": [0, 0], "anchor_max": [0, 0], "pivot": [0, 0],
            "position": [10, 75], "size_delta": [80, 20]
          },
          "text": { "literal": "GAMMA" },
          "point_size": 8,
          "text_color": { "color": "white" },
          "action": "set_gamma",
          "background": { "normal": "normal", "focused": "focused", "pressed": "pressed" },
          "binding": "gamma",
          "minimum": 0.75,
          "maximum": 1.5,
          "step": 0.05,
          "value_format": { "decimals": 2, "show_plus": false },
          "track_color": "track",
          "fill_color": "fill",
          "thumb_color": "thumb"
        },
        {
          "type": "fixed_step_list",
          "id": "room_list",
          "rect": {
            "anchor_min": [0, 0], "anchor_max": [0, 0], "pivot": [0, 0],
            "position": [5, 2], "size_delta": [40, 20]
          },
          "binding": "rooms",
          "max_items": 4,
          "item_step": [0, 9],
          "template": [
            {
              "type": "text",
              "id": "room_row",
              "rect": {
                "anchor_min": [0, 0], "anchor_max": [1, 0], "pivot": [0, 0],
                "position": [0, 0], "size_delta": [0, 8]
              },
              "text": {
                "compose": {
                  "format": "{name} {kills}",
                  "placeholders": {
                    "kills": { "item_field": "kills", "format": { "decimals": 0, "show_plus": false } },
                    "name": { "item_field": "name" }
                  }
                }
              },
              "point_size": 7,
              "text_color": { "color": "gray" }
            }
          ]
        },
        {
          "type": "text",
          "id": "outcome",
          "rect": {
            "anchor_min": [0.5, 0.5], "anchor_max": [0.5, 0.5], "pivot": [0.5, 0.5],
            "position": [0, 0], "size_delta": [30, 10]
          },
          "text": {
            "select": {
              "binding": "outcome",
              "cases": { "lose": "LOSE", "win": "WIN" }
            }
          },
          "point_size": 8,
          "text_color": { "color": "white" },
          "horizontal_align": "center",
          "vertical_align": "center"
        }
      ]
    }
  ]
})json";

} // namespace Engine::Ui::Tests
