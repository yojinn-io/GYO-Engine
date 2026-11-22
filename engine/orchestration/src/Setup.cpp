// The contexts and lists the orchestration layer hands out (declaration
// sections 3.3, 3.6 to 3.8, 3.12).

#include "engine/orchestration/Setup.hpp"
#include "engine/orchestration/detail/RegistrationUnit.hpp"

#include <algorithm>

namespace GYO::Engine::Orchestration {

namespace Detail {
RegistrationUnit::~RegistrationUnit() = default;
void RegistrationUnit::Configure(ProductSetup&) {}
void RegistrationUnit::Finish(const Kernel::FrozenRegistry&, WorldFinish&) {}
void RegistrationUnit::Check(const Kernel::FrozenRegistry&, WorldCheck&) {}
} // namespace Detail

void ScheduleList::Append(std::string schedule) {
    names_.push_back(std::move(schedule));
}

void ScheduleList::InsertBefore(std::string_view anchor, std::string schedule) {
    const auto at = std::find(names_.begin(), names_.end(), anchor);
    if (at == names_.end()) {
        missingAnchors_.emplace_back(anchor);
        return;
    }
    names_.insert(at, std::move(schedule));
}

void ScheduleList::InsertAfter(std::string_view anchor, std::string schedule) {
    const auto at = std::find(names_.begin(), names_.end(), anchor);
    if (at == names_.end()) {
        missingAnchors_.emplace_back(anchor);
        return;
    }
    names_.insert(at + 1, std::move(schedule));
}

void ProductSetup::InstallRunner(std::string name, Runner runner) {
    runners_.push_back(Installed{std::move(name), installer_, std::move(runner)});
}

void WorldSetup::Prepare(PrepareFn fn) {
    prepares_->push_back(std::move(fn));
}

void WorldSetup::ExchangeIn(ExchangeFn fn) {
    exchanges_->in.push_back(Detail::ExchangeEntry{unit_, std::move(fn)});
}

void WorldSetup::ExchangeOut(ExchangeFn fn) {
    exchanges_->out.push_back(Detail::ExchangeEntry{unit_, std::move(fn)});
}

void ProductSetup::Report(std::string code, std::string message) {
    reports_->push_back(UnitReportedError{installer_, std::string(), std::move(code), std::move(message)});
}

void ProductSetup::OnDestroy(std::function<void()> fn) {
    destroys_->push_back(std::move(fn));
}

void WorldSetup::AddCarry(Detail::CarryEntry entry) {
    for (Detail::CarryEntry& existing : *carries_) {
        if (existing.key == entry.key) {
            existing = std::move(entry);
            return;
        }
    }
    carries_->push_back(std::move(entry));
}

void WorldFinish::Report(std::string code, std::string message) {
    sink_->push_back(UnitReportedError{unit_, worldName_, std::move(code), std::move(message)});
}

void WorldCheck::Report(std::string code, std::string message) {
    sink_->push_back(UnitReportedError{unit_, worldName_, std::move(code), std::move(message)});
}

} // namespace GYO::Engine::Orchestration
