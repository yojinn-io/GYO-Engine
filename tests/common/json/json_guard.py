"""Include, name and link guard for the JSON Module's adapter (Design Invariant J-1 of
docs/architecture/target/v2/json-module-declaration.zh-Hant.md and target/v2/json.zh-Hant.md).

The engine's two external JSON components appear only in their own adapter directory:

  J1-YYJSON           an #include of yyjson.h (any path), outside engine/modules/json/src/adapters/yyjson/;
  J1-FASTFLOAT        an #include of a fast_float header (fast_float/<name>.h or fast_float.h, any path;
                      the adapter's own headers are .hpp),
                      outside engine/modules/json/src/adapters/fast_float/;
  J1-YYJSON-NAME      a yyjson_ name in code, outside engine/modules/json/src/adapters/yyjson/ (a header of
                      the adapter included from elsewhere would hand out its types this way);
  J1-FASTFLOAT-NAME   fast_float:: in code, outside engine/modules/json/src/adapters/fast_float/;
  J1-LINK             a component target (gyo_yyjson, gyo_fast_float) or a component's source
                      directory variable (yyjson_SOURCE_DIR, fast_float_SOURCE_DIR) in CMake code,
                      outside engine/modules/json/CMakeLists.txt.

Scanned: C/C++ files under engine/ and tests/; CMake code (CMakeLists.txt and *.cmake) under
engine/, tests/ and build/cmake/, and the repository's CMakeLists.txt. tests/common/json/ is the
adapter's own test directory and may name both components. Product directories are not scanned:
the user decided that this guard keeps the Engine side only (adapter-plugins-plan, line 15).
third_party/ defines the targets and is not scanned. Includes are read from code lines (block
comments blanked); names are read from code with comments and string literals blanked; CMake
comments (from '#' to the end of the line) are ignored.

Positive control: the real adapters must contain at least one allowed include and one allowed
name of each component; none means the scan cannot see what it is looking for, and the guard
fails. A missing scan root fails too (never "no finding" because nothing was read).

Usage: python3 json_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import re
import sys
from pathlib import Path

SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl"}
SOURCE_ROOTS = ("engine", "tests")
CMAKE_ROOTS = ("engine", "tests", "build/cmake")
CMAKE_FILES = ("CMakeLists.txt",)  # at the repository root
ADAPTER_TESTS = "tests/common/json/"
YYJSON_HOME = "engine/modules/json/src/adapters/yyjson/"
FASTFLOAT_HOME = "engine/modules/json/src/adapters/fast_float/"
INCLUDE_RULES = (
    ("J1-YYJSON", re.compile(r"^[ \t]*#[ \t]*include[ \t]*[<\"](?:[^>\"\n]*/)?yyjson\.h[>\"]", re.MULTILINE),
     YYJSON_HOME),
    ("J1-FASTFLOAT",
     re.compile(r"^[ \t]*#[ \t]*include[ \t]*[<\"](?:[^>\"\n]*/)?fast_float(?:/[^>\"/\n]*\.h|\.h)[>\"]",
                re.MULTILINE),
     FASTFLOAT_HOME),
)
NAME_RULES = (
    ("J1-YYJSON-NAME", re.compile(r"\byyjson_\w+"), YYJSON_HOME),
    ("J1-FASTFLOAT-NAME", re.compile(r"\bfast_float::"), FASTFLOAT_HOME),
)
LINK = re.compile(r"\bgyo_(?:yyjson|fast_float)\b|\b(?:yyjson|fast_float)_SOURCE_DIR\b")
LINK_ALLOWED = ("engine/modules/json/CMakeLists.txt",)


def blank(match):
    return re.sub(r"[^\n]", " ", match.group(0))


def strip_block_comments(text):
    """Blank /* */ comments so an include inside one is not read as code."""
    return re.sub(r"/\*.*?\*/", blank, text, flags=re.DOTALL)


CODE_NOISE = re.compile(
    r'R"(?P<d>[^(\s"\\]{0,16})\(.*?\)(?P=d)"'  # raw string literal
    r'|/\*.*?\*/'                            # block comment
    r'|//[^\n]*'                             # line comment
    r'|"(?:\\.|[^"\\\n])*"'                  # string literal
    r"|'(?:\\.|[^'\\\n])*'",                 # character literal
    re.DOTALL)


def code_only(text):
    """Blank comments and string and character literals, keeping line breaks."""
    return CODE_NOISE.sub(blank, text)


def line_of(text, index):
    return text.count("\n", 0, index) + 1


def scan(root):
    """Returns (findings, allowed counts per rule, errors)."""
    findings = []
    allowed = {rule: 0 for rule, _, _ in INCLUDE_RULES + NAME_RULES}
    errors = []
    for top in SOURCE_ROOTS:
        base = root / top
        if not base.is_dir():
            errors.append(f"source scan root {top}/ not found under {root}")
            continue
        for path in sorted(base.rglob("*")):
            if not path.is_file() or path.suffix not in SOURCE_SUFFIXES:
                continue
            relative = path.relative_to(root).as_posix()
            if relative.startswith(ADAPTER_TESTS):
                continue
            raw = path.read_text(encoding="utf-8", errors="replace")
            for rules, text in ((INCLUDE_RULES, strip_block_comments(raw)), (NAME_RULES, code_only(raw))):
                for rule, pattern, home in rules:
                    for match in pattern.finditer(text):
                        if relative.startswith(home):
                            allowed[rule] += 1
                        else:
                            findings.append(f"{relative}:{line_of(text, match.start())}: {rule}: "
                                            f"{match.group(0).strip()} outside {home}")
    cmake_paths = []
    for top in CMAKE_ROOTS:
        base = root / top
        if not base.is_dir():
            errors.append(f"CMake scan root {top}/ not found under {root}")
            continue
        cmake_paths += [p for p in sorted(base.rglob("*"))
                        if p.is_file() and (p.name == "CMakeLists.txt" or p.suffix == ".cmake")]
    cmake_paths += [root / name for name in CMAKE_FILES if (root / name).is_file()]
    for path in cmake_paths:
        relative = path.relative_to(root).as_posix()
        if relative in LINK_ALLOWED or relative.startswith(ADAPTER_TESTS):
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            match = LINK.search(line.split("#", 1)[0])
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
            errors.append(f"{rule}: nothing allowed found in its adapter directory (the scan sees nothing)")
    for line in errors + findings:
        print(line)
    if errors or findings:
        return 1
    print("json_guard: clean (" + ", ".join(f"{rule} allowed {count}" for rule, count in allowed.items()) + ")")
    return 0


if __name__ == "__main__":
    sys.exit(main())
