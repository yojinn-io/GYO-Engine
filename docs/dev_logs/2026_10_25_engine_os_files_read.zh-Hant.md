# Engine os-files-read：唯讀檔案原語與原生檔案系統

日期：2026-10-25。**本機 L1 完成，xhigh 審查已修正**（D47）；四平台 L1 待推送。
計畫見 [os-files-read](../architecture/plans/os-subsystem/PLAN.md)（第 3.2、3.3、7.3 節）；紀錄見 [交接](../architecture/plans/os-subsystem/HANDOFF.md)的「os-files-read」。

## 做了什麼

- `engine/os` 的 Files 部件（唯讀）：
  - `ReadFile` 提供 Open、Read、Size、Seek、Tell。
  - 另有 `Stat`、`Exists`、`MakeAbsolute`、`PathToUtf8`／`PathFromUtf8`。
  - 路徑一律是 UTF-8：Windows 轉成 UTF-16 呼叫寬字元 API，POSIX 原樣使用。
  - 空字串、NUL、不合法 UTF-8 在呼叫 OS 前就拒絕。
  - 開啟目錄在每個平台都回 InvalidPath。
  - 失敗的 detail 帶原生錯誤碼。
- `engine/io` 的 `NativeFileSystem`：IFileSystem 的 file:// 後端，所有 OS 呼叫都交給 engine/os。
  - 目前唯讀：Open 只接受讀取模式；Exists、Stat、ToNativePathString 可用；其他回 NotSupported。
  - Capabilities 每個欄位都明確設定。
  - engine/os 的錯誤用 CauseDetail 保留代碼名與原生碼。
- `GYO::Engine` → PRIVATE `GYO::Os`，依賴方向 asset → io → os → base 從此真的接上。
- 長路徑的行為修正（P1）、asset 接線（P3）、UTF-8 呼叫端（P4）不在本批，屬於消費端的發行任務。

## 驗證與經緯

- 本機 macOS Intel 的 core preset：
  - gyo_os_tests 15／15；
  - NativeFileSystemTests 7 個案例，包含 VFS mount 經 NativeFileSystem 一路讀到 engine/os 的端到端案例；
  - ctest 全過；守衛通過，engine/io 沒有任何 OS 命中。
- 測試涵蓋：
  - 日文與正體中文的路徑（Windows 上驗證不經 ANSI code page）；
  - 不存在時回 NotFound，並帶原生碼；
  - 不合法 UTF-8 的各種形式；
  - 唯讀的 Capabilities。
  - 另有一個只記錄的測試：超過 260 字元的路徑目前回報什麼。給消費端 P1 當 Red。
- 突變：本機 7／7 killed。Windows 專用的 2 個（改用 CP_ACP、NotFound 對應）本機標為 skipped，要在 Windows 機器上跑。
- 突變 runner 加上 `platforms` 欄位。守衛的 R5 改成依實際指向判斷相對 include。
- Windows 的程式碼在本機編譯不到，第一次編譯會是四平台 L1 的 windows-x64。

## xhigh 審查

- 使用者同意由 1 位 xhigh 審查者檢查公開介面與 Windows 實作。沒有 blocker，Windows 的第一次編譯預期能過。
- 1 個 major：`PathToUtf8`／`PathFromUtf8` 在 Windows 會丟例外（MSVC 的 `std::filesystem::path`），違反「例外不穿過公開邊界」。使用者決定改成回傳 Result（D47）。
- 這一批同時修正：
  - symbolic link 的 Stat；
  - 時間換算溢位；
  - `Seek(INT64_MIN)`；
  - moved-from 物件；
  - `GetLastError`／`errno` 的讀取順序；
  - surrogate 被默默替換；
  - seek 超過檔尾；
  - FIFO 與裝置；
  - 平台絕對路徑（新增 `IsAbsolutePath`）；
  - 捏造的原生碼；
  - 補上審查列出的測試缺口。
- 為 P1 預留了加長路徑前綴的位置：Windows 原語的 `OsCallPath`（PLAN 第 8 節）。
- 修正後：gyo_os_tests 19 個案例、NativeFileSystemTests 9 個；整份突變目錄在本機 34／34 killed（Windows 專用的 2 個跳過）；ctest 30／30。

## 證據

本機執行輸出（git 忽略）。重跑：`python3 docs/architecture/plans/time-threads-trace/scripts/run_mutations.py --build-dir build/target/_build/core --catalog docs/architecture/plans/os-subsystem/scripts/mutations.json`
