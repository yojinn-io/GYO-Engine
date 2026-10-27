# Engine os-wake-record：晚醒量測程式合回與 macOS Intel 基準

日期：2026-10-27。**本機完成**：量測程式與程序已合回本分支，並照 D68 量了 macOS Intel 的基準。Windows（沒有視窗、有視窗）與 Linux 要等下一次實機用本批的 commit 量；macOS arm64 沒有實機（D11⑩）。推送要等使用者同意。
計畫見 [os-wake-record](../architecture/plans/os-subsystem/PLAN.md)（第 7.13 節）；紀錄見 [交接](../architecture/plans/os-subsystem/HANDOFF.md)的「os-wake-record」；開工前的決定是 D68。

## 做了什麼

- 合入 TT-1 L2 的量測程式 `tests/common/time/WakeRecord.cpp`：從本機分支 `claude/tt1-l2-wake-record`（`a8f9535f`）合入，沒有衝突。
  - 兩個執行檔：`gyo_time_wake_record`（沒有視窗）、`gyo_time_wake_record_window`（Engine 的 SDL 視窗）。都不是 CTest。
- 新增 notify→wake 的量法（D68：用 high 設計）：
  - 另一個執行緒以 60 Hz 通知，等待的執行緒記錄「通知後多久醒來」。
  - 通知方在 Notify 前蓋時間戳；等待方取用之後，通知方才發下一次，所以時間戳不會在讀取前被下一次覆蓋。等待方還沒取用的週期會跳過並計數（`skipped_periods`）。
  - Waiter 用 `Wait()`（沒有期限，Invariant 6），condition_variable 用帶條件的 `wait`。延遲含 Notify 呼叫本身。
- 把宣告的程序寫成腳本 `tests/common/time/wake_baseline.py`（標準庫的 Python，跨平台）：
  - 每種情境前後各量 5 秒 sleeper。
  - `meta.txt` 記錄 commit、程式的 SHA-256、OS、CPU、開始與結束的負載。
  - 理由：這個程序要在四個平台照樣重跑（AGENTS §5：已確認會重複的手順才寫成 Script）。
- time.md 加「Baselines」一節：程式、程序、各平台的紀錄與證據位置。

## 驗證與經緯

- 1 秒的試跑：五個項目都有樣本，notify→wake 沒有跳過的週期。
- 更正：試跑的 `meta.txt` 顯示本機是 macOS Intel（x86_64，i9-9980HK）。
  - 開工前我把本機說成 macOS arm64，os-path-utf8-sweep 與 os-utf8-codepage 的本機驗證紀錄也寫成 macOS arm64。repo 一直記著「實機只有 macOS Intel」（D11⑩）。
  - 那些紀錄已更正（HANDOFF 三處、兩份 dev_log）。這次本機量的是計畫裡的「macOS Intel 實機」；macOS arm64 不量。
- macOS Intel 的基準（2026-10-27，程式 `f6997194`，只記錄）：
  - 證據在主 checkout 的 `build/target/_build/test/logs/os-wake-baseline-20261027-macos-intel/`，`files.sha256` 的 SHA-256 是 `a3066291…`。
  - 主機：負載 2.06 → 1.82；sleeper 的 P99 在 4.2～8.0 ms 之間。
  - 期限等待的 P99（沒有視窗；有視窗）：
    - Waiter：0.145；0.134 ms。沒有視窗那組有 2 次超過 4 ms，最大 26 ms。
    - condition_variable：4.17；2.11 ms。
    - `sleep_until`：7.61；2.07 ms。
  - notify→wake 的 P50／P99：
    - Waiter 0.049／0.068 ms，condition_variable 0.052／0.067 ms（沒有視窗）。
    - 有視窗時幾乎相同。
    - 都沒有跳過的週期。
  - 觀察：「Waiter 的 P99 低於 condition_variable」兩種情境都成立；和 TT-1 L2（同一台機器）同一量級。
- 和消費端協調（D68）：已告知 v7.1，Windows 沒有視窗的那組數據交給它的 F5／F7 參考，Match 層的數據仍由它在 lan15 量。

## 已知缺口

- Windows 與 Linux 的基準還沒量。Windows 上不初始化 SDL 的程序，仍是 Engine 層從未量過的情況。
- macOS arm64 沒有實機。
- sleeper 用的是 Python 的 sleep，只能當主機忙碌程度的參考，不是 OS 計時器的精度。
