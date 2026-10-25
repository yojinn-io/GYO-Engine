"""Mutation runner for the time-threads-trace plan (Engine-owned; no product dependency).

Each mutant in mutations.json replaces one exact text in one source file, rebuilds
its target, runs its test executable (optionally filtered to doctest test cases)
and counts as killed only when the run fails AND its output matches the mutant's
expected message. A mutant with "expect_build_failure": true is killed instead
when its build fails with the expected message (a configure-time guard). A
mutant with "command" (a list; "{python}" stands for this interpreter and
"{exe:NAME}" for the built executable NAME) runs that command from the
repository root instead of the target's executable, for checks that are
scripts (a source guard, or a script that drives a built probe); with a
"target" as well, that target is built first. A mutant with "platforms" (prefixes
of sys.platform, such as "win32", "darwin", "linux") runs only there: it
mutates code that is compiled on those platforms only, and elsewhere it is
reported as skipped and left out of the count. The
unmutated tree must pass every used check first; every
file is restored afterwards. Python only, with subprocess timeouts, so it runs
the same on Windows, Linux and macOS.

Usage (from the repository root):
  python3 docs/architecture/plans/time-threads-trace/scripts/run_mutations.py \
      --build-dir build/target/_build/test [--cmake PATH] [--only ID ...] [--catalog FILE]
--catalog runs another Engine plan's catalog in the same format (default: the
mutations.json next to this script).
Exit status: 0 when every selected mutant is killed, 1 otherwise, 2 on a baseline error.
"""
import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[5]
CATALOG = Path(__file__).resolve().with_name("mutations.json")


def run(command, timeout):
    try:
        done = subprocess.run(command, cwd=REPOSITORY, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True, errors="replace", timeout=timeout)
    except subprocess.TimeoutExpired:
        return "timeout", ""
    except OSError as error:
        return "error", str(error)
    return ("passed" if done.returncode == 0 else "failed"), done.stdout


def executable(build_dir, name):
    for suffix in ("", ".exe"):
        for path in build_dir.rglob(name + suffix):
            if path.is_file():
                return path
    raise SystemExit(f"test executable {name} not found under {build_dir}")


def check_key(mutant):
    if "command" in mutant:
        return "command|" + mutant.get("target", "") + "|" + "\0".join(mutant["command"])
    return mutant["target"] + "|" + mutant.get("cases", "")


def command_part(part, build_dir):
    if part == "{python}":
        return sys.executable
    if part.startswith("{exe:") and part.endswith("}"):
        return str(executable(build_dir, part[len("{exe:"):-1]))
    return part


def check(mutant, build_dir, cmake):
    if mutant.get("target"):
        status, output = run([cmake, "--build", str(build_dir), "--target", mutant["target"]], 1800)
        if status != "passed":
            return "build_failed", output
    if "command" in mutant:
        return run([command_part(part, build_dir) for part in mutant["command"]], mutant.get("timeout", 300))
    command = [str(executable(build_dir, mutant["target"]))]
    if mutant.get("cases"):
        command.append("-tc=" + mutant["cases"])
    return run(command, mutant.get("timeout", 300))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--only", nargs="*")
    parser.add_argument("--catalog", default=str(CATALOG))
    args = parser.parse_args()
    build_dir = (REPOSITORY / args.build_dir).resolve()
    mutants = json.loads((REPOSITORY / args.catalog).read_text(encoding="utf-8"))["mutants"]
    if args.only:
        mutants = [m for m in mutants if m["id"] in args.only]
    skipped = [m for m in mutants if m.get("platforms") and not sys.platform.startswith(tuple(m["platforms"]))]
    for mutant in skipped:
        print(f"{mutant['id']}: skipped (runs on {', '.join(mutant['platforms'])}; this is {sys.platform})")
    mutants = [m for m in mutants if m not in skipped]
    for mutant in {check_key(m): m for m in mutants}.values():
        status, output = check(mutant, build_dir, args.cmake)
        if status != "passed":
            print(f"baseline {check_key(mutant)}: {status}\n{output[-2000:]}")
            return 2
    killed = 0
    for mutant in mutants:
        path = REPOSITORY / mutant["file"]
        # Bytes, so a restore is byte-identical whatever the line endings are.
        original = path.read_bytes()
        find, replace = mutant["find"].encode("utf-8"), mutant["replace"].encode("utf-8")
        if original.count(find) != 1:
            print(f"{mutant['id']}: stale")
            continue
        try:
            path.write_bytes(original.replace(find, replace))
            status, output = check(mutant, build_dir, args.cmake)
        finally:
            path.write_bytes(original)
        detected = "build_failed" if mutant.get("expect_build_failure") else "failed"
        if status == detected and re.search(mutant["expect"], output):
            outcome = "killed"
            killed += 1
        elif status == detected:
            outcome = "wrong_failure"
        else:
            outcome = status if status != "passed" else "survived"
        print(f"{mutant['id']}: {outcome}")
    # Rebuild the restored sources so the build tree matches the checkout again.
    for target in sorted({m["target"] for m in mutants if "target" in m}):
        run([args.cmake, "--build", str(build_dir), "--target", target], 1800)
    print(f"{killed}/{len(mutants)} killed" + (f", {len(skipped)} skipped on this platform" if skipped else ""))
    return 0 if killed == len(mutants) else 1


if __name__ == "__main__":
    sys.exit(main())
