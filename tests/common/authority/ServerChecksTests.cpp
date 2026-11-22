// The server authority strategy's startup checks (v8-arch-gyo2-authority-rollback
// step 5; authority declaration 5.4, 5.5, 5.11): AU-E3 dynamic insertion, AU-E4
// names, AU-E5 the tick's one source, AU-E6 (Z6), AU-E7 (Z8), AU-E8 (D-E15),
// AU-E9 (Runs × nested). Each CHECK carries a tag in brackets ("[au-...]").

#include "AuthorityTestSupport.hpp"

using namespace GYO::Test::Authority;

namespace {

K::SystemResult WriteHealth(K::Query<K::Write<Health>>) { return {}; }
K::SystemResult WriteTick(K::ResMut<W::Tick>) { return {}; }
K::SystemResult ReadCommand(K::ResMut<Command>) { return {}; }
K::SystemResult HealthHook(K::HookCtx, K::Query<K::Write<Health>>) { return {}; }
K::SystemResult RunNested(K::ExclusiveWorld<>) { return {}; }
K::SystemResult FillRequest(K::CommandsFor<K::Inserts<Armor>, K::Removes<>>) { return {}; }
K::SystemResult SendHit(K::MessageWriter<Hit>) { return {}; }
K::SystemResult AnnotateTick(K::ExclusiveWorld<K::Writes<W::Tick>>) { return {}; }
K::SystemResult TickHook(K::HookCtx, K::ResMut<W::Tick>) { return {}; }
K::SystemResult TickObserver(K::On<HitLanded>, K::ResMut<W::Tick>) { return {}; }
K::SystemResult HealthObserver(K::On<HitLanded>, K::Query<K::Write<Health>>) { return {}; }
K::SystemResult AnnotateHealth(K::ExclusiveWorld<K::Writes<Health>>) { return {}; }
K::SystemResult InsertHealth(K::CommandsFor<K::Inserts<Health>, K::Removes<>>) { return {}; }
K::SystemResult WriteArmorInPlace(K::Query<K::Write<Armor>>) { return {}; }
K::SystemResult RemoveArmor(K::CommandsFor<K::Inserts<>, K::Removes<Armor>>) { return {}; }
K::SystemResult ConsumeCommand(K::ResMut<Command>) { return {}; }
K::SystemResult WriteScore(K::ResMut<Score>) { return {}; }
K::SystemResult FillAndWriteScore(K::CommandsFor<K::Inserts<Armor>, K::Removes<>>, K::Query<K::Write<Score>>) { return {}; }
K::SystemResult ReadArmor(K::Query<K::Read<Armor>>) { return {}; }
Score ScoreFromArmor(const Armor& armor) { return Score{armor.value}; }
Health HealthFromScore(const Score& score) { return Health{score.value}; }
Health HealthFromArmor(const Armor& armor) { return Health{armor.value}; }

// A World with the official rollback strategy and its step schedule FixedUpdate.
Setting Client() {
    Setting s;
    s.rollback = std::make_shared<R::StateRollbackStrategy>();
    return s;
}
void Step(O::WorldSetup& s) { R::RollbackEntry(s).StepSchedule("FixedUpdate", "Last"); }

bool Has(const std::string& text, const std::string& code) { return text.find(code) != std::string::npos; }

} // namespace

TEST_CASE("authority checks: AU-E3: the server authority needs a World without dynamic insertion") {
    Setting open;
    open.dynamicInsert = true;
    const std::string text = BuildText([](O::WorldSetup& s) { A::AuthorityEntry(s).Component<Health>("Health"); }, open);
    CHECK_MESSAGE(Has(text, "AU-E3"), "[au-dynamic-insert] " << text);
    CHECK_MESSAGE(BuildText([](O::WorldSetup& s) { A::AuthorityEntry(s).Component<Health>("Health"); }) == "built",
                  "[au-dynamic-insert] a closed World");
}

TEST_CASE("authority checks: AU-E4: Runs and OneEndOnly name Systems and schedules of the World") {
    const std::string text = BuildText([](O::WorldSetup& s) {
        A::AuthorityEntry authority(s);
        authority.Runs("NoSuchSystem", {"NoSuchSchedule"});
        authority.OneEndOnly("NoPanel", "debug");
    });
    CHECK_MESSAGE(Has(text, "AU-E4 authority.Runs: no System named 'NoSuchSystem'"), "[au-names] " << text);
    CHECK_MESSAGE(Has(text, "no schedule named 'NoSuchSchedule'"), "[au-names] " << text);
    CHECK_MESSAGE(Has(text, "AU-E4 authority.OneEndOnly: no System, hook or observer named 'NoPanel'"), "[au-names] " << text);
}

TEST_CASE("authority checks: AU-E5: only the time Module's drivers write the tick") {
    const std::string text = BuildText([](O::WorldSetup& s) {
        A::AuthorityEntry(s).Mark<W::Tick>();
        s.Registrar().AddSystem<&WriteTick>(In("WriteTick", "Main", "Update"));
    });
    CHECK_MESSAGE(Has(text, "AU-E5 System 'WriteTick' writes Tick"), "[au-tick-writer] " << text);
    // The time Module's own driver (RunFixedSteps) is on the list: a World with the tick marked builds.
    CHECK_MESSAGE(BuildText([](O::WorldSetup& s) { A::AuthorityEntry(s).Mark<W::Tick>(); }) == "built", "[au-tick-driver]");
}

TEST_CASE("authority checks: AU-E6 (Z6): decision data written in a resimulated schedule rolls back or is Derived") {
    const std::string notRolled = BuildText([](O::WorldSetup& s) {
        Step(s);
        A::AuthorityEntry(s).Component<Health>("Health", {.rollback = A::RollbackChoice::NoRollback});
        s.Registrar().AddSystem<&WriteHealth>(In("WriteHealth", "FixedUpdate", "Update"));
    }, Client());
    CHECK_MESSAGE(Has(notRolled, "AU-E6 System 'WriteHealth'"), "[au-z6-system] " << notRolled);
    const std::string rolled = BuildText([](O::WorldSetup& s) {
        Step(s);
        A::AuthorityEntry(s).Component<Health>("Health");
        s.Registrar().AddSystem<&WriteHealth>(In("WriteHealth", "FixedUpdate", "Update"));
    }, Client());
    CHECK_MESSAGE(rolled == "built", "[au-z6-rolled] " << rolled);
    // Outside the resimulated schedules a System may write it.
    const std::string frame = BuildText([](O::WorldSetup& s) {
        Step(s);
        A::AuthorityEntry(s).Component<Health>("Health", {.rollback = A::RollbackChoice::NoRollback});
        s.Registrar().AddSystem<&WriteHealth>(In("WriteHealth", "Main", "Update"));
    }, Client());
    CHECK_MESSAGE(frame == "built", "[au-z6-frame] " << frame);
    // A hook, whoever triggers it, in a World with a step schedule.
    const std::string hook = BuildText([](O::WorldSetup& s) {
        Step(s);
        A::AuthorityEntry(s).Component<Health>("Health", {.rollback = A::RollbackChoice::NoRollback});
        s.Registrar().Hook<Health, &HealthHook>(K::HookTrigger::OnAdd, {.name = "HealthHook"});
    }, Client());
    CHECK_MESSAGE(Has(hook, "AU-E6 hook 'HealthHook'"), "[au-z6-hook] " << hook);
    // Through Registrar::InitialValue (the funnel): inserting the request fills the decision component.
    const std::string funnel = BuildText([](O::WorldSetup& s) {
        Step(s);
        s.Registrar().Component<Armor>({.name = "ArmorRequest"});
        A::AuthorityEntry(s).Component<Health>("Health", {.rollback = A::RollbackChoice::NoRollback});
        s.Registrar().InitialValue<Health, &HealthFromArmor>();
        s.Registrar().AddSystem<&FillRequest>(In("FillRequest", "FixedUpdate", "Update"));
    }, Client());
    CHECK_MESSAGE(Has(funnel, "AU-E6 System 'FillRequest'"), "[au-z6-funnel] " << funnel);
}

TEST_CASE("authority checks: AU-E6 leaves out the tick and messages") {
    const std::string text = BuildText([](O::WorldSetup& s) {
        // The frame schedule as the step schedule: the time Module's driver (RunFixedSteps, which writes Tick)
        // and a message writer run in a resimulated schedule.
        R::RollbackEntry(s).StepSchedule("Main", "Last");
        A::AuthorityEntry authority(s);
        authority.Mark<W::Tick>();
        authority.Message<Hit>("Hit");
        s.Registrar().AddSystem<&SendHit>(In("SendHit", "Main", "Update"));
    }, Client());
    CHECK_MESSAGE(text == "built", "[au-z6-exceptions] " << text);
}

TEST_CASE("authority checks: AU-E7 (Z8): a decision input is not rollback state") {
    const std::string text = BuildText([](O::WorldSetup& s) {
        A::AuthorityEntry(s).Resource<Command>("Command", {.input = true});
        R::RollbackEntry(s).Resource<Command>();
    });
    CHECK_MESSAGE(Has(text, "AU-E7 decision input 'Command'"), "[au-z8] " << text);
    const std::string history = BuildText([](O::WorldSetup& s) {
        A::AuthorityEntry(s).Resource<Command>("Command", {.input = true});
        R::RollbackEntry(s).Input<Command>(64);
    });
    CHECK_MESSAGE(history == "built", "[au-z8-input-history] " << history);
    // A component input too; and Derived is no way out (the kernel's old Z8: any rollback but No).
    const std::string component = BuildText([](O::WorldSetup& s) {
        A::AuthorityEntry(s).Component<Health>("Health", {.input = true});
        R::RollbackEntry(s).Component<Health>();
    });
    CHECK_MESSAGE(Has(component, "AU-E7 decision input 'Health'"), "[au-z8-component] " << component);
    const std::string derived = BuildText([](O::WorldSetup& s) {
        A::AuthorityEntry(s).Component<Health>("Health", {.input = true});
        R::RollbackEntry(s).Derived<Health>();
    });
    CHECK_MESSAGE(Has(derived, "AU-E7 decision input 'Health' is registered in the rollback Module as Derived"), "[au-z8-derived] " << derived);
}

TEST_CASE("authority checks: AU-E8 (D-E15): an entry in a resimulated schedule is skipped while resimulating") {
    // An author's strategy that adds the entry without the run condition (the official one gives it).
    class Bare final : public A::ServerAuthorityStrategy {
    public:
        void OnRegister(A::RegisterContext& context) override {
            if (context.Ingress() != nullptr) {
                context.AddIngress();
                return;
            }
            ServerAuthorityStrategy::OnRegister(context);
        }
    };
    Setting bare = Client();
    bare.authority = std::make_shared<Bare>();
    const auto build = [](O::WorldSetup& s) {
        Step(s);
        A::AuthorityEntry authority(s);
        authority.Resource<Command>("Command", {.input = true});
        authority.Ingress<&ReadCommand>(In("ReadCommand", "FixedUpdate", "First"));
    };
    const std::string text = BuildText(build, bare);
    CHECK_MESSAGE(Has(text, "AU-E8 entry System 'ReadCommand'"), "[au-ingress-not-skipped] " << text);
    CHECK_MESSAGE(BuildText(build, Client()) == "built", "[au-ingress-not-skipped] the official strategy");
    // Skipped while resimulating, the entry is not counted by Z6. Command above is left out of Z6 by its kept
    // history too (the official strategy registers rollback.Input<T> for a resource input); a component input has
    // no kept history (rollback.Input<T> is for resources), so the skip is its only way out.
    const std::string component = BuildText([](O::WorldSetup& s) {
        Step(s);
        A::AuthorityEntry authority(s);
        authority.Component<Health>("Health", {.input = true});
        authority.Ingress<&WriteHealth>(In("WriteHealth", "FixedUpdate", "First"));
    }, Client());
    CHECK_MESSAGE(component == "built", "[au-z6-skipped-entry] " << component);
}

TEST_CASE("authority checks: AU-E6 and AU-E8 ask each mount whether it runs while resimulating, not the System's name") {
    // SkipDuringResimulation puts its condition on the mounts it is handed; a mount added after it runs again.
    const std::string later = BuildText([](O::WorldSetup& s) {
        Step(s);
        A::AuthorityEntry(s).Component<Health>("Health", {.rollback = A::RollbackChoice::NoRollback});
        K::SystemOptions options = In("WriteHealth", "Main", "Update");
        R::RollbackEntry(s).SkipDuringResimulation(options);
        options.mounts.push_back(K::Mount{"FixedUpdate", "Update"});
        s.Registrar().AddSystem<&WriteHealth>(options);
    }, Client());
    CHECK_MESSAGE(Has(later, "AU-E6 System 'WriteHealth'"), "[au-z6-mount-added-after-skip] " << later);
    // Every mount skipped: the System does not write during a resimulation.
    const std::string all = BuildText([](O::WorldSetup& s) {
        Step(s);
        A::AuthorityEntry(s).Component<Health>("Health", {.rollback = A::RollbackChoice::NoRollback});
        K::SystemOptions options = In("WriteHealth", "FixedUpdate", "Update");
        R::RollbackEntry(s).SkipDuringResimulation(options);
        s.Registrar().AddSystem<&WriteHealth>(options);
    }, Client());
    CHECK_MESSAGE(all == "built", "[au-z6-mount-added-after-skip] every mount skipped: " << all);
}

TEST_CASE("authority checks: AU-E9: a Runs System in a resimulated schedule runs nested schedules the rollback Module knows") {
    const auto build = [](bool nested) {
        return [nested](O::WorldSetup& s) {
            Step(s);
            K::ScheduleOptions fog;
            fog.name = "OnEnter_Fog";
            fog.stages = {"S"};
            s.Registrar().Schedule(std::move(fog));
            if (nested) R::RollbackEntry(s).NestedSchedule("OnEnter_Fog", "FixedUpdate");
            s.Registrar().AddSystem<&RunNested>(In("ApplyState", "FixedUpdate", "Update"));
            A::AuthorityEntry(s).Runs("ApplyState", {"OnEnter_Fog"});
        };
    };
    const std::string missing = BuildText(build(false), Client());
    CHECK_MESSAGE(Has(missing, "AU-E9 System 'ApplyState'"), "[au-runs-nested] " << missing);
    CHECK_MESSAGE(BuildText(build(true), Client()) == "built", "[au-runs-nested] registered");
}

TEST_CASE("authority checks: AU-E5 sees an ExclusiveWorld<Writes<Tick>> annotation, a hook and an observer") {
    const std::string annotated = BuildText([](O::WorldSetup& s) {
        A::AuthorityEntry(s).Mark<W::Tick>();
        s.Registrar().AddSystem<&AnnotateTick>(In("AnnotateTick", "Main", "Update"));
    });
    CHECK_MESSAGE(Has(annotated, "AU-E5 System 'AnnotateTick'"), "[au-tick-annotated] " << annotated);
    const std::string hook = BuildText([](O::WorldSetup& s) {
        A::AuthorityEntry(s).Mark<W::Tick>();
        s.Registrar().Component<Armor>({.name = "Armor"});
        s.Registrar().Hook<Armor, &TickHook>(K::HookTrigger::OnAdd, {.name = "TickHook"});
    });
    CHECK_MESSAGE(Has(hook, "AU-E5 hook 'TickHook'"), "[au-tick-hook] " << hook);
    const std::string observer = BuildText([](O::WorldSetup& s) {
        A::AuthorityEntry(s).Mark<W::Tick>();
        s.Registrar().Event<HitLanded>({.name = "HitLanded"});
        s.Registrar().Observer<HitLanded, &TickObserver>({.name = "TickObserver"});
    });
    CHECK_MESSAGE(Has(observer, "AU-E5 observer 'TickObserver'"), "[au-tick-observer] " << observer);
}

TEST_CASE("authority checks: AU-E6 sees deferred writes, annotations, observers and the funnel's chains") {
    const auto notRolled = [](O::WorldSetup& s) {
        Step(s);
        A::AuthorityEntry(s).Component<Health>("Health", {.rollback = A::RollbackChoice::NoRollback});
    };
    const std::string deferred = BuildText([&](O::WorldSetup& s) {
        notRolled(s);
        s.Registrar().AddSystem<&InsertHealth>(In("InsertHealth", "FixedUpdate", "Update"));
    }, Client());
    CHECK_MESSAGE(Has(deferred, "AU-E6 System 'InsertHealth'"), "[au-z6-deferred] " << deferred);
    const std::string annotated = BuildText([&](O::WorldSetup& s) {
        notRolled(s);
        s.Registrar().AddSystem<&AnnotateHealth>(In("AnnotateHealth", "FixedUpdate", "Update"));
    }, Client());
    CHECK_MESSAGE(Has(annotated, "AU-E6 System 'AnnotateHealth'"), "[au-z6-annotated] " << annotated);
    const std::string observer = BuildText([&](O::WorldSetup& s) {
        notRolled(s);
        s.Registrar().Event<HitLanded>({.name = "HitLanded"});
        s.Registrar().Observer<HitLanded, &HealthObserver>({.name = "HealthObserver"});
    }, Client());
    CHECK_MESSAGE(Has(observer, "AU-E6 observer 'HealthObserver'"), "[au-z6-observer] " << observer);
    // A chain: inserting Armor fills Score, Score fills Health.
    const auto chain = [&](O::WorldSetup& s) {
        notRolled(s);
        s.Registrar().Component<Armor>({.name = "ArmorRequest"});
        s.Registrar().Component<Score>({.name = "ScoreRequest"});
        s.Registrar().InitialValue<Score, &ScoreFromArmor>();
        s.Registrar().InitialValue<Health, &HealthFromScore>();
    };
    const std::string through = BuildText([&](O::WorldSetup& s) {
        chain(s);
        s.Registrar().AddSystem<&FillRequest>(In("FillRequest", "FixedUpdate", "Update"));
    }, Client());
    CHECK_MESSAGE(Has(through, "AU-E6 System 'FillRequest'"), "[au-z6-funnel-chain] " << through);
    // A write in place or a remove of the request adds no cell and fills nothing.
    const std::string inPlace = BuildText([&](O::WorldSetup& s) {
        chain(s);
        s.Registrar().AddSystem<&WriteArmorInPlace>(In("WriteArmor", "FixedUpdate", "Update"));
        s.Registrar().AddSystem<&RemoveArmor>(In("RemoveArmor", "FixedUpdate", "Update"));
    }, Client());
    CHECK_MESSAGE(inPlace == "built", "[au-z6-funnel-not-filled] " << inPlace);
}

TEST_CASE("authority checks: AU-E6 accepts Derived for a System, not for a hook") {
    const std::string system = BuildText([](O::WorldSetup& s) {
        Step(s);
        A::AuthorityEntry(s).Component<Health>("Health", {.rollback = A::RollbackChoice::Derived});
        s.Registrar().AddSystem<&WriteHealth>(In("WriteHealth", "FixedUpdate", "PreUpdate"));
    }, Client());
    CHECK_MESSAGE(system == "built", "[au-z6-derived-system] " << system);
    const std::string hook = BuildText([](O::WorldSetup& s) {
        Step(s);
        A::AuthorityEntry(s).Component<Health>("Health", {.rollback = A::RollbackChoice::Derived});
        s.Registrar().Hook<Health, &HealthHook>(K::HookTrigger::OnAdd, {.name = "HealthHook"});
    }, Client());
    CHECK_MESSAGE(Has(hook, "AU-E6 hook 'HealthHook'"), "[au-z6-derived-hook] " << hook);
}

TEST_CASE("authority checks: AU-E6 leaves out an input whose history the rollback keeps, not one without") {
    // The default: the input's history comes on the author's behalf. Self: the author registers none (and here
    // keeps none), so a step that consumes it is reported.
    const auto build = [](bool history) {
        return [history](O::WorldSetup& s) {
            Step(s);
            A::AuthorityEntry(s).Resource<Command>(
                "Command", {.input = true, .rollback = history ? A::RollbackChoice::Rollback : A::RollbackChoice::Self});
            s.Registrar().AddSystem<&ConsumeCommand>(In("ConsumeCommand", "FixedUpdate", "Update"));
        };
    };
    const std::string kept = BuildText(build(true), Client());
    CHECK_MESSAGE(kept == "built", "[au-z6-input-history] " << kept);
    const std::string bare = BuildText(build(false), Client());
    CHECK_MESSAGE(Has(bare, "AU-E6 System 'ConsumeCommand'"), "[au-z6-input-no-history] " << bare);
}

TEST_CASE("authority checks: AU-E4 for an entry the strategy did not add and for a removed System") {
    class NoAdd final : public A::ServerAuthorityStrategy {
    public:
        void OnRegister(A::RegisterContext& context) override {
            if (context.Ingress() == nullptr) ServerAuthorityStrategy::OnRegister(context);
        }
    };
    Setting noAdd;
    noAdd.authority = std::make_shared<NoAdd>();
    const std::string entry = BuildText([](O::WorldSetup& s) {
        A::AuthorityEntry authority(s);
        authority.Resource<Command>("Command", {.input = true});
        authority.Ingress<&ReadCommand>(In("ReadCommand", "Main", "PreUpdate"));
    }, noAdd);
    CHECK_MESSAGE(Has(entry, "AU-E4 authority.Ingress: System 'ReadCommand' was not added"), "[au-names-ingress] " << entry);
    const std::string removed = BuildText([](O::WorldSetup& s) {
        s.Registrar().AddSystem<&RunNested>(In("ApplyState", "Main", "Update"));
        s.Registrar().RemoveSystem("ApplyState");
        A::AuthorityEntry(s).Runs("ApplyState", {"Main"});
    });
    CHECK_MESSAGE(Has(removed, "AU-E4 authority.Runs: no System named 'ApplyState'"), "[au-names-removed] " << removed);
}

TEST_CASE("authority checks: AU-E9 looks only at a Runs System in a resimulated schedule, and wants the nested registration") {
    const auto fog = [](O::WorldSetup& s) {
        K::ScheduleOptions schedule;
        schedule.name = "OnEnter_Fog";
        schedule.stages = {"S"};
        s.Registrar().Schedule(std::move(schedule));
    };
    const std::string outside = BuildText([&](O::WorldSetup& s) {
        Step(s);
        fog(s);
        s.Registrar().AddSystem<&RunNested>(In("ApplyState", "Main", "Update"));
        A::AuthorityEntry(s).Runs("ApplyState", {"OnEnter_Fog"});
    }, Client());
    CHECK_MESSAGE(outside == "built", "[au-runs-outside] " << outside);
    // The step schedule itself is not a nested registration.
    const std::string step = BuildText([&](O::WorldSetup& s) {
        Step(s);
        s.Registrar().AddSystem<&RunNested>(In("ApplyState", "FixedUpdate", "Update"));
        A::AuthorityEntry(s).Runs("ApplyState", {"FixedUpdate"});
    }, Client());
    CHECK_MESSAGE(Has(step, "AU-E9 System 'ApplyState'"), "[au-runs-step-not-nested] " << step);
}

TEST_CASE("authority checks: AU-E6 looks at the nested schedules too, and at hooks only in a World with a step schedule") {
    const std::string nested = BuildText([](O::WorldSetup& s) {
        Step(s);
        K::ScheduleOptions inner;
        inner.name = "Inner";
        inner.stages = {"S"};
        s.Registrar().Schedule(std::move(inner));
        R::RollbackEntry(s).NestedSchedule("Inner", "FixedUpdate");
        A::AuthorityEntry(s).Component<Health>("Health", {.rollback = A::RollbackChoice::NoRollback});
        s.Registrar().AddSystem<&WriteHealth>(In("WriteHealth", "Inner", "S"));
    }, Client());
    CHECK_MESSAGE(Has(nested, "AU-E6 System 'WriteHealth'"), "[au-z6-nested] " << nested);
    // A World with no step schedule (a Match): a hook may write decision data that does not roll back.
    const std::string match = BuildText([](O::WorldSetup& s) {
        A::AuthorityEntry(s).Component<Health>("Health", {.rollback = A::RollbackChoice::NoRollback});
        s.Registrar().Hook<Health, &HealthHook>(K::HookTrigger::OnAdd, {.name = "HealthHook"});
    });
    CHECK_MESSAGE(match == "built", "[au-z6-hook-without-step] " << match);
    // An observer may not take Derived either (as a hook).
    const std::string observer = BuildText([](O::WorldSetup& s) {
        Step(s);
        A::AuthorityEntry(s).Component<Health>("Health", {.rollback = A::RollbackChoice::Derived});
        s.Registrar().Event<HitLanded>({.name = "HitLanded"});
        s.Registrar().Observer<HitLanded, &HealthObserver>({.name = "HealthObserver"});
    }, Client());
    CHECK_MESSAGE(Has(observer, "AU-E6 observer 'HealthObserver'"), "[au-z6-derived-observer] " << observer);
}

TEST_CASE("authority checks: a type marked by type or by name is decision data for AU-E6") {
    const auto marked = [](bool byName) {
        return [byName](O::WorldSetup& s) {
            Step(s);
            s.Registrar().Resource<Score>({.name = "Score"});
            if (byName) {
                A::AuthorityEntry(s).MarkResource("Score");
            } else {
                A::AuthorityEntry(s).Mark<Score>();
            }
            s.Registrar().AddSystem<&WriteScore>(In("WriteScore", "FixedUpdate", "Update"));
        };
    };
    const std::string byType = BuildText(marked(false), Client());
    CHECK_MESSAGE(Has(byType, "AU-E6 System 'WriteScore'"), "[au-z6-mark-type] " << byType);
    const std::string byName = BuildText(marked(true), Client());
    CHECK_MESSAGE(Has(byName, "AU-E6 System 'WriteScore'"), "[au-z6-mark-name] " << byName);
}

TEST_CASE("authority checks: AU-E8 leaves an entry outside the resimulated schedules alone; AU-E9 sees a Runs System by any of its mounts") {
    class Bare final : public A::ServerAuthorityStrategy {
    public:
        void OnRegister(A::RegisterContext& context) override {
            if (context.Ingress() != nullptr) {
                context.AddIngress();
                return;
            }
            ServerAuthorityStrategy::OnRegister(context);
        }
    };
    Setting bare = Client();
    bare.authority = std::make_shared<Bare>();
    const std::string frame = BuildText([](O::WorldSetup& s) {
        Step(s);
        A::AuthorityEntry authority(s);
        authority.Resource<Command>("Command", {.input = true});
        authority.Ingress<&ReadCommand>(In("ReadCommand", "Main", "PreUpdate"));
    }, bare);
    CHECK_MESSAGE(frame == "built", "[au-ingress-frame] " << frame);
    const std::string twice = BuildText([](O::WorldSetup& s) {
        Step(s);
        K::ScheduleOptions fog;
        fog.name = "OnEnter_Fog";
        fog.stages = {"S"};
        s.Registrar().Schedule(std::move(fog));
        K::SystemOptions apply = In("ApplyState", "Main", "Update");
        apply.mounts.push_back(K::Mount{"FixedUpdate", "Update"});
        s.Registrar().AddSystem<&RunNested>(apply);
        A::AuthorityEntry(s).Runs("ApplyState", {"OnEnter_Fog"});
    }, Client());
    CHECK_MESSAGE(Has(twice, "AU-E9 System 'ApplyState'"), "[au-runs-two-mounts] " << twice);
}

TEST_CASE("authority checks: the funnel: every declaration of a request, a chain through a component written in place, and no read") {
    const auto notRolled = [](O::WorldSetup& s) {
        Step(s);
        A::AuthorityEntry(s).Component<Health>("Health", {.rollback = A::RollbackChoice::NoRollback});
        s.Registrar().Component<Armor>({.name = "ArmorRequest"});
        s.Registrar().Component<Score>({.name = "ScoreRequest"});
    };
    // Armor is the request of two declarations; Score is registered before Health, so the ordinary target comes
    // first whichever order the read-only registry keeps them in (registration or component id). Both are filled.
    const std::string both = BuildText([&](O::WorldSetup& s) {
        Step(s);
        s.Registrar().Component<Score>({.name = "ScoreRequest"});
        s.Registrar().Component<Armor>({.name = "ArmorRequest"});
        A::AuthorityEntry(s).Component<Health>("Health", {.rollback = A::RollbackChoice::NoRollback});
        s.Registrar().InitialValue<Score, &ScoreFromArmor>();
        s.Registrar().InitialValue<Health, &HealthFromArmor>();
        s.Registrar().AddSystem<&FillRequest>(In("FillRequest", "FixedUpdate", "Update"));
    }, Client());
    CHECK_MESSAGE(Has(both, "AU-E6 System 'FillRequest'"), "[au-funnel-every-declaration] " << both);
    // The chain Armor -> Score -> Health goes on through Score, which the System also writes in place.
    const std::string through = BuildText([&](O::WorldSetup& s) {
        notRolled(s);
        s.Registrar().InitialValue<Score, &ScoreFromArmor>();
        s.Registrar().InitialValue<Health, &HealthFromScore>();
        s.Registrar().AddSystem<&FillAndWriteScore>(In("FillAndWriteScore", "FixedUpdate", "Update"));
    }, Client());
    CHECK_MESSAGE(Has(through, "AU-E6 System 'FillAndWriteScore'"), "[au-funnel-through-written] " << through);
    // Reading the request adds no cell.
    const std::string read = BuildText([&](O::WorldSetup& s) {
        notRolled(s);
        s.Registrar().InitialValue<Health, &HealthFromArmor>();
        s.Registrar().AddSystem<&ReadArmor>(In("ReadArmor", "FixedUpdate", "Update"));
    }, Client());
    CHECK_MESSAGE(read == "built", "[au-funnel-read-not-filled] " << read);
}
