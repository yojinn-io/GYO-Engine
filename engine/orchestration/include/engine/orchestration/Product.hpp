#pragma once

#include "engine/base/Result.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/WorldExecutor.hpp"
#include "engine/orchestration/OrchestrationError.hpp"
#include "engine/orchestration/RegisteredUnits.hpp"
#include "engine/orchestration/Setup.hpp"

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
// functions, World replacement and dynamic insertion.
//
// Everything happens on the calling thread (CCR-5): building, running,
// replacing, inserting and destroying the Product.

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
    // The initial frame list (schedule names); units insert into it.
    std::vector<std::string> frame;
    // The product's prepare functions, after the units' ones.
    std::vector<PrepareFn> prepare;
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

class Product {
public:
    // Builds the Product (section 3.4): checks, every unit's Configure, then
    // for each World Build, BuildPlans, Finish, Check, the list checks; with
    // no error so far, builds the Worlds and runs their prepare functions.
    static ::Engine::Base::Result<std::unique_ptr<Product>, ProductStartupReport> Build(ProductDescription description,
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
    ::Engine::Base::Result<void, OrchestrationError> RunGameInit();
    // Runs every World's frame list, in World order; game initialization first when it has not run.
    // A schedule failing part way leaves the Worlds before it advanced (the Product has ended then).
    ::Engine::Base::Result<void, OrchestrationError> RunFrame();
    // Runs one schedule on one World; game initialization first when it has not run.
    ::Engine::Base::Result<void, OrchestrationError> RunSchedule(std::string_view world, std::string_view schedule);

    // The World's executor, valid until this name is next replaced. GYO_ASSERT: no World
    // of that name, or its place is empty.
    [[nodiscard]] Kernel::WorldExecutor& World(std::string_view name);
    [[nodiscard]] bool HasWorld(std::string_view name) const noexcept;
    // Sets the error handler of the World's executor, kept across replacements.
    void SetErrorHandler(std::string_view world, std::function<void(const Kernel::KernelError&)> handler);

    // Replaces a healthy World (section 3.9): registers it again from the
    // description, carries the marked Resources over, runs the prepare functions
    // and the beforeSwap schedules. Game initialization does not run again. It
    // does not mend a broken World: with one, it is refused like a Run.
    ::Engine::Base::Result<void, OrchestrationError> ReplaceWorld(std::string_view name, ReplaceRequest request);

    // A dynamic insert (section 3.10): the kernel's two steps on the World's
    // Registry; a refusal or a failed BuildPlans logs one warning
    // (Engine::Base::Log) and returns the error, the World goes on. No unit's
    // Finish or Check runs again.
    ::Engine::Base::Result<void, OrchestrationError> InsertSystems(std::string_view world, std::string owner,
                                                                   InsertFn build);

    [[nodiscard]] bool GameInitRan() const noexcept { return gameInitRan_; }
    // The World that ended the Product, or nullptr while every World is healthy.
    [[nodiscard]] const WorldFailure* Failure() const noexcept { return failure_ ? &*failure_ : nullptr; }
    [[nodiscard]] const std::vector<std::string>& GameInitList() const noexcept { return gameInit_; }

private:
    struct UnitSlot {
        std::string row;
        Detail::RegistrationUnit* unit = nullptr;
        std::uint32_t seq = 0;
    };
    struct WorldSlot {
        WorldDescription description;
        // Indices into units_, in unit order.
        std::vector<std::size_t> units;
        std::unique_ptr<Kernel::Registry> registry;
        std::unique_ptr<Kernel::WorldExecutor> executor;
        std::vector<std::string> frame;
        std::vector<PrepareFn> unitPrepares;
        std::vector<Detail::CarryEntry> carries;
        std::function<void(const Kernel::KernelError&)> handler;
        // The last schedule the Product ran with success on this World (WorldFailure::lastSchedule).
        std::string lastSchedule;
        std::uint32_t nextInsertSeq = 0;
    };
    // One World's registration, built but not yet a World.
    struct Registration {
        std::unique_ptr<Kernel::Registry> registry;
        std::vector<std::string> frame;
        std::vector<PrepareFn> unitPrepares;
        std::vector<Detail::CarryEntry> carries;
    };

    Product() = default;
    // Builds one World's registration (Build, BuildPlans, Finish, Check, list checks).
    // Returns nullopt-equivalent (registry null) when something failed; errors go to the sinks.
    Registration Register(const WorldSlot& slot, bool firstWorld, WorldStartupResult& result,
                          std::vector<OrchestrationStartupError>& errors, std::vector<UnitReportedError>& unitErrors);
    ::Engine::Base::Result<void, OrchestrationError> Prepare(WorldSlot& slot, Kernel::World& world,
                                                             const std::vector<PrepareFn>& unitPrepares,
                                                             const PrepareFn* extra);
    ::Engine::Base::Result<void, OrchestrationError> RunOn(WorldSlot& slot, std::string_view schedule,
                                                           std::string_view entry);
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
    // Units first so they outlive every Registry and World (members are destroyed in reverse).
    RegisteredUnits owned_;
    std::vector<UnitSlot> units_;
    std::function<void(ProductSetup&)> configure_;
    std::vector<WorldSlot> worlds_;
    std::vector<std::string> gameInit_;
    Runner runner_;
    bool gameInitRan_ = false;
    bool ran_ = false;
    std::optional<WorldFailure> failure_;
};

} // namespace GYO::Engine::Orchestration
