# Engine os-console-shutdown：命令列、主控台輸出、結束要求搬進 engine/os

日期：2026-10-25。**本機完成**：xhigh 審查已修正（D50），四平台 L1 待推送後執行。
計畫見 [os-console-shutdown](../architecture/plans/os-subsystem/PLAN.md)（第 3.6、7.7 節）；紀錄見 [交接](../architecture/plans/os-subsystem/HANDOFF.md)的「os-console-shutdown」；開工前的決定是 D49，審查後的決定是 D50。

## 做了什麼

- engine/os 新增 `Console`：
  - `CommandLineUtf8`：程序的命令列，UTF-8。Windows 用 `GetCommandLineW`＋`CommandLineToArgvW`，不受 ANSI code page 影響。
  - `Utf8ConsoleOutput`：存在期間，Windows 主控台用 UTF-8 顯示，解構時還原原本的 code page。
  - `InstallShutdownRequests`、`RequestedShutdown`：整個程序裝一次結束要求的 handler，之後隨時可讀第一個要求的理由。POSIX 是 SIGINT、SIGTERM，旗標和以前的 `std::signal` 相同；Windows 是 Ctrl+C、Ctrl+Break、關閉主控台（最多等 4 秒），要求之後再按 Ctrl+C 或 Ctrl+Break 會直接結束，和以前第二次 Ctrl+C 相同。
- Match 與 ui_editor 改用它們；Match 的 end 紀錄仍是 `reason=signal`。
- 連結守衛允許 Windows 上的 `gyo_os` 連 shell32（D49 同意、D50 定案）。
- os.md 的 Invariant 6 改寫：engine/os 的 OS callback 只寫 atomic，不需要 error-handling.md 的平台邊界例外（D49）。
- 突變 runner：command 型檢查可以先建一個 target，並用 `{exe:NAME}` 指名建好的執行檔。

## 驗證與經緯

- 真的子程序（`os.console_arguments`、`os.console_shutdown`，Windows 另有 `os.console_close`）：
  - 九個難搞的參數（日文、韓文、emoji、空白、引號、反斜線、tab、空字串）回來是相同的 UTF-8。
  - POSIX：handler 的 `sa_flags` 和同一個 C library 上 `std::signal` 設的相同；同一個訊號送兩次、再送另一個，程序不會被結束，理由是第一個。
  - Windows：Ctrl+Break（送給新的程序群組）與 Ctrl+C（探測程式送給自己新開的主控台）各一次是結束要求，第二次會直接結束程序。
  - 不能在這台 runner 上做的檢查回 77，CTest 顯示為 skipped。
- 本機實際跑 Match：SIGTERM、SIGINT 都優雅結束，log 最後一行是 end 紀錄。
- 突變：結束要求的三個 POSIX 突變，前兩次檢查的訊號順序各漏掉一個（handler 重設、後來的要求蓋掉理由），定為「第一個、第一個、另一個」後都抓到。
- xhigh 審查：沒有 blocker、major。改了 4 件：Windows 第二次 Ctrl+C 的行為改回舊的（原本沒宣告）、安裝改成全部成功或全部還原、補上 `SA_RESTART` 與 Windows Ctrl+C 的檢查、os.md 同步。POSIX 拒絕非 UTF-8 參數與 shell32 守衛由使用者定案（D50）。shell32 守衛的 AGENTS §14.4 核對裡，我原本寫「沒有收緊面」是錯的，審查者實測指出 Windows 上「只有 GYO::Base」改成失敗。
- 本機能跑的突變都被抓到；Windows 專用的 6 個（os-c4、os-c5、os-c6、os-c8、os-c9、os-g18）要到 Windows 上跑。
- 四平台 L1：
  - 第一次（run 40202449713）只有 Windows 的 `os.console_close` 失敗：探測程式活到 10 秒逾時，關閉事件沒送到。主控台真的關閉時，OS 最晚在 handler 返回時結束程序，所以這是測試手段在 CI 上不成立，不是實作的錯。
  - Windows 的 Ctrl+C、Ctrl+Break、UTF-8 參數與 Linux 的旗標比對都在 CI 上通過。
  - 第一次修正（「3 秒後視窗還在就 skip」）在第二次 run 40203783441 沒有觸發，仍失敗。依約定停下，交使用者決定。
  - 使用者選定 D51：關閉模式下逾時就判為「關閉事件沒送到」並 skip；關閉主控台的路徑在 lan15 的 Windows 實機驗證。
- 本機 macOS Intel：core 33／33，產品組態的相關測試 20／20。

## 證據

本機執行輸出（git 忽略）。重跑方式：

- 檢查：`python3 tests/common/os/console_check.py --probe build/target/_build/core/tests/common/os/gyo_os_console_probe --check shutdown`（`arguments`、`close` 同理）
- 突變：`python3 docs/architecture/plans/time-threads-trace/scripts/run_mutations.py --build-dir build/target/_build/core --catalog docs/architecture/plans/os-subsystem/scripts/mutations.json --only os-c1-sigterm os-c2-reset-handler os-c3-last-wins os-c4-no-close-branch os-c5-no-break os-c6-ansi-arguments os-g18-windows-extra-link os-g8-link`
