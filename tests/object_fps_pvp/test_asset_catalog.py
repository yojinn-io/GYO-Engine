"""Product-owned check of the object_fps_pvp asset catalog (v6 batch 08).

Every catalog entry points at an existing file; every file under the asset root
is either in the catalog or one of the root manifests; and every catalog id is
used: referenced by the product's sources, tests or acceptance code, or by the
content of another used entry. An id that is used another way is listed in
RETAINED with its reason.
"""
import json
from pathlib import Path
import re
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
ASSETS = REPOSITORY / 'assets' / 'object_fps_pvp'
CODE_ROOTS = [
    REPOSITORY / 'apps' / 'object_fps_pvp',
    REPOSITORY / 'tests' / 'object_fps_pvp',
    REPOSITORY / 'build' / 'acceptance' / 'object_fps_pvp',
]
CODE_SUFFIXES = {'.cpp', '.hpp', '.h', '.py', '.go'}
MANIFESTS = {'asset_catalog.json', 'content.json'}
# Ids in use without a reference by id, with the reason.
RETAINED = {
    'object_fps_pvp.arena': 'loaded by path (pvp_arena.json) by the Match and, through arenas.json, by the Client',
    'object_fps_pvp.arena.corners': 'loaded by path (pvp_corners.json) by the Match and, through arenas.json, by the Client',
    'object_fps_pvp.arenas': 'loaded by path (arenas.json): the arenas the Client installs',
}


def catalog():
    return json.loads((ASSETS / 'asset_catalog.json').read_text(encoding='utf-8'))['assets']


def code_text():
    parts = []
    for root in CODE_ROOTS:
        for path in sorted(root.rglob('*')):
            if path.is_file() and path.suffix in CODE_SUFFIXES and path.name != Path(__file__).name:
                parts.append(path.read_text(encoding='utf-8', errors='replace'))
    return '\n'.join(parts)


def used_ids(entries, code):
    """Ids referenced from code, then transitively from the text of used entries."""
    paths = {entry['id']: ASSETS / entry['path'] for entry in entries}
    pattern = {i: re.compile(r'(?<![\w.])' + re.escape(i) + r'(?![\w.])') for i in paths}
    used = {i for i in paths if pattern[i].search(code)}
    pending = list(used)
    while pending:
        current = paths[pending.pop()]
        if current.suffix in {'.json', '.txt', '.csv'}:
            text = current.read_text(encoding='utf-8', errors='replace')
            for other in paths:
                if other not in used and pattern[other].search(text):
                    used.add(other)
                    pending.append(other)
    return used


class AssetCatalogTests(unittest.TestCase):
    def test_every_entry_points_at_an_existing_file(self):
        missing = [e['id'] for e in catalog() if not (ASSETS / e['path']).is_file()]
        self.assertEqual(missing, [])

    def test_every_asset_file_is_in_the_catalog(self):
        listed = {Path(e['path']).as_posix() for e in catalog()} | MANIFESTS
        files = {p.relative_to(ASSETS).as_posix() for p in ASSETS.rglob('*') if p.is_file()}
        self.assertEqual(sorted(files - listed), [])

    def test_ids_are_unique(self):
        ids = [e['id'] for e in catalog()]
        self.assertEqual(len(ids), len(set(ids)))

    def test_every_id_is_used_or_retained_with_a_reason(self):
        entries = catalog()
        used = used_ids(entries, code_text())
        unused = sorted(e['id'] for e in entries if e['id'] not in used and e['id'] not in RETAINED)
        self.assertEqual(unused, [])

    def test_retained_ids_are_still_in_the_catalog_and_otherwise_unused(self):
        entries = catalog()
        ids = {e['id'] for e in entries}
        self.assertTrue(set(RETAINED) <= ids)
        used = used_ids(entries, code_text())
        self.assertEqual(sorted(set(RETAINED) & used), [])

    def test_the_check_finds_an_orphan(self):
        # Guard against a check that can never fail.
        entries = catalog() + [{'id': 'object_fps_pvp.test.orphan', 'path': 'players/presentation.json'}]
        self.assertNotIn('object_fps_pvp.test.orphan', used_ids(entries, code_text()))


if __name__ == '__main__':
    unittest.main()
