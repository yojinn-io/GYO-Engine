"""Include guard of the two logging Modules (Design Invariants LG-4 and LG-10 of
docs/architecture/target/v2/logging-declaration.zh-Hant.md and target/v2/logging.zh-Hant.md).

Each Module includes only what its links allow, so that neither comes to know a product, another Module's
messages or the parts of the engine it does not depend on:

  LG4-LOGGING         an #include "engine/<module>/..." under engine/logging/ whose module is not base, json,
                      os, threads, time or logging;
  LG4-LOGGING-PUBLIC  an #include "engine/<module>/..." under engine/logging/include/ whose module is not base
                      or logging (the public side links GYO::Base only, LG-10);
  LG4-WORLD           an #include "engine/<module>/..." under engine/world_logging/ whose module is not base,
                      logging, kernel, orchestration or world_logging.

Only includes of the engine's form "engine/<module>/<header>" are read (a standard header or a header of the
Module's own src/ is not an engine include). Includes are read from code lines, with block comments blanked.

Positive control: the real trees must each contain at least one allowed engine include under every rule's
directory; none means the scan cannot see what it is looking for, and the guard fails. A missing directory
fails too (never "no finding" because nothing was read).

Usage: python3 logging_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import re
import sys
from pathlib import Path

SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl"}
ENGINE_INCLUDE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*[<"]engine/([A-Za-z0-9_]+)/[^>"\n]*[>"]', re.MULTILINE)
RULES = (
    # (rule, directory, allowed modules); a file is checked against every rule whose directory holds it.
    ("LG4-LOGGING", "engine/logging/", {"base", "json", "os", "threads", "time", "logging"}),
    ("LG4-LOGGING-PUBLIC", "engine/logging/include/", {"base", "logging"}),
    ("LG4-WORLD", "engine/world_logging/", {"base", "logging", "kernel", "orchestration", "world_logging"}),
)


def blank(match):
    return re.sub(r"[^\n]", " ", match.group(0))


def strip_block_comments(text):
    """Blank /* */ comments so an include inside one is not read as code."""
    return re.sub(r"/\*.*?\*/", blank, text, flags=re.DOTALL)


def line_of(text, index):
    return text.count("\n", 0, index) + 1


def scan(root):
    """Returns (findings, allowed counts per rule, errors)."""
    findings = []
    allowed = {rule: 0 for rule, _, _ in RULES}
    errors = []
    for rule, directory, modules in RULES:
        base = root / directory
        if not base.is_dir():
            errors.append(f"{rule}: {directory} not found under {root}")
            continue
        for path in sorted(base.rglob("*")):
            if not path.is_file() or path.suffix not in SOURCE_SUFFIXES:
                continue
            relative = path.relative_to(root).as_posix()
            text = strip_block_comments(path.read_text(encoding="utf-8", errors="replace"))
            for match in ENGINE_INCLUDE.finditer(text):
                module = match.group(1)
                if module in modules:
                    allowed[rule] += 1
                else:
                    findings.append(f"{relative}:{line_of(text, match.start())}: {rule}: "
                                    f"includes engine/{module}/ (allowed: {', '.join(sorted(modules))})")
    return findings, allowed, errors


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[3]))
    args = parser.parse_args(argv)
    root = Path(args.root)
    findings, allowed, errors = scan(root)
    for rule, count in allowed.items():
        if count == 0 and not any(line.startswith(rule + ":") for line in errors):
            errors.append(f"{rule}: no allowed engine include found in its directory (the scan sees nothing)")
    for line in errors + findings:
        print(line)
    if errors or findings:
        return 1
    print("logging_guard: clean (" + ", ".join(f"{rule} allowed {count}" for rule, count in allowed.items()) + ")")
    return 0


if __name__ == "__main__":
    sys.exit(main())
