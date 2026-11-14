# GYO2 重建：機制的骨架跑通（`v8-arch-rebuild`）

日期：2026-11-14。Owner：架構（GYO2 重建會話）。分支 `claude/gyo2-mechanism`（從 `claude/gyo2-r0b-delete` 切出，含刪除舊實現的那些 commit）。決定與經過：[kernel HANDOFF](../architecture/plans/kernel/HANDOFF.md) 的 `v8-arch-gyo2-*` 各列。

## 做了什麼

- **帶進機制的程式**：master 11 月 13 日的 `engine/kernel`（Registry、從簽名推出存取、Scheduler、Plan）與 #218 分支（`origin/claude/arch-kernel-world-exec`）的 World、Serial Executor、存取驗證，連同它們的測試。
- **儲存用 `engine/entity_store` 模組本身**，從 master 帶回，連同它的測試。使用者決定（`v8-arch-gyo2-use-entity-store`）；修訂 R0 帶過去清單的「entity_store 不帶」。
- **帶回 `tests/common/zone_scan`**（決定性區的掃描器），讓機制的 J-17 測試能跑。
- **型別 `App` 改名 `Product`**（使用者，`v8-arch-gyo2-product-type-name`）。
- **`Zone` 不填視為 B**；要每個區都明寫的產品呼叫 `Registrar::RequireExplicitZones`，才把「沒填是啟動錯誤」打開（使用者選 B，`v8-arch-gyo2-zone-default-b`）。其餘決定性的東西（`Role`、`Rollback`、`DigestSet`、`CFingerprint`、zone C 匯出）沒有動。
- **`Query::Count()` 直接讀元件儲存的大小**，不走一遍；多條件的查詢改用 `CountMatching()`（使用者指出，`v8-arch-gyo2-contract-review-replies`）。
- **骨架測試** `tests/common/kernel/SkeletonTests.cpp`：照單機作者的寫法，整個檔不填任何 zone、role、rollback。

## 經過

- 實作 agent（Opus／high）做了帶入、改名與帶回 zone_scan 三步；架構師（主會話 Fable）做了 `Zone` 的預設、`Count()` 與骨架測試。
- 中途改過三次方向，都是架構師的錯，使用者指正：先整包搬了 13 日的 `engine/kernel`，又因為「它是 11 日之後的」整包撤回（40477b48→8c104e06→68356534）；打算把 entity_store 的四個函式抄進機制內部，改成用模組本身；打算把決定性的部分整個切掉，翻歷史後改成只改 `Zone` 的預設。實作 agent 已做到一半的「切掉」沒有 commit，完整 diff 留作參考（見證據）。

## 驗證

本機 Intel Mac（i9-9980HK），preset `core`（RelWithDebInfo）。

- 機制、entity_store、zone_scan 的測試：64 個全過。
- 完整的 ctest：另有 3 個失敗，`os.boundary_guard`、`cmake.Component`、`build.ci`。它們在帶入機制之前就失敗，原因是先前刪除產品（R0b-2）留下的守衛與 CI 政策測試，處置是 R0b-3 的 11 項判定變更，還等使用者核准。
- 骨架測試的行為部分：9 個案例。生成、重力、移動、事件當 tick 送達、hook、`Local`、run 條件；System 回傳 Err 時 handler 只收到一次、鏈上有 System 名與作者包的那一層、World 進入中斷、未套用的延後變更被丟掉；同一個同步點裡先生成再刪除的實體活下來、先加上再移除的元件還在；兩個寫者沒寫順序時依登記順序平手並列出；預設獨占拒絕第二個寫者；`Count()` 與 `CountMatching()`。
- 骨架測試的極限部分只量不設門檻，量一次、沒有重複取樣。單執行緒、存取驗證關：
  - 啟動分析：1000 個互不衝突的 System 108 ms；1000 個都寫同一個元件 1.6 s（平手 499,500 對）。
  - 每個 tick 一個 System：兩個元件的移動，1 千實體 0.17 ms、1 萬 1.9 ms、10 萬 20.7 ms；四個元件 0.28／3.0／30.7 ms；刪除 100 個實體 0.2／3.7／68.9 ms；生成 100 個約 0.1 ms，與實體數無關。
  - 這些 System 幾乎是空的，量到的是機制自己的開銷，不是實際的負載（使用者指出）。接上第一個真的系統之後再跑一次。

## 還沒做的

- 骨架測試把會被生成又被更新的元件標成 Shared 才過。使用者已定這種情形由作者用漏斗、Engine 提供漏斗的工具（`v8-arch-gyo2-spawn-writer-funnel`）；工具的形式定了之後，樣本改成漏斗的寫法、元件改回預設的獨占。
- Extract 只有宣告與啟動檢查，執行還沒做；runner 還不能被安裝成可呼叫的東西。
- 時間、固定步、跨時鐘事件的設計在提案階段（`rebuild-time-design.zh-Hant.md`）。
- 三平台的 L1 還沒有在這條分支上跑過；骨架測試只在本機的 AppleClang 編過。

## 證據

主 checkout（這台 Intel Mac）`build/evidence/gyo2-mechanism/`：實作 agent 各步的 configure／build／ctest log（`r0b-step1-*`、`r0b-step2-*`、`r0b-final-*`、`r0b-baseline-ctest.log`）、放棄的「切掉」那一步的 diff（`r0b-step3-uncommitted-abandoned.patch`）、架構師的驗證（`verify-*`、`b-default-zone-*`、`count-*`）、骨架測試的輸出（`skeleton-behaviour-*.log`、`skeleton-limits-1.log`、`skeleton-ctest-all.log`）。
