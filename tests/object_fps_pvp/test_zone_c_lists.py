"""Position = zone (naming rule 4; v8-product-kernel-prep-zones): the product's zone-C source lists
agree with the Sim directory, and the confirmed list's scan has exactly the recorded residual.

Three lists (docs/architecture/target/zone-c-sources.zh-Hant.md, format v1) live here:
  zone-c-sources.json     the confirmed zone-C sources (audit section 1.3 "keep"), all in src/Sim
  zone-c-candidates.json  P9 (shot geometry and pose): zone (b) until DET-1 E1, also in src/Sim
  zone-c-rewrite.json     P1/P5/P6: their (c) core is rewritten into Sim by the kernel migration;
                          they stay in src/Pvp and are only reported
The scanner (tests/common/zone_scan/zone_scan.py) checks that every listed file exists; it cannot know
that a Sim file is missing from the lists, so this test pins sources + candidates == src/Sim/*.cpp, the
rewrite list's three members, and that no Sim file includes a Present header (Sim never depends on
the presentation; review 2026-11-12).

THE RESIDUAL BELOW RECORDS TODAY'S FINDINGS. IT PERMITS NOTHING. Each row is a known zone-C violation
the batch could not remove by moving files (declaration section 3 Q3): the P13 loaders take a
std::filesystem::path (the file effect itself is behind NativeFiles; splitting reading from parsing is
an API change for the migration batch), and AnimationSetDefinition's unordered_map waits for the
determinism line's digest check. A new finding, a missing one or a changed count fails this test.
"""
from collections import Counter
from pathlib import Path
import json
import re
import sys
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY / 'tests/common/zone_scan'))
import zone_scan  # noqa: E402

HERE = Path(__file__).resolve().parent
LISTS = {name: HERE / f'zone-c-{name}.json' for name in ('sources', 'candidates', 'rewrite')}
SIM = REPOSITORY / 'apps/object_fps_pvp/src/Sim'
SIM_INCLUDE = REPOSITORY / 'apps/object_fps_pvp/include/RetroFPS/Sim'
PVP = REPOSITORY / 'apps/object_fps_pvp/src/Pvp'
REWRITE = ['apps/object_fps_pvp/src/Pvp/ClientSimulation.cpp', 'apps/object_fps_pvp/src/Pvp/LocalPlayerPrediction.cpp',
           'apps/object_fps_pvp/src/Pvp/PvpMatch.cpp']

# (file, rule, text) of every finding the confirmed list's scan may report: 5 x ZS1 <filesystem> in the
# content loaders' signatures, 2 x ZS7 unordered_map in the animation set definition.
RESIDUAL = Counter({
    ('apps/object_fps_pvp/include/RetroFPS/Sim/Arena.hpp', 'ZS1', '<filesystem>'): 1,
    ('apps/object_fps_pvp/include/RetroFPS/Sim/MovementRulesFile.hpp', 'ZS1', '<filesystem>'): 1,
    ('apps/object_fps_pvp/include/RetroFPS/Sim/PlayerAnimationContent.hpp', 'ZS1', '<filesystem>'): 1,
    ('apps/object_fps_pvp/include/RetroFPS/Sim/PlayerPose.hpp', 'ZS1', '<filesystem>'): 1,
    ('apps/object_fps_pvp/include/RetroFPS/Pvp/NativeFiles.hpp', 'ZS1', '<filesystem>'): 1,
    ('apps/object_fps_pvp/include/RetroFPS/App/AnimationSetDefinition.hpp', 'ZS7', 'unordered_map'): 2,
})


def files_of(name):
    return json.loads(LISTS[name].read_text(encoding='utf-8'))['files']


def relative(path):
    return path.relative_to(REPOSITORY).as_posix()


def sim_sources():
    return sorted(relative(p) for p in SIM.glob('*.cpp'))


def position_differences(listed, present):
    """Files on exactly one side: listed but not in src/Sim, or in src/Sim but not listed."""
    return sorted(set(listed) ^ set(present))


def confirmed_findings():
    report = zone_scan.scan(str(REPOSITORY), LISTS['sources'], zone_scan.load_rules(str(zone_scan.DEFAULT_RULES)))
    return Counter((f['file'], f['rule'], f['text']) for f in report['findings']), report['per_rule']


class ZoneCListsTest(unittest.TestCase):
    def test_the_lists_are_disjoint(self):
        seen = Counter(f for name in LISTS for f in files_of(name))
        self.assertEqual([f for f, n in seen.items() if n > 1], [])

    def test_sim_sources_equal_confirmed_plus_candidates(self):
        listed = sorted(files_of('sources') + files_of('candidates'))
        self.assertEqual(position_differences(listed, sim_sources()), [],
                         'src/Sim/*.cpp and the confirmed + candidate lists differ')
        self.assertGreaterEqual(len(files_of('sources')), 1)

    def test_rewrite_files_are_the_three_in_pvp(self):
        self.assertEqual(files_of('rewrite'), REWRITE)
        for f in REWRITE:
            with self.subTest(file=f):
                self.assertEqual(Path(f).parent.as_posix(), relative(PVP))

    def test_no_sim_file_includes_a_present_header(self):
        include = re.compile(r'^\s*#\s*include\s*"RetroFPS/Present/', re.M)
        offenders = [relative(p) for p in [*SIM.glob('*.cpp'), *SIM_INCLUDE.glob('*.hpp')]
                     if include.search(p.read_text(encoding='utf-8'))]
        self.assertEqual(offenders, [])
        self.assertGreaterEqual(len(list(SIM_INCLUDE.glob('*.hpp'))), 1)

    def test_confirmed_findings_are_exactly_the_recorded_residual(self):
        findings, per_rule = confirmed_findings()
        self.assertEqual(findings, RESIDUAL, 'zone-C findings of the confirmed list changed; see the module docstring')
        for rule in ('ZS2', 'ZS3', 'ZS4', 'ZS5', 'ZS6'):
            self.assertEqual(per_rule[rule], 0, rule)

    def test_a_missing_sim_file_would_be_reported(self):
        # The positive example of the position check: a list that drops one Sim source names exactly that file.
        listed = sorted(files_of('sources') + files_of('candidates'))
        self.assertEqual(position_differences(listed[:-1], sim_sources()), [listed[-1]])
        self.assertEqual(position_differences(listed + ['apps/object_fps_pvp/src/Pvp/PvpMatch.cpp'], sim_sources()),
                         ['apps/object_fps_pvp/src/Pvp/PvpMatch.cpp'])


if __name__ == '__main__':
    unittest.main()
