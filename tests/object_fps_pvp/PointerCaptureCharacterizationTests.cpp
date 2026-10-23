// sdl-isolation J1 (v7.1 D168): the in-world pointer capture policy against its golden. The golden records the
// policy's input at its own boundary (every field but the timestamp of the InputEvents Engine's SdlInput produced)
// and its state after every frame;
// it was recorded where the batch 06 oracle (the pre-batch native handling) and the policy agreed on all 15014
// frames (fixtures/pointer_capture_golden.tsv). Engine's conversion from SDL events (focus, insideWindow by the
// size at handling, this window only, no repeats) is guarded by tests/common/input.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "RetroFPS/Pvp/PointerCapture.hpp"

#include <cstddef>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using Engine::Input::InputEvent;
using Engine::Input::InputEventKind;
using fps::pvp::ApplyPointerCaptureEvents;
using fps::pvp::PointerCaptureState;

namespace {

struct GoldenFrame {
    std::string sequence;
    int frame{};
    bool focusedAtStart{};
    bool presetReloadEdge{};
    std::vector<InputEvent> events;
    PointerCaptureState after;
    unsigned releases{};
    unsigned releasesWhileCaptured{};
};

std::vector<std::string> Split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::string part;
    std::istringstream in(text);
    while (std::getline(in, part, separator)) parts.push_back(part);
    return parts;
}

InputEvent ParseEvent(const std::string& token) {
    const auto parts = Split(token, '.');
    InputEvent event;
    event.kind = static_cast<InputEventKind>(std::stoi(parts.at(0)));
    if (event.kind == InputEventKind::KeyPressed || event.kind == InputEventKind::KeyReleased) {
        event.key = static_cast<Engine::Input::Key>(std::stoul(parts.at(1)));
    } else if (event.kind == InputEventKind::MouseButtonPressed || event.kind == InputEventKind::MouseButtonReleased) {
        event.button = static_cast<Engine::Input::MouseButton>(std::stoul(parts.at(1)));
        event.insideWindow = parts.at(2) == "1";
        event.x = std::stof(parts.at(3));
        event.y = std::stof(parts.at(4));
    } else if (event.kind == InputEventKind::WindowMoved) {
        event.windowX = std::stoi(parts.at(1));
        event.windowY = std::stoi(parts.at(2));
    } else if (event.kind == InputEventKind::WindowResized) {
        event.windowWidth = std::stoi(parts.at(1));
        event.windowHeight = std::stoi(parts.at(2));
    }
    return event;
}

std::vector<GoldenFrame> LoadGolden() {
    std::ifstream in(GYO_POINTER_CAPTURE_GOLDEN);
    REQUIRE(in);
    std::vector<GoldenFrame> frames;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line.front() == '#') continue;
        const auto f = Split(line, '\t');
        REQUIRE(f.size() == 13);
        GoldenFrame frame;
        frame.sequence = f[0];
        frame.frame = std::stoi(f[1]);
        frame.focusedAtStart = f[2] == "1";
        frame.presetReloadEdge = f[3] == "1";
        if (f[4] != "-")
            for (const auto& token : Split(f[4], ';')) frame.events.push_back(ParseEvent(token));
        frame.after.inputCaptured = f[5] == "1";
        frame.after.leftButtonDown = f[6] == "1";
        frame.after.pendingShotEdge = f[7] == "1";
        frame.after.pendingReloadEdge = f[8] == "1";
        frame.after.pointerAcquiredThisFrame = f[9] == "1";
        frame.after.windowInteraction = f[10] == "1";
        frame.releases = static_cast<unsigned>(std::stoul(f[11]));
        frame.releasesWhileCaptured = static_cast<unsigned>(std::stoul(f[12]));
        frames.push_back(std::move(frame));
    }
    return frames;
}

std::string Mismatch(const GoldenFrame& expected, const PointerCaptureState& state, unsigned releases,
                     unsigned releasesWhileCaptured) {
    std::string out;
    const auto field = [&](const char* name, bool golden, bool actual) {
        if (golden != actual) out += std::string(name) + " golden=" + (golden ? "1" : "0") + " policy=" + (actual ? "1" : "0") + "; ";
    };
    field("inputCaptured", expected.after.inputCaptured, state.inputCaptured);
    field("leftButtonDown", expected.after.leftButtonDown, state.leftButtonDown);
    field("pendingShotEdge", expected.after.pendingShotEdge, state.pendingShotEdge);
    field("pendingReloadEdge", expected.after.pendingReloadEdge, state.pendingReloadEdge);
    field("pointerAcquiredThisFrame", expected.after.pointerAcquiredThisFrame, state.pointerAcquiredThisFrame);
    field("windowInteraction", expected.after.windowInteraction, state.windowInteraction);
    if (expected.releases != releases)
        out += "releases golden=" + std::to_string(expected.releases) + " policy=" + std::to_string(releases) + "; ";
    if (expected.releasesWhileCaptured != releasesWhileCaptured)
        out += "releasesWhileCaptured golden=" + std::to_string(expected.releasesWhileCaptured) + " policy=" +
            std::to_string(releasesWhileCaptured) + "; ";
    return out;
}

} // namespace

TEST_CASE("Pointer capture: the policy reproduces its golden frame by frame") {
    const auto frames = LoadGolden();
    REQUIRE(frames.size() == 15014);
    struct Coverage { unsigned captures{}, shots{}, releases{}; };
    std::map<std::string, Coverage> coverage;
    std::size_t mismatches = 0;
    std::string first;
    std::string sequence;
    PointerCaptureState state;
    for (const auto& frame : frames) {
        if (frame.sequence != sequence) {
            sequence = frame.sequence;
            state = {};
        }
        // The product clears these per frame before the frame's events (PvpApplication::ProcessEvents).
        state.pointerAcquiredThisFrame = state.windowInteraction = false;
        state.pendingShotEdge = false;
        if (frame.presetReloadEdge) state.pendingReloadEdge = true;
        const auto result = ApplyPointerCaptureEvents(state, frame.events, frame.focusedAtStart);
        const auto mismatch = Mismatch(frame, state, result.releases, result.releasesWhileCaptured);
        if (!mismatch.empty() && mismatches++ == 0)
            first = frame.sequence + " frame " + std::to_string(frame.frame) + ": " + mismatch;
        auto& counts = coverage[frame.sequence];
        counts.captures += state.pointerAcquiredThisFrame;
        counts.shots += state.pendingShotEdge;
        counts.releases += result.releases > 0;
    }
    INFO("first mismatch: " << first);
    CHECK_MESSAGE(mismatches == 0, "the policy differs from its golden");
    // The seeded sequences exercise every outcome.
    for (const auto& [name, counts] : coverage) {
        if (name == "hand") continue;
        INFO(name);
        CHECK(counts.captures > 50);
        CHECK(counts.shots > 50);
        CHECK(counts.releases > 50);
    }
}

TEST_CASE("Pointer capture: only a release of a captured pointer is reported as one") {
    // The product logs "releasing pointer" only for a captured pointer, as before.
    PointerCaptureState state;
    const InputEvent click{.kind = InputEventKind::MouseButtonPressed, .button = Engine::Input::MouseButton::Left,
                           .x = 1, .y = 1, .insideWindow = true};
    const InputEvent moved{.kind = InputEventKind::WindowMoved};

    auto result = ApplyPointerCaptureEvents(state, std::vector{click}, true);
    CHECK(state.inputCaptured);
    CHECK(result.releases == 0);

    state.pointerAcquiredThisFrame = state.windowInteraction = false;
    result = ApplyPointerCaptureEvents(state, std::vector{moved, moved}, true);
    CHECK_FALSE(state.inputCaptured);
    CHECK(result.releases == 2);
    CHECK(result.releasesWhileCaptured == 1);
}
