// The server authority strategy's startup checks (v8-arch-gyo2-authority-rollback
// step 5; authority declaration 5.4, 5.5, 5.11): AU-E3 dynamic insertion, AU-E4
// names, AU-E5 the tick's one source, AU-E6 (Z6), AU-E7 (Z8), AU-E8 (D-E15),
// AU-E9 (Runs × nested). Each CHECK carries a tag in brackets ("[au-...]").

#include "AuthorityTestSupport.hpp"

using namespace AuthorityTest;

namespace {

K::SystemResult WriteHealth(K::Query<K::Write<Health>>) { return {}; }
K::SystemResult WriteTick(K::ResMut<W::Tick>) { return {}; }
K::SystemResult ReadCommand(K::ResMut<Command>) { return {}; }
K::SystemResult HealthHook(K::HookCtx, K::Query<K::Write<Health>>) { return {}; }
K::SystemResult RunNested(K::ExclusiveWorld<>) { return {}; }
K::SystemResult FillRequest(K::CommandsFor<K::Inserts<Armor>, K::Removes<>>) { return {}; }
K::SystemResult SendHit(K::MessageWriter<Hit>) { return {}; }
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
    CHECK_MESSAGE(Has(text, "AU-E4 authority.OneEndOnly: no System named 'NoPanel'"), "[au-names] " << text);
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
