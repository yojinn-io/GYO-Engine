"""Guard for ui_editor Design Invariant 1 (docs/ui_toolchain.md): no game, and SDL only as a game reaches it.

The rule is a positive list, so no product is named here (AGENTS.md sections 7 and 9):
- every path include in tools/ui_editor starts with an allowed prefix (any Engine module may be used);
- an SDL3/ header, an Engine native-layer header and any SDL_ symbol appear only in the files that use the
  asset preview's native renderer (NATIVE_FILES); there, the only native header is the SDL renderer's and the
  only SDL_ names are the texture and renderer ones (NATIVE_SDL_NAMES); no file uses an SDL_GPU name;
- no file includes imgui_internal.h: the editor uses ImGui's public API only (v8-input-ui-adapter);
- every namespaced target (Name::Name) in the tool's CMake files is an allowed target; SDL3:: is not one.
A new dependency fails until it is added here, which is the point: it changes the editor's Design Invariants.

Usage: python3 ui_editor_dependency_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import re
import sys
from pathlib import Path

OWNER = "tools/ui_editor"
SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".mm"}
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)
SDL_GPU = re.compile(r"\bSDL_\w*GPU\w*")
SDL_SYMBOL = re.compile(r"\bSDL_\w+")
TARGET = re.compile(r"\b[A-Za-z_][A-Za-z0-9_]*::[A-Za-z_][A-Za-z0-9_]*\b")

# The window and ImGui's SDL backends come through the framework's ImGui binding, whose own guard
# (tests/framework/framework_dependency_guard.py) keeps the same rule (docs/framework/architecture.md, Design
# Invariant 2).
ALLOWED_INCLUDE_PREFIXES = ("gyo/ui_editor/", "gyo/framework/", "engine/", "nlohmann/", "SDL3/")
ALLOWED_TARGETS = {
    "GYO::Engine", "GYO::Ui", "GYO::Math", "GYO::Os", "GYO::Input",
    "GYO::ToolFramework", "GYO::ToolFrameworkImGui",
    "GYO::PlatformSDL", "GYO::RenderBackendSDLNative",
    "GYO::AssetSdlImage", "GYO::TextBackendSDLTTF",
    "imgui::core", "nlohmann_json::nlohmann_json",
}
# ImGui's internals: text input goes through the framework (ImGuiShellInput), not ImGui's private state.
FORBIDDEN_HEADERS = {"imgui_internal.h"}
# The asset preview makes its textures on the native SDL renderer (docs/architecture/platform-sdl.md,
# Replacement Criterion 2); only these files touch SDL or Engine's native layer.
NATIVE_FILES = {
    "tools/ui_editor/include/gyo/ui_editor/AssetPreviewContext.hpp",
    "tools/ui_editor/src/AssetPreviewContext.cpp",
    "tools/ui_editor/src/PreviewAdapter.cpp",
    "tools/ui_editor/src/EditorWindow.cpp",
}
# In those files: the renderer's native header, and SDL's texture and renderer API only.
NATIVE_HEADERS = {"engine/render/backend/sdl/SdlRendererNative.hpp"}
NATIVE_SDL_NAMES = re.compile(
    r"SDL_(Renderer|Texture|CreateTexture|UpdateTexture|DestroyTexture|SetTexture\w+|GetError"
    r"|PIXELFORMAT_\w+|TEXTUREACCESS_\w+|BLENDMODE_\w+|SCALEMODE_\w+)")
ENGINE_NATIVE_HEADER = re.compile(r"/native/|Native\.(h|hh|hpp|hxx|inl)$")


def is_native_include(include: str) -> bool:
    return include.startswith("SDL3/") or bool(ENGINE_NATIVE_HEADER.search(include))


def scan(root: Path) -> list[str]:
    findings = []
    for path in sorted((root / OWNER).rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(root).as_posix()
        text = path.read_text(encoding="utf-8", errors="replace")
        if path.suffix in SUFFIXES:
            for include in INCLUDE.findall(text):
                if include.rsplit("/", 1)[-1] in FORBIDDEN_HEADERS:
                    findings.append(f"{relative}: includes {include}")
                    continue
                if "/" not in include:
                    continue  # standard headers and the tool's own local headers
                native = is_native_include(include)
                if not include.startswith(ALLOWED_INCLUDE_PREFIXES) or (native and relative not in NATIVE_FILES) or (
                        native and not include.startswith("SDL3/") and include not in NATIVE_HEADERS):
                    findings.append(f"{relative}: includes {include}")
            for name in sorted(set(SDL_GPU.findall(text))):
                findings.append(f"{relative}: uses {name}")
            for name in sorted(set(SDL_SYMBOL.findall(text)) - set(SDL_GPU.findall(text))):
                if relative not in NATIVE_FILES or not NATIVE_SDL_NAMES.fullmatch(name):
                    findings.append(f"{relative}: uses {name}")
        elif path.name == "CMakeLists.txt" or path.suffix == ".cmake":
            for target in sorted(set(TARGET.findall(text))):
                if target not in ALLOWED_TARGETS:
                    findings.append(f"{relative}: names {target}")
    return findings


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    findings = scan(parser.parse_args().root)
    for finding in findings:
        print(f"ui_editor dependency outside its Design Invariants: {finding}")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
