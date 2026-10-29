# Engine os-wait-backend：Waiter 的等待原語搬進 engine/os

日期：2026-10-27。**完成本機與 L1**（`756073aa`）：macOS Intel 的 L2 通過（事前凍結的 12 項全部通過），xhigh 審查的發現已修，四平台 L1（run 40510515336）第 2 次通過（第 1 次是不相關測試的逾時，見下）。Windows 與 Linux 的 L2 等下一次實機。
計畫見 [os-wait-backend](../architecture/plans/os-subsystem/PLAN.md)（第 3.8、7.15 節）；紀錄見 [交接](../architecture/plans/os-subsystem/HANDOFF.md)的「os-wait-backend」；開工前的決定是 D71。

## 做了什麼

- engine/os：
  - 新增 `WakeSignal.hpp`：`Create()`、`Signal()`、`Block(latch, timeout)`、`Mechanism()`，以及和 Time 的 `WaitBackend` 一一對應的 `WaitMechanism`。
  - 實作從 `Waiter.cpp` 逐行搬到 `engine/os/src/wait/WakeSignal.cpp`。依開放決策 11 (c)，一個檔案以 `#if` 分平台：macOS 的 kqueue、Windows 的 event 加 waitable timer（含兩層退回）、其他平台的 condition_variable。
  - cv 後備由 `Block` 的參數拿到 Time 的 latch 參照，在 mutex 內讀，不保存副本（os.md Invariant 8）。
  - `Block` 保留不是 noexcept 的例外規格：cv 後備可讓標準函式庫的 `std::system_error` 穿過，和搬移前的 Waiter 相同（os.md Invariant 6 的例外，D72 定案）。
  - ★（D71）：`Waiter::Create` 失敗時，detail 改成 engine/os 的固定格式 `IoFailed: errno=<n>`／`IoFailed: win32=<n>`，message 不變。
- engine/time：
  - `Waiter` 只留 latch、單一等待者、deadline 迴圈、醒後重讀時間基準與 LateSince；OS 標頭與 `#if` 都拿掉。
  - `WaitMechanism` 轉 `WaitBackend` 用 `static_cast`，四個值用 `static_assert` 對齊；後端名稱不變。
  - `gyo_time` 改成 PUBLIC `GYO::Base`、PRIVATE `GYO::Os`。`Threads::Threads` 隨 cv 後備移到 engine/os。
- 守衛與連結檢查：
  - Engine 的 allowlist 刪掉 `Waiter.cpp` 的 R1、R2、R3；`WaiterTests.cpp` 的 R2 改標為永久條目（各 OS 必須選的後端，time.md Invariant 7）。
  - `gyo_time` 是 exactly GYO::Base、GYO::Os，且 GYO::Os 不得公開。
- 測試：`tests/common/os/WakeSignalTests.cpp`。
  - 機制、Block 之前送出的信號、另一執行緒的信號（有逾時與沒有逾時）、相對逾時（上限與下限）、latch 檢查期間送出的信號的競爭（20000 輪）、moved-from。
  - 每個可能卡住的 Block 都有逾時或救援，壞掉時是檢查失敗，不是卡住。
  - `WaiterTests`、`RoleThreadTests`、`TraceSinkTests` 的判定不改。
- L2 的工具：`tests/common/time/wake_compare.py`，A、B 兩組程式依 A B A B … 交錯跑，照事前凍結的規則判定。
- 文件：os.md（Wait 的合約、Invariant 6 的例外、Invariant 8 的守衛）、time.md（Ownership、Invariant 7、8、10、比對腳本）、docs/architecture.md 的依賴欄。

## 驗證與經緯

- 開放決策 13 的判定（D71，看 B 之前凍結）：
  - 對抗式檢查（既有證據）：來源每個情境只量過一次；CLI 的 P50 跨日差 0.052 ms；P99 會被主機尖峰推高。
  - A/A 校準 4 對：12 項全部通過；單次的 CLI P50 散得比 +0.02 ms 大，但兩側 4 次的中位數只差 0.0001 ms。
  - 退化版（拿掉 `NOTE_CRITICAL`）1 對：deadline 四項都不通過（CLI P99 8.12 ms）；notify 四項通過，所以這次的檢出力檢查不涵蓋 notify。
  - 兩個事前條件都符合，(a) 原文凍結（`6edb813d`）。
- 邊界守衛的誤報：R3 把成員初始化 `signal(…)` 當成 CRT 的 `signal`；成員改名，規則不動。
- xhigh 審查（1 位，Fable 5.1，PR #98 的規則）：沒有 blocker。
  - major 2 個，都已修：ow-x7 在 Windows 殺不掉（改成平移兩格）；競爭測試對 ow-x1 的檢出力不足（掃描移到 waiter 端；cv 後備強制編譯的探索性檢查，命中率約十倍）。
  - minor：文件把 ow-x1 寫成已殺死（改正）；「沒有建置證據」不成立（審查者讀了主 checkout 的建置樹）；Linux 判定範圍的歧義（在任何 B 之前寫定）；兩個測試缺口（已補，加 ow-x9、ow-x10）。
- 本機（macOS Intel x86_64）：core 34／34；產品組態 98／98；`object_fps_pvp.ipc`、`.worker`、`.close_timing` 逐一串行 3／3。
- 突變（本機能跑的都 killed）：tt1-* 7／7；ow-x2～x4、ow-x7～x9、ti-x1～x3、th-x1、os-g6、os-g8。Linux 的 ow-x1 與 Windows 的 ow-x5、ow-x6、ow-x10 留給實機。
- L2（macOS Intel，A＝`4ffc7c65`、B＝`756073aa`，交錯 4 對）：
  - 收支：一致。計畫 16 次、跑完 16 次、無效 0 次；notify 的「收到＋略過」每次都是 1800（略過的 4 個週期在同一次主機尖峰）。
  - B 通過，12 項全部通過。B 與 A 的中位數：CLI deadline P99 0.152／0.149 ms、視窗 0.132／0.133 ms；notify P99 CLI 0.075／0.071 ms、視窗 0.061／0.059 ms。早醒 0；後端都是 Kqueue。
  - 證據在主 checkout 的 `build/target/_build/test/logs/os-wait-l2-20261027-macos-intel/`（`programs/`、`calibration-AA/`、`detection-AD/`、`AB/`）。

- 四平台 L1（[run 40510515336](https://github.com/yojinn-io/GYO-Engine/actions/runs/40510515336)，`908308b3`）：失敗一個 job。toolchain / linux-x64 的 `platform_sdl_concurrency` 逾時（60 s）；其餘全部成功，四平台的 `gyo_os_tests`、`gyo_time_tests`、連結檢查、邊界守衛都通過。逾時的案例不經過本批的程式碼，同一個 commit 的 L1 / linux-x64 以 2.29 s 通過；這個測試有時序相關失敗的紀錄（消費端 K11、D186）。歸類未能完全切分，記錄不覆寫。
- 重跑（同一個 run 的第 2 次）：toolchain / linux-x64 成功，`platform_sdl_concurrency` 5.27 s 通過，CI gate 成功。**四平台 L1 第 2 次通過**。逾時的調查（兩種方法都沒在 macOS 重現）交給 v7.1 會話與對應的任務卡；細節在 HANDOFF。

- D72：time.md Invariant 8、10，os.md Invariant 6 的例外，★ 錯誤細節，都照草稿定案。

## 已知缺口

- Windows 與 Linux 的 L2 還沒做；Windows 的 L2 通過前，不通知消費端合併這一批（D71）。
- ow-x1（cv 後備在 mutex 外讀 latch）只在 Linux 編得到，還沒在 Linux 跑；macOS 上強制 cv 的探索性檢查殺得到它。
- 逾時換算的進位方式與 ★ 的 detail 格式沒有測試，只靠審查。
