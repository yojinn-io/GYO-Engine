# 2026-11-23 `transform` 的父子傳播（SDL 線子批 X2，`v8-arch-gyo2-sdl-transform-hierarchy`）

Owner：架構（GYO2 主會話開的任務卡「開X2、Object_Connect 移植準備的卡」）。分支 `claude/gyo2-sdl-x2-hierarchy-9f813f`（開向 `gyo2`，基準 7be54ccb）。作者 Opus 5.5。
一頁短說明（取代事前宣告，照 AGENTS 核心版 §2.6）：[`transform-hierarchy.zh-Hant.md`](../architecture/target/v2/transform-hierarchy.zh-Hant.md)。

## 做了什麼

- `engine/modules/transform`：`SyncTransforms` 換成 `PropagateTransforms`（Bevy 的 `sync_simple_transforms`＋`propagate_parent_transforms` 併成一個），讀 Kernel 的 `ChildOf`／`Children`：根＝`ToMatrix(transform)`，根以下＝父的 `GlobalTransform` × `ToMatrix(transform)`；重算的條件照 Bevy（`Changed<Transform>`、`Added<GlobalTransform>`、`Changed<ChildOf>`、孤兒、父被重算）；自己的堆疊、不遞迴。孤兒用 System 的 `Local`（上一次帶 `ChildOf` 的實體）。set、預設的 stage、選項不變。
- 文件：`transform.zh-Hant.md`（用法與後果、TF-1、TF-3 改寫，TF-5 新增，RC-TF1 補句）、新頁 `transform-hierarchy.zh-Hant.md`、README 兩列、`config/modules.csv` 的說明。
- 測試：`tests/common/transform` 加 H-1～H-7（10 案）。
- 突變：`transform_mutations.json` 加 8 個 `tf-h-`，X1 的 `tf-sync-not-mounted`、`tf-sync-ignores-changed`、`tf-links-more` 的 find 跟著改。

## 驗證（本機：Intel Mac，macOS，`build/target/_build/test`，RelWithDebInfo）

- `ctest -L cpu`：208/208 通過兩次（review 修正之前與之後）；`gyo_transform_tests` 21 案。
- 突變（`transform_mutations.json` 全部，D184：本批改了 transform 的程式與測試）：第一輪 17/17 killed；review 修正（加 2 個）之後全部重跑 19/19 killed。

- L1 三平台：PR 開出後手動 dispatch，結果記在 PR。

## 經過與判斷

1. H-1 的「`Update` 生成的子」第一次紅：Host 的 `World::Insert` 不移動配發器，父放在 1、生成的子也拿到 1，`ChildOf{1}` 成了指向自己、被 Kernel 拒絕。是測試的寫法，父改放 1000。
2. 設計取捨（兩個 System 併成一個、孤兒用 `Local`、不查環、`Added<GlobalTransform>` 照 Bevy）與和 Bevy／Unity 的對照在短說明第 3 節。
3. RC-TF1：R2b 還沒宣告幀時間預算，本批不量測（短說明第 5 節）。
4. code review（Opus／high，**作者與審查者同為 Opus**：Fable 用量不足）：blocker 0、should 2、nit 4，全部處理：
   - should：沒有「只移動子」的案例（刪掉子的 `Changed<Transform>` 判斷不會被抓）→ 加 H-2 第二案與突變 `tf-h-own-transform-ignored`。
   - should：TF-5 與 `Changed<GlobalTransform>` 的文字少列「祖先的 `ChildOf` 被寫、祖先成為根、`GlobalTransform` 剛加上」（重寫的旗標往下傳，同 Bevy）→ 改文字；程式不變。
   - nit：短說明「不一致不會發生」太強——`Restore` 照給的放回 `Children` 時不維護（DI-KS 33），作者給錯（違反 R-2 前提）時可能成環、傳播不結束 → 寫明前提，不加檢查（AGENTS §3）。
   - nit：H-5 的根 6 不跑傳播也會過 → 改放單位矩陣；`Restore` 沒放回父的孤兒路沒有案例 → 加 H-6 第二案與突變 `tf-h-orphan-missed-restore`；測試 CMakeLists 的「TF-1 to TF-4」→ TF-5。

## 證據

主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-sdl-x2/`（本機 Intel Mac，git 不追蹤）：`ctest-cpu.txt`、`ctest-cpu-2.txt`、`mutations.txt`、`mutations2.txt`、`build.txt`、`build2.txt`。code review 的報告未另存檔（重點在本紀錄第 4 項）。

## 後續（回報主會話）

- 從 `Transform`＋`ChildOf` 算全域姿態的輔助（Bevy `TransformHelper`）沒加：CCR-12 要判定自己算，目前沒有判定讀姿態。
- 批次表 SDL 列的 X2 狀態由主會話更新。
