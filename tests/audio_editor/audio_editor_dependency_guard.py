"""Guard for audio_editor Design Invariant 1 (docs/audio_editor/architecture.md): no game, GYO::Input or SDL_GPU.

The rule is a positive list, so no product is named here (AGENTS.md sections 7 and 9):
- every path include in tools/audio_editor starts with an allowed prefix;
- no source uses an SDL_GPU function or type (SDL_*GPU*);
- every namespaced target (Name::Name) in the tool's CMake files is an allowed target.
A new dependency fails until it is added here, which is the point: it changes the editor's Design Invariants.

Usage: python3 audio_editor_dependency_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import re
import sys
from pathlib import Path

OWNER = "tools/audio_editor"
SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".mm"}
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)
SDL_GPU = re.compile(r"\bSDL_\w*GPU\w*")
TARGET = re.compile(r"\b[A-Za-z_][A-Za-z0-9_]*::[A-Za-z_][A-Za-z0-9_]*\b")

# The catalog entry is written through the framework layer, whose own guard
# (tests/framework/framework_dependency_guard.py) keeps the same rule (docs/framework/architecture.md, Design
# Invariant 2).
ALLOWED_INCLUDE_PREFIXES = ("gyo/audio_editor/", "gyo/framework/", "engine/base/", "engine/os/", "engine/audio/")
ALLOWED_TARGETS = {"GYO::Base", "GYO::Audio", "GYO::Os", "GYO::ToolFramework"}


def scan(root: Path) -> list[str]:
    findings = []
    for path in sorted((root / OWNER).rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(root).as_posix()
        text = path.read_text(encoding="utf-8", errors="replace")
        if path.suffix in SUFFIXES:
            for include in INCLUDE.findall(text):
                if "/" not in include:
                    continue  # standard headers and the tool's own local headers
                if not include.startswith(ALLOWED_INCLUDE_PREFIXES):
                    findings.append(f"{relative}: includes {include}")
            for name in sorted(set(SDL_GPU.findall(text))):
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
        print(f"audio_editor dependency outside its Design Invariants: {finding}")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
