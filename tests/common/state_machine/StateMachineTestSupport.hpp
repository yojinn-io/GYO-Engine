#pragma once

// The test product of the state machine tests (state machine declaration
// docs/architecture/target/v2/state-machine-declaration.zh-Hant.md, section 7): Flow (MainMenu / InLevel, initial
// MainMenu) with two children that exist under InLevel, Pause (Running / Paused) and Inventory (Closed / Open),
// registered in that order; a second tree Flow2 whose child KPause keeps its value when its parent is entered again;
// and an unrelated Weather (Clear / Rain). Every enter and exit schedule has a System that records its name and the
// values it reads; an action table lets a case make a request or fail inside one.

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/state_machine/Conditions.hpp"
#include "engine/state_machine/RegisterStateMachine.hpp"
#include "engine/state_machine/StateMachine.hpp"

namespace GYO::Test::StateMachine {

enum class Flow : std::uint8_t { MainMenu, InLevel };
enum class Pause : std::uint8_t { Running, Paused };
enum class Inventory : std::uint8_t { Closed, Open };
enum class Flow2 : std::uint8_t { MainMenu, InLevel };
enum class KPause : std::uint8_t { Running, Paused };
enum class KInventory : std::uint8_t { Closed, Open };
enum class Weather : std::uint8_t { Clear, Rain };

} // namespace GYO::Test::StateMachine

namespace TSM = GYO::Test::StateMachine;

template <>
struct GYO::Engine::StateMachine::StateMachineTraits<TSM::Flow> {
    static constexpr std::string_view name = "Flow";
    static constexpr std::array values{ValueName<TSM::Flow>{TSM::Flow::MainMenu, "MainMenu"},
                                       ValueName<TSM::Flow>{TSM::Flow::InLevel, "InLevel"}};
    static constexpr TSM::Flow initial = TSM::Flow::MainMenu;
    using SubStates = Kernel::TypeList<TSM::Pause, TSM::Inventory>;
};
template <>
struct GYO::Engine::StateMachine::StateMachineTraits<TSM::Pause> {
    static constexpr std::string_view name = "Pause";
    static constexpr std::array values{ValueName<TSM::Pause>{TSM::Pause::Running, "Running"},
                                       ValueName<TSM::Pause>{TSM::Pause::Paused, "Paused"}};
    static constexpr TSM::Pause initial = TSM::Pause::Running;
    using Parent = TSM::Flow;
    static constexpr TSM::Flow parentValue = TSM::Flow::InLevel;
};
template <>
struct GYO::Engine::StateMachine::StateMachineTraits<TSM::Inventory> {
    static constexpr std::string_view name = "Inventory";
    static constexpr std::array values{ValueName<TSM::Inventory>{TSM::Inventory::Closed, "Closed"},
                                       ValueName<TSM::Inventory>{TSM::Inventory::Open, "Open"}};
    static constexpr TSM::Inventory initial = TSM::Inventory::Closed;
    using Parent = TSM::Flow;
    static constexpr TSM::Flow parentValue = TSM::Flow::InLevel;
};
template <>
struct GYO::Engine::StateMachine::StateMachineTraits<TSM::Flow2> {
    static constexpr std::string_view name = "Flow2";
    static constexpr std::array values{ValueName<TSM::Flow2>{TSM::Flow2::MainMenu, "MainMenu"},
                                       ValueName<TSM::Flow2>{TSM::Flow2::InLevel, "InLevel"}};
    static constexpr TSM::Flow2 initial = TSM::Flow2::MainMenu;
    using SubStates = Kernel::TypeList<TSM::KPause, TSM::KInventory>;
};
template <>
struct GYO::Engine::StateMachine::StateMachineTraits<TSM::KPause> {
    static constexpr std::string_view name = "KPause";
    static constexpr std::array values{ValueName<TSM::KPause>{TSM::KPause::Running, "Running"},
                                       ValueName<TSM::KPause>{TSM::KPause::Paused, "Paused"}};
    static constexpr TSM::KPause initial = TSM::KPause::Running;
    using Parent = TSM::Flow2;
    static constexpr TSM::Flow2 parentValue = TSM::Flow2::InLevel;
    static constexpr bool keepOnParentReenter = true;
};
template <>
struct GYO::Engine::StateMachine::StateMachineTraits<TSM::KInventory> {
    static constexpr std::string_view name = "KInventory";
    static constexpr std::array values{ValueName<TSM::KInventory>{TSM::KInventory::Closed, "Closed"},
                                       ValueName<TSM::KInventory>{TSM::KInventory::Open, "Open"}};
    static constexpr TSM::KInventory initial = TSM::KInventory::Closed;
    using Parent = TSM::Flow2;
    static constexpr TSM::Flow2 parentValue = TSM::Flow2::InLevel;
};
template <>
struct GYO::Engine::StateMachine::StateMachineTraits<TSM::Weather> {
    static constexpr std::string_view name = "Weather";
    static constexpr std::array values{ValueName<TSM::Weather>{TSM::Weather::Clear, "Clear"},
                                       ValueName<TSM::Weather>{TSM::Weather::Rain, "Rain"}};
    static constexpr TSM::Weather initial = TSM::Weather::Clear;
};

namespace GYO::Test::StateMachine {

namespace K = ::GYO::Kernel;
namespace O = ::GYO::Engine::Orchestration;
namespace SM = ::GYO::Engine::StateMachine;

// What every recording System saw, in order.
inline std::vector<std::string>& Events() {
    static std::vector<std::string> events;
    return events;
}
// The values an enter or exit System read, by its event name ("F=..,P=..,I=..": a value name, "-" absent,
// "0" not entered).
inline std::map<std::string, std::string>& Seen() {
    static std::map<std::string, std::string> seen;
    return seen;
}

template <class S>
std::string Text(const SM::StateMachine<S>& m) {
    switch (m.Status()) {
    case SM::StateStatus::NotEntered: return "0";
    case SM::StateStatus::Absent: return "-";
    case SM::StateStatus::Active: break;
    }
    return std::string(SM::NameOf(*m.Current()).value_or("?"));
}

template <class F, class P, class I>
struct Tree {
    using Root = F;
    using First = P;
    using Second = I;
};
using MainTree = Tree<Flow, Pause, Inventory>;
using KeepTree = Tree<Flow2, KPause, KInventory>;

// What an action in an enter or exit System can do on the main tree.
struct Ctx {
    SM::StateMachineRequest<Flow>& flow;
    SM::StateMachineRequest<Pause>& pause;
    SM::StateMachineRequest<Inventory>& inventory;
};
using Action = std::function<K::SystemResult(Ctx&)>;
inline std::map<std::string, Action>& Actions() {
    static std::map<std::string, Action> actions;
    return actions;
}

inline K::SystemResult Fail(std::string what) {
    return ::GYO::Engine::Base::Err(K::KernelError(K::KernelErrorCode::SystemFailed, std::move(what)));
}

template <class TreeT, class S, S V, bool Enter>
K::SystemResult Note(K::Res<SM::StateMachine<typename TreeT::Root>> root, K::Res<SM::StateMachine<typename TreeT::First>> first,
                     K::Res<SM::StateMachine<typename TreeT::Second>> second,
                     K::ResMut<SM::StateMachineRequest<typename TreeT::Root>> rootRequest,
                     K::ResMut<SM::StateMachineRequest<typename TreeT::First>> firstRequest,
                     K::ResMut<SM::StateMachineRequest<typename TreeT::Second>> secondRequest) {
    const std::string name =
        std::string(Enter ? "Enter." : "Exit.") + std::string(SM::StateName<S>()) + "." + std::string(*SM::NameOf(V));
    Events().push_back(name);
    Seen()[name] = "F=" + Text(*root) + ",P=" + Text(*first) + ",I=" + Text(*second);
    if constexpr (std::is_same_v<TreeT, MainTree>) {
        if (auto found = Actions().find(name); found != Actions().end()) {
            Ctx ctx{*rootRequest, *firstRequest, *secondRequest};
            return found->second(ctx);
        }
    }
    return {};
}

template <class S, S V, bool Enter>
K::SystemResult NoteWeather(K::Res<SM::StateMachine<Weather>>) {
    Events().push_back(std::string(Enter ? "Enter." : "Exit.") + "Weather." + std::string(*SM::NameOf(V)));
    return {};
}

inline K::SystemOptions At(std::string name, std::string schedule, std::string stage,
                           std::vector<K::ConditionRef> conditions = {}, std::vector<std::string> sets = {}) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = std::move(schedule);
    m.stage = std::move(stage);
    m.conditions = std::move(conditions);
    m.sets = std::move(sets);
    o.mounts.push_back(std::move(m));
    return o;
}

template <class TreeT, class S, std::size_t... I>
void AddNotes(K::Registrar& r, std::index_sequence<I...>) {
    constexpr auto& values = SM::StateMachineTraits<S>::values;
    const std::string stage(SM::kEnterExitStage);
    (r.AddSystem<&Note<TreeT, S, values[I].value, true>>(
         At("Note." + SM::EnterScheduleName(values[I].value), SM::EnterScheduleName(values[I].value), stage)),
     ...);
    (r.AddSystem<&Note<TreeT, S, values[I].value, false>>(
         At("Note." + SM::ExitScheduleName(values[I].value), SM::ExitScheduleName(values[I].value), stage)),
     ...);
}

template <class TreeT>
void AddTreeNotes(K::Registrar& r) {
    AddNotes<TreeT, typename TreeT::Root>(r, std::make_index_sequence<2>{});
    AddNotes<TreeT, typename TreeT::First>(r, std::make_index_sequence<2>{});
    AddNotes<TreeT, typename TreeT::Second>(r, std::make_index_sequence<2>{});
}

template <std::size_t... I>
void AddWeatherNotes(K::Registrar& r, std::index_sequence<I...>) {
    constexpr auto& values = SM::StateMachineTraits<Weather>::values;
    const std::string stage(SM::kEnterExitStage);
    (r.AddSystem<&NoteWeather<Weather, values[I].value, true>>(
         At("Note." + SM::EnterScheduleName(values[I].value), SM::EnterScheduleName(values[I].value), stage)),
     ...);
    (r.AddSystem<&NoteWeather<Weather, values[I].value, false>>(
         At("Note." + SM::ExitScheduleName(values[I].value), SM::ExitScheduleName(values[I].value), stage)),
     ...);
}

// A probe System: records "<id>:<Flow>" when it runs (its conditions decide).
template <int Id>
K::SystemResult Probe(K::Res<SM::StateMachine<Flow>> flow) {
    Events().push_back("probe" + std::to_string(Id) + ":" + Text(*flow));
    return {};
}

struct Setup {
    SM::StateMachineOptions options;
    // Register Flow2 (the keeping tree) instead of Flow.
    bool keepTree = false;
    // Also register Weather, after the main tree, at the same anchor.
    bool weather = false;
    std::function<void(O::WorldSetup&)> extra;
    std::vector<O::PrepareFn> prepare;
    std::vector<std::string> gameInit;
    // Register the main tree twice (R-E2).
    bool twice = false;
};

inline std::unique_ptr<O::Product> Make(Setup setup, O::RegisteredUnits units = {}) {
    Events().clear();
    Seen().clear();
    Actions().clear();
    O::ProductDescription d;
    d.name = "game";
    d.gameInit = setup.gameInit;
    O::WorldDescription w;
    w.name = "Main";
    w.build = [setup](O::WorldSetup& s) {
        K::Registrar& r = s.Registrar();
        if (setup.keepTree) {
            SM::RegisterStateMachine<Flow2>(s, setup.options);
            AddTreeNotes<KeepTree>(r);
        } else {
            SM::RegisterStateMachine<Flow>(s, setup.options);
            if (setup.twice) SM::RegisterStateMachine<Flow>(s, setup.options);
            AddTreeNotes<MainTree>(r);
        }
        if (setup.weather) {
            SM::RegisterStateMachine<Weather>(s, setup.options);
            AddWeatherNotes(r, std::make_index_sequence<2>{});
        }
        if (setup.extra) setup.extra(s);
    };
    w.prepare = setup.prepare;
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), std::move(units));
    if (!built) return nullptr;
    return std::move(*built);
}

inline K::World& WorldOf(O::Product& p) { return p.World("Main").GetWorld(); }

template <class S>
SM::StateMachineRequest<S>& Req(O::Product& p) {
    auto* r = WorldOf(p).ResourceMut<SM::StateMachineRequest<S>>();
    REQUIRE(r != nullptr);
    return *r;
}
template <class S>
const SM::StateMachine<S>& Mach(O::Product& p) {
    const auto* m = WorldOf(p).Resource<SM::StateMachine<S>>();
    REQUIRE(m != nullptr);
    return *m;
}

// Runs one frame and returns the events it recorded.
inline std::vector<std::string> Frame(O::Product& p) {
    Events().clear();
    auto ran = p.RunFrame();
    REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + " " + ran.error().detail));
    return Events();
}

inline bool HasRule(const O::OrchestrationError& error, std::string_view rule) {
    for (const K::KernelError& k : error.kernel) {
        for (const K::KernelError* leaf : k.Leaves()) {
            if (leaf->rule == rule) return true;
        }
    }
    return false;
}

using List = std::vector<std::string>;

} // namespace GYO::Test::StateMachine
