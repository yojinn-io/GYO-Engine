"""Unit tests for the record-only window watch; a fake system replaces macOS, no GUI is started."""
import json
from pathlib import Path
import tempfile
import unittest

import black_frame
import black_map_watch as watch
from test_black_frame import WARMUP_BLOCKS, bmp_bytes, scene, world_background

WARMUP = scene((0, 0, 0), blocks=WARMUP_BLOCKS, title=8)
WORLD = scene(world_background, hud=True, blocks=WARMUP_BLOCKS, title=8)
CAPTURE_SECONDS = .15
ENCODED = {}


def encoded(image):
    if id(image) not in ENCODED:
        ENCODED[id(image)] = bmp_bytes(image.width, image.height, image.pixel)
    return ENCODED[id(image)]


def window(identity, pid, layer=0, size=(1280, 748), owner="probe"):
    return {"id": identity, "pid": pid, "layer": layer, "owner": owner, "bounds": [0, 0, *size]}


class FakeSystem:
    """A clock that moves on sleep and capture; windows and frames are functions of the time since the
    command started (-1 before it starts, as during the preflight)."""

    def __init__(self, timeline, table=((100, 1), (101, 100)), frames=None, permitted=True, failing=()):
        self.clock, self.timeline, self._table = 0.0, timeline, list(table)
        self.frames = frames or (lambda identity, t: WARMUP if t < 3 else WORLD)
        self.permitted, self.failing = permitted, set(failing)
        self.captured, self.appeared, self.origin = [], {}, None

    def now(self):
        return self.clock

    def sleep(self, seconds):
        self.clock += seconds

    def preflight(self):
        return self.permitted

    def table(self):
        return self._table

    def arguments(self, pid):
        return f"probe --role {'create' if pid == 101 else 'join'} --fps 60"

    def windows(self):
        listed = self.timeline(-1 if self.origin is None else self.clock - self.origin)
        for entry in listed:
            self.appeared.setdefault(entry["id"], self.clock)
        return listed

    def capture(self, identity, path):
        self.captured.append((identity, self.clock))
        self.clock += CAPTURE_SECONDS
        if identity in self.failing:
            return 1, "could not create image from window"
        image = self.frames(identity, self.clock - self.appeared.get(identity, 0))
        Path(path).write_bytes(encoded(image))
        return 0, ""


class FakeProcess:
    pid = 100

    def __init__(self, system, ends_at, code=0):
        self.system, self.ends_at, self.code, self.returncode = system, ends_at, code, None
        system.origin = system.clock

    def poll(self):
        if self.returncode is None and self.system.clock - self.system.origin >= self.ends_at:
            self.returncode = self.code
        return self.returncode


class ScheduleTests(unittest.TestCase):
    def test_dense_then_sparse(self):
        self.assertEqual([watch.offset(i) for i in (0, 1, 2, 20, 21, 22)], [0, .5, 1, 10, 15, 20])

    def test_a_late_loop_skips_to_the_latest_due_capture(self):
        self.assertEqual(watch.next_index(0, 3.2), 6)
        self.assertEqual(watch.next_index(4, 0.1), 4)
        self.assertEqual(watch.next_index(20, 16), 21)

    def test_process_family_and_role(self):
        table = [(1, 0), (100, 1), (101, 100), (102, 101), (200, 1)]
        self.assertEqual(watch.descendants(100, table), {100, 101, 102})
        self.assertEqual(watch.role_of("probe --role join --fps 60"), "join")
        self.assertEqual(watch.role_of("client --gateway 127.0.0.1:1 --role"), "-")


class WindowImageTests(unittest.TestCase):
    """CGImage rows to BMP: every 32-bit layout CoreGraphics reports, read back by black_frame."""

    @staticmethod
    def colour(x, y):
        return (x * 40 + 3) % 256, (y * 70 + 5) % 256, (x * y * 13 + 7) % 256

    def round_trip(self, info, layout, padding=0):
        width, height = 5, 3
        row = width * 4 + padding
        pixels = bytearray()
        for y in range(height):
            for x in range(width):
                r, g, b = self.colour(x, y)
                pixels += bytes({"RGBA": (r, g, b, 255), "BGRA": (b, g, r, 255),
                                 "ARGB": (255, r, g, b), "ABGR": (255, b, g, r)}[layout])
            pixels += bytes(padding)
        with tempfile.TemporaryDirectory(prefix="pvp-window-image-") as directory:
            path = Path(directory) / "window.bmp"
            path.write_bytes(watch.bmp_from_rows(width, height, row, 32, info, bytes(pixels)))
            image = black_frame.read_image(path)
            self.assertEqual((image.width, image.height), (width, height))
            for y in range(height):
                for x in range(width):
                    self.assertEqual(image.pixel(x, y), self.colour(x, y), (layout, x, y))

    def test_every_layout(self):
        self.round_trip(0x2000 | 2, "BGRA")   # 32-bit little, premultiplied first: the usual window image
        self.round_trip(0x2000 | 6, "BGRA")   # 32-bit little, skip first
        self.round_trip(0x0000 | 1, "RGBA")   # default order, premultiplied last
        self.round_trip(0x4000 | 5, "RGBA")   # 32-bit big, skip last
        self.round_trip(0x2000 | 1, "ABGR")   # 32-bit little, premultiplied last
        self.round_trip(0x4000 | 2, "ARGB")   # 32-bit big, premultiplied first

    def test_row_padding_is_dropped(self):
        self.round_trip(0x2000 | 2, "BGRA", padding=12)

    def test_unsupported_images_are_errors(self):
        with self.assertRaises(ValueError):
            watch.bmp_from_rows(2, 2, 6, 24, 0x2000 | 2, bytes(12))
        with self.assertRaises(ValueError):
            watch.bmp_from_rows(2, 2, 8, 32, 0x2000 | 0, bytes(16))   # alpha only: no colour channels


class PreflightTests(unittest.TestCase):
    def test_no_permission_never_captures(self):
        system = FakeSystem(lambda t: [window(1, 50)], permitted=False)
        self.assertIn("not granted", watch.preflight(system))
        self.assertEqual(system.captured, [])

    def test_a_window_appearing_after_the_capture_stops_the_watch(self):
        appears = [None]

        def screen(t):
            appears[0] = appears[0] or (system.clock + .1 if system.captured else None)
            dialog = appears[0] is not None and system.clock >= appears[0]
            return [window(1, 50)] + ([window(9, 77, owner="Dialog")] if dialog else [])
        system = FakeSystem(screen)
        self.assertIn("Dialog", watch.preflight(system))

    def test_the_menu_bar_capture_indicator_is_not_a_dialog(self):
        appears = [None]

        def screen(t):
            appears[0] = appears[0] or (system.clock if system.captured else None)
            shown = appears[0] is not None
            return [window(1, 50)] + ([window(5, 202, layer=2147483630, size=(28, 28), owner="Window Server")]
                                      if shown else [])
        system = FakeSystem(screen)
        self.assertIsNone(watch.preflight(system))

    def test_a_failed_capture_stops_the_watch(self):
        system = FakeSystem(lambda t: [window(1, 50)], failing={1})
        self.assertIn("failed", watch.preflight(system))

    def test_a_quiet_screen_passes_and_keeps_no_file(self):
        system = FakeSystem(lambda t: [window(1, 50)])
        self.assertIsNone(watch.preflight(system))
        self.assertEqual([identity for identity, _ in system.captured], [1])


class RunTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="pvp-black-map-watch-")
        self.addCleanup(temporary.cleanup)
        self.output = Path(temporary.name)

    def run_watch(self, system, ends_at, duration=None, code=0):
        stopped = []

        def end(process):
            stopped.append(system.clock)
            process.returncode = -15
        status = watch.run(system, self.output, ["probe"], duration=duration,
                           spawn=lambda: FakeProcess(system, ends_at, code), end=end)
        summary = json.loads((self.output / "black-map-summary.json").read_text(encoding="utf-8"))
        records = [json.loads(line) for line in (self.output / "watch.jsonl").read_text(encoding="utf-8").splitlines()]
        return status, summary, records, stopped

    @staticmethod
    def screen(t):
        listed = [window(1, 50, owner="Claude")]                      # present before the command
        if t >= 1:
            listed.append(window(7, 101))                             # the command's child: watched
            listed.append(window(8, 101, size=(120, 40)))             # too small: a menu or tooltip
        if t >= 2:
            listed.append(window(9, 77, layer=25, owner="Dialog"))    # foreign: recorded only
            listed.append(window(5, 202, layer=2147483630, size=(28, 28), owner="Window Server"))  # indicator
        return listed

    def test_the_command_window_is_captured_densely_and_classified(self):
        system = FakeSystem(self.screen)
        status, summary, records, _ = self.run_watch(system, ends_at=14, code=3)
        self.assertEqual(status, 3)
        self.assertEqual(summary["command_exit"], 3)
        self.assertEqual([w["id"] for w in summary["windows"]], [7])
        observed = summary["windows"][0]
        self.assertEqual(observed["role"], "create")
        sequence = observed["sequence"]
        # Due every 0.5 s for 10 s, then every 5 s; each capture lands within one poll and capture of its due time.
        captures = [r for r in records if r["kind"] == "capture"]
        self.assertEqual([c["due"] for c in captures][:21], [i * .5 for i in range(21)])
        self.assertTrue(all(0 <= c["elapsed"] - c["due"] < watch.POLL_SECONDS + CAPTURE_SECONDS for c in captures))
        self.assertEqual({kind for t, kind in sequence if t < 2.8}, {"warmup_like"})
        self.assertEqual({kind for t, kind in sequence if t > 3.2}, {"world"})
        self.assertEqual(observed["warmup_like"]["first"], sequence[0][0])
        self.assertLess(observed["warmup_like"]["last"], 3.2)
        self.assertGreater(observed["first_world"], 2.8)
        self.assertFalse(observed["warmup_after_world"])
        self.assertEqual([f["owner"] for f in summary["foreign_windows"]], ["Dialog"])
        # Files kept: every warmup_like capture and the window's first and last; world files are deleted.
        kept = [c for c in captures if c["kept"]]
        self.assertTrue(all((self.output / c["file"]).exists() == c["kept"] for c in captures))
        self.assertEqual({c["class"] for c in captures if not c["kept"]}, {"world"})
        self.assertTrue(all(c["kept"] for c in captures if c["class"] == "warmup_like"))
        self.assertIn(captures[-1], kept)

    def test_warmup_after_the_world_is_flagged(self):
        frames = lambda identity, t: WARMUP if t < 1 or 4 < t < 5 else WORLD
        _, summary, _, _ = self.run_watch(FakeSystem(self.screen, frames=frames), ends_at=8)
        self.assertTrue(summary["windows"][0]["warmup_after_world"])

    def test_a_failed_capture_is_missing_not_a_class(self):
        _, summary, _, _ = self.run_watch(FakeSystem(self.screen, failing={7}), ends_at=4)
        self.assertEqual(set(summary["classes"]), {"missing"})

    def test_a_window_that_goes_away_is_no_longer_captured(self):
        screen = lambda t: [w for w in self.screen(t) if not (w["id"] == 7 and t > 3)]
        system = FakeSystem(screen)
        _, _, records, _ = self.run_watch(system, ends_at=8)
        gone = [r["time"] for r in records if r["kind"] == "window_gone"]
        self.assertEqual(len(gone), 1)
        self.assertTrue(all(t - system.origin <= gone[0] for identity, t in system.captured if identity == 7))

    def test_duration_stops_the_command_once(self):
        system = FakeSystem(self.screen)
        # The command would end by itself at 30 s: a watch that ignores the duration ends there unstopped.
        _, summary, records, stopped = self.run_watch(system, ends_at=30, duration=6)
        self.assertEqual(len(stopped), 1)
        self.assertGreaterEqual(stopped[0] - system.origin, 6)
        self.assertEqual(summary["command_exit"], -15)
        self.assertEqual([r["kind"] for r in records].count("stop"), 1)

    def test_no_permission_starts_nothing(self):
        spawned = []
        status = watch.run(FakeSystem(self.screen, permitted=False), self.output, ["probe"],
                           spawn=lambda: spawned.append(1))
        self.assertEqual(status, 2)
        self.assertEqual(spawned, [])


if __name__ == "__main__":
    unittest.main()
