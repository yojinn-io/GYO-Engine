#include "object_fps_pvp/client/LobbyWorld.hpp"

#include "object_fps_pvp/lobby/AddressText.hpp"

#include "client/SceneBuilds.hpp"

#include "engine/kernel/Kernel.hpp"
#include "engine/net/NetService.hpp"
#include "engine/ui/UiRoot.hpp"

#include <charconv>
#include <cstdint>
#include <utility>
#include <variant>

namespace object_fps_pvp::client {

namespace Base = GYO::Engine::Base;
namespace N = GYO::Engine::Net;
namespace Ui = GYO::Engine::Ui;

namespace {

constexpr std::string_view kInvitation = "Match のアドレスを入力してください";
constexpr std::string_view kHint = "クリックして入力、Enter で接続（Tab で移動）";

// The user id this Client connects with (the Match without a key takes any; --user).
struct LobbyUser final {
    std::uint64_t userId = 0;
};
void HashValue(K::Hasher&, const LobbyUser&) {}

K::SystemResult LobbyActions(K::MessageReader<Ui::UiAction> actions, K::Res<LobbyUser> user, K::ResMut<SceneRequest> request,
                             K::Query<K::Write<Ui::UiBindings>> bindings) {
    for (const Ui::UiAction* action : actions.Read()) {
        if (action->action == "pvp.quit") {
            request->next = SceneChange{Scene::Menu, {}, std::nullopt, {}};
        } else if (action->action == "pvp.submit_address") {
            const auto* text = std::get_if<std::string>(&action->payload);
            const std::string typed = text != nullptr ? *text : std::string();
            auto root = bindings.GetMut<Ui::UiBindings>(action->root);
            if (const auto server = ParseMatchAddress(typed)) {
                SceneChange next{Scene::MultiGame, {}, N::NetConnectTarget{*server, user->userId, {}}, typed};
                request->next = std::move(next);
                if (root) {
                    root->Set("address", typed);
                    root->Set("status", std::string("接続しています…"));
                }
            } else if (root) {
                // The text stays in the field (written back: a binding change drops the text being edited, UI-4).
                root->Set("address", typed);
                root->Set("status", std::string("アドレスの形が違います。A.B.C.D:ポート（例：192.168.0.10:40000）"));
            }
        }
    }
    return {};
}

} // namespace

std::optional<GYO::Engine::OS::DatagramAddress> ParseMatchAddress(const std::string_view text) {
    const std::size_t colon = text.rfind(':');
    if (colon == std::string_view::npos) return std::nullopt;
    const std::string_view portText = text.substr(colon + 1);
    std::uint16_t port = 0;
    const auto parsed = std::from_chars(portText.data(), portText.data() + portText.size(), port);
    if (parsed.ec != std::errc{} || parsed.ptr != portText.data() + portText.size() || port == 0) return std::nullopt;
    auto address = GYO::Engine::OS::DatagramAddress::Parse(text.substr(0, colon), port);
    if (!address) return std::nullopt;
    return *address;
}

void BuildLobby(O::WorldSetup& setup, const SceneChange& change, const std::uint64_t user) {
    K::Registrar& r = setup.Registrar();
    r.Resource<LobbyUser>({.name = "object_fps_pvp.LobbyUser"});
    r.AddSystem<&LobbyActions>(InSceneUpdate("object_fps_pvp.LobbyActions"));
    setup.Prepare([change, user](K::World& w) -> Base::Result<void, O::OrchestrationError> {
        w.SetResource(LobbyUser{user});
        Ui::UiBindings bindings;
        bindings.Set("address", change.address);
        bindings.Set("status", change.status.empty() ? std::string(kInvitation) : change.status);
        bindings.Set("hint", std::string(kHint));
        PlaceSceneDocument(w, "object_fps_pvp.ui.lobby", "lobby", std::move(bindings),
                           [](const std::string_view element, const std::string_view text) {
                               return element == "address" ? lobby::FilterAddressText(text) : std::string(text);
                           });
        return {};
    });
}

} // namespace object_fps_pvp::client
