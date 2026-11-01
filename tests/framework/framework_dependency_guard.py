"""Guard for the framework's Design Invariants 1-3 (docs/framework/architecture.md).

The rules read the registries instead of naming any product or tool (AGENTS.md sections 7 and 9):
- Invariant 1, not a product: framework/ has no project.json, its CMake files add no executable and no install
  target, and neither registry has a row named framework;
- Invariant 2, one direction: every path include starts with an allowed prefix and none reaches engine/input, a
  game's or a tool's own headers, no source uses an SDL_GPU function or type (SDL_*GPU*), and every namespaced target
  (Name::Name) in the CMake files is an allowed target;
- Invariant 3, neutral: no file names an id registered in engine/config/tools.csv or engine/config/projects.csv.
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
REGISTRIES = ("engine/config/tools.csv", "engine/config/projects.csv")
SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".mm"}
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)
SDL_GPU = re.compile(r"\bSDL_\w*GPU\w*")
TARGET = re.compile(r"\b[A-Za-z_][A-Za-z0-9_]*::[A-Za-z_][A-Za-z0-9_]*\b")
TOOL_COMMAND = re.compile(r"\b(add_executable|gyo_tool_install_target|gyo_install_product_target)\s*\(")

ALLOWED_INCLUDE_PREFIXES = ("gyo/framework/", "engine/", "nlohmann/", "SDL3/", "backends/imgui_impl_sdl")
FORBIDDEN_INCLUDE_PREFIXES = ("engine/input/",)
ALLOWED_TARGETS = {
    "GYO::ToolFramework", "GYO::ToolFrameworkGui",
    "GYO::Engine", "GYO::Os",
    "GYO::PlatformSDL", "GYO::PlatformSDLNative", "GYO::RenderBackendSDL", "GYO::RenderBackendSDLNative",
    "SDL3::SDL3", "imgui::core", "imgui::sdl3_sdlrenderer3", "nlohmann_json::nlohmann_json",
}


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


def scan(root: Path) -> list[str]:
    findings = []
    ids = registered_ids(root)
    if "framework" in ids:
        findings.append("engine/config: registers framework as a product or tool")
    named = [(identifier, re.compile(rf"\b{re.escape(identifier)}\b")) for identifier in ids if identifier != "framework"]
    for path in sorted((root / OWNER).rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(root).as_posix()
        if path.name == "project.json":
            findings.append(f"{relative}: a product's project file")
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for identifier, pattern in named:
            if pattern.search(text):
                findings.append(f"{relative}: names {identifier}")
        if path.suffix in SUFFIXES:
            for include in INCLUDE.findall(text):
                if "/" not in include:
                    continue  # standard headers and the framework's own local headers
                if include.startswith(FORBIDDEN_INCLUDE_PREFIXES) or not include.startswith(ALLOWED_INCLUDE_PREFIXES):
                    findings.append(f"{relative}: includes {include}")
            for name in sorted(set(SDL_GPU.findall(text))):
                findings.append(f"{relative}: uses {name}")
        elif path.name == "CMakeLists.txt" or path.suffix == ".cmake":
            for target in sorted(set(TARGET.findall(text))):
                if target not in ALLOWED_TARGETS:
                    findings.append(f"{relative}: names {target}")
            for command in sorted(set(TOOL_COMMAND.findall(text))):
                findings.append(f"{relative}: calls {command}")
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
