"""Checks of engine/os Console through gyo_os_console_probe (docs/architecture/os.md, Console).

--check arguments  Every platform: arguments outside any ANSI code page and with spaces, quotes, backslashes, tabs
                   and an empty one come back from CommandLineUtf8 as the same UTF-8.
--check shutdown   POSIX: the handlers carry the sa_flags std::signal sets on this C library (SA_RESTART and the
                   rest, as before engine/os); SIGINT and SIGTERM each end the probe's wait with their reason; the
                   same signal again and then the other neither end the process nor change the reason (the
                   handlers stay installed and the first request wins). Windows: Ctrl+Break sent to a new process
                   group is a break request; a second Ctrl+Break ends the process at once.
--check ctrl-c     Windows: Ctrl+C sent to the probe's own console is an interrupt request; a second Ctrl+C ends
                   the process at once, as when the C runtime handled SIGINT.
--check close      Windows: closing the probe's own console is reported as console_close.

Exit status 0 when the check holds, 1 when it fails, 77 when this runner cannot run it (the reason is printed;
CTest reports the test as skipped). Standard library only; every wait has a timeout.
Usage: python3 console_check.py --probe PATH --check arguments|shutdown|close
"""
import argparse
import os
import queue
import signal
import subprocess
import sys
import threading
import time

SKIP = 77
WAIT_SECONDS = 20


class Probe:
    """A running probe whose output lines are read on a thread, so that no read blocks forever."""

    def __init__(self, path, *mode, **kwargs):
        self.process = subprocess.Popen([path, *mode], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                        stdin=subprocess.DEVNULL, **kwargs)
        self.lines = queue.Queue()
        self.output = []
        threading.Thread(target=self._read, daemon=True).start()

    def _read(self):
        for raw in self.process.stdout:
            line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
            self.output.append(line)
            self.lines.put(line)
        self.lines.put(None)

    def wait_line(self, wanted):
        deadline = time.monotonic() + WAIT_SECONDS
        while time.monotonic() < deadline:
            try:
                line = self.lines.get(timeout=max(0.0, deadline - time.monotonic()))
            except queue.Empty:
                break
            if line is None:
                return False
            if line == wanted:
                return True
        return False

    def finish(self):
        try:
            code = self.process.wait(timeout=WAIT_SECONDS)
        except subprocess.TimeoutExpired:
            self.process.kill()
            code = self.process.wait(timeout=WAIT_SECONDS)
        time.sleep(0.1)
        return code, list(self.output)


def fail(message):
    print(f"console check: FAIL: {message}")
    return 1


def check_arguments(probe):
    cases = ["日本語の路徑", "é", "한글", "emoji-\U0001F600", "a b", 'quote"inside', "back\\slash\\", "tab\there", ""]
    done = subprocess.run([probe, "--echo-arguments", *cases], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          timeout=WAIT_SECONDS)
    lines = done.stdout.decode("utf-8", errors="replace").splitlines()
    if done.returncode != 0:
        return fail(f"probe exited {done.returncode}: {lines}")
    got = [line[len("arg="):] for line in lines if line.startswith("arg=")]
    expected = [case.encode("utf-8").hex() for case in cases]
    if got != expected:
        return fail(f"arguments came back as {got}, expected {expected}")
    print(f"console check: {len(cases)} arguments came back as the same UTF-8")
    return 0


def check_windows_repeat(reason, start, send):
    """One request ends the wait with reason; a second one ends the process at once (the default handling).

    start(count) starts a probe that is to receive count requests; send(running, count) sends them, if the probe
    does not send them itself."""
    once = start(1)
    if not once.wait_line("ready"):
        once.process.kill()
        return fail(f"probe never got ready: {once.finish()}")
    try:
        send(once, 1)
    except OSError as error:
        once.process.kill()
        once.finish()
        print(f"console check: skipped, this runner cannot send the console event: {error}")
        return SKIP
    code, output = once.finish()
    if code == 5:
        print(f"console check: skipped, the probe cannot send the console event on this runner: {output}")
        return SKIP
    if code != 0 or f"requested reason={reason}" not in output:
        return fail(f"{reason} once: expected exit 0 with 'requested reason={reason}', got {code}: {output}")
    twice = start(2)
    if not twice.wait_line("ready"):
        twice.process.kill()
        return fail(f"probe never got ready: {twice.finish()}")
    send(twice, 2)
    code, output = twice.finish()
    if code == 0 or any(line.startswith("requested reason=") for line in output):
        return fail(f"{reason} twice: the second request must end the process at once, got {code}: {output}")
    print(f"console check: {reason} requested; a second one ended the process (exit {code:#x})")
    return 0


def check_shutdown(probe):
    if os.name == "nt":
        def send(running, count):
            for _ in range(count):
                os.kill(running.process.pid, signal.CTRL_BREAK_EVENT)
                time.sleep(0.05)
        return check_windows_repeat(
            "break", lambda count: Probe(probe, "--shutdown", creationflags=subprocess.CREATE_NEW_PROCESS_GROUP), send)
    else:
        interrupt = lambda p: p.send_signal(signal.SIGINT)
        terminate = lambda p: p.send_signal(signal.SIGTERM)
        # The first signal again (a reset handler would end the process), then the other (a later request
        # must not replace the first one's reason).
        rounds = [("interrupt", [interrupt, interrupt, terminate]), ("terminate", [terminate, terminate, interrupt])]
    for reason, requests in rounds:
        running = Probe(probe, "--shutdown")
        if not running.wait_line("ready"):
            running.process.kill()
            return fail(f"probe never got ready: {running.finish()}")
        flags = dict(line.split("=", 1) for line in running.output if line.endswith(tuple("0123456789abcdefx"))
                     and line.split("=", 1)[0] in ("reference_flags", "sigint_flags", "sigterm_flags"))
        if not (flags.get("reference_flags") and flags.get("sigint_flags") == flags["reference_flags"]
                and flags.get("sigterm_flags") == flags["reference_flags"]):
            running.process.kill()
            running.finish()
            return fail(f"the handlers' sa_flags differ from what std::signal sets on this C library: {flags}")
        try:
            for request in requests:
                request(running.process)
                time.sleep(0.05)
        except OSError as error:
            running.process.kill()
            running.finish()
            print(f"console check: skipped, this runner cannot send the console event: {error}")
            return SKIP
        code, output = running.finish()
        if code != 0:
            return fail(f"{reason}: probe exited {code} (a later request must not end it): {output}")
        if f"requested reason={reason}" not in output:
            return fail(f"{reason}: expected 'requested reason={reason}' (the first request wins), got {output}")
        print(f"console check: {reason} requested; later requests kept the process and the reason")
    return 0


def check_ctrl_c(probe):
    if os.name != "nt":
        print("console check: skipped, Ctrl+C to a console is a Windows event (POSIX SIGINT is --check shutdown)")
        return SKIP
    # The probe sends Ctrl+C to its own new console, which no other process shares.
    return check_windows_repeat(
        "interrupt",
        lambda count: Probe(probe, "--ctrl-c-own-console", str(count), creationflags=subprocess.CREATE_NEW_CONSOLE),
        lambda running, count: None)


def check_close(probe):
    if os.name != "nt":
        print("console check: skipped, closing a console window is a Windows event")
        return SKIP
    running = Probe(probe, "--close-own-console", creationflags=subprocess.CREATE_NEW_CONSOLE)
    if not running.wait_line("ready"):
        running.process.kill()
        return fail(f"probe never got ready: {running.finish()}")
    code, output = running.finish()
    if code == 5:
        print(f"console check: skipped, closing the probe's console did not reach it on this runner: {output}")
        return SKIP
    if code != 0 or "requested reason=console_close" not in output:
        return fail(f"expected exit 0 with 'requested reason=console_close', got {code}: {output}")
    print("console check: closing the console was a console_close request, and the program finished first")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    parser.add_argument("--check", required=True, choices=("arguments", "shutdown", "ctrl-c", "close"))
    args = parser.parse_args(argv)
    checks = {"arguments": check_arguments, "shutdown": check_shutdown, "ctrl-c": check_ctrl_c, "close": check_close}
    return checks[args.check](args.probe)


if __name__ == "__main__":
    sys.exit(main())
