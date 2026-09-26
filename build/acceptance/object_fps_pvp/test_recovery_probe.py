"""Owner-local regression for deterministic fixed-RTT wire scheduling."""
import heapq
import threading
import unittest
from unittest.mock import patch

from recovery_probe import FixedRTTGateway


def packet(kind, sequence):
    result = bytearray(24)
    result[:4] = b'GYOP'
    result[4:6] = (3).to_bytes(2, 'big')
    result[6:8] = kind.to_bytes(2, 'big')
    result[8:16] = (1).to_bytes(8, 'big')
    result[16:20] = sequence.to_bytes(4, 'big')
    return bytes(result)


class FixedRTTScheduling(unittest.TestCase):
    def relay(self, rtt):
        # Exercise the exact scheduling method without binding sockets or
        # starting threads; the real process matrix separately verifies I/O.
        relay = FixedRTTGateway.__new__(FixedRTTGateway)
        relay.lock = threading.Lock()
        relay.rtt_ms = rtt
        relay.upstream_udp = ('127.0.0.1', 3000)
        relay.clients = {1: ('127.0.0.1', 4000)}
        relay.queue, relay.order = [], 0
        relay.stats = {'maximum_datagram_bytes': 0, 'maximum_queued_datagrams': 0,
                       'input': relay._new_counts(), 'snapshot': relay._new_counts(),
                       'control': relay._new_counts()}
        return relay

    def test_hello_cannot_overtake_prior_input(self):
        for rtt in (0, 20, 40):
            with self.subTest(rtt=rtt):
                relay = self.relay(rtt)
                with patch('recovery_probe.time.monotonic', side_effect=[10, 10.001]):
                    relay._receive(packet(3, 10), relay.clients[1])
                    relay._receive(packet(1, 11), relay.clients[1])
                first, second = heapq.heappop(relay.queue), heapq.heappop(relay.queue)
                self.assertEqual(first[2], packet(3, 10))
                self.assertEqual(second[2], packet(1, 11))
                self.assertAlmostEqual(first[0], 10+rtt/2000)
                self.assertAlmostEqual(second[0], 10.001+rtt/2000)
                self.assertEqual(relay.stats['control']['minimum_scheduled_delay_ms'], rtt/2)

    def test_welcome_cannot_overtake_prior_snapshot(self):
        relay = self.relay(40)
        # Equal receive timestamps must still preserve insertion order.
        with patch('recovery_probe.time.monotonic', return_value=10):
            relay._receive(packet(4, 20), relay.upstream_udp)
            relay._receive(packet(2, 21), relay.upstream_udp)
        first, second = heapq.heappop(relay.queue), heapq.heappop(relay.queue)
        self.assertEqual(first[2], packet(4, 20))
        self.assertEqual(second[2], packet(2, 21))
        self.assertEqual(first[0], second[0])
        self.assertEqual(relay.stats['control']['maximum_scheduled_delay_ms'], 20)


if __name__ == '__main__':
    unittest.main()
