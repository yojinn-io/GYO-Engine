// Synthetic file of Systems that write decision data, for G-5 (authority
// declaration 5.9; the former spec J-17 moved from the kernel, authority-rollback
// declaration I13): the authority Module's export lists this file (its writers
// are registered here), and tests/common/authority_scan/authority_scan.py must
// read the list and find exactly the three planted findings below
// (authority_sources_check.py holds the expected counts). The plants are real
// code so the file compiles; nothing calls them.
//   ZS1  the <random> include
//   ZS2  the namespace-scope counter
//   ZS6  std::random_device
#include "engine/authority/Registrations.hpp"
#include "engine/kernel/Kernel.hpp"

#include <random>

namespace {

using namespace GYO::Kernel;

struct SyntheticPos {
    int value;
    friend bool operator==(const SyntheticPos&, const SyntheticPos&) = default;
};
void HashValue(Hasher& h, const SyntheticPos& v) { HashValue(h, v.value); }

int syntheticCounter = 0;

SystemResult AdvanceSynthetic(Query<Write<SyntheticPos>>) { return {}; }
SystemResult CheckSynthetic(Query<Read<SyntheticPos>>) { return {}; }

} // namespace

int SyntheticRandomPlant() {
    std::random_device device;
    return static_cast<int>(device()) + syntheticCounter;
}

void RegisterSyntheticSimulation(GYO::Engine::Orchestration::WorldSetup& setup) {
    GYO::Engine::Authority::AuthorityEntry(setup).Component<SyntheticPos>("SyntheticPos");
    Mount first;
    first.schedule = "FixedUpdate";
    first.stage = "Update";
    first.before = {"CheckSynthetic"};
    setup.Registrar().AddSystem<&AdvanceSynthetic>({.name = "AdvanceSynthetic", .mounts = {first}});
    Mount second;
    second.schedule = "FixedUpdate";
    second.stage = "Update";
    // Reads only: not in the list on its own (it shares the file with a writer).
    setup.Registrar().AddSystem<&CheckSynthetic>({.name = "CheckSynthetic", .mounts = {second}});
}
