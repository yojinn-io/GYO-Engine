#pragma once

#include "engine/base/Assert.hpp"
#include "engine/base/Result.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/TypeKey.hpp"
#include "engine/kernel/World.hpp"
#include "engine/orchestration/OrchestrationError.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The contexts the orchestration layer hands to registration units and to the
// product's own callbacks (declaration sections 3.3, 3.6 to 3.8, 3.12; the
// Product-level buffer, options, Configure reports, OnDestroy and the exchange
// functions: docs/architecture/target/v2/exchange-declaration.zh-Hant.md
// sections 5.1 and 5.2).

namespace GYO::Engine::Orchestration {

class Product;
class ModuleBuffer;

// Options the World description carries for the Modules to read, keyed by type
// (section 3.4). The orchestration layer stores and hands them over; it never
// reads them (O-2).
class OptionBag {
public:
    template <class T>
    void Set(T value) {
        for (auto& [key, box] : boxes_) {
            if (key == Kernel::KeyOf<T>()) {
                box = std::make_shared<T>(std::move(value));
                return;
            }
        }
        boxes_.emplace_back(Kernel::KeyOf<T>(), std::make_shared<T>(std::move(value)));
    }
    // nullptr when the description gives none.
    template <class T>
    [[nodiscard]] const T* Get() const {
        for (const auto& [key, box] : boxes_) {
            if (key == Kernel::KeyOf<T>()) return static_cast<const T*>(box.get());
        }
        return nullptr;
    }

private:
    std::vector<std::pair<Kernel::TypeKey, std::shared_ptr<const void>>> boxes_;
};

// A list of schedule names with Bevy's insertion rules (section 3.6;
// MainScheduleOrder::insert_startup_before / _after): an anchor is resolved
// when the insertion is made; one that is not there is recorded (O-E5) and the
// name is not inserted. Of two names inserted before one anchor, the later one
// ends next to the anchor. A name may appear twice (it runs twice); an anchor
// that appears twice means its first appearance.
class ScheduleList {
public:
    void Append(std::string schedule);
    void InsertBefore(std::string_view anchor, std::string schedule);
    void InsertAfter(std::string_view anchor, std::string schedule);
    [[nodiscard]] const std::vector<std::string>& Names() const noexcept { return names_; }

private:
    friend class Product;
    explicit ScheduleList(std::vector<std::string> initial) : names_(std::move(initial)) {}
    std::vector<std::string> names_;
    // Anchors an insertion did not find.
    std::vector<std::string> missingAnchors_;
};

// A prepare function (section 3.8): called after a World is built and before
// its first Run, on the Ready World. To fail, return an OrchestrationError with
// code PrepareFailed and the author's own code in reporterCode.
using PrepareFn = std::function<Base::Result<void, OrchestrationError>(Kernel::World&)>;

// An exchange function (exchange declaration section 5.1): called by RunFrame
// on a Ready World, before ("in") or after ("out") the World's round, outside
// any Run. To fail, return an OrchestrationError with the function's own code
// in reporterCode (its code field is replaced by ExchangeFailed); RunFrame then
// stops and returns ExchangeFailed (E2: the World stays healthy). It may keep pointers to objects of the Product-level
// ModuleBuffer, never to objects of a World build's ModuleBuffer (that one is
// let go once the prepare functions have run). It waits for no other thread.
using ExchangeFn = std::function<Base::Result<void, OrchestrationError>(Kernel::World&)>;

enum class RunnerExitCode : unsigned char { Success = 1, Failure };

struct RunnerExit {
    RunnerExitCode code = RunnerExitCode::Success;
    std::string reason;
};

// The runner (section 3.7): owns the loop and calls the Product's entry points.
using Runner = std::function<RunnerExit(Product&)>;

// Product level, once (Configure).
class ProductSetup {
public:
    ProductSetup(const ProductSetup&) = delete;
    ProductSetup& operator=(const ProductSetup&) = delete;

    [[nodiscard]] const std::string& ProductName() const noexcept { return productName_; }
    // The game initialization list: run once, on the first World, before the first frame.
    [[nodiscard]] ScheduleList& GameInit() noexcept { return *gameInit_; }
    // At most one runner per Product; a second is O-E4.
    void InstallRunner(std::string name, Runner runner);
    // The Product's buffer (exchange declaration 5.2): what a unit Creates here
    // the units after it find in their Configure, and every unit in each World build.
    [[nodiscard]] ModuleBuffer& Buffer() noexcept { return *buffer_; }
    // The product description's option of this type, or nullptr. What the product
    // or a test gives is here from the first unit's Configure on (the product's
    // own configure runs after every unit).
    template <class T>
    [[nodiscard]] const T* Options() const {
        return options_->Get<T>();
    }
    // An error in the unit's own Configure: no World is built; the other units'
    // Configure still run and the errors are collected (ProductStartupReport::unitErrors).
    // A unit that finds no object of a dependency here (that dependency reported) reports
    // and returns too; it does not assert.
    void Report(std::string code, std::string message);
    // Called once when the Product is destroyed, after every World and before the
    // Product's buffer, in the reverse order of the calls to OnDestroy; also when
    // Product::Build fails after this Configure. Every object of the buffer is still
    // there while it runs. Work it is responsible for must have stopped when it returns.
    void OnDestroy(std::function<void()> fn);

private:
    friend class Product;
    ProductSetup(std::string productName, std::string installer, ScheduleList& gameInit, ModuleBuffer& buffer,
                 const OptionBag& options, std::vector<UnitReportedError>& reports,
                 std::vector<std::function<void()>>& destroys)
        : productName_(std::move(productName)), installer_(std::move(installer)), gameInit_(&gameInit), buffer_(&buffer),
          options_(&options), reports_(&reports), destroys_(&destroys) {}
    std::string productName_;
    std::string installer_;
    ScheduleList* gameInit_;
    ModuleBuffer* buffer_;
    const OptionBag* options_;
    std::vector<UnitReportedError>* reports_;
    std::vector<std::function<void()>>* destroys_;
    struct Installed {
        std::string name;
        std::string installer;
        Runner runner;
    };
    std::vector<Installed> runners_;
};

namespace Detail {
// One Resource type marked to carry over (section 3.12): its key and typed moves.
struct CarryEntry {
    Kernel::TypeKey key;
    std::string typeName;
    // Whether the World has the type registered.
    std::function<bool(const Kernel::World&)> registered;
    // Takes the value out of a World (nullptr when the World has none).
    std::function<std::shared_ptr<void>(Kernel::World&)> take;
    // Puts a taken value into a World; false when the World has no such type registered.
    std::function<bool(Kernel::World&, std::shared_ptr<void>&)> put;
};
// One registered exchange function and the unit (or the product) that registered it.
struct ExchangeEntry {
    std::string unit;
    ExchangeFn fn;
};
} // namespace Detail

// A place, keyed by type, where one party puts an object the others use. Two
// uses share this one rule ("one owner Creates, the others Find"):
//   - Each World build has one (authority-rollback declaration section 6; the
//     user's decision, 2026-11-20): a unit's registration table, its author
//     entry, used while the World is built. Made anew each time the World is
//     built, kept with that build's prepare functions and then let go. Build
//     (WorldSetup::Buffer) writes; the buffer is frozen when every Build has
//     run, and Finish and Check (WorldFinish, WorldCheck) only read.
//   - The Product has one (exchange declaration section 5.2; the user's
//     decision, 2026-11-22): an object one unit, the product or its runner puts
//     for the others for the Product's whole life (a store, a pool, a device).
//     Never frozen; it outlives every World of the Product. An object another
//     thread uses brings its own synchronization.
// Both:
//   - One type, one owner: the owner Creates it (a second Create of a type is a
//     GYO_ASSERT), the others Find it; Find does not make an empty one.
//   - A Create, or a writable Find, after the freeze is a GYO_ASSERT. A write
//     through a reference kept from Build is the owner's to refuse.
//   - The orchestration layer never reads what is inside (O-2).
//   - Objects are destroyed in the reverse order of their Create.
//   - One thread (CCR-5): no lock.
class ModuleBuffer {
public:
    ModuleBuffer() = default;
    ModuleBuffer(const ModuleBuffer&) = delete;
    ModuleBuffer& operator=(const ModuleBuffer&) = delete;
    // The reverse order of Create (a vector's own order of destruction is not specified).
    ~ModuleBuffer() {
        while (!entries_.empty()) entries_.pop_back();
    }

    // The owner's object of type T, made from args.
    template <class T, class... Args>
    T& Create(Args&&... args) {
        GYO_ASSERT(!frozen_ && "ModuleBuffer::Create after Build: the buffer is frozen");
        GYO_ASSERT(Raw(Kernel::KeyOf<T>()) == nullptr && "two registration units create one type in the ModuleBuffer");
        auto box = std::make_shared<T>(std::forward<Args>(args)...);
        T& made = *box;
        entries_.emplace_back(Kernel::KeyOf<T>(), std::move(box));
        return made;
    }
    // The object of type T to write, or nullptr (a World build's buffer: during Build only).
    template <class T>
    [[nodiscard]] T* Find() {
        GYO_ASSERT(!frozen_ && "a writable ModuleBuffer::Find after Build: the buffer is frozen");
        return static_cast<T*>(Raw(Kernel::KeyOf<T>()));
    }
    // The object of type T to read, or nullptr.
    template <class T>
    [[nodiscard]] const T* Find() const {
        return static_cast<const T*>(Raw(Kernel::KeyOf<T>()));
    }
    [[nodiscard]] bool Frozen() const noexcept { return frozen_; }

private:
    friend class Product;
    void Freeze() noexcept { frozen_ = true; }
    [[nodiscard]] void* Raw(Kernel::TypeKey key) const noexcept {
        for (const auto& [entry, box] : entries_) {
            if (entry == key) return box.get();
        }
        return nullptr;
    }
    std::vector<std::pair<Kernel::TypeKey, std::shared_ptr<void>>> entries_;
    bool frozen_ = false;
};

// Once per World a unit (or the product's own build) joins (Build).
class WorldSetup {
public:
    WorldSetup(const WorldSetup&) = delete;
    WorldSetup& operator=(const WorldSetup&) = delete;

    [[nodiscard]] const std::string& WorldName() const noexcept { return worldName_; }
    // The World this one extracts from (WorldDescription::extractFrom, O-12); empty: none. A unit that runs an
    // extract schedule puts it in the frame list only when this is not empty (else O-E9).
    [[nodiscard]] const std::string& ExtractFrom() const noexcept { return extractFrom_; }
    [[nodiscard]] Kernel::Registrar& Registrar() noexcept { return *registrar_; }
    // The description's option of this type, or nullptr.
    template <class T>
    [[nodiscard]] const T* Options() const {
        return options_->Get<T>();
    }
    // This World's frame list.
    [[nodiscard]] ScheduleList& Frame() noexcept { return *frame_; }
    // This World build's ModuleBuffer, to write.
    [[nodiscard]] ModuleBuffer& Buffer() noexcept { return *buffer_; }
    // The Product's buffer (exchange declaration 5.2), to write.
    [[nodiscard]] ModuleBuffer& ProductBuffer() noexcept { return *productBuffer_; }
    // A prepare function of this World, after the earlier units' ones.
    void Prepare(PrepareFn fn);
    // Exchange functions of this World (exchange declaration 5.1), after the earlier
    // units' ones: RunFrame calls the "in" ones before the World's round and the
    // "out" ones after it. Kept with the World until it is replaced or destroyed.
    void ExchangeIn(ExchangeFn fn);
    void ExchangeOut(ExchangeFn fn);
    // Marks Resource T to be carried to the new World when this World is
    // replaced (section 3.12). The value is moved whole and never read; it must
    // not hold anything that points into the old World (an EntityId, a pointer,
    // a handle): that is the author's responsibility.
    template <class T>
    void CarryOver(std::string typeName = {}) {
        Detail::CarryEntry entry;
        entry.key = Kernel::KeyOf<T>();
        entry.typeName = std::move(typeName);
        entry.registered = [](const Kernel::World& world) -> bool { return world.Types().ResourceIdOf<T>().has_value(); };
        entry.take = [](Kernel::World& world) -> std::shared_ptr<void> {
            if (!world.Types().ResourceIdOf<T>()) return nullptr;
            if (world.State() == Kernel::WorldState::Interrupted) {
                // An interrupted World only reads (spec E.5) and is destroyed
                // right after this: the value is moved out through the read
                // view, never written back into that World.
                const T* kept = world.Resource<T>();
                if (kept == nullptr) return nullptr;
                return std::make_shared<T>(std::move(*const_cast<T*>(kept)));
            }
            T* value = world.ResourceMut<T>();
            if (value == nullptr) return nullptr;
            std::shared_ptr<void> box = std::make_shared<T>(std::move(*value));
            world.RemoveResource<T>();
            return box;
        };
        entry.put = [](Kernel::World& world, std::shared_ptr<void>& box) -> bool {
            if (!world.Types().ResourceIdOf<T>()) return false;
            world.SetResource<T>(std::move(*static_cast<T*>(box.get())));
            box.reset();
            return true;
        };
        AddCarry(std::move(entry));
    }

private:
    friend class Product;
    struct Exchanges {
        std::vector<Detail::ExchangeEntry> in;
        std::vector<Detail::ExchangeEntry> out;
    };
    WorldSetup(std::string worldName, std::string unit, Kernel::Registrar& registrar, const OptionBag& options,
               ScheduleList& frame, std::vector<PrepareFn>& prepares, std::vector<Detail::CarryEntry>& carries,
               ModuleBuffer& buffer, ModuleBuffer& productBuffer, Exchanges& exchanges)
        : worldName_(std::move(worldName)), unit_(std::move(unit)), registrar_(&registrar), options_(&options),
          frame_(&frame), buffer_(&buffer), productBuffer_(&productBuffer), prepares_(&prepares), carries_(&carries),
          exchanges_(&exchanges) {}
    void AddCarry(Detail::CarryEntry entry);
    std::string worldName_;
    std::string extractFrom_;
    std::string unit_;
    Kernel::Registrar* registrar_;
    const OptionBag* options_;
    ScheduleList* frame_;
    ModuleBuffer* buffer_;
    ModuleBuffer* productBuffer_;
    std::vector<PrepareFn>* prepares_;
    std::vector<Detail::CarryEntry>* carries_;
    Exchanges* exchanges_;
};

// After the World's Plans are built (Finish).
class WorldFinish {
public:
    WorldFinish(const WorldFinish&) = delete;
    WorldFinish& operator=(const WorldFinish&) = delete;

    [[nodiscard]] const std::string& WorldName() const noexcept { return worldName_; }
    // This World build's ModuleBuffer, frozen: to read.
    [[nodiscard]] const ModuleBuffer& Buffer() const noexcept { return *buffer_; }
    // The Product's buffer (exchange declaration 5.2), to write: it is never frozen.
    [[nodiscard]] ModuleBuffer& ProductBuffer() const noexcept { return *productBuffer_; }
    // An error in the unit's own code (collected into the startup report).
    void Report(std::string code, std::string message);

private:
    friend class Product;
    WorldFinish(std::string worldName, std::string unit, std::vector<UnitReportedError>& sink, const ModuleBuffer& buffer,
                ModuleBuffer& productBuffer)
        : worldName_(std::move(worldName)), unit_(std::move(unit)), sink_(&sink), buffer_(&buffer),
          productBuffer_(&productBuffer) {}
    std::string worldName_;
    std::string unit_;
    std::vector<UnitReportedError>* sink_;
    const ModuleBuffer* buffer_;
    ModuleBuffer* productBuffer_;
};

// After every unit's Finish of the World (Check). The World is not built yet:
// what it will be built with is handed over (section 3.4).
class WorldCheck {
public:
    WorldCheck(const WorldCheck&) = delete;
    WorldCheck& operator=(const WorldCheck&) = delete;

    [[nodiscard]] const std::string& WorldName() const noexcept { return worldName_; }
    // What Kernel::WorldOptions::dynamicInsert of the World will be.
    [[nodiscard]] bool DynamicInsert() const noexcept { return dynamicInsert_; }
    template <class T>
    [[nodiscard]] const T* Options() const {
        return options_->Get<T>();
    }
    // This World build's ModuleBuffer, frozen: to read.
    [[nodiscard]] const ModuleBuffer& Buffer() const noexcept { return *buffer_; }
    // The Product's buffer (exchange declaration 5.2), to write: it is never frozen.
    [[nodiscard]] ModuleBuffer& ProductBuffer() const noexcept { return *productBuffer_; }
    void Report(std::string code, std::string message);

private:
    friend class Product;
    WorldCheck(std::string worldName, std::string unit, bool dynamicInsert, const OptionBag& options,
               std::vector<UnitReportedError>& sink, const ModuleBuffer& buffer, ModuleBuffer& productBuffer)
        : worldName_(std::move(worldName)), unit_(std::move(unit)), dynamicInsert_(dynamicInsert), options_(&options),
          sink_(&sink), buffer_(&buffer), productBuffer_(&productBuffer) {}
    std::string worldName_;
    std::string unit_;
    bool dynamicInsert_;
    const OptionBag* options_;
    std::vector<UnitReportedError>* sink_;
    const ModuleBuffer* buffer_;
    ModuleBuffer* productBuffer_;
};

} // namespace GYO::Engine::Orchestration
