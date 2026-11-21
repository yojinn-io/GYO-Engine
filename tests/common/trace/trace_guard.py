"""Include guard of the trace Module (Design Invariant TR-1 of docs/architecture/target/v2/trace-declaration.zh-Hant.md
and target/v2/trace.zh-Hant.md).

The trace Module includes only what its links allow, so that it comes to know neither a product, the kernel nor
the orchestration layer:

  TR1-TRACE  an #include "engine/<module>/..." under engine/modules/trace/ whose module is not base, logging, time
             or trace.

The other half of TR-1 (the kernel and the orchestration layer never include the trace Module, only the Base
facade engine/base/Trace.hpp) is kept by their own include scans: kernel_guard.py K5 and orchestration_guard.py O4
allow engine/base/ and not engine/trace/.

Only includes of the engine's form "engine/<module>/<header>" are read; block comments are blanked.

Positive control: the real tree must contain at least one allowed engine include; none means the scan cannot see
what it is looking for, and the guard fails. A missing directory fails too (never "no finding" because nothing
was read).

Usage: python3 trace_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import re
import sys
from pathlib import Path

SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl"}
ENGINE_INCLUDE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*[<"]engine/([A-Za-z0-9_]+)/[^>"\n]*[>"]', re.MULTILINE)
RULE = "TR1-TRACE"
DIRECTORY = "engine/modules/trace/"
ALLOWED = {"base", "logging", "time", "trace"}


def blank(match):
    return re.sub(r"[^\n]", " ", match.group(0))


def strip_block_comments(text):
    return re.sub(r"/\*.*?\*/", blank, text, flags=re.DOTALL)


def scan(root):
    """Returns (findings, allowed count, errors)."""
    base = root / DIRECTORY
    if not base.is_dir():
        return [], 0, [f"{RULE}: {DIRECTORY} not found under {root}"]
    findings = []
    allowed = 0
    for path in sorted(base.rglob("*")):
        if not path.is_file() or path.suffix not in SOURCE_SUFFIXES:
            continue
        relative = path.relative_to(root).as_posix()
        text = strip_block_comments(path.read_text(encoding="utf-8", errors="replace"))
        for match in ENGINE_INCLUDE.finditer(text):
            module = match.group(1)
            if module in ALLOWED:
                allowed += 1
            else:
                line = text.count("\n", 0, match.start()) + 1
                findings.append(f"{relative}:{line}: {RULE}: includes engine/{module}/ (allowed: {', '.join(sorted(ALLOWED))})")
    return findings, allowed, []


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[3]))
    args = parser.parse_args(argv)
    findings, allowed, errors = scan(Path(args.root))
    if not errors and allowed == 0:
        errors.append(f"{RULE}: no allowed engine include found in its directory (the scan sees nothing)")
    for line in errors + findings:
        print(line)
    if errors or findings:
        return 1
    print(f"trace_guard: clean ({RULE} allowed {allowed})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
