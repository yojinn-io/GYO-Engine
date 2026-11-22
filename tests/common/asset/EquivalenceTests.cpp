// Order does not change what the store ends with (declaration section 10 "等價測試（AS-5）" and "真執行緒模式"
// at the store layer; step 2 of section 9). Each order is compared with its own baseline (the same
// requests and drops, every job run at once); lives and attempts are left out, as they follow the order.
#include "StoreSupport.hpp"

using namespace AssetTest;
using Asset::AssetId;

namespace {

// 20 ids: probes, chains of linked assets, a cycle (ca, cb), a self reference (cc) and a missing file (f).
const std::vector<std::string>& AllIds() {
    static const std::vector<std::string> ids{"p0", "p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8", "p9",
                                              "l0", "l1", "l2", "l3", "l4", "l5", "ca", "cb", "cc", "f"};
    return ids;
}

void AddContent(TestContent& content) {
    for (int index = 0; index < 10; ++index) {
        const std::string id = "p" + std::to_string(index);
        content.Add(id, "probe", id + ".probe", "bytes of " + id);
    }
    content.Add("l0", "linked", "l0.links", "probe:p0\nprobe:p1");
    content.Add("l1", "linked", "l1.links", "id:l2\nprobe:p2");
    content.Add("l2", "linked", "l2.links", "probe:p3");
    content.Add("l3", "linked", "l3.links", "id:l4");
    content.Add("l4", "linked", "l4.links", "probe:p4\nprobe:p5");
    content.Add("l5", "linked", "l5.links", "probe:p6\nprobe:p7\nprobe:p8");
    content.Add("ca", "linked", "ca.links", "id:cb");
    content.Add("cb", "linked", "cb.links", "id:ca");
    content.Add("cc", "linked", "cc.links", "id:cc");
    content.Add("f", "probe", "f.probe");
}

using Handles = std::map<std::string, std::optional<Asset::Handle<Linked>>>;
using ProbeHandles = std::map<std::string, std::optional<Asset::Handle<Probe>>>;

// Requests or drops what an order does, by id (probes and linked assets kept apart by their type).
struct Holder {
    Asset::AssetStore& store;
    Handles linked;
    ProbeHandles probes;

    void Request(const std::string& id) {
        if (id.starts_with("p") || id == "f") {
            probes[id] = store.Request<Probe>(AssetId(id));
        } else {
            linked[id] = store.Request<Linked>(AssetId(id));
        }
    }
    void Drop(const std::string& id) {
        probes.erase(id);
        linked.erase(id);
    }
};

using Order = std::function<void(ControlledExecutor&, Asset::AssetStore&, Holder&)>;

struct Result {
    std::vector<ContentLine> contents;
    Asset::AssetStoreCounts counts;
};

// Runs an order on a fresh store; afterwards every job is run and maintenance taken, and the contents read.
Result Run(const std::string& name, const Order& order) {
    TheScript().Reset();
    TestContent content("equivalence-" + name);
    AddContent(content);
    ControlledExecutor executor;
    Asset::AssetStore store(content.Entries(), content.Options(), &executor);
    RegisterTestLoaders(store);
    Holder holder{store, {}, {}};
    order(executor, store, holder);
    executor.RunAll();
    store.Pump();
    Result result{Contents(store.Snapshot()), store.Counts()};
    CHECK(Idle(result.counts));
    CheckEquations(result.counts);
    return result;
}

void RequestAll(Holder& holder, const std::vector<std::string>& ids) {
    for (const std::string& id : ids) holder.Request(id);
}

// Runs up to `count` jobs, taking each from the back of the queue (reverse) or alternately from the back
// and the front (interleaved).
void RunSome(ControlledExecutor& executor, const std::size_t count, const bool interleave) {
    for (std::size_t run = 0; run < count && executor.Pending() > 0; ++run) {
        const bool back = !interleave || run % 2 == 0;
        executor.RunAt(back ? executor.Pending() - 1 : 0);
    }
}

} // namespace

TEST_CASE("equiv: every completion order ends in the same store contents") {
    Fresh fresh;
    const auto requestAll = [](ControlledExecutor&, Asset::AssetStore&, Holder& holder) {
        RequestAll(holder, AllIds());
    };
    // (1) all at once: the baseline of (1) to (3).
    const Result baseline = Run("all-at-once", requestAll);
    // (2) in reverse, in batches of 5 (dependencies queue more jobs, so more than 4 batches).
    const Result reversed = Run("reverse", [](ControlledExecutor& executor, Asset::AssetStore& store, Holder& holder) {
        RequestAll(holder, AllIds());
        while (executor.Pending() > 0) {
            RunSome(executor, 5, false);
            store.Pump();
        }
    });
    // (3) interleaved, in batches of 3, 3, 3, 3, 3, 3, 2 and the rest, maintenance after each.
    const Result interleaved =
        Run("interleaved", [](ControlledExecutor& executor, Asset::AssetStore& store, Holder& holder) {
            RequestAll(holder, AllIds());
            for (const std::size_t batch : {3, 3, 3, 3, 3, 3, 2}) {
                RunSome(executor, batch, true);
                store.Pump();
            }
        });
    CHECK_MESSAGE(reversed.contents == baseline.contents, "reverse:\n"
                                                              << Describe(reversed.contents) << "all at once:\n"
                                                              << Describe(baseline.contents));
    CHECK_MESSAGE(interleaved.contents == baseline.contents, "interleaved:\n"
                                                                 << Describe(interleaved.contents) << "all at once:\n"
                                                                 << Describe(baseline.contents));
    CHECK(baseline.contents.size() == 20);
}

TEST_CASE("equiv: dropping and requesting while jobs run ends as when every job runs at once") {
    Fresh fresh;
    const std::vector<std::string> first{"p0", "p1", "p2", "p3", "p4", "p5", "p6", "p7", "p9",
                                         "l0", "l1", "l2", "l4", "l5", "ca", "cb", "f"};
    const std::vector<std::string> dropped{"p9", "f", "cb"};
    const std::vector<std::string> later{"l3", "cc", "p8"};
    const Result baseline = Run("drop-baseline", [&](ControlledExecutor&, Asset::AssetStore& store, Holder& holder) {
        RequestAll(holder, first);
        for (const std::string& id : dropped) holder.Drop(id);
        store.Pump();
        RequestAll(holder, later);
    });
    const Result midway = Run("drop-midway", [&](ControlledExecutor& executor, Asset::AssetStore& store,
                                                 Holder& holder) {
        RequestAll(holder, first);
        RunSome(executor, 9, true);
        for (const std::string& id : dropped) holder.Drop(id);
        store.Pump();
        RequestAll(holder, later);
        RunSome(executor, 4, false);
        store.Pump();
    });
    CHECK_MESSAGE(midway.contents == baseline.contents, "midway:\n"
                                                            << Describe(midway.contents) << "baseline:\n"
                                                            << Describe(baseline.contents));
}

TEST_CASE("equiv: a cycle and a self reference end the same in both completion orders") {
    Fresh fresh;
    const std::vector<std::string> ids{"ca", "cb", "cc"};
    const Result baseline = Run("cycle-baseline", [&](ControlledExecutor&, Asset::AssetStore&, Holder& holder) {
        RequestAll(holder, ids);
    });
    const Result bFirst = Run("cycle-b-first", [&](ControlledExecutor& executor, Asset::AssetStore&, Holder& holder) {
        RequestAll(holder, ids);
        executor.RunAt(1); // cb before ca
    });
    const Result ccFirst =
        Run("cycle-cc-first", [&](ControlledExecutor& executor, Asset::AssetStore&, Holder& holder) {
            RequestAll(holder, ids);
            executor.RunAt(2);
            executor.RunAt(1);
        });
    CHECK(bFirst.contents == baseline.contents);
    CHECK(ccFirst.contents == baseline.contents);
}

TEST_CASE("threads: threads requesting, dropping and taking maintenance at once end as the controlled run") {
    Fresh fresh;
    const Result expected = Run("threads-expected", [](ControlledExecutor&, Asset::AssetStore&, Holder& holder) {
        RequestAll(holder, AllIds());
    });
    int matched = 0;
    int balanced = 0;
    constexpr int kRuns = 50;
    for (int run = 0; run < kRuns; ++run) {
        TheScript().Reset();
        TestContent content("threads-" + std::to_string(run % 4));
        AddContent(content);
        PoolExecutor executor(3);
        Asset::AssetStoreOptions options = content.Options();
        options.maxInFlight = 4;
        Asset::AssetStore store(content.Entries(), options, &executor);
        RegisterTestLoaders(store);
        std::vector<std::thread> threads;
        for (int thread = 0; thread < 4; ++thread) {
            threads.emplace_back([&store, thread, run] {
                Holder holder{store, {}, {}};
                for (int round = 0; round < 3; ++round) {
                    for (std::size_t index = 0; index < AllIds().size(); ++index) {
                        if ((index + static_cast<std::size_t>(thread + run + round)) % 3 == 0) {
                            holder.Request(AllIds()[index]);
                        }
                    }
                    for (std::size_t index = 0; index < AllIds().size(); index += 2) {
                        holder.Drop(AllIds()[(index + static_cast<std::size_t>(thread)) % AllIds().size()]);
                    }
                    store.Pump();
                }
            });
        }
        for (std::thread& thread : threads) thread.join();
        Holder holder{store, {}, {}};
        RequestAll(holder, AllIds());
        REQUIRE(WaitIdle(store));
        store.Pump();
        REQUIRE(WaitIdle(store));
        store.Pump();
        const auto contents = Contents(store.Snapshot());
        if (contents == expected.contents) {
            ++matched;
        } else {
            MESSAGE("run " << run << ":\n" << Describe(contents));
        }
        const auto counts = store.Counts();
        if (Idle(counts) && counts.jobsQueued == counts.jobsLoaded + counts.jobsFailed + counts.jobsCancelled &&
            counts.requests == counts.hits + counts.revives + counts.newLives + counts.requestFailures &&
            counts.dropsTaken == counts.dropsStale + counts.dropsSkipped + counts.removals) {
            ++balanced;
        }
        store.Shutdown(Asset::AssetShutdown::Discard);
        executor.Stop();
    }
    CHECK(matched == kRuns);
    CHECK(balanced == kRuns);
}
