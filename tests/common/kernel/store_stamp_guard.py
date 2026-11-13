"""Text guard: engine/entity_store holds no change stamp (target/world.zh-Hant.md section 6; world-exec).

The kernel's World keeps every column as std::vector<Cell<T>> through engine/entity_store's algorithms (layout 2,
"Cell<T>", of target/world section 6). The cell's two change stamps are the kernel's: the store must never name a
stamp field or decide on one (entity-store Design Invariant 6 and target/world section 6: "the store's source naming
a stamp field, or deciding on a stamp, is a violation").

Scans every file under engine/entity_store/ (code and comments, CMakeLists.txt included) for the words
  S1  stamp(s), Stamp(s) and words ending in them (ChangeStamp), added, changed, Added, Changed, lastRun, last_run,
      tick(s); each as a word or the start of a camelCase identifier (StampOf, tickOf), not inside a longer
      lowercase word (unchanged, stampede, ticket pass).
A hit has one reading: the store learned about change detection, which belongs to engine/kernel.
The scan root must exist and hold files: an empty or missing root is a finding, so the guard never passes by
scanning nothing. Files are read as UTF-8; a file that cannot be read is a finding.

Usage: python3 store_stamp_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import re
import sys
from pathlib import Path

SCANNED = "engine/entity_store"
S1 = re.compile(r"(?<![A-Za-z0-9])(?:[Ss]tamps?|\w*Stamps?|added|changed|Added|Changed|last_?[Rr]un|[Tt]icks?)(?![a-z])")


def findings(root):
    base = root / SCANNED
    files = sorted(p for p in base.rglob("*") if p.is_file()) if base.is_dir() else []
    if not files:
        yield f"{SCANNED}: S0 no file to scan (missing or empty root)"
        return
    for path in files:
        relative = path.relative_to(root).as_posix()
        try:
            text = path.read_text(encoding="utf-8-sig")
        except (OSError, UnicodeDecodeError) as error:
            yield f"{relative}: S0 unreadable ({error})"
            continue
        for number, line in enumerate(text.splitlines(), start=1):
            for match in S1.finditer(line):
                yield f"{relative}:{number}: S1 {match.group(0)}"


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[3]))
    args = parser.parse_args(argv)
    found = list(findings(Path(args.root)))
    for line in found:
        print(line)
    if found:
        print(f"store stamp guard: {len(found)} finding(s) (target/world section 6: engine/entity_store holds no change stamp)")
        return 1
    print("store stamp guard: clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
