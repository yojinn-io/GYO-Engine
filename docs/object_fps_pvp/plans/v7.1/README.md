# PvP v7.1 重構版（未規劃）

更新：2026-10-14（建立種子文件，[v7 交接](../v7/HANDOFF.md) D73，批次代號 pv7-to-v71）。Owner：`object_fps_pvp`。
**狀態：未規劃；v7 完成後用 ultracode 規劃。** 本文件只收集從 v7 移入的項目與重構候選，還沒有分批、契約或檔位。

版本號：v7.1＝遊戲版本（v7 之後、v8 之前），pvN＝網路協議版本（[v6 交接](../v6/HANDOFF.md) D20）。v7 停在 pv6、不改 wire；**pv7 在 v7.1**。

本目錄和 `v5`、`v6`、`v7`、`v8` 同層，是計畫文件，不是新的 Architecture Concept（不新增 top-level directory、subsystem 或 module）。

## 定位

- v7 之後、v8 之前的重構版。v7 處理執行緒、時間與網路路徑；v7.1 在 v7 的結果上整理 Gateway／Match 的職責與相位追蹤，再做唯一的一次 wire 變更；v8 處理玩法（[v8](../v8/README.md)）。
- **pv7 是 v7.1 唯一的一次 wire 變更**。要放進 wire 的東西，在契約批一次定完；Client、Gateway、Match 三個角色同一個 PR，嚴格版本相等、舊程序重新啟動（沿用 v7 D32、v6 D11①）。
- 為什麼移過來（D73，使用者原話）：「我要把pv7和相關任務從v7裡面拔掉，然後合併到v7.1裡面去。否則pv7會經歷2次架構調整沒有必要。」白話：v7.1 的重構會動到 Gateway 的職責（D49）、late 修正與 backlog，甚至射擊的送法。如果 pv7 先在 v7 定案，這些重構會讓 wire 再改一次；一起設計，wire 只改一次。

## 從 v7 移入的項目

| # | 項目 | v7 的出處 |
|---|---|---|
| 1 | 原第 11 批整批：pv7 契約（`protocol-v7.zh-Hant.md`）、ProtocolVersion 6→7（ClientVersion 與 RuntimeVersion 由它導出）、版本不一致的明確錯誤、驗收工具升 pv7；runtime link 心跳對時（任務 4、D32）；Client↔Gateway 時間回聲（D35） | [v7 README](../v7/README.md) 任務表第 4 列、批次表第 11 列；[v7 交接](../v7/HANDOFF.md)「P3：診斷與 pv7」的第 11 批、D32、D35 |
| 2 | `GatewayDropReport`（D49 第二階段）：Gateway 拿不到內容的丟棄以累計值回報給 Match；原因列舉一次定完整，dwell 的定義只有一個 | [08c](../v7/08c-drop-report.md)「先讀這一段」(G)、「已知缺口」、「對既有順序的影響」的第 11 批；規格草案 `pvp-v7-infoflow-plan-20261010/batch2/design3-counts-pv7.md` B；L2 的數量（D58、D62④） |
| 3 | 依 D49 重新檢視 Gateway 持有的狀態與 `lastResolved` 副本（上限、清理、通知、epoch），以及第 06、08、08b 批加進 Gateway 的狀態；是否轉送 A3／A5 | [08c](../v7/08c-drop-report.md)「Architecture Delta」第 6 項；[v7 交接](../v7/HANDOFF.md)第 11 批的「依 D49 的判斷規則重新檢視」、未結事項「重複過濾」的已解決 |
| 4 | `network-architecture.zh-Hant.md`「Gateway 職責」一節的正式條文（以 PEP／PDP 的名詞寫） | [v7 交接](../v7/HANDOFF.md) D49；[08c](../v7/08c-drop-report.md)「契約與文件」的末項 |
| 5 | ingress v1 與 `GatewayDropReport` 是否統一成一個死信 Data Contract（第 11 批的那一半；第 10 批的 log 契約留在 v7） | [08c](../v7/08c-drop-report.md)「對既有順序的影響」的第 10、11 批共通；[v7 交接](../v7/HANDOFF.md) D57、未結事項「死信隊列」 |
| 6 | `network-architecture.zh-Hant.md:230-233` 的條文歧義 | [事故紀錄](../v7/Incident/2026-10-10-gateway-silent-drop.md)的後續表 |
| 7 | 第 15 批拿掉的量測：Mac Gateway↔Windows Client 的偏移與漂移（≥15 分鐘，需要時間回聲） | [v7 交接](../v7/HANDOFF.md)「P7」第 15 批；[v7 README](../v7/README.md) 平台表 |
| 8 | 第 16 批之後的 D44 再回歸：v7 第 16 批的整體回歸不含 D49 第二階段；v7.1 做完 pv7 後再回歸一次 | [v7 交接](../v7/HANDOFF.md) D44、未結事項 D44 的子項 |
| 9 | FireGate 重推條件 4（心跳對時、時間回聲）與 11（D49 第二階段）在 v7.1 觸發 | [09-firegate-c2](../v7/09-firegate-c2.md)「重推條件」；08c 對條件 11 的改寫建議（[08c](../v7/08c-drop-report.md)「對既有順序的影響」，未核准） |

## 設計上的連動

- 讓 Match 看見 Client→Gateway 的遲到之後，late 修正會變多，D44（偶發 epoch 重設）的風險上升。所以 `GatewayDropReport` 必須和 late 修正、backlog 門檻 105 一起設計，不能單獨改（[v7 交接](../v7/HANDOFF.md)第 11 批的「提醒」；D51 ④）。
- 08c 已經讓已解析命令照常轉送（D49 第二階段中提前的部分），v7 的量測涵蓋它；`GatewayDropReport` 處理的是 Gateway 拿不到內容的那幾類。

## 重構候選清單（v7 完成時要逐項重新核對；已解決的標 commit）

### A. 架構與協議

| 代號 | 候選 | v7 的出處 |
|---|---|---|
| A1 | 射擊併入輸入命令、照命令步序判定，讓 FireGate 的 guard／spread 不再需要；替代做法是權威端早到就延後、不拒絕。**這項要改 wire，必須在 pv7 契約定案前決定放進 pv7 或延到 pv8** | 2026-10-14 使用者（D73）；guard／spread 的背景見 [v7 README](../v7/README.md)「本機射擊閘的兩個常數」與 [09-firegate-c2](../v7/09-firegate-c2.md) |
| A2 | 延遲位移：產生→執行約 38 ms、比 v6 多 1 幀，backlog 餘裕從 75 降為 45；候選是把追蹤用的領先拆成 1 | [09b](../v7/09b-phase-tracking.md)「延遲位移：不是缺陷，是設計造成的結果」；[v7 交接](../v7/HANDOFF.md)未結事項「C1 之後要決定的事」 |
| A3 | D44 偶發 epoch 重設（嚴重度高）：重推 backlog 門檻 105（D51 ④）；延遲 300 ms 左右消失時的重設；延遲消失時的快速正向回收 | [v7 交接](../v7/HANDOFF.md) D44、D51 ④；[09b](../v7/09b-phase-tracking.md) 1-6 節、1-7 節第 5 項、「5. 之後再評估」第 4 項 |

### B. 相位追蹤

| 代號 | 候選 | v7 的出處 |
|---|---|---|
| B1 | 重新擷取的樣本邊界 S／M，目標是一起消掉 1-7 節的三個已知限制（撤回少 3.3 ms、撤回後約 4 秒超過 guard、δ＞70.6 ms 串接，D71） | [09b](../v7/09b-phase-tracking.md)「5. 之後再評估」第 1 項、1-7 節第 1、2、4 項；D71 |
| B2 | Match 在命令到達時就發布 slack 樣本 | [09b](../v7/09b-phase-tracking.md)「5. 之後再評估」第 2 項 |
| B3 | late 修正的幅度依遲到量決定，或改「連續 N 次」 | [09b](../v7/09b-phase-tracking.md)「5. 之後再評估」第 3 項 |
| B4 | 停頓後的 reseed 用過時的 snapshot 播種，領先實際歸零 | [09b](../v7/09b-phase-tracking.md)「其他發現」第 1 項 |
| B5 | 權威 digest 不保護相位追蹤（RunLink 不經 TrackSlack），要補 golden | [09b](../v7/09b-phase-tracking.md)「其他發現」第 2 項 |
| B6 | `FireGate.cpp` 的 earliest 寫死減 2，領先跳升時估晚 | [09-firegate-c2](../v7/09-firegate-c2.md)「領先跳升」（`FireGate.cpp:22`）；[09b](../v7/09b-phase-tracking.md)「延遲位移」的選項 |

### C. Code smell（行為不變）

| 代號 | 候選 | v7 的出處 |
|---|---|---|
| C1 | token 規則三份拷貝，抽成純函式 | [09a](../v7/09a-input-send-pinning.md)「需要決定的事」第 5 項（本批不做）；[09b](../v7/09b-phase-tracking.md)「5. 之後再評估」第 7 項 |
| C2 | 被替代的序號存兩份：`substituted`（64）和帳本（600） | [08c](../v7/08c-drop-report.md)「Architecture Delta」第 6 項 |
| C3 | `p.commands` 的衝突檢查和 link 的內容比對重疊 | [08c](../v7/08c-drop-report.md)「Architecture Delta」第 6 項 |
| C4 | `quad_evidence.py` 和 `command_evidence.py` 收斂成 N 人版（v6 第 16 批） | [v7 README](../v7/README.md)「候選」；[v7 交接](../v7/HANDOFF.md)未結事項 |
| C5 | `-fexperimental-library`（若 v7 仍未移除） | [v7 交接](../v7/HANDOFF.md)「P3」末項（預計在 P3 移除） |
| C6 | 第 04 批 xhigh 審查只記錄的項目 | [第 04 批](../v7/04-client-roles.md)「xhigh 審查」；[v7 交接](../v7/HANDOFF.md)未結事項 |

### D. 暫定決定

| 代號 | 候選 | v7 的出處 |
|---|---|---|
| D-1 | D58 的 (1)(3)(4)(5)(8)（(3)(4)(5) 在 D60 附條件照建議接受，v7 完成時一起核對） | [v7 交接](../v7/HANDOFF.md) D58、D60、「現況」的「等你決定」 |
| D-2 | 08c log 行族 v1（若第 10 批沒有定案） | [v7 交接](../v7/HANDOFF.md) D53 第 3 項；[08c](../v7/08c-drop-report.md)「對既有順序的影響」的第 10 批 |

（D-1、D-2 是本表的列號；使用者清單中這一類的代號是 D，為了和決策編號 Dxx 區分加了連字號。）

### E. 量測與可觀測性

| 代號 | 候選 | v7 的出處 |
|---|---|---|
| E1 | 第 10 批之後仍未解決的觀測缺口 | [v7 交接](../v7/HANDOFF.md)第 10 批的「輸入：觀測缺口清單」 |
| E2 | 需要 wire 資料的紀錄（原本「併入第 11 批」） | [v7 交接](../v7/HANDOFF.md)第 11 批（D73 的加註） |
| E3 | 量測工具時間基準統一（t_frame 與 t_c） | [09b](../v7/09b-phase-tracking.md)「09b-plan」的用語表（t_frame）與修正對照表第 5 列、1-7 節 |
| E4 | network 案例的遺失種子（固定種子） | [08c](../v7/08c-drop-report.md)「L2 的結果」；[09b](../v7/09b-phase-tracking.md)「5. 之後再評估」第 6 項；v7 D63 ④ |
| E5 | probe 起點的幀相位 | [v7 交接](../v7/HANDOFF.md)未結事項「probe 幀格點的相位」（D46 ⑦） |

### F. 平台與雜項

| 代號 | 候選 | v7 的出處 |
|---|---|---|
| F1 | `ClientConnection` 關閉時阻塞約 3 秒（httplib） | [v7 交接](../v7/HANDOFF.md)未結事項；[v7 README](../v7/README.md)「候選」 |
| F2 | `LogFile` 非同步化 | [v7 README](../v7/README.md)「候選」 |
| F3 | `AssetWatcher` 的 `detectedNs` 改用 Engine 的時間基準 | [v7 README](../v7/README.md)「候選」 |
| F4 | D30 門檻可能被逐級放大 | [v7 交接](../v7/HANDOFF.md) D30、未結事項「C1 之後要決定的事」；[第 04 批](../v7/04-client-roles.md) |
| F5 | 期限前自旋 | [v7 交接](../v7/HANDOFF.md)未結事項「自旋」 |
| F6 | App Nap | [v7 交接](../v7/HANDOFF.md)未結事項「Spaces、縮小時的斷線」；[v7 README](../v7/README.md)「v6 L3 的觀察」 |
| F7 | Windows Match Tick 精度（若第 15 批沒有量到） | [v7 交接](../v7/HANDOFF.md)未結事項「Windows Match 的 Tick 與 IPC 精度」 |

### G. 文件

| 代號 | 候選 | v7 的出處 |
|---|---|---|
| G1 | v5／v6 缺陷版要不要重新發佈（v7 D69：不重新發佈、開頭標明「缺陷版」） | [v7 交接](../v7/HANDOFF.md) D62 ③、D69；[事故紀錄](../v7/Incident/2026-10-10-gateway-silent-drop.md) |
| G2 | 「時鐘到網路路徑」系列紀錄（第 ⑤ 集「跨機器的時間」等 pv7） | [v7 交接](../v7/HANDOFF.md)未結事項「時鐘到網路路徑」 |

## 規劃方式（屆時）

- 用 ultracode 規劃（約 3 個方案、1 位評審、1 次對抗式檢查）；pv7 契約批另做 1 次 xhigh 對抗式檢查。開始前先問使用者（v7 D45 的做法）。
- 順序：
  1. 先做行為不變的重構和 golden（例如 C 類、B5）。
  2. 需要 pv6 凍結分析器的新舊對比，排在 pv7 之前。
  3. pv7 契約定稿後才實作（A1 要在定稿前決定放進 pv7 或延到 pv8）。
  4. 之後重推 FireGate（重推條件 4、11）、重跑 M0 型量測和 D44 回歸。
- 範圍、門檻或 ownership 的變更照 AGENTS.md 處理：觀測到的壓力才改架構，Architecture Delta 要明寫。
