"""Naming guard (docs/architecture/target/naming.zh-Hant.md, Design Invariants 1 and 3).

Declared in docs/architecture/plans/kernel/kernel-scaffold-declaration.zh-Hant.md section 3.2.

  N1  namespace <-> directory, from one table (ROOTS; DI 1: "the mapping is given by a directory <-> namespace
      table and need not be spelled the same on both sides").
        forward  every named namespace a C/C++ file under a mapped directory opens outside any named namespace
                 (at file scope, or inside an anonymous namespace or a linkage block such as extern "C++" {})
                 is the mapped namespace or nested in it. "namespace GYO {" is reported too: the qualified form
                 is required, so the check never has to follow a split spelling. Anonymous namespaces are
                 allowed (file-local); a namespace alias there is reported; a namespace declaration this guard
                 cannot read is reported (fail closed).
        reverse  no C/C++ file outside the mapped directory, under REVERSE_ROOTS, opens the mapped namespace
                 (nested spellings such as "namespace GYO { namespace Kernel {" included).
  N3  no type declared under a mapped directory (class, struct, union, enum, concept, "using X =", typedef; a
      qualified name such as Detail::ImplHost is judged by its last part) has a name ending in one of
      ROLE_SUFFIXES (DI 3; naming rule 5). Case-sensitive: "Ghost", "Hostname" and "RuntimeStats" pass. The
      allowlist for old code is empty: no old code lives under a mapped directory. Limits: a type name after an
      export macro ("class GYO_API X") is read as the macro; a template parameter named like a role (THost) is
      reported (fail closed).

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
from kernel_guard import SUFFIXES as KERNEL_SUFFIXES, line_of, strip_comments_and_strings  # noqa: E402

ROOTS = (("engine/kernel", "GYO::Kernel"),)
REVERSE_ROOTS = ("engine", "apps", "tools", "framework", "plugins", "tests")
SUFFIXES = KERNEL_SUFFIXES | {".m", ".mm"}  # the same C/C++ files kernel_guard reads, plus Objective-C++
ROLE_SUFFIXES = ("Manager", "Renderer", "ViewModel", "Host", "Shell", "Runtime", "Controller")

TOKEN = re.compile(r"\{|\}|\bnamespace\b")
NAMESPACE = re.compile(r"namespace\s*(?:\[\[[^\]]*\]\]\s*)?((?:::)?\s*\w+(?:\s*::\s*(?:inline\s+)?\w+)*)?\s*(?:\[\[[^\]]*\]\]\s*)?(\{|=)")
TYPE_DECLARATION = re.compile(
    r"\b(?:class|struct|union|enum(?:\s+(?:class|struct))?|concept)\s+(?:\[\[[^\]]*\]\]\s*)?(?:alignas\s*\([^)]*\)\s*)?"
    r"(\w+(?:\s*::\s*\w+)*)"
    r"|\busing\s+(\w+)\s*="
    r"|\btypedef\b[^;{}]*?(\w+)\s*(?:\[[^\]]*\]\s*)*;")
ROLE_SUFFIX = re.compile(r"(?:" + "|".join(ROLE_SUFFIXES) + r")$")


def namespaces(code):
    """(offset, full name, enclosing) for every namespace a file opens.

    full name joins the enclosing named namespaces; enclosing is "" outside any named namespace. Aliases
    ("namespace X = Y;") come back with full name "=X", and a declaration that cannot be read with "?" followed by
    its text, so callers can report them.
    """
    stack, pending, found = [], None, []
    for token in TOKEN.finditer(code):
        text = token.group(0)
        if text == "namespace":
            enclosing = "::".join(n for n in stack if n)
            if re.search(r"\busing\s+$", code[max(0, token.start() - 16):token.start()]):
                continue  # "using namespace X;" opens nothing
            match = NAMESPACE.match(code, token.start())
            if not match:
                text = code[token.start():token.start() + 60].split("\n")[0]
                found.append((token.start(), "?" + text, enclosing))
                continue
            name = re.sub(r"\s+|\binline\b", "", match.group(1) or "").lstrip(":")
            if match.group(2) == "=":
                found.append((token.start(), "=" + name, enclosing))
                continue
            full = "::".join(part for part in (enclosing, name) if part) if name else ""
            found.append((token.start(), full, enclosing))
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
            for offset, full, enclosing in namespaces(code):
                if enclosing:
                    continue
                if full.startswith("?"):
                    problems.append(f"{relative}:{line_of(code, offset)}: N1 unreadable namespace declaration "
                                    f"{full[1:].strip()!r}")
                elif full.startswith("="):
                    problems.append(f"{relative}:{line_of(code, offset)}: N1 namespace alias {full[1:]} at file "
                                    f"scope (expected only {mapped})")
                elif full and not within(full, mapped):
                    problems.append(f"{relative}:{line_of(code, offset)}: N1 namespace {full} (expected {mapped})")
            for match in TYPE_DECLARATION.finditer(code):
                name = re.sub(r"\s+", "", match.group(1) or match.group(2) or match.group(3))
                if ROLE_SUFFIX.search(name.split("::")[-1]):
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
                for offset, full, _enclosing in namespaces(code):
                    if not full.startswith(("=", "?")) and within(full, mapped):
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
