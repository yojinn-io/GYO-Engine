#include "gyo/ui_editor/CommandLine.hpp"
#include "gyo/ui_editor/DocumentSession.hpp"
#include "gyo/ui_editor/EditorGui.hpp"
#include "gyo/ui_editor/ReadOnlyAssetCatalog.hpp"
#include "gyo/ui_editor/UiDocumentBridge.hpp"

#include <iostream>
#include <string_view>
#include <vector>

namespace {

using Gyo::Tools::UiEditor::Diagnostic;
using Gyo::Tools::UiEditor::DiagnosticSeverity;

void PrintDiagnostics(const std::vector<Diagnostic>& diagnostics) {
    for (const Diagnostic& diagnostic : diagnostics) {
        const char* severity = "info";
        if (diagnostic.severity == DiagnosticSeverity::Warning) {
            severity = "warning";
        } else if (diagnostic.severity == DiagnosticSeverity::Error) {
            severity = "error";
        }
        std::cerr << severity << ": "
                  << (diagnostic.path.empty() ? "/" : diagnostic.path) << ": "
                  << diagnostic.message << '\n';
    }
}

int Validate(const Gyo::Tools::UiEditor::CommandLineOptions& options) {
    using namespace Gyo::Tools::UiEditor;
    if (!options.inputPath.has_value()) {
        std::cerr << "error: --validate requires a JSON path\n";
        return 2;
    }

    ReadOnlyAssetCatalog catalog;
    const ReadOnlyAssetCatalog* catalogPointer = nullptr;
    if (options.assetRoot.has_value()) {
        std::string error;
        const bool mounted = options.catalogPath.has_value()
            ? catalog.Mount(*options.catalogPath, *options.assetRoot, error)
            : catalog.MountRoot(*options.assetRoot, error);
        if (!mounted) {
            std::cerr << "error: failed to mount catalog: " << error << '\n';
            return 3;
        }
        catalogPointer = &catalog;
    }

    DocumentSession session;
    SessionResult opened = session.Open(*options.inputPath);
    PrintDiagnostics(opened.diagnostics);
    if (!opened) {
        if (!opened.error.empty()) {
            std::cerr << "error: " << opened.error << '\n';
        }
        return opened.saveFailure == SaveFailure::InvalidDocument ? 4 : 3;
    }

    const std::vector<Diagnostic> diagnostics = session.Validate(catalogPointer);
    PrintDiagnostics(diagnostics);
    if (HasErrors(diagnostics)) {
        return 4;
    }
    std::cout << "valid: " << options.inputPath->string() << '\n';
    return 0;
}

} // namespace

int main(const int argc, char* argv[]) {
    using namespace Gyo::Tools::UiEditor;
    std::vector<std::string_view> arguments;
    arguments.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0U);
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }

    const CommandLineResult parsed = ParseCommandLine(arguments);
    if (!parsed) {
        std::cerr << "error: " << parsed.error << "\n\n" << CommandLineHelp();
        return 2;
    }
    if (parsed.options->mode == EditorMode::Help) {
        std::cout << CommandLineHelp();
        return 0;
    }
    if (parsed.options->mode == EditorMode::Validate) {
        return Validate(*parsed.options);
    }
    return RunEditorGui(*parsed.options);
}
