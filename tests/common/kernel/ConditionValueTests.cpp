// Run conditions with a value (target/kernel DI-KS 1; the user's decision U2, mode-config declaration section 12):
// Condition<&Fn>(name, value) and the Kernel's ResourceExists, ResourceEquals and ResourceExistsAndEquals (Bevy's
// resource_exists, resource_equals and resource_exists_and_equals, bevy_ecs/src/schedule/condition.rs:688, 728, 770).
// From outside: whether a System under the condition runs when the Resource is equal, different or not there; the
// value's hash in the fingerprints (one name, other values: other fingerprints; a condition without a value keeps its
// fingerprint). Each CHECK carries a tag in brackets ("[cv-...]"): the step's mutants expect it.

#include "WorldTestSupport.hpp"

#include "engine/kernel/Conditions.hpp"

#include <string>

namespace {

using namespace GYO::Test::Kernel;

// A Resource compared by value.
struct Level final {
    std::int32_t value = 0;
    friend bool operator==(const Level&, const Level&) = default;
};
void HashValue(Hasher& h, const Level& v) { HashValue(h, v.value); }

SystemResult Mark() {
    Log().push_back("ran");
    return Ok();
}

// A condition with a value of its own type: the threshold the Level reaches.
bool AtLeast(const std::int32_t& threshold, Res<Level> level) { return level.Present() && level->value >= threshold; }

SystemOptions Under(ConditionRef condition) {
    SystemOptions options = B("Mark", "A");
    options.mounts.front().conditions.push_back(std::move(condition));
    return options;
}

// Whether Mark ran in one Run of Tick, with Level put (or not) first.
bool Runs(ConditionRef condition, const std::optional<Level> level) {
    Log().clear();
    auto rig = MakeRig([condition = std::move(condition)](Registrar& r) {
        Data(r);
        r.Resource<Level>({.name = "Level"});
        r.AddSystem<&Mark>(Under(condition));
    });
    if (level) rig->world().SetResource(*level);
    REQUIRE(static_cast<bool>(rig->Run()));
    return Log().size() == 1;
}

std::string FingerprintOf(ConditionRef condition) {
    auto built = Finish({{"test", [condition = std::move(condition)](Registrar& r) {
                              Data(r);
                              r.Resource<Level>({.name = "Level"});
                              r.AddSystem<&Mark>(Under(condition));
                          }}});
    REQUIRE(static_cast<bool>(built.result));
    return built.registry->Frozen().RegistryFingerprint().Text();
}

} // namespace

TEST_CASE("DI-KS 1: ResourceExistsAndEquals runs on an equal value, not on another, not when the Resource is not there") {
    CHECK_MESSAGE(Runs(ResourceExistsAndEquals("level.two", Level{2}), Level{2}), "[cv-exists-equal]");
    CHECK_FALSE_MESSAGE(Runs(ResourceExistsAndEquals("level.two", Level{2}), Level{3}), "[cv-exists-other]");
    CHECK_FALSE_MESSAGE(Runs(ResourceExistsAndEquals("level.two", Level{2}), std::nullopt), "[cv-exists-absent]");
}

TEST_CASE("DI-KS 1: ResourceEquals runs on an equal value, not on another; the Resource not there is an assertion") {
    CHECK_MESSAGE(Runs(ResourceEquals("level.two", Level{2}), Level{2}), "[cv-equals-equal]");
    CHECK_FALSE_MESSAGE(Runs(ResourceEquals("level.two", Level{2}), Level{3}), "[cv-equals-other]");
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.Resource<Level>({.name = "Level"});
        r.AddSystem<&Mark>(Under(ResourceEquals("level.two", Level{2})));
    });
    GYO_CHECK_ASSERTS(rig->Run());
}

TEST_CASE("DI-KS 1: ResourceExists runs when the Resource is there, whatever its value") {
    CHECK_MESSAGE(Runs(ResourceExists<Level>("level.there"), Level{7}), "[cv-there]");
    CHECK_FALSE_MESSAGE(Runs(ResourceExists<Level>("level.there"), std::nullopt), "[cv-not-there]");
}

TEST_CASE("DI-KS 1: Condition<&Fn>(name, value) calls Fn with the value given at registration") {
    CHECK_MESSAGE(Runs(Condition<&AtLeast>("level.at_least", 3), Level{4}), "[cv-value-passed]");
    CHECK_FALSE_MESSAGE(Runs(Condition<&AtLeast>("level.at_least", 5), Level{4}), "[cv-value-passed]");
}

TEST_CASE("DI-KS 1: a condition's value is in the fingerprints; one without a value keeps its identity, the name") {
    const std::string two = FingerprintOf(ResourceExistsAndEquals("level.is", Level{2}));
    CHECK_MESSAGE(FingerprintOf(ResourceExistsAndEquals("level.is", Level{2})) == two, "[cv-fingerprint-same]");
    CHECK_MESSAGE(FingerprintOf(ResourceExistsAndEquals("level.is", Level{3})) != two, "[cv-fingerprint-value]");
    CHECK(FingerprintOf(Condition<&AtLeast>("level.at_least", 3)) != FingerprintOf(Condition<&AtLeast>("level.at_least", 4)));
    const ConditionRef plain = ResourceExists<Level>("level.there");
    CHECK_MESSAGE(plain.Identity() == "level.there", "[cv-identity-plain]");
    const std::string identity = ResourceEquals("level.is", Level{2}).Identity();
    CHECK(identity.rfind("level.is=", 0) == 0);
    CHECK(identity.size() == std::string("level.is=").size() + 16);
}

TEST_CASE("DI-KS 1: Condition<&Fn>(name, value) takes the value as Fn's first parameter, const V&") {
    CHECK(ValuedConditionRejectionOf<&AtLeast>() == Rejection::None);
    CHECK_MESSAGE(ValuedConditionRejectionOf<&Mark>() == Rejection::ConditionValue, "[cv-shape]");
}
