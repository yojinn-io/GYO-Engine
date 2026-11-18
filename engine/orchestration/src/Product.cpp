// Product: building, running, replacing and inserting into a group of Worlds
// (declaration docs/architecture/target/v2/orchestration-skeleton-declaration.zh-Hant.md,
// sections 3.4 to 3.12).

#include "engine/orchestration/Product.hpp"

#include "engine/base/Assert.hpp"
#include "engine/base/Log.hpp"

#include <algorithm>
#include <set>
#include <utility>

namespace GYO::Engine::Orchestration {

namespace {

const RegisterRow* FindRow(const std::vector<RegisterRow>& rows, std::string_view name) {
    for (const RegisterRow& row : rows) {
        if (row.name == name) return &row;
    }
    return nullptr;
}

bool HasSchedule(const Kernel::FrozenRegistry& registry, std::string_view name) {
    for (const Kernel::ScheduleInfo& schedule : registry.Schedules()) {
        if (schedule.options.name == name) return true;
    }
    return false;
}

std::string Quoted(std::string_view text) {
    return "'" + std::string(text) + "'";
}

void Warn(const std::string& text) {
    ::Engine::Base::Log(::Engine::Base::LogLevel::Warning, "orchestration", text);
}

OrchestrationError FromKernel(OrchestrationErrorCode code, std::string message, Kernel::KernelError error) {
    OrchestrationError result(code, std::move(message), error.Text());
    result.kernel.push_back(std::move(error));
    return result;
}

} // namespace

Product::~Product() = default;

::Engine::Base::Result<std::unique_ptr<Product>, ProductStartupReport> Product::Build(ProductDescription description,
                                                                                    RegisteredUnits units) {
    std::unique_ptr<Product> product(new Product());
    Product& self = *product;
    self.name_ = std::move(description.name);
    self.configure_ = std::move(description.configure);
    self.owned_ = std::move(units);

    std::vector<OrchestrationStartupError> errors;
    std::vector<UnitReportedError> unitErrors;
    std::vector<WorldStartupResult> results;
    const std::vector<RegisterRow>& rows = self.owned_.rows;

    // Step 1: the units against the register (O-E1, O-E2). A second instance
    // of one row and an instance of a row that is missing or library-only are
    // not used; every other unit goes on (as the kernel's Finish did).
    std::set<std::string> seenRows;
    std::uint32_t seq = 0;
    auto take = [&](const std::string& rowName, Detail::RegistrationUnit* unit, UnitKind kind) {
        const RegisterRow* row = FindRow(rows, rowName);
        if (row == nullptr || row->kind != kind || row->libraryOnly) {
            errors.push_back({OrchestrationCode::OE2,
                              "unit instance for " + Quoted(rowName) +
                                  (row == nullptr            ? " names no register row"
                                   : row->kind != kind       ? " is in the other register file"
                                                             : " names a library-only row"),
                              {rowName}});
            return;
        }
        if (!seenRows.insert(rowName).second) {
            errors.push_back({OrchestrationCode::OE1, "register row " + Quoted(rowName) + " has two unit instances; the second is not used",
                              {rowName}});
            return;
        }
        self.units_.push_back(UnitSlot{rowName, unit, seq++});
    };
    for (UnitEntry<Module>& entry : self.owned_.modules) take(entry.row, entry.unit.get(), UnitKind::Module);
    for (UnitEntry<Plugin>& entry : self.owned_.plugins) take(entry.row, entry.unit.get(), UnitKind::Plugin);

    // Step 1: the World descriptions (O-E6) and every World's dependencies (O-E3).
    if (description.worlds.empty()) errors.push_back({OrchestrationCode::OE6, "the Product describes no World", {self.name_}});
    std::set<std::string> worldNames;
    for (WorldDescription& world : description.worlds) {
        if (!worldNames.insert(world.name).second) {
            errors.push_back({OrchestrationCode::OE6, "two Worlds are named " + Quoted(world.name), {world.name}});
            continue;
        }
        bool usable = true;
        for (const std::string& wanted : world.units) {
            const bool carried = std::any_of(self.units_.begin(), self.units_.end(),
                                             [&](const UnitSlot& slot) { return slot.row == wanted; });
            if (!carried) {
                errors.push_back({OrchestrationCode::OE6,
                                  "World " + Quoted(world.name) + " uses unit " + Quoted(wanted) + ", which the Product does not carry",
                                  {world.name, wanted}});
                usable = false;
            }
        }
        if (!usable) continue;
        WorldSlot slot;
        for (std::size_t i = 0; i < self.units_.size(); ++i) {
            if (world.units.empty() ||
                std::find(world.units.begin(), world.units.end(), self.units_[i].row) != world.units.end()) {
                slot.units.push_back(i);
            }
        }
        for (std::size_t k = 0; k < slot.units.size(); ++k) {
            const UnitSlot& unit = self.units_[slot.units[k]];
            const RegisterRow* row = FindRow(rows, unit.row);
            std::vector<std::string> deps = row->depends;
            deps.insert(deps.end(), row->privateDepends.begin(), row->privateDepends.end());
            for (const std::string& dep : deps) {
                const RegisterRow* depRow = FindRow(rows, dep);
                std::string problem;
                if (depRow == nullptr) {
                    problem = "is in no register file";
                } else if (row->kind == UnitKind::Module && depRow->kind == UnitKind::Plugin) {
                    problem = "is a Plugin, and a Module does not depend on a Plugin";
                } else if (!depRow->libraryOnly) {
                    auto at = [&](std::size_t from, std::size_t to) {
                        for (std::size_t j = from; j < to; ++j) {
                            if (self.units_[slot.units[j]].row == dep) return true;
                        }
                        return false;
                    };
                    if (at(k + 1, slot.units.size())) {
                        problem = "comes later in World " + Quoted(world.name) + " (the order is not changed)";
                    } else if (!at(0, k)) {
                        problem = "is not used by World " + Quoted(world.name);
                    }
                }
                if (!problem.empty()) {
                    errors.push_back({OrchestrationCode::OE3, Quoted(unit.row) + " depends on " + Quoted(dep) + ", which " + problem,
                                      {unit.row, dep, world.name}});
                }
            }
        }
        slot.description = std::move(world);
        self.worlds_.push_back(std::move(slot));
    }

    // Step 2: Configure, every unit in order, then the product's own.
    ScheduleList gameInit(std::move(description.gameInit));
    std::vector<ProductSetup::Installed> runners;
    for (UnitSlot& unit : self.units_) {
        ProductSetup setup(self.name_, unit.row, gameInit);
        unit.unit->Configure(setup);
        for (auto& installed : setup.runners_) runners.push_back(std::move(installed));
    }
    if (self.configure_) {
        ProductSetup setup(self.name_, self.name_, gameInit);
        self.configure_(setup);
        for (auto& installed : setup.runners_) runners.push_back(std::move(installed));
    }
    if (runners.size() > 1) {
        std::vector<std::string> installers;
        for (const auto& installed : runners) installers.push_back(installed.installer);
        errors.push_back({OrchestrationCode::OE4, std::to_string(runners.size()) + " runners are installed; a Product has one",
                          installers});
    } else if (runners.size() == 1) {
        self.runner_ = std::move(runners.front().runner);
    }
    for (const std::string& anchor : gameInit.missingAnchors_) {
        errors.push_back({OrchestrationCode::OE5, "game initialization list: no anchor " + Quoted(anchor), {anchor}});
    }
    self.gameInit_ = gameInit.names_;

    // Step 3 and 4: every World's registration and list checks.
    std::vector<Registration> registrations;
    for (std::size_t i = 0; i < self.worlds_.size(); ++i) {
        WorldStartupResult result{self.worlds_[i].description.name, std::nullopt, false};
        registrations.push_back(self.Register(self.worlds_[i], i == 0, result, errors, unitErrors));
        results.push_back(std::move(result));
    }

    // Step 5: any error so far, and no World is built.
    const bool kernelFailed =
        std::any_of(results.begin(), results.end(), [](const WorldStartupResult& r) { return r.kernel.has_value(); });
    if (!errors.empty() || !unitErrors.empty() || kernelFailed) {
        return ::Engine::Base::Err(ProductStartupReport(std::move(results), std::move(errors), std::move(unitErrors)));
    }

    // Step 6: build the Worlds and run their prepare functions (O-E7 alone).
    for (std::size_t i = 0; i < self.worlds_.size(); ++i) {
        WorldSlot& slot = self.worlds_[i];
        Registration& registration = registrations[i];
        slot.registry = std::move(registration.registry);
        slot.frame = std::move(registration.frame);
        slot.unitPrepares = std::move(registration.unitPrepares);
        slot.carries = std::move(registration.carries);
        slot.executor = self.MakeExecutor(slot, *slot.registry);
    }
    for (WorldSlot& slot : self.worlds_) {
        auto prepared = self.Prepare(slot, slot.executor->GetWorld(), slot.unitPrepares, nullptr);
        if (!prepared) {
            const OrchestrationError& error = prepared.error();
            errors.push_back({OrchestrationCode::OE7,
                              "World " + Quoted(slot.description.name) + ": a prepare function failed" +
                                  (error.reporterCode.empty() ? std::string() : " (" + error.reporterCode + ")") + ": " +
                                  error.message,
                              {slot.description.name}});
        }
    }
    if (!errors.empty()) return ::Engine::Base::Err(ProductStartupReport({}, std::move(errors), {}));
    return product;
}

Product::Registration Product::Register(const WorldSlot& slot, bool firstWorld, WorldStartupResult& result,
                                        std::vector<OrchestrationStartupError>& errors,
                                        std::vector<UnitReportedError>& unitErrors) {
    const WorldDescription& world = slot.description;
    Registration registration;
    auto registry = std::make_unique<Kernel::Registry>();
    ScheduleList frame(world.frame);
    std::vector<PrepareFn> prepares;
    std::vector<Detail::CarryEntry> carries;

    // Build: every unit of the World in order, then the product's own (always
    // registered, so a World with no unit still freezes; section 3.3).
    for (std::size_t index : slot.units) {
        UnitSlot& unit = units_[index];
        auto registered = registry->Register(Kernel::RegistrationOwner{unit.row, unit.seq}, [&](Kernel::Registrar& registrar) {
            WorldSetup setup(world.name, registrar, world.options, frame, prepares, carries);
            unit.unit->Build(setup);
        });
        GYO_ASSERT(static_cast<bool>(registered));
    }
    auto own = registry->Register(Kernel::RegistrationOwner{name_, static_cast<std::uint32_t>(units_.size())},
                                  [&](Kernel::Registrar& registrar) {
                                      WorldSetup setup(world.name, registrar, world.options, frame, prepares, carries);
                                      if (world.build) world.build(setup);
                                  });
    GYO_ASSERT(static_cast<bool>(own));

    auto built = registry->BuildPlans();
    if (!built) {
        result.kernel.emplace(std::move(built).error());
        result.notChecked = true;
        return registration;
    }
    const Kernel::FrozenRegistry& frozen = registry->Frozen();
    for (std::size_t index : slot.units) {
        WorldFinish context(world.name, units_[index].row, unitErrors);
        units_[index].unit->Finish(frozen, context);
    }
    for (std::size_t index : slot.units) {
        WorldCheck context(world.name, units_[index].row, world.dynamicInsert, world.options, unitErrors);
        units_[index].unit->Check(frozen, context);
    }
    for (const std::string& anchor : frame.missingAnchors_) {
        errors.push_back({OrchestrationCode::OE5, "frame list of World " + Quoted(world.name) + ": no anchor " + Quoted(anchor),
                          {world.name, anchor}});
    }
    for (const std::string& name : frame.names_) {
        if (!HasSchedule(frozen, name)) {
            errors.push_back({OrchestrationCode::OE5,
                              "frame list of World " + Quoted(world.name) + ": " + Quoted(name) + " is not a schedule of that World",
                              {world.name, name}});
        }
    }
    if (firstWorld) {
        for (const std::string& name : gameInit_) {
            if (!HasSchedule(frozen, name)) {
                errors.push_back({OrchestrationCode::OE5,
                                  "game initialization list: " + Quoted(name) + " is not a schedule of the first World " +
                                      Quoted(world.name),
                                  {world.name, name}});
            }
        }
    }
    registration.registry = std::move(registry);
    registration.frame = std::move(frame.names_);
    registration.unitPrepares = std::move(prepares);
    registration.carries = std::move(carries);
    return registration;
}

std::unique_ptr<Kernel::WorldExecutor> Product::MakeExecutor(WorldSlot& slot, const Kernel::Registry& registry) {
    auto executor = std::make_unique<Kernel::WorldExecutor>(registry, slot.description.name,
                                                            Kernel::WorldOptions{slot.description.dynamicInsert});
    if (slot.handler) executor->SetErrorHandler(slot.handler);
    return executor;
}

::Engine::Base::Result<void, OrchestrationError> Product::Prepare(WorldSlot& slot, Kernel::World& world,
                                                                const std::vector<PrepareFn>& unitPrepares,
                                                                const PrepareFn* extra) {
    auto call = [&](const PrepareFn& fn) -> ::Engine::Base::Result<void, OrchestrationError> {
        if (!fn) return {};
        auto done = fn(world);
        if (!done) {
            OrchestrationError error = std::move(done).error();
            error.code = OrchestrationErrorCode::ReplacePrepareFailed;
            return ::Engine::Base::Err(std::move(error));
        }
        return {};
    };
    for (const PrepareFn& fn : unitPrepares) {
        if (auto done = call(fn); !done) return done;
    }
    for (const PrepareFn& fn : slot.description.prepare) {
        if (auto done = call(fn); !done) return done;
    }
    if (extra != nullptr) {
        if (auto done = call(*extra); !done) return done;
    }
    return {};
}

Product::WorldSlot* Product::Find(std::string_view name) noexcept {
    for (WorldSlot& slot : worlds_) {
        if (slot.description.name == name) return &slot;
    }
    return nullptr;
}

const Product::WorldSlot* Product::Find(std::string_view name) const noexcept {
    for (const WorldSlot& slot : worlds_) {
        if (slot.description.name == name) return &slot;
    }
    return nullptr;
}

void Product::RequireNotRunning() const {
    for (const WorldSlot& slot : worlds_) {
        if (!slot.executor) continue;
        const Kernel::WorldState state = slot.executor->GetWorld().State();
        GYO_ASSERT(state != Kernel::WorldState::Running && state != Kernel::WorldState::Applying);
    }
}

::Engine::Base::Result<void, OrchestrationError> Product::RunOn(WorldSlot& slot, std::string_view schedule) {
    if (!slot.executor) {
        return ::Engine::Base::Err(OrchestrationError(OrchestrationErrorCode::WorldMissing,
                                                      "World " + Quoted(slot.description.name) + " is empty (a replacement failed)"));
    }
    if (slot.executor->GetWorld().State() == Kernel::WorldState::Interrupted) {
        return ::Engine::Base::Err(OrchestrationError(OrchestrationErrorCode::WorldInterrupted,
                                                      "World " + Quoted(slot.description.name) + " is interrupted"));
    }
    auto ran = slot.executor->Run(schedule);
    if (!ran) {
        return ::Engine::Base::Err(FromKernel(OrchestrationErrorCode::ScheduleFailed,
                                              "schedule " + Quoted(schedule) + " on World " + Quoted(slot.description.name) + " failed",
                                              std::move(ran).error()));
    }
    return {};
}

::Engine::Base::Result<void, OrchestrationError> Product::RunGameInit() {
    RequireNotRunning();
    GYO_ASSERT(!gameInitRan_);
    // The first World's place is empty (a DropOldFirst replacement failed):
    // nothing ran, so game initialization is not counted as run.
    if (!worlds_.empty() && !worlds_.front().executor) {
        return ::Engine::Base::Err(OrchestrationError(OrchestrationErrorCode::WorldMissing,
                                                      "World " + Quoted(worlds_.front().description.name) +
                                                          " is empty: game initialization waits until it is filled"));
    }
    gameInitRan_ = true;
    if (worlds_.empty()) return {};
    for (const std::string& schedule : gameInit_) {
        if (auto ran = RunOn(worlds_.front(), schedule); !ran) return ran;
    }
    return {};
}

::Engine::Base::Result<void, OrchestrationError> Product::RunFrame() {
    RequireNotRunning();
    if (!gameInitRan_) {
        if (auto ran = RunGameInit(); !ran) return ran;
    }
    for (WorldSlot& slot : worlds_) {
        if (!slot.executor) {
            return ::Engine::Base::Err(OrchestrationError(OrchestrationErrorCode::WorldMissing,
                                                          "World " + Quoted(slot.description.name) + " is empty (a replacement failed)"));
        }
        if (slot.executor->GetWorld().State() == Kernel::WorldState::Interrupted) {
            return ::Engine::Base::Err(OrchestrationError(OrchestrationErrorCode::WorldInterrupted,
                                                          "World " + Quoted(slot.description.name) + " is interrupted"));
        }
        for (const std::string& schedule : slot.frame) {
            if (auto ran = RunOn(slot, schedule); !ran) return ran;
        }
    }
    return {};
}

::Engine::Base::Result<void, OrchestrationError> Product::RunSchedule(std::string_view world, std::string_view schedule) {
    RequireNotRunning();
    WorldSlot* slot = Find(world);
    if (slot == nullptr) {
        return ::Engine::Base::Err(OrchestrationError(OrchestrationErrorCode::UnknownWorld, "no World " + Quoted(world)));
    }
    if (!gameInitRan_) {
        if (auto ran = RunGameInit(); !ran) return ran;
    }
    return RunOn(*slot, schedule);
}

RunnerExit Product::Run() {
    GYO_ASSERT(!ran_);
    ran_ = true;
    if (runner_) return runner_(*this);
    // The default runner: one frame (Bevy run_once).
    auto ran = RunFrame();
    if (ran) return RunnerExit{};
    return RunnerExit{RunnerExitCode::Failure, ran.error().message + (ran.error().detail.empty() ? "" : "\n" + ran.error().detail)};
}

Kernel::WorldExecutor& Product::World(std::string_view name) {
    WorldSlot* slot = Find(name);
    GYO_ASSERT(slot != nullptr && slot->executor != nullptr);
    return *slot->executor;
}

bool Product::HasWorld(std::string_view name) const noexcept {
    const WorldSlot* slot = Find(name);
    return slot != nullptr && slot->executor != nullptr;
}

void Product::SetErrorHandler(std::string_view world, std::function<void(const Kernel::KernelError&)> handler) {
    WorldSlot* slot = Find(world);
    GYO_ASSERT(slot != nullptr);
    slot->handler = std::move(handler);
    if (slot->executor) slot->executor->SetErrorHandler(slot->handler);
}

::Engine::Base::Result<void, OrchestrationError> Product::ReplaceWorld(std::string_view name, ReplaceRequest request) {
    RequireNotRunning();
    WorldSlot* found = Find(name);
    if (found == nullptr) {
        return ::Engine::Base::Err(OrchestrationError(OrchestrationErrorCode::UnknownWorld, "no World " + Quoted(name)));
    }
    WorldSlot& slot = *found;
    const bool firstWorld = &slot == &worlds_.front();
    const bool dropOldFirst = request.order == ReplaceOrder::DropOldFirst;
    auto fail = [&](OrchestrationError error) -> ::Engine::Base::Result<void, OrchestrationError> {
        Warn("ReplaceWorld " + Quoted(name) + ": " + std::string(ToString(error.code)) + ": " + error.message +
             (error.reporterCode.empty() ? std::string() : " (" + error.reporterCode + ")"));
        return ::Engine::Base::Err(std::move(error));
    };

    // DropOldFirst: the marked values go to the holding place, then the old World and its Registry go.
    if (dropOldFirst && slot.executor) {
        for (const Detail::CarryEntry& entry : slot.carries) {
            if (std::shared_ptr<void> box = entry.take(slot.executor->GetWorld())) slot.held.emplace_back(entry, std::move(box));
        }
        slot.executor.reset();
        slot.registry.reset();
    }

    // Step 1: register the World again from the description (v8-arch-gyo2-restore-reruns-registration).
    std::vector<OrchestrationStartupError> errors;
    std::vector<UnitReportedError> unitErrors;
    WorldStartupResult result{slot.description.name, std::nullopt, false};
    Registration registration = Register(slot, firstWorld, result, errors, unitErrors);
    if (!registration.registry || !errors.empty() || !unitErrors.empty()) {
        std::vector<WorldStartupResult> results;
        results.push_back(std::move(result));
        ProductStartupReport report(std::move(results), std::move(errors), std::move(unitErrors));
        return fail(OrchestrationError(OrchestrationErrorCode::ReplaceRegistrationFailed,
                                       "registering World " + Quoted(name) + " again failed", report.detail));
    }
    std::unique_ptr<Kernel::WorldExecutor> fresh = MakeExecutor(slot, *registration.registry);
    Kernel::World& world = fresh->GetWorld();

    // Step 2: carry the marked Resources over, before every prepare function (section 3.12).
    // The holding place first (an earlier failure left values there), then
    // what the old World still has of the marked types.
    std::vector<std::pair<Detail::CarryEntry, std::shared_ptr<void>>> moved = std::move(slot.held);
    slot.held.clear();
    if (!dropOldFirst && slot.executor) {
        for (const Detail::CarryEntry& entry : slot.carries) {
            const bool held = std::any_of(moved.begin(), moved.end(), [&](const auto& m) { return m.first.key == entry.key; });
            if (held) continue;
            if (std::shared_ptr<void> box = entry.take(slot.executor->GetWorld())) moved.emplace_back(entry, std::move(box));
        }
    }
    // The values back where they came from: the old World when it can take
    // them (it is there and not interrupted: an interrupted World is never
    // written, spec E.5), otherwise the holding place.
    const bool backToOld = !dropOldFirst && slot.executor &&
                           slot.executor->GetWorld().State() != Kernel::WorldState::Interrupted;
    auto giveBack = [&](std::vector<std::pair<Detail::CarryEntry, std::shared_ptr<void>>>& values) {
        for (auto& [entry, box] : values) {
            if (!box) {
                box = entry.take(world);
                if (!box) continue;
            }
            if (!backToOld) {
                slot.held.emplace_back(entry, std::move(box));
            } else {
                entry.put(slot.executor->GetWorld(), box);
            }
        }
        values.clear();
    };
    for (const auto& [entry, box] : moved) {
        if (!entry.registered(world)) {
            giveBack(moved);
            return fail(OrchestrationError(OrchestrationErrorCode::ReplaceCarryOverFailed,
                                           "World " + Quoted(name) + ": a Resource marked to carry over" +
                                               (entry.typeName.empty() ? std::string() : " (" + entry.typeName + ")") +
                                               " is not registered in the new World"));
        }
    }
    for (auto& [entry, box] : moved) entry.put(world, box);

    try {
        // Step 3: prepare functions.
        if (auto prepared = Prepare(slot, world, registration.unitPrepares, request.extra ? &request.extra : nullptr); !prepared) {
            giveBack(moved);
            return fail(std::move(prepared).error());
        }
        // Step 4: game initialization when it has not run (on the new World when it
        // replaces the first one; only right after startup), then beforeSwap.
        if (!gameInitRan_) {
            if (firstWorld) {
                gameInitRan_ = true;
                for (const std::string& schedule : gameInit_) {
                    auto ran = fresh->Run(schedule);
                    if (!ran) {
                        giveBack(moved);
                        return fail(FromKernel(OrchestrationErrorCode::ReplaceScheduleFailed,
                                               "game initialization failed on the new first World " + Quoted(name),
                                               std::move(ran).error()));
                    }
                }
            } else if (auto ran = RunGameInit(); !ran) {
                giveBack(moved);
                return fail(std::move(ran).error());
            }
        }
        for (const std::string& schedule : request.beforeSwap) {
            auto ran = fresh->Run(schedule);
            if (!ran) {
                giveBack(moved);
                return fail(FromKernel(OrchestrationErrorCode::ReplaceScheduleFailed,
                                       "schedule " + Quoted(schedule) + " on the new World " + Quoted(name) + " failed",
                                       std::move(ran).error()));
            }
        }
    } catch (...) {
        giveBack(moved);
        Warn("ReplaceWorld " + Quoted(name) + ": an exception left the replacement; the old World goes on");
        throw;
    }

    // Step 5: swap. The old executor first, then its Registry.
    slot.executor.reset();
    slot.registry.reset();
    slot.registry = std::move(registration.registry);
    slot.executor = std::move(fresh);
    slot.frame = std::move(registration.frame);
    slot.unitPrepares = std::move(registration.unitPrepares);
    slot.carries = std::move(registration.carries);
    slot.held.clear();
    slot.nextInsertSeq = 0;
    return {};
}

::Engine::Base::Result<void, OrchestrationError> Product::InsertSystems(std::string_view worldName, std::string owner,
                                                                      InsertFn build) {
    RequireNotRunning();
    WorldSlot* slot = Find(worldName);
    if (slot == nullptr) {
        return ::Engine::Base::Err(OrchestrationError(OrchestrationErrorCode::UnknownWorld, "no World " + Quoted(worldName)));
    }
    if (!slot->executor) {
        return ::Engine::Base::Err(OrchestrationError(OrchestrationErrorCode::WorldMissing, "World " + Quoted(worldName) + " is empty"));
    }
    const bool taken = owner == name_ ||
                       std::any_of(units_.begin(), units_.end(), [&](const UnitSlot& unit) { return unit.row == owner; });
    if (taken) {
        return ::Engine::Base::Err(OrchestrationError(OrchestrationErrorCode::OwnerTaken,
                                                      "owner " + Quoted(owner) + " is a registration unit's or the Product's own name"));
    }
    const std::uint32_t seq = static_cast<std::uint32_t>(units_.size()) + 1 + slot->nextInsertSeq++;
    auto registered = [&] {
        try {
            return slot->registry->Register(Kernel::RegistrationOwner{owner, seq},
                                            [&](Kernel::Registrar& registrar) { build(registrar); });
        } catch (...) {
            // Kernel rule 3 keeps what was registered in the pending round; drop it so
            // the Registry is not left half-registered.
            slot->registry->DiscardRegistrations();
            throw;
        }
    }();
    if (!registered) {
        Warn("InsertSystems on World " + Quoted(worldName) + " by " + Quoted(owner) +
             ": refused, the World declares no dynamic insert");
        return ::Engine::Base::Err(FromKernel(OrchestrationErrorCode::InsertRefused,
                                              "World " + Quoted(worldName) + " declares no dynamic insert",
                                              std::move(registered).error()));
    }
    auto built = slot->registry->BuildPlans();
    if (!built) {
        const Kernel::StartupReport& report = built.error();
        Warn("InsertSystems on World " + Quoted(worldName) + " by " + Quoted(owner) + ": BuildPlans failed: " + report.detail);
        return ::Engine::Base::Err(OrchestrationError(OrchestrationErrorCode::InsertBuildFailed,
                                                      "BuildPlans of World " + Quoted(worldName) + " failed", report.detail));
    }
    return {};
}

} // namespace GYO::Engine::Orchestration
