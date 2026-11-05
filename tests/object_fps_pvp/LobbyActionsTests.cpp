#include <doctest/doctest.h>

#include "RetroFPS/Pvp/LobbyActions.hpp"

#include <array>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// v8-product-ui-action-check (docs/tools/plans/v8/ui-navigation.md): the
// lobby's handled actions against the lobby document's declarations.
namespace {
using Engine::Ui::UiActionDeclaration;
using Engine::Ui::UiActionPayloadType;
using fps::pvp::CheckLobbyActions;
using fps::pvp::LobbyAction;
using fps::pvp::LobbyActionEntry;

constexpr std::array<LobbyActionEntry, 2> kHandled{{
    {"test.say", UiActionPayloadType::String, LobbyAction::SubmitAddress},
    {"test.go", UiActionPayloadType::None, LobbyAction::Refresh},
}};

std::vector<UiActionDeclaration> Declared(std::initializer_list<UiActionDeclaration> declarations) {
    return declarations;
}

std::string LobbyDocumentText() {
    std::ifstream file(PVP_LOBBY_UI_PATH, std::ios::binary);
    REQUIRE(file.good());
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

bool Contains(const std::string& text, const std::string_view part) { return text.find(part) != std::string::npos; }
} // namespace

TEST_CASE("Lobby actions that match their declarations pass the check") {
    CHECK(CheckLobbyActions(Declared({{"test.say", UiActionPayloadType::String}, {"test.go", UiActionPayloadType::None}}), kHandled)
              .empty());
    CHECK_MESSAGE(CheckLobbyActions(Declared({{"test.go", UiActionPayloadType::None}, {"test.say", UiActionPayloadType::String}}), kHandled)
                      .empty(),
                  "the order of the declarations does not matter");
}

TEST_CASE("A declared lobby action with no handler fails the check and is named") {
    const auto message = CheckLobbyActions(Declared({{"test.say", UiActionPayloadType::String}, {"test.go", UiActionPayloadType::None},
                                                     {"test.extra", UiActionPayloadType::None}}),
                                           kHandled);
    CHECK(Contains(message, "declared but not handled: test.extra"));
}

TEST_CASE("A handled lobby action with no declaration fails the check and is named") {
    const auto message = CheckLobbyActions(Declared({{"test.say", UiActionPayloadType::String}}), kHandled);
    CHECK(Contains(message, "handled but not declared: test.go"));
}

TEST_CASE("A lobby action handled with another payload type than declared fails the check and is named") {
    const auto message = CheckLobbyActions(Declared({{"test.say", UiActionPayloadType::None}, {"test.go", UiActionPayloadType::None}}), kHandled);
    CHECK(Contains(message, "payload type differs from the declaration: test.say"));
    CHECK_MESSAGE(Contains(CheckLobbyActions(Declared({{"test.say", UiActionPayloadType::String}, {"test.go", UiActionPayloadType::Number}}),
                                             kHandled),
                           "payload type differs from the declaration: test.go"),
                  "none against number differs too");
}

TEST_CASE("A lobby action handled twice fails the check and is named once") {
    constexpr std::array<LobbyActionEntry, 3> twice{{
        {"test.go", UiActionPayloadType::None, LobbyAction::Refresh},
        {"test.say", UiActionPayloadType::String, LobbyAction::SubmitAddress},
        {"test.go", UiActionPayloadType::String, LobbyAction::Create},
    }};
    const auto message = CheckLobbyActions(Declared({{"test.say", UiActionPayloadType::String}, {"test.go", UiActionPayloadType::None}}), twice);
    CHECK(Contains(message, "handled more than once: test.go"));
    CHECK_FALSE(Contains(message, "test.go, test.go"));
}

TEST_CASE("Every mismatched lobby action is named in one message") {
    const auto message = CheckLobbyActions(Declared({{"test.say", UiActionPayloadType::Number}, {"test.a", UiActionPayloadType::None},
                                                     {"test.b", UiActionPayloadType::String}}),
                                           kHandled);
    CHECK(Contains(message, "declared but not handled: test.a, test.b"));
    CHECK(Contains(message, "handled but not declared: test.go"));
    CHECK(Contains(message, "payload type differs from the declaration: test.say"));
}

// What each lobby id does: the handler the product's table gives it and the
// payload it reads. A swapped or retyped entry still passes the check against
// the document, so the table itself is fixed here.
TEST_CASE("The product's lobby actions map each id to its handler and payload") {
    const std::array<LobbyActionEntry, 6> expected{{
        {"pvp.submit_address", UiActionPayloadType::String, LobbyAction::SubmitAddress},
        {"pvp.refresh", UiActionPayloadType::None, LobbyAction::Refresh},
        {"pvp.create", UiActionPayloadType::None, LobbyAction::Create},
        {"pvp.join", UiActionPayloadType::None, LobbyAction::Join},
        {"pvp.leave", UiActionPayloadType::None, LobbyAction::Leave},
        {"pvp.quit", UiActionPayloadType::None, LobbyAction::Quit},
    }};
    REQUIRE(fps::pvp::kLobbyActions.size() == expected.size());
    for (const auto& want : expected) {
        const auto* entry = fps::pvp::FindLobbyAction(want.id);
        REQUIRE_MESSAGE(entry != nullptr, want.id);
        CHECK_MESSAGE(entry->action == want.action, want.id);
        CHECK_MESSAGE(entry->payload == want.payload, want.id);
    }
    CHECK(fps::pvp::FindLobbyAction("pvp.unknown") == nullptr);
}

TEST_CASE("The product lobby document and the product's lobby actions match") {
    std::string error;
    const auto document = fps::pvp::ParseLobbyDocument(LobbyDocumentText(), "pvp_lobby.json", error);
    CHECK_MESSAGE(document.has_value(), error);
    CHECK(error.empty());
}

TEST_CASE("Starting the lobby fails when its document declares an action the product does not handle") {
    auto text = LobbyDocumentText();
    const std::string actions = "\"actions\": [";
    const auto at = text.find(actions);
    REQUIRE(at != std::string::npos);
    text.insert(at + actions.size(), "{\"id\": \"pvp.unhandled\", \"payload\": \"none\"},");
    std::string error;
    const auto document = fps::pvp::ParseLobbyDocument(text, "pvp_lobby.json", error);
    CHECK_FALSE(document.has_value());
    CHECK_MESSAGE(Contains(error, "declared but not handled: pvp.unhandled"), error);
}

TEST_CASE("Starting the lobby fails with the codec's message when the document does not parse") {
    const auto parsed = Engine::Ui::UiDocumentCodec::Parse("{", "broken.json");
    REQUIRE_FALSE(parsed);
    std::string error;
    CHECK_FALSE(fps::pvp::ParseLobbyDocument("{", "broken.json", error).has_value());
    CHECK(error == fps::pvp::ExplainUi(parsed.error()));
}
