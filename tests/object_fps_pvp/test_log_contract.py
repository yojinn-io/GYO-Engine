"""Guards of the log Data Contract v1 (docs/object_fps_pvp/log-contract.zh-Hant.md).

1. Writer pins: for every A-grade family in fixtures/log_v1/keys-v1.tsv, the keys of the writer's statement (read from
   the C++ or Go source) start with the registered v1 key sequence: within v1 keys are only appended at the end.
2. Frozen readers: the repository's copies of the analyzers the L2 sessions freeze (network_statistics.py,
   ingress_evidence.py, command_evidence.py) read real logs of the current writers (fixtures/log_v1/sample-*) without a
   ContractStop or a ValueError, and find every v1 key.
3. Reverse guards: a line whose meaning changed under a new event name is not read as v1 (rule R2), and the guard
   itself fails on a broken sample (a string value appended to a worker line, two ingress keys swapped).
Standard library only; run by CTest object_fps_pvp.log_contract.
"""
import importlib.util
import re
import sys
import unittest
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
FIXTURES = Path(__file__).resolve().parent / 'fixtures' / 'log_v1'
ACCEPTANCE = REPOSITORY / 'build' / 'acceptance' / 'object_fps_pvp'
KEY = re.compile(r'([A-Za-z_][A-Za-z0-9_]*)=')


def load(name):
    spec = importlib.util.spec_from_file_location(f'log_contract_{name}', ACCEPTANCE / f'{name}.py')
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def registry():
    rows = []
    for line in (FIXTURES / 'keys-v1.tsv').read_text(encoding='utf-8').splitlines():
        if line and not line.startswith('#'):
            family, language, path, source_anchor, line_anchor, keys = line.split('\t')
            rows.append({'family': family, 'language': language, 'path': REPOSITORY / path,
                         'source_anchor': source_anchor, 'line_anchor': line_anchor, 'keys': keys.split()})
    return rows


def statement_literals(text, quote, language):
    """The string literals of the statement whose literal opens at index quote: for Go up to the end of the format
    argument (a ',' or ')' outside strings and nested parentheses), for C++ up to the ';' ending the statement."""
    pieces, depth, i = [], 0, quote
    while i < len(text):
        c = text[i]
        if c == '"':
            j, piece = i + 1, []
            while text[j] != '"':
                if text[j] == '\\':
                    piece.append(text[j:j + 2])
                    j += 2
                    continue
                piece.append(text[j])
                j += 1
            pieces.append(''.join(piece))
            i = j + 1
            continue
        if c == "'":   # a C++ character literal
            i = text.index("'", i + 2 if text[i + 1] == '\\' else i + 1) + 1
            continue
        if language == 'go':
            if c == '(':
                depth += 1
            elif c == ')':
                if depth == 0:
                    break
                depth -= 1
            elif c == ',' and depth == 0:
                break
        elif c == ';':
            break
        i += 1
    return ''.join(pieces)


def writer_keys(row):
    text = row['path'].read_text(encoding='utf-8')
    at = text.find(row['source_anchor'])
    if at < 0:
        return None
    joined = statement_literals(text, text.rfind('"', 0, at + 1), row['language'])
    return KEY.findall(joined[joined.find(row['source_anchor']):])


class WriterPins(unittest.TestCase):
    def test_every_family_starts_with_its_v1_keys(self):
        rows = registry()
        self.assertGreaterEqual(len(rows), 18)
        for row in rows:
            if row['language'] == 'native':
                continue
            with self.subTest(family=row['family']):
                keys = writer_keys(row)
                self.assertIsNotNone(keys, f"{row['path']}: the statement starting {row['source_anchor']!r} is gone")
                self.assertEqual(keys[:len(row['keys'])], row['keys'],
                                 f"{row['family']}: v1 keys {row['keys']} are not a prefix of the writer's {keys}")

    def test_the_line_anchor_is_what_the_readers_match(self):
        evidence = (ACCEPTANCE / 'ingress_evidence.py').read_text(encoding='utf-8')
        statistics = (ACCEPTANCE / 'network_statistics.py').read_text(encoding='utf-8')
        for anchor in ('gateway statistics players=', 'player send statistics ', 'Object FPS Gateway start executable_sha256=',
                       'Object FPS Gateway HTTP=', 'player evicted player=', '[ObjectFPS/PvP Match] start executable_sha256=',
                       'Object_FPS_PVP Match ready: ', '[ObjectFPS/PvP Match] ipc accept failed'):
            self.assertIn(anchor, [r['line_anchor'] for r in registry()])
            self.assertTrue(anchor.replace('[', '\\[').replace(']', '\\]') in evidence or anchor in evidence
                            or anchor.replace('[', '\\[').replace(']', '\\]') in statistics or anchor in statistics,
                            f'no reader matches {anchor!r}')

    def test_the_pin_reads_concatenated_and_streamed_statements(self):
        go = 'log.Printf("a b x=%d y=%s "+\n\t\t"z=%d", f(1, 2), "w=%d")'
        self.assertEqual(KEY.findall(statement_literals(go, go.index('"'), 'go')), ['x', 'y', 'z'])
        cpp = 'std::clog<<"p q x="<<f(a, b)<<" y="<<g<<\'\\n\';\nother("z=");'
        self.assertEqual(KEY.findall(statement_literals(cpp, cpp.index('"'), 'cpp')), ['x', 'y'])


class FrozenReaders(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.statistics = load('network_statistics')
        cls.evidence = load('ingress_evidence')
        cls.commands = load('command_evidence')

    def text(self, name):
        return (FIXTURES / name).read_text(encoding='utf-8')

    def test_network_statistics_reads_every_window(self):
        s = self.statistics
        for name, pattern, family in (('sample-match.log', s.MATCH, 'match_network_statistics'),
                                      ('sample-gateway.log', s.GATEWAY, 'gateway_player_send_statistics'),
                                      ('sample-probe.log', s.WORKER, 'client_worker_statistics')):
            rows = s.windows(self.text(name), pattern)
            self.assertTrue(rows, name)
            keys = next(r['keys'] for r in registry() if r['family'] == family)
            for row in rows:
                self.assertEqual(list(row)[:len(keys)], keys, name)

    def test_ingress_evidence_reads_without_a_contract_stop(self):
        result = self.evidence.analyze_text(self.text('sample-gateway.log'), self.text('sample-match.log'),
                                            self.text('sample-match-ingress.jsonl'))
        self.assertNotEqual(result.get('status'), 'stopped', result.get('stops'))
        self.assertTrue(result['passed'], {k: v.get('status') for k, v in result.get('checks', {}).items()})
        self.assertEqual(result['totals']['gateway']['unknown_keys'], [])
        self.assertEqual(result['totals']['match']['unknown_keys'], [])

    def test_command_evidence_reads_the_movement_trace(self):
        events = self.commands.read_trace_events(FIXTURES / 'sample-commands.jsonl')
        self.assertTrue(events)


class ReverseGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.statistics = load('network_statistics')
        cls.evidence = load('ingress_evidence')

    def test_a_renamed_event_is_not_read_as_v1(self):
        # R2: a breaking change of a family without a version key takes a new event name, so v1 readers find nothing.
        line = (FIXTURES / 'sample-match.log').read_text(encoding='utf-8').splitlines()
        v2 = '\n'.join(x.replace('] network statistics ', '] network statistics.v2 ') for x in line)
        self.assertEqual(self.statistics.windows(v2, self.statistics.MATCH), [])

    def test_a_string_value_appended_to_a_worker_line_breaks_the_frozen_reader(self):
        text = (FIXTURES / 'sample-probe.log').read_text(encoding='utf-8')
        broken = text.replace('other_version_datagrams=0', 'other_version_datagrams=0 backend=kqueue', 1)
        self.assertNotEqual(broken, text)
        with self.assertRaises(ValueError):
            self.statistics.windows(broken, self.statistics.WORKER)

    def test_swapped_ingress_keys_stop_the_frozen_reader(self):
        gateway = (FIXTURES / 'sample-gateway.log').read_text(encoding='utf-8')
        swapped = re.sub(r'(gateway ingress statistics version=1 window_ms=\d+ final=\d+ )(received_datagrams=\d+) (\S+)',
                         r'\1\3 \2', gateway, count=1)
        self.assertNotEqual(swapped, gateway)
        result = self.evidence.analyze_text(swapped, (FIXTURES / 'sample-match.log').read_text(encoding='utf-8'))
        self.assertEqual(result.get('status'), 'stopped')


if __name__ == '__main__':
    unittest.main()
