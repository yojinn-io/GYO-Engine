# Engine os-threads-backend：執行緒命名與優先度的 OS 呼叫搬進 engine/os

日期：2026-10-27。**本機完成**：程式與文件都在本機；macOS Intel 的 L2 逐字相同；Windows 與 Linux 的 L2 要等下一次實機用 A、B 兩個 commit 補。推送要等使用者同意。
計畫見 [os-threads-backend](../architecture/plans/os-subsystem/PLAN.md)（第 3.7、7.14 節）；紀錄見 [交接](../architecture/plans/os-subsystem/HANDOFF.md)的「os-threads-backend」；開工前的決定是 D69。

## 做了什麼

- engine/os：
  - 新增 `Thread.hpp`：`SetCurrentName(utf8Name)`、`SetCurrentPriority(PriorityHint)`，都回傳「平台有沒有接受」（Invariant 10）。
  - 實作從 `RoleThread.cpp` 逐行搬到 `engine/os/src/thread/Thread.cpp`。依開放決策 11 (c)，一個檔案以 `#if` 分平台，方便對照舊碼。
  - ★（D69）：Windows 的名稱改用新的 `Detail::Utf8ToWide` 轉成 UTF-16。以前是逐 byte 擴寬，非 ASCII 的名稱會亂掉；不是 UTF-8 的名稱現在不套用。
  - POSIX 上 `gyo_os` 以 PRIVATE 連結 `Threads::Threads`（`pthread_setname_np`）。
- engine/threads：
  - `RoleThread` 改呼叫 engine/os，OS 標頭與 `#if` 都拿掉；`Start` 的順序不變，仍在新執行緒、`Start` 返回前套用名稱與優先度。
  - 優先度以 `static_cast` 轉成提示，三個值用 `static_assert` 對齊。
  - `gyo_threads` 以 PRIVATE 連結 `GYO::Os`。
- 守衛與連結檢查：
  - Engine 的 allowlist 刪掉 `RoleThread.cpp` 的 R1、R2、R3；`RoleThreadTests.cpp` 的 R2 改標為永久條目。
  - `gyo_threads` 是 exactly GYO::Time、Threads::Threads、GYO::Os，且 GYO::Os 不得公開；`gyo_os` 在 POSIX 上是 exactly GYO::Base、Threads::Threads。
- 測試：`tests/common/os/ThreadTests.cpp`。
  - 名稱與優先度用 OS 自己的查詢讀回：`pthread_getname_np`、`qos_class_self`、`GetThreadDescription`、`GetThreadPriority`、`getpriority`。
  - 涵蓋 Windows 的非 ASCII 名稱與 Linux 的 15 bytes。
  - 另有一個案例經 `RoleThread` 在角色自己的執行緒上讀回優先度。
  - `RoleThreadTests` 的判定不改。
- 文件：
  - threads.md：Ownership、名稱的 Contracts、Invariant 5 與 6 的守衛、Invariant 9 的新條文。
  - os.md：Components、Thread 的 Contracts、Invariant 3 的新條文、連結檢查的說明、分層圖。
  - docs/architecture.md 的依賴欄；inactive_products.md。

## 驗證與經緯

- L2（D69 的事前宣告，只記錄，附撤回條件）：
  - 程序：A 是搬動前（`5733d361`），B 是搬動後。用消費端自己的測試程序啟動它的角色：ctest 的 `object_fps_pvp.worker`、`.ipc`、`.close_timing`、`.cpu`。涵蓋 `gyo-match-sim`、`gyo-match-ipc`、`gyo-client-net` 與用戶端模擬角色。
  - 結果：macOS Intel 上四種角色的 `role started` 行在 A 和 B 逐字相同，都是 `wait_backend=Kqueue name_applied=1 priority_applied=1`。審查修正之後重抓（B2）也相同。
  - 證據在主 checkout 的 `build/target/_build/test/logs/os-threads-l2-20261027-macos-intel/`。
  - Windows 與 Linux 在下一次實機用 A、B 兩個 commit 補做；任一項不同就撤回整批。
- 第一次全測時，`os.header_hygiene` 失敗：新的公開標頭沒有加進標頭衛生測試。補上後通過。
- xhigh 審查（1 位）：
  - major 2 個，都已修：
    - 讀回測試的寬字串字面值直接寫了中日韓文字，而 `gyo_os_tests` 沒有 `/utf-8`。MSVC 會用 ACP 讀原始碼，windows-x64 的 L1 會失敗，Windows 上整份突變目錄的基準線也會跟著失敗。改成 `\x` 跳脫。
    - 既有突變 os-g8-link 的 expect，比不到 CMake 換行後的訊息。
  - minor 3 個，都已修：
    - RoleThread 的優先度對應沒有守衛：改成 cast 加 `static_assert`，並加經 RoleThread 讀回的案例。
    - 在 `std::thread` 裡用 REQUIRE：改成工作執行緒只讀值、回主執行緒再斷言。
    - allowlist 的註記過期。
  - note：
    - `Utf8ToWide` 與 Files.cpp 的 `ToWide` 近似重複：記為 code smell。
    - os.md Invariant 6 與 bool 回傳的關係：交使用者決定。
- 本機（macOS Intel x86_64）：core 34／34；產品組態 96／96；`worker` 與 `close_timing` 逐一串行 2／2；host-tools 3／3。
- 突變：
  - 本機能跑的都 killed：ti-x1、ti-x2、th-x1、th-x2、th-x4、th-x7、th-x8、os-g6、os-g8、tt1-stop-without-notify、tt2-role-backend-fixed。
  - 第一次跑時，ti-x2、th-x4、th-x7 是 wrong_failure。原因是 expect 沒寫對：CMake 的訊息會換行，測試的運算式也改了。改正後都 killed。
  - Windows 的 th-x3、th-x5，以及 Linux 的 th-x6、ti-x4，留給實機。
- 判定的變更（§14.4）：
  - 連結檢查的期望值多了 GYO::Os（threads）與 Threads::Threads（os）。這兩個檢查都只收緊，新加的「不得公開」也一樣。
  - `RoleThreadTests` 的判定沒有改。

## 已知缺口

- Windows 與 Linux 的 L2 還沒做。Windows 的非 ASCII 名稱讀回沒有先跑 Red，要等這次的 L1 與實機。
- `Utf8ToWide` 與 `ToWide` 近似重複。
