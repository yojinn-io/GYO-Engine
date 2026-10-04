#!/usr/bin/env python3
"""Count failure-handling patterns for the Assert/Result unification plan.

Usage (from the repository root):
    python3 docs/architecture/plans/result-unification/scripts/result_audit.py [--detail PATTERN]

Prints a TSV of `pattern<TAB>area<TAB>count`. With --detail, prints every
`file:line<TAB>text` for one pattern instead. Lines whose first non-blank
characters are `//` are ignored; block comments are not, so a pattern quoted in
a block comment is still counted (report it in HANDOFF if it happens).

Areas in scope for full unification: engine, tools_ui_editor, tests_common,
tests_ui_editor. apps_pvp (product, test and acceptance code) is reported only
to size forced changes. Inactive products and the host shader tool
(engine/render/shaders/pipeline, a build-time tool) are never scanned.
"""

import argparse
import pathlib
import re
import sys

AREAS = {
    "engine": ["engine"],
    "tools_ui_editor": ["tools/ui_editor"],
    "tests_common": ["tests/common"],
    "tests_ui_editor": ["tests/ui_editor"],
    "apps_pvp": ["apps/object_fps_pvp", "tests/object_fps_pvp", "build/acceptance/object_fps_pvp"],
}

PATTERNS = {
    # R2: Result construction and queries
    "static_ok": r"::Ok\(",
    "static_err": r"(?<!Base)::Err\(",
    "base_err": r"\bBase::Err\(",
    "result_ok_call": r"\.ok\(\)",
    "io_result_void": r"\bIoResultVoid\b",
    # R3: Error semantics and aliases
    "error_code_none": r"ErrorCode::None\b",
    "error_code_value_init": r"ErrorCode\s*\{\s*\}",
    "error_getters": r"\.(CodeValue|Message|Detail)\(\)",
    "alias_asset_error": r"\busing\s+AssetError\s*=",
    "alias_io_error": r"\busing\s+IoError\s*=",
    "alias_io_result": r"\busing\s+IoResult\s*=",
    # R4: string errors
    "string_error_type": r"Result<[^;]*,\s*std::string\s*>",
    # R1/R5: Programmer Error signalling and out-params
    "throw_logic": r"\bthrow\s+std::(invalid_argument|logic_error|out_of_range|domain_error)\b",
    "throws_test": r"\b(CHECK|REQUIRE|WARN)_THROWS(_AS|_WITH)?\b",
    "cassert": r"\bassert\(|<cassert>",
    "string_error_out_param": r"std::string\s*[&*]\s*(out)?[eE]rror\b",
    "gyo_assert": r"\bGYO_ASSERT\(",
}

SUFFIXES = {".hpp", ".cpp", ".h", ".inl"}

# Build-time tools are exempt from the runtime failure rules.
EXCLUDED = ("engine/render/shaders/pipeline/",)


def iter_files(root, prefixes):
    for prefix in prefixes:
        base = root / prefix
        if not base.exists():
            continue
        for path in sorted(base.rglob("*")):
            relative = path.relative_to(root).as_posix()
            if path.is_file() and path.suffix in SUFFIXES and not relative.startswith(EXCLUDED):
                yield path


def scan(root, regex):
    for area, prefixes in AREAS.items():
        for path in iter_files(root, prefixes):
            try:
                lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
            except OSError as error:
                print(f"cannot read {path}: {error}", file=sys.stderr)
                continue
            for number, line in enumerate(lines, 1):
                if line.lstrip().startswith("//"):
                    continue
                for _ in regex.finditer(line):
                    yield area, path.relative_to(root).as_posix(), number, line.strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--detail", choices=sorted(PATTERNS), help="list matches for one pattern")
    parser.add_argument("--root", default=".", help="repository root (default: current directory)")
    args = parser.parse_args()
    root = pathlib.Path(args.root).resolve()

    if args.detail:
        for area, file, number, text in scan(root, re.compile(PATTERNS[args.detail])):
            print(f"{area}\t{file}:{number}\t{text}")
        return 0

    print("pattern\tarea\tcount")
    for name, pattern in PATTERNS.items():
        counts = {area: 0 for area in AREAS}
        for area, *_ in scan(root, re.compile(pattern)):
            counts[area] += 1
        for area, count in counts.items():
            print(f"{name}\t{area}\t{count}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
