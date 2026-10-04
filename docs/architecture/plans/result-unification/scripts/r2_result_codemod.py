#!/usr/bin/env python3
"""R2 codemod: rewrite static Result::Ok / Result::Err calls to the new syntax.

Usage (from the repository root):
    python3 docs/architecture/plans/result-unification/scripts/r2_result_codemod.py [--dry-run]

Rewrites, for every `<ResultType>::Ok(args)` and `<ResultType>::Err(args)`:

    return X::Ok(v);        ->  return v;
    return X::Ok();         ->  return {};
    return X::Err(e);       ->  return Base::Err(e);
    X::Ok(v)  (expression)  ->  X(v)
    X::Ok()   (expression)  ->  X{}
    X::Err(e) (expression)  ->  X(Base::Err(e))

`Base::Err` is spelled `Engine::Base::Err` outside engine/. Braced arguments
(`Ok({...})`, `Err({...})`) cannot be deduced and are reported for a manual
edit instead of being rewritten. Then every `IoResultVoid` becomes
`IoResult<void>` and the `using IoResultVoid = ...;` aliases are removed.
Local `using ... = ...Result<...>;` aliases in .cpp files that are no longer
referenced in their scope are removed as well.

Prints one line per rewrite kind with its count, and every site left for a
manual edit, so the Err count can be checked for conservation.
"""

import argparse
import collections
import pathlib
import re
import sys

SCOPE = [
    "engine",
    "tools/ui_editor",
    "tests/common",
    "tests/ui_editor",
    "apps/object_fps_pvp",
    "tests/object_fps_pvp",
    "build/acceptance/object_fps_pvp",
]
EXCLUDED = ("engine/render/shaders/pipeline/", "engine/base/include/engine/base/Result.hpp")
SUFFIXES = {".hpp", ".cpp", ".h", ".inl"}
CALL = re.compile(r"::(Ok|Err)\(")
IDENT = re.compile(r"[A-Za-z0-9_:]")


def skip_literal(text, i):
    """Return the index just past the string or char literal starting at i."""
    quote = text[i]
    i += 1
    while i < len(text):
        if text[i] == "\\":
            i += 2
            continue
        if text[i] == quote:
            return i + 1
        i += 1
    return i


def match_paren(text, open_index):
    depth = 0
    i = open_index
    while i < len(text):
        c = text[i]
        if c in "\"'":
            if c == "'" and i > 0 and text[i - 1].isalnum():  # digit separator 1'000
                i += 1
                continue
            i = skip_literal(text, i)
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise ValueError("unbalanced parenthesis")


def receiver_start(text, end):
    """Walk back from end (exclusive) over `Name<...>::Name` to its start."""
    i = end - 1
    while i >= 0:
        c = text[i]
        if c == ">":
            depth = 0
            while i >= 0:
                if text[i] == ">":
                    depth += 1
                elif text[i] == "<":
                    depth -= 1
                    if depth == 0:
                        break
                i -= 1
            i -= 1
            continue
        if IDENT.match(c):
            i -= 1
            continue
        break
    return i + 1


def rewrite(text, err_name, stats, manual, path):
    out = text
    for match in reversed(list(CALL.finditer(text))):
        kind = match.group(1)
        start = receiver_start(text, match.start())
        receiver = text[start:match.start()]
        if not receiver or receiver.endswith("Base"):  # already Base::Err
            continue
        open_index = match.end() - 1
        close_index = match_paren(text, open_index)
        args = text[open_index + 1:close_index].strip()
        before = text[:start].rstrip()
        after = text[close_index + 1:].lstrip()
        returning = re.search(r"\breturn$", before) is not None and after.startswith(";")
        line = text.count("\n", 0, start) + 1
        if args.startswith("{"):
            manual.append(f"{path}:{line}: braced {kind}({args[:60]})")
            continue
        if kind == "Ok":
            if returning:
                replacement = args if args else "{}"
                stats["ok_return"] += 1
            else:
                replacement = f"{receiver}({args})" if args else f"{receiver}{{}}"
                stats["ok_expression"] += 1
        else:
            if returning:
                replacement = f"{err_name}({args})"
                stats["err_return"] += 1
            else:
                replacement = f"{receiver}({err_name}({args}))"
                stats["err_expression"] += 1
        out = out[:start] + replacement + out[close_index + 1:]
    return out


def remove_io_result_void(text, stats):
    lines = text.split("\n")
    kept = []
    for line in lines:
        if re.match(r"\s*using\s+IoResultVoid\s*=", line):
            stats["io_result_void_alias_removed"] += 1
            continue
        kept.append(line)
    text = "\n".join(kept)
    text, count = re.subn(r"\bIoResultVoid\b", "IoResult<void>", text)
    stats["io_result_void_renamed"] += count
    return text


def remove_unused_local_aliases(text, stats, path):
    lines = text.split("\n")
    result = []
    i = 0
    while i < len(lines):
        line = lines[i]
        alias = re.match(r"(\s+)using\s+(\w+)\s*=\s*[\w:]*Result\b", line)
        if alias:
            name = alias.group(2)
            indent = len(alias.group(1))
            end = i
            while not lines[end].rstrip().endswith(";"):
                end += 1
            used = False
            for later in lines[end + 1:]:
                stripped = later.strip()
                if stripped.startswith("}") and len(later) - len(later.lstrip()) < indent:
                    break
                if re.search(rf"\b{re.escape(name)}\b", later):
                    used = True
                    break
            if not used:
                stats["local_alias_removed"] += 1
                i = end + 1
                continue
        result.append(line)
        i += 1
    return "\n".join(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    root = pathlib.Path(".").resolve()
    stats = collections.Counter()
    manual = []
    for prefix in SCOPE:
        for path in sorted((root / prefix).rglob("*")):
            relative = path.relative_to(root).as_posix()
            if not path.is_file() or path.suffix not in SUFFIXES or relative.startswith(EXCLUDED):
                continue
            original = path.read_text(encoding="utf-8")
            err_name = "Base::Err" if relative.startswith("engine/") else "Engine::Base::Err"
            text = rewrite(original, err_name, stats, manual, relative)
            text = remove_io_result_void(text, stats)
            if path.suffix == ".cpp":  # header aliases may serve other files
                text = remove_unused_local_aliases(text, stats, relative)
            if text != original:
                stats["files"] += 1
                if not args.dry_run:
                    path.write_text(text, encoding="utf-8")
    for key in sorted(stats):
        print(f"{key}\t{stats[key]}")
    for site in manual:
        print(f"manual\t{site}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
