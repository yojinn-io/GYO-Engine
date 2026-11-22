// Judgement 1 of the state machine declaration (docs/architecture/target/v2/state-machine-declaration.zh-Hant.md,
// section 7): the behaviour the user decided (revision 7 section 7 judgement 1, with revision 8's changes). This
// target links only gyo_state_machine (with the kernel and the orchestration layer it links): it compiling is half of
// Design Invariant 8's guard.
#include "StateMachineTestSupport.hpp"

#include <type_traits>

using namespace GYO::Test::StateMachine;

namespace {

struct Settings {
    std::int32_t buttons = 0;
};
inline void HashValue(K::Hasher& h, const Settings& v) { HashValue(h, v.buttons); }

std::int32_t& SeenButtons() {
    static std::int32_t seen = -1;
    return seen;
}

K::SystemResult PutSettings(K::ResMut<Settings> settings) {
    settings->buttons = 3;
    return {};
}
K::SystemResult ReadSettings(K::Res<Settings> settings) {
    SeenButtons() = settings->buttons;
    return {};
}
K::SystemResult InitRequestsLevel(K::ResMut<SM::StateMachineRequest<Flow>> flow) {
    flow->Request(Flow::InLevel);
    return {};
}
K::SystemResult InitRequestsPaused(K::ResMut<SM::StateMachineRequest<Pause>> pause) {
    pause->Request(Pause::Paused);
    return {};
}

// A product whose game initialization puts Settings (and maybe makes a request), and whose "enter MainMenu" reads it.
Setup WithGameInit(bool requestLevel, bool requestPaused) {
    Setup s;
    s.gameInit = {"Init"};
    s.extra = [requestLevel, requestPaused](O::WorldSetup& w) {
        K::Registrar& r = w.Registrar();
        r.Resource<Settings>({.name = "Settings"});
        r.Schedule({.name = "Init", .stages = {"Update"}});
        r.AddSystem<&PutSettings>(At("PutSettings", "Init", "Update"));
        if (requestLevel) r.AddSystem<&InitRequestsLevel>(At("InitRequestsLevel", "Init", "Update", {}, {}));
        if (requestPaused) r.AddSystem<&InitRequestsPaused>(At("InitRequestsPaused", "Init", "Update"));
        r.AddSystem<&ReadSettings>(At("ReadSettings", "Flow.Enter.MainMenu", "Update"));
    };
    s.prepare.push_back([](K::World& world) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(Settings{});
        return {};
    });
    return s;
}

// The main tree in InLevel, Running, Closed after two frames.
std::unique_ptr<O::Product> InLevel(Setup s = {}) {
    auto p = Make(std::move(s));
    REQUIRE(p);
    Frame(*p);
    Req<Flow>(*p).Request(Flow::InLevel);
    Frame(*p);
    return p;
}

} // namespace

TEST_CASE("behaviour: initial enter: after game initialization, once, reading its settings and the initial value") {
    SeenButtons() = -1;
    auto p = Make(WithGameInit(false, false));
    REQUIRE(p);
    CHECK(Frame(*p) == List{"Enter.Flow.MainMenu"});
    CHECK(SeenButtons() == 3);
    CHECK(Seen()["Enter.Flow.MainMenu"] == "F=MainMenu,P=-,I=-");
    CHECK(Frame(*p).empty());
}

TEST_CASE("behaviour: a request made in game initialization waits for the next apply") {
    auto p = Make(WithGameInit(true, false));
    REQUIRE(p);
    CHECK(Frame(*p) == List{"Enter.Flow.MainMenu"});
    CHECK(Frame(*p) == List{"Exit.Flow.MainMenu", "Enter.Flow.InLevel", "Enter.Pause.Running", "Enter.Inventory.Closed"});
}

TEST_CASE("behaviour: a child's request in game initialization is not taken by the first apply, then dropped") {
    auto p = Make(WithGameInit(false, true));
    REQUIRE(p);
    CHECK(Frame(*p) == List{"Enter.Flow.MainMenu"});
    CHECK(Req<Pause>(*p).Pending() == Pause::Paused);
    CHECK(Frame(*p).empty());
    CHECK_FALSE(Req<Pause>(*p).Pending().has_value());
    Req<Flow>(*p).Request(Flow::InLevel);
    Frame(*p);
    CHECK(Mach<Pause>(*p).Current() == Pause::Running);
}

TEST_CASE("behaviour: conditions are false before the first apply") {
    Setup s;
    s.extra = [](O::WorldSetup& w) {
        K::Registrar& r = w.Registrar();
        const std::string main(O::DefaultSchedules::frame);
        r.AddSystem<&Probe<1>>(At("Probe1", main, std::string(O::DefaultSchedules::preUpdate),
                                  {SM::InState<Flow, Flow::MainMenu>()}));
        r.AddSystem<&Probe<2>>(At("Probe2", main, std::string(O::DefaultSchedules::preUpdate),
                                  {SM::NotInState<Flow, Flow::InLevel>()}));
        r.AddSystem<&Probe<3>>(At("Probe3", main, std::string(O::DefaultSchedules::update),
                                  {SM::InState<Flow, Flow::MainMenu>()}));
        r.AddSystem<&Probe<4>>(At("Probe4", main, std::string(O::DefaultSchedules::update),
                                  {SM::NotInState<Flow, Flow::InLevel>()}));
    };
    auto p = Make(std::move(s));
    REQUIRE(p);
    CHECK(Frame(*p) == List{"Enter.Flow.MainMenu", "probe3:MainMenu", "probe4:MainMenu"});
    CHECK(Frame(*p) == List{"probe1:MainMenu", "probe2:MainMenu", "probe3:MainMenu", "probe4:MainMenu"});
}

TEST_CASE("behaviour: changing state: the old value's exit, then the new value's enter, and the conditions follow") {
    Setup s;
    s.extra = [](O::WorldSetup& w) {
        K::Registrar& r = w.Registrar();
        const std::string main(O::DefaultSchedules::frame);
        const std::string update(O::DefaultSchedules::update);
        r.AddSystem<&Probe<1>>(At("Probe1", main, update, {SM::InState<Flow, Flow::InLevel>()}));
        r.AddSystem<&Probe<2>>(At("Probe2", main, update, {SM::InState<Flow, Flow::MainMenu>()}));
        r.AddSystem<&Probe<3>>(At("Probe3", main, update, {SM::NotInState<Flow, Flow::InLevel>()}));
        r.AddSystem<&Probe<4>>(At("Probe4", main, update, {SM::NotInState<Flow, Flow::MainMenu>()}));
    };
    auto p = Make(std::move(s));
    REQUIRE(p);
    Frame(*p);
    Req<Flow>(*p).Request(Flow::InLevel);
    // Only the old value's exit and the new value's enter (and the children appearing): the right schedules.
    CHECK(Frame(*p) == List{"Exit.Flow.MainMenu", "Enter.Flow.InLevel", "Enter.Pause.Running", "Enter.Inventory.Closed",
                            "probe1:InLevel", "probe4:InLevel"});
    // The value an exit reads is the old one, an enter's the new one.
    CHECK(Seen()["Exit.Flow.MainMenu"] == "F=MainMenu,P=-,I=-");
    CHECK(Seen()["Enter.Flow.InLevel"] == "F=InLevel,P=-,I=-");
    // No request: nothing changes.
    CHECK(Frame(*p) == List{"probe1:InLevel", "probe4:InLevel"});
}

TEST_CASE("behaviour: one request per state per apply: a request made in an exit or an enter waits") {
    SUBCASE("in the parent's enter") {
        auto p = Make({});
        REQUIRE(p);
        Frame(*p);
        Actions()["Enter.Flow.InLevel"] = [](Ctx& c) -> K::SystemResult { c.flow.Request(Flow::MainMenu); return {}; };
        Req<Flow>(*p).Request(Flow::InLevel);
        Frame(*p);
        CHECK(Mach<Flow>(*p).Current() == Flow::InLevel);
        Actions().clear();
        CHECK(Frame(*p) == List{"Exit.Pause.Running", "Exit.Inventory.Closed", "Exit.Flow.InLevel", "Enter.Flow.MainMenu"});
    }
    SUBCASE("in the parent's exit") {
        auto p = Make({});
        REQUIRE(p);
        Frame(*p);
        Actions()["Exit.Flow.MainMenu"] = [](Ctx& c) -> K::SystemResult { c.flow.Request(Flow::MainMenu); return {}; };
        Req<Flow>(*p).Request(Flow::InLevel);
        Frame(*p);
        CHECK(Mach<Flow>(*p).Current() == Flow::InLevel);
        Actions().clear();
        const List back = Frame(*p);
        REQUIRE_FALSE(back.empty());
        CHECK(back.back() == "Enter.Flow.MainMenu");
    }
    SUBCASE("in a child's enter") {
        auto p = Make({});
        REQUIRE(p);
        Frame(*p);
        Actions()["Enter.Pause.Running"] = [](Ctx& c) -> K::SystemResult { c.pause.Request(Pause::Paused); return {}; };
        Req<Flow>(*p).Request(Flow::InLevel);
        Frame(*p);
        CHECK(Mach<Pause>(*p).Current() == Pause::Running);
        Actions().clear();
        CHECK(Frame(*p) == List{"Exit.Pause.Running", "Enter.Pause.Paused"});
    }
}

TEST_CASE("behaviour: a request for another child made in one child's enter waits for the next apply") {
    auto p = Make({});
    REQUIRE(p);
    Frame(*p);
    Actions()["Enter.Pause.Running"] = [](Ctx& c) -> K::SystemResult { c.inventory.Request(Inventory::Open); return {}; };
    Req<Flow>(*p).Request(Flow::InLevel);
    Frame(*p);
    CHECK(Mach<Inventory>(*p).Current() == Inventory::Closed);
    Actions().clear();
    CHECK(Frame(*p) == List{"Exit.Inventory.Closed", "Enter.Inventory.Open"});
}

TEST_CASE("behaviour: children appear with the parent, in order, reading their own value; absent in the parent's enter") {
    auto p = InLevel();
    CHECK(Mach<Pause>(*p).Current() == Pause::Running);
    CHECK(Mach<Inventory>(*p).Current() == Inventory::Closed);
    CHECK(Seen()["Enter.Flow.InLevel"] == "F=InLevel,P=-,I=-");
    CHECK(Seen()["Enter.Pause.Running"] == "F=InLevel,P=Running,I=-");
    CHECK(Seen()["Enter.Inventory.Closed"] == "F=InLevel,P=Running,I=Closed");
}

TEST_CASE("behaviour: game logic does not run while paused") {
    Setup s;
    s.extra = [](O::WorldSetup& w) {
        w.Registrar().AddSystem<&Probe<1>>(At("Probe1", std::string(O::DefaultSchedules::frame),
                                               std::string(O::DefaultSchedules::update),
                                               {SM::InState<Pause, Pause::Running>()}));
    };
    auto p = InLevel(std::move(s));
    CHECK(Frame(*p) == List{"probe1:InLevel"});
    Req<Pause>(*p).Request(Pause::Paused);
    CHECK(Frame(*p) == List{"Exit.Pause.Running", "Enter.Pause.Paused"});
    Req<Pause>(*p).Request(Pause::Running);
    CHECK(Frame(*p) == List{"Exit.Pause.Paused", "Enter.Pause.Running", "probe1:InLevel"});
}

TEST_CASE("behaviour: children leave with the parent, in order, each absent as soon as it has left") {
    Setup s;
    s.extra = [](O::WorldSetup& w) {
        K::Registrar& r = w.Registrar();
        const std::string main(O::DefaultSchedules::frame);
        const std::string update(O::DefaultSchedules::update);
        r.AddSystem<&Probe<1>>(At("Probe1", main, update, {SM::InState<Pause, Pause::Paused>()}));
        r.AddSystem<&Probe<2>>(At("Probe2", main, update, {SM::NotInState<Pause, Pause::Paused>()}));
    };
    auto p = InLevel(std::move(s));
    Req<Pause>(*p).Request(Pause::Paused);
    CHECK(Frame(*p) == List{"Exit.Pause.Running", "Enter.Pause.Paused", "probe1:InLevel"});
    Req<Flow>(*p).Request(Flow::MainMenu);
    CHECK(Frame(*p) == List{"Exit.Pause.Paused", "Exit.Inventory.Closed", "Exit.Flow.InLevel", "Enter.Flow.MainMenu"});
    CHECK(Seen()["Exit.Inventory.Closed"] == "F=InLevel,P=-,I=Closed");
    CHECK(Seen()["Exit.Flow.InLevel"] == "F=InLevel,P=-,I=-");
    CHECK(Seen()["Enter.Flow.MainMenu"] == "F=MainMenu,P=-,I=-");
    // Both conditions on an absent child are false: no probe ran.
    CHECK(Frame(*p).empty());
    // Back in the level: the child starts from its initial value again.
    Req<Flow>(*p).Request(Flow::InLevel);
    Frame(*p);
    CHECK(Mach<Pause>(*p).Current() == Pause::Running);
}

TEST_CASE("behaviour: a child changing leaves the parent and the other child alone") {
    auto p = InLevel();
    Req<Pause>(*p).Request(Pause::Paused);
    CHECK(Frame(*p) == List{"Exit.Pause.Running", "Enter.Pause.Paused"});
    // A child's own change: its exit reads the old value, its enter the new one.
    CHECK(Seen()["Exit.Pause.Running"] == "F=InLevel,P=Running,I=Closed");
    CHECK(Seen()["Enter.Pause.Paused"] == "F=InLevel,P=Paused,I=Closed");
    Req<Pause>(*p).Request(Pause::Running);
    CHECK(Frame(*p) == List{"Exit.Pause.Paused", "Enter.Pause.Running"});
}

TEST_CASE("behaviour: a request for an absent child is dropped") {
    auto p = Make({});
    REQUIRE(p);
    Frame(*p);
    Req<Pause>(*p).Request(Pause::Paused);
    CHECK(Frame(*p).empty());
    Req<Flow>(*p).Request(Flow::InLevel);
    Frame(*p);
    CHECK(Mach<Pause>(*p).Current() == Pause::Running);
}

TEST_CASE("behaviour: parent and child requests in one apply: the parent first, the child from its initial value") {
    auto p = Make({});
    REQUIRE(p);
    Frame(*p);
    Req<Flow>(*p).Request(Flow::InLevel);
    Req<Pause>(*p).Request(Pause::Paused);
    CHECK(Frame(*p) == List{"Exit.Flow.MainMenu", "Enter.Flow.InLevel", "Enter.Pause.Running", "Enter.Inventory.Closed",
                            "Exit.Pause.Running", "Enter.Pause.Paused"});
}

TEST_CASE("behaviour: two children's requests in one apply go in registration order") {
    auto p = InLevel();
    Req<Inventory>(*p).Request(Inventory::Open);
    Req<Pause>(*p).Request(Pause::Paused);
    CHECK(Frame(*p) == List{"Exit.Pause.Running", "Enter.Pause.Paused", "Exit.Inventory.Closed", "Enter.Inventory.Open"});
}

TEST_CASE("behaviour: the parent entered again with the same value resets its children by default") {
    auto p = InLevel();
    Req<Pause>(*p).Request(Pause::Paused);
    Frame(*p);
    Req<Flow>(*p).Request(Flow::InLevel);
    CHECK(Frame(*p) == List{"Exit.Pause.Paused", "Exit.Inventory.Closed", "Exit.Flow.InLevel", "Enter.Flow.InLevel",
                            "Enter.Pause.Running", "Enter.Inventory.Closed"});
    CHECK(Mach<Pause>(*p).Current() == Pause::Running);
}

TEST_CASE("behaviour: a child with keepOnParentReenter keeps its value, only for the same parent value") {
    Setup s;
    s.keepTree = true;
    auto p = Make(std::move(s));
    REQUIRE(p);
    Frame(*p);
    Req<Flow2>(*p).Request(Flow2::InLevel);
    Frame(*p);
    Req<KPause>(*p).Request(KPause::Paused);
    Frame(*p);
    Req<Flow2>(*p).Request(Flow2::InLevel);
    CHECK(Frame(*p) == List{"Exit.KInventory.Closed", "Exit.Flow2.InLevel", "Enter.Flow2.InLevel", "Enter.KInventory.Closed"});
    CHECK(Mach<KPause>(*p).Current() == KPause::Paused);
    Req<Flow2>(*p).Request(Flow2::MainMenu);
    CHECK(Frame(*p) == List{"Exit.KPause.Paused", "Exit.KInventory.Closed", "Exit.Flow2.InLevel", "Enter.Flow2.MainMenu"});
    CHECK(Mach<KPause>(*p).Status() == SM::StateStatus::Absent);
}

TEST_CASE("behaviour: two requests before one apply: the later wins, the earlier is counted") {
    auto p = Make({});
    REQUIRE(p);
    Frame(*p);
    Req<Flow>(*p).Request(Flow::MainMenu);
    CHECK(Req<Flow>(*p).Overwritten() == 0);
    Req<Flow>(*p).Request(Flow::InLevel);
    CHECK(Req<Flow>(*p).Overwritten() == 1);
    Frame(*p);
    CHECK(Mach<Flow>(*p).Current() == Flow::InLevel);
    Req<Flow>(*p).Request(Flow::InLevel);
    Frame(*p);
    CHECK(Req<Flow>(*p).Overwritten() == 1);
    Req<Flow>(*p).Request(Flow::MainMenu);
    Req<Flow>(*p).Request(Flow::InLevel);
    CHECK(Req<Flow>(*p).Overwritten() == 2);
}

TEST_CASE("behaviour: the same value: its exit and enter run again") {
    SUBCASE("the parent's") {
        auto p = Make({});
        REQUIRE(p);
        Frame(*p);
        Req<Flow>(*p).Request(Flow::MainMenu);
        CHECK(Frame(*p) == List{"Exit.Flow.MainMenu", "Enter.Flow.MainMenu"});
    }
    SUBCASE("a child's") {
        auto p = InLevel();
        Req<Pause>(*p).Request(Pause::Running);
        CHECK(Frame(*p) == List{"Exit.Pause.Running", "Enter.Pause.Running"});
    }
}

TEST_CASE("behaviour: a value not in the table: Err SM-E1, no exit, the value unchanged") {
    SUBCASE("the parent's") {
        auto p = InLevel();
        Req<Pause>(*p).Request(Pause::Paused);
        Frame(*p);
        Events().clear();
        Req<Flow>(*p).Request(static_cast<Flow>(7));
        auto ran = p->RunFrame();
        REQUIRE_FALSE(ran);
        CHECK(HasRule(ran.error(), "SM-E1"));
        CHECK(Events().empty());
        CHECK(Mach<Flow>(*p).Current() == Flow::InLevel);
        CHECK(Mach<Pause>(*p).Current() == Pause::Paused);
    }
    SUBCASE("a child's, checked with the parent's, absent or not") {
        auto p = Make({});
        REQUIRE(p);
        Frame(*p);
        Events().clear();
        Req<Flow>(*p).Request(Flow::InLevel);
        Req<Pause>(*p).Request(static_cast<Pause>(9));
        auto ran = p->RunFrame();
        REQUIRE_FALSE(ran);
        CHECK(HasRule(ran.error(), "SM-E1"));
        CHECK(Events().empty());
        CHECK(Mach<Flow>(*p).Current() == Flow::MainMenu);
    }
}

TEST_CASE("behaviour: an exit or an enter failing interrupts the World where it stands") {
    SUBCASE("the parent's exit") {
        auto p = Make({});
        REQUIRE(p);
        Frame(*p);
        Actions()["Exit.Flow.MainMenu"] = [](Ctx&) { return Fail("exit"); };
        Req<Flow>(*p).Request(Flow::InLevel);
        CHECK_FALSE(p->RunFrame());
        CHECK(Mach<Flow>(*p).Current() == Flow::MainMenu);
        CHECK(Seen().count("Enter.Flow.InLevel") == 0);
    }
    SUBCASE("the parent's enter") {
        auto p = Make({});
        REQUIRE(p);
        Frame(*p);
        Actions()["Enter.Flow.InLevel"] = [](Ctx&) { return Fail("enter"); };
        Req<Flow>(*p).Request(Flow::InLevel);
        CHECK_FALSE(p->RunFrame());
        CHECK(Mach<Flow>(*p).Current() == Flow::InLevel);
        CHECK(Mach<Pause>(*p).Status() == SM::StateStatus::Absent);
        CHECK(Mach<Inventory>(*p).Status() == SM::StateStatus::Absent);
        CHECK(Seen().count("Enter.Pause.Running") == 0);
    }
    SUBCASE("a child's exit") {
        auto p = InLevel();
        Req<Pause>(*p).Request(Pause::Paused);
        Frame(*p);
        Actions()["Exit.Pause.Paused"] = [](Ctx&) { return Fail("child exit"); };
        Seen().clear();
        Req<Flow>(*p).Request(Flow::MainMenu);
        CHECK_FALSE(p->RunFrame());
        CHECK(Mach<Flow>(*p).Current() == Flow::InLevel);
        CHECK(Seen().count("Exit.Flow.InLevel") == 0);
    }
    SUBCASE("a child's enter") {
        auto p = Make({});
        REQUIRE(p);
        Frame(*p);
        Actions()["Enter.Pause.Running"] = [](Ctx&) { return Fail("child enter"); };
        Req<Flow>(*p).Request(Flow::InLevel);
        CHECK_FALSE(p->RunFrame());
        CHECK(Mach<Inventory>(*p).Status() == SM::StateStatus::Absent);
        CHECK(Seen().count("Enter.Inventory.Closed") == 0);
    }
}

TEST_CASE("behaviour: the first values are put by the registration") {
    auto p = Make({});
    REQUIRE(p);
    CHECK(Mach<Flow>(*p).Status() == SM::StateStatus::NotEntered);
    CHECK(Mach<Pause>(*p).Status() == SM::StateStatus::Absent);
    CHECK(Mach<Inventory>(*p).Status() == SM::StateStatus::Absent);
    // No current value while not entered or absent.
    CHECK_FALSE(Mach<Flow>(*p).Current().has_value());
    CHECK_FALSE(Mach<Pause>(*p).Current().has_value());
}

TEST_CASE("behaviour: an initial value given with SetInitial in a prepare function") {
    Setup s;
    s.prepare.push_back([](K::World& world) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.ResourceMut<SM::StateMachine<Flow>>()->SetInitial(Flow::InLevel);
        return {};
    });
    auto p = Make(std::move(s));
    REQUIRE(p);
    CHECK(Frame(*p) == List{"Enter.Flow.InLevel", "Enter.Pause.Running", "Enter.Inventory.Closed"});
}

namespace {

std::int32_t& HoldFrames() {
    static std::int32_t frames = 0;
    return frames;
}
std::int32_t& GivenPause() {
    static std::int32_t given = 0;  // 0 none, 1 with the parent, 2 the child only
    return given;
}
// A generation System (WhileGenerating): holds HoldFrames() frames; the first frame it puts the saved values.
K::SystemResult LoadSave(K::ResMut<O::WorldGeneration> generation, K::ResMut<SM::StateMachine<Flow>> flow,
                         K::ResMut<SM::StateMachine<Pause>> pause) {
    if (GivenPause() != 2 && HoldFrames() == 2) flow->SetInitial(Flow::InLevel);
    if (GivenPause() != 0 && HoldFrames() == 2) pause->SetInitial(Pause::Paused);
    if (HoldFrames()-- > 0) generation->Hold();
    Events().push_back("load");
    return {};
}

Setup Loading(int givenPause) {
    HoldFrames() = 2;
    GivenPause() = givenPause;
    Setup s;
    s.options.conditions = {O::AfterGenerating()};
    s.extra = [](O::WorldSetup& w) {
        w.Registrar().AddSystem<&LoadSave>(At("LoadSave", std::string(O::DefaultSchedules::frame),
                                               std::string(O::DefaultSchedules::update), {O::WhileGenerating()}));
    };
    return s;
}

} // namespace

TEST_CASE("behaviour: loading a save enters the saved values directly, once generation has ended") {
    SUBCASE("the parent and the child") {
        auto p = Make(Loading(1));
        REQUIRE(p);
        CHECK(Frame(*p) == List{"load"});
        CHECK(Frame(*p) == List{"load"});
        CHECK(Frame(*p) == List{"load"});
        CHECK(Frame(*p) == List{"Enter.Flow.InLevel", "Enter.Pause.Paused", "Enter.Inventory.Closed"});
        // The child's given value is used once: back to the level, it starts from its initial value.
        Req<Flow>(*p).Request(Flow::MainMenu);
        Frame(*p);
        Req<Flow>(*p).Request(Flow::InLevel);
        Frame(*p);
        CHECK(Mach<Pause>(*p).Current() == Pause::Running);
    }
    SUBCASE("the child only: not under the parent's initial value, so it is dropped") {
        auto p = Make(Loading(2));
        REQUIRE(p);
        Frame(*p);
        Frame(*p);
        Frame(*p);
        CHECK(Frame(*p) == List{"Enter.Flow.MainMenu"});
        CHECK(Mach<Pause>(*p).Status() == SM::StateStatus::Absent);
        Req<Flow>(*p).Request(Flow::InLevel);
        Frame(*p);
        CHECK(Mach<Pause>(*p).Current() == Pause::Running);
    }
}

TEST_CASE("behaviour: the default place: right after Main/PreUpdate") {
    Setup s;
    s.extra = [](O::WorldSetup& w) {
        K::Registrar& r = w.Registrar();
        const std::string main(O::DefaultSchedules::frame);
        r.AddSystem<&Probe<1>>(At("Probe1", main, std::string(O::DefaultSchedules::preUpdate)));
        r.AddSystem<&Probe<2>>(At("Probe2", main, std::string(O::DefaultSchedules::update)));
        r.AddSystem<&Probe<3>>(At("Probe3", main, std::string(O::DefaultSchedules::fixedMainLoop)));
    };
    auto p = Make(std::move(s));
    REQUIRE(p);
    Frame(*p);
    Req<Flow>(*p).Request(Flow::InLevel);
    const List events = Frame(*p);
    REQUIRE(events.size() >= 3);
    CHECK(events.front() == "probe1:MainMenu");
    // Before FixedMainLoop: a System there already reads the new value.
    CHECK(events[events.size() - 2] == "probe3:InLevel");
    CHECK(events.back() == "probe2:InLevel");
}

TEST_CASE("behaviour: the apply System's conditions hold back the first apply") {
    HoldFrames() = 1;
    GivenPause() = 0;
    Setup s;
    s.options.conditions = {O::AfterGenerating()};
    s.extra = [](O::WorldSetup& w) {
        w.Registrar().AddSystem<&LoadSave>(At("LoadSave", std::string(O::DefaultSchedules::frame),
                                               std::string(O::DefaultSchedules::update), {O::WhileGenerating()}));
    };
    auto p = Make(std::move(s));
    REQUIRE(p);
    CHECK(Frame(*p) == List{"load"});
    CHECK(Frame(*p) == List{"load"});
    CHECK(Frame(*p) == List{"Enter.Flow.MainMenu"});
}

namespace {
struct LevelStarted {
    std::int32_t level = 0;
};
K::SystemResult SendLevelStarted(K::MessageWriter<LevelStarted> out) {
    out.Send(LevelStarted{1});
    return {};
}
K::SystemResult ReadLevelStarted(K::MessageReader<LevelStarted> in) {
    for (const LevelStarted* m : in.Read()) Events().push_back("read level " + std::to_string(m->level));
    return {};
}
} // namespace

TEST_CASE("behaviour: an enter writes a message a later reader reads the same frame") {
    Setup s;
    s.extra = [](O::WorldSetup& w) {
        K::Registrar& r = w.Registrar();
        r.Message<LevelStarted>({.name = "LevelStarted"});
        r.AddSystem<&SendLevelStarted>(At("SendLevelStarted", "Flow.Enter.InLevel", std::string(SM::kEnterExitStage)));
        r.AddSystem<&ReadLevelStarted>(
            At("ReadLevelStarted", std::string(O::DefaultSchedules::frame), std::string(O::DefaultSchedules::update)));
    };
    auto p = Make(std::move(s));
    REQUIRE(p);
    Frame(*p);
    Req<Flow>(*p).Request(Flow::InLevel);
    const List events = Frame(*p);
    REQUIRE_FALSE(events.empty());
    CHECK(events.back() == "read level 1");
}

TEST_CASE("behaviour: names come from the traits") {
    CHECK(SM::ResourceName<Flow>() == "StateMachine.Flow");
    CHECK(SM::RequestName<Pause>() == "StateMachineRequest.Pause");
    CHECK(SM::ApplySystemName<Flow>() == "ApplyStateMachine.Flow");
    CHECK(SM::ApplyStageName<Weather>() == "Weather.Apply");
    CHECK(SM::EnterScheduleName(Flow::InLevel) == "Flow.Enter.InLevel");
    CHECK(SM::ExitScheduleName(Pause::Paused) == "Pause.Exit.Paused");
    CHECK(SM::InState<Flow, Flow::InLevel>().Name() == "InState.Flow.InLevel");
    CHECK(SM::NotInState<Flow, Flow::InLevel>().Name() == "NotInState.Flow.InLevel");
    CHECK(SM::InState<Pause, Pause::Running>().Name() == "InState.Pause.Running");
    CHECK(SM::EnterScheduleName(Flow::MainMenu) != SM::EnterScheduleName(Flow2::MainMenu));
    // What the registration put into the kernel: the Resource names, the apply System and its Writes<...>
    // annotation of the tree's six Resources (the authority Module reads it, AU-E6).
    auto p = Make({});
    REQUIRE(p);
    const auto& types = WorldOf(*p).Types();
    const auto flowId = types.ResourceIdOf<SM::StateMachine<Flow>>();
    const auto requestId = types.ResourceIdOf<SM::StateMachineRequest<Pause>>();
    REQUIRE(flowId.has_value());
    REQUIRE(requestId.has_value());
    CHECK(types.Resources()[*flowId].name == "StateMachine.Flow");
    CHECK(types.Resources()[*requestId].name == "StateMachineRequest.Pause");
    const K::SystemInfo* apply = types.SystemNamed("ApplyStateMachine.Flow");
    REQUIRE(apply != nullptr);
    CHECK(apply->mounts.at(0).stage == "Flow.Apply");
    CHECK(apply->access.annotatedWrites.size() == 6);
}

TEST_CASE("behaviour: the closed type: no construction from a value, no default construction; copy, move, ==") {
    using M = SM::StateMachine<Flow>;
    CHECK_FALSE(std::is_constructible_v<M, Flow>);
    CHECK_FALSE(std::is_default_constructible_v<M>);
    CHECK(std::is_copy_constructible_v<M>);
    CHECK(std::is_nothrow_move_constructible_v<M>);
    CHECK(std::equality_comparable<M>);
    CHECK(std::is_default_constructible_v<SM::StateMachineRequest<Flow>>);
}

TEST_CASE("behaviour: an absent child is the same whatever value it had") {
    auto paused = InLevel();
    Req<Pause>(*paused).Request(Pause::Paused);
    Frame(*paused);
    Req<Flow>(*paused).Request(Flow::MainMenu);
    Frame(*paused);
    auto running = InLevel();
    Req<Flow>(*running).Request(Flow::MainMenu);
    Frame(*running);
    CHECK(Mach<Pause>(*paused) == Mach<Pause>(*running));
}

namespace {
std::vector<K::ChangeStamp>& RequestStamps() {
    static std::vector<K::ChangeStamp> stamps;
    return stamps;
}
K::SystemResult RecordRequestStamp(K::ExclusiveWorld<> world) {
    RequestStamps().push_back(world.ResourceStamp<SM::StateMachineRequest<Flow>>().value_or(0));
    return {};
}
} // namespace

TEST_CASE("behaviour: an apply with no request does not write the request Resource (CCR-8)") {
    RequestStamps().clear();
    Setup s;
    s.extra = [](O::WorldSetup& w) {
        w.Registrar().AddSystem<&RecordRequestStamp>(At("RecordRequestStamp", std::string(O::DefaultSchedules::frame),
                                                        std::string(O::DefaultSchedules::last)));
    };
    auto p = Make(std::move(s));
    REQUIRE(p);
    Frame(*p);
    Frame(*p);
    Frame(*p);
    REQUIRE(RequestStamps().size() == 3);
    CHECK(RequestStamps()[1] == RequestStamps()[2]);
}
