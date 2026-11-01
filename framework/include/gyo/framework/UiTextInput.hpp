#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

#include "engine/input/PhysicalInputFrame.hpp"
#include "engine/ui/UiTypes.hpp"

namespace Gyo::Tools::Framework {

// The modifier the select-all and paste shortcuts take: the product's choice,
// for example Gui (Command) where its platform's text fields use it.
enum class TextShortcutModifier : std::uint8_t { Ctrl, Gui };

// A frame's text input entries as Ui's neutral text input, for the field being
// edited. Which key means which edit is the product's mapping (Ui Design
// Invariant 7); this is the one the products that call the framework share
// (v8-input-ui-adapter, Design Invariant 5). Typed text is an insert and an
// input method's composition stays a composition (text and cursor); shortcut+A
// selects all and shortcut+V inserts clipboard(); Backspace, Delete, Left, Right,
// Home and End edit. Keys are read by the key the layout produces, repeats
// included; any other key is not text input. clipboard is called only for
// shortcut+V; with no clipboard function, it inserts an empty text.
[[nodiscard]] std::vector<Engine::Ui::UiTextInput>
ToUiTextInput(std::span<const Engine::Input::TextEvent> events, TextShortcutModifier shortcut,
              const std::function<std::string()>& clipboard);

} // namespace Gyo::Tools::Framework
