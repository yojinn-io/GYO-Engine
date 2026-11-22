// Serial Executor guards of the world-exec batch: spec J-6 (Running half),
// J-26, J-29, J-30, J-32b, J-35, J-45, J-50, J-52, J-53, and the three run-time
// halves handed over by registry-schedule (declaration section 3 item 2):
// J-11 full against incremental rebuild (W-11), J-12 Fetch has no order
// dependence (spec B.4 point 7), J-44 the World summary (D-9).
// target/kernel DI-KS 2, 6, 9, 12 (B-12 half), 14; spec E.2, E.5, X-1 to X-5,
// F.2 steps 0 and 2, LIM-6.

#include "WorldTestSupport.hpp"

#include "engine/base/IdIssuer.hpp"

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace GYO::Test::Kernel;

std::string Join(const std::vector<std::string>& parts) {
    std::string out;
    for (const std::string& part : parts) out += (out.empty() ? "" : " ") + part;
    return out;
}

constexpr bool kNdebug =
#if defined(NDEBUG)
    true;
#else
    false;
#endif

// ---- J-6 (W-6)

// The Host's code reached while a schedule runs, with no System of this World
// current on the thread (as on a thread a System started itself: spec R-03).
SystemResult ReadWithoutCurrentCall(Query<Read<Tag>>) {
    Detail::SystemContext* saved = Detail::CurrentThreadCall();
    Detail::SetCurrentThreadCall(nullptr);
    struct Restore {
        Detail::SystemContext* call;
        ~Restore() { Detail::SetCurrentThreadCall(call); }
    } restore{saved};
    static_cast<void>(KeptWorld()->Get<Tag>(1));
    return Ok();
}

SystemResult Idle(Query<Read<Tag>>) { return Ok(); }

} // namespace

TEST_CASE("J-6: a World access while Running with no System current fails a GYO_ASSERT; the Host writes any data in Ready") {
    {
        // Verification off: the always-on GYO_ASSERT alone must stop it.
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&ReadWithoutCurrentCall>(B("ReadWithoutCurrentCall", "A"));
        }, false);
        rig->world().Insert(1, Tag{1});
        KeptWorld() = &rig->world();
        GYO_CHECK_ASSERTS(rig->Run());
        KeptWorld() = nullptr;
    }
    {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&Idle>(B("Idle", "A"));
        });
        World& world = rig->world();
        world.Insert(1, Pos{1});  // before the first Run: the initial content
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        // No run-time interception (v8-arch-gyo2-no-runtime-interception): zone C state too
        // after the first Run (W-6b is gone); boundary (role Input) and zone B data as before.
        world.Insert(1, Boundary{1});
        world.Insert(1, Tag{1});
        world.SetResource(Out{3});
        world.Insert(1, Pos{2});
        world.SetResource(CRes{4});
        CHECK_MESSAGE((world.Get<Pos>(1)->x == 2 && world.Resource<CRes>()->value == 4),
                      "[ks-s8-host-zone-c] the Host's zone C writes after the first Run were not made");
        KERNEL_REQUIRE_RUN_OK(rig->Run());
    }
}

// ---- J-6: a thread-bound resource (ResourceOptions::threadBound; v8-arch-gyo2-thread-binding-by-runner)

namespace {

struct Bound { std::int64_t value; };  // resource, zone B, thread-bound
void HashValue(Hasher& h, const Bound& v) { HashValue(h, v.value); }

// One thread kept alive for a whole case, so the thread a value was put in on
// can still use it, remove it or destroy the World.
class Worker {
public:
    Worker() : thread_([this] { Loop(); }) {}
    ~Worker() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        wake_.notify_all();
        thread_.join();
    }
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    // Runs task on the worker and waits for it.
    void Do(std::function<void()> task) {
        std::unique_lock<std::mutex> lock(mutex_);
        task_ = std::move(task);
        wake_.notify_all();
        done_.wait(lock, [this] { return !task_; });
    }

private:
    void Loop() {
        std::unique_lock<std::mutex> lock(mutex_);
        for (;;) {
            wake_.wait(lock, [this] { return stop_ || task_; });
            if (task_) {
                task_();
                task_ = nullptr;
                done_.notify_all();
            } else if (stop_) {
                return;
            }
        }
    }

    std::mutex mutex_;
    std::condition_variable wake_;
    std::condition_variable done_;
    std::function<void()> task_;
    bool stop_ = false;
    std::thread thread_;
};

// Whether f fails a GYO_ASSERT on this thread.
bool Asserts(const std::function<void()>& f) {
    ::GYO::Test::ScopedAssertionHandler scope;
    try {
        f();
    } catch (const ::GYO::Engine::Base::AssertionFailure&) {
        return true;
    }
    return false;
}

SystemResult ReadsBound(Res<Bound> bound) {
    Log().push_back("bound " + std::to_string(bound->value));
    return Ok();
}
SystemResult WritesBound(ResMut<Bound> bound) {
    bound->value += 1;
    return Ok();
}
SystemResult ExclusiveReadsBound(ExclusiveWorld<> world) {
    auto value = world.Read<Bound>([](const Bound& b) { return b.value; });
    if (!value) return GYO::Engine::Base::Err(std::move(value).error());
    return Ok();
}
SystemResult CommandsSetsBound(Commands commands) {
    commands.SetResource(Bound{4});
    return Ok();
}
SystemResult ExclusiveSetsBound(ExclusiveWorld<> world) {
    if (auto set = world.SetResource(Bound{5}); !set) return GYO::Engine::Base::Err(std::move(set).error());
    return Ok();
}
SystemResult CommandsRemovesBound(Commands commands) {
    commands.RemoveResource<Bound>();
    return Ok();
}
SystemResult ExclusiveRemovesBound(ExclusiveWorld<> world) {
    if (auto removed = world.RemoveResource<Bound>(); !removed) return GYO::Engine::Base::Err(std::move(removed).error());
    return Ok();
}
struct Free { std::int64_t value; };  // resource, zone B, not thread-bound

std::uint64_t BoundThread(World& world) {
    return Detail::ResourceAt(world, Detail::ResourceIdOf(world, KeyOf<Bound>())).thread;
}

std::unique_ptr<Rig> BoundRig(void (*add)(Registrar&)) {
    return MakeRig([add](Registrar& r) {
        Data(r);
        r.Resource<Bound>({.name = "Bound", .threadBound = true});
        if (add != nullptr) add(r);
    }, false);
}

} // namespace

TEST_CASE("J-6: a thread-bound resource is used on the thread it was put in on; after a removal another thread may put it in") {
    auto rig = BoundRig([](Registrar& r) {
        r.AddSystem<&ReadsBound>(B("ReadsBound", "A"));
        r.AddSystem<&ExclusiveReadsBound>(B("ExclusiveReadsBound", "B"));
    });
    rig->world().SetResource(Bound{1});
    Log().clear();
    CHECK_MESSAGE(!Asserts([&] { KERNEL_REQUIRE_RUN_OK(rig->Run()); }), "J-6 thread-bound: the same thread runs it");
    CHECK(Log() == std::vector<std::string>{"bound 1"});
    CHECK_MESSAGE(!Asserts([&] { static_cast<void>(rig->world().Summary()); }), "J-6 thread-bound: the same thread reads the summary");
    Worker worker;
    rig->world().RemoveResource<Bound>();
    bool asserted = false;
    worker.Do([&] { asserted = Asserts([&] { rig->world().SetResource(Bound{2}); }); });
    // Judged here, on this thread: when the thread was not forgotten the worker's put-in failed a GYO_ASSERT.
    REQUIRE_MESSAGE(!asserted, "[ks-s10-thread-forgotten] after World::RemoveResource the thread was kept: another thread could not put it in");
    worker.Do([&] { rig->world().RemoveResource<Bound>(); });
    CHECK_MESSAGE(!Asserts([&] { rig->world().SetResource(Bound{3}); }),
                  "J-6 thread-bound: after a removal the thread is forgotten and another thread puts it in");
    CHECK(rig->world().Resource<Bound>()->value == 3);
}

// The stored thread is read back on the same thread here; other threads are
// the touched cases' and the other-direction cases'.
TEST_CASE("J-6: a thread-bound resource put in by a sync point's Commands::SetResource or by ExclusiveWorld takes the applying thread") {
    SUBCASE("Commands::SetResource, applied at the sync point") {
        auto rig = BoundRig([](Registrar& r) {
            r.AddSystem<&CommandsSetsBound>(B("CommandsSetsBound", "A"));
            r.AddSystem<&ReadsBound>(B("ReadsBound", "B"));
        });
        Log().clear();
        CHECK_MESSAGE(!Asserts([&] { KERNEL_REQUIRE_RUN_OK(rig->Run()); }), "J-6 thread-bound: the applying thread reads it");
        CHECK(Log() == std::vector<std::string>{"bound 4"});
        CHECK_MESSAGE(BoundThread(rig->world()) == GYO::Engine::Base::CurrentThreadSerial(), "[ks-s6-thread-bound] the sync point did not record its thread");
    }
    SUBCASE("ExclusiveWorld::SetResource, now") {
        auto rig = BoundRig([](Registrar& r) {
            r.AddSystem<&ExclusiveSetsBound>(B("ExclusiveSetsBound", "A"));
            r.AddSystem<&ReadsBound>(B("ReadsBound", "B"));
        });
        Log().clear();
        CHECK_MESSAGE(!Asserts([&] { KERNEL_REQUIRE_RUN_OK(rig->Run()); }), "J-6 thread-bound: the applying thread reads it");
        CHECK(Log() == std::vector<std::string>{"bound 5"});
        CHECK_MESSAGE(BoundThread(rig->world()) == GYO::Engine::Base::CurrentThreadSerial(), "[ks-s5-thread-bound] the immediate change did not record its thread");
    }
}

// The same thread here, as above: the stored thread is read back.
TEST_CASE("J-6: a thread-bound resource taken out by a sync point's Commands::RemoveResource or by ExclusiveWorld forgets its thread") {
    SUBCASE("Commands::RemoveResource, applied at the sync point") {
        auto rig = BoundRig([](Registrar& r) { r.AddSystem<&CommandsRemovesBound>(B("CommandsRemovesBound", "A")); });
        rig->world().SetResource(Bound{1});
        REQUIRE(BoundThread(rig->world()) == GYO::Engine::Base::CurrentThreadSerial());
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK(rig->world().Resource<Bound>() == nullptr);
        CHECK_MESSAGE(BoundThread(rig->world()) == 0, "[ks-s6-thread-forgotten] the sync point's removal kept the thread");
    }
    SUBCASE("ExclusiveWorld::RemoveResource, now") {
        auto rig = BoundRig([](Registrar& r) { r.AddSystem<&ExclusiveRemovesBound>(B("ExclusiveRemovesBound", "A")); });
        rig->world().SetResource(Bound{1});
        REQUIRE(BoundThread(rig->world()) == GYO::Engine::Base::CurrentThreadSerial());
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK(rig->world().Resource<Bound>() == nullptr);
        CHECK_MESSAGE(BoundThread(rig->world()) == 0, "[ks-s6-thread-forgotten] the immediate removal kept the thread");
    }
}

TEST_CASE("J-6: a thread-bound resource touched on another thread fails a GYO_ASSERT") {
    struct Touch {
        std::string what;
        void (*add)(Registrar&);
        std::function<void(Rig&)> touch;
    };
    const std::vector<Touch> touches{
        {"Res in a System", [](Registrar& r) { r.AddSystem<&ReadsBound>(B("ReadsBound", "A")); },
         [](Rig& rig) { static_cast<void>(rig.Run()); }},
        {"ResMut in a System", [](Registrar& r) { r.AddSystem<&WritesBound>(B("WritesBound", "A")); },
         [](Rig& rig) { static_cast<void>(rig.Run()); }},
        {"an ExclusiveWorld read", [](Registrar& r) { r.AddSystem<&ExclusiveReadsBound>(B("ExclusiveReadsBound", "A")); },
         [](Rig& rig) { static_cast<void>(rig.Run()); }},
        {"World::Resource", nullptr, [](Rig& rig) { static_cast<void>(rig.world().Resource<Bound>()); }},
        {"World::ResourceMut", nullptr, [](Rig& rig) { static_cast<void>(rig.world().ResourceMut<Bound>()); }},
        {"an overwrite with World::SetResource", nullptr, [](Rig& rig) { rig.world().SetResource(Bound{9}); }},
        {"World::RemoveResource", nullptr, [](Rig& rig) { rig.world().RemoveResource<Bound>(); }},
        {"World::Summary", nullptr, [](Rig& rig) { static_cast<void>(rig.world().Summary()); }},
        // The resource step of a sync point and of an immediate change (S6, S5).
        {"Commands::SetResource at a sync point", [](Registrar& r) { r.AddSystem<&CommandsSetsBound>(B("CommandsSetsBound", "A")); },
         [](Rig& rig) { static_cast<void>(rig.Run()); }},
        {"Commands::RemoveResource at a sync point", [](Registrar& r) { r.AddSystem<&CommandsRemovesBound>(B("CommandsRemovesBound", "A")); },
         [](Rig& rig) { static_cast<void>(rig.Run()); }},
        {"ExclusiveWorld::SetResource", [](Registrar& r) { r.AddSystem<&ExclusiveSetsBound>(B("ExclusiveSetsBound", "A")); },
         [](Rig& rig) { static_cast<void>(rig.Run()); }},
        {"ExclusiveWorld::RemoveResource", [](Registrar& r) { r.AddSystem<&ExclusiveRemovesBound>(B("ExclusiveRemovesBound", "A")); },
         [](Rig& rig) { static_cast<void>(rig.Run()); }},
    };
    for (const Touch& touch : touches) {
        CAPTURE(touch.what);
        Worker worker;
        std::unique_ptr<Rig> rig = BoundRig(touch.add);
        worker.Do([&] { rig->world().SetResource(Bound{1}); });
        const bool asserted = Asserts([&] { touch.touch(*rig); });
        CHECK_MESSAGE(asserted, "J-6 thread-bound: " << touch.what << " on another thread did not assert");
        if (asserted) {
            // The thread that put it in destroys the World (destroying it elsewhere would assert too).
            worker.Do([&] { rig.reset(); });
        } else {
            // The touch may have taken the resource to this thread: no thread can destroy it without an assertion.
            static_cast<void>(rig.release());
        }
    }
}

TEST_CASE("J-6: the other direction: a thread-bound value asserts on a thread that took its serial later, also when the thread that put it in has ended") {
    SUBCASE("put in on this thread, touched on a worker started afterwards") {
        std::unique_ptr<Rig> rig = BoundRig(nullptr);
        rig->world().SetResource(Bound{1});  // this thread has its serial by now; the worker takes a later one
        Worker worker;
        bool asserted = false;
        worker.Do([&] { asserted = Asserts([&] { static_cast<void>(rig->world().Resource<Bound>()); }); });
        CHECK_MESSAGE(asserted, "[ks-s10-later-thread] J-6 thread-bound: a worker started after the value was put in read it without an assertion");
    }
    SUBCASE("put in on a worker that has ended, touched on a worker started afterwards") {
        // The case a thread_local address missed: the second worker may get the first one's storage.
        std::unique_ptr<Rig> rig = BoundRig(nullptr);
        {
            Worker first;
            first.Do([&] { rig->world().SetResource(Bound{1}); });
        }
        Worker second;
        bool asserted = false;
        second.Do([&] { asserted = Asserts([&] { static_cast<void>(rig->world().Resource<Bound>()); }); });
        CHECK_MESSAGE(asserted, "[ks-s10-ended-thread] J-6 thread-bound: a worker started after the binding worker ended read it without an assertion");
        // The thread that put it in has ended: no thread can destroy the World without an assertion.
        static_cast<void>(rig.release());
    }
}

TEST_CASE("J-6: a resource not marked threadBound is used on any thread") {
    std::unique_ptr<Rig> rig = MakeRig([](Registrar& r) {
        Data(r);
        r.Resource<Free>({.name = "Free"});
    }, false);
    Worker worker;
    bool asserted = false;
    worker.Do([&] { asserted = Asserts([&] { rig->world().SetResource(Free{1}); }); });
    CHECK_MESSAGE(!asserted, "[ks-s10-unbound-free] J-6: a worker's put-in of a resource that is not thread-bound asserted");
    bool here = Asserts([&] {
        static_cast<void>(rig->world().Resource<Free>());
        rig->world().SetResource(Free{2});
        static_cast<void>(rig->world().Summary());
    });
    CHECK_MESSAGE(!here, "[ks-s10-unbound-free] J-6: reading, overwriting or summarizing on this thread a resource a worker put in asserted");
    bool removed = false;
    worker.Do([&] { removed = Asserts([&] { rig->world().RemoveResource<Free>(); rig->world().SetResource(Free{3}); }); });
    CHECK_MESSAGE(!removed, "[ks-s10-unbound-free] J-6: a worker's removal and put-in of a resource this thread wrote asserted");
    // Destroying the World checks every thread-bound value: only when nothing asserted is it safe here.
    if (asserted || here || removed) static_cast<void>(rig.release());
}

TEST_CASE("J-6: the thread mark is not World data: the same value put in on two threads gives one summary") {
    std::unique_ptr<Rig> here = BoundRig(nullptr);
    std::unique_ptr<Rig> there = BoundRig(nullptr);
    here->world().SetResource(Bound{7});
    const std::string hereSummary = SummaryOf(here->world()).Text();
    Worker worker;
    std::string thereSummary;
    worker.Do([&] {
        there->world().SetResource(Bound{7});
        // No REQUIRE off the test thread: an error is carried back as text.
        const auto summary = there->world().Summary();
        thereSummary = summary ? summary->Text() : "error: " + summary.error().Text();
    });
    CHECK_MESSAGE(hereSummary == thereSummary,
                  "[ks-s10-mark-not-data] J-6: two Worlds with the same value put in on two threads have different summaries");
    worker.Do([&] { there.reset(); });
}

// ---- J-26 (X-1)

namespace {

SystemResult LogFirst(Query<Read<Tag>>) {
    Log().push_back("First");
    return Ok();
}
SystemResult LogSecond(Query<Read<Tag>>) {
    Log().push_back("Second");
    return Ok();
}
SystemResult LogSkipped(Query<Read<Tag>>) {
    Log().push_back("Skipped");
    return Ok();
}
SystemResult LogThird(Query<Read<Tag>>) {
    Log().push_back("Third");
    return Ok();
}
bool Never() { return false; }

} // namespace

TEST_CASE("J-26: the Serial call sequence is the Plan's System steps without those whose condition is false") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&LogThird>(B("Third", "C"));
        r.AddSystem<&LogSecond>(B("Second", "B"));
        SystemOptions skipped = B("Skipped", "B", {}, {"Second"});
        skipped.mounts[0].conditions.push_back(Condition<&Never>("Never"));
        r.AddSystem<&LogSkipped>(std::move(skipped));
        r.AddSystem<&LogFirst>(B("First", "A"));
    });
    const Plan* plan = rig->built.registry->Frozen().PlanOf("Tick");
    REQUIRE(plan != nullptr);
    std::vector<std::string> expected;
    for (const std::string& name : SystemSteps(*plan)) {
        if (name != "Skipped") expected.push_back(name);
    }
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(expected == std::vector<std::string>{"First", "Second", "Third"});
    CHECK(Log() == expected);
}

// ---- J-29 (X-4)

namespace {

SystemResult ToggleFlag(ResMut<Flag> flag, Res<TickCount> tick) {
    flag->on = tick->value == 1 ? 1 : 0;
    return Ok();
}
bool FlagOn(Res<Flag> flag) { return flag->on != 0; }
SystemResult Guarded(Query<Read<Tag>>, Res<TickCount> tick) {
    Log().push_back("guarded@" + std::to_string(tick->value));
    return Ok();
}
bool CountedCondition(Res<Out>) {
    Log().push_back("condition");
    return true;
}
SystemResult MemberOne(Query<Read<Tag>>) { return Ok(); }
SystemResult MemberTwo(Query<Read<Tag>>) { return Ok(); }

} // namespace

TEST_CASE("J-29: a run condition sees the step before it, is evaluated every run, and once per set member") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&ToggleFlag>(B("ToggleFlag", "A"));
        SystemOptions guarded = B("Guarded", "B");
        guarded.mounts[0].conditions.push_back(Condition<&FlagOn>("FlagOn"));
        r.AddSystem<&Guarded>(std::move(guarded));
        r.Set({.name = "Counted", .schedule = "Tick"}, {Condition<&CountedCondition>("CountedCondition")});
        SystemOptions one = B("MemberOne", "C");
        one.mounts[0].sets = {"Counted"};
        SystemOptions two = B("MemberTwo", "C");
        two.mounts[0].sets = {"Counted"};
        r.AddSystem<&MemberOne>(std::move(one));
        r.AddSystem<&MemberTwo>(std::move(two));
    });
    rig->world().SetResource(Flag{0});
    rig->world().SetResource(Out{0});
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Join(Log()) == "guarded@1 condition condition condition condition");
}

// ---- J-30 (X-5, W-11)

namespace {
SystemResult SeeAdded(Query<Read<Tag>, Added<Tag>> added) {
    Log().push_back("added:" + std::to_string(added.Count()));
    return Ok();
}
} // namespace

// tickStamps (a tick's first stamp) left with the clock, not moved (G5: no kernel judgment read them).
TEST_CASE("J-30: Added sees new cells only") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&SeeAdded>(B("SeeAdded", "A"));
    });
    rig->world().Insert(1, Tag{1});
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    rig->world().GetMut<Tag>(1)->t = 2;  // changed, not added
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Join(Log()) == "added:1 added:0");
}

namespace {

bool NeverRuns(Res<Flag>) { return false; }
SystemResult SkippedWriter(Query<Write<Tag>>) { return Ok(); }
SystemResult WriteTagOne(Query<Write<Tag>> tags) {
    if (Mut<Tag> tag = tags.GetMut<Tag>(1)) tag->t += 1;
    return Ok();
}
SystemResult ReadTagTwo(Query<Read<Tag>> tags) {
    static_cast<void>(tags.Get<Tag>(2));
    return Ok();
}

std::vector<ChangeStamp> StampsOfTwoRuns() {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        SystemOptions skipped = B("SkippedWriter", "A");
        skipped.mounts[0].conditions.push_back(Condition<&NeverRuns>("NeverRuns"));
        r.AddSystem<&SkippedWriter>(std::move(skipped));
        r.AddSystem<&WriteTagOne>(B("WriteTagOne", "B"));
        r.AddSystem<&ReadTagTwo>(B("ReadTagTwo", "B", {}, {"WriteTagOne"}));
    });
    World& world = rig->world();
    world.SetResource(Flag{0});
    world.Insert(1, Tag{0});
    world.Insert(2, Tag{0});
    std::vector<ChangeStamp> stamps{world.Stamp()};
    for (int run = 0; run < 2; ++run) {
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        stamps.push_back(world.Stamp());
        stamps.push_back(world.Stamps<Tag>(1)->second);
        stamps.push_back(world.Stamps<Tag>(2)->second);
    }
    return stamps;
}

} // namespace

TEST_CASE("J-30: a skipped System takes its stamp; the same history gives the same stamps; writes mark, reads do not") {
    // Plan: SkippedWriter, sync A, WriteTagOne, ReadTagTwo, sync B, sync C = 6 steps a Run.
    const std::vector<ChangeStamp> first = StampsOfTwoRuns();
    // A Host write in Ready carries the stamp the next step takes (1 here).
    CHECK(first == std::vector<ChangeStamp>{0, 6, 3, 1, 12, 9, 1});
    CHECK(StampsOfTwoRuns() == first);
}

// ---- J-32b (untyped Commands; a kernel Err the System drops)

namespace {

SystemResult UntypedIntoShared(Commands commands) {
    commands.Insert(1, Tag{5});
    return Ok();
}
// No run-time interception (S8): untyped Commands into any column are recorded and
// applied like any other.
SystemResult UntypedIntoAnyColumn(Commands commands) {
    commands.Insert(1, Pos{7});
    commands.Insert(1, Mode{8});
    commands.Remove<Pos>(2);
    return Ok();
}
// Spawn::New on a used-up allotter is a kernel Err (W-9). Drops it and returns
// another one: both are reported.
SystemResult DroppedBesideOwn(Spawn<Bundle<Tag>> spawn) {
    static_cast<void>(spawn.New(Tag{1}));
    return GYO::Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "own"));
}
// Drops the Err it got: the Executor reports it all the same.
SystemResult SpawnDropped(Spawn<Bundle<Tag>> spawn) {
    static_cast<void>(spawn.New(Tag{1}));
    return Ok();
}

void CheckErrOnce(Rig& rig, KernelErrorCode code, const char* rule) {
    const auto result = rig.Run();
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(result), "expected Err " << std::string(ToString(code)));
    CHECK_MESSAGE(result.error().Has(code), result.error().Text());
    bool ruled = false;
    for (const KernelError* leaf : result.error().Leaves()) ruled = ruled || leaf->rule == rule;
    CHECK(ruled);
    CHECK(result.error().world == "test-world");
    CHECK(rig.world().State() == WorldState::Interrupted);
    CHECK(rig.recorded.size() == 1);
}

} // namespace

// The F-10 and Z5 halves went with the run-time interception
// (v8-arch-gyo2-no-runtime-interception). A kernel Err the System drops is still
// reported (CallError lists it in call.returned): the trigger is now Spawn::New
// on a used-up allotter (W-9), the only kernel Err a plain System's call returns.
TEST_CASE("J-32b: untyped Commands write any column; a kernel Err the System drops is still reported") {
    MESSAGE("NDEBUG (release-like target): " << std::string(kNdebug ? "defined" : "not defined"));
    SUBCASE("a dropped Err is still reported") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r, true);
            r.AddSystem<&SpawnDropped>(B("SpawnDropped", "A"));
        }, false);
        rig->world().SetResource(TestRangeAllotter::Of(1, 0));  // used up
        CheckErrOnce(*rig, KernelErrorCode::AllotterExhausted, "W-9");
    }
    SUBCASE("a dropped Err beside the System's own Err") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r, true);
            r.AddSystem<&DroppedBesideOwn>(B("DroppedBesideOwn", "A"));
        }, false);
        rig->world().SetResource(TestRangeAllotter::Of(1, 0));  // used up
        const auto result = rig->Run();
        REQUIRE_FALSE(static_cast<bool>(result));
        CHECK(result.error().Has(KernelErrorCode::AllotterExhausted));
        CHECK(result.error().Has(KernelErrorCode::SystemFailed));
    }
    SUBCASE("untyped Commands into a column: applied") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&UntypedIntoShared>(B("UntypedIntoShared", "A"));
        }, false);
        rig->world().Insert(1, Mode{0});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        REQUIRE(rig->world().Get<Tag>(1) != nullptr);
        CHECK(rig->world().Get<Tag>(1)->t == 5);
        CHECK(rig->recorded.empty());
    }
    SUBCASE("any column: each column is applied") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&UntypedIntoAnyColumn>(B("UntypedIntoAnyColumn", "A"));
        }, false);
        rig->world().Insert(1, Tag{0});
        rig->world().Insert(2, Pos{2});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE((rig->world().Get<Pos>(1) != nullptr && rig->world().Get<Pos>(1)->x == 7),
                      "J-32b any column: the untyped insert was not applied (Pos)");
        CHECK_MESSAGE((rig->world().Get<Mode>(1) != nullptr && rig->world().Get<Mode>(1)->m == 8),
                      "J-32b any column: the untyped insert was not applied (Mode)");
        CHECK_MESSAGE(rig->world().Get<Pos>(2) == nullptr, "J-32b any column: the untyped remove was not applied (Pos)");
        CHECK(rig->recorded.empty());
    }
}

// ---- J-35 (E.5, X-9)

namespace {

SystemResult SpawnThenThrow(SpawnWithId<Bundle<Tag>> spawn, DiagnosticsWriter<Note> notes) {
    spawn.New(5, Tag{5});
    notes.Write(Note{1});
    throw std::runtime_error("System threw");
}
SystemResult SpawnThenErr(SpawnWithId<Bundle<Tag>> spawn, DiagnosticsWriter<Note> notes) {
    spawn.New(5, Tag{5});
    notes.Write(Note{2});
    return GYO::Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "the System's own Err"));
}

} // namespace

namespace {
SystemResult SpawnTagOne(SpawnWithId<Bundle<Tag>> spawn) {
    spawn.New(1, Tag{1});
    return Ok();
}
SystemResult HookReturnsErr(HookCtx) {
    return GYO::Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "the hook's own Err"));
}
SystemResult HookWritesThroughReadPointer(HookCtx, Query<Read<Pos>> pos) {
    if (const Pos* p = pos.Get<Pos>(2)) const_cast<Pos*>(p)->x = 64;
    return Ok();
}
} // namespace

TEST_CASE("J-35: a hook returns Err: the World is interrupted and the leaf keeps its code and rule") {
    SUBCASE("a hook returns its own Err") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&SpawnTagOne>(B("SpawnTagOne", "A"));
            r.Hook<Tag, &HookReturnsErr>(HookTrigger::OnAdd, {.name = "HookReturnsErr"});
        }, false);
        const auto result = rig->Run();
        CHECK(HasCode(result, KernelErrorCode::SystemFailed));
        REQUIRE_FALSE(static_cast<bool>(result));
        CHECK(result.error().Text().find("hook 'HookReturnsErr'") != std::string::npos);
        CHECK(rig->world().State() == WorldState::Interrupted);
        CHECK(rig->recorded.size() == 1);
    }
    SUBCASE("a hook writes through a read pointer: caught by the comparison around the hook") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&SpawnTagOne>(B("SpawnTagOne", "A"));
            r.Hook<Tag, &HookWritesThroughReadPointer>(HookTrigger::OnAdd, {.name = "HookWritesThroughReadPointer"});
        }, true);
        rig->world().Insert(2, Pos{2});
        const auto result = rig->Run();
        CHECK(HasCode(result, KernelErrorCode::BoundaryChange));
        REQUIRE_FALSE(static_cast<bool>(result));
        CHECK(result.error().Text().find("hook 'HookWritesThroughReadPointer'") != std::string::npos);
    }
}

TEST_CASE("J-35: a throw, a System Err or a kernel-level Err interrupt the World: records dropped, diagnostics kept, no further Run") {
    SUBCASE("a System throws") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&SpawnThenThrow>(B("SpawnThenThrow", "A"));
        }, false);
        CHECK_THROWS_AS(static_cast<void>(rig->Run()), std::runtime_error);
        CHECK(rig->world().State() == WorldState::Interrupted);
        CHECK_FALSE(rig->world().Alive(5));
        REQUIRE(rig->world().Diagnostics<Note>().size() == 1);
        CHECK(rig->world().Diagnostics<Note>()[0].code == 1);
        GYO_CHECK_ASSERTS(rig->Run());
    }
    SUBCASE("a System returns Err") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&SpawnThenErr>(B("SpawnThenErr", "A"));
        }, false);
        const auto result = rig->Run();
        CHECK(HasCode(result, KernelErrorCode::SystemFailed));
        CHECK(rig->world().State() == WorldState::Interrupted);
        CHECK(rig->recorded.size() == 1);
        CHECK_FALSE(rig->world().Alive(5));
        REQUIRE(rig->world().Diagnostics<Note>().size() == 1);
        CHECK(rig->world().Diagnostics<Note>()[0].code == 2);
        GYO_CHECK_ASSERTS(rig->Run());
    }
    SUBCASE("the comparison before the first step finds a Host write through a kept pointer") {
        // The first stage is empty: the first step is a sync point, whose snapshot must not hide the write.
        auto build = [](Registrar& r) {
            Data(r);
            r.AddSystem<&Idle>(B("Idle", "B"));
        };
        auto rig = MakeRig(build, true);
        rig->world().Insert(1, Tag{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        const Tag* kept = rig->world().Get<Tag>(1);
        const_cast<Tag*>(kept)->t = 42;  // not through the World API
        const auto result = rig->Run();
        CHECK(HasCode(result, KernelErrorCode::PreCallChange));
        REQUIRE_FALSE(static_cast<bool>(result));
        CHECK(result.error().Text().find("Ready window") != std::string::npos);
        CHECK(rig->world().State() == WorldState::Interrupted);
        GYO_CHECK_ASSERTS(rig->Run());
        GYO_CHECK_ASSERTS(rig->world().Insert(2, Tag{2}));  // an Interrupted World takes no Host write
        // Restore arrives with rollback-extract; rebuilding the World (a new WorldExecutor) is the other way back.
        WorldExecutor rebuilt(*rig->built.registry, "rebuilt");
        KERNEL_REQUIRE_RUN_OK(rebuilt.Run("Tick"));
    }
}

// ---- J-45 (ExclusiveWorld writes any value)

namespace {

SystemResult ExclusiveWritesPos(ExclusiveWorld<> world) {
    auto r = world.Modify<Pos>(1, [](Pos& pos) { pos.x += 1; });
    if (!r) return GYO::Engine::Base::Err(std::move(r).error());
    return Ok();
}
SystemResult ExclusiveWritesMode(ExclusiveWorld<> world) {
    auto r = world.Modify<Mode>(1, [](Mode& mode) { mode.m += 1; });
    if (!r) return GYO::Engine::Base::Err(std::move(r).error());
    return Ok();
}
SystemResult ExclusiveWritesCShared(ExclusiveWorld<> world) {
    auto r = world.Modify<CShared>(1, [](CShared& value) { value.s += 1; });
    if (!r) return GYO::Engine::Base::Err(std::move(r).error());
    return Ok();
}
SystemResult ExclusiveAllowed(ExclusiveWorld<> world) {
    auto pos = world.Read<Pos>(1, [](const Pos& p) { return p.x; });
    if (!pos) return GYO::Engine::Base::Err(std::move(pos).error());
    if (!*pos) return Ok();
    const std::int32_t x = **pos;
    auto tag = world.Modify<Tag>(1, [x](Tag& t) { t.t = x + 1; });
    if (!tag) return GYO::Engine::Base::Err(std::move(tag).error());
    return Ok();
}

SystemResult ExclusiveSpawns(ExclusiveWorld<> world) {
    auto id = world.Spawn(Tag{3});
    if (!id) return GYO::Engine::Base::Err(std::move(id).error());
    auto tag = world.Read<Tag>(*id, [](const Tag& t) { return t.t; });
    if (!tag) return GYO::Engine::Base::Err(std::move(tag).error());
    Log().push_back("spawned " + std::to_string(*id) + " tag " + std::to_string(tag->value_or(-1)));
    return Ok();
}

std::unique_ptr<Rig> ExclusiveRig(void (*add)(Registrar&), bool verification) {
    auto rig = MakeRig([add](Registrar& r) {
        Data(r);
        add(r);
    }, verification);
    rig->world().Insert(1, Pos{7});
    rig->world().Insert(1, Mode{1});
    rig->world().Insert(1, Tag{0});
    return rig;
}

} // namespace

// ExclusiveWorld reads and writes any component and resource value
// (v8-arch-gyo2-exclusive-writes-optional, DI-KS 1) and spawns with a type
// (v8-arch-gyo2-immediate-change-via-exclusive-world, DI-KS 18). Any System may
// hold it since S-13 left with the zone rules (v8-arch-gyo2-authority-rollback step 2).
TEST_CASE("J-45: ExclusiveWorld writes any component value, in every build") {
    MESSAGE("NDEBUG (release-like target): " << std::string(kNdebug ? "defined" : "not defined"));
    SUBCASE("a write of Pos") {
        auto rig = ExclusiveRig([](Registrar& r) { r.AddSystem<&ExclusiveWritesPos>(B("X", "A")); }, false);
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(rig->world().Get<Pos>(1)->x == 8, "J-45 written: Pos");
    }
    SUBCASE("a write of Mode") {
        auto rig = ExclusiveRig([](Registrar& r) { r.AddSystem<&ExclusiveWritesMode>(B("X", "A")); }, false);
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(rig->world().Get<Mode>(1)->m == 2, "J-45 written: Mode");
    }
    SUBCASE("a write of CShared") {
        auto rig = ExclusiveRig([](Registrar& r) { r.AddSystem<&ExclusiveWritesCShared>(B("X", "A")); }, false);
        rig->world().Insert(1, CShared{0});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(rig->world().Get<CShared>(1)->s == 1, "J-45 written: CShared");
    }
    SUBCASE("spawn: the typed Spawn gives an id from the allotter, and the entity is there in the call") {
        auto rig = ExclusiveRig([](Registrar& r) {
            r.Resource<TestRangeAllotter>({.name = "Allotter"});
            r.Allotter<TestRangeAllotter>();
            r.AddSystem<&ExclusiveSpawns>(B("X", "A"));
        }, false);
        rig->world().SetResource(TestRangeAllotter::Of(10, 19));
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(Log() == std::vector<std::string>{"spawned 10 tag 3"}, "J-45 spawn: " << (Log().empty() ? std::string("nothing") : Log().front()));
        CHECK(rig->world().Get<Tag>(10)->t == 3);
    }
    SUBCASE("a write and a read pass, also with verification on") {
        auto rig = ExclusiveRig([](Registrar& r) { r.AddSystem<&ExclusiveAllowed>(B("X", "A")); }, true);
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK(rig->world().Get<Tag>(1)->t == 8);
    }
}

// ---- J-50 (LIM-6; KernelSettings read when each Run starts)

namespace {

SystemResult ReadUndeclared(Query<Read<Tag>>) {
    static_cast<void>(KeptWorld()->Get<Pos>(1));
    return Ok();
}
SystemResult ReadUndeclaredWhenKept(Query<Read<Tag>>) {
    if (KeptWorld() != nullptr) static_cast<void>(KeptWorld()->Get<Pos>(1));
    return Ok();
}
// Turns access verification on (KernelSettings is an ordinary resource: D-E18 is gone).
SystemResult TurnVerificationOn(ResMut<KernelSettings> settings) {
    settings->accessVerification = true;
    return Ok();
}

} // namespace

// KernelSettings is read once when each WorldExecutor::Run call starts
// (v8-arch-gyo2-kernel-settings-read-write, DI-KS 24): the Host may write it
// between Runs and a System may declare it; a change counts from the next Run.
TEST_CASE("J-50: KernelSettings is read when each Run starts; the Host writes it between Runs too") {
    {
        auto clean = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&ReadUndeclared>(B("ReadUndeclared", "A"));
        }, false);
        clean->world().Insert(1, Pos{1});
        KeptWorld() = &clean->world();
        KERNEL_REQUIRE_RUN_OK(clean->Run());  // verification off: the undeclared read is not reported
        clean->world().SetResource(KernelSettings{true, false});
        CHECK_FALSE(clean->executor->Verification().accessVerification);
        const auto next = clean->Run();
        KeptWorld() = nullptr;
        CHECK_MESSAGE(HasCode(next, KernelErrorCode::AccessViolation),
                      "J-50 next Run: the Host's write between Runs was not read when the next Run started");
        CHECK(clean->executor->Verification().accessVerification);
    }
    {
        // breakOnKernelError turned on between two Runs: the break hook is called in the next Run.
        auto late = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&ReadUndeclared>(B("ReadUndeclared", "A"));
        }, false);
        late->world().Insert(1, Pos{1});
        int breaks = 0;
        late->world().SetBreakHook([&breaks](const KernelError&) { ++breaks; });
        KeptWorld() = &late->world();
        KERNEL_REQUIRE_RUN_OK(late->Run());
        late->world().SetResource(KernelSettings{true, true});
        const auto next = late->Run();
        KeptWorld() = nullptr;
        CHECK(HasCode(next, KernelErrorCode::AccessViolation));
        CHECK_MESSAGE(breaks > 0, "J-50 next Run: breakOnKernelError written between Runs was not read (break hook calls " << breaks << ")");
        CHECK(late->executor->Verification().breakOnKernelError);
    }

    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&ReadUndeclared>(B("ReadUndeclared", "A"));
    }, false);
    // The Ready window: the last value before the first Run counts.
    rig->world().SetResource(KernelSettings{false, false});
    rig->world().SetResource(KernelSettings{true, false});
    rig->world().Insert(1, Pos{1});
    KeptWorld() = &rig->world();
    const auto result = rig->Run();
    KeptWorld() = nullptr;
    CHECK(HasCode(result, KernelErrorCode::AccessViolation));
    CHECK(rig->executor->Verification().accessVerification);

    // breakOnKernelError: the returning break hook is called where the error is made, then the Err goes on.
    for (const bool breakOn : {true, false}) {
        auto breaking = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&ReadUndeclared>(B("ReadUndeclared", "A"));
        });
        breaking->world().SetResource(KernelSettings{true, breakOn});
        int breaks = 0;
        breaking->world().SetBreakHook([&breaks](const KernelError&) { ++breaks; });
        breaking->world().Insert(1, Pos{1});
        KeptWorld() = &breaking->world();
        const auto broken = breaking->Run();
        KeptWorld() = nullptr;
        CHECK(HasCode(broken, KernelErrorCode::AccessViolation));
        CHECK_MESSAGE((breaks > 0) == breakOn, "breakOnKernelError " << breakOn << ", break hook calls " << breaks);
        CHECK(breaking->executor->Verification().breakOnKernelError == breakOn);
    }
}

TEST_CASE("J-50: a System's change of KernelSettings takes effect at the next Run, not in the Run that made it") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&TurnVerificationOn>(B("TurnVerificationOn", "A"));
        r.AddSystem<&ReadUndeclared>(B("ReadUndeclared", "B"));
    }, false);
    rig->world().Insert(1, Pos{1});
    KeptWorld() = &rig->world();
    const auto first = rig->Run();
    // Judged before the second Run: a failed first Run interrupts the World and the second would assert.
    CHECK_MESSAGE(static_cast<bool>(first), "J-50 same Run: verification turned on in the Run that wrote it: " << Text(first));
    if (first) {
        const auto second = rig->Run();
        CHECK_MESSAGE(HasCode(second, KernelErrorCode::AccessViolation),
                      "J-50 next Run: a System's write of KernelSettings was not read when the next Run started");
    }
    KeptWorld() = nullptr;
}

TEST_CASE("J-50: KernelSettings removed between two Runs: the next Run uses the defaults (verification and break off)") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&ReadUndeclaredWhenKept>(B("ReadUndeclaredWhenKept", "A"));
    }, false);
    rig->world().Insert(1, Pos{1});
    rig->world().SetResource(KernelSettings{true, true});
    int breaks = 0;
    rig->world().SetBreakHook([&breaks](const KernelError&) { ++breaks; });
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // no World kept: nothing undeclared is read yet
    REQUIRE(rig->executor->Verification().accessVerification);
    REQUIRE(rig->executor->Verification().breakOnKernelError);
    rig->world().RemoveResource<KernelSettings>();
    KeptWorld() = &rig->world();
    const auto next = rig->Run();
    KeptWorld() = nullptr;
    CHECK_MESSAGE(!rig->executor->Verification().accessVerification, "J-50 removed: accessVerification kept the last value");
    CHECK_MESSAGE(!rig->executor->Verification().breakOnKernelError, "J-50 removed: breakOnKernelError kept the last value");
    CHECK_MESSAGE(static_cast<bool>(next), "J-50 removed: the undeclared read was reported: " << Text(next));
    CHECK(breaks == 0);
}

TEST_CASE("J-50: verification on, off for two Runs while data changes, on again: no false report") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&WriteTagOne>(B("WriteTagOne", "A"));
    }, true);
    rig->world().Insert(1, Tag{0});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    rig->world().SetResource(KernelSettings{false, false});
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // Tag changes while nothing is compared
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    rig->world().SetResource(KernelSettings{true, false});
    const auto result = rig->Run();
    CHECK_MESSAGE(static_cast<bool>(result),
                  "J-50 flip: the snapshots from before verification was off were compared: " << Text(result));
    CHECK(rig->world().Get<Tag>(1)->t == 4);
}

// ---- J-52 (F.2 steps 0 and 2, B-12)

namespace {

// A zone B hook that despawns the entity whose Tag was added.
SystemResult HookDespawns(HookCtx ctx, Despawn despawn) {
    despawn.Entity(ctx.Entity());
    return Ok();
}
SystemResult InsertTagOnce(CommandsFor<Inserts<Tag>, Removes<>> commands, Res<TickCount> tick) {
    if (tick->value == 1) commands.Insert(1, Tag{1});
    return Ok();
}
SystemResult DespawnsOne(Despawn despawn) {
    despawn.Entity(1);
    return Ok();
}
SystemResult TagHookDespawnsOne(HookCtx, Despawn despawn) {
    despawn.Entity(1);
    return Ok();
}
SystemResult TagsTwo(CommandsFor<Inserts<Tag>, Removes<>> commands) {
    commands.Insert(2, Tag{2});
    return Ok();
}
SystemResult InsertsPosAtTwo(CommandsFor<Inserts<Pos>, Removes<>> commands, Res<TickCount> tick) {
    if (tick->value == 2) commands.Insert(1, Pos{2});
    return Ok();
}

bool FlagIsOn(Res<Flag> flag) { return flag->on != 0; }
SystemResult SeeChangesWhenOn(Query<Read<Tag>, Changed<Tag>> changed) {
    Log().push_back("changed:" + std::to_string(changed.Count()));
    return Ok();
}

// Two mounts: Tick (A) and Other (B). Changed and Local are per mount. Which mount runs: Flag, written by the
// test before each Run (1: Tick), in place of the kernel's clock domain (v8-arch-gyo2-clock-out-of-kernel).
SystemResult SeeChanges(Query<Read<Tag>, Changed<Tag>> changed, Local<std::int32_t> runs, Res<Flag> mount) {
    *runs += 1;
    Log().push_back(std::string(mount->on == 1 ? "A" : "B") + " run" + std::to_string(*runs) + " changed:" +
                    std::to_string(changed.Count()));
    return Ok();
}

} // namespace

// Z5 is gone (S8): a System's or hook's despawn of an entity is recorded and
// applied like any other, whatever columns it holds.
TEST_CASE("J-52: a despawn of an entity with any column is applied (no Z5)") {
    SUBCASE("a System") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&DespawnsOne>(B("DespawnsOne", "A"));
        }, false);
        rig->world().Insert(1, Pos{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(!rig->world().Alive(1), "J-52 entity with Pos: the despawn was not applied (System)");
    }
    SUBCASE("a hook") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.Hook<Tag, &TagHookDespawnsOne>(HookTrigger::OnAdd, {.name = "TagHookDespawnsOne"});
            r.AddSystem<&TagsTwo>(B("TagsTwo", "A"));
        }, false);
        rig->world().Insert(1, Pos{1});
        rig->world().Insert(2, Pos{2});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(!rig->world().Alive(1), "J-52 entity with Pos: the despawn was not applied (hook)");
    }
}

// The Z5 recheck at the sync point went with the run-time interception
// (v8-arch-gyo2-no-runtime-interception); what stays is how a hook's despawn and
// a later insert into the same entity come out, and the per-mount state.
TEST_CASE("J-52: a hook's despawn and a later insert into it; Changed and Local are per mount") {
    SUBCASE("one stage: a hook's despawn at the only sync point is applied by the hook rounds of the same Run") {
        ScheduleOptions one;
        one.name = "Tick";
        one.stages = {"A"};
        auto rig = MakeRig([one](Registrar& r) {
            r.Component<Pos>({.name = "Pos"});
            r.Component<Tag>({.name = "Tag"});
            r.Component<Mode>({.name = "Mode"});
            r.Resource<TickCount>({.name = "TickCount"});
            r.Schedule(one);
            r.AddSystem<&InsertTagOnce>(B("InsertTagOnce", "A"));
            r.AddSystem<&InsertsPosAtTwo>(C("InsertsPosAtTwo", "A"));
            r.Hook<Tag, &HookDespawns>(HookTrigger::OnAdd, {.name = "HookDespawns"});
        }, false);
        rig->world().Insert(1, Mode{0});
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // tick 1: Tag added; the hook's despawn(1) is applied by the hook rounds
        CHECK_FALSE(rig->world().Alive(1));
        const std::uint64_t dropped = rig->world().DroppedMissingEntity();
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // tick 2: the zone C insert into 1 finds no entity: dropped, no Err
        CHECK(rig->world().DroppedMissingEntity() == dropped + 1);
        CHECK(rig->world().Get<Pos>(1) == nullptr);
    }
    SUBCASE("a skipped mount keeps its last-run stamp") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            SystemOptions options = B("SeeChangesWhenOn", "A");
            options.mounts[0].conditions.push_back(Condition<&FlagIsOn>("FlagIsOn"));
            r.AddSystem<&SeeChangesWhenOn>(std::move(options));
        });
        rig->world().SetResource(Flag{0});
        rig->world().Insert(1, Tag{1});
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // skipped
        rig->world().SetResource(Flag{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // runs: the insert made before the skipped run is still a change
        CHECK(Join(Log()) == "changed:1");
    }
    SUBCASE("two mounts keep their own last-run stamp and Local") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            ScheduleOptions other;
            other.name = "Other";
            other.stages = {"B"};
            r.Schedule(other);
            SystemOptions options = Sys("SeeChanges", {At("Tick", "A"), At("Other", "B")});
            r.AddSystem<&SeeChanges>(std::move(options));
        });
        World& world = rig->world();
        world.Insert(1, Tag{0});
        Log().clear();
        world.SetResource(Flag{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run("Tick"));   // A run1: sees the Host's insert
        world.GetMut<Tag>(1)->t = 5;               // after A, before B
        world.SetResource(Flag{2});
        KERNEL_REQUIRE_RUN_OK(rig->Run("Other"));  // B run1: sees the insert and the write
        world.SetResource(Flag{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run("Tick"));   // A run2: still sees the write
        KERNEL_REQUIRE_RUN_OK(rig->Run("Tick"));   // A run3: nothing new
        CHECK(Join(Log()) == "A run1 changed:1 B run1 changed:1 A run2 changed:1 A run3 changed:0");
    }
}

// ---- J-53 (E.2 step 5)

namespace {

const Tag* g_keptTag = nullptr;

SystemResult KeepTag(Query<Read<Tag>> tags) {
    g_keptTag = tags.Get<Tag>(1);
    return Ok();
}
// Returns Ok after writing through a kept pointer it never declared.
SystemResult WriteKeptAndReturnOk(Query<Read<Pos>>) {
    if (g_keptTag != nullptr) const_cast<Tag*>(g_keptTag)->t = 77;
    return Ok();
}
// A false condition that read undeclared data through a kept World.
bool FalseAfterUndeclaredRead(Res<Flag>) {
    static_cast<void>(KeptWorld()->Get<Pos>(1));
    return false;
}
SystemResult NeverCalled(Query<Read<Tag>>) {
    Log().push_back("called");
    return Ok();
}

} // namespace

TEST_CASE("J-53: a System that returns Ok with a boundary change, and a false condition with a violation, are Err") {
    SUBCASE("Ok with a change after the call") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&KeepTag>(B("KeepTag", "A"));
            r.AddSystem<&WriteKeptAndReturnOk>(B("WriteKeptAndReturnOk", "B"));
        });
        rig->world().Insert(1, Tag{1});
        const auto result = rig->Run();
        g_keptTag = nullptr;
        CHECK(HasCode(result, KernelErrorCode::BoundaryChange));
        REQUIRE_FALSE(static_cast<bool>(result));
        CHECK(result.error().Text().find("WriteKeptAndReturnOk") != std::string::npos);
        CHECK(result.error().Text().find("schedule 'Tick'") != std::string::npos);
        CHECK(rig->world().State() == WorldState::Interrupted);
    }
    SUBCASE("a false condition that recorded a violation") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            SystemOptions options = B("NeverCalled", "A");
            options.mounts[0].conditions.push_back(Condition<&FalseAfterUndeclaredRead>("FalseAfterUndeclaredRead"));
            r.AddSystem<&NeverCalled>(std::move(options));
        });
        rig->world().SetResource(Flag{0});
        rig->world().Insert(1, Pos{1});
        KeptWorld() = &rig->world();
        Log().clear();
        const auto result = rig->Run();
        KeptWorld() = nullptr;
        CHECK(Log().empty());
        CHECK(HasCode(result, KernelErrorCode::AccessViolation));
        CHECK(rig->recorded.size() == 1);
    }
}

// ---- J-11 run-time half: full against incremental rebuild (W-11)

namespace {

std::map<EntityId, std::int32_t>& Mirror() {
    static std::map<EntityId, std::int32_t> mirror;
    return mirror;
}

// Writes a few cells each tick through Mut (the only way to take a value for writing).
SystemResult MoveSome(Query<Write<Pos>> positions, Res<TickCount> tick) {
    for (EntityId id : positions.Entities()) {
        if ((id + tick->value) % 3 == 0) {
            if (Mut<Pos> pos = positions.GetMut<Pos>(id)) pos->x += static_cast<std::int32_t>(tick->value);
        }
    }
    return Ok();
}
// Keeps a mirror up to date from the changes only.
SystemResult MirrorChanges(Query<Read<Pos>, Changed<Pos>> changed) {
    changed.ForEach([](EntityId id, const Pos& pos) { Mirror()[id] = pos.x; });
    return Ok();
}

} // namespace

TEST_CASE("J-11: a mirror rebuilt from Changed equals a full copy (no missed change stamp)") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&MoveSome>(C("MoveSome", "A"));
        r.AddSystem<&MirrorChanges>(B("MirrorChanges", "B"));
    });
    for (EntityId id = 1; id <= 12; ++id) rig->world().Insert(id, Pos{static_cast<std::int32_t>(id)});
    Mirror().clear();
    for (int tick = 0; tick < 6; ++tick) {
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        std::map<EntityId, std::int32_t> full;
        for (EntityId id : rig->world().Keys<Pos>()) full[id] = rig->world().Get<Pos>(id)->x;
        CHECK(Mirror() == full);
    }
}

// ---- J-12 run-time half: Fetch has no order dependence (spec B.4 point 7)

namespace {

SystemResult ResThenQuery(ResMut<Out> out, Query<Write<Tag>> tags) {
    for (EntityId id : tags.Entities()) {
        if (Mut<Tag> tag = tags.GetMut<Tag>(id)) tag->t += 1;
    }
    out->value += 1;
    return Ok();
}
SystemResult QueryThenRes(Query<Write<Mode>> modes, ResMut<Flag> flag) {
    for (EntityId id : modes.Entities()) {
        if (Mut<Mode> mode = modes.GetMut<Mode>(id)) mode->m += 1;
    }
    flag->on += 1;
    return Ok();
}

} // namespace

TEST_CASE("J-12: the same parameters in two orders give the same result and the same stamps") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&ResThenQuery>(B("ResThenQuery", "A"));
        r.AddSystem<&QueryThenRes>(B("QueryThenRes", "B"));
    });
    World& world = rig->world();
    world.SetResource(Out{0});
    world.SetResource(Flag{0});
    world.Insert(1, Tag{0});
    world.Insert(1, Mode{0});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    // Plan: ResThenQuery (stamp 1), sync A (2), QueryThenRes (3), sync B (4), sync C (5).
    CHECK(world.Get<Tag>(1)->t == 1);
    CHECK(world.Get<Mode>(1)->m == 1);
    CHECK(world.Resource<Out>()->value == 1);
    CHECK(world.Resource<Flag>()->on == 1);
    CHECK(world.Stamps<Tag>(1)->second == 1);
    CHECK(world.Stamps<Mode>(1)->second == 3);
}

// ---- J-44 run-time half: the World summary under 20 registration orders (D-9)

namespace {

struct Ca { std::int32_t v; };
struct Cb { std::int32_t v; };
struct Cc { std::int32_t v; };
void HashValue(Hasher& h, const Ca& v) { HashValue(h, v.v); }
void HashValue(Hasher& h, const Cb& v) { HashValue(h, v.v); }
void HashValue(Hasher& h, const Cc& v) { HashValue(h, v.v); }

SystemResult P0(Query<Write<Ca>> a) {
    for (EntityId id : a.Entities()) a.GetMut<Ca>(id)->v += 1;
    return Ok();
}
SystemResult P1(Query<Read<Ca>, Write<Cb>> q) {
    q.ForEach([](EntityId, const Ca& a, Mut<Cb> b) { b->v = b.Read().v * 2 + a.v; });
    return Ok();
}
SystemResult P2(Query<Read<Cb>, Write<Cc>> q) {
    q.ForEach([](EntityId, const Cb& b, Mut<Cc> c) { c->v = c.Read().v + b.v; });
    return Ok();
}
SystemResult P3(Query<Read<Cc>> q) {
    static_cast<void>(q.Count());
    return Ok();
}
SystemResult P4(Query<Read<Ca>, Read<Cc>> q) {
    static_cast<void>(q.Count());
    return Ok();
}

using Registration = void (*)(Registrar&);
std::vector<Mount> Sim(std::vector<std::string> before = {}, std::vector<std::string> after = {}) {
    return {At("Tick", "A", std::move(before), std::move(after))};
}
constexpr std::array<Registration, 5> kSystems{{
    [](Registrar& r) { r.AddSystem<&P0>(Sys("P0", Sim({"P1", "P4"}))); },
    [](Registrar& r) { r.AddSystem<&P1>(Sys("P1", Sim({"P2"}))); },
    [](Registrar& r) { r.AddSystem<&P2>(Sys("P2", Sim({"P3", "P4"}))); },
    [](Registrar& r) { r.AddSystem<&P3>(Sys("P3", Sim())); },
    [](Registrar& r) { r.AddSystem<&P4>(Sys("P4", Sim())); },
}};

std::string OrderSummary(const std::array<int, 5>& order) {
    auto rig = MakeRig([&order](Registrar& r) {
        r.Component<Ca>({.name = "Ca"});
        r.Component<Cb>({.name = "Cb"});
        r.Component<Cc>({.name = "Cc"});
        r.Schedule(TickSchedule());
        for (int index : order) kSystems[static_cast<std::size_t>(index)](r);
    });
    for (EntityId id = 1; id <= 4; ++id) {
        rig->world().Insert(id, Ca{static_cast<std::int32_t>(id)});
        rig->world().Insert(id, Cb{1});
        rig->world().Insert(id, Cc{0});
    }
    for (int tick = 0; tick < 3; ++tick) {
        const auto result = rig->Run();
        REQUIRE_MESSAGE(static_cast<bool>(result), Text(result));
    }
    return SummaryOf(rig->world()).Text();
}

} // namespace

TEST_CASE("J-44: 20 registration orders of Systems give the same World summary") {
    std::array<int, 5> order{0, 1, 2, 3, 4};
    std::vector<std::array<int, 5>> orders;
    int index = 0;
    do {
        if (index % 6 == 0) orders.push_back(order);
        ++index;
    } while (std::next_permutation(order.begin(), order.end()));
    REQUIRE(orders.size() == 20);
    const std::string first = OrderSummary(orders.front());
    for (const auto& each : orders) CHECK(OrderSummary(each) == first);
}
