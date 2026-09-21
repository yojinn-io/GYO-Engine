#include "doctest/doctest.h"

#include <filesystem>
#include <chrono>
#include <limits>
#include <fstream>
#include <unordered_map>
#include <vector>

#include "engine/asset/AssetManager.hpp"
#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/AssetRequest.hpp"
#include "engine/asset/core/AssetStorage.hpp"
#include "engine/asset/core/AssetLifetime.hpp"
#include "engine/asset/core/AssetCachePolicy.hpp"
#include "engine/asset/loading/AssetPipeline.hpp"
#include "engine/asset/loading/LoaderRegistry.hpp"
#include "engine/asset/loading/IAssetSource.hpp"
#include "engine/asset/loading/IAssetLoader.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"
#include "engine/asset/catalog/CatalogParser.hpp"


using namespace Engine::Asset;
namespace fs = std::filesystem;

namespace {
    

    // テスト用：メモリから読む IAssetSource
    class MemoryAssetSource final : public Loading::IAssetSource {
    public:
        std::size_t reads = 0;
        void Put(const std::string& path, std::vector<std::byte> bytes) {
            map_[path] = std::move(bytes);
        }

        Engine::Base::Result<std::vector<std::byte>, Engine::Base::Error<AssetErrorCode>>
        ReadAll(std::string_view resolvedPath) override {
            ++reads;
            auto it = map_.find(std::string(resolvedPath));
            if (it == map_.end()) {
                return Engine::Base::Result<std::vector<std::byte>, Engine::Base::Error<AssetErrorCode>>::Err(
                    Engine::Base::Error<AssetErrorCode>::Make(AssetErrorCode::SourceReadFailed, "MemoryAssetSource: not found", std::string(resolvedPath)));
            }
            return Engine::Base::Result<std::vector<std::byte>, Engine::Base::Error<AssetErrorCode>>::Ok(it->second);
        }

    private:
        std::unordered_map<std::string, std::vector<std::byte>> map_;
    };

    static std::vector<std::byte> BytesOf(const std::string& s) {
        std::vector<std::byte> b;
        b.resize(s.size());
        for (size_t i = 0; i < s.size(); ++i) b[i] = static_cast<std::byte>(s[i]);
        return b;
    }

} // namespace

TEST_CASE("AssetManager: sync load -> cache hit") {
    // --- 準備：catalog で AssetId/type を正式に解決する ---
    const fs::path tmp = fs::temp_directory_path() / "asset_manager_cache_hit_test";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    const fs::path catalogPath = tmp / "asset_catalog.json";
    {
        std::ofstream catalogFile(catalogPath, std::ios::binary);
        REQUIRE(catalogFile.good());
        catalogFile << R"({
          "version": 1, "assets": [
            {"id":"ui.title","type":"text","path":"ui/title.txt"}
          ]
        })";
    }

    Resolver::AssetPathResolver::Options resolverOptions;
    resolverOptions.assetsRoot = (tmp / "assets").string();
    Resolver::AssetPathResolver resolver(resolverOptions);
    Catalog::CatalogParser parser;
    AssetCatalog catalog;
    auto catalogResult = catalog.LoadFromFile(catalogPath.string(), parser, resolver);
    REQUIRE(catalogResult);

    // --- pipeline 組み立て（実装に合わせて調整） ---
    Loading::LoaderRegistry registry;
    registry.Register(std::make_unique<Loaders::TextLoader>());

    auto memSource = std::make_unique<MemoryAssetSource>();
    memSource->Put("mem://ui/title.txt", BytesOf("hello"));

    Loading::AssetPipeline pipeline(/*source*/ *memSource, /*registry*/ registry);
    // ↑ コンストラクタが違う場合はここだけ調整

    Core::AssetStorage storage;
    Core::AssetLifetime lifetime;
    Core::AssetCachePolicy::Options options;
    Core::AssetCachePolicy policy(options);

    AssetManager mgr(catalog, pipeline, storage, lifetime, policy, nullptr, nullptr);

    // --- 本体：Load 1回目（miss） ---
    AssetRequest req = AssetRequest::Default();
    req.sync = AssetRequest::SyncWith::Sync;
    req.overridePath = "mem://ui/title.txt"; // テスト用 source のパスだけを上書きする
    req.useTypeHint = true;
    req.expectedType = AssetType::FromString("text");

    auto h1 = mgr.Load(AssetId::FromString("ui.title"), req);
    if (!h1) {
        INFO("code = " << static_cast<int>(h1.error().code));
        INFO("msg  = "  << h1.error().message);
        INFO("detail = "  << h1.error().detail);
        FAIL("mgr.Load failed");
    }

    auto sp1 = mgr.GetShared<Loaders::TextAsset>(h1.value());
    REQUIRE(sp1 != nullptr);
    CHECK(sp1->text == "hello");

    // --- 2回目：reload無しならキャッシュヒット（同generation） ---
    auto h2 = mgr.Load(AssetId::FromString("ui.title"), req);
    REQUIRE(h2);
    CHECK(h2.value().generation() == h1.value().generation());

    fs::remove_all(tmp);
}

TEST_CASE("AssetManager: stale handle release balances its pre-reload reference") {
    const fs::path tmp = fs::temp_directory_path() /
                         "asset_manager_reload_generation_test";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    const fs::path catalogPath = tmp / "asset_catalog.json";
    {
        std::ofstream catalogFile(catalogPath, std::ios::binary);
        REQUIRE(catalogFile.good());
        catalogFile << R"({
          "version": 1, "assets": [
            {"id":"engine.test.reloadable_text","type":"text","path":"reloadable.txt"}
          ]
        })";
    }

    Resolver::AssetPathResolver::Options resolverOptions;
    resolverOptions.assetsRoot = (tmp / "assets").string();
    Resolver::AssetPathResolver resolver(resolverOptions);
    Catalog::CatalogParser parser;
    AssetCatalog catalog;
    REQUIRE(catalog.LoadFromFile(catalogPath.string(), parser, resolver));

    Loading::LoaderRegistry registry;
    registry.Register(std::make_unique<Loaders::TextLoader>());

    auto memSource = std::make_unique<MemoryAssetSource>();
    constexpr auto kMemoryPath = "mem://reloadable.txt";
    memSource->Put(kMemoryPath, BytesOf("before reload"));
    Loading::AssetPipeline pipeline(*memSource, registry);

    Core::AssetStorage storage;
    Core::AssetLifetime lifetime;
    Core::AssetCachePolicy::Options cacheOptions;
    Core::AssetCachePolicy policy(cacheOptions);
    AssetManager manager(catalog, pipeline, storage, lifetime, policy);

    const AssetId id = AssetId::FromString("engine.test.reloadable_text");
    AssetRequest request = AssetRequest::Default();
    request.sync = AssetRequest::SyncWith::Sync;
    request.overridePath = kMemoryPath;

    const auto original = manager.Load(id, request);
    REQUIRE(original);
    REQUIRE(manager.GetShared<Loaders::TextAsset>(original.value()) != nullptr);

    auto* record = storage.Find(id);
    REQUIRE(record != nullptr);
    CHECK(record->refCount == 1);

    // A queued reload must remember that a usable asset existed before the
    // record entered Loading, so KeepOldIfAny can restore it on failure.
    AssetRequest failedQueuedReload = AssetRequest::Reload();
    failedQueuedReload.sync = AssetRequest::SyncWith::Async;
    failedQueuedReload.overridePath = "mem://missing.txt";
    const auto queuedHandle = manager.Load(id, failedQueuedReload);
    REQUIRE(queuedHandle);
    CHECK(record->refCount == 2);
    manager.Update();
    CHECK(record->generation == original.value().generation());
    CHECK(manager.GetState(original.value()) == AssetState::Ready);
    const auto preservedText = manager.GetShared<Loaders::TextAsset>(original.value());
    REQUIRE(preservedText != nullptr);
    CHECK(preservedText->text == "before reload");
    manager.Release(queuedHandle.value());
    CHECK(record->refCount == 1);

    memSource->Put(kMemoryPath, BytesOf("after reload"));
    AssetRequest reload = AssetRequest::Reload();
    reload.sync = AssetRequest::SyncWith::Sync;
    reload.overridePath = kMemoryPath;
    const auto current = manager.Load(id, reload);
    REQUIRE(current);
    CHECK(current.value().generation() != original.value().generation());
    CHECK(record->refCount == 2);

    // Read/query safety remains generation-strict.
    CHECK(manager.GetState(original.value()) == AssetState::Unloaded);
    CHECK(manager.GetShared<Loaders::TextAsset>(original.value()) == nullptr);
    CHECK_FALSE(manager.Acquire(original.value()));

    const auto currentText = manager.GetShared<Loaders::TextAsset>(current.value());
    REQUIRE(currentText != nullptr);
    CHECK(currentText->text == "after reload");

    // A destructive reload failure must also retire the generation. Otherwise
    // a later recovery load would make the old successful handle readable
    // again with unrelated replacement content.
    AssetRequest destructiveReload = AssetRequest::Reload();
    destructiveReload.sync = AssetRequest::SyncWith::Sync;
    destructiveReload.fallback = AssetRequest::Fallback::None;
    destructiveReload.overridePath = "mem://missing.txt";
    const auto failedReload = manager.Load(id, destructiveReload);
    REQUIRE_FALSE(failedReload);
    CHECK(record->generation != current.value().generation());
    CHECK(manager.GetShared<Loaders::TextAsset>(current.value()) == nullptr);

    const auto recovered = manager.Load(id, request);
    REQUIRE(recovered);
    CHECK(recovered.value().generation() == record->generation);
    CHECK(manager.GetShared<Loaders::TextAsset>(current.value()) == nullptr);
    CHECK(record->refCount == 3);

    // The stale handle still balances the reference acquired by its Load call.
    manager.Release(original.value());
    CHECK(record->refCount == 2);

    // Releasing it again must not consume another generation's reference.
    manager.Release(original.value());
    CHECK(record->refCount == 2);
    manager.Release(current.value());
    CHECK(record->refCount == 1);
    CHECK_FALSE(manager.EvictIfPossible(id));

    manager.Release(recovered.value());
    CHECK(record->refCount == 0);
    CHECK(manager.EvictIfPossible(id));

    // Eviction must not reset the incarnation to generation 1 and resurrect a
    // previously issued handle (ABA).
    const auto reincarnated = manager.Load(id, request);
    REQUIRE(reincarnated);
    CHECK(reincarnated.value().generation() != original.value().generation());
    CHECK(reincarnated.value().generation() != current.value().generation());
    CHECK(reincarnated.value().generation() != recovered.value().generation());
    CHECK(manager.GetShared<Loaders::TextAsset>(original.value()) == nullptr);
    manager.Release(reincarnated.value());
    CHECK(manager.EvictIfPossible(id));

    fs::remove_all(tmp);
}

namespace {
struct ManagerFixture {
    fs::path root = fs::temp_directory_path() / ("gyo_manager_contract_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    AssetCatalog catalog;
    MemoryAssetSource source;
    Loading::LoaderRegistry registry;
    Loading::AssetPipeline pipeline{source, registry};
    Core::AssetStorage storage;
    Core::AssetLifetime lifetime;
    Core::AssetCachePolicy policy{Core::AssetCachePolicy::Options{}};
    HotReload::AssetWatcher watcher{HotReload::AssetWatcher::Options{.debounceMs = 0, .emitModified = false}};
    AssetManager manager{catalog, pipeline, storage, lifetime, policy, nullptr, &watcher};
    AssetId id = AssetId::FromString("engine.test.contract");

    ManagerFixture() {
        fs::create_directories(root);
        {
            std::ofstream file(root / "catalog.json");
            file << R"({"version":1,"assets":[{"id":"engine.test.contract","type":"text","path":"text.txt"}]})";
        }
        Catalog::CatalogParser parser;
        Resolver::AssetPathResolver::Options options;
        options.assetsRoot = root.generic_string();
        Resolver::AssetPathResolver resolver(options);
        REQUIRE(catalog.LoadFromFile((root / "catalog.json").string(), parser, resolver));
        registry.Register(std::make_unique<Loaders::TextLoader>());
        source.Put(Path(), BytesOf("old"));
    }
    ~ManagerFixture() { std::error_code error; fs::remove_all(root, error); }
    std::string Path() const { return (root / "text.txt").generic_string(); }
    AssetHandle Initial() {
        auto result = manager.Load(id, AssetRequest::Default());
        REQUIRE(result);
        return result.value();
    }
    AssetRequest ReloadAsync() const {
        auto request = AssetRequest::Reload();
        request.sync = AssetRequest::SyncWith::Async;
        return request;
    }
};
}

TEST_CASE("AssetManager: async reload publishes the reserved handle and balances merged references") {
    ManagerFixture f;
    const auto original = f.Initial();
    f.source.Put(f.Path(), BytesOf("new"));
    const auto pending = f.manager.Load(f.id, f.ReloadAsync());
    REQUIRE(pending);
    CHECK(pending.value().generation() != original.generation());
    CHECK(f.manager.GetState(pending.value()) == AssetState::Loading);
    CHECK(f.manager.GetState(original) == AssetState::Ready);
    REQUIRE(f.manager.GetShared<Loaders::TextAsset>(original));
    CHECK(f.manager.GetShared<Loaders::TextAsset>(original)->text == "old");
    const auto merged = f.manager.Load(f.id, f.ReloadAsync());
    REQUIRE(merged);
    CHECK(merged.value() == pending.value());
    CHECK(f.source.reads == 1);
    CHECK(f.storage.Find(f.id)->refCount == 3);
    f.manager.Update();
    CHECK(f.source.reads == 2);
    CHECK(f.manager.GetState(pending.value()) == AssetState::Ready);
    REQUIRE(f.manager.GetShared<Loaders::TextAsset>(pending.value()));
    CHECK(f.manager.GetShared<Loaders::TextAsset>(pending.value())->text == "new");
    CHECK(f.manager.GetState(original) == AssetState::Unloaded);
    f.manager.Release(original);
    f.manager.Release(pending.value());
    f.manager.Release(merged.value());
    CHECK(f.storage.Find(f.id)->refCount == 0);
    CHECK(f.manager.EvictIfPossible(f.id));
}

TEST_CASE("AssetManager: failed candidate preserves old data without reusing its generation") {
    ManagerFixture f;
    const auto original = f.Initial();
    auto request = f.ReloadAsync();
    request.overridePath = "missing";
    const auto failed = f.manager.Load(f.id, request);
    REQUIRE(failed);
    f.manager.Update();
    CHECK(f.manager.GetState(failed.value()) == AssetState::Failed);
    REQUIRE(f.manager.GetError(failed.value()));
    CHECK(f.manager.GetState(original) == AssetState::Ready);
    CHECK(f.manager.GetShared<Loaders::TextAsset>(failed.value()) == nullptr);
    REQUIRE(f.manager.GetShared<Loaders::TextAsset>(original));
    CHECK(f.manager.GetShared<Loaders::TextAsset>(original)->text == "old");
    const auto next = f.manager.Load(f.id, f.ReloadAsync());
    REQUIRE(next);
    CHECK(next.value().generation() != failed.value().generation());
    CHECK(f.manager.GetState(failed.value()) == AssetState::Failed);
    REQUIRE(f.manager.GetError(failed.value()));
    f.manager.Update();
    CHECK(f.manager.GetState(next.value()) == AssetState::Ready);
    CHECK(f.manager.GetState(failed.value()) == AssetState::Failed);
    REQUIRE(f.manager.GetError(failed.value()));
    CHECK(f.manager.GetShared<Loaders::TextAsset>(failed.value()) == nullptr);
    f.manager.Release(original);
    f.manager.Release(failed.value());
    f.manager.Release(next.value());
    CHECK(f.manager.EvictIfPossible(f.id));
    const auto afterEviction = f.Initial();
    CHECK(afterEviction.generation() != failed.value().generation());
    CHECK(afterEviction.generation() != next.value().generation());
}

TEST_CASE("AssetManager: destructive async failure invalidates old handles and reports candidate error") {
    ManagerFixture f;
    const auto original = f.Initial();
    auto request = f.ReloadAsync();
    request.overridePath = "missing";
    request.fallback = AssetRequest::Fallback::None;
    const auto pending = f.manager.Load(f.id, request);
    REQUIRE(pending);
    f.manager.Update();
    CHECK(f.manager.GetState(pending.value()) == AssetState::Failed);
    REQUIRE(f.manager.GetError(pending.value()));
    CHECK(f.manager.GetState(original) == AssetState::Unloaded);
    const auto recovered = f.Initial();
    CHECK(recovered.generation() != pending.value().generation());
    CHECK(f.manager.GetShared<Loaders::TextAsset>(original) == nullptr);
    CHECK(f.manager.GetShared<Loaders::TextAsset>(pending.value()) == nullptr);
}

TEST_CASE("AssetManager: sync promotes the same queued operation without a second read") {
    ManagerFixture f;
    SUBCASE("initial load") {
        const auto pending = f.manager.Load(f.id, AssetRequest::AsyncLoad());
        REQUIRE(pending);
        const auto sync = f.manager.Load(f.id, AssetRequest::Default());
        REQUIRE(sync);
        CHECK(sync.value() == pending.value());
        CHECK(f.source.reads == 1);
        f.manager.Update();
        CHECK(f.source.reads == 1);
    }
    SUBCASE("reload") {
        f.Initial();
        const auto pending = f.manager.Load(f.id, f.ReloadAsync());
        REQUIRE(pending);
        const auto sync = f.manager.Load(f.id, AssetRequest::Reload());
        REQUIRE(sync);
        CHECK(sync.value() == pending.value());
        CHECK(f.source.reads == 2);
        f.manager.Update();
        CHECK(f.source.reads == 2);
    }
}

TEST_CASE("AssetManager: conflicting requests leave the queued operation and policy untouched") {
    ManagerFixture f;
    const auto pending = f.manager.Load(f.id, AssetRequest::AsyncLoad());
    REQUIRE(pending);
    auto conflict = AssetRequest::Default();
    SUBCASE("path") { conflict.overridePath = "other"; }
    SUBCASE("fallback") { conflict.fallback = AssetRequest::Fallback::None; }
    SUBCASE("loader tag") { conflict.tag = "different decode options"; }
    SUBCASE("async path") {
        conflict.sync = AssetRequest::SyncWith::Async;
        conflict.overridePath = "other";
    }
    conflict.pin = true;
    const auto rejected = f.manager.Load(f.id, conflict);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code == AssetErrorCode::RequestInProgress);
    CHECK_FALSE(f.lifetime.IsPinned(f.id));
    CHECK(f.storage.Find(f.id)->refCount == 1);
    CHECK(f.source.reads == 0);
    f.manager.Release(pending.value());
    CHECK_FALSE(f.manager.EvictIfPossible(f.id));
    f.manager.Update();
    CHECK(f.source.reads == 1);
    CHECK(f.manager.EvictIfPossible(f.id));
}

TEST_CASE("AssetManager: unsupported hints fail before IO references or pinning") {
    ManagerFixture f;
    SUBCASE("fresh priority") {
        auto request = AssetRequest::AsyncLoad(5);
        request.pin = true;
        const auto rejected = f.manager.Load(f.id, request);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().code == AssetErrorCode::UnsupportedRequest);
        CHECK(f.storage.Size() == 0);
    }
    SUBCASE("cached TTL") {
        f.Initial();
        auto request = AssetRequest::Default();
        request.keepAliveFramesOverride = 30;
        request.pin = true;
        const auto rejected = f.manager.Load(f.id, request);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().code == AssetErrorCode::UnsupportedRequest);
        CHECK(f.storage.Find(f.id)->refCount == 1);
        CHECK(f.source.reads == 1);
    }
    SUBCASE("queued priority") {
        const auto pending = f.manager.Load(f.id, AssetRequest::AsyncLoad());
        REQUIRE(pending);
        auto request = AssetRequest::AsyncLoad(1);
        request.pin = true;
        const auto rejected = f.manager.Load(f.id, request);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().code == AssetErrorCode::UnsupportedRequest);
        CHECK(f.storage.Find(f.id)->refCount == 1);
        CHECK(f.manager.GetState(pending.value()) == AssetState::Loading);
        CHECK(f.source.reads == 0);
    }
    CHECK_FALSE(f.lifetime.IsPinned(f.id));
    f.manager.Update();
    CHECK(f.source.reads <= 1);
}

TEST_CASE("AssetManager: cache hit applies pin and Auto keeps the published asset during reload") {
    ManagerFixture f;
    const auto original = f.Initial();
    const auto pending = f.manager.Load(f.id, f.ReloadAsync());
    REQUIRE(pending);
    auto request = AssetRequest::Default();
    request.pin = true;
    const auto cached = f.manager.Load(f.id, request);
    REQUIRE(cached);
    CHECK(cached.value() == original);
    CHECK(f.lifetime.IsPinned(f.id));
    f.manager.Update();
    f.manager.Release(original);
    f.manager.Release(cached.value());
    f.manager.Release(pending.value());
    CHECK_FALSE(f.manager.EvictIfPossible(f.id));
    f.lifetime.Unpin(f.id);
    CHECK(f.manager.EvictIfPossible(f.id));
}

TEST_CASE("AssetManager: watcher reserves and merges without owning a handle reference") {
    ManagerFixture f;
    const auto original = f.Initial();
    f.manager.Release(original);
    auto options = f.manager.GetOptions();
    options.enableHotReload = true;
    options.maxLoadsPerFrame = 0;
    f.manager.SetOptions(options);
    f.manager.Watch(f.id, f.Path());
    { std::ofstream file(f.Path()); file << "changed"; }
    f.source.Put(f.Path(), BytesOf("changed"));
    f.manager.Update();
    CHECK(f.storage.Find(f.id)->refCount == 0);
    CHECK_FALSE(f.manager.EvictIfPossible(f.id));
    CHECK(f.source.reads == 1);
    const auto pending = f.manager.Load(f.id, f.ReloadAsync());
    REQUIRE(pending);
    CHECK(f.manager.GetState(pending.value()) == AssetState::Loading);
    options.maxLoadsPerFrame = 1;
    f.manager.SetOptions(options);
    f.manager.Update();
    CHECK(f.source.reads == 2);
    CHECK(f.manager.GetState(original) == AssetState::Unloaded);
    REQUIRE(f.manager.GetShared<Loaders::TextAsset>(pending.value()));
    CHECK(f.manager.GetShared<Loaders::TextAsset>(pending.value())->text == "changed");
    f.manager.Release(pending.value());
    CHECK(f.storage.Find(f.id)->refCount == 0);
    CHECK(f.manager.EvictIfPossible(f.id));
}

TEST_CASE("AssetManager: failed reserved generations survive eviction and storage clear") {
    ManagerFixture f;
    const auto original = f.Initial();
    auto request = f.ReloadAsync();
    request.overridePath = "missing";
    const auto failed = f.manager.Load(f.id, request);
    REQUIRE(failed);
    f.manager.Update();
    REQUIRE(f.manager.GetState(failed.value()) == AssetState::Failed);
    f.manager.Release(original);
    f.manager.Release(failed.value());
    SUBCASE("eviction") { CHECK(f.manager.EvictIfPossible(f.id)); }
    SUBCASE("explicit storage clear") { f.storage.Clear(); }
    const auto fresh = f.Initial();
    CHECK(fresh.generation() != original.generation());
    CHECK(fresh.generation() != failed.value().generation());
    CHECK(f.manager.GetShared<Loaders::TextAsset>(failed.value()) == nullptr);
}

TEST_CASE("AssetManager: queued input is resolved once and failed promotion preserves both outcomes") {
    ManagerFixture f;
    SUBCASE("queued catalog snapshot") {
        const auto pending = f.manager.Load(f.id, AssetRequest::AsyncLoad());
        REQUIRE(pending);
        f.catalog.Clear();
        f.manager.Update();
        CHECK(f.manager.GetState(pending.value()) == AssetState::Ready);
        CHECK(f.source.reads == 1);
    }
    SUBCASE("sync KeepOldIfAny promotion") {
        const auto original = f.Initial();
        auto request = f.ReloadAsync();
        request.overridePath = "missing";
        const auto pending = f.manager.Load(f.id, request);
        REQUIRE(pending);
        request.sync = AssetRequest::SyncWith::Sync;
        const auto sync = f.manager.Load(f.id, request);
        REQUIRE(sync);
        CHECK(sync.value() == original);
        CHECK(f.manager.GetState(pending.value()) == AssetState::Failed);
        CHECK(f.manager.GetState(original) == AssetState::Ready);
        CHECK(f.storage.Find(f.id)->refCount == 3);
        f.manager.Update();
        CHECK(f.source.reads == 2);
        f.manager.Release(original);
        f.manager.Release(pending.value());
        f.manager.Release(sync.value());
        CHECK(f.manager.EvictIfPossible(f.id));
    }
}

TEST_CASE("AssetManager: storage clear retires queued handles before the next load") {
    ManagerFixture f;
    const auto retired = f.manager.Load(f.id, AssetRequest::AsyncLoad());
    REQUIRE(retired);
    f.storage.Clear();
    const auto current = f.manager.Load(f.id, AssetRequest::AsyncLoad());
    REQUIRE(current);
    CHECK(current.value().generation() != retired.value().generation());
    f.manager.Update();
    CHECK(f.source.reads == 1);
    CHECK(f.manager.GetState(retired.value()) == AssetState::Unloaded);
    CHECK(f.manager.GetState(current.value()) == AssetState::Ready);
    f.manager.Release(retired.value());
    CHECK(f.storage.Find(f.id)->refCount == 1);
}

TEST_CASE("AssetManager: only the most recent failure is retained beside a published asset") {
    ManagerFixture f;
    const auto original = f.Initial();
    auto request = f.ReloadAsync();
    request.overridePath = "first missing";
    const auto first = f.manager.Load(f.id, request);
    REQUIRE(first);
    f.manager.Update();
    request.overridePath = "second missing";
    const auto second = f.manager.Load(f.id, request);
    REQUIRE(second);
    CHECK(f.manager.GetState(first.value()) == AssetState::Failed);
    f.manager.Update();
    CHECK(f.manager.GetState(first.value()) == AssetState::Unloaded);
    CHECK(f.manager.GetError(first.value()) == nullptr);
    CHECK(f.manager.GetState(second.value()) == AssetState::Failed);
    REQUIRE(f.manager.GetError(second.value()));
    CHECK(f.manager.GetState(original) == AssetState::Ready);
    f.manager.Release(original);
    f.manager.Release(first.value());
    f.manager.Release(second.value());
    CHECK(f.storage.Find(f.id)->refCount == 0);
}

TEST_CASE("AssetManager: generation exhaustion rejects new loads without wrapping or side effects") {
    ManagerFixture f;
    const auto original = f.Initial();
    auto* record = f.storage.Find(f.id);
    REQUIRE(record);
    record->lastIssuedGeneration = (std::numeric_limits<std::uint32_t>::max)();
    auto request = f.ReloadAsync();
    request.pin = true;
    const auto rejected = f.manager.Load(f.id, request);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code == AssetErrorCode::GenerationExhausted);
    CHECK(record->refCount == 1);
    CHECK_FALSE(record->IsLoading());
    CHECK_FALSE(f.lifetime.IsPinned(f.id));
    CHECK(f.manager.GetState(original) == AssetState::Ready);
    f.manager.Update();
    CHECK(f.source.reads == 1);
    const auto cached = f.manager.Load(f.id, AssetRequest::Default());
    REQUIRE(cached);
    CHECK(cached.value() == original);
    f.manager.Release(original);
    f.manager.Release(cached.value());
    CHECK(f.manager.EvictIfPossible(f.id));
    const auto afterEviction = f.manager.Load(f.id, AssetRequest::Default());
    REQUIRE_FALSE(afterEviction);
    CHECK(afterEviction.error().code == AssetErrorCode::GenerationExhausted);
    CHECK(f.storage.Size() == 0);
    CHECK(f.source.reads == 1);
}

TEST_CASE("AssetManager: final available generation still supports queued joins and sync promotion") {
    ManagerFixture f;
    f.Initial();
    f.storage.Find(f.id)->lastIssuedGeneration = (std::numeric_limits<std::uint32_t>::max)() - 1;
    const auto pending = f.manager.Load(f.id, f.ReloadAsync());
    REQUIRE(pending);
    CHECK(pending.value().generation() == (std::numeric_limits<std::uint32_t>::max)());
    const auto merged = f.manager.Load(f.id, f.ReloadAsync());
    REQUIRE(merged);
    CHECK(merged.value() == pending.value());
    const auto sync = f.manager.Load(f.id, AssetRequest::Reload());
    REQUIRE(sync);
    CHECK(sync.value() == pending.value());
    f.manager.Update();
    CHECK(f.source.reads == 2);
}
