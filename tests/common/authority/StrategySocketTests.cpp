// The authority Module's strategy socket (v8-arch-gyo2-authority-rollback step
// 3; authority declaration 4.4 and 4.6): a World with no strategy builds; the
// peer-to-peer placeholder reports AU-E1 at startup; an author's strategy is
// asked to check. Each CHECK carries a tag in brackets ("[au-...]"): the step's
// mutants expect it (docs/architecture/target/v2/scripts/authority_mutations.json).

#include "engine/authority/AuthorityModule.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/rollback/RollbackModule.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

#include <doctest/doctest.h>

#include <memory>
#include <string>

namespace {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace A = GYO::Engine::Authority;
namespace R = GYO::Engine::Rollback;
namespace W = GYO::Engine::WorldTime;

O::RegisterRow ModuleRow(std::string name, std::vector<std::string> depends = {}) {
    O::RegisterRow row;
    row.name = std::move(name);
    row.kind = O::UnitKind::Module;
    row.libraryOnly = false;
    row.defaultOn = true;
    row.depends = std::move(depends);
    return row;
}

O::RegisteredUnits Units() {
    O::RegisteredUnits units;
    units.rows.push_back(ModuleRow("world_time"));
    units.modules.push_back({"world_time", std::make_unique<W::WorldTimeModule>()});
    units.rows.push_back(ModuleRow("rollback", {"world_time"}));
    units.modules.push_back({"rollback", std::make_unique<R::RollbackModule>()});
    units.rows.push_back(ModuleRow("authority", {"world_time", "rollback"}));
    units.modules.push_back({"authority", std::make_unique<A::AuthorityModule>()});
    return units;
}

O::ProductDescription Game(O::OptionBag options = {}) {
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = "Main";
    w.options = std::move(options);
    d.worlds.push_back(std::move(w));
    return d;
}

template <class Built>
std::string Reported(const Built& built) {
    if (built) return "built";
    std::string out;
    for (const O::UnitReportedError& error : built.error().unitErrors) out += error.code + " " + error.message + "\n";
    return out.empty() ? built.error().detail : out;
}

// An author's strategy that only records that it was asked to check.
class Watching final : public A::AuthorityStrategy {
public:
    explicit Watching(int& checks) : checks_(&checks) {}
    [[nodiscard]] std::string_view Name() const override { return "Watching"; }
    [[nodiscard]] std::uint32_t Version() const override { return 1; }
    void OnRegister(A::RegisterContext&) override {}
    void Check(A::CheckContext& context) override {
        ++*checks_;
        if (context.Registry().Version() == 0) context.Report("AU-TEST", "no registry");
    }
    [[nodiscard]] K::Fingerprint ConsistencyFingerprint(A::FingerprintContext&) override { return {}; }

private:
    int* checks_;
};

} // namespace

TEST_CASE("authority socket: a World with no strategy builds (a render World, a single-player game)") {
    auto built = O::Product::Build(Game(), Units());
    CHECK_MESSAGE(static_cast<bool>(built), "[au-no-strategy] " << Reported(built));
}

TEST_CASE("authority socket: the peer-to-peer placeholder reports AU-E1 at startup") {
    O::OptionBag options;
    options.Set(A::AuthorityOptions{std::make_shared<A::PeerAuthorityStrategy>()});
    auto built = O::Product::Build(Game(std::move(options)), Units());
    CHECK_MESSAGE(Reported(built).find("AU-E1") != std::string::npos, "[au-e1-peer] " << Reported(built));
}

TEST_CASE("authority socket: an author's strategy goes through the same socket: its Check runs once per World") {
    int checks = 0;
    O::OptionBag options;
    options.Set(A::AuthorityOptions{std::make_shared<Watching>(checks)});
    auto built = O::Product::Build(Game(std::move(options)), Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    CHECK_MESSAGE(checks == 1, "[au-strategy-check] " << checks);
}
