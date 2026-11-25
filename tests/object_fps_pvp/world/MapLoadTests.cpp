// The map a World plays, read while the World generates (world/ArenaLoad.hpp; big-map declaration 15.6): the World
// holds its generation until every region is read and its walls are built (and no step runs before), the progress
// never goes back, a map that does not read fails with its reason and keeps the World generating, the connections
// that come meanwhile wait, and the Match keeps none of the map once its walls are built.
#include <doctest/doctest.h>

#include "WorldTestSupport.hpp"

#include "engine/asset/AssetStore.hpp"
#include "engine/physics_3d/Collider.hpp"

#include <algorithm>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>

using namespace object_fps_pvp::test;
namespace AS = GYO::Engine::Asset;
namespace WA = GYO::Engine::WorldAsset;
namespace fs = std::filesystem;

namespace {

// Jobs that wait until the test runs them, one at a time.
class HeldJobs final : public AS::AssetExecutor {
public:
    void Submit(Job job) override {
        const std::lock_guard lock(mutex_);
        jobs_.push_back(std::move(job));
    }
    bool RunOne() {
        Job job;
        {
            const std::lock_guard lock(mutex_);
            if (jobs_.empty()) return false;
            job = std::move(jobs_.front());
            jobs_.pop_front();
        }
        job();
        return true;
    }
    std::size_t Waiting() {
        const std::lock_guard lock(mutex_);
        return jobs_.size();
    }

private:
    std::mutex mutex_;
    std::deque<Job> jobs_;
};

std::size_t ArenaIndex(const std::string& id) {
    const auto* arena = ShippedContent()->Find(id);
    REQUIRE(arena != nullptr);
    return static_cast<std::size_t>(arena - ShippedContent()->arenas.data());
}

// A Local World on the arena, reading the content under root with the asset jobs on assets and the walls built on
// walls (none: in the System). Built, not generated.
std::unique_ptr<O::Product> LocalOn(const std::size_t arena, AS::AssetExecutor& assets, AS::AssetExecutor* walls,
                                    const std::string& root = PVP_CONTENT_ROOT,
                                    std::shared_ptr<const PW::GameContent> content = ShippedContent()) {
    O::ProductDescription description;
    description.name = "pvp-solo";
    WA::AssetOptions options;
    options.contentRoot = root;
    options.executor = &assets;
    description.options.Set(options);
    description.configure = [](O::ProductSetup& setup) { PW::ConfigureGame(setup); };
    description.worlds.push_back(Stepped(PW::LocalWorld({.content = std::move(content), .arena = arena, .localUser = 9, .jobs = walls})));
    auto built = O::Product::Build(std::move(description), O::MakeRegisteredUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    return std::move(built).value();
}

// A directory with the shipped content's catalog and one map's files (the catalog's other files are not read).
struct MapCopy {
    fs::path root;
    explicit MapCopy(const std::string& map) {
        std::random_device random;
        root = fs::temp_directory_path() / ("gyo_pvp_map_" + std::to_string(random()));
        const fs::path from = PVP_CONTENT_ROOT;
        fs::create_directories(root / "maps");
        fs::copy_file(from / "content.json", root / "content.json");
        fs::copy_file(from / "asset_catalog.json", root / "asset_catalog.json");
        fs::copy(from / "maps" / map, root / "maps" / map, fs::copy_options::recursive);
    }
    ~MapCopy() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
    fs::path Region(const std::string& map, const std::size_t index) const {
        std::vector<fs::path> regions;
        for (const auto& entry : fs::directory_iterator(root / "maps" / map / "regions")) regions.push_back(entry.path());
        std::sort(regions.begin(), regions.end());
        REQUIRE(index < regions.size());
        return regions[index];
    }
};

const PW::MapLoad& LoadOf(O::Product& product) { return *GameWorld(product).Resource<PW::MapLoad>(); }

// Runs frames until the map is done or failed (the jobs run at once), at most a few.
void UntilSettled(O::Product& product) {
    for (int frame = 0; frame < 32; ++frame) {
        const PW::MapStage stage = LoadOf(product).stage;
        if (stage == PW::MapStage::Done || stage == PW::MapStage::Failed) return;
        REQUIRE(static_cast<bool>(product.RunFrame()));
    }
}

} // namespace

TEST_CASE("world: a World generates until its map's regions are read and its walls built, and ends in the frame they are") {
    HeldJobs assets;
    HeldJobs walls;
    const std::size_t arena = ArenaIndex("kiriyu_town_graybox");
    auto product = LocalOn(arena, assets, &walls);
    K::World& world = GameWorld(*product);
    std::uint32_t done = 0;
    bool monotonic = true;
    bool stepped = false;
    // The index and its regions, one job a frame: generating all along, no step taken.
    for (int frame = 0; frame < 64 && LoadOf(*product).stage == PW::MapStage::Reading; ++frame) {
        REQUIRE(static_cast<bool>(product->RunFrame()));
        CHECK_MESSAGE(product->Generating("Game"), "[pvp-map-held] generating while the map is read");
        monotonic = monotonic && LoadOf(*product).done >= done;
        done = LoadOf(*product).done;
        stepped = stepped || TickOf(world) != 0;
        (void)assets.RunOne();
    }
    CHECK_MESSAGE(monotonic, "[pvp-map-progress] the regions read never go back");
    CHECK_MESSAGE(!stepped, "[pvp-steps-wait] no step before the World has generated");
    REQUIRE(LoadOf(*product).stage == PW::MapStage::Building);
    CHECK(LoadOf(*product).total == 4);
    CHECK(LoadOf(*product).done == 4);
    // The walls wait on the worker: still generating.
    REQUIRE(walls.Waiting() == 1);
    REQUIRE(static_cast<bool>(product->RunFrame()));
    REQUIRE(static_cast<bool>(product->RunFrame()));
    CHECK_MESSAGE(product->Generating("Game"), "[pvp-walls-held] generating while the walls are built");
    CHECK(world.Get<GYO::Engine::Physics3d::Collider>(PW::kArenaEntity) == nullptr);
    // Built: the frame that takes them ends the generation.
    REQUIRE(walls.RunOne());
    REQUIRE(static_cast<bool>(product->RunFrame()));
    CHECK_MESSAGE(!product->Generating("Game"), "[pvp-map-ends] generated in the frame the walls came");
    CHECK(LoadOf(*product).stage == PW::MapStage::Done);
    REQUIRE(world.Get<GYO::Engine::Physics3d::Collider>(PW::kArenaEntity) != nullptr);
    const std::uint64_t digest = world.Resource<PW::GameRules>()->wallsDigest;
    CHECK(digest != 0);
    CHECK(world.Resource<PW::MatchInfo>()->arenaId == "kiriyu_town_graybox");
    CHECK(world.Resource<PW::MatchInfo>()->wallsDigest == digest);
    // Then the game is played: the steps run and the player is there.
    REQUIRE(static_cast<bool>(product->RunFrame()));
    REQUIRE(static_cast<bool>(product->RunFrame()));
    CHECK(TickOf(world) > 0);
    CHECK(PlayerOf(world, 9) != 0);
}

TEST_CASE("world: a map with a region missing or not the index's fails with its reason, and the World keeps generating") {
    {
        MapCopy copy("pvp_corners");
        fs::remove(copy.Region("pvp_corners", 0));
        auto product = LocalOn(ArenaIndex("pvp_corners_v1"), TestJobs(), nullptr, copy.root.string());
        UntilSettled(*product);
        const PW::MapLoad& load = LoadOf(*product);
        CHECK_MESSAGE(load.stage == PW::MapStage::Failed, "[pvp-map-failure] a missing region fails the map");
        CHECK_MESSAGE(load.failure.find("object_fps_pvp.map.pvp_corners") != std::string::npos, load.failure);
        REQUIRE(static_cast<bool>(product->RunFrame()));
        CHECK(product->Generating("Game"));
        CHECK(PlayerOf(GameWorld(*product), 9) == 0);
    }
    {
        MapCopy copy("pvp_corners");
        {
            std::ofstream out(copy.Region("pvp_corners", 0), std::ios::binary | std::ios::app);
            out << ' ';
        }
        auto product = LocalOn(ArenaIndex("pvp_corners_v1"), TestJobs(), nullptr, copy.root.string());
        UntilSettled(*product);
        const PW::MapLoad& load = LoadOf(*product);
        CHECK_MESSAGE(load.stage == PW::MapStage::Failed, "[pvp-map-failure] a region that is not the index's fails the map");
        CHECK_MESSAGE(load.failure.find("R3") != std::string::npos, load.failure);
        CHECK(product->Generating("Game"));
    }
}

TEST_CASE("world: an arena whose spawn is inside its map's walls fails, naming the spawn") {
    // pvp_arena's pillar stands over x and z in [9, 11].
    auto content = std::make_shared<PW::GameContent>(*ShippedContent());
    content->arenas[0].spawns[1].position = {10.0F, 0.0F, 10.0F};
    auto product = LocalOn(0, TestJobs(), nullptr, PVP_CONTENT_ROOT, content);
    UntilSettled(*product);
    const PW::MapLoad& load = LoadOf(*product);
    CHECK_MESSAGE(load.stage == PW::MapStage::Failed, "[pvp-spawn-in-wall] " << load.failure);
    CHECK(load.failure.find("spawn 1") != std::string::npos);
}

TEST_CASE("world: a Client that connects while the Match generates is seated once its walls are built") {
    HeldJobs assets;
    std::pair<N::MemoryLink, N::MemoryLink> link = N::MemoryLink::Pair({});
    auto clock = std::make_shared<N::NetTime>(0);
    auto reader = [clock] { return *clock; };
    O::ProductDescription description;
    description.name = "pvp-match";
    N::NetProductOptions net;
    net.role = N::NetRole::Server;
    net.link = std::make_shared<N::MemoryLink>(link.first);
    net.clock = reader;
    net.seed = 7;
    auto seats = std::make_shared<PW::Seats>();
    net.admission = PW::SeatAdmission(seats);
    description.options.Set(net);
    WA::AssetOptions options;
    options.contentRoot = PVP_CONTENT_ROOT;
    options.executor = &assets;
    description.options.Set(options);
    description.configure = [](O::ProductSetup& setup) { PW::ConfigureGame(setup); };
    description.worlds.push_back(Stepped(PW::MatchWorld({.content = ShippedContent(), .arena = 0, .seats = seats})));
    auto built = O::Product::Build(std::move(description), O::MakeRegisteredUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    auto match = std::move(built).value();
    auto client = BuildClient(ShippedContent(), std::make_shared<N::MemoryLink>(link.second), 1, reader);
    // The Match's map waits: the Client connects, and has no player yet.
    for (int frame = 0; frame < 60; ++frame) {
        REQUIRE(static_cast<bool>(match->RunFrame()));
        REQUIRE(static_cast<bool>(client->RunFrame()));
        *clock += Ms(16);
    }
    REQUIRE(match->Generating("Game"));
    CHECK(seats->Taken() == 1);
    CHECK_MESSAGE(PlayerOf(GameWorld(*match), 1) == 0, "[pvp-seat-waits] no player before the walls");
    while (assets.RunOne()) {
    }
    for (int frame = 0; frame < 60; ++frame) {
        (void)assets.RunOne();
        REQUIRE(static_cast<bool>(match->RunFrame()));
        REQUIRE(static_cast<bool>(client->RunFrame()));
        *clock += Ms(16);
    }
    REQUIRE_FALSE(match->Generating("Game"));
    CHECK_MESSAGE(PlayerOf(GameWorld(*match), 1) != 0, "[pvp-seat-waits] seated once the walls are there");
    CHECK(GameWorld(*client).Resource<PW::ContentMismatch>()->reason.empty());
}

TEST_CASE("world: the Match keeps none of its map once its walls are built") {
    auto seats = std::make_shared<PW::Seats>();
    std::pair<N::MemoryLink, N::MemoryLink> link = N::MemoryLink::Pair({});
    auto match = BuildMatch(ShippedContent(), std::make_shared<N::MemoryLink>(link.first), seats, [] { return N::NetTime{}; });
    for (int frame = 0; frame < 4; ++frame) REQUIRE(static_cast<bool>(match->RunFrame()));
    const auto* diagnostics = std::as_const(match->Buffer()).Find<AS::AssetStoreDiagnostics>();
    REQUIRE(diagnostics != nullptr);
    const AS::AssetProbe probe = diagnostics->Probe(AS::AssetId(ShippedContent()->arenas[0].map));
    CHECK_MESSAGE(probe.state != AS::AssetState::Loaded, "[pvp-map-released] the map's index is not held: " << AS::ToString(probe.state));
}
