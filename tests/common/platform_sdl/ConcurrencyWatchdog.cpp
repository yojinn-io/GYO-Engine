#include "ConcurrencyWatchdog.hpp"

#include <doctest/doctest.h>

#include <SDL3/SDL.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

namespace Engine::Test::Watchdog {

std::atomic<long long> firstCount{0};
std::atomic<long long> secondCount{0};

namespace {

using Clock = std::chrono::steady_clock;
constexpr auto ReportEvery = std::chrono::seconds(10);

std::atomic<const char*> markPhase{nullptr};
std::atomic<int> markRound{-1};
std::atomic<Clock::rep> markAt{0};
std::atomic<const char*> firstName{nullptr};
std::atomic<const char*> secondName{nullptr};

// The case being run, guarded by `mutex`.
std::mutex mutex;
std::condition_variable wake;
const char* caseName = nullptr;
Clock::time_point caseStart{};
int reports = 0;
bool stopping = false;

// One write and a flush per line: the output must survive the test being killed.
template <typename... Arguments>
void WriteLine(const char* format, Arguments... arguments) noexcept {
    char line[512];
    std::snprintf(line, sizeof line, format, arguments...);
    std::fputs(line, stderr);
    std::fflush(stderr);
}

const char* OrNone(const char* text) noexcept { return text ? text : "(none)"; }

void Report(const char* name, const Clock::duration running) noexcept {
    const auto now = Clock::now().time_since_epoch().count();
    const double sinceMark =
        markAt.load() == 0 ? -1.0
                           : std::chrono::duration<double>(Clock::duration(now - markAt.load())).count();
    WriteLine("[watchdog] \"%s\" running %lld s: last mark \"%s\" round %d, %.1f s ago; %s %lld, %s %lld\n", name,
              static_cast<long long>(std::chrono::duration_cast<std::chrono::seconds>(running).count()),
              OrNone(markPhase.load()), markRound.load(), sinceMark, OrNone(firstName.load()), firstCount.load(),
              OrNone(secondName.load()), secondCount.load());
    // Written on its own line after the report: if SDL's queue lock is what the
    // test is stuck on, this read blocks too and the line never appears.
    const int queued = SDL_PeepEvents(nullptr, 0, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
    WriteLine("[watchdog] SDL event queue: %d\n", queued);
}

void Run() {
    std::unique_lock lock(mutex);
    while (!stopping) {
        wake.wait_for(lock, std::chrono::milliseconds(250));
        if (!caseName) continue;
        const auto running = Clock::now() - caseStart;
        if (running < ReportEvery * (reports + 1)) continue;
        ++reports;
        const char* name = caseName;
        lock.unlock();
        Report(name, running);
        lock.lock();
    }
}

struct Listener final : doctest::IReporter {
    explicit Listener(const doctest::ContextOptions&) {}
    ~Listener() override { Stop(); }

    void report_query(const doctest::QueryData&) override {}
    void test_run_start() override {
        if (!thread.joinable()) thread = std::thread(&Run);
    }
    void test_run_end(const doctest::TestRunStats&) override { Stop(); }
    void test_case_start(const doctest::TestCaseData& data) override {
        markPhase = nullptr;
        markRound = -1;
        markAt = 0;
        firstName = nullptr;
        secondName = nullptr;
        firstCount = 0;
        secondCount = 0;
        {
            const std::scoped_lock guard(mutex);
            caseName = data.m_name;
            caseStart = Clock::now();
            reports = 0;
        }
        WriteLine("[watchdog] start \"%s\"\n", data.m_name);
    }
    void test_case_reenter(const doctest::TestCaseData&) override {}
    void test_case_end(const doctest::CurrentTestCaseStats& stats) override {
        const char* name = nullptr;
        {
            const std::scoped_lock guard(mutex);
            name = caseName;
            caseName = nullptr;
        }
        WriteLine("[watchdog] end \"%s\" %.2f s %s\n", OrNone(name), stats.seconds,
                  stats.testCaseSuccess ? "passed" : "failed");
    }
    void test_case_exception(const doctest::TestCaseException&) override {}
    void subcase_start(const doctest::SubcaseSignature&) override {}
    void subcase_end() override {}
    void log_assert(const doctest::AssertData&) override {}
    void log_message(const doctest::MessageData&) override {}
    void test_case_skipped(const doctest::TestCaseData&) override {}

    void Stop() {
        if (!thread.joinable()) return;
        {
            const std::scoped_lock guard(mutex);
            stopping = true;
        }
        wake.notify_all();
        thread.join();
    }

    std::thread thread;
};

} // namespace

void Mark(const char* phase, const int round) noexcept {
    markPhase.store(phase, std::memory_order_relaxed);
    markRound.store(round, std::memory_order_relaxed);
    markAt.store(Clock::now().time_since_epoch().count(), std::memory_order_relaxed);
}

void NameCounters(const char* first, const char* second) noexcept {
    firstName = first;
    secondName = second;
}

} // namespace Engine::Test::Watchdog

REGISTER_LISTENER("concurrency_watchdog", 1, Engine::Test::Watchdog::Listener);
