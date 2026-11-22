#include "engine/save_version/SaveVersionModule.hpp"

#include <string>

namespace GYO::Engine::SaveVersion {

Base::Result<void, Orchestration::OrchestrationError> Policy::Check(std::uint32_t saved) const {
    const auto refuse = [&](std::string_view code, const char* what) {
        return Base::Err(Orchestration::OrchestrationError(
            Orchestration::OrchestrationErrorCode::PrepareFailed,
            std::string("the save's version ") + std::to_string(saved) + " is " + what,
            "taken: " + std::to_string(oldestSupported_) + " to " + std::to_string(current_), std::string(code)));
    };
    if (saved < oldestSupported_) return refuse(SaveVersionCodes::tooOld, "older than the oldest supported");
    if (saved > current_) return refuse(SaveVersionCodes::tooNew, "newer than the current version");
    return {};
}

void SaveVersionModule::Configure(Orchestration::ProductSetup& setup) {
    const SaveVersionOptions* options = setup.Options<SaveVersionOptions>();
    if (options == nullptr) {
        setup.Report(std::string(SaveVersionCodes::optionsMissing), "save_version is enabled without SaveVersionOptions");
        return;
    }
    if (options->oldestSupported > options->current) {
        setup.Report(std::string(SaveVersionCodes::optionsOrder),
                     "SaveVersionOptions: oldestSupported " + std::to_string(options->oldestSupported) +
                         " is greater than current " + std::to_string(options->current));
        return;
    }
    setup.Buffer().Create<Policy>(options->current, options->oldestSupported);
}

void SaveVersionModule::Build(Orchestration::WorldSetup&) {}

} // namespace GYO::Engine::SaveVersion
