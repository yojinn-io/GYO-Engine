// The Product's buffer, the Product-level options, Configure reports, OnDestroy and the order
// ~Product destroys things in (docs/architecture/target/v2/exchange-declaration.zh-Hant.md,
// section 5.2 and section 10 cases P-1 to P-9). Synthetic units and services only.
// Each CHECK carries a tag in brackets ("[or-pbuf-...]"): the batch's mutants expect it
// (docs/architecture/target/v2/scripts/orchestration_mutations.json).

#include "OrchestrationTestSupport.hpp"
#include "AssertTestSupport.hpp"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace GYO::Test::Orchestration;

namespace {

using PResult = ::GYO::Engine::Base::Result<void, O::OrchestrationError>;

// A Product-level object a unit puts for the others (a store, a pool).
struct Store {
    std::vector<std::string> entries;
};

// An object the product itself puts in its own configure.
struct ProductThing {
    int value = 7;
};

// Logs "~<name>" when destroyed (only the live one: a moved-from copy logs nothing).
struct Probe {
    std::string name;
    bool live = true;
    explicit Probe(std::string n) : name(std::move(n)) {}
    Probe(Probe&& other) noexcept : name(std::move(other.name)), live(other.live) { other.live = false; }
    Probe& operator=(Probe&& other) noexcept {
        if (live) Log().push_back("~" + name);
        name = std::move(other.name);
        live = other.live;
        other.live = false;
        return *this;
    }
    Probe(const Probe&) = delete;
    Probe& operator=(const Probe&) = delete;
    ~Probe() {
        if (live) Log().push_back("~" + name);
    }
};
// A probe shared by closures (copies of a std::function share it): logs once, when the last closure goes.
std::shared_ptr<Probe> Shared(std::string name) {
    return std::make_shared<Probe>(std::move(name));
}

// A second probe type for the buffer (one type, one owner).
struct LateProbe {
    Probe probe{"buffer-second"};
};

// A World Resource that logs when the World destroys it.
struct WorldProbe {
    Probe probe{"world"};
    WorldProbe() = default;
    explicit WorldProbe(std::string name) : probe(std::move(name)) {}
};

// An interface the product gives a unit through the options, in a holder struct.
class Executor {
public:
    virtual ~Executor() = default;
    virtual void Submit(std::string job) = 0;
};
class RecordingExecutor final : public Executor {
public:
    void Submit(std::string job) override { jobs.push_back(std::move(job)); }
    std::vector<std::string> jobs;
};
struct ExecutorOption {
    std::shared_ptr<Executor> executor;
};
// An option only the Product's option bag holds: logs when the options go.
struct OptionProbe {
    std::shared_ptr<Probe> probe;
};

O::ProductDescription Game(std::function<void(O::WorldSetup&)> build = {}, std::string world = "Main") {
    O::ProductDescription d;
    d.name = "game";
    d.worlds.push_back(World(world, [build](O::WorldSetup& s) {
        Basics(s.Registrar(), {FrameClock("Tick")});
        if (build) build(s);
    }, {"Tick"}));
    return d;
}

std::string LogSince(std::size_t from) {
    std::string out;
    for (std::size_t i = from; i < Log().size(); ++i) out += (out.empty() ? "" : " ") + Log()[i];
    return out;
}

} // namespace

TEST_CASE("product-buffer: P-1 one unit creates in Configure; later units, the product and the runner find it") {
    Log().clear();
    std::vector<std::string> seen;
    const O::ModuleBuffer* productBuffer = nullptr;
    Units units;
    units.Module("store", {.configure = [](O::ProductSetup& p) { p.Buffer().Create<Store>().entries.push_back("store"); }});
    units.Module("user",
                 {.configure = [&seen, &productBuffer](O::ProductSetup& p) {
                      productBuffer = &p.Buffer();
                      Store* store = p.Buffer().Find<Store>();
                      seen.push_back(store != nullptr ? "configure:found" : "configure:none");
                      if (store != nullptr) store->entries.push_back("user");
                      // The product's own configure runs after every unit: its object is not there yet.
                      seen.push_back(p.Buffer().Find<ProductThing>() == nullptr ? "configure:no-product-thing"
                                                                                : "configure:product-thing");
                  },
                  .build = [&seen](O::WorldSetup& s) {
                      seen.push_back(s.ProductBuffer().Find<Store>() != nullptr ? "build:found" : "build:none");
                      seen.push_back(s.ProductBuffer().Find<ProductThing>() != nullptr ? "build:product-thing"
                                                                                       : "build:no-product-thing");
                  },
                  .finish = [&seen, &productBuffer](const K::FrozenRegistry&, O::WorldFinish& c) {
                      if (&c.ProductBuffer() != productBuffer) return seen.push_back("finish:other-buffer");
                      Store* store = c.ProductBuffer().Find<Store>();  // writable: the Product's buffer is never frozen
                      seen.push_back(store != nullptr ? "finish:found" : "finish:none");
                  },
                  .check = [&seen, &productBuffer](const K::FrozenRegistry&, O::WorldCheck& c) {
                      if (&c.ProductBuffer() != productBuffer) return seen.push_back("check:other-buffer");
                      Store* store = c.ProductBuffer().Find<Store>();
                      seen.push_back(store != nullptr ? "check:found" : "check:none");
                  }},
                 {"store"});
    O::ProductDescription d = Game([&seen](O::WorldSetup& s) {
        seen.push_back(s.ProductBuffer().Find<Store>() != nullptr ? "product-build:found" : "product-build:none");
    });
    d.configure = [&seen](O::ProductSetup& p) {
        seen.push_back(p.Buffer().Find<Store>() != nullptr ? "product-configure:found" : "product-configure:none");
        p.Buffer().Create<ProductThing>();
        p.InstallRunner("reader", [&seen](O::Product& product) {
            Store* store = product.Buffer().Find<Store>();
            seen.push_back(store != nullptr ? "runner:" + std::to_string(store->entries.size()) : "runner:none");
            return O::RunnerExit{};
        });
    };
    auto built = O::Product::Build(std::move(d), units.Take());
    ORCH_REQUIRE_BUILT(built);
    (*built)->Run();
    std::string all;
    for (const std::string& s : seen) all += s + " ";
    CHECK_MESSAGE(all ==
                      "configure:found configure:no-product-thing product-configure:found build:found "
                      "build:product-thing product-build:found finish:found check:found runner:2 ",
                  "[or-pbuf-reach] " << all);
}

TEST_CASE("product-buffer: P-2 Find of a type nobody created is nullptr; two creators of one type are a Programmer Error") {
    bool absent = false;
    Units units;
    units.Module("reader", {.configure = [&absent](O::ProductSetup& p) { absent = p.Buffer().Find<Store>() == nullptr; }});
    auto built = O::Product::Build(Game(), units.Take());
    ORCH_REQUIRE_BUILT(built);
    CHECK_MESSAGE(absent, "[or-pbuf-absent] Find found a Store nobody created");
    CHECK_MESSAGE((*built)->Buffer().Find<Store>() == nullptr, "[or-pbuf-absent] Find made a Store");

    Units twice;
    twice.Module("first", {.configure = [](O::ProductSetup& p) { p.Buffer().Create<Store>(); }});
    twice.Module("second", {.configure = [](O::ProductSetup& p) { p.Buffer().Create<Store>(); }});
    GYO_CHECK_ASSERTS(O::Product::Build(Game(), twice.Take()));
}

TEST_CASE("product-buffer: P-3 a replaced World keeps the same Product buffer; the World build's buffer is new") {
    std::vector<const void*> productBuffers;
    std::vector<const void*> stores;
    std::vector<const void*> worldBuffers;
    Units units;
    units.Module("store", {.configure = [](O::ProductSetup& p) { p.Buffer().Create<Store>(); },
                           .build = [&](O::WorldSetup& s) {
                               productBuffers.push_back(&s.ProductBuffer());
                               stores.push_back(s.ProductBuffer().Find<Store>());
                               worldBuffers.push_back(&s.Buffer());
                           }});
    auto built = O::Product::Build(Game(), units.Take());
    ORCH_REQUIRE_BUILT(built);
    REQUIRE(static_cast<bool>((*built)->ReplaceWorld("Main", O::ReplaceRequest{})));
    REQUIRE(productBuffers.size() == 2);
    CHECK_MESSAGE(productBuffers[0] == productBuffers[1], "[or-pbuf-same] the replacement saw another Product buffer");
    CHECK_MESSAGE((stores[0] != nullptr && stores[0] == stores[1]), "[or-pbuf-same] the Store changed");
    CHECK_MESSAGE(productBuffers[0] == &(*built)->Buffer(), "[or-pbuf-same] Product::Buffer is another buffer");
    CHECK_MESSAGE(productBuffers[0] != worldBuffers[0], "[or-pbuf-same] the World build was given the Product's buffer");
}

TEST_CASE("product-buffer: P-4 the description's options are there from the first unit's Configure; an interface comes in a holder") {
    auto executor = std::make_shared<RecordingExecutor>();
    bool sawInFirst = false;
    Units units;
    units.Module("asset", {.configure = [&sawInFirst](O::ProductSetup& p) {
                     const ExecutorOption* option = p.Options<ExecutorOption>();
                     sawInFirst = option != nullptr && option->executor != nullptr;
                     if (sawInFirst) option->executor->Submit("decode");
                 }});
    O::ProductDescription d = Game();
    d.options.Set(ExecutorOption{executor});
    auto built = O::Product::Build(std::move(d), units.Take());
    ORCH_REQUIRE_BUILT(built);
    CHECK_MESSAGE(sawInFirst, "[or-pbuf-options] the first unit's Configure saw no option");
    CHECK_MESSAGE((executor->jobs.size() == 1 && executor->jobs[0] == "decode"), "[or-pbuf-options] the executor was not called");
}

TEST_CASE("product-buffer: P-5 ~Product destroys Worlds in reverse, then OnDestroy in reverse, the runner, the buffer, the descriptions, the units") {
    SUBCASE("a Product that was built") {
        Log().clear();
        auto unitProbe = Shared("unit-closure");
        auto describeProbe = Shared("description");
        Units units;
        units.Module("store", {.configure = [unitProbe](O::ProductSetup& p) {
                         p.Buffer().Create<Probe>("buffer-first");
                         p.OnDestroy([] { Log().push_back("ondestroy store"); });
                     }});
        units.Module("pool", {.configure = [](O::ProductSetup& p) {
                        p.Buffer().Create<Store>();
                        p.OnDestroy([] { Log().push_back("ondestroy pool"); });
                    }},
                     {"store"});
        O::ProductDescription d;
        d.name = "game";
        for (const char* name : {"A", "B"}) {
            const std::string world = name;
            d.worlds.push_back(World(world, [world, describeProbe](O::WorldSetup& s) {
                Basics(s.Registrar(), {FrameClock("Tick")});
                s.Registrar().Resource<WorldProbe>({.name = "WorldProbe"});
                auto exchangeProbe = Shared("exchange " + world);
                s.ExchangeOut([exchangeProbe](K::World&) -> PResult { return {}; });
                s.Prepare([world](K::World& w) -> PResult {
                    w.SetResource(WorldProbe("world " + world));
                    return {};
                });
            }, {"Tick"}));
        }
        d.options.Set(OptionProbe{Shared("options")});
        d.configure = [](O::ProductSetup& p) {
            auto runnerProbe = Shared("runner");  // only the runner holds it
            p.InstallRunner("r", [runnerProbe](O::Product&) { return O::RunnerExit{}; });
        };
        auto built = O::Product::Build(std::move(d), units.Take());
        ORCH_REQUIRE_BUILT(built);
        unitProbe.reset();
        describeProbe.reset();
        // The buffer's second object, created after the first.
        (*built)->Buffer().Create<LateProbe>();
        const std::size_t from = Log().size();
        (*built).reset();
        CHECK_MESSAGE(LogSince(from) ==
                          "~exchange B ~world B ~exchange A ~world A ondestroy pool ondestroy store ~runner "
                          "~buffer-second ~buffer-first ~description ~options ~unit-closure",
                      "[or-pbuf-order] " << LogSince(from));
    }
    SUBCASE("a Product whose Build failed (two runners: O-E4) follows the same order") {
        Log().clear();
        Units units;
        units.Module("store", {.configure = [](O::ProductSetup& p) {
                         p.Buffer().Create<Probe>("buffer");
                         p.OnDestroy([] { Log().push_back("ondestroy store"); });
                         auto runnerProbe = Shared("runners");  // only the two runners hold it
                         p.InstallRunner("one", [runnerProbe](O::Product&) { return O::RunnerExit{}; });
                         p.InstallRunner("two", [runnerProbe](O::Product&) { return O::RunnerExit{}; });
                     }});
        const std::size_t from = Log().size();
        {
            auto built = O::Product::Build(Game(), units.Take());
            REQUIRE_FALSE(static_cast<bool>(built));
            CHECK(built.error().Has(O::OrchestrationCode::OE4));
        }
        CHECK_MESSAGE(LogSince(from).find("ondestroy store ~runners ~buffer") != std::string::npos,
                      "[or-pbuf-order-failed] " << LogSince(from));
    }
}

TEST_CASE("product-buffer: P-6 OnDestroy runs exactly once on every way a Product ends; the buffer is still there") {
    struct Way {
        const char* name;
        std::function<void(Units&, O::ProductDescription&)> setup;
        std::function<void(O::Product&)> afterBuild;
    };
    const std::vector<Way> ways = {
        {"normal", {}, {}},
        {"broken World",
         [](Units&, O::ProductDescription& d) {
             d.worlds[0].build = [](O::WorldSetup& s) {
                 Basics(s.Registrar(), {FrameClock("Tick")});
                 s.Registrar().AddSystem<&Fail>(In("Breaks", "Tick"));
             };
         },
         [](O::Product& p) { static_cast<void>(p.RunFrame()); }},
        {"DropOldFirst left the place empty",
         [](Units&, O::ProductDescription& d) {
             d.worlds[0].build = [](O::WorldSetup& s) {
                 Basics(s.Registrar(), {FrameClock("Tick"), Once("Junk")});
                 s.Registrar().AddSystem<&Fail>(In("FailJunk", "Junk"));
             };
         },
         [](O::Product& p) {
             static_cast<void>(p.ReplaceWorld("Main", O::ReplaceRequest{{}, {"Junk"}, O::ReplaceOrder::DropOldFirst}));
         }},
        {"O-E error", [](Units&, O::ProductDescription& d) { d.worlds.push_back(d.worlds[0]); }, {}},
        {"kernel startup error",
         [](Units&, O::ProductDescription& d) {
             d.worlds[0].build = [](O::WorldSetup& s) {
                 Basics(s.Registrar(), {FrameClock("Tick")});
                 s.Registrar().AddSystem<&Mark<'x'>>(In("Nowhere", "NoSuchSchedule"));
             };
         },
         {}},
        {"Configure report", [](Units& u, O::ProductDescription&) {
             u.Module("reporter", {.configure = [](O::ProductSetup& p) { p.Report("bad-content", "the catalog is broken"); }},
                      {"store"});
         },
         {}},
        {"prepare failure (O-E7)",
         [](Units&, O::ProductDescription& d) {
             d.worlds[0].prepare.push_back([](K::World&) -> PResult {
                 return ::GYO::Engine::Base::Err(O::OrchestrationError(O::OrchestrationErrorCode::PrepareFailed, "no"));
             });
         },
         {}},
    };
    for (const Way& way : ways) {
        CAPTURE(way.name);
        int calls = 0;
        bool storeThere = false;
        {
            Units units;
            units.Module("store", {.configure = [&calls, &storeThere](O::ProductSetup& p) {
                             p.Buffer().Create<Store>();
                             O::ModuleBuffer* buffer = &p.Buffer();
                             p.OnDestroy([&calls, &storeThere, buffer] {
                                 ++calls;
                                 storeThere = buffer->Find<Store>() != nullptr;
                             });
                         }});
            O::ProductDescription d = Game();
            if (way.setup) way.setup(units, d);
            auto built = O::Product::Build(std::move(d), units.Take());
            if (built && way.afterBuild) way.afterBuild(**built);
        }
        CHECK_MESSAGE(calls == 1, "[or-pbuf-ondestroy-once] " << way.name << ": " << calls);
        CHECK_MESSAGE(storeThere, "[or-pbuf-ondestroy-buffer] " << way.name << ": the Store was gone");
    }
    SUBCASE("Configure throws") {
        int calls = 0;
        Units units;
        units.Module("store", {.configure = [&calls](O::ProductSetup& p) { p.OnDestroy([&calls] { ++calls; }); }});
        units.Module("thrower", {.configure = [](O::ProductSetup&) { throw std::runtime_error("configure throws"); }},
                     {"store"});
        CHECK_THROWS_AS(static_cast<void>(O::Product::Build(Game(), units.Take())), std::runtime_error);
        CHECK_MESSAGE(calls == 1, "[or-pbuf-ondestroy-once] Configure throws: " << calls);
    }
}

TEST_CASE("product-buffer: P-7 a Configure report builds no World; the other units' Configure run; a dependant reports itself") {
    Log().clear();
    Units units;
    units.Module("asset", {.configure = [](O::ProductSetup& p) { p.Report("asset-catalog", "content.json is broken"); }});
    units.Module("image", {.configure = [](O::ProductSetup& p) {
                     // Its dependency reported and made nothing: it reports and returns, it does not assert.
                     if (p.Buffer().Find<Store>() == nullptr) p.Report("image-no-loader-registry", "asset made no registry");
                 }},
                 {"asset"});
    O::ProductDescription d = Game();
    d.configure = [](O::ProductSetup& p) { p.Report("product-bad", "the product's own setting is broken"); };
    auto built = O::Product::Build(std::move(d), units.Take());
    REQUIRE_FALSE(static_cast<bool>(built));
    const O::ProductStartupReport& report = built.error();
    REQUIRE_MESSAGE(report.unitErrors.size() == 3, "[or-pbuf-report-all] " << report.detail);
    CHECK_MESSAGE((report.unitErrors[0].unit == "asset" && report.unitErrors[0].code == "asset-catalog" &&
                   report.unitErrors[0].world.empty()),
                  "[or-pbuf-report-fields] " << report.detail);
    CHECK_MESSAGE((report.unitErrors[1].unit == "image" && report.unitErrors[1].code == "image-no-loader-registry"),
                  "[or-pbuf-report] " << report.detail);
    CHECK_MESSAGE((report.unitErrors[2].unit == "game" && report.unitErrors[2].code == "product-bad"),
                  "[or-pbuf-report] the product's own report names the product: " << report.detail);
    CHECK_MESSAGE(!Logged("build asset@Main"), "[or-pbuf-report-no-build] a World's Build ran: " << Joined());
    REQUIRE(report.worlds.size() == 1);
    CHECK_MESSAGE(report.worlds[0].notChecked, "[or-pbuf-report-not-checked] the World is not marked not checked");
}

TEST_CASE("product-buffer: P-8 a World Resource pointing at a Product buffer object is still valid when the World goes") {
    // The object in the Product's buffer logs "~store"; the World's Resource logs "points" when the World
    // destroys it. The Resource must go first.
    struct Points {
        Probe probe{"points"};
        const Probe* store = nullptr;
    };
    Log().clear();
    Units units;
    units.Module("store", {.configure = [](O::ProductSetup& p) { p.Buffer().Create<Probe>("store"); },
                           .build = [](O::WorldSetup& s) {
                               s.Registrar().Resource<Points>({.name = "Points"});
                               const Probe* store = s.ProductBuffer().Find<Probe>();
                               s.Prepare([store](K::World& w) -> PResult {
                                   Points points;
                                   points.store = store;
                                   w.SetResource(std::move(points));
                                   return {};
                               });
                           }});
    std::size_t from = 0;
    {
        auto built = O::Product::Build(Game(), units.Take());
        ORCH_REQUIRE_BUILT(built);
        from = Log().size();
    }
    const std::string order = LogSince(from);
    CHECK_MESSAGE(order.find("~points ~store") != std::string::npos, "[or-pbuf-outlives] " << order);
}

namespace {

// A service with its own worker thread and its own lock (exchange declaration 5.1, apply point):
// the World never locks; the apply System takes the results in one go.
class DecodeService {
public:
    DecodeService() : worker_([this] { Work(); }) {}
    ~DecodeService() { Stop(); }
    void Request(int job) {
        std::lock_guard lock(mutex_);
        requests_.push_back(job);
        wake_.notify_all();
    }
    // Waits until n results are done (the test's latch; the World never calls it).
    void WaitDone(std::size_t n) {
        std::unique_lock lock(mutex_);
        wake_.wait(lock, [&] { return doneCount_ >= n; });
    }
    std::vector<int> Take() {
        std::lock_guard lock(mutex_);
        std::vector<int> out(results_.begin(), results_.end());
        results_.clear();
        return out;
    }
    // Stops the worker and joins it: nothing touches the service after this returns.
    void Stop() {
        {
            std::lock_guard lock(mutex_);
            if (stopped_) return;
            stopped_ = true;
            wake_.notify_all();
        }
        worker_.join();
    }

private:
    void Work() {
        std::unique_lock lock(mutex_);
        for (;;) {
            wake_.wait(lock, [&] { return stopped_ || !requests_.empty(); });
            if (stopped_ && requests_.empty()) return;
            const int job = requests_.front();
            requests_.pop_front();
            results_.push_back(job * 10);
            ++doneCount_;
            wake_.notify_all();
        }
    }
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<int> requests_;
    std::deque<int> results_;
    std::size_t doneCount_ = 0;
    bool stopped_ = false;
    std::thread worker_;
};

struct ServiceRef {
    DecodeService* service = nullptr;
};
struct Decoded {
    std::vector<int> values;
};

K::SystemResult ApplyDecoded(K::Res<ServiceRef> ref, K::ResMut<Decoded> decoded) {
    for (int value : ref->service->Take()) decoded->values.push_back(value);
    return {};
}

} // namespace

TEST_CASE("product-buffer: P-9 a service with its own thread: put by a unit, applied by a System, stopped by OnDestroy") {
    DecodeService* service = nullptr;
    bool stoppedInCallback = false;
    Units units;
    units.Module("decode", {.configure = [&service, &stoppedInCallback](O::ProductSetup& p) {
                      DecodeService& made = p.Buffer().Create<DecodeService>();
                      service = &made;
                      p.OnDestroy([&made, &stoppedInCallback] {
                          made.Stop();
                          stoppedInCallback = true;
                      });
                  },
                  .build = [](O::WorldSetup& s) {
                      s.Registrar().Resource<ServiceRef>({.name = "ServiceRef"});
                      s.Registrar().Resource<Decoded>({.name = "Decoded"});
                      s.Registrar().AddSystem<&ApplyDecoded>(In("ApplyDecoded", "Tick"));
                      DecodeService* made = s.ProductBuffer().Find<DecodeService>();
                      s.Prepare([made](K::World& w) -> PResult {
                          w.SetResource(ServiceRef{made});
                          w.SetResource(Decoded{});
                          return {};
                      });
                  }});
    {
        auto built = O::Product::Build(Game(), units.Take());
        ORCH_REQUIRE_BUILT(built);
        O::Product& p = **built;
        REQUIRE(service != nullptr);
        service->Request(1);
        service->Request(2);
        service->WaitDone(2);
        REQUIRE(static_cast<bool>(p.RunFrame()));
        const Decoded* decoded = p.World("Main").GetWorld().Resource<Decoded>();
        REQUIRE(decoded != nullptr);
        CHECK_MESSAGE((decoded->values == std::vector<int>{10, 20}), "[or-pbuf-service] the apply System took no results");
    }
    CHECK_MESSAGE(stoppedInCallback, "[or-pbuf-service] OnDestroy did not run");
}
