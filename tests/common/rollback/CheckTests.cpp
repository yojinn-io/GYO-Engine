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

K::SystemResult ComputeSpeed(K::Query<K::Write<Speed>>) { return {}; }
K::SystemResult InsertSpeed(K::CommandsFor<K::Inserts<Speed>, K::Removes<>>) { return {}; }
K::SystemResult ReadSpeed(K::Query<K::Read<Speed>>) { return {}; }
K::SystemResult SendHit(K::MessageWriter<Hit>) { return {}; }
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
