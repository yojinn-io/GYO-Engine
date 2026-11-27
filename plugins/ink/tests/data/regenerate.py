"""Compile the ink Plugin's test stories (*.ink) to ink.json with inklecate.

The .ink files are the writer's sources and the .ink.json files the data the tests read; both are committed, so
CI needs no inklecate. Run this after changing a .ink file and commit both together:

  INKLECATE=/path/to/inklecate python3 plugins/ink/tests/data/regenerate.py

inklecate 1.1.1 (the ink version inkcpp reads), from the ink release page.
"""
import os
from pathlib import Path
import subprocess
import sys


def main():
    inklecate = os.environ.get("INKLECATE")
    if not inklecate:
        print("set INKLECATE to the inklecate 1.1.1 program", file=sys.stderr)
        return 1
    here = Path(__file__).resolve().parent
    failed = False
    for source in sorted(here.glob("*.ink")):
        target = source.with_name(source.name + ".json")
        result = subprocess.run([inklecate, "-o", str(target), str(source)], capture_output=True, text=True)
        if result.returncode != 0:
            print(f"{source.name}: inklecate failed\n{result.stdout}{result.stderr}", file=sys.stderr)
            failed = True
            continue
        # inklecate writes a byte order mark; the json is ASCII here, so drop it for plain diffs.
        text = target.read_text(encoding="utf-8-sig")
        target.write_text(text, encoding="utf-8", newline="\n")
        print(f"{source.name} -> {target.name}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
