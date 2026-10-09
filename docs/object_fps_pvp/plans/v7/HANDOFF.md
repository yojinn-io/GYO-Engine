# PvP v7 交接

更新：2026-10-09。Owner：`object_fps_pvp`。**狀態：P1a、P1b 完成**（PR [#70](https://github.com/yojinn-io/GYO-Engine/pull/70) `f3d176d`、PR [#71](https://github.com/yojinn-io/GYO-Engine/pull/71) `660310e` 已合併；第 01～05 批與 TT-1 完成）；**P2 進行中**（PR [#73](https://github.com/yojinn-io/GYO-Engine/pull/73)，分支 `claude/pvp-v7-p2`：P2-log 完成；第 06 批的實作與 L1 完成，L2 的事前宣告待核准）。
本文件是 v7 的記錄器：每批開始、里程碑、停止時，和工作在同一個變更中更新。
數字未標「實機」的，是 CPU 模擬或靜態分析的結果。

## 現況（每次停下來時更新）

停下來等使用者、或回報里程碑之前，先更新本節再回報。「等你決定」只列需要使用者決定的事；決定之後移到決策紀錄或進度記錄器，並從本節刪掉。長期的已知問題放「未結事項」，不在這裡重複。

- **等你決定**：
  1. 第 06 批 L2 的事前宣告（[第 06 批](06-gateway-results-30hz.md)的「L2 的事前宣告」）：核准、修改或不做。建議核准。內容：5 案 × 6 輪 × before／after，共 60 次，約 30 分鐘；判定 after 全部通過、結果間隔 P50 約 33.3 ms、Gateway 端最小間隔 ≥16.6 ms。跑的時候機器要閒置（像 C1 第 2 次那樣關掉 Chrome 等）。
- **進行中**：
  - PR [#73](https://github.com/yojinn-io/GYO-Engine/pull/73)（P2 線）：`61ca497` 的 CI 全綠（macos-arm64 的測試修正生效）。第 06 批（`7da6b0d`）與本次文件推送後重跑 CI。
- **下一步**：核准後執行第 06 批的 L2，結果寫回批次文件，再請你決定是否開始第 07 批。
- **最後更新**：2026-10-09 14:05，依據 commit `7da6b0d`（第 06 批）。

## 閱讀入口

1. [進度與執行規則](README.md)：任務、子系統的處理、批次、PR 線、依賴、平台表。
2. 本文件：先看「現況」；再看決策、各批紀錄、P2 以後各批的範圍、未結事項。
3. [盤點](INVENTORY.md)：時間、執行緒、sleep、socket 輪詢的唯讀盤點，以及更正與研究摘要。
4. 指定批次的文件（02～06、P2-log）。
5. Engine 部分的正式來源：
   - [時間、執行緒與 Trace](../../../architecture/plans/time-threads-trace/README.md)：TT-1、TT-2。
   - [輸入與呈現](../../../architecture/plans/input-and-present/README.md)：IP-3～IP-5。
6. v6 的紀錄：[v6 交接](../v6/HANDOFF.md)（D0～D26）、[v6 穩定基線](../v6/STABLE_BASELINE.md)。

## 決策紀錄

決策從 D27 接續編號（v6 交接與各 Engine 計畫夾共用同一套 D 編號）。引用 v6 的決策時寫「v6 Dxx」。

| # | 決策 |
|---|---|
| D27 | （2026-10-08）Engine 子系統的分配。使用者的修正（原話）：「照這樣修正」，針對「Engine 做最小的角色執行緒，不做執行緒池」的提案。<br>①建進 Engine：Time、最小的 Threads（角色執行緒：名稱、協作停止、優先級提示）、精簡的 Trace、SDL 隔離、Display、Settings 的 io 機制、Audio。<br>②Channels 不先統一，各自實作，重複出現再抽出。<br>③Net transport 留在產品內（asio）。<br>④Job／執行緒池 v7 不建；方向寫進 README：建在 Threads 之上，worker 數＝核心數扣掉角色執行緒。<br>⑤不改 `FixedTickRuntime` 與 `RuntimeLoop` 的語意，不做 render 執行緒，不做錄製重播。<br>⑥修訂輸入與呈現計畫的 D19（原文：「主執行緒（事件＋畫面）、模擬、網路三個角色的分離屬新的 Engine 計畫」）：三角色的執行緒由 Engine 的 `GYO::Threads` 與 `GYO::Time` 提供；角色本身（迴圈、交接、政策）由消費端在產品內做。<br>⑦framework 層的現況與啟動條件寫進 README（使用者同意） |
| D28 | （2026-10-08，照建議）Engine 計畫夾：新建 `time-threads-trace`（TT-1、TT-2）；SDL 隔離與顯示接續在輸入與呈現計畫，編為 IP-3～IP-5（owner 模組相同，另加 `engine/io`）；`audio` 等 P6 開始時再建 |
| D29 | （2026-10-08）`GYO::Time` 是獨立的 Base 層 target（`engine/time`，不含 SDL），並且是 **Engine 的時間基準**。使用者原話：「D29 選 A，並把 GYO::Time 定為 Engine 的時間基準；RuntimeLoop 也在 TT-1 改用它，只換時鐘來源，不改 loop 的語意。」<br>Waiter 採 latch，不收 `std::stop_token`；後端：macOS kqueue、Windows 高解析度 waitable timer、其他平台 cv，都用相對期限加醒後依 steady 重新檢查。例外：`AssetWatcher` 的檔案時間屬牆鐘語意，v7 不改 |
| D30 | （2026-10-08，照建議；2026-10-09 修訂）主執行緒停住、沒有發布輸入時，模擬角色沿用最後的意圖，直到年齡超過 max(150 ms, 3×近期發布間隔)；之後軸回中立、瞄準維持、丟棄跳躍上升沿。宣告最低支援 20 FPS。修訂：最小門檻由 100 ms 改為 3×(1／20 FPS)＝150 ms，讓單一 120 ms 的長幀也不中途停步（第 04 批的驗收 (d) 照原文） |
| D31 | （2026-10-08，照建議）30 FPS 對比拆成 C1（第 05 批：`fee92ff` 對 P1b 頭，只有任務 1、2，Gateway 缺陷兩樹都保留）與 C2（第 09 批：任務 3 之後、pv7 之前）。各指標的權威來源、主機狀態的分層與獨立性檢查、輪數，見 README 的對比做法 |
| D32 | （2026-10-08，照建議）pv7 只在第 11 批升一次，嚴格版本相等，三個角色同一個 PR，舊程序重新啟動（沿用 v6 D11①） |
| D33 | （2026-10-08，照建議）任務 7 的完成條件以 SDL 符號判定，範圍含產品測試；Engine 後端公開標頭去掉 SDL 型別（`*Native.hpp`），SDL3 改為 PRIVATE 連結。ui_editor 與未啟用產品只寫遷移清單 |
| D34 | （2026-10-08，照建議）跨平台驗收依 README 的平台表：Windows 只靠 LAN 場次；Linux 與 macOS arm64 標「未驗證」；不做 CI 只記錄的計時測試；LAN 排不出時，是否升格由使用者在第 16 批開始時決定（比照 v6 D26） |
| — | （2026-10-08）P1 拆成 P1a（02、03、03a，只改產品）與 P1b（TT-1、04、05，跨層）；加做 03a 小量測（使用者決定） |
| D40 | （2026-10-08）第 03 批移到 P1b，和第 04 批一起合併（使用者原話：「選 (1)，第 03 批移到 P1b」）。理由：產品路徑的回復測試在 fps 30 的 1 個組合出現 Held（v6 路徑沒有），原因是在命令跟著畫面幀產生的架構下相位更早收斂；不放寬測試，也不讓 master 出現 30 FPS 變差的中間狀態。P1a＝02、03a；P1b＝03、TT-1、04、05。03a 改在含 03 的 P1b 分支頭上量測，那個頭不合併。D35～D39 是預留給各功能線開始前確認的編號，所以本決定編為 D40 |
| D41 | （2026-10-09，同日更正與補充）FPS 和解析度一樣是玩家的選擇（如同遊戲內的畫面設定），不只是限幀。v7 沒有 UI，第 13 批以命令列選項讓玩家選擇解析度、視窗模式與 FPS 上限（`--fps <上限>` 等），30 FPS 手感的 L3 也在第 13 批做；在 v7 收尾時和解析度功能一起做，不另開批次與 PR。遊戲內的設定 UI（設定選單、套用後未確認就還原）與保存選擇的 `settings.json` 移到 v8。連帶：IP-5 的使用者目錄與 `engine/io` 的原子寫入只服務設定的保存，也一起移到 v8。使用者原話：「30 FPS 手感：PR #71 合併後另開小批，可以開在v7驗收之前的批次裡，產品加 --fps 限幀選項，再做 30 FPS 的 L3。在調整畫面的功能裡，要有畫面分辨率和fps選擇。」；更正：「D41 理解有誤：30 FPS 的 --fps 限幀和 30 FPS 手感的 L3 併進第 13 批（解析度與畫面設定），在 v7 收尾時和解析度功能一起做，不另開 05b／P1c。」；補充：「D41 我來說說我的想法，首先這個不單單是一個fps限制，也應該是玩家的選擇，像是在遊戲中那樣調整fps和解析度那樣。不過現在可以用 --fps 的方式來做，因為現在沒有UI。UI的部分我想是放到v8裡來實現。」「1 照建議用命令列，2 放到 v8」。最初誤記為另開第 05b 批（PR 線 P1c），已刪除 |
| D42 | （2026-10-09）第 06 批碰到停止條件（`action_probe.py` 的「結果每秒 ≤31」在 relay 量到 32）的處理選 (a)：接受本批的行為，不改凍結的 `action_probe.py`；Gateway 的統計行加上最短的寫出間隔，「不爆量」改在 Gateway 端判定。使用者原話：「a 照建議，繼續」 |

在各功能線開始前確認（先附建議）：

| # | 問題 | 建議 | 時點 |
|---|---|---|---|
| D35 | Client↔Gateway 時間回聲要不要放進 pv7 | 放進：LAN 實際的偏移（約 0.45 s）在 Client 與 Mac 之間，runtime link 同機；這是量兩機漂移的唯一方法 | P3 開始前 |
| D36 | 任務 3 之後的動作送出語意 | 保持 30 Hz 的最小間隔，符合資格時在 SubmitAction 當下就送出（平均約縮短 16 ms，不改 wire） | P2 |
| D37 | 設定與日誌的位置；ui_editor 的原子寫入要不要併入 `engine/io` | 設定放在 `SDL_GetPrefPath`；日誌留在執行檔旁（LAN 手冊依賴）；併入（工具 ownership 的變更） | P5 |
| D38 | 音效資產的來源 | 由產品腳本程序生成 WAV（48 kHz），可重現、授權單純 | P6 |
| D39 | 審查方式 | pv7 契約用 ultracode 審查（比照 v6 第 09 批）；TT-1 與 IP-3 的公開介面各 1 位 xhigh 審查；C1 宣告草案由 1 位評審加 1 次對抗式檢查。各批開始時徵求同意 | 各批開始時 |

## 第 01 批進度（記錄器）：計畫

- 2026-10-08：v7 開始（使用者指示）。先確認 v1.1.0 已發佈，紀錄寫進 v6 交接的第 17 批。
- 2026-10-08：唯讀盤點（ultracode）：9 個區域 agent（Engine 核心、Engine 其他模組與 services、Client、Match、Gateway、驗收 C++、驗收 Python／測試／CI、其他產品與工具、本機證據日誌）＋1 次 xhigh 對抗式完整性檢查。303 個地點；結果見 [盤點](INVENTORY.md)。
- 2026-10-08：分批規劃（ultracode）：研究 2 個、方案 3 個（證據優先、產品垂直切片、Engine 由下而上）、評審 1 個（xhigh，選垂直切片為主幹，嫁接 15 個做法）、對抗式檢查 1 次（xhigh，0 blocker、10 major，全部修進計畫）。
- 2026-10-08：使用者決定 D27～D34，以及 P1 拆成 P1a／P1b、加做 03a。檔位：第 01 批 medium。
- 本機證據（git 忽略）：
  - `build/target/_build/test/logs/pvp-v7-inventory-20261008/`：inventory.json、digest、規劃的 JSON、確認後的計畫草稿、分析輔助腳本；`files.sha256` 的 SHA-256 `ddc23da0…`。
  - `build/target/_build/test/logs/engine-time-platform-inventory-20261008/`：Engine 範圍的研究 JSON 與計時探針原始碼；`files.sha256` 的 SHA-256 `3f2822ba…`。
- 2026-10-08：PR [#69](https://github.com/yojinn-io/GYO-Engine/pull/69) 開出。CI 的「Select CI scope」失敗：共通測試 `test_app_registry.py` 的 Prepare 測試以寫死的 `v2026.10.1` 在真正的 repo 上執行，`tools-v2026.10.1` 實際發佈之後，`9a6fa8e` 以外的 commit 都會失敗。改用不會發佈的 `v2099.12.99`（`1f7dfc6`），本機 `tests/common/ci` 241 個測試通過，CI 全綠。
- 2026-10-08：#69 依使用者指示合併（`a7cba38`）。**第 01 批完成。**

## 第 02 批進度（記錄器）：ClientSimulation 接縫

- 2026-10-08：開始（使用者指示）。主對話檔位 high。分支 `claude/pvp-v7-p1a` 自 master `a7cba38`。
- 2026-10-08：實作與 L1 完成。細節與結果見[第 02 批](02-client-simulation-seam.md)。
  - 新的產品庫 `client_simulation`（`ClientSimulation`）：持有 `LocalPlayerPrediction`、`PredictionElapsedTime` 與 snapshot 閘；`Observe` 與 `Frame` 對應 v6 的 `PvpApplication.cpp:419-424` 與 `:969-976`。`SelectArena` 照 v6 保留 snapshot 閘。
  - 邊界：Drain、`SnapshotTimeline.Push` 與 `SendInput` 留在呼叫端（`Frame` 回傳要發布的視窗）；本批的庫不依賴網路庫。這是相對於計畫文字（「包住 Drain 與 SendInput」）的調整，理由是 Drain 的其他結果屬於呈現，第 04 批的模擬角色改用另一個自身樣本佇列。
  - 產品與 5 個無頭 probe（矩陣 `gameplay_action.hpp`、`action_main`、`timing_main`、`quad_main`、`network_main`）改走 `ClientSimulation`；GUI probe 經由 `PvpApplication`。
  - 等價測試：v6 產品路徑的凍結參考模型與 `ClientSimulation` 在 60／30／144 FPS、停頓、長幀、重設、arena 重選、失去控制下逐位元組相同。
  - 原始碼守衛 `object_fps_pvp.probe_command_path`；突變 `v7-02-*` 5／5 killed。
  - CTest 全標籤 64／64；權威兩樹比對 35／35（base `a7cba38`，worktree `../GYO-Engine-v7base`）。
  - 開發跑次（不計次）：矩陣 clean-60、clean-30 各 1 輪通過；`run_network.py` 1 次通過。
  - probe 端的行為差異（第一幀 elapsed、`action_main`／`timing_main` 開始套用移動規則、`network_main` 停頓後的 elapsed）照實記在批次文件。
  - 證據：`build/target/_build/test/logs/pvp-v7-batch02-20261008/`。
- 下一步：第 03 批（每份 snapshot 進相位追蹤）。

## 第 03 批進度（記錄器）：每份 snapshot 進相位追蹤

- 2026-10-08：開始（使用者指示）。主對話 high；實作後由 1 位 xhigh 審查 agent 對抗式審查（使用者同意）。
- 2026-10-08：實作與 L1。細節見[第 03 批](03-per-snapshot-phase.md)。
  - `LocalPlayerPrediction::ObservePhaseSample`、`Reseeds`；`ClientSimulation::ObserveSample`；觀測值新增 `phaseSamples`、`phaseSampleSequence`。
  - 一幀一份時與 v6 參考模型逐位元組相同；多份時每個樣本只用一次、依序；突變 v7-03 4／4 killed；權威 35／35；CTest 64／64（審查前）。
  - 開發跑次：clean-60 通過；clean-30 在 8 ms 狀態失敗（Actual 97.6%、Held 21／20），保留不重跑；`run_network.py` 通過。
- 2026-10-08：xhigh 審查：沒有 blocker。major：既有的收斂與 Held 測試只走 v6 路徑。minor：舊樣本觸發的修正被重新播種丟掉但計數照算；測試涵蓋與守衛的順序檢查；settling 期間的舊樣本留在下一個視窗（既有，屬定義，列為後續）。審查也解釋了 clean-30：樣本加倍讓相位更早收斂，在幀量化之下餘裕偏緊。
- 2026-10-08：依審查修正（使用者同意 A）。`MovementRecoveryTests` 改為 v6／產品兩條路徑各跑一次之後，產品路徑出現 1 個失敗組合（fps 30、RTT 20、108 ms 停頓、受損網路：恢復期限後 Held 4 次）。拿掉「重新播種時丟掉舊樣本」仍失敗，原因是本批的核心。依變速箱規則停下回報。
- 2026-10-08：使用者決定 D40：第 03 批移到 P1b。工作 commit 在 `claude/pvp-v7-p1b`（`03b2ea7`，含已知失敗的測試）；`claude/pvp-v7-p1a` 維持只有第 02 批。
- 證據：`build/target/_build/test/logs/pvp-v7-batch03-20261008/`。
- 下一步：第 03a 批（量測含 03 的 P1b 分支頭），建議 medium。

## 第 03a 批進度（記錄器）：相位追蹤的小量測

- 2026-10-08：開始（使用者指示，主對話 medium）。worktree `../GYO-Engine-v6final`（`fee92ff`）與 `../GYO-Engine-p1b03`（`03b2ea7`）從零建置。事前宣告（使用者核准，`56bba67e…`）。
- 2026-10-08：第一次嘗試因 runner 錯誤，12 輪都在啟動前結束（沒有數據，保留）；修正後經使用者同意照原宣告重跑，12 輪完成。結果與觀察見[第 03a 批](03a-phase-only-measurement.md)：11 輪在 4 ms 狀態；clean-30 看不出方向；clean-60 的 after 有 1 輪集中的 Held 與 2 輪延遲中位數約晚 1 幀（和審查指出的相位偏晚同方向）。**第 03a 批完成。**
- 證據：`build/target/_build/test/logs/pvp-v7-batch03a-20261008/`（`08922f71…`）。
- 下一步：開 P1a 的 PR（第 02 批的程式＋文件）。

## P1b 進度（記錄器）

- 2026-10-08：#70 合併（`f3d176d`），P1a 完成；worktree `../GYO-Engine-v7base`、`../GYO-Engine-p1b03` 移除；`claude/pvp-v7-p1b` rebase 到新的 master（第 03 批為 `a393f7e`）。
- 2026-10-08：使用者決定：TT-1 的公開介面以 1 位 xhigh 審查 agent 檢查（D39）；相位追蹤的既有問題（settling 的樣本留在下一個視窗）先不改，第 04 批照計畫做，看 C1 的結果再決定。
- 2026-10-08：TT-1 實作與 L1 完成、xhigh 審查完成並修正（Engine 計畫的交接）；L2（只記錄）等使用者核准事前宣告。下一步：第 04 批。
- 2026-10-08：第 04 批開始（WIP commit）：模擬角色 `ClientSimulationRole`、PvpApplication 與無頭 probe 改接、GUI probe 斷言改寫、L1 (a)～(g)；產品路徑的回復測試在模擬角色下通過（第 03 批的已知失敗解除）。未完：分析器 v7、突變正式跑次、TSan、紀錄。待使用者決定的兩點寫在[第 04 批](04-client-roles.md)。
- 2026-10-09：第 04 批的實作與 L1 大致完成（`aa9a342` 起）：全量 CTest 67／67、突變 9／9、權威 35／35、TSan 0 報告、開發跑次 clean-30／clean-60／250 ms 停頓都通過。剩下：GUI probe 的開發跑次與 GUI 突變（需要畫面）、使用者的 2 項決定（D30 與 (d)、輸入延遲），以及是否對局部做 xhigh 審查。
- 2026-10-09：使用者決定 D30 修訂（150 ms）、其他照建議；GUI 開發跑次（使用者不在、caffeinate）與 GUI 突變完成；xhigh 審查（1 major、4 minor）修正完成（`347d375`）：突變 14／14、GUI 突變 killed、開發跑次與 TSan 都通過。待使用者：D30 門檻逐級放大的小決定（見第 04 批）、TT-1 的 L2 宣告、第 05 批的宣告。下一步：P1b 的 PR（CI 綠燈後才能做 C1）。
- 2026-10-09：使用者決定：D30 門檻逐級放大的問題先不改（C1 之後再看）；第 05 批宣告做 D39 審查（1 位評審加 1 次對抗式檢查）。宣告草案已具體化（`05-30fps-comparison.md`），PR #71 已開（auto-fix）。
- 2026-10-09：第 05 批宣告的 D39 審查完成（評審：必修 6、建議 13；對抗式檢查 12 項），草案依審查改寫，待使用者核准。PR #71 的 CI 在 `805bd11` 全綠。
- 2026-10-09：使用者核准 C1 宣告與 TT-1 的 L2（「C1 宣告核准，TT-1 的 L2 也核准，可以啟動了」）。TT-1 L2 的量測程式不放進 PR #71（會讓 C1 作廢），留在本機分支 `claude/tt1-l2-wake-record`（`a8f9535`），之後的 PR 再合入（使用者選 (b)）。after tree 從零建置（`../GYO-Engine-p1b-c1`，`805bd11`）、預檢通過；暫停 PR #71 的 auto-fix；C1 於 10:27 開始。
- 2026-10-09：C1 第 1 次 session 在基本 30 輪後依停止條件停下（before 無法重現參考範圍；主機不在閒置狀態：`host_late_p99` 10～47 ms、sleeper 間歇 20～75 ms、背景的瀏覽器約 50% CPU）。依規則沒有重跑。TT-1 的 L2（只記錄）已執行：方向規則兩種情境都成立。待使用者決定：在閒置的機器上以同一宣告開新的 session。PR #71 的 auto-fix 維持暫停，直到 C1 結束。
- 2026-10-09：C1 第 2 次 session（使用者核准閒置檢查；關掉 Chrome、ChatGPT／Codex）**通過**：4 ms 狀態有對照，v7 clean-30 的 Client 替代 0.010%（12／12 通過）對 v6 0.570%（9／11）；v7 clean-60 0%（12／12）；停頓重設 0。8 ms 狀態未驗證（after 沒有樣本）。TT-1 的 L2 在閒置主機再記錄一次（Waiter P99 0.10／0.13 ms）。剩下：使用者操作的 L2（拖動與縮放）與 L3，之後合併 PR #71；C1 之後要決定的：延遲位移與相位追蹤、D30 門檻的放大。
- 2026-10-09：使用者操作的 L2、L3 通過：拖動與縮放造成的 11 次 modal loop（1.1～4.0 秒）中，兩位玩家的 Match 替代 0 筆、停頓重設 0、LifeRespawn 以外的 epoch 重設 0；Spaces、縮小、遮住都沒有斷線，模擬角色沒有 ≥100 ms 的晚醒。第 05 批完成，P1b 的驗收全部完成。
- 2026-10-09：#71 依使用者指示合併（`660310e`），**P1b 完成**；worktree `../GYO-Engine-p1b-c1` 移除。
- 2026-10-09：使用者更正 D41：`--fps` 限幀與 30 FPS 手感的 L3 併進第 13 批，不另開第 05b 批與 P1c。下一步改回 P2。
- 2026-10-09：使用者補充 D41：FPS 與解析度是玩家的選擇；v7 的第 13 批用命令列選項，設定 UI 與 `settings.json` 移到 v8（連帶 IP-5 的使用者目錄與原子寫入）。第 13 批、IP-5 與 v8 的 README 依此更新。

## P2 進度（記錄器）

- 2026-10-09：使用者指示開始 P2-log（「IP-5 照你的判斷，開始 P2-log」）。批次文件 [P2-log](p2-log-network-statistics.md)：Gateway、Match、Client worker 各自每 10 秒一行統計，只加記錄、不改行為。分支 `claude/pvp-v7-p2`（從 `660310e`）。
- 2026-10-09：master 的 CI #244（#71 合併）只有 `Snapshot / object_fps_pvp` 失敗：剛建立的 snapshot tag 立刻讀回時還讀不到（GitHub 的讀取延遲），與 #71 的內容無關。依使用者指示重跑失敗的 job 後發布；發行流程的修正單獨開 PR [#72](https://github.com/yojinn-io/GYO-Engine/pull/72)（`fix(ci)`），依使用者指示合併（`247aa2b`）。
- 2026-10-09：P2-log 的實作與 L1 完成：全量 CTest 68／68（權威 digest 不變）、Go 的 vet／test／race 通過；clean-60 開發跑次確認三個程序都有統計行（結果通道約 15 Hz，是第 06 批要修的缺陷）。見[批次文件](p2-log-network-statistics.md)的「結果」。P2 的 PR 待開。
- 2026-10-09：#72 合併後，`claude/pvp-v7-p2` rebase 到 `247aa2b`，P2-log 的 commit 為 `983e091`（P2 的 before）。PR [#73](https://github.com/yojinn-io/GYO-Engine/pull/73) 已開（auto-fix）。
- 2026-10-09：使用者指示繼續，第 06 批開始（[批次文件](06-gateway-results-30hz.md)）。實作、L1、開發跑次完成；`action_probe.py` 的「結果每秒 ≤31」在 2 案超出，依停止條件停下，等使用者決定。程式留在本機，未推送。
- 2026-10-09：PR #73 的 `L1 / macos-arm64` 失敗：P2-log 的 Match host 測試以固定 200 ms 等 Tick，忙碌的 runner 只量到 2 次。改為輪詢累計（`364884f`），重跑後 CI 全綠（`61ca497`）。
- 2026-10-09：使用者選 (a)（D42）。Gateway 的統計行加上最短間隔；故障案例的確認跑次 6 次全部通過，Gateway 端的最短間隔 ≥18.0 ms。全量 CTest 68／68。第 06 批的程式為 `7da6b0d`，推上 PR #73。L2 的事前宣告寫進批次文件，待使用者核准。

## P2 以後各批的範圍

批次文件在該線開始時撰寫；行號以那時的程式為準重新核對。

### P2：網路路徑（任務 3）

- **P2-log**（單獨的子批次，先 commit，作為 P2 的 before；06～08 的修正都依賴它）：
  - Gateway 每 10 秒的統計：每個 session 的 results 與 snapshot 送出間隔分布、runtime link action batch 的間隔。
  - Match：程序 CPU 秒數、IPC 迴圈每秒迭代、Tick 的預定與實際喚醒。
  - Client worker：每 10 秒的喚醒次數與 CPU 秒數。
- **06** Gateway 結果通道：資格判定改為只在 `now + I/2 < nextSend` 時才跳過（`action_delivery.go:205`），保留寫出後的重新錨定與「不爆量」。修正後若碰到分析器「每秒 ≤31」的窗口規則，停下由使用者決定。runtime link 的 action batch 不改，只量測。
- **07** Match：Tick 改為 Waiter 的絕對期限（Advance 前的取樣時刻加 `secondsUntilNextTick`），晚醒寫進 10 秒統計；MatchRuntimeHost 在 snapshot、results、evictions、重設完成時通知 IpcHost，並計數 `snapshot_` 槽被覆蓋的次數；IpcHost 改為一條 asio io 執行緒（async accept／read／write，pump 的優先序 controls > actions > snapshot 不變），1／5／10 ms 的輪詢全部移除；連線結束的路徑明確化。執行緒改用 `GYO::Threads`。
- **08** Client：worker 改為一條 asio io 執行緒（async receive、各期限一個 steady_timer），SendInput／SubmitAction 以 post 喚醒；速率語意不變；httplib 留在 worker（只在大廳切換與關閉時阻塞 UDP）。ACK 判定在 L1 顯示語意不變時改為網路角色收到裁決時前進，否則維持並記錄理由。動作送出語意依 D36。
- **09** FireGate 與 C2：先推導常數並凍結，再跑 C2 與 25 案回歸（README「本機射擊閘的兩個常數」）。P2 頭另跑一次 30 FPS，只記錄，單獨顯示任務 3 的影響。
- 停止條件：權威 digest 改變；需要改 wire；macOS 的 Tick 晚醒沒有改善；worker_main 的斷言需要放寬；常數必須比 v6 大；乾淨跑次出現權威 Cooldown 拒絕。

### P3：診斷與 pv7（任務 8、4）

- **TT-2**（Engine）：見 Engine 計畫。寫檔執行緒用 `GYO::Threads`；只在佇列由空轉為非空、或達到批次門檻時才 Notify。
- **10** 任務 8 的紀錄：Client 的拒絕原因、未指定 `--gateway` 的提示、以 worker 收包時間戳計算的 snapshot 年齡、模擬步晚醒的 10 秒摘要；Match 的動作裁決與原因（`match-actions.jsonl`）、結束紀錄、runtime link 關閉紀錄（含原本吞掉的例外）、每位玩家每 10 秒的 Held／Neutral；Gateway 的收包間隔分布、拒絕原因、control lane 丟棄計數、週期性 IPC 寫出延遲、loopback advertise-ip 的警告。日誌格式是新的產品 Data Contract：帶版本與驗證規則，C++ 與 Go 兩端的測試解析同一份樣本。新紀錄寫到另外的檔案。
- **11** pv7：契約文件 `docs/object_fps_pvp/protocol-v7.zh-Hant.md`；ProtocolVersion 6→7（ClientVersion 與 RuntimeVersion 都由它導出）；runtime link 每秒 1 次心跳（偏移、RTT 最小值濾波、漂移視窗回歸，每 10 秒寫進兩端日誌）；Client↔Gateway 時間回聲（D35）；版本不一致時給明確的錯誤；驗收工具升 pv7。紀錄若需要 wire 上的資料，併入本批，不做第二次 wire 變更。
- 產品的 `-fexperimental-library` 在最後一個 `std::jthread`／`std::stop_token` 使用者遷移完時移除（`apps/object_fps_pvp/CMakeLists.txt:30-32`、`tests/object_fps_pvp/CMakeLists.txt:187`），並加守衛（產品、probe、產品測試中 0 件）。預計在 P3。

### P4：SDL 隔離（任務 7）

- IP-3、IP-4（Engine）：見輸入與呈現計畫。
- **12**：main、PvpApplication（移除 `SdlPlatform&`／`SdlGpuRenderDevice&` 的公開暴露）、7 個 probe 檔（gui_main、gui_quad_main、gui_input、action_short、player_short、native_window、platform_fingerprint）、產品測試（PointerCaptureCharacterizationTests、PlayerPresentationTests）改用 Engine API；SDL_Delay 改用 Waiter；SDL_Log 經 facade 寫進 LogFile。完成條件的 CTest（只在選擇本產品時啟用）以符號判定（D33），例外清單從空開始。L2 在同一場次交錯跑 P4 的 base 與頭的 GUI 短測；L3 確認大廳的文字輸入與剪貼簿。

### P5：解析度與 FPS 的選擇（任務 6）

- IP-5（Engine）：見輸入與呈現計畫。使用者目錄與 `engine/io` 的原子寫入依 D41 移到 v8；其餘項目在 P5 開始時依「只有命令列選項」重新核對（例如執行中改變大小是否仍需要）。
- **13**：玩家以產品 Client 的命令列選項選擇解析度、視窗模式與 FPS 上限（D41；例如 `--resolution 1920x1080`、`--window-mode`、`--fps <上限>`）。選項名稱、允許值與預設值在批次開始時提案確認；顯示器、像素密度、vsync 是否也做成選項，同時提案。指定的值不能用時退回預設並記一行日誌；選擇的值寫進啟動日誌。限幀的機制放在哪一層（產品，或 Engine 的 `RuntimeLoop`）在開工時依當時的程式決定，v8 的設定 UI 共用同一個機制。HUD 依解析度縮放；移除寫死的 1280×720。設定 UI 與 `settings.json` 在 v8（D41）。L1 包含選項的解析與限幀的節拍（注入時鐘）；L3 包含 30 FPS 的手感（D41；使用者實際遊玩，保留日誌與 movement trace）。

### P6：音效（任務 5）

- AU-1（Engine）：P6 開始時建立 audio 計畫夾。中立混音器（PCM16、48 kHz、自己的樣本計數、即時安全的有界 SPSC、依時間戳換算樣本位置）＋SDL 後端（`SDL_OpenAudioDeviceStream` 的 callback）。
- **14**：在事件真正發生的地方以 Engine 時間戳觸發：本機射擊（輸入事件的時間戳）、命中確認、受擊、換彈、遠端射擊。L2 以 `SDL_AUDIO_DRIVER=disk` 在 30 FPS 連射，分析起音間隔要對應射擊 Tick，而不是 33 ms 的幀格點。

### P7：整合與升格

- **15** LAN 場次（事前宣告，只記錄）：Windows Client 的模擬步晚醒（含縮小與遮住）、Windows 切換視窗時的 Held、Mac Gateway↔Windows Client 的偏移與漂移（至少 15 分鐘）、Gateway 各通道在真實網路下的分布、任務 8 的清單（只看日誌能否回答 v6 LAN 的問題）；朋友能主持時加測 Windows Match。
- **16** 整合驗收與升格：CTest 全部、權威 35／35、Match 的連結閉包不含 SDL 與 audio、產品移除檢查、25 案矩陣（clean-30 是否計入判定在開始時依 C1 決定）、短測、quad、1 GUI＋3 bot、L3 清單、STABLE_BASELINE v7；是否發行由使用者決定。

## 未結事項

- C1 之後要決定的事（C1 已在 2026-10-09 完成，現在待使用者決定）：
  - 相位追蹤的既有問題（第 03 批的 xhigh 審查；第 03a 批 clean-60 的延遲中位數約晚 1 幀，方向相同）：settling 期間收進來的舊樣本留在下一個視窗，修正後第一個視窗的 P90 實際約 P93；settling 期間累積的 late 樣本，會在 settle 完成時立刻觸發第二次 late 修正。
  - 延遲的位移（第 04 批記錄，C1 證實）：常數不變時，v7 的「產生→執行」約 38 ms（30／60 FPS 相同），v6 是 21～25 ms；「輸入取樣→執行」約多 1 幀。
  - 這兩項都要改相位常數或相位追蹤的定義，屬於停止條件，要另立批次；候選是第 09 批（FireGate 重估）。
  - D30 門檻可能被逐漸變長的發布間隔逐級放大（低機率）：使用者決定 C1 之後再看（見[第 04 批](04-client-roles.md)）。
- C1 的 8 ms 狀態未驗證（第 2 次 session 中 after 沒有落到 8 ms 狀態）：之後的量測出現 8 ms 狀態時再補。
- TT-1 L2 的量測程式在本機分支 `claude/tt1-l2-wake-record`（`a8f9535`，未推送）：為了不讓 C1 作廢，沒有放進 PR #71；之後的 PR（例如 TT-2）再合入。
- 第 04 批 xhigh 審查留下、只記錄的項目（時間倒退的意圖、網路 probe 不再涵蓋伺服器端的輸入逾時、世代檢查與送出路徑沒有 L1、負向 slew 時 FireGate 偏保守）：見[第 04 批](04-client-roles.md)的「xhigh 審查」。
- worktree：`../GYO-Engine-v6final`（`fee92ff`）保留給第 09 批 C2 的 before。
- Spaces、縮小時的斷線可能來自 App Nap（任務 1 解決不了）：第 05 批的 L3（2026-10-09）沒有重現，模擬角色也沒有 ≥100 ms 的晚醒。之後若重現，提出程序活動宣告作為新的 Architecture Delta。
- Windows Match 的 Tick 與 IPC 精度從未量過；Match 不連結 SDL，所以 SDL 調高計時器解析度的效果不適用。朋友能主持時在第 15 批量，否則標「未驗證」。
- `ClientConnection` 關閉時最多約 3 秒的阻塞（httplib），維持已知限制。
- 自旋（期限前忙等）：2026-10-09 使用者決定目前不加，現行的分離執行緒已經夠用。等第 07 批（Match Tick 改用 Waiter）與第 15 批（Windows 退回一般 waitable timer 時的精度）的數據再評估；要加就是 Architecture Delta，先量測再決定。
- 驗收分析器 `quad_evidence` 與 `command_evidence` 的收斂：維持候選。
- v6 文件中其他舊的行號（D20 的 `runtime_v5.proto`、D21 的 `IpcHost.cpp:267`、v6 交接延後項目 8 的 `backpressure_test.go:102-158` 等）：只列在[盤點](INVENTORY.md)，不修改（AGENTS §11）。
