# PvP v8：buns 髮型改成測試 fixture（v8-product-asset-buns-fixture）

日期：2026-11-01。分支 `claude/v8-asset-layout`（PR [#125](https://github.com/yojinn-io/GYO-Engine/pull/125)）。計畫：[asset-layout](../object_fps_pvp/plans/v8/asset-layout.md)。標準：[PvP 素材標準](../object_fps_pvp/assets.zh-Hant.md)。

**狀態：完成。**

## 為什麼

- 支撐的 Feature：出貨內容只放產品用到的素材（AGENTS §7；素材標準 DI-1、第 7 節）。
- buns 髮型（`Hair_Buns.fbx`、`character.json`）與它的貼圖 `T_Hair_2_BaseColor.png`（約 2 MB）只有 `PlayerPresentationTests` 在用，卻列在產品 catalog 裡跟著出貨。貼圖還放在沒有模型的 `characters/superhero_female/textures/`，buns 的定義跨資料夾引用它（DI-3 的例外）。

## 做了什麼

- 3 個檔以 `git mv` 搬到 `tests/object_fps_pvp/fixtures/hair_buns/`（`Hair_Buns.fbx`、`character.json`、`textures/T_Hair_2_BaseColor.png`），內容不變；刪掉留下的空資料夾（`characters/hairstyles/`、`characters/superhero_female/`）。
- 新增 fixture 自己的 `asset_catalog.json`（3 筆，id 沿用原本的 id）與 `README.zh-Hant.md`（用途、來源與授權）。
- 產品的 `asset_catalog.json` 刪掉這 3 筆；`asset_sources.json` 刪掉 2 筆（text 的定義本來就不用記）。來源與授權改記在 fixture 的 README。
- `PlayerPresentationTests.cpp`：載入組裝後的產品 catalog 後，用 Engine 既有的 `AssetCatalog::AppendFromFile` 附加 fixture 的 catalog（以 fixture 資料夾為根）。測試的期望值不變（id 沒改）。
- `tests/object_fps_pvp/CMakeLists.txt`：presentation 測試加編譯定義 `PVP_PRESENTATION_FIXTURE_DIR`（和既有的 `PVP_DOMAIN_TEST_ARENA_PATH` 等同一種做法）。
- `test_asset_catalog.py`：刪掉 4 筆例外（buns 的 own-files 1 筆、product-use 3 筆）與不再使用的常數。

## 判定變更的報告（AGENTS §14.4，刪除例外，已事先核准）

1. 守的性質：出貨的素材要被產品程式用到（DI-1）；text 項目引用的模型與貼圖在自己的資料夾（DI-3）。
2. 緩和的地方：沒有。
3. 收緊的地方：只被測試引用的素材、以及 buns 這種跨資料夾引用貼圖的定義，從「例外放行」變成失敗。
4. 沒有緩和，不適用。
5. 根據：搬移後 `test_every_exception_is_still_needed` 要求刪掉這 4 筆；刪掉後 `test_every_id_is_used_by_the_product`、`test_definitions_reference_their_own_files` 通過。

`PlayerPresentationTests` 的判定沒有改：同樣的配件、同樣的 id、同樣的斷言，只是資料改從 fixture 讀。

## 驗證

- test preset 建置 `gyo_object_fps_pvp_presentation_tests`：組裝後的產品素材裡沒有任何頭髮檔，所以配件只可能從 fixture 讀到。
- CTest：`object_fps_pvp.presentation_cpu`、`object_fps_pvp.asset_catalog`、`object_fps_pvp.mutation_tool` 通過。
- 突變：測試檔改了，所以重跑 presentation 檢查的 12 個既有突變：10 個 killed，2 個 wrong_failure（`11-pitch-ignored`、`11-pitch-reversed`）。
  - 第一次執行時 zsh 沒有把 id 清單分開，工具回報 unknown mutant ids、一個都沒跑，不算結果。
  - 2 個 wrong_failure 的原因不在本批：它們的 `expect` 是「a positive pitch points the pistol down」，但 PR #120（commit `2a8e021e`）把測試名稱改成「…points the weapon down」，突變清單沒有跟著改。本批之前的 commit `c6d65be1` 與 master 上，這段文字只出現在 `mutations.json`，不在任何測試裡，所以這 2 個突變從 PR #120 起就不可能 killed。
  - 只為確認，把 `expect` 暫時改成「weapon」再跑：2／2 killed（改名後的測試抓得到這 2 個突變）。這個修改沒有提交，已還原；要不要修由使用者決定（AGENTS §14、§11）。
- 證據：主 checkout 的 `build/evidence/asset-layout-20261031/`（`mutations-buns-fixture.json`、`mutations-pitch-expect-check.json`，git 不追蹤）。

## 追加：修正 2 個過時的突變 expect（使用者核准）

- 使用者決定在 PR #125 一起修：「在 #125 一起修」。
- `mutations.json` 的 `11-pitch-ignored`、`11-pitch-reversed`：`expect` 從「a positive pitch points the pistol down」改成「a positive pitch points the weapon down」，對上 PR #120 改名後的 TEST CASE（`PlayerPresentationTests.cpp` 的「PvP remote upper body follows the pitch: a positive pitch points the weapon down」）。
- 理由：舊的 `expect` 不出現在任何測試裡，這 2 個突變不可能 killed，檢出力的證明因此失效。新的 `expect` 一樣只對上這一個 TEST CASE，沒有變寬（不是只寫「失敗」之類的泛用訊息）。突變本身（`find`／`replace`）不變。
- 重跑：2／2 killed。證據：`build/evidence/asset-layout-20261031/mutations-pitch-expect-fix.json`（主 checkout，git 不追蹤）。
- after-pv8 分支的測試名稱還是 pistol（在 PR #120 之前切出），已通知 v8 規劃會話：合併 master 後測試名稱會是 weapon，和這裡的 `expect` 一致。

## 範圍的限制

- 停用的 `object_fps`、`object_fps_v2` 各自的 buns 與頭髮貼圖不動（凍結）。
