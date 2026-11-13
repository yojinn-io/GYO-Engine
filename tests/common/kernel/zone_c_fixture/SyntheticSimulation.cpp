// Synthetic zone C System file for spec J-17's kernel half (registry-schedule
// declaration judgement 8): the kernel's G-5 export lists this file (its zone C
// Systems are registered here), and tests/common/zone_scan/zone_scan.py must
// read the list and find exactly the three planted findings below
// (zone_c_export_check.py holds the expected counts). The plants are real code
// so the file compiles; nothing calls them.
//   ZS1  the <random> include
//   ZS2  the namespace-scope counter
//   ZS6  std::random_device
#include "engine/kernel/Kernel.hpp"

#include <random>

namespace {

using namespace GYO::Kernel;

struct SyntheticPos {
    int value;
};

int syntheticCounter = 0;

SystemResult AdvanceSynthetic(Query<Write<SyntheticPos>>) { return {}; }
SystemResult CheckSynthetic(Query<Read<SyntheticPos>>) { return {}; }

} // namespace

int SyntheticRandomPlant() {
    std::random_device device;
    return static_cast<int>(device()) + syntheticCounter;
}

void RegisterSyntheticSimulation(Registrar& registrar) {
    registrar.Component<SyntheticPos>({.name = "SyntheticPos", .zone = Zone::C});
    ScheduleOptions schedule;
    schedule.name = "SyntheticFixed";
    schedule.domain = Domain::Fixed;
    schedule.clock = true;
    schedule.stages = {"Simulate"};
    registrar.Schedule(schedule);
    Mount first;
    first.schedule = "SyntheticFixed";
    first.stage = "Simulate";
    first.before = {"CheckSynthetic"};
    registrar.AddSystem<&AdvanceSynthetic>({.name = "AdvanceSynthetic", .zone = Zone::C, .mounts = {first}});
    Mount second;
    second.schedule = "SyntheticFixed";
    second.stage = "Simulate";
    registrar.AddSystem<&CheckSynthetic>({.name = "CheckSynthetic", .zone = Zone::C, .mounts = {second}});
}
