# Engine os-guard-wide：守衛擴到產品、工具、產品測試與驗收程式

日期：2026-10-25。**本機完成**，四平台 L1 待推送後執行。
計畫見 [os-guard-wide](../architecture/plans/os-subsystem/PLAN.md)（第 7.5 節）；紀錄見 [交接](../architecture/plans/os-subsystem/HANDOFF.md)的「os-guard-wide」。

## 做了什麼

- 守衛 `os.boundary_guard` 的掃描範圍從 engine 與 tests/common 擴到 engine、apps、tools、build/acceptance、tests。
  - 直接開檔的規則（R4）也套用到 apps 與 tools。tests 與 build/acceptance 只寫開發者控制的暫存路徑，不受 R4 約束。
  - 根目錄不寫產品名稱，新增或刪除產品都不必改守衛。
  - 其他規則不變。
- 各 owner 自己的 allowlist（只縮不增，每條寫明理由與移除它的批次）：
  - `apps/object_fps_pvp/os_boundary_allowlist.txt`：15 條，執行檔路徑、CPU 時間、本地時間（os-process-info），結束訊號（os-console-shutdown），trace 與場地檔的讀寫（os-files-write）。
  - `tools/ui_editor/os_boundary_allowlist.txt`：6 條，檔案服務、文件 session、系統字型目錄（os-tool-files）。
  - `build/acceptance/object_fps_pvp/os_boundary_allowlist.txt`：1 條誤報，變數名剛好叫 `signal`。
- 標頭衛生測試 `os.header_hygiene`：engine/os 的公開標頭不可以帶進 windows.h、winsock 或 `min`／`max` 巨集（os.md Invariant 4 的 Windows 側），並檢查每個公開標頭都被測試 include。
- 設定期檢查：engine/os 以外的 target 與目錄，不可以有識別 OS 的 compile definition（名稱含 OS 字，例如 `GYO_WINDOWS`、`WIN32_LEAN_AND_MEAN`，或以 `$<PLATFORM_ID:...>` 為條件）。
- os.md 的 Guard 節與已知盲點同步更新（Design Invariants 節不變）。

## 驗證與經緯

- 基準：空 allowlist 實跑，產品、工具與驗收程式共 63 個發現、13 個檔案，和盤點對照：
  - 多出 1 個盤點沒列的站點（`TracePaths.hpp` 的 `exists`），歸 os-files-write。
  - 盤點列了、守衛抓不到的是命令列：窄字串 argv 直接當路徑，沒有 OS 標頭、巨集或 API。寫進已知盲點，由 os-console-shutdown 處理。
  - 1 個誤報（驗收程式裡名為 `signal` 的 `std::ofstream`）。
    - 一開始把 R3 的裸 `signal(` 拿掉。之後發現這是守衛判定的放寬：已 allowlist 的檔，或經 asio 間接拿到 `<signal.h>` 的檔，新增裸呼叫就看不到了。依 AGENTS §14.4 要先徵求同意，所以提交前改回原樣式。
    - 改成依 PLAN 第 6 節「不放寬樣式」，列進驗收程式的 allowlist，寫明是誤報。
  - 未啟用產品（object_fps、object_fps_v2）沒有發現，不需要 allowlist。
- 設定期檢查的基準：現有的定義（`NOMINMAX`、`GYO_*`、`PVP_*`）都不識別 OS。
  - 產品組態（`GYO_APPS=object_fps_pvp`、`GYO_TOOLS=ui_editor`、`BUILD_TESTING=ON`）設定成功，沒有誤報。
  - 暫時在產品的 CMake 加一個以 `$<PLATFORM_ID:Darwin>` 為條件的定義，設定失敗並指出該 target，之後還原。
- 本機 macOS Intel 的 core preset：ctest 31／31，守衛通過，自我測試 30 項通過。產品組態只做了設定，沒有建置。
- 突變：新增 os-g11～os-g17，全部只改 Engine 擁有的檔案。
  - 守衛相關的 os-g1～os-g17 重跑，16／16 killed。
  - os-g16（公開標頭 include windows.h）只在 Windows 跑，本機跳過。
  - os-g14 第一次是 wrong_failure：檢查正確，但 CMake 把長訊息折行。改成印相對路徑，正則的空白改用 `\s+` 後 killed。

## 證據

本機執行輸出（git 忽略）。重跑方式：

- 守衛：`python3 tests/common/os/os_boundary_guard.py --root .`
- 自我測試：`python3 -m unittest discover -s tests/common/os -p "test_*.py"`
- 突變：`python3 docs/architecture/plans/time-threads-trace/scripts/run_mutations.py --build-dir build/target/_build/core --catalog docs/architecture/plans/os-subsystem/scripts/mutations.json --only os-g1-header os-g2-macro os-g3-api os-g4-io-opens os-g5-upward os-g6-stale os-g7-public-header os-g8-link os-g9-macro-list os-g10-filesystem-op os-g11-roots-skip-products os-g12-allowlist-scope os-g13-definition-name os-g14-definition-platform-id os-g15-runtime-roots os-g16-public-header-windows os-g17-hygiene-coverage`
