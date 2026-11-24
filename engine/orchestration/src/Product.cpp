// Product: building, running, replacing and inserting into a group of Worlds
// (declaration docs/architecture/target/v2/orchestration-skeleton-declaration.zh-Hant.md,
// sections 3.4 to 3.12).

#include "engine/orchestration/Product.hpp"
#include "engine/orchestration/WorldGeneration.hpp"

#include "engine/base/Assert.hpp"
#include "engine/base/Trace.hpp"

#include <algorithm>
#include <new>
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

// Whether name is an extract schedule of the Registry (DI-KS 35); false when there is none of that name.
bool IsExtractSchedule(const Kernel::FrozenRegistry& registry, std::string_view name) {
    for (const Kernel::ScheduleInfo& schedule : registry.Schedules()) {
        if (schedule.options.name == name) return schedule.options.extract;
    }
    return false;
}

std::string Quoted(std::string_view text) {
    return "'" + std::string(text) + "'";
}

// The owner name of the orchestration layer's own declarations, the default skeleton and WorldGeneration (no unit
// row is "orchestration": the orchestration layer is library-only in the register).
constexpr const char* kSkeletonOwner = "orchestration";

// The default skeleton (ScheduleSkeleton.hpp): names and stage order only.
void DeclareDefaultSkeleton(Kernel::Registrar& registrar) {
    using S = DefaultSchedules;
    Kernel::ScheduleOptions frame;
    frame.name = std::string(S::frame);
    frame.stages = {std::string(S::first),  std::string(S::preUpdate),  std::string(S::fixedMainLoop),
                    std::string(S::update), std::string(S::postUpdate), std::string(S::last)};
    registrar.Schedule(std::move(frame));
    Kernel::ScheduleOptions fixed;
    fixed.name = std::string(S::fixed);
    fixed.stages = {std::string(S::first), std::string(S::preUpdate), std::string(S::update), std::string(S::postUpdate),
                    std::string(S::last)};
    registrar.Schedule(std::move(fixed));
}

// The World's initial frame list: with the default skeleton the frame schedule first, unless the list names it.
std::vector<std::string> InitialFrame(const WorldDescription& world) {
    std::vector<std::string> frame = world.frame;
    const std::string main(DefaultSchedules::frame);
    if (world.skeleton == ScheduleSkeleton::Default && std::find(frame.begin(), frame.end(), main) == frame.end()) {
        frame.insert(frame.begin(), main);
    }
    return frame;
}

OrchestrationError FromKernel(OrchestrationErrorCode code, std::string message, Kernel::KernelError error) {
    OrchestrationError result(code, std::move(message), error.Text());
    result.kernel.push_back(std::move(error));
    return result;
}

} // namespace

Product::~Product() {
    // Exchange declaration 5.2 point 5: an explicit order (the order members and vector elements are
    // destroyed in is not relied on). No World is lent to another thread (O-13): the runner returned them.
    for (const WorldSlot& slot : worlds_) GYO_ASSERT(!slot.lease && "a Product is destroyed with a World lent to another thread");
    // 1. Every World, in the reverse order of the description (a World that extracts from another comes
    //    after it, so it goes first): its exchange functions, the World, its Registry.
    for (auto slot = worlds_.rbegin(); slot != worlds_.rend(); ++slot) {
        slot->exchangeOut.clear();
        slot->exchangeIn.clear();
        slot->executor.reset();
        slot->registry.reset();
    }
    // 2. The OnDestroy callbacks, the last registered first; each closure goes once it has run. Every
    //    object of the Product's buffer is still there.
    while (!onDestroy_.empty()) {
        std::function<void()> callback = std::move(onDestroy_.back());
        onDestroy_.pop_back();
        if (callback) callback();
    }
    // 3. The runners (one may hold a window or threads a callback still used, and may point into the buffer).
    while (!runners_.empty()) runners_.pop_back();
    // 4. The Product's buffer: its objects in the reverse order of their Create.
    buffer_.reset();
    // 5. The descriptions (their closures) and the options.
    while (!worlds_.empty()) worlds_.pop_back();
    configure_ = nullptr;
    options_ = OptionBag();
    // 6. The units, the last added first.
    while (!units_.empty()) units_.pop_back();
    while (!owned_.plugins.empty()) owned_.plugins.pop_back();
    while (!owned_.modules.empty()) owned_.modules.pop_back();
}

Base::Result<std::unique_ptr<Product>, ProductStartupReport> Product::Build(ProductDescription description,
                                                                                    RegisteredUnits units) {
    std::unique_ptr<Product> product(new Product());
    Product& self = *product;
    self.name_ = std::move(description.name);
    self.configure_ = std::move(description.configure);
    self.options_ = std::move(description.options);
    self.buffer_ = std::make_unique<ModuleBuffer>();
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
    // The described names in order, taken before the loop moves each description into its slot.
    std::vector<std::string> describedNames;
    for (const WorldDescription& world : description.worlds) describedNames.push_back(world.name);
    for (std::size_t index = 0; index < description.worlds.size(); ++index) {
        WorldDescription& world = description.worlds[index];
        // O-E9 (a) (O-12; v8-arch-gyo2-sdl-extract): extractFrom names a World described before this
        // one. Checked against the description itself, so a source that O-E6 drops below is not
        // reported again here (the reader is then left unchecked at registration: its source is not
        // before it among the slots).
        if (!world.extractFrom.empty()) {
            std::size_t at = describedNames.size();
            for (std::size_t other = 0; other < describedNames.size(); ++other) {
                if (describedNames[other] == world.extractFrom) {
                    at = other;
                    break;
                }
            }
            std::string problem;
            if (at == describedNames.size()) {
                problem = "names no World of the description";
            } else if (at == index) {
                problem = "names the World itself";
            } else if (at > index) {
                problem = "names a World described after it (the source runs first)";
            }
            if (!problem.empty()) {
                errors.push_back({OrchestrationCode::OE9,
                                  "World " + Quoted(world.name) + ": extractFrom " + Quoted(world.extractFrom) + " " + problem,
                                  {world.name, world.extractFrom}});
            }
        }
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

    // Step 2: Configure, every unit in order, then the product's own. A runner, an
    // OnDestroy callback and the Product's buffer are the Product's from here on, so a
    // failed Build destroys them in ~Product's order (exchange declaration 5.2).
    ScheduleList gameInit(std::move(description.gameInit));
    std::vector<ProductSetup::Installed>& runners = self.runners_;
    for (UnitSlot& unit : self.units_) {
        ProductSetup setup(self.name_, unit.row, gameInit, *self.buffer_, self.options_, unitErrors, self.onDestroy_);
        unit.unit->Configure(setup);
        for (auto& installed : setup.runners_) runners.push_back(std::move(installed));
    }
    if (self.configure_) {
        ProductSetup setup(self.name_, self.name_, gameInit, *self.buffer_, self.options_, unitErrors, self.onDestroy_);
        self.configure_(setup);
        for (auto& installed : setup.runners_) runners.push_back(std::move(installed));
    }
    if (runners.size() > 1) {
        std::vector<std::string> installers;
        for (const auto& installed : runners) installers.push_back(installed.installer);
        errors.push_back({OrchestrationCode::OE4, std::to_string(runners.size()) + " runners are installed; a Product has one",
                          installers});
    }
    // A Configure report (exchange declaration 5.2 point 3): no World is built and no
    // World's Build runs (a unit that reported may not have made what its dependants find).
    const bool configureReported = !unitErrors.empty();
    for (const std::string& anchor : gameInit.missingAnchors_) {
        errors.push_back({OrchestrationCode::OE5, "game initialization list: no anchor " + Quoted(anchor), {anchor}});
    }
    self.gameInit_ = gameInit.names_;

    // Step 3 and 4: every World's registration and list checks.
    std::vector<Registration> registrations;
    // A reader's Registry goes before its extract source's on every way out (DI-KS 37, O-15; an
    // exception from a unit's Build included): in reverse order, not left to the vector's destructor.
    struct ReverseDrop {
        std::vector<Registration>& registrations;
        ~ReverseDrop() {
            while (!registrations.empty()) registrations.pop_back();
        }
    } reverseDrop{registrations};
    for (std::size_t i = 0; i < self.worlds_.size(); ++i) {
        if (configureReported) {
            results.push_back(WorldStartupResult{self.worlds_[i].description.name, std::nullopt, true});
            continue;
        }
        // O-12: a World that extracts from another registers after it, with its Registry as the extract
        // source. Not registered (unchecked; the error that stops it is reported once, declaration Q1):
        // its extractFrom names no World before it among the slots (refused by O-E9, or dropped by O-E6),
        // or one that did not register.
        const WorldSlot& slot = self.worlds_[i];
        const Kernel::Registry* source = nullptr;
        if (!slot.description.extractFrom.empty()) {
            const std::size_t at = self.ExtractSourceIndex(slot);
            if (at < i) source = registrations[at].registry.get();
            if (source == nullptr) {
                results.push_back(WorldStartupResult{slot.description.name, std::nullopt, true});
                registrations.emplace_back();
                continue;
            }
        }
        WorldStartupResult result{slot.description.name, std::nullopt, false};
        registrations.push_back(self.Register(slot, i == 0, source, result, errors, unitErrors));
        results.push_back(std::move(result));
    }

    // Step 5: any error so far, and no World is built.
    const bool kernelFailed =
        std::any_of(results.begin(), results.end(), [](const WorldStartupResult& r) { return r.kernel.has_value(); });
    // A World left unchecked has no registration (a reader whose source did not register, O-12): it
    // always comes with the error that stopped it, and is never built on its own.
    const bool unchecked =
        std::any_of(results.begin(), results.end(), [](const WorldStartupResult& r) { return r.notChecked; });
    if (!errors.empty() || !unitErrors.empty() || kernelFailed || unchecked) {
        return Base::Err(ProductStartupReport(std::move(results), std::move(errors), std::move(unitErrors)));
    }

    // Step 6: build the Worlds and run their prepare functions (O-E7 alone).
    for (std::size_t i = 0; i < self.worlds_.size(); ++i) {
        WorldSlot& slot = self.worlds_[i];
        Registration& registration = registrations[i];
        slot.registry = std::move(registration.registry);
        slot.frame = std::move(registration.frame);
        slot.carries = std::move(registration.carries);
        slot.exchangeIn = std::move(registration.exchangeIn);
        slot.exchangeOut = std::move(registration.exchangeOut);
        slot.executor = self.MakeExecutor(slot, *slot.registry);
    }
    for (std::size_t i = 0; i < self.worlds_.size(); ++i) {
        WorldSlot& slot = self.worlds_[i];
        auto prepared = self.Prepare(slot, slot.executor->GetWorld(), registrations[i].unitPrepares, nullptr);
        if (!prepared) {
            const OrchestrationError& error = prepared.error();
            errors.push_back({OrchestrationCode::OE7,
                              "World " + Quoted(slot.description.name) + ": a prepare function failed" +
                                  (error.reporterCode.empty() ? std::string() : " (" + error.reporterCode + ")") + ": " +
                                  error.message,
                              {slot.description.name}});
        }
    }
    if (!errors.empty()) return Base::Err(ProductStartupReport({}, std::move(errors), {}));
    return product;
}

Product::Registration Product::Register(const WorldSlot& slot, bool firstWorld, const Kernel::Registry* extractSource,
                                        WorldStartupResult& result, std::vector<OrchestrationStartupError>& errors,
                                        std::vector<UnitReportedError>& unitErrors) {
    const WorldDescription& world = slot.description;
    Registration registration;
    auto registry = std::make_unique<Kernel::Registry>();
    // O-12: the extract source is set before the first BuildPlans (Kernel::Registry::SetExtractSource).
    if (extractSource != nullptr) registry->SetExtractSource(*extractSource);
    ScheduleList frame(InitialFrame(world));
    std::vector<PrepareFn> prepares;
    std::vector<Detail::CarryEntry> carries;
    WorldSetup::Exchanges exchanges;
    // The Modules' buffer of this build (authority-rollback declaration section 6): written during Build, frozen
    // before BuildPlans, read by Finish, Check and the prepare functions, let go once those have run (O-2).
    auto buffer = std::make_shared<ModuleBuffer>();

    // The orchestration layer's own declarations first, before any unit's Build: WorldGeneration in every World
    // (O-2, O-14; world-gen declaration 5.2), the default skeleton when it is on (O-4; time-events declaration 3.6).
    auto own0 = registry->Register(Kernel::RegistrationOwner{kSkeletonOwner, 0}, [&world](Kernel::Registrar& registrar) {
        registrar.Resource<WorldGeneration>({.name = "WorldGeneration"});
        if (world.skeleton == ScheduleSkeleton::Default) DeclareDefaultSkeleton(registrar);
    });
    GYO_ASSERT(static_cast<bool>(own0));

    // Build: every unit of the World in order, then the product's own (always
    // registered, so a World with no unit still freezes; section 3.3).
    for (std::size_t index : slot.units) {
        UnitSlot& unit = units_[index];
        auto registered = registry->Register(Kernel::RegistrationOwner{unit.row, unit.seq}, [&](Kernel::Registrar& registrar) {
            WorldSetup setup(world.name, unit.row, registrar, world.options, frame, prepares, carries, *buffer, *buffer_,
                             exchanges);
            setup.extractFrom_ = world.extractFrom;
            unit.unit->Build(setup);
        });
        GYO_ASSERT(static_cast<bool>(registered));
    }
    auto own = registry->Register(Kernel::RegistrationOwner{name_, static_cast<std::uint32_t>(units_.size())},
                                  [&](Kernel::Registrar& registrar) {
                                      WorldSetup setup(world.name, name_, registrar, world.options, frame, prepares, carries,
                                                       *buffer, *buffer_, exchanges);
                                      setup.extractFrom_ = world.extractFrom;
                                      if (world.build) world.build(setup);
                                  });
    GYO_ASSERT(static_cast<bool>(own));
    buffer->Freeze();

    auto built = registry->BuildPlans();
    if (!built) {
        result.kernel.emplace(std::move(built).error());
        result.notChecked = true;
        return registration;
    }
    const Kernel::FrozenRegistry& frozen = registry->Frozen();
    for (std::size_t index : slot.units) {
        WorldFinish context(world.name, units_[index].row, unitErrors, *buffer, *buffer_);
        units_[index].unit->Finish(frozen, context);
    }
    for (std::size_t index : slot.units) {
        WorldCheck context(world.name, units_[index].row, world.dynamicInsert, world.options, unitErrors, *buffer, *buffer_);
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
        } else if (world.extractFrom.empty() && IsExtractSchedule(frozen, name)) {
            // O-E9 (b): an extract schedule needs a source to run.
            errors.push_back({OrchestrationCode::OE9,
                              "frame list of World " + Quoted(world.name) + ": " + Quoted(name) +
                                  " is an extract schedule, and the World has no extractFrom",
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
            } else if (IsExtractSchedule(frozen, name)) {
                // O-E9 (b): the first World has no World before it to extract from.
                errors.push_back({OrchestrationCode::OE9,
                                  "game initialization list: " + Quoted(name) + " is an extract schedule; the first World " +
                                      Quoted(world.name) + " has no World before it to extract from",
                                  {world.name, name}});
            }
        }
    }
    registration.registry = std::move(registry);
    registration.frame = std::move(frame.names_);
    registration.unitPrepares = std::move(prepares);
    registration.carries = std::move(carries);
    registration.exchangeIn = std::move(exchanges.in);
    registration.exchangeOut = std::move(exchanges.out);
    registration.buffer = std::move(buffer);
    return registration;
}

std::size_t Product::ExtractSourceIndex(const WorldSlot& slot) const noexcept {
    if (slot.description.extractFrom.empty()) return worlds_.size();
    for (std::size_t i = 0; i < worlds_.size(); ++i) {
        if (worlds_[i].description.name == slot.description.extractFrom) return i;
    }
    return worlds_.size();
}

Base::Result<void, Kernel::KernelError> Product::RunWith(Kernel::WorldExecutor& executor, const WorldSlot& slot,
                                                         std::string_view schedule) {
    if (!IsExtractSchedule(executor.GetWorld().Types(), schedule)) return executor.Run(schedule);
    // DI-KS 35: an extract schedule runs through RunExtract with the World this one extracts from.
    // Every World is healthy here (each entry point checks first, O-11), so the source is Ready.
    const std::size_t at = ExtractSourceIndex(slot);
    GYO_ASSERT(at < worlds_.size() && worlds_[at].executor != nullptr &&
               "an extract schedule runs on a World that has no extractFrom");
    return executor.RunExtract(schedule, worlds_[at].executor->GetWorld());
}

std::unique_ptr<Kernel::WorldExecutor> Product::MakeExecutor(WorldSlot& slot, const Kernel::Registry& registry) {
    auto executor = std::make_unique<Kernel::WorldExecutor>(registry, slot.description.name,
                                                            Kernel::WorldOptions{slot.description.dynamicInsert});
    // Generating from the moment it is built, before its prepare functions (world-gen declaration 5.1, O-14).
    executor->GetWorld().SetResource(WorldGeneration{true, false});
    return executor;
}

Base::Result<void, OrchestrationError> Product::Prepare(WorldSlot& slot, Kernel::World& world,
                                                                const std::vector<PrepareFn>& unitPrepares,
                                                                const PrepareFn* extra) {
    auto call = [&](const PrepareFn& fn) -> Base::Result<void, OrchestrationError> {
        if (!fn) return {};
        auto done = fn(world);
        if (!done) {
            OrchestrationError error = std::move(done).error();
            error.code = OrchestrationErrorCode::ReplacePrepareFailed;
            return Base::Err(std::move(error));
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
        if (!slot.executor || slot.lease) continue;  // a lent World is its holder's (O-13 (2))
        const Kernel::WorldState state = slot.executor->GetWorld().State();
        GYO_ASSERT(state != Kernel::WorldState::Running && state != Kernel::WorldState::Applying);
    }
}

const WorldFailure& Product::Break(WorldFailure failure) {
    // The first failure ends the Product; nothing runs after it, so there is no second.
    GYO_ASSERT(!failure_);
    if (failure.product.empty()) failure.product = name_;  // made earlier when it had to allocate nothing later (P12)
    failure_ = std::move(failure);
    return *failure_;
}

OrchestrationError Product::EndedError() const {
    const bool missing = failure_->kind != WorldFailureKind::Interrupted;
    OrchestrationError error(missing ? OrchestrationErrorCode::WorldMissing : OrchestrationErrorCode::WorldInterrupted,
                             "World " + Quoted(failure_->world) + (missing ? " is empty" : " is interrupted") +
                                 ": the Product has ended",
                             failure_->Text());
    error.failure = *failure_;
    return error;
}

std::optional<OrchestrationError> Product::Ended() {
    // A World that broke while lent (O-13 (4)): its failure was written before the flag (release).
    if (!failure_) {
        for (const WorldSlot& slot : worlds_) {
            if (slot.lease && slot.lease->broken.load(std::memory_order_acquire)) {
                Break(*slot.lease->failure);
                break;
            }
        }
    }
    if (!failure_) {
        // An interrupted World without a recorded failure was run outside the
        // Product (WorldExecutor::Run called directly). An empty place without
        // one is a DropOldFirst replacement in progress (its game initialization
        // step); a failed one is always recorded.
        for (WorldSlot& slot : worlds_) {
            if (slot.lease) continue;  // its holder's; its failure comes through the lease
            if (!slot.executor) {
                // Error levels P12: an empty place without a failure is only a DropOldFirst
                // replacement in progress (its game initialization step); anything else is a
                // broken invariant of this class.
                GYO_ASSERT(slot.replacing);
                continue;
            }
            if (slot.executor->GetWorld().State() != Kernel::WorldState::Interrupted) continue;
            WorldFailure failure;
            failure.world = slot.description.name;
            failure.kind = WorldFailureKind::Interrupted;
            failure.entry = "outside the Product";
            failure.lastSchedule = slot.lastSchedule;
            Break(std::move(failure));
            break;
        }
    }
    if (!failure_) return std::nullopt;
    return EndedError();
}

Base::Result<void, OrchestrationError> Product::RunOn(WorldSlot& slot, std::string_view schedule,
                                                              std::string_view entry, Detail::LeaseShared* lease) {
    // Where the failure that ends the Product goes: the Product, or the lease of a World run on its holder's thread
    // (written first, then the flag with release; the Product's thread reads it after an acquire, O-13 (4)).
    const auto record = [this, lease](WorldFailure&& failure) -> const WorldFailure& {
        if (lease == nullptr) return Break(std::move(failure));
        if (failure.product.empty()) failure.product = name_;
        lease->failure = std::move(failure);
        lease->broken.store(true, std::memory_order_release);
        return *lease->failure;
    };
    // DI-KS 35: an extract schedule needs the World it extracts from. Checked before the run, so the
    // Programmer Error is not taken for an exception that left the schedule (which ends the Product).
    GYO_ASSERT((!IsExtractSchedule(slot.executor->GetWorld().Types(), schedule) || ExtractSourceIndex(slot) < worlds_.size()) &&
               "an extract schedule runs on a World that has no extractFrom");
    // Read before the schedule runs, from the World while it is healthy.
    WorldFailure failure;
    failure.world = slot.description.name;
    failure.entry = std::string(entry);
    failure.schedule = std::string(schedule);
    failure.lastSchedule = slot.lastSchedule;
    failure.syncEpoch = slot.executor->GetWorld().SyncEpoch();
    // Made before the schedule runs, so recording the failure after an exception allocates
    // nothing but the optional text (error levels D4: the failure is recorded, then the
    // exception goes on unchanged).
    failure.product = name_;
    failure.where = "an exception left the schedule";
    auto ran = [&] {
        try {
            return RunWith(*slot.executor, slot, schedule);
        } catch (const std::bad_alloc&) {
            failure.kind = WorldFailureKind::ResourceExhausted;
            try {
                failure.where = "std::bad_alloc left the schedule";
            } catch (...) {
                // No memory for the text: the preset one stays.
            }
            record(std::move(failure));
            throw;
        } catch (...) {
            failure.kind = WorldFailureKind::Interrupted;
            record(std::move(failure));
            throw;
        }
    }();
    if (!ran) {
        Kernel::KernelError kernelError = std::move(ran).error();
        failure.kind = WorldFailureKind::Interrupted;
        failure.kernelCode = kernelError.code;
        failure.where = kernelError.Text();
        OrchestrationError error = FromKernel(OrchestrationErrorCode::ScheduleFailed,
                                              "schedule " + Quoted(schedule) + " on World " + Quoted(slot.description.name) + " failed",
                                              std::move(kernelError));
        error.failure = record(std::move(failure));
        return Base::Err(std::move(error));
    }
    slot.lastSchedule = std::string(schedule);
    return {};
}

namespace {

// Whether an entry point's error only passes through (trace declaration D4 (i)): a schedule the kernel ran
// failed (the kernel's Fail marked it where it was produced), or the Product had already ended (the World broke
// earlier, marked then). Everything else is produced here, including a DropOldFirst replacement that fails at
// one of the orchestration layer's own steps and ends the Product with it (a failure with this call's code).
bool PassesThrough(const OrchestrationError& error) noexcept {
    switch (error.code) {
    case OrchestrationErrorCode::ScheduleFailed:
    case OrchestrationErrorCode::ReplaceScheduleFailed:
    case OrchestrationErrorCode::WorldInterrupted:
    case OrchestrationErrorCode::WorldMissing: return true;
    default: return false;
    }
}

// One trace span around an entry point's steps (trace declaration D6); marked when they return an error. It
// only tells.
template <class Steps>
Base::Result<void, OrchestrationError> Traced(const std::string_view entry, Steps&& steps) {
    Base::TraceSpan span;
    span.Begin("orchestration", entry);
    auto result = steps();
    if (!result && span.Active()) {
        const OrchestrationError& error = result.error();
        span.MarkError(std::string(ToString(error.code)) + ": " + error.message, !PassesThrough(error));
    }
    return result;
}

} // namespace

Base::Result<void, OrchestrationError> Product::RunGameInit() {
    return Traced("RunGameInit", [&] { return RunGameInitSteps(); });
}

Base::Result<void, OrchestrationError> Product::RunFrame() {
    return Traced("RunFrame", [&] { return RunFrameSteps(); });
}

Base::Result<void, OrchestrationError> Product::RunSchedule(std::string_view world, std::string_view schedule) {
    return Traced("RunSchedule", [&] { return RunScheduleSteps(world, schedule); });
}

Base::Result<void, OrchestrationError> Product::ReplaceWorld(std::string_view name, ReplaceRequest request) {
    return Traced("ReplaceWorld", [&] { return ReplaceWorldSteps(name, std::move(request)); });
}

Base::Result<void, OrchestrationError> Product::InsertSystems(std::string_view world, std::string owner, InsertFn build) {
    return Traced("InsertSystems", [&] { return InsertSystemsSteps(world, std::move(owner), std::move(build)); });
}

Base::Result<void, OrchestrationError> Product::RunGameInitSteps() {
    RequireNotRunning();
    GYO_ASSERT(!gameInitRan_);
    if (auto ended = Ended()) return Base::Err(std::move(*ended));
    gameInitRan_ = true;
    if (worlds_.empty()) return {};
    for (const std::string& schedule : gameInit_) {
        if (auto ran = RunOn(worlds_.front(), schedule, "RunGameInit"); !ran) return ran;
    }
    return {};
}

Base::Result<void, OrchestrationError> Product::RunFrameSteps() {
    RequireNotRunning();
    // Every World is checked before any runs: no half frame over a broken World.
    if (auto ended = Ended()) return Base::Err(std::move(*ended));
    if (!gameInitRan_) {
        if (auto ran = RunGameInit(); !ran) return ran;
    }
    // Each World: its "in" exchange functions, its round, its "out" ones (exchange declaration 5.1).
    // A World breaking in its round stops here: neither its "out" nor any later World's exchange runs.
    for (WorldSlot& slot : worlds_) {
        if (slot.lease) continue;  // lent to another thread: its holder runs it (O-13)
        if (auto ran = RunRound(slot); !ran) return ran;
    }
    return {};
}

Base::Result<void, OrchestrationError> Product::Exchange(std::vector<Detail::ExchangeEntry>& entries,
                                                                    Kernel::World& world, std::string_view side) {
    for (Detail::ExchangeEntry& entry : entries) {
        if (!entry.fn) continue;
        auto done = entry.fn(world);
        if (!done) {
            OrchestrationError error = std::move(done).error();
            error.code = OrchestrationErrorCode::ExchangeFailed;
            error.message = "exchange function (" + std::string(side) + ") of " + Quoted(entry.unit) + " on World " +
                            Quoted(world.Name()) + " failed" + (error.message.empty() ? std::string() : ": " + error.message);
            return Base::Err(std::move(error));
        }
    }
    return {};
}

Base::Result<void, OrchestrationError> Product::RunRound(WorldSlot& slot, Detail::LeaseShared* lease) {
    Kernel::World& world = slot.executor->GetWorld();
    const WorldGeneration* before = world.Resource<WorldGeneration>();
    const bool generating = before != nullptr && before->generating;
    // A generating frame starts with nobody holding.
    if (generating) world.SetResource(WorldGeneration{.generating = true, .held = false});
    // The exchange functions' "in", after the step above (world-gen declaration 5.1; exchange declaration W-2):
    // an "in" that holds generation for this frame is not cleared by it.
    if (auto exchanged = Exchange(slot.exchangeIn, world, "in"); !exchanged) return exchanged;
    for (const std::string& schedule : slot.frame) {
        if (auto ran = RunOn(slot, schedule, lease == nullptr ? "RunFrame" : "WorldLease::RunRound", lease); !ran) return ran;
    }
    // Judged once the whole frame list ran (an interrupted World is not written): nobody held, generation ends.
    if (generating) {
        const WorldGeneration* after = world.Resource<WorldGeneration>();
        if (after != nullptr && !after->held) world.SetResource(WorldGeneration{.generating = false, .held = false});
    }
    // The exchange functions' "out", after the judgement: the "out" of the frame generation ended in reads it ended.
    if (auto exchanged = Exchange(slot.exchangeOut, world, "out"); !exchanged) return exchanged;
    return {};
}

namespace {

OrchestrationError LentError(const std::string_view world) {
    return OrchestrationError(OrchestrationErrorCode::WorldLent,
                              "World " + Quoted(world) + " is lent to another thread (O-13): its holder runs it until it is returned");
}

} // namespace

Base::Result<void, OrchestrationError> Product::RunScheduleSteps(std::string_view world, std::string_view schedule) {
    RequireNotRunning();
    if (auto ended = Ended()) return Base::Err(std::move(*ended));
    WorldSlot* slot = Find(world);
    if (slot == nullptr) {
        return Base::Err(OrchestrationError(OrchestrationErrorCode::UnknownWorld, "no World " + Quoted(world)));
    }
    if (slot->lease) return Base::Err(LentError(world));
    if (!gameInitRan_) {
        if (auto ran = RunGameInit(); !ran) return ran;
    }
    return RunOn(*slot, schedule, "RunSchedule");
}

RunnerExit Product::Run() {
    GYO_ASSERT(!ran_);
    ran_ = true;
    if (runners_.size() == 1 && runners_.front().runner) return runners_.front().runner(*this);
    // The default runner: one frame (Bevy run_once). It writes nothing: it returns the
    // reason with the error's context, and the layer that decides what follows (a product's
    // main) writes it once and decides (the error is handled in one place).
    auto ran = RunFrame();
    if (ran) return RunnerExit{};
    const OrchestrationError& error = ran.error();
    if (error.failure) {
        // E1: a World is broken and the Product cannot run again.
        return RunnerExit{RunnerExitCode::Failure, "the Product ended: " + error.failure->Text()};
    }
    // E2: this request was not done (an exchange function failed; the World is healthy).
    return RunnerExit{RunnerExitCode::Failure,
                      std::string(ToString(error.code)) + ": " + error.message + (error.detail.empty() ? "" : "\n" + error.detail)};
}

Kernel::WorldExecutor& Product::World(std::string_view name) {
    WorldSlot* slot = Find(name);
    GYO_ASSERT(slot != nullptr && slot->executor != nullptr);
    GYO_ASSERT(!slot->lease && "Product::World: the World is lent to another thread (use its WorldLease)");
    return *slot->executor;
}

bool Product::Generating(std::string_view name) const {
    const WorldSlot* slot = Find(name);
    GYO_ASSERT(slot != nullptr && slot->executor != nullptr && "Product::Generating: no World of that name, or its place is empty");
    GYO_ASSERT(!slot->lease && "Product::Generating: the World is lent to another thread (W-5: through its out)");
    const WorldGeneration* generation = slot->executor->GetWorld().Resource<WorldGeneration>();
    return generation != nullptr && generation->generating;
}

bool Product::HasWorld(std::string_view name) const noexcept {
    const WorldSlot* slot = Find(name);
    return slot != nullptr && slot->executor != nullptr;
}

Base::Result<void, OrchestrationError> Product::ReplaceWorldSteps(std::string_view name, ReplaceRequest request) {
    RequireNotRunning();
    // A replacement does not mend a broken World (v8-arch-gyo2-broken-world-ends-product).
    if (auto ended = Ended()) return Base::Err(std::move(*ended));
    WorldSlot* found = Find(name);
    if (found == nullptr) {
        return Base::Err(OrchestrationError(OrchestrationErrorCode::UnknownWorld, "no World " + Quoted(name)));
    }
    WorldSlot& slot = *found;
    if (slot.lease) return Base::Err(LentError(name));
    // O-8 (v8-arch-gyo2-sdl-extract): until R1b decides how, replacing a World another one extracts
    // from is a Programmer Error: the reader's Registry keeps a pointer to this World's Registry,
    // which the swap destroys. Checked before anything is dropped.
    for (const WorldSlot& other : worlds_) {
        GYO_ASSERT(other.description.extractFrom != slot.description.name &&
                   "ReplaceWorld: another World extracts from this one (source replacement comes with R1b)");
    }
    const bool firstWorld = &slot == &worlds_.front();
    const bool dropOldFirst = request.order == ReplaceOrder::DropOldFirst;
    // The schedule running when a step failed (WorldFailure::schedule of a DropOldFirst failure).
    std::string failedSchedule;
    auto fail = [&](OrchestrationError error) -> Base::Result<void, OrchestrationError> {
        if (failure_) {
            // Game initialization run from here broke the first World.
            error.failure = *failure_;
        } else if (dropOldFirst) {
            // The old World is gone and the new one failed: the place stays empty, the Product has ended.
            WorldFailure failure;
            failure.world = slot.description.name;
            failure.kind = WorldFailureKind::ReplacementFailed;
            failure.entry = "ReplaceWorld";
            failure.schedule = failedSchedule;
            if (!error.kernel.empty()) failure.kernelCode = error.kernel.front().code;
            failure.where = std::string(ToString(error.code)) + ": " + error.message +
                            (error.reporterCode.empty() ? std::string() : " (" + error.reporterCode + ")") +
                            (error.detail.empty() ? std::string() : ": " + error.detail);
            failure.lastSchedule = slot.lastSchedule;
            error.failure = Break(std::move(failure));
        }
        return Base::Err(std::move(error));
    };

    // The failure a DropOldFirst replacement records if the new World does not come: made
    // now, while the old World is healthy, so recording it after an exception allocates
    // nothing but the optional text below (error levels P12, D4).
    WorldFailure dropFailure;
    if (dropOldFirst) {
        dropFailure.product = name_;
        dropFailure.world = slot.description.name;
        dropFailure.kind = WorldFailureKind::ReplacementFailed;
        dropFailure.entry = "ReplaceWorld";
        dropFailure.lastSchedule = slot.lastSchedule;
        dropFailure.where = "an exception left the replacement";
    }

    // DropOldFirst: the marked values are taken out, then the old World and its Registry go.
    std::vector<std::pair<Detail::CarryEntry, std::shared_ptr<void>>> moved;
    if (dropOldFirst) {
        try {
            for (const Detail::CarryEntry& entry : slot.carries) {
                if (std::shared_ptr<void> box = entry.take(slot.executor->GetWorld())) moved.emplace_back(entry, std::move(box));
            }
        } catch (...) {
            // Nothing is dropped yet: what was taken goes back and the exception goes on.
            for (auto& [entry, box] : moved) entry.put(slot.executor->GetWorld(), box);
            throw;
        }
        slot.executor.reset();
        slot.registry.reset();
        // An empty place from here on is this replacement's, until it swaps or records the failure.
        slot.replacing = true;
    }

    Registration registration;
    std::unique_ptr<Kernel::WorldExecutor> fresh;
    Kernel::World* world = nullptr;
    // A failure gives the values back to the old World (BuildNewFirst: it goes
    // on, healthy). DropOldFirst has no old World: they go with the failed one
    // (the Product has ended).
    auto giveBack = [&](std::vector<std::pair<Detail::CarryEntry, std::shared_ptr<void>>>& values) {
        if (!dropOldFirst) {
            for (auto& [entry, box] : values) {
                if (!box) {
                    if (world == nullptr) continue;
                    box = entry.take(*world);
                    if (!box) continue;
                }
                entry.put(slot.executor->GetWorld(), box);
            }
        }
        values.clear();
    };
    // Error levels D10: game initialization started here on the new first World.
    bool initRunHere = false;
    // Error levels P12: every step after the old World is gone is in this try, so an
    // exception from the registration, the executor or a carry-over records the failure too.
    try {
        // Step 1: register the World again from the description (v8-arch-gyo2-restore-reruns-registration).
        std::vector<OrchestrationStartupError> errors;
        std::vector<UnitReportedError> unitErrors;
        WorldStartupResult result{slot.description.name, std::nullopt, false};
        // O-12: a reader registers again with the current Registry of the World it extracts from
        // (healthy: checked by Ended above; not this World, which no World extracts from).
        const Kernel::Registry* source = nullptr;
        if (!slot.description.extractFrom.empty()) source = worlds_[ExtractSourceIndex(slot)].registry.get();
        registration = Register(slot, firstWorld, source, result, errors, unitErrors);
        if (!registration.registry || !errors.empty() || !unitErrors.empty()) {
            std::vector<WorldStartupResult> results;
            results.push_back(std::move(result));
            ProductStartupReport report(std::move(results), std::move(errors), std::move(unitErrors));
            giveBack(moved);
            return fail(OrchestrationError(OrchestrationErrorCode::ReplaceRegistrationFailed,
                                           "registering World " + Quoted(name) + " again failed", report.detail));
        }
        fresh = MakeExecutor(slot, *registration.registry);
        world = &fresh->GetWorld();

        // Step 2: carry the marked Resources over, before every prepare function (section 3.12):
        // what the old World has of the marked types (DropOldFirst took them out above).
        if (!dropOldFirst) {
            for (const Detail::CarryEntry& entry : slot.carries) {
                if (std::shared_ptr<void> box = entry.take(slot.executor->GetWorld())) moved.emplace_back(entry, std::move(box));
            }
        }
        for (const auto& [entry, box] : moved) {
            if (!entry.registered(*world)) {
                giveBack(moved);
                return fail(OrchestrationError(OrchestrationErrorCode::ReplaceCarryOverFailed,
                                               "World " + Quoted(name) + ": a Resource marked to carry over" +
                                                   (entry.typeName.empty() ? std::string() : " (" + entry.typeName + ")") +
                                                   " is not registered in the new World"));
            }
        }
        for (auto& [entry, box] : moved) entry.put(*world, box);

        // Step 3: prepare functions.
        if (auto prepared = Prepare(slot, *world, registration.unitPrepares, request.extra ? &request.extra : nullptr); !prepared) {
            giveBack(moved);
            return fail(std::move(prepared).error());
        }
        // Step 4: game initialization when it has not run (on the new World when it
        // replaces the first one; only right after startup), then beforeSwap.
        if (!gameInitRan_) {
            if (firstWorld) {
                gameInitRan_ = true;
                initRunHere = true;
                for (const std::string& schedule : gameInit_) {
                    failedSchedule = schedule;
                    auto ran = fresh->Run(schedule);
                    if (!ran) {
                        giveBack(moved);
                        // Error levels D10: under BuildNewFirst the World that broke was never the
                        // Product's; game initialization is not counted as run and the old first
                        // World runs it on the next entry point (O-1).
                        if (!dropOldFirst) gameInitRan_ = false;
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
            failedSchedule = schedule;
            auto ran = RunWith(*fresh, slot, schedule);
            if (!ran) {
                giveBack(moved);
                return fail(FromKernel(OrchestrationErrorCode::ReplaceScheduleFailed,
                                       "schedule " + Quoted(schedule) + " on the new World " + Quoted(name) + " failed",
                                       std::move(ran).error()));
            }
        }
    } catch (...) {
        giveBack(moved);
        if (!dropOldFirst) {
            if (initRunHere) gameInitRan_ = false;  // D10, as for an Err
            throw;
        }
        if (!failure_) {
            // Recorded before the exception goes on (error levels P12, D4): the place is
            // empty, the Product has ended. dropFailure was made before the old World went.
            dropFailure.schedule.swap(failedSchedule);
            try {
                throw;
            } catch (const std::bad_alloc&) {
                dropFailure.kind = WorldFailureKind::ResourceExhausted;
                try {
                    dropFailure.where = "std::bad_alloc left the replacement";
                } catch (...) {
                    // No memory for the text: the preset one stays.
                }
            } catch (...) {
            }
            Break(std::move(dropFailure));
        }
        throw;
    }

    // Step 5: swap. The old executor first, then its Registry; the exchange functions come with the new build.
    slot.replacing = false;
    slot.executor.reset();
    slot.registry.reset();
    slot.registry = std::move(registration.registry);
    slot.executor = std::move(fresh);
    slot.frame = std::move(registration.frame);
    slot.carries = std::move(registration.carries);
    slot.exchangeIn = std::move(registration.exchangeIn);
    slot.exchangeOut = std::move(registration.exchangeOut);
    slot.lastSchedule.clear();
    slot.nextInsertSeq = 0;
    return {};
}

Base::Result<void, OrchestrationError> Product::InsertSystemsSteps(std::string_view worldName, std::string owner,
                                                                           InsertFn build) {
    RequireNotRunning();
    if (auto ended = Ended()) return Base::Err(std::move(*ended));
    WorldSlot* slot = Find(worldName);
    if (slot == nullptr) {
        return Base::Err(OrchestrationError(OrchestrationErrorCode::UnknownWorld, "no World " + Quoted(worldName)));
    }
    if (slot->lease) return Base::Err(LentError(worldName));
    const bool taken = owner == name_ ||
                       std::any_of(units_.begin(), units_.end(), [&](const UnitSlot& unit) { return unit.row == owner; });
    if (taken) {
        return Base::Err(OrchestrationError(OrchestrationErrorCode::OwnerTaken,
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
        return Base::Err(FromKernel(OrchestrationErrorCode::InsertRefused,
                                              "World " + Quoted(worldName) + " declares no dynamic insert (owner " + Quoted(owner) + ")",
                                              std::move(registered).error()));
    }
    auto built = slot->registry->BuildPlans();
    if (!built) {
        const Kernel::StartupReport& report = built.error();
        return Base::Err(OrchestrationError(OrchestrationErrorCode::InsertBuildFailed,
                                                      "BuildPlans of World " + Quoted(worldName) + " failed", report.detail));
    }
    return {};
}

} // namespace GYO::Engine::Orchestration

namespace GYO::Engine::Orchestration {

// ---- Lending a World to another thread (O-13; live-frame-sim-thread declaration 4.2)

Base::Result<WorldLease, OrchestrationError> Product::Lend(std::string_view world) {
    RequireNotRunning();
    if (auto ended = Ended()) return Base::Err(std::move(*ended));
    WorldSlot* slot = Find(world);
    if (slot == nullptr) {
        return Base::Err(OrchestrationError(OrchestrationErrorCode::UnknownWorld, "no World " + Quoted(world)));
    }
    const auto refused = [&](const std::string& why) {
        return Base::Err(OrchestrationError(OrchestrationErrorCode::LendRefused, "World " + Quoted(world) + " is not lent: " + why));
    };
    if (!gameInitRan_) return refused("game initialization has not run (O-1: it runs before a World is lent)");
    if (slot->lease) return refused("it is lent already");
    if (!slot->description.extractFrom.empty()) return refused("it extracts from World " + Quoted(slot->description.extractFrom));
    for (const WorldSlot& other : worlds_) {
        if (other.description.extractFrom == slot->description.name) {
            return refused("World " + Quoted(other.description.name) + " extracts from it");
        }
    }
    WorldLease lease;
    lease.product_ = this;
    lease.slot_ = static_cast<std::size_t>(slot - worlds_.data());
    lease.shared_ = std::make_shared<Detail::LeaseShared>();
    slot->lease = lease.shared_;
    return lease;
}

void Product::Return(WorldLease&& lease) {
    GYO_ASSERT(lease.product_ == this && lease.slot_ < worlds_.size() && worlds_[lease.slot_].lease == lease.shared_ &&
               "Product::Return: the lease is not this Product's");
    WorldSlot& slot = worlds_[lease.slot_];
    // A World that broke while lent ends the Product (O-11): recorded here if no entry point has seen it yet.
    if (!failure_ && lease.shared_->broken.load(std::memory_order_acquire)) Break(*lease.shared_->failure);
    slot.lease.reset();
    lease.product_ = nullptr;
    lease.shared_.reset();
}

WorldLease::WorldLease(WorldLease&& other) noexcept
    : product_(std::exchange(other.product_, nullptr)), slot_(other.slot_), shared_(std::move(other.shared_)) {}

WorldLease& WorldLease::operator=(WorldLease&& other) noexcept {
    if (this != &other) {
        GYO_ASSERT(product_ == nullptr && "a WorldLease is overwritten before it was returned");
        product_ = std::exchange(other.product_, nullptr);
        slot_ = other.slot_;
        shared_ = std::move(other.shared_);
    }
    return *this;
}

WorldLease::~WorldLease() { GYO_ASSERT(product_ == nullptr && "a WorldLease is dropped without Product::Return"); }

Base::Result<void, OrchestrationError> WorldLease::RunRound() {
    GYO_ASSERT(product_ != nullptr);
    Product::WorldSlot& slot = product_->worlds_[slot_];
    if (shared_->broken.load(std::memory_order_relaxed)) {
        const WorldFailure& failure = *shared_->failure;
        OrchestrationError error(OrchestrationErrorCode::WorldInterrupted,
                                 "World " + Quoted(failure.world) + " is interrupted: the Product has ended", failure.Text());
        error.failure = failure;
        return Base::Err(std::move(error));
    }
    return product_->RunRound(slot, shared_.get());
}

Kernel::World& WorldLease::World() {
    GYO_ASSERT(product_ != nullptr);
    return product_->worlds_[slot_].executor->GetWorld();
}

const std::string& WorldLease::Name() const {
    GYO_ASSERT(product_ != nullptr);
    return product_->worlds_[slot_].description.name;
}

} // namespace GYO::Engine::Orchestration
