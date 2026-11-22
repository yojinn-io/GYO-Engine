// C16 (world-gen declaration section 11): the unit table gyo_product_units generates from the real register
// (config/modules.csv) carries entity_ids by default, so a World that lists no unit can Spawn.
#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <vector>

#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;

namespace {

struct Card {
    std::int32_t value = 0;
};

int& Dealt() {
    static int dealt = 0;
    return dealt;
}

K::SystemResult DealCard(K::Spawn<K::Bundle<Card>> spawn) {
    auto id = spawn.New(Card{1});
    if (!id) return ::GYO::Engine::Base::Err(std::move(id).error());
    ++Dealt();
    return {};
}

}  // namespace

TEST_CASE("default: the real register carries entity_ids, so a World of no listed unit spawns") {
    O::RegisteredUnits units = O::MakeRegisteredUnits();
    bool carried = false;
    for (const auto& entry : units.modules) carried = carried || entry.row == "entity_ids";
    CHECK_MESSAGE(carried, "[ei-default] entity_ids is carried by default (config/modules.csv default=yes)");
    O::ProductDescription d;
    d.name = "defaults";
    O::WorldDescription w;
    w.name = "Main";
    w.build = [](O::WorldSetup& s) {
        s.Registrar().Component<Card>({.name = "Card"});
        K::SystemOptions o;
        o.name = "DealCard";
        K::Mount m;
        m.schedule = std::string(O::DefaultSchedules::frame);
        m.stage = std::string(O::DefaultSchedules::update);
        o.mounts.push_back(std::move(m));
        s.Registrar().AddSystem<&DealCard>(o);
    };
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), std::move(units));
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    Dealt() = 0;
    REQUIRE(static_cast<bool>((*built)->RunFrame()));
    CHECK_MESSAGE(Dealt() == 1, "[ei-default-spawn] a World that lists no unit spawns with the default allotter");
}
