"""Guard for voxel_map_editor Design Invariant 1 (docs/voxel_map_editor/architecture.md): no game, and two parts.

Adapted from the model viewer's guard. The core (every file of the tool outside src/window/, and the libraries
gyo_voxel_map_editor_core and gyo_voxel_map_editor) has no SDL, ImGui, render or platform; the window (src/window/ and
the library gyo_voxel_map_editor_window, gui variant only) may use the framework's ImGui shell and offscreen preview,
Engine's render and platform targets and imgui.h, but still no SDL of its own.

The rule is a positive list, so no product is named here (AGENTS.md sections 7 and 9):
- every path include starts with an allowed prefix of its part (any Engine module may be used), and none is an SDL3/
  header or an Engine native-layer header; only the window includes the framework's ImGui binding
  (gyo/framework/imgui/); the core may include the window's entry header window/EditorWindow.hpp,
  which declares two functions and includes nothing of ImGui;
- the core includes no ImGui header at all; the window only imgui.h (never imgui_internal.h or a backends/ header);
- no source uses an SDL_ name;
- every namespaced target (Name::Name) in the tool's CMake files is an allowed target of either part; no SDL or *Native
  one;
- every library a target_link_libraries call names is an allowed target of the linking target's part, or one of the
  tool's own (gyo_voxel_map_editor*), so a game's plain target name (gyo_app_add_library gives such names) fails too;
- Design Invariant 6, neutral: no file names an id registered in engine/config/tools.csv or engine/config/projects.csv
  other than the tool's own.
A new dependency fails until it is added here, which is the point: it changes the editor's Design Invariants.

Usage: python3 voxel_map_editor_dependency_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import csv
import re
import sys
from pathlib import Path

OWNER = "tools/voxel_map_editor"
SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".mm"}
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)
SDL_SYMBOL = re.compile(r"\bSDL_\w+")
# The whole CMake name: a name that goes on after an allowed one with -, ., + or :: is another target.
TARGET = re.compile(r"\b[A-Za-z_][A-Za-z0-9_]*::[A-Za-z_][A-Za-z0-9_]*(?:(?:[.+-]|::)[A-Za-z0-9_]+)*")

# The baked file and its validator are Engine's (GYO::VoxelMesh); files go through engine/io; picking uses Engine's math
# (GYO::Math); the command line goes through the framework's headless shell; gyo.voxel_grid is read with the tool's own
# JSON parser.
CORE_INCLUDE_PREFIXES = ("gyo/voxel_map_editor/", "gyo/framework/", "engine/", "nlohmann/")
CORE_TARGETS = {"GYO::Base", "GYO::Engine", "GYO::Math", "GYO::VoxelMesh", "GYO::ToolFramework",
                "nlohmann_json::nlohmann_json"}
CORE_WINDOW_ENTRY = "window/EditorWindow.hpp"
# The window: the framework's ImGui shell and offscreen preview (which bring Engine's render targets), shown as a
# texture of Engine's SDL renderer (framework Design Invariant 5); imgui.h only.
WINDOW_DIRECTORY = "src/window/"
WINDOW_INCLUDE_PREFIXES = CORE_INCLUDE_PREFIXES + ("window/",)
WINDOW_TARGETS = CORE_TARGETS | {"GYO::ToolFrameworkPreview", "GYO::ToolFrameworkImGui", "GYO::Render",
                                 "GYO::RenderBackendSDL", "GYO::PlatformSDL", "imgui::core"}
WINDOW_LIBRARY = "gyo_voxel_map_editor_window"
WINDOW_IMGUI_HEADER = "imgui.h"
# The framework's ImGui binding (its shell): the window's, never the core's.
FRAMEWORK_IMGUI = "gyo/framework/imgui/"

LINK_CALL = re.compile(r"target_link_libraries\s*\(([^)]*)\)", re.IGNORECASE)
LINK_KEYWORDS = {"PUBLIC", "PRIVATE", "INTERFACE", "LINK_PUBLIC", "LINK_PRIVATE", "LINK_INTERFACE_LIBRARIES"}
OWN_TARGET = re.compile(r"^gyo_voxel_map_editor\w*$")
REGISTRIES = ("engine/config/tools.csv", "engine/config/projects.csv")
OWN_ID = "voxel_map_editor"

ENGINE_NATIVE_HEADER = re.compile(r"/native/|Native\.(h|hh|hpp|hxx|inl)$")
IMGUI_HEADER = re.compile(r"^imgui", re.IGNORECASE)


def is_native_include(include: str) -> bool:
    return bool(ENGINE_NATIVE_HEADER.search(include))


def registered_ids(root: Path) -> list[str]:
    ids = []
    for name in REGISTRIES:
        path = root / name
        if not path.is_file():
            continue
        with path.open(encoding="utf-8", newline="") as file:
            rows = list(csv.reader(file))
        ids.extend(row[0].strip() for row in rows[1:] if row and row[0].strip() and row[0].strip() != OWN_ID)
    return ids


def linked_names(text: str) -> list[tuple[str, str]]:
    """(linking target, linked name) for every library a target_link_libraries call names."""
    names = []
    for call in LINK_CALL.findall(re.sub(r"#[^\n]*", "", text)):
        tokens = call.split()
        if not tokens:
            continue
        for token in tokens[1:]:  # the first is the target being linked
            if token in LINK_KEYWORDS or token.startswith(("${", "$<")):
                continue
            names.append((tokens[0], token))
    return names


def scan(root: Path) -> list[str]:
    findings = []
    ids = [(name, re.compile(r"\b" + re.escape(name) + r"\b")) for name in registered_ids(root)]
    for path in sorted((root / OWNER).rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(root).as_posix()
        text = path.read_text(encoding="utf-8", errors="replace")
        for name, pattern in ids:
            if pattern.search(text):
                findings.append(f"{relative}: names the registered id {name}")
        if path.suffix in SUFFIXES:
            window = path.relative_to(root / OWNER).as_posix().startswith(WINDOW_DIRECTORY)
            prefixes = WINDOW_INCLUDE_PREFIXES if window else CORE_INCLUDE_PREFIXES
            for include in INCLUDE.findall(text):
                if include.startswith(FRAMEWORK_IMGUI):
                    if not window:
                        findings.append(f"{relative}: includes {include}")
                    continue
                if IMGUI_HEADER.match(include.rsplit("/", 1)[-1]) or include.startswith(("imgui/", "backends/")):
                    if not (window and include == WINDOW_IMGUI_HEADER):
                        findings.append(f"{relative}: includes {include}")
                    continue
                if "/" not in include:
                    continue  # standard headers and the tool's own local headers
                if not window and include == CORE_WINDOW_ENTRY:
                    continue
                if not include.startswith(prefixes) or is_native_include(include):
                    findings.append(f"{relative}: includes {include}")
            for name in sorted(set(SDL_SYMBOL.findall(text))):
                findings.append(f"{relative}: uses {name}")
        elif path.name == "CMakeLists.txt" or path.suffix == ".cmake":
            for target in sorted(set(TARGET.findall(text))):
                if target not in WINDOW_TARGETS:
                    findings.append(f"{relative}: names {target}")
            for linking, name in sorted(set(linked_names(text))):
                allowed = WINDOW_TARGETS if linking == WINDOW_LIBRARY else CORE_TARGETS
                if OWN_TARGET.match(name):
                    continue
                if name not in allowed:
                    findings.append(f"{relative}: {linking} links {name}")
    return findings


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    findings = scan(parser.parse_args().root)
    for finding in findings:
        print(f"voxel_map_editor dependency outside its Design Invariants: {finding}")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
