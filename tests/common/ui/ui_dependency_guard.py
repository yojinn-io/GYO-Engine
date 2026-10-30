"""Guard for GYO::Ui Design Invariant 2 (docs/ui_toolchain.md): GYO::Ui does not depend on GYO::Input.

Under engine/ui, no C/C++ source includes an engine/input header and no CMake file names a GYO::Input target.
Products convert input into Ui's own neutral types (UiInputFrame) instead.

Usage: python3 ui_dependency_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import re
import sys
from pathlib import Path

OWNER = "engine/ui"
SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".mm"}
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)
INPUT_TARGET = re.compile(r"\bGYO::Input\w*")


def scan(root: Path) -> list[str]:
    findings = []
    owner = root / OWNER
    for path in sorted(owner.rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(root).as_posix()
        text = path.read_text(encoding="utf-8", errors="replace")
        if path.suffix in SUFFIXES:
            for include in INCLUDE.findall(text):
                if include.startswith("engine/input/"):
                    findings.append(f"{relative}: includes {include}")
        elif path.name == "CMakeLists.txt" or path.suffix == ".cmake":
            for target in INPUT_TARGET.findall(text):
                findings.append(f"{relative}: names {target}")
    return findings


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[3])
    findings = scan(parser.parse_args().root)
    for finding in findings:
        print(f"GYO::Ui depends on GYO::Input: {finding}")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
