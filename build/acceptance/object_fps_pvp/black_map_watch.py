"""Record-only window captures of a GUI command for N-gui-black-map-intermittent (macOS).

Runs a command unchanged and captures every on-screen window of its process tree, densely right
after a window appears, then classifies the captures with black_frame.py. Captures are taken in
process with CGWindowListCreateImage (what `screencapture -l` uses; spawning screencapture took
about 600 ms a capture) at the nominal (1x) resolution and written as BMP. Never a verdict: the
command's own result is passed through, and the timing results of a watched run do not count
(the captures load the machine and WindowServer).

Usage:
  python3 black_map_watch.py --output DIR [--duration SECONDS] -- COMMAND [ARGUMENT ...]
Exit status: the command's, or 2 when the watch could not start (not macOS, no Screen
Recording permission, or a window appeared during the preflight capture).
"""
import argparse
import json
import os
from pathlib import Path
import signal
import struct
import subprocess
import sys
import tempfile
import time

import black_frame

# Declared with the plan (black-map-observation.md section 3): dense while the warmup frame could show.
EARLY_SECONDS, EARLY_INTERVAL, LATE_INTERVAL = 10.0, 0.5, 5.0
MINIMUM_WINDOW = 200              # points; smaller windows (menus, tooltips, the menu-bar capture
                                  # indicator) are neither game windows nor dialogs
POLL_SECONDS, PROCESS_SECONDS = 0.05, 0.25
PREFLIGHT_SETTLE_SECONDS = 1.5
STOP_GRACE_SECONDS = 5.0


def offset(index):
    """Seconds after a window appeared at which capture `index` is due."""
    early = int(EARLY_SECONDS / EARLY_INTERVAL)
    if index <= early:
        return index * EARLY_INTERVAL
    return EARLY_SECONDS + (index - early) * LATE_INTERVAL


def next_index(index, elapsed):
    """The first capture index still due at `elapsed`: a late loop skips, never bursts."""
    while offset(index + 1) <= elapsed:
        index += 1
    return index


def descendants(root, table):
    """PIDs of `root` and its descendants in a [(pid, ppid)] table."""
    children = {}
    for pid, parent in table:
        children.setdefault(parent, []).append(pid)
    found, pending = set(), [root]
    while pending:
        pid = pending.pop()
        if pid not in found:
            found.add(pid)
            pending.extend(children.get(pid, ()))
    return found


def large(window):
    return window["bounds"][2] >= MINIMUM_WINDOW and window["bounds"][3] >= MINIMUM_WINDOW


def role_of(arguments):
    words = arguments.split()
    return words[words.index("--role") + 1] if "--role" in words[:-1] else "-"


class MacSystem:
    """The only part that touches macOS: process table, window list, screen capture."""

    def __init__(self):
        import ctypes
        self.ctypes = ctypes
        self.cg = ctypes.CDLL("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")
        self.cf = ctypes.CDLL("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation")
        p, c = ctypes.c_void_p, ctypes
        self.cg.CGPreflightScreenCaptureAccess.restype = c.c_bool
        self.cg.CGWindowListCopyWindowInfo.restype, self.cg.CGWindowListCopyWindowInfo.argtypes = p, [c.c_uint32, c.c_uint32]
        self.cf.CFArrayGetCount.restype, self.cf.CFArrayGetCount.argtypes = c.c_long, [p]
        self.cf.CFArrayGetValueAtIndex.restype, self.cf.CFArrayGetValueAtIndex.argtypes = p, [p, c.c_long]
        self.cf.CFDictionaryGetValue.restype, self.cf.CFDictionaryGetValue.argtypes = p, [p, p]
        self.cf.CFNumberGetValue.restype, self.cf.CFNumberGetValue.argtypes = c.c_bool, [p, c.c_int, p]
        self.cf.CFStringGetCString.restype, self.cf.CFStringGetCString.argtypes = c.c_bool, [p, c.c_char_p, c.c_long, c.c_uint32]
        self.cf.CFRelease.argtypes = [p]

        class Rect(c.Structure):
            _fields_ = [("x", c.c_double), ("y", c.c_double), ("width", c.c_double), ("height", c.c_double)]
        self.Rect = Rect
        self.cg.CGRectMakeWithDictionaryRepresentation.restype = c.c_bool
        self.cg.CGRectMakeWithDictionaryRepresentation.argtypes = [p, c.POINTER(Rect)]
        self.keys = {name: p.in_dll(self.cg, "kCGWindow" + name).value
                     for name in ("Number", "OwnerPID", "Layer", "OwnerName", "Bounds")}
        self.cg.CGWindowListCreateImage.restype = p
        self.cg.CGWindowListCreateImage.argtypes = [Rect, c.c_uint32, c.c_uint32, c.c_uint32]
        for name in ("Width", "Height", "BytesPerRow", "BitsPerPixel", "BitmapInfo"):
            function = getattr(self.cg, "CGImageGet" + name)
            function.restype, function.argtypes = c.c_size_t if name != "BitmapInfo" else c.c_uint32, [p]
        self.cg.CGImageGetDataProvider.restype, self.cg.CGImageGetDataProvider.argtypes = p, [p]
        self.cg.CGDataProviderCopyData.restype, self.cg.CGDataProviderCopyData.argtypes = p, [p]
        self.cg.CGImageRelease.argtypes = [p]
        self.cf.CFDataGetBytePtr.restype, self.cf.CFDataGetBytePtr.argtypes = p, [p]
        self.cf.CFDataGetLength.restype, self.cf.CFDataGetLength.argtypes = c.c_long, [p]

    @staticmethod
    def now():
        return time.monotonic()

    @staticmethod
    def sleep(seconds):
        time.sleep(seconds)

    def preflight(self):
        """Screen Recording permission, asked without a dialog."""
        return bool(self.cg.CGPreflightScreenCaptureAccess())

    @staticmethod
    def table():
        text = subprocess.run(["ps", "-A", "-o", "pid=,ppid="], capture_output=True, text=True).stdout
        return [tuple(int(value) for value in line.split()) for line in text.splitlines() if line.strip()]

    @staticmethod
    def arguments(pid):
        return subprocess.run(["ps", "-o", "args=", "-p", str(pid)], capture_output=True, text=True).stdout.strip()

    def windows(self):
        """On-screen windows (excluding desktop elements): id, pid, layer, owner, bounds in points."""
        c, cf = self.ctypes, self.cf
        listing = self.cg.CGWindowListCopyWindowInfo(1 | 16, 0)
        if not listing:
            return []
        result = []
        try:
            for index in range(cf.CFArrayGetCount(listing)):
                entry = cf.CFArrayGetValueAtIndex(listing, index)

                def number(name):
                    value = c.c_longlong()
                    reference = cf.CFDictionaryGetValue(entry, self.keys[name])
                    return value.value if reference and cf.CFNumberGetValue(reference, 4, c.byref(value)) else -1
                owner = c.create_string_buffer(256)
                reference = cf.CFDictionaryGetValue(entry, self.keys["OwnerName"])
                named = bool(reference) and cf.CFStringGetCString(reference, owner, 256, 0x08000100)
                rect = self.Rect()
                self.cg.CGRectMakeWithDictionaryRepresentation(
                    cf.CFDictionaryGetValue(entry, self.keys["Bounds"]), c.byref(rect))
                result.append({"id": number("Number"), "pid": number("OwnerPID"), "layer": number("Layer"),
                               "owner": owner.value.decode("utf-8", "replace") if named else "",
                               "bounds": [rect.x, rect.y, rect.width, rect.height]})
        finally:
            cf.CFRelease(listing)
        return result

    def capture(self, window, path):
        """(status, error) of one capture of one window's content, written as a 32-bit top-down BMP."""
        cg, cf = self.cg, self.cf
        # CGRectNull, kCGWindowListOptionIncludingWindow, kCGWindowImageBoundsIgnoreFraming | NominalResolution.
        image = cg.CGWindowListCreateImage(self.Rect(float("inf"), float("inf"), 0, 0), 8, window, 1 | 16)
        if not image:
            return 1, "CGWindowListCreateImage returned no image"
        try:
            width, height = cg.CGImageGetWidth(image), cg.CGImageGetHeight(image)
            row, bits, info = cg.CGImageGetBytesPerRow(image), cg.CGImageGetBitsPerPixel(image), cg.CGImageGetBitmapInfo(image)
            data = cg.CGDataProviderCopyData(cg.CGImageGetDataProvider(image))
            if not data:
                return 1, "the window image has no data"
            try:
                pixels = self.ctypes.string_at(cf.CFDataGetBytePtr(data), cf.CFDataGetLength(data))
            finally:
                cf.CFRelease(data)
        finally:
            cg.CGImageRelease(image)
        try:
            Path(path).write_bytes(bmp_from_rows(width, height, row, bits, info, pixels))
        except ValueError as error:
            return 1, str(error)
        return 0, ""


def bmp_from_rows(width, height, row, bits, info, pixels):
    """A 32-bit top-down BI_BITFIELDS BMP of CGImage rows, without touching each pixel."""
    if bits != 32 or row < width * 4 or len(pixels) < row * height:
        raise ValueError(f"unsupported window image bits={bits} row={row} width={width}")
    order, alpha = info & 0x7000, info & 0x1F
    # CGImageAlphaInfo: 1, 3, 5 put alpha (or skip) last; 2, 4, 6 put it first. Read each pixel as a
    # little-endian word: memory R,G,B,A / B,G,R,A (32-bit little) / A,R,G,B / A,B,G,R (32-bit little).
    last, first = alpha in (1, 3, 5), alpha in (2, 4, 6)
    little = order == 0x2000
    if order not in (0, 0x2000, 0x4000) or not (last or first):
        raise ValueError(f"unsupported window image bitmap info {info:#x}")
    if last:
        masks = (0xFF000000, 0x00FF0000, 0x0000FF00) if little else (0x000000FF, 0x0000FF00, 0x00FF0000)
    else:
        masks = (0x00FF0000, 0x0000FF00, 0x000000FF) if little else (0x0000FF00, 0x00FF0000, 0xFF000000)
    body = pixels[:row * height] if row == width * 4 else b"".join(
        pixels[y * row:y * row + width * 4] for y in range(height))
    header = struct.pack("<IiiHHIIiiII", 40, width, -height, 1, 32, 3, len(body), 2835, 2835, 0, 0)
    offset = 14 + len(header) + 12
    return b"BM" + struct.pack("<IHHI", offset + len(body), 0, 0, offset) + header + struct.pack("<III", *masks) + body


def preflight(system):
    """None when a capture can run without a dialog, else the reason it cannot."""
    if not system.preflight():
        return "Screen Recording is not granted to the app that runs this watch; grant it first"
    before = system.windows()
    known = {window["id"] for window in before}
    candidates = [window for window in before if window["layer"] == 0]
    if candidates:
        with tempfile.TemporaryDirectory(prefix="black-map-preflight-") as directory:
            # The capture is of the user's own screen: it is discarded, never kept.
            status, error = system.capture(candidates[0]["id"], Path(directory) / "preflight.bmp")
        if status != 0:
            return f"the preflight capture failed: {error}"
    deadline = system.now() + PREFLIGHT_SETTLE_SECONDS
    while system.now() < deadline:
        appeared = [window for window in system.windows() if window["id"] not in known and large(window)]
        if appeared:
            owners = sorted({window["owner"] for window in appeared})
            return f"a window appeared during the preflight capture (a permission dialog?): {owners}"
        system.sleep(POLL_SECONDS)
    return None


class Watch:
    def __init__(self, system, output, root):
        self.system, self.output, self.root = system, Path(output), root
        self.started = system.now()
        self.windows, self.foreign, self.records = {}, {}, []
        self.initial = {window["id"] for window in system.windows()}
        self.family, self.family_at = {root}, None

    def record(self, entry):
        self.records.append(entry)

    def step(self):
        now = self.system.now()
        if self.family_at is None or now - self.family_at >= PROCESS_SECONDS:
            self.family, self.family_at = descendants(self.root, self.system.table()), now
        present = set()
        for window in self.system.windows():
            identity = window["id"]
            present.add(identity)
            ours = window["pid"] in self.family
            if ours and window["layer"] == 0 and large(window):
                if identity not in self.windows:
                    arguments = self.system.arguments(window["pid"])
                    self.windows[identity] = {"id": identity, "pid": window["pid"], "role": role_of(arguments),
                                              "arguments": arguments, "first_seen": now - self.started,
                                              "bounds": window["bounds"], "next": 0, "gone": None}
                    self.record({"kind": "window", **{k: v for k, v in self.windows[identity].items()
                                                      if k not in ("next", "gone")}})
            elif not ours and large(window) and identity not in self.initial and identity not in self.foreign:
                # Anything else that appears while watching (a dialog would take the focus): recorded only.
                self.foreign[identity] = {"kind": "foreign_window", "time": now - self.started, **window}
                self.record(self.foreign[identity])
        for identity, window in self.windows.items():
            if window["gone"] is not None:
                continue
            if identity not in present:
                window["gone"] = now - self.started
                self.record({"kind": "window_gone", "id": identity, "time": window["gone"]})
                continue
            elapsed = now - self.started - window["first_seen"]
            if elapsed < offset(window["next"]):
                continue
            index = next_index(window["next"], elapsed)
            name = f"w{identity}-{index:03d}.bmp"
            before = self.system.now()
            status, error = self.system.capture(identity, self.output / name)
            after = self.system.now()
            self.record({"kind": "capture", "id": identity, "index": index, "due": offset(index),
                         "elapsed": before - self.started - window["first_seen"],
                         "duration_ms": round((after - before) * 1000, 1), "file": name,
                         "status": status, "error": error})
            window["next"] = index + 1


def classify(output, records):
    for entry in records:
        if entry["kind"] != "capture":
            continue
        path = Path(output) / entry["file"]
        if entry["status"] != 0 or not path.exists():
            entry["class"] = "missing"
            continue
        try:
            entry.update(black_frame.measure(black_frame.read_image(path)))
        except (OSError, ValueError) as error:
            entry["class"], entry["read_error"] = "unreadable", str(error)


def prune(output, records):
    """Keep every capture that is not plainly world or lobby, and each window's first and last one;
    delete the other files (about 4 MB each). Their measurements stay in watch.jsonl."""
    captures = {}
    for entry in records:
        if entry["kind"] == "capture":
            captures.setdefault(entry["id"], []).append(entry)
    for entries in captures.values():
        for position, entry in enumerate(entries):
            entry["kept"] = (position in (0, len(entries) - 1) or entry["class"] not in ("world", "no_hud"))
            if not entry["kept"]:
                (Path(output) / entry["file"]).unlink(missing_ok=True)


def summarize(records, exit_code):
    """Per window: the class sequence and its warmup_like episode, in seconds after it appeared."""
    windows = {}
    for entry in records:
        if entry["kind"] == "window":
            windows[entry["id"]] = {"id": entry["id"], "pid": entry["pid"], "role": entry["role"],
                                    "first_seen": round(entry["first_seen"], 3), "sequence": []}
    for entry in records:
        if entry["kind"] == "capture" and entry["id"] in windows:
            windows[entry["id"]]["sequence"].append([round(entry["elapsed"], 3), entry["class"]])
    totals, durations = {}, []
    for entry in records:
        if entry["kind"] == "capture":
            totals[entry["class"]] = totals.get(entry["class"], 0) + 1
            durations.append(entry["duration_ms"])
    for window in windows.values():
        times = [t for t, kind in window["sequence"] if kind == "warmup_like"]
        world = [t for t, kind in window["sequence"] if kind == "world"]
        window["warmup_like"] = {"count": len(times), "first": min(times, default=None),
                                 "last": max(times, default=None)}
        window["first_world"] = min(world, default=None)
        window["warmup_after_world"] = bool(world) and any(t > min(world) for t in times)
    durations.sort()
    return {"version": 1, "use": "record only; never a verdict", "command_exit": exit_code,
            "windows": list(windows.values()), "classes": totals,
            "foreign_windows": [entry for entry in records if entry["kind"] == "foreign_window"],
            "capture_ms": {"count": len(durations), "p50": durations[len(durations) // 2] if durations else None,
                           "max": durations[-1] if durations else None}}


def stop(process):
    """End the command's whole process group: SIGTERM, then SIGKILL after a grace period."""
    try:
        os.killpg(process.pid, signal.SIGTERM)
        process.wait(timeout=STOP_GRACE_SECONDS)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait()
    except ProcessLookupError:
        pass


def run(system, output, command, duration=None, spawn=None, end=stop):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    reason = preflight(system)
    if reason:
        print(f"black_map_watch: {reason}", file=sys.stderr)
        return 2
    spawn = spawn or (lambda: subprocess.Popen(command, start_new_session=True))
    process = spawn()
    watch = Watch(system, output, process.pid)
    stopped = False
    while process.poll() is None:
        watch.step()
        if duration is not None and not stopped and system.now() - watch.started >= duration:
            watch.record({"kind": "stop", "time": system.now() - watch.started, "duration": duration})
            end(process)
            stopped = True
        system.sleep(POLL_SECONDS)
    exit_code = process.returncode
    classify(output, watch.records)
    prune(output, watch.records)
    with (output / "watch.jsonl").open("w", encoding="utf-8") as stream:
        for entry in watch.records:
            stream.write(json.dumps(entry, sort_keys=True) + "\n")
    summary = summarize(watch.records, exit_code)
    (output / "black-map-summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n",
                                                    encoding="utf-8")
    flagged = sum(window["warmup_like"]["count"] > 0 for window in summary["windows"])
    print(f"black_map_watch: {len(summary['windows'])} windows, {summary['capture_ms']['count']} captures, "
          f"{flagged} with warmup_like, classes {summary['classes']}", file=sys.stderr)
    return exit_code


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--duration", type=float, help="stop the command after this many seconds")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    arguments = parser.parse_args(argv)
    command = arguments.command[1:] if arguments.command[:1] == ["--"] else arguments.command
    if not command:
        parser.error("a command to run is required after --")
    if sys.platform != "darwin":
        print("black_map_watch: macOS only", file=sys.stderr)
        return 2
    return run(MacSystem(), arguments.output, command, arguments.duration)


if __name__ == "__main__":
    sys.exit(main())
