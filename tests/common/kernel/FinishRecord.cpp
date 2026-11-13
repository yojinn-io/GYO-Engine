// BU-3 record executable (migration plan section 6; registry-schedule
// declaration judgement 11): Registry::Finish (freeze and Scheduler analysis)
// with 500 synthetic Systems, budget <= 50 ms. Not a ctest (shared-runner
// timings are noise). Run in Release; it finishes the same Registry 10 times
// and prints every time, the median and the spread. Keep the output under the
// main checkout's build/evidence/kernel-registry-schedule/.
//
// Shape: 50 components (zone B, Shared), 500 Systems over 5 stages of one
// schedule; System n writes component n % 50 and reads component (n + 7) % 50,
// so most pairs in a stage conflict and are ordered by registration (ties), the
// worst case of spec D.4's O(N^2) pair scan. Every 10th System declares
// "before" the next one.

#include "engine/kernel/Kernel.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace GYO::Kernel;

template <int N>
struct Column {
    int value;
};

template <int N>
SystemResult Work(Query<Write<Column<N % 50>>, Read<Column<(N + 7) % 50>>>) {
    return {};
}

constexpr int kSystems = 500;
constexpr int kStages = 5;

template <int... C>
void RegisterColumns(Registrar& r, std::integer_sequence<int, C...>) {
    (r.Component<Column<C>>({.name = "Column" + std::to_string(C), .zone = Zone::B,
                             .writePolicy = WritePolicy::Shared}),
     ...);
}

template <int S>
void RegisterOne(Registrar& r) {
    Mount m;
    m.schedule = "Update";
    m.stage = "Stage" + std::to_string(S % kStages);
    if (S % 10 == 0 && S + kStages < kSystems) m.before = {"System" + std::to_string(S + kStages)};
    r.AddSystem<&Work<S>>({.name = "System" + std::to_string(S), .zone = Zone::B, .mounts = {m}});
}

// In chunks of 100: one fold over 500 arguments exceeds AppleClang's expression nesting limit (256).
template <int Offset, int... S>
void RegisterChunk(Registrar& r, std::integer_sequence<int, S...>) {
    (RegisterOne<Offset + S>(r), ...);
}

template <int... Chunk>
void RegisterSystems(Registrar& r, std::integer_sequence<int, Chunk...>) {
    (RegisterChunk<Chunk * 100>(r, std::make_integer_sequence<int, 100>{}), ...);
}

class SyntheticPlugin final : public Plugin {
public:
    std::string_view Name() const override { return "synthetic"; }
    void Build(Registrar& r) override {
        RegisterColumns(r, std::make_integer_sequence<int, 50>{});
        ScheduleOptions s;
        s.name = "Update";
        s.domain = Domain::Frame;
        s.clock = true;
        for (int i = 0; i < kStages; ++i) s.stages.push_back("Stage" + std::to_string(i));
        r.Schedule(s);
        RegisterSystems(r, std::make_integer_sequence<int, kSystems / 100>{});
    }
};

} // namespace

int main() {
    std::vector<double> ms;
    std::string fingerprint;
    for (int run = 0; run < 10; ++run) {
        SyntheticPlugin plugin;
        Registry registry({&plugin});
        const auto start = std::chrono::steady_clock::now();
        const auto result = registry.Finish();
        const auto end = std::chrono::steady_clock::now();
        if (!result) {
            std::printf("finish failed:\n%s\n", result.error().detail.c_str());
            return 1;
        }
        const double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
        ms.push_back(elapsed);
        const std::string text = registry.Frozen().RegistryFingerprint().Text();
        if (!fingerprint.empty() && text != fingerprint) {
            std::printf("fingerprint changed between runs\n");
            return 1;
        }
        fingerprint = text;
        std::printf("run %d: %.3f ms (%zu systems, %zu ties)\n", run, elapsed, registry.Frozen().Systems().size(),
                    registry.Frozen().TieBreaks().size());
    }
    std::vector<double> sorted = ms;
    std::sort(sorted.begin(), sorted.end());
    const double median = (sorted[4] + sorted[5]) / 2.0;
    std::printf("BU-3 finish @ %d systems: median %.3f ms, min %.3f ms, max %.3f ms, budget 50 ms -> %s\n", kSystems,
                median, sorted.front(), sorted.back(), median <= 50.0 ? "within" : "OVER");
    std::printf("registry fingerprint %s\n", fingerprint.c_str());
    return median <= 50.0 ? 0 : 2;
}
