#pragma once

// The test Product of the rollback Module's tests (authority-rollback declaration I15): one World "Main" on the
// default skeleton with the time Module and the rollback Module, built from synthetic data only (no product).

#include "AssertTestSupport.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/rollback/RollbackModule.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <utility>

namespace GYO::Test::Rollback {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace R = GYO::Engine::Rollback;
namespace W = GYO::Engine::WorldTime;

struct Health {
    std::int32_t hp = 0;
    friend bool operator==(const Health&, const Health&) = default;
};
inline void HashValue(K::Hasher& h, const Health& v) { HashValue(h, v.hp); }

struct Score {
    std::int32_t value = 0;
    friend bool operator==(const Score&, const Score&) = default;
};
inline void HashValue(K::Hasher& h, const Score& v) { HashValue(h, v.value); }

struct Command {
    std::int32_t move = 0;
    friend bool operator==(const Command&, const Command&) = default;
};
inline void HashValue(K::Hasher& h, const Command& v) { HashValue(h, v.move); }

inline O::RegisterRow ModuleRow(std::string name, std::vector<std::string> depends = {}) {
    O::RegisterRow row;
    row.name = std::move(name);
    row.kind = O::UnitKind::Module;
    row.libraryOnly = false;
    row.defaultOn = true;
    row.depends = std::move(depends);
    return row;
}

// The time Module and, unless told otherwise, the rollback Module, in the register's order.
inline O::RegisteredUnits Units(bool rollback = true) {
    O::RegisteredUnits units;
    units.rows.push_back(ModuleRow("world_time"));
    units.modules.push_back({"world_time", std::make_unique<W::WorldTimeModule>()});
    if (rollback) {
        units.rows.push_back(ModuleRow("rollback", {"world_time"}));
        units.modules.push_back({"rollback", std::make_unique<R::RollbackModule>()});
    }
    return units;
}

// One World "Main" whose own build is build; options go into the World description.
inline O::ProductDescription Game(std::function<void(O::WorldSetup&)> build, O::OptionBag options = {}) {
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = "Main";
    w.options = std::move(options);
    w.build = std::move(build);
    d.worlds.push_back(std::move(w));
    return d;
}

inline K::SystemOptions In(std::string name, std::string schedule, std::string stage) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = std::move(schedule);
    m.stage = std::move(stage);
    o.mounts.push_back(std::move(m));
    return o;
}

// The reported unit errors of a failed build, one per line ("code message").
template <class Built>
std::string Reported(const Built& built) {
    if (built) return "built";
    std::string out;
    for (const O::UnitReportedError& error : built.error().unitErrors) out += error.code + " " + error.message + "\n";
    return out.empty() ? built.error().detail : out;
}

} // namespace GYO::Test::Rollback
