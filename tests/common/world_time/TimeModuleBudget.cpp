// The budget measurements of judgement 5 (time-events declaration
// docs/architecture/target/v2/time-events-declaration.zh-Hant.md, sections 9.2 and 13; event cursor declaration
// 7.3). Not a ctest: a local measurement, run by hand on one host, its output kept as evidence.
//
// 1. time: the time Module's extra cost per frame (AdvanceTime + RunFixedSteps, the fixed step's schedule empty:
//    no author System in it), with 0, 1 and 5 steps a frame, against a Product with the same skeleton and the same
//    frame System but no time Module. The two Products run frame by frame alternately so drift hits both alike.
//    Declared before the run: the extra (module minus baseline) of the mean and of the p99 is at most 156 us
//    (1% of a 64 Hz step) for each of 0, 1 and 5 steps.
// 2. memory: the message buffers of a World with 100 message types of 32-byte payloads, each holding 1024 records
//    (a reader that ran before the writes holds them). Measured as each buffer's records allocation (vector
//    capacity x record size; a record is the payload plus an 8-byte seq) plus the buffer object; the allocator's own
//    overhead is not counted. Calculated (event cursor declaration 7.3): 1024 x 40 bytes a type, 4,096,000 bytes
//    for 100. Declared: at most twice the calculation.
//
// Usage: gyo_world_time_budget [time|memory|all] [frames]   (default: all, 10000 frames after 1000 of warm-up)

#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/world_time/TimeTypes.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace W = GYO::Engine::WorldTime;

constexpr W::Nanoseconds kStep = 15'625'000;  // 64 Hz
constexpr double kTimeBudgetUs = 156.0;

// ---- 1. time

struct FrameCount {
    std::uint64_t value = 0;
};
// The same frame System in both Products.
K::SystemResult CountFrame(K::ResMut<FrameCount> count) {
    ++count->value;
    return {};
}

void FrameSystems(O::WorldSetup& s) {
    K::Registrar& r = s.Registrar();
    r.Resource<FrameCount>({.name = "FrameCount"});
    K::SystemOptions o;
    o.name = "CountFrame";
    K::Mount m;
    m.schedule = std::string(O::DefaultSchedules::frame);
    m.stage = std::string(O::DefaultSchedules::update);
    o.mounts.push_back(std::move(m));
    r.AddSystem<&CountFrame>(std::move(o));
}

O::ProductDescription TimedGame(std::uint32_t steps, bool withTime) {
    O::ProductDescription d;
    d.name = withTime ? "with-time" : "baseline";
    O::WorldDescription w;
    w.name = "Main";
    w.build = &FrameSystems;
    if (withTime) w.options.Set(W::WorldTimeOptions{});
    const W::Nanoseconds delta = static_cast<W::Nanoseconds>(steps) * kStep;
    w.prepare.push_back([delta, withTime](K::World& world) -> ::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(FrameCount{});
        if (withTime) world.SetResource(W::TimeSampling{delta, W::SamplingMode::ManualDelta});
        return {};
    });
    d.worlds.push_back(std::move(w));
    return d;
}

O::RegisteredUnits Units(bool withTime) {
    O::RegisteredUnits units;
    if (!withTime) return units;
    O::RegisterRow row;
    row.name = "world_time";
    row.kind = O::UnitKind::Module;
    row.libraryOnly = false;
    row.defaultOn = true;
    units.rows.push_back(row);
    units.modules.push_back({"world_time", std::make_unique<W::WorldTimeModule>()});
    return units;
}

std::unique_ptr<O::Product> Make(O::ProductDescription d, O::RegisteredUnits units) {
    auto built = O::Product::Build(std::move(d), std::move(units));
    if (!built) {
        std::fprintf(stderr, "build failed: %s\n", built.error().detail.c_str());
        std::exit(2);
    }
    return std::move(*built);
}

double Frame(O::Product& p) {
    const auto t0 = std::chrono::steady_clock::now();
    auto ran = p.RunFrame();
    const auto t1 = std::chrono::steady_clock::now();
    if (!ran) {
        std::fprintf(stderr, "frame failed: %s\n", ran.error().message.c_str());
        std::exit(2);
    }
    return std::chrono::duration<double, std::micro>(t1 - t0).count();
}

struct Stats {
    double mean = 0;
    double p50 = 0;
    double p99 = 0;
    double max = 0;
};
Stats Of(std::vector<double> us) {
    Stats s;
    for (double v : us) s.mean += v;
    s.mean /= static_cast<double>(us.size());
    std::sort(us.begin(), us.end());
    const auto at = [&us](double q) { return us[std::min(us.size() - 1, static_cast<std::size_t>(q * static_cast<double>(us.size())))]; };
    s.p50 = at(0.50);
    s.p99 = at(0.99);
    s.max = us.back();
    return s;
}

bool MeasureTime(int frames) {
    std::printf("== time: per-frame cost of RunFrame, %d frames after 1000 of warm-up, alternating; budget %.0f us extra\n",
                frames, kTimeBudgetUs);
    std::printf("steps | module mean p50 p99 max (us) | baseline mean p50 p99 max (us) | extra mean p99 (us) | ticks | verdict\n");
    bool pass = true;
    for (std::uint32_t steps : {0u, 1u, 5u}) {
        auto timed = Make(TimedGame(steps, true), Units(true));
        auto base = Make(TimedGame(steps, false), Units(false));
        for (int i = 0; i < 1000; ++i) {
            Frame(*timed);
            Frame(*base);
        }
        std::vector<double> a;
        std::vector<double> b;
        a.reserve(static_cast<std::size_t>(frames));
        b.reserve(static_cast<std::size_t>(frames));
        for (int i = 0; i < frames; ++i) {
            a.push_back(Frame(*timed));
            b.push_back(Frame(*base));
        }
        const Stats m = Of(a);
        const Stats n = Of(b);
        const std::uint64_t ticks = timed->World("Main").GetWorld().Resource<W::Tick>()->value;
        const std::uint64_t expected = static_cast<std::uint64_t>(steps) * static_cast<std::uint64_t>(frames + 1000);
        const double extraMean = m.mean - n.mean;
        const double extraP99 = m.p99 - n.p99;
        const bool ok = extraMean <= kTimeBudgetUs && extraP99 <= kTimeBudgetUs && ticks == expected;
        pass = pass && ok;
        std::printf("%5u | %.3f %.3f %.3f %.3f | %.3f %.3f %.3f %.3f | %.3f %.3f | %llu/%llu | %s\n", steps, m.mean, m.p50,
                    m.p99, m.max, n.mean, n.p50, n.p99, n.max, extraMean, extraP99, static_cast<unsigned long long>(ticks),
                    static_cast<unsigned long long>(expected), ok ? "pass" : "FAIL");
    }
    return pass;
}

// ---- 2. memory

template <int N>
struct Payload32 {
    std::uint64_t v[4] = {};
};
static_assert(sizeof(Payload32<0>) == 32);
constexpr int kTypes = 100;
constexpr std::uint32_t kCapacity = 1024;

template <int N>
K::SystemResult Fill(K::MessageWriter<Payload32<N>> out) {
    for (std::uint32_t i = 0; i < kCapacity; ++i) out.Send(Payload32<N>{{i, i, i, i}});
    return {};
}
// A reader that ran before the writes and has not read them: it holds them past the Run's trim.
template <int N>
K::SystemResult Hold(K::MessageReader<Payload32<N>>) {
    return {};
}

K::SystemOptions In(std::string name, std::string schedule) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = std::move(schedule);
    m.stage = "S";
    o.mounts.push_back(std::move(m));
    return o;
}

template <int... N>
void RegisterAll(K::Registrar& r, std::integer_sequence<int, N...>) {
    (r.Message<Payload32<N>>({.name = "Payload" + std::to_string(N)}), ...);
    (r.AddSystem<&Fill<N>>(In("Fill" + std::to_string(N), "Fill")), ...);
    (r.AddSystem<&Hold<N>>(In("Hold" + std::to_string(N), "Hold")), ...);
}

void MemorySystems(O::WorldSetup& s) {
    K::Registrar& r = s.Registrar();
    for (const char* name : {"Hold", "Fill"}) {
        K::ScheduleOptions o;
        o.name = name;
        o.stages = {"S"};
        r.Schedule(std::move(o));
    }
    RegisterAll(r, std::make_integer_sequence<int, kTypes>{});
}

struct BufferBytes {
    std::size_t records = 0;
    std::size_t bytes = 0;
};
template <int N>
BufferBytes BytesOf(K::World& w) {
    using Buffer = K::Detail::MessageBuffer<Payload32<N>>;
    const K::MessageTypeId id = K::Detail::MessageIdOf(w, K::KeyOf<Payload32<N>>());
    const auto& buffer = static_cast<const Buffer&>(K::Detail::MessagesAt(w, id));
    return {buffer.records.size(), buffer.records.capacity() * sizeof(typename Buffer::Record) + sizeof(Buffer)};
}
template <int... N>
BufferBytes SumBytes(K::World& w, std::integer_sequence<int, N...>) {
    BufferBytes total;
    ((total.records += BytesOf<N>(w).records, total.bytes += BytesOf<N>(w).bytes), ...);
    return total;
}

bool MeasureMemory() {
    O::ProductDescription d;
    d.name = "memory";
    O::WorldDescription w;
    w.name = "Main";
    w.skeleton = O::ScheduleSkeleton::Off;
    w.build = &MemorySystems;
    d.worlds.push_back(std::move(w));
    auto p = Make(std::move(d), O::RegisteredUnits{});
    for (const char* schedule : {"Hold", "Fill"}) {
        auto ran = p->RunSchedule("Main", schedule);
        if (!ran) {
            std::fprintf(stderr, "run %s failed: %s\n", schedule, ran.error().message.c_str());
            return false;
        }
    }
    K::World& world = p->World("Main").GetWorld();
    const BufferBytes total = SumBytes(world, std::make_integer_sequence<int, kTypes>{});
    const std::size_t record = sizeof(typename K::Detail::MessageBuffer<Payload32<0>>::Record);
    const std::size_t calculated = static_cast<std::size_t>(kTypes) * kCapacity * (32 + 8);
    const BufferBytes one = BytesOf<0>(world);
    const bool ok = total.bytes <= 2 * calculated && total.records == static_cast<std::size_t>(kTypes) * kCapacity;
    std::printf("== memory: %d message types x %u records x 32-byte payload (record %zu bytes)\n", kTypes, kCapacity, record);
    std::printf("records kept %zu; one type %zu bytes; all types %zu bytes; calculated %zu bytes; ratio %.4f; budget 2.0; %s\n",
                total.records, one.bytes, total.bytes, calculated,
                static_cast<double>(total.bytes) / static_cast<double>(calculated), ok ? "pass" : "FAIL");
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    const std::string what = argc > 1 ? argv[1] : "all";
    const int frames = argc > 2 ? std::atoi(argv[2]) : 10000;
    bool pass = true;
    if (what == "time" || what == "all") pass = MeasureTime(frames > 0 ? frames : 10000) && pass;
    if (what == "memory" || what == "all") pass = MeasureMemory() && pass;
    return pass ? 0 : 1;
}
