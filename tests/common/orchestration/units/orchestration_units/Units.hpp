#pragma once

// Synthetic registration units named by the synthetic registers next to this
// directory (modules.csv, plugins.csv). gyo_product_units generates the table
// that instantiates the carried ones (declaration section 3.2 point 3).

#include "engine/orchestration/Orchestration.hpp"

#include <string>
#include <vector>

namespace OrchUnits {

namespace O = GYO::Engine::Orchestration;

inline std::vector<std::string>& Log() {
    static std::vector<std::string> log;
    return log;
}

template <class Base, const char* Name>
class Logged : public Base {
public:
    void Configure(O::ProductSetup&) override { Log().push_back(std::string("configure ") + Name); }
    void Build(O::WorldSetup& setup) override { Log().push_back(std::string("build ") + Name + "@" + setup.WorldName()); }
};

inline constexpr char kWalk[] = "walk";
inline constexpr char kExtra[] = "extra";
inline constexpr char kOff[] = "off";
inline constexpr char kTool[] = "tool";
inline constexpr char kSpare[] = "spare";

class WalkModule final : public Logged<O::Module, kWalk> {};
class ExtraModule final : public Logged<O::Module, kExtra> {};
class OffModule final : public Logged<O::Module, kOff> {};
class ToolPlugin final : public Logged<O::Plugin, kTool> {};
class SparePlugin final : public Logged<O::Plugin, kSpare> {};

} // namespace OrchUnits
