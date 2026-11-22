#include "engine/world_asset/WorldAssetModule.hpp"

#include <span>
#include <string>
#include <utility>
#include <vector>

#include "engine/asset/Catalog.hpp"
#include "engine/asset/TextLoader.hpp"
#include "engine/base/Result.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/World.hpp"

#include "AssetAccess.hpp"

namespace GYO::Engine::WorldAsset {

namespace {

using SystemResult = Base::Result<void, Kernel::KernelError>;

const WorldAssetOptions& OptionsOf(const WorldAssetOptions* given) {
    static const WorldAssetOptions defaults{};
    return given != nullptr ? *given : defaults;
}

// The apply point (5.6): the store's maintenance, then the view, pulled again only when the store's version is
// not the one the view was pulled at. A view that is gone (an author replaced or removed it) is built again; it
// is in place from the next sync point of the frame on. With no AssetRequests there is no store to reach.
SystemResult ApplyAssets(Kernel::Res<AssetRequests> requests, Kernel::ResMut<AssetView> view, Kernel::Commands commands) {
    if (!requests.Present()) return {};
    Asset::AssetStore& store = Detail::AssetAccess::Store(*requests);
    store.Pump();
    if (!view.Present()) {
        commands.SetResource(Detail::AssetAccess::MakeView(store.Snapshot()));
        return {};
    }
    if (auto pulled = store.PullIfChanged(Detail::AssetAccess::Version(view.Read()))) {
        view.Get() = Detail::AssetAccess::MakeView(std::move(*pulled));
    }
    return {};
}

Kernel::SystemOptions Mounted(std::string name, const std::string& schedule, const std::string& stage) {
    Kernel::SystemOptions options;
    options.name = std::move(name);
    Kernel::Mount mount;
    mount.schedule = schedule;
    mount.stage = stage;
    options.mounts.push_back(std::move(mount));
    return options;
}

Asset::AssetStoreOptions StoreOptionsOf(const AssetOptions& options) {
    Asset::AssetStoreOptions store;
    store.contentRoot = options.contentRoot;
    store.sourceDigest = options.sourceDigest;
    store.maxInFlight = options.maxInFlight;
    store.byteLimits = options.byteLimits;
    return store;
}

} // namespace

void WorldAssetModule::Configure(Orchestration::ProductSetup& setup) {
    const AssetOptions* given = setup.Options<AssetOptions>();
    const AssetOptions options = given != nullptr ? *given : AssetOptions{};
    // The catalog is content: read once, kept for the Product's life (5.4). No content root: an empty catalog.
    std::vector<Asset::CatalogEntry> entries;
    if (!options.contentRoot.empty()) {
        auto content = Asset::ReadContent(options.contentRoot);
        if (content) {
            for (Asset::Catalog& catalog : content->catalogs) {
                for (Asset::CatalogEntry& entry : catalog.assets) entries.push_back(std::move(entry));
            }
        } else {
            // A startup error (5.8): no World is built. The store is still made (with no entries), so the units
            // that register loaders find it and report nothing of their own.
            const Asset::CatalogFailure& failure = content.error();
            setup.Report("WA-E1", "the content at '" + options.contentRoot + "' does not read as the catalog Data Contract: " +
                                      std::string(Asset::ToString(failure.code)) + ": " + failure.message +
                                      (failure.detail.empty() ? std::string() : " (" + failure.detail + ")"));
        }
    }
    Asset::AssetStore& store = setup.Buffer().Create<Asset::AssetStore>(
        std::span<const Asset::CatalogEntry>(entries), StoreOptionsOf(options), options.executor);
    store_ = &store;
    setup.Buffer().Create<Asset::AssetLoaderRegistry>(store.Loaders());
    if (auto registered = store.Loaders().Register<Asset::TextLoader>(); !registered) {
        setup.Report("WA-E2", "registering the text loader failed: " +
                                  std::string(Asset::ToString(registered.error().code)) + ": " + registered.error().message);
    }
    // The Product's end (O-A4): no job starts after this, and what did not finish counts as discarded in the
    // store's counts. No World is left to log to (5.8), so the count is the only record.
    setup.OnDestroy([&store] { store.Shutdown(Asset::AssetShutdown::Discard); });
}

void WorldAssetModule::Build(Orchestration::WorldSetup& setup) {
    const WorldAssetOptions& options = OptionsOf(setup.Options<WorldAssetOptions>());
    Kernel::Registrar& r = setup.Registrar();
    r.Resource<AssetRequests>({.name = "AssetRequests"});
    r.Resource<AssetView>({.name = "AssetView"});
    r.AddSystem<&ApplyAssets>(Mounted("ApplyAssets", options.applySchedule, options.applyStage));

    // A World's start (5.6): maintenance first, so a DropOldFirst replacement's old World's drops are taken
    // before anything here requests, then the view. Requests made later in the prepare functions, beforeSwap or
    // game initialization hit what is loaded; new ones enter the view at the next apply point (G-3).
    Asset::AssetStore* store = store_;
    setup.Prepare([store](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
        store->Pump();
        world.SetResource(Detail::AssetAccess::MakeRequests(*store));
        world.SetResource(Detail::AssetAccess::MakeView(store->Snapshot()));
        return {};
    });
}

} // namespace GYO::Engine::WorldAsset
