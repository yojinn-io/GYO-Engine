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
        const auto mounted = options.catalogPath.has_value()
            ? catalog.Mount(*options.catalogPath, *options.assetRoot)
            : catalog.MountRoot(*options.assetRoot);
        if (!mounted) {
            std::cerr << "error: failed to mount catalog: "
                      << Engine::Base::Describe(mounted.error()) << '\n';
            return 3;
        }
        catalogPointer = &catalog;
    }

    DocumentSession session;
    const auto opened = session.Open(*options.inputPath);
    if (!opened) {
        PrintDiagnostics(opened.error().diagnostics);
        std::cerr << "error: " << Engine::Base::Describe(opened.error()) << '\n';
        return opened.error().code == SessionErrorCode::InvalidDocument ? 4 : 3;
    }
    PrintDiagnostics(opened->diagnostics);

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

    const auto parsed = ParseCommandLine(arguments);
    if (!parsed) {
        std::cerr << "error: " << parsed.error().message << "\n\n" << CommandLineHelp();
        return 2;
    }
    if (parsed->mode == EditorMode::Help) {
        std::cout << CommandLineHelp();
        return 0;
    }
    if (parsed->mode == EditorMode::Validate) {
        return Validate(*parsed);
    }
    return RunEditorGui(*parsed);
}
