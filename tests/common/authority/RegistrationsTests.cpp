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
    CHECK_MESSAGE(RollbackOf(**built).Skipped("ReadCommand"), "[au-ingress-skipped]");
    CHECK_MESSAGE((!entry->mounts.empty() && !entry->mounts[0].conditions.empty()), "[au-ingress-skipped] no run condition");
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
