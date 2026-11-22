// What a World sees of the store (declaration docs/architecture/target/v2/asset-declaration.zh-Hant.md, 5.4 to
// 5.7, section 10): one frame sees one view (AS-4); an older view against a newer life is Queued (5.7); the
// request-level failures; the readiness of a group with dependencies, a loader still to come and a skipped
// dependency; a Product with no executor and one with no content; the catalog's startup error; a view an author
// removed is built again.
#include "WorldAssetSupport.hpp"

namespace WorldAssetTest {
namespace {

// ---- One frame, one view (AS-4) ----

AssetTest::ControlledExecutor*& TheExecutor() {
    static AssetTest::ControlledExecutor* executor = nullptr;
    return executor;
}

std::vector<Asset::AssetState>& Seen() {
    static std::vector<Asset::AssetState> seen;
    return seen;
}

// PreUpdate: a job finishes in the middle of the frame (on the World's thread here; a worker would do the same
// under the store's lock).
K::SystemResult FinishJobsMidFrame() {
    if (TheExecutor() != nullptr) TheExecutor()->RunAll();
    return {};
}

// Update and Last: what the frame's Systems see of the first held Handle.
K::SystemResult ObserveFirst(K::Res<WA::AssetView> view, K::Res<Held> held) {
    Seen().push_back(held->handles.empty() ? Asset::AssetState::NotRequested : view->State(held->handles.front()));
    return {};
}

}  // namespace

TEST_CASE("view: a job that finishes in the middle of a frame is seen at the next apply point, not in that frame") {
    Fresh fresh;
    Seen().clear();
    Content content("same-frame");
    content.Add("a.one", "text", "a/one.txt", "one").Write();
    AssetTest::ControlledExecutor executor;
    TheExecutor() = &executor;
    O::ProductDescription description;
    description.name = "same-frame";
    WA::AssetOptions options;
    options.contentRoot = content.Root();
    options.executor = &executor;
    description.options.Set(options);
    description.worlds.push_back(AuthorWorld("W", [](O::WorldSetup& s) {
        K::Registrar& r = s.Registrar();
        r.AddSystem<&FinishJobsMidFrame>(InMain("FinishJobsMidFrame", O::DefaultSchedules::preUpdate));
        r.AddSystem<&ObserveFirst>(InMain("ObserveInUpdate", O::DefaultSchedules::update));
        r.AddSystem<&ObserveFirst>(InMain("ObserveInLast", O::DefaultSchedules::last));
    }));
    auto product = Build(std::move(description), Units());
    O::Product& p = *product;
    HeldOf(p, "W").handles.push_back(RequestsOf(p, "W").Load<Text>(Asset::AssetId("a.one")));
    REQUIRE(executor.Pending() == 1);

    Frame(p);  // the job finishes in PreUpdate
    CHECK(executor.Pending() == 0);
    CHECK(StoreOf(p).Probe(Asset::AssetId("a.one")).state == Asset::AssetState::Loaded);
    REQUIRE(Seen().size() == 2);
    CHECK_MESSAGE(Seen()[0] == Asset::AssetState::Queued, "[aw-same-frame] Update saw " << std::string(Asset::ToString(Seen()[0])));
    CHECK_MESSAGE(Seen()[1] == Asset::AssetState::Queued, "[aw-same-frame] Last saw " << std::string(Asset::ToString(Seen()[1])));
    Frame(p);
    REQUIRE(Seen().size() == 4);
    CHECK_MESSAGE(Seen()[2] == Asset::AssetState::Loaded, "[aw-next-apply] the next frame's Update saw "
                                                              << std::string(Asset::ToString(Seen()[2])));
    CHECK(Seen()[3] == Asset::AssetState::Loaded);
    TheExecutor() = nullptr;
}

namespace {

// ---- An older view against a newer life (5.7) ----

struct Reload {
    Asset::AssetState state = Asset::AssetState::NotRequested;
    bool value = true;
    std::uint64_t life = 0;
};
Reload& TheReload() {
    static Reload reload;
    return reload;
}

// Handles a World hands to another as Extract would (test only).
std::vector<TextHandle>& Passed() {
    static std::vector<TextHandle> passed;
    return passed;
}

K::SystemResult ReleaseHeld(K::ResMut<Held> held) {
    held->handles.clear();
    return {};
}

K::SystemResult ReloadOne(K::Res<WA::AssetRequests> requests, K::Res<WA::AssetView> view, K::ResMut<Held> held) {
    TextHandle handle = requests->Load<Text>(Asset::AssetId("a.one"));
    TheReload().state = view->State(handle);
    TheReload().value = view->Get(handle) != nullptr;
    TheReload().life = handle.Life();
    held->handles.push_back(std::move(handle));
    return {};
}

}  // namespace

TEST_CASE("view: an older view against a newer life is Queued, with no value (5.7)") {
    Fresh fresh;
    TheReload() = Reload{};
    Passed().clear();
    Content content("old-view");
    content.Add("a.one", "text", "a/one.txt", "one").Add("a.two", "text", "a/two.txt", "two").Write();
    AssetTest::ControlledExecutor executor;
    O::ProductDescription description;
    description.name = "old-view";
    WA::AssetOptions options;
    options.contentRoot = content.Root();
    options.executor = &executor;
    description.options.Set(options);
    description.worlds.push_back(AuthorWorld("A", [](O::WorldSetup& s) {
        K::Registrar& r = s.Registrar();
        r.Schedule(Once("ReleaseHeld"));
        r.AddSystem<&ReleaseHeld>(At("ReleaseHeld", "ReleaseHeld", "S"));
        r.Schedule(Once("ReloadOne"));
        r.AddSystem<&ReloadOne>(At("ReloadOne", "ReloadOne", "S"));
    }));
    description.worlds.push_back(AuthorWorld("B"));
    auto product = Build(std::move(description), Units());
    O::Product& p = *product;
    const Asset::AssetStore& store = StoreOf(p);
    HeldOf(p, "A").handles.push_back(RequestsOf(p, "A").Load<Text>(Asset::AssetId("a.one")));
    executor.RunAll();
    Frame(p);
    const std::uint64_t oldLife = HeldOf(p, "A").handles.front().Life();
    REQUIRE(ViewOf(p, "A").State(HeldOf(p, "A").handles.front()) == Asset::AssetState::Loaded);

    // World A applies; within A's frame it lets the last Handle go; World B's apply point runs maintenance and
    // removes a.one; later in A's same frame a.one is requested again: a new life A's view has not seen
    // (the schedules interleaved by hand; RunFrame runs the Worlds one after the other).
    Schedule(p, "A", O::DefaultSchedules::frame);
    Schedule(p, "A", "ReleaseHeld");
    Schedule(p, "B", O::DefaultSchedules::frame);
    REQUIRE(store.Probe(Asset::AssetId("a.one")).state == Asset::AssetState::NotRequested);
    Schedule(p, "A", "ReloadOne");
    CHECK(TheReload().life == oldLife + 1);
    CHECK_MESSAGE(TheReload().state == Asset::AssetState::Queued,
                  "[aw-old-view] A's older view gave " << std::string(Asset::ToString(TheReload().state)) << " for the new life");
    CHECK_MESSAGE(!TheReload().value, "[aw-old-view-value] A's older view gave the old life's value");
    executor.RunAll();
    Schedule(p, "A", O::DefaultSchedules::frame);
    CHECK(ViewOf(p, "A").State(HeldOf(p, "A").handles.front()) == Asset::AssetState::Loaded);

    // A Handle made in B after A's last pull, handed to A as Extract would hand it: Queued in A until A pulls.
    Passed().push_back(RequestsOf(p, "B").Load<Text>(Asset::AssetId("a.two")));
    executor.RunAll();
    CHECK(ViewOf(p, "A").State(Passed().front()) == Asset::AssetState::Queued);
    CHECK(ViewOf(p, "A").Get(Passed().front()) == nullptr);
    Schedule(p, "A", O::DefaultSchedules::frame);
    CHECK(ViewOf(p, "A").State(Passed().front()) == Asset::AssetState::Loaded);
    Passed().clear();
}

TEST_CASE("view: request-level failures are the Handle's, and touch no item") {
    Fresh fresh;
    Content content("request-level");
    content.Add("a.one", "text", "a/one.txt", "one").Write();
    AssetTest::ControlledExecutor executor(AssetTest::ControlledExecutor::Mode::RunNow);
    O::ProductDescription description;
    description.name = "request-level";
    WA::AssetOptions options;
    options.contentRoot = content.Root();
    options.executor = &executor;
    description.options.Set(options);
    description.worlds.push_back(AuthorWorld("A"));
    description.worlds.push_back(AuthorWorld("B"));
    auto product = Build(std::move(description), Units());
    O::Product& p = *product;
    HeldOf(p, "A").handles.push_back(RequestsOf(p, "A").Load<Text>(Asset::AssetId("a.one")));
    Frame(p);
    {
        // The wrong type for a.one (no loader for "probe" is needed to tell), and an id in no catalog.
        const Asset::Handle<AssetTest::Probe> wrong = RequestsOf(p, "B").Load<AssetTest::Probe>(Asset::AssetId("a.one"));
        const TextHandle unknown = RequestsOf(p, "B").Load<Text>(Asset::AssetId("a.nowhere"));
        const WA::AssetView& view = ViewOf(p, "B");
        CHECK(view.State(wrong) == Asset::AssetState::Failed);
        REQUIRE(view.Failure(wrong) != nullptr);
        CHECK(view.Failure(wrong)->code == Asset::AssetLoadFailureCode::TypeMismatch);
        CHECK(view.Get(wrong) == nullptr);
        CHECK(view.State(unknown) == Asset::AssetState::Failed);
        CHECK(view.Failure(unknown)->code == Asset::AssetLoadFailureCode::UnknownId);
        const WA::AssetReadiness ready = view.Ready({wrong, unknown});
        CHECK(ready.allFinished);
        CHECK(ready.failed == 2);
        CHECK(ready.failures.size() == 2);
        CHECK(view.Failures().empty());  // no item failed
    }
    Frame(p);  // B's wrong-type Handle went: maintenance has nothing of it
    CHECK(StoreOf(p).Probe(Asset::AssetId("a.one")).dropsToSkip == 0);
    CHECK(ViewOf(p, "A").State(HeldOf(p, "A").handles.front()) == Asset::AssetState::Loaded);
    CHECK(StoreOf(p).Counts().removals == 0);
}

TEST_CASE("view: readiness with dependencies, a loader still to come, a skipped and a failed reference") {
    Fresh fresh;
    Content content("ready");
    // l.root references a text, a probe (no probe loader, none preregistered: skipped), an image (its loader
    // preregistered, not registered: waiting) and an id in no catalog (an edge-level failure).
    content.Add("l.root", "linked", "l/root.links", "text:a.one\nprobe:p.none\nimage:i.wait\nid:l.nowhere\n")
        .Add("a.one", "text", "a/one.txt", "one")
        .Add("a.missing", "text", "a/missing.txt", std::nullopt)
        .Add("p.none", "probe", "p/none.probe", "probe")
        .Add("i.wait", "image", "i/wait.png", "png")
        .Write();
    AssetTest::ControlledExecutor executor;
    O::ProductDescription description;
    description.name = "ready";
    WA::AssetOptions options;
    options.contentRoot = content.Root();
    options.executor = &executor;
    description.options.Set(options);
    description.worlds.push_back(AuthorWorld("W"));
    auto product = Build(std::move(description), Units({.linkedType = true}));
    O::Product& p = *product;
    const Asset::Handle<AssetTest::Linked> root = RequestsOf(p, "W").Load<AssetTest::Linked>(Asset::AssetId("l.root"));
    const TextHandle one = RequestsOf(p, "W").Load<Text>(Asset::AssetId("a.one"));
    const TextHandle missing = RequestsOf(p, "W").Load<Text>(Asset::AssetId("a.missing"));
    const TextHandle unknown = RequestsOf(p, "W").Load<Text>(Asset::AssetId("a.nowhere"));
    Frame(p);
    CHECK(ViewOf(p, "W").Unfinished({root, one, missing, unknown}) == 3);
    CHECK_FALSE(ViewOf(p, "W").Ready({root, one, missing, unknown}).allFinished);
    executor.RunAll();
    Frame(p);
    {
        const WA::AssetView& view = ViewOf(p, "W");
        const WA::AssetReadiness ready = view.Ready({root, one, missing, unknown});
        CHECK(view.Unfinished({root, one, missing, unknown}) == 0);
        CHECK(ready.allFinished);
        CHECK_FALSE(ready.complete);
        CHECK_FALSE(ready.dependenciesFinished);  // i.wait waits for its loader
        CHECK(ready.loaded == 2);
        CHECK(ready.failed == 2);
        CHECK(Ids(ready.skipped) == "p.none");
        CHECK(Ids(ready.waitingForLoader) == "i.wait");
        CHECK(Ids(ready.ids) == "l.root,a.one,i.wait,a.missing,a.nowhere");
        REQUIRE(ready.failures.size() == 3);
        CHECK(ready.failures[0] == WA::AssetReadyFailure{Asset::AssetId("l.root"), Asset::AssetId("l.root"),
                                                         Asset::AssetId("l.nowhere"), {},
                                                         Asset::AssetLoadFailureCode::UnknownId});
        CHECK(ready.failures[1].handle == Asset::AssetId("a.missing"));
        CHECK(ready.failures[1].code == Asset::AssetLoadFailureCode::NotFound);
        CHECK(ready.failures[2].handle == Asset::AssetId("a.nowhere"));
        CHECK(ready.failures[2].code == Asset::AssetLoadFailureCode::UnknownId);
        const std::vector<Asset::FailureRecord> failures = view.Failures();
        REQUIRE(failures.size() == 1);
        CHECK(failures[0] == Asset::FailureRecord{Asset::AssetId("a.missing"), 1, Asset::AssetLoadFailureCode::NotFound});
        // The reference reads through the view once the parent is loaded.
        const AssetTest::Linked* linked = view.Get(root);
        REQUIRE(linked != nullptr);
        CHECK(view.Get(Asset::AssetRef<Text>(Asset::AssetId("a.one"))) == view.Get(one));
        CHECK(view.SourceDigest(one) != nullptr);
    }
    // The loader comes (registered from a System's side: any time, 5.4): the waiting one loads.
    const Asset::AssetLoaderRegistry* registry = std::as_const(p.Buffer()).Find<Asset::AssetLoaderRegistry>();
    REQUIRE(registry != nullptr);
    REQUIRE(static_cast<bool>(registry->Register<AssetTest::PngLoader>()));
    executor.RunAll();
    Frame(p);
    const WA::AssetReadiness after = ViewOf(p, "W").Ready({root});
    CHECK(after.dependenciesFinished);
    CHECK(after.waitingForLoader.empty());
    CHECK_FALSE(after.complete);  // l.nowhere stays an edge-level failure
}

TEST_CASE("view: no executor fails the request with NoExecutor; no content root is an empty catalog") {
    Fresh fresh;
    Content content("no-executor");
    content.Add("a.one", "text", "a/one.txt", "one").Write();
    {
        O::ProductDescription description;
        description.name = "no-executor";
        WA::AssetOptions options;
        options.contentRoot = content.Root();
        description.options.Set(options);
        description.worlds.push_back(AuthorWorld("W"));
        auto product = Build(std::move(description), Units());
        O::Product& p = *product;
        HeldOf(p, "W").handles.push_back(RequestsOf(p, "W").Load<Text>(Asset::AssetId("a.one")));
        Frame(p);
        const WA::AssetView& view = ViewOf(p, "W");
        CHECK(view.State(HeldOf(p, "W").handles.front()) == Asset::AssetState::Failed);
        REQUIRE(view.Failure(HeldOf(p, "W").handles.front()) != nullptr);
        CHECK(view.Failure(HeldOf(p, "W").handles.front())->code == Asset::AssetLoadFailureCode::NoExecutor);
        CHECK(StoreOf(p).Counts().newLivesNoExecutor == 1);
    }
    {
        O::ProductDescription description;
        description.name = "no-options";
        description.worlds.push_back(AuthorWorld("W"));
        auto product = Build(std::move(description), Units());
        O::Product& p = *product;
        const TextHandle handle = RequestsOf(p, "W").Load<Text>(Asset::AssetId("a.one"));
        REQUIRE(handle.RequestFailure() != nullptr);
        CHECK(handle.RequestFailure()->code == Asset::AssetLoadFailureCode::UnknownId);
        Frame(p);
    }
}

TEST_CASE("view: content that breaks the catalog's rules is a startup error reported from Configure") {
    Fresh fresh;
    Content content("bad-catalog");
    content.WriteFile("a/one.txt", "one");
    // Written by hand: WriteCatalog refuses a duplicate id.
    content.WriteFile("asset_catalog.json",
                      R"({"version":1,"assets":[{"id":"a.one","type":"text","path":"a/one.txt"},)"
                      R"({"id":"a.one","type":"text","path":"a/two.txt"}]})");
    content.WriteFile("content.json", R"({"version":1,"catalogs":["asset_catalog.json"]})");
    O::ProductDescription description;
    description.name = "bad-catalog";
    WA::AssetOptions options;
    options.contentRoot = content.Root();
    description.options.Set(options);
    description.worlds.push_back(AuthorWorld("W"));
    auto built = O::Product::Build(std::move(description), Units({.linkedType = true}));
    REQUIRE_FALSE(static_cast<bool>(built));
    const std::string detail = built.error().detail;
    CHECK_MESSAGE(detail.find("WA-E1") != std::string::npos, detail);
    CHECK_MESSAGE(detail.find("DuplicateId") != std::string::npos, detail);
    // The unit that registers a loader still found the registry: no error of its own.
    CHECK_MESSAGE(detail.find("LT-E") == std::string::npos, detail);
}

TEST_CASE("view: an AssetView an author removed is built again by the apply System") {
    Fresh fresh;
    Content content("rebuilt");
    content.Add("a.one", "text", "a/one.txt", "one").Write();
    AssetTest::ControlledExecutor executor(AssetTest::ControlledExecutor::Mode::RunNow);
    O::ProductDescription description;
    description.name = "rebuilt";
    WA::AssetOptions options;
    options.contentRoot = content.Root();
    options.executor = &executor;
    description.options.Set(options);
    description.worlds.push_back(AuthorWorld("W"));
    auto product = Build(std::move(description), Units());
    O::Product& p = *product;
    HeldOf(p, "W").handles.push_back(RequestsOf(p, "W").Load<Text>(Asset::AssetId("a.one")));
    WorldOf(p, "W").RemoveResource<WA::AssetView>();
    REQUIRE(std::as_const(WorldOf(p, "W")).Resource<WA::AssetView>() == nullptr);
    Frame(p);
    CHECK(ViewOf(p, "W").State(HeldOf(p, "W").handles.front()) == Asset::AssetState::Loaded);
}

}  // namespace WorldAssetTest
