// A third synthetic file for G-5: it registers a decision type and a System that only reads it, so the export
// does not list it (only what writes decision data is listed, authority declaration 5.9). No planted finding here.
#include "engine/authority/Registrations.hpp"
#include "engine/kernel/Kernel.hpp"

namespace {

using namespace GYO::Kernel;

struct ReaderOnlyPos {
    int value;
    friend bool operator==(const ReaderOnlyPos&, const ReaderOnlyPos&) = default;
};
void HashValue(Hasher& h, const ReaderOnlyPos& v) { HashValue(h, v.value); }

SystemResult WatchReaderOnly(Query<Read<ReaderOnlyPos>>) { return {}; }

} // namespace

void RegisterReaderOnly(GYO::Engine::Orchestration::WorldSetup& setup) {
    GYO::Engine::Authority::AuthorityEntry(setup).Component<ReaderOnlyPos>("ReaderOnlyPos");
    Mount mount;
    mount.schedule = "FixedUpdate";
    mount.stage = "Update";
    setup.Registrar().AddSystem<&WatchReaderOnly>({.name = "WatchReaderOnly", .mounts = {mount}});
}
