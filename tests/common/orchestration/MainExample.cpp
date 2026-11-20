// The standard main of a product (O-3; error levels: the exit flow), kept compiling and run
// by orchestration.O-3-main (MainExampleProbe.cmake). With "bad" its one frame runs a System
// that returns an Err: the default runner ends with Failure and returns the reason (it writes
// nothing); RunProduct, the layer that decides what follows, writes one Error line with that
// reason and returns exit code 1 after the Product is destroyed. Without "bad" the frame
// succeeds and the exit code is 0.

#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;

struct Tag {
    int t = 0;
};

K::SystemResult Step(K::Query<K::Read<Tag>>) {
    return {};
}

K::SystemResult Break(K::Query<K::Read<Tag>>) {
    return Engine::Base::Err(K::KernelError(K::KernelErrorCode::SystemFailed, "the example System failed"));
}

K::ScheduleOptions Schedule(std::string name) {
    K::ScheduleOptions s;
    s.name = std::move(name);
    s.stages = {"S"};
    return s;
}

K::SystemOptions In(std::string name, std::string schedule) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = std::move(schedule);
    m.stage = "S";
    o.mounts.push_back(std::move(m));
    return o;
}

O::ProductDescription Describe(bool breakFrame) {
    O::WorldDescription world;
    world.name = "Main";
    world.skeleton = O::ScheduleSkeleton::Off;
    world.build = [](O::WorldSetup& setup) {
        K::Registrar& r = setup.Registrar();
        r.Component<Tag>({.name = "Tag"});
        r.Schedule(Schedule("Step"));
        r.Schedule(Schedule("Bad"));
        r.AddSystem<&Step>(In("Step", "Step"));
        r.AddSystem<&Break>(In("Break", "Bad"));
    };
    world.frame = {breakFrame ? "Bad" : "Step"};
    O::ProductDescription description;
    description.name = "example";
    description.worlds.push_back(std::move(world));
    return description;
}

} // namespace

int main(int argc, char** argv) {
    const bool breakFrame = argc > 1 && std::string_view(argv[1]) == "bad";
    return O::RunProduct(Describe(breakFrame), O::RegisteredUnits{});
}
