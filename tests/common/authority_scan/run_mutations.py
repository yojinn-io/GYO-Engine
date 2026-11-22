"""Mutation runner of the zone-C scanner (docs/architecture/plans/kernel/j17-scanner.zh-Hant.md,
judgement 2).

Each mutant in mutations.json replaces one exact text in one file of this directory, runs the
self-test (python -m unittest discover), and counts as killed only when the self-test fails AND its
output matches the mutant's expected test name. Any other outcome is reported on its own: survived
(the self-test passed), stale (the text is not found exactly once), wrong_failure (failed without the
expected name) or timeout. The unmutated tree must pass first, and every file is restored afterwards.
Python only, subprocess timeouts, no shell tools: the same on Windows, Linux and macOS. The self-test
runs with -B and a cleared __pycache__, so no mutant can run another mutant's cached bytecode.

Usage: python3 run_mutations.py [--only ID ...] [--report FILE]
Exit status: 0 when every selected mutant is killed, 1 otherwise, 2 on a usage or baseline error.
"""
import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

DIRECTORY = Path(__file__).resolve().parent
CATALOG = DIRECTORY / "mutations.json"
TIMEOUT_SECONDS = 300


def run_selftest():
    """('passed' | 'failed' | 'timeout' | 'error', output).

    -B and a cleared __pycache__: a mutant that keeps the file's size and lands in the same second
    would otherwise run the previous mutant's cached bytecode (seen 2026-11-12: two masking mutants
    reported the line-comment mutant's failure)."""
    shutil.rmtree(DIRECTORY / "__pycache__", ignore_errors=True)
    command = [sys.executable, "-I", "-B", "-m", "unittest", "discover", "-s", str(DIRECTORY), "-p", "test_*.py"]
    try:
        completed = subprocess.run(command, cwd=str(DIRECTORY), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   text=True, errors="replace", timeout=TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired as expired:
        return "timeout", expired.output if isinstance(expired.output, str) else ""
    except OSError as error:
        return "error", str(error)
    return ("passed" if completed.returncode == 0 else "failed"), completed.stdout


def run_mutant(mutant):
    path = DIRECTORY / mutant["file"]
    original = path.read_bytes()
    text = original.decode("utf-8")
    if text.count(mutant["find"]) != 1:
        return {"id": mutant["id"], "status": "stale", "detail": f"text found {text.count(mutant['find'])} times"}
    try:
        path.write_bytes(text.replace(mutant["find"], mutant["replace"]).encode("utf-8"))
        status, output = run_selftest()
    finally:
        path.write_bytes(original)
    if status == "passed":
        result = "survived"
    elif status == "failed" and re.search(mutant["expect"], output):
        result = "killed"
    elif status == "failed":
        result = "wrong_failure"
    else:
        result = status
    return {"id": mutant["id"], "status": result, "detail": "" if result == "killed" else output[-2000:]}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--only", nargs="*", default=[])
    parser.add_argument("--report")
    args = parser.parse_args(argv)
    catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    mutants = catalog["mutants"]
    if args.only:
        unknown = set(args.only) - {m["id"] for m in mutants}
        if unknown:
            print(f"unknown mutant ids: {sorted(unknown)}", file=sys.stderr)
            return 2
        mutants = [m for m in mutants if m["id"] in args.only]
    status, output = run_selftest()
    print(f"baseline selftest: {status}", flush=True)
    if status != "passed":
        print(output[-2000:], file=sys.stderr)
        return 2
    results = []
    for mutant in mutants:
        result = run_mutant(mutant)
        results.append(result)
        print(f"{result['id']}: {result['status']}", flush=True)
    killed = sum(r["status"] == "killed" for r in results)
    print(f"{killed}/{len(results)} killed")
    if args.report:
        Path(args.report).parent.mkdir(parents=True, exist_ok=True)
        Path(args.report).write_text(json.dumps({"results": results}, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return 0 if killed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
