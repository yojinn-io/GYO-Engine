// Judgement 7 of the trace declaration (docs/architecture/target/v2/trace-declaration.zh-Hant.md, section 10;
// question 4, approved: measured only, no threshold). Run by hand on one host, Release; not a ctest.
//   (a) one TraceSpan (Begin and end) with no receiver installed;
//   (b) the same with Trace installed and a Logger whose sink takes no span record (only the stack is kept);
//   (c) the same with the span record taken into a JSON lines file sink's queue (its output discards bytes);
//   (d) a frame of a Product whose one World runs 20 Systems: trace off, on and not taken, on and taken.
// Method (declared before the run, as the logging declaration's judgement 7): warm-up, then 5 rounds; each
// call timed alone on the engine time base; per round the median and the p99; reported: the median of the
// 5 medians and of the 5 p99s. (a) to (c): 10,000 warm-up calls, rounds of 100,000; (d): 1,000 warm-up frames,
// rounds of 10,000. No retry, no round dropped.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/base/Trace.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/logging/JsonLinesFileSink.hpp"
#include "engine/logging/Logger.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/time/MonotonicClock.hpp"
#include "engine/trace/Trace.hpp"

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace L = GYO::Engine::Logging;
namespace T = GYO::Engine::Trace;

namespace {

class DiscardOutput final : public L::LineOutput {
public:
    bool Write(std::string_view bytes) noexcept override {
        bytes_ += bytes.size();
        return true;
    }
    std::uint64_t bytes_ = 0;
};

struct Round {
    std::int64_t median;
    std::int64_t p99;
};

template <class F>
std::vector<Round> Measure(const int warmup, const int perRound, F&& call) {
    for (int i = 0; i < warmup; ++i) call();
    std::vector<Round> rounds;
    std::vector<std::int64_t> samples(static_cast<std::size_t>(perRound));
    for (int r = 0; r < 5; ++r) {
        for (int i = 0; i < perRound; ++i) {
            const std::int64_t start = ::Engine::Time::NowNs();
            call();
            samples[static_cast<std::size_t>(i)] = ::Engine::Time::NowNs() - start;
        }
        std::sort(samples.begin(), samples.end());
        rounds.push_back({samples[static_cast<std::size_t>(perRound / 2)], samples[static_cast<std::size_t>(perRound * 99 / 100)]});
    }
    return rounds;
}

void Print(const char* name, const std::vector<Round>& rounds) {
    std::vector<std::int64_t> medians;
    std::vector<std::int64_t> p99s;
    std::printf("%s rounds:", name);
    for (const Round& round : rounds) {
        std::printf(" [median %lld ns, p99 %lld ns]", static_cast<long long>(round.median), static_cast<long long>(round.p99));
        medians.push_back(round.median);
        p99s.push_back(round.p99);
    }
    std::sort(medians.begin(), medians.end());
    std::sort(p99s.begin(), p99s.end());
    std::printf("\n%s summary: median of medians %lld ns, median of p99s %lld ns\n", name, static_cast<long long>(medians[2]),
                static_cast<long long>(p99s[2]));
}

void OneSpan() {
    ::Engine::Base::TraceSpan span;
    span.Begin("kernel", "MoveRacer", "Match");
}

// A Logger whose one sink is a JSON lines file sink discarding bytes; takeSpans: it takes "span.".
std::optional<L::LoggerHandle> InstallLogger(const bool takeSpans) {
    L::LoggerOptions options;
    auto output = std::make_shared<DiscardOutput>();
    auto fatal = std::make_shared<DiscardOutput>();
    L::SinkFilter filter;
    filter.minimum = L::Level::Trace;
    if (takeSpans) filter.include = {"span."};
    else filter.exclude = {"span."};
    options.sinks.push_back({.name = "file", .sink = L::JsonLinesFileOptions{.capacity = 1u << 20, .output = output, .fatalOutput = fatal}, .filter = filter});
    options.takeAssertions = false;
    options.takeTerminate = false;
    auto installed = L::Install(std::move(options));
    if (!installed) return std::nullopt;
    return std::move(installed.value());
}

// ---- (d)

struct Counter final {
    std::uint64_t value = 0;
};
K::SystemResult Count(K::ResMut<Counter> counter) {
    counter->value += 1;
    return {};
}

std::unique_ptr<O::Product> Twenty() {
    O::ProductDescription d;
    d.name = "budget";
    O::WorldDescription w;
    w.name = "Match";
    w.build = [](O::WorldSetup& s) {
        s.Registrar().Resource<Counter>({.name = "Counter", .zone = K::Zone::B, .writePolicy = K::WritePolicy::Shared});
        for (int i = 0; i < 20; ++i) {
            K::SystemOptions o;
            o.name = "Count" + std::to_string(i);
            o.zone = K::Zone::B;
            K::Mount m;
            m.schedule = "Main";
            m.stage = "Update";
            if (i > 0) m.after = {"Count" + std::to_string(i - 1)};
            o.mounts.push_back(std::move(m));
            s.Registrar().AddSystem<&Count>(std::move(o));
        }
    };
    w.prepare.push_back([](K::World& world) -> ::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(Counter{});
        return {};
    });
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), O::RegisteredUnits{});
    if (!built) {
        std::printf("Build failed: %s\n", built.error().detail.c_str());
        return nullptr;
    }
    return std::move(*built);
}

void Frames(const char* name) {
    auto product = Twenty();
    if (!product) return;
    Print(name, Measure(1'000, 10'000, [&] { static_cast<void>(product->RunFrame()); }));
}

} // namespace

int main() {
    Print("(a) span, no receiver", Measure(10'000, 100'000, OneSpan));
    Frames("(d) frame of 20 Systems, trace off");
    {
        auto logger = InstallLogger(false);
        auto trace = T::Install({});
        Print("(b) span, installed, not taken", Measure(10'000, 100'000, OneSpan));
        Frames("(d) frame of 20 Systems, trace on, not taken");
    }
    {
        auto logger = InstallLogger(true);
        auto trace = T::Install({});
        Print("(c) span, installed, taken into the queue", Measure(10'000, 100'000, OneSpan));
        Frames("(d) frame of 20 Systems, trace on, taken");
    }
    return 0;
}
