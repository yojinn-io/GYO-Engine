// What both ends compare (v8-arch-gyo2-authority-rollback step 5; authority
// declaration 5.5, 5.6; DI-A 5): the consistency fingerprint changes with the
// decision part and not with ordinary data, a OneEndOnly System or the rollback
// that only one end has; the digest is order free, equal cells do not cancel,
// digestExclude is left out, the marked tick is in. Each CHECK carries a tag in
// brackets ("[au-...]").

#include "AuthorityTestSupport.hpp"

using namespace AuthorityTest;

namespace {

K::SystemResult WriteHealth(K::Query<K::Write<Health>>) { return {}; }
K::SystemResult SecondHealth(K::Query<K::Write<Health>>) { return {}; }
K::SystemResult WriteGlow(K::Query<K::Write<Glow>>) { return {}; }
K::SystemResult Panel(K::Query<K::Write<Health>>) { return {}; }

// The base World: Health decides; WriteHealth then SecondHealth write it (registration order breaks the tie).
void Base(O::WorldSetup& s) {
    A::AuthorityEntry(s).Component<Health>("Health");
    s.Registrar().AddSystem<&WriteHealth>(In("WriteHealth", "FixedUpdate", "Update"));
    s.Registrar().AddSystem<&SecondHealth>(In("SecondHealth", "FixedUpdate", "Update"));
}

std::string FingerprintOf(std::function<void(O::WorldSetup&)> build, const Setting& setting = {}) {
    auto built = Build(std::move(build), setting);
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    const auto fingerprint = A::ConsistencyFingerprintOf((*built)->World("Main").GetWorld());
    REQUIRE_MESSAGE(static_cast<bool>(fingerprint), (fingerprint ? std::string() : fingerprint.error().message));
    return fingerprint->Text();
}

std::uint64_t DigestOf(K::World& world) {
    const auto digest = A::Digest(world);
    REQUIRE_MESSAGE(static_cast<bool>(digest), (digest ? std::string() : digest.error().message));
    return *digest;
}

} // namespace

TEST_CASE("authority consistency: the fingerprint changes with the decision part") {
    const std::string base = FingerprintOf(&Base);
    CHECK_MESSAGE(FingerprintOf(&Base) == base, "[au-fp-stable]");
    CHECK_MESSAGE(FingerprintOf([](O::WorldSetup& s) {
                      Base(s);
                      A::AuthorityEntry(s).Component<Armor>("Armor");
                  }) != base, "[au-fp-decision-type]");
    // The two writers the other way round: the tie break orders them differently.
    CHECK_MESSAGE(FingerprintOf([](O::WorldSetup& s) {
                      A::AuthorityEntry(s).Component<Health>("Health");
                      s.Registrar().AddSystem<&SecondHealth>(In("SecondHealth", "FixedUpdate", "Update"));
                      s.Registrar().AddSystem<&WriteHealth>(In("WriteHealth", "FixedUpdate", "Update"));
                  }) != base, "[au-fp-order]");
    CHECK_MESSAGE(FingerprintOf([](O::WorldSetup& s) {
                      Base(s);
                      s.Registrar().AddSystem<&Panel>(In("Panel", "Main", "Update"));
                  }) != base, "[au-fp-writer]");
    const auto other = std::make_shared<A::ServerAuthorityStrategy>();
    class Renamed final : public A::ServerAuthorityStrategy {
    public:
        [[nodiscard]] std::string_view Name() const override { return "RenamedAuthority"; }
    };
    Setting renamed;
    renamed.authority = std::make_shared<Renamed>();
    CHECK_MESSAGE(FingerprintOf(&Base, renamed) != base, "[au-fp-strategy]");
}

TEST_CASE("authority consistency: the fingerprint does not change with ordinary data, a OneEndOnly System or one end's rollback") {
    const std::string base = FingerprintOf(&Base);
    CHECK_MESSAGE(FingerprintOf([](O::WorldSetup& s) {
                      s.Registrar().Component<Glow>({.name = "Glow"});
                      Base(s);
                      s.Registrar().AddSystem<&WriteGlow>(In("WriteGlow", "FixedUpdate", "Update"));
                  }) == base, "[au-fp-ordinary]");
    CHECK_MESSAGE(FingerprintOf([](O::WorldSetup& s) {
                      Base(s);
                      s.Registrar().AddSystem<&Panel>(In("Panel", "Main", "Update"));
                      A::AuthorityEntry(s).OneEndOnly("Panel", "the Client's debug panel");
                  }) == base, "[au-fp-one-end]");
    // A Client (a rollback strategy, a step schedule, its record System) and a Match (none) connect.
    Setting client;
    client.rollback = std::make_shared<R::StateRollbackStrategy>();
    CHECK_MESSAGE(FingerprintOf([](O::WorldSetup& s) {
                      Base(s);
                      R::RollbackEntry(s).StepSchedule("FixedUpdate", "Last");
                  }, client) == base, "[au-fp-two-ends]");
}

TEST_CASE("authority consistency: the digest is order free, equal cells do not cancel, digestExclude is out, the tick is in") {
    const auto make = [](std::function<void(K::World&)> fill) {
        auto built = Build([](O::WorldSetup& s) {
            A::AuthorityEntry authority(s);
            authority.Component<Health>("Health");
            authority.Component<Armor>("Armor", {.digestExclude = true});
            authority.Mark<W::Tick>();
        });
        REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
        fill((*built)->World("Main").GetWorld());
        return std::move(*built);
    };
    auto forward = make([](K::World& w) {
        w.Insert(1, Health{5});
        w.Insert(2, Health{7});
    });
    auto backward = make([](K::World& w) {
        w.Insert(2, Health{7});
        w.Insert(1, Health{5});
    });
    const std::uint64_t base = DigestOf(forward->World("Main").GetWorld());
    CHECK_MESSAGE(DigestOf(backward->World("Main").GetWorld()) == base, "[au-digest-order]");
    auto twins = make([](K::World& w) {
        w.Insert(1, Health{5});
        w.Insert(2, Health{5});
    });
    auto single = make([](K::World& w) { w.Insert(1, Health{5}); });
    auto empty = make([](K::World&) {});
    const std::uint64_t twinsDigest = DigestOf(twins->World("Main").GetWorld());
    CHECK_MESSAGE((twinsDigest != DigestOf(empty->World("Main").GetWorld()) &&
                   twinsDigest != DigestOf(single->World("Main").GetWorld())), "[au-digest-no-cancel]");
    K::World& world = forward->World("Main").GetWorld();
    world.Insert(1, Armor{3});
    CHECK_MESSAGE(DigestOf(world) == base, "[au-digest-exclude] an excluded type changed the digest");
    world.ResourceMut<W::Tick>()->value += 1;
    CHECK_MESSAGE(DigestOf(world) != base, "[au-digest-tick] the marked tick is not in the digest");
    const std::uint64_t ticked = DigestOf(world);
    *world.GetMut<Health>(2) = Health{8};
    CHECK_MESSAGE(DigestOf(world) != ticked, "[au-digest-value]");
}

TEST_CASE("authority consistency: G-5: the export lists the files of what writes decision data") {
    auto built = Build(&Base);
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    K::World& world = (*built)->World("Main").GetWorld();
    std::string here = __FILE__;
    for (char& c : here) {
        if (c == '\\') c = '/';
    }
    const std::string marker = "/tests/common/authority/";
    const std::string root = here.substr(0, here.rfind(marker));
    const auto exported = A::ExportAuthoritySources(world, {root, "tests/common/authority", {}, "0000000"});
    REQUIRE_MESSAGE(static_cast<bool>(exported), (exported ? std::string() : exported.error().message + " " + exported.error().detail));
    CHECK_MESSAGE(exported->find("\"format\": \"gyo.authority-sources\"") != std::string::npos, "[au-g5-format] " << *exported);
    CHECK_MESSAGE(exported->find("\"tests/common/authority/ConsistencyTests.cpp\"") != std::string::npos, "[au-g5-lists] " << *exported);
    const auto outside = A::ExportAuthoritySources(world, {root, "tests/common/somewhere_else", {}, "0000000"});
    REQUIRE_MESSAGE(!outside, "[au-g5-outside] a writer outside the owner root was left out silently");
    CHECK_MESSAGE(outside.error().code == A::AuthorityFailureCode::SourceOutsideRoot, "[au-g5-outside] code");
}
