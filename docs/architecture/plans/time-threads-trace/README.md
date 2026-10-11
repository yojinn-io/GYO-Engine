# 時間、執行緒與 Trace：分批計畫與進度

更新：2026-10-08。Owner：Engine（新增 `engine/time`、`engine/threads`、`engine/trace`；`engine/runtime` 與 `engine/render/backend/sdl_gpu` 只改時鐘來源）。
**狀態：TT-1 完成**（2026-10-09，和消費端的第一個使用者同一個 PR 合併，`660310e`；L2 的量測程式留待之後的 PR）；**TT-2 未開始**。 本文件與 [PLAN](PLAN.md)、[HANDOFF](HANDOFF.md) 於 2026-10-08 由提出需求的消費端在其規劃批次中建立。

Engine 目前沒有自己的時間基準、等待原語、執行緒管理，也沒有結構化紀錄：

- 時間：`engine/` 內同時用三種時鐘（`RuntimeLoop` 用 `steady_clock`、`SdlGpuRenderDevice` 用 `SDL_GetTicksNS`、`AssetWatcher` 用 `system_clock`）。消費端與驗收工具各自手寫時鐘加等待；macOS 上 std 與 SDL 的等待都有計時器合併造成的晚醒，沒有任何晚醒紀錄。
- 執行緒：Engine 的模組都不開執行緒；消費端的執行緒各自建立、各自停止（stop_token、旗標、20 ms 輪詢），沒有名稱與優先級；AppleClang 為了 `std::jthread` 需要實驗性旗標。
- 紀錄：消費端的日誌、量測與驗收寫出器各自實作，Engine 自己的診斷日誌又用另一個時鐘。

本計畫補上這三樣。迴圈怎麼跑、Tick 政策、紀錄的內容與 schema，仍由各消費端決定。

先讀 [批次計畫](PLAN.md)（範圍、驗收、Architecture Delta）和 [交接](HANDOFF.md)（壓力摘要、研究事實、決策）。

## 來源

- 提出需求的消費端（一個線上遊戲）在新版本開始時做了唯讀盤點，對象是時間、執行緒、sleep、socket 輪詢；壓力摘要以文字寫在 [HANDOFF](HANDOFF.md)。刪除該消費端後，本計畫仍可獨立理解。
- 本夾的文件不連結、也不點名消費端（D12）；程式檔案路徑與證據路徑作為資料保留。

## 已定案方針（2026-10-08，使用者）

1. **D27**：Engine 建最小的角色執行緒（名稱、協作停止、優先級提示），不做執行緒池與排程器；Channels 不先統一；Job 系統等第一個平行運算的消費端出現時，建在 Threads 之上。修訂輸入與呈現計畫的 D19：多角色的執行緒由本計畫的 `GYO::Threads` 與 `GYO::Time` 提供，角色本身（迴圈、交接、政策）由消費端做。
2. **D29**：`GYO::Time` 是獨立的 Base 層 target，並且是 **Engine 的時間基準**；`RuntimeLoop` 在 TT-1 改用它，只換時鐘來源，不改 loop 的語意。
3. 每個 Engine 子系統與它的第一個消費端放在同一個 PR（跨層必須一起生效的改動）。
4. **D12 匿名**：本夾的文件只寫「提出需求的消費端」。

## 進度

| 批次 | 建議檔位 | 狀態 | 交付邊界 |
|---|---|---|---|
| TT-1 Time 與 Threads | high；後端、Notify 與期限的競爭、不早醒規則、晚醒的定義、停止與 join 順序局部 xhigh；公開介面由 1 位 xhigh 審查 agent 檢查（開始時徵求同意） | 完成（`660310e` 合併）：實作與 L1、xhigh 審查與修正、L2（只記錄，2026-10-09） | `GYO::Time`（時間基準、Waiter、LateWakeStats、各平台後端）、`GYO::Threads`（角色執行緒）；`RuntimeLoop` 與 SdlGpu 改用時間基準。與消費端第一個使用者同一個 PR |
| TT-2 Trace | high；寫檔關閉與 flush 的順序、丟棄計數局部 xhigh | 未開始 | `GYO::Trace`：有界多生產者 sink、以通知喚醒的背景寫檔、Base 的日誌 facade；SdlGpu 的慢 acquire 日誌改走 facade |

```text
GYO::Base ◄── GYO::Time ◄── GYO::Threads ◄── GYO::Trace
                  ▲
             GYO::Engine（RuntimeLoop 改用時間基準）
```

## 執行規則

- 每次只執行使用者指定的批次；commit 與 PR 用日語。
- 每批開始、里程碑、停止時更新本表、[HANDOFF](HANDOFF.md) 與 dev_log（`docs/dev_logs/YYYY_MM_DD_engine_tt1.zh-Hant.md` 等），然後停止。
- 主對話檔位由使用者決定；ultracode 與高於主對話的檔位，在批次開始時說明並徵求同意（D8）。
- Engine 的驗收只驗 Engine 的目標（Engine 測試、Engine target 的依賴閉包），不以特定產品的存在為前提（AGENTS §7）；消費端的檢查屬於消費端的批次。
- 計時相關的 L1 不設比結構條件更嚴的時間上界（FF-2 的規則）；精度只在本機 L2 以「只記錄」的方式量。
- 每批同步更新未啟用產品的遷移清單（[foundation-followups/inactive_products.md](../foundation-followups/inactive_products.md)，加「TT」節）。
- 出現非預期回歸、範圍擴大，或需要新的第三方依賴、SDL 邊時，停下回報並重新規劃。
