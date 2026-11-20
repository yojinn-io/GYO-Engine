// A product's main and how it handles the runner's exit (O-3; error levels: the exit flow).
// The Engine gives no main: the default runner (Product::Run) returns RunnerExit and writes
// nothing; this main is the layer that decides what follows, so it logs once and picks the
// exit code here. A product that wants something else (back to its menu with another Product)
// writes that instead.
//
//   gyo_example_product_main        a good frame: exit code 0, nothing written
//   gyo_example_product_main bad    a broken frame: one Error line with the reason, exit code 1

#include "engine/base/Log.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"

#include <memory>
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

void LogError(const std::string& text) {
    ::Engine::Base::Log(::Engine::Base::LogLevel::Error, "example", text);
}

} // namespace

int main(int argc, char** argv) {
    const bool breakFrame = argc > 1 && std::string_view(argv[1]) == "bad";

    auto built = O::Product::Build(Describe(breakFrame), O::RegisteredUnits{});
    if (!built) {
        const O::ProductStartupReport& report = built.error();
        LogError("the Product did not start: " + report.message + (report.detail.empty() ? "" : "\n" + report.detail));
        return 1;
    }

    O::RunnerExit exit;
    {
        // Destroy the Product and its Worlds before main returns.
        std::unique_ptr<O::Product> product = std::move(built).value();
        exit = product->Run();
    }
    if (exit.code == O::RunnerExitCode::Failure) {
        LogError(exit.reason); // for a broken World the reason carries the WorldFailure text
        return 1;
    }
    return 0;
}
