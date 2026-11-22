// The combination cases of the state machine declaration (docs/architecture/target/v2/state-machine-declaration.zh-Hant.md,
// section 7): how the kernel, the orchestration layer and the other Modules meet this Module. They fix actual
// behaviour and do not count toward judgement 2.
#include "StateMachineTestSupport.hpp"

#include <algorithm>

#include "engine/authority/AuthorityModule.hpp"
#include "engine/authority/Consistency.hpp"
#include "engine/authority/Registrations.hpp"
#include "engine/authority/ServerAuthorityStrategy.hpp"
#include "engine/rollback/Inbox.hpp"
#include "engine/rollback/Registrations.hpp"
#include "engine/rollback/RollbackModule.hpp"
#include "engine/rollback/StateRollbackStrategy.hpp"
#include "engine/world_time/TimeTypes.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

using namespace GYO::Test::StateMachine;

namespace {

struct Button {
    std::int32_t id = 0;
};
inline void HashValue(K::Hasher& h, const Button& v) { HashValue(h, v.id); }

K::SystemResult SpawnButton(K::SpawnWithId<K::Bundle<Button>> spawn) {
    spawn.New(100, Button{1});
    return {};
}
K::SystemResult CountButtons(K::Query<K::Read<Button>> buttons) {
    Events().push_back("buttons " + std::to_string(buttons.Count()));
    return {};
}

} // namespace

TEST_CASE("combination: an entity spawned in an enter is seen by a System after the apply the same frame") {
    Setup s;
    s.extra = [](O::WorldSetup& w) {
        K::Registrar& r = w.Registrar();
        r.Component<Button>({.name = "Button"});
        r.AddSystem<&SpawnButton>(At("SpawnButton", "Flow.Enter.MainMenu", std::string(SM::kEnterExitStage)));
        r.AddSystem<&CountButtons>(
            At("CountButtons", std::string(O::DefaultSchedules::frame), std::string(O::DefaultSchedules::update)));
    };
    auto p = Make(std::move(s));
    REQUIRE(p);
    CHECK(Frame(*p) == List{"Enter.Flow.MainMenu", "buttons 1"});
}

TEST_CASE("combination: registering one state twice is the kernel's R-E2") {
    Events().clear();
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = "Main";
    w.build = [](O::WorldSetup& s) {
        SM::RegisterStateMachine<Flow>(s);
        SM::RegisterStateMachine<Flow>(s);
    };
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), {});
    REQUIRE_FALSE(built);
    CHECK(built.error().detail.find("R-E2") != std::string::npos);
}

TEST_CASE("combination: a replaced World starts from the initial value; SetInitial in ReplaceRequest.extra enters that value") {
    auto p = Make({});
    REQUIRE(p);
    Frame(*p);
    Req<Flow>(*p).Request(Flow::InLevel);
    Frame(*p);
    REQUIRE(p->ReplaceWorld("Main", O::ReplaceRequest{}));
    CHECK(Mach<Flow>(*p).Status() == SM::StateStatus::NotEntered);
    CHECK(Frame(*p) == List{"Enter.Flow.MainMenu"});
    O::ReplaceRequest load;
    load.extra = [](K::World& world) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.ResourceMut<SM::StateMachine<Flow>>()->SetInitial(Flow::InLevel);
        return {};
    };
    REQUIRE(p->ReplaceWorld("Main", std::move(load)));
    CHECK(Frame(*p) == List{"Enter.Flow.InLevel", "Enter.Pause.Running", "Enter.Inventory.Closed"});
}

TEST_CASE("combination: two trees at one anchor: the later registered stage sits next to the anchor and applies first") {
    Setup s;
    s.weather = true;  // registered after Flow, both right after Main/PreUpdate
    auto p = Make(std::move(s));
    REQUIRE(p);
    CHECK(Frame(*p) == List{"Enter.Weather.Clear", "Enter.Flow.MainMenu"});
    Req<Flow>(*p).Request(Flow::InLevel);
    Req<Weather>(*p).Request(Weather::Rain);
    const List events = Frame(*p);
    REQUIRE(events.size() >= 3);
    CHECK(events[0] == "Exit.Weather.Clear");
    CHECK(events[1] == "Enter.Weather.Rain");
    CHECK(events[2] == "Exit.Flow.MainMenu");
}

namespace {
K::SystemResult HoldTwice(K::ResMut<O::WorldGeneration> generation) {
    static int frames = 0;
    if (frames++ % 3 < 2) generation->Hold();
    return {};
}
} // namespace

TEST_CASE("combination: without conditions the apply System enters the initial value in the first frame, while generating") {
    Setup s;
    s.extra = [](O::WorldSetup& w) {
        w.Registrar().AddSystem<&HoldTwice>(At("HoldTwice", std::string(O::DefaultSchedules::frame),
                                                std::string(O::DefaultSchedules::update), {O::WhileGenerating()}));
    };
    auto p = Make(std::move(s));
    REQUIRE(p);
    CHECK(Frame(*p) == List{"Enter.Flow.MainMenu"});
    CHECK(p->Generating("Main"));
}

namespace {
struct LevelSelected {
    std::int32_t level = 0;
};
bool& SendSelected() {
    static bool send = false;
    return send;
}
K::SystemResult SelectLevel(K::MessageWriter<LevelSelected> out) {
    if (SendSelected()) out.Send(LevelSelected{3});
    return {};
}
K::SystemResult ReadSelected(K::MessageReader<LevelSelected> in) {
    int count = 0;
    for (const LevelSelected* m : in.Read()) {
        (void)m;
        ++count;
    }
    Events().push_back("selected " + std::to_string(count));
    return {};
}
} // namespace

TEST_CASE("combination: a reader in an enter schedule: nothing the first time, everything since the last entry after that") {
    Setup s;
    s.extra = [](O::WorldSetup& w) {
        K::Registrar& r = w.Registrar();
        r.Message<LevelSelected>({.name = "LevelSelected"});
        r.AddSystem<&SelectLevel>(
            At("SelectLevel", std::string(O::DefaultSchedules::frame), std::string(O::DefaultSchedules::update)));
        r.AddSystem<&ReadSelected>(At("ReadSelected", "Flow.Enter.InLevel", std::string(SM::kEnterExitStage)));
    };
    auto p = Make(std::move(s));
    REQUIRE(p);
    Frame(*p);
    SendSelected() = true;  // the level is chosen this frame, the request applies next frame
    Frame(*p);
    SendSelected() = false;
    Req<Flow>(*p).Request(Flow::InLevel);
    List first = Frame(*p);
    CHECK(std::find(first.begin(), first.end(), "selected 0") != first.end());
    // Out and back in: two levels chosen meanwhile, read at once.
    Req<Flow>(*p).Request(Flow::MainMenu);
    SendSelected() = true;
    Frame(*p);
    Frame(*p);
    SendSelected() = false;
    Req<Flow>(*p).Request(Flow::InLevel);
    List second = Frame(*p);
    CHECK(std::find(second.begin(), second.end(), "selected 2") != second.end());
}

// ---- (c) "both ends, same rule" (declaration section 5.3): the server authority strategy and the state rollback.

namespace {

namespace A = ::GYO::Engine::Authority;
namespace R = ::GYO::Engine::Rollback;
namespace W = ::GYO::Engine::WorldTime;

constexpr W::Nanoseconds kStep = 15'625'000;  // 64 Hz, one step a frame

struct EnterCounts {
    std::int32_t runs = 0;
    std::int32_t sounds = 0;
};
inline void HashValue(K::Hasher& h, const EnterCounts& v) {
    HashValue(h, v.runs);
    HashValue(h, v.sounds);
}

// The rule both ends run: it starts raining at tick 5.
K::SystemResult WeatherRule(K::Res<W::Tick> tick, K::ResMut<SM::StateMachineRequest<Weather>> weather) {
    if (tick->value == 5) weather->Request(Weather::Rain);
    return {};
}
// "Enter Rain": the world change runs again in a resimulation; the sound only the first time (Resimulation).
K::SystemResult EnterRain(K::ResMut<EnterCounts> counts, K::Res<W::Resimulation> resimulation) {
    ++counts->runs;
    if (resimulation->active == 0) ++counts->sounds;
    return {};
}

O::RegisteredUnits AllUnits() {
    O::RegisteredUnits units;
    auto row = [](std::string name, std::vector<std::string> depends) {
        O::RegisterRow r;
        r.name = std::move(name);
        r.kind = O::UnitKind::Module;
        r.libraryOnly = false;
        r.defaultOn = true;
        r.depends = std::move(depends);
        return r;
    };
    units.rows.push_back(row("world_time", {}));
    units.modules.push_back({"world_time", std::make_unique<W::WorldTimeModule>()});
    units.rows.push_back(row("rollback", {"world_time"}));
    units.modules.push_back({"rollback", std::make_unique<R::RollbackModule>()});
    units.rows.push_back(row("authority", {"world_time", "rollback"}));
    units.modules.push_back({"authority", std::make_unique<A::AuthorityModule>()});
    return units;
}

void BuildWeather(O::WorldSetup& s) {
    K::Registrar& r = s.Registrar();
    SM::RegisterStateMachine<Weather>(s, {.schedule = "FixedUpdate", .anchor = "First"});
    A::AuthorityEntry authority(s);
    authority.Mark<SM::StateMachine<Weather>>();
    authority.Mark<SM::StateMachineRequest<Weather>>();
    authority.Runs(SM::ApplySystemName<Weather>(),
                   {SM::EnterScheduleName(Weather::Clear), SM::ExitScheduleName(Weather::Clear),
                    SM::EnterScheduleName(Weather::Rain), SM::ExitScheduleName(Weather::Rain)});
    R::RollbackEntry rollback(s);
    rollback.StepSchedule("FixedUpdate", "Last");
    rollback.Resource<SM::StateMachine<Weather>>();
    rollback.Resource<SM::StateMachineRequest<Weather>>();
    for (Weather v : {Weather::Clear, Weather::Rain}) {
        rollback.NestedSchedule(SM::EnterScheduleName(v), "FixedUpdate");
        rollback.NestedSchedule(SM::ExitScheduleName(v), "FixedUpdate");
    }
    r.Resource<EnterCounts>({.name = "EnterCounts"});
    r.AddSystem<&WeatherRule>(At("WeatherRule", "FixedUpdate", "First"));
    r.AddSystem<&EnterRain>(At("EnterRain", SM::EnterScheduleName(Weather::Rain), std::string(SM::kEnterExitStage)));
}

std::unique_ptr<O::Product> MakeWeather(bool withRollback) {
    O::ProductDescription d;
    d.name = "duel";
    O::WorldDescription w;
    w.name = "Main";
    w.dynamicInsert = false;
    w.options.Set(A::AuthorityOptions{std::make_shared<A::ServerAuthorityStrategy>()});
    if (withRollback) {
        R::StateRollbackSettings settings;
        settings.depth = 20;
        settings.mode = R::RollbackMode::Always;
        w.options.Set(R::RollbackOptions{std::make_shared<R::StateRollbackStrategy>(settings)});
    }
    w.build = &BuildWeather;
    w.prepare.push_back([](K::World& world) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(W::TimeSampling{kStep, W::SamplingMode::ManualDelta});
        world.SetResource(EnterCounts{});
        return {};
    });
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), AllUnits());
    if (!built) {
        std::string out = built.error().detail;
        for (const O::UnitReportedError& e : built.error().unitErrors) out += "\n" + e.code + " " + e.message;
        FAIL_CHECK(out);
        return nullptr;
    }
    return std::move(*built);
}

} // namespace

TEST_CASE("combination: (c) both ends, same rule: marked, Runs, rolled back; the digest follows; a resimulation re-enters") {
    auto p = MakeWeather(true);
    REQUIRE(p);  // no AU-E4 (marks and Runs found), AU-E6 (both Resources roll back), AU-E9 (nested registered)
    K::World& world = WorldOf(*p);
    for (int i = 0; i < 3; ++i) REQUIRE(p->RunFrame());
    const auto before = A::Digest(world);
    REQUIRE(before);
    for (int i = 0; i < 6; ++i) REQUIRE(p->RunFrame());
    REQUIRE(world.Resource<W::Tick>()->value == 9);
    CHECK(world.Resource<SM::StateMachine<Weather>>()->Current() == Weather::Rain);
    const auto after = A::Digest(world);
    REQUIRE(after);
    CHECK(*before != *after);
    CHECK(world.Resource<EnterCounts>()->runs == 1);
    CHECK(world.Resource<EnterCounts>()->sounds == 1);
    // The authority confirms step 3: mode Always goes back to it and runs steps 4 to 9 again; the change at 5 again.
    world.ResourceMut<R::RollbackInbox>()->ConfirmedThrough(3);
    REQUIRE(p->RunFrame());
    CHECK(world.Resource<SM::StateMachine<Weather>>()->Current() == Weather::Rain);
    CHECK(world.Resource<EnterCounts>()->runs == 2);
    CHECK(world.Resource<EnterCounts>()->sounds == 1);
}

// ---- The message cursor contract under state conditions (declaration section 2.3, consequences 1 and 2; the user
// decided it is the author's to handle: v8-arch-gyo2-event-cursor-reeval-author-side). Fixes today's behaviour.

namespace {
struct ButtonPressed {
    std::int32_t id = 0;
};
std::int32_t& PressesToSend() {
    static std::int32_t presses = 0;
    return presses;
}
K::SystemResult PressButtons(K::MessageWriter<ButtonPressed> out) {
    for (; PressesToSend() > 0; --PressesToSend()) out.Send(ButtonPressed{PressesToSend()});
    return {};
}
K::SystemResult MenuButtons(K::MessageReader<ButtonPressed> in) {
    int count = 0;
    for (const ButtonPressed* m : in.Read()) {
        (void)m;
        ++count;
    }
    Events().push_back("menu read " + std::to_string(count));
    return {};
}
} // namespace

TEST_CASE("combination: a reader held back by a state condition holds the messages; it reads the old ones when it runs again") {
    Setup s;
    s.extra = [](O::WorldSetup& w) {
        K::Registrar& r = w.Registrar();
        const std::string main(O::DefaultSchedules::frame);
        r.Message<ButtonPressed>({.name = "ButtonPressed", .capacity = 4});
        r.AddSystem<&PressButtons>(At("PressButtons", main, std::string(O::DefaultSchedules::preUpdate)));
        r.AddSystem<&MenuButtons>(
            At("MenuButtons", main, std::string(O::DefaultSchedules::update), {SM::InState<Flow, Flow::MainMenu>()}));
    };
    auto p = Make(std::move(s));
    REQUIRE(p);
    CHECK(Frame(*p) == List{"Enter.Flow.MainMenu", "menu read 0"});
    Req<Flow>(*p).Request(Flow::InLevel);
    Frame(*p);
    // Consequence 1: in the level the reader does not run, but it ran before, so it holds; past the capacity each
    // write drops the oldest and the World records it, naming the reader.
    PressesToSend() = 6;
    Frame(*p);
    const auto dropped = WorldOf(*p).TakeDiagnostics<K::DroppedMessages>();
    REQUIRE(dropped.size() == 1);
    CHECK(dropped[0].count == 2);
    REQUIRE(dropped[0].readers.size() == 1);
    CHECK(dropped[0].readers[0].system == "MenuButtons");
    // Consequence 2: back in the main menu, it reads what the level wrote, all at once.
    Req<Flow>(*p).Request(Flow::MainMenu);
    const List back = Frame(*p);
    REQUIRE_FALSE(back.empty());
    CHECK(back.back() == "menu read 4");
}
