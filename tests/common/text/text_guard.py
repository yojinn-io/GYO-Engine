"""Include, name and link guard for the text Module's adapters (Design Invariant TX-1 of
docs/architecture/target/v2/text.zh-Hant.md; declaration text-declaration.zh-Hant.md 8.6).

The text Module's three external components appear only in their own adapter directory, and
hb-ft (HarfBuzz's FreeType glue) appears nowhere:

  TX1-FREETYPE           an #include of ft2build.h or a freetype/<name>.h header (any path; the
                         adapters' own headers are .hpp), outside
                         engine/modules/text/src/adapters/freetype/;
  TX1-HARFBUZZ           an #include of hb.h or an hb-*.h header (any path), outside
                         engine/modules/text/src/adapters/harfbuzz/;
  TX1-HBFT               an #include of hb-ft.h, anywhere;
  TX1-LIBUNIBREAK        an #include of a libunibreak header (linebreak.h, graphemebreak.h,
                         wordbreak.h, unibreakbase.h, unibreakdef.h, linebreakdef.h, any path),
                         outside engine/modules/text/src/adapters/libunibreak/;
  TX1-FREETYPE-NAME      an FT_ name in code, outside the FreeType adapter;
  TX1-HARFBUZZ-NAME      an hb_ name in code, outside the HarfBuzz adapter;
  TX1-LIBUNIBREAK-NAME   a libunibreak name (set_linebreaks_*, set_graphemebreaks_*,
                         set_wordbreaks_*, LINEBREAK_*, GRAPHEMEBREAK_*) in code, outside the
                         libunibreak adapter;
  TX1-LINK               a component target (gyo_freetype, gyo_harfbuzz, gyo_libunibreak) or a
                         component's source directory variable in CMake code, outside
                         engine/modules/text/CMakeLists.txt and tests/common/text/CMakeLists.txt
                         (which checks the Module's links and reads Unicode's test files from
                         libunibreak's release).

Scanned: C/C++ files and CMake code (CMakeLists.txt, *.cmake) under engine/ and tests/ (they must
exist) and under plugins/, games/, tools/ and examples/ when present (a Plugin that drew text with
FreeType itself, such as imgui_freetype, is caught here); build/cmake/ and the repository's
CMakeLists.txt. third_party/ defines the targets and is not scanned. The tests have no exception:
they use the adapters' own interfaces. Includes are read from code lines (block comments blanked);
names from code with comments and string literals blanked; CMake comments are ignored.

Positive control: each adapter must contain at least one allowed include and one allowed name of
its component; none means the scan cannot see what it looks for, and the guard fails. A missing
required root fails too.

Usage: python3 text_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import re
import sys
from pathlib import Path

SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl"}
REQUIRED_ROOTS = ("engine", "tests")
OPTIONAL_ROOTS = ("plugins", "games", "tools", "examples")
CMAKE_EXTRA = ("build/cmake",)
CMAKE_FILES = ("CMakeLists.txt",)
ADAPTERS = "engine/modules/text/src/adapters/"
FREETYPE_HOME = ADAPTERS + "freetype/"
HARFBUZZ_HOME = ADAPTERS + "harfbuzz/"
LIBUNIBREAK_HOME = ADAPTERS + "libunibreak/"
NOWHERE = None


def include(names):
    return re.compile(r"^[ \t]*#[ \t]*include[ \t]*[<\"](?:[^>\"\n]*/)?(?:" + names + r")[>\"]", re.MULTILINE)


INCLUDE_RULES = (
    ("TX1-FREETYPE", include(r"ft2build\.h|freetype/[^>\"\n]*\.h"), FREETYPE_HOME),
    ("TX1-HBFT", include(r"hb-ft\.h"), NOWHERE),
    ("TX1-HARFBUZZ", include(r"hb\.h|hb-(?!ft\.h)[\w-]+\.h"), HARFBUZZ_HOME),
    ("TX1-LIBUNIBREAK", include(r"(?:linebreak|graphemebreak|wordbreak|unibreakbase|unibreakdef|linebreakdef)\.h"),
     LIBUNIBREAK_HOME),
)
NAME_RULES = (
    ("TX1-FREETYPE-NAME", re.compile(r"\bFT_[A-Z]\w*"), FREETYPE_HOME),
    ("TX1-HARFBUZZ-NAME", re.compile(r"\bhb_[a-z]\w*"), HARFBUZZ_HOME),
    ("TX1-LIBUNIBREAK-NAME",
     re.compile(r"\b(?:set_(?:line|grapheme|word)breaks_\w+|LINEBREAK_[A-Z]+|GRAPHEMEBREAK_[A-Z]+)\b"),
     LIBUNIBREAK_HOME),
)
LINK = re.compile(r"\bgyo_(?:freetype|harfbuzz|libunibreak)\b|\b(?:freetype|harfbuzz|libunibreak)_SOURCE_DIR\b")
LINK_ALLOWED = ("engine/modules/text/CMakeLists.txt", "tests/common/text/CMakeLists.txt")


def blank(match):
    return re.sub(r"[^\n]", " ", match.group(0))


def strip_block_comments(text):
    return re.sub(r"/\*.*?\*/", blank, text, flags=re.DOTALL)


CODE_NOISE = re.compile(
    r'R"(?P<d>[^(\s"\\]{0,16})\(.*?\)(?P=d)"'
    r'|/\*.*?\*/'
    r'|//[^\n]*'
    r'|"(?:\\.|[^"\\\n])*"'
    r"|'(?:\\.|[^'\\\n])*'",
    re.DOTALL)


def code_only(text):
    return CODE_NOISE.sub(blank, text)


def line_of(text, index):
    return text.count("\n", 0, index) + 1


def roots(root, errors):
    found = []
    for top in REQUIRED_ROOTS:
        if (root / top).is_dir():
            found.append(root / top)
        else:
            errors.append(f"scan root {top}/ not found under {root}")
    found += [root / top for top in OPTIONAL_ROOTS if (root / top).is_dir()]
    return found


def scan(root):
    """Returns (findings, allowed counts per rule, errors)."""
    findings = []
    errors = []
    allowed = {rule: 0 for rule, _, home in INCLUDE_RULES + NAME_RULES if home is not NOWHERE}
    bases = roots(root, errors)
    for base in bases:
        for path in sorted(base.rglob("*")):
            if not path.is_file() or path.suffix not in SOURCE_SUFFIXES:
                continue
            relative = path.relative_to(root).as_posix()
            raw = path.read_text(encoding="utf-8", errors="replace")
            for rules, text in ((INCLUDE_RULES, strip_block_comments(raw)), (NAME_RULES, code_only(raw))):
                for rule, pattern, home in rules:
                    for match in pattern.finditer(text):
                        if home is not NOWHERE and relative.startswith(home):
                            allowed[rule] += 1
                            continue
                        where = "anywhere" if home is NOWHERE else f"outside {home}"
                        findings.append(f"{relative}:{line_of(text, match.start())}: {rule}: "
                                        f"{match.group(0).strip()} {where}")
    cmake_paths = []
    for base in bases + [root / top for top in CMAKE_EXTRA if (root / top).is_dir()]:
        cmake_paths += [p for p in sorted(base.rglob("*"))
                        if p.is_file() and (p.name == "CMakeLists.txt" or p.suffix == ".cmake")]
    cmake_paths += [root / name for name in CMAKE_FILES if (root / name).is_file()]
    for path in cmake_paths:
        relative = path.relative_to(root).as_posix()
        if relative in LINK_ALLOWED:
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            match = LINK.search(line.split("#", 1)[0])
            if match:
                findings.append(f"{relative}:{number}: TX1-LINK: {match.group(0)} outside "
                                + " and ".join(LINK_ALLOWED))
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
    print("text_guard: clean (" + ", ".join(f"{rule} allowed {count}" for rule, count in allowed.items()) + ")")
    return 0


if __name__ == "__main__":
    sys.exit(main())
