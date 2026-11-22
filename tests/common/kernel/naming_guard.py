"""Naming guard (docs/architecture/target/naming.zh-Hant.md, Design Invariants 1 and 3; the GYO2 rule is N17 of
docs/architecture/target/v2/rebuild-naming.zh-Hant.md, whose section 3 names this guard).

Rewritten by v8-arch-gyo2-namespace-root: the namespace table is no longer a list of rows. It is the closed table
of the roots under GYO (GYO_ROOTS), and every Engine unit's namespace is derived from its directory, so a unit
added later is checked without changing this script.

  Units. engine/ holds the layers in ENGINE_LAYERS: a layer is either a set of units (each subdirectory is one)
  or one unit itself. plugins/<plugin>/ is one unit each. A unit's namespace is GYO::Engine::<Name> (plugins:
  GYO::Plugins::<Name>), Name derived from the directory name (unit_name: each "_" part capitalized, a part in
  ACRONYMS written as listed); UNIT_OVERRIDES replaces the whole namespace (engine/ecs/kernel is GYO::Kernel).
  A directory under engine/ that is not a listed layer, and a C/C++ file directly in engine/, in a layer of units
  or in plugins/ (in no unit), are findings (fail closed: no source goes unchecked).

  N1  namespace <-> directory.
        forward  every named namespace a C/C++ file in a unit opens outside any named namespace (at file scope,
                 or inside an anonymous namespace or a linkage block such as extern "C++" {}) is the unit's
                 namespace or nested in it. "namespace GYO {" is reported too: the qualified form is required,
                 so the check never has to follow a split spelling. Anonymous namespaces are allowed
                 (file-local); a namespace alias there is reported; a namespace declaration this guard cannot
                 read is reported (fail closed).
        roots    in every C/C++ file under SCAN_ROOTS that is in no unit, every named namespace it opens (nested
                 spellings such as "namespace GYO { namespace Kernel {" included):
                   - is not the old root Engine (namespace Engine or Engine::..., v8-arch-gyo2-namespace-root);
                   - if it is under GYO, is under a root of GYO_ROOTS, and only in that root's directory: the
                     Kernel and Engine and Plugins roots belong to their units (opened in no other file), GYO::Test
                     to tests/. GYO alone, or a GYO::<X> not in the table, is reported: a new root is added to
                     the naming rule and to GYO_ROOTS first.
                 A namespace alias named Engine is reported in every file (it would bring the old spelling
                 back without opening the namespace).
                 A product opens its own root (<product>::), which this guard does not read. A namespace
                 declaration this guard cannot read is reported here too (fail closed).
        target   each Engine unit's CMakeLists.txt declares the target gyo_<directory> and the alias
                 GYO::<Name> (N17: directory, namespace and target names agree). A unit with no CMakeLists.txt is
                 reported (fail closed).
  N3  no type declared under N3_DIRECTORIES (class, struct, union, enum, concept, "using X =", typedef; a qualified
      name such as Detail::ImplHost is judged by its last part) has a name ending in one of ROLE_SUFFIXES (DI 3;
      naming rule 5). Case-sensitive: "Ghost", "Hostname" and "RuntimeStats" pass. Kept on the three directories
      it covered before the rewrite (not widened by v8-arch-gyo2-namespace-root). Limits: a type name after an
      export macro ("class GYO_API X") is read as the macro; a template parameter named like a role (THost) is
      reported (fail closed).

A scan root that does not exist is skipped (apps/ does not exist yet), not a finding; engine/ must exist and hold
units. spikes/ is not scanned (an independent project). Limits (text level, the rest is review): a top-level
directory outside SCAN_ROOTS is not read; two units deriving the same name are left to CMake's duplicate-target
error; a namespace opened through a macro or split across #if branches is not followed; .cppm and *.h.in files
are not read; a CMake alias inside a bracket comment or a string counts. Product rows belong to the product owner (declaration
LIM-4); this guard only keeps products out of GYO.

Usage: python3 naming_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from kernel_guard import SUFFIXES as KERNEL_SUFFIXES, line_of, strip_comments_and_strings  # noqa: E402

ENGINE_LAYERS = {"core": "units", "ecs": "units", "modules": "units", "orchestration": "unit"}
UNIT_OVERRIDES = {"engine/ecs/kernel": "GYO::Kernel"}
ACRONYMS = {"io": "IO", "os": "OS"}
GYO_ROOTS = {"Kernel": "units", "Engine": "units", "Plugins": "units", "Test": "tests"}
SCAN_ROOTS = ("engine", "apps", "tools", "framework", "plugins", "tests", "examples")
OLD_ROOT = "Engine"
N3_DIRECTORIES = ("engine/ecs/kernel", "engine/orchestration", "engine/modules/world_time")
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


def unit_name(directory_name):
    return "".join(ACRONYMS.get(part, part[:1].upper() + part[1:]) for part in directory_name.split("_"))


def units(root, problems):
    """(directory, namespace, target alias or None) for every unit; findings for what belongs to no unit."""
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
        name = unit_name(base.name)
        result.append((relative, UNIT_OVERRIDES.get(relative, f"GYO::Engine::{name}"), f"GYO::{name}"))
    plugins = root / "plugins"
    if plugins.is_dir():
        for base in sorted(p for p in plugins.iterdir() if p.is_dir()):
            result.append((base.relative_to(root).as_posix(), f"GYO::Plugins::{unit_name(base.name)}", None))
    # A source directly in engine/, in a layer of units or in plugins/ belongs to no unit.
    containers = [engine, plugins] + [engine / n for n, kind in ENGINE_LAYERS.items() if kind == "units"]
    for container in containers:
        if container.is_dir():
            for path in sorted(p for p in container.iterdir() if p.is_file() and p.suffix in SUFFIXES):
                problems.append(f"{path.relative_to(root).as_posix()}: N1 source in "
                                f"{container.relative_to(root).as_posix()} outside any unit")
    return result


def check_target(root, directory, alias, problems):
    cmake = root / directory / "CMakeLists.txt"
    if not cmake.is_file():
        problems.append(f"{directory}: N1 no CMakeLists.txt to read the target from (expected gyo_"
                        f"{Path(directory).name} and {alias})")
        return
    text = re.sub(r"#[^\n]*", "", cmake.read_text(encoding="utf-8"))
    target = f"gyo_{Path(directory).name}"
    if not re.search(r"\badd_library\s*\(\s*" + re.escape(target) + r"\b(?!\s+ALIAS)", text):
        problems.append(f"{directory}/CMakeLists.txt: N1 target {target} not declared")
    if not re.search(r"\badd_library\s*\(\s*" + re.escape(alias) + r"\s+ALIAS\s+" + re.escape(target) + r"\s*\)",
                     text):
        problems.append(f"{directory}/CMakeLists.txt: N1 alias {alias} for {target} not declared")


def check_forward(root, directory, mapped, problems):
    base = root / directory
    files = source_files(base) if base.is_dir() else []
    if not files:
        problems.append(f"{directory}: no source to scan (moved or renamed? update this guard)")
    for path in files:
        relative = path.relative_to(root).as_posix()
        code = strip_comments_and_strings(path.read_text(encoding="utf-8"))
        for offset, full, enclosing in namespaces(code):
            if full == "=" + OLD_ROOT:
                problems.append(f"{relative}:{line_of(code, offset)}: N1 namespace alias {OLD_ROOT} brings the old "
                                f"root back")
                continue
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
        if directory in N3_DIRECTORIES:
            for match in TYPE_DECLARATION.finditer(code):
                name = re.sub(r"\s+", "", match.group(1) or match.group(2) or match.group(3))
                if ROLE_SUFFIX.search(name.split("::")[-1]):
                    problems.append(f"{relative}:{line_of(code, match.start())}: N3 type {name} ends in a role "
                                    f"suffix ({'|'.join(ROLE_SUFFIXES)})")


def check_roots(root, unit_directories, problems):
    for scan_root in SCAN_ROOTS:
        base = root / scan_root
        if not base.is_dir():
            continue  # a scan root that does not exist yet (apps/) is skipped, not a finding
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
                if full == "=" + OLD_ROOT:
                    problems.append(f"{where}: N1 namespace alias {OLD_ROOT} brings the old root back")
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
    for directory, mapped, alias in found:
        check_forward(root, directory, mapped, problems)
        if alias is not None:
            check_target(root, directory, alias, problems)
    check_roots(root, [directory for directory, _mapped, _alias in found], problems)
    return problems


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[3]))
    args = parser.parse_args(argv)
    problems = check(args.root)
    for line in problems:
        print(line)
    if problems:
        print(f"naming guard: {len(problems)} finding(s) (target/naming Design Invariants 1 and 3; rebuild-naming N17)")
        return 1
    print("naming guard: clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
