#include "gyo/ui_editor/CommandLine.hpp"
#include "gyo/ui_editor/DocumentSession.hpp"
#include "gyo/ui_editor/FileService.hpp"
#include "gyo/ui_editor/ReadOnlyAssetCatalog.hpp"
#include "gyo/ui_editor/UiDocumentBridge.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {

using namespace Gyo::Tools::UiEditor;

int failures{};

void Expect(const bool condition, const std::string_view message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAILED: " << message << '\n';
    }
}

struct TemporaryDirectory final {
    TemporaryDirectory() {
        const auto nonce = std::chrono::steady_clock::now()
                               .time_since_epoch()
                               .count();
        path = std::filesystem::temp_directory_path() /
               ("gyo-ui-editor-tests-" + std::to_string(nonce));
        std::filesystem::create_directories(path);
    }

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }

    std::filesystem::path path;
};

void TestCommandLine() {
    constexpr std::string_view arguments[]{
        "--open", "working.json", "--output", "export.json",
        "--asset-root", "app-assets",
    };
    const CommandLineResult parsed = ParseCommandLine(arguments);
    Expect(static_cast<bool>(parsed), "valid editor command line parses");
    if (parsed) {
        Expect(
            parsed.options->catalogPath ==
                std::filesystem::path{"app-assets"} / "asset_catalog.json",
            "asset-root derives the read-only catalog path");
    }

    constexpr std::string_view invalid[]{"--validate", "a.json", "--output", "b.json"};
    Expect(!ParseCommandLine(invalid), "validate rejects an output path");
}

void TestDefaultAndUndo() {
    DocumentSession session;
    const std::vector<Diagnostic> diagnostics = session.Validate(nullptr);
    Expect(!HasErrors(diagnostics), "new document is schema-valid without a catalog");
    Expect(
        !diagnostics.empty() &&
            diagnostics.front().severity == DiagnosticSeverity::Warning,
        "unmounted catalog leaves an unresolved AssetId warning");

    session.EditDocument()["design_canvas"]["size"][0] = 1920.0;
    session.CommitEdit("resize canvas");
    Expect(session.CanUndo(), "committed edit is undoable");
    Expect(session.Undo(), "undo restores a document snapshot");
    Expect(
        session.Document()["design_canvas"]["size"][0] == 1280.0,
        "undo restores the previous canvas width");
    Expect(session.Redo(), "redo reapplies a document snapshot");
    Expect(
        session.Document()["design_canvas"]["size"][0] == 1920.0,
        "redo restores the edited canvas width");
}

void TestTransactionCoalescingAndSavePoint(const std::filesystem::path& root) {
    DocumentSession session;
    Expect(session.IsDirty(), "a new untitled document starts unsaved");

    const std::filesystem::path output = root / "save-point" / "menu.json";
    Expect(
        static_cast<bool>(session.Export(output, nullptr)),
        "initial export establishes a save point");
    Expect(!session.IsDirty(), "successful export clears dirty state");

    session.BeginEdit("drag canvas width");
    session.EditDocument()["design_canvas"]["size"][0] = 1360.0;
    session.UpdateEdit();
    session.EditDocument()["design_canvas"]["size"][0] = 1440.0;
    session.UpdateEdit();
    session.EndEdit();
    Expect(session.IsDirty(), "committed transaction marks the document dirty");
    Expect(session.CanUndo(), "coalesced transaction creates one undo step");
    Expect(session.Undo(), "coalesced transaction can be undone");
    Expect(
        session.Document()["design_canvas"]["size"][0] == 1280.0,
        "one undo restores the state from before every drag update");
    Expect(!session.CanUndo(), "multi-frame transaction produced only one undo step");
    Expect(!session.IsDirty(), "undoing to the save point clears dirty state");
    Expect(session.Redo(), "coalesced transaction can be redone");
    Expect(session.IsDirty(), "redo away from the save point restores dirty state");

    session.BeginEdit("cancel canvas width");
    session.EditDocument()["design_canvas"]["size"][0] = 1600.0;
    session.UpdateEdit();
    session.CancelEdit();
    Expect(
        session.Document()["design_canvas"]["size"][0] == 1440.0,
        "cancelling a transaction restores its original snapshot");
}

void TestExportAndExternalModification(const std::filesystem::path& root) {
    DocumentSession session;
    const std::filesystem::path output = root / "working" / "menu.json";
    SessionResult exported = session.Export(output, nullptr);
    Expect(static_cast<bool>(exported), "valid document exports without a catalog");

    const TextFileResult text = ReadTextFile(output);
    Expect(static_cast<bool>(text), "exported JSON can be read");
    if (text) {
        Expect(text.text->starts_with("{\n  \"schema\": \"gyo.ui\""),
               "export uses GYO codec field ordering and two-space indentation");
        Expect(text.text->ends_with('\n'), "export has exactly a trailing newline");
        const DocumentParseResult parsed = UiDocumentBridge::Parse(*text.text);
        Expect(static_cast<bool>(parsed), "export round-trips through GYO::Ui codec");
        if (parsed) {
            Expect(
                UiDocumentBridge::SerializeCanonical(*parsed.document) == *text.text,
                "valid export is exactly the codec canonical representation");
        }
    }

    session.EditDocument()["design_canvas"]["size"][1] = 900.0;
    session.CommitEdit("resize canvas");
    Expect(
        static_cast<bool>(WriteTextFileAtomically(output, "externally changed\n")),
        "test can simulate an external edit");
    const SessionResult guarded = session.Save(nullptr);
    Expect(
        !guarded && guarded.saveFailure == SaveFailure::ExternalModification,
        "save detects external output changes");
}

void TestReadOnlyCatalogAndExportGuard(const std::filesystem::path& root) {
    const std::filesystem::path assetRoot = root / "app-assets";
    const std::filesystem::path catalogPath = assetRoot / "asset_catalog.json";
    const std::string catalog = R"({
  "version": 1,
  "assets": [
    {"id":"font.default","type":"font","path":"fonts/default.ttf"},
    {"id":"texture.logo","type":"texture","path":"textures/logo.png"}
  ]
}
)";
    Expect(
        static_cast<bool>(WriteTextFileAtomically(catalogPath, catalog)),
        "catalog fixture is written");

    ReadOnlyAssetCatalog mounted;
    std::string error;
    Expect(
        mounted.Mount(catalogPath, assetRoot, error),
        "asset catalog mounts read-only");
    Expect(mounted.Find("texture.logo") != nullptr, "catalog lookup exposes AssetIds");

    DocumentSession session;
    const SessionResult guarded = session.Export(assetRoot / "ui" / "menu.json", &mounted);
    Expect(
        !guarded &&
            guarded.saveFailure == SaveFailure::OutputInsideMountedAssetRoot,
        "export rejects every path below the mounted app asset root");
    Expect(
        !std::filesystem::exists(assetRoot / "ui" / "menu.json"),
        "guarded export writes no app asset");

    DocumentSession missing;
    missing.EditDocument()["fonts"]["default"] = "font.missing";
    missing.CommitEdit("use missing font");
    const std::vector<Diagnostic> missingDiagnostics = missing.Validate(&mounted);
    Expect(
        HasErrors(missingDiagnostics),
        "a missing catalog AssetId is a validation error");
    const SessionResult missingExport = missing.Export(
        root / "working" / "missing.json", &mounted);
    Expect(
        !missingExport && missingExport.saveFailure == SaveFailure::InvalidDocument,
        "a missing catalog AssetId blocks export");

    DocumentSession wrongType;
    wrongType.EditDocument()["fonts"]["default"] = "texture.logo";
    wrongType.CommitEdit("use texture as font");
    const std::vector<Diagnostic> wrongTypeDiagnostics =
        wrongType.Validate(&mounted);
    Expect(
        HasErrors(wrongTypeDiagnostics),
        "a catalog AssetId with the wrong type is a validation error");
    const SessionResult wrongTypeExport = wrongType.Export(
        root / "working" / "wrong-type.json", &mounted);
    Expect(
        !wrongTypeExport &&
            wrongTypeExport.saveFailure == SaveFailure::InvalidDocument,
        "a wrong-type catalog AssetId blocks export");
}

} // namespace

int main() {
    TemporaryDirectory temporary;
    TestCommandLine();
    TestDefaultAndUndo();
    TestTransactionCoalescingAndSavePoint(temporary.path);
    TestExportAndExternalModification(temporary.path);
    TestReadOnlyCatalogAndExportGuard(temporary.path);
    if (failures != 0) {
        std::cerr << failures << " editor core test(s) failed\n";
        return 1;
    }
    std::cout << "GYO UI editor core tests passed\n";
    return 0;
}
