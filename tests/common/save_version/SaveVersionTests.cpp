// The save version Module (restore-entry declaration docs/architecture/target/v2/restore-entry-declaration.zh-Hant.md,
// section 19.5, cases V1 to V6 and V8; Design Invariants SV-1 to SV-3 in
// docs/architecture/target/v2/save-version.zh-Hant.md). A Product with SaveVersionModule and units written by hand;
// options {current 5, oldestSupported 4}. Each CHECK carries a tag the mutants of 19.5 expect.
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/save_version/SaveVersionModule.hpp"

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace V = GYO::Engine::SaveVersion;

namespace {

O::RegisteredUnits VersionUnits() {
    O::RegisteredUnits units;
    O::RegisterRow row;
    row.name = "save_version";
    row.kind = O::UnitKind::Module;
    row.libraryOnly = false;
    row.defaultOn = false;
    units.rows.push_back(row);
    units.modules.push_back({"save_version", std::make_unique<V::SaveVersionModule>()});
    return units;
}

// A World of one schedule-less registration; startupSave, when set, is the version a startup load checks in a
// prepare function the build callback registers (the Policy taken from the Product's buffer, 19.1 position 3).
O::WorldDescription MainWorld(std::optional<std::uint32_t> startupSave = std::nullopt) {
    O::WorldDescription w;
    w.name = "Main";
    w.build = [startupSave](O::WorldSetup& s) {
        if (!startupSave) return;
        const V::Policy* policy = std::as_const(s.ProductBuffer()).Find<V::Policy>();
        REQUIRE(policy != nullptr);
        const std::uint32_t saved = *startupSave;
        s.Prepare([policy, saved](K::World&) { return policy->Check(saved); });
    };
    return w;
}

O::ProductDescription Description(std::optional<V::SaveVersionOptions> options, std::optional<std::uint32_t> startupSave = std::nullopt) {
    O::ProductDescription d;
    d.name = "rpg";
    if (options) d.options.Set(*options);
    d.worlds.push_back(MainWorld(startupSave));
    return d;
}

std::unique_ptr<O::Product> Make() {
    auto built = O::Product::Build(Description(V::SaveVersionOptions{5, 4}), VersionUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    return std::move(*built);
}

const V::Policy* PolicyOf(O::Product& p) { return std::as_const(p.Buffer()).Find<V::Policy>(); }

bool HasUnitError(const O::ProductStartupReport& report, std::string_view code) {
    return std::any_of(report.unitErrors.begin(), report.unitErrors.end(),
                       [&](const O::UnitReportedError& e) { return e.unit == "save_version" && e.code == code; });
}

} // namespace

TEST_CASE("check: the oldest supported and the current version are taken") {
    auto p = Make();
    const V::Policy* policy = PolicyOf(*p);
    REQUIRE(policy != nullptr);
    CHECK_MESSAGE(static_cast<bool>(policy->Check(4)), "[sv-accept-oldest]");
    CHECK_MESSAGE(static_cast<bool>(policy->Check(5)), "[sv-accept-current]");
}

TEST_CASE("check: an older save refused in the extra prepare: ReplacePrepareFailed with the too-old code") {
    auto p = Make();
    const V::Policy* policy = PolicyOf(*p);
    REQUIRE(policy != nullptr);
    auto replaced = p->ReplaceWorld("Main", O::ReplaceRequest{[policy](K::World&) { return policy->Check(3); }, {}, O::ReplaceOrder::BuildNewFirst});
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(replaced), "[sv-too-old-refused] the too-old save was taken");
    CHECK_MESSAGE((replaced.error().code == O::OrchestrationErrorCode::ReplacePrepareFailed &&
                   replaced.error().reporterCode == "save-version-too-old"),
                  "[sv-too-old] " << O::ToString(replaced.error().code) << " " << replaced.error().reporterCode);
}

TEST_CASE("check: a newer save is refused with the too-new code") {
    auto p = Make();
    const V::Policy* policy = PolicyOf(*p);
    REQUIRE(policy != nullptr);
    auto refused = policy->Check(6);
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(refused), "[sv-too-new-refused] the too-new save was taken");
    CHECK_MESSAGE(refused.error().reporterCode == "save-version-too-new", "[sv-too-new] " << refused.error().reporterCode);
}

TEST_CASE("check: a runner finds the Policy in the Product's buffer and checks before replacing") {
    auto p = Make();
    const V::Policy* policy = PolicyOf(*p);
    REQUIRE_MESSAGE(policy != nullptr, "[sv-buffer-policy] no Policy in the Product's buffer");
    auto refused = policy->Check(3);
    CHECK_MESSAGE((!refused && refused.error().reporterCode == "save-version-too-old"), "[sv-buffer-check]");
}

TEST_CASE("check: a startup load refused in a prepare function fails the build as O-E7, the code in the text") {
    auto built = O::Product::Build(Description(V::SaveVersionOptions{5, 4}, 3u), VersionUnits());
    REQUIRE_FALSE(static_cast<bool>(built));
    const auto& errors = built.error().errors;
    CHECK_MESSAGE(std::any_of(errors.begin(), errors.end(),
                              [](const O::OrchestrationStartupError& e) {
                                  return e.code == O::OrchestrationCode::OE7 && e.message.find("save-version-too-old") != std::string::npos;
                              }),
                  "[sv-startup] " << built.error().detail);
}

TEST_CASE("configure: options out of order or missing fail the build with the Module's report") {
    auto order = O::Product::Build(Description(V::SaveVersionOptions{5, 6}), VersionUnits());
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(order), "[sv-config-order] built with oldestSupported > current");
    CHECK_MESSAGE(HasUnitError(order.error(), V::SaveVersionCodes::optionsOrder), "[sv-config-order-report] " << order.error().detail);
    auto missing = O::Product::Build(Description(std::nullopt), VersionUnits());
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(missing), "[sv-config-missing] built without SaveVersionOptions");
    CHECK_MESSAGE(HasUnitError(missing.error(), V::SaveVersionCodes::optionsMissing), "[sv-config-missing-report] " << missing.error().detail);
}

TEST_CASE("configure: oldestSupported equal to current builds and takes only the current version (19.4 item 1)") {
    auto built = O::Product::Build(Description(V::SaveVersionOptions{5, 5}), VersionUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), "[sv-config-equal] " << (built ? std::string() : built.error().detail));
    const V::Policy* policy = PolicyOf(**built);
    REQUIRE(policy != nullptr);
    CHECK_MESSAGE((static_cast<bool>(policy->Check(5)) && !policy->Check(4)), "[sv-config-equal-check]");
}

TEST_CASE("check: Current gives the version to write into a save") {
    auto p = Make();
    const V::Policy* policy = PolicyOf(*p);
    REQUIRE(policy != nullptr);
    CHECK_MESSAGE(policy->Current() == 5, "[sv-current] " << policy->Current());
}
