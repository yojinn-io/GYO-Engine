#include "doctest/doctest.h"

#include <cstddef>
#include <cstdint>
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
