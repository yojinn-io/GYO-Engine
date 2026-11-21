#pragma once

#include "engine/orchestration/detail/RegistrationUnit.hpp"

namespace GYO::Engine::Orchestration {

// The registration unit of a Plugin: a third-party part, officially maintained
// elsewhere, plugged in (config/plugins.csv;
// v8-arch-gyo2-module-plugin-owner-definition). A Plugin may depend on a
// Module; products list the Plugins they use (none by default). Subclass names
// end in Plugin (N20).
class Plugin : public Detail::RegistrationUnit {
protected:
    Plugin() = default;
};

} // namespace GYO::Engine::Orchestration
