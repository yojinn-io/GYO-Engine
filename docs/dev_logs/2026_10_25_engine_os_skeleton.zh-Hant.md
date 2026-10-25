# Engine os-skeleton：建立 engine/os 與守衛

日期：2026-10-25。**本機 L1 完成**；四平台 L1 要等分支推送。
計畫見 [os-skeleton](../architecture/plans/os-subsystem/PLAN.md)（第 7.1 節）；紀錄見 [交接](../architecture/plans/os-subsystem/HANDOFF.md)的「os-skeleton」。

## 做了什麼

- 新增 `GYO::Os`（`engine/os`）：
  - 只連 GYO::Base，位在 math 之後、time 之前。
  - `OsError` 的錯誤碼從 1 開始。detail 的固定格式是 `errno=<n>` 或 `win32=<n>`，與路徑有關時加上 `; path_bytes=<n>`。
- `docs/architecture/os.md`：寫下 engine/os 的契約、守衛、已知盲點、Design Invariants 與 Replacement Criteria（依 D46 的同意）。
- 守衛 `os.boundary_guard`：
  - 在 engine/os 以外，不准 include OS 標頭（R1）、不准在前處理條件判斷 OS 巨集（R2）、不准呼叫列管的 OS API（R3）。
  - runtime 程式不准直接開檔（R4）。
  - engine/os 不准往上 include（R5）。
  - 這一批只掃 engine 與 tests/common；現有的 58 個發現（12 個檔案）凍結在只縮不增的 allowlist。
- 突變 runner 加上 command 型檢查，可以直接執行守衛腳本。

## 驗證與經緯

- 本機 macOS Intel 的 core preset：ctest 30／30 通過。
- 守衛自我測試 24 項，用臨時目錄的合成樹驗證以下各點：每條規則抓得到、註解與字串不誤報、編譯器與架構巨集不算 OS 判斷、allowlist 的增減與越界、刪掉合成產品時不必改共通檔。
- 突變 11／11 killed：R1～R5 各一個、allowlist 過期、公開標頭、連結守衛、巨集清單漏掉 `__linux__`、std::filesystem 操作、detail 格式。
- 空 allowlist 的基準和盤點一致。

## 證據

本機執行輸出（git 忽略）。重跑方式：

- 守衛：`python3 tests/common/os/os_boundary_guard.py --root .`
- 突變：`python3 docs/architecture/plans/time-threads-trace/scripts/run_mutations.py --build-dir build/target/_build/core --catalog docs/architecture/plans/os-subsystem/scripts/mutations.json`
