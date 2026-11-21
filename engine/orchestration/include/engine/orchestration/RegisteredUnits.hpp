#pragma once

#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/Plugin.hpp"

#include <memory>
#include <string>
#include <vector>

// What a Product is built from on the register side (declaration section 3.2):
// every row of the two register files (config/modules.csv, plugins.csv)
// and the unit instances the product carries, each paired with its row by name.
// gyo_product_units (build/cmake/GyoUnitRegistry.cmake) generates
// MakeRegisteredUnits() for a product target; tests write it by hand.

namespace GYO::Engine::Orchestration {

enum class UnitKind : unsigned char { Module, Plugin };

// One row of a register file.
struct RegisterRow {
    std::string name;
    UnitKind kind = UnitKind::Module;
    // Does not go into a World (a library only; no registration unit).
    bool libraryOnly = true;
    // A Module a product carries unless it disables it (Plugins: always false).
    bool defaultOn = false;
    // Dependencies (Module or Plugin names), public and private alike for the orchestration layer.
    std::vector<std::string> depends;
    std::vector<std::string> privateDepends;
};

template <class Unit>
struct UnitEntry {
    // The register row this instance belongs to.
    std::string row;
    std::unique_ptr<Unit> unit;
};

struct RegisteredUnits {
    // Every row of modules.csv, then every row of plugins.csv, in file order.
    std::vector<RegisterRow> rows;
    // The carried units, in register order (Modules before Plugins).
    std::vector<UnitEntry<Module>> modules;
    std::vector<UnitEntry<Plugin>> plugins;
};

// Defined by the source gyo_product_units generates for a product target.
RegisteredUnits MakeRegisteredUnits();

} // namespace GYO::Engine::Orchestration
