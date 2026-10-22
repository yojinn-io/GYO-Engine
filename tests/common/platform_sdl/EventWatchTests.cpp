// IP-4 ip4b: event watches and capture files (ip4-declaration.md section 2).
#include <doctest/doctest.h>

#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/platform/sdl/SdlPlatformNative.hpp"
#include "engine/time/Waiter.hpp"

#include <SDL3/SDL.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

using Engine::Platform::Sdl::ClockPair;
using Engine::Platform::Sdl::EventWatch;
using Engine::Platform::Sdl::ObservedEvent;
using Engine::Platform::Sdl::ObservedEventKind;
using Engine::Platform::Sdl::SdlPlatform;
using Engine::Time::TimePoint;
using namespace std::chrono_literals;

namespace {

std::unique_ptr<SdlPlatform> MakeWatchPlatform() {
    auto platform = SdlPlatform::Create();
    REQUIRE(platform);
    auto made = std::move(platform.value());
    static_cast<void>(made->PumpEvents());  // the window's own start-up events
    return made;
}

struct Seen final {
    std::vector<ObservedEvent> events;
};

void Record(void* context, const ObservedEvent& event) noexcept {
    try {
        static_cast<Seen*>(context)->events.push_back(event);
    } catch (...) {
    }
}

SDL_WindowID WindowOf(const SdlPlatform& platform) {
    return SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(platform));
}

void PushMotion(const SDL_WindowID window, const std::uint64_t sdlNs = 0, const std::uint32_t source = 0) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = window;
    event.motion.which = source;
    event.motion.xrel = 1.0F;
    event.common.timestamp = sdlNs;
    REQUIRE(SDL_PushEvent(&event));
}

std::chrono::nanoseconds Abs(const std::chrono::nanoseconds value) {
    return value < 0ns ? -value : value;
}

// Pushes a motion queued 10 ms in the past and checks its time against the
// test's own reads (the window around the push, ip3c's tolerance tau).
void CheckPastEventTime(SdlPlatform& platform, Seen& seen) {
    seen.events.clear();
    const SDL_WindowID window = WindowOf(platform);
    const ClockPair atPush = Engine::Platform::Sdl::SampleClockPair();
    const std::uint64_t queued = atPush.sdlNs - 10'000'000ULL;
    PushMotion(window, queued);
    const std::uint64_t sdlAfter = SDL_GetTicksNS();
    const TimePoint after = Engine::Time::Now();
    REQUIRE(seen.events.size() == 1);
    const auto watched = Engine::Platform::Sdl::LastWatchClockPairForTesting();
    REQUIRE(watched);
    const auto engineSpan = std::chrono::duration_cast<std::chrono::nanoseconds>(watched->engine - atPush.engine);
    const std::chrono::nanoseconds sdlSpan{static_cast<std::int64_t>(watched->sdlNs - atPush.sdlNs)};
    const auto tau = Abs(engineSpan - sdlSpan) + atPush.bracket + watched->bracket + 2ns;
    // The watch samples its pair between the test's reads (no fixed offset).
    CHECK_MESSAGE((watched->sdlNs >= atPush.sdlNs && watched->sdlNs <= sdlAfter),
                  "a watch samples SDL's clock while it runs");
    CHECK_MESSAGE((watched->engine >= atPush.engine && watched->engine <= after),
                  "a watch samples the Engine clock while it runs");
    const TimePoint stamped = seen.events[0].timestamp;
    CHECK_MESSAGE(stamped >= atPush.engine - 10ms - tau, "a watched event's time is not earlier than its queue time");
    CHECK_MESSAGE(stamped <= after - 10ms + tau, "a watched event's time is not later than its queue time");
    CHECK_MESSAGE((seen.events[0].observedAt >= atPush.engine && seen.events[0].observedAt <= after),
                  "a watch observes the event while the push runs");
    // Exact: the watch converts with the pair it sampled, observing at that
    // pair's second Engine read (as ip3c's "Each pump converts with its own clock pair").
    CHECK_MESSAGE(seen.events[0].observedAt == watched->engine + (watched->bracket - watched->bracket / 2),
                  "a watch observes at its pair's second Engine read");
    CHECK_MESSAGE(stamped == Engine::Platform::Sdl::ConvertSdlTimestamp(queued, *watched, seen.events[0].observedAt),
                  "a watch converts with the pair it sampled");
    CHECK(stamped <= seen.events[0].observedAt);
}

} // namespace

TEST_CASE("An event watch stamps each event with its queue time") {
    auto platform = MakeWatchPlatform();
    Seen seen;
    const EventWatch watch = platform->AddEventWatch(&Record, &seen);
    auto waiter = Engine::Time::Waiter::Create();
    REQUIRE(waiter);
    CheckPastEventTime(*platform, seen);
    const auto first = Engine::Platform::Sdl::LastWatchClockPairForTesting();
    // A second event later: each callback samples its own pair.
    static_cast<void>(waiter.value().WaitUntil(Engine::Time::Now() + 20ms));
    CheckPastEventTime(*platform, seen);
    const auto second = Engine::Platform::Sdl::LastWatchClockPairForTesting();
    REQUIRE(first);
    REQUIRE(second);
    CHECK_MESSAGE((second->sdlNs > first->sdlNs && second->engine > first->engine),
                  "each watch callback samples a new clock pair");
}

TEST_CASE("An event watch gives a future event its observation time") {
    auto platform = MakeWatchPlatform();
    Seen seen;
    const EventWatch watch = platform->AddEventWatch(&Record, &seen);
    PushMotion(WindowOf(*platform), SDL_GetTicksNS() + 1'000'000'000ULL);
    REQUIRE(seen.events.size() == 1);
    CHECK_MESSAGE(seen.events[0].timestamp == seen.events[0].observedAt,
                  "a watched event's time is never later than its observation");
}

TEST_CASE("An event watch marks injected events only") {
    auto platform = MakeWatchPlatform();
    Seen seen;
    const EventWatch watch = platform->AddEventWatch(&Record, &seen);
    SDL_Event injected{};
    injected.type = SDL_EVENT_MOUSE_MOTION;
    injected.motion.windowID = WindowOf(*platform);
    REQUIRE(Engine::Platform::Sdl::PushInjectedEvent(*platform, injected));
    // The same event object pushed again, not injected: the mark ends with the push.
    REQUIRE(SDL_PushEvent(&injected));
    REQUIRE(platform->SetWindowSize(500, 300));  // the dummy driver queues RESIZED at once
    REQUIRE(seen.events.size() >= 3);
    CHECK_MESSAGE(seen.events[0].injected, "an injected event is marked injected");
    CHECK_MESSAGE(!seen.events[1].injected, "an event pushed after the injected push is not marked injected");
    bool resizedSeen = false;
    for (std::size_t index = 2; index < seen.events.size(); ++index) {
        if (seen.events[index].kind != ObservedEventKind::WindowResized) continue;
        resizedSeen = true;
        CHECK_MESSAGE(!seen.events[index].injected, "an event of a real window operation is not marked injected");
    }
    CHECK(resizedSeen);
}

TEST_CASE("An event watch marks injection per thread") {
    auto platform = MakeWatchPlatform();
    struct Shared final {
        std::atomic<int> otherInjected{0};
        std::atomic<int> otherSeen{0};
    } shared;
    const EventWatch watch = platform->AddEventWatch(
        [](void* context, const ObservedEvent& event) noexcept {
            auto* counts = static_cast<Shared*>(context);
            if (event.sourceId != 7) return;
            counts->otherSeen.fetch_add(1);
            if (event.injected) counts->otherInjected.fetch_add(1);
        },
        &shared);
    const SDL_WindowID window = WindowOf(*platform);
    std::atomic<bool> stop{false};
    std::thread other([&] {
        while (!stop.load()) {
            SDL_Event event{};
            event.type = SDL_EVENT_MOUSE_MOTION;
            event.motion.windowID = window;
            event.motion.which = 7;
            SDL_PushEvent(&event);
        }
    });
    // Start only once the other thread is pushing.
    const auto waitUntil = std::chrono::steady_clock::now() + 5s;
    while (shared.otherSeen.load() == 0 && std::chrono::steady_clock::now() < waitUntil) std::this_thread::yield();
    for (int round = 0; round < 1000; ++round) {
        SDL_Event event{};
        event.type = SDL_EVENT_MOUSE_MOTION;
        event.motion.windowID = window;
        static_cast<void>(Engine::Platform::Sdl::PushInjectedEvent(*platform, event));
        if (round % 50 == 0) static_cast<void>(platform->PumpEvents());
    }
    stop = true;
    other.join();
    static_cast<void>(platform->PumpEvents());
    CHECK(shared.otherSeen > 0);
    CHECK_MESSAGE(shared.otherInjected == 0, "another thread's event is never marked injected");
}

TEST_CASE("An event watch hands out only its window's observed kinds") {
    auto platform = MakeWatchPlatform();
    Seen seen;
    const EventWatch watch = platform->AddEventWatch(&Record, &seen);
    const SDL_WindowID window = WindowOf(*platform);
    PushMotion(window + 1);
    CHECK_MESSAGE(seen.events.empty(), "another window's event is not handed out");
    SDL_Event key{};
    key.type = SDL_EVENT_KEY_DOWN;
    key.key.windowID = window;
    REQUIRE(SDL_PushEvent(&key));
    SDL_Event user{};
    user.type = SDL_EVENT_USER;
    REQUIRE(SDL_PushEvent(&user));
    CHECK_MESSAGE(seen.events.empty(), "kinds that are not observed are not handed out");
    SDL_Event moved{};
    moved.type = SDL_EVENT_WINDOW_MOVED;
    moved.window.windowID = window;
    REQUIRE(SDL_PushEvent(&moved));
    REQUIRE(seen.events.size() == 1);
    CHECK(seen.events[0].kind == ObservedEventKind::WindowMoved);
    static_cast<void>(platform->PumpEvents());
}

TEST_CASE("Destroying an event watch stops its callbacks") {
    auto platform = MakeWatchPlatform();
    Seen seen;
    {
        const EventWatch watch = platform->AddEventWatch(&Record, &seen);
        PushMotion(WindowOf(*platform));
        REQUIRE(seen.events.size() == 1);
    }
    PushMotion(WindowOf(*platform));
    CHECK_MESSAGE(seen.events.size() == 1, "a destroyed event watch receives nothing");
    static_cast<void>(platform->PumpEvents());
}

TEST_CASE("A moved event watch keeps observing") {
    auto platform = MakeWatchPlatform();
    std::vector<Seen> seen(8);
    std::vector<EventWatch> watches;
    for (auto& counts : seen) watches.push_back(platform->AddEventWatch(&Record, &counts));  // reallocates
    PushMotion(WindowOf(*platform));
    for (const auto& counts : seen) CHECK(counts.events.size() == 1);
    EventWatch moved = std::move(watches[0]);
    CHECK_MESSAGE(!watches[0].Active(), "a moved-from watch is inactive");
    watches.clear();  // destroys the moved-from watch and the others
    PushMotion(WindowOf(*platform));
    CHECK_MESSAGE(seen[0].events.size() == 2, "a moved watch keeps observing after its moved-from object is gone");
    CHECK(seen[1].events.size() == 1);
    moved = EventWatch();
    PushMotion(WindowOf(*platform));
    CHECK(seen[0].events.size() == 2);
    static_cast<void>(platform->PumpEvents());
}

TEST_CASE("Destroying an event watch waits for a callback on another thread") {
    auto platform = MakeWatchPlatform();
    struct Flight final {
        std::atomic<bool> inFlight{false};
    } flight;
    const SDL_WindowID window = WindowOf(*platform);
    std::atomic<bool> stop{false};
    std::thread pusher([&] {
        while (!stop.load()) {
            SDL_Event event{};
            event.type = SDL_EVENT_MOUSE_MOTION;
            event.motion.windowID = window;
            SDL_PushEvent(&event);
        }
    });
    bool finishedInFlight = false;
    int overlappedRounds = 0;  // destruction started while a callback ran
    for (int round = 0; round < 200; ++round) {
        {
            const EventWatch watch = platform->AddEventWatch(
                [](void* context, const ObservedEvent&) noexcept {
                    auto* state = static_cast<Flight*>(context);
                    state->inFlight = true;
                    const auto until = std::chrono::steady_clock::now() + 100us;
                    while (std::chrono::steady_clock::now() < until) {
                    }
                    state->inFlight = false;
                },
                &flight);
            std::this_thread::yield();
            if (flight.inFlight.load()) ++overlappedRounds;
        }
        if (flight.inFlight.load()) finishedInFlight = true;
        if (round % 20 == 0) static_cast<void>(platform->PumpEvents());
    }
    stop = true;
    pusher.join();
    static_cast<void>(platform->PumpEvents());
    CHECK_MESSAGE(!finishedInFlight, "no callback runs after its watch's destruction returned");
    CHECK_MESSAGE(overlappedRounds > 0, "destruction overlapped a running callback at least once");
}

TEST_CASE("An event watch may outlive its platform") {
    Seen seen;
    EventWatch watch;
    SDL_WindowID window = 0;
    {
        auto platform = MakeWatchPlatform();
        window = WindowOf(*platform);
        watch = platform->AddEventWatch(&Record, &seen);
    }
    const std::size_t before = seen.events.size();
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = window;
    static_cast<void>(SDL_PushEvent(&event));
    CHECK_MESSAGE(seen.events.size() == before, "a watch receives nothing after its platform is gone");
    watch = EventWatch();
    CHECK(!watch.Active());
}

TEST_CASE("An event watch outlives its platform while SDL stays up") {
    // A second platform keeps SDL's event system alive, so the watch stays
    // registered after the first platform goes. SDL never reuses a window ID,
    // so only other windows' events can still arrive, and the filter drops them.
    auto keeper = MakeWatchPlatform();
    Seen seen;
    EventWatch watch;
    {
        auto platform = MakeWatchPlatform();
        watch = platform->AddEventWatch(&Record, &seen);
    }
    const std::size_t before = seen.events.size();
    PushMotion(WindowOf(*keeper));
    CHECK_MESSAGE(seen.events.size() == before, "a watch receives no other window's events after its platform is gone");
    const int registered = Engine::Platform::Sdl::RegisteredEventWatchesForTesting();
    watch = EventWatch();
    CHECK(!watch.Active());
    CHECK_MESSAGE(Engine::Platform::Sdl::RegisteredEventWatchesForTesting() == registered - 1,
                  "destroying the watch removes it from SDL");
}

TEST_CASE("Move assignment releases the watch it replaces") {
    auto platform = MakeWatchPlatform();
    Seen first;
    Seen second;
    const int before = Engine::Platform::Sdl::RegisteredEventWatchesForTesting();
    EventWatch a = platform->AddEventWatch(&Record, &first);
    EventWatch b = platform->AddEventWatch(&Record, &second);
    REQUIRE(Engine::Platform::Sdl::RegisteredEventWatchesForTesting() == before + 2);
    a = std::move(b);
    CHECK_MESSAGE(Engine::Platform::Sdl::RegisteredEventWatchesForTesting() == before + 1,
                  "move assignment removes the watch it replaces from SDL");
    a = EventWatch();
    CHECK(Engine::Platform::Sdl::RegisteredEventWatchesForTesting() == before);
}

TEST_CASE("An event watch reports each kind with its fields") {
    auto platform = MakeWatchPlatform();
    Seen seen;
    const EventWatch watch = platform->AddEventWatch(&Record, &seen);
    const SDL_WindowID window = WindowOf(*platform);
    SDL_Event down{};
    down.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    down.button.windowID = window;
    down.button.which = 5;
    REQUIRE(SDL_PushEvent(&down));
    SDL_Event up = down;
    up.type = SDL_EVENT_MOUSE_BUTTON_UP;
    REQUIRE(SDL_PushEvent(&up));
    SDL_Event motion{};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = window;
    motion.motion.xrel = 2.0F;
    motion.motion.yrel = 9.0F;
    REQUIRE(SDL_PushEvent(&motion));
    SDL_Event gained{};
    gained.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
    gained.window.windowID = window;
    REQUIRE(SDL_PushEvent(&gained));
    SDL_Event lost = gained;
    lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    REQUIRE(SDL_PushEvent(&lost));
    REQUIRE(seen.events.size() == 5);
    CHECK_MESSAGE((seen.events[0].kind == ObservedEventKind::MouseButtonPressed && seen.events[0].sourceId == 5),
                  "a button press is reported with its source");
    CHECK_MESSAGE((seen.events[1].kind == ObservedEventKind::MouseButtonReleased && seen.events[1].sourceId == 5),
                  "a button release is reported with its source");
    CHECK_MESSAGE((seen.events[2].kind == ObservedEventKind::MouseMotion && seen.events[2].deltaX == 2.0F &&
                   seen.events[2].deltaY == 9.0F),
                  "a motion is reported with its x and y");
    CHECK_MESSAGE(seen.events[3].kind == ObservedEventKind::WindowFocusGained, "a focus change keeps its direction");
    CHECK_MESSAGE(seen.events[4].kind == ObservedEventKind::WindowFocusLost, "a focus change keeps its direction");
    static_cast<void>(platform->PumpEvents());
}

TEST_CASE("Only the injected event itself is marked injected") {
    auto platform = MakeWatchPlatform();
    Seen seen;
    const EventWatch watch = platform->AddEventWatch(&Record, &seen);
    const SDL_WindowID window = WindowOf(*platform);
    // A live frame runs inside the injected push and causes a real RESIZED.
    platform->SetLiveFrameHandler([&] {
        static_cast<void>(SDL_SetWindowSize(Engine::Platform::Sdl::NativeWindow(*platform), 410, 270));
    });
    SDL_Event trigger{};
    trigger.type = SDL_EVENT_USER;
    REQUIRE(SDL_PushEvent(&trigger));
    bool pushed = false;
    static_cast<void>(platform->PumpEvents([&](const Engine::Platform::Sdl::PlatformEvent& view) {
        if (Engine::Platform::Sdl::NativeEvent(view).type != SDL_EVENT_USER || pushed) return;
        pushed = true;
        SDL_Event exposed{};
        exposed.type = SDL_EVENT_WINDOW_EXPOSED;
        exposed.window.windowID = window;
        exposed.window.data1 = 1;
        static_cast<void>(Engine::Platform::Sdl::PushInjectedEvent(*platform, exposed));
    }));
    platform->SetLiveFrameHandler({});
    bool resizedSeen = false;
    for (const auto& event : seen.events) {
        if (event.kind == ObservedEventKind::WindowExposed) CHECK(event.injected);
        if (event.kind != ObservedEventKind::WindowResized) continue;
        resizedSeen = true;
        CHECK_MESSAGE(!event.injected, "an event queued during an injected push is not marked injected");
    }
    CHECK(resizedSeen);
}

