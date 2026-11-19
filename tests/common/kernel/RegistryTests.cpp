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
    r.Message<TestEvent>({.name = "TestEvent", .zone = Zone::C, .domain = Domain::Fixed});
}

void Life(Registrar& r) {
    r.Component<TestHealth>({.name = "TestHealth", .zone = Zone::C, .rollback = Rollback::Yes});
    r.Resource<TestOtherRes>({.name = "TestOtherRes", .zone = Zone::B});
    r.Message<TestOtherEvent>({.name = "TestOtherEvent", .zone = Zone::C, .domain = Domain::Fixed});
}

void Schedules(Registrar& r) {
    r.Schedule(FixedSchedule());
}

} // namespace

// The Registry fingerprint golden of the J-18 registry (movement, life, host in
// this order). Same value on linux-x64, windows-x64 and macos-arm64
// (declaration judgement 4); gyo_kernel_tests_reversed checks it too (link order).
inline constexpr const char* kJ18RegistryGolden =
    "5:530eaa66f80b7f8e038200db96df6a8c4eb01860fb11491a526b17f30a1ded70";

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
    CHECK(frozen.MessageIdOf<TestEvent>() == 0U);
    CHECK(frozen.MessageIdOf<TestOtherEvent>() == 1U);
    REQUIRE(frozen.SystemNamed("ReadPos") != nullptr);
    CHECK(frozen.SystemNamed("ReadPos")->registrationSeq == 0U);
    CHECK(frozen.SystemNamed("WriteHealth")->registrationSeq == 1U);
    MESSAGE("J-18 registry fingerprint: " << frozen.RegistryFingerprint().Text());
    CHECK(frozen.RegistryFingerprint().Text() == kJ18RegistryGolden);
    CHECK(frozen.RegistryFingerprint().version == 5U);
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
    CHECK(frozen.MessageIdOf<TestOtherEvent>() == 0U);
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

// The two fields v8-arch-gyo2-kernel-subtract-structure added to the Registry
// fingerprint (declaration 3.4, 3.11; DI-KS 3): each changes it.
namespace {
SystemResult EditsNothingListed(ExclusiveWorld<>) { return Ok(); }
SystemResult EditsListingOther(ExclusiveWorld<Writes<TestOtherRes>>) { return Ok(); }
Built BoundOrNot(bool threadBound) {
    return Finish({{"test", [threadBound](Registrar& r) {
                        Schedules(r);
                        r.Resource<TestOtherRes>({.name = "TestOtherRes", .zone = Zone::B, .threadBound = threadBound});
                    }}});
}
template <auto System>
Built Editing() {
    return Finish({{"test", [](Registrar& r) {
                        r.Schedule(UpdateSchedule());
                        r.Resource<TestOtherRes>({.name = "TestOtherRes", .zone = Zone::B});
                        r.AddSystem<System>(Sys("Edit", Zone::B, {At("Update", "First")}));
                    }}});
}
} // namespace

TEST_CASE("J-18: the Registry fingerprint sees threadBound") {
    const Built plain = BoundOrNot(false);
    const Built bound = BoundOrNot(true);
    KERNEL_REQUIRE_OK(plain);
    KERNEL_REQUIRE_OK(bound);
    CHECK_MESSAGE(plain.registry->Frozen().RegistryFingerprint() != bound.registry->Frozen().RegistryFingerprint(),
                  "J-18 threadBound: two Registries that differ only in ResourceOptions::threadBound have one fingerprint");
}

TEST_CASE("J-18: the Registry fingerprint sees annotatedWrites") {
    const Built none = Editing<&EditsNothingListed>();
    const Built listed = Editing<&EditsListingOther>();
    KERNEL_REQUIRE_OK(none);
    KERNEL_REQUIRE_OK(listed);
    CHECK_MESSAGE(none.registry->Frozen().RegistryFingerprint() != listed.registry->Frozen().RegistryFingerprint(),
                  "J-18 annotatedWrites: ExclusiveWorld<> and ExclusiveWorld<Writes<R>> give one fingerprint");
}

// The plugin list (build once each, finish after every build in list order,
// R-E1, R-E3) is the orchestration layer's since v8-arch-gyo2-orchestration-skeleton
// (tests/common/orchestration, O-2, O-E1, O-E3).
TEST_CASE("J-19: lookup creates no id; the view after BuildPlans has final ids") {
    Registry registry;
    const auto result = RegisterAll(registry, {{"first", Movement}, {"second", [](Registrar& r) { Life(r); Schedules(r); }}});
    REQUIRE(static_cast<bool>(result));
    CHECK(registry.Frozen().ComponentIdOf<TestHealth>() == 2U);
    CHECK_FALSE(registry.Frozen().ComponentIdOf<TestMarker>().has_value());
    CHECK(registry.Frozen().Components().size() == 3);
    CHECK(registry.IsFrozen());
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

// Group H of v8-arch-gyo2-kernel-subtract-structure (declaration 5.1): registering
// again after the Plans are built is Registry::Register's job now
// (RegistrationRoundTests.cpp, DI-KS 26); what stays is that a Registrar exists
// only inside a Register call's build callable.
TEST_CASE("J-19: a Registrar exists only inside Register") {
    // Registrar has no public constructor.
    static_assert(!std::is_constructible_v<Registrar, Registry&>);
    static_assert(!std::is_default_constructible_v<Registrar>);
    static_assert(!std::is_copy_constructible_v<Registry>);
    CHECK(true);
}

TEST_CASE("J-24: R-E5 (two types share one key) is reported") {
    Built collision = Finish({{"movement", Movement}, {"forged", [](Registrar& r) {
                                  TypeKeyCollisionProbe::RegisterComponent(r, KeyOf<TestPos>(), 64, 8,
                                                                           {.name = "Forged", .zone = Zone::C});
                                  Schedules(r);
                              }}});
    CHECK(KernelTest::ErrorCount(collision, StartupCode::RE5) == 1);
    REQUIRE_FALSE(static_cast<bool>(collision.result));
    CHECK(KernelTest::ErrorCount(collision, StartupCode::RE2) == 0);
}

TEST_CASE("J-24: R-E2 and R-E4 each from a minimal Registry, all in one report") {
    Built built = Finish({{"twice", [](Registrar& r) {
                               r.Component<TestPos>({.name = "TestPos", .zone = Zone::C});
                               r.Component<TestPos>({.name = "TestPos2", .zone = Zone::C});
                               Schedules(r);
                               r.AddSystem<&WriteHealth>(Sys("WriteHealth", Zone::C, {At("FixedUpdate", "Simulate")}));
                           }}});
    REQUIRE_FALSE(static_cast<bool>(built.result));
    const StartupReport& report = built.result.error();
    CHECK(report.Has(StartupCode::RE2));
    CHECK(report.Has(StartupCode::RE4));  // TestHealth is registered by no one
    CHECK(report.errors.size() >= 2);
}

// DI-KS 15 (target/kernel.zh-Hant.md): the startup errors are only the kept list
// of the kernel's duties, plus the codes still in transition (taken out by a
// later batch: then they leave this table in the same change). Read through
// ToString: an enum value without a text is "Unknown" and not a code.
TEST_CASE("DI-KS 15: the StartupCode values are the kernel's kept codes plus the codes still in transition") {
    std::vector<std::string> codes;
    for (int value = 0; value <= 255; ++value) {
        const std::string_view text = ToString(static_cast<StartupCode>(value));
        if (text != "Unknown") codes.emplace_back(text);
    }
    std::sort(codes.begin(), codes.end());
    // Kept (integration declaration section 3.1, plus R-E7: a later round registered what only the first round
    // may; declaration v8-arch-gyo2-kernel-subtract-structure 3.2 rule 6, inside duty (12) "registration").
    std::vector<std::string> expected{"D-E1", "D-E3", "D-E4", "D-E5", "D-E6", "D-E7", "D-E14", "D-E17", "D-E19", "D-E20",
                                      "R-E2", "R-E4", "R-E5", "R-E7"};
    // In transition, still in the kernel: clock and event domains (batch 3: D-E12, F-E1); zones, the writer
    // count and rollback declarations (batch 4, authority and rollback: D-E2, D-E9, D-E11, D-E15, D-E16, X-E1).
    // (Startup and the plugin list, D-E21, R-E1, R-E3, R-E6, left with batch 2, the orchestration layer.)
    for (const char* code : {"D-E12", "F-E1", "D-E2", "D-E9", "D-E11", "D-E15", "D-E16", "X-E1"}) {
        expected.emplace_back(code);
    }
    std::sort(expected.begin(), expected.end());
    std::string listed;
    for (const std::string& code : codes) listed += (listed.empty() ? "" : " ") + code;
    CHECK_MESSAGE(codes == expected, "DI-KS 15 codes: the StartupCode values are " << listed);
}
