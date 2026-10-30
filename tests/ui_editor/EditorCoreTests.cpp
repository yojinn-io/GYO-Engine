#include "gyo/ui_editor/CommandLine.hpp"
#include "gyo/ui_editor/DocumentSession.hpp"
#include "gyo/ui_editor/FileService.hpp"
#include "gyo/ui_editor/GizmoMath.hpp"
#include "gyo/ui_editor/PathDialog.hpp"
#include "gyo/ui_editor/ReadOnlyAssetCatalog.hpp"
#include "gyo/ui_editor/UiDocumentBridge.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

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
    const auto parsed = ParseCommandLine(arguments);
    Expect(static_cast<bool>(parsed), "valid editor command line parses");
    if (parsed) {
        Expect(
            !parsed->catalogPath.has_value() &&
                parsed->assetRoot == std::filesystem::path{"app-assets"},
            "asset-root selects the content manifest without guessing a catalog");
    }

    constexpr std::string_view invalid[]{"--validate", "a.json", "--output", "b.json"};
    Expect(!ParseCommandLine(invalid), "validate rejects an output path");
    constexpr std::string_view local[]{"--asset-catalog", "catalog.json"};
    const auto single = ParseCommandLine(local);
    Expect(single && single->catalogPath == "catalog.json" &&
        single->assetRoot == ".", "explicit catalog retains the local-root mode");
    constexpr std::string_view both[]{"--asset-root", "assets", "--asset-catalog", "elsewhere/catalog.json"};
    const auto explicitRoot = ParseCommandLine(both);
    Expect(explicitRoot && explicitRoot->assetRoot == "assets" &&
        explicitRoot->catalogPath == "elsewhere/catalog.json", "explicit catalog overrides manifest selection");

    // The arguments are UTF-8 on every platform (Os::Console::CommandLineUtf8):
    // a path outside any ANSI code page keeps its characters.
    constexpr std::string_view utf8[]{"--open", "\xE6\x97\xA5\xE6\x9C\xAC/\xED\x95\x9C.json"};
    const auto unicode = ParseCommandLine(utf8);
    Expect(unicode && unicode->inputPath &&
        unicode->inputPath->generic_u8string() == u8"\u65E5\u672C/\uD55C.json",
        "a UTF-8 path argument keeps its characters");
    constexpr std::string_view notUtf8[]{"--open", "\xFF.json"};
    Expect(!ParseCommandLine(notUtf8), "a path argument that is not UTF-8 is refused");
}

void TestContentManifestMount(const std::filesystem::path& root) {
    const auto assets = root / "manifest-assets";
    Expect(static_cast<bool>(WriteTextFileAtomically(assets / "content.json",
        R"({"version":1,"catalogs":["fonts.json","images.json"],"shader_bundles":[]})")), "write content manifest");
    Expect(static_cast<bool>(WriteTextFileAtomically(assets / "fonts.json",
        R"({"version":1,"assets":[{"id":"font.default","type":"font","path":"font.bin"}]})")), "write font catalog");
    Expect(static_cast<bool>(WriteTextFileAtomically(assets / "images.json",
        R"({"version":1,"assets":[{"id":"texture.logo","type":"texture","path":"image.bmp"}]})")), "write image catalog");
    ReadOnlyAssetCatalog catalog;
    Expect(static_cast<bool>(catalog.MountRoot(assets)), "mount all manifest catalogs");
    Expect(catalog.IsMounted() && catalog.CatalogPath().empty() && catalog.Assets().size() == 2,
        "root mode exposes the complete content snapshot");
    const auto refs = UiDocumentBridge::AssetReferences(nlohmann::json{
        {"fonts", {{"default", "font.default"}}}, {"image", {{"texture_asset", "texture.logo"}}}});
    Expect(!HasErrors(ValidateCatalogReferences(refs, &catalog)), "font and image references resolve across catalogs");
    for (const auto bad : {
            R"({"version":1,"assets":[{"id":"font.default","type":"texture","path":"image.bmp"}]})",
            R"({"version":1,"assets":[{"id":"texture.logo","type":"texture","path":"../escape.bmp"}]})",
            "broken"}) {
        Expect(static_cast<bool>(WriteTextFileAtomically(assets / "images.json", bad)), "replace candidate catalog");
        Expect(!catalog.MountRoot(assets), "invalid candidate is rejected");
        Expect(catalog.Find("texture.logo") != nullptr && catalog.Find("font.default") != nullptr,
            "failed candidate preserves the previous complete snapshot");
    }
    Expect(static_cast<bool>(WriteTextFileAtomically(assets / "content.json",
        R"({"version":1,"catalogs":["missing.json"],"shader_bundles":[]})")), "declare missing catalog");
    Expect(!catalog.MountRoot(assets), "missing declared catalog does not partially mount");
    Expect(catalog.Assets().size() == 2, "missing file preserves old mount");
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
    auto exported = session.Export(output, nullptr);
    Expect(static_cast<bool>(exported), "valid document exports without a catalog");

    const auto text = ReadTextFile(output);
    Expect(static_cast<bool>(text), "exported JSON can be read");
    if (text) {
        Expect(text->starts_with("{\n  \"schema\": \"gyo.ui\""),
               "export uses GYO codec field ordering and two-space indentation");
        Expect(text->ends_with('\n'), "export has exactly a trailing newline");
        const DocumentParseResult parsed = UiDocumentBridge::Parse(*text);
        Expect(static_cast<bool>(parsed), "export round-trips through GYO::Ui codec");
        if (parsed) {
            Expect(
                UiDocumentBridge::SerializeCanonical(*parsed.document) == *text,
                "valid export is exactly the codec canonical representation");
        }
    }

    session.EditDocument()["design_canvas"]["size"][1] = 900.0;
    session.CommitEdit("resize canvas");
    Expect(
        static_cast<bool>(WriteTextFileAtomically(output, "externally changed\n")),
        "test can simulate an external edit");
    const auto guarded = session.Save(nullptr);
    Expect(
        !guarded && guarded.error().code == SessionErrorCode::ExternalModification,
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
    Expect(
        static_cast<bool>(mounted.Mount(catalogPath, assetRoot)),
        "asset catalog mounts read-only");
    Expect(mounted.Find("texture.logo") != nullptr, "catalog lookup exposes AssetIds");

    DocumentSession session;
    const auto guarded = session.Export(assetRoot / "ui" / "menu.json", &mounted);
    Expect(
        !guarded &&
            guarded.error().code == SessionErrorCode::OutputInsideMountedAssetRoot,
        "export rejects every path below the mounted app asset root");
    Expect(
        !std::filesystem::exists(assetRoot / "ui" / "menu.json"),
        "guarded export writes no app asset");

    // A path that reaches the asset root through a symbolic link is below it
    // too: links in the existing part of a path are resolved, as
    // std::filesystem::weakly_canonical did.
    const std::filesystem::path working = root / "working";
    std::filesystem::create_directories(working);
    std::error_code linkError;
    std::filesystem::create_directory_symlink(assetRoot, working / "assets-link", linkError);
    if (linkError) {
        // Windows needs Developer Mode or a privilege to create links.
        std::cout << "symbolic links unavailable here: " << linkError.message() << '\n';
    } else {
        DocumentSession linked;
        const auto throughLink = linked.Export(working / "assets-link" / "ui" / "linked.json", &mounted);
        Expect(
            !throughLink &&
                throughLink.error().code == SessionErrorCode::OutputInsideMountedAssetRoot,
            "export rejects a path that reaches the asset root through a symbolic link");
        Expect(
            !std::filesystem::exists(assetRoot / "ui" / "linked.json"),
            "the linked export writes no app asset");
    }

    DocumentSession missing;
    missing.EditDocument()["fonts"]["default"] = "font.missing";
    missing.CommitEdit("use missing font");
    const std::vector<Diagnostic> missingDiagnostics = missing.Validate(&mounted);
    Expect(
        HasErrors(missingDiagnostics),
        "a missing catalog AssetId is a validation error");
    const auto missingExport = missing.Export(
        root / "working" / "missing.json", &mounted);
    Expect(
        !missingExport && missingExport.error().code == SessionErrorCode::InvalidDocument,
        "a missing catalog AssetId blocks export");

    DocumentSession wrongType;
    wrongType.EditDocument()["fonts"]["default"] = "texture.logo";
    wrongType.CommitEdit("use texture as font");
    const std::vector<Diagnostic> wrongTypeDiagnostics =
        wrongType.Validate(&mounted);
    Expect(
        HasErrors(wrongTypeDiagnostics),
        "a catalog AssetId with the wrong type is a validation error");
    const auto wrongTypeExport = wrongType.Export(
        root / "working" / "wrong-type.json", &mounted);
    Expect(
        !wrongTypeExport &&
            wrongTypeExport.error().code == SessionErrorCode::InvalidDocument,
        "a wrong-type catalog AssetId blocks export");
}

// A content root the Windows ANSI code page cannot name (os-path-utf8-sweep,
// D63): the catalog's resolved paths keep their characters, the export guard
// still sees the root, and an export under such a folder is written.
void TestRootOutsideAnsiCodePage(const std::filesystem::path& root) {
    const std::filesystem::path folder = root / std::filesystem::path(u8"\u8cc7\u7522-\ud55c");
    const std::filesystem::path assetRoot = folder / "app-assets";
    const std::filesystem::path catalogPath = assetRoot / "asset_catalog.json";
    Expect(
        static_cast<bool>(WriteTextFileAtomically(catalogPath, R"({
  "version": 1,
  "assets": [
    {"id":"font.default","type":"font","path":"fonts/default.ttf"},
    {"id":"texture.logo","type":"texture","path":"textures/logo.png"}
  ]
}
)")),
        "catalog fixture is written under a root outside the ANSI code page");
    ReadOnlyAssetCatalog mounted;
    Expect(static_cast<bool>(mounted.Mount(catalogPath, assetRoot)),
        "a catalog mounts under a root outside the ANSI code page");
    const CatalogAsset* logo = mounted.Find("texture.logo");
    Expect(logo != nullptr && logo->resolvedPath == assetRoot / "textures" / "logo.png",
        "a resolved asset path keeps the characters of its root");

    DocumentSession guarded;
    const auto inside = guarded.Export(assetRoot / "ui" / "menu.json", &mounted);
    Expect(!inside && inside.error().code == SessionErrorCode::OutputInsideMountedAssetRoot,
        "the export guard sees a root outside the ANSI code page");

    DocumentSession exporter;
    const std::filesystem::path output = folder / std::filesystem::path(u8"\u51fa\u529b") / "menu.json";
    Expect(static_cast<bool>(exporter.Export(output, nullptr)), "an export under such a folder succeeds");
    Expect(std::filesystem::exists(output), "the export is written under such a folder");
}

void TestContentHashIsFnv1a(const std::filesystem::path& root) {
    // Pinned before FNV-1a moved to GYO::Base: external-change detection
    // compares stamps, so the content hash must stay the same function.
    const auto path = root / "content-hash.txt";
    Expect(static_cast<bool>(WriteTextFileAtomically(path, "foobar")), "content hash fixture is written");
    const auto stamp = ProbeFileStamp(path);
    Expect(stamp && stamp.value().contentHash == 0x85944171f73967e8ULL,
        "file content hash is the 64-bit FNV-1a of the text");
}

} // namespace

// A dialog result posted from another thread (as SDL may call back) is taken
// once, in posting order, by the editor's thread.
void TestPathDialogInbox() {
    PathDialogInbox inbox;
    std::thread poster([&inbox] {
        inbox.Post({.purpose = PathDialogPurpose::OpenDocument,
                    .outcome = PathDialogOutcome::Chosen,
                    .path = "a.json"});
        inbox.Post({.purpose = PathDialogPurpose::AssetFolder,
                    .outcome = PathDialogOutcome::Cancelled});
    });
    poster.join();
    const auto taken = inbox.Take();
    Expect(taken.size() == 2, "inbox returns every posted result");
    Expect(taken.size() == 2 && taken[0].path == "a.json" &&
               taken[1].purpose == PathDialogPurpose::AssetFolder,
        "inbox keeps posting order");
    Expect(inbox.Take().empty(), "a taken result is not returned again");
}

// Canvas gizmo rule without ImGui. The parent is a 1280x720 canvas at the
// screen origin; a drag starts on a handle and moves the mouse by delta.
GizmoFrame CanvasFrame(const Engine::Math::Vec2 anchorMin, const Engine::Math::Vec2 anchorMax,
    const Engine::Math::Rect element, const Engine::Math::Vec2 pivot) {
    return {{0.0F, 0.0F, 1280.0F, 720.0F}, element, anchorMin, anchorMax, pivot};
}

Engine::Math::Vec2 DragFromHandle(const GizmoField field, const GizmoFrame& frame,
    const Engine::Math::Vec2 delta, const GizmoSnap snap = {}) {
    const GizmoPoints points = ComputeGizmoPoints(frame);
    const Engine::Math::Vec2 handle = field == GizmoField::AnchorMin ? points.anchorMinHandle
        : field == GizmoField::AnchorMax ? points.anchorMaxHandle : points.pivotHandle;
    return DragGizmoHandle(field, frame, handle + delta, snap);
}

bool Near(const Engine::Math::Vec2 value, const float x, const float y) {
    return std::abs(value.x - x) < 1e-5F && std::abs(value.y - y) < 1e-5F;
}

void TestGizmoHandlesAndHit() {
    // The lobby buttons: both anchors at the parent's top-left corner.
    const GizmoFrame lobby = CanvasFrame({0.0F, 0.0F}, {0.0F, 0.0F}, {220.0F, 238.0F, 840.0F, 48.0F}, {0.0F, 0.0F});
    const GizmoPoints points = ComputeGizmoPoints(lobby);
    Expect(Near(points.anchorMinHandle, -9.0F, -9.0F) && Near(points.anchorMaxHandle, 9.0F, -9.0F),
        "point anchors put both anchor handles beside the parent corner, not the element");
    Expect(Near(points.pivotHandle, 220.0F, 247.0F), "the pivot handle sits below the pivot");
    Expect(HitGizmoHandle(points, {9.0F, -9.0F}) == GizmoField::AnchorMax, "a click on a handle grabs it");
    Expect(HitGizmoHandle(points, {0.0F, -9.0F}) == GizmoField::AnchorMax,
        "a tie between the anchor handles goes to anchor max");
    Expect(!HitGizmoHandle(points, {9.0F, 2.0F}).has_value(), "beyond the 10 px radius nothing is grabbed");
}

// Characterizes the reported drag defect (ui-editor-review.md S-5): with equal
// anchors each anchor handle moves in only one direction per axis, and the
// [0, 1] range stops anchors at the canvas edge. These assertions describe the
// current rule; a fix changes them under AGENTS.md section 14.4.
void TestGizmoDragCurrentRule() {
    const GizmoFrame centered = CanvasFrame({0.5F, 0.5F}, {0.5F, 0.5F}, {520.0F, 324.0F, 240.0F, 72.0F}, {0.5F, 0.5F});
    Expect(Near(DragFromHandle(GizmoField::AnchorMin, centered, {0.0F, -50.0F}), 0.5F, 0.5F - 50.0F / 720.0F),
        "anchor min moves up");
    Expect(Near(DragFromHandle(GizmoField::AnchorMin, centered, {0.0F, 50.0F}), 0.5F, 0.5F),
        "anchor min cannot move down past equal anchor max");
    Expect(Near(DragFromHandle(GizmoField::AnchorMin, centered, {-50.0F, 0.0F}), 0.5F - 50.0F / 1280.0F, 0.5F),
        "anchor min moves left");
    Expect(Near(DragFromHandle(GizmoField::AnchorMin, centered, {50.0F, 0.0F}), 0.5F, 0.5F),
        "anchor min cannot move right past equal anchor max");
    Expect(Near(DragFromHandle(GizmoField::AnchorMax, centered, {0.0F, -50.0F}), 0.5F, 0.5F),
        "anchor max cannot move up past equal anchor min (the reported defect)");
    Expect(Near(DragFromHandle(GizmoField::AnchorMax, centered, {0.0F, 50.0F}), 0.5F, 0.5F + 50.0F / 720.0F),
        "anchor max moves down");
    Expect(Near(DragFromHandle(GizmoField::AnchorMax, centered, {-50.0F, 0.0F}), 0.5F, 0.5F),
        "anchor max cannot move left past equal anchor min");
    Expect(Near(DragFromHandle(GizmoField::AnchorMax, centered, {50.0F, 0.0F}), 0.5F + 50.0F / 1280.0F, 0.5F),
        "anchor max moves right");

    const GizmoFrame lobby = CanvasFrame({0.0F, 0.0F}, {0.0F, 0.0F}, {220.0F, 238.0F, 840.0F, 48.0F}, {0.0F, 0.0F});
    for (const Engine::Math::Vec2 delta : {Engine::Math::Vec2{0.0F, -50.0F}, Engine::Math::Vec2{0.0F, 50.0F},
             Engine::Math::Vec2{-50.0F, 0.0F}, Engine::Math::Vec2{50.0F, 0.0F}}) {
        Expect(Near(DragFromHandle(GizmoField::AnchorMin, lobby, delta), 0.0F, 0.0F),
            "anchor min at the corner with equal anchor max cannot move at all");
    }
    Expect(Near(DragFromHandle(GizmoField::AnchorMax, lobby, {0.0F, -50.0F}), 0.0F, 0.0F),
        "anchor max at the top edge cannot move up");
    Expect(Near(DragFromHandle(GizmoField::AnchorMax, lobby, {50.0F, 50.0F}), 50.0F / 1280.0F, 50.0F / 720.0F),
        "anchor max moves right and down from the corner");
}

void TestGizmoDragSnapAndPivot() {
    const GizmoFrame centered = CanvasFrame({0.5F, 0.5F}, {0.5F, 0.5F}, {520.0F, 324.0F, 240.0F, 72.0F}, {0.5F, 0.5F});
    // 640 + 13 = 653 px snaps to 656 on an 8 px grid at scale 1.
    Expect(Near(DragFromHandle(GizmoField::AnchorMax, centered, {13.0F, 0.0F}, {true, 8.0F, 1.0F}), 656.0F / 1280.0F, 0.5F),
        "snap rounds the anchor to the grid in screen pixels");
    // At scale 2 the grid is 16 screen px: x 653 snaps to 656 (41 * 16), and
    // y 360 (22.5 cells) rounds to 368, so an untouched axis can move too.
    Expect(Near(DragFromHandle(GizmoField::AnchorMax, centered, {13.0F, 0.0F}, {true, 8.0F, 2.0F}), 656.0F / 1280.0F, 368.0F / 720.0F),
        "snap scales the grid by the design scale");
    Expect(Near(DragFromHandle(GizmoField::Pivot, centered, {-60.0F, -18.0F}), 0.25F, 0.25F),
        "pivot moves relative to the element's own bounds");
    Expect(Near(DragFromHandle(GizmoField::Pivot, centered, {500.0F, 0.0F}), 1.0F, 0.5F),
        "pivot stays inside the element");
}

int main() {
    TemporaryDirectory temporary;
    TestCommandLine();
    TestPathDialogInbox();
    TestGizmoHandlesAndHit();
    TestGizmoDragCurrentRule();
    TestGizmoDragSnapAndPivot();
    TestDefaultAndUndo();
    TestTransactionCoalescingAndSavePoint(temporary.path);
    TestExportAndExternalModification(temporary.path);
    TestReadOnlyCatalogAndExportGuard(temporary.path);
    TestRootOutsideAnsiCodePage(temporary.path);
    TestContentManifestMount(temporary.path);
    TestContentHashIsFnv1a(temporary.path);
    if (failures != 0) {
        std::cerr << failures << " editor core test(s) failed\n";
        return 1;
    }
    std::cout << "GYO UI editor core tests passed\n";
    return 0;
}
