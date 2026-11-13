"""Spec J-17, kernel half (registry-schedule declaration judgement 8).

Runs tests/common/zone_scan/zone_scan.py on the list the kernel exported for the synthetic zone C fixture
(tests/common/kernel/zone_c_fixture/SyntheticSimulation.cpp; written by the gyo_kernel_tests case "J-17: the
G-5 export ...") and checks that the scanner reads it (exit status 1: findings, not 2: contract error) and finds
exactly the planted findings: ZS1 1, ZS2 1, ZS6 1, nothing else; and that the list header is the kernel's
(generated_by gyo-kernel-report, one file).

Usage: python3 zone_c_export_check.py --root REPOSITORY --list EXPORTED.json
Exit status 0 when every check holds; 1 otherwise (each failed check is printed).
"""
import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

EXPECTED_PER_RULE = {"ZS1": 1, "ZS2": 1, "ZS6": 1}
EXPECTED_FILES = ["tests/common/kernel/zone_c_fixture/SyntheticSimulation.cpp"]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--root", required=True)
    parser.add_argument("--list", required=True)
    args = parser.parse_args(argv)
    root = Path(args.root)
    problems = []
    listed = json.loads(Path(args.list).read_text(encoding="utf-8"))
    if listed.get("generated_by") != "gyo-kernel-report":
        problems.append(f"generated_by is {listed.get('generated_by')!r}, not 'gyo-kernel-report'")
    if listed.get("files") != EXPECTED_FILES:
        problems.append(f"files are {listed.get('files')!r}, expected {EXPECTED_FILES!r}")
    with tempfile.TemporaryDirectory() as scratch:
        report_path = Path(scratch) / "report.json"
        done = subprocess.run(
            [sys.executable, "-B", str(root / "tests/common/zone_scan/zone_scan.py"), "--root", str(root),
             "--list", args.list, "--report", str(report_path)],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=120)
        print(done.stdout, end="")
        if done.returncode != 1:
            problems.append(f"zone_scan.py exited {done.returncode}; expected 1 (findings), not 0 (clean) or 2 "
                            f"(the list does not meet the contract)")
        if report_path.is_file():
            report = json.loads(report_path.read_text(encoding="utf-8"))
            per_rule = {rule: count for rule, count in report.get("per_rule", {}).items() if count}
            if per_rule != EXPECTED_PER_RULE:
                problems.append(f"findings per rule {per_rule}, expected {EXPECTED_PER_RULE}")
            if report.get("files_listed") != 1:
                problems.append(f"files_listed {report.get('files_listed')}, expected 1")
        else:
            problems.append("zone_scan.py wrote no report")
    for problem in problems:
        print("kernel.J-17: " + problem)
    if problems:
        return 1
    print("kernel.J-17: the kernel's export is read by zone_scan.py with the planted findings")
    return 0


if __name__ == "__main__":
    sys.exit(main())
