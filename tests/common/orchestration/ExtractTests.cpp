// Extract in the orchestration layer (declaration docs/architecture/target/v2/sdl-extract-declaration.zh-Hant.md,
// v8-arch-gyo2-sdl-extract, section 2.3 and 2.6 XO-1 to XO-7): WorldDescription::extractFrom, O-E9, the source's
// Registry set before the reader's BuildPlans, extract schedules run through RunExtract from the frame list,
// RunSchedule and beforeSwap, the reader left unchecked when its source did not register, replacement and
// destruction. target O-5, O-8, O-12, O-15; Kernel DI-KS 35, 37.

#include "OrchestrationTestSupport.hpp"

#include "AssertTestSupport.hpp"

#include <string>
#include <utility>
#include <vector>

using namespace GYO::Test::Orchestration;

namespace {

struct Mirror {
    int n = 0;
};
// Logs "dtor <name>" when a World holding it is destroyed (a moved-from one logs nothing).
struct Dtor {
    explicit Dtor(std::string n) : name(std::move(n)) {}
    Dtor(Dtor&& other) noexcept : name(std::move(other.name)) { other.name.clear(); }
    Dtor& operator=(Dtor&& other) noexcept {
        name = std::move(other.name);
        other.name.clear();
        return *this;
    }
    Dtor(const Dtor&) = delete;
    Dtor& operator=(const Dtor&) = delete;
    ~Dtor() {
        if (!name.empty()) Log().push_back("dtor " + name);
    }
    std::string name;
};

K::ScheduleOptions ExtractOnce(std::string name) {
    K::ScheduleOptions s = Once(std::move(name));
    s.extract = true;
    return s;
}

K::SystemResult Bump(K::ResMut<Counter> counter) {
    counter->n += 1;
    return {};
}
// The reader's extract System: logs the source's Counter and copies it into its own Mirror.
K::SystemResult Copy(K::ExtractRes<Counter> counter, K::ResMut<Mirror> mirror) {
    const int n = counter.Present() ? counter->n : -1;
    Log().push_back("extract n=" + std::to_string(n));
    mirror->n = n;
    return {};
}
// A reader of a reader (XO-7): reads the first reader's Mirror.
K::SystemResult CopyMirror(K::ExtractRes<Mirror> mirror) {
    Log().push_back("chain n=" + std::to_string(mirror.Present() ? mirror->n : -1));
    return {};
}
K::SystemResult ReadsMissing(K::Query<K::Read<Tag>>, K::ResMut<Mirror>) { return {}; }

O::PrepareFn Put(int n) {
    return [n](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(Counter{n});
        return {};
    };
}
O::PrepareFn PutMirror() {
    return [](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(Mirror{});
        return {};
    };
}

// The source World "Sim": Counter, one schedule Step that bumps it.
O::WorldDescription Sim(std::string name = "Sim") {
    O::WorldDescription w = World(
        std::move(name),
        [](O::WorldSetup& s) {
            Basics(s.Registrar(), {Once("Step")});
            s.Registrar().Resource<Counter>({.name = "Counter"});
            s.Registrar().AddSystem<&Bump>(In("Bump", "Step"));
        },
        {"Step"});
    w.skeleton = O::ScheduleSkeleton::Off;
    w.prepare.push_back(Put(0));
    return w;
}

// A reader World: Mirror, an extract schedule Extract with Copy, in its frame list.
O::WorldDescription Reader(std::string name, std::string from) {
    O::WorldDescription w = World(
        std::move(name),
        [](O::WorldSetup& s) {
            Basics(s.Registrar(), {ExtractOnce("Extract")});
            s.Registrar().Resource<Mirror>({.name = "Mirror"});
            s.Registrar().AddSystem<&Copy>(In("Copy", "Extract"));
        },
        {"Extract"});
    w.skeleton = O::ScheduleSkeleton::Off;
    w.extractFrom = std::move(from);
    w.prepare.push_back(PutMirror());
    return w;
}

// A reader of the reader "Render" (XO-7): reads its Mirror.
O::WorldDescription ChainReader(std::string name, std::string from) {
    O::WorldDescription w = World(
        std::move(name),
        [](O::WorldSetup& s) {
            Basics(s.Registrar(), {ExtractOnce("Extract")});
            s.Registrar().AddSystem<&CopyMirror>(In("CopyMirror", "Extract"));
        },
        {"Extract"});
    w.skeleton = O::ScheduleSkeleton::Off;
    w.extractFrom = std::move(from);
    return w;
}

O::ProductDescription Pair() {
    O::ProductDescription d;
    d.name = "game";
    d.worlds.push_back(Sim());
    d.worlds.push_back(Reader("Render", "Sim"));
    return d;
}

bool NotChecked(const O::ProductStartupReport& report, const std::string& world) {
    for (const O::WorldStartupResult& result : report.worlds) {
        if (result.world == world) return result.notChecked && !result.kernel.has_value();
    }
    return false;
}

} // namespace

TEST_CASE("extract: each frame the reader extracts what the source has after its round and its out; the reader's in comes first") {
    Log().clear();
    O::ProductDescription d = Pair();
    d.worlds[0].build = [base = d.worlds[0].build](O::WorldSetup& s) {
        base(s);
        s.ExchangeOut([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            world.ResourceMut<Counter>()->n += 100;
            return {};
        });
    };
    d.worlds[1].build = [base = d.worlds[1].build](O::WorldSetup& s) {
        base(s);
        s.ExchangeIn([](K::World&) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            Log().push_back("in render");
            return {};
        });
    };
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    REQUIRE(static_cast<bool>(p.RunFrame()));
    CHECK_MESSAGE(Joined() == "in render | extract n=101 | in render | extract n=202", "[or-ex-frame] " << Joined());
    CHECK(p.World("Render").GetWorld().Resource<Mirror>()->n == 202);
}

TEST_CASE("extract: a unit's WorldSetup tells the World's extractFrom; empty when there is none") {
    std::vector<std::string> seen;
    O::ProductDescription d = Pair();
    for (O::WorldDescription& w : d.worlds) {
        w.build = [base = w.build, &seen](O::WorldSetup& s) {
            base(s);
            seen.push_back(s.WorldName() + "<-" + s.ExtractFrom());
        };
    }
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    REQUIRE(seen.size() == 2);
    CHECK_MESSAGE(seen[0] == "Sim<-", "[or-ex-setup-none] " << seen[0]);
    CHECK_MESSAGE(seen[1] == "Render<-Sim", "[or-ex-setup-from] " << seen[1]);
}

TEST_CASE("extract: O-E9: extractFrom names no World, the World itself, or a later one; the reader is not checked") {
    struct Case {
        std::string what;
        O::ProductDescription description;
    };
    O::ProductDescription unknown = Pair();
    unknown.worlds[1].extractFrom = "Nope";
    O::ProductDescription self = Pair();
    self.worlds[1].extractFrom = "Render";
    O::ProductDescription later;
    later.name = "game";
    later.worlds.push_back(Reader("Render", "Sim"));
    later.worlds.push_back(Sim());
    Case cases[] = {{"unknown", std::move(unknown)}, {"self", std::move(self)}, {"later", std::move(later)}};
    for (Case& c : cases) {
        auto built = O::Product::Build(std::move(c.description), Units().Take());
        const std::size_t oe9 = built ? 0 : built.error().Count(O::OrchestrationCode::OE9);
        CHECK_MESSAGE(oe9 == 1, "[or-ex-oe9-" << c.what << "] " << Describe(built));
        CHECK_MESSAGE((!built && NotChecked(built.error(), "Render")), "[or-ex-oe9-unchecked] " << c.what << ": " << Describe(built));
    }
}

TEST_CASE("extract: O-E9: an extract schedule in a frame list without extractFrom, or in the game initialization list") {
    // No Extract System, so the kernel builds the World and the list check runs (it is skipped after a
    // kernel error: a World with Extract parameters and no source has the kernel's D-E14 instead).
    O::ProductDescription noSource = Pair();
    noSource.worlds[1] = World("Render", [](O::WorldSetup& s) { Basics(s.Registrar(), {ExtractOnce("Extract")}); }, {"Extract"});
    noSource.worlds[1].skeleton = O::ScheduleSkeleton::Off;
    auto built = O::Product::Build(std::move(noSource), Units().Take());
    CHECK_MESSAGE((built ? 0 : built.error().Count(O::OrchestrationCode::OE9)) == 1, "[or-ex-oe9-list] " << Describe(built));

    O::ProductDescription gameInit;
    gameInit.name = "game";
    O::WorldDescription first = World("First", [](O::WorldSetup& s) { Basics(s.Registrar(), {Once("Step"), ExtractOnce("Extract")}); },
                                      {"Step"});
    first.skeleton = O::ScheduleSkeleton::Off;
    gameInit.worlds.push_back(std::move(first));
    gameInit.gameInit = {"Extract"};
    auto initBuilt = O::Product::Build(std::move(gameInit), Units().Take());
    CHECK_MESSAGE((initBuilt ? 0 : initBuilt.error().Count(O::OrchestrationCode::OE9)) == 1,
                  "[or-ex-oe9-gameinit] " << Describe(initBuilt));
}

TEST_CASE("extract: a source that did not register leaves the reader unchecked, with only the source's error") {
    // The source's kernel registration fails (a System reads an unregistered resource).
    O::ProductDescription kernelFails = Pair();
    kernelFails.worlds[0].build = [](O::WorldSetup& s) {
        Basics(s.Registrar(), {Once("Step")});
        s.Registrar().Resource<Counter>({.name = "Counter"});
        s.Registrar().AddSystem<&ReadsMissing>(In("ReadsMissing", "Step"));
    };
    kernelFails.worlds[0].prepare.clear();
    auto built = O::Product::Build(std::move(kernelFails), Units().Take());
    REQUIRE_FALSE(static_cast<bool>(built));
    CHECK_MESSAGE(built.error().KernelReportOf("Sim") != nullptr, "the source's own error: " << Describe(built));
    CHECK_MESSAGE(NotChecked(built.error(), "Render"), "[or-ex-reader-unchecked] kernel: " << Describe(built));

    // The source is dropped by O-E6 (it uses a unit the Product does not carry): no O-E9 for the reader.
    O::ProductDescription dropped = Pair();
    dropped.worlds[0].units = {"nope"};
    auto droppedBuilt = O::Product::Build(std::move(dropped), Units().Take());
    REQUIRE_FALSE(static_cast<bool>(droppedBuilt));
    CHECK(droppedBuilt.error().Has(O::OrchestrationCode::OE6));
    CHECK_MESSAGE(!droppedBuilt.error().Has(O::OrchestrationCode::OE9), "[or-ex-dropped-no-oe9] " << Describe(droppedBuilt));
    CHECK_MESSAGE(NotChecked(droppedBuilt.error(), "Render"), "[or-ex-reader-unchecked] O-E6: " << Describe(droppedBuilt));
}

TEST_CASE("extract: a reader after a World that was not registered still gets its own source") {
    // The skipped World keeps its place in the registrations: the reader is given Sim's Registry, not Other's.
    O::ProductDescription d;
    d.name = "game";
    d.worlds.push_back(Reader("Bad", "Nope"));
    d.worlds.push_back(Sim());
    O::WorldDescription other = World("Other", [](O::WorldSetup& s) { Basics(s.Registrar(), {Once("Step")}); }, {"Step"});
    other.skeleton = O::ScheduleSkeleton::Off;
    d.worlds.push_back(std::move(other));
    d.worlds.push_back(Reader("Render", "Sim"));
    auto built = O::Product::Build(std::move(d), Units().Take());
    REQUIRE_FALSE(static_cast<bool>(built));
    CHECK(built.error().Count(O::OrchestrationCode::OE9) == 1);
    CHECK_MESSAGE(built.error().KernelReportOf("Render") == nullptr, "[or-ex-registrations-aligned] " << Describe(built));
}

TEST_CASE("extract: RunSchedule on an extract schedule reads what the source has now") {
    Log().clear();
    auto built = O::Product::Build(Pair(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    // The runner writes the source between two entry points.
    p.World("Sim").GetWorld().SetResource(Counter{42});
    Log().clear();
    REQUIRE(static_cast<bool>(p.RunSchedule("Render", "Extract")));
    CHECK_MESSAGE(Joined() == "extract n=42", "[or-ex-runschedule] " << Joined());
}

TEST_CASE("extract: a replaced reader extracts again, beforeSwap included, in both orders") {
    for (O::ReplaceOrder order : {O::ReplaceOrder::BuildNewFirst, O::ReplaceOrder::DropOldFirst}) {
        Log().clear();
        auto built = O::Product::Build(Pair(), Units().Take());
        ORCH_REQUIRE_BUILT(built);
        O::Product& p = **built;
        REQUIRE(static_cast<bool>(p.RunFrame()));
        Log().clear();
        O::ReplaceRequest request;
        request.order = order;
        request.beforeSwap = {"Extract"};
        const auto replaced = p.ReplaceWorld("Render", std::move(request));
        REQUIRE_MESSAGE(static_cast<bool>(replaced),
                        "[or-ex-replace-reader-registered] " << (replaced ? std::string() : replaced.error().message + ": " + replaced.error().detail));
        REQUIRE(static_cast<bool>(p.RunFrame()));
        CHECK_MESSAGE(Joined() == "extract n=1 | extract n=2", "[or-ex-replace-reader] " << Joined());
    }
}

TEST_CASE("extract: the reader World is destroyed before its source") {
    Log().clear();
    {
        O::ProductDescription d = Pair();
        d.worlds[0].prepare.push_back([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            world.SetResource(Dtor("sim"));
            return {};
        });
        d.worlds[1].prepare.push_back([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            world.SetResource(Dtor("render"));
            return {};
        });
        const auto build0 = d.worlds[0].build;
        d.worlds[0].build = [build0](O::WorldSetup& s) {
            build0(s);
            s.Registrar().Resource<Dtor>({.name = "Dtor"});
        };
        const auto build1 = d.worlds[1].build;
        d.worlds[1].build = [build1](O::WorldSetup& s) {
            build1(s);
            s.Registrar().Resource<Dtor>({.name = "Dtor"});
        };
        auto built = O::Product::Build(std::move(d), Units().Take());
        ORCH_REQUIRE_BUILT(built);
        Log().clear();
    }
    CHECK_MESSAGE(Joined() == "dtor render | dtor sim", "[or-ex-destroy-order] " << Joined());
}

TEST_CASE("extract: one source with two readers, and a reader of a reader") {
    Log().clear();
    O::ProductDescription d = Pair();
    d.worlds.push_back(Reader("Overlay", "Sim"));
    d.worlds.push_back(ChainReader("Chain", "Render"));
    auto built = O::Product::Build(std::move(d), Units().Take());
    REQUIRE_MESSAGE(static_cast<bool>(built), "[or-ex-two-readers-chain] " << Describe(built));
    REQUIRE(static_cast<bool>((*built)->RunFrame()));
    CHECK_MESSAGE(Joined() == "extract n=1 | extract n=1 | chain n=1", "[or-ex-two-readers-chain] " << Joined());
}

// The asserts: each case ends right after its GYO_CHECK_ASSERTS, so a mutant that lets the call through
// (and leaves a reader with a dangling source) does not run on into undefined behaviour.
static O::ProductDescription WithChain() {
    O::ProductDescription d = Pair();
    d.worlds.push_back(ChainReader("Chain", "Render"));
    return d;
}

TEST_CASE("extract: the asserts: replacing the source of a reader (BuildNewFirst)") {
    auto built = O::Product::Build(WithChain(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    REQUIRE(static_cast<bool>((*built)->RunFrame()));
    GYO_CHECK_ASSERTS((*built)->ReplaceWorld("Sim", O::ReplaceRequest{}));
}

TEST_CASE("extract: the asserts: replacing the source of a reader (DropOldFirst) asserts before the old World goes") {
    auto built = O::Product::Build(WithChain(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    REQUIRE(static_cast<bool>((*built)->RunFrame()));
    O::ReplaceRequest request;
    request.order = O::ReplaceOrder::DropOldFirst;
    GYO_CHECK_ASSERTS((*built)->ReplaceWorld("Sim", std::move(request)));
    CHECK_MESSAGE((*built)->HasWorld("Sim"), "[or-ex-replace-source-before-drop] the source was dropped before the assert");
}

TEST_CASE("extract: the asserts: the middle of a chain is a source too") {
    auto built = O::Product::Build(WithChain(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    REQUIRE(static_cast<bool>((*built)->RunFrame()));
    GYO_CHECK_ASSERTS((*built)->ReplaceWorld("Render", O::ReplaceRequest{}));
}

TEST_CASE("extract: the asserts: an extract schedule on a World without a source; the Product goes on") {
    O::ProductDescription d = Pair();
    // Declares an extract schedule but has no source (not in its frame list: no O-E9).
    O::WorldDescription lone = World("Lone", [](O::WorldSetup& s) { Basics(s.Registrar(), {Once("Step"), ExtractOnce("Extract")}); },
                                     {"Step"});
    lone.skeleton = O::ScheduleSkeleton::Off;
    d.worlds.push_back(std::move(lone));
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    GYO_CHECK_ASSERTS(p.RunSchedule("Lone", "Extract"));
    // The assert came before the run, not as an exception out of it.
    CHECK_MESSAGE(static_cast<bool>(p.RunFrame()), "[or-ex-assert-before-run] the Product ended");
}
