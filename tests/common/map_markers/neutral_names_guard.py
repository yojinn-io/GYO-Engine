"""Guard for neutral names in Engine's map formats: GYO::MapMarkers Design Invariant 1
(docs/architecture/map-markers.md) and GYO::VoxelMesh Design Invariant 1 (docs/architecture/voxel-mesh.md).

Engine's map formats carry opaque names; what a name means belongs to a product (docs/architecture.md, Layers
check 1). So these modules, their tests and their contracts name no gameplay concept (decision
v8-arch-big-map-neutral-wording). The words below are the
gameplay nouns a map's markers are typically used for; none may appear, in any case, anywhere in those files
(comments and examples included). This file is the only place that lists them and is not scanned.

Usage: neutral_names_guard.py --root <repository root>. Exit status 0 when clean, 1 with the hits listed.
"""
import argparse
import re
import sys
from pathlib import Path

WORDS = ("spawn", "boss", "loot", "extract", "patrol", "team", "zone")
SCANNED = ("engine/map_markers", "tests/common/map_markers", "docs/architecture/map-markers.md",
           "engine/voxel_mesh", "tests/common/voxel_mesh", "docs/architecture/voxel-mesh.md")


def scan(root):
    pattern = re.compile("|".join(WORDS), re.IGNORECASE)
    this = Path(__file__).resolve()
    hits = []
    for entry in SCANNED:
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
    root = Path(parser.parse_args().root).resolve()
    hits = scan(root)
    for hit in hits:
        print(hit)
    if hits:
        print(f"neutral names guard: {len(hits)} gameplay word(s) in Engine map format files")
        return 1
    print("neutral names guard: clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
