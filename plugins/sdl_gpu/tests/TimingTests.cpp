// The passes' names, debug groups and times (render-pipeline declaration section 15, A3). sdl_gpu pass labels (label
// cpu, ctest sdl_gpu.passlabels): each pass's name from its place in RenderCommands, no GPU. sdl_gpu timing (label gpu,
// ctest sdl_gpu.timing): a frame drawn on an 8 by 8 picture (GPUDevice::Capture) gives one timing per pass, a debug
// group per recorded pass and the whole frame's GPU time from its fence; the times are printed, not judged (as
// sdl_gpu.budget). What a capture tool shows of the groups is not seen here: only how many were pushed. Tags "[tm-...]"
// are the mutants' expectations.
#include <doctest/doctest.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <utility>

#include "engine/render/RenderCommands.hpp"
#include "plugins/sdl_gpu/SDLGPUPlugin.hpp"

#include "GPUDevice.hpp"
#include "PassLabels.hpp"

// The whole file is in the Plugin's namespace (naming N17: a unit opens only its own root, aliases included).
namespace GYO::Plugins::SDLGPU {

namespace G = GYO::Plugins::SDLGPU;
namespace R = GYO::Engine::Render;

namespace {

// A frame of the four kinds of pass, on the main target and on render targets 3 and 5.
R::RenderCommands FourKinds() {
    R::RenderCommands commands;
    R::PassDepth depth;
    depth.target = R::TextureId{3};
    commands.passes.push_back(depth);
    commands.passes.push_back(R::Pass3D{});
    R::PassFullscreen fullscreen;
    fullscreen.target = R::TextureId{5};
    fullscreen.shader = "test_invert.frag";
    commands.passes.push_back(fullscreen);
    R::Pass2D overlay;
    overlay.target = R::TextureId{5};
    commands.passes.push_back(overlay);
    return commands;
}

} // namespace

TEST_CASE("sdl_gpu pass labels: each pass is named by its place in RenderCommands, its kind and its target") {
    const R::RenderCommands commands = FourKinds();
    CHECK_MESSAGE(G::PassLabel(commands.passes[0], 0) == "#0 PassDepth -> target 3", "[tm-names-kind] " << G::PassLabel(commands.passes[0], 0));
    CHECK_MESSAGE(G::PassLabel(commands.passes[1], 1) == "#1 Pass3D -> main", "[tm-names-target] " << G::PassLabel(commands.passes[1], 1));
    CHECK_MESSAGE(G::PassLabel(commands.passes[2], 2) == "#2 PassFullscreen test_invert.frag -> target 5",
                  "[tm-names-shader] " << G::PassLabel(commands.passes[2], 2));
    CHECK_MESSAGE(G::PassLabel(commands.passes[3], 3) == "#3 Pass2D -> target 5", "[tm-names-target] " << G::PassLabel(commands.passes[3], 3));

    // Two passes alike are told apart by their place.
    R::RenderCommands alike;
    alike.passes.push_back(R::Pass2D{});
    alike.passes.push_back(R::Pass2D{});
    const G::FrameTiming timing = G::TimingOf(7, alike);
    REQUIRE(timing.passes.size() == 2);
    CHECK_MESSAGE(timing.passes[0].name != timing.passes[1].name, "[tm-names-distinct] " << timing.passes[0].name);

    // The frame's timing before it is recorded: one per pass, in order, named, none recorded.
    const G::FrameTiming four = G::TimingOf(7, commands);
    CHECK(four.frame == 7);
    CHECK(four.debugGroups == 0);
    REQUIRE(four.passes.size() == commands.passes.size());
    std::set<std::string> names;
    for (std::size_t i = 0; i < four.passes.size(); ++i) {
        CHECK_MESSAGE(four.passes[i].name == G::PassLabel(commands.passes[i], i), "[tm-timing-named] pass " << i << ": " << four.passes[i].name);
        CHECK_MESSAGE((!four.passes[i].recorded && four.passes[i].cpu == std::chrono::nanoseconds{0}), "[tm-timing-unrecorded] pass " << i);
        names.insert(four.passes[i].name);
    }
    CHECK_MESSAGE(names.size() == commands.passes.size(), "[tm-names-distinct] one name per pass");
}

TEST_CASE("sdl_gpu timing: a captured frame has one timing per pass, a debug group per recorded pass and a GPU time") {
    auto made = G::GPUDevice::Create(nullptr, true);  // SDL's validation on
    REQUIRE_MESSAGE(static_cast<bool>(made), (made ? std::string() : std::string(G::ToString(made.error().code)) + ": " + made.error().message));
    G::GPUDevice& device = **made;
    CHECK_MESSAGE(!device.LastGPUFrameTime().has_value(), "[tm-gpu-none] no frame was seen done before the first");

    // A 2D pass on the main target, a pass on a target that is not mapped (never made), a 3D pass on the main target.
    R::RenderCommands commands;
    R::Pass2D clear;
    clear.clear = R::Color8{0, 0, 255, 255};
    commands.passes.push_back(clear);
    R::Pass2D unmapped;
    unmapped.target = R::TextureId{999};
    unmapped.clear = R::Color8{255, 0, 0, 255};
    commands.passes.push_back(unmapped);
    commands.passes.push_back(R::Pass3D{});

    G::FrameTally tally;
    R::RenderFrameReceipt receipt;
    REQUIRE(static_cast<bool>(device.Capture({}, {}, commands, R::TargetSize{8, 8}, tally, receipt)));
    REQUIRE(tally.unmappedPasses == 1);
    const G::FrameTiming first = device.LastFrameTiming();
    REQUIRE_MESSAGE(first.passes.size() == commands.passes.size(), "[tm-one-per-pass] " << first.passes.size() << " timings");
    for (std::size_t i = 0; i < commands.passes.size(); ++i) {
        CHECK_MESSAGE(first.passes[i].name == G::PassLabel(commands.passes[i], i), "[tm-one-per-pass] pass " << i << ": " << first.passes[i].name);
        MESSAGE("[tm-print] " << first.passes[i].name << ": recorded " << first.passes[i].recorded << ", CPU "
                              << std::chrono::duration<double, std::micro>(first.passes[i].cpu).count() << " us");
    }
    CHECK_MESSAGE((first.passes[0].recorded && first.passes[2].recorded), "[tm-recorded] the passes with a target are recorded");
    CHECK_MESSAGE((first.passes[0].cpu > std::chrono::nanoseconds{0} && first.passes[2].cpu > std::chrono::nanoseconds{0}),
                  "[tm-cpu] a recorded pass has its CPU time");
    CHECK_MESSAGE((!first.passes[1].recorded && first.passes[1].cpu == std::chrono::nanoseconds{0}),
                  "[tm-unmapped-unrecorded] the unmapped pass is not recorded and has no time");
    CHECK_MESSAGE(first.debugGroups == 2, "[tm-groups] one debug group per recorded pass: " << first.debugGroups);

    // The whole frame's GPU time: Capture waits for its fence, so the frame is seen done.
    const std::optional<G::GPUFrameTime> gpu = device.LastGPUFrameTime();
    REQUIRE_MESSAGE(gpu.has_value(), "[tm-gpu] the captured frame's GPU time");
    CHECK_MESSAGE(gpu->frame == first.frame, "[tm-gpu-frame] of the same frame: " << gpu->frame << " for " << first.frame);
    CHECK_MESSAGE(gpu->atMost > std::chrono::nanoseconds{0}, "[tm-gpu] more than nothing");
    MESSAGE("[tm-print] driver " << device.DriverName() << ": frame " << gpu->frame << " done on the GPU within "
                                 << std::chrono::duration<double, std::milli>(gpu->atMost).count() << " ms of its submission");

    // The next frame has the next number, and its own timings.
    REQUIRE(static_cast<bool>(device.Capture({}, {}, commands, R::TargetSize{8, 8}, tally, receipt)));
    CHECK_MESSAGE(device.LastFrameTiming().frame == first.frame + 1, "[tm-frames-count] " << device.LastFrameTiming().frame);
    CHECK_MESSAGE(device.LastFrameTiming().debugGroups == 2, "[tm-groups-per-frame] counted per frame: " << device.LastFrameTiming().debugGroups);
    REQUIRE(device.LastGPUFrameTime().has_value());
    CHECK_MESSAGE(device.LastGPUFrameTime()->frame == first.frame + 1, "[tm-gpu-frame] the latest frame seen done");
}

} // namespace GYO::Plugins::SDLGPU
