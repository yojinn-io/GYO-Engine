"""Retired names of Object_FPS_PVP (v8-arch-gyo2-interact-door, step 0).

The game-flow screens were called Scene; Scene now names the scene data (the furnishing placed over a map), so the
screens are called Screen. This check keeps the old names from coming back: it reads the C++ and CMake files under
games/object_fps_pvp/ and tests/object_fps_pvp/ and reports every retired identifier and every retired file name,
one line each. It reads only the names listed here; the word Scene itself stays free for the scene data.

Usage: python3 retired_names.py [--root REPOSITORY]
"""
import argparse
import re
import sys
from pathlib import Path

SCAN_DIRS = ("games/object_fps_pvp", "tests/object_fps_pvp")
SUFFIXES = (".hpp", ".cpp", ".h", ".cmake")
CMAKE_NAMES = ("CMakeLists.txt",)

# The flow screens' old identifiers (whole words). The enum itself is matched through its declaration and its
# qualified values, since a bare Scene is the new concept's.
RETIRED_IDENTIFIERS = (
    "SceneChange", "SceneRequest", "SceneDirector", "ChangeScene", "SceneChangeWanted", "SceneName",
    "PlaceSceneDocument", "PlaceSceneCamera", "BuildSceneCommon", "InSceneUpdate", "kSceneCamera", "kSceneRoot",
    "ClientScene", "GameScene", "firstScene", "documentScene",
)
RETIRED_ENUM = "Scene"
RETIRED_ENUM_VALUES = ("Menu", "Splash", "SoloRoom", "SoloGame", "Lobby", "Room", "MultiGame")
RETIRED_FILES = (
    "Scenes.hpp", "Scenes.cpp", "SceneBuilds.hpp", "GatewayScene.cpp", "LobbyScene.cpp", "RoomScene.cpp",
    "SoloScene.cpp",
)


def patterns():
    found = [(name, re.compile(r"\b" + name + r"\b")) for name in RETIRED_IDENTIFIERS]
    found.append(("enum class " + RETIRED_ENUM, re.compile(r"\benum\s+class\s+" + RETIRED_ENUM + r"\b")))
    values = "|".join(RETIRED_ENUM_VALUES)
    found.append((RETIRED_ENUM + "::<value>", re.compile(r"\b" + RETIRED_ENUM + r"::(" + values + r")\b")))
    found.extend((name, re.compile(r"(?<![A-Za-z0-9_])" + re.escape(name) + r"\b")) for name in RETIRED_FILES)
    return found


def scanned(root):
    for directory in SCAN_DIRS:
        base = root / directory
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if path.is_file() and (path.suffix in SUFFIXES or path.name in CMAKE_NAMES):
                yield path


def findings(root):
    """Every retired name in the scanned files and every scanned file that has a retired name, as text lines."""
    root = Path(root)
    out = []
    checks = patterns()
    for path in scanned(root):
        relative = path.relative_to(root).as_posix()
        if path.name in RETIRED_FILES:
            out.append(f"{relative}: retired file name {path.name}")
        text = path.read_text(encoding="utf-8")
        for number, line in enumerate(text.splitlines(), start=1):
            for name, pattern in checks:
                if pattern.search(line):
                    out.append(f"{relative}:{number}: retired name {name}")
    return out


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", default=Path(__file__).resolve().parents[3], type=Path)
    arguments = parser.parse_args(argv)
    if not all((arguments.root / directory).is_dir() for directory in SCAN_DIRS):
        print(f"retired_names: {arguments.root} has no {' or '.join(SCAN_DIRS)}", file=sys.stderr)
        return 2
    lines = findings(arguments.root)
    for line in lines:
        print(line)
    if lines:
        print(f"retired_names: {len(lines)} finding(s)", file=sys.stderr)
        return 1
    print("retired_names: no retired name")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
