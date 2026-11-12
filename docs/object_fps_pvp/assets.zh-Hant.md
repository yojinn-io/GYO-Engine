# PvP 素材標準（object_fps_pvp）

更新：2026-11-12。Owner：`object_fps_pvp`。決定與批次見 [asset-layout 計畫](plans/v8/asset-layout.md)。

本文件規定 `assets/object_fps_pvp/` 底下的檔案放哪裡、怎麼命名、怎麼記錄來源。檢查寫在 `tests/object_fps_pvp/test_asset_catalog.py`（CTest `object_fps_pvp.asset_catalog`）。

## 1. 範圍

- 只適用 `assets/object_fps_pvp/`。舊的 `object_fps`、`object_fps_v2` 已移除（2026-11-12，`v8-arch-remove-object-fps`）。
- 標準只寫在本文件與 PvP 自己的測試裡。Engine、共通層、工具不知道這些路徑（AGENTS §7、§9），刪 PvP 時一起刪。
- 素材根的位置（`assets/<id>`）、`content.json`、打包契約是 Engine 的契約，不在本文件的範圍，見 [creating_apps.md](../creating_apps.md)。
- pack 清單與授權記錄見[附錄 A](#附錄-a素材包清單與授權記錄)。原本記在描述 `object_fps`（v1）的 `docs/architecture/3d-assets.md`，該產品已移除（2026-11-12，`v8-arch-remove-object-fps`），授權段移入本文件、原文件刪除。

## 2. 結構

原則：

- **依對象分資料夾**：一個角色、一個配件、一個武器視角各一個資料夾。模型、定義、專屬貼圖、animset、anim_events 放在一起。
- **依種類分的資料夾只有這幾個**：動畫庫、字型、UI、音效、規則資料，以及兩個以上對象共用時才建的貼圖。
- **原始檔不進 repo**（第 8 節）。

```text
assets/object_fps_pvp/
├── content.json, asset_catalog.json        Engine 契約（2 個 manifest）
├── characters/
│   ├── player/                             模型、character.json、presentation.json、animations.animset.json、
│   │                                       ［v8］hitbox.json、textures/（有才建）
│   └── hairstyles/<name>/                  配件
├── weapons/<weapon>/{viewmodel,world}/     模型＋定義＋animset／anim_events／event_sounds＋textures/
├── animations/<library>/                   clip 來源模型
├── fonts/
├── ui/                                     gyo.ui 文件、HUD 參數
│   └── textures/                           ［v8］UI 圖片
├── audio/[<group>/]<name>.wav, audio/sfx.json
├── rules/<topic>.json                      Match 規則資料（movement_rules.json，依路徑載入）
├── maps/                                   地圖一組平放：arenas.json、pvp_<name>.json，以及 arena 以相鄰檔名
│                                           引用的烘焙地圖 pvp_<name>.voxel_mesh.json（gyo.voxel_mesh）
└── textures/<group>/                       兩個以上對象共用時才建（目前沒有）
```

## 3. 各種檔案放哪裡

| 種類 | 位置 | 命名 |
|---|---|---|
| Manifest | 根目錄，固定 | `content.json`、`asset_catalog.json` |
| 定義（character、presentation、viewmodel、hitbox） | 對象資料夾 | 檔名就是 schema 名：`character.json`、`presentation.json`、`viewmodel.json`、`hitbox.json` |
| animset、anim_events、event_sounds | 被它驅動的對象資料夾 | `<set>.animset.json`、`<set>.anim_events.json`、`<set>.event_sounds.json`（v8-product-event-sounds-migrate） |
| 3D 模型 | 對象資料夾 | 交付檔名，大小寫不改 |
| clip 來源模型 | `animations/<library>/` | 交付檔名；只有這裡的模型可以被別的資料夾引用 |
| 專屬貼圖 | `<對象資料夾>/textures/` | 交付檔名；轉換過的用轉換後的檔名 |
| 共用貼圖 | `textures/<group>/` | 第二個對象要用時才建 |
| UI 文件、HUD 參數 | `ui/` | 小寫 snake_case |
| UI 圖片 | `ui/textures/` | 交付檔名 |
| 字型 | `fonts/` | 交付檔名 |
| 音效 | `audio/[<group>/]<name>.wav`（48 kHz PCM16，Engine 的音效契約） | 小寫 snake_case |
| 音效設定 | `audio/sfx.json` | |
| Match 規則資料 | `rules/<topic>.json`，列進同一份 catalog | 主題名，小寫 snake_case |
| 地圖 | `maps/`，arena 和它引用的烘焙地圖放在一起 | `pvp_<name>.json`、`pvp_<name>.voxel_mesh.json` |
| 只有測試用的素材 | `tests/object_fps_pvp/fixtures/`，不出貨（第 7 節） | |
| 原始檔、交付包、原始錄音 | repo 外（第 8 節） | 依交付版本 |
| 生成腳本（例如程序生成的音效） | `apps/object_fps_pvp/<子目錄>/` | 腳本裡不寫 AssetId |

各第一段資料夾能放的 catalog type：

| 資料夾 | type |
|---|---|
| `characters/`、`weapons/` | model、texture、text |
| `animations/` | model |
| `fonts/` | font |
| `ui/` | text；`ui/textures/` 放 texture |
| `audio/` | sound、text |
| `rules/` | text |
| `maps/` | text |
| `textures/` | texture |

在 `characters/`、`weapons/`、`ui/` 裡，貼圖一定放在名為 `textures` 的資料夾。

## 4. 命名

- 作者自己寫的檔案（JSON、轉換出的 WAV）與所有資料夾，一律小寫 snake_case。
- 交付的模型、貼圖、字型保留原檔名與大小寫。
- 同一個 product 內不能有只差大小寫的路徑。檢查比對 catalog 的路徑字串，不靠掃檔，因為 macOS、Windows 的檔案系統不分大小寫。
- 既有的不一致只在「有 Feature 碰到它」的批次才順便修，不為了整齊追溯改名。

## 5. AssetId 與 catalog

- AssetId 是契約。整理結構只改 `asset_catalog.json` 的 `path`，不改 `id`，C++ 不動。
- id 的語法：`object_fps_pvp` 開頭，之後是一個以上以 `.` 分隔的段，每段是小寫英數與底線。中間的段（例如 `model`、`texture`）不檢查。
- type 和副檔名要對得上：

  | type | 副檔名 |
  |---|---|
  | model | `.fbx` |
  | texture | `.png` |
  | font | `.ttf` |
  | sound | `.wav` |
  | text | `.json` |

  要用新的 type 或副檔名時，在這張表與守衛的對照表各加一行。
- 換版本時新增 id（例如 `player_v19` → `player_v20`），不沿用舊 id 裝新內容。

## 6. 依路徑載入的檔

- 一般的素材由程式以 AssetId 經 catalog 讀取。
- 依路徑讀的檔，在 `test_asset_catalog.py` 的 `RETAINED` 列出 id 與理由。目前有 6 個：`maps/` 的 `arenas.json`、`pvp_arena.json`、`pvp_corners.json`、`pvp_arena.voxel_mesh.json`、`pvp_corners.voxel_mesh.json`，以及 `rules/movement_rules.json`。
- 新增依路徑載入的檔時，也要列進 `RETAINED`。位置照第 3 節的種類決定，不因為依路徑載入就放根目錄。
- 依路徑載入的檔放在哪裡不寫成固定清單：hitbox 由誰、怎麼讀還沒定，而 Match 也已經開始經 catalog 以 AssetId 讀檔。

## 7. 只有測試用的素材

- 出貨的素材都要被產品用到：被 `apps/object_fps_pvp/` 的程式以 id 引用，或被這樣的素材的內容引用（可以一層一層往下）。只被測試引用的不算。
- 只有測試用的素材放 `tests/object_fps_pvp/fixtures/`，不列進產品的 catalog。

## 8. 來源與授權、原始檔

- 每個不是 text 的 catalog 項目（model、texture、font、sound），在 `tests/object_fps_pvp/asset_sources.json` 記一筆：
  - `source`：來自哪裡（pack 名、交付者、轉換方式）。
  - `license`：授權。沒有資料時寫 `unrecorded`，不猜。
  - `record`：這個事實記在哪裡（文件、commit、檔案本身的 metadata）。
- pack 的授權（例如 Universal Animation Library、Universal Base Characters 為 CC0）記錄在[附錄 A](#附錄-a素材包清單與授權記錄)。只在那裡維護，不在兩處維護同一事實。
- text 項目由作者在 repo 裡寫，來源就是 git 歷史，不用記。
- 原始檔（DCC 原檔、交付包、原始錄音）不進 repo，放 repo 外的美術儲存：主 checkout 的 `build/art-source/object_fps_pvp/<對象>/<交付版本>/`（git 不追蹤，不上 GitHub）。對象的路徑和本資料夾裡的一致，例如 `weapons/nailer_v2/viewmodel/version02/`。
  - 這裡的檔大多無法從 repo 重建：清建置輸出時只清 `build/target/`，不要整個清掉 `build/`。說明與清單在該資料夾的 `README.md`。
  - 交付檔不要留在 `assets/object_fps_pvp/` 裡：未追蹤的檔會讓本機的 `object_fps_pvp.asset_catalog` 失敗；和要拉進來的檔同路徑時，即使內容相同，git 也會拒絕更新。

## 9. 例外

- 現有結構不符標準的地方列在 `test_asset_catalog.py` 的 `LAYOUT_EXCEPTIONS`。每筆寫違反的規則、路徑，以及由哪一批解除。
- **例外只能減少**：
  - 新增例外等於放寬守衛，要經使用者核准（AGENTS §14.4）。
  - 某一批解除了不一致，就在同一批刪掉對應的例外。例外不再需要時守衛會失敗，所以例外不會留著過期。
  - 刪除已解除的例外只締不緩，已經事先核准，事後在 dev_log 報告 AGENTS §14.4 的 1～5 項。
- 依路徑載入的檔原本放在根目錄，是例外；2026-11-05 由 `v8-product-asset-path-loaded-move`（併進 `v8-product-arena-v2`）搬進 `maps/`、`rules/`，例外已刪除。

## 10. 工具與其他程式

- 工具（audio_editor 寫入 catalog）與工具和遊戲共用的 `framework/` 保持中立。PvP 的標準不寫進工具，落點由 PvP 的守衛在 CI 上檢查。
- 其他 product、Engine、共通層不引用 PvP 的素材路徑。

## Design Invariants

改這一節（含刪除）要經使用者同意（AGENTS §3）。違反這一節的變更，再小也是 Architecture Delta。

| 編號 | 約束 | 守衛（`tests/object_fps_pvp/test_asset_catalog.py`） |
|---|---|---|
| DI-1 | `assets/object_fps_pvp/` 只放 catalog 列出的檔與 2 個 manifest；catalog 列出的每個檔都存在，而且被產品用到（第 7 節）。 | `test_every_asset_file_is_in_the_catalog`、`test_every_entry_points_at_an_existing_file`、`test_every_id_is_used_by_the_product` |
| DI-2 | 根目錄只放 2 個 manifest；第一段資料夾只能是第 3 節列出的種類，各放它能放的 type；貼圖放在 `textures` 資料夾。 | `test_paths_follow_the_layout` |
| DI-3 | 自足引用：text 項目以 id 引用的 model 與 texture，檔案要在該 text 項目的資料夾或它的 `textures/` 下。例外只有 `animations/`、`textures/<group>/`。 | `test_definitions_reference_their_own_files` |
| DI-4 | 整理結構時不改 AssetId，只改 catalog 的 `path`。 | 無自動守衛；審查時確認 diff 裡 catalog 只改了 `path` |
| DI-5 | 新增依路徑載入的檔，要列進 `RETAINED` 並寫理由。 | `test_every_id_is_used_or_retained_with_a_reason`、`test_retained_ids_are_still_in_the_catalog_and_otherwise_unused` |
| DI-6 | Engine、共通層、工具、其他 product 不寫 PvP 的素材路徑。 | `test_no_other_owner_names_a_pvp_asset_path` |

命名（第 4 節）、id 與 type（第 5 節）、來源記錄（第 8 節）也由守衛檢查（`test_names_follow_the_naming_rules`、`test_no_paths_differ_only_by_case`、`test_ids_and_types_match`、`test_every_file_asset_has_a_source_record`），但不列為 Design Invariants：它們是標準的細節，可以隨 Feature 調整。例外的政策見第 9 節，`test_every_exception_is_still_needed` 檢查例外沒有過期。

### Replacement Criteria

出現下面任何一種情況時，不再修補本標準與守衛，改向使用者提出「重新設計還是繼續修補」：

1. 例外表或種類表要靠條件（「若…則…」、依檔案內容判斷）才寫得出來。
2. 新增一種素材時，必須改守衛的判斷邏輯，而不只是在表裡加一行。
3. 例外不減反增：連續兩批都需要新增例外。
4. 第二個啟用中的 product 需要同一套標準，或工具需要讀這套標準才能把素材放對位置。這時標準要變成明確的 Data Contract，屬 Architecture Delta。

## 附錄 A：素材包清單與授權記錄

2026-11-12 自 `docs/architecture/3d-assets.md` 第 146～212 行移入（`v8-arch-remove-object-fps`）。原文件描述已移除的 `object_fps`（v1）的素材組裝，隨該產品刪除；這一段記的素材包授權仍有效，因為 `assets/object_fps_pvp/animations/ual_mannequin/` 用的是同一個包，所以歸到實際使用者 PvP。下文照原文翻譯，事實與日期不改；描述 v1 組裝行為的句子保留為當時的紀錄。原文見 git 歷史 `56b8dda3`。

### A.1 歷史來源清單

2026-09-20 的歷史來源清單記錄了 **511 個檔／1,076,255,418 bytes**，包含 189 個 FBX、72 個 Blender 檔、10 個 Blender 備份、61 組 OBJ/MTL、18 組 glTF/BIN、5 個 GLB、66 個 PNG，以及隨附的參考資料、readme 與授權檔。清單（`apps/object_fps/art_source/source_inventory.json`，放在本機的美術儲存，從未進 repo）的每一筆記錄原始絕對路徑、相對於 repo 的封存路徑、bytes、SHA-256、runtime 複本與只留在原始檔的理由。Runtime 對應另記 catalog ID 與 type。二進位 FBX 的項目含 mesh／骨骼／材質／片段的 metadata 與材質樣板、骨骼階層、原始 rest／bind 指紋、座標設定與原始權重統計；glTF/GLB 的項目含外部引用是否存在。其他創作檔只記身分與格式，不解碼。

| 封存的 pack | 檔數 | 選進 runtime 的內容／延後的內容 |
| --- | ---: | --- |
| Animated FPS Guns, June 2018 | 25 | 只有 Pistol。其他武器、Blender 與 OBJ 留在原始檔。 |
| Retro Weapon Pack V1 | 130 | 全部只留在原始檔：分開的手臂／槍與 84 個只含動畫的 FBX（手臂 70、槍 14）需要額外組裝／外部綁定。 |
| Ultimate Gun Pack, July 2019 | 222 | 只有 Pistol_1。其他武器／配件與其他格式留在原始檔。 |
| Universal Animation Library Standard | 9 | Standard FBX 與它自己的 mannequin。Root Motion 與其他格式留在原始檔。 |
| Universal Animation Library 2 Standard | 13 | 全部延後，包括 Root Motion 與分開的女性 mannequin。 |
| Universal Base Characters Standard | 112 | 兩個 FullBody FBX 與六份角色專屬的貼圖複本。其餘髮型、變體、normal／roughness 貼圖與其他格式留在原始檔。 |

### A.2 模型的限制（當時的檢查結果）

兩個 superhero 模型有三個原始 mesh、65 個骨骼節點，沒有片段。UAL1 與 UAL2 各含一個 mannequin mesh、65 個骨骼節點與 43 個片段。檢查原始檔發現這些人形來源的骨骼名稱／階層相同，但 superhero 的 rest transform 與蒙皮綁定和兩個動畫庫不同。UAL1／UAL2 在比對過的本地 rest transform 與 bind 矩陣上一致；這不代表 Engine 實作了節點對節點的對應。Retro 的手臂與其動畫檔共用 63 根骨骼的階層，同樣不能把一個檔的節點索引匯入另一個檔。Animated Pistol 有七個骨骼節點、四個 skin cluster，每個控制點最多兩個影響。

因此（v1 的）可執行組裝只在 AnimationSet 的 canonical model ID 與不可變的來源模型歸屬都和角色／武器模型相符時才接受它。不同檔案即使骨骼名稱或階層相同也拒絕。原始指紋是清單的證據，不是相容性的關卡；節點索引、參考姿勢、單位、座標轉換與綁定都有關係。不提供 retargeting、外部綁定、root motion 抽取或新的動畫狀態機。UAL1 只和它自己的 mannequin 一起使用。superhero 的定義刻意沒有 AnimationSet。

匯入器維持既有的「取最大四個權重再正規化」。正權重超過四個的原始控制點：

| 原始身體 mesh | 受影響的控制點 | 捨棄的權重占原始總權重的最大比例 |
| --- | ---: | ---: |
| Superhero male | 275 / 6,287 (4.374%) | 7.6608% |
| Superhero female | 130 / 6,408 (2.029%) | 2.2612% |
| UAL mannequin | 145 / 6,994 (2.073%) | 9.5054% |

這些數字指原始控制點，不是 renderer 拆開後的頂點。CPU 蒙皮測試涵蓋的是匯入後的表示；它們不證明和保留所有原始權重的 DCC 工具逐像素相同。

部分原始 glTF 引用了不存在的 `T_Eye_Normal_png.png`，男性模型另有 `T_Hair_1_Normal_png.png`；對應的不帶 `_png` 的檔在 pack 的其他位置。清單保留並回報這些缺失的引用。選用的 runtime FBX／base-color PNG 路徑不依賴那些 glTF 引用；原檔不改寫。

### A.3 授權

Ultimate Gun Pack、兩個 UAL pack 與 Universal Base Characters 隨附的本地授權檔都寫明 CC0。Retro 隨附的 `Readme.pdf` 允許個人／商業使用，並寫明不需要標示出處。Animated FPS Guns 沒有隨附授權檔或 readme，所以它的授權狀態在這份封存裡仍未記錄；不把其他 pack 的 CC0 套到它身上。所有隨附的授權／參考檔都逐 byte 保留在各自的 pack 裡。這是隨附文件的紀錄，不是替代授權，也不對未提供的條款做任何主張。
