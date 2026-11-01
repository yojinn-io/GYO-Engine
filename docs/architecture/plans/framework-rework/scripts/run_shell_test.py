"""Runs one shell test executable the way its CTest entry does, for the mutation runner.

docs/architecture/plans/time-threads-trace/scripts/run_mutations.py starts a "command" from the repository root
with the inherited environment. The shell tests need SDL's dummy video driver (and, for the ImGui binding, the
software renderer and a working directory of their own), as tests/framework/CMakeLists.txt sets for CTest. This
script sets those and runs the executable, so a mutant's check matches the CTest one. Python only, so it runs the
same on Windows, Linux and macOS.

Usage: python3 run_shell_test.py [--env NAME=VALUE ...] [--directory NAME] -- EXECUTABLE [ARGUMENT ...]
--directory runs the executable in NAME next to it (created when missing). Exit status: the executable's.
"""
import argparse
import os
import subprocess
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--env", action="append", default=[], metavar="NAME=VALUE")
    parser.add_argument("--directory")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    options = parser.parse_args()
    command = options.command[1:] if options.command[:1] == ["--"] else options.command
    if not command:
        parser.error("no executable")
    environment = dict(os.environ)
    for item in options.env:
        name, separator, value = item.partition("=")
        if not separator:
            parser.error(f"--env needs NAME=VALUE: {item}")
        environment[name] = value
    directory = None
    if options.directory:
        directory = Path(command[0]).resolve().parent / options.directory
        directory.mkdir(parents=True, exist_ok=True)
    return subprocess.run(command, env=environment, cwd=directory).returncode


if __name__ == "__main__":
    sys.exit(main())
