#include "object_fps_pvp/client/Scenes.hpp"

#include "client/ClientExchange.hpp"
#include "client/SceneBuilds.hpp"

#include "engine/asset/AssetStore.hpp"
#include "engine/base/Assert.hpp"
#include "engine/camera/Camera.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/ui/UiAsset.hpp"
#include "engine/window/Window.hpp"
#include "engine/world_asset/AssetRequests.hpp"
#include "engine/world_asset/AssetView.hpp"
#include "engine/state_machine/RegisterStateMachine.hpp"

#include "client/Pages.hpp"

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
namespace SM = GYO::Engine::StateMachine;

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

// ---- The main menu (assets/object_fps_pvp/ui/pvp_menu.json): two pages, MenuPage::Main (canvas "menu") and
// MenuPage::Settings (canvas "settings"), each with its own root, put when the page is entered and taken away when it
// is left.

constexpr std::string_view kMenuInvitation = "モードを選んでください";
constexpr K::EntityId kSettingsRoot = 3'000'032;

// The menu's document (held for the scene, so a page's root finds it loaded) and the menu's first status line.
struct MenuDocument final {
    A::Handle<Ui::UiAsset> document;
    std::string status;
};
void HashValue(K::Hasher&, const MenuDocument&) {}

// The settings page's own state: what the player is choosing (applied with 適用), and the file.
struct SettingsEdit final {
    ClientSettings edited;
    std::string path;
};
void HashValue(K::Hasher&, const SettingsEdit&) {}

void ShowSettings(const ClientSettings& settings, Ui::UiBindings& bindings) {
    const bool borderless = settings.window == W::WindowMode::BorderlessFullscreen;
    // Fullscreen takes the screen's size: the size chosen is the window's when it is a window again.
    bindings.Set("resolution", "解像度: " + ResolutionText(settings.resolution) + (borderless ? "（ウィンドウ時）" : ""));
    bindings.Set("window_mode", std::string(borderless ? "画面: 全画面（枠なし）" : "画面: ウィンドウ"));
    bindings.Set("vsync", std::string(settings.vsync ? "VSync: オン" : "VSync: オフ"));
}

Ui::UiRoot PageRoot(const MenuDocument& menu, std::string canvas) {
    Ui::UiRoot root;
    root.document = menu.document;
    root.canvas = std::move(canvas);
    root.camera = kSceneCamera;
    return root;
}

using PageRoots = K::CommandsFor<K::Inserts<Ui::UiRoot, Ui::UiRootState, Ui::UiBindings>, K::Removes<Ui::UiRoot, Ui::UiRootState, Ui::UiBindings>>;

void TakeRoot(PageRoots& roots, const K::EntityId id) {
    roots.Remove<Ui::UiRoot>(id);
    roots.Remove<Ui::UiRootState>(id);
    roots.Remove<Ui::UiBindings>(id);
}

// MenuPage.Enter.Main / Exit.Main.
K::SystemResult EnterMenuMain(K::Res<MenuDocument> menu, PageRoots roots) {
    Ui::UiBindings bindings;
    bindings.Set("status", menu->status.empty() ? std::string(kMenuInvitation) : menu->status);
    roots.Insert(kSceneRoot, PageRoot(*menu, "menu"));
    roots.Insert(kSceneRoot, Ui::UiRootState{});
    roots.Insert(kSceneRoot, std::move(bindings));
    return {};
}
K::SystemResult ExitMenuMain(PageRoots roots) {
    TakeRoot(roots, kSceneRoot);
    return {};
}

// MenuPage.Enter.Settings: the choice starts from the settings in force, the window's mode as the window has it (the
// player may have changed it: macOS's green button). Exit.Settings drops what was not applied.
K::SystemResult EnterSettings(K::Res<MenuDocument> menu, K::Res<ClientSettings> settings, K::Res<W::WindowState> window,
                              K::ResMut<SettingsEdit> edit, PageRoots roots) {
    edit->edited = *settings;
    if (window->known) edit->edited.window = window->mode;
    Ui::UiBindings bindings;
    ShowSettings(edit.Read().edited, bindings);
    bindings.Set("settings_status", std::string("変更したら「適用」を押してください"));
    roots.Insert(kSettingsRoot, PageRoot(*menu, "settings"));
    roots.Insert(kSettingsRoot, Ui::UiRootState{});
    roots.Insert(kSettingsRoot, std::move(bindings));
    return {};
}
K::SystemResult ExitSettings(PageRoots roots) {
    TakeRoot(roots, kSettingsRoot);
    return {};
}

// The menu page: pvp.solo, pvp.multi (other scenes), pvp.settings (the settings page), pvp.quit.
K::SystemResult MenuActions(K::MessageReader<Ui::UiAction> actions, K::ResMut<SceneRequest> request, K::ResMut<MenuPageRequest> page,
                            K::ResMut<W::WindowRequest> window) {
    for (const Ui::UiAction* action : actions.Read()) {
        const std::string& id = action->action;
        if (id == "pvp.quit") window->close = true;
        if (id == "pvp.solo") request->next = SceneChange{.scene = Scene::SoloRoom};
        if (id == "pvp.multi") request->next = SceneChange{.scene = Scene::Lobby};
        if (id == "pvp.settings") page->Request(MenuPage::Settings);
    }
    return {};
}

// The settings page: pvp.resolution / pvp.window_mode / pvp.vsync change what is chosen; pvp.apply puts it in force
// (the window: window's ResizeWindow and SetWindowMode; VSync: the Render World's Presentation, CopyPresentation) and
// saves it; pvp.settings_back goes back to the menu page.
K::SystemResult SettingsActions(K::MessageReader<Ui::UiAction> actions, K::ResMut<SettingsEdit> edit, K::ResMut<ClientSettings> settings,
                                K::Query<K::Write<Ui::UiBindings>> roots, K::MessageWriter<W::ResizeWindow> resize,
                                K::MessageWriter<W::SetWindowMode> mode, K::ResMut<MenuPageRequest> page) {
    for (const Ui::UiAction* action : actions.Read()) {
        auto bindings = roots.GetMut<Ui::UiBindings>(action->root);
        if (!bindings) continue;
        ClientSettings& edited = edit->edited;
        const std::string& id = action->action;
        if (id == "pvp.settings_back") {
            page->Request(MenuPage::Main);
            continue;
        }
        if (id == "pvp.resolution") {
            edited.resolution = NextResolution(edited.resolution);
        } else if (id == "pvp.window_mode") {
            edited.window = edited.window == W::WindowMode::Windowed ? W::WindowMode::BorderlessFullscreen : W::WindowMode::Windowed;
        } else if (id == "pvp.vsync") {
            edited.vsync = !edited.vsync;
        } else if (id == "pvp.apply") {
            // Sent each time (the same size again is harmless, and the window may not be at it: the OS clamps it).
            resize.Send(W::ResizeWindow{edited.resolution});
            mode.Send(W::SetWindowMode{edited.window});
            *settings = edited;
            std::string status = "適用しました";
            if (edit.Read().path.empty()) {
                status += "（保存先がないため、保存していません）";
            } else if (auto saved = SaveSettings(edit.Read().path, edited); !saved) {
                status += "。保存できません: " + Base::Describe(saved.error());
            } else {
                status += "。保存しました";
            }
            bindings->Set("settings_status", status);
        } else {
            continue;
        }
        ShowSettings(edited, *bindings);
    }
    return {};
}

// A System in a page's enter or exit schedule.
K::SystemOptions OnPage(std::string name, std::string schedule) {
    K::SystemOptions options;
    options.name = std::move(name);
    K::Mount mount;
    mount.schedule = std::move(schedule);
    mount.stage = std::string(SM::kEnterExitStage);
    options.mounts.push_back(std::move(mount));
    return options;
}

} // namespace

bool SimPlays(const Scene scene) noexcept { return scene == Scene::MultiGame; }
bool SoloPlays(const Scene scene) noexcept { return scene == Scene::SoloGame; }

std::string SoloSavePath(const std::string& dataDirectory) {
    return dataDirectory.empty() ? std::string() : dataDirectory + "/solo-save.json";
}

bool SceneChangeWanted(O::Product& product) {
    const auto* request = std::as_const(product.World("Frame").GetWorld()).Resource<SceneRequest>();
    return request != nullptr && request->next.has_value();
}

Base::Result<void, O::OrchestrationError> ChangeScene(O::Product& product) {
    auto* director = product.Buffer().Find<SceneDirector>();
    GYO_ASSERT(director != nullptr && "the Client's Product needs ConfigureClient (it makes its SceneDirector)");
    auto* request = product.World("Frame").GetWorld().ResourceMut<SceneRequest>();
    if (request == nullptr || !request->next) return {};
    const bool simBefore = SimPlays(director->Current().scene);
    const bool soloBefore = SoloPlays(director->Current().scene);
    SceneChange next = std::move(*request->next);
    request->next.reset();
    const bool simAfter = SimPlays(next.scene);
    const bool soloAfter = SoloPlays(next.scene);
    director->Enter(std::move(next));
    // Neither World's last picture or controls reach the next scene.
    if (auto* exchange = product.Buffer().Find<ClientExchange>()) exchange->Reset();
    if (soloBefore || soloAfter) {
        if (auto replaced = product.ReplaceWorld("Solo", {}); !replaced) return Base::Err(std::move(replaced).error());
    }
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

void PlaceSceneCamera(K::World& world) {
    // The camera the document is laid out in (its target is the window: window_camera writes the size).
    world.Insert(kSceneCamera, C::Camera{});
    world.Insert(kSceneCamera, C::Camera2d{});
    world.Insert(kSceneCamera, C::CameraTarget{});
    world.Insert(kSceneCamera, C::Projection{C::OrthographicProjection::Default2d()});
    world.Insert(kSceneCamera, C::ComputedCamera{});
    const T::Transform at;
    world.Insert(kSceneCamera, at);
    world.Insert(kSceneCamera, T::GlobalTransform{T::ToMatrix(at)});
}

void PlaceSceneDocument(K::World& world, const std::string_view documentId, const std::string_view canvas, Ui::UiBindings bindings,
                        Ui::UiTextFilter filter) {
    PlaceSceneCamera(world);
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
    r.Resource<ClientSettings>({.name = "object_fps_pvp.ClientSettings"});
    setup.CarryOver<ClientSettings>("object_fps_pvp.ClientSettings");
    r.Resource<GatewayClient>({.name = "object_fps_pvp.GatewayClient"});
    setup.CarryOver<GatewayClient>("object_fps_pvp.GatewayClient");
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
    GYO_ASSERT(start != nullptr && "the Client's Product needs ConfigureClient (it keeps its ClientStart)");
    const auto* director = setup.ProductBuffer().Find<SceneDirector>();
    const Scene scene = director != nullptr ? director->Current().scene : Scene::Menu;
    const bool gatewayNeeded = scene == Scene::Lobby || scene == Scene::Room || scene == Scene::MultiGame;
    setup.Prepare([start = *start, gatewayNeeded](K::World& w) -> Base::Result<void, O::OrchestrationError> {
        w.SetResource(SceneRequest{});
        if (w.Resource<FrameNotes>() == nullptr) w.SetResource(FrameNotes{start.frames, false, {}, {}, {}});
        if (w.Resource<ClientSettings>() == nullptr) w.SetResource(start.settings);
        PlaceGatewayClient(w, start.gateway, start.dataDirectory, gatewayNeeded);
        return {};
    });
}

void BuildMenu(O::WorldSetup& setup, const SceneChange& change) {
    K::Registrar& r = setup.Registrar();
    r.Resource<MenuDocument>({.name = "object_fps_pvp.MenuDocument"});
    r.Resource<SettingsEdit>({.name = "object_fps_pvp.SettingsEdit"});
    SM::RegisterStateMachine<MenuPage>(setup);
    r.AddSystem<&EnterMenuMain>(OnPage("object_fps_pvp.EnterMenuMain", SM::EnterScheduleName(MenuPage::Main)));
    r.AddSystem<&ExitMenuMain>(OnPage("object_fps_pvp.ExitMenuMain", SM::ExitScheduleName(MenuPage::Main)));
    r.AddSystem<&EnterSettings>(OnPage("object_fps_pvp.EnterSettings", SM::EnterScheduleName(MenuPage::Settings)));
    r.AddSystem<&ExitSettings>(OnPage("object_fps_pvp.ExitSettings", SM::ExitScheduleName(MenuPage::Settings)));
    K::SystemOptions menu = InSceneUpdate("object_fps_pvp.MenuActions");
    menu.mounts.front().conditions.push_back(SM::InState<MenuPage, MenuPage::Main>());
    r.AddSystem<&MenuActions>(std::move(menu));
    K::SystemOptions settings = InSceneUpdate("object_fps_pvp.SettingsActions");
    settings.mounts.front().conditions.push_back(SM::InState<MenuPage, MenuPage::Settings>());
    r.AddSystem<&SettingsActions>(std::move(settings));
    const auto* start = setup.ProductBuffer().Find<ClientStart>();
    GYO_ASSERT(start != nullptr && "the Client's Product needs ConfigureClient (it keeps its ClientStart)");
    setup.Prepare([status = change.status, path = start->settingsPath](K::World& w) -> Base::Result<void, O::OrchestrationError> {
        w.SetResource(SettingsEdit{*w.Resource<ClientSettings>(), path});
        w.SetResource(MenuDocument{w.ResourceMut<WA::AssetRequests>()->Load<Ui::UiAsset>(A::AssetId("object_fps_pvp.ui.menu")), status});
        PlaceSceneCamera(w);
        // The pages' roots: the entities are there from the start (a page's enter puts the root's components on them).
        w.Insert(kSceneRoot, T::Transform{});
        w.Insert(kSettingsRoot, T::Transform{});
        return {};
    });
}

} // namespace object_fps_pvp::client
