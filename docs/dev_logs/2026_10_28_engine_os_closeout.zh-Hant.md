# Engine os-closeout：OS 子系統盤點與收口

日期：2026-10-28。**本機完成**：os-freeze-wait（從 OS 分支撤出 os-wait-backend）與 os-closeout-docs（計畫文件改成收口後的狀態，寫下今後的規則）；os-handoff-v71（和消費端的會話對齊）也完成。四平台 L1 等推送；os-land-master 等消費端發行後。
計畫與理由見 [收口提案](../architecture/plans/os-subsystem/closeout.md)；決定是 D78（[交接](../architecture/plans/os-subsystem/HANDOFF.md)的決策紀錄）。

## 為什麼做

- 使用者的方向（2026-10-28）：OS 子系統的原意是一層 OS 相容層（adaptor），讓 Engine 的工作獨立於 OS。現在的核心是讓 Engine 支撐遊戲跑起來（v7.1 發行、v8 的玩法），不是 QA，也不是多系統相容。驗收以 CI 為主。
- 執行時擴大成約 18 批、決策 D40～D77，整條線估 100～150 h；帶了不少事前宣告的行為變更（★）、io 路徑契約的重新設計，以及計時搬移的四平台實機 L2。
- 消費端的 D218：收口方案核准前，發行線不再併入 OS 批次，也不為 OS 安排實機量測。

## 做了什麼

- 盤點（只讀）：逐批列出程式碼的位置（master、發行線、只在 OS 分支）、四平台 L1（用 `gh run view --json jobs` 確認 `L1 /` 開頭的 job 都是 success）、搬移還是 ★、支撐的功能。撤回的代價用 `git merge-tree` 在物件庫裡試算，沒有動任何 ref。
- 盤點時更正的前提：
  - os-windows-run 第 1 次的 W4～W7 已跑完（`844e92b1`、`7306a142`）。
  - Linux 的 Docker 量測已不在執行，沒有任何結果。
  - 消費端發行線的整合樹自 `4a4a0ef7` 起沒有四平台 L1。
  - PR #96 的標題只寫 A1～A3 與 P1～P5，實際上另外併了 10 批。
- 使用者核准（D78）：決定 1～4 都選 (a)，不做對抗式檢查。
- 本分支（`claude/trusting-joliot-f13b78`；在本工作樹以 `claude/os-closeout` 作業）併入 `claude/os-windows-run`（`3ab8f09c`）：HANDOFF 的決策紀錄在 D75、D77 與 D76 衝突，依編號三列都留下。
- os-freeze-wait：
  - `4ffc7c65`（os-threads-backend 定案）之後動過程式碼的只有 `4041cf42`、`756073aa`，動過條文的只有 `7e6592d1`、`756073aa`、`6316d657`（逐檔用 `git log 4ffc7c65..HEAD -- <檔案>` 確認）。
  - 從 `4ffc7c65` 取回：`engine/CMakeLists.txt`、`engine/os/CMakeLists.txt`、`engine/time/CMakeLists.txt`、`engine/time/src/Waiter.cpp`、`tests/common/os/CMakeLists.txt`、`tests/common/os/OsHeaderHygieneTests.cpp`、`tests/common/os/os_boundary_allowlist.txt`、`tests/common/time/CMakeLists.txt`、兩份突變目錄、`docs/architecture/os.md`、`docs/architecture/time.md`、`docs/architecture.md`。
  - 刪除：`engine/os/include/engine/os/WakeSignal.hpp`、`engine/os/src/wait/WakeSignal.cpp`、`tests/common/os/WakeSignalTests.cpp`、`tests/common/time/wake_compare.py`。
  - 取回後和 `4ffc7c65` 的差別，只有之後加的 `linux-run.Dockerfile`、`udp_error_probe.py` 與 master 的 platform_sdl 看門狗。
  - `docs/architecture.md` 的 GYO::Os 一列，保留 os-threads-backend 的「thread names and priority hints」（`7e6592d1` 順手補上的，屬於留下的批次）。
  - os.md 的部件表：Wait 標凍結，Net 標不開始。time.md 開頭改成「os-wait-backend 凍結」；Design Invariants 節保持 `4ffc7c65` 的原文。
  - 結果：time.md Invariant 8、10 與 os.md Invariant 6 的例外撤回（取代 D72）；依賴邊 `gyo_time → GYO::Os` 移除；`Waiter.cpp` 回到 OS 守衛的 allowlist；突變 ow-x1～ow-x10 離開目錄。
- os-closeout-docs：
  - README：狀態、D78、架構樹、進度表、依賴圖、「執行規則」開頭的今後規則 6 條。
  - PLAN：開頭的狀態，第 7.13～7.17 節開頭標上 D78 的去留。
  - HANDOFF：狀態行、現況、代號對照（os-closeout、os-freeze-wait、os-closeout-docs、os-handoff-v71、os-land-master）、D78、os-wait-backend 的紀錄、「下一次實機的清單」標取消、未結事項。
  - windows-run.md 標「第 1 次結案」，linux-run.md 標「取消」。
  - os.md 加 Replacement Criterion 9：OS 層是 OS 差異唯一的落點，只在觀測到壓力時擴大；搬移只用 CI 加守衛驗收，需要實機 L2／L3、★ 或判定變更的就不是純搬移。
  - closeout.md：狀態改成核准，更正盤點的錯誤（見下）。
- os-handoff-v71（使用者要求提前並改成雙向）：
  - 送出：
    - 發行線不再併 OS 批次；
    - lan15 的前提（D209、D215）解除；
    - worker-10x 沒有執行者；
    - 整合樹沒有 L1；
    - W4 與 W6 的產品線索；
    - 5 個問題。
  - 消費端回覆（消費端 D219，發行線 `6b448c16`）：
    - 發行前不需要 OS 這邊做任何事；
    - worker-10x 由消費端安排；
    - 4 批 ★ 會寫進 PR #96 的說明與發行說明；
    - 合併前的 L1 由消費端派發（run 40644602172）；
    - K9／W6 列進 lan15 的觀察。
  - 本會話再回覆了 4 批 ★ 各一句的行為摘要。

## 驗證與經緯

- 本機（macOS Intel）：
  - core preset：configure、build、`ctest --preset core` 34／34 通過。
  - test preset（產品組態）：configure、build、`ctest --preset test`（標籤 `cpu|shader`）92／92 通過。
- 相關突變（D184：Waiter.cpp、allowlist、engine/time 的 CMake 有變；用 test preset 的建置樹，每條 `--only` 單獨跑）：**9／9 killed**，每條 exit 0。
  - 涵蓋：tt1-waiter-early、tt1-notify-dropped、tt1-latch-cleared-on-entry、tt1-latch-never-consumed、tt1-no-recheck、tt1-latch-before-deadline（Waiter.cpp 回到搬移前）、os-g6-stale（allowlist）、ti-x1-time-links-more（`GYO::Time` 的連結檢查）、ti-x3-backend-name-changed（後端名稱）。
  - runner 跑完後程式碼檔案都已還原。
  - 撤回後的 `engine/time` 和發行線（`origin/claude/pvp-v71-release`）逐位元組相同（`git diff --stat` 沒有列出）。本分支和發行線在 engine 與 tests/common 的程式差異，只剩留下的 os-threads-backend 與 os-wake-record。
- 四平台 L1：推送後派發（推送前先問使用者）。
- 盤點的更正：
  - 提案原本寫 os-console-shutdown 的主控台 L3「未做」。實際上消費端在 10-27 的 D201，已在 conhost 實機做了 Ctrl+C 與關閉視窗，都通過。
  - 原因是盤點時沒有讀消費端的 D201 dev_log。使用者問「你在做規劃時有考慮v7.1會話的需求嗎？」，回頭對照消費端的 HANDOFF 時才發現，已在 closeout.md 與 HANDOFF 的 D78 更正。
  - 同時找到的漏項：消費端現況裡的「OS 子系統的 3 批」、lan15 的前提、worker-10x 失去執行者。這些都改由雙向的 os-handoff-v71 處理。

## 證據

- 盤點：[closeout.md](../architecture/plans/os-subsystem/closeout.md) 的第 2 節（run 號、commit）與第 8 節（證據位置）。
- 本機 log：會話 scratchpad 的 `core.log`、`test.log`、`mutations.log`（不進 repo）。
