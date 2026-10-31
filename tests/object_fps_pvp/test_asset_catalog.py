"""Product-owned check of the object_fps_pvp asset catalog (v6 batch 08, v8 asset layout).

Every catalog entry points at an existing file; every file under the asset root
is either in the catalog or one of the root manifests; and every catalog id is
used: referenced by the product's sources, tests or acceptance code, or by the
content of another used entry. An id that is used another way is listed in
RETAINED with its reason.

The layout checks enforce docs/object_fps_pvp/assets.zh-Hant.md: where each kind
of file lives, how it is named, that text entries reference their own models and
textures, that shipped ids are used by the product itself, that every file asset
has a source record, and that no other owner names this product's asset paths.
Known departures are listed in LAYOUT_EXCEPTIONS; the table only shrinks.
"""
import json
from pathlib import Path, PurePosixPath
import re
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
ASSETS = REPOSITORY / 'assets' / 'object_fps_pvp'
CODE_ROOTS = [
    REPOSITORY / 'apps' / 'object_fps_pvp',
    REPOSITORY / 'tests' / 'object_fps_pvp',
    REPOSITORY / 'build' / 'acceptance' / 'object_fps_pvp',
]
# The product's own code: an id referenced only from tests or acceptance code is not shipped content.
PRODUCT_ROOTS = [REPOSITORY / 'apps' / 'object_fps_pvp']
CODE_SUFFIXES = {'.cpp', '.hpp', '.h', '.py', '.go'}
MANIFESTS = {'asset_catalog.json', 'content.json'}
SOURCES = REPOSITORY / 'tests' / 'object_fps_pvp' / 'asset_sources.json'
# Ids in use without a reference by id, with the reason.
RETAINED = {
    'object_fps_pvp.arena': 'loaded by path (pvp_arena.json) by the Match and, through arenas.json, by the Client',
    'object_fps_pvp.arena.corners': 'loaded by path (pvp_corners.json) by the Match and, through arenas.json, by the Client',
    'object_fps_pvp.arenas': 'loaded by path (arenas.json): the arenas the Client installs',
    'object_fps_pvp.movement_rules': 'loaded by path (movement_rules.json) by the Match only (protocol-v8 section 3)',
}

# First folder -> catalog types it may hold.
FOLDER_TYPES = {
    'characters': {'model', 'texture', 'text'},
    'weapons': {'model', 'texture', 'text'},
    'animations': {'model'},
    'fonts': {'font'},
    'ui': {'text', 'texture'},
    'audio': {'sound', 'text'},
    'rules': {'text'},
    'textures': {'texture'},
}
# Under these first folders a texture sits in a folder named "textures".
TEXTURE_FOLDER_ROOTS = {'characters', 'weapons', 'ui'}
# Models and textures under these first folders may be referenced from any folder.
SHARED_ROOTS = {'animations', 'textures'}
# Definitions in object folders are named after their schema.
OBJECT_ROOTS = {'characters', 'weapons'}
DEFINITION_NAMES = {'character.json', 'presentation.json', 'viewmodel.json', 'hitbox.json'}
DEFINITION_SUFFIXES = ('.animset.json', '.anim_events.json')
# Files of these types are written in the repository and named in snake_case;
# delivered models, textures and fonts keep their names.
AUTHORED_TYPES = {'text', 'sound'}
EXTENSIONS = {'model': '.fbx', 'texture': '.png', 'font': '.ttf', 'sound': '.wav', 'text': '.json'}
SNAKE_CASE = re.compile(r'[a-z0-9]+(?:_[a-z0-9]+)*')
ASSET_ID = re.compile(r'object_fps_pvp(?:\.[a-z0-9_]+)+')
ID_TOKEN = re.compile(r'[\w.]+')
RULES = {'root-file', 'first-folder', 'folder-type', 'texture-folder', 'own-files', 'snake-case',
         'definition-name', 'case', 'id-syntax', 'id-type', 'product-use'}
# Other owners must not name this product's asset paths (DI-6). Every top-level
# folder is a common layer except these: content, documents, vendored sources,
# build outputs, and the per-product groups scanned on their own.
NOT_COMMON = {'assets', 'docs', 'agents', 'third_party', 'build', 'apps', 'tests'}
COMMON_IN_GROUPS = ['tests/common', 'tests/framework', 'build/acceptance/common', 'build/ci', 'build/cmake']
PRODUCT_GROUPS = ['apps', 'tests', 'build/acceptance']
OTHER_OWNER_SUFFIXES = CODE_SUFFIXES | {'.c', '.cmake', '.txt', '.json', '.csv', '.yml', '.yaml', '.sh', '.ps1'}

PATH_LOADED_MOVE = 'v8-product-asset-path-loaded-move'
BUNS_FIXTURE = 'v8-product-asset-buns-fixture'
# Known departures from the layout, keyed by (rule, catalog path), with the batch
# that removes each. Adding one loosens the guard and needs the user's approval.
LAYOUT_EXCEPTIONS = {
    ('root-file', 'arenas.json'): PATH_LOADED_MOVE,
    ('root-file', 'pvp_arena.json'): PATH_LOADED_MOVE,
    ('root-file', 'pvp_corners.json'): PATH_LOADED_MOVE,
    ('root-file', 'movement_rules.json'): PATH_LOADED_MOVE,
    ('own-files', 'characters/hairstyles/buns/character.json'): BUNS_FIXTURE,
    ('product-use', 'characters/hairstyles/buns/character.json'): BUNS_FIXTURE,
    ('product-use', 'characters/hairstyles/buns/Hair_Buns.fbx'): BUNS_FIXTURE,
    ('product-use', 'characters/superhero_female/textures/T_Hair_2_BaseColor.png'): BUNS_FIXTURE,
    ('definition-name', 'weapons/nailer_v2/viewmodel/nailer_v2_viewmodel.json'):
        'the next feature batch that edits this definition',
}


def catalog():
    return json.loads((ASSETS / 'asset_catalog.json').read_text(encoding='utf-8'))['assets']


def read_entry(entry):
    return json.loads((ASSETS / entry['path']).read_text(encoding='utf-8'))


def code_text(roots=CODE_ROOTS):
    parts = []
    for root in roots:
        for path in sorted(root.rglob('*')):
            if path.is_file() and path.suffix in CODE_SUFFIXES and path.name != Path(__file__).name:
                parts.append(path.read_text(encoding='utf-8', errors='replace'))
    return '\n'.join(parts)


def used_ids(entries, code):
    """Ids referenced from code, then transitively from the text of used entries.

    A reference is the id as a whole run of word characters and dots, so a longer
    id or name that contains it does not count.
    """
    paths = {entry['id']: ASSETS / entry['path'] for entry in entries}
    used = set(paths) & set(ID_TOKEN.findall(code))
    pending = list(used)
    while pending:
        current = paths[pending.pop()]
        if current.suffix in {'.json', '.txt', '.csv'}:
            tokens = set(ID_TOKEN.findall(current.read_text(encoding='utf-8', errors='replace')))
            for other in (set(paths) - used) & tokens:
                used.add(other)
                pending.append(other)
    return used


def strings_in(value):
    if isinstance(value, str):
        yield value
    elif isinstance(value, dict):
        for item in value.values():
            yield from strings_in(item)
    elif isinstance(value, list):
        for item in value:
            yield from strings_in(item)


def layout_findings(entries):
    """(rule, path) for entries outside the folder layout (DI-2)."""
    found = set()
    for entry in entries:
        parts = PurePosixPath(entry['path']).parts
        if len(parts) == 1:
            found.add(('root-file', entry['path']))
        elif parts[0] not in FOLDER_TYPES:
            found.add(('first-folder', entry['path']))
        else:
            if entry['type'] not in FOLDER_TYPES[parts[0]]:
                found.add(('folder-type', entry['path']))
            if entry['type'] == 'texture' and (parts[0] in TEXTURE_FOLDER_ROOTS and parts[-2] != 'textures'
                                               or parts[0] == 'textures' and len(parts) < 3):
                found.add(('texture-folder', entry['path']))
    return found


def own_file_findings(entries, read=read_entry):
    """(rule, path) for text entries that reference a model or texture outside their folder (DI-3)."""
    by_id = {entry['id']: entry for entry in entries}
    found = set()
    for entry in entries:
        if entry['type'] != 'text':
            continue
        folder = PurePosixPath(entry['path']).parent
        for value in strings_in(read(entry)):
            target = by_id.get(value)
            if target is None or target['type'] not in {'model', 'texture'}:
                continue
            target_path = PurePosixPath(target['path'])
            if target_path.parent not in (folder, folder / 'textures') and target_path.parts[0] not in SHARED_ROOTS:
                found.add(('own-files', entry['path']))
    return found


def naming_findings(entries):
    """(rule, path) for folders or authored files off snake_case, and definitions not named after their schema."""
    found = set()
    for entry in entries:
        path = PurePosixPath(entry['path'])
        words = list(path.parts[:-1])
        if entry['type'] in AUTHORED_TYPES:
            words += path.name.split('.')[:-1]
        if not all(SNAKE_CASE.fullmatch(word) for word in words):
            found.add(('snake-case', entry['path']))
        if (entry['type'] == 'text' and path.parts[0] in OBJECT_ROOTS
                and path.name not in DEFINITION_NAMES and not path.name.endswith(DEFINITION_SUFFIXES)):
            found.add(('definition-name', entry['path']))
    return found


def case_findings(entries):
    """(rule, path) for catalog paths that differ only by case: macOS and Windows file systems cannot hold both."""
    spellings = {}
    for path in [entry['path'] for entry in entries] + sorted(MANIFESTS):
        spellings.setdefault(path.lower(), set()).add(path)
    return {('case', path) for group in spellings.values() if len(group) > 1 for path in group}


def id_findings(entries):
    """(rule, path) for ids off the syntax and types whose extension does not match."""
    found = set()
    for entry in entries:
        if not ASSET_ID.fullmatch(entry['id']):
            found.add(('id-syntax', entry['path']))
        if PurePosixPath(entry['path']).suffix.lower() != EXTENSIONS.get(entry['type']):
            found.add(('id-type', entry['path']))
    return found


def product_use_findings(entries, code):
    """(rule, path) for ids the product's own code does not use, directly or through used content (DI-1)."""
    used = used_ids(entries, code)
    return {('product-use', entry['path']) for entry in entries if entry['id'] not in used and entry['id'] not in RETAINED}


def all_findings(entries):
    return (layout_findings(entries) | own_file_findings(entries) | naming_findings(entries) | case_findings(entries)
            | id_findings(entries) | product_use_findings(entries, code_text(PRODUCT_ROOTS)))


def unexcused(findings, exceptions=LAYOUT_EXCEPTIONS):
    return sorted(finding for finding in findings if finding not in exceptions)


def stale_exceptions(findings, exceptions=LAYOUT_EXCEPTIONS):
    return sorted(set(exceptions) - set(findings))


def source_record_problems(entries, sources):
    """Missing, stale or incomplete source records for the entries that are not text."""
    needed = {entry['id'] for entry in entries if entry['type'] != 'text'}
    problems = [('missing', i) for i in sorted(needed - set(sources))]
    problems += [('stale', i) for i in sorted(set(sources) - needed)]
    problems += [('incomplete', i) for i in sorted(needed & set(sources))
                 if not all(isinstance(sources[i].get(key), str) and sources[i][key].strip()
                            for key in ('source', 'license', 'record'))]
    return problems


def owner_texts(roots):
    files = []
    for root in roots:
        candidates = sorted(root.rglob('*')) if root.is_dir() else [root]
        files += [path for path in candidates if path.is_file() and path.suffix in OTHER_OWNER_SUFFIXES]
    return [(path.relative_to(REPOSITORY).as_posix(), path.read_text(encoding='utf-8', errors='replace')) for path in files]


def common_layer_texts():
    roots = [path for path in sorted(REPOSITORY.iterdir())
             if path.name not in NOT_COMMON and (not path.name.startswith('.') or path.name == '.github')]
    roots += [REPOSITORY / name for name in COMMON_IN_GROUPS] + sorted((REPOSITORY / 'build').glob('*.py'))
    return owner_texts(roots)


def other_product_texts():
    common = {REPOSITORY / name for name in COMMON_IN_GROUPS}
    return owner_texts([path for group in PRODUCT_GROUPS for path in sorted((REPOSITORY / group).iterdir())
                        if path.is_dir() and path.name != 'object_fps_pvp' and path not in common])


def asset_path_mentions(texts, entries, catalog_paths=True):
    """(file, path) for each text naming this product's asset root or, in a common layer, one of its catalog paths.

    Another product may lay out its own assets the same way, so its relative paths
    can match ours; only the asset root identifies this product there.
    """
    needles = ['assets/object_fps_pvp'] + (sorted({entry['path'] for entry in entries}) if catalog_paths else [])
    return sorted((name, needle) for name, text in texts for needle in needles if needle in text)


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
        entries = catalog() + [{'id': 'object_fps_pvp.test.orphan', 'path': 'characters/player/presentation.json'}]
        self.assertNotIn('object_fps_pvp.test.orphan', used_ids(entries, code_text()))

    def test_a_reference_is_the_whole_id(self):
        # A longer id or a file name that contains an id does not reference it.
        entries = [{'id': 'object_fps_pvp.arena', 'path': 'pvp_arena.json'}]
        longer = '"object_fps_pvp.arena.corners" "object_fps_pvp.arenas" object_fps_pvp.arena.json'
        self.assertEqual(used_ids(entries, longer), set())
        self.assertEqual(used_ids(entries, 'Find("object_fps_pvp.arena");'), {'object_fps_pvp.arena'})


class AssetLayoutTests(unittest.TestCase):
    def test_every_id_is_used_by_the_product(self):
        self.assertEqual(unexcused(product_use_findings(catalog(), code_text(PRODUCT_ROOTS))), [])

    def test_paths_follow_the_layout(self):
        self.assertEqual(unexcused(layout_findings(catalog())), [])

    def test_definitions_reference_their_own_files(self):
        self.assertEqual(unexcused(own_file_findings(catalog())), [])

    def test_names_follow_the_naming_rules(self):
        self.assertEqual(unexcused(naming_findings(catalog())), [])

    def test_no_paths_differ_only_by_case(self):
        self.assertEqual(unexcused(case_findings(catalog())), [])

    def test_ids_and_types_match(self):
        self.assertEqual(unexcused(id_findings(catalog())), [])

    def test_every_file_asset_has_a_source_record(self):
        sources = json.loads(SOURCES.read_text(encoding='utf-8'))['assets']
        self.assertEqual(source_record_problems(catalog(), sources), [])

    def test_no_other_owner_names_a_pvp_asset_path(self):
        entries = catalog()
        self.assertEqual(asset_path_mentions(common_layer_texts(), entries), [])
        self.assertEqual(asset_path_mentions(other_product_texts(), entries, catalog_paths=False), [])

    def test_every_exception_is_still_needed(self):
        self.assertEqual(stale_exceptions(all_findings(catalog())), [])
        self.assertEqual(sorted({rule for rule, _ in LAYOUT_EXCEPTIONS} - RULES), [])
        self.assertTrue(all(isinstance(batch, str) and batch.strip() for batch in LAYOUT_EXCEPTIONS.values()))


def entry(path, type_, id_=None):
    return {'id': id_ or 'object_fps_pvp.test.' + re.sub(r'\W', '_', path.lower()), 'type': type_, 'path': path}


class AssetLayoutSelfTests(unittest.TestCase):
    """Each check finds the departure injected into it and accepts the layout it allows."""

    def test_the_layout_check_finds_a_root_file_and_an_unknown_folder(self):
        found = layout_findings([entry('stray.json', 'text'), entry('props/box/Box.fbx', 'model')])
        self.assertEqual(found, {('root-file', 'stray.json'), ('first-folder', 'props/box/Box.fbx')})

    def test_the_layout_check_finds_a_wrong_type_and_a_loose_texture(self):
        found = layout_findings([
            entry('fonts/Glyphs.png', 'texture'),
            entry('characters/box/Box_D.png', 'texture'),
            entry('textures/White.png', 'texture'),
            entry('characters/box/textures/Box_D.png', 'texture'),
            entry('textures/common/White.png', 'texture'),
            entry('ui/textures/Icon.png', 'texture'),
        ])
        self.assertEqual(found, {('folder-type', 'fonts/Glyphs.png'), ('texture-folder', 'characters/box/Box_D.png'),
                                 ('texture-folder', 'textures/White.png')})

    def test_the_reference_check_finds_a_texture_in_another_folder(self):
        entries = [
            entry('characters/box/Box.fbx', 'model', 'object_fps_pvp.model.box'),
            entry('characters/box/textures/Box_D.png', 'texture', 'object_fps_pvp.texture.box'),
            entry('characters/crate/textures/Crate_D.png', 'texture', 'object_fps_pvp.texture.crate'),
            entry('animations/library/Library.fbx', 'model', 'object_fps_pvp.model.library'),
            entry('textures/common/White.png', 'texture', 'object_fps_pvp.texture.white'),
            entry('characters/box/character.json', 'text', 'object_fps_pvp.box'),
            entry('characters/box/walk.animset.json', 'text', 'object_fps_pvp.animset.box'),
        ]
        content = {
            'object_fps_pvp.box': {'model_asset_id': 'object_fps_pvp.model.box',
                                   'materials': {'Box': 'object_fps_pvp.texture.box',
                                                 'Crate': 'object_fps_pvp.texture.crate'}},
            'object_fps_pvp.animset.box': {'clips': [{'model_asset_id': 'object_fps_pvp.model.library'},
                                                     {'texture': 'object_fps_pvp.texture.white'}]},
        }
        found = own_file_findings(entries, read=lambda e: content[e['id']])
        self.assertEqual(found, {('own-files', 'characters/box/character.json')})

    def test_the_naming_check_finds_case_authored_names_and_definition_names(self):
        found = naming_findings([
            entry('characters/Box/character.json', 'text'),
            entry('audio/Gun-Shot.wav', 'sound'),
            entry('characters/box/box_definition.json', 'text'),
            entry('characters/box/Box_Delivered.fbx', 'model'),
            entry('characters/box/walk.animset.json', 'text'),
            entry('ui/hud.json', 'text'),
        ])
        self.assertEqual(found, {('snake-case', 'characters/Box/character.json'), ('snake-case', 'audio/Gun-Shot.wav'),
                                 ('definition-name', 'characters/box/box_definition.json')})

    def test_the_case_check_finds_paths_differing_only_by_case(self):
        found = case_findings(catalog() + [entry('characters/player/player_v20.fbx', 'model')])
        self.assertEqual(found, {('case', 'characters/player/player_v20.fbx'), ('case', 'characters/player/Player_V20.fbx')})

    def test_the_id_check_finds_bad_syntax_and_a_type_mismatch(self):
        found = id_findings([
            entry('characters/box/Box.fbx', 'model', 'Object_FPS_PVP.Box'),
            entry('characters/box/Box.png', 'model', 'object_fps_pvp.model.box_png'),
            entry('characters/box/Box.obj', 'mesh', 'object_fps_pvp.model.box_obj'),
            entry('characters/box/textures/Box_D.PNG', 'texture', 'object_fps_pvp.texture.box'),
        ])
        self.assertEqual(found, {('id-syntax', 'characters/box/Box.fbx'), ('id-type', 'characters/box/Box.png'),
                                 ('id-type', 'characters/box/Box.obj')})

    def test_the_product_use_check_finds_an_id_the_product_does_not_use(self):
        orphan = entry('ui/hit_feedback.json', 'text', 'object_fps_pvp.test.unused')
        product = code_text(PRODUCT_ROOTS)
        self.assertIn(('product-use', orphan['path']), product_use_findings(catalog() + [orphan], product))
        self.assertNotIn(('product-use', orphan['path']),
                         product_use_findings(catalog() + [orphan], product + '\n"object_fps_pvp.test.unused"'))

    def test_the_exception_check_finds_a_stale_exception(self):
        exceptions = {('root-file', 'kept.json'): 'batch', ('root-file', 'gone.json'): 'batch'}
        findings = {('root-file', 'kept.json'), ('root-file', 'new.json')}
        self.assertEqual(stale_exceptions(findings, exceptions), [('root-file', 'gone.json')])
        self.assertEqual(unexcused(findings, exceptions), [('root-file', 'new.json')])

    def test_the_source_check_finds_missing_stale_and_incomplete_records(self):
        entries = [entry('characters/box/Box.fbx', 'model', 'object_fps_pvp.model.box'),
                   entry('characters/box/textures/Box_D.png', 'texture', 'object_fps_pvp.texture.box'),
                   entry('characters/box/character.json', 'text', 'object_fps_pvp.box')]
        sources = {'object_fps_pvp.texture.box': {'source': 'delivery', 'license': ' ', 'record': 'commit'},
                   'object_fps_pvp.model.gone': {'source': 'delivery', 'license': 'unrecorded', 'record': 'commit'}}
        self.assertEqual(source_record_problems(entries, sources), [
            ('missing', 'object_fps_pvp.model.box'), ('stale', 'object_fps_pvp.model.gone'),
            ('incomplete', 'object_fps_pvp.texture.box')])

    def test_the_owner_check_finds_a_pvp_path_in_another_owner(self):
        texts = [('engine/Loader.cpp', 'Load("weapons/nailer_v2/world/Nailer2_TPS.fbx");'),
                 ('tools/editor.py', 'root = "assets/object_fps_pvp"'),
                 ('tools/other.py', 'Load("characters/other/Other.fbx")')]
        self.assertEqual(asset_path_mentions(texts, catalog()), [
            ('engine/Loader.cpp', 'weapons/nailer_v2/world/Nailer2_TPS.fbx'), ('tools/editor.py', 'assets/object_fps_pvp')])
        products = [('tests/object_fps_v2/Assets.cpp', 'Load("weapons/nailer_v2/world/Nailer2_TPS.fbx");'),
                    ('apps/object_fps/Root.cpp', 'Mount("assets/object_fps_pvp");')]
        self.assertEqual(asset_path_mentions(products, catalog(), catalog_paths=False),
                         [('apps/object_fps/Root.cpp', 'assets/object_fps_pvp')])

    def test_the_owner_scan_covers_every_common_layer(self):
        # A new top-level folder is scanned without editing this test.
        scanned = {name.split('/')[0] for name, _ in common_layer_texts()}
        self.assertTrue({'engine', 'framework', 'tools', 'services'} <= scanned)
        self.assertFalse({'assets', 'docs', 'apps'} & scanned)
        names = {name for name, _ in common_layer_texts()}
        self.assertIn('build/content_contract.py', names)
        self.assertFalse(any(name.startswith(('tests/object_fps_pvp/', 'build/acceptance/object_fps_pvp/', 'build/target/'))
                             for name in names | {n for n, _ in other_product_texts()}))


if __name__ == '__main__':
    unittest.main()
