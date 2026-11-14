// Second synthetic zone C System file for spec J-17's kernel half: it sorts
// before SyntheticSimulation.cpp in byte order but is registered after it, so
// the export's file list must be sorted, not in registration order (contract
// gyo.zone-c-sources v1: files sorted and unique). No planted finding here.
#include "engine/kernel/Kernel.hpp"

namespace {

using namespace GYO::Kernel;

struct AnotherPos {
    int value;
};

SystemResult StepAnother(Query<Write<AnotherPos>>) { return {}; }

} // namespace

void RegisterAnotherSimulation(Registrar& registrar) {
    registrar.Component<AnotherPos>({.name = "AnotherPos", .zone = Zone::C});
    Mount mount;
    mount.schedule = "SyntheticFixed";
    mount.stage = "Simulate";
    registrar.AddSystem<&StepAnother>({.name = "StepAnother", .zone = Zone::C, .mounts = {mount}});
}
