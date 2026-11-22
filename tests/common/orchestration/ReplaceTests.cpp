// O-7 (prepare), O-8 (replacement), O-10 (carry over) (declaration 7.3).

#include "SaveGame.hpp"

#include "AssertTestSupport.hpp"

#include <stdexcept>

using namespace GYO::Test::Orchestration;
using namespace GYO::Test::Orchestration::Save;

namespace {

O::PrepareFn Note(std::string text) {
    return [text](K::World&) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        Log().push_back(text);
        return {};
    };
}

O::Product* gProduct = nullptr;
K::SystemResult ReplaceFromInside(K::Query<K::Read<Tag>>) {
    GYO_CHECK_ASSERTS(gProduct->ReplaceWorld("Main", O::ReplaceRequest{}));
    return {};
}

} // namespace

TEST_CASE("O-8: prepare order (units, description, extra), beforeSwap after them") {
    Log().clear();
    Calls unit;
    unit.build = [](O::WorldSetup& s) { s.Prepare(Note("unit prepare")); };
    O::ProductDescription d = Game();
    d.worlds[0].prepare.push_back(Note("description prepare"));
    d.worlds[0].build = [](O::WorldSetup& s) {
        Register(s);
        s.Registrar().AddSystem<&Mark<'L'>>(In("LoadMark", "Load"));
    };
    auto built = O::Product::Build(std::move(d), Units().Module("audio", unit).Take());
    ORCH_REQUIRE_BUILT(built);
    CHECK_MESSAGE(Joined().find("unit prepare | description prepare") != std::string::npos, "[or-prepare-order] " << Joined());
    Log().clear();
    REQUIRE(static_cast<bool>((*built)->ReplaceWorld("Main", O::ReplaceRequest{Note("extra"), {"Load"}})));
    const std::string joined = Joined();
    const auto unitAt = joined.find("unit prepare");
    const auto extraAt = joined.find("extra");
    const auto loadAt = joined.find("L");
    CHECK_MESSAGE((loadAt != std::string::npos && unitAt < joined.find("description prepare") &&
                   joined.find("description prepare") < extraAt && extraAt < loadAt),
                  "[or-replace-order] " << joined);
}

TEST_CASE("O-8: replacing registers again: a new Registry at version 1, Build Finish Check again, no Configure, the old World destroyed") {
    Log().clear();
    Sentinel::Destroyed() = 0;
    auto built = O::Product::Build(Game(), Units().Module("audio").Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    REQUIRE(static_cast<bool>(p.InsertSystems("Main", "console", [](K::Registrar& r) { r.AddSystem<&Mark<'i'>>(In("Inserted", "Play")); })));
    REQUIRE(static_cast<bool>(p.RunFrame()));
    const K::FrozenRegistry* before = &p.World("Main").GetWorld().Types();
    REQUIRE(before->Version() == 2);
    Log().clear();
    const int destroyedBefore = Sentinel::Destroyed();
    REQUIRE(static_cast<bool>(p.ReplaceWorld("Main", O::ReplaceRequest{})));
    const K::FrozenRegistry& after = p.World("Main").GetWorld().Types();
    CHECK_MESSAGE(&after != before, "[or-replace-new-registry] the Registry was kept");
    CHECK_MESSAGE(after.Version() == 1, "[or-replace-new-registry] version " << after.Version());
    CHECK_MESSAGE(after.SystemNamed("Inserted") == nullptr, "[or-replace-inserted-gone]");
    CHECK_MESSAGE((Logged("build audio@Main") && Logged("finish audio@Main v1") && Logged("check audio@Main")),
                  "[or-replace-lifecycle] " << Joined());
    CHECK_MESSAGE(!Logged("configure audio"), "[or-replace-no-configure] " << Joined());
    CHECK_MESSAGE(Sentinel::Destroyed() == destroyedBefore + 1, "[or-replace-old-destroyed] " << Sentinel::Destroyed());
}

TEST_CASE("O-8: a Check that fails on the second registration: ReplaceRegistrationFailed, the old World goes on") {
    int checks = 0;
    Calls strategy;
    strategy.check = [&](const K::FrozenRegistry&, O::WorldCheck& c) {
        if (++checks == 2) c.Report("refused", "second time");
    };
    auto built = O::Product::Build(Game(), Units().Module("authority", strategy).Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    WarningCounter warnings;
    auto replaced = p.ReplaceWorld("Main", O::ReplaceRequest{});
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(replaced), "[or-replace-check-fails] replaced despite the Check error");
    CHECK_MESSAGE(replaced.error().code == O::OrchestrationErrorCode::ReplaceRegistrationFailed, "[or-replace-check-fails]");
    CHECK_MESSAGE(warnings.Warnings() == 0, "[or-replace-check-silent] " << warnings.Warnings());  // error levels D2
    CHECK_MESSAGE(static_cast<bool>(p.RunFrame()), "[or-replace-check-old-goes-on]");
}

TEST_CASE("O-8: ReplaceWorld inside a Run asserts") {
    O::ProductDescription d = Game();
    d.worlds[0].build = [](O::WorldSetup& s) {
        Register(s);
        s.Registrar().AddSystem<&Fail>(In("FailJunk", "Junk"));
        s.Registrar().AddSystem<&ReplaceFromInside>(In("Inside", "Load"));
    };
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    gProduct = &p;
    REQUIRE(static_cast<bool>(p.ReplaceWorld("Main", O::ReplaceRequest{})));
    CHECK(static_cast<bool>(p.RunSchedule("Main", "Load")));  // Inside checks the assertion
    REQUIRE_FALSE(static_cast<bool>(p.RunSchedule("Main", "Junk")));
    gProduct = nullptr;
}

TEST_CASE("O-8: DropOldFirst destroys the old World before the new registration; BuildNewFirst after it") {
    Sentinel::Destroyed() = 0;
    std::vector<int> destroyedAtBuild;
    Calls unit;
    unit.build = [&](O::WorldSetup&) { destroyedAtBuild.push_back(Sentinel::Destroyed()); };
    auto built = O::Product::Build(Game(), Units().Module("audio", unit).Take());
    ORCH_REQUIRE_BUILT(built);
    REQUIRE(static_cast<bool>((*built)->RunFrame()));
    const int before = Sentinel::Destroyed();
    REQUIRE(static_cast<bool>((*built)->ReplaceWorld("Main", O::ReplaceRequest{{}, {}, O::ReplaceOrder::DropOldFirst})));
    REQUIRE(destroyedAtBuild.size() == 2);
    CHECK_MESSAGE(destroyedAtBuild[1] == before + 1, "[or-drop-old-first-order] " << destroyedAtBuild[1] << " vs " << before + 1);
    const int beforeNew = Sentinel::Destroyed();
    REQUIRE(static_cast<bool>((*built)->ReplaceWorld("Main", O::ReplaceRequest{})));
    REQUIRE(destroyedAtBuild.size() == 3);
    CHECK_MESSAGE(destroyedAtBuild[2] == beforeNew, "[or-build-new-first-order] the old World went before the new registration");
    CHECK(Sentinel::Destroyed() == beforeNew + 1);
}

TEST_CASE("O-10: two types (a unit's and the product's) carry over; prepare sees them; absent ones are not carried") {
    Calls audio;
    audio.build = [](O::WorldSetup& s) {
        s.Registrar().Resource<Volume>({.name = "Volume"});
        s.CarryOver<Volume>("Volume");
        s.Prepare([](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            Log().push_back(w.Resource<Volume>() ? "volume present" : "volume absent");
            if (w.Resource<Volume>() == nullptr) w.SetResource(Volume{3});
            return {};
        });
    };
    auto built = O::Product::Build(Game(), Units().Module("audio", audio).Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    p.World("Main").GetWorld().ResourceMut<Volume>()->level = 9;
    p.World("Main").GetWorld().ResourceMut<KeyBindings>()->jump = 4;
    Log().clear();
    REQUIRE(static_cast<bool>(p.ReplaceWorld("Main", O::ReplaceRequest{})));
    CHECK_MESSAGE(Logged("volume present"), "[or-carry-before-prepare] " << Joined());
    CHECK_MESSAGE(VolumeOf(p) == 9, "[or-carry-unit-type]");
    CHECK_MESSAGE(JumpOf(p) == 4, "[or-carry-product-type]");
    p.World("Main").GetWorld().RemoveResource<Volume>();
    REQUIRE(static_cast<bool>(p.ReplaceWorld("Main", O::ReplaceRequest{})));
    CHECK_MESSAGE(VolumeOf(p) == 3, "[or-carry-absent]");
}

TEST_CASE("O-10: a marked type the new registration lacks: ReplaceCarryOverFailed, the value goes back") {
    int builds = 0;
    O::ProductDescription d = Game();
    d.worlds[0].build = [&](O::WorldSetup& s) {
        Register(s);
        if (++builds == 1) {
            s.Registrar().Resource<Volume>({.name = "Volume"});
            s.CarryOver<Volume>("Volume");
        }
    };
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    p.World("Main").GetWorld().SetResource(Volume{6});
    auto failed = p.ReplaceWorld("Main", O::ReplaceRequest{});
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(failed), "[or-carry-unregistered] replaced anyway");
    CHECK_MESSAGE(failed.error().code == O::OrchestrationErrorCode::ReplaceCarryOverFailed, "[or-carry-unregistered]");
    CHECK_MESSAGE(VolumeOf(p) == 6, "[or-carry-unregistered-back]");
}

// "After a DropOldFirst failure the held value is carried by the next
// replacement" left with its property: the empty place ends the Product, there
// is no next replacement (v8-arch-gyo2-broken-world-ends-product).

TEST_CASE("O-8: an exception from extra leaves the old World as it was") {
    auto built = O::Product::Build(Game(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    p.World("Main").GetWorld().ResourceMut<KeyBindings>()->jump = 2;
    bool thrown = false;
    try {
        static_cast<void>(p.ReplaceWorld("Main", O::ReplaceRequest{[](K::World&) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
            throw std::runtime_error("disk gone");
        }, {}, O::ReplaceOrder::BuildNewFirst}));
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    CHECK(thrown);
    CHECK_MESSAGE(JumpOf(p) == 2, "[or-replace-throw-keeps-old]");
    CHECK(static_cast<bool>(p.RunFrame()));
}
