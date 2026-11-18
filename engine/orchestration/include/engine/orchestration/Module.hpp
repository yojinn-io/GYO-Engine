#pragma once

#include "engine/orchestration/detail/RegistrationUnit.hpp"

namespace GYO::Engine::Orchestration {

// The registration unit of a Module: a part we maintain and whose interface is
// ours (engine/config/modules.csv; v8-arch-gyo2-module-plugin-owner-definition).
// Modules come before Plugins, and a Module never depends on a Plugin
// (v8-arch-gyo2-module-plugin-registers). Subclass names end in Module (N20).
class Module : public Detail::RegistrationUnit {
protected:
    Module() = default;
};

} // namespace GYO::Engine::Orchestration
