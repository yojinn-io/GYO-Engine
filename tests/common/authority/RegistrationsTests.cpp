// The authority Module's registrations and the server authority strategy's
// registrations on the author's behalf (v8-arch-gyo2-authority-rollback step 5;
// authority declaration 4.3, 4.4, 4.7; DI-A 10, DI-A 11). Each CHECK carries a
// tag in brackets ("[au-...]"): the step's mutants expect it.

#include "AuthorityTestSupport.hpp"

#include <algorithm>

using namespace AuthorityTest;

namespace {

K::SystemResult ReadCommand(K::ResMut<Command>) { return {}; }
K::SystemResult UseHealth(K::Query<K::Read<Health>>) { return {}; }
K::SystemResult RunNested(K::ExclusiveWorld<>) { return {}; }

// An author's strategy that counts which registration each OnRegister call carries, then does the official thing.
struct Calls {
    int marks = 0;
    int runs = 0;
    int oneEnd = 0;
    int other = 0;
};
class Counting final : public A::ServerAuthorityStrategy {
public:
    explicit Counting(Calls& calls) : calls_(&calls) {}
    void OnRegister(A::RegisterContext& context) override {
        const int set = (context.Data() != nullptr) + (context.Mark() != nullptr) + (context.Ingress() != nullptr) +
                        (context.Runs() != nullptr) + (context.OneEnd() != nullptr);
        if (set != 1) {
            ++calls_->other;
        } else if (context.Mark() != nullptr) {
            ++calls_->marks;
        } else if (context.Runs() != nullptr) {
            ++calls_->runs;
        } else if (context.OneEnd() != nullptr) {
            ++calls_->oneEnd;
        }
        ServerAuthorityStrategy::OnRegister(context);
    }

private:
    Calls* calls_;
};

const R::RollbackRegistry& RollbackOf(O::Product& p) { return *p.World("Main").GetWorld().Resource<R::RollbackRegistry>(); }
const A::AuthorityRegistry& AuthorityOf(O::Product& p) { return *p.World("Main").GetWorld().Resource<A::AuthorityRegistry>(); }

} // namespace

TEST_CASE("authority registrations: a decision type goes into the kernel and, with the server authority, into the rollback Module") {
    auto built = Build([](O::WorldSetup& s) {
        A::AuthorityEntry authority(s);
        authority.Component<Health>("Health");
        authority.Resource<Score>("Score");
        authority.Message<Hit>("Hit", 64);
    });
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    K::World& world = (*built)->World("Main").GetWorld();
    CHECK_MESSAGE((world.Types().ComponentIdOf<Health>() && world.Types().ResourceIdOf<Score>() &&
                   world.Types().MessageIdOf<Hit>()), "[au-in-kernel]");
    CHECK_MESSAGE(world.Types().Messages()[*world.Types().MessageIdOf<Hit>()].capacity == 64, "[au-in-kernel] capacity");
    const R::RollbackRegistry& rollback = RollbackOf(**built);
    const R::DataRecord* health = rollback.Data(K::KeyOf<Health>());
    REQUIRE_MESSAGE(health != nullptr, "[au-on-behalf-rollback] Health is not in the rollback registrations");
    CHECK_MESSAGE(health->source == R::Source::OnBehalf, "[au-on-behalf-rollback] source");
    CHECK_MESSAGE(rollback.Data(K::KeyOf<Score>()) != nullptr, "[au-on-behalf-rollback] Score");
    CHECK_MESSAGE(AuthorityOf(**built).data.size() == 3, "[au-registry-put] " << AuthorityOf(**built).data.size());
}

TEST_CASE("authority registrations: the rollback options: an input never, NoRollback none, Derived, and the author's own over the on-behalf one") {
    auto built = Build([](O::WorldSetup& s) {
        A::AuthorityEntry authority(s);
        authority.Resource<Command>("Command", {.input = true});
        authority.Component<Armor>("Armor", {.rollback = A::RollbackChoice::NoRollback});
        authority.Component<Score>("ScoreC", {.rollback = A::RollbackChoice::Derived});
        authority.Component<Health>("Health");
        R::RollbackEntry(s).Component<Health>({.equal = [](const Health& a, const Health& b) { return a.hp == b.hp; }});
    });
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    const R::RollbackRegistry& rollback = RollbackOf(**built);
    CHECK_MESSAGE(rollback.Data(K::KeyOf<Command>()) == nullptr, "[au-input-not-rolled] an input is rollback state");
    CHECK_MESSAGE(rollback.Data(K::KeyOf<Armor>()) == nullptr, "[au-norollback] NoRollback registered");
    CHECK_MESSAGE(rollback.IsDerived(K::KeyOf<Score>()), "[au-derived] Derived not registered");
    REQUIRE(rollback.Data(K::KeyOf<Health>()) != nullptr);
    CHECK_MESSAGE(rollback.Data(K::KeyOf<Health>())->source == R::Source::Author, "[au-author-wins] the on-behalf one counts");
}

TEST_CASE("authority registrations: without a strategy the types still go into the kernel and AU-E2 is reported") {
    Setting none;
    none.authority = nullptr;
    const std::string text = BuildText([](O::WorldSetup& s) {
        A::AuthorityEntry(s).Component<Health>("Health");
        // A System that uses the type: the kernel accepts it (no R-E4), so AU-E2 is the reported cause.
        s.Registrar().AddSystem<&UseHealth>(In("UseHealth", "Main", "Update"));
    }, none);
    CHECK_MESSAGE(text.find("AU-E2") != std::string::npos, "[au-no-strategy] " << text);
    CHECK_MESSAGE(text.find("R-E4") == std::string::npos, "[au-no-strategy] the kernel refused the type: " << text);
    // A World with no authority registration needs no strategy.
    CHECK_MESSAGE(BuildText([](O::WorldSetup&) {}, none) == "built", "[au-no-strategy-needed]");
}

TEST_CASE("authority registrations: an entry is added on the author's behalf and skipped while resimulating") {
    auto built = Build([](O::WorldSetup& s) {
        A::AuthorityEntry authority(s);
        authority.Resource<Command>("Command", {.input = true});
        authority.Ingress<&ReadCommand>(In("ReadCommand", "Main", "PreUpdate"));
    });
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    const K::SystemInfo* entry = (*built)->World("Main").GetWorld().Types().SystemNamed("ReadCommand");
    REQUIRE_MESSAGE(entry != nullptr, "[au-ingress-added]");
    CHECK_MESSAGE(!R::RunsWhileResimulating(*entry, R::ResimulatedSchedules(RollbackOf(**built))), "[au-ingress-skipped]");
    CHECK_MESSAGE((!entry->mounts.empty() && R::SkippedWhileResimulating(entry->mounts[0])), "[au-ingress-skipped] no run condition");
}

TEST_CASE("authority registrations: a mark only records (no second kernel registration) and resolves at Check") {
    const std::string marked = BuildText([](O::WorldSetup& s) {
        A::AuthorityEntry authority(s);
        authority.Mark<W::Tick>();
        authority.MarkResource("Resimulation");
    });
    CHECK_MESSAGE(marked == "built", "[au-mark] " << marked);
    const std::string missing = BuildText([](O::WorldSetup& s) {
        A::AuthorityEntry authority(s);
        authority.Mark<Score>();
        authority.MarkResource("NoSuchValue");
    });
    CHECK_MESSAGE(std::count(missing.begin(), missing.end(), '\n') >= 2, "[au-mark-missing] " << missing);
    CHECK_MESSAGE(missing.find("AU-E4 authority.MarkResource: no resource named 'NoSuchValue'") != std::string::npos,
                  "[au-mark-missing] " << missing);
}

TEST_CASE("authority registrations: Self registers nothing on behalf, and a resource's NoRollback neither") {
    auto built = Build([](O::WorldSetup& s) {
        A::AuthorityEntry authority(s);
        authority.Component<Health>("Health", {.rollback = A::RollbackChoice::Self});
        authority.Resource<Score>("Score", {.rollback = A::RollbackChoice::NoRollback});
    });
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    CHECK_MESSAGE(RollbackOf(**built).Data(K::KeyOf<Health>()) == nullptr, "[au-self] registered on behalf");
    CHECK_MESSAGE(RollbackOf(**built).Data(K::KeyOf<Score>()) == nullptr, "[au-resource-norollback] registered on behalf");
}

TEST_CASE("authority registrations: without a strategy any registration needs one") {
    Setting none;
    none.authority = nullptr;
    const std::string mark = BuildText([](O::WorldSetup& s) { A::AuthorityEntry(s).Mark<W::Tick>(); }, none);
    CHECK_MESSAGE(mark.find("AU-E2") != std::string::npos, "[au-no-strategy-mark] " << mark);
}

TEST_CASE("authority registrations: the peer-to-peer placeholder still puts the types into the kernel (AU-E1, not R-E4)") {
    Setting peer;
    peer.authority = std::make_shared<A::PeerAuthorityStrategy>();
    const std::string text = BuildText([](O::WorldSetup& s) {
        A::AuthorityEntry(s).Component<Health>("Health");
        s.Registrar().AddSystem<&UseHealth>(In("UseHealth", "Main", "Update"));
    }, peer);
    CHECK_MESSAGE((text.find("AU-E1") != std::string::npos && text.find("R-E4") == std::string::npos), "[au-peer-kernel] " << text);
}

TEST_CASE("authority registrations: every entry hands its one registration to the strategy's OnRegister") {
    Calls calls;
    Setting counting;
    counting.authority = std::make_shared<Counting>(calls);
    const std::string text = BuildText([](O::WorldSetup& s) {
        A::AuthorityEntry authority(s);
        authority.Mark<W::Tick>();
        s.Registrar().AddSystem<&RunNested>(In("ApplyState", "Main", "Update"));
        authority.Runs("ApplyState", {"Main"});
        s.Registrar().Component<Health>({.name = "Health"});
        s.Registrar().AddSystem<&UseHealth>(In("Panel", "Main", "Update"));
        authority.OneEndOnly("Panel", "a debug panel");
    }, counting);
    CHECK_MESSAGE(text == "built", text);
    CHECK_MESSAGE(calls.marks == 1, "[au-onregister-mark] " << calls.marks);
    CHECK_MESSAGE(calls.runs == 1, "[au-onregister-runs] " << calls.runs);
    CHECK_MESSAGE(calls.oneEnd == 1, "[au-onregister-one-end] " << calls.oneEnd);
    CHECK_MESSAGE(calls.other == 0, "[au-onregister-one-each] calls with not exactly one registration: " << calls.other);
}
