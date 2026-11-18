// The table gyo_product_units generates (declaration section 3.2 point 3;
// Design Invariant O-6): built from the synthetic registers next to this file
// with DISABLE_MODULES off, MODULES extra, PLUGINS tool, then given to
// Product::Build as it is.

#include "orchestration_units/Units.hpp"

#include <doctest/doctest.h>

#include <ostream>
#include <string>
#include <vector>

namespace {

namespace O = GYO::Engine::Orchestration;

std::string Joined(const std::vector<std::string>& lines) {
    std::string out;
    for (const std::string& line : lines) out += (out.empty() ? "" : " | ") + line;
    return out;
}

} // namespace

TEST_CASE("units: the generated table has every register row in file order, with kind, flags and dependencies") {
    const O::RegisteredUnits units = O::MakeRegisteredUnits();
    std::vector<std::string> names;
    for (const O::RegisterRow& row : units.rows) names.push_back(row.name);
    CHECK_MESSAGE((names == std::vector<std::string>{"base", "kernel", "walk", "extra", "off", "tool", "spare"}),
                  "[or-units-rows] " << Joined(names));
    REQUIRE(units.rows.size() == 7);
    const O::RegisterRow& kernel = units.rows[1];
    CHECK_MESSAGE((kernel.libraryOnly && !kernel.defaultOn && kernel.depends == std::vector<std::string>{"base"}),
                  "[or-units-row-kernel]");
    const O::RegisterRow& extra = units.rows[3];
    CHECK_MESSAGE((extra.kind == O::UnitKind::Module && !extra.libraryOnly && !extra.defaultOn &&
                   extra.depends == std::vector<std::string>{"walk"} &&
                   extra.privateDepends == std::vector<std::string>{"base"}),
                  "[or-units-row-extra]");
    CHECK_MESSAGE((units.rows[4].defaultOn && units.rows[5].kind == O::UnitKind::Plugin && !units.rows[5].defaultOn),
                  "[or-units-row-off-tool]");
}

TEST_CASE("units: the generated table carries default Modules minus the disabled, plus the listed Module and Plugin") {
    const O::RegisteredUnits units = O::MakeRegisteredUnits();
    std::vector<std::string> modules;
    for (const auto& entry : units.modules) modules.push_back(entry.row);
    std::vector<std::string> plugins;
    for (const auto& entry : units.plugins) plugins.push_back(entry.row);
    CHECK_MESSAGE((modules == std::vector<std::string>{"walk", "extra"}), "[or-units-carried-modules] " << Joined(modules));
    CHECK_MESSAGE((plugins == std::vector<std::string>{"tool"}), "[or-units-carried-plugins] " << Joined(plugins));
    for (const auto& entry : units.modules) CHECK(entry.unit != nullptr);
    for (const auto& entry : units.plugins) CHECK(entry.unit != nullptr);
}

TEST_CASE("units: a Product builds from the generated table; the units run in register order, the product's own last") {
    OrchUnits::Log().clear();
    O::ProductDescription description;
    description.name = "probe";
    O::WorldDescription world;
    world.name = "Main";
    world.build = [](O::WorldSetup& setup) { OrchUnits::Log().push_back("build probe@" + setup.WorldName()); };
    description.worlds.push_back(std::move(world));
    auto built = O::Product::Build(std::move(description), O::MakeRegisteredUnits());
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    const std::vector<std::string> expected = {"configure walk", "configure extra", "configure tool",
                                               "build walk@Main", "build extra@Main", "build tool@Main",
                                               "build probe@Main"};
    CHECK_MESSAGE(OrchUnits::Log() == expected, "[or-units-product] " << Joined(OrchUnits::Log()));
}
