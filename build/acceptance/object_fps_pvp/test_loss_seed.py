"""observability-tools OT-4 (INVENTORY E4): the network relays' loss seed is a parameter whose default reproduces the
generators the relays used before it was one."""
import random
import unittest

import action_probe
import impaired_network


def drops(generator, count=400, probability=0.05):
    return [i for i in range(count) if generator.random() < probability]


class LossSeedTests(unittest.TestCase):
    def test_default_reproduces_the_former_constants(self):
        for index in range(4):
            inputs, snapshots = impaired_network.flow_generators(impaired_network.DEFAULT_LOSS_SEED, index)
            self.assertEqual(drops(inputs), drops(random.Random(0x47594F + index * 2)))
            self.assertEqual(drops(snapshots), drops(random.Random(0x475950 + index * 2)))
            self.assertEqual(drops(action_probe.network_generator(impaired_network.DEFAULT_LOSS_SEED, index)),
                             drops(random.Random(0x47594F + index)))

    def test_another_seed_drops_elsewhere(self):
        base = drops(impaired_network.flow_generators(impaired_network.DEFAULT_LOSS_SEED, 0)[0])
        other = drops(impaired_network.flow_generators(12345, 0)[0])
        self.assertTrue(base and other)
        self.assertNotEqual(base, other)
        self.assertNotEqual(drops(action_probe.network_generator(12345, 0)),
                            drops(action_probe.network_generator(impaired_network.DEFAULT_LOSS_SEED, 0)))

    def test_the_seed_reaches_the_evidence(self):
        import threading
        from collections import Counter
        relay = action_probe.ActionRelay.__new__(action_probe.ActionRelay)
        relay.__dict__.update(mode="network20", loss_seed=7, lock=threading.Lock(), fault={}, events=[], players={},
                              error=None, network_maximum_scheduled_delay_ms=0, network_counts=Counter(),
                              network_queue=[])
        self.assertEqual(relay.evidence()["network"]["loss_seed"], 7)
        relay.mode = "baseline"
        self.assertIsNone(relay.evidence()["network"]["loss_seed"])
        # the impairments relay opens sockets in its constructor; its stats carry the seed it was given
        import inspect
        source = inspect.getsource(impaired_network._ImpairedGateway.__init__)
        self.assertIn('"loss_seed": loss_seed', source)
        self.assertIn("flow_generators(self.loss_seed, len(self.clients))", inspect.getsource(impaired_network._ImpairedGateway))

if __name__ == "__main__":
    unittest.main()
