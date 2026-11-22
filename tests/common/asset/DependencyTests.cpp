// Dependencies at the store layer (declaration 5.17, section 10 "依賴（AS-13）"; step 2 of section 9),
// with the test loader that declares references (StoreSupport.hpp, LinkedLoader).
#include "StoreSupport.hpp"

using namespace AssetTest;
using Asset::AssetId;
using Asset::AssetLoadFailureCode;
using Asset::AssetState;
using Asset::DependencyState;
using Asset::EdgeStatus;

namespace {

std::vector<std::string> Texts(const std::vector<AssetId>& ids) {
    std::vector<std::string> texts;
    for (const AssetId& id : ids) texts.push_back(id.Text());
    return texts;
}

Asset::DependencyStatus StatusOf(const Asset::AssetStore& store, const std::string& id) {
    return store.Snapshot().WithDependencies(AssetId(id));
}

std::vector<EdgeStatus> EdgeStatuses(const Asset::AssetStore& store, const std::string& id) {
    std::vector<EdgeStatus> statuses;
    const auto snapshot = store.Snapshot();
    const Asset::AssetRecord* record = snapshot.Find(AssetId(id));
    REQUIRE(record != nullptr);
    for (const Asset::AssetEdge& edge : record->edges) statuses.push_back(edge.status);
    return statuses;
}

std::vector<AssetLoadFailureCode> EdgeFailures(const Asset::AssetStore& store, const std::string& id) {
    std::vector<AssetLoadFailureCode> failures;
    const auto snapshot = store.Snapshot();
    const Asset::AssetRecord* record = snapshot.Find(AssetId(id));
    REQUIRE(record != nullptr);
    for (const Asset::AssetEdge& edge : record->edges) {
        if (edge.failure) failures.push_back(*edge.failure);
    }
    return failures;
}

} // namespace

TEST_CASE("depend: a chain is complete when its last link loads") {
    Fresh fresh;
    TestContent content("chain");
    content.Add("a", "linked", "a.links", "id:b").Add("b", "linked", "b.links", "id:c").Add("c", "linked", "c.links",
                                                                                           "end");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    const Asset::Handle<Linked> a = store.Request<Linked>(AssetId("a"));
    CHECK(executor.RunNext());
    CHECK(StatusOf(store, "a").state == DependencyState::Incomplete);
    CHECK(executor.RunNext());
    CHECK_MESSAGE(StatusOf(store, "a").state == DependencyState::Incomplete, "complete before the last link loaded");
    CHECK(executor.RunNext());
    const auto status = StatusOf(store, "a");
    CHECK(status.state == DependencyState::Complete);
    CHECK(Texts(status.ids) == std::vector<std::string>{"a", "b", "c"});
    const auto counts = store.Counts();
    CHECK(counts.references == 2);
    CHECK(counts.referenceNewLivesQueued == 2);
    CheckEquations(counts);
}

TEST_CASE("depend: dropping the head of a chain removes the chain at the next maintenance") {
    Fresh fresh;
    TestContent content("chain-drop");
    content.Add("a", "linked", "a.links", "id:b").Add("b", "linked", "b.links", "id:c").Add("c", "linked", "c.links",
                                                                                           "end");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    {
        const Asset::Handle<Linked> a = store.Request<Linked>(AssetId("a"));
        executor.RunAll();
    }
    store.Pump();
    for (const char* id : {"a", "b", "c"}) {
        CHECK_MESSAGE(StateOf(store, id) == AssetState::NotRequested,
                      "[as-store-pump-to-empty] " << id << " outlived the head of its chain after one Pump");
    }
    CHECK(store.Counts().removals == 3);
}

TEST_CASE("depend: a diamond reads its shared dependency once") {
    Fresh fresh;
    TestContent content("diamond");
    content.Add("a", "linked", "a.links", "id:b\nid:c").Add("b", "linked", "b.links", "id:d");
    content.Add("c", "linked", "c.links", "id:d").Add("d", "linked", "d.links", "end");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    const Asset::Handle<Linked> a = store.Request<Linked>(AssetId("a"));
    executor.RunAll();
    CHECK(TheScript().CallsOf("d") == 1);
    CHECK(store.Counts().fileReads == 4);
    CHECK(StatusOf(store, "a").state == DependencyState::Complete);
    CHECK_MESSAGE(store.Counts().referenceHits == 1,
                  "[as-store-edge-holds] the second parent's edge did not hit the dependency the first edge holds");
    CheckEquations(store.Counts());
}

TEST_CASE("depend: a cycle is marked whichever finishes first, and holds nothing") {
    for (const std::size_t first : {std::size_t{0}, std::size_t{1}}) {
        CAPTURE(first);
        Fresh fresh;
        TestContent content("cycle-" + std::to_string(first));
        content.Add("a", "linked", "a.links", "id:b").Add("b", "linked", "b.links", "id:a");
        ControlledExecutor executor;
        Asset::AssetStore store(content.Entries(), content.Options(), &executor);
        RegisterTestLoaders(store);
        std::optional<Asset::Handle<Linked>> a = store.Request<Linked>(AssetId("a"));
        const Asset::Handle<Linked> b = store.Request<Linked>(AssetId("b"));
        CHECK(executor.RunAt(first));
        CHECK(executor.RunNext());
        CHECK(EdgeStatuses(store, "a") == std::vector<EdgeStatus>{EdgeStatus::Cyclic});
        CHECK(EdgeStatuses(store, "b") == std::vector<EdgeStatus>{EdgeStatus::Cyclic});
        CHECK(EdgeFailures(store, "a") == std::vector<AssetLoadFailureCode>{AssetLoadFailureCode::CyclicReference});
        CHECK(StatusOf(store, "a").state == DependencyState::Failed);
        CHECK(StatusOf(store, "b").state == DependencyState::Failed);
        CHECK(store.Counts().referenceCyclic == 2);
        CheckEquations(store.Counts());
        a.reset(); // only b is held
        store.Pump();
        CHECK_MESSAGE(StateOf(store, "a") == AssetState::NotRequested, "a cyclic edge kept its target alive");
        CHECK(StateOf(store, "b") == AssetState::Loaded);
    }
}

TEST_CASE("depend: a self reference is a cycle") {
    Fresh fresh;
    TestContent content("self");
    content.Add("c", "linked", "c.links", "id:c");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    {
        const Asset::Handle<Linked> c = store.Request<Linked>(AssetId("c"));
        executor.RunAll();
        CHECK(EdgeStatuses(store, "c") == std::vector<EdgeStatus>{EdgeStatus::Cyclic});
        CHECK(StatusOf(store, "c").state == DependencyState::Failed);
    }
    store.Pump();
    CHECK_MESSAGE(StateOf(store, "c") == AssetState::NotRequested, "a self reference kept the asset alive");
}

TEST_CASE("depend: a cycle closed through an edge already marked is marked too") {
    // a -> b -> a is marked first; then n (b -> n, n -> a) closes a larger component through the marked
    // edge a -> b. The final marks depend only on the content (AS-5).
    std::vector<std::string> results;
    for (const bool nFirst : {false, true}) {
        Fresh fresh;
        TestContent content(std::string("cycle-through-") + (nFirst ? "n" : "ab"));
        content.Add("a", "linked", "a.links", "id:b").Add("b", "linked", "b.links", "id:a\nid:n");
        content.Add("n", "linked", "n.links", "id:a");
        ControlledExecutor executor;
        Asset::AssetStore store(content.Entries(), content.Options(), &executor);
        RegisterTestLoaders(store);
        std::vector<Asset::Handle<Linked>> handles;
        for (const char* id : {"a", "b", "n"}) handles.push_back(store.Request<Linked>(AssetId(id)));
        if (nFirst) {
            executor.RunAt(2);
        }
        executor.RunAll();
        results.push_back(Describe(Contents(store.Snapshot())));
        CHECK(EdgeStatuses(store, "n") == std::vector<EdgeStatus>{EdgeStatus::Cyclic});
        CHECK(EdgeStatuses(store, "b") == (std::vector<EdgeStatus>{EdgeStatus::Cyclic, EdgeStatus::Cyclic}));
    }
    CHECK(results[0] == results[1]);
}

TEST_CASE("depend: a failed dependency is listed with its parent; the parent itself is Loaded") {
    Fresh fresh;
    TestContent content("dependency-failed");
    content.Add("a", "linked", "a.links", "probe:x").Add("x", "probe", "x.probe");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    const Asset::Handle<Linked> a = store.Request<Linked>(AssetId("a"));
    executor.RunAll();
    CHECK(StateOf(store, "a") == AssetState::Loaded);
    const auto status = StatusOf(store, "a");
    CHECK(status.state == DependencyState::Failed);
    REQUIRE(status.failures.size() == 1);
    CHECK(status.failures[0] ==
          Asset::DependencyFailure{AssetId("a"), AssetId("x"), "", AssetLoadFailureCode::NotFound});
}

TEST_CASE("depend: references by path map through the catalog") {
    Fresh fresh;
    TestContent content("by-path");
    content.Add("m", "linked", "models/m.links",
                "probe-path:tex/skin.probe\nprobe-path:tex/none.probe\nprobe-path:../shared/dup.probe\n"
                "probe-path:../../x.probe\nprobe-path:/abs.probe\nprobe-path:tex\\a.probe");
    content.Add("skin", "probe", "models/tex/skin.probe", "skin");
    content.Add("dup1", "probe", "shared/dup.probe", "dup").Add("dup2", "probe", "shared/dup.probe");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    const Asset::Handle<Linked> m = store.Request<Linked>(AssetId("m"));
    executor.RunAll();
    const Asset::AssetSnapshot snapshot = store.Snapshot();
    const Asset::AssetRecord* record = snapshot.Find(AssetId("m"));
    REQUIRE(record != nullptr);
    REQUIRE(record->edges.size() == 6);
    CHECK(record->edges[0].status == EdgeStatus::Held);
    CHECK(record->edges[0].target == AssetId("skin"));
    CHECK(record->Value<Linked>()->references[0] == AssetId("skin"));
    CHECK(EdgeFailures(store, "m") == std::vector<AssetLoadFailureCode>{
                                          AssetLoadFailureCode::UnlistedReference,
                                          AssetLoadFailureCode::AmbiguousReference, AssetLoadFailureCode::BadReference,
                                          AssetLoadFailureCode::BadReference, AssetLoadFailureCode::BadReference});
    CHECK(StateOf(store, "skin") == AssetState::Loaded);
    const auto status = snapshot.WithDependencies(AssetId("m"));
    CHECK(status.state == DependencyState::Failed);
    CHECK(status.failures.size() == 5);
    CHECK(status.failures[0].writtenPath == "tex/none.probe");
    CHECK(store.Counts().referenceFailures == 5);
    CheckEquations(store.Counts());
}

TEST_CASE("depend: edge-level UnknownId and TypeMismatch") {
    Fresh fresh;
    TestContent content("edge-level");
    content.Add("p", "linked", "p.links", "probe:nope\nprobe:t").Add("t", "text", "t.txt", "t");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    const Asset::Handle<Linked> p = store.Request<Linked>(AssetId("p"));
    executor.RunAll();
    const auto statuses = EdgeStatuses(store, "p");
    REQUIRE(statuses.size() == 2);
    CHECK_MESSAGE(statuses[1] == EdgeStatus::Failed, "an edge to an entry of another type was held");
    const auto failures = EdgeFailures(store, "p");
    CHECK(failures == std::vector<AssetLoadFailureCode>{AssetLoadFailureCode::UnknownId,
                                                        AssetLoadFailureCode::TypeMismatch});
    CHECK(StateOf(store, "t") == AssetState::NotRequested);
}

TEST_CASE("depend: a dependency with no loader is skipped, and stays skipped") {
    Fresh fresh;
    TestContent content("skipped");
    content.Add("p", "linked", "p.links", "image:img").Add("img", "image", "img.png", "i");
    content.Add("q", "linked", "q.links", "image:jay").Add("jay", "image", "jay.jpg", "j");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    REQUIRE(store.Loaders().Preregister<JpgLoader>({"jpg"}));
    const Asset::Handle<Linked> p = store.Request<Linked>(AssetId("p"));
    const Asset::Handle<Linked> q = store.Request<Linked>(AssetId("q"));
    executor.RunAll();
    CHECK(EdgeStatuses(store, "p") == std::vector<EdgeStatus>{EdgeStatus::Skipped});
    auto status = StatusOf(store, "p");
    CHECK(status.state == DependencyState::Complete);
    CHECK(Texts(status.skipped) == std::vector<std::string>{"img"});
    CHECK(store.Counts().fileReads == 2);
    // A preregistered dependency waits.
    CHECK_MESSAGE(StateOf(store, "jay") == AssetState::WaitingForLoader, "a preregistered dependency was skipped");
    status = StatusOf(store, "q");
    CHECK(status.state == DependencyState::Incomplete);
    CHECK(Texts(status.waiting) == std::vector<std::string>{"jay"});

    REQUIRE(store.Loaders().Register<PngLoader>());
    REQUIRE(store.Loaders().Register<JpgLoader>());
    executor.RunAll();
    store.Pump();
    CHECK_MESSAGE(EdgeStatuses(store, "p") == std::vector<EdgeStatus>{EdgeStatus::Skipped},
                  "a skipped edge was held after its loader came");
    CHECK(StateOf(store, "img") == AssetState::NotRequested);
    CHECK(StatusOf(store, "q").state == DependencyState::Complete);
    CHECK(store.Counts().referenceSkipped == 1);
    CHECK(store.Counts().referenceNewLivesWaiting == 1);
    CheckEquations(store.Counts());
}

TEST_CASE("depend: an ancestor recovers when a failed dependency is loaded again") {
    Fresh fresh;
    TestContent content("ancestor");
    content.Add("c", "linked", "c.links", "id:a").Add("a", "linked", "a.links", "probe:x");
    content.Add("x", "probe", "x.probe", "x");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    TheScript().FailNext("x");
    const Asset::Handle<Linked> c = store.Request<Linked>(AssetId("c"));
    executor.RunAll();
    auto status = StatusOf(store, "c");
    CHECK(status.state == DependencyState::Failed);
    REQUIRE(status.failures.size() == 1);
    CHECK_MESSAGE(status.failures[0].parent == AssetId("a"), "a dependency failure names the wrong parent");
    CHECK(status.failures[0].reference == AssetId("x"));
    const Asset::Handle<Probe> x = store.Request(Asset::AssetRef<Probe>(AssetId("x")));
    executor.RunAll();
    CHECK_MESSAGE(StatusOf(store, "c").state == DependencyState::Complete,
                  "the ancestor did not recover after the dependency loaded");
}

TEST_CASE("depend: a failed dependency is read again by each parent that finishes after it (5.5, as Bevy)") {
    // A, B -> X; X's file is missing. A finishes and queues X; X fails; B finishes later: its edge is a request,
    // and a request judges a Failed info again, so X is read again (Bevy's load_dependency does the same). A
    // parent that finishes while X is still queued only hits it.
    Fresh fresh;
    TestContent content("failed-dependency-again");
    content.Add("a", "linked", "a.links", "probe:x").Add("b", "linked", "b.links", "probe:x");
    content.Add("x", "probe", "x.probe"); // no file: NotFound
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    const Asset::Handle<Linked> a = store.Request<Linked>(AssetId("a"));
    const Asset::Handle<Linked> b = store.Request<Linked>(AssetId("b"));
    REQUIRE(executor.Pending() == 2);
    executor.RunNext(); // a: queues x behind b
    REQUIRE(executor.RunAt(1)); // x fails
    CHECK(FailureOf(store, "x") == AssetLoadFailureCode::NotFound);
    CHECK(store.Probe(AssetId("x")).attempts == 1);
    executor.RunAll(); // b finishes: its edge queues x again, which fails again
    CHECK_MESSAGE(store.Probe(AssetId("x")).attempts == 2,
                  "[as-store-edge-reloads-failed] a parent finishing after its dependency failed did not read it again");
    const auto counts = store.Counts();
    CHECK(counts.fileReads == 4); // a, x, b, x
    CHECK(counts.jobsFailed == 2);
    CHECK(counts.hitsReloading == 0); // the reload was an edge's, not a request's
    CheckEquations(counts);
    CHECK(StatusOf(store, "a").state == DependencyState::Failed);
    CHECK(StatusOf(store, "b").state == DependencyState::Failed);
}

TEST_CASE("depend: an edge-level failure never recovers") {
    Fresh fresh;
    TestContent content("edge-never");
    content.Add("c", "linked", "c.links", "id:a").Add("a", "linked", "a.links", "probe:y");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    const Asset::Handle<Linked> c = store.Request<Linked>(AssetId("c"));
    executor.RunAll();
    CHECK(StatusOf(store, "c").state == DependencyState::Failed);
    const Asset::Handle<Probe> y = store.Request<Probe>(AssetId("y"));
    CHECK(y.RequestFailure() != nullptr);
    executor.RunAll();
    CHECK(StatusOf(store, "c").state == DependencyState::Failed);
}

TEST_CASE("depend: a parent that fails to decode loads none of its dependencies") {
    Fresh fresh;
    TestContent content("parent-fails");
    content.Add("p", "linked", "p.links", "probe:z").Add("z", "probe", "z.probe", "z");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    TheScript().FailNext("p");
    const Asset::Handle<Linked> p = store.Request<Linked>(AssetId("p"));
    executor.RunAll();
    CHECK(FailureOf(store, "p") == AssetLoadFailureCode::LoaderFailed);
    CHECK_MESSAGE(StatusOf(store, "p").state == DependencyState::Failed,
                  "[as-snapshot-root-failed] a root that failed itself is not Failed with its dependencies");
    CHECK(StateOf(store, "z") == AssetState::NotRequested);
    CHECK(store.Counts().references == 0);
}

TEST_CASE("depend: an AssetRef holds nothing; a Handle requested with it does") {
    Fresh fresh;
    TestContent content("ref-not-holding");
    content.Add("p", "linked", "p.links", "probe:d1\nprobe:d2").Add("d1", "probe", "d1.probe", "1");
    content.Add("d2", "probe", "d2.probe", "2");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    std::optional<Asset::Handle<Linked>> p = store.Request<Linked>(AssetId("p"));
    executor.RunAll();
    const std::shared_ptr<const Linked> value = store.Read(*p);
    REQUIRE(value != nullptr);
    const Asset::Handle<Probe> d2 = store.Request(Asset::AssetRef<Probe>(value->references[1]));
    p.reset();
    store.Pump();
    CHECK(StateOf(store, "p") == AssetState::NotRequested);
    CHECK_MESSAGE(StateOf(store, "d1") == AssetState::NotRequested, "a dependency outlived its parent");
    CHECK(StateOf(store, "d2") == AssetState::Loaded);
    CHECK(store.Read(d2) != nullptr);
}
