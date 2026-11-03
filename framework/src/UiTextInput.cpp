#include "gyo/framework/UiTextInput.hpp"

namespace Gyo::Tools::Framework {

std::vector<Engine::Ui::UiTextInput>
ToUiTextInput(const std::span<const Engine::Input::TextEvent> events, const TextShortcutModifier shortcut,
              const std::function<std::string()>& clipboard) {
    using Engine::Input::Key;
    using Engine::Input::TextEvent;
    using Engine::Ui::UiTextInputKind;
    std::vector<Engine::Ui::UiTextInput> result;
    for (const TextEvent& entry : events) {
        const bool shortcutHeld = shortcut == TextShortcutModifier::Gui ? entry.modifiers.gui : entry.modifiers.ctrl;
        if (entry.kind == TextEvent::Kind::Text) {
            result.push_back({UiTextInputKind::Insert, entry.text});
        } else if (entry.kind == TextEvent::Kind::Composition) {
            result.push_back({UiTextInputKind::Composition, entry.text, entry.cursor});
        } else if (shortcutHeld && entry.layoutKey == Key::A) {
            result.push_back({UiTextInputKind::SelectAll, {}});
        } else if (shortcutHeld && entry.layoutKey == Key::V) {
            result.push_back({UiTextInputKind::Insert, clipboard ? clipboard() : std::string{}});
        } else if (entry.layoutKey == Key::Backspace) {
            result.push_back({UiTextInputKind::Backspace, {}});
        } else if (entry.layoutKey == Key::Delete) {
            result.push_back({UiTextInputKind::Delete, {}});
        } else if (entry.layoutKey == Key::Left) {
            result.push_back({UiTextInputKind::MoveLeft, {}});
        } else if (entry.layoutKey == Key::Right) {
            result.push_back({UiTextInputKind::MoveRight, {}});
        } else if (entry.layoutKey == Key::Home) {
            result.push_back({UiTextInputKind::MoveHome, {}});
        } else if (entry.layoutKey == Key::End) {
            result.push_back({UiTextInputKind::MoveEnd, {}});
        }
    }
    return result;
}

} // namespace Gyo::Tools::Framework
