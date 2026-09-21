"""Validate the installed editor CLI with its own independent documents."""
import argparse
from pathlib import Path
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from workspace import TemporaryDirectory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    args = parser.parse_args()
    executable = str(args.executable.resolve(strict=True))
    valid = Path(__file__).resolve().parent / "fixtures/minimal.ui.json"
    with TemporaryDirectory(prefix="gyo-editor-acceptance-") as work:
        invalid = Path(work) / "invalid.ui.json"
        invalid.write_text('{"schema":"wrong","version":999}', encoding="utf-8")
        for command, expected in (([executable, "--help"], 0),
                                  ([executable, "--validate", str(valid)], 0),
                                  ([executable, "--validate", str(invalid)], 4)):
            result = subprocess.run(command, cwd=work, timeout=30)
            if result.returncode != expected:
                raise RuntimeError(f"Editor returned {result.returncode}, expected {expected}: {command}")


if __name__ == "__main__":
    main()
