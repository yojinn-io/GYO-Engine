// Second synthetic writer file for G-5: it sorts before SyntheticSimulation.cpp
// in byte order but is registered after it, so the export's file list must be
// sorted, not in registration order (contract: files sorted and unique). No
// planted finding here.
#include "engine/authority/Registrations.hpp"
#include "engine/kernel/Kernel.hpp"

namespace {

using namespace GYO::Kernel;

struct AnotherPos {
    int value;
    friend bool operator==(const AnotherPos&, const AnotherPos&) = default;
};
void HashValue(Hasher& h, const AnotherPos& v) { HashValue(h, v.value); }

SystemResult StepAnother(Query<Write<AnotherPos>>) { return {}; }

} // namespace

void RegisterAnotherSimulation(GYO::Engine::Orchestration::WorldSetup& setup) {
    GYO::Engine::Authority::AuthorityEntry(setup).Component<AnotherPos>("AnotherPos");
    Mount mount;
    mount.schedule = "FixedUpdate";
    mount.stage = "Update";
    setup.Registrar().AddSystem<&StepAnother>({.name = "StepAnother", .mounts = {mount}});
}
