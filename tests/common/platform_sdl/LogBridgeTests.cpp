// IP-3 ip3e: the SDL log bridge (docs/architecture/platform-sdl.md, "Log bridge";
// judgments declared in plans/input-and-present/ip3-declaration.md).
#include <doctest/doctest.h>

#include "AssertTestSupport.hpp"
#include "engine/base/Log.hpp"
#include "engine/platform/sdl/SdlLogBridge.hpp"

#include <SDL3/SDL.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using Engine::Base::LogLevel;
using Engine::Base::LogMessage;
using Engine::Platform::Sdl::SdlLogBridge;
using Engine::Platform::Sdl::SdlLogBridgeOptions;

namespace {

struct Received final {
    LogLevel level;
    std::string category;
    std::string text;
};

std::mutex receivedMutex;
std::vector<Received> received;
std::atomic<int> engineLines{0};
std::atomic<int> previousLines{0};
std::atomic<bool> nestOnce{false};
// Per logging thread (both outputs run on the thread that logs).
thread_local int threadEngineLines = 0;
thread_local int threadPreviousLines = 0;
int previousSentinel = 0;  // the userdata of SDL's previous output in these tests

void Capture(const LogMessage& message) noexcept {
    engineLines.fetch_add(1);
    ++threadEngineLines;
    try {
        const std::scoped_lock lock(receivedMutex);
        received.push_back({message.level, std::string(message.category), std::string(message.text)});
    } catch (...) {
    }
    // Same-thread re-entry: a line logged to SDL from the Engine handler.
    if (nestOnce.exchange(false)) SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, "nested");
}

void SDLCALL Previous(void*, int, SDL_LogPriority, const char*) {
    previousLines.fetch_add(1);
    ++threadPreviousLines;
}

// Every test starts from a known SDL output, Engine handler and priorities.
struct LogScope final {
    LogScope() {
        received.clear();
        engineLines = 0;
        previousLines = 0;
        nestOnce = false;
        previousHandler = Engine::Base::SetLogHandler(&Capture);
        SDL_SetLogOutputFunction(&Previous, &previousSentinel);
        SDL_SetLogPriorities(SDL_LOG_PRIORITY_TRACE);
    }
    ~LogScope() {
        SDL_ResetLogPriorities();
        SDL_SetLogOutputFunction(SDL_GetDefaultLogOutputFunction(), nullptr);
        Engine::Base::SetLogHandler(previousHandler);
    }
    Engine::Base::LogHandler previousHandler{};
};

} // namespace

TEST_CASE("SdlLogBridge maps SDL priorities and categories to Engine levels") {
    const LogScope scope;
    {
        const SdlLogBridge bridge;
        for (int priority = SDL_LOG_PRIORITY_TRACE; priority <= SDL_LOG_PRIORITY_CRITICAL; ++priority)
            SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, static_cast<SDL_LogPriority>(priority), "p");
        SDL_LogMessage(SDL_LOG_CATEGORY_CUSTOM + 3, SDL_LOG_PRIORITY_INFO, "custom");
        for (int category = SDL_LOG_CATEGORY_APPLICATION; category <= SDL_LOG_CATEGORY_GPU; ++category)
            SDL_LogMessage(category, SDL_LOG_PRIORITY_INFO, "category");
    }
    const std::vector<std::string> names{"sdl.application", "sdl.error", "sdl.assert", "sdl.system", "sdl.audio",
                                         "sdl.video", "sdl.render", "sdl.input", "sdl.test", "sdl.gpu"};
    REQUIRE(received.size() == 7 + 1 + names.size());
    for (std::size_t index = 0; index < names.size(); ++index)
        CHECK_MESSAGE(received[8 + index].category == names[index], "each SDL category has its name");
    received.resize(8);
    const std::vector<LogLevel> expected{LogLevel::Debug, LogLevel::Debug, LogLevel::Debug, LogLevel::Info,
                                         LogLevel::Warning, LogLevel::Error, LogLevel::Error};
    REQUIRE(received.size() == expected.size() + 1);
    for (std::size_t index = 0; index < expected.size(); ++index) {
        CHECK_MESSAGE(received[index].level == expected[index], "each SDL priority maps to its Engine level");
        CHECK(received[index].category == "sdl.application");
    }
    CHECK(received.back().category == "sdl.custom");
    CHECK(received.back().text == "custom");
}

TEST_CASE("SdlLogBridge tees by default and forwards only when asked") {
    const LogScope scope;
    {
        const SdlLogBridge bridge;
        for (int line = 0; line < 5; ++line) SDL_Log("tee");
    }
    CHECK_MESSAGE(engineLines == 5, "the Engine log receives every line");
    CHECK_MESSAGE(previousLines == 5, "a teeing bridge keeps SDL's previous output");
    engineLines = 0;
    previousLines = 0;
    {
        const SdlLogBridge bridge(SdlLogBridgeOptions{.keepSdlOutput = false});
        for (int line = 0; line < 5; ++line) SDL_Log("forward");
    }
    CHECK_MESSAGE(engineLines == 5, "the Engine log receives every line");
    CHECK_MESSAGE(previousLines == 0, "a forwarding bridge leaves SDL's previous output out");
}

TEST_CASE("SdlLogBridge restores SDL's previous output") {
    const LogScope scope;
    { const SdlLogBridge bridge; }
    SDL_LogOutputFunction current{};
    void* data{};
    SDL_GetLogOutputFunction(&current, &data);
    CHECK_MESSAGE((current == &Previous && data == &previousSentinel), "SDL's previous output is back after the bridge");
    SDL_Log("after");
    CHECK(engineLines == 0);
    CHECK(previousLines == 1);
}

TEST_CASE("SdlLogBridge sends a line logged from the Engine handler to SDL's previous output only") {
    const LogScope scope;
    {
        const SdlLogBridge bridge(SdlLogBridgeOptions{.keepSdlOutput = false});
        nestOnce = true;
        SDL_Log("outer");
    }
    CHECK_MESSAGE(engineLines == 1, "a line logged from the Engine handler does not come back to it");
    CHECK_MESSAGE(previousLines == 1, "a line logged from the Engine handler reaches SDL's previous output exactly once");
}

TEST_CASE("SdlLogBridge keeps every line under concurrent logging") {
    const LogScope scope;
    constexpr int Threads = 8;
    constexpr int Rounds = 200;
    std::atomic<bool> stop{false};
    std::atomic<int> started{0};  // threads that have logged at least one line
    std::vector<int> pushedBy(Threads), engineBy(Threads), previousBy(Threads);
    std::vector<std::thread> loggers;
    for (int thread = 0; thread < Threads; ++thread) {
        loggers.emplace_back([&, thread] {
            int pushed = 0;
            while (!stop.load()) {
                SDL_Log("line");
                if (++pushed == 1) started.fetch_add(1);
            }
            pushedBy[thread] = pushed;
            engineBy[thread] = threadEngineLines;
            previousBy[thread] = threadPreviousLines;
        });
    }
    // Start the rounds only once every thread is logging (thread starts can take
    // longer than all the rounds; seen on windows-x64, run 40052105536). A CHECK,
    // not a REQUIRE, so the threads are still stopped and joined on failure.
    const auto waitUntil = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (started.load() < Threads && std::chrono::steady_clock::now() < waitUntil) std::this_thread::yield();
    CHECK_MESSAGE(started.load() == Threads, "every thread logs before the rounds start");
    for (int round = 0; round < Rounds; ++round) {
        const SdlLogBridge bridge(SdlLogBridgeOptions{.keepSdlOutput = false});
        std::this_thread::yield();
    }
    stop = true;
    for (auto& logger : loggers) logger.join();
    for (int thread = 0; thread < Threads; ++thread) {
        CHECK(pushedBy[thread] > 0);
        CHECK_MESSAGE(engineBy[thread] + previousBy[thread] == pushedBy[thread],
                      "every line of a thread reaches either the Engine log (bridge present) or SDL's previous output");
    }
}

TEST_CASE("SdlLogBridge leaves SDL's previous output after bridges made on two threads") {
    const LogScope scope;
    constexpr int Iterations = 2000;
    std::atomic<bool> stop{false};
    std::vector<std::thread> loggers;
    for (int thread = 0; thread < 4; ++thread)
        loggers.emplace_back([&] { while (!stop.load()) SDL_Log("line"); });
    {
        // A bridge made while another one exists fails a GYO_ASSERT (one at a
        // time); the makers retry, so each starts as soon as the other ends.
        const Engine::Test::ScopedAssertionHandler handler;
        auto maker = [] {
            for (int made = 0; made < Iterations;) {
                try {
                    const SdlLogBridge bridge(SdlLogBridgeOptions{.keepSdlOutput = false});
                    ++made;
                } catch (const Engine::Base::AssertionFailure&) {
                    std::this_thread::yield();
                }
            }
        };
        std::thread first(maker);
        std::thread second(maker);
        first.join();
        second.join();
    }
    stop = true;
    for (auto& logger : loggers) logger.join();
    SDL_LogOutputFunction current{};
    void* data{};
    SDL_GetLogOutputFunction(&current, &data);
    CHECK_MESSAGE((current == &Previous && data == &previousSentinel),
                  "after every bridge is gone SDL's previous output is back");
    CHECK_FALSE(Engine::Platform::Sdl::SdlLogBridgeInstalled());
}
