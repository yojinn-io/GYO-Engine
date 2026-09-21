#include "doctest/doctest.h"

#include <filesystem>
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
        void Put(const std::string& path, std::vector<std::byte> bytes) {
            map_[path] = std::move(bytes);
        }

        Engine::Base::Result<std::vector<std::byte>, Engine::Base::Error<AssetErrorCode>>
        ReadAll(std::string_view resolvedPath) override {
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
          "assets": [
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
          "assets": [
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
