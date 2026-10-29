// Wake precision record (time-threads-trace, TT-1 L2; os-subsystem os-wake-record,
// D68; record only). Periodic waits on absolute deadlines one 60 Hz period apart,
// for a fixed time per primitive: the engine Waiter,
// std::condition_variable::wait_until and std::this_thread::sleep_until, and how
// late each wake came. Then notify to wake: another thread notifies at 60 Hz and
// the waiting one records how long after the notify it woke, for the Waiter and
// a condition_variable. The window build keeps an Engine SDL window open and
// pumps its events between waits, as a process with a window does. Not a CTest:
// timing on a shared host is never a pass condition.
#include "engine/time/MonotonicClock.hpp"
#include "engine/time/Waiter.hpp"
#ifdef GYO_WAKE_RECORD_WINDOW
#include "engine/platform/sdl/SdlPlatform.hpp"
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
using Engine::Time::Duration;
using Engine::Time::Now;
using Engine::Time::TimePoint;

double Milliseconds(Duration value) { return std::chrono::duration<double, std::milli>(value).count(); }

// One JSON object for a set of delays in milliseconds; extra is appended as is.
std::string Summary(const char* name, const char* measure, std::vector<double> late, const std::string& extra = {}) {
    std::sort(late.begin(), late.end());
    const auto pick = [&](double q) { return late[std::min(late.size() - 1, static_cast<std::size_t>(q * double(late.size() - 1)))]; };
    const auto over = [&](double limit) { return std::count_if(late.begin(), late.end(), [&](double v) { return v > limit; }); };
    std::ostringstream out;
    out << "{\"primitive\":\"" << name << "\",\"measure\":\"" << measure << "\",\"samples\":" << late.size()
        << ",\"p50_ms\":" << pick(.5) << ",\"p99_ms\":" << pick(.99) << ",\"max_ms\":" << late.back()
        << ",\"min_ms\":" << late.front() << ",\"over_1ms\":" << over(1) << ",\"over_4ms\":" << over(4) << extra
        << "}";
    return out.str();
}

std::string Measure(const char* name, double seconds, const std::function<void(TimePoint)>& wait,
                    const std::function<void()>& between) {
    const auto period = std::chrono::nanoseconds(16'666'667);
    std::vector<double> late;
    const auto start = Now();
    auto deadline = start;
    while (Now() - start < std::chrono::duration<double>(seconds)) {
        deadline += period;
        wait(deadline);
        late.push_back(Milliseconds(Now() - deadline));
        between();
        // Never replay missed periods: a late wake starts the next period from now.
        if (deadline + period < Now()) deadline = Now();
    }
    return Summary(name, "deadline", std::move(late));
}

// Notify to wake. The notifier stamps the time base just before it notifies and
// sends the next notification only after the waiting thread took the previous
// one, so a stamp is never overwritten before it is read; a period in which the
// previous one was not taken yet is skipped and counted. The delay includes the
// notify call itself.
std::string MeasureNotify(const char* name, double seconds, const std::function<void()>& wait,
                          const std::function<void()>& notify, const std::function<void()>& between) {
    const auto period = std::chrono::nanoseconds(16'666'667);
    std::atomic<std::int64_t> stamp{0};
    std::atomic<std::uint64_t> sent{0};
    std::atomic<std::uint64_t> taken{0};
    std::atomic<bool> stop{false};
    std::uint64_t skipped = 0;
    std::thread notifier([&] {
        auto next = Now();
        while (!stop.load(std::memory_order_acquire)) {
            next += period;
            std::this_thread::sleep_until(next);
            if (taken.load(std::memory_order_acquire) != sent.load(std::memory_order_relaxed)) {
                ++skipped;
                continue;
            }
            stamp.store(Engine::Time::NowNs(), std::memory_order_relaxed);
            sent.fetch_add(1, std::memory_order_release);
            notify();
        }
        notify();  // releases a waiter still blocked after the last period
    });
    std::vector<double> late;
    const auto start = Now();
    while (Now() - start < std::chrono::duration<double>(seconds)) {
        wait();
        const auto woke = Engine::Time::NowNs();
        const auto count = sent.load(std::memory_order_acquire);
        if (count == taken.load(std::memory_order_relaxed)) continue;  // a wake with nothing new sent
        late.push_back(double(woke - stamp.load(std::memory_order_relaxed)) / 1e6);
        taken.store(count, std::memory_order_release);
        between();
    }
    stop.store(true, std::memory_order_release);
    notifier.join();
    return Summary(name, "notify", std::move(late), ",\"skipped_periods\":" + std::to_string(skipped));
}
} // namespace

int main(int argc, char** argv) {
    double seconds = 30;
    std::string output;
    for (int index = 1; index + 1 < argc; index += 2) {
        const std::string key = argv[index];
        if (key == "--seconds") seconds = std::stod(argv[index + 1]);
        else if (key == "--output") output = argv[index + 1];
        else { std::cerr << "unknown option " << key << '\n'; return 2; }
    }
    std::function<void()> between = [] {};
    std::string mode = "cli";
#ifdef GYO_WAKE_RECORD_WINDOW
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create({"GYO wake record", 640, 360, false});
    if (!platform) { std::cerr << "cannot open the window\n"; return 1; }
    between = [&] { static_cast<void>(platform.value()->PumpEvents()); };
    mode = "window";
    // Let the window finish appearing before measuring.
    for (const auto until = Now() + std::chrono::seconds(1); Now() < until;) {
        between();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
#endif
    auto waiter = Engine::Time::Waiter::Create();
    if (!waiter) { std::cerr << "no wait primitive\n"; return 1; }
    std::mutex mutex;
    std::condition_variable condition;
    std::ostringstream json;
    json << "{\"mode\":\"" << mode << "\",\"seconds_per_primitive\":" << seconds << ",\"period_ns\":16666667"
         << ",\"waiter_backend\":\"" << Engine::Time::ToString(waiter->Backend()) << "\",\"primitives\":[";
    json << Measure("waiter", seconds, [&](TimePoint deadline) { static_cast<void>(waiter->WaitUntil(deadline)); }, between) << ',';
    json << Measure("condition_variable", seconds, [&](TimePoint deadline) {
        std::unique_lock lock(mutex);
        while (Now() < deadline) static_cast<void>(condition.wait_until(lock, deadline));
    }, between) << ',';
    json << Measure("sleep_until", seconds, [](TimePoint deadline) { std::this_thread::sleep_until(deadline); }, between) << ',';
    json << MeasureNotify("waiter", seconds, [&] { static_cast<void>(waiter->Wait()); }, [&] { waiter->Notify(); }, between) << ',';
    bool signalled = false;
    json << MeasureNotify("condition_variable", seconds, [&] {
        std::unique_lock lock(mutex);
        condition.wait(lock, [&] { return signalled; });
        signalled = false;
    }, [&] {
        {
            std::lock_guard lock(mutex);
            signalled = true;
        }
        condition.notify_one();
    }, between);
    json << "]}";
    if (!output.empty()) std::ofstream(output) << json.str() << '\n';
    std::cout << json.str() << '\n';
    return 0;
}
