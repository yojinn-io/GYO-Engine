"""Guard for neutral names in Engine's data contracts (decision v8-arch-big-map-neutral-wording).

Engine's formats carry opaque names; what a name means belongs to a product (docs/architecture.md, Layers check 1).
So a module that uses this guard, its tests and its contract name no gameplay concept. The words below are gameplay
nouns a map's data is typically used for; none may appear, in any case, anywhere in the scanned files (comments and
examples included). This file is the only place that lists them.

Each module registers its own test that calls this script with its own paths (tests/common/<module>/CMakeLists.txt),
so removing a module removes only its own check (AGENTS.md section 7). The modules that use it are the ones whose
CMakeLists register such a test; this file does not list them, so removing a module needs no change here.

Usage: neutral_names_guard.py --root <repository root> --scan <path> [<path> ...]
Paths are relative to the root, files or directories. Exit status 0 when clean, 1 with the hits listed.
"""
import argparse
import re
import sys
from pathlib import Path

WORDS = ("spawn", "boss", "loot", "extract", "patrol", "team", "zone")


def scan(root, entries):
    pattern = re.compile("|".join(WORDS), re.IGNORECASE)
    this = Path(__file__).resolve()
    hits = []
    for entry in entries:
        base = root / entry
        if not base.exists():
            hits.append(f"{entry}: missing (the guard scans it)")
            continue
        files = [base] if base.is_file() else sorted(p for p in base.rglob("*") if p.is_file())
        for path in files:
            if path.resolve() == this:
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            for number, line in enumerate(text.splitlines(), 1):
                for match in pattern.finditer(line):
                    hits.append(f"{path.relative_to(root)}:{number}: '{match.group(0)}'")
    return hits


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--scan", nargs="+", required=True)
    arguments = parser.parse_args()
    root = Path(arguments.root).resolve()
    hits = scan(root, arguments.scan)
    for hit in hits:
        print(hit)
    if hits:
        print(f"neutral names guard: {len(hits)} gameplay word(s) in {', '.join(arguments.scan)}")
        return 1
    print("neutral names guard: clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
