"""Guard for the framework's Design Invariants 1-3 (docs/framework/architecture.md).

The rules read the registries instead of naming any product or tool (AGENTS.md sections 7 and 9):
- Invariant 1, not a product: framework/ has no project.json, its CMake files add no executable and no install
  target, and neither registry has a row named framework;
- Invariant 2, one direction, core and bindings: everything under framework/bindings/<name>/ is a binding, the rest
  is the core.
  - The framework may use any Engine module, input included (v8-tool-ui-editor-ime-composition-source,
    v8-framework-engine-access): adding an Engine target that is not *Native to a positive list follows
    Invariant 2 (the guard change itself still follows AGENTS.md section 14.4), adding an SDL, *Native or
    third-party target changes the Invariant.
  - The core includes only gyo/framework/ and Engine's neutral headers (no *Native.hpp) besides standard and local
    headers, and nothing whose name starts with SDL, imgui or json.hpp; it names no SDL_ (SDL_GPU included), ImGui
    or nlohmann symbol in code; its CMake files name only the core's targets (CORE["targets"]), and each core
    library links only what its own list allows (CORE["links"], one list per target the core's CMake defines with
    add_library; a core library without a list fails). So the windowed shell links no render device and the
    offscreen preview alone links Engine's render targets (v8-framework-preview-component).
  - Each binding has its own positive list (BINDINGS); a directory under framework/bindings/ without one fails.
    Only the ImGui binding may include a *Native.hpp header or name a GYO::*Native target, and no binding names
    SDL3::SDL3 or an SDL_ symbol.
- Invariant 3, neutral: no file names an id registered in engine/config/tools.csv or engine/config/projects.csv.

There are no exceptions. The ratchet allowlist of the core's old sites (v8-framework-guard-ratchet) emptied in
v8-framework-invariants and was removed with its rules; the file coming back fails, so an exception needs a change
to this guard and the user's approval (AGENTS.md section 14.4). The configure-time link check in
tests/framework/CMakeLists.txt has none either.
Includes, targets and registered names are read from the raw text (a comment counts); symbols from the code only,
with include lines removed and comments and literals blanked.

A new dependency fails until it is added here, which is the point: each edge is recorded, and one that is an SDL,
*Native or third-party target changes the framework's Design Invariants.

Usage: python3 framework_dependency_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import csv
import re
import sys
from pathlib import Path

OWNER = "framework"
BINDINGS_DIRECTORY = "framework/bindings/"
# The ratchet allowlist removed in v8-framework-invariants; it must not come back.
REMOVED_ALLOWLIST = "tests/framework/framework_guard_allowlist.txt"
REGISTRIES = ("engine/config/tools.csv", "engine/config/projects.csv")
SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".mm"}
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)
TARGET = re.compile(r"\b[A-Za-z_][A-Za-z0-9_]*::[A-Za-z_][A-Za-z0-9_]*\b")
TOOL_COMMAND = re.compile(r"\b(add_executable|gyo_tool_install_target|gyo_install_product_target)\s*\(")
NATIVE_HEADER = re.compile(r"^engine/.*(/native/|Native\.hpp$)")
# Bare names (no slash) that are SDL's, ImGui's or nlohmann's own headers, not a standard or local one.
LIBRARY_BARE_INCLUDE = re.compile(r"^(SDL\.h|SDL_\w*\.h|imgui(_\w+)?\.h|json\.hpp)$")

CORE = {
    "include_prefixes": ("gyo/framework/", "engine/"),
    "bare_includes": (),
    "native_headers": (),
    "symbols": re.compile(r"\bSDL_\w+|\bImGui\w*|\bnlohmann\b"),
    # GYO::Model and GYO::Audio: gyo.event_sounds binds to Engine's event set and checks gains with Engine
    # (docs/framework/event-sounds.md, v8-arch-event-sounds-impl). GYO::Render, GYO::RenderBackendSDLGPU and
    # GYO::RenderBackendSDL: the offscreen preview (docs/framework/preview.md, v8-framework-preview-component).
    "targets": {"GYO::ToolFramework", "GYO::ToolFrameworkWindow", "GYO::ToolFrameworkPreview", "GYO::Engine", "GYO::Os",
                "GYO::PlatformSDL", "GYO::Input", "GYO::InputBackendSDL", "GYO::Ui", "GYO::Model", "GYO::Audio",
                "GYO::Render", "GYO::RenderBackendSDLGPU", "GYO::RenderBackendSDL"},
    # What each core library may link (target_link_libraries), by the name its add_library gives it. The windowed
    # shell reaches SDL through Engine's neutral platform only and links no render device (Design Invariant 6); the
    # preview is the one core library on Engine's render backends.
    "links": {
        "gyo_tool_framework": {"GYO::Engine", "GYO::Input", "GYO::Ui", "GYO::Model", "GYO::Os", "GYO::Audio"},
        "gyo_tool_framework_window": {"gyo_tool_framework", "GYO::PlatformSDL"},
        "gyo_tool_framework_preview": {"gyo_tool_framework", "GYO::PlatformSDL", "GYO::Render",
                                       "GYO::RenderBackendSDLGPU", "GYO::RenderBackendSDL", "GYO::Os"},
    },
}
LINK_CALL = re.compile(r"target_link_libraries\s*\(([^)]*)\)", re.IGNORECASE)
LINK_KEYWORDS = {"PUBLIC", "PRIVATE", "INTERFACE", "LINK_PUBLIC", "LINK_PRIVATE", "LINK_INTERFACE_LIBRARIES"}
# A library the core's CMake defines (an ALIAS defines a name for one, not another library).
# A library defined through a variable has no name to look its list up by.
ADD_LIBRARY_VARIABLE = re.compile(r"^\s*add_library\s*\(\s*\$\{", re.IGNORECASE | re.MULTILINE)
ADD_LIBRARY = re.compile(r"^\s*add_library\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s+(?!ALIAS\b)", re.IGNORECASE | re.MULTILINE)
BINDINGS = {
    "imgui": {
        "include_prefixes": ("gyo/framework/", "engine/"),
        "exact_includes": ("backends/imgui_impl_sdl3.h", "backends/imgui_impl_sdlrenderer3.h"),
        "bare_includes": ("imgui.h",),
        "native_headers": ("engine/platform/sdl/SdlPlatformNative.hpp",
                           "engine/render/backend/sdl/SdlRendererNative.hpp"),
        "symbols": re.compile(r"\bSDL_\w+|\bnlohmann\b"),
        "targets": {"GYO::ToolFramework", "GYO::ToolFrameworkWindow", "GYO::ToolFrameworkImGui", "GYO::Engine",
                    "GYO::Os", "GYO::PlatformSDL", "GYO::PlatformSDLNative", "GYO::RenderBackendSDL",
                    "GYO::RenderBackendSDLNative", "GYO::InputBackendSDL", "imgui::core", "imgui::sdl3_sdlrenderer3"},
    },
    "json": {
        "include_prefixes": ("gyo/framework/", "engine/", "nlohmann/"),
        "exact_includes": (),
        "bare_includes": (),
        "native_headers": (),
        "symbols": re.compile(r"\bSDL_\w+|\bImGui\w*"),
        "targets": {"GYO::ToolFramework", "GYO::ToolFrameworkJson", "GYO::Engine", "GYO::Os",
                    "nlohmann_json::nlohmann_json"},
    },
}


def strip_comments_and_strings(text):
    """Replaces comments, string and character literals with spaces, keeping line breaks.

    The same rules as tests/common/os/os_boundary_guard.py (Engine's guard), copied so that the framework's test
    does not load another owner's script.
    """
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            end = text.find("\n", i)
            end = n if end < 0 else end
            out.append(" " * (end - i)); i = end
        elif text.startswith("/*", i):
            end = text.find("*/", i + 2)
            end = n if end < 0 else end + 2
            out.append(re.sub(r"[^\n]", " ", text[i:end])); i = end
        elif c == "R" and text.startswith('R"', i) and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == "_")):
            delimiter_end = text.find("(", i + 2)
            closing = ")" + text[i + 2:delimiter_end] + '"'
            end = text.find(closing, delimiter_end)
            end = n if end < 0 else end + len(closing)
            out.append(re.sub(r"[^\n]", " ", text[i:end])); i = end
        elif c in "\"'" and not (c == "'" and i > 0 and text[i - 1].isalnum()):
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            end = min(j + 1, n)
            out.append(re.sub(r"[^\n]", " ", text[i:end])); i = end
        else:
            out.append(c); i += 1
    return "".join(out)


def registered_ids(root: Path) -> list[str]:
    ids = []
    for name in REGISTRIES:
        path = root / name
        if not path.is_file():
            continue
        with path.open(encoding="utf-8", newline="") as file:
            rows = list(csv.reader(file))
        ids.extend(row[0].strip() for row in rows[1:] if row and row[0].strip())
    return ids


def owner_of(relative: str):
    """Returns ("core", rules), ("binding <name>", rules) or ("binding <name>", None) for an unlisted binding."""
    if not relative.startswith(BINDINGS_DIRECTORY):
        return "core", CORE
    name = relative[len(BINDINGS_DIRECTORY):].split("/", 1)[0]
    return f"binding {name}", BINDINGS.get(name)


def include_allowed(include: str, rules: dict) -> bool:
    if NATIVE_HEADER.search(include):
        return include in rules["native_headers"]
    if "/" not in include:
        return include in rules["bare_includes"] or not LIBRARY_BARE_INCLUDE.match(include)
    return include in rules.get("exact_includes", ()) or include.startswith(rules["include_prefixes"])


def link_findings(relative: str, text: str, rules: dict) -> list[str]:
    """The core's per-library link lists: each target_link_libraries call of a listed library names only its own
    libraries, the names inside a generator expression included; a library the CMake defines without a list, or
    through a variable, is a finding. Comments are dropped first. A call on a variable has no library to look up:
    its names meet the core's list only, and the configure-time check in tests/framework/CMakeLists.txt reads what
    the core libraries really link, whatever the spelling."""
    links = rules.get("links")
    if links is None:
        return []
    findings = []
    code = re.sub(r"#[^\n]*", "", text)
    if ADD_LIBRARY_VARIABLE.search(code):
        findings.append(f"{relative}: defines a library through a variable")
    for library in ADD_LIBRARY.findall(code):
        if library not in links:
            findings.append(f"{relative}: {library} has no link list in the guard")
    for call in LINK_CALL.findall(code):
        tokens = call.split()
        if not tokens or tokens[0] not in links:
            continue
        for token in tokens[1:]:
            token = token[1:-1] if len(token) >= 2 and token[0] == token[-1] == '"' else token  # CMake's "X" is X
            if token in LINK_KEYWORDS or token.startswith("${"):
                continue
            names = TARGET.findall(token) if token.startswith("$<") else [token]
            for name in names:
                if name not in links[tokens[0]]:
                    findings.append(f"{relative}: {tokens[0]} links {name}")
    return findings


def file_findings(relative: str, path: Path, named) -> list[str]:
    findings = []
    if path.name == "project.json":
        return [f"{relative}: a product's project file"]
    text = path.read_text(encoding="utf-8", errors="replace")
    for identifier, pattern in named:
        if pattern.search(text):
            findings.append(f"{relative}: names {identifier}")
    owner, rules = owner_of(relative)
    if rules is None:
        return findings + [f"{relative}: {owner} has no positive list in the guard"]
    if path.suffix in SUFFIXES:
        for include in INCLUDE.findall(text):
            if not include_allowed(include, rules):
                findings.append(f"{relative}: includes {include}")
        code = strip_comments_and_strings(INCLUDE.sub("", text))  # an include is reported by the include rule
        findings.extend(f"{relative}: uses {name}" for name in rules["symbols"].findall(code))
    elif path.name == "CMakeLists.txt" or path.suffix == ".cmake":
        for target in TARGET.findall(text):
            if target not in rules["targets"]:
                findings.append(f"{relative}: names {target}")
        for command in sorted(set(TOOL_COMMAND.findall(text))):
            findings.append(f"{relative}: calls {command}")
        findings.extend(link_findings(relative, text, rules))
    return findings


def scan(root: Path) -> list[str]:
    findings = []
    ids = registered_ids(root)
    if "framework" in ids:
        findings.append("engine/config: registers framework as a product or tool")
    if (root / REMOVED_ALLOWLIST).exists():
        findings.append(f"{REMOVED_ALLOWLIST}: the framework has no exceptions; remove the file")
    named = [(identifier, re.compile(rf"\b{re.escape(identifier)}\b")) for identifier in ids if identifier != "framework"]
    for path in sorted((root / OWNER).rglob("*")):
        if path.is_file():
            findings.extend(file_findings(path.relative_to(root).as_posix(), path, named))
    return findings


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    findings = scan(parser.parse_args().root)
    for finding in findings:
        print(f"framework outside its Design Invariants: {finding}")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
