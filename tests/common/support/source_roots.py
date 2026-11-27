"""The top-level directories of the repository's own code (build/cmake/GyoSourceRoots.txt).

The guards that scan the repository read their roots from here, so the list is kept in one place; text after "#"
and blank lines are ignored, as build/cmake/GyoSourceRoots.cmake does.
"""
from pathlib import Path

LIST = Path(__file__).resolve().parents[3] / "build" / "cmake" / "GyoSourceRoots.txt"


def source_roots(path=LIST):
    roots = (line.split("#", 1)[0].strip() for line in path.read_text(encoding="utf-8").splitlines())
    return tuple(root for root in roots if root)
