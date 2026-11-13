"""Mutation runner of the H-7 host scanner (h7-scanner-declaration.zh-Hant.md, judgement 2).

The zone-C scanner's runner (tests/common/zone_scan/run_mutations.py) pointed at this directory: the
same rules (each mutant replaces one exact text once; killed only when the self-test fails AND its
output matches the mutant's expected test name; survived / stale / wrong_failure / timeout reported on
their own; -I -B and a cleared __pycache__ per run; every file restored). Not copied, so the two
runners cannot drift.

Usage: python3 run_mutations.py [--only ID ...] [--report FILE]
Exit status: 0 when every selected mutant is killed, 1 otherwise, 2 on a usage or baseline error.
"""
import importlib.util
import sys
from pathlib import Path

DIRECTORY = Path(__file__).resolve().parent
SHARED = DIRECTORY.parent / "zone_scan" / "run_mutations.py"


def load_runner():
    if not SHARED.is_file():
        print(f"run_mutations: the shared runner {SHARED} is missing", file=sys.stderr)
        sys.exit(2)
    spec = importlib.util.spec_from_file_location("zone_scan_run_mutations", SHARED)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    for name in ("DIRECTORY", "CATALOG", "main"):
        if not hasattr(module, name):
            print(f"run_mutations: the shared runner lacks {name}", file=sys.stderr)
            sys.exit(2)
    module.DIRECTORY = DIRECTORY
    module.CATALOG = DIRECTORY / "mutations.json"
    return module


if __name__ == "__main__":
    sys.exit(load_runner().main())
