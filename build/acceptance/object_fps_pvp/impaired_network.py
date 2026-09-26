"""Product acceptance wire impairment; never imported by a shipped product.

The HTTP forwarder redirects successful joins to a loopback UDP relay. The
relay preserves every datagram byte and routes replies by the existing session
header. Only movement inputs and snapshots are impaired; no world state or
Protobuf data is generated here.
"""

from contextlib import contextmanager
import heapq
import http.client
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import random
import select
import socket
import threading
import time


_HOP_HEADERS = {"connection", "keep-alive", "proxy-authenticate", "proxy-authorization",
                "te", "trailer", "transfer-encoding", "upgrade", "content-length", "host"}


class _ImpairedGateway:
    def __init__(self, http_port, udp_port, rtt_ms):
        self.upstream_http = http_port
        self.upstream_udp = ("127.0.0.1", udp_port)
        self.rtt_ms = rtt_ms
        self.stop = threading.Event()
        self.lock = threading.Lock()
        self.error = None
        self.clients = {}
        self.flows = {}
        self.queue = []
        self.order = 0
        self.stats = {
            "configured_rtt_ms": rtt_ms,
            "one_way_jitter_ms": 10,
            "random_loss_probability": 0.05,
            "forced_input_loss_batches_per_session": [1, 61, 62],
            "http_join_redirects": 0,
            "maximum_datagram_bytes": 0,
            "control_forwarded": 0,
            "unroutable": 0,
            "queue_discarded_on_close": 0,
            "input": self._new_counts(),
            "snapshot": self._new_counts(),
        }
        self.udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.http = None
        self.http_thread = None
        self.udp_thread = None
        try:
            self.udp.bind(("127.0.0.1", 0))
            self.udp.setblocking(False)
            owner = self

            class Handler(BaseHTTPRequestHandler):
                protocol_version = "HTTP/1.0"

                def setup(self):
                    self.request.settimeout(3)
                    super().setup()

                def log_message(self, *_):
                    pass

                def forward(self):
                    upstream = http.client.HTTPConnection("127.0.0.1", owner.upstream_http, timeout=3)
                    try:
                        length = int(self.headers.get("Content-Length", "0"))
                        body = self.rfile.read(length) if length else None
                        headers = {key: value for key, value in self.headers.items()
                                   if key.lower() not in _HOP_HEADERS}
                        upstream.request(self.command, self.path, body=body, headers=headers)
                        response = upstream.getresponse()
                        payload = response.read()
                        if (self.command == "POST" and self.path.endswith("/join") and
                                200 <= response.status < 300):
                            joined = json.loads(payload)
                            joined["udp_ip"] = "127.0.0.1"
                            joined["udp_port"] = owner.udp.getsockname()[1]
                            payload = json.dumps(joined).encode("utf-8")
                            with owner.lock:
                                owner.stats["http_join_redirects"] += 1
                        self.send_response(response.status, response.reason)
                        for key, value in response.getheaders():
                            if key.lower() not in _HOP_HEADERS:
                                self.send_header(key, value)
                        self.send_header("Content-Length", str(len(payload)))
                        self.send_header("Connection", "close")
                        self.end_headers()
                        self.wfile.write(payload)
                    except (OSError, ValueError, http.client.HTTPException) as failure:
                        owner._fail(f"HTTP relay: {failure}")
                        try:
                            self.send_error(502, "Acceptance relay could not forward the request")
                        except OSError:
                            pass
                    finally:
                        upstream.close()
                        self.close_connection = True

                do_GET = forward
                do_POST = forward
                do_DELETE = forward

            self.http = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
            self.http.daemon_threads = False
            self.http.block_on_close = True
            self.http_thread = threading.Thread(target=lambda: self.http.serve_forever(poll_interval=0.02),
                                                name="pvp-impairment-http")
            self.udp_thread = threading.Thread(target=self._relay, name="pvp-impairment-udp")
            self.http_thread.start()
            self.udp_thread.start()
        except BaseException:
            self.close()
            raise

    @staticmethod
    def _new_counts():
        return {"received": 0, "forwarded": 0, "dropped": 0, "received_bytes": 0, "forwarded_bytes": 0,
                "dropped_random": 0, "dropped_first": 0, "dropped_burst": 0,
                "minimum_scheduled_delay_ms": None, "maximum_scheduled_delay_ms": 0,
                "maximum_observed_delay_ms": 0}

    @property
    def gateway(self):
        return f"127.0.0.1:{self.http.server_port}"

    def _fail(self, error):
        with self.lock:
            if self.error is None:
                self.error = str(error)

    def _receive(self, payload, source):
        if len(payload) < 24:
            with self.lock:
                self.stats["unroutable"] += 1
            return
        kind = int.from_bytes(payload[6:8], "big")
        session = int.from_bytes(payload[8:16], "big")
        from_gateway = source == self.upstream_udp
        if from_gateway:
            destination = self.clients.get(session)
            if destination is None:
                with self.lock:
                    self.stats["unroutable"] += 1
                return
        else:
            if session not in self.clients:
                index = len(self.clients)
                self.flows[session] = {
                    "input_count": 0,
                    "input_random": random.Random(0x47594F + index * 2),
                    "snapshot_random": random.Random(0x475950 + index * 2),
                }
            self.clients[session] = source
            destination = self.upstream_udp
        direction = "snapshot" if from_gateway and kind == 4 else (
            "input" if not from_gateway and kind == 3 else None)
        now = time.monotonic()
        with self.lock:
            self.stats["maximum_datagram_bytes"] = max(self.stats["maximum_datagram_bytes"], len(payload))
            if direction is None:
                self.udp.sendto(payload, destination)
                self.stats["control_forwarded"] += 1
                return
            counts = self.stats[direction]
            counts["received"] += 1
            counts["received_bytes"] += len(payload)
            flow = self.flows[session]
            generator = flow[direction + "_random"]
            random_drop = generator.random() < 0.05
            reason = None
            if direction == "input":
                flow["input_count"] += 1
                if flow["input_count"] == 1:
                    reason = "first"
                elif flow["input_count"] in (61, 62):
                    reason = "burst"
            if reason is None and random_drop:
                reason = "random"
            if reason:
                counts["dropped"] += 1
                counts["dropped_" + reason] += 1
                return
            delay_ms = max(0, self.rtt_ms / 2 + generator.uniform(-10, 10))
            previous = counts["minimum_scheduled_delay_ms"]
            counts["minimum_scheduled_delay_ms"] = delay_ms if previous is None else min(previous, delay_ms)
            counts["maximum_scheduled_delay_ms"] = max(counts["maximum_scheduled_delay_ms"], delay_ms)
        self.order += 1
        heapq.heappush(self.queue, (now + delay_ms / 1000, self.order, payload,
                                    destination, direction, now))

    def _relay(self):
        try:
            while not self.stop.is_set():
                now = time.monotonic()
                while self.queue and self.queue[0][0] <= now:
                    _, _, payload, destination, direction, received = heapq.heappop(self.queue)
                    self.udp.sendto(payload, destination)
                    with self.lock:
                        counts = self.stats[direction]
                        counts["forwarded"] += 1
                        counts["forwarded_bytes"] += len(payload)
                        counts["maximum_observed_delay_ms"] = max(
                            counts["maximum_observed_delay_ms"], (now - received) * 1000)
                timeout = min(0.01, max(0, self.queue[0][0] - now)) if self.queue else 0.01
                readable, _, _ = select.select([self.udp], [], [], timeout)
                if readable:
                    payload, source = self.udp.recvfrom(65535)
                    self._receive(payload, source)
        except Exception as failure:
            self._fail(f"UDP relay: {failure}")

    def evidence(self):
        with self.lock:
            result = json.loads(json.dumps(self.stats))
            result["sessions"] = len(self.clients)
            result["relay_error"] = self.error
        return result

    def close(self):
        self.stop.set()
        if self.http is not None:
            if self.http_thread is not None and self.http_thread.is_alive():
                self.http.shutdown()
            self.http.server_close()
        for worker in (self.http_thread, self.udp_thread):
            if worker is not None and worker.ident is not None:
                worker.join(timeout=5)
                if worker.is_alive():
                    self._fail(f"Relay worker did not stop: {worker.name}")
        with self.lock:
            self.stats["queue_discarded_on_close"] += len(self.queue)
        self.queue.clear()
        self.udp.close()


@contextmanager
def impaired_gateway(http_port, udp_port, rtt_ms):
    """Yield a forwarding endpoint; close listeners and all workers on any exit."""
    proxy = _ImpairedGateway(http_port, udp_port, rtt_ms)
    try:
        yield proxy
    finally:
        proxy.close()
