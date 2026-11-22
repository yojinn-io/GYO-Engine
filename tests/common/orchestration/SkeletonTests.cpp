// O-4: the default schedule skeleton the orchestration layer gives every World
// (time-events declaration docs/architecture/target/v2/time-events-declaration.zh-Hant.md,
// section 3.6; v8-arch-gyo2-orchestration-gives-schedule-skeleton). The orchestration
// layer declares Main and FixedUpdate before any unit's Build and runs Main every
// frame; it knows no time and nothing a unit mounts into them.

#include "OrchestrationTestSupport.hpp"

#include <string>
#include <vector>

using namespace GYO::Test::Orchestration;

namespace {

struct Seen {
    std::vector<K::ScheduleInfo> schedules;
};
Seen& Captured() {
    static Seen seen;
    return seen;
}

// A unit that mounts one logging System into Main's Update (and nothing else) and
// keeps the frozen Registry's schedules.
Calls MountsIntoMain() {
    Calls calls;
    calls.build = [](O::WorldSetup& s) {
        s.Registrar().Component<Tag>({.name = "Tag"});
        K::SystemOptions o = In("UnitUpdate", std::string(O::DefaultSchedules::frame));
        o.mounts[0].stage = std::string(O::DefaultSchedules::update);
        s.Registrar().AddSystem<&Mark<'u'>>(std::move(o));
    };
    calls.finish = [](const K::FrozenRegistry& registry, O::WorldFinish&) {
        Captured().schedules.assign(registry.Schedules().begin(), registry.Schedules().end());
    };
    return calls;
}

// A unit that only keeps the frozen Registry's schedules.
Calls KeepsSchedules() {
    Calls calls;
    calls.finish = [](const K::FrozenRegistry& registry, O::WorldFinish&) {
        Captured().schedules.assign(registry.Schedules().begin(), registry.Schedules().end());
    };
    return calls;
}

const K::ScheduleInfo* Named(const std::string& name) {
    for (const K::ScheduleInfo& info : Captured().schedules) {
        if (info.options.name == name) return &info;
    }
    return nullptr;
}

K::SystemOptions At(std::string name, std::string_view stage) {
    K::SystemOptions o = In(std::move(name), std::string(O::DefaultSchedules::frame));
    o.mounts[0].stage = std::string(stage);
    return o;
}

} // namespace

TEST_CASE("O-4: by default every World has Main and FixedUpdate, declared before any unit's Build, with their stages in order") {
    Log().clear();
    Captured() = {};
    O::ProductDescription d;
    d.name = "game";
    d.worlds.push_back(World("Main"));
    auto built = O::Product::Build(std::move(d), Units().Module("keep", KeepsSchedules()).Take());
    ORCH_REQUIRE_BUILT(built);
    const K::ScheduleInfo* main = Named("Main");
    const K::ScheduleInfo* fixed = Named("FixedUpdate");
    REQUIRE_MESSAGE(main != nullptr, "[or-skeleton-main] no schedule Main");
    REQUIRE_MESSAGE(fixed != nullptr, "[or-skeleton-fixed] no schedule FixedUpdate");
    CHECK_MESSAGE((main->options.stages ==
                   std::vector<std::string>{"First", "PreUpdate", "FixedMainLoop", "Update", "PostUpdate", "Last"}),
                  "[or-skeleton-main-stages]");
    CHECK_MESSAGE((fixed->options.stages == std::vector<std::string>{"First", "PreUpdate", "Update", "PostUpdate", "Last"}),
                  "[or-skeleton-fixed-stages]");
    // Declared by the orchestration layer itself, first: not by a unit, not by the product.
    CHECK_MESSAGE((main->owner == "orchestration" && fixed->owner == "orchestration"), "[or-skeleton-owner] " << main->owner);
    CHECK_MESSAGE((Captured().schedules.size() == 2 && Captured().schedules[0].options.name == "Main"),
                  "[or-skeleton-only-two] " << Captured().schedules.size());
}

TEST_CASE("O-4: by default the frame list runs Main first; the description's frame list follows") {
    Log().clear();
    O::ProductDescription d;
    d.name = "game";
    d.worlds.push_back(World(
        "Main",
        [](O::WorldSetup& s) {
            s.Registrar().Schedule(Once("Tick"));  // Tag is the unit's
            s.Registrar().AddSystem<&Mark<'t'>>(In("MarkTick", "Tick"));
        },
        {"Tick"}));
    auto built = O::Product::Build(std::move(d), Units().Module("walk", MountsIntoMain()).Take());
    ORCH_REQUIRE_BUILT(built);
    REQUIRE(static_cast<bool>((*built)->RunFrame()));
    REQUIRE(static_cast<bool>((*built)->RunFrame()));
    CHECK_MESSAGE(Joined() == "configure walk | build walk@Main | finish walk@Main v1 | check walk@Main | u | t | u | t",
                  "[or-skeleton-frame-main] " << Joined());
}

TEST_CASE("O-4: a frame list that names Main runs it once") {
    Log().clear();
    O::ProductDescription d;
    d.name = "game";
    d.worlds.push_back(World("Main", {}, {"Main"}));
    auto built = O::Product::Build(std::move(d), Units().Module("walk", MountsIntoMain()).Take());
    ORCH_REQUIRE_BUILT(built);
    Log().clear();
    REQUIRE(static_cast<bool>((*built)->RunFrame()));
    CHECK_MESSAGE(Joined() == "u", "[or-skeleton-main-once] " << Joined());
}

TEST_CASE("O-4: Main runs its stages in order: First, PreUpdate, FixedMainLoop, Update, PostUpdate, Last") {
    Log().clear();
    O::ProductDescription d;
    d.name = "game";
    d.worlds.push_back(World("Main", [](O::WorldSetup& s) {
        K::Registrar& r = s.Registrar();
        r.Component<Tag>({.name = "Tag"});
        // Registered in reverse: only the stages order them.
        r.AddSystem<&Mark<'6'>>(At("InLast", O::DefaultSchedules::last));
        r.AddSystem<&Mark<'5'>>(At("InPostUpdate", O::DefaultSchedules::postUpdate));
        r.AddSystem<&Mark<'4'>>(At("InUpdate", O::DefaultSchedules::update));
        r.AddSystem<&Mark<'3'>>(At("InFixedMainLoop", O::DefaultSchedules::fixedMainLoop));
        r.AddSystem<&Mark<'2'>>(At("InPreUpdate", O::DefaultSchedules::preUpdate));
        r.AddSystem<&Mark<'1'>>(At("InFirst", O::DefaultSchedules::first));
    }));
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    REQUIRE(static_cast<bool>((*built)->RunFrame()));
    CHECK_MESSAGE(Joined() == "1 | 2 | 3 | 4 | 5 | 6", "[or-skeleton-stage-order] " << Joined());
}

TEST_CASE("O-4: the frame list does not run FixedUpdate: a schedule runs it nested or nothing does") {
    Log().clear();
    O::ProductDescription d;
    d.name = "game";
    d.worlds.push_back(World("Main", [](O::WorldSetup& s) {
        s.Registrar().Component<Tag>({.name = "Tag"});
        K::SystemOptions o = In("InFixed", std::string(O::DefaultSchedules::fixed));
        o.mounts[0].stage = std::string(O::DefaultSchedules::update);
        s.Registrar().AddSystem<&Mark<'f'>>(std::move(o));
    }));
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    REQUIRE(static_cast<bool>((*built)->RunFrame()));
    CHECK_MESSAGE(!Logged("f"), "[or-skeleton-fixed-not-in-frame] " << Joined());
    REQUIRE(static_cast<bool>((*built)->RunSchedule("Main", "FixedUpdate")));
    CHECK(Logged("f"));
}

TEST_CASE("O-4: with the skeleton off nothing is declared, the frame list is the description's, and the author may declare Main") {
    Log().clear();
    Captured() = {};
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w = World(
        "Main",
        [](O::WorldSetup& s) {
            K::ScheduleOptions own = Once("Main");
            Basics(s.Registrar(), {own, Once("Tick")});
            s.Registrar().AddSystem<&Mark<'m'>>(In("MarkMain", "Main"));
            s.Registrar().AddSystem<&Mark<'t'>>(In("MarkTick", "Tick"));
        },
        {"Tick"});
    w.skeleton = O::ScheduleSkeleton::Off;
    d.worlds.push_back(std::move(w));
    Calls keep;
    keep.finish = [](const K::FrozenRegistry& registry, O::WorldFinish&) {
        Captured().schedules.assign(registry.Schedules().begin(), registry.Schedules().end());
    };
    auto built = O::Product::Build(std::move(d), Units().Module("keep", keep).Take());
    ORCH_REQUIRE_BUILT(built);
    CHECK_MESSAGE(Named("FixedUpdate") == nullptr, "[or-skeleton-off] FixedUpdate declared");
    REQUIRE(Named("Main") != nullptr);
    CHECK_MESSAGE(Named("Main")->owner == "game", "[or-skeleton-off] Main is the author's");
    Log().clear();
    REQUIRE(static_cast<bool>((*built)->RunFrame()));
    CHECK_MESSAGE(Joined() == "t", "[or-skeleton-off-frame] " << Joined());
}

TEST_CASE("O-4: with the skeleton on, an author's own Main is a second declaration (R-E2)") {
    O::ProductDescription d;
    d.name = "game";
    d.worlds.push_back(World("Main", [](O::WorldSetup& s) { Basics(s.Registrar(), {Once("Main")}); }));
    auto built = O::Product::Build(std::move(d), Units().Take());
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(built), "[or-skeleton-twice] built");
    CHECK_MESSAGE(built.error().detail.find("schedule 'Main' registered twice (first by orchestration") != std::string::npos,
                  "[or-skeleton-twice] " << built.error().detail);
}

TEST_CASE("O-4: a replacement World gets the skeleton again") {
    Log().clear();
    O::ProductDescription d;
    d.name = "game";
    d.worlds.push_back(World("Main"));
    auto built = O::Product::Build(std::move(d), Units().Module("walk", MountsIntoMain()).Take());
    ORCH_REQUIRE_BUILT(built);
    REQUIRE(static_cast<bool>((*built)->ReplaceWorld("Main", O::ReplaceRequest{})));
    Log().clear();
    REQUIRE(static_cast<bool>((*built)->RunFrame()));
    CHECK_MESSAGE(Joined() == "u", "[or-skeleton-replace] " << Joined());
}

TEST_CASE("O-4: a unit inserts a stage into Main in its Build: the skeleton is there before any unit's Build") {
    Log().clear();
    Captured() = {};
    O::ProductDescription d;
    d.name = "game";
    d.worlds.push_back(World("Main"));
    Calls calls;
    calls.build = [](O::WorldSetup& s) {
        s.Registrar().InsertStage(O::DefaultSchedules::frame, O::DefaultSchedules::update, K::StagePlace::After, "Physics");
    };
    calls.finish = [](const K::FrozenRegistry& registry, O::WorldFinish&) {
        Captured().schedules.assign(registry.Schedules().begin(), registry.Schedules().end());
    };
    auto built = O::Product::Build(std::move(d), Units().Module("physics", calls).Take());
    REQUIRE_MESSAGE(static_cast<bool>(built), "[or-skeleton-before-build] " << Describe(built));
    REQUIRE(Named("Main") != nullptr);
    CHECK((Named("Main")->options.stages ==
           std::vector<std::string>{"First", "PreUpdate", "FixedMainLoop", "Update", "Physics", "PostUpdate", "Last"}));
}
