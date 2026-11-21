// Judgement 7 of the logging declaration (docs/architecture/target/v2/logging-declaration.zh-Hant.md,
// section 10; question 6, approved: measured only, no threshold). Run by hand on one host, Release; not a
// ctest. Three paths, recorded apart:
//   (a) ④ Logging::Write of one record of 8 fields (about 400 bytes as a JSON line) into a JSON lines file
//       sink's queue (its output discards the bytes, so the writer thread never falls behind on I/O);
//   (b) ① the same content through Forward: a frame whose System sends one message, logged by Forward's
//       System, minus the same frame without Forward (both medians are printed too);
//   (c) ④ the same record refused by the filter.
// Method (declared before the run): 10,000 warm-up calls, then 5 rounds of 100,000; each call timed alone
// on the engine time base; per round the median and the p99; reported: the median of the 5 medians and of
// the 5 p99s. No retry, no round dropped.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "engine/kernel/Kernel.hpp"
#include "engine/logging/JsonLinesFileSink.hpp"
#include "engine/logging/Logger.hpp"
#include "engine/logging/Write.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/time/MonotonicClock.hpp"
#include "engine/world_logging/Forward.hpp"

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace L = GYO::Engine::Logging;
namespace WL = GYO::Engine::WorldLogging;

namespace {

constexpr int kWarmup = 10'000;
constexpr int kRounds = 5;
constexpr int kPerRound = 100'000;

class DiscardOutput final : public L::LineOutput {
public:
    bool Write(std::string_view bytes) noexcept override {
        bytes_ += bytes.size();
        return true;
    }
    std::uint64_t bytes_ = 0;
};

const std::string& Padding() {
    static const std::string padding(150, 'p');
    return padding;
}

void WriteOne(const std::int64_t i) {
    L::Write(L::Level::Info, "trace.movement", "input applied",
             {{"tick", static_cast<std::uint64_t>(i)}, {"player", 3}, {"x", 1234.5678}, {"y", -87.25}, {"z", 0.125},
              {"queued", 2u}, {"accepted", true}, {"note", std::string_view(Padding())}});
}

struct Round {
    std::int64_t median;
    std::int64_t p99;
};

template <class F>
std::vector<Round> Measure(F&& call) {
    for (int i = 0; i < kWarmup; ++i) call(i);
    std::vector<Round> rounds;
    std::vector<std::int64_t> samples(kPerRound);
    for (int r = 0; r < kRounds; ++r) {
        for (int i = 0; i < kPerRound; ++i) {
            const std::int64_t start = ::Engine::Time::NowNs();
            call(i);
            samples[static_cast<std::size_t>(i)] = ::Engine::Time::NowNs() - start;
        }
        std::sort(samples.begin(), samples.end());
        rounds.push_back({samples[kPerRound / 2], samples[kPerRound * 99 / 100]});
    }
    return rounds;
}

Round MedianOf(const std::vector<Round>& rounds) {
    std::vector<std::int64_t> medians;
    std::vector<std::int64_t> p99s;
    for (const Round& round : rounds) {
        medians.push_back(round.median);
        p99s.push_back(round.p99);
    }
    std::sort(medians.begin(), medians.end());
    std::sort(p99s.begin(), p99s.end());
    return {medians[medians.size() / 2], p99s[p99s.size() / 2]};
}

void Print(const char* name, const std::vector<Round>& rounds) {
    std::printf("%s rounds:", name);
    for (const Round& round : rounds) std::printf(" [median %lld ns, p99 %lld ns]", static_cast<long long>(round.median), static_cast<long long>(round.p99));
    const Round summary = MedianOf(rounds);
    std::printf("\n%s summary: median of medians %lld ns, median of p99s %lld ns\n", name,
                static_cast<long long>(summary.median), static_cast<long long>(summary.p99));
}

// ---- (b)

struct Move final {
    std::uint64_t tick = 0;
    std::int32_t player = 3;
    std::int32_t queued = 2;
    double x = 1234.5678;
    double y = -87.25;
    double z = 0.125;
};
struct MoveLog final {
    using Message = Move;
    static constexpr std::string_view name = "Move";
    static constexpr std::string_view category = "trace.movement";
    static constexpr L::Level level = L::Level::Info;
    static void Describe(const Move& m, L::RecordBuilder& out) {
        out.Text("input applied");
        out.Add("tick", m.tick);
        out.Add("player", m.player);
        out.Add("x", m.x);
        out.Add("y", m.y);
        out.Add("z", m.z);
        out.Add("queued", m.queued);
        out.Add("accepted", true);
        out.Add("note", std::string_view(Padding()));
    }
};
struct TickCount final {
    std::uint64_t value = 0;
};

K::SystemResult SendMove(K::ResMut<TickCount> tick, K::MessageWriter<Move> moves) {
    moves.Send(Move{tick->value++});
    return {};
}

K::SystemOptions At(std::string name, std::string stage) {
    K::SystemOptions o;
    o.name = std::move(name);
    o.zone = K::Zone::B;
    K::Mount m;
    m.schedule = "Main";
    m.stage = std::move(stage);
    o.mounts.push_back(std::move(m));
    return o;
}

std::unique_ptr<O::Product> MoveProduct(const bool forward) {
    O::ProductDescription d;
    d.name = "budget";
    O::WorldDescription w;
    w.name = "Match";
    w.build = [forward](O::WorldSetup& s) {
        s.Registrar().Resource<TickCount>({.name = "TickCount", .zone = K::Zone::B});
        s.Registrar().Message<Move>({.name = "Move", .zone = K::Zone::B});
        s.Registrar().AddSystem<&SendMove>(At("SendMove", "Update"));
        if (forward) WL::Forward<MoveLog>(s, {.schedule = "Main", .stage = "Last"});
    };
    w.prepare.push_back([](K::World& world) -> ::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(TickCount{});
        return {};
    });
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), {});
    if (!built) {
        std::fprintf(stderr, "budget: the Product did not build: %s\n", built.error().detail.c_str());
        std::abort();
    }
    return std::move(*built);
}

} // namespace

int main() {
    auto output = std::make_shared<DiscardOutput>();
    auto fatal = std::make_shared<DiscardOutput>();
    L::LoggerOptions options;
    options.sinks.push_back({.name = "file",
                             .sink = L::JsonLinesFileOptions{.capacity = 1u << 22, .output = output, .fatalOutput = fatal},
                             .filter = {.include = {"trace."}, .minimum = L::Level::Info}});
    options.takeAssertions = false;
    options.takeTerminate = false;
    auto handle = L::Install(std::move(options));
    if (!handle) return 1;

    std::string line;
    {
        // The size of the record as a JSON line.
        const L::Field fields[] = {{"tick", 1u}, {"player", 3}, {"x", 1234.5678}, {"y", -87.25}, {"z", 0.125},
                                   {"queued", 2u}, {"accepted", true}, {"note", std::string_view(Padding())}};
        L::AppendJsonLine(line, L::Record{.category = "trace.movement", .text = "input applied", .fields = fields});
        std::printf("record: 8 fields, %zu bytes as a JSON line\n", line.size());
    }

    Print("(a) Write into the queue", Measure([](int i) { WriteOne(i); }));
    Print("(c) Write refused by the filter", Measure([](int i) {
              L::Write(L::Level::Debug, "trace.movement", "input applied",
                       {{"tick", static_cast<std::uint64_t>(i)}, {"player", 3}, {"x", 1234.5678}, {"y", -87.25},
                        {"z", 0.125}, {"queued", 2u}, {"accepted", true}, {"note", std::string_view(Padding())}});
          }));

    auto without = MoveProduct(false);
    auto with = MoveProduct(true);
    const auto framesWithout = Measure([&](int) { static_cast<void>(without->RunFrame()); });
    const auto framesWith = Measure([&](int) { static_cast<void>(with->RunFrame()); });
    Print("(b) frame without Forward", framesWithout);
    Print("(b) frame with Forward", framesWith);
    const Round a = MedianOf(framesWithout);
    const Round b = MedianOf(framesWith);
    std::printf("(b) Forward of one message: %lld ns (difference of the medians of medians)\n",
                static_cast<long long>(b.median - a.median));
    handle.value().Close();
    return 0;
}
