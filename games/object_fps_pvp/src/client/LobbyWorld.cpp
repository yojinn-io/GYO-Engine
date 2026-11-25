#include "object_fps_pvp/client/LobbyWorld.hpp"

#include "object_fps_pvp/lobby/AddressText.hpp"

#include "engine/asset/AssetStore.hpp"
#include "engine/camera/Camera.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/text/FontLoader.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/ui/UiAsset.hpp"
#include "engine/ui/UiDocumentLoader.hpp"
#include "engine/ui/UiRoot.hpp"
#include "engine/window/Window.hpp"
#include "engine/world_asset/AssetRequests.hpp"
#include "engine/world_asset/AssetView.hpp"

#include <charconv>
#include <cstdint>
#include <utility>
#include <variant>

namespace object_fps_pvp::client {

namespace K = GYO::Kernel;
namespace A = GYO::Engine::Asset;
namespace Base = GYO::Engine::Base;
namespace C = GYO::Engine::Camera;
namespace T = GYO::Engine::Transform;
namespace TX = GYO::Engine::Text;
namespace Ui = GYO::Engine::Ui;
namespace W = GYO::Engine::Window;
namespace WA = GYO::Engine::WorldAsset;

namespace {

// Fixed ids outside the allotter's ranges (as the Client's camera and HUD: ClientWorld.cpp).
constexpr K::EntityId kLobbyCamera = 3'000'030;
constexpr K::EntityId kLobbyRoot = 3'000'031;

constexpr std::string_view kInvitation = "Match のアドレスを入力してください";
constexpr std::string_view kHint = "クリックして入力、Enter で接続（Tab で移動）";

K::SystemResult LobbyActions(K::MessageReader<Ui::UiAction> actions, K::ResMut<LobbyOutcome> result,
                             K::Query<K::Write<Ui::UiBindings>> bindings, K::ResMut<W::WindowRequest> request) {
    for (const Ui::UiAction* action : actions.Read()) {
        if (action->action == "pvp.quit") {
            request->close = true;
        } else if (action->action == "pvp.submit_address") {
            const auto* text = std::get_if<std::string>(&action->payload);
            const std::string typed = text != nullptr ? *text : std::string();
            auto root = bindings.GetMut<Ui::UiBindings>(action->root);
            if (const auto server = ParseMatchAddress(typed)) {
                result->server = *server;
                result->address = typed;
                request->close = true;
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

// Whether the document is shown; when it is not, why (its asset failed, or world_ui wrote the root's failure: a
// document that does not validate, a missing binding, a font it could not add). Noted once for the program to log.
K::SystemResult WatchDocument(K::Query<K::Read<Ui::UiRoot>, K::Read<Ui::UiRootState>> roots, K::Res<WA::AssetView> view,
                              K::ResMut<LobbyOutcome> result) {
    roots.ForEach([&](K::EntityId, const Ui::UiRoot& root, const Ui::UiRootState& state) {
        if (state.runtime && !state.failure && !result.Read().shown) result->shown = true;
        if (!result.Read().failure.empty()) return;
        if (state.failure) {
            result->failure = "the lobby's document: " + state.failure->message;
        } else if (const auto* handle = std::get_if<A::Handle<Ui::UiAsset>>(&root.document)) {
            if (view->State(*handle) == A::AssetState::Failed) {
                const auto* failure = view->Failure(*handle);
                result->failure = "the lobby's document did not load: " + (failure != nullptr ? Base::Describe(*failure) : std::string("Failed"));
            }
        }
    });
    return {};
}

void PlaceAt(K::World& world, const K::EntityId id) {
    const T::Transform t;
    world.Insert(id, t);
    world.Insert(id, T::GlobalTransform{T::ToMatrix(t)});
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

void ConfigureLobby(O::ProductSetup& setup) {
    const auto* registry = setup.Buffer().Find<A::AssetLoaderRegistry>();
    if (registry == nullptr) {
        setup.Report("NoAssetStore", "the lobby needs world_asset's loader registry");
        return;
    }
    if (auto ui = registry->template Register<Ui::UiDocumentLoader>(); !ui) {
        setup.Report("LoaderRefused", "the ui loader was refused: " + Base::Describe(ui.error()));
        return;
    }
    if (auto font = registry->template Register<TX::FontLoader>(); !font) {
        setup.Report("LoaderRefused", "the font loader was refused: " + Base::Describe(font.error()));
    }
}

O::WorldDescription LobbyGameWorld(LobbyOptions options, const bool window) {
    O::WorldDescription world;
    world.name = "Game";
    world.units = {"entity_ids", "world_asset", "transform", "camera", "input", "window", "window_camera", "world_ui", "world_render"};
    if (window) world.units.push_back("sdl_window");
    world.build = [options](O::WorldSetup& setup) {
        K::Registrar& r = setup.Registrar();
        r.Resource<LobbyOutcome>({.name = "object_fps_pvp.LobbyOutcome"});
        K::SystemOptions actions;
        actions.name = "object_fps_pvp.LobbyActions";
        K::Mount mount;
        mount.schedule = std::string(O::DefaultSchedules::frame);
        mount.stage = std::string(O::DefaultSchedules::update);
        actions.mounts.push_back(std::move(mount));
        r.AddSystem<&LobbyActions>(std::move(actions));
        K::SystemOptions watch;
        watch.name = "object_fps_pvp.WatchDocument";
        K::Mount after;
        after.schedule = std::string(O::DefaultSchedules::frame);
        after.stage = std::string(O::DefaultSchedules::last);
        watch.mounts.push_back(std::move(after));
        r.AddSystem<&WatchDocument>(std::move(watch));
        setup.Prepare([options](K::World& w) -> Base::Result<void, O::OrchestrationError> {
            w.SetResource(LobbyOutcome{});
            // The camera the document is laid out in (its target is the window: window_camera writes the size).
            w.Insert(kLobbyCamera, C::Camera{});
            w.Insert(kLobbyCamera, C::Camera2d{});
            w.Insert(kLobbyCamera, C::CameraTarget{});
            w.Insert(kLobbyCamera, C::Projection{C::OrthographicProjection::Default2d()});
            w.Insert(kLobbyCamera, C::ComputedCamera{});
            PlaceAt(w, kLobbyCamera);
            // The document, asked for by its asset id; shown once it and its font are loaded.
            Ui::UiRoot root;
            root.document = w.ResourceMut<WA::AssetRequests>()->Load<Ui::UiAsset>(A::AssetId("object_fps_pvp.ui.lobby"));
            root.canvas = "lobby";
            root.camera = kLobbyCamera;
            root.filter = [](const std::string_view element, const std::string_view text) {
                return element == "address" ? lobby::FilterAddressText(text) : std::string(text);
            };
            w.Insert(kLobbyRoot, std::move(root));
            w.Insert(kLobbyRoot, Ui::UiRootState{});
            Ui::UiBindings bindings;
            bindings.Set("address", options.address);
            bindings.Set("status", options.status.empty() ? std::string(kInvitation) : options.status);
            bindings.Set("hint", std::string(kHint));
            w.Insert(kLobbyRoot, std::move(bindings));
            return {};
        });
    };
    return world;
}

O::WorldDescription LobbyRenderWorld(const bool gpu) {
    O::WorldDescription world;
    world.name = "Render";
    world.extractFrom = "Game";
    world.units = {"entity_ids", "render_world", "camera_render", "ui_render"};
    if (gpu) world.units.push_back("sdl_gpu");
    return world;
}

} // namespace object_fps_pvp::client
