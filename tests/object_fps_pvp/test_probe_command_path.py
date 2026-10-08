"""Acceptance probes generate commands only through the product ClientSimulation.

A probe that drove LocalPlayerPrediction (or its elapsed sampler) itself would
measure a command path the application does not run.
"""
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROBES = ROOT / "build" / "acceptance" / "object_fps_pvp"
FORBIDDEN = re.compile(r"\b(LocalPlayerPrediction|PredictionElapsedTime)\b")
INCLUDE = re.compile(r"^\s*#\s*include\b")


def violations(name: str, text: str) -> list[str]:
    found = []
    for number, line in enumerate(text.splitlines(), 1):
        if INCLUDE.match(line):
            continue
        if FORBIDDEN.search(line):
            found.append(f"{name}:{number}: {line.strip()}")
    return found


class ProbeCommandPathTests(unittest.TestCase):
    def test_probes_do_not_drive_prediction_directly(self):
        sources = sorted([*PROBES.glob("*.cpp"), *PROBES.glob("*.hpp")])
        self.assertTrue(sources, f"No probe sources under {PROBES}")
        found = [v for path in sources for v in violations(path.name, path.read_text(encoding="utf-8"))]
        self.assertEqual(found, [], "probes must use ClientSimulation:\n" + "\n".join(found))

    def test_scanner_detects_direct_use_and_ignores_includes(self):
        self.assertEqual(violations("x.cpp", '#include "RetroFPS/Pvp/LocalPlayerPrediction.hpp"\n'), [])
        self.assertEqual(len(violations("x.cpp", "LocalPlayerPrediction p(*arena);\n")), 1)
        self.assertEqual(len(violations("x.cpp", "std::array<LocalPlayerPrediction,2> p;\n")), 1)
        self.assertEqual(len(violations("x.cpp", "PredictionElapsedTime elapsed;\n")), 1)
        self.assertEqual(violations("x.cpp", "ClientSimulation simulation(*arena);\n"), [])


if __name__ == "__main__":
    sys.exit(unittest.main())
