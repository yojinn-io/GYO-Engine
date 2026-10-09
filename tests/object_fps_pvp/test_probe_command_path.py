"""Acceptance probes generate commands only through the product simulation role.

The application constructs one ClientSimulationRole and publishes intents to it;
a probe that stepped ClientSimulation, ClientSimulationLoop or
LocalPlayerPrediction (or its elapsed sampler) itself, by a Waiter, a sleep or
its frame loop, or took the role's snapshot history (DrainSimulation), would
measure a command path the application does not run.
Only the transport probe (worker_main.cpp) sends hand-made command windows.
"""
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROBES = ROOT / "build" / "acceptance" / "object_fps_pvp"
APPLICATION = ROOT / "apps" / "object_fps_pvp" / "src" / "Pvp" / "PvpApplication.cpp"
FORBIDDEN = re.compile(r"\b(LocalPlayerPrediction|PredictionElapsedTime|ClientSimulation|ClientSimulationLoop|DrainSimulation)\b")
SENDS = re.compile(r"\bSendInput\(")
PUBLISHES = re.compile(r"\bPublishIntent\(")
CONSTRUCTS = re.compile(r"make_unique<ClientSimulationRole>")
INCLUDE = re.compile(r"^\s*#\s*include\b")
# The transport probe tests the worker with hand-made windows; it generates no movement.
TRANSPORT_PROBE = "worker_main.cpp"


def violations(name: str, text: str) -> list[str]:
    found = []
    for number, line in enumerate(text.splitlines(), 1):
        if INCLUDE.match(line):
            continue
        if FORBIDDEN.search(line):
            found.append(f"{name}:{number}: {line.strip()}")
        if SENDS.search(line) and name != TRANSPORT_PROBE:
            found.append(f"{name}:{number}: sends a command window itself: {line.strip()}")
    if PUBLISHES.search(text) and not CONSTRUCTS.search(text):
        found.append(f"{name}: publishes intents without constructing the product ClientSimulationRole")
    return found


class ProbeCommandPathTests(unittest.TestCase):
    def test_probes_use_the_product_simulation_role(self):
        sources = sorted([*PROBES.glob("*.cpp"), *PROBES.glob("*.hpp")])
        self.assertTrue(sources, f"No probe sources under {PROBES}")
        found = [v for path in sources for v in violations(path.name, path.read_text(encoding="utf-8"))]
        self.assertEqual(found, [], "probes must generate commands only through ClientSimulationRole:\n" + "\n".join(found))
        # Every headless probe that moves publishes to the same role type as the application.
        publishing = [path.name for path in sources if PUBLISHES.search(path.read_text(encoding="utf-8"))]
        self.assertGreaterEqual(len(publishing), 4, publishing)
        self.assertRegex(APPLICATION.read_text(encoding="utf-8"), CONSTRUCTS)

    def test_scanner_detects_direct_use_and_ignores_includes(self):
        self.assertEqual(violations("x.cpp", '#include "RetroFPS/Pvp/ClientSimulation.hpp"\n'), [])
        self.assertEqual(len(violations("x.cpp", "LocalPlayerPrediction p(*arena);\n")), 1)
        self.assertEqual(len(violations("x.cpp", "std::array<LocalPlayerPrediction,2> p;\n")), 1)
        self.assertEqual(len(violations("x.cpp", "PredictionElapsedTime elapsed;\n")), 1)
        self.assertEqual(len(violations("x.cpp", "ClientSimulation simulation(*arena);\n")), 1)
        self.assertEqual(len(violations("x.cpp", "ClientSimulationLoop loop({*arena});\n")), 1)
        self.assertEqual(len(violations("x.cpp", "auto stolen=c.DrainSimulation();\n")), 1)
        self.assertEqual(len(violations("x.cpp", "c.SendInput(std::move(*window));\n")), 1)
        self.assertEqual(violations(TRANSPORT_PROBE, "connection.SendInput(Input(1,2));\n"), [])
        role = "auto r=std::make_unique<ClientSimulationRole>(c,std::vector<Arena>{*arena});\nr->PublishIntent(i);\n"
        self.assertEqual(violations("x.cpp", role), [])
        self.assertEqual(len(violations("x.cpp", "r->PublishIntent(i);\n")), 1)


if __name__ == "__main__":
    sys.exit(unittest.main())
