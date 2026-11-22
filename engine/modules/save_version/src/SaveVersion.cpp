#include "engine/save_version/SaveVersion.hpp"

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

} // namespace GYO::Engine::SaveVersion
