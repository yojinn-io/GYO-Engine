#pragma once

// The test Product of the authority Module's tests (authority-rollback
// declaration I15): one World "Main" on the default skeleton with the time, the
// rollback and the authority Modules, built from synthetic data only.

#include "engine/authority/AuthorityModule.hpp"
#include "engine/authority/Consistency.hpp"
#include "engine/authority/Registrations.hpp"
#include "engine/authority/ServerAuthorityStrategy.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/rollback/RollbackModule.hpp"
#include "engine/rollback/StateRollbackStrategy.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

#include <doctest/doctest.h>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace AuthorityTest {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace A = GYO::Engine::Authority;
namespace R = GYO::Engine::Rollback;
namespace W = GYO::Engine::WorldTime;

struct Health {
    std::int32_t hp = 0;
    friend bool operator==(const Health&, const Health&) = default;
};
inline void HashValue(K::Hasher& h, const Health& v) { HashValue(h, v.hp); }

struct Armor {
    std::int32_t value = 0;
    friend bool operator==(const Armor&, const Armor&) = default;
};
inline void HashValue(K::Hasher& h, const Armor& v) { HashValue(h, v.value); }

struct Command {
    std::int32_t move = 0;
    friend bool operator==(const Command&, const Command&) = default;
};
inline void HashValue(K::Hasher& h, const Command& v) { HashValue(h, v.move); }

struct Score {
    std::int32_t value = 0;
    friend bool operator==(const Score&, const Score&) = default;
};
inline void HashValue(K::Hasher& h, const Score& v) { HashValue(h, v.value); }

struct Hit {
    std::int32_t at = 0;
};
inline void HashValue(K::Hasher& h, const Hit& v) { HashValue(h, v.at); }

// Pushed events (observers watch them).
struct HitLanded {
    std::int32_t at = 0;
};
struct BlockLanded {
    std::int32_t at = 0;
};

// Not decision data: a screen-side value.
struct Glow {
    std::int32_t level = 0;
};
inline void HashValue(K::Hasher& h, const Glow& v) { HashValue(h, v.level); }

inline O::RegisterRow ModuleRow(std::string name, std::vector<std::string> depends = {}) {
    O::RegisterRow row;
    row.name = std::move(name);
    row.kind = O::UnitKind::Module;
    row.libraryOnly = false;
    row.defaultOn = true;
    row.depends = std::move(depends);
    return row;
}

inline O::RegisteredUnits Units() {
    O::RegisteredUnits units;
    units.rows.push_back(ModuleRow("world_time"));
    units.modules.push_back({"world_time", std::make_unique<W::WorldTimeModule>()});
    units.rows.push_back(ModuleRow("rollback", {"world_time"}));
    units.modules.push_back({"rollback", std::make_unique<R::RollbackModule>()});
    units.rows.push_back(ModuleRow("authority", {"world_time", "rollback"}));
    units.modules.push_back({"authority", std::make_unique<A::AuthorityModule>()});
    return units;
}

struct Setting {
    std::shared_ptr<A::AuthorityStrategy> authority = std::make_shared<A::ServerAuthorityStrategy>();
    std::shared_ptr<R::RollbackStrategy> rollback;
    bool dynamicInsert = false;
};

inline O::ProductDescription Game(std::function<void(O::WorldSetup&)> build, const Setting& setting) {
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = "Main";
    w.dynamicInsert = setting.dynamicInsert;
    if (setting.authority) w.options.Set(A::AuthorityOptions{setting.authority});
    if (setting.rollback) w.options.Set(R::RollbackOptions{setting.rollback});
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

template <class Built>
std::string Reported(const Built& built) {
    if (built) return "built";
    std::string out;
    for (const O::UnitReportedError& error : built.error().unitErrors) out += error.code + " " + error.message + "\n";
    return out.empty() ? built.error().detail : out;
}

inline auto Build(std::function<void(O::WorldSetup&)> build, const Setting& setting = {}) {
    return O::Product::Build(Game(std::move(build), setting), Units());
}

inline std::string BuildText(std::function<void(O::WorldSetup&)> build, const Setting& setting = {}) {
    return Reported(Build(std::move(build), setting));
}

} // namespace AuthorityTest
