# PvP v7.1 交接

更新：2026-10-16。Owner：`object_fps_pvp`。**狀態：`heartbeat-sync` 完成（睡眠那段 L2 待做）；下一批 `time-echo` 等使用者說開始。**
本文件是 v7.1 的記錄器：每批開始、里程碑、停止時，和工作在同一個變更中更新。人看的計畫在 [README](README.md)，盤點在 [清單核對結果](INVENTORY.md)。

## 代號對照

本文件主要給 agent 接手用。批次代號像函數名，下表是它的註釋；樹狀總覽與各批明細見 README。

| 代號 | 意思 | 出處 |
|---|---|---|
| D86～D101 | 使用者在 v7.1 的決定（接續 v7 的 D1～D85） | 本文件「決策紀錄」 |
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
  1. `heartbeat-sync` 的睡眠那段 L2：soak 中途讓系統睡眠幾分鐘，量兩端的跳變（只記錄，約 40 分，要你在場）。現在做，或等之後的 L2 一起做。
  2. 開始 `time-echo`（檔位 high，估 4 h）。
- **進行中**：PR [#78](https://github.com/yojinn-io/GYO-Engine/pull/78)（draft，pv7-line，含計畫文件）。CI 對 draft PR 會跳過 L1（`build/ci/common/ci_scope.py` 的 `reason="draft"`），所以每批推送後手動觸發 Cross-platform CI 跑四平台 L1，PR 維持 draft。`proto-rename-v7`（`d5104a0`）是 run 38771828429，`version-errors`（`046a0fd`）是 run 38784720725，都全部通過。
- **下一步**：`time-echo`。
- **臨時測試（只記錄）**：2026-10-16 的全 Windows LAN 臨時測試（`16216ff`）可以穩定遊玩；見規劃紀錄同日條目與 INVENTORY 的 F7、`N-summary-counter-wrap`、`N-hit-feedback-not-logged`、`N-windows-max-path`。
- **最後更新**：2026-10-16 13:30，`heartbeat-sync` 完成並推送。

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
    - 睡眠那段（中途讓系統睡眠，量跳變）要你在場，還沒做。
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

## 未結事項

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
- **`edge-contract` 的約束**（文件會話，設計事故行動 4，PR #79 已合併到 master `23457e1`）：AGENTS.md 不點名產品，而是用 `git grep -n "不可破壞的契約" -- docs` 找各元件的設計約束。把 Gateway 的正式條文寫進 network-architecture 時，「不可破壞的契約」的標題文字要逐字保持不變。
