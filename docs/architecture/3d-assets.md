# Object_FPS 3D assets

Object_FPS owns prepared runtime models and game-specific presentation definitions. Complete vendor archives and working DCC projects are kept outside `apps/` and runtime asset roots. They are manual authoring inputs, not game build or release dependencies. This workstation keeps the historical Object_FPS archive under `D:/common/3DModel/temp/gyo-engine/object_fps/art_source`; that local path is not required by another checkout. The game still uses enemy sprites; the 3D character definitions provide a loadable presentation boundary rather than a replacement scene/controller.

For an independent variant, copy game code and runtime content following the [manual game guide](../creating_apps.md). Existing AssetIds, clip names and animation/material references remain internal data contracts. Deployment identity changes do not require global renaming.

## Directories and responsibility

```text
external authoring storage/            complete vendor archives and DCC work
apps/object_fps/
  include/RetroFPS/App/                game presentation definition contracts
  src/App/                            definitions and viewmodel assembly
assets/object_fps/
  content.json                        catalogs and shader assembly declaration
  asset_catalog.json
  characters/superhero_male/           FBX, character.json, textures/
  characters/superhero_female/         FBX, character.json, textures/
  animations/ual_mannequin/            model and animation-set definitions
  weapons/ultimate_pistol_1/world/     static Pistol_1.fbx
  weapons/animated_pistol/viewmodel/   Pistol.fbx and viewmodel.animset.json
  weapons/mark23/viewmodel/            FBX, textures and presentation definitions
  textures/common/white1x1.png         game-owned reusable texture copy
engine/model/                         neutral model/animation and FBX adapter
engine/render/shaders/pipeline/       offline compiler
```

Dedicated textures stay beside their character or weapon. Small duplicated eye textures intentionally remain character-local. FBX files are copied without conversion; meshes, nodes, skins and clips stay in their original container. World weapons and first-person viewmodels remain separate assets.

Authors prepare runtime copies and catalog entries manually. The generic build hook then invokes asset assembly from `content.json` for both local and CI builds. Game CMake does not select files or implement copy/install rules. The assembled product reads only `build/target/object_fps/bin/assets/object_fps`, including compiled `shaders/builtin`; it never falls back to source art or mounts a separate `assets/common` directory. `common.texture.white` remains a logical ID in the game's own catalog.

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

## Historical source inventory and current model limits

The historical 2026-09-20 source inventory recorded **511 files / 1,076,255,418 bytes**, including 189 FBX,
72 Blender files, 10 Blender backups, 61 OBJ/MTL pairs, 18 glTF/BIN pairs,
5 GLB, 66 PNG and the supplied references, readmes and licenses. Each entry in
The historical inventory at `apps/object_fps/art_source/source_inventory.json` recorded the original absolute
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

1. Keep original archives and licenses in external authoring storage. Record source paths/hashes and selection decisions there; do not copy complete packs into the game project.
2. Prepare only the runtime FBX/textures required by the game. Preserve the original FBX container, put character/world/viewmodel variants in their respective asset directories, and register each runtime file in `asset_catalog.json`.
3. Add game definition JSON with `model_asset_id`, optional material overrides and animation-set IDs. Select exact source clip names and retain the same-source ownership checks described above.
4. Keep `content.json` as the asset assembly declaration. A normal local build automatically prepares the same executable-relative asset tree used by CI.
5. Enable game tests separately. Model/definition tests live under `tests/object_fps`, and diagnostics use `gyo_object_fps_acceptance`; no test source is injected into the game executable.

```sh
cmake --preset test -DGYO_APPS=object_fps -DGYO_TOOLS=
cmake --build --preset test
ctest --preset test
```

First enable the game and intended platforms in `engine/config/projects.csv`. Test generated content from the assembled product, including missing-file failures; runtime loading must work without any source art or checkout path. GPU appearance checks are separate from model import and definition tests. See [Object_FPS acceptance](../object_fps/acceptance.zh-Hant.md).

The following dated evidence is preserved exactly as historical context. Its paths, executable switches and ownership describe the old tree; they are not current commands or proof that this refactor passed.

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
