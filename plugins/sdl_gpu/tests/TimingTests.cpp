// The passes' names, debug groups and times (render-pipeline declaration section 15, A3). sdl_gpu pass labels (label
// cpu, ctest sdl_gpu.passlabels): each pass's name from its place in RenderCommands, no GPU. sdl_gpu timing (label gpu,
// ctest sdl_gpu.timing): a frame drawn on an 8 by 8 picture (GPUDevice::Capture) gives one timing per pass, a debug
// group per recorded pass and the frame seen done on the GPU (its fence: within how long of its handing), each within
// the wall time of the Capture that measured it; the times are printed, not judged (as sdl_gpu.budget). sdl_gpu frames in
// flight (label cpu, ctest sdl_gpu.framesinflight): the account of the fences, with scripted times. What a capture tool
// shows of the groups is not seen here: only how many were pushed. Tags "[tm-...]" are the mutants' expectations.
#include <doctest/doctest.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "engine/render/RenderCommands.hpp"
#include "engine/time/MonotonicClock.hpp"
#include "plugins/sdl_gpu/SDLGPUPlugin.hpp"

#include "GPUDevice.hpp"
#include "FramesInFlight.hpp"
#include "PassLabels.hpp"

// The whole file is in the Plugin's namespace (naming N17: a unit opens only its own root, aliases included).
namespace GYO::Plugins::SDLGPU {

namespace G = GYO::Plugins::SDLGPU;
namespace R = GYO::Engine::Render;
namespace T = GYO::Engine::Time;

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

double Micro(const T::Duration d) { return std::chrono::duration<double, std::micro>(d).count(); }

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
    G::FrameTiming timing;
    G::BeginTiming(timing, 7, alike);
    REQUIRE(timing.passes.size() == 2);
    CHECK_MESSAGE(timing.passes[0].name != timing.passes[1].name, "[tm-names-distinct] " << timing.passes[0].name);

    // The frame's timing before it is recorded: one per pass, in order, named, none recorded. Begun again on the same
    // timing (as the device does each frame), what the last frame left is gone.
    timing.passes[0].recorded = true;
    timing.passes[0].cpu = T::Duration{5};
    timing.debugGroups = 2;
    G::BeginTiming(timing, 8, commands);
    CHECK(timing.frame == 8);
    CHECK_MESSAGE(timing.debugGroups == 0, "[tm-timing-groups-reset] " << timing.debugGroups);
    REQUIRE(timing.passes.size() == commands.passes.size());
    std::set<std::string> names;
    for (std::size_t i = 0; i < timing.passes.size(); ++i) {
        CHECK_MESSAGE(timing.passes[i].name == G::PassLabel(commands.passes[i], i), "[tm-timing-named] pass " << i << ": " << timing.passes[i].name);
        CHECK_MESSAGE((!timing.passes[i].recorded && timing.passes[i].cpu == T::Duration{0}), "[tm-timing-unrecorded] pass " << i);
        names.insert(timing.passes[i].name);
    }
    CHECK_MESSAGE(names.size() == commands.passes.size(), "[tm-names-distinct] one name per pass");
}

TEST_CASE("sdl_gpu timing: a captured frame has one timing per pass, a debug group per recorded pass and a GPU time") {
    auto made = G::GPUDevice::Create(nullptr, true);  // SDL's validation on
    REQUIRE_MESSAGE(static_cast<bool>(made), (made ? std::string() : std::string(G::ToString(made.error().code)) + ": " + made.error().message));
    G::GPUDevice& device = **made;
    CHECK_MESSAGE(!device.LastFrameTiming().has_value(), "[tm-timing-none] no frame recorded before the first");
    CHECK_MESSAGE(!device.LastFrameSeenDone().has_value(), "[tm-gpu-none] no frame was seen done before the first");

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
    // The wall time of the Capture: recording, handing to the GPU and the wait all happen inside it, so it bounds every
    // time the frame reports (from outside, not a number written in).
    const T::TimePoint before = T::Now();
    REQUIRE(static_cast<bool>(device.Capture({}, {}, commands, R::TargetSize{8, 8}, tally, receipt)));
    const T::Duration wall = T::Now() - before;
    REQUIRE(tally.unmappedPasses == 1);
    const std::optional<G::FrameTiming> first = device.LastFrameTiming();
    REQUIRE_MESSAGE(first.has_value(), "[tm-one-per-pass] the frame's timing");
    REQUIRE_MESSAGE(first->passes.size() == commands.passes.size(), "[tm-one-per-pass] " << first->passes.size() << " timings");
    T::Duration recording{0};
    for (std::size_t i = 0; i < commands.passes.size(); ++i) {
        CHECK_MESSAGE(first->passes[i].name == G::PassLabel(commands.passes[i], i), "[tm-one-per-pass] pass " << i << ": " << first->passes[i].name);
        MESSAGE("[tm-print] " << first->passes[i].name << ": recorded " << first->passes[i].recorded << ", CPU " << Micro(first->passes[i].cpu)
                              << " us");
        recording += first->passes[i].cpu;
    }
    CHECK_MESSAGE((first->passes[0].recorded && first->passes[2].recorded), "[tm-recorded] the passes with a target are recorded");
    CHECK_MESSAGE((first->passes[0].cpu > T::Duration{0} && first->passes[2].cpu > T::Duration{0}), "[tm-cpu] a recorded pass has its CPU time");
    CHECK_MESSAGE(recording <= wall, "[tm-cpu-within] the passes' CPU times fit in the Capture: " << Micro(recording) << " of " << Micro(wall) << " us");
    CHECK_MESSAGE((!first->passes[1].recorded && first->passes[1].cpu == T::Duration{0}),
                  "[tm-unmapped-unrecorded] the unmapped pass is not recorded and has no time");
    CHECK_MESSAGE(first->debugGroups == 2, "[tm-groups] one debug group per recorded pass: " << first->debugGroups);

    // The frame seen done: Capture waits for its fence before it looks, so it is seen done at its first look (no look
    // found it not done: after is 0), within the Capture.
    const std::optional<G::FrameSeenDone> seen = device.LastFrameSeenDone();
    REQUIRE_MESSAGE(seen.has_value(), "[tm-gpu] the captured frame seen done");
    CHECK_MESSAGE(seen->frame == first->frame, "[tm-gpu-frame] of the same frame: " << seen->frame << " for " << first->frame);
    CHECK_MESSAGE(seen->within > T::Duration{0}, "[tm-gpu] more than nothing");
    CHECK_MESSAGE(seen->within <= wall, "[tm-gpu-within] within the Capture: " << Micro(seen->within) << " of " << Micro(wall) << " us");
    CHECK_MESSAGE(seen->after == T::Duration{0}, "[tm-seen-first-look] no look found it not done: " << Micro(seen->after) << " us");
    MESSAGE("[tm-print] driver " << device.DriverName() << ": frame " << seen->frame << " seen done on the GPU within "
                                 << Micro(seen->within) / 1000.0 << " ms of its handing (the Capture took " << Micro(wall) / 1000.0 << " ms)");

    // The next frame has the next number, and its own timings.
    REQUIRE(static_cast<bool>(device.Capture({}, {}, commands, R::TargetSize{8, 8}, tally, receipt)));
    REQUIRE(device.LastFrameTiming().has_value());
    CHECK_MESSAGE(device.LastFrameTiming()->frame == first->frame + 1, "[tm-frames-count] " << device.LastFrameTiming()->frame);
    CHECK_MESSAGE(device.LastFrameTiming()->debugGroups == 2, "[tm-groups-per-frame] counted per frame: " << device.LastFrameTiming()->debugGroups);
    REQUIRE(device.LastFrameSeenDone().has_value());
    CHECK_MESSAGE(device.LastFrameSeenDone()->frame == first->frame + 1, "[tm-gpu-frame] the latest frame seen done");
}

TEST_CASE("sdl_gpu frames in flight: a frame seen done between the last look that found it not done and the look that found it done") {
    // Fences are numbers here; a frame is done when the test says so. Times are scripted: t0 + n ms.
    const T::TimePoint t0 = T::Now();
    const auto at = [&](const int ms) { return t0 + std::chrono::milliseconds(ms); };
    const auto ms = [](const int n) { return T::Duration{std::chrono::milliseconds(n)}; };
    std::set<int> done;
    std::vector<int> released;
    const auto isDone = [&](const int fence) { return done.contains(fence); };
    const auto release = [&](const int fence) { released.push_back(fence); };

    G::FramesInFlight<int> frames;
    CHECK_MESSAGE(!frames.LastSeenDone().has_value(), "[tm-seen-none] none seen before a frame is handed");
    frames.Hand(1, 101, at(0));
    frames.Look(at(4), isDone, release);  // frame 1 not done 4 ms after its handing
    CHECK(!frames.LastSeenDone().has_value());
    CHECK(released.empty());
    frames.Hand(2, 102, at(10));
    done.insert(101);
    frames.Look(at(12), isDone, release);  // frame 1 done, frame 2 not (2 ms after its handing)
    REQUIRE(frames.LastSeenDone().has_value());
    CHECK_MESSAGE(frames.LastSeenDone()->frame == 1, "[tm-seen-frame] " << frames.LastSeenDone()->frame);
    CHECK_MESSAGE(frames.LastSeenDone()->within == ms(12), "[tm-seen-within] handed at 0, seen done at 12");
    CHECK_MESSAGE(frames.LastSeenDone()->after == ms(4), "[tm-seen-after] not done at 4: the last look before");
    CHECK_MESSAGE((released == std::vector<int>{101}), "[tm-seen-released] the fence seen done is let go, once");
    CHECK(frames.InFlight() == 1);

    done.insert(102);
    frames.Look(at(20), isDone, release);
    REQUIRE(frames.LastSeenDone().has_value());
    CHECK_MESSAGE((frames.LastSeenDone()->frame == 2 && frames.LastSeenDone()->within == ms(10) && frames.LastSeenDone()->after == ms(2)),
                  "[tm-seen-after] frame 2: handed at 10, not done at 12, seen done at 20");
    CHECK((released == std::vector<int>{101, 102}));
    CHECK(frames.InFlight() == 0);

    // Two frames seen done at one look: the latest is kept, both fences let go.
    frames.Hand(3, 103, at(30));
    frames.Hand(4, 104, at(31));
    done.insert(103);
    done.insert(104);
    frames.Look(at(35), isDone, release);
    CHECK_MESSAGE(frames.LastSeenDone()->frame == 4, "[tm-gpu-frame] the latest frame seen done: " << frames.LastSeenDone()->frame);
    CHECK((released == std::vector<int>{101, 102, 103, 104}));

    // The device's end lets every fence still in flight go.
    frames.Hand(5, 105, at(40));
    frames.Clear(release);
    CHECK_MESSAGE((released.back() == 105 && frames.InFlight() == 0), "[tm-seen-cleared] the fences in flight let go at the end");
}

} // namespace GYO::Plugins::SDLGPU
