// The store at the store layer, without a World (declaration 5.4 to 5.8, section 10 "引用計數與救回",
// "在飛上限", "BadSettings 的兩條路", "(B)5", "持鎖不做的事", "原始位元組雜湊", AS-10 (b) (c); step 2 of
// section 9). Executors are the controlled one (X-5) unless a case needs real threads.
#include "StoreSupport.hpp"

#include "engine/base/Sha256.hpp"

using namespace AssetTest;
using Asset::AssetId;
using Asset::AssetLoadFailureCode;
using Asset::AssetState;

TEST_CASE("store: a request is refused at the request level without the lock and without an info") {
    Fresh fresh;
    TestContent content("request-level");
    content.Add("x", "probe", "x.probe", "x");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);

    const Asset::Handle<Probe> unknown = store.Request<Probe>(AssetId("nope"));
    REQUIRE(unknown.RequestFailure() != nullptr);
    CHECK(unknown.RequestFailure()->code == AssetLoadFailureCode::UnknownId);
    CHECK(unknown.Life() == 0);
    // No loader is registered: the type name alone decides (section 10 "型別名").
    const Asset::Handle<Linked> mismatch = store.Request<Linked>(AssetId("x"));
    REQUIRE(mismatch.RequestFailure() != nullptr);
    CHECK(mismatch.RequestFailure()->code == AssetLoadFailureCode::TypeMismatch);
    CHECK(mismatch.Id() == AssetId("x"));
    CHECK_MESSAGE(StateOf(store, "x") == AssetState::NotRequested, "a request-level failure touched the info");
    const auto counts = store.Counts();
    CHECK(counts.requests == 2);
    CHECK(counts.requestFailures == 2);
    CHECK(executor.Submitted() == 0);
    CheckEquations(counts);
}

TEST_CASE("store: a request-level Handle holds no core") {
    Fresh fresh;
    TestContent content("request-level-no-core");
    content.Add("x", "probe", "x.probe", "x-bytes");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);

    const Asset::Handle<Probe> held = store.Request<Probe>(AssetId("x")); // "World A"
    executor.RunAll();
    {
        const Asset::Handle<Linked> wrong = store.Request<Linked>(AssetId("x")); // "World B", wrong type
        CHECK(wrong.RequestFailure() != nullptr);
    }
    store.Pump();
    CHECK_MESSAGE(store.Read(held) != nullptr, "dropping a request-level Handle removed the asset another holds");
    CHECK(store.Probe(AssetId("x")).dropsToSkip == 0);
    CHECK(store.Counts().removals == 0);
}

TEST_CASE("store: a request after the last Handle went and before maintenance revives the asset") {
    Fresh fresh;
    TestContent content("revive");
    content.Add("x", "probe", "x.probe", "x-bytes");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);

    std::shared_ptr<const Probe> first;
    {
        const Asset::Handle<Probe> handle = store.Request<Probe>(AssetId("x"));
        executor.RunAll();
        first = store.Read(handle);
        REQUIRE(first != nullptr);
    }
    const Asset::Handle<Probe> again = store.Request<Probe>(AssetId("x"));
    CHECK(store.Probe(AssetId("x")).dropsToSkip == 1);
    CHECK(store.Read(again) == first);
    CHECK(store.Counts().revives == 1);
    CHECK(store.Counts().fileReads == 1);
    store.Pump();
    // The drop of the old core used up the skip: the asset stays.
    CHECK_MESSAGE(store.Read(again) == first, "the value went after a revival");
    CHECK(store.Probe(AssetId("x")).dropsToSkip == 0);
    CHECK(store.Counts().dropsSkipped == 1);
    CHECK(store.Counts().fileReads == 1);
    CheckEquations(store.Counts());
}

TEST_CASE("store: a revived asset dropped again is removed") {
    Fresh fresh;
    TestContent content("revive-drop");
    content.Add("x", "probe", "x.probe", "x-bytes");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    {
        const Asset::Handle<Probe> handle = store.Request<Probe>(AssetId("x"));
        executor.RunAll();
    }
    {
        const Asset::Handle<Probe> revived = store.Request<Probe>(AssetId("x"));
        CHECK(store.Probe(AssetId("x")).dropsToSkip == 1);
    }
    store.Pump(); // two drops: the first uses up the skip, the second removes
    const auto probe = store.Probe(AssetId("x"));
    CHECK_MESSAGE(probe.state == AssetState::NotRequested, "a revived asset dropped again was kept");
    CHECK(probe.life == 2);
    CHECK(store.Counts().dropsSkipped == 1);
    CHECK(store.Counts().removals == 1);
    CheckEquations(store.Counts());
}

TEST_CASE("store: a request after maintenance starts a new life and reads again") {
    Fresh fresh;
    TestContent content("new-life");
    content.Add("x", "probe", "x.probe", "x-bytes");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    {
        const Asset::Handle<Probe> handle = store.Request<Probe>(AssetId("x"));
        CHECK(handle.Life() == 1);
        executor.RunAll();
    }
    store.Pump();
    const Asset::Handle<Probe> again = store.Request<Probe>(AssetId("x"));
    CHECK(again.Life() == 2);
    CHECK(StateOf(store, "x") == AssetState::Queued);
    executor.RunAll();
    CHECK(store.Read(again) != nullptr);
    CHECK(store.Counts().fileReads == 2);
    CHECK(store.Counts().newLives == 2);
    CheckEquations(store.Counts());
}

TEST_CASE("store: two requests of an id not loaded queue one job") {
    Fresh fresh;
    TestContent content("one-job");
    content.Add("x", "probe", "x.probe", "x-bytes");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    const Asset::Handle<Probe> first = store.Request<Probe>(AssetId("x"));
    const Asset::Handle<Probe> second = store.Request<Probe>(AssetId("x"));
    CHECK(first == second);
    CHECK(executor.Submitted() == 1);
    CHECK(store.Counts().hits == 1);
    executor.RunAll();
    CHECK(store.Read(first) == store.Read(second));
    CHECK(store.Counts().fileReads == 1);
    CheckEquations(store.Counts());
}

TEST_CASE("store: the result of an old life arriving late is cancelled") {
    Fresh fresh;
    TestContent content("late-result");
    content.Add("x", "probe", "x.probe", "x-bytes");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    TheScript().FailNext("x"); // the old job's result is a failure, the new one's a value
    TheScript().Gate("x");

    std::optional<Asset::Handle<Probe>> old = store.Request<Probe>(AssetId("x"));
    std::thread oldJob = executor.StartNext();
    REQUIRE(TheScript().WaitForCalls(1));
    CHECK(StateOf(store, "x") == AssetState::Loading);
    old.reset();
    store.Pump(); // removed while its job is decoding
    CHECK(StateOf(store, "x") == AssetState::NotRequested);
    const Asset::Handle<Probe> fresh2 = store.Request<Probe>(AssetId("x")); // new life; its job is held
    TheScript().Open("x");
    oldJob.join();
    // After the old job finished and before the new one runs:
    const AssetState between = StateOf(store, "x");
    CHECK_MESSAGE((between == AssetState::Queued || between == AssetState::Loading),
                  "the old life's result was written into the new life");
    CHECK(store.Counts().jobsCancelled == 1);
    executor.RunAll();
    CHECK(StateOf(store, "x") == AssetState::Loaded);
    REQUIRE(store.Read(fresh2) != nullptr);
    CHECK(store.Read(fresh2)->Bytes() == "x-bytes");
    CheckEquations(store.Counts());
}

TEST_CASE("store: a Failed asset loads again when requested; BadSettings stays Failed") {
    Fresh fresh;
    TestContent content("failed-reload");
    content.Add("x", "probe", "x.probe");
    content.Add("bad", "probe", "bad.probe", "b", std::string(R"({"bad":1})"));
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);

    const Asset::Handle<Probe> first = store.Request<Probe>(AssetId("x"));
    executor.RunAll();
    CHECK(FailureOf(store, "x") == AssetLoadFailureCode::NotFound);
    content.WriteFile("x.probe", "x-bytes");
    const Asset::Handle<Probe> second = store.Request<Probe>(AssetId("x")); // first still held
    CHECK(StateOf(store, "x") == AssetState::Queued);
    CHECK(store.Counts().hitsReloading == 1);
    executor.RunAll();
    CHECK(store.Read(first) != nullptr);
    CHECK(store.Probe(AssetId("x")).attempts == 2);
    const auto failures = store.Snapshot().Failures();
    CHECK(failures.empty());

    const Asset::Handle<Probe> bad = store.Request<Probe>(AssetId("bad"));
    CHECK(FailureOf(store, "bad") == AssetLoadFailureCode::BadSettings);
    const auto queued = store.Counts().jobsQueued;
    const Asset::Handle<Probe> badAgain = store.Request<Probe>(AssetId("bad"));
    CHECK(FailureOf(store, "bad") == AssetLoadFailureCode::BadSettings);
    CHECK_MESSAGE(store.Counts().jobsQueued == queued, "a BadSettings entry queued a job");
    CheckEquations(store.Counts());
}

TEST_CASE("store: BadSettings both ways") {
    Fresh fresh;
    TestContent content("bad-settings");
    content.Add("a", "image", "a.png", "a", std::string(R"({"bad":true})"));
    content.Add("b", "image", "b.png", "b", std::string(R"({"bad":true})"));
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);

    SUBCASE("(a) preregistered, waiting, then a loader whose settings fail") {
        REQUIRE(store.Loaders().Preregister<PngLoader>({"png"}));
        const Asset::Handle<Image> handle = store.Request<Image>(AssetId("a"));
        CHECK(StateOf(store, "a") == AssetState::WaitingForLoader);
        CHECK(store.Loaders().Register<PngLoader>());
        CHECK(FailureOf(store, "a") == AssetLoadFailureCode::BadSettings);
    }
    SUBCASE("(b) NoLoader, then a loader whose settings fail, then a request") {
        const Asset::Handle<Image> handle = store.Request<Image>(AssetId("b"));
        CHECK(FailureOf(store, "b") == AssetLoadFailureCode::NoLoader);
        CHECK(store.Loaders().Register<PngLoader>());
        CHECK(FailureOf(store, "b") == AssetLoadFailureCode::NoLoader);
        const Asset::Handle<Image> again = store.Request<Image>(AssetId("b"));
        CHECK(FailureOf(store, "b") == AssetLoadFailureCode::BadSettings);
    }
    CHECK_MESSAGE(store.Counts().jobsQueued == 0, "BadSettings queued a job");
    CHECK(executor.Submitted() == 0);
    CheckEquations(store.Counts());
}

TEST_CASE("store: a loader can come after a request failed with NoLoader") {
    Fresh fresh;
    TestContent content("late-loader");
    content.Add("x", "probe", "x.probe", "x-bytes");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    const Asset::Handle<Probe> first = store.Request<Probe>(AssetId("x"));
    CHECK(FailureOf(store, "x") == AssetLoadFailureCode::NoLoader);
    REQUIRE(store.Loaders().Register<ProbeLoader>());
    CHECK(FailureOf(store, "x") == AssetLoadFailureCode::NoLoader); // judged again only on a request
    const Asset::Handle<Probe> second = store.Request<Probe>(AssetId("x"));
    CHECK(StateOf(store, "x") == AssetState::Queued);
    executor.RunAll();
    CHECK(store.Read(first) != nullptr);
    CheckEquations(store.Counts());
}

TEST_CASE("store: preregistration makes requests wait; formats the loader does not take become NoLoader") {
    Fresh fresh;
    TestContent content("preregister");
    content.Add("p", "image", "p.png", "p").Add("j", "image", "j.jpg", "j");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    REQUIRE(store.Loaders().Preregister<PngLoader>({"png", "jpg"}));
    REQUIRE(store.Loaders().Preregister<PngLoader>({"png", "jpg"})); // the same loader type again
    const Asset::Handle<Image> png = store.Request<Image>(AssetId("p"));
    const Asset::Handle<Image> jpg = store.Request<Image>(AssetId("j"));
    CHECK(StateOf(store, "p") == AssetState::WaitingForLoader);
    CHECK(StateOf(store, "j") == AssetState::WaitingForLoader);
    CHECK(store.Snapshot().WaitingForLoader().size() == 2);
    CHECK(store.Counts().newLivesWaiting == 2);

    REQUIRE(store.Loaders().Register<PngLoader>()); // takes png only
    CHECK(StateOf(store, "p") == AssetState::Queued);
    CHECK_MESSAGE(FailureOf(store, "j") == AssetLoadFailureCode::NoLoader,
                  "a waiter of a format the loader does not take still waits");
    executor.RunAll();
    CHECK(store.Read(png) != nullptr);
    // Another loader may still take jpg.
    CHECK(store.Loaders().Register<JpgLoader>());
    const Asset::Handle<Image> jpgAgain = store.Request<Image>(AssetId("j"));
    executor.RunAll();
    CHECK(store.Read(jpg) != nullptr);
    CheckEquations(store.Counts());
}

TEST_CASE("store: preregistration conflicts in three directions") {
    Fresh fresh;
    TestContent content("preregister-conflicts");
    content.Add("p", "image", "p.png", "p");
    ControlledExecutor executor;

    SUBCASE("a loader registers a pair another loader type preregistered") {
        Asset::AssetStore store(content.Entries(), content.Options(), &executor);
        REQUIRE(store.Loaders().Preregister<PngLoader>({"png"}));
        const Asset::Handle<Image> waiting = store.Request<Image>(AssetId("p"));
        const auto refused = store.Loaders().Register<OtherPngLoader>();
        REQUIRE_FALSE_MESSAGE(refused, "a loader registered a pair another loader type preregistered");
        CHECK(refused.error().code == Asset::LoaderRegistrationFailureCode::FormatPreregistered);
        // The failed registration leaves the preregistration and its waiter.
        CHECK(StateOf(store, "p") == AssetState::WaitingForLoader);
        REQUIRE(store.Loaders().Register<PngLoader>());
        CHECK(StateOf(store, "p") == AssetState::Queued);
    }
    SUBCASE("a loader type preregisters a pair another loader has") {
        Asset::AssetStore store(content.Entries(), content.Options(), &executor);
        REQUIRE(store.Loaders().Register<PngLoader>());
        const auto refused = store.Loaders().Preregister<OtherPngLoader>({"png"});
        REQUIRE_FALSE_MESSAGE(refused, "a loader type preregistered a pair another loader has");
        CHECK(refused.error().code == Asset::LoaderRegistrationFailureCode::FormatTaken);
        CHECK(store.Loaders().Preregister<PngLoader>({"png"})); // its own pair
    }
    SUBCASE("two loader types preregister one pair") {
        Asset::AssetStore store(content.Entries(), content.Options(), &executor);
        REQUIRE(store.Loaders().Preregister<PngLoader>({"png"}));
        const auto refused = store.Loaders().Preregister<OtherPngLoader>({"gif", "png"});
        REQUIRE_FALSE_MESSAGE(refused, "two loader types preregistered one pair");
        CHECK(refused.error().code == Asset::LoaderRegistrationFailureCode::FormatPreregistered);
        // Nothing of the refused list was recorded: gif has no preregistration.
        REQUIRE_MESSAGE(store.Loaders().Preregister<PngLoader>({"gif"}), "a refused preregistration recorded part of its list");
    }
}

TEST_CASE("store: registration refuses a taken pair and a taken type name; the same loader again is idempotent") {
    Fresh fresh;
    TestContent content("registration");
    content.Add("t", "text", "t.txt", "hello");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    REQUIRE(store.Loaders().Register<Asset::TextLoader>());
    REQUIRE_MESSAGE(store.Loaders().Register<Asset::TextLoader>(), "registering the same loader again failed");
    const auto taken = store.Loaders().Register<OtherTextLoader>();
    REQUIRE_FALSE_MESSAGE(taken, "a second loader took a pair another loader has");
    CHECK(taken.error().code == Asset::LoaderRegistrationFailureCode::FormatTaken);
    REQUIRE(store.Loaders().Register<GyoTextLoader>()); // another format of the same type
    REQUIRE(store.Loaders().Register<ProbeLoader>());
    const auto twin = store.Loaders().Register<ProbeTwinLoader>();
    REQUIRE_FALSE_MESSAGE(twin, "a second C++ type took a type name");
    CHECK(twin.error().code == Asset::LoaderRegistrationFailureCode::TypeNameTaken);
    const Asset::Handle<Asset::Text> text = store.Request<Asset::Text>(AssetId("t"));
    executor.RunAll();
    REQUIRE(store.Read(text) != nullptr);
    CHECK(store.Read(text)->Bytes() == "hello"); // the text loader, not the refused one
}

TEST_CASE("store: the same id moved to an engine format gives the same value") {
    Fresh fresh;
    TestContent original("format-original");
    original.Add("t", "text", "t.txt", "hello");
    TestContent converted("format-converted");
    converted.Add("t", "text", "t.bin", "GYOTXT\nhello", std::nullopt, std::string("gyotxt"));
    ControlledExecutor executor;
    std::string values[2];
    int index = 0;
    for (const TestContent* content : {&original, &converted}) {
        Asset::AssetStore store(content->Entries(), content->Options(), &executor);
        REQUIRE(store.Loaders().Register<Asset::TextLoader>());
        REQUIRE(store.Loaders().Register<GyoTextLoader>());
        const Asset::Handle<Asset::Text> handle = store.Request<Asset::Text>(AssetId("t")); // the same code
        executor.RunAll();
        REQUIRE(store.Read(handle) != nullptr);
        values[index++] = store.Read(handle)->Bytes();
    }
    CHECK(values[0] == values[1]);
}

TEST_CASE("store: the in-flight limit keeps request order and passes on the slot of a job removed before it starts") {
    Fresh fresh;
    TestContent content("in-flight");
    for (const char* id : {"a", "b", "c", "d", "e"}) content.Add(id, "probe", std::string(id) + ".probe", id);
    ControlledExecutor executor;
    Asset::AssetStoreOptions options = content.Options();
    options.maxInFlight = 2;
    Asset::AssetStore store(content.Entries(), options, &executor);
    RegisterTestLoaders(store);

    std::map<std::string, std::optional<Asset::Handle<Probe>>> handles;
    for (const char* id : {"a", "b", "c", "d", "e"}) handles[id] = store.Request<Probe>(AssetId(id));
    CHECK(executor.Pending() == 2);
    CHECK(store.Counts().jobsWaiting == 3);
    // a's job is in the executor, not started; a is removed and requested again.
    handles["a"].reset();
    store.Pump();
    handles["a"] = store.Request<Probe>(AssetId("a"));
    CHECK(executor.RunNext()); // a's old job: cancelled before it reads, and gives its slot to c
    CHECK_MESSAGE(store.Counts().fileReads == 0, "a job removed before it started read its file");
    CHECK(store.Counts().jobsCancelled == 1);
    CHECK(executor.Pending() == 2); // b, c
    executor.RunAll();
    CHECK(TheScript().Calls() == std::vector<std::string>{"b", "c", "d", "e", "a"});
    CHECK(store.Counts().fileReads == 5);
    CHECK(Idle(store.Counts()));
    CheckEquations(store.Counts());
}

TEST_CASE("store: a cancelled job keeps its slot until it finishes; a dependency gets a slot") {
    Fresh fresh;
    TestContent content("in-flight-cancelled");
    content.Add("a", "probe", "a.probe", "a").Add("b", "probe", "b.probe", "b");
    content.Add("p", "linked", "p.links", "probe:d").Add("d", "probe", "d.probe", "d");
    ControlledExecutor executor;
    Asset::AssetStoreOptions options = content.Options();
    options.maxInFlight = 1;
    Asset::AssetStore store(content.Entries(), options, &executor);
    RegisterTestLoaders(store);
    TheScript().Gate("a");
    std::optional<Asset::Handle<Probe>> a = store.Request<Probe>(AssetId("a"));
    const Asset::Handle<Probe> b = store.Request<Probe>(AssetId("b"));
    std::thread running = executor.StartNext();
    REQUIRE(TheScript().WaitForCalls(1));
    a.reset();
    store.Pump(); // removed while decoding
    CHECK(executor.Pending() == 0);
    CHECK(store.Counts().jobsInFlight == 1);
    TheScript().Open("a");
    running.join();
    CHECK(store.Counts().jobsCancelled == 1);
    CHECK(executor.Pending() == 1); // b, only now
    executor.RunAll();
    const Asset::Handle<Linked> parent = store.Request<Linked>(AssetId("p"));
    executor.RunAll();
    CHECK(StateOf(store, "d") == AssetState::Loaded);
    CHECK(Idle(store.Counts()));
    CheckEquations(store.Counts());
}

TEST_CASE("store: the in-flight limit holds with real threads") {
    Fresh fresh;
    TestContent content("in-flight-threads");
    std::vector<std::string> ids{"a", "b", "c", "d", "e"};
    for (const std::string& id : ids) content.Add(id, "probe", id + ".probe", id);
    PoolExecutor executor(4);
    Asset::AssetStoreOptions options = content.Options();
    options.maxInFlight = 2;
    Asset::AssetStore store(content.Entries(), options, &executor);
    RegisterTestLoaders(store);
    for (const std::string& id : ids) TheScript().Gate(id);
    std::vector<Asset::Handle<Probe>> handles;
    for (const std::string& id : ids) handles.push_back(store.Request<Probe>(AssetId(id)));
    REQUIRE(TheScript().WaitForCalls(2));
    std::this_thread::sleep_for(50ms); // a third start would show here
    CHECK(TheScript().Calls().size() == 2);
    for (const std::string& id : ids) TheScript().Open(id);
    REQUIRE(WaitIdle(store));
    CHECK(TheScript().Peak() <= 2);
    CHECK(store.Counts().jobsLoaded == 5);
    CheckEquations(store.Counts());
    store.Shutdown(Asset::AssetShutdown::Discard);
    executor.Stop();
}

TEST_CASE("store: a job decoding does not block a request") {
    Fresh fresh;
    TestContent content("decoding-does-not-block");
    content.Add("slow", "probe", "slow.probe", "s").Add("other", "probe", "other.probe", "o");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    TheScript().Gate("slow");
    const Asset::Handle<Probe> slow = store.Request<Probe>(AssetId("slow"));
    std::thread job = executor.StartNext();
    REQUIRE(TheScript().WaitForCalls(1));

    std::mutex mutex;
    std::condition_variable done;
    bool returned = false;
    std::thread requester([&] {
        const Asset::Handle<Probe> other = store.Request<Probe>(AssetId("other"));
        const std::lock_guard guard(mutex);
        returned = true;
        done.notify_all();
    });
    bool inTime = false;
    {
        std::unique_lock lock(mutex);
        inTime = done.wait_for(lock, kBound, [&] { return returned; });
    }
    CHECK_MESSAGE(inTime, "a request waited for a job that was decoding");
    // Always let go and join, so a failure here is this check and not a hang.
    TheScript().Open("slow");
    job.join();
    requester.join();
    executor.RunAll();
    CHECK(store.Read(slow) != nullptr);
}

TEST_CASE("store: the store holds no lock while it submits, destroys a value or reads settings") {
    Fresh fresh;
    TestContent content("no-lock-callbacks");
    content.Add("x", "probe", "x.probe", "x-bytes");
    content.Add("p", "linked", "p.links", "probe:d").Add("d", "probe", "d.probe", "d");
    content.Add("t", "text", "t.gyo", "GYOTXT\nt", std::string(R"({"k":1})"), std::string("gyotxt"));
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    TheHooks().store = &store;
    executor.onSubmit = [] { CallStoreFromHook("executor Submit"); };

    SetHookLabel("request");
    std::optional<Asset::Handle<Probe>> x = store.Request<Probe>(AssetId("x"));
    executor.onSubmit = nullptr;
    executor.RunAll();

    executor.onSubmit = [] { CallStoreFromHook("executor Submit"); };
    const Asset::Handle<Linked> parent = store.Request<Linked>(AssetId("p"));
    SetHookLabel("completion");
    executor.RunAll(); // p's job submits d's job when it finishes
    executor.onSubmit = nullptr;
    CHECK(StateOf(store, "d") == AssetState::Loaded);

    SetHookLabel("maintenance");
    TheHooks().destructor = true;
    const int destroyed = TheDestructions().count.load();
    x.reset();
    store.Pump();
    CHECK(TheDestructions().count.load() == destroyed + 1);
    TheHooks().destructor = false;

    SetHookLabel("registration");
    TheHooks().settings = true;
    REQUIRE(store.Loaders().Register<GyoTextLoader>());
    TheHooks().settings = false;
    TheHooks().store = nullptr;
}

TEST_CASE("store: the source digest is the SHA-256 of the bytes read, or not computed") {
    Fresh fresh;
    TestContent content("digest");
    content.Add("a", "probe", "a.probe", "same").Add("b", "probe", "b.probe", "same").Add("c", "probe", "c.probe",
                                                                                           "samf");
    ControlledExecutor executor;
    SUBCASE("on") {
        Asset::AssetStore store(content.Entries(), content.Options(), &executor);
        RegisterTestLoaders(store);
        std::vector<Asset::Handle<Probe>> handles;
        for (const char* id : {"a", "b", "c"}) handles.push_back(store.Request<Probe>(AssetId(id)));
        executor.RunAll();
        const auto snapshot = store.Snapshot();
        const std::string same = "same";
        const std::string expected =
            GYO::Engine::Base::Sha256({reinterpret_cast<const std::byte*>(same.data()), same.size()});
        REQUIRE(snapshot.Find(AssetId("a"))->sourceDigest.has_value());
        CHECK(*snapshot.Find(AssetId("a"))->sourceDigest == expected);
        CHECK(snapshot.Find(AssetId("a"))->sourceDigest == snapshot.Find(AssetId("b"))->sourceDigest);
        CHECK(snapshot.Find(AssetId("a"))->sourceDigest != snapshot.Find(AssetId("c"))->sourceDigest);
    }
    SUBCASE("off") {
        Asset::AssetStoreOptions options = content.Options();
        options.sourceDigest = false;
        Asset::AssetStore store(content.Entries(), options, &executor);
        RegisterTestLoaders(store);
        const Asset::Handle<Probe> handle = store.Request<Probe>(AssetId("a"));
        executor.RunAll();
        CHECK(store.Read(handle) != nullptr);
        CHECK_MESSAGE(!store.Snapshot().Find(AssetId("a"))->sourceDigest.has_value(),
                      "the digest is computed although it is off");
    }
}

TEST_CASE("store: id-level failures of a job carry their codes") {
    Fresh fresh;
    TestContent content("job-failures");
    content.Add("missing", "probe", "missing.probe");
    content.Add("large", "probe", "large.probe", std::string(2000, 'x'));
    content.Add("limited", "text", "limited.txt", std::string(20, 'x'));
    content.Add("told", "probe", "told.probe", "t");
    content.Add("throws", "probe", "throws.probe", "throw");
    content.Add("notutf8", "text", "notutf8.txt", "\xff");
    ControlledExecutor executor;
    Asset::AssetStoreOptions options = content.Options();
    options.byteLimits.push_back(Asset::AssetByteLimit{"text", "txt", 10});
    Asset::AssetStore store(content.Entries(), options, &executor);
    RegisterTestLoaders(store);
    TheScript().FailNext("told");
    std::vector<Asset::Handle<Probe>> probes;
    for (const char* id : {"missing", "large", "told", "throws"}) probes.push_back(store.Request<Probe>(AssetId(id)));
    const Asset::Handle<Asset::Text> limited = store.Request<Asset::Text>(AssetId("limited"));
    const Asset::Handle<Asset::Text> notUtf8 = store.Request<Asset::Text>(AssetId("notutf8"));
    executor.RunAll();
    CHECK(FailureOf(store, "missing") == AssetLoadFailureCode::NotFound);
    CHECK(FailureOf(store, "large") == AssetLoadFailureCode::TooLarge);
    CHECK_MESSAGE(FailureOf(store, "limited") == AssetLoadFailureCode::TooLarge, "the byte limit override is ignored");
    CHECK(FailureOf(store, "told") == AssetLoadFailureCode::LoaderFailed);
    CHECK(FailureOf(store, "throws") == AssetLoadFailureCode::LoaderThrew);
    CHECK(FailureOf(store, "notutf8") == AssetLoadFailureCode::LoaderFailed);
    const auto failures = store.Snapshot().Failures();
    CHECK(failures.size() == 6);
    for (const Asset::FailureRecord& failure : failures) CHECK(failure.attempt == 1);
    CHECK(store.Counts().jobsFailed == 6);
    CheckEquations(store.Counts());
}

TEST_CASE("store: without an executor a request that needs a job is NoExecutor") {
    Fresh fresh;
    TestContent content("no-executor");
    content.Add("x", "probe", "x.probe", "x").Add("img", "image", "img.png", "i");
    content.Add("p", "linked", "p.links", "probe:x");
    Asset::AssetStore store(content.Entries(), content.Options(), nullptr);
    RegisterTestLoaders(store);
    const Asset::Handle<Probe> x = store.Request<Probe>(AssetId("x"));
    CHECK(FailureOf(store, "x") == AssetLoadFailureCode::NoExecutor);
    CHECK(store.Counts().fileReads == 0);
    const Asset::Handle<Probe> again = store.Request<Probe>(AssetId("x"));
    CHECK(FailureOf(store, "x") == AssetLoadFailureCode::NoExecutor);
    // An entry with no loader is still NoLoader.
    const Asset::Handle<Image> image = store.Request<Image>(AssetId("img"));
    CHECK(FailureOf(store, "img") == AssetLoadFailureCode::NoLoader);
    const auto counts = store.Counts();
    CHECK(counts.newLivesNoExecutor == 1);
    CHECK(counts.jobsQueued == 0);
    CheckEquations(counts);
}

TEST_CASE("store: Shutdown discards or drains") {
    Fresh fresh;
    TestContent content("shutdown");
    content.Add("a", "probe", "a.probe", "a").Add("b", "probe", "b.probe", "b").Add("c", "probe", "c.probe", "c");
    SUBCASE("discard") {
        ControlledExecutor executor;
        Asset::AssetStoreOptions options = content.Options();
        options.maxInFlight = 2;
        Asset::AssetStore store(content.Entries(), options, &executor);
        RegisterTestLoaders(store);
        std::vector<Asset::Handle<Probe>> handles;
        for (const char* id : {"a", "b", "c"}) handles.push_back(store.Request<Probe>(AssetId(id)));
        executor.RunNext(); // a loads
        store.Shutdown(Asset::AssetShutdown::Discard);
        auto counts = store.Counts();
        CHECK(counts.jobsDiscarded == 2); // b and c, submitted and not run
        CHECK(counts.jobsLoaded == 1);
        executor.RunAll(); // b's job finds the store shut down
        CHECK(store.Counts().jobsLoaded == 1);
        CHECK(store.Counts().fileReads == 1);
        CheckEquations(store.Counts());
        CHECK(StateOf(store, "a") == AssetState::NotRequested);
        handles.clear(); // pushes go nowhere
        store.Pump();
        CHECK(store.Counts().dropsTaken == 0);
    }
    SUBCASE("drain") {
        TheScript().Gate("a");
        std::thread opener([] {
            std::this_thread::sleep_for(100ms);
            TheScript().Open("a");
        });
        PoolExecutor executor(2);
        Asset::AssetStoreOptions options = content.Options();
        options.maxInFlight = 1;
        Asset::AssetStore store(content.Entries(), options, &executor);
        RegisterTestLoaders(store);
        std::vector<Asset::Handle<Probe>> handles;
        for (const char* id : {"a", "b", "c"}) handles.push_back(store.Request<Probe>(AssetId(id)));
        store.Shutdown(Asset::AssetShutdown::Drain);
        const auto counts = store.Counts();
        CHECK(counts.jobsLoaded == 3);
        CHECK(counts.jobsDiscarded == 0);
        CheckEquations(counts);
        executor.Stop();
        opener.join();
    }
}

TEST_CASE("store: the last holder of a value on another thread destroys it, once") {
    Fresh fresh;
    TestContent content("destroy-elsewhere");
    content.Add("x", "probe", "x.probe", "x-bytes");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    std::shared_ptr<const Probe> value;
    {
        const Asset::Handle<Probe> handle = store.Request<Probe>(AssetId("x"));
        executor.RunAll();
        value = store.Read(handle);
    }
    store.Pump();
    CHECK(StateOf(store, "x") == AssetState::NotRequested);
    const int before = TheDestructions().count.load();
    std::thread::id other;
    std::thread holder([&value, &other] {
        other = std::this_thread::get_id();
        value.reset();
    });
    holder.join();
    CHECK(TheDestructions().count.load() == before + 1);
    const std::lock_guard guard(TheDestructions().mutex);
    CHECK(TheDestructions().lastThread == other);
}

TEST_CASE("store: a Handle outlives its store") {
    Fresh fresh;
    TestContent content("handle-outlives");
    content.Add("x", "probe", "x.probe", "x-bytes");
    ControlledExecutor executor;
    std::optional<Asset::Handle<Probe>> handle;
    {
        Asset::AssetStore store(content.Entries(), content.Options(), &executor);
        RegisterTestLoaders(store);
        handle = store.Request<Probe>(AssetId("x"));
        executor.RunAll();
    }
    const Asset::Handle<Probe> copy = *handle;
    CHECK(copy == *handle);
    handle.reset(); // nothing to push to
    CHECK(copy.Id() == AssetId("x"));
}

TEST_CASE("store: the version changes with every info and a pull is skipped when it did not") {
    Fresh fresh;
    TestContent content("version");
    content.Add("x", "probe", "x.probe", "x-bytes");
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    const std::uint64_t start = store.Version();
    CHECK_FALSE(store.PullIfChanged(start).has_value());
    std::optional<Asset::Handle<Probe>> handle = store.Request<Probe>(AssetId("x"));
    const std::uint64_t requested = store.Version();
    CHECK(requested != start);
    executor.RunAll();
    const std::uint64_t loaded = store.Version();
    CHECK(loaded != requested);
    const auto pulled = store.PullIfChanged(requested);
    REQUIRE(pulled.has_value());
    CHECK(pulled->Version() == loaded);
    CHECK(pulled->State(AssetId("x")) == AssetState::Loaded);
    CHECK_MESSAGE(!store.PullIfChanged(loaded).has_value(), "a pull with the current version copied the infos");
    handle.reset();
    store.Pump();
    CHECK_MESSAGE(store.Version() != loaded, "removing an info did not change the version");
}

TEST_CASE("store: a value of another C++ type with the same type name reads as null") {
    Fresh fresh;
    TestContent content("same-type-name");
    content.Add("twin", "probe", "twin.bin", "t", std::nullopt, std::string("twin"));
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    REQUIRE(store.Loaders().Register<ProbeTwinLoader>()); // only ProbeTwin has a loader for "probe"
    const Asset::Handle<Probe> handle = store.Request<Probe>(AssetId("twin"));
    CHECK(handle.RequestFailure() == nullptr);
    executor.RunAll();
    CHECK(StateOf(store, "twin") == AssetState::Loaded);
    CHECK_MESSAGE(store.Read(handle) == nullptr, "a value read as a C++ type it is not");
    CHECK(store.Snapshot().Find(AssetId("twin"))->Value<ProbeTwin>() != nullptr);
}
