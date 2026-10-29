"""UDP socket error probe (os-subsystem os-net-errors evidence, D73; record only, never a pass condition).

Records which error, if any, the operating system reports on a UDP socket after a datagram is sent to
a closed port. It uses the interpreter's socket module, so it observes the operating system's own
socket calls; asio's mapping of those errors is not exercised. Everything runs on the loopback
interface, so it needs neither a network nor a person.

Scenarios:
  unconnected   an unconnected socket (as the consumer's client uses it: send_to and receive_from) sends one
                datagram to a closed loopback port, then receives
  usable-after  the same socket then exchanges a datagram with an echo socket
  repeated      a fresh unconnected socket sends three datagrams to the closed port, then receives
                until nothing more arrives
  connected     a connected socket sends to the closed port, receives, then sends again (comparison
                only; the consumer's client does not connect its socket)

Usage (from the repository root):
  python3 docs/architecture/plans/os-subsystem/scripts/udp_error_probe.py --output <file.json>
Exit status: 0 when every scenario ran, 1 when one could not run, 2 on a usage error.
"""
import argparse
import datetime
import errno
import json
import platform
import select
import socket
import sys
from pathlib import Path

LOOPBACK = "127.0.0.1"
WAIT_SECONDS = 1.0
DRAIN_LIMIT = 10


def describe(error):
    return {"kind": "error", "type": type(error).__name__, "errno": error.errno,
            "errno_name": errno.errorcode.get(error.errno, ""), "winerror": getattr(error, "winerror", None),
            "message": error.strerror}


def udp_socket():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((LOOPBACK, 0))
    sock.setblocking(False)
    return sock


def closed_port():
    sock = udp_socket()
    port = sock.getsockname()[1]
    sock.close()
    return port


def send(sock, payload, address=None):
    try:
        sent = sock.send(payload) if address is None else sock.sendto(payload, address)
        return {"kind": "sent", "bytes": sent}
    except OSError as error:
        return describe(error)


def receive(sock):
    readable, _, _ = select.select([sock], [], [], WAIT_SECONDS)
    if not readable:
        return {"kind": "nothing", "within_s": WAIT_SECONDS}
    try:
        data, sender = sock.recvfrom(2048)
        return {"kind": "datagram", "bytes": len(data), "from_port": sender[1]}
    except OSError as error:
        return describe(error)


def unconnected_and_usable_after(port):
    sock, echo = udp_socket(), udp_socket()
    try:
        first = {"send": send(sock, b"probe", (LOOPBACK, port)), "receive": receive(sock)}
        ping = send(sock, b"ping", echo.getsockname())
        heard = receive(echo)
        pong = send(echo, b"pong", sock.getsockname()) if heard["kind"] == "datagram" else None
        after = {"send": ping, "echo_receive": heard, "echo_send": pong, "receive": receive(sock)}
        return first, after
    finally:
        sock.close()
        echo.close()


def repeated(port):
    sock = udp_socket()
    try:
        sends = [send(sock, b"probe", (LOOPBACK, port)) for _ in range(3)]
        receives = []
        while len(receives) < DRAIN_LIMIT:
            receives.append(receive(sock))
            if receives[-1]["kind"] == "nothing":
                break
        return {"sends": sends, "receives": receives}
    finally:
        sock.close()


def connected(port):
    sock = udp_socket()
    try:
        sock.connect((LOOPBACK, port))
        return {"send": send(sock, b"probe"), "receive": receive(sock), "send_again": send(sock, b"probe")}
    finally:
        sock.close()


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    port = closed_port()
    scenarios, failures = {}, []
    steps = [("unconnected+usable-after", lambda: unconnected_and_usable_after(port)),
             ("repeated", lambda: repeated(port)), ("connected", lambda: connected(port))]
    for name, step in steps:
        try:
            result = step()
        except OSError as error:
            failures.append(name)
            scenarios[name] = {"could_not_run": describe(error)}
            continue
        if name == "unconnected+usable-after":
            scenarios["unconnected"], scenarios["usable-after"] = result
        else:
            scenarios[name] = result
    record = {
        "recorded": datetime.datetime.now().isoformat(timespec="seconds"),
        "os": f"{platform.platform()} ({platform.machine()})",
        "python": platform.python_version(),
        "observes": "the interpreter's socket module on the loopback interface; asio's mapping is not exercised",
        "closed_port": port,
        "wait_s": WAIT_SECONDS,
        "scenarios": scenarios,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(record, indent=1) + "\n", encoding="utf-8")
    for name, result in scenarios.items():
        print(f"{name}: {json.dumps(result)}")
    print(f"udp error probe: recorded in {args.output}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
