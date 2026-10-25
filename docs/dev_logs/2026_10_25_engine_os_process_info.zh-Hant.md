# Engine os-process-info：執行檔路徑、CPU 時間、本地時間搬進 engine/os

日期：2026-10-25。**本機完成**，四平台 L1 待推送後和 os-guard-wide 一起執行。
計畫見 [os-process-info](../architecture/plans/os-subsystem/PLAN.md)（第 3.5、7.6 節）；紀錄見 [交接](../architecture/plans/os-subsystem/HANDOFF.md)的「os-process-info」。

## 做了什麼

- engine/os 新增 `Process`：
  - `ExecutablePath`：執行中映像的絕對路徑（UTF-8）。
  - `ProcessCpuTime`、`CurrentThreadCpuTime`：user＋kernel 的 CPU 時間（奈秒）。
  - `ToLocalTime`：把時間點換成本地的年月日時分秒與 UTC 偏移；格式化留給呼叫端。
  - 每次都向 OS 讀，不快取。各 OS 一個原始檔，由 CMake 選；UTC 偏移由欄位算出，三平台共用。
- 消費端改呼叫它們：
  - `LogTimestamp`、`RunningExecutablePath`、`ProcessCpuSeconds`、`CurrentThreadCpuSeconds` 保留名稱與簽名，變成薄轉接，其他呼叫端不用改。
  - 日誌時間戳與預設 log 檔名改由新的純函式 `FormatLogTimestamp`、`LogFileName` 組字串，格式和以前相同。
  - 產品裡不再有 OS 標頭、OS 巨集或 OS API；`apps/object_fps_pvp` 的 allowlist 從 15 條縮到 7 條。
- os.md 的 Components 與 Contracts 加上 Process（Design Invariants 節不變）。

## 驗證與經緯

- 位元組不變的證據：
  - Engine 測試在 5 個固定時區 × 6 個固定時間點下，確認「`ToLocalTime` 的欄位與偏移，照呼叫端的方式印出來」和以前的 `localtime`＋`strftime`（含 `%z`）逐 byte 相同。Windows 的 UCRT `%z` 要等 windows-x64 的 L1 才確認。
  - 消費端測試確認固定欄位會印成固定的字串。
  - 執行檔路徑和舊的各平台查法得到同一個路徑。
- 事前宣告的差異（都不改日誌格式）：
  - CPU 秒數的 double 最後一位元可能不同。
  - Windows 帶 `\\?\` 前綴的執行檔路徑會去掉前綴；沒有 UTF-8 形式的路徑變成錯誤。
  - 找不到執行檔時的例外訊息改寫。
  - 本地時間轉不了時的後備輸出（目前時刻不會發生）。
- `object_fps_pvp.cpu` 第一次逾時，原因是本機產品組態沒有指定建置類型（未最佳化）。改成和 preset 相同的 RelWithDebInfo 後 23 秒通過。
- 本機 macOS Intel：
  - core preset：ctest 31／31，守衛通過。
  - 產品組態：相關的 12 個 CTest 通過，產品測試 326 個案例全過。
  - Client 與 Match 的入口檔只做了語法檢查（連結要等 shader 主機工具）。
- 突變：新增 os-p1～os-p6，本機 7／7 killed，Windows 專用的 os-p5 跳過。消費端格式化的 3 個探索性突變都被抓到（不進突變目錄）。

## 證據

本機執行輸出（git 忽略）。重跑方式：

- Engine 測試：`build/target/_build/core/tests/common/os/gyo_os_tests -tc="Os process*"`
- 突變：`python3 docs/architecture/plans/time-threads-trace/scripts/run_mutations.py --build-dir build/target/_build/core --catalog docs/architecture/plans/os-subsystem/scripts/mutations.json --only os-p1-thread-clock os-p2-offset-sign os-p3-month-zero-based os-p4-rusage-units os-p5-windows-thread-times os-p6-executable-path`
- 產品組態：`cmake -S . -B build/target/_build/wide -G Ninja -DGYO_APPS=object_fps_pvp -DGYO_TOOLS=ui_editor -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo`，再建 `gyo_object_fps_pvp_tests` 並跑 `ctest -R "^object_fps_pvp\.(cpu|log_contract|authority_digest)$"`。
