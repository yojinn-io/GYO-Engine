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
  only the executable links the window's library; a window-only target is named only in the window library's links
  and in the gui variant's existence check (not in link_libraries, a property, a variable or a generator expression);
- links are made only by target_link_libraries with literal names (no variable, generator expression,
  link_libraries or LINK_LIBRARIES property), so the existence check's loop variable cannot link;
- a # inside a quoted CMake string is not a comment;
- no include steps up with "..";
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

# The baked file and its validator are Engine's (GYO::VoxelMesh); markers and their kinds are read and written only
# through Engine's GYO::MapMarkers (v8-arch-big-map-tool-chunked, user 2026-11-06); files go through engine/io; picking
# uses Engine's math (GYO::Math); the command line goes through the framework's headless shell; gyo.voxel_grid is read
# with the tool's own JSON parser.
CORE_INCLUDE_PREFIXES = ("gyo/voxel_map_editor/", "gyo/framework/", "engine/", "nlohmann/")
CORE_TARGETS = {"GYO::Base", "GYO::Engine", "GYO::Math", "GYO::VoxelMesh", "GYO::MapMarkers", "GYO::ToolFramework",
                "nlohmann_json::nlohmann_json"}
CORE_WINDOW_ENTRY = "window/EditorWindow.hpp"
# What only the window may include: Engine's render and platform, and the framework's window-side headers.
WINDOW_ONLY_INCLUDES = ("engine/render/", "engine/platform/", "gyo/framework/Preview", "gyo/framework/WindowShell")
# The window: the framework's ImGui shell and offscreen preview (which bring Engine's render targets), shown as a
# texture of Engine's SDL renderer (framework Design Invariant 5); imgui.h only.
WINDOW_DIRECTORY = "src/window/"
# Files of src/window/ that the core compiles or includes keep the core's rules: the entry header (the executable
# includes it in every variant) and the stand-in the cli variant compiles into the executable.
WINDOW_CORE_FILES = {"src/window/EditorWindow.hpp", "src/window/EditorWindowUnavailable.cpp"}
# Only the window library compiles the other files of src/window/, and only in the gui variant.
GUI_BLOCK = re.compile(r'\bif\s*\(\s*GYO_TOOL_VARIANT\s+STREQUAL\s+"gui"\s*\)', re.IGNORECASE)
BLOCK_WORD = re.compile(r"\b(if|elseif|else|endif)\s*\(", re.IGNORECASE)
WINDOW_SOURCE = re.compile(r"src/window/[^\s)\"]*")
# An SDL header written without a directory ("SDL.h", "SDL_events.h").
BARE_SDL_HEADER = re.compile(r"^SDL\w*\.h$", re.IGNORECASE)
WINDOW_INCLUDE_PREFIXES = CORE_INCLUDE_PREFIXES + ("window/",)
WINDOW_TARGETS = CORE_TARGETS | {"GYO::ToolFrameworkPreview", "GYO::ToolFrameworkImGui", "GYO::Render",
                                 "GYO::RenderBackendSDL", "GYO::PlatformSDL", "imgui::core"}
WINDOW_LIBRARY = "gyo_voxel_map_editor_window"
WINDOW_IMGUI_HEADER = "imgui.h"
# The framework's ImGui binding (its shell): the window's, never the core's; even there, no native, internal or
# backend header.
FRAMEWORK_IMGUI = "gyo/framework/imgui/"
IMGUI_INTERNAL = "imgui_internal.h"
# Only the executable links the window's library.
WINDOW_LINKERS = {"gyo_voxel_map_editor"}
# Where a window-only target may be named in the tool's CMake files: the window library's own links, and the gui
# variant's existence check (a foreach over IN ITEMS). Anywhere else (another target's links, link_libraries,
# a property, a variable, a generator expression) it fails.
WINDOW_LINK_CALL = re.compile(r"target_link_libraries\s*\(\s*" + WINDOW_LIBRARY + r"\b[^)]*\)", re.IGNORECASE)
OTHER_LINK = re.compile(r"(?<![A-Za-z_])link_libraries\s*\(|LINK_LIBRARIES\w*|link_options\s*\(|TARGET_OBJECTS", re.IGNORECASE)
# Where the window library's name may stand: as the first argument of its own commands, in the executable's links,
# and in the list of the tool's targets for the warning flags.
COMMAND = re.compile(r"\b([A-Za-z_]\w*)\s*\(([^()]*)\)")
WINDOW_OWN_COMMANDS = {"add_library", "target_link_libraries", "target_include_directories", "target_sources",
                       "target_compile_options", "target_compile_definitions", "target_compile_features",
                       "add_dependencies"}
TOOL_TARGET_LIST = "_gyo_voxel_map_editor_targets"
EXISTENCE_CHECK = re.compile(r"foreach\s*\(\s*_target\s+IN\s+ITEMS\b[^)]*\)", re.IGNORECASE)

LINK_CALL = re.compile(r"target_link_libraries\s*\(([^)]*)\)", re.IGNORECASE)
LINK_KEYWORDS = {"PUBLIC", "PRIVATE", "INTERFACE", "LINK_PUBLIC", "LINK_PRIVATE", "LINK_INTERFACE_LIBRARIES"}
OWN_TARGET = re.compile(r"^gyo_voxel_map_editor\w*$")
REGISTRIES = ("engine/config/tools.csv", "engine/config/projects.csv")
OWN_ID = "voxel_map_editor"

ENGINE_NATIVE_HEADER = re.compile(r"/native/|Native\.(h|hh|hpp|hxx|inl)$", re.IGNORECASE)
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


def strip_comments(text: str) -> str:
    """CMake text without its whole-line comments. A # after code is kept: what follows it is still checked, so a
    # inside a string or a bracket argument cannot hide a link (stricter, never looser)."""
    return "\n".join(line for line in text.split("\n") if not line.lstrip().startswith("#"))


def linked_names(text: str) -> list[tuple[str, str]]:
    """(linking target, linked name) for every library a target_link_libraries call names."""
    names = []
    # On the stripped text and on the raw text: a whole-line "comment" may sit inside a multi-line string.
    for call in LINK_CALL.findall(strip_comments(text)) + LINK_CALL.findall(text):
        tokens = call.split()
        if not tokens:
            continue
        for token in tokens[1:]:  # the first is the target being linked
            if token in LINK_KEYWORDS:
                continue
            names.append((tokens[0], token))
    return names


def gui_spans(code: str) -> list[tuple[int, int]]:
    """[start, end) of every if(GYO_TOOL_VARIANT STREQUAL "gui") branch, up to its else, elseif or endif."""
    spans = []
    for opening in GUI_BLOCK.finditer(code):
        depth = 0
        end = len(code)
        for word in BLOCK_WORD.finditer(code, opening.end()):
            name = word.group(1).lower()
            if name == "if":
                depth += 1
            elif name == "endif" and depth > 0:
                depth -= 1
            elif depth == 0:
                end = word.start()
                break
        spans.append((opening.end(), end))
    return spans


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
            local = path.relative_to(root / OWNER).as_posix()
            window = local.startswith(WINDOW_DIRECTORY) and local not in WINDOW_CORE_FILES
            prefixes = WINDOW_INCLUDE_PREFIXES if window else CORE_INCLUDE_PREFIXES
            for include in INCLUDE.findall(text):
                # Only plain forward-slash paths: a parent step leaves every prefix; a backslash or a "." step hides one.
                if "\\" in include or any(step in ("", ".", "..") for step in include.split("/")):
                    findings.append(f"{relative}: includes {include}")
                    continue
                if BARE_SDL_HEADER.match(include):
                    findings.append(f"{relative}: includes {include}")
                    continue
                if include.lower().startswith(FRAMEWORK_IMGUI):
                    base = include.rsplit("/", 1)[-1].lower()
                    if (not window or is_native_include(include) or base == IMGUI_INTERNAL
                            or base.startswith("imgui_impl") or "backends" in include.lower()):
                        findings.append(f"{relative}: includes {include}")
                    continue
                if IMGUI_HEADER.match(include.rsplit("/", 1)[-1]) or include.startswith(("imgui/", "backends/")):
                    if not (window and include == WINDOW_IMGUI_HEADER):
                        findings.append(f"{relative}: includes {include}")
                    continue
                if "/" not in include:
                    continue  # standard headers and the tool's own local headers
                if not window and include.lower().startswith(tuple(p.lower() for p in WINDOW_ONLY_INCLUDES)):
                    findings.append(f"{relative}: includes {include}")
                    continue
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
            # A window-only target outside its two places.
            code = strip_comments(text)
            # Links only by target_link_libraries with literal names, so every link is checked above and below.
            # On the raw text too: a whole-line "comment" may sit inside a multi-line string (stricter only).
            if any(OTHER_LINK.search(re.sub(r"\btarget_link_libraries\s*\(", "(", body, flags=re.IGNORECASE))
                   for body in (code, text)):
                findings.append(f"{relative}: links outside target_link_libraries")
            outside = EXISTENCE_CHECK.sub("", WINDOW_LINK_CALL.sub("", code))
            raw_outside = EXISTENCE_CHECK.sub("", WINDOW_LINK_CALL.sub("", text))
            commented = {target for line in text.split("\n") if line.lstrip().startswith("#")
                         for target in TARGET.findall(line)}
            # A window-only target in a "comment" line that is really inside a string counts as named outside.
            hidden = set(TARGET.findall(raw_outside)) - set(TARGET.findall(outside)) - commented
            for target in sorted(set(TARGET.findall(outside)) | hidden):
                if target in WINDOW_TARGETS and target not in CORE_TARGETS:
                    findings.append(f"{relative}: names {target} outside the window's links")
            # The window's sources and window-only targets only in the gui variant; its sources only in the
            # window library (the stand-in excepted).
            spans = gui_spans(code)
            inside = lambda at: any(start <= at < end for start, end in spans)  # noqa: E731
            for target in WINDOW_TARGETS - CORE_TARGETS:
                for match in re.finditer(re.escape(target) + r"(?![A-Za-z0-9_.+:-])", code):
                    if not inside(match.start()):
                        findings.append(f"{relative}: names {target} outside the gui variant")
            for match in re.finditer(r"\b" + WINDOW_LIBRARY + r"\b", code):
                if not inside(match.start()):
                    findings.append(f"{relative}: names {WINDOW_LIBRARY} outside the gui variant")
            for command, arguments in COMMAND.findall(code):
                tokens = arguments.split()
                for source in WINDOW_SOURCE.findall(arguments):
                    if source in WINDOW_CORE_FILES:
                        continue
                    if not (command.lower() in ("add_library", "target_sources") and tokens and tokens[0] == WINDOW_LIBRARY):
                        findings.append(f"{relative}: compiles {source} outside the window library")
            allowed_uses = 0
            for command, arguments in COMMAND.findall(code):
                tokens = arguments.split()
                uses = tokens.count(WINDOW_LIBRARY)
                if not uses:
                    continue
                name = command.lower()
                if name in WINDOW_OWN_COMMANDS and tokens[0] == WINDOW_LIBRARY and uses == 1:
                    allowed_uses += 1
                elif name == "target_link_libraries" and tokens[0] in WINDOW_LINKERS and uses == 1:
                    allowed_uses += 1
                elif name == "list" and tokens[:2] == ["APPEND", TOOL_TARGET_LIST] and uses == 1:
                    allowed_uses += 1
            if len(re.findall(r"\b" + WINDOW_LIBRARY + r"\b", code)) != allowed_uses:
                findings.append(f"{relative}: names {WINDOW_LIBRARY} outside its own commands and the executable's links")
            for linking, name in sorted(set(linked_names(text))):
                if name.startswith(("${", "$<")):
                    findings.append(f"{relative}: {linking} links {name}, not a literal name")
                    continue
                allowed = WINDOW_TARGETS if linking == WINDOW_LIBRARY else CORE_TARGETS
                if name == WINDOW_LIBRARY:
                    if linking not in WINDOW_LINKERS:
                        findings.append(f"{relative}: {linking} links {name}")
                    continue
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
