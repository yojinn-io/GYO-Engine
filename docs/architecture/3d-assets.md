# Object_FPS 3D assets

Object_FPS keeps complete vendor sources and a small set of runtime assets. The
game still uses sprites for enemies; the new character definitions provide a
loadable 3D presentation boundary, not a replacement enemy controller or scene.

For a new app or independent copy, follow the [manual app guide](../creating_apps.md).
Copy private runtime content to `assets/<new_name>` and use the app's generated
`Gyo::AppConfig::Assets` path. Existing catalog AssetIds and animation/material
references are internal data contracts; they do not need a global rename when
the deployment identity changes. Keep `assets/common` shared unless a resource
is deliberately moved into the copy's private content.

## Directories and responsibility

```text
apps/object_fps/
  art_source/
    vendor/<pack>/...                 complete original inner directory trees
    source_inventory.json            paths, hashes, contents and decisions
  tools/inventory_3d_sources.py       offline archive/identity/metadata utility
  include/RetroFPS/App/               game-specific presentation definitions
  src/App/                           definition loading and viewmodel assembly
assets/object_fps/
  asset_catalog.json
  characters/superhero_male/          FullBody FBX, character.json, textures/
  characters/superhero_female/        FullBody FBX, character.json, textures/
  animations/ual_mannequin/           UAL1_Standard.fbx + its mannequin definition
                                     + locomotion.animset.json
  weapons/ultimate_pistol_1/world/    static Pistol_1.fbx
  weapons/animated_pistol/viewmodel/  Pistol.fbx + viewmodel.animset.json
  weapons/mark23/viewmodel/           Mark23.fbx, textures/, mark23_viewmodel.json
                                     + viewmodel.animset.json
```

Dedicated textures stay beside their character or weapon. The two small eye
texture copies intentionally stay local to their character packages. Nothing is
promoted into `shared/` merely because it might eventually be shared. Existing
2D enemy, world, UI, font and map assets keep their locations. The FBX files are
copied without conversion; meshes, nodes, skin bindings and clips remain in the
original container. World weapons and first-person viewmodels are separate.

`apps/object_fps/CMakeLists.txt` stages and installs `assets/object_fps` and
`assets/common`; it does not stage the app's source directory. Therefore
`art_source` is outside both runtime asset roots. Do not extend staging to the
entire application directory. The local `art_source/.gitignore` allows vendor
Wavefront `.obj` models despite the repository's compiler-object ignore rule.

## Asset IDs and definitions

The existing catalog remains `{ "id", "type", "path" }`; paths are relative to
its mounted root. TextLoader loads definition JSON. A canonical Model AssetId
identifies each complete FBX parse, and AnimationSet selectors choose clips from
that shared model. There is no parallel asset manager or separate binary clip,
skeleton or material format.

```text
CharacterPresentationDefinition (Object_FPS, immutable)
  -> model_asset_id -> AssetManager cache -> UfbxModelLoader -> ModelAsset
       -> nodes + hierarchy + local reference transforms
       -> mesh parts + material-slot indices + geometry-to-node transforms
       -> skin joints + weights + geometry-to-bone binding matrices
       -> material names + baseColorLinear
       -> animation clips + node-targeted tracks
  -> animation_set_asset_id -> TextLoader -> AnimationSetDefinition
       -> semantic -> canonical model_asset_id + exact clip name
       -> shared_ptr<const ModelAsset> + clip index
  -> optional material_overrides -> own MaterialBinding values
       -> optional Texture AssetId -> SDL_image CPU image -> renderer upload

Instance (game/presentation)
  -> world transform, clip choice, clock, sampled Pose, mutable render buffers
  -> renderer-owned GPU mesh/texture handles
```

Model/animation data belongs to `GYO::Model`; FBX decoding belongs to the optional
`GYO::AssetUfbx` adapter. The engine neither knows these character names nor
creates GPU resources while importing assets. Game-specific JSON loading lives
in `AnimationSetDefinition` and `CharacterPresentationDefinition` under
`apps/object_fps/include/RetroFPS/App/` and `apps/object_fps/src/App/`.

| Canonical Model AssetId | Runtime source | Defined selectors / use |
| --- | --- | --- |
| `object_fps.model.character.superhero_male` | `characters/superhero_male/Superhero_Male_FullBody.fbx` | Reference pose; no animation set |
| `object_fps.model.character.superhero_female` | `characters/superhero_female/Superhero_Female_FullBody.fbx` | Reference pose; no animation set |
| `object_fps.model.animation.ual1_mannequin` | `animations/ual_mannequin/UAL1_Standard.fbx` | `idle`, `walk`, `run` from its 43 clips |
| `object_fps.model.weapon.ultimate_pistol_1.world` | `weapons/ultimate_pistol_1/world/Pistol_1.fbx` | Static world mesh |
| `object_fps.model.weapon.animated_pistol.viewmodel` | `weapons/animated_pistol/viewmodel/Pistol.fbx` | `fire`, `reload`, `slide`; no arms |
| `object_fps.model.mark23` | `weapons/mark23/viewmodel/Mark23.fbx` | Existing first-person weapon; all original IDs retained |

Character IDs are `object_fps.character.superhero_male`,
`object_fps.character.superhero_female` and `object_fps.character.ual1_mannequin`.
The last definition belongs beside the library because it uses the library's
own mannequin mesh and rig. `ual_mannequin` denotes that same-file assembly, not
a promise that arbitrary humanoid models accept its animations.

AnimationSet JSON uses exact, case-sensitive source clip names:

```json
{
  "version": 1,
  "clips": {
    "walk": {
      "model_asset_id": "object_fps.model.animation.ual1_mannequin",
      "clip": "Armature|Walk_Loop"
    }
  }
}
```

`object_fps.animset.ual1.locomotion` maps `idle/walk/run` to
`Armature|Idle_Loop`, `Armature|Walk_Loop`, `Armature|Jog_Fwd_Loop`.
`object_fps.animset.weapon.animated_pistol.viewmodel` maps `fire/reload/slide` to
`PistolArmature|Fire`, `PistolArmature|Reload`, `PistolArmature|Slide`.
`object_fps.animset.mark23.viewmodel` maps `Idle/Shoot/Reload/Draw/Hide` to the
identically named Mark23 clips. Multiple selectors keep ownership of the same
cached ModelAsset; they do not parse the FBX once per clip.

A minimal same-file character assembly is:

```json
{
  "version": 1,
  "model_asset_id": "object_fps.model.animation.ual1_mannequin",
  "animation_set_asset_id": "object_fps.animset.ual1.locomotion"
}
```

`animation_set_asset_id` and `material_overrides` are optional. Each override key
is an actual imported material name. An override may supply `texture_asset_id`,
`base_color_linear` (four components) and `sampler` (the existing sampler names,
such as `linear_clamp`). Omitting an override retains imported defaults. Bindings
are copied into the definition; changing one assembly does not mutate the shared
ModelAsset. Missing assets, missing/ambiguous clips, unknown material slots and
unsupported cross-source bindings return contextual errors.

The male slots are `MI_Eyes`, `MI_Hair_1`, `MI_Superhero_Male`; female slots are
`MI_Eyes`, `MI_Hair_2`, `MI_Superhero_Female`. Their overrides explicitly bind the
supplied eye, hair and dark body base-color PNGs with white tint and linear clamp.
Texture IDs follow `object_fps.texture.character.<male|female>.<eyes|hair|body>`.
The renderer remains unlit. `Model::Material::baseColorLinear` retains available
FBX diffuse color multiplied by its factor, defaulting to white when absent.
No PBR, automatic external texture discovery, normal/roughness processing, or
standalone MaterialAsset loader is introduced. Ultimate Pistol's six slots use
its FBX constants. Animated Pistol has no local diffuse properties on its six
materials, but inherits RGB `(0.8, 0.8, 0.8)` and factor `1` from the FBX
`FbxSurfacePhong` property template. Those source defaults are retained; white
is used only when neither local nor template color is supplied. Its alternate
OBJ/MTL is not used to infer FBX colors.

Mark23's existing weapon definition still drives offset, camera, sampler,
material textures, Idle anchor and muzzle geometry. Its five clip mappings now
come from `animation_set_asset_id`. Legacy inline `clips` is accepted for old
definitions, but a definition must choose exactly one of those two forms. The
existing Mark23 texture bindings retain white tint.

## Inventory, skeleton limits and source-only material

The archive contains **511 files / 1,076,255,418 bytes**, including 189 FBX,
72 Blender files, 10 Blender backups, 61 OBJ/MTL pairs, 18 glTF/BIN pairs,
5 GLB, 66 PNG and the supplied references, readmes and licenses. Each entry in
`apps/object_fps/art_source/source_inventory.json` records the original absolute
path, repository-relative archive path, bytes, SHA-256, runtime copies and
source-only reason. Runtime mappings also identify their catalog IDs and types.
Binary FBX entries include mesh/bone/material/clip metadata and material templates,
bone hierarchy, raw rest/bind fingerprints, coordinate settings and source
weight statistics; glTF/GLB entries include external-reference existence.
Other authoring files are inventoried by identity and format, not decoded.

| Archived pack | Files | Selected runtime content / deferred content |
| --- | ---: | --- |
| Animated FPS Guns, June 2018 | 25 | Pistol only. Other weapons, Blender and OBJ stay in source. |
| Retro Weapon Pack V1 | 130 | Entirely source-only: separate arms/guns and 84 animation-only FBX (70 arms, 14 guns) need additional assembly/external binding. |
| Ultimate Gun Pack, July 2019 | 222 | Pistol_1 only. Other weapons/accessories and alternate formats stay in source. |
| Universal Animation Library Standard | 9 | Standard FBX with its own mannequin. Root Motion and alternate formats stay in source. |
| Universal Animation Library 2 Standard | 13 | Entirely deferred, including Root Motion and separate female mannequin. |
| Universal Base Characters Standard | 112 | Two FullBody FBX and six character-local texture copies. Extra hairstyles, variants, normal/roughness maps and alternate formats stay in source. |

The two superhero models have three source meshes, 65 bone nodes and no clips.
UAL1 and UAL2 each contain a mannequin mesh, 65 bone nodes and 43 clips. Raw source
inspection found the same bone names/hierarchy across these humanoid sources,
but the superhero rest transforms and skin bindings differ from the libraries.
UAL1/UAL2 match in the compared local rest transforms and bind matrices; this
does not implement a node-target mapping in the engine. Retro arms and their
animation files share a 63-bone hierarchy, which also does not enable importing
one file's node indices into another file. Animated Pistol has seven bone nodes,
four skin clusters and at most two influences per control point.

Executable assembly therefore accepts an AnimationSet only when its canonical
model ID and immutable source-model ownership match the character/weapon model.
Even matching bone names or hierarchy in different files is rejected. Raw
fingerprints are inventory evidence, not a compatibility gate; node indexing,
reference pose, units, coordinate conversion and bindings all matter. No
retargeting, external binding, root-motion extraction or new animation state
machine is provided. UAL1 is exercised only with its own mannequin. Superhero
definitions intentionally have no AnimationSet.

The importer keeps its existing largest-four-weights-then-normalize behavior.
Source control points exceeding four positive weights are:

| Source body mesh | Control points affected | Maximum discarded fraction of original total weight |
| --- | ---: | ---: |
| Superhero male | 275 / 6,287 (4.374%) | 7.6608% |
| Superhero female | 130 / 6,408 (2.029%) | 2.2612% |
| UAL mannequin | 145 / 6,994 (2.073%) | 9.5054% |

These counts refer to source control points, not the renderer's split vertices.
CPU skinning tests cover the imported representation; they do not demonstrate
pixel-equivalence to a DCC tool retaining every source weight.

Some source glTF files refer to absent `T_Eye_Normal_png.png` and, for the male,
`T_Hair_1_Normal_png.png`; corresponding non-`_png` files exist elsewhere in the
pack. The inventory preserves and reports the missing references. The selected
runtime FBX/base-color PNG path avoids depending on those glTF references;
originals are not rewritten.

Local license files in Ultimate Gun Pack, both UAL packs and Universal Base
Characters state CC0. Retro's supplied `Readme.pdf` permits personal/commercial
use and says attribution is not required. Animated FPS Guns contains no supplied
license/readme, so its license status remains undocumented in this archive;
another pack's CC0 is not applied to it. All supplied license/reference files
remain byte-for-byte with their own packs. This is a record of the supplied
documents, not a replacement license or a claim about unprovided terms.

## Adding and verifying assets

1. Keep the complete incoming pack under `apps/object_fps/art_source/vendor` with
   its license and original inner paths. Record hashes, actual materials/clips,
   source bindings and source-only reasons. Keep working DCC projects outside
   runtime roots; do not create empty directories or placeholder assets.
2. Choose only runtime-needed files and place them by ownership. For a character,
   put its FBX and dedicated PNGs under `characters/<name>/`; register one Model
   ID and each texture ID in `asset_catalog.json`. Preserve the source container.
3. Add a text `character.json` with `model_asset_id` and optional overrides keyed
   by the actual imported material slots. For a reference-pose-only model, omit
   the AnimationSet field. Load it with `LoadCharacterPresentationDefinition`.
4. For an additional UAL1 semantic, add a selector to
   `animations/ual_mannequin/locomotion.animset.json`, using an exact clip name
   from the inventory and the existing canonical model ID. To create a different
   animation library, keep its own mannequin/rig and use same-file assembly
   until explicit cross-file binding is available.
5. For a first-person weapon, keep the gun, any supplied arms and matching clips
   under `weapons/<name>/viewmodel/`. Use a separate `world/` asset when needed.
   The current WeaponViewModel requires the established five actions and anchor/
   muzzle definition; Animated Pistol's three-action set is loadable inventory
   content, not a drop-in Mark23 gameplay replacement.
6. Validate through AssetManager/ufbx before admitting the model as usable:
   reference pose, material slots, exact clips, finite CPU skinning, constant
   material fallback, missing-reference diagnostics, independent poses at two
   times and unchanged shared material defaults. A failed model stays source-only
   with its error recorded. Avoid widening the importer merely to fit the pack.

To verify the original archive and runtime copies (without rewriting files):

```powershell
python apps/object_fps/tools/inventory_3d_sources.py --source-root D:\common\3DModel\temp
```

The initial copy used the same command with `--copy --output
apps/object_fps/art_source/source_inventory.json`. `--copy` refuses existing
files with different hashes. The utility's raw metadata inspection does not
certify engine loading or rendering.

Enable `object_fps` and the target platform in `config/engine/projects.csv`, then
use the generic preset with an explicit app selection so asset verification
cannot silently configure another app:

```powershell
cmake --preset test -DGYO_APPS=object_fps
cmake --build --preset test
ctest --preset test
```

The required regression coverage includes Mark23 ViewModel/reload/muzzle GPU
smokes and a staging/install inspection confirming every catalog path exists
while `art_source` is absent. Build/GPU outcomes are recorded in the task's
implementation report; commands listed here are instructions, not claims that
every environment has already passed them.

### Historical verified implementation (2026-09-20, Windows)

These results predate the CSV project-management migration. The old preset and
build paths below identify that historical run; use `test` for current quality commands.

- Configured `cmake --preset object-fps` with the installed MSVC toolchain and
  existing local FetchContent source caches; `cmake --build --preset object-fps`
  completed successfully. No dependency versions were changed. The local
  invocation/logs are under the ignored `build/asset-configure.*` and
  `build/asset-build.*` files.
- `ctest --preset object-fps --parallel 4`: **15/15 passed**. The Object_FPS
  headless suite imports all five selected FBX files through AssetManager/ufbx,
  checks their real material constants and clips, samples/skins the animation
  examples, verifies independent instances and shared cache ownership, and
  decodes all six character-local PNGs through the production SDL_image loader.
- `ctest --test-dir build/object-fps -R
  '^object_fps\.(viewmodel_smoke|reload_smoke|muzzle_smoke)$' --output-on-failure`:
  **3/3 GPU regressions passed**. This covers the existing Mark23 presentation;
  it does not claim a new superhero gameplay renderer or cross-file retargeting.
- `cmake --install build/object-fps --prefix build/asset-install` succeeded.
  Both staging and installation contain all **37** combined Object_FPS/common
  catalog entries and no `art_source`, `.blend` or `.blend1` content. The
  installed executable passed `--validate-package` and `--startup-smoke-test`.
- Archive verification matched all **511 files / 1,076,255,418 bytes** and all
  **11 selected runtime copies** by SHA-256. Mark23's relocated FBX and three
  PNGs also match their original Git blobs. The inventory utility was corrected
  to distinguish bone-to-model parent links from bone-to-skin-cluster links,
  and records FBX material property templates as well as local properties.

Full CPU and GPU results are in the ignored `build/asset-tests.log` and
`build/asset-gpu-tests.log`. New character assets are verified at the loading,
image decoding and CPU pose/skinning boundaries; their complete in-game visual
appearance is not covered by the Mark23 GPU tests. The retained source-only
assets and remaining limitations are listed above.
