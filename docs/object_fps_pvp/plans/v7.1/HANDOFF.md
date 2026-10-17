# PvP v7.1 交接

更新：2026-10-16。Owner：`object_fps_pvp`。**狀態：`edge-input-passthrough` 完成（L2 通過）。**
本文件是 v7.1 的記錄器：每批開始、里程碑、停止時，和工作在同一個變更中更新。人看的計畫在 [README](README.md)，盤點在 [清單核對結果](INVENTORY.md)。

## 代號對照

本文件主要給 agent 接手用。批次代號像函數名，下表是它的註釋；樹狀總覽與各批明細見 README。

| 代號 | 意思 | 出處 |
|---|---|---|
| D86～D110 | 使用者在 v7.1 的決定（接續 v7 的 D1～D85） | 本文件「決策紀錄」 |
| P1～P8、A1、A4、A5、B6～B11、C12～C17、D58-*、D-LOG、E1～E5、F1～F7、G1、G2、M0、TT2-B10、IP34-B12、LAN15、HB、N-* | 盤點的項目代號（使用者的初始清單＋新發現） | [INVENTORY](INVENTORY.md) |
| plan-sync-docs | 同步 master（PR #75～77），寫本目錄 | README「樹狀總覽」 |
| pv7-line | pv7 的 PR 線：`pv7-design`～`pv7-merge`（含 `edge-*` 六批），三角色同一個 PR，最後一次合併 | README「pv7-line 各批」 |
| pv7-design | 所有 wire 欄位與語意一次定完，產出 `protocol-v7.zh-Hant.md` 草案 | 同上 |
| D-1～D-7 | `pv7-design` 的待決事項（決定後轉成 Dxx） | [pv7-design](pv7-design.md)「等你決定」 |
| D-8～D-15 | Gateway 方向（D-8）與選 (a) 時接著要決定的事 | [gateway-direction](gateway-direction.md) |
| edge-* | 選 (a) 時的 Gateway 批次：`edge-results-resend`、`edge-input-passthrough`、`edge-actions-passthrough`、`edge-results-passthrough`、`edge-analyzers`、`edge-contract` | [gateway-direction](gateway-direction.md) |
| proto-rename-v7 | proto、Go 套件、CMake target 改名為 v7，版本值仍是 6 | 同上 |
| version-errors | P1：版本 6→7、版本不符的明確錯誤、凍結 pv7 版分析器 | 同上 |
| heartbeat-sync | P2：runtime link 心跳對時（只作診斷） | 同上 |
| time-echo | P3：Client↔Gateway 時間回聲 | 同上 |
| drop-report | P4：Gateway 丟棄回報（D49 第二階段） | 同上 |
| gateway-duties | （已由 `edge-contract` 取代，D94）P5：Gateway 狀態的職責分類與正式條文 | 同上 |
| cause-records | E2、動作拒收回覆、Leave 原因 | 同上 |
| hitbox-fields | 骨骼 hitbox 的 wire 欄位，填等同現行的值 | 同上 |
| pv7-checkpoint | 行為改變前的整體回歸 | 同上 |
| a1-shot-step | A1：射擊照命令步判定，FireGate 退役（線上唯一的行為改變） | 同上 |
| pv7-merge | 四平台 L1 後合併 | 同上 |
| after-pv7 | `tt2-trace`、`log-contract`、`dead-letter-position`、`d44-regression-pv7`、`incident-close`、`observability-tools`、`re-evaluate` | README「after-pv7 各批」 |
| after-re-evaluate | 內容不變、評估後再排的項目（SDL 隔離、code smell、Windows LAN 等） | README「after-re-evaluate」 |
| ◆ | 事故後遺症（2026-10-10 Gateway 悄悄丟命令），v7.1 結束前全部結案 | README |
| pvN／vN | 網路協議版本／遊戲版本 | v6 交接 D20 |

## 現況（每次停下來時更新）

停下來等使用者、或回報里程碑之前，先更新本節再回報。「等你決定」只列需要使用者決定的事；決定之後移到決策紀錄，並從本節刪掉。

- **等你決定**：無。
- **進行中**：PR [#78](https://github.com/yojinn-io/GYO-Engine/pull/78)（draft，pv7-line，含計畫文件）。CI 對 draft PR 會跳過 L1（`build/ci/common/ci_scope.py` 的 `reason="draft"`），所以每批推送後手動觸發 Cross-platform CI 跑四平台 L1，PR 維持 draft。`proto-rename-v7`（`d5104a0`）是 run 38771828429，`version-errors`（`046a0fd`）是 run 38784720725，都全部通過。
- **下一步**：`edge-results-passthrough`（說「開始」才啟動）。
- **臨時測試（只記錄）**：2026-10-16 的全 Windows LAN 臨時測試（`16216ff`）可以穩定遊玩；見規劃紀錄同日條目與 INVENTORY 的 F7、`N-summary-counter-wrap`、`N-hit-feedback-not-logged`、`N-windows-max-path`。
- **最後更新**：2026-10-17 11:48，heartbeat-sync 的睡眠那段完成（只記錄）。

## 閱讀入口

1. [README](README.md)：定位、樹狀總覽、各批範圍與驗收、工時、跨分支依賴、取消的項目。
2. 本文件：先看「現況」，再看決策與規劃紀錄。
3. [INVENTORY](INVENTORY.md)：盤點的每一項、證據與處置。
4. v7 的紀錄：[v7 交接](../v7/HANDOFF.md)（D1～D85）、[STABLE_BASELINE v7](../v7/STABLE_BASELINE.md)、[事故紀錄](../v7/Incident/2026-10-10-gateway-silent-drop.md)。
5. 規則：AGENTS.md §2「Derivation Order：Feature → Contract → Architecture」（PR #77）。

## 決策紀錄

| 編號 | 決定 | 理由與出處 |
|---|---|---|
| D86 | （2026-10-15）「功能 → 契約 → 架構」是有限度的參考順序。主對話提出三個限度：需要 pv7 資料才能校準的功能排在契約後；架構項目中和 wire 有關的決定提前到契約設計；A1 最先決定。使用者：「嗯，就這樣」 | 使用者：「把功能想任務往前推，架構調整放後面。否則功能項會一直被卡住」。之後由文件會話寫進 AGENTS.md §2（PR #77） |
| D87 | （2026-10-15）**v7.1 把架構和契約穩定下來，v8 只做功能和 editor。** v8 目前看得到的契約需求只有骨骼 hitbox：pv7 只做 hitbox 的契約支持，v8 才實作。其他 v8 候選不預先定契約。SDL 隔離不是移除 SDL，是讓產品不直接呼叫 SDL | 使用者：「否則 v8 有要做功能又要改契約，驗收難度大」「v8 契約目前可見的就是 hitbox，其他還都沒影子呢」 |
| D88 | （2026-10-15）契約是架構的一部分。「功能 → 契約 → 架構」是標準推導過程：架構支撐契約，契約改變時架構不一定要變。v7.1 同時處理契約與架構，是 2026-10-10 事故的後遺症；不在 v7.1 處理乾淨，v8 的功能製作很可能難產 | 使用者的說明；AGENTS.md §2 |
| D89 | （2026-10-15）v8 的三個 editor：UI Editor（已有）、GridMap Editor、Assets Editor。後兩者是通用工具，PvP 是實驗場，完成後其他遊戲可以參考；格式由 Engine 或工具層擁有，計畫放 Engine 計畫夾。GridMap 的 2D、3D 都支援，建地圖時選。對 v7.1 的影響不大，v7.1 不做內容格式的讀取端 | 使用者的回答 |
| D90 | （2026-10-15）**v7.1 是 hotfix：先檢測、再評估。** 使用者排定 pv7 與契約的順序：P1 版本升級（資訊分離）→ P2 心跳對時、P3 時間回聲（檢測功能）→ P4 丟棄回報（檢測完成後優先級最高）→ P5 Gateway 職責 → E2、動作拒收回覆與 Leave 原因 → 之後是日誌 Data Contract、P6 死信定位、hitbox 契約支持。**做法 (a)**：所有 wire 欄位一次設計，照這個順序在同一條 PR 線分批實作，最後一次合併。採用 A1，M0 隨之取消；輸入防偽造取消。其餘項目在 pv7 與契約完成後重新評估；缺工具時告訴使用者，由使用者判斷 | 使用者：「沒有正確的檢測數據，就沒法評估事故和故障。評估不了，事故再如何計算都解決不了」「這種情況下，升契約版本沒有意義」。背景見下面的「規劃紀錄」 |
| D91 | （2026-10-15）計畫的調整：A1 的手感盲測不做；proto 照慣例改名；v7 的 3 人 LAN 測試不跑（「先處理完問題」），所以 v7 不發行 v1.2.0。after-pv7 的順序照主對話的建議（`tt2-trace` → `log-contract` → `dead-letter-position` → `d44-regression-pv7` → `incident-close` → `observability-tools` → `re-evaluate`），使用者：「OK」。主對話對 pv7-line 的 4 個調整，使用者沒有提出異議：hitbox 欄位排在 A1 之前；凍結 pv7 版分析器併進 `version-errors`；A3／A5 的轉送與否在 `pv7-design` 決定；proto 改名獨立成一個 commit | 理由見 README 各批的備註 |
| D92 | （2026-10-15）核准開始 `pv7-design`，用 ultracode（3 個設計方向、1 位評審、1 次對抗式檢查），另做 1 次 xhigh 對抗式檢查 | 使用者：「開始 pv7-design，用 ultracode，另做 1 次 xhigh 對抗式檢查。開始前先說明檔位與時間估計。」；v7 D45 |
| D93 | （2026-10-15）pv7 定稿前先評估 Gateway 的方向（[設計事故 2026-10-15](Incident/2026-10-15-gateway-state-drift.md) 行動項目 1），用 ultracode（3 位評估者各負責 (a) 無狀態邊緣、(b) 遊戲 UDP 直接到 Match、(c) 維持現狀，1 位評審、1 次對抗式檢查）；之後重評 `pv7-design` 的 D-1～D-7 與 pv7-line | 使用者經分支會話：「轉給原規劃會話後，重新評估一次計劃。」；使用者：「用 ultracode 評估 Gateway 的三個方向（3 位評估者、1 位評審、1 次對抗式檢查），之後重評 D-1～D-7 與 pv7-line。」 |
| D94 | （2026-10-16）**Gateway 回到無狀態邊緣**（[gateway-direction](gateway-direction.md) D-8 選 (a)）：Gateway 只翻譯、逐則檢查、原樣轉送；內部訊息保留身分並冪等，去重、帳本與結果重送由 Match 負責；Gateway 只留連線與 session 的軟狀態。順序守住 D90（丟棄回報排在 Gateway 改動之前）。接受 pv7-line 約 170～179 h〔推論〕，約為 README 原估 91 h 的 1.9 倍。這是設計前提的改變（[設計事故 2026-10-15](Incident/2026-10-15-gateway-state-drift.md)），依 stable-version-first 在 v7.1 處理：v7 是穩定基準 | 使用者：「D-8 選 (a) 無狀態邊緣，順序守住 D90，接受 pv7-line 約 170～179 h。」 |
| D95 | （2026-10-16）`pv7-design` 與 Gateway 方向的其餘決定全部照建議：D-1 不再需要；D-2～D-6 照建議；D-7 確認排除清單（第 2 項改為「逐命令的完整丟棄身分」）；D-9 順序守住 D90，edge 批「結果先、動作後」，丟棄回報先照現行出口實作再縮減；D-10 IPC 依到達順序、controls 不丟、資料每位玩家有界 FIFO（丟最舊並回報）、結果 30 Hz 由 Gateway 只記上次送出時刻來守；D-11 解碼成功就提交活性；D-12 移除 snapshot 守衛 D1、D2；D-13 丟棄回報上 wire（約 29 個原因），逐出時佇列中的資料轉給 Match；D-14 Gateway 約束條文與白名單守衛；D-15 保留兩份 proto 加 adapter。檔位：`pv7-design` 的 IPC 送達語意做 1 次 xhigh 對抗式檢查、`edge-input-passthrough` 局部 xhigh，都同意。設計事故的行動 4（AGENTS.md）交給文件會話 | 使用者：「D-9～D-15、重評後的 D-1～D-7 全部照建議；pv7-design 做 1 次 xhigh 檢查、edge-input-passthrough 局部 xhigh 都同意；事故行動 4 交給文件會話。修訂草案與 README 後 commit 並推送。」 |
| D96 | （2026-10-16）**protocol-v7 定稿**（§2a 的 xhigh 檢查已套用：結果的 30 Hz 改為 Gateway 單槽持有、Match 最小 id 優先；中間 commit 轉送 ACK-only 批；結果重送不刷新期限；唯一 UDP 寫出者；給已移除玩家的結果不算損失；鍵對應與事前宣告的例外；FIFO 上限準則不少於 64 則窗口；守衛加強）。`PlayerInput`／`ActionBatch` 加 Client 的 GYOP 序號作為診斷欄位（runtime 7、5）。接受 pv7-line 約 177～191 h（加上序號欄位約 178～193 h）。定稿後改 wire 要先徵求同意 | 使用者：「草案定稿；PlayerInput／ActionBatch 加 Client 的 GYOP 序號作為診斷欄位；接受 pv7-line 約 177～191 h。更新文件後 commit 並推送。」 |
| D97 | （2026-10-16）開始 `proto-rename-v7`；規劃文件併進 pv7-line 的 PR，pv7-line 在 `claude/pvp-v71-plan` 接著做 | 使用者：「開始 proto-rename-v7。規劃文件併進 pv7-line 的 PR，在這個分支接著做。」；fewer-prs |
| D98 | （2026-10-16）開 pv7-line 的 draft PR #78（標題與內容用日語），開 Auto-fix；確認 `proto-rename-v7` 的四平台 L1 後開始 `version-errors` | 使用者：「開 pv7-line 的 draft PR（標題與內容用日語），讓四平台 CI 跑 L1。」「為 PR #78 開 Auto-fix。」「CI 可以了」「開始 version-errors。」 |
| D99 | （2026-10-16）protocol-v7 §1 的兩處條文照建議修改：「版本不符時的告知」加上實作範圍的附註（只有 Gateway 在讀 Ready 時送告知；Ready 之後的不符直接關閉、不送告知）；Gateway 拒絕 join 的 log 行加上 `remote=<addr>`，和既有的 `join refused remote=<addr> code=<code>` 同一系。wire 與實際行為不變 | 使用者：「契約兩處都照建議：§1 加上版本告知範圍的附註；拒絕 log 行加上 remote=<addr>。」；`version-errors` 的 ultracode 審查 architecture-F3、contract-F5 |
| D100 | （2026-10-16）開始 `heartbeat-sync`（檔位 high，時鐘域局部 xhigh，估 9 h） | 使用者：「開始 heartbeat-sync。」 |
| D101 | （2026-10-16）會話的 ultracode 開關不覆蓋計畫的檔位：每批照 README 樹狀總覽標的檔位做；標 ultracode 的批才用，標「ultracode（先問）」的到了先問一次；其餘不逐批確認，也不因為開關另開 workflow | 使用者（經分叉會話）：「一開始計劃裡有檔位規劃，以那個為準，計劃太多了，我沒法每一個都確認。」起因：`version-errors` 在開關打開後，事前沒問就跑了審查 workflow |
| D102 | （2026-10-16）開始 `time-echo`（檔位 high，估 4 h）；`heartbeat-sync` 的睡眠那段 L2 等之後的 L2 一起做 | 使用者：「開始 time-echo。睡眠那段 L2 等之後的 L2 一起做。」 |
| D103 | （2026-10-16）開始 `drop-report`（檔位 high，列舉與上限局部 xhigh，估 13～16 h） | 使用者：「開始 drop-report。」 |
| D104 | （2026-10-16）protocol-v7 §2「診斷訊框的計時與佇列」改成配合實作：GatewayDropReport 不經過單槽，計時器只標記一輪，writer 寫出前一刻才組回報（序號、樣本、dwell 終點在寫出時決定）。wire 與 Match 的檢查不變 | 使用者：「1a」；`drop-report` 的 xhigh should-3 |
| D105 | （2026-10-16）核准 `heartbeat-sync`、`time-echo`、`drop-report` 新增的 Gateway 欄位為診斷狀態（D-14 的 Architecture Delta）。條件：`edge-contract` 的白名單分三類——診斷狀態（判定路徑不得讀取）、單次呼叫的脈絡（`datagram`，不得跨資料包保留）、測試縫隙（正式路徑只用預設值）；分類先寫進 protocol-v7 §2a | 使用者：「2a（含條件）」；`drop-report` 的 xhigh should-4 |
| D106 | （2026-10-16）開始 `edge-results-resend`（檔位 high，通道計時局部 xhigh，估 7～8 h） | 使用者：「開始 edge-results-resend」 |
| D107 | （2026-10-16）protocol-v7 §9 的 `edge-results-resend` 驗收改成「Client 收到的內容不變；新裁決最多晚 1 個間隔」，並把這個延遲列進 `edge-results-passthrough` 事前宣告的 L2 對照。量測：無損模型 8 個種子，369 個裁決中 63 個晚到，最多晚 33 ms；未確認超過 8 筆時新 id 要等循環槽位（推論最晚約 7 個間隔），條文以括號註明 | 使用者：「§9 照 (a)」；`edge-results-resend` 的 xhigh should-1 |
| D108 | （2026-10-16）開始 `edge-input-passthrough`（檔位 high，局部 xhigh，估 20～26 h）。行為改變：L2 的事前宣告（門檻、試行次數、重試規則）在看結果之前經使用者核准（AGENTS.md §14.1），宣告前做門檻來源的對抗式檢查（D45） | 使用者：「開始」 |
| D109 | （2026-10-16）核准 `edge-input-passthrough` 的 L2 事前宣告第 3 版；第 10 節照建議：J4a 1.5 倍、跑 soak-long、heartbeat-sync 的睡眠那段在本 session 結束後接著做、次數照第 5 節（98 個項目）。確認 `ingress_cross_check.py` 只接受 protocol-v7 §9 宣告形狀的 I3 失敗（AGENTS.md §14.4） | 使用者：「核准 edge-input-passthrough 的 L2 事前宣告第 3 版，第 10 節照建議…ingress_cross_check 的 I3 例外也確認。」 |
| D110 | （2026-10-17）L2 第 1 次 session 在第 58 項停下（B 樹 clean-60 的 I1 undetermined：關閉時被 reset，已知的競態）。依 §14.1 回報後選 (a)：只改 I1 的規則（任何情境，只有 `eof_before_final` 不成立、其他前提成立的 undetermined 只記錄），整個 session 重跑；第 1 次保留為證據 | 使用者：「照 (a)」 |

## 規劃紀錄（記錄器）

- 2026-10-15：盤點 workflow（5 位讀者、2 位對抗式核對、1 位完整性評審，約 34 分）。結果整理成 [INVENTORY](INVENTORY.md)，對抗式核對的更正已套用。
- 2026-10-15：第一次分批規劃。中途 Claude Code 程序結束，接續時又碰到用量上限；恢復後，兩份方案草稿把 v7.1 估到 359～398 h（43～65 批）。使用者：「400 小時是一個很恐怖的工作量」，規劃停止，草稿不採用。
  - 原因（主對話的檢討）：把「盤點清單全部結案」當成目標；把「不留坑」解讀成每一項都必做；把 v8 的契約支持做成內容讀取端；又拿 v7 的規模當錨點。
  - 原因（使用者）：沒有檢測數據的死循環。在量不到的問題上直接排修正，每一項都得自帶量測與宣告。
  - 之後改由主對話列出「功能項＋理由＋簡介」，使用者自己排順序（D90）。
- 2026-10-15：照使用者的順序整理計畫（D91）。工時沿用草稿各批的估計：pv7-line 約 91 h、after-pv7 約 52～60 h、after-re-evaluate 約 95～105 h。
- 2026-10-15：`plan-sync-docs`。合併 origin/master（`8f08f2c`，PR #75～77）；只有 v7 HANDOFF 的「現況」衝突，保留 master 的寫法並補上 v7 結束的紀錄。寫本目錄的 README、HANDOFF、INVENTORY；更正 v8 README 中 hitbox 與 FireGate 的過時備註。合併與文件分成兩個 commit 推送。

- 2026-10-15：`pv7-design`（D92）。ultracode：3 個設計方向（最小改動 7 分、語意分離 7.5 分、以可診斷為主 6 分），評審合成；對抗式檢查（必須修正 3、應修正 5）與 xhigh 檢查（必須修正 2、應修正 7）的結果都已套用或轉成 D-1～D-7。產出 [protocol-v7 草案](../../protocol-v7.zh-Hant.md) 與 [pv7-design](pv7-design.md)。約 84 分的 workflow 加上草案撰寫。尚未 commit。

- 2026-10-15～16：Gateway 方向的評估（D93）。分支會話轉來設計事故（Gateway 由無狀態漂移成持有 Match 決策狀態）與你的要求；事故紀錄補上「跨 agent 漂移」與「AGENTS.md 的重要性」。ultracode：3 位評估者（(a) 153 h、(b) 171 h、(c) 114 h）、評審（建議 (a)，7.5 分；(b) 5 分；(c) 5.5 分）、對抗式檢查（必須修正 2：結果 30 Hz 的執行點、順序要守 D90）。主對話預查 v7 L2 語料：有狀態的出口不是 0（拒絕線溢出丟棄 1488、A3 丟棄 42、舊序號 Actions 436；Gateway 擋掉的重複命令約 201 萬）。產出 [gateway-direction](gateway-direction.md)。尚未 commit。

- 2026-10-16：D94（Gateway 回到無狀態邊緣）、D95（其餘決定照建議）。protocol-v7 草案新增 §2a「IPC 送達語意」，丟棄原因縮為 29 個（D94 之前的過渡原因只存在於中間 commit），dwell 改為一對一；README 的 pv7-line 改為 17 批、約 170～179 h；INVENTORY、pv7-design、gateway-direction、設計事故紀錄同步更新；事故行動 4 轉給文件會話。與本紀錄同一個 commit 推送。

- 2026-10-16：§2a「IPC 送達語意」的 xhigh 對抗式檢查（D95，1 位 xhigh 審查者，約 22 分）：沒有推翻 D94；必須修正 2（結果 30 Hz 改為 Gateway 單槽持有加最小 id 優先；中間 commit 的 ACK-only 轉送）、應修正 6、附註 5，都已套用到草案與 pv7-design 的測試清單；工時加 7～12 h。尚未 commit。

- 2026-10-16：`pv7-design` 完成（D96）。protocol-v7 定稿，加 Client 的 GYOP 序號欄位；README 的 pv7-design 標為完成，pv7-line 約 178～193 h。與本紀錄同一個 commit 推送。

- 2026-10-16：`proto-rename-v7` 完成（D97）。`client_v6`／`runtime_v6` 改名為 `client_v7`／`runtime_v7`（proto 檔名、package `object_fps_pvp.{client,runtime}.v7`、Go 套件 `clientv7`／`runtimev7`、CMake target `gyo_<app>_{client,runtime}_v7`、C++ 的 include 與 namespace、`protocol/README.md`），版本值仍是 6。Go 綁定用專案建置中的 protoc 36.2.0 與從本機模組快取離線建置的 protoc-gen-go v1.36.11 重產；先確認重產 v6 與提交版逐位元一致，新舊的差只有 v6→v7 的名稱。驗收（L1，本機）：`go vet`、`go test ./...` 通過；`test` preset 全量 CTest 70／70 通過；權威 digest 同機兩樹比較（base＝v7final `6d3f287`）scale 1、3 各 35 情境 0 差異（`build/target/_build/test/logs/pvp-v71-proto-rename-20261016/`）。四平台 CI 等 PR。Architecture Delta：Build Graph 的 target 名稱（只改名，依賴關係不變）。實際約 1 h（估 3 h）。

- 2026-10-16：開 draft PR [#78](https://github.com/yojinn-io/GYO-Engine/pull/78)（pv7-line，含計畫文件，D97）。CI 對 draft PR 跳過 L1，改以手動觸發 Cross-platform CI 跑四平台 L1（`d5104a0`：run 38771828429）。 四平台 L1、Quick acceptance、CI gate 全部通過，`proto-rename-v7` 驗收完成。

- 2026-10-16 02:01：`version-errors` 開始（D98，檔位 high，估 12 h）。

- 2026-10-16：`version-errors` 實作與驗收。
  - 版本值 6→7：Client、Gateway、Match、驗收工具四處一起改，`test_protocol_version.py` 核對四者一致。
  - HTTP：Gateway 的拒絕 body 帶自己的版本，並寫一行 log（期望值、收到值）。Client 依 body 的 `error` 判斷，產生 `protocol_version_mismatch: Client 7, Gateway <n|unknown>`；join 成功回覆的版本不符也用同一個錯誤。
  - IPC：兩端把版本不符、解析失敗、訊息為空、未知欄位（寫出欄位號）分開。Gateway 讀到其他版本的 Ready 時，關閉前送出只含版本的 envelope；Match 對「Connected 之後沒收到任何框架就 eof」寫一行提示。告知只在 Ready 時送：Match 先送 Ready，實際會偵測到不符的只有這裡；之後再送會讓 link 出現第二個寫入者。這比契約原文窄，D99 核准後在契約加上附註。
  - Client：其他版本的 GYOP 照舊靜默丟棄，另外計數，寫進 worker 統計行的 `other_version_datagrams`。
  - 測試：Go 新增 3 個；IPC 新增 2 個（各種協議違反的關閉原因、eof 提示）；Wire 的位元組黃金值改為 `0x0007`，並測 `OtherVersion`；worker 的模擬 Gateway 測 4 種拒絕（上一版、下一版、沒寫版本、`room_full`）。突變新增 7 個（`v71-version-errors`），`09-version-python` 改 find 字串。
  - 建置中途的修正：IpcHostTests 的 `pb` 別名在 doctest 的測試函式（全域範圍）與 protobuf 的 `::pb` 衝突，改用匿名命名空間裡的 `Envelope`。
  - 驗收（L1，本機）：`test` preset 全量 CTest 70／70（含權威 golden 30／30）；突變 7／7 與 `09-version-python` 都 killed；權威 digest 同機兩樹比較（base＝v7final `6d3f287`）scale 1、3 各 35 情境 0 差異。
  - 凍結 pv7 版分析器：`frozen-pv7.sha256`（54 檔，清單的 sha256 `b4a8963001883160d5bb7642b7da89afd343c686d29752874461762e17a291c2`，審查修正後重產）。和 v7final 相比只改了 `acceptance_util.py`（版本常數）與 `run_quad.py`（mixed）。
  - 凍結前的 pv6 語料核對（16-L2 的 43 項工作：遊戲 25、backpressure 6、soak 1、quad 10、GUI 1；每項分析複本，不寫入語料）：
    - 舊分析器 43／43 重現 session 存下的結果（確認輸入重建正確）。
    - 新舊的差只在事前列出的版本比對項：`gameplay_evidence.py:140-141`、`gameplay_soak_evidence.py:178-181`、`combat_gui_evidence.py:155`，以及輸出的 `protocol` 欄位。GUI 在版本檢查就提前返回，所以另做一輪：把 pv6 報告的版本欄位改成 7 再比，只剩 `protocol` 欄位本身不同，GUI 的其餘證據也逐值一致。
  - `run_quad` 的 mixed 改寫成 pv7 版（protocol-v7 §1「驗收工具」）：older-client 只接受版本錯誤；older-match 要求 pv7 Gateway 寫出期望值與收到值；older-gateway 要求 pv7 Match 寫出 eof 提示。`older-arena` 退役：它驗的是 v6 的容量變更，pv7 不改 arena（v7final 的 arena 逐位元相同），容量拒絕由 `ArenaDigestTests.cpp:133-134` 與突變覆蓋。對 v7final 的 pv6 執行檔跑一次，3／3 通過；改寫前那一次的 older-arena 失敗保留在 `short/mixed/`。
  - 短跑次（只記錄；pv7 工具與執行檔，沒有閒置閘門）：clean-60、network、backpressure 6 情境、soak-short、quad clean-60 一輪，全部通過；network 的 `other_version_datagrams` 都是 0。有 11 個數值落在 16-L2 的範圍外，最大的是 quad 一輪有 8 個替代命令、actual P95 42 ms。之後 v7final 與 pv7 交錯各跑 4 輪 quad clean-60：兩邊都偶爾出現替代命令（v7final 一輪 4 個、pv7 一輪 11 個），其餘輪次都是 0、P95 37.5～38.0 ms。判斷為主機擾動，不是回歸。
  - Gateway 執行檔 `gyo_object_fps_pvp-gateway` 是不在 ALL 裡的自訂 target，跑驗收前要單獨建（`cmake --build --preset test --target gyo_object_fps_pvp-gateway`）；這次建之前，它還是 10-11 的舊版。
  - 證據：`build/target/_build/test/logs/pvp-v71-version-errors-20261016/`（`digest-scale{1,3}`、`frozen-pv7.sha256`、`pv6-corpus/`、`short/`、`short/quad-ab/`）。
  - ultracode 審查（3 位審查者：契約一致性、測試與驗收工具、架構與跨平台；1 位對抗式檢查；沿用主對話檔位；約 25 分）：19 項發現，對抗式檢查確認 17 項（有重複）、推翻 2 項。在契約範圍內的都已修正：
    - HTTP join 先讀版本，再做嚴格解碼與 request_id 檢查：之後版本的 Client 多帶一個鍵，仍然得到 409 與 Gateway 的版本（原本會是 400 `invalid_join`）。
    - Gateway 讀 IPC envelope 時先單獨讀欄位 1：另一版的內容在本版 schema 下解析失敗時，仍報版本不符，Ready 時也照樣送告知。Match 端不改：Ready 之後它會收到的另一版框架只有只含欄位 1 的告知，一定能解析。
    - Client：join 成功回覆缺少 `protocol_version` 時也是 `protocol_version_mismatch`（原本是 JSON 例外）。
    - Match 的 eof 提示：閒置的同版 Gateway 停下時也會觸發，措辭改為「it was idle or speaks another protocol version」（前綴不變）。`heartbeat-sync` 的心跳之後會讓這種情況變少。
    - 補測試：join 的版本優先與 log 行；400 的版本拒絕；不是無號整數的版本（null、-1、"8"、7.5）；join 回覆缺版本；只含版本的告知（版本 1～6 與 8）；另一版加上本版會拒絕或無法解析的內容；eof 之後沒有提示的反面案例；arena 出生點不足時 `IpcHost` 拒絕建構。IPC 違規測試改成只看該次連線之後的 log；eof 提示的測試改成正常關閉（shutdown 後讀到 Match 關閉），避免連線被重設造成的偶發失敗。
    - 突變再加 7 個（這批共 14 個）：join 先判內容再判版本、envelope 先解析再讀版本、Client 只認 409、Client 接受任何數字、Match 先判空訊息、提示不看框架數、Match 接受出生點不足的 arena。`v71ve-envelope-version-unnamed` 改 find 字串。
    - `run_quad` 的 mixed：older-match 加上端到端確認（pv6 Match 以自己的原因 `frame parse or protocol version` 關閉，表示收到了告知）；Match 記錄連線重設（`read:`）時標為無法判定，最多跑 3 次並保留每次的紀錄；docstring 改指 IpcHostTests 的 arena 測試（審查指出 `ArenaDigestTests` 只測判斷函式，沒測到 `IpcHost` 的呼叫）。
    - 推翻的 2 項：§1 附註的措辭（屬措辭偏好）；v6 驗收文件裡 mixed 的說明（v6 的歷史紀錄，不改）。
    - 動到已定稿契約文字的 2 項先保留契約原文、轉成使用者決定；D99 照建議修改（另一個 commit）。
  - 修正後重驗：`test` preset 全量 CTest 70／70；`go vet`、`go test ./...` 通過（新的 Go 測試另以 `-race -count=3` 跑過）；突變 14／14 與 `09-version-python` 都 killed（`v71ve-ipc-version-as-parse` 第一次是 wrong_failure：測試改用 `mismatch(version)` 組字串後，doctest 只印出運算式，改 expect 後重跑 killed）；權威 digest 兩樹比較 scale 1、3 各 35 情境 0 差異；mixed 對 v7final 的 pv6 執行檔 3／3 通過，都在第一次判定。分析器只多改了 `run_quad.py`（runner，不在 pv6 語料核對的範圍內），核對結果仍然有效。證據 `after-review/`。
  - Architecture Delta：無。wire 不變；契約文字兩處依 D99 修改（Data Contract 的條文，非 wire）。新的依賴只有產品內部的：`IpcHost.cpp` 用 protobuf 的 `UnknownFieldSet` 列出未知欄位，Gateway 用 `protowire` 寫版本告知；Engine 與共通層沒有改。
  - 實際約 1.3 h（02:01～03:20，含審查 25 分；估 12 h）。
  - 四平台 L1（手動觸發，`046a0fd`）：run 38784720725，L1 四平台、Quick acceptance、CI gate 全部通過。

- 2026-10-16：D99，protocol-v7 §1 的兩處條文照建議修改（`16216ff`）。

- 2026-10-16 10:22：`heartbeat-sync` 開始（D100，檔位 high，時鐘域局部 xhigh，估 9 h）。一開始依會話的 ultracode 開關啟動了設計 workflow，D101 之後停止（沒有 agent 完成），改照計畫的檔位：high 實作，時鐘域局部 xhigh。

- 2026-10-16：`heartbeat-sync` 實作與驗收。
  - wire：`runtime_v7.proto` 加 `ClockSample`（欄位 1～3）與 `RuntimeEnvelope.clock`（oneof 20）；Go 綁定照上一批的方式重產。`client_v7` 的 ClockSample 留到 `time-echo` 再加（這批用不到）。
  - 估計器：Match 的 `ClockSync`（`ClockSync.hpp/.cpp`，放在 ipc 函式庫、不依賴 protobuf，`time-echo` 時 Client 也能用）與 Gateway 的 `clock_sync.go` 是同一套演算法，時間都由呼叫者傳入：
    - transmit＝max(now, 上一則＋1)；比對自己最近 8 個 transmit；四時間戳公式；負 delay 丟棄並計數。
    - 語意違規計數後忽略（沒有否決權）；時間 ≥2^53 µs 視為不合法，int64 運算因此精確。
    - 視窗不用固定格線：滿 10 秒（或連線結束）時結算，offset 取 delay 最小的樣本。
    - 跳變：兩個視窗的 offset 差，超過 max(1 ms, 100 ppm × 兩樣本的間隔)，再加上兩邊 delay 的一半（各自的不確定度）。
    - 漂移：跳變之後最多 30 個視窗的最小平方回歸（ppm）。
    - log 行族兩端同格式：`clock sync window_ms=… samples=… missing=… invalid=… negative_delay=… replaced=… offset_us=… delay_min_us=… delay_max_us=… drift_ppm=… jump=… final=…`（日誌格式在 `log-contract` 之前暫定）。
  - Match（IpcHost）：
    - 心跳回覆放在 session 的單槽（新蓋舊，計入 replaced），寫出排在 controls、action lane、snapshot 之後，transmit 在選中框架時才蓋；ScheduleWrite 也看這個槽。
    - 每次 Pump 檢查視窗；連線關閉時寫最後一個視窗（`final=1`）。
    - 心跳本身也是框架，所以送過心跳的同版 Gateway 停下時，不再觸發 eof 提示。
  - Gateway：
    - Server 擁有的 1 Hz 診斷 goroutine（`diagnostics.go`）組心跳，放進 link 的單槽診斷佇列（新蓋舊並計數）。
    - writer 只多一條獨立喚醒的分支：寫出這個槽，不呼叫 `take()`、不動 `l.wake` 與動作計時器。
    - reader 的活性改成以剩餘時間呼叫 Read，只由 Snapshot、JoinResult、Evicted、Error 刷新（ActionResults 與時鐘回覆不刷新）；剩餘 ≤0 時不呼叫 Read，直接以 `runtime liveness: i/o timeout` 失效。活性從 `run()` 起算。
    - `Server.Close` 時同步寫最後一個視窗（和診斷 goroutine 共用一次）。
  - D32：
    - 原始碼守衛：`test_diagnostic_isolation.py`（CTest `object_fps_pvp.diagnostic_isolation`）確認契約列出的決策檔案不引用時鐘符號、估計器只在 I/O 層；Go 的 `TestClockStaysOutOfDecisions` 確認 session 套件與 `server.go` 不引用。
    - 差分測試：IpcHost＋host（假時鐘，由測試推進 tick；每步以 Leave(99) 的 log 行當柵欄）同一情境跑兩次，一次沒有心跳，一次有極端值的心跳（相隔 10 s、重複、比對不到的 origin、receive 晚於 transmit、跳變）。決策（含由參考年齡判出的 EXPIRED）、join 結果與最後一個 snapshot 逐位元相同；有心跳那次 Match 確實量到樣本。
  - 測試：
    - Go：估計器 7 個；活性表格（4 種刷新、2 種不刷新，假時鐘）；實時預算；單槽；寫出分支 3 個（不取資料 pass、不延後計時器、不吃掉資料喚醒）；往返量測；golden bytes；守衛。
    - C++：估計器 7 個；IPC 4 個（回覆與量測、burst 的「回覆數＋replaced＝送出數」、差分、golden bytes）。
    - 突變 15 個（`v71-heartbeat-sync`）。
  - 時鐘域局部 xhigh 檢查（1 位，約 30 分）：沒有 must 級缺陷；活性、寫出分支、鎖順序、時鐘域、D32 隔離都核對為正確。should 4、note 10 都是測試面的缺口：
    - 已補：寫出分支的計時器與資料喚醒、回覆計數、差分測試量到樣本並正常關閉、golden bytes、活性表格、跳變的兩項容許、驗證順序、Gateway 結束時的最後一個視窗、實時測試的預算放寬到 1 s。
    - 審查中發現、已修：reader 的活性原本從 goroutine 啟動時起算，改成從 `run()` 起算（測試推進假時鐘時暴露）。
    - 未做，記在未結事項：見下方。
  - 驗收（L1，本機）：`test` preset 全量 CTest 71／71；`go vet`、`go test -race -count=3` 通過；權威 digest 同機兩樹比較（base＝v7final `6d3f287`）scale 1、3 各 35 情境 0 差異；突變選出 149 個（這批 15、`v71-version-errors` 14，以及目標檔案在這批被改過的既有突變 120），全部 killed。過程中有兩個先是逾時、判讀失敗，修測試後重跑 killed：
    - `v7-07-ipc-no-publish-wake`：Match 完全不回應時，IPC 測試逐一等待逾時，超過 CTest 上限。修法：違規測試開頭先用 REQUIRE 做一次檢查；ipc 測試的 TIMEOUT 由 60 秒放寬到 120 秒。
    - `v71hb-heartbeat-wakes-data`：Go 測試等心跳寫出時沒有截止時間，加上了。
    - 改過 IPC 測試之後，21 個 ipc 突變重跑一次，全部 killed。
  - 同機長跑次（只記錄，soak-long 113 個循環約 30 分，pv7 工具與執行檔）：兩端各約 180 個時鐘視窗、約 1810 個樣本。缺樣本、跳變、不合法、負 delay、被替換都是 0；offset 在 30 分鐘內只飄 32～87 µs；delay 最小值的中位數約 0.1 ms；漂移約 0 ppm。soak 本身的判定兩次都沒有通過，都是主機干擾：
    - 第 1 次（`long/`，10:56～11:29）：Client 的長幀與 gap 發生在第 1246、1399、1599 秒，同一段時間 xhigh 審查者正在反覆跑整套 `go test -race`。
    - 第 2 次（`long2/`，12:54～13:27）：唯一的異常（Match 的 runtime gap、兩位玩家的 Client gap、111 ms 長幀）全都在第 1206.6 秒，也就是 13:14:18。系統電源紀錄顯示 13:12～13:15 用電池（使用者移動機器）。其餘 Client gap 都是正常的復活（序號 3）。
    - 判斷：心跳不碰 Client 與模擬，時鐘數據全程正常，所以不算這批的回歸；soak 的正式判定留到之後的 L2。
    - 睡眠那段（中途讓系統睡眠，量跳變）要你在場，等之後的 L2 一起做（D102）。
  - 證據：`build/target/_build/test/logs/pvp-v71-heartbeat-sync-20261016/`。
  - Architecture Delta：無。新的依賴只在產品內：ipc 函式庫多一個原始檔 `ClockSync.cpp`；Gateway 多 `clock_sync.go`、`diagnostics.go`。Engine 與共通層（`services/gyo_gateway`）沒有改。wire 新增 `ClockSample`（契約 §3 已定）。
  - 實際約 3.1 h（10:22～13:30，含 xhigh 檢查約 30 分、突變約 1 h、soak 兩次各 30 分；估 9 h）。

- 2026-10-16：全 Windows LAN 臨時測試（使用者要求，不是批次；只記錄）。產物：Package trial run 38833224220（`16216ff`，即 `version-errors` 完成時，`heartbeat-sync` 之前）。構成：Main Client（i7-8750H、GTX 1060）同時跑 Match、Gateway 與 Client，Client A（Core 7 240H、RTX 5050）、Client B（i7-12700H、RTX 4060）連線；三台都是 Wi-Fi、JST，連線都用 192.168.50.x。三人同時在場約 4 分 47 秒（10:55:53～11:00:40），Gateway 從 10:49:11 運作到 11:01:13。方案 A 健康報告（檔位 high，約 1.5 h）：
  - 結論：可以穩定遊玩。斷線、逐出、版本錯誤都是 0，Gateway 的 `drop_*` 與限流也都是 0。
  - 替代命令：p1（A）233／42129 晚到（0.55%），p3（B）64／17154（0.37%），p2（Main）0。九成集中在 4 波（A：10:52:17、10:52:47、10:55:17～28；B：10:57:38～48），和 Client 端的 Snapshot 年齡尖峰（A 最大 124.2 ms）、`connection quality failed_windows=1 of 3` 同時出現。同一窗口 Gateway 收到的封包數不變（每秒約 61），重複命令與寫出時已解決的命令暴增，代表是延遲後一次擠過來，不是遺失。〔推論〕來源是各 Client 那一段 Wi-Fi：10:57 那一波兩人同時在場，只有 B 受影響；Main 用自己的 LAN IP 連線，封包不經過無線電。
  - Windows Match 的 Tick（F7 的第一份實機數據）：主機上沒開 Client 時（10:49:11～10:51:50）晚醒 ≥2 ms 佔 1.67%（9632 Tick 中 161 個，最大 14.7 ms），開著時（10:51:50～11:01:13）0（33707 Tick，最大 1.6 ms）。〔推論〕本機的 GPU 程式改變了計時器解析度或省電狀態；樣本只有 16 個窗口。
  - Client A 在 11:00:51～11:01:03 只有 13～43 FPS（最長幀約 90 ms），Snapshot 年齡正常，是本機畫面的問題，原因不明。
  - 新發現記到 INVENTORY：`N-summary-counter-wrap`、`N-hit-feedback-not-logged`（使用者回報 Main 有一次受擊沒閃紅，時間不記得，日誌無法定位）、`N-windows-max-path`；F7 加上這次的數據。
  - 端到端延遲和凍結分析器的 trace 分析（方案 B）沒有做。
  - 證據：`build/target/_build/test/logs/pvp-v71-windows-lan-20261016/`（`logs/`、`Info.txt`、分析腳本 `scripts/`；10/08 的舊日誌已排除）。

- 2026-10-16 13:50：`time-echo` 開始（D102，檔位 high，估 4 h）。`heartbeat-sync`（`4a66b58`）的四平台 L1 是 run 38850836007。

- 2026-10-16：`time-echo` 實作與驗收。
  - wire：`client_v7.proto` 加 `ClockSample`（和 runtime 同號同型）、`Hello.clock`（2）、`Welcome.clock`（11）；Go 綁定照前兩批的方式重產。不新增封包與 GYOP 種類。
  - 估計器：C++ 的 `ClockSync` 改成只有標頭檔（`ClockSync.cpp` 刪除），讓 ipc 與 client_network 兩個函式庫共用，不新增 build target。三個突變的檔案路徑跟著改，find 字串不變。
  - Gateway（`echo.go`，時鐘程式不進 `server.go`）：
    - 每個 session 一份估計器；Hello 通過認證後才讀它的 clock，不合法就計數、忽略，Hello 的 admission、活性、Join、Welcome 照常。
    - Welcome 回聲最新的已認證 Hello，transmit 在編碼時蓋。
    - 每次 Hello 檢查 10 秒視窗，寫 `client clock sync player=… window_ms=…`；session 移除時寫最後一個視窗。runtime 失效時直接清空玩家，不寫最後一個視窗。
  - Client（`ClientConnection`）：每次建立 UDP 連線時重設估計器；Welcome 一解析出來就量測，排在 `CheckArena` 與規則檢查之前，不影響 welcomed；Hello 帶回聲並蓋 transmit；每次網路處理檢查 10 秒視窗，寫 `clock sync` 行；關閉連線時寫最後一個視窗。
  - `ReadyFitsWelcome` 把最大的 ClockSample（三個欄位都是最大值）算進最壞情況的 Welcome；既有的邊界測試改成含 clock 時剛好 1200 bytes。
  - D32 守衛：估計器的使用者改為 `ClientConnection.cpp`、`ClockSync.hpp`、`IpcHost.cpp`。
  - 測試：
    - Go：Welcome 回聲最新的已認證 Hello 並量到樣本；回聲不合法或沒有回聲時一切照常；未認證的 Hello 不被量測；離開時寫最後一個視窗；最壞情況 Welcome 含 clock。
    - C++（worker 的模擬 Gateway）：第一個 Welcome 的回聲不合法時 Client 照樣進入 Playing；每個 Hello 回聲最新的 Welcome、transmit 遞增；第一個 Hello 沒有回聲。
    - 突變 5 個（`v71-time-echo`）。
  - 驗收（L1，本機）：`test` preset 全量 CTest 71／71；`go vet`、`go test -race` 通過；權威 digest 同機兩樹比較 scale 1、3 各 35 情境 0 差異；突變選出 80 個（這批 5 個，以及目標檔案在這批被改過的既有突變 75 個），全部 killed。
  - 短跑次（只記錄，pv7 工具與執行檔）：clean-60、network、soak-short、quad clean-60 一輪全部通過，凍結的分析器能接受新的 log 行。回聲的數據互相吻合：
    - Gateway 量到的 offset（C−G）和 Client 量到的（G−C）正負相反、大小相同，差在幾十 µs 以內。
    - 同一台機器上 Match 和 Client 都用 steady_clock，所以 Gateway 對兩者量到的 offset 幾乎相同。
    - delay 最小值 0.1～0.4 ms；沒有不合法、負 delay 或缺樣本。
  - 證據：`build/target/_build/test/logs/pvp-v71-time-echo-20261016/`。
  - Architecture Delta：無。產品內的變動：`ClockSync` 改成只有標頭檔，ipc 的原始檔少一個；Gateway 多 `echo.go`。
  - 實際約 0.7 h（13:48～14:30；估 4 h）。

- 2026-10-16 14:41：`drop-report` 開始（D103，檔位 high，列舉與上限局部 xhigh，估 13～16 h）。`time-echo`（`55dc734`）的四平台 L1 是 run 38855969436。

- 2026-10-16：`drop-report` 實作與驗收。
  - wire：`runtime_v7.proto` 加 `GatewayDropReport`、`DropCount`、`DropSample`、`DropReason` 與 oneof 21。過渡原因的號碼由契約的保留號與 `ingress_statistics.go` 的鍵順序還原：16、17＝A3／A5 的封包，20、21＝其射擊數，22＝截短，23＝拒絕線溢出，24～26＝Leave 時清除（命令、射擊、ACK），43、44＝D1、D2；這個中間 commit 先用，edge 批刪掉出口時一起刪、改成 reserved。32～34 只定名稱，出口在 edge 批。
  - 名稱表：`drop_report.go` 的 `dropReasons` 是 DropReason↔ingress 鍵的唯一綁定；`tests/object_fps_pvp/fixtures/drop_reasons.json` 是 Go、C++、Python 共用的 golden（40 個值）。
  - 新出口（新鍵加在行尾）：玩家的 `drop_write_failed_{snapshot,results,control}_packets`（D5）、`drop_control_queue_full_control_packets`（D6）、`drop_replaced_snapshot_packets`（送出迴圈中途被取代，每位還沒拿到的 active 玩家各 1）；Gateway 的 `drop_coalesced_snapshots`（讀取端新蓋舊）。它們不在 IG 內（是事件，不是收到的資料包）。ingress golden fixture 維持凍結分析器釘住的 v1 鍵，Go 測試另外確認行尾的新鍵。
  - 帳本（`dropLedger`，葉鎖）：Server 的 `countIngress`／`countIngressAt`／`countGatewayLocked` 與 link 的 `countLocked` 在更新 10 秒行的同一處記入累計值。樣本帶收包時刻與 GYOP 序號（Server 在處理資料包時記下）或事件時刻與身分（下行的 Gateway 序號、snapshot tick）；link 內的過渡原因只帶項目數與事件時刻。
  - 回報：1 Hz 計時器隨心跳標記一輪；writer 在寫出前一刻才組每條串流的回報（report_sequence、樣本、dwell 都在那時決定），每輪最多 MaxPlayers×2 條；玩家移除後送一則 final（從沒計數過的不送），延後佇列滿了就併進 Gateway 串流的 `merged_streams`；每寫出一則記一行 `drop report player=… report_sequence=… …`。
  - 優雅關閉：`Server.Close` 改成先寫所有非空串流的 final（含 Gateway 串流與延後中的）再關 link；writer 正在寫時照原本的方式中斷，不寫 final（記一行 log）。link 已失效時無法回報（契約的「只寫 log」）。
  - Match（`IpcHost` 的 `DropReports`）：每則回報記 `gw_drop report <同 Gateway 的內容>`；序號、累計倒退或消失、final 之後、範圍、樣本原因、samples_omitted／merged_streams 倒退都記 `gw_drop violation …` 並計數，不關 link（D-5）。
  - D32：守衛加入丟棄回報的符號；差分測試的有心跳那一輪也送極端值的回報。
  - 驗收（L1，本機）：`test` preset 全量 CTest 72／72；`go vet`、`go test -race -count=3`；權威 digest 同機兩樹比較 scale 1、3 各 35 情境 0 差異；突變：本批 17 個（`v71-drop-report`）加上受影響的既有突變共 194 個全部殺死。第一輪有 6 個沒殺死，都是突變或測試寫法的問題，修正後重跑全部殺死：兩個 Go 突變改成仍能編譯的替換；兩個 C++ 違規檢查改成逐行字串常值（doctest 原本印出的是變數名，不是內容）；`v7-flow-snapshot-regressed-uncounted` 的期望容許行尾新增的鍵；`v7-flow-final-before-writer-exit` 見下面 must-1。
  - 短跑次（只記錄）：clean-60、loss-shot、backpressure 6 情境、network、soak-short、quad clean-60 一輪全部通過。有回報的跑次（backpressure 的 gateway 與 host-ipc 故障）兩端依 (player, report_sequence) 逐則相等、沒有違規；乾淨跑次沒有丟棄、也沒有回報。host-ipc 的串流沒有 final：Match 那端先停，Gateway 讀到 EOF、link 失效，無法回報，符合契約。比對腳本 `compare_reports.py`。
  - 局部 xhigh 檢查（列舉與上限，1 位，約 38 分）：列舉、範圍、名稱、golden 四方一致，累計、上限、合併與單一寫出者都核對為正確；must 1、should 6、note 7。處理：
    - must-1（已修）：優雅關閉時 writer 正在寫，所有 final 都不送。改成一律有上限地等 writer 退出（3.5 秒，長於一次寫出的期限）再寫 final；writer 在每則寫出前看到 link 關閉，就把剩下的記為 abandoned 並返回，不再觸發 runtime 失效。等待上限改成套件變數；既有的 `blocked_writer` 收尾測試把它縮成 20 ms，讓 writer 撐過等待、關閉放棄 final 並中斷寫出，期望值維持 abandoned 3（這樣「final 行要等 writer 退出」的突變仍會被抓到）。
    - should-1（已修）：「每條串流每秒最多 1 則」原本只做到「每次觸發最多 1 輪」。現在距離上一輪不到 1 秒時延後，並在滿 1 秒時喚醒 writer。
    - should-2（已修）：Match 對 dwell 超過 10 秒的樣本記 `dwell_range` 違規。
    - should-5、should-6（已補）：
      - Go：執行中的 Server 有變化才送、離開後恰好一則 final；關閉時已回報過的串流與延後中的 final 也送；不先取消 ctx 的 Close；生產路徑的下行出口（Welcome、snapshot）帶 Gateway 序號與 tick；43、44 帶 tick；上行樣本帶 GYOP 序號、dwell 從收包起算；dwell 夾在 10 秒；每秒最多一輪；上限對照契約值；排序用 7 個原因；log 行格式釘死（C++ 測試用同一字串）。
      - C++：補齊各類違規（merged_streams、samples_omitted 倒退、超過 4 個樣本、counts 亂序／重複／為 0、counts 消失、dwell 範圍）。
      - 測試改用 `setForTest`（`t.Cleanup` 還原全域變數），避免 race detector 抓到的「還原早於背景 goroutine 結束」。
    - should-3、should-4：動到定稿契約與架構，由使用者決定（D104、D105）。
    - note：拿掉沒人讀的 `Violations()`；更正 IG 的註解（新增的下行 `_packets` 不在 IG 內）。其餘記在未結事項。
  - 證據：`build/target/_build/test/logs/pvp-v71-drop-report-20261016/`。
  - Architecture Delta：Gateway 新增的診斷狀態欄位，已核准（D105）。其餘在產品內：Gateway 多 `drop_report.go`；`Server.Close` 的收尾改成先寫 final；ipc 測試多連結 nlohmann_json（讀 golden）。
  - 實際約 2.7 h（14:41～17:21，含 xhigh 檢查約 38 分、突變約 1 h；估 13～16 h）。

- 2026-10-16 17:25：`edge-results-resend` 開始（D106，檔位 high，通道計時局部 xhigh，估 7～8 h）。`drop-report`（`598b609`）的四平台 L1 是 run 38873139138（手動觸發）。D104 的 §2 措辭與 D105 的白名單分類已寫進 protocol-v7。
- 2026-10-16：`edge-results-resend` 實作與驗收。
  - Match（`IpcHost` 的 `ActionLane`）：由「每個裁決只送一次」改成 §2a 的重送。Client ACK 之前每位玩家每 1/30 秒最多一框；每框是退役值加至多 8 筆裁決：最小的 4 筆未確認 id 每框都有，其餘 4 個槽位用游標循環，框內依 id 遞增。退役前進時送一次；ACK-only 而且 0 < ack ≤ 退役值，或批被拒收（Host 拒收、超過 8 筆、格式錯誤）時，再送一次（`asked`，仍守每間隔一框）。
  - Gateway：行為不變，仍以帳本冪等合併副本；只更正過時的註解。runtime proto 的 `ActionBatch`／`ActionResults` 加上送達註解（Go 綁定重產，差異只有註解）。
  - 測試：
    - IpcHost：改寫原本「每個裁決只送一次」的測試（現在是重送次數的上下限與退役後停止），新增最小 4 筆與循環（16 筆、14 筆兩種，後者在框內繞回）、ACK-only 補送退役、三種拒收各回一框、連續 60 次拒收仍守每間隔一框。
    - Go：副本在 Gateway 不改變任何東西（「只送一次」與「加上副本」兩種 IPC 輸入，假時鐘下 Client 的資料包逐則相同，含遺失與 ACK-only）；另用忠實的通道模型和「立刻送」比較 Client 第一次收到每個裁決的時刻（見「等你決定」）。
  - 突變：本批 12 個（`v71-edge-results-resend`）加上受影響的 v7-08b 與 `v7-flow-ledger-ipc-*` 共 31 個，全部殺死。`v7-08b-lane-no-sent-tracking` 刪除（它描述的「重送到 ACK 為止」就是現在要的行為）；`v7-08b-lane-repeat`、兩個 `v7-flow-ledger-ipc-*-silent` 改成新寫法。第一輪有 2 個是錯誤訊息不符：`v7-08b-lane-unpaced` 讓差分測試的讀取迴圈停不下來（重送不設節奏時 Match 一直在寫），整個 ipc 測試逾時；讀取迴圈加上 2 秒期限後，用預期的訊息失敗。`v71rr-cursor-stuck` 讓 id 14／16 永遠送不出去，在等那一個裁決時就失敗，期望值加上這個訊息。
  - 驗收（L1，本機）：`test` preset 全量 CTest 72／72；`go vet`、`go test -race -count=3`；權威 digest 同機兩樹比較 scale 1、3 各 35 情境 0 差異（IpcHost 不在 digest 的路徑上，只作確認）。
  - 局部 xhigh 檢查（通道計時，1 位，約 13 分）：must 0、should 3、note 8。
    - should-1：新裁決可能晚到，§9「Client 看到的不變」的說法太強。無損模型 8 個種子：369 個裁決中 63 個（17%）比「立刻送」晚到，最多晚 33 ms（上限「≤1 個間隔＋1 ms」在跑之前寫定）。§9 依 D107 修改。
    - should-2（已補）：連續觸發時的速率上限測試，與「被觸發的通道立即到期」突變。
    - should-3（已補）：循環槽位在框內繞回的情形、框內嚴格遞增（Client 會整框拒收重複 id），與「不排序」「不繞回」兩個突變。
    - note（已處理）：只有 ACK-only 批才讀 Host 的結果；補「超過 8 筆的拒收不回覆」突變；更正兩處註解。
    - note（記錄）：見未結事項。
  - 證據：`build/target/_build/test/logs/pvp-v71-edge-results-resend-20261016/`。
  - Architecture Delta：無（Match 的通道狀態多 `cursor`、`asked`，少了 `sent`；屬 IpcHost 內部）。
  - 實際約 1 h（17:25～18:19，含 xhigh 檢查約 13 分；估 7～8 h）。
- 2026-10-16：同步 master（`497c138`，PR #79、#80）。只有 AGENTS.md 與 network-architecture 的標題改變，沒有衝突。之後照新的 AGENTS.md 做：§3 的 `Design Invariants`（含 `Replacement Criteria`）與「状態の権威」、§14 Test and Defect Handling（改既有測試的判定要寫理由；受入判定與 Design Invariants 守衛的判定要經你核准；事前宣告的門檻要在看結果之前核准）。本批改動既有測試判定的理由已寫在上面的紀錄與 commit 訊息。

- 2026-10-16 20:09：`edge-input-passthrough` 開始（D108，檔位 high，局部 xhigh，估 20～26 h）。`edge-results-resend`（`5c0d8f5`）的四平台 L1 是 run 38879679708（手動觸發）。

- 2026-10-16：`edge-input-passthrough` 實作與 L1（進行中，記錄器）。
  - Gateway：`runtime_link.go` 的輸入改成依到達順序的單一佇列（controls 與輸入窗口同一個 FIFO；controls 不丟、超過 64 則 link 失效；每位玩家至多 64 則輸入窗口，滿了丟最舊，計入 `drop_ipc_queue_overflow_input_packets`（回報 32，樣本帶被丟資料包的身分）與行尾的新屬性鍵 `drop_ipc_queue_overflow_input_commands`）。刪除 `lastResolved` 副本、命令表、epoch／life、拒絕線、已寫出去重、合併、截短、D1、D2；Leave 與逐出不再丟佇列中的輸入。解碼成功就提交序號與活性（D-11）。
  - 丟棄原因 22～24、43、44 在 proto 改成 reserved；golden 重產（35 個原因）。被刪路徑的 ingress v1 鍵照樣輸出 0。
  - Match：host mutex 的鎖等待直方圖（SubmitInput 與 step 各一，`network statistics` 行尾的 `host_lock_*` 鍵；只記錄，D32 守衛加一項）。protocol-v7 §9 寫「IpcHost 層」，實作在 `MatchRuntimeHost`（鎖在那裡），用語差異記在未結事項。
  - 測試：Go 的原樣轉送、到達順序、Leave 不超車、FIFO 丟最舊與回報、寫出卡住時的佇列深度、控制訊息上限、輸入立刻喚醒 writer、解碼後才提交、無 snapshot 守衛、I0；C++ 的副本差分（每個窗口送兩次並夾帶已解析命令與另一個 life 的窗口，判定與最終 snapshot 逐位元相同，並斷言 host 確實計到副本）、鎖等待的記錄；worker probe 的「Client 拒收舊、重複、倒退的 snapshot」。
  - 改了既有檢查的判定（AGENTS.md §14.4，理由）：`ingress_cross_check.py` 只接受 I3 以 protocol-v7 §9 宣告的形狀失敗（Gateway 副本 0、Match 副本 > 0）；其他形狀照樣失敗。舊的去重、路由、拒絕線測試隨程式刪除（`dedup_test.go` 整檔）。
  - 突變：舊的 50 個（去重、路由、拒絕線、截短、D1、D2）隨程式刪除；7 個改成新寫法；本批新增。第一輪 214 個中 205 個殺死，其餘是新程式下的寫法或期望值問題，修正後殺死（其中 `v71ip-match-takes-another-life` 讓副本差分的未來 life 窗口改帶不同內容才抓得到）。
  - 局部 xhigh 檢查（程式，1 位，約 27 分）：must 0、should 5、note 11。should 全部補測試與突變（控制訊息上限與歸零、另一位玩家的佇列、Server 組的樣本身分、輸入喚醒、溢出命令數）；note 處理了 link 拒收不再讓房間失效、Client 的 epoch 倒退、副本差分的計數斷言、測試持鎖 100 ms、過時註解；其餘記在未結事項。
  - D45 門檻來源檢查（L2 草稿第 1 版，1 位 xhigh，約 17 分）：送核准前必須修正 7、應修正 8；改寫成 [第 2 版](edge-input-passthrough-l2.md)，對應表在其第 11 節。也發現 clean 時 B 本來就是一包一則 IPC，D-8 擔心的負載主要在故障情境。
  - 兩個 commit：B `42f6e88`（只加鎖等待量測；B 樹全量 CTest 72／72）、A `6d9ecf5`；四平台 L1 是 run 38914084925。兩棵樹：`../GYO-Engine-ip-before`、`../GYO-Engine-ip-after`（detached、乾淨）。L2 工具在 `build/target/_build/test/logs/pvp-v71-edge-input-l2-prep-20261016/`，乾跑 31 種情況全部符合預期，A 沒有跑過任何項目。
  - 時間：22:46 時，實作、L1、兩次 xhigh 檢查完成；接著是兩個 commit、L2 工具與乾跑，然後停下來等你核准宣告。

- 2026-10-17：`edge-input-passthrough` 的 L2（D109、D110）**通過**。
  - 第 1 次 session 停在第 58 項（B 樹 clean-60 的 I1：關閉時被 reset，宣告缺漏），依 D110 只改 I1 的規則後整個重跑；第 1 次保留在 `build/target/_build/test/logs/pvp-v71-edge-input-l2-20261017/`。
  - 第 2 次 session（`pvp-v71-edge-input-l2-20261017b/`，00:58～03:08）：98／98 完成，逐跑次判定沒有停下，受干擾 1 個（network-impairments-r2-A，上限 3）；`judgement.md`。
  - J2 替代率：各情境 A−B 都在門檻內（clean、quad 0／0；network-impairments 21.9→22.7‰，門檻 +1.25；backpressure、short-stall 差 ≤0.03‰；x1 −0.4‰）。ramp、soak 只記錄（61.2→60.7、0.05→0.03‰）。
  - J3（soak）：兩棵樹逐出 0、`connection_quality_failures_max` 0。J4a（soak 180 個視窗）：A／B＝0.97（門檻 1.5）。J4b：所有情境的晚醒 p99 都在第 0 格。J4c：鎖等待 ≥250 µs 的比例兩棵樹都是 0。J7：成立。
  - 預測都沒有被推翻：P0 clean 的 `received_inputs` A 1921／B 1920，host-ipc-1000ms +101（預測約 +100）；P1 clean 的 Match 命令 A／B＝2.99；P2 clean 的 Match 副本 3838（B 的 written duplicate 3836）；P4 CPU clean 1.06、quad 1.02。故障情境的佇列溢出也都是 0。
  - 差異帶（給 `pv7-checkpoint`，宣告第 9 節）：以上的 A−B。
  - 實際約 7 h（2026-10-16 20:09～10-17 03:15；含 xhigh 兩次、L2 工具、兩次 session 約 3.5 h；估 20～26 h）。

- 2026-10-17：`heartbeat-sync` 的睡眠那段（D102、D109，只記錄）。A 樹（`6d9ecf5`）的 Match、Gateway 與兩個 Client 的 timing probe，跑次中由使用者從選單讓 Mac 睡眠。證據 `build/target/_build/test/logs/pvp-v71-heartbeat-sleep-20261017/`（`sleep_run.py`、`run/`、`clocks.jsonl`）。
  - 睡眠：`pmset` 紀錄 11:42:40 進入、11:43:14 喚醒（34 秒）；腳本每秒的讀數顯示牆鐘過了 31 秒，`time.monotonic`（mach_absolute_time）與 `CLOCK_UPTIME_RAW` 只過了 4.2 秒，`CLOCK_MONOTONIC`／`_RAW` 過了 31 秒：實際暫停約 26.8 秒。
  - 時鐘域：Go 的單調時鐘（G）睡眠時停止，C++ 的 `steady_clock`（M、C）繼續走。Match 的時鐘視窗在醒來後出現一次 offset 跳變約 26.73 秒（`jump=1`，跨睡眠的那個樣本 delay 約 26.7 秒）；Gateway 對兩個 Client 的回聲在跨睡眠的視窗各有 1 個 `negative_delay`。和事前的推論一致（未結事項的 heartbeat-sync 項）；protocol-v7 §2 的〔推論〕改成實測。
  - runtime link 撐過睡眠：Gateway 的活性用 G，看不到這段時間，沒有逾時；Match 到結束都沒有斷線（最後是正常的 EOF）。
  - Client 在醒來後約 1 秒斷線（`Gateway connection timed out`）：Client 用 C，看到約 27 秒沒有 Gateway 的封包，超過自己的逾時。兩位玩家因此 Leave，probe 以失敗結束。這是既有行為（筆電睡眠＝斷線），不是本批造成的；要不要在醒來後改成重連或延長逾時，是之後的功能題，記在未結事項。

## 未結事項

- **筆電睡眠後 Client 會斷線**（2026-10-17 睡眠測試）：Client 的逾時用 `steady_clock`，睡眠時繼續走，醒來就判定 Gateway 逾時。既有行為，不在 v7.1 範圍；之後要不要處理（醒來後重連、或用不含睡眠的時鐘判逾時）由你決定。

- **`edge-input-passthrough` 的 xhigh note**（記錄）：
  - N3：佇列項目攜帶收包時刻與 GYOP 序號（回報 32 的 dwell 起點），字面上和 D105「單次呼叫的脈絡不得跨資料包保留」衝突。建議 `edge-contract` 把它歸類為「隨佇列項目攜帶的診斷狀態」，由你決定。
  - N5：protocol-v7 §9 寫鎖等待直方圖在「IpcHost 層」，實作在 `MatchRuntimeHost`；`network_statistics.py` 不彙總 `host_lock_*`，L2 的 judge 直接讀。
  - N10：`fwd_main_input_packets` 現在是「交給 link」，溢出的資料包同時算它和回報 32；`edge-analyzers` 照這個語意定義 I0。

- **`edge-results-resend` 的 xhigh note**（記錄，之後的批次處理）：
  - N1：這批 Match 的「ACK-only 補送退役」只在競態下觸發（link 只在 ACK 超前退役時才送 ACK-only），Client 漏收退役仍由 Gateway session 補送；`edge-results-passthrough` 讓 link 原樣轉送 ACK-only 後才真的用到。
  - N2：link 對每個副本都 `notify()` 一次，每位玩家約 30 Hz 多一次 writer 檢查，不影響時序；可改成合併有變化時才喚醒。
  - N3：link 對無法裁決的被拒請求照舊 30 Hz 重送，現在 Match 每次都回一框；L2 的流量數字會看到雙向 30 Hz。
  - N4：重送的副本排在 snapshot 前面；只有 TCP 低於約 120 框／秒才有影響（推論，loopback 可忽略）。

- **丟棄帳本的 `ended`**（D105）：只增不減，上限是離開過的玩家數；長時間執行的 Gateway 會累積。在 `edge-contract` 時一起看。

- **保留的 worktree 與本機分支**：
  - `GYO-Engine-v7final`（`6d3f287`）、`GYO-Engine-c2-after`（`d685d1e`）：v7 的基準與凍結工具副本，保留到 `d44-regression-pv7` 完成。
  - `GYO-Engine-m0`（`5ef8a89`，分支 `m0-probe-5ef8a89`，未推送）：M0 已取消（D90），經使用者同意後歸檔。
  - `tt1-l2-wake-record`（`a8f9535`）、`v6c2`（`1be1edd`）等其他未推送的 commit 與 worktree：列清單請使用者決定。
  - 任何刪除都要先經使用者同意。
- **D58 (1)(8) 的暫定決定**：在 `log-contract` 一起確認。
- **檔位**：`pv7-design`、`log-contract` 開始前先問 ultracode（v7 D45）；`pv7-design` 另做 1 次 xhigh 對抗式檢查；每次 L2 宣告前做門檻來源的對抗式檢查（D45）。
- **after-re-evaluate 的項目**：內容不變，等 `re-evaluate` 之後和評估結果一起排（README）。
- **Client 的 `other_version_datagrams`**：只確認了接線（短跑次值為 0），沒有正向測試：統計視窗 10 秒，補測試會讓 worker 測試變長很多。在 `drop-report` 或 `observability-tools` 補。
- **`heartbeat-sync` 留下的項目**（xhigh 檢查的 note）：
  - Match 回覆「排在 controls、action lane、snapshot 之後」沒有可決定的測試（asio 下同一個 socket 可寫優先於可讀），只靠審查與 L2。
  - 差分測試不到 10 秒，所以視窗只在關閉時結算一次，跳變與漂移沒有在差分中算到（估計器另有單元測試，且結構上隔離）；也不跑滿品質視窗（600 tick × 3），逐出與 160 ms 門檻沒有在差分中判定。要延長會讓 CI 多數十秒。
  - Match 被 `Stop()`（不經過 `Close`）時沒有最後一個視窗的 log；runner 先停 Gateway，所以平常會走 `Close`。
  - 活性逾時有兩種字串：Read 逾時是 `read tcp …: i/o timeout`，剩餘 ≤0 是 `runtime liveness: i/o timeout`，兩者都含 `i/o timeout`；在 `log-contract` 統一。
  - 只為測試存在的狀態：`runtimeLink.now`、`takes`，套件變數 `runtimeLiveness`、`writerIdleWait`、`diagnosticInterval`。`edge-contract` 的白名單（D-14）到時要分類。
  - 差分測試的結尾等 300 ms 沒有訊息，已改成整體截止 3 秒；`edge-results-resend` 之後 Match 會重送未確認的結果，到時再確認。
  - L2 的預期（xhigh 推導，〔推論〕）：Windows 上 Go 的單調時鐘解析度可能是 15.6 ms，Gateway 在 Windows 時會出現假的 `jump=1` 與 `negative_delay`（`lan15-windows` 要事前宣告）；macOS 上 Go 用 `mach_absolute_time`（睡眠時停），libc++ 的 steady_clock 可能睡眠時繼續走，所以睡眠那段兩端都預期出現一次約等於睡眠長度的跳變，不算失敗。
- **`drop-report` 留下的項目**（xhigh 的 note）：
  - 記憶體只增不減：Gateway 帳本的 `ended` 與 Match 的串流表都依玩家 id 累積；惡意的 join／leave 推不高訊息數，但推得高記憶體。
  - 過渡原因的樣本身分不完整：16、17、20、21、28 沒有 action id 範圍；22～26 的 dwell 起點是計數時刻。edge 批刪除過渡原因時一併消失；28（結果寫出失敗）的 action id 範圍在 `edge-results-*` 補。
  - Match 照單接收違規回報的值、繼續往下比對（不標不可信）；L2 分析要把出現過違規的串流視為不可信。
  - 兩個 `coalesced_snapshots`：`gateway transport coalesced_snapshots` 含送出迴圈中途的取代，新鍵 45 只計讀取端；在 `log-contract` 統一。
  - L2 的比對腳本 `compare_reports.py` 還在證據目錄；L2 事前宣告前要收進 `build/acceptance/object_fps_pvp` 並加測試（會動到凍結清單：新增檔案）。
- **`edge-contract` 的約束**（文件會話，設計事故行動 4）：契約節的標題改成英文 `Design Invariants`，不隨文件語言翻譯（使用者的決定，文件會話 PR #80，2026-10-16 通知時尚未合併；這更正了 PR #79 時「標題逐字保持『不可破壞的契約』」的要求）。
  - `edge-contract` 把 Gateway 的正式條文寫在 `network-architecture.zh-Hant.md` 的 `### Design Invariants` 節下。PR #80 已合併到 master（`8f2f3fe`，四平台 L1 通過）；本分支在 `edge-results-resend` commit 之後同步 master，不自行改名。master 的 AGENTS.md 另新增 §14 Test and Defect Handling（原 §14、§15 往後移），同步後照新規則做。
  - AGENTS.md §3 改成一般規則「状態の権威（State Authority）」，不再列 Gateway 的細節；Gateway 的允許／禁止清單（protocol-v7 §2a）只放在 Design Invariants 節。
  - 同節新增 `Replacement Criteria`：新建元件或加大既有元件的責務之前，先在 Design Invariants 寫下「什麼情況就換掉它」。`edge-contract` 時提案 Gateway 的 Replacement Criteria，由你決定。
