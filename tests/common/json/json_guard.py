"""Include and link guard for the JSON Module's adapter (Design Invariant J-1 of
docs/architecture/target/v2/json-module-declaration.zh-Hant.md).

The engine's two external JSON components appear only in their own adapter directory:

  J1-YYJSON      an #include of yyjson.h, outside engine/json/src/adapters/yyjson/;
  J1-FASTFLOAT   an #include of a fast_float header, outside engine/json/src/adapters/fast_float/;
  J1-LINK        the name of a component target (gyo_yyjson, gyo_fast_float) in a CMakeLists.txt,
                 outside engine/json/CMakeLists.txt.

Scanned: engine/ (C/C++ files and CMakeLists.txt). tests/common/json/ is the adapter's own test
directory and may name both components; the rest of tests/ is scanned like engine/. Product
directories are not scanned: the user decided that this guard keeps the Engine side only
(adapter-plugins-plan, line 15). third_party/ defines the targets and is not scanned.
Includes are read from code lines; a comment or a string that mentions a header is not an include.
CMake comments (from '#' to the end of the line) are ignored.

Positive control: the real adapters must contain at least one allowed include of each component;
none means the scan cannot see what it is looking for, and the guard fails. A missing scan root
fails too (never "no finding" because nothing was read).

Usage: python3 json_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import re
import sys
from pathlib import Path

SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl"}
SCAN_ROOTS = ("engine", "tests")
ADAPTER_TESTS = "tests/common/json/"
RULES = (
    # rule, include pattern, the one allowed directory
    ("J1-YYJSON", re.compile(r"^[ \t]*#[ \t]*include[ \t]*[<\"](?:[^>\"]*/)?yyjson\.h[>\"]", re.MULTILINE),
     "engine/json/src/adapters/yyjson/"),
    ("J1-FASTFLOAT", re.compile(r"^[ \t]*#[ \t]*include[ \t]*[<\"]fast_float/[^>\"]*[>\"]", re.MULTILINE),
     "engine/json/src/adapters/fast_float/"),
)
LINK = re.compile(r"\bgyo_(?:yyjson|fast_float)\b")
LINK_ALLOWED = ("engine/json/CMakeLists.txt",)


def strip_block_comments(text):
    """Blank /* */ comments so an include inside one is not read as code."""
    return re.sub(r"/\*.*?\*/", lambda match: re.sub(r"[^\n]", " ", match.group(0)), text, flags=re.DOTALL)


def scan(root):
    """Returns (findings, allowed include counts per rule, errors)."""
    findings = []
    allowed = {rule: 0 for rule, _, _ in RULES}
    errors = []
    for top in SCAN_ROOTS:
        base = root / top
        if not base.is_dir():
            errors.append(f"scan root {top}/ not found under {root}")
            continue
        for path in sorted(base.rglob("*")):
            if not path.is_file():
                continue
            relative = path.relative_to(root).as_posix()
            if relative.startswith(ADAPTER_TESTS):
                continue
            if path.suffix in SOURCE_SUFFIXES:
                text = strip_block_comments(path.read_text(encoding="utf-8", errors="replace"))
                for rule, pattern, home in RULES:
                    for match in pattern.finditer(text):
                        if relative.startswith(home):
                            allowed[rule] += 1
                        else:
                            line = text.count("\n", 0, match.start()) + 1
                            findings.append(f"{relative}:{line}: {rule}: {match.group(0).strip()} outside {home}")
            elif path.name == "CMakeLists.txt":
                if relative in LINK_ALLOWED:
                    continue
                for number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                    code = line.split("#", 1)[0]
                    match = LINK.search(code)
                    if match:
                        findings.append(f"{relative}:{number}: J1-LINK: {match.group(0)} outside {LINK_ALLOWED[0]}")
    return findings, allowed, errors


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[3]))
    args = parser.parse_args(argv)
    root = Path(args.root)
    findings, allowed, errors = scan(root)
    for rule, count in allowed.items():
        if count == 0:
            errors.append(f"{rule}: no allowed include found in its adapter directory (the scan sees nothing)")
    for line in errors + findings:
        print(line)
    if errors or findings:
        return 1
    print("json_guard: clean (" + ", ".join(f"{rule} allowed {count}" for rule, count in allowed.items()) + ")")
    return 0


if __name__ == "__main__":
    sys.exit(main())
