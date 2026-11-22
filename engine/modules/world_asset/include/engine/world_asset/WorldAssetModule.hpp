#pragma once

// The World end of asset (declaration docs/architecture/target/v2/asset-declaration.zh-Hant.md, 5.1, 5.4 to 5.7,
// 5.9.1; step 3 of section 9): the registration unit that owns the Product's AssetStore and puts asset into every
// World it is built into.
//
// - Configure (Product level, once): reads AssetOptions from the product description, reads the content's catalog,
//   makes the AssetStore (held by this unit, not put in the Product's buffer: only world_asset requests, reads
//   and pumps), Creates its AssetLoaderRegistry (a unit that owns a type Finds the registry in its own Configure
//   and registers its loader) and its AssetStoreDiagnostics (the counts, read-only) in the Product's buffer,
//   registers the text loader, and stops the store when the Product is destroyed (OnDestroy: Shutdown(Discard),
//   which returns once no job is in a loader; the discarded jobs stay in the store's counts).
// - Build (each World): registers AssetRequests and AssetView, mounts the apply System ApplyAssets (Main/First by
//   default) and gives a prepare function that first runs the store's maintenance and then pulls the view.
// - ApplyAssets, each frame: maintenance (Pump), then the view pulled again only when the store's version
//   changed. It runs while the World generates too (world-gen G-1): an author's generation System waits with the
//   view's readiness and Hold().
//
// The kernel and the orchestration layer know nothing of asset (RC-AS5): the World sees results through its own
// apply System, not through an exchange function.

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "engine/asset/AssetExecutor.hpp"
#include "engine/asset/AssetStore.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"
#include "engine/world_asset/AssetRequests.hpp"
#include "engine/world_asset/AssetView.hpp"

namespace GYO::Engine::WorldAsset {

// The store's settings, given when it is made (decision 13; Bevy's AssetPlugin { file_path, ... }):
// ProductDescription::options. None given: these defaults.
struct AssetOptions final {
    // The content root, a native path in UTF-8, holding content.json. Empty: no content, an empty catalog, and every
    // request is UnknownId (not a startup error).
    std::string contentRoot;
    // SHA-256 of each file's bytes as read (5.5, decision 10).
    bool sourceDigest = true;
    // Jobs in flight at once; 0 is no limit.
    std::size_t maxInFlight = 0;
    // Overrides of loaders' default byte limits, by (type, format).
    std::vector<Asset::AssetByteLimit> byteLimits;
    // Where jobs run; not owned, and it must outlive the Product. world_asset makes no threads: the product puts
    // threads' WorkerPool behind WorkerPoolExecutor (WorkerPoolExecutor.hpp), or gives any other AssetExecutor. Null: no executor, and every request that needs a job is
    // Failed(NoExecutor) (E2, not a startup error: world_asset is carried by default).
    Asset::AssetExecutor* executor = nullptr;
};

// Where ApplyAssets is mounted in one World (WorldDescription::options; none given: the default skeleton's
// Main/First). A World with the skeleton off names its own (O-4).
struct WorldAssetOptions final {
    std::string applySchedule = std::string(Orchestration::DefaultSchedules::frame);
    std::string applyStage = std::string(Orchestration::DefaultSchedules::first);
};

class WorldAssetModule final : public Orchestration::Module {
public:
    WorldAssetModule() = default;

    void Configure(Orchestration::ProductSetup& setup) override;
    void Build(Orchestration::WorldSetup& setup) override;

private:
    // The Product's store, made in Configure. The unit outlives the Product's buffer (O-15: units go last), so
    // the registry and the diagnostics there never outlive it; OnDestroy has stopped it before then.
    std::unique_ptr<Asset::AssetStore> store_;
};

} // namespace GYO::Engine::WorldAsset
