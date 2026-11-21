// The official strategy's startup checks and the entry's placeholders
// (v8-arch-gyo2-authority-rollback step 4; rollback declaration R-7 and 7.1,
// authority-rollback declaration 5.5): a Derived component read in a
// resimulated schedule needs an immediate writer ordered before the reader
// (RB-E7, the kernel's former D-E16); a message does not cross between the
// resimulated schedules and the others, either way (RB-E8); the four entries
// this batch does not make return NotImplemented. Each CHECK carries a tag in
// brackets ("[rb-...]"): the step's mutants expect it.

#include "RollbackTestSupport.hpp"

#include "engine/rollback/Inbox.hpp"
#include "engine/rollback/StateRollbackStrategy.hpp"

#include <string>

using namespace RollbackTest;

namespace {

struct Speed {
    std::int32_t v = 0;
    friend bool operator==(const Speed&, const Speed&) = default;
};
void HashValue(K::Hasher& h, const Speed& v) { HashValue(h, v.v); }
struct Hit {
    std::int32_t at = 0;
};
void HashValue(K::Hasher& h, const Hit& v) { HashValue(h, v.at); }
struct Aim {
    std::int32_t dir = 0;
};
void HashValue(K::Hasher& h, const Aim& v) { HashValue(h, v.dir); }

K::SystemResult ComputeSpeed(K::Query<K::Write<Speed>>) { return {}; }
K::SystemResult InsertSpeed(K::CommandsFor<K::Inserts<Speed>, K::Removes<>>) { return {}; }
K::SystemResult ReadSpeed(K::Query<K::Read<Speed>>) { return {}; }
K::SystemResult SendHit(K::MessageWriter<Hit>) { return {}; }
K::SystemResult HitOnAdd(K::HookCtx, K::MessageWriter<Hit>) { return {}; }
K::SystemResult ReadHit(K::MessageReader<Hit>) { return {}; }

O::OptionBag WithStrategy() {
    O::OptionBag options;
    options.Set(R::RollbackOptions{std::make_shared<R::StateRollbackStrategy>()});
    return options;
}

template <class Extra>
std::string BuildWith(Extra extra, bool strategy = true) {
    auto built = O::Product::Build(Game([extra](O::WorldSetup& s) {
                                       s.Registrar().Component<Speed>({.name = "Speed"});
                                       s.Registrar().Message<Hit>({.name = "Hit"});
                                       R::RollbackEntry rollback(s);
                                       rollback.Derived<Speed>();
                                       rollback.StepSchedule("FixedUpdate", "Last");
                                       extra(s);
                                   },
                                   strategy ? WithStrategy() : O::OptionBag{}),
                                   Units());
    return Reported(built);
}

// A schedule "Nested", run from the step schedule (the rollback treats it as resimulated).
void AddNested(O::WorldSetup& s) {
    K::ScheduleOptions nested;
    nested.name = "Nested";
    nested.stages = {"S"};
    s.Registrar().Schedule(std::move(nested));
    R::RollbackEntry(s).NestedSchedule("Nested", "FixedUpdate");
}

} // namespace

TEST_CASE("rollback checks: R-7: a Derived read needs an immediate writer ordered before it in a resimulated schedule") {
    const std::string ordered = BuildWith([](O::WorldSetup& s) {
        s.Registrar().AddSystem<&ComputeSpeed>(In("ComputeSpeed", "FixedUpdate", "PreUpdate"));
        s.Registrar().AddSystem<&ReadSpeed>(In("ReadSpeed", "FixedUpdate", "Update"));
    });
    CHECK_MESSAGE(ordered == "built", "[rb-r7-ordered] " << ordered);
    const std::string none = BuildWith([](O::WorldSetup& s) {
        s.Registrar().AddSystem<&ReadSpeed>(In("ReadSpeed", "FixedUpdate", "Update"));
    });
    CHECK_MESSAGE(none.find("RB-E7") != std::string::npos, "[rb-r7-no-writer] " << none);
    const std::string deferred = BuildWith([](O::WorldSetup& s) {
        s.Registrar().AddSystem<&InsertSpeed>(In("InsertSpeed", "FixedUpdate", "PreUpdate"));
        s.Registrar().AddSystem<&ReadSpeed>(In("ReadSpeed", "FixedUpdate", "Update"));
    });
    CHECK_MESSAGE(deferred.find("RB-E7") != std::string::npos, "[rb-r7-deferred] " << deferred);
    const std::string after = BuildWith([](O::WorldSetup& s) {
        s.Registrar().AddSystem<&ReadSpeed>(In("ReadSpeed", "FixedUpdate", "Update"));
        s.Registrar().AddSystem<&ComputeSpeed>(In("ComputeSpeed", "FixedUpdate", "PostUpdate"));
    });
    CHECK_MESSAGE(after.find("RB-E7") != std::string::npos, "[rb-r7-writer-after] " << after);
}

TEST_CASE("rollback checks: R-7 looks at the resimulated schedules only, and only in a World with the strategy") {
    const std::string frame = BuildWith([](O::WorldSetup& s) {
        s.Registrar().AddSystem<&ReadSpeed>(In("ReadSpeed", "Main", "Update"));
    });
    CHECK_MESSAGE(frame == "built", "[rb-r7-frame-unchecked] " << frame);
    const std::string noStrategy = BuildWith([](O::WorldSetup& s) {
        s.Registrar().AddSystem<&ReadSpeed>(In("ReadSpeed", "FixedUpdate", "Update"));
    }, false);
    CHECK_MESSAGE(noStrategy == "built", "[rb-r7-no-strategy] " << noStrategy);
}

TEST_CASE("rollback checks: a message does not cross between the resimulated schedules and the others, either way") {
    const std::string inside = BuildWith([](O::WorldSetup& s) {
        s.Registrar().AddSystem<&SendHit>(In("SendHit", "FixedUpdate", "Update"));
        s.Registrar().AddSystem<&ReadHit>(In("ReadHit", "FixedUpdate", "PostUpdate"));
    });
    CHECK_MESSAGE(inside == "built", "[rb-msg-inside] " << inside);
    const std::string readOutside = BuildWith([](O::WorldSetup& s) {
        s.Registrar().AddSystem<&SendHit>(In("SendHit", "FixedUpdate", "Update"));
        s.Registrar().AddSystem<&ReadHit>(In("ReadHit", "Main", "Update"));
    });
    CHECK_MESSAGE(readOutside.find("RB-E8") != std::string::npos, "[rb-msg-read-outside] " << readOutside);
    const std::string writtenOutside = BuildWith([](O::WorldSetup& s) {
        s.Registrar().AddSystem<&SendHit>(In("SendHit", "Main", "Update"));
        s.Registrar().AddSystem<&ReadHit>(In("ReadHit", "FixedUpdate", "Update"));
    });
    CHECK_MESSAGE(writtenOutside.find("RB-E8") != std::string::npos, "[rb-msg-written-outside] " << writtenOutside);
}

TEST_CASE("rollback checks: the four entries this batch does not make return NotImplemented") {
    R::RollbackInbox inbox;
    const auto partial = inbox.PartialValue<Health>(1, 3, Health{1});
    const auto notSent = inbox.NotSent(1, 3);
    const auto entered = inbox.Entered(1, 3);
    const auto exited = inbox.Exited(1, 3);
    const auto fromEvent = inbox.FromEvent<Health>(1, 3, Health{1});
    for (const auto* result : {&partial, &notSent, &entered, &exited, &fromEvent}) {
        REQUIRE_MESSAGE(!*result, "[rb-not-implemented] an entry that is not made returned Ok");
        CHECK_MESSAGE(result->error().code == R::RollbackFailureCode::NotImplemented, "[rb-not-implemented] code");
    }
}

TEST_CASE("rollback checks: R-7 looks at the nested schedules too, and a tie by registration is no order") {
    const std::string nested = BuildWith([](O::WorldSetup& s) {
        AddNested(s);
        s.Registrar().AddSystem<&ReadSpeed>(In("ReadSpeed", "Nested", "S"));
    });
    CHECK_MESSAGE(nested.find("RB-E7") != std::string::npos, "[rb-r7-nested] " << nested);
    const std::string tie = BuildWith([](O::WorldSetup& s) {
        s.Registrar().AddSystem<&ComputeSpeed>(In("ComputeSpeed", "FixedUpdate", "Update"));
        s.Registrar().AddSystem<&ReadSpeed>(In("ReadSpeed", "FixedUpdate", "Update"));
    });
    CHECK_MESSAGE(tie.find("RB-E7") != std::string::npos, "[rb-r7-tie-not-order] " << tie);
}

TEST_CASE("rollback checks: messages: the nested schedules are inside, and a hook's writes are not looked at") {
    const std::string nested = BuildWith([](O::WorldSetup& s) {
        AddNested(s);
        s.Registrar().AddSystem<&SendHit>(In("SendHit", "Nested", "S"));
        s.Registrar().AddSystem<&ReadHit>(In("ReadHit", "FixedUpdate", "Update"));
    });
    CHECK_MESSAGE(nested == "built", "[rb-msg-nested-inside] " << nested);
    const std::string hook = BuildWith([](O::WorldSetup& s) {
        s.Registrar().Hook<Speed, &HitOnAdd>(K::HookTrigger::OnAdd, {.name = "HitOnAdd"});
        s.Registrar().AddSystem<&ReadHit>(In("ReadHit", "FixedUpdate", "Update"));
    });
    CHECK_MESSAGE(hook == "built", "[rb-msg-hook-unchecked] " << hook);
}

TEST_CASE("rollback checks: RB-E9: an input history that keeps fewer steps than the depth") {
    const std::string few = BuildWith([](O::WorldSetup& s) {
        s.Registrar().Resource<Aim>({.name = "Aim"});
        R::RollbackEntry(s).Input<Aim>(4);
    });
    CHECK_MESSAGE(few.find("RB-E9") != std::string::npos, "[rb-input-capacity] " << few);
    const std::string enough = BuildWith([](O::WorldSetup& s) {
        s.Registrar().Resource<Aim>({.name = "Aim"});
        R::RollbackEntry(s).Input<Aim>(20);
    });
    CHECK_MESSAGE(enough == "built", "[rb-input-capacity] " << enough);
}

TEST_CASE("rollback checks: R-7 follows a path of order edges, not only a direct edge") {
    const std::string path = BuildWith([](O::WorldSetup& s) {
        K::SystemOptions compute = In("ComputeSpeed", "FixedUpdate", "Update");
        s.Registrar().AddSystem<&ComputeSpeed>(compute);
        K::SystemOptions middle = In("Middle", "FixedUpdate", "Update");
        middle.mounts[0].after = {"ComputeSpeed"};
        s.Registrar().AddSystem<&SendHit>(middle);
        K::SystemOptions read = In("ReadSpeed", "FixedUpdate", "Update");
        read.mounts[0].after = {"Middle"};
        s.Registrar().AddSystem<&ReadSpeed>(read);
        s.Registrar().AddSystem<&ReadHit>(In("ReadHit", "FixedUpdate", "PostUpdate"));
    });
    CHECK_MESSAGE(path == "built", "[rb-r7-transitive] " << path);
}
