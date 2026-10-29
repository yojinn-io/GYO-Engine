# PvP v7.1：Windows 上的 worker 驗收（D216～D231）與 lan15-windows（D232～D240）

日期：2026-10-28～29。分支 `claude/pvp-v71-release`（PR #96）。

**狀態：結束（未通過的部分記為已知限界，使用者決定先發版，D231、D240）**。量測與判讀見 [worker-windows-timing.md](../object_fps_pvp/plans/v7.1/worker-windows-timing.md)；lan15 的規劃與宣告見 [lan15-windows.md](../object_fps_pvp/plans/v7.1/lan15-windows.md)、[lan15-declaration.md](../object_fps_pvp/plans/v7.1/lan15-declaration.md)。

## worker 在 Windows 上（D216～D231）

- **D216～D222（B）**：mock 的輪詢改用 Engine 的 Waiter（`f29001e0`）。Windows 上 `:817` 不再失敗，改停在重送檢查。
- **D224（C）**：重送檢查的失敗訊息附上 mock 與 Client 的送出次數和間隔（`2e15d99b`）。
- **D225～D227**：B 讓 `v7-08-submit-no-wake` 的檢出變弱（D225：A 20／20、B 5／20）。機制是 SubmitShot 和 Client 每 250 ms 一次的 Hello 之間的相位（D226）。
  - 修法：v7-08 的檢查等 Hello 之後再隔 100 ms 才 SubmitShot（`1870c2ad`）。
  - 突變確認（D227）：發行線 20／20、A 10／10，原版 10／10 通過，47／47 killed。
- **D228**：沒有 SDL 的 Windows 程序裡，Client 1 秒只重送 43 次。間隔符合 asio 計時器在 15.6 ms tick 下的分布。
- **D229**：開著 SDL 視窗的探針（`1c0f892d`），重送是 57 次；沒有視窗的是 43～53 次。判定是「其他」，因為對照的 P7 失敗在重送之前（WSAECONNRESET）。
- **D230**：token 等待的觀測（`bff5bf36`）。19 次中 8 次失敗，超過量 0.03～0.16 ms，看起來是 mock 輪詢約 1 ms 的刻度。判定是「其他」（W11 的 WSAECONNRESET）。
- **D231**：同一個問題換了 4 次測量方式，依 §14 開頭停下。使用者決定：Windows 的 worker 驗收不算發行條件，記為已知限界；mock 的兩個問題與 K10 排在 v7.1 之後。

## lan15-windows（D232～D240）

- **規劃（D232～D234）**：
  - 構成：Mac 主持（Match＋Gateway）；測試包用 L1 的產物。
  - F5 以 W4 結案；K10 移到 v7.1 之後。
  - 新模型（Player_V19＋nailer_v2，`f320c304`）併進發行線（`646b808f`），一起進 lan15。
- **宣告（D235～D239）**：
  - 第 1 版經 xhigh-reviewer 的對抗式檢查（blocker 2、major 11、minor 14），改成第 2 版。
  - 之後補上人數、Q2、判定工具（2.1）、`ping.txt` 的 UTF-16 與 Gateway 的版本核對（2.2）、Mac 不當玩家（2.3）。
- **工具**：`lan15_judge.py`，CTest `object_fps_pvp.lan15_judge`。
  - 合成場次判通過。
  - 25 份竄改都判出失敗或確認不了；另有 1 個 UTF-16 的測試，共 27 個測試。
  - 判定工具的 7 個突變都被殺。
- **L1**：run 40793575034（`646b808f`）四平台成功，產出測試包。
- **第 1 次場次（2026-10-29，離線的現場）**：確認不了。細節見 lan15-declaration.md 最後一節，要點如下。
  - 沒有照程序做：`marks.txt`、ping、斷線、Q1 都沒做；Match 與 Gateway 各啟動 2 次；S1 約 6 分鐘。
  - Q2 的 quad probe 命令違反 probe 的限制而失敗：主會話交出前沒有實際跑過。
  - 第 1 次的 Gateway 公告了別的網段的位址：腳本自動選了 en0。LUO 加入後 5 秒逾時 3 次。
  - KLYU 在對戰中出現 1 次連線品質警告，10 秒後恢復，沒有被逐出。為什麼判失敗，從 log 確認不了（觀測的缺口）。
  - S7：使用者看到新模型有問題，判斷是模型而不是 Engine，自己處理。
  - 探索性的觀察（第 2 段）：ingress 守恆通過；兩台的動作計數與 Match 完全一致；0 次中斷。
- **D240**：使用者決定先發版（v1.2.0），帶著新模型，不排第 2 次場次。lan15 的結果記為已知限界。

## 教訓（已存進會話記憶）

- 實機場次給每台機器一支腳本，包辦啟動、ping、收 log。交出前在本機實際跑過一次。
- 程式事先放好，不假設現場有網路。
- 伺服器公告的位址不自動選，由使用者確認和 Client 同網段。

## 證據

- worker：Windows 機器的 `build/target/_build/test/logs/worker-*-20261028/`、`worker-D230-20261029/`；本機的會話 scratchpad `v708/`。
- lan15：Mac 的 `~/gyo-lan15/session-20261029/`；`~/Downloads/20261029_KLYU_logs.zip`、`~/Downloads/logs-LUO.zip`。
