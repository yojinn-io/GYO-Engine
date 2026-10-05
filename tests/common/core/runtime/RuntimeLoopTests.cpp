#include "doctest/doctest.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#include "engine/runtime/RuntimeLoop.hpp"

namespace {

using Engine::Runtime::FrameContext;
using Engine::Runtime::IRuntimeClient;
using Engine::Runtime::RuntimeControl;
using Engine::Runtime::RuntimeLoop;

enum class Phase {
    ProcessEvents,
    Update,
    Render,
    Never,
};

struct Call {
    Phase phase;
    FrameContext frame;
};

class FakeRuntimeClient final : public IRuntimeClient {
public:
    Phase stopPhase = Phase::Never;
    std::uint64_t stopFrame = 0;
    std::vector<Call> calls;

    RuntimeControl ProcessEvents(const FrameContext& frame) override {
        return Record(Phase::ProcessEvents, frame);
    }

    RuntimeControl Update(const FrameContext& frame) override {
        return Record(Phase::Update, frame);
    }

    RuntimeControl Render(const FrameContext& frame) override {
        return Record(Phase::Render, frame);
    }

private:
    RuntimeControl Record(Phase phase, const FrameContext& frame) {
        calls.push_back(Call{phase, frame});
        return phase == stopPhase && frame.frameIndex == stopFrame
            ? RuntimeControl::Stop
            : RuntimeControl::Continue;
    }
};

} // namespace

TEST_CASE("RuntimeLoop: processes events, update, and render in order") {
    FakeRuntimeClient client;
    client.stopPhase = Phase::Render;
    client.stopFrame = 1;

    RuntimeLoop loop(client);
    loop.Run();

    REQUIRE(client.calls.size() == 6);
    CHECK(client.calls[0].phase == Phase::ProcessEvents);
    CHECK(client.calls[1].phase == Phase::Update);
    CHECK(client.calls[2].phase == Phase::Render);
    CHECK(client.calls[3].phase == Phase::ProcessEvents);
    CHECK(client.calls[4].phase == Phase::Update);
    CHECK(client.calls[5].phase == Phase::Render);
}

TEST_CASE("RuntimeLoop: stop from ProcessEvents skips update and render") {
    FakeRuntimeClient client;
    client.stopPhase = Phase::ProcessEvents;

    RuntimeLoop loop(client);
    loop.Run();

    REQUIRE(client.calls.size() == 1);
    CHECK(client.calls[0].phase == Phase::ProcessEvents);
}

TEST_CASE("RuntimeLoop: stop from Update skips render") {
    FakeRuntimeClient client;
    client.stopPhase = Phase::Update;

    RuntimeLoop loop(client);
    loop.Run();

    REQUIRE(client.calls.size() == 2);
    CHECK(client.calls[0].phase == Phase::ProcessEvents);
    CHECK(client.calls[1].phase == Phase::Update);
}

TEST_CASE("RuntimeLoop: stop from Render exits after render") {
    FakeRuntimeClient client;
    client.stopPhase = Phase::Render;

    RuntimeLoop loop(client);
    loop.Run();

    REQUIRE(client.calls.size() == 3);
    CHECK(client.calls[0].phase == Phase::ProcessEvents);
    CHECK(client.calls[1].phase == Phase::Update);
    CHECK(client.calls[2].phase == Phase::Render);
}

TEST_CASE("RuntimeLoop: frame context starts at zero with zero first delta") {
    FakeRuntimeClient client;
    client.stopPhase = Phase::Render;
    client.stopFrame = 2;

    RuntimeLoop loop(client);
    loop.Run();

    REQUIRE(client.calls.size() == 9);

    for (std::size_t callIndex = 0; callIndex < client.calls.size(); ++callIndex) {
        const auto expectedFrameIndex = static_cast<std::uint64_t>(callIndex / 3);
        CHECK(client.calls[callIndex].frame.frameIndex == expectedFrameIndex);
        CHECK(client.calls[callIndex].frame.deltaSeconds >= 0.0);
    }

    CHECK(client.calls[0].frame.deltaSeconds == 0.0);
    CHECK(client.calls[1].frame.deltaSeconds == 0.0);
    CHECK(client.calls[2].frame.deltaSeconds == 0.0);

    CHECK(client.calls[3].frame.deltaSeconds == client.calls[4].frame.deltaSeconds);
    CHECK(client.calls[4].frame.deltaSeconds == client.calls[5].frame.deltaSeconds);
    CHECK(client.calls[6].frame.deltaSeconds == client.calls[7].frame.deltaSeconds);
    CHECK(client.calls[7].frame.deltaSeconds == client.calls[8].frame.deltaSeconds);
}

namespace {

// Runs live frames from inside ProcessEvents, as the platform does while an OS
// modal loop holds the event pump.
class LiveFrameClient final : public IRuntimeClient {
public:
    RuntimeLoop* loop = nullptr;
    std::uint64_t liveFrameIndex = 1;   // ProcessEvents of this frame runs live frames
    int liveFrames = 0;                 // how many
    std::chrono::milliseconds liveUpdateSleep{0};
    std::chrono::milliseconds regularUpdateSleep{0};
    Phase stopPhase = Phase::Never;
    std::uint64_t stopFrame = 0;
    std::vector<Call> calls;
    std::vector<std::chrono::steady_clock::time_point> entered;  // per call
    std::vector<bool> liveResults;
    bool nestedResult = true;
    bool outsideResult = true;

    RuntimeControl ProcessEvents(const FrameContext& frame) override {
        Record(Phase::ProcessEvents, frame);
        if (frame.frameIndex == liveFrameIndex) {
            for (int i = 0; i < liveFrames; ++i) {
                inLive_ = true;
                liveResults.push_back(loop->RunLiveFrame());
                inLive_ = false;
            }
        }
        return Stop(Phase::ProcessEvents, frame);
    }

    RuntimeControl Update(const FrameContext& frame) override {
        Record(Phase::Update, frame);
        if (inLive_) {
            nestedResult = loop->RunLiveFrame();
            std::this_thread::sleep_for(liveUpdateSleep);
        } else {
            outsideResult = loop->RunLiveFrame();
            std::this_thread::sleep_for(regularUpdateSleep);
        }
        return Stop(Phase::Update, frame);
    }

    RuntimeControl Render(const FrameContext& frame) override {
        Record(Phase::Render, frame);
        return Stop(Phase::Render, frame);
    }

private:
    void Record(const Phase phase, const FrameContext& frame) {
        entered.push_back(std::chrono::steady_clock::now());
        calls.push_back(Call{phase, frame});
    }

    RuntimeControl Stop(const Phase phase, const FrameContext& frame) const {
        // A broken loop that never reaches the stop frame ends here instead of
        // running forever; the call sequence checks then fail.
        if (calls.size() > 100) {
            return RuntimeControl::Stop;
        }
        return phase == stopPhase && frame.frameIndex == stopFrame
            ? RuntimeControl::Stop
            : RuntimeControl::Continue;
    }

    bool inLive_ = false;
};

std::vector<Phase> Phases(const std::vector<Call>& calls) {
    std::vector<Phase> phases;
    for (const Call& call : calls) {
        phases.push_back(call.phase);
    }
    return phases;
}

std::vector<std::uint64_t> Indices(const std::vector<Call>& calls) {
    std::vector<std::uint64_t> indices;
    for (const Call& call : calls) {
        indices.push_back(call.frame.frameIndex);
    }
    return indices;
}

} // namespace

TEST_CASE("RuntimeLoop: live frames run update and render pairs inside ProcessEvents") {
    LiveFrameClient client;
    client.liveFrames = 2;
    client.liveUpdateSleep = std::chrono::milliseconds(5);
    client.stopPhase = Phase::Render;
    client.stopFrame = 3;
    RuntimeLoop loop(client);
    client.loop = &loop;
    loop.Run();

    using P = Phase;
    CHECK(Phases(client.calls) == std::vector<Phase>{
        P::ProcessEvents, P::Update, P::Render,
        P::ProcessEvents, P::Update, P::Render, P::Update, P::Render,  // two live frames
        P::Update, P::Render,                                           // the regular pair
    });
    // One index per Update/Render pair; ProcessEvents carries the next pair's index.
    CHECK(Indices(client.calls) == std::vector<std::uint64_t>{0, 0, 0, 1, 1, 1, 2, 2, 3, 3});
    CHECK(client.liveResults == std::vector<bool>{true, true});

    const auto& calls = client.calls;
    for (const Call& call : calls) {
        CHECK(call.frame.deltaSeconds >= 0.0);
    }
    // Each pair shares one context.
    for (std::size_t update : {1u, 4u, 6u, 8u}) {
        CHECK(calls[update].frame.deltaSeconds == calls[update + 1].frame.deltaSeconds);
    }
    // The first live pair measures from the previous pair's start, so the time
    // ProcessEvents waited is not lost; later pairs include the sleeping live update.
    CHECK(calls[4].frame.deltaSeconds >= calls[3].frame.deltaSeconds);
    CHECK(calls[6].frame.deltaSeconds >= 0.005);
    CHECK(calls[8].frame.deltaSeconds >= 0.005);
    // A pair's delta covers only the time since the previous pair started, and
    // a pair starts after the previous pair's Render was entered and before its
    // own Update. So it is bounded by those entry times and never reaches back
    // to earlier pairs.
    const auto seconds = [&](std::size_t from, std::size_t to) {
        return std::chrono::duration<double>(client.entered[to] - client.entered[from]).count();
    };
    CHECK(calls[8].frame.deltaSeconds <= seconds(5, 8));
    CHECK(calls[6].frame.deltaSeconds <= seconds(2, 6));
}

TEST_CASE("RuntimeLoop: live frames are refused outside ProcessEvents and inside a live frame") {
    LiveFrameClient client;
    client.liveFrames = 1;
    client.stopPhase = Phase::Render;
    client.stopFrame = 2;
    RuntimeLoop loop(client);
    client.loop = &loop;

    CHECK_FALSE(loop.RunLiveFrame());  // before Run
    loop.Run();

    CHECK_FALSE(client.outsideResult);  // from a regular Update
    CHECK_FALSE(client.nestedResult);   // from a live Update
    CHECK(client.liveResults == std::vector<bool>{true});
    CHECK(client.calls.size() == 8);  // PE U R, PE, live U R, regular U R
    CHECK_FALSE(loop.RunLiveFrame());  // after Run
}

TEST_CASE("RuntimeLoop: a stop from a live frame ends the loop when ProcessEvents returns") {
    LiveFrameClient client;
    client.liveFrames = 3;
    client.stopPhase = Phase::Update;
    client.stopFrame = 1;  // the first live frame
    RuntimeLoop loop(client);
    client.loop = &loop;
    loop.Run();

    using P = Phase;
    CHECK(Phases(client.calls) == std::vector<Phase>{
        P::ProcessEvents, P::Update, P::Render,
        P::ProcessEvents, P::Update,  // live Update stops; no Render, no regular pair
    });
    CHECK(client.liveResults == std::vector<bool>{true, false, false});
}

TEST_CASE("RuntimeLoop: without live frames each run starts again at frame zero") {
    FakeRuntimeClient client;
    client.stopPhase = Phase::Render;
    client.stopFrame = 1;
    RuntimeLoop loop(client);
    loop.Run();
    loop.Run();

    REQUIRE(client.calls.size() == 12);
    CHECK(client.calls[6].frame.frameIndex == 0);
    CHECK(client.calls[6].frame.deltaSeconds == 0.0);
}

TEST_CASE("RuntimeLoop: after live frames the next frames measure from the regular pair") {
    LiveFrameClient client;
    client.liveFrames = 2;
    client.regularUpdateSleep = std::chrono::milliseconds(5);
    client.stopPhase = Phase::Render;
    client.stopFrame = 5;
    RuntimeLoop loop(client);
    client.loop = &loop;
    loop.Run();

    // PE0 U0 R0 | PE1 U1 R1 U2 R2 U3 R3 | PE4 U4 R4 | PE5 U5 R5
    REQUIRE(client.calls.size() == 16);
    CHECK(Indices(client.calls) ==
          std::vector<std::uint64_t>{0, 0, 0, 1, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5, 5, 5});
    const auto& calls = client.calls;
    const auto seconds = [&](std::size_t from, std::size_t to) {
        return std::chrono::duration<double>(client.entered[to] - client.entered[from]).count();
    };
    // Frame 4 starts after the regular pair (calls 8, 9), whose Update slept.
    CHECK(calls[10].frame.deltaSeconds >= 0.005);
    CHECK(calls[10].frame.deltaSeconds <= seconds(7, 10));
    // Without live frames a frame's three phases share one context again.
    for (std::size_t first : {10u, 13u}) {
        CHECK(calls[first].frame.deltaSeconds == calls[first + 1].frame.deltaSeconds);
        CHECK(calls[first + 1].frame.deltaSeconds == calls[first + 2].frame.deltaSeconds);
    }
}

TEST_CASE("RuntimeLoop: live frames inside the first ProcessEvents start from a zero delta") {
    LiveFrameClient client;
    client.liveFrameIndex = 0;
    client.liveFrames = 1;
    client.stopPhase = Phase::Render;
    client.stopFrame = 1;
    RuntimeLoop loop(client);
    client.loop = &loop;
    loop.Run();

    using P = Phase;
    CHECK(Phases(client.calls) == std::vector<Phase>{P::ProcessEvents, P::Update, P::Render, P::Update, P::Render});
    CHECK(client.calls[1].frame.frameIndex == 0);
    CHECK(client.calls[1].frame.deltaSeconds == 0.0);
}

TEST_CASE("RuntimeLoop: a stop from a live Render ends the loop when ProcessEvents returns") {
    LiveFrameClient client;
    client.liveFrames = 2;
    client.stopPhase = Phase::Render;
    client.stopFrame = 1;  // the first live frame's Render
    RuntimeLoop loop(client);
    client.loop = &loop;
    loop.Run();

    using P = Phase;
    CHECK(Phases(client.calls) == std::vector<Phase>{
        P::ProcessEvents, P::Update, P::Render, P::ProcessEvents, P::Update, P::Render});
    CHECK(client.liveResults == std::vector<bool>{true, false});
}

TEST_CASE("RuntimeLoop: a run after a live-frame stop starts afresh") {
    LiveFrameClient client;
    client.liveFrames = 1;
    client.stopPhase = Phase::Update;
    client.stopFrame = 1;  // the live Update
    RuntimeLoop loop(client);
    client.loop = &loop;
    loop.Run();
    REQUIRE(client.calls.size() == 5);

    client.calls.clear();
    client.liveFrames = 0;
    client.stopPhase = Phase::Render;
    client.stopFrame = 2;
    loop.Run();
    CHECK(client.calls.size() == 9);
    CHECK(client.calls[0].frame.frameIndex == 0);
}
