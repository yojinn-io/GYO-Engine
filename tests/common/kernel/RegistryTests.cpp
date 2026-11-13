// Spec J-18, J-19 and the registry half of J-24 (target/kernel DI-KS 4;
// spec R-1 to R-6, C.4). Moved and rewritten from
// spikes/kernel/tests/RegistryWorldTests.cpp (ci-readiness section 2.2): ids
// follow registration order (J-18), lookup does not create ids and a type
// registers once (J-19); "unregistered type" is reported at Finish, all at
// once, instead of asserting when the System is made.

#include "KernelTestSupport.hpp"

namespace {

using namespace KernelTest;

SystemResult ReadPos(Query<Read<TestPos>>) { return Ok(); }
SystemResult ReadVel(Query<Read<TestVel>>) { return Ok(); }
SystemResult WriteHealth(Query<Write<TestHealth>>) { return Ok(); }

void Movement(Registrar& r) {
    r.Component<TestPos>({.name = "TestPos", .zone = Zone::C, .rollback = Rollback::Yes});
    r.Component<TestVel>({.name = "TestVel", .zone = Zone::C, .rollback = Rollback::Yes});
    r.Resource<TestRes>({.name = "TestRes", .zone = Zone::C});
    r.Event<TestEvent>({.name = "TestEvent", .zone = Zone::C, .domain = Domain::Fixed});
}

void Life(Registrar& r) {
    r.Component<TestHealth>({.name = "TestHealth", .zone = Zone::C, .rollback = Rollback::Yes});
    r.Resource<TestOtherRes>({.name = "TestOtherRes", .zone = Zone::B});
    r.Event<TestOtherEvent>({.name = "TestOtherEvent", .zone = Zone::C, .domain = Domain::Fixed});
}

void Schedules(Registrar& r) {
    r.Schedule(FixedSchedule());
}

} // namespace

// The Registry fingerprint golden of the J-18 registry (movement, life, host in
// this order). Same value on linux-x64, windows-x64 and macos-arm64
// (declaration judgement 4); gyo_kernel_tests_reversed checks it too (link order).
inline constexpr const char* kJ18RegistryGolden =
    "1:3e586bc47dcc22f989c0cb4f4b3dfff1da9614539ed1266440d07543d392b4ac";

TEST_CASE("J-18: ids follow the Host's plugin order and the order inside each plugin") {
    Built built = Finish({{"movement", Movement}, {"life", Life}, {"host", [](Registrar& r) {
                              Schedules(r);
                              r.AddSystem<&ReadPos>(Sys("ReadPos", Zone::C, {At("FixedUpdate", "Simulate")}));
                              r.AddSystem<&WriteHealth>(Sys("WriteHealth", Zone::C, {At("FixedUpdate", "Simulate")}));
                          }}});
    KERNEL_REQUIRE_OK(built);
    const FrozenRegistry& frozen = built.registry->Frozen();
    CHECK(frozen.ComponentIdOf<TestPos>() == 0U);
    CHECK(frozen.ComponentIdOf<TestVel>() == 1U);
    CHECK(frozen.ComponentIdOf<TestHealth>() == 2U);
    // ResourceId 0 is the kernel's KernelSettings, registered before any plugin.
    CHECK(frozen.ResourceIdOf<KernelSettings>() == 0U);
    CHECK(frozen.ResourceIdOf<TestRes>() == 1U);
    CHECK(frozen.ResourceIdOf<TestOtherRes>() == 2U);
    CHECK(frozen.EventIdOf<TestEvent>() == 0U);
    CHECK(frozen.EventIdOf<TestOtherEvent>() == 1U);
    REQUIRE(frozen.SystemNamed("ReadPos") != nullptr);
    CHECK(frozen.SystemNamed("ReadPos")->registrationSeq == 0U);
    CHECK(frozen.SystemNamed("WriteHealth")->registrationSeq == 1U);
    MESSAGE("J-18 registry fingerprint: " << frozen.RegistryFingerprint().Text());
    CHECK(frozen.RegistryFingerprint().Text() == kJ18RegistryGolden);
    CHECK(frozen.RegistryFingerprint().version == 1U);
}

TEST_CASE("J-18: swapping two plugins gives ids in the new order") {
    Built swapped = Finish({{"life", Life}, {"movement", Movement}, {"host", Schedules}});
    KERNEL_REQUIRE_OK(swapped);
    const FrozenRegistry& frozen = swapped.registry->Frozen();
    CHECK(frozen.ComponentIdOf<TestHealth>() == 0U);
    CHECK(frozen.ComponentIdOf<TestPos>() == 1U);
    CHECK(frozen.ComponentIdOf<TestVel>() == 2U);
    CHECK(frozen.ResourceIdOf<TestOtherRes>() == 1U);
    CHECK(frozen.ResourceIdOf<TestRes>() == 2U);
    CHECK(frozen.EventIdOf<TestOtherEvent>() == 0U);
    Built original = Finish({{"movement", Movement}, {"life", Life}, {"host", Schedules}});
    KERNEL_REQUIRE_OK(original);
    CHECK(original.registry->Frozen().RegistryFingerprint() != frozen.RegistryFingerprint());
}

TEST_CASE("J-18: a type key used before registration does not change ids") {
    // Touch TestVel's key first; ids still follow registration order, not key use.
    const TypeKey first = KeyOf<TestVel>();
    const TypeKey second = KeyOf<TestPos>();
    CHECK(first != second);
    Built built = Finish({{"movement", Movement}, {"host", Schedules}});
    KERNEL_REQUIRE_OK(built);
    CHECK(built.registry->Frozen().ComponentIdOf<TestPos>() == 0U);
    CHECK(built.registry->Frozen().ComponentIdOf<TestVel>() == 1U);
}

TEST_CASE("J-18: two Finishes of the same plugin list give the same fingerprint") {
    Built a = Finish({{"movement", Movement}, {"life", Life}, {"host", Schedules}});
    Built b = Finish({{"movement", Movement}, {"life", Life}, {"host", Schedules}});
    KERNEL_REQUIRE_OK(a);
    KERNEL_REQUIRE_OK(b);
    CHECK(a.registry->Frozen().RegistryFingerprint() == b.registry->Frozen().RegistryFingerprint());
    CHECK(a.registry->Frozen().CFingerprint() == b.registry->Frozen().CFingerprint());
}

TEST_CASE("J-19: lookup creates no id; build runs once per plugin; finish after every build, in list order") {
    std::vector<std::string> calls;
    TestPlugin first("first", [&](Registrar& r) { calls.push_back("build first"); Movement(r); }, {},
                     [&](const FrozenRegistry& frozen, FinishContext&) {
                         calls.push_back("finish first");
                         // Finish reads final ids; an unregistered type has none.
                         CHECK(frozen.ComponentIdOf<TestHealth>() == 2U);
                         CHECK_FALSE(frozen.ComponentIdOf<TestMarker>().has_value());
                     });
    TestPlugin second("second", [&](Registrar& r) { calls.push_back("build second"); Life(r); Schedules(r); }, {"first"},
                      [&](const FrozenRegistry&, FinishContext&) { calls.push_back("finish second"); });
    Registry registry({&first, &second});
    const auto result = registry.Finish();
    REQUIRE(static_cast<bool>(result));
    CHECK(calls == std::vector<std::string>{"build first", "build second", "finish first", "finish second"});
    CHECK(first.builds == 1);
    CHECK(second.finishes == 1);
    CHECK_FALSE(registry.Frozen().ComponentIdOf<TestMarker>().has_value());
    CHECK(registry.Frozen().Components().size() == 3);
    CHECK(registry.IsFrozen());
}

TEST_CASE("J-19: a duplicate plugin name is R-E1 and the duplicate is not built") {
    TestPlugin a("same", Movement);
    TestPlugin b("same", Life);
    TestPlugin host("host", Schedules);
    Registry registry({&a, &b, &host});
    const auto result = registry.Finish();
    CHECK(KernelTest::ErrorCount(result, StartupCode::RE1) == 1);
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK(a.builds == 1);
    CHECK(b.builds == 0);
    CHECK_FALSE(registry.IsFrozen());
}

TEST_CASE("J-19: a missing or later required plugin is R-E3; the kernel does not reorder") {
    TestPlugin early("early", Movement, {"late", "absent"});
    TestPlugin late("late", [](Registrar& r) { Life(r); Schedules(r); });
    Registry registry({&early, &late});
    const auto result = registry.Finish();
    CHECK(KernelTest::ErrorCount(result, StartupCode::RE3) == 2);
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK(result.error().detail.find("comes later") != std::string::npos);
    CHECK(result.error().detail.find("not in the Host's list") != std::string::npos);
}

TEST_CASE("J-19: a type registered by two plugins is R-E2") {
    Built built = Finish({{"movement", Movement}, {"again", [](Registrar& r) {
                              r.Component<TestPos>({.name = "TestPosAgain", .zone = Zone::C});
                              Schedules(r);
                          }}});
    CHECK(KernelTest::ErrorCount(built, StartupCode::RE2) == 1);
    REQUIRE_FALSE(static_cast<bool>(built.result));
    const StartupError& error = built.result.error().errors.front();
    CHECK(std::find(error.subjects.begin(), error.subjects.end(), "movement") != error.subjects.end());
    CHECK(std::find(error.subjects.begin(), error.subjects.end(), "again") != error.subjects.end());
}

TEST_CASE("J-19: removing the owner plugin gives R-E4 to every System that uses its types") {
    const auto host = [](Registrar& r) {
        Schedules(r);
        r.AddSystem<&ReadPos>(Sys("ReadPos", Zone::C, {At("FixedUpdate", "Simulate")}));
        r.AddSystem<&ReadVel>(Sys("ReadVel", Zone::C, {At("FixedUpdate", "Simulate")}));
    };
    Built with = Finish({{"movement", Movement}, {"host", host}});
    KERNEL_REQUIRE_OK(with);
    Built without = Finish({{"host", host}});
    CHECK(KernelTest::ErrorCount(without, StartupCode::RE4) == 2);
    REQUIRE_FALSE(static_cast<bool>(without.result));
    CHECK(without.result.error().detail.find("ReadPos") != std::string::npos);
    CHECK(without.result.error().detail.find("ReadVel") != std::string::npos);
}

TEST_CASE("J-19: registering after Finish is impossible by type") {
    // FinishContext has no Registrar and Registrar has no public constructor.
    static_assert(!std::is_constructible_v<Registrar, Registry&>);
    static_assert(!std::is_default_constructible_v<Registrar>);
    static_assert(!std::is_copy_constructible_v<Registry>);
    CHECK(true);
}

TEST_CASE("J-24: R-E5 (two types share one key) and R-E6 (two runners) are reported") {
    Built collision = Finish({{"movement", Movement}, {"forged", [](Registrar& r) {
                                  TypeKeyCollisionProbe::RegisterComponent(r, KeyOf<TestPos>(), 64, 8,
                                                                           {.name = "Forged", .zone = Zone::C});
                                  Schedules(r);
                              }}});
    CHECK(KernelTest::ErrorCount(collision, StartupCode::RE5) == 1);
    REQUIRE_FALSE(static_cast<bool>(collision.result));
    CHECK(KernelTest::ErrorCount(collision, StartupCode::RE2) == 0);

    TestPlugin a("window", Movement, {}, [](const FrozenRegistry&, FinishContext& c) { c.InstallRunner("window"); });
    TestPlugin b("headless", Schedules, {}, [](const FrozenRegistry&, FinishContext& c) { c.InstallRunner("headless"); });
    Registry registry({&a, &b});
    const auto result = registry.Finish();
    CHECK(KernelTest::ErrorCount(result, StartupCode::RE6) == 1);
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK(result.error().errors.front().subjects == std::vector<std::string>{"window", "headless"});

    TestPlugin one("window", Movement, {}, [](const FrozenRegistry&, FinishContext& c) { c.InstallRunner("window"); });
    TestPlugin other("host", Schedules);
    Registry fine({&one, &other});
    CHECK(static_cast<bool>(fine.Finish()));
}

TEST_CASE("J-24: R-E1 to R-E4 each from a minimal Registry, all in one report") {
    TestPlugin dup1("dup", Movement, {"missing"});
    TestPlugin dup2("dup", Life);
    TestPlugin twice("twice", [](Registrar& r) {
        r.Component<TestPos>({.name = "TestPos2", .zone = Zone::C});
        Schedules(r);
        r.AddSystem<&WriteHealth>(Sys("WriteHealth", Zone::C, {At("FixedUpdate", "Simulate")}));
    });
    Registry registry({&dup1, &dup2, &twice});
    const auto result = registry.Finish();
    REQUIRE_FALSE(static_cast<bool>(result));
    const StartupReport& report = result.error();
    CHECK(report.Has(StartupCode::RE1));
    CHECK(report.Has(StartupCode::RE2));
    CHECK(report.Has(StartupCode::RE3));
    CHECK(report.Has(StartupCode::RE4));  // TestHealth's owner "dup" (the second) was not built
    CHECK(report.errors.size() >= 4);
}
