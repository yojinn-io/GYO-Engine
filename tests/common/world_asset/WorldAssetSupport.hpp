#pragma once

// Support for the tests of world_asset (declaration docs/architecture/target/v2/asset-declaration.zh-Hant.md,
// section 10; step 3 of section 9): content written with its content.json and catalog, Products built from the
// register rows by hand, the author's Resources and Systems the tests use. The executors and test loaders are the
// store's (tests/common/asset/StoreSupport.hpp).

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "StoreSupport.hpp"
#include "engine/asset/Catalog.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/world_asset/AssetRequests.hpp"
#include "engine/world_asset/AssetView.hpp"
#include "engine/world_asset/WorldAssetModule.hpp"

namespace WorldAssetTest {

namespace Asset = GYO::Engine::Asset;
namespace Base = GYO::Engine::Base;
namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace WA = GYO::Engine::WorldAsset;
using Text = Asset::Text;
using TextHandle = Asset::Handle<Text>;

// ---- Content ----

struct EntrySpec {
    std::string id;
    std::string type;
    std::string path;
    std::optional<std::string> bytes;
    std::optional<std::string> format;
};

// A content root under the test binary directory, emptied first: the files, content.json and asset_catalog.json
// with the entries in the order given.
class Content final {
public:
    explicit Content(const std::string& name) : root_(std::string(GYO_ASSET_TEST_BINARY_DIR) + "/content/" + name) {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_);
    }

    // An entry; with bytes, its file too.
    Content& Add(std::string id, std::string type, std::string path, std::optional<std::string> bytes,
                 std::optional<std::string> format = std::nullopt) {
        specs_.push_back(EntrySpec{std::move(id), std::move(type), std::move(path), std::move(bytes), std::move(format)});
        return *this;
    }

    // Writes every file and the catalog, its entries in the order given or reversed.
    const Content& Write(const bool reversed = false) const {
        Asset::Catalog catalog;
        for (const EntrySpec& spec : specs_) {
            if (spec.bytes) WriteFile(spec.path, *spec.bytes);
            Asset::CatalogEntry entry;
            entry.id = Asset::AssetId(spec.id);
            entry.type = spec.type;
            entry.path = spec.path;
            entry.format = spec.format;
            catalog.assets.push_back(std::move(entry));
        }
        if (reversed) std::reverse(catalog.assets.begin(), catalog.assets.end());
        auto written = Asset::WriteCatalog(catalog);
        REQUIRE(static_cast<bool>(written));
        WriteFile("asset_catalog.json", *written);
        Asset::ContentManifest manifest;
        manifest.catalogs = {"asset_catalog.json"};
        auto manifestText = Asset::WriteContentManifest(manifest);
        REQUIRE(static_cast<bool>(manifestText));
        WriteFile("content.json", *manifestText);
        return *this;
    }

    void WriteFile(const std::string& path, const std::string& bytes) const {
        const std::filesystem::path file = std::filesystem::path(root_) / path;
        std::filesystem::create_directories(file.parent_path());
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

    [[nodiscard]] const std::string& Root() const noexcept { return root_; }

private:
    std::string root_;
    std::vector<EntrySpec> specs_;
};

// ---- The author's Resources ----

// Handles a World holds (the author's own Resource; nothing world_asset knows).
struct Held {
    std::vector<TextHandle> handles;
};
inline void HashValue(K::Hasher& h, const Held& value) { HashValue(h, value.handles); }

// Handles in a Resource the author marks to carry over (O-10, 5.4).
struct Carried {
    std::vector<TextHandle> handles;
};
inline void HashValue(K::Hasher& h, const Carried& value) { HashValue(h, value.handles); }

// What the test asks IssueOrders to do on the next frame.
struct Orders {
    std::vector<std::string> load;
    std::vector<std::string> carry;
    bool dropHeld = false;
};
inline void HashValue(K::Hasher& h, const Orders& value) {
    HashValue(h, value.load);
    HashValue(h, value.carry);
    HashValue(h, value.dropHeld);
}

// The ids a save records; LoadSave (the beforeSwap schedule) requests them in the new World and notes what it saw.
struct SaveSeen {
    std::string id;
    Asset::AssetState state = Asset::AssetState::NotRequested;
    std::string value;
};
struct SaveFile {
    std::vector<std::string> ids;
    std::vector<SaveSeen> seen;
};
inline SaveFile& TheSave() {
    static SaveFile save;
    return save;
}

// ---- The author's Systems ----

// Main/Update: drop, request, carry, as Orders says; then Orders is empty.
inline K::SystemResult IssueOrders(K::Res<WA::AssetRequests> requests, K::ResMut<Orders> orders, K::ResMut<Held> held,
                                   K::ResMut<Carried> carried) {
    if (orders->dropHeld) held->handles.clear();
    for (const std::string& id : orders->load) held->handles.push_back(requests->Load<Text>(Asset::AssetId(id)));
    for (const std::string& id : orders->carry) carried->handles.push_back(requests->Load<Text>(Asset::AssetId(id)));
    *orders = Orders{};
    return {};
}

// The beforeSwap schedule of a save being loaded (5.14 step 2, section 10 steps 4, 5, 7).
inline K::SystemResult LoadSave(K::Res<WA::AssetRequests> requests, K::Res<WA::AssetView> view, K::ResMut<Held> held) {
    for (const std::string& id : TheSave().ids) {
        TextHandle handle = requests->Load<Text>(Asset::AssetId(id));
        SaveSeen seen;
        seen.id = id;
        seen.state = view->State(handle);
        if (const Text* text = view->Get(handle)) seen.value = text->Bytes();
        TheSave().seen.push_back(std::move(seen));
        held->handles.push_back(std::move(handle));
    }
    return {};
}

inline K::SystemOptions At(std::string name, std::string schedule, std::string stage,
                           std::vector<std::string> sets = {}) {
    K::SystemOptions options;
    options.name = std::move(name);
    K::Mount mount;
    mount.schedule = std::move(schedule);
    mount.stage = std::move(stage);
    mount.sets = std::move(sets);
    options.mounts.push_back(std::move(mount));
    return options;
}
inline K::SystemOptions InMain(std::string name, std::string_view stage) {
    return At(std::move(name), std::string(O::DefaultSchedules::frame), std::string(stage));
}

inline K::ScheduleOptions Once(std::string name) {
    K::ScheduleOptions schedule;
    schedule.name = std::move(name);
    schedule.stages = {"S"};
    return schedule;
}

// ---- Units ----

// What DiscardWatchModule saw when the Product was destroyed, after world_asset's OnDestroy.
struct DiscardRecord {
    bool ran = false;
    Asset::AssetStoreCounts counts;
};
inline DiscardRecord& TheDiscardRecord() {
    static DiscardRecord record;
    return record;
}

// A unit before world_asset: its OnDestroy runs after world_asset's (reverse order), with the Product's buffer
// still there, so it reads the store's counts after Shutdown (5.8, O-A4).
class DiscardWatchModule final : public O::Module {
public:
    void Configure(O::ProductSetup& setup) override {
        O::ModuleBuffer* buffer = &setup.Buffer();
        setup.OnDestroy([buffer] {
            const Asset::AssetStore* store = std::as_const(*buffer).Find<Asset::AssetStore>();
            TheDiscardRecord().ran = store != nullptr;
            if (store != nullptr) TheDiscardRecord().counts = store->Counts();
        });
    }
    void Build(O::WorldSetup&) override {}
};

// A unit after world_asset that owns a type: it registers its loaders through the registry it Finds in the
// Product's buffer, in its own Configure (O-A2, 5.16).
class LinkedTypeModule final : public O::Module {
public:
    void Configure(O::ProductSetup& setup) override {
        const Asset::AssetLoaderRegistry* registry = std::as_const(setup.Buffer()).Find<Asset::AssetLoaderRegistry>();
        if (registry == nullptr) {
            setup.Report("LT-E1", "no AssetLoaderRegistry in the Product's buffer");
            return;
        }
        if (!registry->Register<AssetTest::LinkedLoader>()) setup.Report("LT-E2", "registering LinkedLoader failed");
        if (!registry->Preregister<AssetTest::PngLoader>({"png"})) setup.Report("LT-E3", "preregistering failed");
    }
    void Build(O::WorldSetup&) override {}
};

inline O::RegisterRow Row(std::string name, bool libraryOnly, std::vector<std::string> depends = {}) {
    O::RegisterRow row;
    row.name = std::move(name);
    row.kind = O::UnitKind::Module;
    row.libraryOnly = libraryOnly;
    row.defaultOn = !libraryOnly;
    row.depends = std::move(depends);
    return row;
}

struct UnitChoice {
    bool discardWatch = false;
    bool linkedType = false;
};

inline O::RegisteredUnits Units(const UnitChoice choice = {}) {
    O::RegisteredUnits units;
    units.rows.push_back(Row("asset", true));
    if (choice.discardWatch) {
        units.rows.push_back(Row("discard_watch", false));
        units.modules.push_back({"discard_watch", std::make_unique<DiscardWatchModule>()});
    }
    units.rows.push_back(Row("world_asset", false, {"asset"}));
    units.modules.push_back({"world_asset", std::make_unique<WA::WorldAssetModule>()});
    if (choice.linkedType) {
        units.rows.push_back(Row("linked_type", false, {"world_asset"}));
        units.modules.push_back({"linked_type", std::make_unique<LinkedTypeModule>()});
    }
    return units;
}

// A World with the author's Resources (Held, Carried marked to carry over, Orders), IssueOrders in Main/Update,
// the beforeSwap schedule LoadSave, and what the test adds.
inline O::WorldDescription AuthorWorld(std::string name, std::function<void(O::WorldSetup&)> more = {}) {
    O::WorldDescription world;
    world.name = std::move(name);
    world.build = [more](O::WorldSetup& s) {
        K::Registrar& r = s.Registrar();
        r.Resource<Held>({.name = "Held"});
        r.Resource<Carried>({.name = "Carried"});
        r.Resource<Orders>({.name = "Orders"});
        s.CarryOver<Carried>("Carried");
        r.AddSystem<&IssueOrders>(InMain("IssueOrders", O::DefaultSchedules::update));
        r.Schedule(Once("LoadSave"));
        r.AddSystem<&LoadSave>(At("LoadSave", "LoadSave", "S"));
        if (more) more(s);
    };
    world.prepare.push_back([](K::World& w) -> Base::Result<void, O::OrchestrationError> {
        if (w.Resource<Held>() == nullptr) w.SetResource(Held{});
        // Carried over from the old World when it is replaced (O-10): kept as it came.
        if (w.Resource<Carried>() == nullptr) w.SetResource(Carried{});
        w.SetResource(Orders{});
        return {};
    });
    return world;
}

inline std::unique_ptr<O::Product> Build(O::ProductDescription description, O::RegisteredUnits units) {
    auto built = O::Product::Build(std::move(description), std::move(units));
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    return std::move(*built);
}

inline void Frame(O::Product& product) {
    auto ran = product.RunFrame();
    REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
}

inline void Schedule(O::Product& product, std::string_view world, std::string_view schedule) {
    auto ran = product.RunSchedule(world, schedule);
    REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
}

inline K::World& WorldOf(O::Product& product, std::string_view name) { return product.World(name).GetWorld(); }

inline const WA::AssetView& ViewOf(O::Product& product, std::string_view name) {
    const WA::AssetView* view = std::as_const(WorldOf(product, name)).Resource<WA::AssetView>();
    REQUIRE(view != nullptr);
    return *view;
}
inline const WA::AssetRequests& RequestsOf(O::Product& product, std::string_view name) {
    const WA::AssetRequests* requests = std::as_const(WorldOf(product, name)).Resource<WA::AssetRequests>();
    REQUIRE(requests != nullptr);
    return *requests;
}
inline Held& HeldOf(O::Product& product, std::string_view name) {
    Held* held = WorldOf(product, name).ResourceMut<Held>();
    REQUIRE(held != nullptr);
    return *held;
}
inline Orders& OrdersOf(O::Product& product, std::string_view name) {
    Orders* orders = WorldOf(product, name).ResourceMut<Orders>();
    REQUIRE(orders != nullptr);
    return *orders;
}
inline const Asset::AssetStore& StoreOf(O::Product& product) {
    const Asset::AssetStore* store = std::as_const(product.Buffer()).Find<Asset::AssetStore>();
    REQUIRE(store != nullptr);
    return *store;
}

inline std::vector<WA::UntypedHandle> Group(const std::vector<TextHandle>& handles, std::size_t from = 0,
                                            std::size_t count = static_cast<std::size_t>(-1)) {
    std::vector<WA::UntypedHandle> group;
    for (std::size_t index = from; index < handles.size() && index - from < count; ++index) group.emplace_back(handles[index]);
    return group;
}

inline std::string Ids(const std::vector<Asset::AssetId>& ids) {
    std::string text;
    for (const Asset::AssetId& id : ids) text += (text.empty() ? "" : ",") + id.Text();
    return text;
}

// Resets the shared test state at the start and end of a test.
struct Fresh {
    Fresh() { Clear(); }
    ~Fresh() { Clear(); }
    static void Clear() {
        AssetTest::Fresh::Clear();
        TheSave() = SaveFile{};
        TheDiscardRecord() = DiscardRecord{};
    }
};

} // namespace WorldAssetTest
