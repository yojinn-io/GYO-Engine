#include "gyo/ui_editor/CommandLine.hpp"
#include "gyo/ui_editor/DocumentSession.hpp"
#include "gyo/ui_editor/EditorGui.hpp"
#include "gyo/framework/HeadlessShell.hpp"
#include "gyo/framework/ReadOnlyAssetCatalog.hpp"
#include "gyo/ui_editor/UiDocumentBridge.hpp"

#include "engine/base/Error.hpp"
#include "engine/os/Files.hpp"

#include <iostream>
#include <span>
#include <string>
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
    namespace Framework = Gyo::Tools::Framework;
    if (!options.inputPath.has_value()) {
        std::cerr << "error: --validate requires a JSON path\n";
        return 2;
    }

    Framework::ReadOnlyAssetCatalog catalog;
    const Framework::ReadOnlyAssetCatalog* catalogPointer = nullptr;
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
    std::cout << "valid: " << Engine::Os::Files::PathForMessage(*options.inputPath) << '\n';
    return 0;
}

int Run(const std::span<const std::string> commandLine) {
    using namespace Gyo::Tools::UiEditor;
    std::vector<std::string_view> arguments;
    arguments.reserve(commandLine.size() > 1 ? commandLine.size() - 1 : 0U);
    for (std::size_t index = 1; index < commandLine.size(); ++index) {
        arguments.emplace_back(commandLine[index]);
    }

    const auto parsed = ParseCommandLine(arguments);
    if (!parsed) {
        std::cerr << "error: " << Engine::Base::Describe(parsed.error()) << "\n\n" << CommandLineHelp();
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

} // namespace

int main(const int argc, char* argv[]) {
    // The shell turns the command line into UTF-8 (the narrow argv is the
    // ANSI code page on Windows) and shows UTF-8 console output as UTF-8.
    return Gyo::Tools::Framework::RunHeadlessShell(argc, argv, Run, [](const std::string_view reason) {
        std::cerr << "error: " << reason << "\n\n" << Gyo::Tools::UiEditor::CommandLineHelp();
        return 2;
    });
}
