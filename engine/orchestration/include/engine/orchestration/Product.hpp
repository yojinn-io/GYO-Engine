#pragma once

#include "engine/base/Result.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/WorldExecutor.hpp"
#include "engine/orchestration/OrchestrationError.hpp"
#include "engine/orchestration/RegisteredUnits.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Product (declaration docs/architecture/target/v2/orchestration-skeleton-declaration.zh-Hant.md,
// sections 3.4 to 3.12): a group of Worlds, each with its own kernel Registry,
// built from a description and the registration units the product carries;
// the game initialization list, each World's frame list, the runner, prepare
// functions, World replacement and dynamic insertion; the Product's buffer,
// options, OnDestroy and the exchange functions
// (docs/architecture/target/v2/exchange-declaration.zh-Hant.md, sections 5.1, 5.2).
//
// Everything happens on the calling thread (CCR-5): building, running,
// replacing, inserting and destroying the Product. The one exception is a World lent to another thread
// (O-13, Product::Lend): while lent only the thread holding its WorldLease touches it.

namespace GYO::Engine::Orchestration {

struct WorldDescription {
    // Unique in the Product (O-E6).
    std::string name;
    // Kernel::WorldOptions::dynamicInsert of the World (L5).
    bool dynamicInsert = true;
    // Register row names of the units this World uses; empty: every unit the Product carries.
    std::vector<std::string> units;
    // Options the Modules read; the orchestration layer hands them over without reading them.
    OptionBag options;
    // The product's own registration (section 3.3), after every unit's Build of this World.
    std::function<void(WorldSetup&)> build;
    // The initial frame list (schedule names); units insert into it. With the
    // default skeleton it starts with DefaultSchedules::frame unless it names it.
    std::vector<std::string> frame;
    // The schedules the orchestration layer declares before any unit's Build
    // (ScheduleSkeleton.hpp); Off: none, the author declares its own.
    ScheduleSkeleton skeleton = ScheduleSkeleton::Default;
    // The product's prepare functions, after the units' ones.
    std::vector<PrepareFn> prepare;
    // The World this one extracts from (O-12, DI-KS 35; v8-arch-gyo2-sdl-extract): a World described
    // before this one; empty: none. Its Registry is this World's extract source, and every extract
    // schedule this World runs (frame list, RunSchedule, beforeSwap) runs through
    // Kernel::WorldExecutor::RunExtract with that World. A name that is not described before this
    // World, or an extract schedule in the frame list of a World without one, is O-E9.
    std::string extractFrom;
};

struct ProductDescription {
    // The owner name of the product's own registration.
    std::string name;
    // The first is "the first World": game initialization runs on it.
    std::vector<WorldDescription> worlds;
    // The initial game initialization list; units insert into it.
    std::vector<std::string> gameInit;
    // The product's own Configure, after every unit's.
    std::function<void(ProductSetup&)> configure;
    // Options for the Modules at Product level (exchange declaration 5.2): there from the first unit's
    // Configure on (ProductSetup::Options). What the product or a test gives a unit (an executor, a
    // content root) goes here, in a small holder struct when it is an interface. Never read here (O-2).
    OptionBag options;
};

enum class ReplaceOrder : unsigned char {
    // Build the new World first; swap only when everything succeeded (the old one goes on otherwise).
    BuildNewFirst,
    // Destroy the old World first (two do not fit in memory); a failure leaves the
    // place empty and ends the Product. Validate the save, assets or snapshot
    // before asking for it: the orchestration layer does not read their content.
    DropOldFirst,
};

struct ReplaceRequest {
    // This replacement's prepare function, after the units' and the description's.
    PrepareFn extra;
    // Schedules run on the new World before it is swapped in (loading a save).
    std::vector<std::string> beforeSwap;
    ReplaceOrder order = ReplaceOrder::BuildNewFirst;
};

// A dynamic insert's registration (section 3.10).
using InsertFn = std::function<void(Kernel::Registrar&)>;

class Product;

namespace Detail {
// What a lease and its Product share (O-13 (4)): set once by the lending thread when the World breaks in a
// round (failure written first, then broken with release); read by the Product's thread after an acquire.
struct LeaseShared {
    std::atomic<bool> broken{false};
    std::optional<WorldFailure> failure;
};
} // namespace Detail

// A World lent to another thread (O-13 (2); live-frame-sim-thread declaration 4.2): moved to that thread, which
// alone runs it (RunRound) and reads or writes it between rounds (World). Product::Return takes it back on the
// Product's thread; a Product is not destroyed with a World lent (GYO_ASSERT).
class WorldLease {
public:
    WorldLease() = default;
    WorldLease(WorldLease&& other) noexcept;
    WorldLease& operator=(WorldLease&& other) noexcept;
    WorldLease(const WorldLease&) = delete;
    WorldLease& operator=(const WorldLease&) = delete;
    ~WorldLease();

    [[nodiscard]] bool Valid() const noexcept { return product_ != nullptr; }
    // One round of the World, as RunFrame runs it for one World: the generation's step, the exchange functions'
    // "in", the frame list, the "out" (exchange declaration W-10). The World breaking ends the Product (O-11):
    // the error carries the WorldFailure, and the Product's next entry point returns WorldInterrupted. After a
    // break every call returns that error again. GYO_ASSERT: not Valid.
    Base::Result<void, OrchestrationError> RunRound();
    // The World, for the holding thread between rounds (the runner's direct read and write, O-12).
    [[nodiscard]] Kernel::World& World();
    [[nodiscard]] const std::string& Name() const;

private:
    friend class Product;
    Product* product_ = nullptr;
    std::size_t slot_ = 0;
    std::shared_ptr<Detail::LeaseShared> shared_;
};

class Product {
public:
    // Builds the Product (section 3.4): checks, every unit's Configure, then
    // for each World Build, BuildPlans, Finish, Check, the list checks; with
    // no error so far, builds the Worlds and runs their prepare functions.
    static Base::Result<std::unique_ptr<Product>, ProductStartupReport> Build(ProductDescription description,
                                                                                       RegisteredUnits units);
    ~Product();
    Product(const Product&) = delete;
    Product& operator=(const Product&) = delete;

    // Calls the runner once (the default one runs one frame). GYO_ASSERT: a second call.
    RunnerExit Run();

    // The entry points for the runner and the Product's holder, between two Runs.
    // One broken World (interrupted, or its place empty) ends the Product
    // (v8-arch-gyo2-broken-world-ends-product): each entry point first checks
    // every World and, with one broken, runs nothing, changes nothing and
    // returns WorldInterrupted or WorldMissing with the WorldFailure. The runner
    // decides what follows (close the program, or build another Product).
    // Runs the game initialization list on the first World. GYO_ASSERT: it ran already.
    Base::Result<void, OrchestrationError> RunGameInit();
    // Runs every World's frame list, in World order; game initialization first when it has not run.
    // A schedule failing part way leaves the Worlds before it advanced (the Product has ended then).
    Base::Result<void, OrchestrationError> RunFrame();
    // Runs one schedule on one World; game initialization first when it has not run.
    Base::Result<void, OrchestrationError> RunSchedule(std::string_view world, std::string_view schedule);

    // The World's executor, valid until this name is next replaced. GYO_ASSERT: no World
    // of that name, or its place is empty.
    [[nodiscard]] Kernel::WorldExecutor& World(std::string_view name);
    [[nodiscard]] bool HasWorld(std::string_view name) const noexcept;
    // The Product's buffer (exchange declaration 5.2), between two entry points: what the units put
    // there for the Product's whole life. It outlives every World.
    [[nodiscard]] ModuleBuffer& Buffer() noexcept { return *buffer_; }
    // Whether that World is generating (world-gen declaration 5.1, O-14): read from its WorldGeneration, the one
    // place the state lives. GYO_ASSERT: no World of that name, or its place is empty.
    [[nodiscard]] bool Generating(std::string_view name) const;

    // Replaces a healthy World (section 3.9): registers it again from the
    // description, carries the marked Resources over, runs the prepare functions
    // and the beforeSwap schedules. Game initialization does not run again. It
    // does not mend a broken World: with one, it is refused like a Run.
    Base::Result<void, OrchestrationError> ReplaceWorld(std::string_view name, ReplaceRequest request);

    // A dynamic insert (section 3.10): the kernel's two steps on the World's
    // Registry; a refusal or a failed BuildPlans returns the error and the World
    // goes on. The orchestration layer logs nothing (O-9, O-11; error levels D2).
    // No unit's Finish or Check runs again.
    Base::Result<void, OrchestrationError> InsertSystems(std::string_view world, std::string owner,
                                                                   InsertFn build);

    // Lends a whole World to another thread (O-13): until Return, RunFrame skips it, RunSchedule, ReplaceWorld
    // and InsertSystems naming it return WorldLent, World(name) and Generating(name) on it are GYO_ASSERTs, and only
    // the lease's holder touches it. LendRefused: game initialization has not run, it is lent already, or it is an
    // Extract source or reader. A broken Product is refused like a Run.
    Base::Result<WorldLease, OrchestrationError> Lend(std::string_view world);
    // Takes a lent World back (after the holding thread stopped using it; the caller makes that happen-before,
    // e.g. by joining the thread). A World that broke while lent stays broken: the Product has ended.
    // GYO_ASSERT: the lease is not this Product's.
    void Return(WorldLease&& lease);

    [[nodiscard]] bool GameInitRan() const noexcept { return gameInitRan_; }
    // The World that ended the Product, or nullptr while every World is healthy.
    [[nodiscard]] const WorldFailure* Failure() const noexcept { return failure_ ? &*failure_ : nullptr; }
    [[nodiscard]] const std::vector<std::string>& GameInitList() const noexcept { return gameInit_; }

private:
    // The entry points' steps; each public entry point runs its steps inside one trace span (trace
    // declaration D6) and marks it when they return an error.
    Base::Result<void, OrchestrationError> RunGameInitSteps();
    Base::Result<void, OrchestrationError> RunFrameSteps();
    Base::Result<void, OrchestrationError> RunScheduleSteps(std::string_view world, std::string_view schedule);
    Base::Result<void, OrchestrationError> ReplaceWorldSteps(std::string_view name, ReplaceRequest request);
    Base::Result<void, OrchestrationError> InsertSystemsSteps(std::string_view world, std::string owner, InsertFn build);
    // Calls one side of a World's exchange functions in order; the first error stops it (ExchangeFailed).
    Base::Result<void, OrchestrationError> Exchange(std::vector<Detail::ExchangeEntry>& entries, Kernel::World& world,
                                                    std::string_view side);

    struct UnitSlot {
        std::string row;
        Detail::RegistrationUnit* unit = nullptr;
        std::uint32_t seq = 0;
    };
    struct WorldSlot {
        WorldDescription description;
        // Lent to another thread (O-13): set and cleared on the Product's thread, before and after the lend.
        std::shared_ptr<Detail::LeaseShared> lease;
        // Indices into units_, in unit order.
        std::vector<std::size_t> units;
        std::unique_ptr<Kernel::Registry> registry;
        std::unique_ptr<Kernel::WorldExecutor> executor;
        std::vector<std::string> frame;
        std::vector<Detail::CarryEntry> carries;
        // The exchange functions of this World's build: kept until the World is replaced or destroyed.
        std::vector<Detail::ExchangeEntry> exchangeIn;
        std::vector<Detail::ExchangeEntry> exchangeOut;
        // The last schedule the Product ran with success on this World (WorldFailure::lastSchedule).
        std::string lastSchedule;
        std::uint32_t nextInsertSeq = 0;
        // A DropOldFirst replacement dropped the old World and has not swapped yet (error
        // levels P12: an empty place is told apart from a broken one by this, not by guessing).
        bool replacing = false;
    };
    // One World's registration, built but not yet a World. Its prepare functions and ModuleBuffer live until the
    // prepare functions have run (O-2): the World's slot keeps neither.
    struct Registration {
        std::unique_ptr<Kernel::Registry> registry;
        std::vector<std::string> frame;
        std::vector<PrepareFn> unitPrepares;
        std::vector<Detail::CarryEntry> carries;
        std::vector<Detail::ExchangeEntry> exchangeIn;
        std::vector<Detail::ExchangeEntry> exchangeOut;
        std::shared_ptr<ModuleBuffer> buffer;
    };

    Product() = default;
    // Builds one World's registration (Build, BuildPlans, Finish, Check, list checks).
    // Returns nullopt-equivalent (registry null) when something failed; errors go to the sinks.
    // extractSource: the Registry of the World it extracts from, set before BuildPlans (O-12).
    Registration Register(const WorldSlot& slot, bool firstWorld, const Kernel::Registry* extractSource,
                          WorldStartupResult& result, std::vector<OrchestrationStartupError>& errors,
                          std::vector<UnitReportedError>& unitErrors);
    // The slot index of the World that slot extracts from, or worlds_.size() when it names none
    // (or the World it names was not taken in, O-E6).
    std::size_t ExtractSourceIndex(const WorldSlot& slot) const noexcept;
    // Runs one schedule on executor: through RunExtract with the slot's extract source when the
    // schedule is an extract schedule (DI-KS 35), else Run. GYO_ASSERT: an extract schedule and no source.
    Base::Result<void, Kernel::KernelError> RunWith(Kernel::WorldExecutor& executor, const WorldSlot& slot,
                                                    std::string_view schedule);
    Base::Result<void, OrchestrationError> Prepare(WorldSlot& slot, Kernel::World& world,
                                                             const std::vector<PrepareFn>& unitPrepares,
                                                             const PrepareFn* extra);
    // lease: the World is lent and runs on the holder's thread; its failure goes to the lease, not the Product.
    Base::Result<void, OrchestrationError> RunOn(WorldSlot& slot, std::string_view schedule,
                                                           std::string_view entry, Detail::LeaseShared* lease = nullptr);
    // One World's round of a RunFrame (world-gen declaration 5.1): the generation's step before (a generating World
    // is written {true, false}), the frame list, the generation's judgement after (nobody held: {false, false}).
    // The entry point that will run one World's round on its own (the exchange declaration's lent World) shares it.
    Base::Result<void, OrchestrationError> RunRound(WorldSlot& slot, Detail::LeaseShared* lease = nullptr);
    friend class WorldLease;
    // The refusal of an entry point while a World is broken (nullopt: every World is healthy).
    // A World found interrupted without a recorded failure (run outside the Product) is recorded here.
    std::optional<OrchestrationError> Ended();
    // Records the failure that ends the Product (it is not logged: whether and how to log
    // is the logging mechanism's, not the orchestration layer's).
    const WorldFailure& Break(WorldFailure failure);
    OrchestrationError EndedError() const;
    WorldSlot* Find(std::string_view name) noexcept;
    const WorldSlot* Find(std::string_view name) const noexcept;
    void RequireNotRunning() const;
    std::unique_ptr<Kernel::WorldExecutor> MakeExecutor(WorldSlot& slot, const Kernel::Registry& registry);

    std::string name_;
    // ~Product destroys these in an explicit order (exchange declaration 5.2 point 5): the Worlds
    // (reverse description order), the OnDestroy callbacks, the runners, the Product's buffer, the
    // descriptions and options, the units. The declaration order below is not relied on.
    RegisteredUnits owned_;
    std::vector<UnitSlot> units_;
    std::function<void(ProductSetup&)> configure_;
    OptionBag options_;
    std::unique_ptr<ModuleBuffer> buffer_;
    std::vector<std::function<void()>> onDestroy_;
    std::vector<WorldSlot> worlds_;
    std::vector<std::string> gameInit_;
    // Every runner installed (one at most is valid, O-E4); the runner is the only one. Kept until ~Product's runner step.
    std::vector<ProductSetup::Installed> runners_;
    bool gameInitRan_ = false;
    bool ran_ = false;
    std::optional<WorldFailure> failure_;
};

} // namespace GYO::Engine::Orchestration
