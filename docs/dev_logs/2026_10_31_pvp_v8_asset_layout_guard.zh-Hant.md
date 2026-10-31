# PvP v8：素材結構的守衛（v8-product-asset-layout-guard）

日期：2026-10-31。分支 `claude/jolly-heisenberg-78148d`（從 master `3d5fe1a5`，推送為 `claude/v8-asset-layout`；中途合併 master `ebfe3225`）。計畫：[asset-layout](../object_fps_pvp/plans/v8/asset-layout.md)。標準：[PvP 素材標準](../object_fps_pvp/assets.zh-Hant.md)。

**狀態：完成。和 `v8-product-asset-layout-doc` 放在同一個 PR。**

## 為什麼

- 支撐的 Feature：hitbox 資料與之後的 v8 素材需要唯一的落點，而且落點要在 CI 上自動檢查，不靠記憶。
- 素材標準的 Design Invariants（`v8-product-asset-invariants-approve`）要有守衛。

## 做了什麼

`tests/object_fps_pvp/test_asset_catalog.py`（CTest `object_fps_pvp.asset_catalog`）：

- 每條規則寫成函式，回傳 `(規則, catalog 路徑)` 的集合，可以注入假的 catalog 測試。
- 新的檢查（`AssetLayoutTests`）：

  | 測試 | 檢查什麼 | 對應 |
  |---|---|---|
  | `test_every_id_is_used_by_the_product` | 出貨的 id 被 `apps/object_fps_pvp/` 的程式用到，只被測試引用的不算 | DI-1 |
  | `test_paths_follow_the_layout` | 根目錄的檔、第一段資料夾、各資料夾能放的 type、貼圖放在 `textures` 資料夾 | DI-2 |
  | `test_definitions_reference_their_own_files` | text 項目引用的 model、texture 在自己的資料夾或 `textures/` 下（`animations/`、`textures/` 除外） | DI-3 |
  | `test_no_other_owner_names_a_pvp_asset_path` | 共通層（最上層除了 `assets`、`docs`、`agents`、`third_party`、`build`、`apps`、`tests` 以外的所有資料夾，加上 `tests/common`、`tests/framework`、`build/acceptance/common`、`build/ci`、`build/cmake`、`build/*.py`）不寫 PvP 的素材根與 catalog 路徑；其他 product（`apps`、`tests`、`build/acceptance` 底下的其他資料夾）不寫 PvP 的素材根 | DI-6 |
  | `test_names_follow_the_naming_rules` | 資料夾與作者寫的檔案是小寫 snake_case；角色、武器資料夾裡的定義以 schema 命名 | 第 4 節 |
  | `test_no_paths_differ_only_by_case` | catalog 路徑（含 2 個 manifest）不能只差大小寫 | 第 4 節 |
  | `test_ids_and_types_match` | id 的語法；type 和副檔名對得上 | 第 5 節 |
  | `test_every_file_asset_has_a_source_record` | 不是 text 的項目在 `asset_sources.json` 都有完整記錄，沒有多餘的記錄 | 第 8 節 |
  | `test_every_exception_is_still_needed` | 每筆例外都還對應一個實際的不一致，規則名有效，寫了由哪一批解除 | 第 9 節 |

- 自我檢查（`AssetLayoutSelfTests`，11 個）：每條規則注入假條目，確認抓得到，同時確認允許的寫法不被誤判。涵蓋根目錄的檔、未知的資料夾、type 放錯資料夾、貼圖不在 `textures/`、跨資料夾引用貼圖、大小寫與命名、只差大小寫、id 語法與 type 不符、產品沒用到的 id、過期的例外、來源記錄缺漏／多餘／不完整、其他 owner 寫了 PvP 的路徑，以及共通層的掃描範圍（新的最上層資料夾不必改測試就會被掃到）。
- 初始例外 13 筆（`LAYOUT_EXCEPTIONS`），已在 Design Invariants 核准時列給使用者：

  | 規則 | 路徑 | 由哪一批解除 |
  |---|---|---|
  | root-file | `arenas.json`、`pvp_arena.json`、`pvp_corners.json`、`movement_rules.json` | `v8-product-asset-path-loaded-move` |
  | first-folder | `players/` 的 3 個檔 | `v8-product-asset-player-folder` |
  | own-files | `players/character.json`（模型在 `characters/player/`） | `v8-product-asset-player-folder` |
  | own-files | `characters/hairstyles/buns/character.json`（貼圖在 `superhero_female/`） | `v8-product-asset-buns-fixture` |
  | product-use | buns 的 3 個項目 | `v8-product-asset-buns-fixture` |
  | definition-name | `weapons/nailer_v2/viewmodel/nailer_v2_viewmodel.json` | 下一個改這份定義的 Feature 批 |

- `used_ids` 改寫：原本對每個 id 用正規表示式掃整份程式碼，一次呼叫約 2.5 秒，加上新檢查後整個測試要 12.7 秒。改成先把文字切成「英數、底線與點的連續片段」再比對，判定不變（下節），整個測試 1.1 秒。
- 新增測試 `test_a_reference_is_the_whole_id`：「比 id 長的片段（例如 `object_fps_pvp.arena.corners`）不算引用 `object_fps_pvp.arena`」原本沒有測試守著（把比對改成「包含」的突變，在改寫前後都不會被抓到）。
- DI-6 的掃描範圍：起初用固定清單列出 engine、tools 等資料夾。推送後合併 master，PR #123 新增了最上層的 `framework/`，固定清單會漏掉它。所以改成「最上層除了排除的資料夾以外全部掃」。
  - 其他 product 只檢查素材根：它們的素材也照同樣的方式擺，相對路徑可能和 PvP 的相同（例如停用產品的 `animations/ual_mannequin/UAL1_Standard.fbx`），只有素材根能分辨是不是 PvP。
- `tests/object_fps_pvp/mutations.json`：加檢查 `asset_catalog` 與 13 個突變（批次 `v8-asset-layout-guard`）。插入位置避開 after-pv8 分支改動的段落（檢查放在 `protocol_version` 後，突變放在 `11-limit-uncalibrated` 後）。

## 判定變更的報告（AGENTS §14.4）

既有測試的判定都沒有改。改了實作的是 `used_ids`：

1. 守的性質：每個 catalog id 都被用到。引用以完整的 id 為單位，比 id 長的片段不算。
2. 緩和的地方：沒有。新舊實作在 4 組實際程式碼（全部 code root、只有產品、只有 tests＋acceptance、只有 acceptance）與 20,000 段隨機文字上結果相同，共 20,004 例。
3. 收緊的地方：沒有（等價）。
4. 沒有緩和，不適用。
5. 根據：上述等價核對；突變 `id-substring`（改成「包含」）與 `id-token-split`（片段在點斷開）都被抓到。

新加的檢查是新的守衛（只締）。初始例外是新規則的現況，不是放寬既有的判定。

## 驗證

- `python3 tests/object_fps_pvp/test_asset_catalog.py -v`（合併 master `ebfe3225` 後）：27 個測試通過，約 1.2 秒。
- CTest（test preset 只做了 configure，沒有建置 C++）：`object_fps_pvp.asset_catalog`、`object_fps_pvp.mutation_tool` 通過。C++ 沒有改；全量交給 CI L1。
- 突變：`run_mutations.py --batch v8-asset-layout-guard`，**13／13 killed**（每個都以預期的測試名稱失敗；合併 master 後重跑）。
- 文件核對（doc 批的腳本重跑）：素材標準裡的 13 個測試名稱都存在；其餘需人工判讀的項目和 doc 批相同。
- 證據：主 checkout 的 `build/evidence/asset-layout-20261031/`（`mutations-guard.json`、`equiv.py`、`old_test.py`、`equiv-guard-batch.txt`、`asset-catalog-guard-batch.txt`、`doc-check-guard-batch.txt`、`README.md`，git 不追蹤）。

## 範圍的限制

- `DI-4`（整理結構不改 AssetId）沒有自動守衛，靠審查。
- `test_no_other_owner_names_a_pvp_asset_path` 只看列出副檔名的文字檔；`docs/`、`.md`、`assets/`（停用產品的 catalog 與 PvP 有相同的相對路徑）不看。工具的測試（`tests/ui_editor`、`tests/audio_editor`）和其他 product 一樣只檢查素材根；工具本身（`tools/`）檢查全部。
