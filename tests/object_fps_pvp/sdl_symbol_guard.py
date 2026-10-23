"""Completion guard (a) of sdl-isolation (docs/object_fps_pvp/plans/v7.1/sdl-isolation-ip4-declaration.md section 4).

The product, its acceptance probes and its tests call no SDL directly: their C/C++ sources, with comments and string
literals removed, name no SDL symbol, include no SDL header, no Engine *Native.hpp header, and call none of the
Native access functions. The exception list starts empty and stays empty (v7 D33).

Usage: python3 sdl_symbol_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import re
import sys
from pathlib import Path

DIRECTORIES = ("apps/object_fps_pvp", "build/acceptance/object_fps_pvp", "tests/object_fps_pvp")
SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl"}
# Applied to code with comments and string literals removed.
CODE_PATTERNS = (
    re.compile(r"\bSDL_\w+"),
    re.compile(r"\bSDLK_\w+"),
    re.compile(r"\bSDLCALL\b"),
    re.compile(r"\b[US]int(?:8|16|32|64)\b"),
    re.compile(r"\b(?:NativeWindow|NativeRenderer|NativeEvent|HandleNativeEvent)\s*\("),
)
# Applied to #include lines as written (their path is a string or <...>).
INCLUDE_PATTERNS = (
    re.compile(r"#\s*include\s*<SDL3/"),
    re.compile(r"#\s*include\s*[<\"][^>\"]*Native\.hpp[>\"]"),
)


def strip_comments_and_strings(text):
    """Replaces comments, string and character literals with spaces, keeping line breaks."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            end = text.find("\n", i)
            end = n if end < 0 else end
            out.append(" " * (end - i)); i = end
        elif text.startswith("/*", i):
            end = text.find("*/", i + 2)
            end = n if end < 0 else end + 2
            out.append(re.sub(r"[^\n]", " ", text[i:end])); i = end
        elif c == "R" and text.startswith('R"', i) and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == "_")):
            delimiter_end = text.find("(", i + 2)
            closing = ")" + text[i + 2:delimiter_end] + '"'
            end = text.find(closing, delimiter_end)
            end = n if end < 0 else end + len(closing)
            out.append(re.sub(r"[^\n]", " ", text[i:end])); i = end
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            end = min(j + 1, n)
            out.append(re.sub(r"[^\n]", " ", text[i:end])); i = end
        else:
            out.append(c); i += 1
    return "".join(out)


def findings(root):
    for directory in DIRECTORIES:
        base = root / directory
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if path.suffix not in SUFFIXES or not path.is_file():
                continue
            text = path.read_text(encoding="utf-8")
            relative = path.relative_to(root).as_posix()
            for number, line in enumerate(text.splitlines(), 1):
                for pattern in INCLUDE_PATTERNS:
                    match = pattern.search(line)
                    if match:
                        yield f"{relative}:{number}: {match.group(0)}"
            for number, line in enumerate(strip_comments_and_strings(text).splitlines(), 1):
                for pattern in CODE_PATTERNS:
                    for match in pattern.finditer(line):
                        yield f"{relative}:{number}: {match.group(0).rstrip('( ')}"


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args(argv)
    found = list(findings(args.root.resolve()))
    for line in found:
        print(f"SDL symbol guard: {line}")
    if found:
        print(f"SDL symbol guard: {len(found)} direct SDL uses (the exception list is empty)")
        return 1
    print("SDL symbol guard: no direct SDL use in the product, its probes or its tests")
    return 0


if __name__ == "__main__":
    sys.exit(main())
