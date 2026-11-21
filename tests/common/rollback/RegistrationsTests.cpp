// The rollback Module's public registrations (v8-arch-gyo2-authority-rollback
// step 3; rollback declaration 5.4, authority-rollback declaration sections 5.1
// and 6): appended with their source while the World is built, resolved after
// it (the author's over one made on behalf, whatever the order; two of one
// source with different options are RB-E1), checked against the read-only
// registry (RB-E2 to RB-E5), put into the World as RollbackRegistry. Each CHECK
// carries a tag in brackets ("[rb-...]"): the step's mutants expect it
// (docs/architecture/target/v2/scripts/rollback_mutations.json).

#include "RollbackTestSupport.hpp"

#include <string>
#include <vector>

using namespace RollbackTest;

namespace {

// A type no Registrar registers.
struct Unregistered {
    std::int32_t v = 0;
    friend bool operator==(const Unregistered&, const Unregistered&) = default;
};

bool HealthNear(const Health& a, const Health& b) { return a.hp / 10 == b.hp / 10; }
bool HealthExact(const Health& a, const Health& b) { return a.hp == b.hp; }

void RegisterData(O::WorldSetup& s) {
    s.Registrar().Component<Health>({.name = "Health"});
    s.Registrar().Resource<Score>({.name = "Score"});
    s.Registrar().Resource<Command>({.name = "Command"});
}

const R::RollbackRegistry& RegistryOf(O::Product& product) {
    const R::RollbackRegistry* registry = product.World("Main").GetWorld().Resource<R::RollbackRegistry>();
    REQUIRE_MESSAGE(registry != nullptr, "[rb-registry-resource] the World has no RollbackRegistry");
    return *registry;
}

int& Counted() {
    static int count = 0;
    return count;
}
K::SystemResult CountRuns(K::Res<Score>) {
    ++Counted();
    return {};
}

} // namespace

TEST_CASE("rollback registrations: each kind is in the World's RollbackRegistry after it is built") {
    auto built = O::Product::Build(Game([](O::WorldSetup& s) {
                                       RegisterData(s);
                                       R::RollbackEntry rollback(s);
                                       rollback.Component<Health>();
                                       rollback.Resource<Score>();
                                       rollback.Input<Command>(16);
                                       rollback.StepSchedule("FixedUpdate", "Last");
                                       rollback.NestedSchedule("Main", "FixedUpdate");
                                       K::SystemOptions counts = In("CountRuns", "Main", "Update");
                                       rollback.SkipDuringResimulation(counts);
                                       s.Registrar().AddSystem<&CountRuns>(counts);
                                   }),
                                   Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    const R::RollbackRegistry& registry = RegistryOf(**built);
    CHECK_MESSAGE(registry.Data(K::KeyOf<Health>()) != nullptr, "[rb-registry-data] Health");
    CHECK_MESSAGE((registry.Data(K::KeyOf<Score>()) != nullptr &&
                   registry.Data(K::KeyOf<Score>())->kind == R::DataKind::Resource),
                  "[rb-registry-data] Score as a resource");
    CHECK_MESSAGE((registry.inputs.size() == 1 && registry.inputs[0].capacity == 16), "[rb-registry-input]");
    CHECK_MESSAGE((registry.StepSchedule("FixedUpdate") != nullptr && registry.StepSchedule("FixedUpdate")->recordStage == "Last"),
                  "[rb-registry-step]");
    CHECK_MESSAGE((registry.nestedSchedules.size() == 1 && registry.nestedSchedules[0].stepSchedule == "FixedUpdate"),
                  "[rb-registry-nested]");
    const K::SystemInfo* counts = (*built)->World("Main").GetWorld().Types().SystemNamed("CountRuns");
    REQUIRE(counts != nullptr);
    CHECK_MESSAGE((!counts->mounts.empty() && R::SkippedWhileResimulating(counts->mounts[0])), "[rb-registry-skip]");
}

TEST_CASE("rollback registrations: SkipDuringResimulation puts its condition on every mount") {
    auto built = O::Product::Build(Game([](O::WorldSetup& s) {
                                       RegisterData(s);
                                       K::SystemOptions counts = In("CountRuns", "Main", "Update");
                                       counts.mounts.push_back(K::Mount{"FixedUpdate", "Update"});
                                       R::RollbackEntry(s).SkipDuringResimulation(counts);
                                       s.Registrar().AddSystem<&CountRuns>(counts);
                                   }),
                                   Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    const K::SystemInfo* counts = (*built)->World("Main").GetWorld().Types().SystemNamed("CountRuns");
    REQUIRE((counts != nullptr && counts->mounts.size() == 2));
    CHECK_MESSAGE((R::SkippedWhileResimulating(counts->mounts[0]) && R::SkippedWhileResimulating(counts->mounts[1])),
                  "[rb-skip-every-mount]");
}

TEST_CASE("rollback registrations: a System registered as skipped does not run while the World resimulates") {
    Counted() = 0;
    auto built = O::Product::Build(Game([](O::WorldSetup& s) {
                                       RegisterData(s);
                                       K::SystemOptions counts = In("CountRuns", "Main", "Update");
                                       R::RollbackEntry(s).SkipDuringResimulation(counts);
                                       s.Registrar().AddSystem<&CountRuns>(counts);
                                   }),
                                   Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    K::World& world = (*built)->World("Main").GetWorld();
    world.SetResource(Score{});
    REQUIRE(static_cast<bool>((*built)->RunFrame()));
    CHECK_MESSAGE(Counted() == 1, "[rb-skip-runs] not resimulating: " << Counted());
    world.SetResource(W::Resimulation{1});
    REQUIRE(static_cast<bool>((*built)->RunFrame()));
    CHECK_MESSAGE(Counted() == 1, "[rb-skip-skipped] resimulating: " << Counted());
}

TEST_CASE("rollback registrations: the author's registration counts over one made on behalf, whatever the order") {
    for (bool authorFirst : {true, false}) {
        CAPTURE(authorFirst);
        auto built = O::Product::Build(Game([authorFirst](O::WorldSetup& s) {
                                           RegisterData(s);
                                           R::RollbackEntry author(s);
                                           R::RollbackEntry onBehalf(s, R::Source::OnBehalf);
                                           if (authorFirst) author.Component<Health>({.equal = &HealthNear});
                                           onBehalf.Component<Health>({.equal = &HealthExact});
                                           if (!authorFirst) author.Component<Health>({.equal = &HealthNear});
                                       }),
                                       Units());
        REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
        const R::DataRecord* health = RegistryOf(**built).Data(K::KeyOf<Health>());
        REQUIRE(health != nullptr);
        CHECK_MESSAGE(health->source == R::Source::Author, "[rb-author-counts] the one made on behalf counted");
        CHECK_MESSAGE(health->comparison == reinterpret_cast<R::Comparison>(&HealthNear), "[rb-author-counts] options");
        CHECK_MESSAGE(RegistryOf(**built).data.size() == 1, "[rb-author-counts] one registration of Health");
    }
}

TEST_CASE("rollback registrations: two of the author's with different options are RB-E1; with the same, one") {
    auto conflicting = O::Product::Build(Game([](O::WorldSetup& s) {
                                             RegisterData(s);
                                             R::RollbackEntry rollback(s);
                                             rollback.Component<Health>({.equal = &HealthNear});
                                             rollback.Component<Health>({.equal = &HealthExact});
                                         }),
                                         Units());
    CHECK_MESSAGE(Reported(conflicting).find("RB-E1") != std::string::npos, "[rb-e1-author] " << Reported(conflicting));
    CHECK_MESSAGE(Reported(conflicting).find("'Health'") != std::string::npos, "[rb-e1-names] " << Reported(conflicting));
    auto same = O::Product::Build(Game([](O::WorldSetup& s) {
                                      RegisterData(s);
                                      R::RollbackEntry rollback(s);
                                      rollback.Component<Health>();
                                      rollback.Component<Health>();
                                  }),
                                  Units());
    CHECK_MESSAGE(static_cast<bool>(same), "[rb-e1-same-ok] " << Reported(same));
}

TEST_CASE("rollback registrations: two made on behalf with different options are RB-E1") {
    auto built = O::Product::Build(Game([](O::WorldSetup& s) {
                                       RegisterData(s);
                                       R::RollbackEntry onBehalf(s, R::Source::OnBehalf);
                                       onBehalf.Component<Health>({.equal = &HealthNear});
                                       onBehalf.Component<Health>({.equal = &HealthExact});
                                   }),
                                   Units());
    CHECK_MESSAGE(Reported(built).find("RB-E1") != std::string::npos, "[rb-e1-on-behalf] " << Reported(built));
}

TEST_CASE("rollback registrations: rollback state and Derived of one type: the author's counts across the kinds") {
    // The author's Derived over a Component made on behalf (the authority Module registers one by default).
    auto derived = O::Product::Build(Game([](O::WorldSetup& s) {
                                         RegisterData(s);
                                         R::RollbackEntry(s, R::Source::OnBehalf).Component<Health>();
                                         R::RollbackEntry(s).Derived<Health>();
                                     }),
                                     Units());
    REQUIRE_MESSAGE(static_cast<bool>(derived), Reported(derived));
    CHECK_MESSAGE((RegistryOf(**derived).Data(K::KeyOf<Health>()) == nullptr && RegistryOf(**derived).IsDerived(K::KeyOf<Health>())),
                  "[rb-cross-kind-author] the Component made on behalf still counts beside the author's Derived");
    // The author's Component over a Derived made on behalf.
    auto rolled = O::Product::Build(Game([](O::WorldSetup& s) {
                                        RegisterData(s);
                                        R::RollbackEntry(s, R::Source::OnBehalf).Derived<Health>();
                                        R::RollbackEntry(s).Component<Health>();
                                    }),
                                    Units());
    REQUIRE_MESSAGE(static_cast<bool>(rolled), Reported(rolled));
    CHECK_MESSAGE((RegistryOf(**rolled).Data(K::KeyOf<Health>()) != nullptr && !RegistryOf(**rolled).IsDerived(K::KeyOf<Health>())),
                  "[rb-cross-kind-author] the Derived made on behalf still counts beside the author's Component");
    // Both the author's: RB-E1.
    auto both = O::Product::Build(Game([](O::WorldSetup& s) {
                                      RegisterData(s);
                                      R::RollbackEntry rollback(s);
                                      rollback.Component<Health>();
                                      rollback.Derived<Health>();
                                  }),
                                  Units());
    CHECK_MESSAGE(Reported(both).find("RB-E1") != std::string::npos, "[rb-cross-kind-e1] " << Reported(both));
}

TEST_CASE("rollback registrations: what the read-only registry does not have is reported (RB-E2 to RB-E5)") {
    auto built = O::Product::Build(Game([](O::WorldSetup& s) {
                                       RegisterData(s);
                                       R::RollbackEntry rollback(s);
                                       rollback.Component<Unregistered>();
                                       rollback.Input<Unregistered>(8);
                                       rollback.StepSchedule("NoSuchSchedule", "Last");
                                       rollback.NestedSchedule("Main", "NotAStep");
                                   }),
                                   Units());
    const std::string reported = Reported(built);
    CHECK_MESSAGE(reported.find("RB-E2") != std::string::npos, "[rb-e2] " << reported);
    CHECK_MESSAGE(reported.find("RB-E3") != std::string::npos, "[rb-e3] " << reported);
    CHECK_MESSAGE(reported.find("RB-E4") != std::string::npos, "[rb-e4] " << reported);
    CHECK_MESSAGE(reported.find("RB-E5") != std::string::npos, "[rb-e5] " << reported);
}

TEST_CASE("rollback registrations: the peer-to-peer strategy is a placeholder: RB-E6 at startup") {
    O::OptionBag options;
    options.Set(R::RollbackOptions{std::make_shared<R::PeerRollbackStrategy>()});
    auto built = O::Product::Build(Game([](O::WorldSetup& s) { RegisterData(s); }, std::move(options)), Units());
    CHECK_MESSAGE(Reported(built).find("RB-E6") != std::string::npos, "[rb-e6-peer] " << Reported(built));
}

TEST_CASE("rollback registrations: without a strategy the World builds and still has its registrations") {
    auto built = O::Product::Build(Game([](O::WorldSetup& s) {
                                       RegisterData(s);
                                       R::RollbackEntry(s).Component<Health>();
                                   }),
                                   Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    CHECK_MESSAGE(RegistryOf(**built).Data(K::KeyOf<Health>()) != nullptr, "[rb-no-strategy] registrations readable");
}

TEST_CASE("rollback registrations: a registration after Build, or without the rollback Module, is a Programmer Error") {
    SUBCASE("through an entry kept from Build, in a prepare function") {
        auto build = [](O::WorldSetup& s) {
            RegisterData(s);
            auto kept = std::make_shared<R::RollbackEntry>(s);
            s.Prepare([kept](K::World&) -> ::Engine::Base::Result<void, O::OrchestrationError> {
                kept->Component<Health>();
                return {};
            });
        };
        GYO_CHECK_ASSERTS(O::Product::Build(Game(build), Units()));
    }
    SUBCASE("in a World without the rollback Module") {
        GYO_CHECK_ASSERTS(O::Product::Build(Game([](O::WorldSetup& s) {
                                                RegisterData(s);
                                                R::RollbackEntry rollback(s);
                                            }),
                                            Units(false)));
    }
}
