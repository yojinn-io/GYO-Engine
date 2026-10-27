# Engine time-threads-invariants：time 與 threads 的設計約束

日期：2026-10-27。**完成**：使用者同意兩個 Design Invariants 節（D61）。這一批只改文件與測試，不改產品程式碼。
計畫見 [time-threads-invariants](../architecture/plans/os-subsystem/PLAN.md)（第 7.12 節）；紀錄見 [交接](../architecture/plans/os-subsystem/HANDOFF.md)的「time-threads-invariants」。

## 做了什麼

- 新增 [time.md](../architecture/time.md)：
  - Contracts。
  - Design Invariants 11 條：時間基準、不早醒、latch 與合併、已過期限優先、相對期限加重新檢查、無計時器的 `Wait()`、後端回報與名稱、latch 的狀態權威、moved-from、連結、LateWakeStats。
  - Replacement Criteria 4 條。
- 新增 [threads.md](../architecture/threads.md)：
  - Contracts。
  - Design Invariants 9 條：`Start` 交出正在執行的角色、協作停止必定喚醒、解構時停止並 join、順序由擁有者決定、優先級是有結果的提示、名稱與截斷、每個角色有自己的 Waiter、body 的義務、連結。
  - Replacement Criteria 3 條。
- 每一條都寫明由哪個測試或突變守住，沒有守衛的寫「No guard」。
- docs/architecture.md 的模組表補上 `GYO::Os`，Trace 的依賴加上「Os privately」；這是 os-skeleton 與 os-trace-writers 漏改的。
- 便宜的守衛（使用者要求）：
  - 後端名稱的測試。
  - 長名稱的檢查：放在 RoleThreadTests 既有的 `#if` 裡，不增加 OS 巨集。
  - 突變 ti-x1～ti-x4：連結檢查兩個、後端名稱一個、Linux 名稱截斷一個。

## 驗證與經緯

- 內容取自 `Waiter.hpp`、`RoleThread.hpp`、TT-1 的 PLAN 與 D29。寫的過程中對照了實作：
  - `Waiter.cpp` 確實用 `EVFILT_USER` 與 `NOTE_CRITICAL`。
  - `RoleThread.cpp` 的 `Normal` 在 macOS 與 Windows 會呼叫 OS。草稿原本寫「不需要 OS 呼叫」，照程式改正。
- 本機：gyo_time_tests、gyo_threads_tests 通過，守衛通過。突變 ti-x1～ti-x3 killed；ti-x4 是 Linux 專用，跳過。
- 未結：無守衛的項目，列在 HANDOFF 的這一批紀錄裡。
