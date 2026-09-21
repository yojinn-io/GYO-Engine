#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <thread>

#include "engine/asset/hot_reload/AssetWatcher.hpp"
#include "engine/asset/AssetId.hpp"

namespace fs = std::filesystem;
using Engine::Asset::HotReload::AssetWatcher;
using Engine::Asset::HotReload::AssetChangeKind;
using Engine::Asset::AssetId;

static void WriteFile(const fs::path& p, const std::string& s) {
    fs::create_directories(p.parent_path());
    std::ofstream ofs(p.string(), std::ios::binary);
    ofs << s;
    ofs.flush();
}

TEST_CASE("AssetWatcher: modified") {
    fs::path tmp = fs::temp_directory_path() / "asset_watcher_test";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    fs::path f = tmp / "a.txt";
    WriteFile(f, "1");

    AssetWatcher::Options opt;
    opt.debounceMs = 0;
    AssetWatcher w(opt);

    AssetId id = AssetId::FromString("a");
    w.Watch(id, f.string());

    (void)w.Poll(); // 初回は変化なし想定

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    WriteFile(f, "2");

    auto ch = w.Poll();
    REQUIRE(!ch.empty());
    CHECK(ch[0].kind == AssetChangeKind::Modified);
    CHECK(ch[0].id == id);
}

TEST_CASE("AssetWatcher: removed and keepWatchingMissing=false") {
    fs::path tmp = fs::temp_directory_path() / "asset_watcher_test2";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    fs::path f = tmp / "b.txt";
    WriteFile(f, "x");

    AssetWatcher::Options opt;
    opt.keepWatchingMissing = false;
    opt.debounceMs = 0;
    AssetWatcher w(opt);

    AssetId id = AssetId::FromString("b");
    w.Watch(id, f.string());
    (void)w.Poll();

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    fs::remove(f);

    auto ch = w.Poll();
    REQUIRE(!ch.empty());
    CHECK(ch[0].kind == AssetChangeKind::Removed);
    CHECK(w.IsWatching(id) == false);
}
