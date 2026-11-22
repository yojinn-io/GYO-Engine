// Save versions (restore-entry declaration docs/architecture/target/v2/restore-entry-declaration.zh-Hant.md,
// section 19.5; Design Invariants SV-1 to SV-3 in docs/architecture/target/v2/save-version.zh-Hant.md). A Policy the
// author keeps, {current 5, oldestSupported 4}; the refusal goes through the orchestration layer's load paths. Each
// CHECK carries a tag the mutants of 19.5 expect.
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
// MSVC: doctest messages stream std::string_view (ToString of the codes); that needs the full ostream.
#include <ostream>
#include <string>
#include <utility>

#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/save_version/SaveVersion.hpp"

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace V = GYO::Engine::SaveVersion;

namespace {

constexpr V::Policy kSaveVersion{5, 4};

// One World; startupSave, when set, is the version a startup load checks in the description's prepare function.
O::ProductDescription Description(std::optional<std::uint32_t> startupSave = std::nullopt) {
    O::ProductDescription d;
    d.name = "rpg";
    O::WorldDescription w;
    w.name = "Main";
    if (startupSave) {
        const std::uint32_t saved = *startupSave;
        w.prepare.push_back([saved](K::World&) { return kSaveVersion.Check(saved); });
    }
    d.worlds.push_back(std::move(w));
    return d;
}

std::unique_ptr<O::Product> Make() {
    auto built = O::Product::Build(Description(), O::RegisteredUnits{});
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    return std::move(*built);
}

} // namespace

TEST_CASE("check: the oldest supported and the current version are taken") {
    CHECK_MESSAGE(static_cast<bool>(kSaveVersion.Check(4)), "[sv-accept-oldest]");
    CHECK_MESSAGE(static_cast<bool>(kSaveVersion.Check(5)), "[sv-accept-current]");
}

TEST_CASE("check: an older save refused in the extra prepare: ReplacePrepareFailed with the too-old code") {
    auto p = Make();
    auto replaced = p->ReplaceWorld("Main", O::ReplaceRequest{[](K::World&) { return kSaveVersion.Check(3); }, {}, O::ReplaceOrder::BuildNewFirst});
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(replaced), "[sv-too-old-refused] the too-old save was taken");
    CHECK_MESSAGE((replaced.error().code == O::OrchestrationErrorCode::ReplacePrepareFailed &&
                   replaced.error().reporterCode == "save-version-too-old"),
                  "[sv-too-old] " << O::ToString(replaced.error().code) << " " << replaced.error().reporterCode);
}

TEST_CASE("check: a newer save is refused with the too-new code") {
    auto refused = kSaveVersion.Check(6);
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(refused), "[sv-too-new-refused] the too-new save was taken");
    CHECK_MESSAGE(refused.error().reporterCode == "save-version-too-new", "[sv-too-new] " << refused.error().reporterCode);
}

TEST_CASE("check: a startup load refused in the description's prepare fails the build as O-E7, the code in the text") {
    auto built = O::Product::Build(Description(3u), O::RegisteredUnits{});
    REQUIRE_FALSE(static_cast<bool>(built));
    const auto& errors = built.error().errors;
    CHECK_MESSAGE(std::any_of(errors.begin(), errors.end(),
                              [](const O::OrchestrationStartupError& e) {
                                  return e.code == O::OrchestrationCode::OE7 && e.message.find("save-version-too-old") != std::string::npos;
                              }),
                  "[sv-startup] " << built.error().detail);
}

TEST_CASE("check: oldestSupported equal to current takes only the current version (19.4 item 1)") {
    constexpr V::Policy onlyCurrent{5, 5};
    CHECK_MESSAGE((static_cast<bool>(onlyCurrent.Check(5)) && !onlyCurrent.Check(4) && !onlyCurrent.Check(6)), "[sv-equal]");
}

TEST_CASE("check: Current gives the version to write into a save") {
    CHECK_MESSAGE(kSaveVersion.Current() == 5, "[sv-current] " << kSaveVersion.Current());
}
