"""Orchestration guard (docs/architecture/target/v2/orchestration.zh-Hant.md, Design Invariants O-3, O-4, O-6).

The scans the orchestration-skeleton declaration (section 7.4) deferred until scenario segment 1 passed
(v8-arch-gyo2-guards-after-mechanism-stable); stood up by v8-arch-gyo2-time-events step 1.

  O4  an include in engine/orchestration/ outside the allowlist: in quotes only engine/kernel/, engine/base/ and
      engine/orchestration/; in angle brackets only a C++ standard library header (kernel_guard's
      STANDARD_HEADERS). The layer carries no third-party ability and no Module's internals (O-4).
  O3  engine/orchestration/ reads no Plan step and calls no System itself (O-3: the runner calls only the
      Product's entry points and orders no System): Plans(, PlanOf(, ExportPlans(, .steps, StepKind, and the
      kernel's System invokers InvokerOf, CallFunction, CallLambda. Read on code with comments and literals blanked.
  O6  an engine-internal link written by hand in an engine CMakeLists.txt: a GYO:: alias among the arguments of
      target_link_libraries( in engine/<module>/CMakeLists.txt ("#" comments stripped; ALIAS lines are not calls).
      Dependencies live only in engine/config/modules.csv and plugins.csv and the links are generated from them
      (O-6, user v8-arch-gyo2-module-deps-in-registers).
  O0  a directory this guard scans is missing or holds no file (fail closed: the guard never passes by scanning
      nothing).

Usage: python3 orchestration_guard.py [--root REPOSITORY]
Exit status 0 when clean; 1 with one line per finding otherwise.
"""
import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "kernel"))
from kernel_guard import (  # noqa: E402
    DIRECTIVE, STANDARD_HEADERS, SUFFIXES, TARGET, line_of, strip_comments_and_strings)

SCANNED = "engine/orchestration"
ALLOWED_QUOTED = ("engine/kernel/", "engine/base/", "engine/orchestration/")
O3 = re.compile(r"\bPlans\s*\(|\bPlanOf\s*\(|\bExportPlans\s*\(|\.\s*steps\b|\bStepKind\b"
                r"|\bInvokerOf\b|\bCallFunction\b|\bCallLambda\b")
LINK_CALL = re.compile(r"target_link_libraries\s*\((.*?)\)", re.S)
ALIAS = re.compile(r"GYO::\w+")


def orchestration_findings(relative, raw):
    lines = raw.split("\n")
    for number, line in enumerate(lines, start=1):
        if not DIRECTIVE.match(line):
            continue
        target = TARGET.match(line[DIRECTIVE.match(line).end():])
        if not target:
            yield number, "O4", line.strip()
            continue
        bracket, name = target.groups()
        if bracket == "<" and name not in STANDARD_HEADERS:
            yield number, "O4", f"<{name}>"
        if bracket == '"' and not name.startswith(ALLOWED_QUOTED):
            yield number, "O4", f'"{name}"'
    code = strip_comments_and_strings(raw)
    for match in O3.finditer(code):
        yield line_of(code, match.start()), "O3", match.group(0)


def cmake_findings(raw):
    body = "\n".join(line.split("#", 1)[0] for line in raw.split("\n"))
    for call in LINK_CALL.finditer(body):
        for alias in ALIAS.finditer(call.group(1)):
            yield line_of(body, call.start(1) + alias.start()), "O6", alias.group(0)


def check(root):
    root = Path(root)
    problems = []
    scanned = root / SCANNED
    files = sorted(p for p in scanned.rglob("*") if p.is_file() and p.suffix in SUFFIXES) if scanned.is_dir() else []
    if not files:
        problems.append(f"{SCANNED}: O0 no source to scan (moved or renamed? update this guard)")
    for path in files:
        relative = path.relative_to(root).as_posix()
        for number, rule, text in orchestration_findings(relative, path.read_text(encoding="utf-8")):
            problems.append(f"{relative}:{number}: {rule} {text}")
    engine = root / "engine"
    # Every unit's CMakeLists.txt: engine/<layer>/<unit>/ and the orchestration layer engine/orchestration/.
    cmake_files = sorted(p for p in engine.rglob("CMakeLists.txt") if p.parent != engine) if engine.is_dir() else []
    if not cmake_files:
        problems.append("engine: O0 no unit CMakeLists.txt under engine/ to scan")
    for path in cmake_files:
        relative = path.relative_to(root).as_posix()
        for number, rule, text in cmake_findings(path.read_text(encoding="utf-8")):
            problems.append(f"{relative}:{number}: {rule} {text}")
    return problems


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[3]))
    arguments = parser.parse_args(argv)
    problems = check(arguments.root)
    for problem in problems:
        print(problem)
    if problems:
        print(f"orchestration guard: {len(problems)} finding(s) (target/v2/orchestration O-3, O-4, O-6)")
        return 1
    print("orchestration guard: clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
