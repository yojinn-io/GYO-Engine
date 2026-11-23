// The listener described in CaseTiming.hpp. This directory may branch on the
// OS: the process counters and the load average are read where the platform
// has them, and written as n/a elsewhere.
#include "CaseTiming.hpp"

// gyo_os_wake_fallback_tests defines this for its whole target so that
// WakeSignalTests.cpp holds main; this file must not hold a second one.
#undef DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>

#if defined(_WIN32)
#include <process.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

#ifndef GYO_OS_CASE_TIMING_DIR
#define GYO_OS_CASE_TIMING_DIR "case-timing"
#endif

namespace GYO::Test::CaseTiming {
namespace {

using Clock = std::chrono::steady_clock;

// A run that passed in less than this has nothing to keep: the slowest case
// takes about 0.15 s on an idle machine. Only the file's removal depends on it.
constexpr std::chrono::seconds KeepAfter{5};

struct Sample {
    Clock::time_point wall{};
    bool hasProcess{false};
    double cpuSeconds{0.0};
    long voluntarySwitches{0};
    long involuntarySwitches{0};
    bool hasLoad{false};
    double load1{0.0};
};

Sample Take() {
    Sample sample;
    sample.wall = Clock::now();
#if !defined(_WIN32)
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        sample.hasProcess = true;
        sample.cpuSeconds = static_cast<double>(usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) +
                            static_cast<double>(usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1e6;
        sample.voluntarySwitches = usage.ru_nvcsw;
        sample.involuntarySwitches = usage.ru_nivcsw;
    }
    double load[1]{};
    if (getloadavg(load, 1) == 1) {
        sample.hasLoad = true;
        sample.load1 = load[0];
    }
#endif
    return sample;
}

double Seconds(const Clock::duration duration) {
    return std::chrono::duration<double>(duration).count();
}

class Log final {
public:
    void Open(const char* binaryName) {
        const std::lock_guard lock(mutex_);
        runStart_ = Clock::now();
        const char* fromEnvironment = std::getenv("GYO_OS_CASE_TIMING_DIR");
        const std::filesystem::path directory =
            (fromEnvironment != nullptr && fromEnvironment[0] != '\0') ? fromEnvironment : GYO_OS_CASE_TIMING_DIR;
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        const std::time_t now = std::time(nullptr);
        std::tm local{};
#if defined(_WIN32)
        localtime_s(&local, &now);
        const int pid = _getpid();
#else
        localtime_r(&now, &local);
        const int pid = static_cast<int>(getpid());
#endif
        char stamp[32]{};
        std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &local);
        const std::string program = std::filesystem::path(binaryName != nullptr ? binaryName : "tests").stem().string();
        path_ = directory / (program + "-" + stamp + "-" + std::to_string(pid) + ".log");
        file_ = std::fopen(path_.string().c_str(), "w");
        if (file_ == nullptr) std::fprintf(stderr, "[case-timing] cannot write %s\n", path_.string().c_str());
    }

    // Removes the file when the run passed quickly, and says where it is otherwise.
    void Close(const bool failed) {
        const std::lock_guard lock(mutex_);
        if (file_ == nullptr) return;
        std::fclose(file_);
        file_ = nullptr;
        if (!failed && Clock::now() - runStart_ < KeepAfter) {
            std::error_code error;
            std::filesystem::remove(path_, error);
        } else {
            std::fprintf(stderr, "[case-timing] kept %s\n", path_.string().c_str());
        }
    }

    void Write(const char* format, std::va_list arguments) {
        char body[1024]{};
        std::vsnprintf(body, sizeof body, format, arguments);
        const std::lock_guard lock(mutex_);
        char line[1200]{};
        std::snprintf(line, sizeof line, "[case-timing] +%.3fs %s\n", Seconds(Clock::now() - runStart_), body);
        std::fputs(line, stderr);
        std::fflush(stderr);
        if (file_ != nullptr) {
            std::fputs(line, file_);
            std::fflush(file_);
        }
    }

private:
    std::mutex mutex_;
    Clock::time_point runStart_{Clock::now()};
    std::filesystem::path path_;
    std::FILE* file_{nullptr};
};

Log& TheLog() {
    static Log log;
    return log;
}

void Line(const char* format, ...) {
    std::va_list arguments;
    va_start(arguments, format);
    TheLog().Write(format, arguments);
    va_end(arguments);
}

// "cpu=0.012s vcsw=3 ivcsw=1 load=2.51" for the span from `from` to `to`,
// with n/a where the platform gives no counter.
std::string Span(const Sample& from, const Sample& to) {
    char text[160]{};
    const double wall = Seconds(to.wall - from.wall);
    int used = 0;
    if (from.hasProcess && to.hasProcess) {
        used = std::snprintf(text, sizeof text, "wall=%.3fs cpu=%.3fs vcsw=%ld ivcsw=%ld", wall,
            to.cpuSeconds - from.cpuSeconds, to.voluntarySwitches - from.voluntarySwitches,
            to.involuntarySwitches - from.involuntarySwitches);
    } else {
        used = std::snprintf(text, sizeof text, "wall=%.3fs cpu=n/a", wall);
    }
    if (used > 0 && static_cast<std::size_t>(used) < sizeof text) {
        if (to.hasLoad) std::snprintf(text + used, sizeof text - static_cast<std::size_t>(used), " load=%.2f", to.load1);
        else std::snprintf(text + used, sizeof text - static_cast<std::size_t>(used), " load=n/a");
    }
    return text;
}

std::string Load(const Sample& sample) {
    char text[32]{};
    if (sample.hasLoad) std::snprintf(text, sizeof text, "load=%.2f", sample.load1);
    else std::snprintf(text, sizeof text, "load=n/a");
    return text;
}

struct Listener final : doctest::IReporter {
    explicit Listener(const doctest::ContextOptions& options) : options_(options) {}

    void report_query(const doctest::QueryData&) override {}

    void test_run_start() override {
        TheLog().Open(options_.binary_name.c_str());
        run_ = Take();
        Line("run start %s", Load(run_).c_str());
    }

    void test_run_end(const doctest::TestRunStats& stats) override {
        const bool failed = stats.numTestCasesFailed > 0 || stats.numAssertsFailed > 0;
        Line("run end failed_cases=%u %s", stats.numTestCasesFailed, Span(run_, Take()).c_str());
        TheLog().Close(failed);
    }

    void test_case_start(const doctest::TestCaseData& data) override {
        name_ = data.m_name;
        case_ = Take();
        Line("case start \"%s\" %s", name_.c_str(), Load(case_).c_str());
    }

    void test_case_reenter(const doctest::TestCaseData&) override {
        Line("case reenter \"%s\"", name_.c_str());
    }

    void test_case_end(const doctest::CurrentTestCaseStats& stats) override {
        Line("case end \"%s\" failed=%s %s", name_.c_str(), stats.testCaseSuccess ? "no" : "yes",
            Span(case_, Take()).c_str());
    }

    void test_case_exception(const doctest::TestCaseException& exception) override {
        Line("case exception \"%s\" crash=%s", name_.c_str(), exception.is_crash ? "yes" : "no");
    }

    void subcase_start(const doctest::SubcaseSignature& signature) override {
        Line("subcase start \"%s\"", signature.m_name.c_str());
    }

    void subcase_end() override {}
    void log_assert(const doctest::AssertData&) override {}
    void log_message(const doctest::MessageData&) override {}
    void test_case_skipped(const doctest::TestCaseData&) override {}

private:
    const doctest::ContextOptions& options_;
    std::string name_;
    Sample run_{};
    Sample case_{};
};

} // namespace

void Note(const char* format, ...) {
    std::va_list arguments;
    va_start(arguments, format);
    TheLog().Write(format, arguments);
    va_end(arguments);
}

} // namespace GYO::Test::CaseTiming

REGISTER_LISTENER("gyo_os_case_timing", 1, GYO::Test::CaseTiming::Listener);
