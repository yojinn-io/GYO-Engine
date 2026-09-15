#pragma once

#include <string>

namespace Gyo::Tools::UiEditor {

enum class DiagnosticSeverity {
    Info,
    Warning,
    Error,
};

struct Diagnostic final {
    DiagnosticSeverity severity{DiagnosticSeverity::Info};
    std::string path;
    std::string message;
};

} // namespace Gyo::Tools::UiEditor
