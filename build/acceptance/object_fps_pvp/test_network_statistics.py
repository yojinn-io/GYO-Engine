"""Synthetic logs for network_statistics.py: the lines the C++ and Go tests pin, with the logs' own prefixes and the
existing lines that look alike (the IPC thread's "statistics tick=" line, the Gateway's "player statistics")."""
from pathlib import Path
import tempfile
import unittest

import network_statistics as ns

GATEWAY = """2026/10/09 12:00:10.000001 player statistics player=1 phase=2 udp=127.0.0.1:5000 received=600 last_received_ms=3
2026/10/09 12:00:10.000002 player send statistics player=1 window_ms=10000 results=180 results_interval_ms=55.1/66.8/70.2/80.0 snapshots=300 snapshot_interval_ms=33.3/33.5/34.1/40.0 link_actions=0 link_action_interval_ms=-
2026/10/09 12:00:20.000002 player send statistics player=1 window_ms=10000 results=300 results_interval_ms=33.3/34.0/35.0/36.0 snapshots=300 snapshot_interval_ms=33.3/33.6/34.0/41.0 link_actions=20 link_action_interval_ms=33.4/40.0/50.0/60.0
2026/10/09 12:00:20.000003 gateway transport coalesced_snapshots=0 rate_accepted_packets=1 rate_limited_packets=0 max_session_window_packets=1
"""
MATCH = """Object_FPS_PVP Match ready: 127.0.0.1:27016 arena=a authority=60Hz
[ObjectFPS/PvP Match] statistics tick=602 players=2 coalesced_snapshots=0
[ObjectFPS/PvP Match] network statistics window_s=10.000 cpu_s=0.250 cpu_total_s=12.500 ipc_iterations=8123 ipc_iterations_per_s=812.3 tick_deadline_wakes=600 tick_notified=1 tick_early=0 tick_late_p50_le_us=4000 tick_late_p99_le_us=8000 tick_late_max_us=7100 tick_late_bins=0,1,0,0,500,99,0,0,0,0
[ObjectFPS/PvP Match] network statistics window_s=10.000 cpu_s=0.350 cpu_total_s=12.850 ipc_iterations=8000 ipc_iterations_per_s=800.0 tick_deadline_wakes=600 tick_notified=0 tick_early=2 tick_late_p50_le_us=4000 tick_late_p99_le_us=8000 tick_late_max_us=9000 tick_late_bins=2,0,0,0,500,97,1,0,0,0
"""
WORKER = """[ObjectFPS/PvP] worker statistics player=1 window_s=10.000 wakes=4812 wakes_per_s=481.2 cpu_s=0.051
[ObjectFPS/PvP] connection failed player=1 reason=closed
[ObjectFPS/PvP] worker statistics player=0 window_s=10.000 wakes=1 wakes_per_s=0.1 cpu_s=na
"""


class NetworkStatisticsTests(unittest.TestCase):
    def logs(self, **texts):
        directory = Path(self.enterContext(tempfile.TemporaryDirectory()))
        for name, text in texts.items():
            (directory / f"{name}.log").write_text(text, encoding="utf-8")
        return directory

    def test_each_source_parses_its_own_lines_only(self):
        directory = self.logs(gateway=GATEWAY, match=MATCH, client=WORKER)
        result = ns.collect(directory / "gateway.log", directory / "match.log", [directory / "client.log"])
        gateway = result["gateway"]
        self.assertEqual(len(gateway["windows"]), 2)
        self.assertEqual(gateway["windows"][0]["results_interval_ms"], {"p50": 55.1, "p90": 66.8, "p99": 70.2, "max": 80.0})
        self.assertIsNone(gateway["windows"][0]["link_action_interval_ms"])
        self.assertEqual(gateway["summary"]["1"]["results_per_s"], 24.0)
        self.assertEqual(gateway["summary"]["1"]["link_actions_per_s"], 1.0)
        self.assertEqual(gateway["summary"]["1"]["link_action_interval_ms_p50_median"], 33.4)

        match = result["match"]["summary"]
        self.assertEqual(match["ipc_iterations_per_s"], 806.15)
        self.assertAlmostEqual(match["cpu_per_s"], 0.03)
        self.assertEqual(match["tick_late_bins"], [2, 1, 0, 0, 1000, 196, 1, 0, 0, 0])
        self.assertEqual((match["tick_notified"], match["tick_early"], match["tick_late_max_us"]), (1, 2, 9000))

        worker = result[f"client:{directory / 'client.log'}"]
        self.assertEqual((worker["summary"]["1"]["windows"], worker["summary"]["1"]["wakes_per_s"]), (1, 481.2))
        self.assertAlmostEqual(worker["summary"]["1"]["cpu_per_s"], 0.0051)
        self.assertIsNone(worker["summary"]["0"]["cpu_per_s"])  # that window could not read the thread's CPU
        self.assertIsNone(worker["windows"][1]["cpu_s"])

    def test_a_log_without_statistics_is_an_error(self):
        directory = self.logs(match="Object_FPS_PVP Match ready: 127.0.0.1:27016\n")
        with self.assertRaisesRegex(ValueError, "no statistics lines"):
            ns.collect(match=directory / "match.log")
        self.assertEqual(ns.main(["--match", str(directory / "match.log")]), 1)


if __name__ == "__main__":
    unittest.main()
