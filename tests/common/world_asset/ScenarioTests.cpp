// The scenario test of asset (declaration docs/architecture/target/v2/asset-declaration.zh-Hant.md, section 10,
// steps 1 to 8), along the game's flow: a World generated in two batches that waits until its content is ready,
// a second World sharing it, loading a save (validate first, then BuildNewFirst), DropOldFirst with and without
// the author's carried Resource, a save whose id is gone, and the reconciliation of the counts (5.8). The jobs
// run when the test says (X-5, the store's controlled executor).
#include "WorldAssetSupport.hpp"

#include <chrono>
#include <thread>

#include "engine/threads/WorkerPool.hpp"

namespace WorldAssetTest {
namespace {

// The level's two batches (section 10 step 1): carried over, so a replaced World does not generate the level again.
struct GenPlan {
    int batch = 0;
};
inline void HashValue(K::Hasher& h, const GenPlan& value) { HashValue(h, value.batch); }

const std::vector<std::string>& FirstBatch() {
    static const std::vector<std::string> ids{"a.one", "a.two", "a.three", "a.missing", "a.big", "a.unknown"};
    return ids;
}

// PreUpdate, in the set that runs while the World generates: batch 1 requests six ids, batch 2 one more once the
// first are finished; it holds generation until every batch it requested is finished (G-1, G-2).
K::SystemResult GenerateLevel(K::Res<WA::AssetRequests> requests, K::Res<WA::AssetView> view, K::ResMut<Held> held,
                              K::ResMut<GenPlan> plan, K::ResMut<O::WorldGeneration> generation) {
    switch (plan->batch) {
    case 0:
        for (const std::string& id : FirstBatch()) held->handles.push_back(requests->Load<Text>(Asset::AssetId(id)));
        plan->batch = 1;
        generation->Hold();
        break;
    case 1:
        if (view->Ready(Group(held->handles, 0, FirstBatch().size())).allFinished) {
            held->handles.push_back(requests->Load<Text>(Asset::AssetId("a.four")));
            plan->batch = 2;
        }
        generation->Hold();
        break;
    case 2:
        if (view->Ready(Group(held->handles, FirstBatch().size(), 1)).allFinished) {
            plan->batch = 3;
        } else {
            generation->Hold();
        }
        break;
    default: break;
    }
    return {};
}

O::WorldDescription SimWorld() {
    O::WorldDescription world = AuthorWorld("Sim", [](O::WorldSetup& s) {
        K::Registrar& r = s.Registrar();
        r.Resource<GenPlan>({.name = "GenPlan"});
        s.CarryOver<GenPlan>("GenPlan");
        const std::string main(O::DefaultSchedules::frame);
        r.Set(K::SetOptions{.name = "Gen", .schedule = main}, {O::WhileGenerating()});
        r.AddSystem<&GenerateLevel>(At("GenerateLevel", main, std::string(O::DefaultSchedules::preUpdate), {"Gen"}));
    });
    world.prepare.push_back([](K::World& w) -> Base::Result<void, O::OrchestrationError> {
        if (w.Resource<GenPlan>() == nullptr) w.SetResource(GenPlan{});
        return {};
    });
    return world;
}

std::string ValueOf(const WA::AssetView& view, const TextHandle& handle) {
    const Text* text = view.Get(handle);
    return text == nullptr ? std::string("<none>") : text->Bytes();
}

}  // namespace

TEST_CASE("scenario: generate, share, load a save (BuildNewFirst, DropOldFirst), a missing id, the counts") {
    Fresh fresh;
    Content content("scenario");
    content.Add("a.one", "text", "a/one.txt", "one")
        .Add("a.two", "text", "a/two.txt", "two")
        .Add("a.three", "text", "a/three.txt", "three")
        .Add("a.four", "text", "a/four.txt", "four")
        .Add("a.missing", "text", "a/missing.txt", std::nullopt)
        .Add("a.big", "text", "a/big.txt", std::string(100, 'x'))
        .Write();
    AssetTest::ControlledExecutor executor;
    O::ProductDescription description;
    description.name = "scenario";
    WA::AssetOptions options;
    options.contentRoot = content.Root();
    options.byteLimits = {Asset::AssetByteLimit{"text", "txt", 64}};
    options.executor = &executor;
    description.options.Set(options);
    description.worlds.push_back(SimWorld());
    description.worlds.push_back(AuthorWorld("View"));
    auto product = Build(std::move(description), Units({.discardWatch = true}));
    O::Product& p = *product;
    const Asset::AssetStoreDiagnostics& store = StoreOf(p);

    // Step 1: the World generates in two batches; between frames the jobs finish a few at a time (G-1: the
    // apply System runs every generating frame, or the view would never change and generation never end).
    Frame(p);
    CHECK(p.Generating("Sim"));
    CHECK(executor.Pending() == 5);  // a.unknown is refused at the request level: no job
    executor.RunNext();
    executor.RunNext();
    Frame(p);
    CHECK_MESSAGE(p.Generating("Sim"), "[aw-gen-waits] generation ended before its first batch finished");
    executor.RunAll();
    Frame(p);  // batch 1 finished: batch 2 requests a.four
    CHECK(p.Generating("Sim"));
    CHECK(executor.Pending() == 1);
    executor.RunAll();
    Frame(p);  // batch 2 finished: nobody holds
    CHECK_MESSAGE(!p.Generating("Sim"), "[aw-gen-ends] generation did not end once every batch finished");

    // Step 2: finished, not complete: 3 Loaded, NotFound and TooLarge at the id level, UnknownId at the request level.
    {
        const WA::AssetView& view = ViewOf(p, "Sim");
        const Held& held = HeldOf(p, "Sim");
        REQUIRE(held.handles.size() == 7);
        const WA::AssetReadiness ready = view.Ready(Group(held.handles, 0, 6));
        CHECK(ready.allFinished);
        CHECK_FALSE(ready.complete);
        CHECK(ready.dependenciesFinished);
        CHECK(ready.loaded == 3);
        CHECK(ready.failed == 3);
        REQUIRE(ready.failures.size() == 3);
        CHECK(ready.failures[0] == WA::AssetReadyFailure{Asset::AssetId("a.missing"), {}, Asset::AssetId("a.missing"), {},
                                                         Asset::AssetLoadFailureCode::NotFound});
        CHECK(ready.failures[1] == WA::AssetReadyFailure{Asset::AssetId("a.big"), {}, Asset::AssetId("a.big"), {},
                                                         Asset::AssetLoadFailureCode::TooLarge});
        CHECK(ready.failures[2] == WA::AssetReadyFailure{Asset::AssetId("a.unknown"), {}, Asset::AssetId("a.unknown"), {},
                                                         Asset::AssetLoadFailureCode::UnknownId});
        CHECK(view.State(held.handles[5]) == Asset::AssetState::Failed);
        REQUIRE(view.Failure(held.handles[5]) != nullptr);
        CHECK(view.Failure(held.handles[5])->code == Asset::AssetLoadFailureCode::UnknownId);
        CHECK(ValueOf(view, held.handles[0]) == "one");
        CHECK(view.Unfinished(Group(held.handles)) == 0);
    }

    // Step 3: a second World requests the same three: no file is read again and both read the same value.
    const std::uint64_t readsAfterGeneration = store.Counts().fileReads;
    OrdersOf(p, "View").load = {"a.one", "a.two", "a.three"};
    Frame(p);
    {
        const Held& sim = HeldOf(p, "Sim");
        const Held& view = HeldOf(p, "View");
        REQUIRE(view.handles.size() == 3);
        CHECK_MESSAGE(store.Counts().fileReads == readsAfterGeneration,
                      "[aw-share-no-read] a second World's request read the file again");
        for (std::size_t index = 0; index < 3; ++index) {
            CHECK(ViewOf(p, "View").State(view.handles[index]) == Asset::AssetState::Loaded);
            const Text* inView = ViewOf(p, "View").Get(view.handles[index]);
            const Text* inSim = ViewOf(p, "Sim").Get(sim.handles[index]);
            REQUIRE(inView != nullptr);
            CHECK_MESSAGE(inView == inSim, "[aw-share-same-value] the two Worlds read different values");
        }
    }
    OrdersOf(p, "View").dropHeld = true;
    Frame(p);  // the second World lets the Handles go
    Frame(p);  // and runs its apply point (maintenance)
    CHECK(store.Counts().removals == 0);
    CHECK(ViewOf(p, "Sim").State(HeldOf(p, "Sim").handles[2]) == Asset::AssetState::Loaded);

    // Step 4: loading a save, validated first: the save's two ids are finished in the current World, then
    // BuildNewFirst; the new World's beforeSwap requests them again and hits (step 7: read at once, Loaded).
    OrdersOf(p, "Sim").load = {"a.one", "a.two"};
    Frame(p);
    {
        const Held& held = HeldOf(p, "Sim");
        REQUIRE(held.handles.size() == 9);
        CHECK(ViewOf(p, "Sim").Ready(Group(held.handles, 7, 2)).allFinished);
    }
    const std::uint64_t readsBeforeSave = store.Counts().fileReads;
    TheSave().ids = {"a.one", "a.two"};
    REQUIRE(static_cast<bool>(p.ReplaceWorld("Sim", O::ReplaceRequest{{}, {"LoadSave"}, O::ReplaceOrder::BuildNewFirst})));
    REQUIRE(TheSave().seen.size() == 2);
    for (const SaveSeen& seen : TheSave().seen) {
        CHECK_MESSAGE(seen.state == Asset::AssetState::Loaded,
                      "[aw-before-swap-loaded] " << seen.id << " was " << std::string(Asset::ToString(seen.state)) << " in beforeSwap");
    }
    CHECK(TheSave().seen[0].value == "one");
    CHECK(TheSave().seen[1].value == "two");
    CHECK_MESSAGE(store.Counts().fileReads == readsBeforeSave, "[aw-build-new-first-no-read] BuildNewFirst read a file again");
    // The old World's Handles went with it; the one no longer wanted goes at the next maintenance.
    CHECK(store.Probe(Asset::AssetId("a.three")).state == Asset::AssetState::Loaded);
    Frame(p);
    CHECK_MESSAGE(store.Probe(Asset::AssetId("a.three")).state == Asset::AssetState::NotRequested,
                  "[aw-unwanted-removed] a.three, held by no World, is still alive");
    CHECK(store.Probe(Asset::AssetId("a.one")).state == Asset::AssetState::Loaded);
    CHECK(store.Counts().removals == 4);  // a.three, a.four, a.missing, a.big

    // Step 5: DropOldFirst. a.one is also in the author's carried Resource (a): not read again. a.two is held only
    // by the old World (b): world_asset's prepare function takes the old World's drops first, so it is read again.
    OrdersOf(p, "Sim").carry = {"a.one"};
    Frame(p);
    const std::uint64_t lifeOne = store.Probe(Asset::AssetId("a.one")).life;
    const std::uint64_t lifeTwo = store.Probe(Asset::AssetId("a.two")).life;
    const std::uint64_t readsBeforeDrop = store.Counts().fileReads;
    TheSave() = SaveFile{{"a.one", "a.two"}, {}};
    REQUIRE(static_cast<bool>(p.ReplaceWorld("Sim", O::ReplaceRequest{{}, {"LoadSave"}, O::ReplaceOrder::DropOldFirst})));
    REQUIRE(TheSave().seen.size() == 2);
    CHECK(TheSave().seen[0].state == Asset::AssetState::Loaded);
    CHECK(TheSave().seen[0].value == "one");
    CHECK(TheSave().seen[1].state == Asset::AssetState::Queued);
    CHECK(executor.Pending() == 1);
    executor.RunAll();
    CHECK_MESSAGE(store.Probe(Asset::AssetId("a.one")).life == lifeOne,
                  "[aw-drop-old-carried] a.one, carried over, got a new life");
    CHECK_MESSAGE(store.Probe(Asset::AssetId("a.two")).life == lifeTwo + 1,
                  "[aw-drop-old-reread] a.two, held only by the old World, was not removed and read again");
    CHECK(store.Counts().fileReads == readsBeforeDrop + 1);
    Frame(p);
    {
        const Held& held = HeldOf(p, "Sim");
        REQUIRE(held.handles.size() == 2);
        CHECK(ViewOf(p, "Sim").State(held.handles[1]) == Asset::AssetState::Loaded);
        CHECK(ValueOf(ViewOf(p, "Sim"), held.handles[1]) == "two");
    }

    // Step 6: a save that records an id no longer in the catalog fails validation (request level); no
    // replacement is asked for and the World goes on.
    K::WorldExecutor* before = &p.World("Sim");
    OrdersOf(p, "Sim").load = {"a.gone"};
    Frame(p);
    {
        const Held& held = HeldOf(p, "Sim");
        REQUIRE(held.handles.size() == 3);
        const WA::AssetReadiness ready = ViewOf(p, "Sim").Ready(Group(held.handles, 2, 1));
        CHECK(ready.allFinished);
        CHECK(ready.failed == 1);
        REQUIRE(ready.failures.size() == 1);
        CHECK(ready.failures[0].code == Asset::AssetLoadFailureCode::UnknownId);
    }
    Frame(p);
    CHECK(&p.World("Sim") == before);

    // Step 8: the reconciliation (5.8), every number written here, with nothing in flight.
    {
        const Asset::AssetStoreCounts c = store.Counts();
        AssetTest::CheckEquations(c);
        // Loads: 6 + 1 (generation), 3 (the second World), 2 (validation), 2 (beforeSwap), 1 (carry),
        // 2 (beforeSwap), 1 (a.gone).
        CHECK(c.requests == 18);
        CHECK(c.hits == 9);
        CHECK(c.hitsReloading == 0);
        CHECK(c.revives == 0);
        CHECK(c.newLives == 7);
        CHECK(c.newLivesQueued == 7);
        CHECK(c.requestFailures == 2);
        CHECK(c.jobsQueued == 7);
        CHECK(c.jobsLoaded == 5);
        CHECK(c.jobsFailed == 2);
        CHECK(c.jobsCancelled == 0);
        CHECK(c.jobsDiscarded == 0);
        CHECK(c.jobsWaiting == 0);
        CHECK(c.jobsInFlight == 0);
        CHECK(c.dropsTaken == 5);
        CHECK(c.dropsSkipped == 0);
        CHECK(c.removals == 5);
    }

    // The Product's end: a job not finished is discarded by world_asset's OnDestroy, and counted (5.8, O-A4).
    OrdersOf(p, "Sim").load = {"a.three"};
    Frame(p);
    CHECK(executor.Pending() == 1);
    product.reset();
    REQUIRE_MESSAGE(TheDiscardRecord().ran, "[aw-on-destroy] the store was not there when the Product ended");
    CHECK_MESSAGE(TheDiscardRecord().counts.jobsDiscarded == 1, "[aw-discarded] the unfinished job was not discarded");
    CHECK(TheDiscardRecord().counts.jobsQueued == 8);
    AssetTest::CheckEquations(TheDiscardRecord().counts);
}

TEST_CASE("scenario: two Worlds of a Product share one store: one read, one value (AS-1)") {
    Fresh fresh;
    Content content("one-store");
    content.Add("a.one", "text", "a/one.txt", "one").Write();
    AssetTest::ControlledExecutor executor(AssetTest::ControlledExecutor::Mode::RunNow);
    O::ProductDescription description;
    description.name = "one-store";
    WA::AssetOptions options;
    options.contentRoot = content.Root();
    options.executor = &executor;
    description.options.Set(options);
    description.worlds.push_back(AuthorWorld("A"));
    description.worlds.push_back(AuthorWorld("B"));
    auto product = Build(std::move(description), Units());
    O::Product& p = *product;
    OrdersOf(p, "A").load = {"a.one"};
    Frame(p);
    OrdersOf(p, "B").load = {"a.one"};
    Frame(p);
    Frame(p);
    const Text* inA = ViewOf(p, "A").Get(HeldOf(p, "A").handles.front());
    const Text* inB = ViewOf(p, "B").Get(HeldOf(p, "B").handles.front());
    REQUIRE(inA != nullptr);
    REQUIRE(inB != nullptr);
    CHECK_MESSAGE(inA == inB, "[aw-one-store-shared] the two Worlds read different objects for one id");
    CHECK(StoreOf(p).Counts().fileReads == 1);
}

// threads' WorkerPool behind an AssetExecutor: the adapter a product writes (AssetOptions::executor; merge review
// S3). WorkerPool::Submit queues a job submitted from inside a job, so a finished job submitting its dependencies
// does not recurse; after Stop it takes nothing, which the executor contract allows. The store's jobs hold the
// store's state, so a job the pool runs after the Product is gone returns at once.
class WorkerPoolExecutor final : public Asset::AssetExecutor {
public:
    explicit WorkerPoolExecutor(GYO::Engine::Threads::WorkerPool& pool) : pool_(pool) {}
    void Submit(Job job) override { (void)pool_.Submit(std::move(job)); }

private:
    GYO::Engine::Threads::WorkerPool& pool_;
};

TEST_CASE("scenario: threads' WorkerPool behind an AssetExecutor loads what two Worlds request") {
    Fresh fresh;
    Content content("worker-pool");
    content.Add("a.one", "text", "a/one.txt", "one")
        .Add("a.two", "text", "a/two.txt", "two")
        .Add("a.three", "text", "a/three.txt", "three")
        .Write();
    auto started = GYO::Engine::Threads::WorkerPool::Start({.name = "asset-test", .threadCount = 2});
    REQUIRE(started.has_value());
    GYO::Engine::Threads::WorkerPool pool = std::move(started).value();
    WorkerPoolExecutor executor(pool);
    {
        O::ProductDescription description;
        description.name = "worker-pool";
        WA::AssetOptions options;
        options.contentRoot = content.Root();
        options.executor = &executor;
        description.options.Set(options);
        description.worlds.push_back(AuthorWorld("A"));
        description.worlds.push_back(AuthorWorld("B"));
        auto product = Build(std::move(description), Units());
        O::Product& p = *product;
        OrdersOf(p, "A").load = {"a.one", "a.two"};
        OrdersOf(p, "B").load = {"a.two", "a.three"};
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        bool ready = false;
        while (!ready && std::chrono::steady_clock::now() < deadline) {
            Frame(p);
            ready = ViewOf(p, "A").Ready(Group(HeldOf(p, "A").handles)).complete &&
                    ViewOf(p, "B").Ready(Group(HeldOf(p, "B").handles)).complete;
            if (!ready) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        REQUIRE_MESSAGE(ready, "the pool did not load the four requests within 10 s");
        REQUIRE(HeldOf(p, "A").handles.size() == 2);
        REQUIRE(HeldOf(p, "B").handles.size() == 2);
        CHECK(ValueOf(ViewOf(p, "A"), HeldOf(p, "A").handles[0]) == "one");
        CHECK(ValueOf(ViewOf(p, "A"), HeldOf(p, "A").handles[1]) == "two");
        CHECK(ValueOf(ViewOf(p, "B"), HeldOf(p, "B").handles[0]) == "two");
        CHECK(ValueOf(ViewOf(p, "B"), HeldOf(p, "B").handles[1]) == "three");
        CHECK(StoreOf(p).Counts().fileReads == 3);
    }
    pool.Stop(GYO::Engine::Threads::WorkerPool::StopMode::Drain);
}

}  // namespace WorldAssetTest
