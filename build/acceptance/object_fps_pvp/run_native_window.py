"""Owned-window X11/XTest acceptance. No SDL event or gameplay-state injection.

This intentionally perturbs desktop focus/window management and writes read-only
observer state. It is a separate functional run, never a clean timing sample.
"""
import argparse
import ctypes as C
import ctypes.util
import hashlib
import json
import math
import os
from pathlib import Path
import socket
import struct
import subprocess
import time
import urllib.request

from run_network import free_port, wait_for_match_ready


class ClientData(C.Union):
    _fields_ = [("b", C.c_char * 20), ("s", C.c_short * 10), ("l", C.c_long * 5)]


class ClientMessage(C.Structure):
    _fields_ = [("type", C.c_int), ("serial", C.c_ulong), ("send_event", C.c_int),
                ("display", C.c_void_p), ("window", C.c_ulong), ("message_type", C.c_ulong),
                ("format", C.c_int), ("data", ClientData)]


class Event(C.Union):
    _fields_ = [("message", ClientMessage), ("pad", C.c_long * 24)]


class Desktop:
    def __init__(self):
        self.x = C.CDLL(ctypes.util.find_library("X11"))
        self.t = C.CDLL(ctypes.util.find_library("Xtst"))
        def declare(lib, name, result, *args):
            function = getattr(lib, name)
            function.restype, function.argtypes = result, args
            return function
        V, U, I, L = C.c_void_p, C.c_ulong, C.c_int, C.c_long
        declare(self.x, "XOpenDisplay", V, C.c_char_p)
        declare(self.x, "XDefaultRootWindow", U, V)
        declare(self.x, "XDefaultScreen", I, V)
        declare(self.x, "XDisplayWidth", I, V, I)
        declare(self.x, "XDisplayHeight", I, V, I)
        declare(self.x, "XInternAtom", U, V, C.c_char_p, I)
        declare(self.x, "XGetWindowProperty", I, V, U, U, L, L, I, U,
                C.POINTER(U), C.POINTER(I), C.POINTER(U), C.POINTER(U), C.POINTER(V))
        declare(self.x, "XFree", I, V)
        declare(self.x, "XGetGeometry", I, V, U, C.POINTER(U), C.POINTER(I), C.POINTER(I),
                C.POINTER(C.c_uint), C.POINTER(C.c_uint), C.POINTER(C.c_uint), C.POINTER(C.c_uint))
        declare(self.x, "XTranslateCoordinates", I, V, U, U, I, I, C.POINTER(I), C.POINTER(I), C.POINTER(U))
        declare(self.x, "XQueryTree", I, V, U, C.POINTER(U), C.POINTER(U), C.POINTER(V), C.POINTER(C.c_uint))
        declare(self.x, "XSendEvent", I, V, U, I, L, C.POINTER(Event))
        declare(self.x, "XRaiseWindow", I, V, U)
        declare(self.x, "XMoveWindow", I, V, U, I, I)
        declare(self.x, "XGetInputFocus", I, V, C.POINTER(U), C.POINTER(I))
        declare(self.x, "XSetInputFocus", I, V, U, I, U)
        declare(self.x, "XStringToKeysym", U, C.c_char_p)
        declare(self.x, "XKeysymToKeycode", C.c_uint, V, U)
        declare(self.x, "XFlush", I, V)
        declare(self.x, "XSync", I, V, I)
        declare(self.x, "XCloseDisplay", I, V)
        declare(self.t, "XTestFakeMotionEvent", I, V, I, I, I, U)
        declare(self.t, "XTestFakeRelativeMotionEvent", I, V, I, I, U)
        declare(self.t, "XTestFakeButtonEvent", I, V, C.c_uint, I, U)
        declare(self.t, "XTestFakeKeyEvent", I, V, C.c_uint, I, U)
        self.display = self.x.XOpenDisplay(None)
        if not self.display:
            raise RuntimeError("Cannot open authorized X11 display")
        self.root = self.x.XDefaultRootWindow(self.display)
        self.screen = self.x.XDefaultScreen(self.display)
        self.size = (self.x.XDisplayWidth(self.display, self.screen), self.x.XDisplayHeight(self.display, self.screen))
        self.owned, self.keys, self.buttons = {}, set(), set()

    def atom(self, name):
        return self.x.XInternAtom(self.display, name.encode(), 0)

    def property(self, window, name):
        actual, count, remaining, data = C.c_ulong(), C.c_ulong(), C.c_ulong(), C.c_void_p()
        fmt = C.c_int()
        status = self.x.XGetWindowProperty(self.display, window, self.atom(name), 0, 4096, 0, 0,
                                          C.byref(actual), C.byref(fmt), C.byref(count), C.byref(remaining), C.byref(data))
        if status or not data.value:
            return None
        try:
            if fmt.value == 32:
                return list(C.cast(data, C.POINTER(C.c_ulong))[:count.value])
            if fmt.value == 8:
                return C.string_at(data, count.value).decode(errors="replace")
            return None
        finally:
            self.x.XFree(data)

    def own(self, window, pid, title):
        if self.property(window, "_NET_WM_PID") != [pid] or self.property(window, "_NET_WM_NAME") != title:
            raise RuntimeError("Refusing desktop control: probe PID/title ownership mismatch")
        self.owned[window] = (pid, title)

    def validate(self, window):
        if window not in self.owned:
            raise RuntimeError("Refusing unowned desktop target")
        pid, title = self.owned[window]
        if self.property(window, "_NET_WM_PID") != [pid] or self.property(window, "_NET_WM_NAME") != title:
            raise RuntimeError("Owned native window identity changed")

    def parent(self, window):
        root, parent, children, count = C.c_ulong(), C.c_ulong(), C.c_void_p(), C.c_uint()
        if not self.x.XQueryTree(self.display, window, C.byref(root), C.byref(parent), C.byref(children), C.byref(count)):
            raise RuntimeError("Cannot inspect native decoration ancestry")
        if children.value:
            self.x.XFree(children)
        return parent.value

    def geometry(self, window):
        self.validate(window)
        root, child = C.c_ulong(), C.c_ulong()
        x, y, width, height, border, depth = C.c_int(), C.c_int(), C.c_uint(), C.c_uint(), C.c_uint(), C.c_uint()
        if not self.x.XGetGeometry(self.display, window, C.byref(root), C.byref(x), C.byref(y),
                                  C.byref(width), C.byref(height), C.byref(border), C.byref(depth)):
            raise RuntimeError("Cannot read native geometry")
        self.x.XTranslateCoordinates(self.display, window, self.root, 0, 0, C.byref(x), C.byref(y), C.byref(child))
        return [x.value, y.value, width.value, height.value]

    def flush(self):
        self.x.XSync(self.display, 0)

    def focus(self, window):
        self.validate(window)
        event = Event()
        event.message.type, event.message.send_event = 33, 1
        event.message.display, event.message.window = self.display, window
        event.message.message_type, event.message.format = self.atom("_NET_ACTIVE_WINDOW"), 32
        event.message.data.l[0], event.message.data.l[1] = 2, 0
        self.x.XSendEvent(self.display, self.root, 0, (1 << 20) | (1 << 19), C.byref(event))
        self.x.XRaiseWindow(self.display, window)
        self.x.XSetInputFocus(self.display, window, 2, 0)
        self.flush()
        deadline = time.monotonic() + 2
        while True:
            focused, revert = C.c_ulong(), C.c_int()
            self.x.XGetInputFocus(self.display, C.byref(focused), C.byref(revert))
            if focused.value == window:
                break
            if time.monotonic() >= deadline:
                raise RuntimeError("WM did not focus owned native probe")
            time.sleep(.03)

    def position_initial(self, window, x, y):
        self.validate(window)
        self.x.XMoveWindow(self.display, window, x, y)
        self.flush()

    def move_pointer(self, window, x, y):
        self.validate(window)
        if not (0 <= x < self.size[0] and 0 <= y < self.size[1]):
            raise RuntimeError("Native pointer coordinate outside screen")
        self.t.XTestFakeMotionEvent(self.display, self.screen, int(x), int(y), 0)
        self.flush()

    def relative(self, window, dx, dy):
        self.focus(window)
        self.t.XTestFakeRelativeMotionEvent(self.display, int(dx), int(dy), 0)
        self.flush()

    def button(self, window, down, button=1):
        self.validate(window)
        self.t.XTestFakeButtonEvent(self.display, button, int(down), 0)
        self.buttons.add(button) if down else self.buttons.discard(button)
        self.flush()

    def key(self, window, name, down):
        self.focus(window) if down else self.validate(window)
        code = self.x.XKeysymToKeycode(self.display, self.x.XStringToKeysym(name.encode()))
        if not code:
            raise RuntimeError("Unknown native keysym " + name)
        self.t.XTestFakeKeyEvent(self.display, code, int(down), 0)
        self.keys.add(code) if down else self.keys.discard(code)
        self.flush()

    def tap(self, window, name):
        self.key(window, name, True)
        time.sleep(.06)
        self.key(window, name, False)
        time.sleep(.12)

    def click(self, window, hold=.08):
        self.focus(window)
        self.button(window, True)
        time.sleep(hold)
        self.button(window, False)
        time.sleep(.12)

    def drag(self, window, resize=False):
        self.focus(window)
        before = self.geometry(window)
        extents = self.property(window, "_NET_FRAME_EXTENTS")
        if not extents or len(extents) != 4 or extents[2] < 8 or self.parent(window) == self.root:
            raise RuntimeError("No validated window-manager title bar for actual native drag")
        left, right, top, bottom = extents
        x, y, width, height = before
        # Decoration parent was discovered from this exact owned child. Never
        # select a window merely by approximate screen coordinates.
        borderless_resize = resize and right == 0 and bottom == 0
        start = (x + width // 2, y - top // 2) if not resize else (x + width + max(0, right // 2), y + height + max(0, bottom // 2))
        button = 1
        if borderless_resize:
            modifier = subprocess.check_output(["gsettings", "get", "org.gnome.desktop.wm.preferences", "mouse-button-modifier"], text=True).strip()
            right_button = subprocess.check_output(["gsettings", "get", "org.gnome.desktop.wm.preferences", "resize-with-right-button"], text=True).strip()
            require(modifier == "'<Super>'" and right_button == "false", "Unsupported native borderless WM resize binding")
            start, button = (x + width - 50, y + height - 50), 2
        delta = (45, 35) if not resize else (80, 45)
        self.move_pointer(window, *start)
        time.sleep(.08)
        if borderless_resize:
            self.key(window, "Super_L", True)
        self.button(window, True, button)
        for i in range(1, 16):
            self.move_pointer(window, start[0] + delta[0] * i / 15, start[1] + delta[1] * i / 15)
            time.sleep(.025)
        self.button(window, False, button)
        if borderless_resize:
            self.key(window, "Super_L", False)
        time.sleep(.3)
        after = self.geometry(window)
        if (after[2:] == before[2:]) if resize else (after[:2] == before[:2]):
            raise RuntimeError("Native WM gesture produced no expected geometry change: " + json.dumps(
                {"resize": resize, "before": before, "after": after, "extents": extents, "start": start}))
        return {"before": before, "after": after, "frame_extents": extents,
                "method": "XTest Super+middle native WM resize on owned window" if borderless_resize else
                    "XTest physical pointer/button events on owned WM decoration", "resize": resize}

    def close(self):
        if not self.display:
            return
        for key in self.keys:
            self.t.XTestFakeKeyEvent(self.display, key, 0, 0)
        for button in self.buttons:
            self.t.XTestFakeButtonEvent(self.display, button, 0, 0)
        self.flush()
        self.x.XCloseDisplay(self.display)
        self.display = None


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def circle_clear_of_aabb(position, minimum, maximum, radius):
    # PvP collision uses a horizontal circle. An axis-expanded rectangle would
    # incorrectly reject legal rounded-corner positions outside that circle.
    closest = [min(high, max(low, value)) for value, low, high in zip(position, minimum, maximum)]
    return math.dist(position, closest) >= radius


def state(directory, role):
    return json.loads((directory / f"{role}-native-state.json").read_text())


def wait_state(directory, role, condition, message, timeout=4):
    end = time.monotonic() + timeout
    last = None
    while time.monotonic() < end:
        try:
            last = state(directory, role)
            if condition(last):
                return last
        except (FileNotFoundError, json.JSONDecodeError):
            pass
        time.sleep(.025)
    raise RuntimeError(message + ": " + json.dumps(last))


def screenshot(directory, label, window):
    # XWD captures the owned client drawable including actual UI/HUD. Unlike
    # scene GPU capture this is not pre-UI; capture runs outside latency tests.
    raw = directory / f"{label}.xwd"
    subprocess.run(["xwd", "-silent", "-id", str(window), "-out", str(raw)], check=True, timeout=5)
    data = raw.read_bytes()
    header = struct.unpack(">25I", data[:100])
    size, version, fmt, depth, width, height = header[:6]
    byte_order, bits, stride = header[7], header[11], header[12]
    red, green, blue, colors = header[14], header[15], header[16], header[19]
    require(version == 7 and fmt == 2 and bits in (24, 32), "Unsupported captured XWD pixel format")
    pixels = memoryview(data)[size + colors * 12:]
    rgb = bytearray(width * height * 3)
    masks = (red, green, blue)
    shifts = [(mask & -mask).bit_length() - 1 for mask in masks]
    for y in range(height):
        for x in range(width):
            offset = y * stride + x * (bits // 8)
            value = int.from_bytes(pixels[offset:offset + bits // 8], "little" if byte_order == 0 else "big")
            for channel, (mask, shift) in enumerate(zip(masks, shifts)):
                rgb[(y * width + x) * 3 + channel] = ((value & mask) >> shift) * 255 // (mask >> shift)
    from PIL import Image
    Image.frombytes("RGB", (width, height), bytes(rgb)).save(directory / f"{label}.png")
    return {"file": label + ".png", "native_window": window, "width": width, "height": height,
            "source": "XGetImage via xwd, owned client drawable including actual UI"}


def run(args):
    directory = args.output
    directory.mkdir(parents=True, exist_ok=False)
    ipc, http, udp = free_port(), free_port(), free_port(socket.SOCK_DGRAM)
    processes, logs, commands = [], [], []
    desktop = None
    result = {"passed": False, "scope": "Actual X11/XTest input and WM decoration gestures; no human judgement, scanout timing, physical LAN or Windows claim",
              "checks": {}, "events": [], "captures": [], "commands": commands}
    def start(label, command):
        commands.append(command)
        log = (directory / f"{label}.log").open("w")
        logs.append(log)
        process = subprocess.Popen(command, cwd=directory, stdout=log, stderr=subprocess.STDOUT)
        processes.append(process)
        return process
    def record(label, **extra):
        result["events"].append({"label": label, "steady_seconds": time.monotonic(),
                                  "create": state(directory, "create"), "join": state(directory, "join"), **extra})
    try:
        match = start("match", [str(args.match), "--arena", str(args.arena), "--listen", f"127.0.0.1:{ipc}"])
        wait_for_match_ready(match, directory / "match.log", f"127.0.0.1:{ipc}")
        gateway = start("gateway", [str(args.gateway), "--runtime", f"127.0.0.1:{ipc}", "--http", f"127.0.0.1:{http}",
                                    "--udp", f"127.0.0.1:{udp}", "--advertise-ip", "127.0.0.1"])
        deadline = time.monotonic() + 10
        while True:
            require(match.poll() is None and gateway.poll() is None, "Service exited during startup")
            try:
                with urllib.request.urlopen(f"http://127.0.0.1:{http}/rooms", timeout=.2):
                    break
            except OSError:
                require(time.monotonic() < deadline, "Gateway startup deadline")
                time.sleep(.03)
        gui = {}
        for role in ("create", "join"):
            gui[role] = start(role, [str(args.gui_probe), "--native-window", "--duration", "180", "--fps", "60",
                "--arena-root", str(args.arena_root), "--gateway", f"127.0.0.1:{http}", "--role", role,
                "--gpu-driver", args.gpu_driver, "--output", str(directory)])
        desktop = Desktop()
        result["display"] = {"name": os.environ.get("DISPLAY"), "pixels": desktop.size,
                             "wm": desktop.property(desktop.root, "_NET_SUPPORTING_WM_CHECK")}
        ids = {}
        for role in gui:
            ready = wait_state(directory, role, lambda s: len(s["players"]) == 2, "Two native players not ready", 15)
            ids[role] = ready["window_id"]
            desktop.own(ids[role], gui[role].pid, ready["title"])
        a, b = ids["create"], ids["join"]
        desktop.position_initial(a, 60, 100)
        desktop.position_initial(b, min(900, desktop.size[0] - 830), 100)
        time.sleep(.3)
        def sample(role="create"):
            return state(directory, role)
        def fresh(role="create", delay=.16):
            time.sleep(delay)
            return sample(role)
        def capture(role="create"):
            window = ids[role]
            desktop.focus(window)
            value = fresh(role)
            if value["weapon"]["captured"]:
                return value
            x, y, width, height = desktop.geometry(window)
            desktop.move_pointer(window, x + width // 2, y + height // 2)
            before = value["weapon"]["submitted"]
            desktop.click(window)
            value = wait_state(directory, role, lambda s: s["weapon"]["captured"], "Native capture failed")
            require(value["weapon"]["submitted"] == before, "Capture click fired a ghost shot")
            return value
        def aim(yaw, pitch=0., role="create"):
            capture(role)
            for _ in range(10):
                w = sample(role)["weapon"]
                dx = math.remainder(yaw - w["yaw"], 2 * math.pi)
                dy = pitch - w["pitch"]
                if abs(dx) < .004 and abs(dy) < .004:
                    return
                desktop.relative(ids[role], round(dx / .0025), round(dy / .0025))
                time.sleep(.10)
            raise RuntimeError("Native aim did not converge from read-only angle")
        def shoot(role="create", hold=.08):
            capture(role)
            value = sample(role)
            before = value["weapon"]["submitted"]
            if value["weapon"]["cooldown_remaining"] > 0:
                time.sleep(value["weapon"]["cooldown_remaining"] + .08)
            desktop.click(ids[role], hold)
            value = wait_state(directory, role, lambda s: s["weapon"]["last_decision"] > before, "Native shot decision missing")
            require(value["weapon"]["submitted"] == before + 1 and value["weapon"]["animations"] == before + 1,
                    "Native click/hold was not exactly one shot and animation")
            require(value["weapon"]["accepted"] == before + 1, "Legal native shot rejected")
            return value
        if args.window_only:
            capture()
            desktop.tap(a, "Tab")
            require(not fresh()["weapon"]["captured"], "Native Tab did not release capture")
            move = desktop.drag(a)
            record("native-WM-titlebar-drag", drag=move)
            resized = desktop.drag(a, resize=True)
            record("native-WM-resize", resize=resized)
            require(not fresh()["weapon"]["captured"] and sample()["weapon"]["submitted"] == 0,
                    "Native decoration gesture entered gameplay")
            result["captures"].append(screenshot(directory, "native-window-only-hud", a))
            result["checks"]["native_WM_drag_resize_only"] = {"drag": move, "resize": resized}
            result["passed"] = True
            return 0
        # At original spawns, yaw zero points directly from creator to peer.
        capture()
        aim(0)
        record("capture-click-no-shot")
        hp = []
        for index in range(4):
            value = shoot(hold=.65 if index == 0 else .08)
            target = wait_state(directory, "join", lambda s: s["weapon"]["hp"] == 75 - index * 25, "Authority HP mismatch")
            hp.append(target["weapon"]["hp"])
            require(value["weapon"]["last_damage"] == 25 and value["weapon"]["last_target"] == target["player_id"], "Native hit/damage attribution mismatch")
            record("native-player-hit", shot=index + 1)
            time.sleep(.42)
        require(hp == [75, 50, 25, 0], "Unexpected HP path")
        result["checks"]["V3_capture_hold_single_fire"] = True
        result["checks"]["V4_native_four_hits_hp"] = hp
        desktop.focus(b)
        result["captures"].append(screenshot(directory, "peer-hp-zero-hud", b))
        # HP zero remains playable; release later, then rejoin via real Lobby UI.
        capture("join")
        initial = sample("join")["local"]["predicted"]
        desktop.key(b, "w", True)
        shoot("join")
        time.sleep(.25)
        desktop.key(b, "w", False)
        target = fresh("join")
        require(math.dist(initial, target["local"]["predicted"]) > .3, "HP zero blocked movement")
        record("hp-zero-native-move-shot")
        old_id = target["player_id"]
        desktop.tap(b, "Escape")
        wait_state(directory, "join", lambda s: s["phase"] == 0 and not s["weapon"]["active"], "Native Escape did not leave")
        # Lobby begins at Refresh; ordinary arrow navigation chooses Join.
        desktop.tap(b, "Down"); desktop.tap(b, "Down"); desktop.tap(b, "Return")
        target = wait_state(directory, "join", lambda s: s["phase"] == 3 and s["player_id"] != old_id and s["weapon"]["active"], "Native Lobby rejoin failed")
        require(target["weapon"]["hp"] == 100 and target["weapon"]["submitted"] == 0 and not target["weapon"]["hit_marker"], "Rejoin retained old effects")
        result["checks"]["V6_zero_hp_rejoin_new_identity"] = {"old_id": old_id, "new_id": target["player_id"]}
        record("native-rejoined")
        # Native movement: straight, strafe/diagonal, stop, pure mouse turn.
        capture()
        aim(0)
        origin = sample()["local"]["predicted"]
        desktop.key(a, "d", True); desktop.key(a, "w", True)
        time.sleep(.4)
        desktop.key(a, "d", False); desktop.key(a, "w", False)
        stopped = fresh()["local"]["predicted"]
        require(stopped[0] > origin[0] + .5 and stopped[1] > origin[1] + .5, "Native diagonal did not move on both axes")
        time.sleep(.3)
        require(math.dist(stopped, sample()["local"]["predicted"]) < .02, "Released native keys kept moving")
        desktop.relative(a, 40, 8)
        looked = fresh()
        require(abs(looked["weapon"]["yaw"]) > .05 and abs(looked["weapon"]["pitch"]) > .005, "Native mouse look absent")
        require(math.dist(stopped, looked["local"]["predicted"]) < .02, "Pure native look moved position")
        # Read-only position computes input yaw. No state is teleported.
        pos = looked["local"]["predicted"]
        aim(math.atan2(10 - pos[0], 10 - pos[1]))
        desktop.key(a, "w", True)
        desktop.relative(a, 5, 0)
        shoot()
        fired_angles = (sample()["weapon"]["yaw"], sample()["weapon"]["pitch"])
        time.sleep(.2)
        require(math.dist(fired_angles, (sample()["weapon"]["yaw"], sample()["weapon"]["pitch"])) < .0001,
                "Native shot recoil changed gameplay aim without new mouse motion")
        time.sleep(2.0)
        desktop.key(a, "w", False)
        wall = fresh()
        position = wall["local"]["predicted"]
        require(circle_clear_of_aabb(position, (9, 9), (11, 11), .25), "Native approach penetrated wall")
        # Continue diagonal into center corner: collision must keep both
        # predicted and displayed cylinder outside the solid wall footprint.
        aim(math.atan2(10 - position[0], 10 - position[1]))
        desktop.key(a, "w", True)
        time.sleep(.7)
        desktop.key(a, "w", False)
        wall = fresh()
        for p in (wall["local"]["predicted"], wall["local"]["render"]):
            require(circle_clear_of_aabb(p, (9, 9), (11, 11), .25), "Native corner/camera penetrated wall")
        result["checks"]["V1_native_motion_stop_look_wall_corner"] = True
        result["checks"]["V2_native_move_aim_shoot"] = True
        # Near the center wall, aim directly into it; player HP must stay full.
        position = wall["local"]["predicted"]
        aim(math.atan2(10 - position[0], 10 - position[1]))
        before_hp = sample("join")["weapon"]["hp"]
        value = shoot()
        require(value["weapon"]["last_hit_kind"] == 1 and value["weapon"]["last_damage"] == 0, "Native wall shot not adjudicated as world")
        require(fresh("join")["weapon"]["hp"] == before_hp, "Native wall shot damaged peer")
        time.sleep(.25)
        require(not sample()["weapon"]["hit_marker"], "World shot displayed player marker")
        # The creator slid around the corner to the east face. Put the peer
        # west of the block using a route around its north side. Validate the
        # actual ray geometry before calling this an occlusion test.
        require(sample()["local"]["predicted"][0] >= 11.24, "Occlusion fixture requires creator on east face")
        capture("join"); aim(math.pi, role="join")
        desktop.key(b, "a", True)
        time.sleep(2 / 3)
        desktop.key(b, "a", False)
        desktop.key(b, "w", True)
        time.sleep(.6)
        desktop.key(b, "w", False)
        target_position = fresh("join")["local"]["predicted"]
        require(target_position[0] < 8.7 and 9.2 < target_position[1] < 10.8, "Native peer did not reach west side of the wall")
        position = sample()["local"]["predicted"]
        crossing_t = (11 - position[0]) / (target_position[0] - position[0])
        crossing_z = position[1] + crossing_t * (target_position[1] - position[1])
        require(0 < crossing_t < 1 and 9 < crossing_z < 11, "Native shot fixture ray does not intersect center wall")
        aim(math.atan2(target_position[0] - position[0], target_position[1] - position[1]))
        occluded = shoot()
        require(occluded["weapon"]["last_hit_kind"] == 1 and occluded["weapon"]["last_damage"] == 0 and
                fresh("join")["weapon"]["hp"] == before_hp, "Native shot through wall damaged the occluded peer")
        record("native-peer-occluded-by-wall", target_position=target_position)
        # Deliberate miss points upward beyond player geometry; bounded arena
        # may still return a world hit, but must never damage the player.
        aim(-math.pi / 2, -1.1)
        shoot()
        require(fresh("join")["weapon"]["hp"] == before_hp, "Deliberate native miss damaged peer")
        result["checks"]["V5_native_wall_occluded_peer_and_deliberate_miss_no_damage"] = True
        record("wall-and-miss")
        desktop.focus(a)
        result["captures"].append(screenshot(directory, "creator-wall-hud", a))
        # Native focus while W is physically held must release game capture and
        # prevent persistent movement; release the physical key after focusing.
        capture(); aim(0)
        before_shots = sample()["weapon"]["submitted"]
        desktop.key(a, "w", True)
        time.sleep(.15)
        desktop.focus(b)
        wait_state(directory, "create", lambda s: not s["weapon"]["captured"] and not s["focused"], "Native focus loss retained capture")
        # Release without refocusing A (key-up has no gameplay side effect).
        key = desktop.x.XKeysymToKeycode(desktop.display, desktop.x.XStringToKeysym(b"w"))
        desktop.t.XTestFakeKeyEvent(desktop.display, key, 0, 0); desktop.keys.discard(key); desktop.flush()
        focus_stop = fresh()["local"]["predicted"]
        time.sleep(.3)
        require(math.dist(focus_stop, sample()["local"]["predicted"]) < .02, "Focus loss left held movement")
        desktop.focus(a); capture()
        desktop.tap(a, "Tab")
        require(not fresh()["weapon"]["captured"], "Native Tab did not release capture")
        move = desktop.drag(a)
        record("native-WM-titlebar-drag", drag=move)
        resized = desktop.drag(a, resize=True)
        after = fresh()
        require(not after["weapon"]["captured"] and after["weapon"]["submitted"] == before_shots, "Native decoration gesture captured pointer or fired")
        require(gui["create"].poll() is None and gui["join"].poll() is None, "Client exited during native WM drag")
        result["checks"]["V7_native_focus_tab_titlebar_resize"] = {"drag": move, "resize": resized}
        record("native-WM-titlebar-drag-resize", drag=move, resize=resized)
        result["captures"].append(screenshot(directory, "resized-actual-hud", a))
        # Exit both via native Escape; explicit native Refresh reconciles Lobby.
        for role in ("join", "create"):
            desktop.tap(ids[role], "Escape")
            wait_state(directory, role, lambda s: s["phase"] == 0 and not s["weapon"]["active"], "Both native Escapes did not leave")
        desktop.tap(a, "Return")
        empty = wait_state(directory, "create", lambda s: s["phase"] == 0 and s["rooms"] and s["rooms"][0]["players"] == 0, "Native Lobby occupancy was not zero")
        result["captures"].append(screenshot(directory, "empty-lobby-ui", a))
        record("both-left-zero-lobby")
        desktop.focus(b)
        desktop.key(b, "Alt_L", True)
        desktop.key(b, "F4", True)
        time.sleep(.08)
        # After F4 the target may be gone: release physical keys directly.
        for name in ("F4", "Alt_L"):
            code = desktop.x.XKeysymToKeycode(desktop.display, desktop.x.XStringToKeysym(name.encode()))
            desktop.t.XTestFakeKeyEvent(desktop.display, code, 0, 0); desktop.keys.discard(code)
        desktop.flush()
        require(gui["join"].wait(timeout=5) == 0, "Native close did not exit cleanly")
        result["checks"]["V8_both_leave_lobby_zero_native_close"] = True
        (directory / "create-native-stop").write_text("Observer complete\n")
        require(gui["create"].wait(timeout=5) == 0, "Creator observer did not exit cleanly")
        for role in ("create", "join"):
            frames = [json.loads(line) for line in (directory / f"{role}-native-frames.jsonl").read_text().splitlines()]
            require(frames and all(s["local"]["pending"] <= 12 for s in frames), "Native frame window violated bound")
        result["passed"] = True
    except Exception as error:
        result["error"] = str(error)
    finally:
        if desktop:
            desktop.close()
        for role in ("create", "join"):
            (directory / f"{role}-native-stop").write_text("Runner cleanup\n")
        # Allow passive probes to flush bounded evidence before forced cleanup.
        for process in reversed(processes):
            if process.poll() is None:
                try:
                    process.wait(timeout=2 if process in list(locals().get("gui", {}).values()) else .1)
                except subprocess.TimeoutExpired:
                    process.terminate()
        for process in reversed(processes):
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait(timeout=3)
        for log in logs:
            log.close()
        result["binary_and_asset_hashes"] = {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in (args.match, args.gateway, args.gui_probe, args.arena, args.arena_root / "asset_catalog.json")}
        (directory / "native-window-result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"passed": result["passed"], "error": result.get("error"), "checks": list(result["checks"])}), flush=True)
    return 0 if result["passed"] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("match", "gateway", "gui-probe", "arena", "arena-root", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--gpu-driver", default="vulkan")
    parser.add_argument("--window-only", action="store_true", help="Bounded native geometry diagnostic, not complete V1-V8 acceptance")
    args = parser.parse_args()
    for name in ("match", "gateway", "gui_probe", "arena", "arena_root", "output"):
        setattr(args, name, getattr(args, name).resolve())
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main())
