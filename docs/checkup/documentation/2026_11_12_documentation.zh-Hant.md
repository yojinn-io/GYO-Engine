# 文件健檢（2026-11-12）

基準：master `74e0eb07`（PR #185 合併後）。上一份報告的基準是 `5ee499a`（2026-10-21），中間約 1300 個 first-parent commit。
範圍：README 第 1～9 項，加上架構師的[交接清單](2026_11_12_handoff_from_architect.zh-Hant.md)。判斷標準與步驟見 [README](README.md)。
做法：第 1 項（與程式碼不符）與第 2、5 項（狀態、dev_log）各交一個只讀子 agent（general-purpose，Opus 5.5，medium）掃描，結論由本會話逐條反查後才改；其餘項目由本會話以腳本（git grep、Python）掃描，每個掃描都先以已知例子命中一次。掃描的中間輸出在會話 scratchpad，未保存，只作出處。

## 結論

- **計畫文件的狀態行大量落後**：約 40 處寫「進行中」「待合併」「未開始」，實際已完成並合併。PR 號與 merge commit 沒有寫錯的，全是沒跟上。本變更在各狀態行補「2026-11-12 圖書管理員補註：後續：…」，PR 號與 merge commit 逐一以 `git log --merges` 核對。
- **PvP 入口文件的協議版本停在 pv7**：程式碼是 pv9（PR #128 起），`network-architecture.zh-Hant.md` 的開頭、第 1 節的圖、第 3 節的版本與 UDP 標頭表已改。和上一份報告的 pv6→pv7 是同一種落後。
- **`-remove-object-fps` 留下的 Object_FPS 現況描述**已改（交接清單 §2）。
- **跨元件規則登記頁**建立（`docs/cross-component-rules.md`，交接清單 §3）。
- **機械檢查大致乾淨**：現行文件的斷鏈 1（已修）、多語版本 0 差異；繁中檔混入的日語標籤與異體字已改。
- 有幾處屬於 owner 的狀態或架構說法互相矛盾，列在本文末「交給 owner」與交接清單的「回報架構師」。

## 一、與程式碼不符（第 1 項）

已改：

| 位置 | 原文 | 實際 |
|---|---|---|
| `object_fps_pvp/network-architecture.zh-Hant.md` 開頭、第 1 節圖、第 3 節、UDP 標頭表 | pv7／pv8、`client_v7`／`runtime_v7` | pv9（`Wire.hpp:17`、`adapter.go:17` 都是 9；proto 只有 `*_v9.proto`） |
| `architecture.md` 的 match_domain 段、`network-architecture` 同段 | 不連 `PlayerController`、`Player`、`PlayerSettings`、Campaign | 這些型別已不存在；改寫成不連 Client 專用的 `client_simulation`、`app_support`（`apps/object_fps_pvp/CMakeLists.txt:41-55`，依賴方向是它們依賴 domain） |
| `object_fps_pvp/entity-model.zh-Hant.md` G2 | `ColumnIdsForTest()` | `PvpMatchTestAccess::ColumnIds`（`tests/object_fps_pvp/EntityModelTests.cpp:20`） |
| `object_fps_pvp/entity-model.zh-Hant.md` §1.5 | split 前的成員名與行號 | 加註「撰寫時、split 之前」與現在的欄名 |
| `architecture/determinism.md` 兩處 | `FromAxisAngle` | `MakeQuaternionFromAxisAngle`（`Quaternion.hpp:65`） |
| `ui_toolchain.md` | action 例子 `object_fps_pvp.start_game` | 真實存在的 `pvp.join`（`pvp_lobby.json`） |
| `architecture/{animation-events,collision,map-markers,voxel-mesh}.md` 開頭 | header 路徑寫成縮寫 | 補成 `engine/<m>/include/engine/<m>/…` |

只記錄：`entity-model.zh-Hant.md:73` 的 `EntityAllocation.hpp` 是契約定稿時規劃、尚未實作的檔（功能線停在 kernel 改造之前），不是錯。

## 二、狀態不符與 dev_log 缺漏（第 2、5 項）

已補「後續」的狀態行（每一條的 PR 與 merge commit 已核對）：

- Engine：audio（README 批次表、HANDOFF 兩處）、event-sounds（README、HANDOFF）、framework-preview、framework-rework（README、HANDOFF）、entity-store、third-party-evaluation、determinism（DET-0～3 暫停）、input-and-present（HANDOFF、PLAN、README）、time-threads-trace（WakeRecord 已進 master）、foundation-followups（PLAN、inactive_products）。
- PvP：v7.1 README 的 after-re-evaluate、sdl-isolation；v8 README 的狀態行、hitbox-line、asset-layout、entity-line、pm-detect-diag。
- 工具：v8 README、HANDOFF 三處、model-viewer、voxel-map-editor、voxel-map-tool-chunked、voxel HANDOFF 兩處。
- 停線紀錄：產品線、工具線、voxel 線的 HANDOFF「現況」開頭補 2026-11-11 停線（使用者 2026-11-12 決定由圖書管理員直接補）。
- dev_log 狀態行：`2026_10_31_pvp_v8_nailer_tps`、`2026_11_07_pvp_v8_pm_detect_diag`、`2026_10_24_pvp_v71_system_observe_test`、`2026_10_29_pvp_v71_player_v20`。

補列進計畫 README 的 dev_log：audio 的 disk_flake；v7.1 的三份；v8 的 sfx、lobby_text_field、lobby_fullwidth、nailer_tps、arena_v2，asset-layout 線改指 `asset-layout.md`；工具 v8 的 voxel core、window 與 voxel 設計稿。

交給 owner（本變更不改）：

| 位置 | 問題 | 交給 |
|---|---|---|
| `architecture/plans/kernel/README.md:3`、`:24`、`:54`、`:55`、`:62` | 狀態寫「設計稿未開工」、規格 v0.1、待定 K.3、-spike 沒寫 PR #180、#173 待合併 | 架構師（PR #188 已合併，這頁仍未改） |
| kernel 線 `-spec`（v0.1，PR #179） | 沒有獨立 dev_log | 架構師（dev_log 由做那一批的會話寫） |
| `object_fps_pvp/plans/v8/HANDOFF.md:3`、`:47`、`:50-51`、`:54`、`:217` | 狀態行停在 10-30；audio、工具計畫、sfx 的來源、#127 都已完成或合併 | 產品會話（PR #188 已合併，這頁仍未改） |
| `object_fps_pvp/plans/v8/README.md` 的樹狀總覽 | audio-line 三批、pv8-design、anim-authority、hitbox 各批等沒有標 ✓ | 產品會話 |
| `docs/tools/plans/v8/HANDOFF.md` 的「已知缺陷（改造期間不修）」 | 寫在分支 `claude/v8-tool-line-records-2`（`920b6fc6`），沒有開 PR，master 上沒有 | 工具線 |

## 三、斷鏈（第 3 項）

- 現行文件 1 處：`object_fps_pvp/plans/v8/asset-layout.md:29` 連到已刪的 `3d-assets.md`，改指 `assets.zh-Hant.md` 附錄 A 並加註。
- 不算問題：`architecture/determinism.md:190` 與 `plans/entity-store/delta.zh-Hant.md:126-138` 是引用的「預定寫進另一份文件的條文」，連結以目的檔的位置寫，在本檔看起來斷。dev_log 內的 6 處是歷史連結。
- `3d-assets.md` 的入鏈：現行文件已無；`plans/` 與 `dev_logs/` 的歷史連結照交接清單不改。

## 四、各語言版本（第 4 項）

README 三語、`releasing`、`rendering_architecture`：標題結構、程式碼區塊數、反引號內容一致，連結只差各自的語言版本。三語 README 的文件一覽加了目標架構、kernel 計畫與跨元件規則登記頁（交接清單 §4）。

## 五、用字（第 6 項）

- 「範圔」5 處、「範圲」1 處改成「範圍」（big-map 計畫兩處之外，還有 kernel audit、scene README、pv10-requirements、spike-pageguard-report）。scene README 的標題改了，全 repo 沒有指向舊錨點的連結。
- 繁中檔裡當標籤用的日語：big-map 計畫的「採る／採らない／改装／當てはめ／弱い」改為「採用／不採用／改裝／套用／弱」；big-map、工具 v8 的「ギア」改為「檔位」（標題的錨點沒有入鏈）。
- 不改：引用 AGENTS.md 條文名（「範囲の限界」「緩める面」「Architecture の反復を計画する条件」等）、引用使用者原話（voxel feel-survey）、健檢與架構健檢的歷史報告、`protocol-v7` 引用原意的「接口」。
- `object_fps_pvp/assets.zh-Hant.md` 附錄 A（從英文翻的）：沒有發現大陸用語或日文字形。

## 六、證據的所在（第 8 項）

- 新增 2 處補「未保留」：`object_fps_pvp/plans/v7.1/player-v19-judgment-change.md:8`、`sdl-isolation-j1-judgment-change.md:65`。
- 其餘命中是程序上的用法（工作樹路徑、base 樹、輸出位置）或已標「未保存」「只作出處」「不算證據」。
- 只記錄：`tools/plans/v8/model-viewer-trial.md:60`、`:66` 的命令寫死已刪除的 `.claude/worktrees/trusting-joliot-f13b78`，照著跑會失敗；屬工具線的試用手冊，歷史紀錄。

## 七、目標架構與現況分開（第 9 項、交接清單 §4）

- `target/` 的 9 份部件文件都有 `> Status: target, not implemented`；`target/README.md` 沒有 Status 行。
- 同名並存：`determinism.md`、`error-handling.md` 在頂層與 `target/` 各一份。兩份都是 target、尚未搬回，依規則搬回時取代，不算違反。
- `limits.zh-Hant.md`：LIM-1～LIM-6 各一列、沒有重複；部件文件引用的 LIM 都存在。
- 頂層現況文件沒有頁首 Status 行；`determinism.md`、`error-handling.md`、`entity-store.md`、`architecture.md` 指向 target 的段落都以 `v8-arch-kernel-delta-approved`／spec K.3 為據，措辭一致，連結都通。
- `target/README.md` 的目錄和 spec 引用的 target 檔一致（spec 沒有引用 scheduler，目錄有）。
- 舊架構文件何時換（使用者要求與架構師確認，2026-11-12）：逐部件，在該部件的遷移批完成時由 target 版取代、舊檔刪；遷移批待 `v8-arch-migration-plan` 排定。遷移前只做整體標示：本變更在 `architecture.md`、`determinism.md`、`error-handling.md`、`entity-store.md` 檔頭加一行指向 target（不是 Status 行）；framework 三份 deprecated 文件保留到 `framework/` 程式刪除時一起刪。規則寫進 `target/README.md`「命名」節。
- 之後使用者核准 `v8-arch-docs-legacy`（架構師轉達，使用者 2026-11-12 在本會話確認）：設計權威即刻移到 `target/`，上述檔頭行改為 `> **Legacy**` 標示，清單擴大到 framework 三份與 `entity-model`、`network-architecture`；原地標示、不搬到 `legacy/` 資料夾（約 300 個引用，含約 40 處程式註解與守衛測試、約 70 處不改寫的 dev_log；部件遷移完成時本來就會刪檔）。規則寫進 `target/README.md`，檢查清單第 9 項加 legacy 的檢查。
- framework 三份文件用 `> Status: deprecated`：它們不在 `docs/architecture/`，不受 target 的兩種 Status 規則約束。

## 八、規則的遵守（第 7 項）

- 架構師交接的事項本身就是一例：三線停線紀錄約定「下次變更時補」，到現在三線都沒有變更，所以都還沒補。約定本身沒有被違反，但「下次變更時」對停線中的線等於不會發生。使用者 2026-11-12 決定由圖書管理員直接補；本變更已在三線 HANDOFF 的「現況」開頭補上停線紀錄。
- 計畫狀態行落後：AGENTS.md §15 與 HANDOFF 的「每批開始、里程碑、停止時同一變更更新」被遵守在 HANDOFF 的日期條目上，但 README 與 HANDOFF 開頭的狀態行常常沒跟著改。這是本次最大宗的問題。

## 跨元件規則登記頁（交接清單 §3）

`docs/cross-component-rules.md`（英文）：頁首用法；每列「規則｜現況歸屬｜目標歸屬｜守衛｜已知實作」，現況與目標分欄（framework 退役到 `plugins/` 的計畫寫在目標欄）。第一條 CCR-1 選擇窗記帳規則的每個位置都照原始碼核對過（2026-11-12）：ui_editor `PathDialogRequests` 與 audio_editor `ApplyDialogAnswers` 合規；model_viewer `ViewerWindow.cpp:43`、`:112`、`:621-627` 不合規（讀碼結論，未實機重現）。工具線的缺陷紀錄沒有合併，所以引用 kernel README 的同一條。入口：`docs/architecture.md`「Repository ownership」與三語 README。
