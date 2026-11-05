# v8-arch-entity-store-impl-3a：驗收的事前宣告

更新：2026-11-05。Owner：Engine（架構）。**狀態：草稿，待使用者承認（AGENTS §14.1：判定在看結果前宣告並取得承認；結果出來後不放寬）。**
依據：[delta §7 第 9 項](delta.zh-Hant.md)（已核准的內容）、entity-model §9.2 的方法（件數對照與 compare 的用法）、[契約 Guard 節](../../entity-store.md)。

## 範圍

- 新模組 `engine/entity_store/` → `GYO::EntityStore`（header-only，只連 `GYO::Base`；`engine/CMakeLists.txt` 在 math 之後加一行）：`EntityId`（`std::uint64_t`）、`EntityKey<T>` trait 與 `EntityOf`、`FindEntity`／`InsertEntity`／`EraseEntity`／`StrictlyAscending`（函數名沿用）。
- common 測試 `tests/common/entity_store/`：契約 Guard 表第 1～3、10 列與 DI 9 的連結檢查（沿用 `tests/common/time/CMakeLists.txt:22` 的 `gyo_expect_links`）、字詞檢查（無產品名、無 unordered／hash／clock／random／thread／atomic）。
- Engine 的突變目錄 `docs/architecture/plans/entity-store/scripts/mutations.json`，以 `docs/architecture/plans/time-threads-trace/scripts/run_mutations.py --catalog` 執行（os-subsystem 的先例）。
- pvp 換上：`EntityId.hpp` 改為 Engine 型別的別名（保留 `PlayerId`、`NoEntity`）；`EntityColumns.hpp` 變成轉接（`EntityKey` 對 `playerId` 欄位的特化＋using 宣告；刪除產品的 `HasEntityId` 版多載）；`match_domain` 公開連結 `GYO::EntityStore`。`PvpMatch.cpp` 的 30 行呼叫不改。
- 產品測試：`EntityColumnsTests.cpp` 的 `Fresh` 搬到 common（換合成型別），`Legacy` 留在產品；`mutations.json` 的 M1、M2（`entity-split-insert-appends`、`entity-split-erase-swap-and-pop`）`file` 改到 Engine 路徑、`expect` 改成 common 測試；M3～M6 不動。
- 產品契約的註記（delta §6 列出的 entity-model §0 第 2 項、§3.1、§11 第 4～5 項、E-5 守衛欄；E-12 措辭留到 3b）；Engine 契約 `entity-store.md` 的狀態句（「Nothing in `engine/` exists yet」、Components 表各列「proposed (batch 3)」）改成已實作；本線的 dev_log、README、HANDOFF。
- 圖書管理員會話（同一 PR、日語 commit、等我通知 3a 的 commit 後）：`docs/architecture.md` 第 129 行、模組表、「Architecture changes」節的 3a 版，以及 `docs/creating_apps.md:41` 公開 engine target 清單加 `GYO::EntityStore`（圖書管理員 2026-11-05 提出，已同意）。
- **不做**：`DeferredChanges`（3b）、G12、Gateway、wire、digest 編碼、Client 行為；`docs/architecture.md` 與 `docs/creating_apps.md` 由圖書管理員做（上一項）。

## 判定（結果出來前宣告）

報告開頭一行照 AGENTS §14 的格式：`件數的對照：一致（base 與 branch 情境數 42＝42；每個情境的 record 數相等，由兩樹 `--run --ticks` 輸出的 json 逐情境比對）／不一致（…）／確認できない（…）`。

| # | 判定 | 通過條件 | 方法 |
|---|---|---|---|
| 1 | 行為不變（搬移 0 差分） | `tests/object_fps_pvp/compare_authority_trees.py --base <base runner> --branch <3a runner>`，`--scale 1` 與 `--scale 10` 各一次，exit 0，42 個情境全部 0 差分；不得用 `--scaled` 或 `--added` | base＝本分支 3a 第一個程式 commit 的父 commit（文件只有的 `1867ff0a` 或其後的文件 commit）；兩樹各自以 test preset 建置 runner |
| 2 | golden 不變 | `authority_digest` CTest 通過；`git diff <base> -- tests/object_fps_pvp/fixtures/authority_golden.txt` 為空；`DigestVersion` 與 `DigestFields` 不變 | diff 與 grep |
| 3 | 範圍不外溢 | `git diff --stat <base> -- apps/object_fps_pvp/protocol apps/object_fps_pvp/gateway services` 為空；Client 端原始碼（ClientConnection、ClientSimulation、LocalPlayerPrediction、SnapshotTimeline、PvpApplication）為空；`MatchRuntimeHost.cpp`、`IpcHost.cpp` 不改 | diff |
| 4 | Engine 守衛的檢出力 | Engine 突變目錄的全部突變被殺，且**只靠 common 測試**（執行時產品測試不在對象內）；至少含：Insert 改 append、Erase 改 swap-and-pop、Find 不比鍵、Find 改線性（由比較次數上限抓）、`EntityOf` 繞過 `EntityKey` 直讀 `id` | run_mutations.py；只在預期訊息失敗才算 killed |
| 5 | 產品守衛不退化 | 搬到 Engine 的 M1、M2 由 common 測試殺；留在產品的 M3～M6 照舊被殺；G1、G2、G7、G13 Green | 兩個突變目錄各跑相關項（D184） |
| 6 | 依賴與字詞 | configure 時 `gyo_expect_links(gyo_entity_store GYO::Base)` 通過；`engine/entity_store` 無產品名與禁用字詞；pvp 的 `HasEntityId` 多載已刪 | CMake 檢查、grep 測試 |
| 7 | CI | PR 的 L1 cpu\|shader 全部通過（四平台） | GitHub Actions |

- 任一項不成立：不修正、不重跑，先回報（§14.1）。「不一致」「確認できない」時不確定受理判定。
- 範圍的限界：效能不量（RC 3 的門檻由產品之後量）；network 標籤的測試只在本機；Windows 專用突變沒有。
- 證據：runner 輸出與 compare 報告、突變報告存主 checkout `build/evidence/entity-store/impl-3a/`（git 管理外），dev_log 寫結論與數值。

## 模型／檔位／估時

- 上段（Architecture 的實作、Design Invariants 的守衛）。實作者 Opus 5.5／high；審查者 Opus 5.5／high；§14.4 檢證者（若有判定變更：M1、M2 的 expect 改成 common 測試，屬「守的性質不變、只換守衛位置」，要照 §14.4 示 1～5）Fable 5.1／high——與主會話同模型，啟動前問使用者（任務卡）。局部 xhigh：不需要（3a 沒有 `DeferredChanges` 的套用順序；`EntityKey` 的特化可見性由編譯與 Guard 第 10 列守）。
- 估時 9～13 h（delta §8）：模組與 CMake 1～1.5、common 測試與突變 2.5～3.5、pvp 換上 1.5～2.5、驗收兩樹 compare 2～3、文件 1～1.5、審查 1。超過 1.5 倍或單一問題累計 2 h 停下回報。
- 步驗順序：(1) Engine 模組＋common 測試＋突變（Green 與 killed）→ (2) pvp 換上（build、產品 cpu 測試）→ (3) 兩樹 compare 與 golden → (4) 審查 → (5) 檢證者（若有 §14.4 項目）→ (6) 推送、L1、通知圖書管理員改 architecture.md（3a 版）。
