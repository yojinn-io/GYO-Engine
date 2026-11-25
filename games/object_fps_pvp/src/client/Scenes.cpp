#include "object_fps_pvp/client/Scenes.hpp"

#include "client/ClientExchange.hpp"
#include "client/SceneBuilds.hpp"

#include "engine/asset/AssetStore.hpp"
#include "engine/camera/Camera.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/ui/UiAsset.hpp"
#include "engine/window/Window.hpp"
#include "engine/world_asset/AssetRequests.hpp"
#include "engine/world_asset/AssetView.hpp"

#include <utility>
#include <variant>

namespace object_fps_pvp::client {

namespace A = GYO::Engine::Asset;
namespace Base = GYO::Engine::Base;
namespace C = GYO::Engine::Camera;
namespace T = GYO::Engine::Transform;
namespace Ui = GYO::Engine::Ui;
namespace W = GYO::Engine::Window;
namespace WA = GYO::Engine::WorldAsset;

namespace {

// --frames: the window closes itself when the budget runs out (every scene counts on: FrameNotes is carried).
K::SystemResult CountFrames(K::ResMut<FrameNotes> notes, K::ResMut<W::WindowRequest> request) {
    if (notes.Read().framesLeft > 0 && --notes->framesLeft == 0) request->close = true;
    return {};
}

// Whether the scene's document is shown; when it is not, why (its asset failed, or world_ui wrote the root's failure:
// a document that does not validate, a missing binding, a font it could not add). Noted once for the program to log.
K::SystemResult WatchDocument(K::Query<K::Read<Ui::UiRoot>, K::Read<Ui::UiRootState>> roots, K::Res<WA::AssetView> view,
                              K::ResMut<FrameNotes> notes) {
    roots.ForEach([&](K::EntityId, const Ui::UiRoot& root, const Ui::UiRootState& state) {
        if (state.runtime && !state.failure && !notes.Read().shown) notes->shown = true;
        if (!notes.Read().failure.empty()) return;
        if (state.failure) {
            notes->failure = "the document of canvas '" + root.canvas + "': " + state.failure->message;
        } else if (const auto* handle = std::get_if<A::Handle<Ui::UiAsset>>(&root.document)) {
            if (view->State(*handle) == A::AssetState::Failed) {
                const auto* failure = view->Failure(*handle);
                notes->failure = "the document of canvas '" + root.canvas +
                                 "' did not load: " + (failure != nullptr ? Base::Describe(*failure) : std::string("Failed"));
            }
        }
    });
    return {};
}

// ---- The main menu (assets/object_fps_pvp/ui/pvp_menu.json, canvas "menu").

constexpr std::string_view kMenuInvitation = "モードを選んでください";
constexpr std::string_view kNotYet = "準備中です";

K::SystemResult MenuActions(K::MessageReader<Ui::UiAction> actions, K::Query<K::Write<Ui::UiBindings>> bindings,
                            K::ResMut<SceneRequest> request, K::ResMut<W::WindowRequest> window) {
    for (const Ui::UiAction* action : actions.Read()) {
        if (action->action == "pvp.quit") {
            window->close = true;
        } else if (action->action == "pvp.multi") {
            request->next = SceneChange{Scene::Lobby, {}, std::nullopt, {}};
        } else if (action->action == "pvp.solo" || action->action == "pvp.settings") {
            if (auto root = bindings.GetMut<Ui::UiBindings>(action->root)) root->Set("status", std::string(kNotYet));
        }
    }
    return {};
}

} // namespace

bool SimPlays(const Scene scene) noexcept { return scene == Scene::MultiGame; }

bool SceneChangeWanted(O::Product& product) {
    const auto* request = std::as_const(product.World("Frame").GetWorld()).Resource<SceneRequest>();
    return request != nullptr && request->next.has_value();
}

Base::Result<void, O::OrchestrationError> ChangeScene(O::Product& product) {
    auto* director = product.Buffer().Find<SceneDirector>();
    auto* request = product.World("Frame").GetWorld().ResourceMut<SceneRequest>();
    if (director == nullptr || request == nullptr || !request->next) return {};
    const bool simBefore = SimPlays(director->Current().scene);
    SceneChange next = std::move(*request->next);
    request->next.reset();
    const bool simAfter = SimPlays(next.scene);
    director->Enter(std::move(next));
    // Neither World's last picture or controls reach the next scene.
    if (auto* exchange = product.Buffer().Find<ClientExchange>()) exchange->Reset();
    if (simBefore || simAfter) {
        if (auto replaced = product.ReplaceWorld("Sim", {}); !replaced) return Base::Err(std::move(replaced).error());
    }
    return product.ReplaceWorld("Frame", {});
}

K::SystemOptions InSceneUpdate(std::string name, std::vector<std::string> after) {
    K::SystemOptions options;
    options.name = std::move(name);
    K::Mount mount;
    mount.schedule = std::string(O::DefaultSchedules::frame);
    mount.stage = std::string(O::DefaultSchedules::update);
    mount.after = std::move(after);
    options.mounts.push_back(std::move(mount));
    return options;
}

void PlaceSceneDocument(K::World& world, const std::string_view documentId, const std::string_view canvas, Ui::UiBindings bindings,
                        Ui::UiTextFilter filter) {
    // The camera the document is laid out in (its target is the window: window_camera writes the size).
    world.Insert(kSceneCamera, C::Camera{});
    world.Insert(kSceneCamera, C::Camera2d{});
    world.Insert(kSceneCamera, C::CameraTarget{});
    world.Insert(kSceneCamera, C::Projection{C::OrthographicProjection::Default2d()});
    world.Insert(kSceneCamera, C::ComputedCamera{});
    const T::Transform at;
    world.Insert(kSceneCamera, at);
    world.Insert(kSceneCamera, T::GlobalTransform{T::ToMatrix(at)});
    // The document, asked for by its asset id; shown once it and its font are loaded.
    Ui::UiRoot root;
    root.document = world.ResourceMut<WA::AssetRequests>()->Load<Ui::UiAsset>(A::AssetId(std::string(documentId)));
    root.canvas = std::string(canvas);
    root.camera = kSceneCamera;
    root.filter = std::move(filter);
    world.Insert(kSceneRoot, std::move(root));
    world.Insert(kSceneRoot, Ui::UiRootState{});
    world.Insert(kSceneRoot, std::move(bindings));
}

void BuildSceneCommon(O::WorldSetup& setup) {
    K::Registrar& r = setup.Registrar();
    r.Resource<SceneRequest>({.name = "object_fps_pvp.SceneRequest"});
    r.Resource<FrameNotes>({.name = "object_fps_pvp.FrameNotes"});
    setup.CarryOver<FrameNotes>("object_fps_pvp.FrameNotes");
    r.AddSystem<&CountFrames>(InSceneUpdate("object_fps_pvp.CountFrames"));
    K::SystemOptions watch;
    watch.name = "object_fps_pvp.WatchDocument";
    K::Mount last;
    last.schedule = std::string(O::DefaultSchedules::frame);
    last.stage = std::string(O::DefaultSchedules::last);
    watch.mounts.push_back(std::move(last));
    r.AddSystem<&WatchDocument>(std::move(watch));
    // The first World's notes start from the product's budget; a replaced World's come carried (O-7: a prepare
    // function that puts a default does not overwrite a value already there).
    const auto* start = setup.ProductBuffer().Find<ClientStart>();
    const std::int64_t frames = start != nullptr ? start->frames : -1;
    setup.Prepare([frames](K::World& w) -> Base::Result<void, O::OrchestrationError> {
        w.SetResource(SceneRequest{});
        if (w.Resource<FrameNotes>() == nullptr) w.SetResource(FrameNotes{frames, false, {}, {}, {}});
        return {};
    });
}

void BuildMenu(O::WorldSetup& setup, const SceneChange& change) {
    setup.Registrar().AddSystem<&MenuActions>(InSceneUpdate("object_fps_pvp.MenuActions"));
    setup.Prepare([status = change.status](K::World& w) -> Base::Result<void, O::OrchestrationError> {
        Ui::UiBindings bindings;
        bindings.Set("status", status.empty() ? std::string(kMenuInvitation) : status);
        PlaceSceneDocument(w, "object_fps_pvp.ui.menu", "menu", std::move(bindings), {});
        return {};
    });
}

} // namespace object_fps_pvp::client
