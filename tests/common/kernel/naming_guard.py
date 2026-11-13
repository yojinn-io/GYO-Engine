"""Naming guard (docs/architecture/target/naming.zh-Hant.md, Design Invariants 1 and 3).

Declared in docs/architecture/plans/kernel/kernel-scaffold-declaration.zh-Hant.md section 3.2.

  N1  namespace <-> directory, from one table (ROOTS; DI 1: "the mapping is given by a directory <-> namespace
      table and need not be spelled the same on both sides").
        forward  every named namespace a C/C++ file under a mapped directory opens at file scope is the mapped
                 namespace or nested in it. "namespace GYO {" at file scope is reported too: the qualified form
                 is required, so the check never has to follow a split spelling. Anonymous namespaces are
                 allowed (file-local); a namespace alias at file scope is reported.
        reverse  no C/C++ file outside the mapped directory, under REVERSE_ROOTS, opens the mapped namespace
                 (nested spellings such as "namespace GYO { namespace Kernel {" included).
  N3  no type declared under a mapped directory (class, struct, union, enum, concept, "using X =") has a name
      ending in one of ROLE_SUFFIXES (DI 3; naming rule 5). Case-sensitive: "Ghost", "Hostname" and
      "RuntimeStats" pass. The allowlist for old code is empty: no old code lives under a mapped directory.

Roots behave differently on purpose, and the self-test pins both:
  - a mapped directory (engine/kernel) must exist and hold files: missing or empty is a finding, so the guard
    never passes by scanning nothing;
  - a reverse root that does not exist is skipped (plugins/ does not exist yet), not a finding.
spikes/ is not scanned (an independent project, namespace KernelSpike, deleted under migration plan U10).

The table has one row in this batch. Product rows belong to the product owner and are to come from product-side
data (like the zone-c-sources list), proposed by the first product batch that needs them; they are not added
to this common script (declaration LIM-4).

Usage: python3 naming_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from kernel_guard import strip_comments_and_strings, line_of  # noqa: E402

ROOTS = (("engine/kernel", "GYO::Kernel"),)
REVERSE_ROOTS = ("engine", "apps", "tools", "framework", "plugins", "tests")
SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp"}
ROLE_SUFFIXES = ("Manager", "Renderer", "ViewModel", "Host", "Shell", "Runtime", "Controller")

TOKEN = re.compile(r"\{|\}|\bnamespace\b")
NAMESPACE = re.compile(r"namespace\s*((?:::)?\s*\w+(?:\s*::\s*(?:inline\s+)?\w+)*)?\s*(\{|=)")
TYPE_DECLARATION = re.compile(
    r"\b(?:class|struct|union|enum(?:\s+(?:class|struct))?|concept)\s+(?:\[\[[^\]]*\]\]\s*)?(?:alignas\s*\([^)]*\)\s*)?(\w+)"
    r"|\busing\s+(\w+)\s*=")
ROLE_SUFFIX = re.compile(r"(?:" + "|".join(ROLE_SUFFIXES) + r")$")


def namespaces(code):
    """(offset, full name, depth) for every namespace a file opens; full name joins the enclosing namespaces.

    Aliases ("namespace X = Y;") come back with full name "=X" so callers can report them.
    """
    stack, pending, found = [], None, []
    for token in TOKEN.finditer(code):
        text = token.group(0)
        if text == "namespace":
            before = code[max(0, token.start() - 16):token.start()]
            if re.search(r"\busing\s*$", before):
                continue
            match = NAMESPACE.match(code, token.start())
            if not match:
                continue
            name = re.sub(r"\s+|inline", "", match.group(1) or "").lstrip(":")
            enclosing = "::".join(n for n in stack if n)
            if match.group(2) == "=":
                found.append((token.start(), "=" + name, len(stack)))
                continue
            full = "::".join(part for part in (enclosing, name) if part) if name else ""
            found.append((token.start(), full, len(stack)))
            pending = name if name else ""
        elif text == "{":
            # The brace that opens a pending namespace carries its name; any other brace carries None, so a
            # namespace opened inside a class or function body is not mistaken for a nested namespace.
            stack.append(pending if pending is not None else None)
            pending = None
        else:
            if stack:
                stack.pop()
    return found


def within(name, mapped):
    return name == mapped or name.startswith(mapped + "::")


def source_files(base):
    return sorted(p for p in base.rglob("*") if p.is_file() and p.suffix in SUFFIXES)


def check(root):
    root = Path(root)
    problems = []
    for directory, mapped in ROOTS:
        base = root / directory
        files = source_files(base) if base.is_dir() else []
        if not files:
            problems.append(f"{directory}: no source to scan (moved or renamed? update this guard)")
            continue
        for path in files:
            relative = path.relative_to(root).as_posix()
            code = strip_comments_and_strings(path.read_text(encoding="utf-8"))
            for offset, full, depth in namespaces(code):
                if depth != 0:
                    continue
                if full.startswith("="):
                    problems.append(f"{relative}:{line_of(code, offset)}: N1 namespace alias {full[1:]} at file "
                                    f"scope (expected only {mapped})")
                elif full and not within(full, mapped):
                    problems.append(f"{relative}:{line_of(code, offset)}: N1 namespace {full} (expected {mapped})")
            seen = set()
            for match in TYPE_DECLARATION.finditer(code):
                name = match.group(1) or match.group(2)
                key = (match.start(), name)
                if ROLE_SUFFIX.search(name) and key not in seen:
                    seen.add(key)
                    problems.append(f"{relative}:{line_of(code, match.start())}: N3 type {name} ends in a role "
                                    f"suffix ({'|'.join(ROLE_SUFFIXES)})")
        for reverse_root in REVERSE_ROOTS:
            base_reverse = root / reverse_root
            if not base_reverse.is_dir():
                continue  # a reverse root that does not exist yet (plugins/) is skipped, not a finding
            for path in source_files(base_reverse):
                if path.is_relative_to(base):
                    continue
                relative = path.relative_to(root).as_posix()
                code = strip_comments_and_strings(path.read_text(encoding="utf-8", errors="replace"))
                for offset, full, _depth in namespaces(code):
                    if not full.startswith("=") and within(full, mapped):
                        problems.append(f"{relative}:{line_of(code, offset)}: N1 namespace {full} opened outside "
                                        f"{directory}")
    return problems


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[3]))
    args = parser.parse_args(argv)
    problems = check(args.root)
    for line in problems:
        print(line)
    if problems:
        print(f"naming guard: {len(problems)} finding(s) (target/naming Design Invariants 1 and 3)")
        return 1
    print("naming guard: clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
