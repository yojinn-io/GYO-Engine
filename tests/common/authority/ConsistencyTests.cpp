// What both ends compare (v8-arch-gyo2-authority-rollback step 5; authority
// declaration 5.5, 5.6; DI-A 5): the consistency fingerprint changes with the
// decision part and not with ordinary data, a OneEndOnly System or the rollback
// that only one end has; the digest is order free, equal cells do not cancel,
// digestExclude is left out, the marked tick is in. Each CHECK carries a tag in
// brackets ("[au-...]").

#include "AuthorityTestSupport.hpp"

using namespace GYO::Test::Authority;

namespace {

K::SystemResult WriteHealth(K::Query<K::Write<Health>>) { return {}; }
K::SystemResult SecondHealth(K::Query<K::Write<Health>>) { return {}; }
K::SystemResult WriteGlow(K::Query<K::Write<Glow>>) { return {}; }
K::SystemResult Panel(K::Query<K::Write<Health>>) { return {}; }
Health HealthFromArmor(const Armor& armor) { return Health{armor.value}; }
K::SystemResult ReadsArmor(K::Query<K::Write<Health>, K::Read<Armor>>) { return {}; }
K::SystemResult HealthHook(K::HookCtx, K::Query<K::Write<Health>>) { return {}; }
K::SystemResult OnHit(K::On<HitLanded>, K::Query<K::Write<Health>>) { return {}; }
K::SystemResult OnBlock(K::On<BlockLanded>, K::Query<K::Write<Health>>) { return {}; }
K::SystemResult RunNested(K::ExclusiveWorld<>) { return {}; }
bool Yes() { return true; }
bool No() { return false; }
K::SystemResult GlowHook(K::HookCtx, K::Query<K::Write<Glow>>) { return {}; }
K::SystemResult GlowOnHit(K::On<HitLanded>, K::Query<K::Write<Glow>>) { return {}; }
Glow GlowFromArmor(const Armor& armor) { return Glow{armor.value}; }
Health HealthFromScore(const Score& score) { return Health{score.value}; }
K::SystemResult WriteHealthAndGlow(K::Query<K::Write<Health>, K::Write<Glow>>) { return {}; }
K::SystemResult SecondHook(K::HookCtx, K::Query<K::Write<Health>>) { return {}; }
K::SystemResult OnHitReadsArmor(K::On<HitLanded>, K::Query<K::Write<Health>, K::Read<Armor>>) { return {}; }

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
    // The same registrations, the Plan order turned round by a before (before / after themselves are not taken).
    CHECK_MESSAGE(FingerprintOf([](O::WorldSetup& s) {
                      A::AuthorityEntry(s).Component<Health>("Health");
                      s.Registrar().AddSystem<&WriteHealth>(In("WriteHealth", "FixedUpdate", "Update"));
                      K::SystemOptions second = In("SecondHealth", "FixedUpdate", "Update");
                      second.mounts[0].before = {"WriteHealth"};
                      s.Registrar().AddSystem<&SecondHealth>(second);
                  }) != base, "[au-fp-plan-order]");
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
    // The same values on the other entities: a merge that depends on the order of the values tells them apart.
    auto backward = make([](K::World& w) {
        w.Insert(1, Health{7});
        w.Insert(2, Health{5});
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
    // A control character in a string is escaped (the list stays JSON).
    const auto escaped = A::ExportAuthoritySources(world, {root, "tests/common/authority", {}, "abc\ndef"});
    REQUIRE(static_cast<bool>(escaped));
    CHECK_MESSAGE(escaped->find("\"repo_commit\": \"abc\\u000adef\"") != std::string::npos, "[au-g5-json] " << *escaped);
    const auto outside = A::ExportAuthoritySources(world, {root, "tests/common/somewhere_else", {}, "0000000"});
    REQUIRE_MESSAGE(!outside, "[au-g5-outside] a writer outside the owner root was left out silently");
    CHECK_MESSAGE(outside.error().code == A::AuthorityFailureCode::SourceOutsideRoot, "[au-g5-outside] code");
}

TEST_CASE("authority consistency: the dataset report lists the types, the collected writers and every OneEndOnly") {
    auto built = Build([](O::WorldSetup& s) {
        Base(s);
        A::AuthorityEntry(s).Mark<W::Tick>();
        s.Registrar().AddSystem<&Panel>(In("Panel", "Main", "Update"));
        A::AuthorityEntry(s).OneEndOnly("Panel", "the Client's debug panel");
    });
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    const auto report = A::DatasetReport((*built)->World("Main").GetWorld());
    REQUIRE(static_cast<bool>(report));
    CHECK_MESSAGE(report->find("type 0 component Health\n") != std::string::npos, "[au-dataset] " << *report);
    CHECK_MESSAGE(report->find("type 1 marked resource Tick\n") != std::string::npos, "[au-dataset] " << *report);
    CHECK_MESSAGE((report->find("system WriteHealth\n") != std::string::npos && report->find("system Panel\n") == std::string::npos),
                  "[au-dataset] " << *report);
    CHECK_MESSAGE(report->find("one-end-only Panel (the Client's debug panel)\n") != std::string::npos, "[au-dataset-one-end] " << *report);
}

TEST_CASE("authority consistency: G-5 lists the file of a Registrar::InitialValue function for a decision component") {
    auto built = Build([](O::WorldSetup& s) {
        s.Registrar().Component<Armor>({.name = "ArmorRequest"});
        A::AuthorityEntry(s).Component<Health>("Health");
        s.Registrar().InitialValue<Health, &HealthFromArmor>();
    });
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    std::string here = __FILE__;
    for (char& c : here) {
        if (c == '\\') c = '/';
    }
    const std::string root = here.substr(0, here.rfind("/tests/common/authority/"));
    const auto exported = A::ExportAuthoritySources((*built)->World("Main").GetWorld(), {root, "tests/common/authority", {}, "0"});
    REQUIRE(static_cast<bool>(exported));
    CHECK_MESSAGE(exported->find("\"tests/common/authority/ConsistencyTests.cpp\"") != std::string::npos, "[au-g5-initial-value] " << *exported);
}

TEST_CASE("authority consistency: the digest hashes each value with its type's number") {
    auto built = Build([](O::WorldSetup& s) {
        A::AuthorityEntry authority(s);
        authority.Component<Health>("Health");
        authority.Component<Armor>("Armor");
    });
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    K::World& world = (*built)->World("Main").GetWorld();
    world.Insert(1, Health{5});
    const std::uint64_t health = DigestOf(world);
    world.Remove<Health>(1);
    world.Insert(1, Armor{5});
    CHECK_MESSAGE(DigestOf(world) != health, "[au-digest-type-index] the same value of two types hashes alike");
}

TEST_CASE("authority consistency: each item of the fingerprint's content is in it") {
    const std::string base = FingerprintOf(&Base);
    const auto differs = [&base](std::string tag, std::function<void(O::WorldSetup&)> build, const Setting& setting = {}) {
        const std::string other = FingerprintOf(std::move(build), setting);
        CHECK_MESSAGE(other != base, tag);
    };
    differs("[au-fp-c-stage]", [](O::WorldSetup& s) {
        A::AuthorityEntry(s).Component<Health>("Health");
        s.Registrar().AddSystem<&WriteHealth>(In("WriteHealth", "FixedUpdate", "PreUpdate"));
        s.Registrar().AddSystem<&SecondHealth>(In("SecondHealth", "FixedUpdate", "PreUpdate"));
    });
    differs("[au-fp-c-condition]", [](O::WorldSetup& s) {
        A::AuthorityEntry(s).Component<Health>("Health");
        K::SystemOptions first = In("WriteHealth", "FixedUpdate", "Update");
        first.mounts[0].conditions.push_back(K::Condition<&Yes>("Yes"));
        s.Registrar().AddSystem<&WriteHealth>(first);
        s.Registrar().AddSystem<&SecondHealth>(In("SecondHealth", "FixedUpdate", "Update"));
    });
    differs("[au-fp-c-access]", [](O::WorldSetup& s) {
        s.Registrar().Component<Armor>({.name = "Armor"});
        A::AuthorityEntry(s).Component<Health>("Health");
        s.Registrar().AddSystem<&ReadsArmor>(In("WriteHealth", "FixedUpdate", "Update"));
        s.Registrar().AddSystem<&SecondHealth>(In("SecondHealth", "FixedUpdate", "Update"));
    });
    differs("[au-fp-c-hook]", [](O::WorldSetup& s) {
        Base(s);
        s.Registrar().Hook<Health, &HealthHook>(K::HookTrigger::OnAdd, {.name = "HealthHook"});
    });
    differs("[au-fp-c-initial-value]", [](O::WorldSetup& s) {
        Base(s);
        s.Registrar().Component<Armor>({.name = "ArmorRequest"});
        s.Registrar().InitialValue<Health, &HealthFromArmor>();
    });
    differs("[au-fp-c-runs]", [](O::WorldSetup& s) {
        Base(s);
        s.Registrar().AddSystem<&RunNested>(In("ApplyState", "Main", "Update"));
        A::AuthorityEntry(s).Runs("ApplyState", {"Main"});
    });
    differs("[au-fp-c-message]", [](O::WorldSetup& s) {
        Base(s);
        A::AuthorityEntry(s).Message<Hit>("Hit", 64);
    });
    // A marked type nobody writes with a typed parameter (the rollback's inbox): only the types section changes.
    differs("[au-fp-c-mark]", [](O::WorldSetup& s) {
        Base(s);
        A::AuthorityEntry(s).Mark<R::RollbackInbox>();
    });
    differs("[au-fp-c-digest-exclude]", [](O::WorldSetup& s) {
        A::AuthorityEntry(s).Component<Health>("Health", {.digestExclude = true});
        s.Registrar().AddSystem<&WriteHealth>(In("WriteHealth", "FixedUpdate", "Update"));
        s.Registrar().AddSystem<&SecondHealth>(In("SecondHealth", "FixedUpdate", "Update"));
    });
    class Version2 final : public A::ServerAuthorityStrategy {
    public:
        [[nodiscard]] std::uint32_t Version() const override { return 2; }
    };
    Setting version2;
    version2.authority = std::make_shared<Version2>();
    differs("[au-fp-c-version]", &Base, version2);
    // The same two types registered the other way round.
    const std::string healthFirst = FingerprintOf([](O::WorldSetup& s) {
        Base(s);
        A::AuthorityEntry(s).Component<Armor>("Armor");
    });
    CHECK_MESSAGE(FingerprintOf([](O::WorldSetup& s) {
                      A::AuthorityEntry(s).Component<Armor>("Armor");
                      Base(s);
                  }) != healthFirst, "[au-fp-c-type-order]");
    // A sync point between the two writers, or both on one side of it (same schedule, same order).
    const auto synced = [](bool between) {
        return [between](O::WorldSetup& s) {
            K::ScheduleOptions sim;
            sim.name = "Sim";
            sim.stages = {"S"};
            sim.syncPoints = {{"Mid", "S"}};
            s.Registrar().Schedule(std::move(sim));
            A::AuthorityEntry(s).Component<Health>("Health");
            K::SystemOptions first = In("WriteHealth", "Sim", "S");
            K::SystemOptions second = In("SecondHealth", "Sim", "S");
            if (between) {
                first.mounts[0].before = {"Mid"};
                second.mounts[0].after = {"Mid"};
            } else {
                first.mounts[0].after = {"Mid"};
                second.mounts[0].after = {"WriteHealth"};
            }
            s.Registrar().AddSystem<&WriteHealth>(first);
            s.Registrar().AddSystem<&SecondHealth>(second);
        };
    };
    CHECK_MESSAGE(FingerprintOf(synced(true)) != FingerprintOf(synced(false)), "[au-fp-c-sync]");
    // Message capacity and the kind of a type: same name, other capacity; same name as a resource.
    const std::string withHit = FingerprintOf([](O::WorldSetup& s) {
        Base(s);
        A::AuthorityEntry(s).Message<Hit>("Hit", 64);
    });
    CHECK_MESSAGE(FingerprintOf([](O::WorldSetup& s) {
                      Base(s);
                      A::AuthorityEntry(s).Message<Hit>("Hit", 32);
                  }) != withHit, "[au-fp-c-capacity]");
    const std::string asComponent = FingerprintOf([](O::WorldSetup& s) {
        Base(s);
        A::AuthorityEntry(s).Component<Score>("Score");
    });
    CHECK_MESSAGE(FingerprintOf([](O::WorldSetup& s) {
                      Base(s);
                      A::AuthorityEntry(s).Resource<Score>("Score");
                  }) != asComponent, "[au-fp-c-kind]");
    // A set's run condition, one end Yes and the other No.
    const auto gated = [](bool yes) {
        return [yes](O::WorldSetup& s) {
            A::AuthorityEntry(s).Component<Health>("Health");
            s.Registrar().Set({.name = "Gated", .schedule = "FixedUpdate"},
                              {yes ? K::Condition<&Yes>("Yes") : K::Condition<&No>("No")});
            K::SystemOptions first = In("WriteHealth", "FixedUpdate", "Update");
            first.mounts[0].sets = {"Gated"};
            s.Registrar().AddSystem<&WriteHealth>(first);
        };
    };
    CHECK_MESSAGE(FingerprintOf(gated(true)) != FingerprintOf(gated(false)), "[au-fp-c-set-condition]");
    // An observer watching another event.
    const auto watching = [](bool hit) {
        return [hit](O::WorldSetup& s) {
            A::AuthorityEntry(s).Component<Health>("Health");
            s.Registrar().Event<HitLanded>({.name = "HitLanded"});
            s.Registrar().Event<BlockLanded>({.name = "BlockLanded"});
            if (hit) {
                s.Registrar().Observer<HitLanded, &OnHit>({.name = "OnStrike"});
            } else {
                s.Registrar().Observer<BlockLanded, &OnBlock>({.name = "OnStrike"});
            }
        };
    };
    CHECK_MESSAGE(FingerprintOf(watching(true)) != FingerprintOf(watching(false)), "[au-fp-c-observer-target]");
}

TEST_CASE("authority consistency: the rollback's skipped-while-resimulating condition on one end does not change the fingerprint") {
    const std::string base = FingerprintOf(&Base);
    Setting client;
    client.rollback = std::make_shared<R::StateRollbackStrategy>();
    CHECK_MESSAGE(FingerprintOf([](O::WorldSetup& s) {
                      A::AuthorityEntry(s).Component<Health>("Health");
                      R::RollbackEntry(s).StepSchedule();
                      K::SystemOptions first = In("WriteHealth", "FixedUpdate", "Update");
                      R::RollbackEntry(s).SkipDuringResimulation(first);
                      s.Registrar().AddSystem<&WriteHealth>(first);
                      s.Registrar().AddSystem<&SecondHealth>(In("SecondHealth", "FixedUpdate", "Update"));
                  }, client) == base, "[au-fp-skip-one-end]");
}

TEST_CASE("authority consistency: a hook that writes decision data at one end only is left out with OneEndOnly") {
    const std::string base = FingerprintOf(&Base);
    CHECK_MESSAGE(FingerprintOf([](O::WorldSetup& s) {
                      Base(s);
                      s.Registrar().Hook<Health, &HealthHook>(K::HookTrigger::OnAdd, {.name = "PanelHook"});
                      A::AuthorityEntry(s).OneEndOnly("PanelHook", "the Client's debug panel");
                  }) == base, "[au-fp-one-end-hook]");
}

TEST_CASE("authority consistency: what only one end has does not change the fingerprint") {
    const std::string base = FingerprintOf(&Base);
    // A schedule with no decision writer (the Client's screen schedule).
    CHECK_MESSAGE(FingerprintOf([](O::WorldSetup& s) {
                      Base(s);
                      K::ScheduleOptions screen;
                      screen.name = "Screen";
                      screen.stages = {"S"};
                      s.Registrar().Schedule(std::move(screen));
                      s.Registrar().Component<Glow>({.name = "Glow"});
                      s.Registrar().AddSystem<&WriteGlow>(In("WriteGlow", "Screen", "S"));
                  }) == base, "[au-fp-same-other-schedule]");
    // A hook, an observer and an InitialValue of ordinary data, each on its own.
    const auto ordinary = [](int which) {
        return [which](O::WorldSetup& s) {
            Base(s);
            s.Registrar().Component<Glow>({.name = "Glow"});
            s.Registrar().Component<Armor>({.name = "Armor"});
            s.Registrar().Event<HitLanded>({.name = "HitLanded"});
            if (which == 0) s.Registrar().Hook<Glow, &GlowHook>(K::HookTrigger::OnAdd, {.name = "GlowHook"});
            if (which == 1) s.Registrar().Observer<HitLanded, &GlowOnHit>({.name = "GlowOnHit"});
            if (which == 2) s.Registrar().InitialValue<Glow, &GlowFromArmor>();
        };
    };
    const std::string plain = FingerprintOf(ordinary(-1));
    CHECK_MESSAGE(FingerprintOf(ordinary(0)) == plain, "[au-fp-same-ordinary-hook]");
    CHECK_MESSAGE(FingerprintOf(ordinary(1)) == plain, "[au-fp-same-ordinary-observer]");
    CHECK_MESSAGE(FingerprintOf(ordinary(2)) == plain, "[au-fp-same-ordinary-initial]");
    // An observer at one end only, named with OneEndOnly.
    CHECK_MESSAGE(FingerprintOf([](O::WorldSetup& s) {
                      Base(s);
                      s.Registrar().Event<HitLanded>({.name = "HitLanded"});
                      s.Registrar().Observer<HitLanded, &OnHit>({.name = "OnStrike"});
                      A::AuthorityEntry(s).OneEndOnly("OnStrike", "the Client's hit flash");
                  }) == base, "[au-fp-same-one-end-observer]");
    // A stage in front of the first decision writer (one end's screen stage): the sync point at the stage
    // boundary lies before the first collected step.
    const auto lead = [](bool front) {
        return [front](O::WorldSetup& s) {
            K::ScheduleOptions sim;
            sim.name = "Sim";
            sim.stages = front ? std::vector<std::string>{"Pre", "S"} : std::vector<std::string>{"S"};
            if (front) sim.syncPoints = {{"Lead", "Pre"}};
            s.Registrar().Schedule(std::move(sim));
            A::AuthorityEntry(s).Component<Health>("Health");
            s.Registrar().AddSystem<&WriteHealth>(In("WriteHealth", "Sim", "S"));
        };
    };
    CHECK_MESSAGE(FingerprintOf(lead(true)) == FingerprintOf(lead(false)), "[au-fp-same-leading-sync]");
    // Two observers registered the other way round (the order among observers is not taken).
    const auto observers = [](bool hitFirst) {
        return [hitFirst](O::WorldSetup& s) {
            A::AuthorityEntry(s).Component<Health>("Health");
            s.Registrar().Event<HitLanded>({.name = "HitLanded"});
            s.Registrar().Event<BlockLanded>({.name = "BlockLanded"});
            if (hitFirst) s.Registrar().Observer<HitLanded, &OnHit>({.name = "OnHit"});
            s.Registrar().Observer<BlockLanded, &OnBlock>({.name = "OnBlock"});
            if (!hitFirst) s.Registrar().Observer<HitLanded, &OnHit>({.name = "OnHit"});
        };
    };
    CHECK_MESSAGE(FingerprintOf(observers(true)) == FingerprintOf(observers(false)), "[au-fp-same-observer-order]");
}

TEST_CASE("authority consistency: a hook round limit, Runs schedules, writes, hook order, observer access and the InitialValue request are in the fingerprint") {
    const auto limited = [](std::optional<std::uint32_t> limit) {
        return [limit](O::WorldSetup& s) {
            K::ScheduleOptions sim;
            sim.name = "Sim";
            sim.stages = {"S"};
            sim.hookRoundLimit = limit;
            s.Registrar().Schedule(std::move(sim));
            A::AuthorityEntry(s).Component<Health>("Health");
            s.Registrar().AddSystem<&WriteHealth>(In("WriteHealth", "Sim", "S"));
        };
    };
    CHECK_MESSAGE(FingerprintOf(limited(5U)) != FingerprintOf(limited(std::nullopt)), "[au-fp-c-hook-round-limit]");
    const auto runs = [](std::vector<std::string> schedules) {
        return [schedules](O::WorldSetup& s) {
            Base(s);
            s.Registrar().AddSystem<&RunNested>(In("ApplyState", "Main", "Update"));
            A::AuthorityEntry(s).Runs("ApplyState", schedules);
        };
    };
    CHECK_MESSAGE(FingerprintOf(runs({"Main"})) != FingerprintOf(runs({"Main", "FixedUpdate"})), "[au-fp-c-runs-schedules]");
    CHECK_MESSAGE(FingerprintOf([](O::WorldSetup& s) {
                      s.Registrar().Component<Glow>({.name = "Glow"});
                      A::AuthorityEntry(s).Component<Health>("Health");
                      s.Registrar().AddSystem<&WriteHealthAndGlow>(In("WriteHealth", "FixedUpdate", "Update"));
                      s.Registrar().AddSystem<&SecondHealth>(In("SecondHealth", "FixedUpdate", "Update"));
                  }) != FingerprintOf([](O::WorldSetup& s) {
                      s.Registrar().Component<Glow>({.name = "Glow"});
                      Base(s);
                  }), "[au-fp-c-writes]");
    const auto hooks = [](bool alphaFirst) {
        return [alphaFirst](O::WorldSetup& s) {
            Base(s);
            if (alphaFirst) s.Registrar().Hook<Health, &HealthHook>(K::HookTrigger::OnAdd, {.name = "Alpha"});
            s.Registrar().Hook<Health, &SecondHook>(K::HookTrigger::OnAdd, {.name = "Beta"});
            if (!alphaFirst) s.Registrar().Hook<Health, &HealthHook>(K::HookTrigger::OnAdd, {.name = "Alpha"});
        };
    };
    CHECK_MESSAGE(FingerprintOf(hooks(true)) != FingerprintOf(hooks(false)), "[au-fp-c-hook-order]");
    const auto observerAccess = [](bool readsArmor) {
        return [readsArmor](O::WorldSetup& s) {
            s.Registrar().Component<Armor>({.name = "Armor"});
            A::AuthorityEntry(s).Component<Health>("Health");
            s.Registrar().Event<HitLanded>({.name = "HitLanded"});
            if (readsArmor) {
                s.Registrar().Observer<HitLanded, &OnHitReadsArmor>({.name = "OnStrike"});
            } else {
                s.Registrar().Observer<HitLanded, &OnHit>({.name = "OnStrike"});
            }
        };
    };
    CHECK_MESSAGE(FingerprintOf(observerAccess(true)) != FingerprintOf(observerAccess(false)), "[au-fp-c-observer-access]");
    const auto request = [](bool fromArmor) {
        return [fromArmor](O::WorldSetup& s) {
            Base(s);
            s.Registrar().Component<Armor>({.name = "ArmorRequest"});
            s.Registrar().Component<Score>({.name = "ScoreRequest"});
            if (fromArmor) {
                s.Registrar().InitialValue<Health, &HealthFromArmor>();
            } else {
                s.Registrar().InitialValue<Health, &HealthFromScore>();
            }
        };
    };
    CHECK_MESSAGE(FingerprintOf(request(true)) != FingerprintOf(request(false)), "[au-fp-c-initial-request]");
}

TEST_CASE("authority consistency: a decision resource's value is in the digest") {
    auto built = Build([](O::WorldSetup& s) { A::AuthorityEntry(s).Resource<Score>("Score"); });
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    K::World& world = (*built)->World("Main").GetWorld();
    world.SetResource(Score{1});
    const std::uint64_t one = DigestOf(world);
    world.SetResource(Score{2});
    CHECK_MESSAGE(DigestOf(world) != one, "[au-digest-resource]");
}

TEST_CASE("authority consistency: G-5 lists each file once, and the files of hooks and observers, not of ordinary InitialValue") {
    std::string here = __FILE__;
    for (char& c : here) {
        if (c == '\\') c = '/';
    }
    const std::string root = here.substr(0, here.rfind("/tests/common/authority/"));
    const auto exportOf = [&root](std::function<void(O::WorldSetup&)> build) {
        auto built = Build(std::move(build));
        REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
        const auto exported = A::ExportAuthoritySources((*built)->World("Main").GetWorld(), {root, "tests/common/authority", {}, "0"});
        REQUIRE_MESSAGE(static_cast<bool>(exported), (exported ? std::string() : exported.error().message));
        return *exported;
    };
    const auto count = [](const std::string& text, const std::string& what) {
        std::size_t n = 0;
        for (std::size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1)) ++n;
        return n;
    };
    const std::string file = "\"tests/common/authority/ConsistencyTests.cpp\"";
    // Base has two writers in this file.
    CHECK_MESSAGE(count(exportOf(&Base), file) == 1, "[au-g5-unique] " << exportOf(&Base));
    CHECK_MESSAGE(count(exportOf([](O::WorldSetup& s) {
                      A::AuthorityEntry(s).Component<Health>("Health");
                      s.Registrar().Hook<Health, &HealthHook>(K::HookTrigger::OnAdd, {.name = "HealthHook"});
                  }), file) == 1, "[au-g5-hook]");
    CHECK_MESSAGE(count(exportOf([](O::WorldSetup& s) {
                      A::AuthorityEntry(s).Component<Health>("Health");
                      s.Registrar().Event<HitLanded>({.name = "HitLanded"});
                      s.Registrar().Observer<HitLanded, &OnHit>({.name = "OnStrike"});
                  }), file) == 1, "[au-g5-observer]");
    CHECK_MESSAGE(count(exportOf([](O::WorldSetup& s) {
                      A::AuthorityEntry(s).Component<Health>("Health");
                      s.Registrar().Component<Glow>({.name = "Glow"});
                      s.Registrar().Component<Armor>({.name = "Armor"});
                      s.Registrar().InitialValue<Glow, &GlowFromArmor>();
                  }), file) == 0, "[au-g5-initial-ordinary]");
}
