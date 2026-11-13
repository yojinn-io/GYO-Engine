// v8-arch-measure-env-baseline M0: the cost of reading the clock the baseline
// record times with. Owner object_fps_pvp (a companion of BaselineRecord.cpp);
// run by hand, never a CTest (docs/architecture/plans/kernel/
// measure-env-baseline-declaration.zh-Hant.md section 3.4).
//
// It is a separate executable on purpose: adding it to BaselineRecord.cpp would
// change that binary, and the earlier container measurements are reused only
// because the measured binaries stay byte-identical.
//
//   gyo_object_fps_pvp_timer_floor --out FILE [--pairs N]
// writes one row: the distribution of the difference between two back-to-back
// std::chrono::steady_clock::now() calls, in nanoseconds.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::int64_t NearestRank(const std::vector<std::int64_t>& sorted, double quantile) {
    // The same nearest-rank rule as the baseline record: element ceil(q n).
    const auto n = sorted.size();
    auto rank = static_cast<std::size_t>(quantile * static_cast<double>(n));
    if (static_cast<double>(rank) < quantile * static_cast<double>(n)) ++rank;
    rank = std::clamp<std::size_t>(rank, 1, n);
    return sorted[rank - 1];
}

} // namespace

int main(int argc, char** argv) {
    std::string out;
    long pairs = 100000;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--out" && index + 1 < argc) {
            out = argv[++index];
        } else if (argument == "--pairs" && index + 1 < argc) {
            pairs = std::strtol(argv[++index], nullptr, 10);
        } else {
            std::fprintf(stderr, "usage: %s --out FILE [--pairs N]\n", argv[0]);
            return 2;
        }
    }
    if (out.empty() || pairs <= 0) {
        std::fprintf(stderr, "usage: %s --out FILE [--pairs N]\n", argv[0]);
        return 2;
    }

    using Clock = std::chrono::steady_clock;
    std::vector<std::int64_t> deltas(static_cast<std::size_t>(pairs));
    for (auto& delta : deltas) {
        const auto first = Clock::now();
        const auto second = Clock::now();
        delta = std::chrono::duration_cast<std::chrono::nanoseconds>(second - first).count();
    }
    std::sort(deltas.begin(), deltas.end());
    std::int64_t sum = 0;
    for (const auto delta : deltas) sum += delta;

    std::FILE* file = std::fopen(out.c_str(), "w");
    if (!file) {
        std::fprintf(stderr, "cannot write %s\n", out.c_str());
        return 1;
    }
    std::fprintf(file, "# format=gyo.kernel-baseline, version=1\n");
    std::fprintf(file, "# kind=timer-floor clock=steady_clock span=two back-to-back now() calls unit=ns "
                       "period_num=%lld period_den=%lld\n",
                 static_cast<long long>(Clock::period::num), static_cast<long long>(Clock::period::den));
    std::fprintf(file, "pairs,p50_ns,p95_ns,p99_ns,max_ns,min_ns,mean_ns\n");
    std::fprintf(file, "%ld,%lld,%lld,%lld,%lld,%lld,%.3f\n", pairs,
                 static_cast<long long>(NearestRank(deltas, 0.50)),
                 static_cast<long long>(NearestRank(deltas, 0.95)),
                 static_cast<long long>(NearestRank(deltas, 0.99)),
                 static_cast<long long>(deltas.back()), static_cast<long long>(deltas.front()),
                 static_cast<double>(sum) / static_cast<double>(pairs));
    std::fclose(file);
    return 0;
}
