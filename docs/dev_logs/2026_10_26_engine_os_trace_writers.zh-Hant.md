# Engine os-trace-writers：trace 寫檔改走 engine/os

日期：2026-10-26。**本機完成**：Invariant 7 與 os.md Invariant 2 由使用者定案（D60）；修正的四平台 L1 待推送後執行。
計畫見 [os-trace-writers](../architecture/plans/os-subsystem/PLAN.md)（第 7.10 節）；紀錄見 [交接](../architecture/plans/os-subsystem/HANDOFF.md)的「os-trace-writers」；開工前的決定是 D58，定案是 D60。

## 做了什麼

- engine/trace：`TraceSink` 原本用文字模式的 `std::ofstream`，換成 `Os::Files::FileWriter`。
  - 換行用 `LineEndings::Native`，寫出的位元組相同（Windows 是 CRLF）。
  - 寫入執行緒把一批的行集中在緩衝裡，在 64 KiB 或批尾寫出；`flushEachLine` 時每行寫一次。
  - 寫入失敗後不再寫出任何東西，和失敗的串流相同。
  - 錯誤 detail 的路徑改成 UTF-8。
- `gyo_trace` 以 PRIVATE 連結 `GYO::Os`（開放決策 2 的 (a)）；設定期的連結檢查改成 exactly GYO::Threads and GYO::Os。
- Engine 的 allowlist 刪掉 `TraceSink.cpp R4 1`。
- 文件：trace.md 的 Contracts 與 Invariant 7、os.md 的 Invariant 2（D60 定案），以及事前、事後宣告的差異。

## 驗證與經緯

- Red：先單獨推送新的長路徑測試（`a75e4028`，實作仍是 `ofstream`），跑四平台 L1（[run 40331764169](https://github.com/yojinn-io/GYO-Engine/actions/runs/40331764169)）。如預期，只有 windows-x64 失敗，而且只失敗在新案例的開檔（`REQUIRE( sink.has_value() )`）；macOS、Linux 全部通過（macOS 沒有 260 字元的限制）。
- Green（本機）：core 33／33，gyo_trace_tests 12／12。
- 產品組態的相關測試 8／9。`object_fps_pvp.worker` 第一次失敗，原因是 action 封包的間隔不到 30 ms；單獨重跑 3／3 通過。依 §14.1 判為環境：那次與另外 8 項測試和產品建置並行，5 分鐘平均負載 9.17；而且 worker 探針不經 TraceSink。
- 突變：tt2 6／6 killed（`tt2-final-batch-unwritten` 重新定位）。新增的 tt-x1～tt-x6 在本機能跑的都 killed；Windows、Linux 專用的各 1 個跳過。`os-g6-stale` 原本指向刪掉的 allowlist 條目，改指 Waiter 的條目。
  - tt-x1 第一次的結果是 wrong_failure：CMake 把錯誤訊息折成兩行，`.*` 比不到換行，改用 `\s+` 後 killed。
- xhigh 審查（1 位）：沒有 blocker、major。
  - 確認的：Invariant 1～6；POSIX 上新舊的位元組逐位元組相同；失敗後的 Good／FlushNow 與舊版相同。
  - 已修：trace.md 的圖；「privately」補上設定期檢查；長行之後的緩衝；長路徑測試的可攜性與清理；補了清空模式與錯誤 detail 的測試（tt-x7～tt-x9）。
  - §14.4 的核對：1、2、4 點不完整的部分已補檢查，或記為未結（`GYO::Base` 沒有連結檢查，是既有缺口）。
- 修正後：core 33／33，gyo_trace_tests 13／13，產品組態的相關測試逐一串行 9／9，突變 tt2 6／6、tt-x 與 os-g6 共 8／8 killed。
- D60：條文照草稿定案；審查發現的差異（非 UTF-8 路徑、錯誤 detail 的文字、`flushEachLine` 的長行）接受並補宣告。
- 已知的缺口：失敗後不再寫、`flushEachLine` 每行一寫、64 KiB 門檻、Close 的結果，測試都看不到；Windows 專用的 tt-x3、Linux 專用的 tt-x5 待對應平台。
