# Engine os-utf8-codepage：Windows 程序自己把 code page 定為 UTF-8

日期：2026-10-27。**完成**：Red 的 L1 如預期（[run 40450140816](https://github.com/yojinn-io/GYO-Engine/actions/runs/40450140816)，`8a677bb9`）；Green 的 L1 （[run 40456874710](https://github.com/yojinn-io/GYO-Engine/actions/runs/40456874710)，`a9c9dc85`）全部成功。windows-x64 的 ctest 96／96：gyo_os_tests 通過（`GetACP()` 是 65001，合併後的 manifest 有 activeCodePage），`cmake.ProcessCodePage` 在 MSVC 下通過（嵌入方式是 `.manifest`），產品與編輯器的執行檔也都帶著 manifest 建置、測試通過。
計畫見 [os-utf8-codepage](../architecture/plans/os-subsystem/PLAN.md)（第 3.10、7.18、9.2 節）；紀錄見 [交接](../architecture/plans/os-subsystem/HANDOFF.md)的「os-utf8-codepage」；決定是 D64（方向）、D65（排在計時三批之前）、D66（開工前的決定）。

## 為什麼

使用者：「程序不應該由環境參數來決定。」玩家機器的系統 ANSI code page 多半是 950、932 等。D201 那台 Windows 機器的系統 ACP 是 65001，結果 6 條突變無法判定，記錄 ACP 的測試也失敗。程序的行為和驗證都隨機器改變。

## 做了什麼

- engine/os：
  - `engine/os/cmake/utf8-codepage.manifest` 只宣告 `<activeCodePage>UTF-8</activeCodePage>`。
  - `engine/os/cmake/GyoProcessCodePage.cmake` 把它嵌進一個執行檔：MSVC、clang-cl、MSVC ABI 的 Clang 用 `.manifest` 原始檔，由連結器合併；MinGW 用 `.rc` 的 `RT_MANIFEST` 資源 1，明寫語言，並依賴 manifest 檔。用哪一種，在 include 時決定。
  - `Process::AnsiCodePage()`：Windows 回 `GetACP()`，POSIX 回 Unsupported。給啟動行之類的診斷用。
  - `gyo_os` 以 `UNICODE` 編譯：不帶後綴的 Win32 名稱一律解析到寬字元版本。
- 建置層：`build/cmake/GyoCodePage.cmake`。
  - `GyoBuild.cmake` 依 binary dir 記錄它加入的每個目錄（engine、各產品與工具、它們的測試與驗收程式、tests/common）。
  - 設定期最後，對這些目錄底下 engine、apps、tools、build/acceptance、tests 的每個執行檔套用 manifest，並在每個平台打上標記。
  - 不出現產品名；FetchContent 的第三方程式不在範圍內；shader 工具的 host 專案自己呼叫嵌入函式。
- 檢查：
  - 設定期檢查 `tests/common/os/ProcessCodePageCheck.cmake`：和套用分開寫，從頂層依 source 路徑、也從記錄的目錄走訪；沒有標記又沒有例外的執行檔就讓設定失敗，例外只列出。
  - `cmake.ProcessCodePage`：在合成的樹上跑 plain、獨立工具入口、沒有記錄的目錄、例外四種情況。
  - 守衛 R6：engine/os 不用 `CP_ACP` 類的常數，也不呼叫 ANSI 版的 Win32 入口（含 `::` 前綴的呼叫）。
  - `FilesTests` 的 ACP 測試改成「Os files run with UTF-8 as the process ANSI code page」：`GetACP()` 是 65001，並讀自己的 `RT_MANIFEST` 確認宣告。
- D201 的測試缺漏：
  - `FileWriterTests` 加「create directories under a UNC share」。
  - `FileChangeTests` 的 Windows 字型預期清單補上使用者的字型目錄。
- 文件：
  - os.md 的 Components、Contracts、Guard 與盲點、Invariant 11（D66 同意的草稿，範圍依審查改成「CMake 從 repo 自己的 C/C++ 原始碼建出的執行檔」）。
  - Replacement Criterion 8 的草稿，要使用者同意。
  - inactive_products.md。

## 驗證與經緯

- Red（`8a677bb9`，只有新測試）：
  - L1 只有 windows-x64 失敗，失敗的只有新案例：`GetACP()` 是 1252，manifest 的結構檢查也失敗。三個 POSIX 平台全部通過。
  - 印出的 manifest 是 MSVC 連結器預設的那份，只有 `requestedExecutionLevel asInvoker`。
- 實作時：
  - R6 的第一版 lookbehind 把 `::CreateFileA(` 也排除了，而 engine/os 正是用 `::` 呼叫 Win32 的。寫 os.md 時發現並修正，加了案例與 pu-x14。
  - 設定期檢查第一次在 macOS 上用突變驗證：把 tests 從套用範圍拿掉，設定就會失敗並列出所有測試執行檔。
- xhigh 審查（1 位）：
  - major 2 個：
    - 獨立工具入口 `cmake -S tools/ui_editor` 漏套，而且在 MSVC 下會走錯分支。審查者在 scratchpad 重現了漏套。
    - R6 漏掉 `::` 的呼叫，審查開始前已修。
  - minor 6 個：
    - pu-x12 可能等價；走訪的 owner 應是建置層；MinGW 的增量建置與預設 manifest；檢查沒有自我測試。這 4 個都已修。
    - p4 仍在本計畫的目錄；沒有 PE 層的檢查。這 2 個記為缺口。
  - note：Invariant 11 的範圍、★ 的補充、R6 的盲點，都寫進 os.md。
  - 修正後，把走訪改回只依 source 路徑時，`cmake.ProcessCodePage` 的 wrapper 案例會失敗（pu-x15）；這證明新的測試守得住 major 1。
- 本機（macOS arm64，修正後）：core 34／34；產品組態 96／96；`worker` 與 `close_timing` 逐一串行 2／2；host-tools 3／3。
- 突變：
  - 新增與改線的、本機能跑的都 killed：os-f4、os-c6、pu-x5（改由 R6 判定）、pu-x8、pu-x11、pu-x13～pu-x16。
  - 相關的既有突變 20／20 killed。
  - Windows 專用的 pu-x9、pu-x10、pu-x12，以及 os-w10、os-g18，留給 Windows 機器。
  - pu-x6、pu-x7 依 D66 退場。
- 判定的變更（AGENTS §14.4，D66 同意開工、定案前核對）：

  | 判定 | 守的性質 | 放寬面 | 收緊面 | 放寬的部分由誰守 |
  |---|---|---|---|---|
  | ACP 測試 `!= 65001` → `== 65001`＋結構檢查 | 以前：runner 的 ACP 不是 UTF-8，非 ACP 的測試才有檢出力。現在：程序的 ACP 是 UTF-8（Invariant 11） | 系統 ACP＝65001 的機器上，以前失敗，現在通過 | 沒有 manifest 的程序，以前在 1252 上通過，現在失敗 | 「engine/os 不用 ANSI」改由 R6 守（任何 OS） |
  | FontDirectories 的預期清單補上使用者字型目錄 | 列出的都是文件寫的目錄，順序正確 | 有使用者字型的機器上，以前失敗，現在通過 | 無 | 同一個迴圈仍檢查成員與順序；系統目錄排第一仍由 `front()` 檢查 |
  | os-f4、os-c6、pu-x5 改由 R6 判定 | engine/os 不用 ANSI code page 轉換 | 無（以前只在 ACP 不是 65001 的 Windows 上能判定） | 現在任何 OS 都能判定 | — |
  | pu-x6、pu-x7 退場 | engine 與工具的窄字元轉換是 UTF-8 | 沒有 manifest 的程序裡的窄字元轉換不再有突變守 | 無 | manifest 的設定期檢查與結構檢查 |
  | pu-x12 改成回傳 1252 | 查詢回報真正的 ACP | 無 | 原本可能是等價突變，現在一定被殺 | — |

## 已知缺口

- 沒有對每個建出的執行檔做 PE 層的檢查。shader 工具的呼叫、產品目標的例外、`BUILD_TESTING` 關閉的組態，都只靠審查。
- p4-manifest-ansi-path 在 manifest 之下會存活，交 v7.1 會話。
- MinGW 的嵌入、非 ASCII 的 repo 路徑、OEM code page，都要等 Windows 機器確認。
