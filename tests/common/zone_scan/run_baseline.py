"""Baseline run of zone_scan.py over every list in baseline/ (ctest zone_scan.baseline).

A report, not a gate: each list is scanned with the --no-fail semantics, its JSON report is written
to --out/<list stem>.json and its findings are printed, so the run passes with findings. It fails
(exit status 2) only on a contract error, such as a listed file that no longer exists or an unknown
list version: a product that moves or deletes a listed file must update its list (AGENTS.md section 7,
an explicit dependency). With no list in baseline/ the run passes and says so.

Usage: python3 run_baseline.py --root REPOSITORY --out DIRECTORY [--lists FILE ...]
"""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import zone_scan  # noqa: E402

BASELINE_DIRECTORY = Path(__file__).resolve().with_name("baseline")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[3]))
    parser.add_argument("--out", required=True, help="directory for the JSON reports")
    parser.add_argument("--lists", nargs="*", help="list files (default: every baseline/*.json)")
    parser.add_argument("--rules", default=str(zone_scan.DEFAULT_RULES))
    args = parser.parse_args(argv)
    lists = [Path(p) for p in args.lists] if args.lists else sorted(BASELINE_DIRECTORY.glob("*.json"))
    if not lists:
        print("zone_scan baseline: no list in baseline/, nothing to scan")
        return 0
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    try:
        rules = zone_scan.load_rules(args.rules)
        for list_path in lists:
            report = zone_scan.scan(args.root, list_path, rules)
            target = out / (list_path.stem + ".json")
            target.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
            print(f"== {list_path.name} -> {target.as_posix()}")
            for line in zone_scan.format_lines(report):
                print(line)
            print("per rule: " + ", ".join(f"{rule} {count}" for rule, count in report["per_rule"].items()), flush=True)
    except zone_scan.ContractError as error:
        print(str(error), file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
