# PvP v7.1 交接

更新：2026-10-16。Owner：`object_fps_pvp`。**狀態：`proto-rename-v7` 完成；下一批 `version-errors` 等使用者說開始。**
本文件是 v7.1 的記錄器：每批開始、里程碑、停止時，和工作在同一個變更中更新。人看的計畫在 [README](README.md)，盤點在 [清單核對結果](INVENTORY.md)。

## 代號對照

本文件主要給 agent 接手用。批次代號像函數名，下表是它的註釋；樹狀總覽與各批明細見 README。

| 代號 | 意思 | 出處 |
|---|---|---|
| D86～D91 | 使用者在 v7.1 的決定（接續 v7 的 D1～D85） | 本文件「決策紀錄」 |
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

- **等你決定**：
  1. 開 pv7-line 的 PR（draft），讓四平台 CI 跑 L1（建議：之後每批推送都有 L1）。
  2. 開始 `version-errors`（檔位 high，估 12 h）。
- **進行中**：無。
- **下一步**：`version-errors`。
- **最後更新**：2026-10-16，依據本次推送（`proto-rename-v7` 完成）。

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

## 未結事項

- **保留的 worktree 與本機分支**：
  - `GYO-Engine-v7final`（`6d3f287`）、`GYO-Engine-c2-after`（`d685d1e`）：v7 的基準與凍結工具副本，保留到 `d44-regression-pv7` 完成。
  - `GYO-Engine-m0`（`5ef8a89`，分支 `m0-probe-5ef8a89`，未推送）：M0 已取消（D90），經使用者同意後歸檔。
  - `tt1-l2-wake-record`（`a8f9535`）、`v6c2`（`1be1edd`）等其他未推送的 commit 與 worktree：列清單請使用者決定。
  - 任何刪除都要先經使用者同意。
- **D58 (1)(8) 的暫定決定**：在 `log-contract` 一起確認。
- **檔位**：`pv7-design`、`log-contract` 開始前先問 ultracode（v7 D45）；`pv7-design` 另做 1 次 xhigh 對抗式檢查；每次 L2 宣告前做門檻來源的對抗式檢查（D45）。
- **after-re-evaluate 的項目**：內容不變，等 `re-evaluate` 之後和評估結果一起排（README）。
