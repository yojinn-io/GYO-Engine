"""Naming guard (docs/architecture/target/naming.zh-Hant.md, Design Invariant 1; the GYO2 rule is N17 of
docs/architecture/target/v2/rebuild-naming.zh-Hant.md, whose section 3 names this guard).

Rewritten by v8-arch-gyo2-namespace-root: the namespace table is no longer a list of rows. It is the closed table
of the roots under GYO (GYO_ROOTS), and every Engine unit's namespace is derived from its directory, so a unit
added later is checked without changing this script.

  Units. engine/ holds the layers in ENGINE_LAYERS: a layer is either a set of units (each subdirectory is one)
  or one unit itself. A unit's namespace is GYO::Engine::<Name>, Name derived from the directory name
  (camel_case of tests/common/support/source_text.py with ACRONYMS: each "_" part capitalized, a part in ACRONYMS
  written as listed); UNIT_OVERRIDES is the exception table of DI 1 (a unit whose namespace is not the derived
  one; engine/ecs/kernel is GYO::Kernel). A directory under engine/ that is not a listed layer is a finding (fail
  closed). Each subdirectory of plugins/ is one Plugin unit, GYO::Plugins::<Name>, Name derived the same way
  (v8-arch-gyo2-sdl-a, the first Plugin with code: "sdl" is an acronym, so plugins/sdl_audio is
  GYO::Plugins::SDLAudio).

  N1  namespace <-> directory.
        forward  every named namespace a C/C++ file in a unit opens outside any named namespace (at file scope,
                 or inside an anonymous namespace or a linkage block such as extern "C++" {}) is the unit's
                 namespace or nested in it. "namespace GYO {" is reported too: the qualified form is required,
                 so the check never has to follow a split spelling. Anonymous namespaces are allowed
                 (file-local); a namespace alias there is reported; a namespace declaration this guard cannot
                 read is reported (fail closed).
        roots    in every C/C++ file under SCAN_ROOTS that is in no unit, every named namespace it opens (nested
                 spellings such as "namespace GYO { namespace Kernel {" included):
                   - is not the old root Engine (namespace Engine or Engine::...). A transitional rule
                     (v8-arch-gyo2-namespace-root): it keeps the carried code from coming back under the old
                     root, and goes when no code is carried from master any more;
                   - if it is under GYO, is under a root of GYO_ROOTS, and only in that root's place: the Kernel
                     and Engine roots belong to their units (opened in no other file), GYO::Plugins to plugins/,
                     GYO::Test to tests/. GYO alone, or a GYO::<X> not in the table, is reported: a new root is
                     added to the naming rule and to GYO_ROOTS first.
                 A product opens its own root (<product>::), which this guard does not read. A namespace
                 declaration this guard cannot read is reported here too (fail closed).
      The CMake target names (N14) are a convention checked in review, not here: the build owns them.
N3 (the role suffixes of naming rule 5, DI 3) was removed by v8-arch-gyo2-namespace-root: rule 5 was not carried
to gyo2 (user 2026-11-14), and the user removed the check on 2026-11-22; names are seen in each batch's design review.

SCAN_ROOTS are the top-level directories of the repository's own code, read from build/cmake/GyoSourceRoots.txt
(the build adds its directories under them): engine/, plugins/, games/ and tools/ (one subdirectory per registered
product), examples/, build/acceptance/ and tests/; framework/ was dropped with the framework layer
(v8-arch-gyo2-naming-cleanup). A scan root that does not exist is skipped, not a finding; engine/ must exist and
hold units. spikes/ is not scanned (an
independent project). Limits (text level, the rest is review): a top-level
directory outside SCAN_ROOTS is not read; two units deriving the same name are left to CMake's duplicate-target
error; a namespace opened through a macro or split across #if branches is not followed; .cppm and *.h.in files
are not read; a namespace alias is not read (only opening a namespace is ownership). Product rows belong to the product owner (declaration
LIM-4); this guard only keeps products out of GYO.

Usage: python3 naming_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "support"))
from source_roots import source_roots  # noqa: E402
from source_text import SUFFIXES as C_SUFFIXES, camel_case, line_of, strip_comments_and_strings  # noqa: E402

ENGINE_LAYERS = {"core": "units", "ecs": "units", "modules": "units", "orchestration": "unit"}
UNIT_OVERRIDES = {"engine/ecs/kernel": "GYO::Kernel"}
ACRONYMS = {"io": "IO", "os": "OS", "sdl": "SDL", "spsc": "SPSC", "gpu": "GPU"}
GYO_ROOTS = {"Kernel": "units", "Engine": "units", "Plugins": "plugins", "Test": "tests"}
SCAN_ROOTS = source_roots()  # build/cmake/GyoSourceRoots.txt
OLD_ROOT = "Engine"
SUFFIXES = C_SUFFIXES | {".m", ".mm"}  # the C/C++ files the guards read (source_text), plus Objective-C++

TOKEN = re.compile(r"\{|\}|\bnamespace\b")
NAMESPACE = re.compile(r"namespace\s*(?:\[\[[^\]]*\]\]\s*)?((?:::)?\s*\w+(?:\s*::\s*(?:inline\s+)?\w+)*)?\s*(?:\[\[[^\]]*\]\]\s*)?(\{|=)")


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


def units(root, problems):
    """(directory, namespace) for every Engine unit; findings for a layer the table does not list or that is gone."""
    found = []
    engine = root / "engine"
    layers = sorted(p for p in engine.iterdir() if p.is_dir()) if engine.is_dir() else []
    if not layers:
        problems.append("engine: no layer to scan (moved or renamed? update this guard)")
    # A listed layer or override that is gone is a finding, so the guard never passes by scanning nothing; a unit
    # in a layer of units may go (removing a Module needs no change here).
    for name, kind in ENGINE_LAYERS.items():
        layer = engine / name
        if not layer.is_dir() or (kind == "units" and not any(p.is_dir() for p in layer.iterdir())):
            problems.append(f"engine/{name}: no source to scan (moved or renamed? update this guard)")
    for directory in UNIT_OVERRIDES:
        if not (root / directory).is_dir():
            problems.append(f"{directory}: no source to scan (moved or renamed? update this guard)")
    for layer in layers:
        kind = ENGINE_LAYERS.get(layer.name)
        if kind is None:
            problems.append(f"engine/{layer.name}: not a layer of the naming table (ENGINE_LAYERS); add it with "
                            f"the naming rule (N17)")
        elif kind == "unit":
            found.append(layer)
        else:
            found.extend(sorted(p for p in layer.iterdir() if p.is_dir()))
    result = []
    for base in found:
        relative = base.relative_to(root).as_posix()
        result.append((relative, UNIT_OVERRIDES.get(relative, f"GYO::Engine::{camel_case(base.name, ACRONYMS)}")))
    plugins = root / "plugins"
    if plugins.is_dir():
        for base in sorted(p for p in plugins.iterdir() if p.is_dir()):
            result.append((base.relative_to(root).as_posix(), f"GYO::Plugins::{camel_case(base.name, ACRONYMS)}"))
    return result


def check_forward(root, directory, mapped, problems):
    base = root / directory
    files = source_files(base) if base.is_dir() else []
    if not files:
        problems.append(f"{directory}: no source to scan (moved or renamed? update this guard)")
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


def check_roots(root, unit_directories, problems):
    for scan_root in SCAN_ROOTS:
        base = root / scan_root
        if not base.is_dir():
            continue  # a scan root that does not exist yet (games/) is skipped, not a finding
        for path in source_files(base):
            if any(path.is_relative_to(root / d) for d in unit_directories):
                continue  # read by the forward check
            relative = path.relative_to(root).as_posix()
            code = strip_comments_and_strings(path.read_text(encoding="utf-8", errors="replace"))
            for offset, full, _enclosing in namespaces(code):
                where = f"{relative}:{line_of(code, offset)}"
                if full.startswith("?"):
                    problems.append(f"{where}: N1 unreadable namespace declaration {full[1:].strip()!r}")
                    continue
                if full.startswith("=") or not full:
                    continue
                parts = full.split("::")
                if parts[0] == OLD_ROOT:
                    problems.append(f"{where}: N1 namespace {full} uses the old root {OLD_ROOT} (expected a root "
                                    f"of the naming table)")
                elif parts[0] != "GYO":
                    continue  # a product's own root
                elif len(parts) == 1:
                    problems.append(f"{where}: N1 namespace GYO opened alone (write the qualified root)")
                elif parts[1] not in GYO_ROOTS:
                    problems.append(f"{where}: N1 namespace {full}: GYO::{parts[1]} is not a root of the naming "
                                    f"table (GYO_ROOTS)")
                elif GYO_ROOTS[parts[1]] == "units":
                    problems.append(f"{where}: N1 namespace {full} opened outside its unit")
                elif not path.is_relative_to(root / GYO_ROOTS[parts[1]]):
                    problems.append(f"{where}: N1 namespace {full} opened outside {GYO_ROOTS[parts[1]]}/")


def check(root):
    root = Path(root)
    problems = []
    found = units(root, problems)
    for directory, mapped in found:
        check_forward(root, directory, mapped, problems)
    check_roots(root, [directory for directory, _mapped in found], problems)
    return problems


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[3]))
    args = parser.parse_args(argv)
    problems = check(args.root)
    for line in problems:
        print(line)
    if problems:
        print(f"naming guard: {len(problems)} finding(s) (target/naming Design Invariant 1; rebuild-naming N17)")
        return 1
    print("naming guard: clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
