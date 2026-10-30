#include "gyo/ui_editor/CommandLine.hpp"
#include "gyo/ui_editor/DocumentSession.hpp"
#include "gyo/ui_editor/FileService.hpp"
#include "gyo/ui_editor/GizmoMath.hpp"
#include "gyo/ui_editor/PathDialog.hpp"
#include "gyo/ui_editor/ReadOnlyAssetCatalog.hpp"
#include "gyo/ui_editor/UiDocumentBridge.hpp"

#include "engine/ui/UiDocumentCodec.hpp"
#include "engine/ui/UiRuntime.hpp"

#include <nlohmann/json.hpp>

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

// Canvas gizmo rule without ImGui. The parent is a 1280x720 design canvas at
// the screen origin, drawn at designScale screen pixels per design pixel.
using Engine::Math::Rect;
using Engine::Math::Vec2;

constexpr Vec2 kParentDesignSize{1280.0F, 720.0F};

// The element's rectangle in design pixels from the parent's corner, by the
// gyo.ui v1 layout rule.
Rect DesignRect(const GizmoRect& rect) {
    const Vec2 size{kParentDesignSize.x * (rect.anchorMax.x - rect.anchorMin.x) + rect.sizeDelta.x,
        kParentDesignSize.y * (rect.anchorMax.y - rect.anchorMin.y) + rect.sizeDelta.y};
    const Vec2 reference{
        kParentDesignSize.x * (rect.anchorMin.x + (rect.anchorMax.x - rect.anchorMin.x) * rect.pivot.x),
        kParentDesignSize.y * (rect.anchorMin.y + (rect.anchorMax.y - rect.anchorMin.y) * rect.pivot.y)};
    return {reference.x + rect.position.x - rect.pivot.x * size.x,
        reference.y + rect.position.y - rect.pivot.y * size.y, size.x, size.y};
}

GizmoFrame MakeFrame(const GizmoRect& rect, const float scale = 1.0F) {
    const Rect element = DesignRect(rect);
    return {{0.0F, 0.0F, kParentDesignSize.x * scale, kParentDesignSize.y * scale},
        {element.x * scale, element.y * scale, element.width * scale, element.height * scale}, rect, scale};
}

// Point anchors at the canvas centre; the element is 520,324 240x72.
const GizmoRect kCentered{{0.5F, 0.5F}, {0.5F, 0.5F}, {0.5F, 0.5F}, {0.0F, 0.0F}, {240.0F, 72.0F}};
// The lobby buttons: point anchors at the top-left corner; 220,238 840x48.
const GizmoRect kLobby{{0.0F, 0.0F}, {0.0F, 0.0F}, {0.0F, 0.0F}, {220.0F, 238.0F}, {840.0F, 48.0F}};
// Stretched anchors with a 20 px margin; 20,20 1240x680.
const GizmoRect kStretch{{0.0F, 0.0F}, {1.0F, 1.0F}, {0.5F, 0.5F}, {0.0F, 0.0F}, {-40.0F, -40.0F}};

GizmoRect Drag(const GizmoHandle handle, const GizmoFrame& frame, const Vec2 delta, const GizmoSnap snap = {}) {
    constexpr Vec2 grab{100.0F, 100.0F};
    return DragGizmo(handle, frame, grab, grab + delta, snap);
}

bool Near(const Vec2 value, const float x, const float y) {
    return std::abs(value.x - x) < 1e-5F && std::abs(value.y - y) < 1e-5F;
}

bool Near(const Rect value, const Rect expected) {
    return std::abs(value.x - expected.x) < 1e-3F && std::abs(value.y - expected.y) < 1e-3F &&
        std::abs(value.width - expected.width) < 1e-3F && std::abs(value.height - expected.height) < 1e-3F;
}

bool SameAnchors(const GizmoRect& left, const GizmoRect& right) {
    return Near(left.anchorMin, right.anchorMin.x, right.anchorMin.y) &&
        Near(left.anchorMax, right.anchorMax.x, right.anchorMax.y) &&
        Near(left.pivot, right.pivot.x, right.pivot.y);
}

void TestGizmoHit() {
    const GizmoFrame lobby = MakeFrame(kLobby);
    const GizmoPoints points = ComputeGizmoPoints(lobby);
    Expect(Near(points.anchorMinHandle, -9.0F, -9.0F) && Near(points.anchorMaxHandle, 9.0F, -9.0F),
        "point anchors put both anchor handles beside the parent corner, not the element");
    Expect(Near(points.pivotHandle, 220.0F, 247.0F), "the pivot handle sits below the pivot");
    Expect(HitGizmoHandle(lobby, {9.0F, -9.0F}, false) == GizmoHandle::AnchorMax, "a click on a handle grabs it");
    Expect(HitGizmoHandle(lobby, {0.0F, -9.0F}, false) == GizmoHandle::AnchorMax,
        "a tie between the anchor handles goes to anchor max");
    Expect(HitGizmoHandle(lobby, {220.0F, 238.0F}, true) == GizmoHandle::TopLeft,
        "a corner nearer than the pivot handle is grabbed");
    Expect(HitGizmoHandle(lobby, {1060.0F, 286.0F}, true) == GizmoHandle::BottomRight, "the far corner is grabbed");
    Expect(HitGizmoHandle(lobby, {600.0F, 240.0F}, true) == GizmoHandle::Top, "near the top edge grabs it");
    Expect(HitGizmoHandle(lobby, {600.0F, 284.0F}, true) == GizmoHandle::Bottom, "near the bottom edge grabs it");
    Expect(HitGizmoHandle(lobby, {1058.0F, 262.0F}, true) == GizmoHandle::Right, "near the right edge grabs it");
    Expect(HitGizmoHandle(lobby, {222.0F, 270.0F}, true) == GizmoHandle::Left, "near the left edge grabs it");
    Expect(HitGizmoHandle(lobby, {600.0F, 262.0F}, true) == GizmoHandle::Move,
        "inside the element on top grabs the body");
    Expect(!HitGizmoHandle(lobby, {600.0F, 262.0F}, false).has_value(),
        "inside the element under a child grabs nothing, so the child can be selected");
    Expect(!HitGizmoHandle(lobby, {600.0F, 400.0F}, true).has_value(), "away from the element grabs nothing");
}

// The reported defect (ui-editor-review.md S-5): with equal anchors each
// anchor handle moved in only one direction per axis.
void TestGizmoAnchorDrag() {
    const GizmoFrame centered = MakeFrame(kCentered);
    const Rect centeredRect{520.0F, 324.0F, 240.0F, 72.0F};
    GizmoRect dragged = Drag(GizmoHandle::AnchorMax, centered, {0.0F, -50.0F});
    Expect(Near(dragged.anchorMax, 0.5F, 0.5F - 50.0F / 720.0F) && Near(dragged.anchorMin, 0.5F, 0.5F - 50.0F / 720.0F),
        "equal anchors move up together from anchor max");
    Expect(Near(DesignRect(dragged), centeredRect), "moving the anchors keeps the element's rectangle");
    dragged = Drag(GizmoHandle::AnchorMin, centered, {0.0F, 50.0F});
    Expect(Near(dragged.anchorMin, 0.5F, 0.5F + 50.0F / 720.0F) && Near(dragged.anchorMax, 0.5F, 0.5F + 50.0F / 720.0F),
        "equal anchors move down together from anchor min");
    Expect(Near(DesignRect(dragged), centeredRect), "moving the anchors down keeps the rectangle");
    dragged = Drag(GizmoHandle::AnchorMax, centered, {50.0F, 50.0F});
    Expect(Near(dragged.anchorMin, 0.5F, 0.5F) && Near(dragged.anchorMax, 0.5F + 50.0F / 1280.0F, 0.5F + 50.0F / 720.0F),
        "dragging anchor max away from anchor min splits them");
    Expect(Near(DesignRect(dragged), centeredRect), "splitting the anchors keeps the rectangle");

    const GizmoFrame lobby = MakeFrame(kLobby);
    dragged = Drag(GizmoHandle::AnchorMin, lobby, {50.0F, 50.0F});
    Expect(Near(dragged.anchorMin, 50.0F / 1280.0F, 50.0F / 720.0F) && Near(dragged.anchorMax, 50.0F / 1280.0F, 50.0F / 720.0F),
        "anchor min at the corner pushes anchor max along");
    Expect(Near(dragged.position, 170.0F, 188.0F) && Near(DesignRect(dragged), {220.0F, 238.0F, 840.0F, 48.0F}),
        "position compensates the anchor move");
    dragged = Drag(GizmoHandle::AnchorMax, lobby, {0.0F, -50.0F});
    Expect(Near(dragged.anchorMax, 0.0F, 0.0F) && Near(dragged.position, 220.0F, 238.0F),
        "anchors stay inside the parent");
    Expect(Near(Drag(GizmoHandle::AnchorMax, centered, {1000.0F, 0.0F}).anchorMax, 1.0F, 0.5F),
        "anchor max stops at the parent's right edge");
}

void TestGizmoMoveAndResize() {
    const GizmoFrame lobby = MakeFrame(kLobby);
    GizmoRect dragged = Drag(GizmoHandle::Move, lobby, {13.0F, -7.0F});
    Expect(Near(dragged.position, 233.0F, 231.0F) && Near(dragged.sizeDelta, 840.0F, 48.0F) && SameAnchors(dragged, kLobby),
        "the body moves position only");

    const GizmoFrame centered = MakeFrame(kCentered);
    dragged = Drag(GizmoHandle::Right, centered, {30.0F, 5.0F});
    Expect(Near(DesignRect(dragged), {520.0F, 324.0F, 270.0F, 72.0F}) && SameAnchors(dragged, kCentered),
        "the right edge resizes and the left edge stays");
    Expect(Near(DesignRect(Drag(GizmoHandle::Left, centered, {300.0F, 0.0F})), {760.0F, 324.0F, 0.0F, 72.0F}),
        "the left edge stops at the right edge");
    Expect(Near(DesignRect(Drag(GizmoHandle::TopLeft, centered, {-10.0F, -20.0F})), {510.0F, 304.0F, 250.0F, 92.0F}),
        "a corner moves both of its edges");
    Expect(Near(DesignRect(Drag(GizmoHandle::Bottom, centered, {40.0F, 8.0F})), {520.0F, 324.0F, 240.0F, 80.0F}),
        "an edge ignores the other axis");

    dragged = Drag(GizmoHandle::Right, MakeFrame(kStretch), {-40.0F, 0.0F});
    Expect(Near(DesignRect(dragged), {20.0F, 20.0F, 1200.0F, 680.0F}) && Near(dragged.sizeDelta, -80.0F, -40.0F),
        "stretched anchors stay and size_delta takes the change");
    Expect(Near(DesignRect(Drag(GizmoHandle::Move, MakeFrame(kLobby, 2.0F), {20.0F, 0.0F})), {230.0F, 238.0F, 840.0F, 48.0F}),
        "screen pixels become design pixels at the design scale");
}

void TestGizmoSnapAndPivot() {
    const GizmoSnap grid{true, 8.0F};
    // 220 + 13 = 233 snaps to 232; the top edge (238, off the grid) did not move.
    Expect(Near(Drag(GizmoHandle::Move, MakeFrame(kLobby), {13.0F, 0.0F}, grid).position, 232.0F, 238.0F),
        "snap rounds only the moved axis of the body");
    // At scale 2, 13 screen px is 6.5 design px: 226.5 snaps to 224.
    Expect(Near(Drag(GizmoHandle::Move, MakeFrame(kLobby, 2.0F), {13.0F, 0.0F}, grid).position, 224.0F, 238.0F),
        "snap uses the grid in design pixels");
    const GizmoFrame centered = MakeFrame(kCentered);
    Expect(Near(DesignRect(Drag(GizmoHandle::Right, centered, {13.0F, 0.0F}, grid)), {520.0F, 324.0F, 256.0F, 72.0F}),
        "snap rounds the moved edge: 773 to 776");
    GizmoRect dragged = Drag(GizmoHandle::AnchorMax, centered, {13.0F, 0.0F}, grid);
    Expect(Near(dragged.anchorMax, 656.0F / 1280.0F, 0.5F) && Near(dragged.anchorMin, 0.5F, 0.5F),
        "snap rounds the anchor to the grid: 653 to 656");

    dragged = Drag(GizmoHandle::Pivot, centered, {-60.0F, -18.0F});
    Expect(Near(dragged.pivot, 0.25F, 0.25F), "pivot moves relative to the element's own bounds");
    Expect(Near(dragged.position, -60.0F, -18.0F) && Near(DesignRect(dragged), {520.0F, 324.0F, 240.0F, 72.0F}),
        "moving the pivot keeps the rectangle");
    Expect(Near(Drag(GizmoHandle::Pivot, centered, {500.0F, 0.0F}).pivot, 1.0F, 0.5F), "pivot stays inside the element");
}

// ui_editor Design Invariant 4 (docs/ui_toolchain.md): GizmoMath keeps its
// own copy of the gyo.ui layout rule to solve position and size_delta. Every
// result is laid out again by the GYO::Ui evaluator, which must put the
// element where the drag said: unchanged for anchor and pivot drags, moved or
// resized by the drag otherwise. The expectations use only evaluator output.
nlohmann::json GuardDocument(const GizmoRect& child) {
    const auto vec2 = [](const Vec2 value) { return nlohmann::json::array({value.x, value.y}); };
    const auto rect = [&vec2](const GizmoRect& value) {
        return nlohmann::json{{"anchor_min", vec2(value.anchorMin)}, {"anchor_max", vec2(value.anchorMax)},
            {"pivot", vec2(value.pivot)}, {"position", vec2(value.position)}, {"size_delta", vec2(value.sizeDelta)}};
    };
    // The parent is not the root, so the parent's own layout is part of the check.
    const GizmoRect parent{{0.1F, 0.2F}, {0.9F, 0.95F}, {0.3F, 0.6F}, {17.0F, -11.0F}, {-30.0F, -24.0F}};
    return nlohmann::json{{"schema", "gyo.ui"}, {"version", 1},
        {"design_canvas", {{"size", {1280.0, 720.0}}, {"scale_mode", "fit"}}},
        {"fonts", {{"default", "font.default"}}}, {"default_font", "default"},
        {"colors", {{"transparent", "#00000000"}}}, {"actions", nlohmann::json::array()},
        {"bindings", nlohmann::json::array()},
        {"canvases", nlohmann::json::array({{{"id", "main"}, {"backdrop_color", "transparent"},
            {"focus_order", nlohmann::json::array()},
            {"children", nlohmann::json::array({{{"type", "container"}, {"id", "parent"}, {"rect", rect(parent)},
                {"children", nlohmann::json::array({{{"type", "container"}, {"id", "child"},
                    {"rect", rect(child)}}})}}})}}})}};
}

std::optional<std::pair<Rect, Rect>> EvaluateGuard(const GizmoRect& child, const float scale) {
    auto parsed = Engine::Ui::UiDocumentCodec::Parse(GuardDocument(child).dump(), "gizmo-guard");
    if (!parsed) {
        return std::nullopt;
    }
    Engine::Ui::UiRuntime runtime;
    if (!runtime.Initialize(std::make_shared<const Engine::Ui::UiDocument>(std::move(parsed).value())) ||
        !runtime.ActivateCanvas("main")) {
        return std::nullopt;
    }
    auto evaluated = runtime.EvaluateLayout({}, {1280.0F * scale, 720.0F * scale});
    if (!evaluated) {
        return std::nullopt;
    }
    std::optional<Rect> parent;
    std::optional<Rect> element;
    for (const Engine::Ui::UiEvaluatedElement& entry : evaluated.value()) {
        if (entry.id == "parent") {
            parent = entry.boundsPixels;
        } else if (entry.id == "child") {
            element = entry.boundsPixels;
        }
    }
    if (!parent || !element) {
        return std::nullopt;
    }
    return std::pair{*parent, *element};
}

bool NearPixels(const Rect value, const Rect expected) {
    return std::abs(value.x - expected.x) < 1e-2F && std::abs(value.y - expected.y) < 1e-2F &&
        std::abs(value.width - expected.width) < 1e-2F && std::abs(value.height - expected.height) < 1e-2F;
}

void TestGizmoAgreesWithUiEvaluator() {
    const GizmoRect cases[]{
        {{0.5F, 0.5F}, {0.5F, 0.5F}, {0.5F, 0.5F}, {12.0F, -7.0F}, {240.0F, 72.0F}},
        {{0.0F, 0.0F}, {0.0F, 0.0F}, {0.0F, 0.0F}, {220.0F, 238.0F}, {300.0F, 48.0F}},
        {{0.2F, 0.1F}, {0.7F, 0.8F}, {0.25F, 0.75F}, {-15.0F, 9.0F}, {-40.0F, 22.0F}},
        {{1.0F, 1.0F}, {1.0F, 1.0F}, {1.0F, 0.0F}, {-30.0F, 40.0F}, {120.0F, 60.0F}},
    };
    struct Step final {
        GizmoHandle handle;
        Vec2 delta;     // screen pixels
        Rect change;    // expected change of the element's screen rectangle
    };
    for (const float scale : {1.0F, 2.0F}) {
        const Step steps[]{
            {GizmoHandle::AnchorMin, {37.0F, -23.0F}, {}},
            {GizmoHandle::AnchorMax, {-41.0F, 29.0F}, {}},
            {GizmoHandle::Pivot, {13.0F, -7.0F}, {}},
            {GizmoHandle::Move, {12.0F, -8.0F}, {12.0F, -8.0F, 0.0F, 0.0F}},
            {GizmoHandle::Right, {30.0F, 5.0F}, {0.0F, 0.0F, 30.0F, 0.0F}},
            {GizmoHandle::TopLeft, {-10.0F, -20.0F}, {-10.0F, -20.0F, 10.0F, 20.0F}},
        };
        for (const GizmoRect& start : cases) {
            const auto before = EvaluateGuard(start, scale);
            Expect(before.has_value(), "the guard document lays out");
            if (!before) {
                continue;
            }
            const GizmoFrame frame{before->first, before->second, start, scale};
            for (const Step& step : steps) {
                const GizmoRect dragged = DragGizmo(step.handle, frame, {0.0F, 0.0F}, step.delta, {});
                const auto after = EvaluateGuard(dragged, scale);
                const Rect expected{before->second.x + step.change.x, before->second.y + step.change.y,
                    before->second.width + step.change.width, before->second.height + step.change.height};
                Expect(after.has_value() && NearPixels(after->second, expected),
                    "the GYO::Ui evaluator places a dragged element where the gizmo intended");
            }
        }
    }
}

int main() {
    TemporaryDirectory temporary;
    TestCommandLine();
    TestPathDialogInbox();
    TestGizmoHit();
    TestGizmoAnchorDrag();
    TestGizmoMoveAndResize();
    TestGizmoSnapAndPivot();
    TestGizmoAgreesWithUiEvaluator();
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
