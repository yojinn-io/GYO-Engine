#pragma once

#include "engine/ui/UiDocument.hpp"
#include "engine/ui/UiDocumentCodec.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace fps::pvp {

// The lobby's UI actions this product handles (v8-product-ui-action-check).
// The lobby document declares its actions; start-up checks both lists against
// each other so that a declared action with no handler, a handler for an
// undeclared action, or a different payload type fails with the ids named
// instead of a button that silently does nothing.
enum class LobbyAction {
    SubmitAddress,
    Refresh,
    Create,
    Join,
    Leave,
    Quit,
};

struct LobbyActionEntry final {
    std::string_view id;
    Engine::Ui::UiActionPayloadType payload{Engine::Ui::UiActionPayloadType::None};
    LobbyAction action{LobbyAction::Refresh};
};

inline constexpr std::array<LobbyActionEntry, 6> kLobbyActions{{
    {"pvp.submit_address", Engine::Ui::UiActionPayloadType::String, LobbyAction::SubmitAddress},
    {"pvp.refresh", Engine::Ui::UiActionPayloadType::None, LobbyAction::Refresh},
    {"pvp.create", Engine::Ui::UiActionPayloadType::None, LobbyAction::Create},
    {"pvp.join", Engine::Ui::UiActionPayloadType::None, LobbyAction::Join},
    {"pvp.leave", Engine::Ui::UiActionPayloadType::None, LobbyAction::Leave},
    {"pvp.quit", Engine::Ui::UiActionPayloadType::None, LobbyAction::Quit},
}};

[[nodiscard]] inline const LobbyActionEntry* FindLobbyAction(const std::string_view id) {
    const auto found = std::find_if(kLobbyActions.begin(), kLobbyActions.end(),
        [id](const LobbyActionEntry& entry) { return entry.id == id; });
    return found == kLobbyActions.end() ? nullptr : &*found;
}

// Empty when every declared action is handled with the payload type it is
// declared with, every handled action is declared and handled once; otherwise
// one message naming every id that does not match. (The codec already rejects
// an id declared twice.)
[[nodiscard]] inline std::string CheckLobbyActions(const std::span<const Engine::Ui::UiActionDeclaration> declared,
                                                   const std::span<const LobbyActionEntry> handled) {
    std::string unhandled, undeclared, payload, repeated;
    const auto append = [](std::string& list, const std::string_view id) {
        list += list.empty() ? "" : ", ";
        list += id;
    };
    for (const auto& declaration : declared) {
        const auto entry = std::find_if(handled.begin(), handled.end(),
            [&](const LobbyActionEntry& candidate) { return candidate.id == declaration.id; });
        if (entry == handled.end()) append(unhandled, declaration.id);
        else if (entry->payload != declaration.payload) append(payload, declaration.id);
    }
    for (auto entry = handled.begin(); entry != handled.end(); ++entry) {
        const auto same = [&](const LobbyActionEntry& other) { return other.id == entry->id; };
        if (std::find_if(handled.begin(), entry, same) == entry && std::count_if(entry, handled.end(), same) > 1)
            append(repeated, entry->id);
    }
    for (const auto& entry : handled) {
        const bool found = std::any_of(declared.begin(), declared.end(),
            [&](const Engine::Ui::UiActionDeclaration& declaration) { return declaration.id == entry.id; });
        if (!found) append(undeclared, entry.id);
    }
    std::string message;
    const auto report = [&message](const std::string_view what, const std::string& list) {
        if (list.empty()) return;
        message += message.empty() ? "PvP lobby actions do not match the lobby document: " : "; ";
        message += what;
        message += list;
    };
    report("declared but not handled: ", unhandled);
    report("handled but not declared: ", undeclared);
    report("payload type differs from the declaration: ", payload);
    report("handled more than once: ", repeated);
    return message;
}

[[nodiscard]] inline std::string ExplainUi(const Engine::Ui::UiError& error) {
    return error.message + (error.jsonPointer.empty() ? "" : " at " + error.jsonPointer);
}

// The lobby document as the application starts it: parsed, then checked
// against the actions this product handles.
[[nodiscard]] inline std::optional<Engine::Ui::UiDocument> ParseLobbyDocument(const std::string_view text,
                                                                              const std::string_view source,
                                                                              std::string& error) {
    auto document = Engine::Ui::UiDocumentCodec::Parse(text, source);
    if (!document) {
        error = ExplainUi(document.error());
        return std::nullopt;
    }
    error = CheckLobbyActions(document.value().actions, kLobbyActions);
    if (!error.empty()) return std::nullopt;
    return std::move(document).value();
}

} // namespace fps::pvp
