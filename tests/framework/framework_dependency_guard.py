"""Guard for the framework's Design Invariants 1-3 (docs/framework/architecture.md).

The rules read the registries instead of naming any product or tool (AGENTS.md sections 7 and 9):
- Invariant 1, not a product: framework/ has no project.json, its CMake files add no executable and no install
  target, and neither registry has a row named framework;
- Invariant 2, one direction, core and bindings: everything under framework/bindings/<name>/ is a binding, the rest
  is the core.
  - Everywhere: no include reaches engine/input, no SDL_GPU function or type (SDL_*GPU*) is used.
  - The core includes only gyo/framework/ and Engine's neutral headers (no *Native.hpp) besides standard and local
    headers, and nothing whose name starts with SDL, imgui or json.hpp; it names no SDL_, ImGui or nlohmann symbol
    in code; its CMake files name only the core's targets.
  - Each binding has its own positive list (BINDINGS); a directory under framework/bindings/ without one fails.
    Only the ImGui binding may include a *Native.hpp header or name a GYO::*Native target, and no binding names
    SDL3::SDL3 or an SDL_ symbol.
- Invariant 3, neutral: no file names an id registered in engine/config/tools.csv or engine/config/projects.csv.

Existing core sites are frozen in tests/framework/framework_guard_allowlist.txt, a ratchet in the format of
tests/common/os/os_boundary_guard.py: each line is "path rule count # reason" for the rules include, symbol and
target; the file must exist, lie in the core, and its finding count for that rule must equal count, so a new site
fails and a removed one asks for the entry to shrink. Lines with rule "link" ("target:<cmake target> link <count>
<name>,<name>...") belong to the configure-time link check in tests/framework/CMakeLists.txt and are only checked
for form here. The registries, project files,
executables, engine/input, SDL_GPU and anything the guard refused before v8-framework-guard-ratchet (an include
outside its old prefixes, a target outside its old list and the shell's and bindings' targets) are never
allowlisted, nor is anything in a binding.
Includes, targets and registered names are read from the raw text (a comment counts); symbols from the code only,
with include lines removed and comments and literals blanked.

A new dependency fails until it is added here, which is the point: it changes the framework's Design Invariants.

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
ALLOWLIST = "tests/framework/framework_guard_allowlist.txt"
REGISTRIES = ("engine/config/tools.csv", "engine/config/projects.csv")
SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".mm"}
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)
SDL_GPU = re.compile(r"\bSDL_\w*GPU\w*")
SYMBOL = re.compile(r"\bSDL_\w+|\bImGui\w*|\bnlohmann\b")
TARGET = re.compile(r"\b[A-Za-z_][A-Za-z0-9_]*::[A-Za-z_][A-Za-z0-9_]*\b")
TOOL_COMMAND = re.compile(r"\b(add_executable|gyo_tool_install_target|gyo_install_product_target)\s*\(")
NATIVE_HEADER = re.compile(r"^engine/.*(/native/|Native\.hpp$)")
# Bare names (no slash) that are SDL's, ImGui's or nlohmann's own headers, not a standard or local one.
LIBRARY_BARE_INCLUDE = re.compile(r"^(SDL\.h|SDL_\w*\.h|imgui(_\w+)?\.h|json\.hpp)$")

FORBIDDEN_INCLUDE_PREFIXES = ("engine/input/",)
# What the guard allowed before v8-framework-guard-ratchet. Anything outside it stays a hard finding everywhere,
# so the allowlist's counts can never trade an old site for something the old guard refused. The only names added
# since are the shell's and the bindings' own targets.
OLD_INCLUDE_PREFIXES = ("gyo/framework/", "engine/", "nlohmann/", "SDL3/", "backends/imgui_impl_sdl")
OLD_TARGETS = {
    "GYO::ToolFramework", "GYO::ToolFrameworkGui",
    "GYO::Engine", "GYO::Os",
    "GYO::PlatformSDL", "GYO::PlatformSDLNative", "GYO::RenderBackendSDL", "GYO::RenderBackendSDLNative",
    "SDL3::SDL3", "imgui::core", "imgui::sdl3_sdlrenderer3", "nlohmann_json::nlohmann_json",
}
NEW_TARGETS = {"GYO::ToolFrameworkWindow", "GYO::ToolFrameworkImGui", "GYO::ToolFrameworkJson"}
ALLOWLIST_RULES = ("include", "symbol", "target")
LINK_RULE = "link"

CORE = {
    "include_prefixes": ("gyo/framework/", "engine/"),
    "bare_includes": (),
    "native_headers": (),
    "symbols": re.compile(r"\bSDL_\w+|\bImGui\w*|\bnlohmann\b"),
    "targets": {"GYO::ToolFramework", "GYO::ToolFrameworkWindow", "GYO::Engine", "GYO::Os", "GYO::PlatformSDL"},
}
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
                    "GYO::RenderBackendSDLNative", "imgui::core", "imgui::sdl3_sdlrenderer3"},
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


def file_findings(relative: str, path: Path, named) -> tuple[list[str], list[tuple[str, str]]]:
    """Returns (hard findings, [(rule, finding)] that the allowlist may cover)."""
    hard, soft = [], []
    if path.name == "project.json":
        return [f"{relative}: a product's project file"], []
    text = path.read_text(encoding="utf-8", errors="replace")
    for identifier, pattern in named:
        if pattern.search(text):
            hard.append(f"{relative}: names {identifier}")
    owner, rules = owner_of(relative)
    if rules is None:
        return hard + [f"{relative}: {owner} has no positive list in the guard"], []
    if path.suffix in SUFFIXES:
        for include in INCLUDE.findall(text):
            if include.startswith(FORBIDDEN_INCLUDE_PREFIXES) or (
                    "/" in include and not include.startswith(OLD_INCLUDE_PREFIXES)):
                hard.append(f"{relative}: includes {include}")
            elif not include_allowed(include, rules):
                soft.append(("include", f"{relative}: includes {include}"))
        gpu = sorted(set(SDL_GPU.findall(text)))
        hard.extend(f"{relative}: uses {name}" for name in gpu)
        code = strip_comments_and_strings(INCLUDE.sub("", text))  # an include is counted by the include rule
        for name in rules["symbols"].findall(code):
            if name not in gpu:
                soft.append(("symbol", f"{relative}: uses {name}"))
    elif path.name == "CMakeLists.txt" or path.suffix == ".cmake":
        for target in TARGET.findall(text):
            if target not in OLD_TARGETS | NEW_TARGETS:
                hard.append(f"{relative}: names {target}")
            elif target not in rules["targets"]:
                soft.append(("target", f"{relative}: names {target}"))
        for command in sorted(set(TOOL_COMMAND.findall(text))):
            hard.append(f"{relative}: calls {command}")
    if owner != "core":
        return hard + [finding for _, finding in soft], []
    return hard, soft


def load_allowlist(root: Path):
    """Returns ({(path, rule): (count, where)}, [problem lines])."""
    entries, problems = {}, []
    allowlist = root / ALLOWLIST
    if not allowlist.is_file():
        return entries, problems
    for number, raw in enumerate(allowlist.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        where = f"{ALLOWLIST}:{number}"
        fields = line.split()
        if len(fields) >= 2 and fields[1] == LINK_RULE:
            names = fields[3].split(",") if len(fields) == 4 else []
            if (len(fields) != 4 or not fields[0].startswith("target:") or not fields[2].isdigit()
                    or int(fields[2]) != len(names) or int(fields[2]) == 0):
                problems.append(f"{where}: expected 'target:<cmake target> link count name,name...', got '{line}'")
            continue  # checked at configure time by tests/framework/CMakeLists.txt
        if len(fields) != 3 or fields[1] not in ALLOWLIST_RULES or not fields[2].isdigit() or int(fields[2]) == 0:
            problems.append(f"{where}: expected 'path rule count', got '{line}'")
            continue
        path, rule, count = fields[0], fields[1], int(fields[2])
        if not path.startswith(OWNER + "/") or owner_of(path)[0] != "core":
            problems.append(f"{where}: {path} is not in the framework's core; bindings have no exceptions")
            continue
        if not (root / path).is_file():
            problems.append(f"{where}: {path} does not exist; remove the entry")
            continue
        if (path, rule) in entries:
            problems.append(f"{where}: duplicate entry for {path} {rule}")
            continue
        entries[(path, rule)] = (count, where)
    return entries, problems


def scan(root: Path) -> list[str]:
    findings = []
    ids = registered_ids(root)
    if "framework" in ids:
        findings.append("engine/config: registers framework as a product or tool")
    named = [(identifier, re.compile(rf"\b{re.escape(identifier)}\b")) for identifier in ids if identifier != "framework"]
    found = {}
    for path in sorted((root / OWNER).rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(root).as_posix()
        hard, soft = file_findings(relative, path, named)
        findings.extend(hard)
        for rule, finding in soft:
            found.setdefault((relative, rule), []).append(finding)
    entries, problems = load_allowlist(root)
    findings.extend(problems)
    for key, lines in sorted(found.items()):
        allowed = entries.get(key)
        if allowed is None:
            findings.extend(lines)
        elif len(lines) > allowed[0]:
            findings.extend(lines)
            findings.append(f"{allowed[1]}: {key[0]} {key[1]} has {len(lines)} findings, allowlisted {allowed[0]}")
    for key, (count, where) in sorted(entries.items()):
        actual = len(found.get(key, []))
        if actual < count:
            findings.append(f"{where}: {key[0]} {key[1]} has {actual} findings, allowlisted {count}; "
                            f"shrink the allowlist")
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
