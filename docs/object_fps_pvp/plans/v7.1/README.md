# PvP v7.1 hotfix 分批計畫

更新：2026-10-15。Owner：`object_fps_pvp`。
**狀態：規劃完成（[交接](HANDOFF.md) D86～D91），等使用者核准開始第一批 `pv7-design`。** 開始時先問是否用 ultracode（v7 D45）。

版本號：v7.1＝遊戲版本（v7 之後、v8 之前），pvN＝網路協議版本（[v6 交接](../v6/HANDOFF.md) D20）。v7 停在 pv6；**pv7 在 v7.1**。
本目錄和 `v5`～`v8` 同層，是計畫文件，不是新的 Architecture Concept。
盤點與每一項的處置見 [清單核對結果](INVENTORY.md)。2026-10-14 的種子清單（從 v7 移入的 13 項與重構候選 A～G）已逐項併入該文件；原文見 git 歷史 `c742836`。

## 定位

- **v7.1 是 hotfix**（D90）。事故後遺症、檢測功能、遺留功能混在一起，形成一個死循環：沒有正確的檢測數據，就無法評估事故和故障；評估不了，事故怎麼計算都解決不了。所以**先檢測、再評估**：版本與資訊分離、對時、回聲做在前面，丟棄回報接著做，其餘修正等有了數據再重新評估。
- **目的**（D87）：做功能之前，先把架構與契約穩定下來；v8 只做功能和 editor。v8 目前看得到的契約需求只有**骨骼 hitbox**，pv7 只給它契約支持，實作在 v8。
- **pv7 是唯一一次 wire 變更**（做法 (a)，D90）：所有 wire 欄位在 `pv7-design` 一次定完，照下面的順序在同一條 PR 線分批實作，最後一次合併。v7.1 的情況特殊，分次升版沒有意義。
- **推導順序**照 AGENTS.md §2「Feature → Contract → Architecture」：契約和架構的工作都要說出它支撐的功能。

## 樹狀總覽

```
v71-plan                              「v7.1 hotfix：先檢測、再評估；wire 一次設計、一次合併」
├── prep
│   └── plan-sync-docs ▶              「low｜同步 master（PR #75～77），寫 plans/v7.1」
├── pv7-line                          「同一條 PR 線，三角色，最後一次合併」
│   ├── pv7-design ⏸                  「ultracode（先問）＋xhigh 檢查｜所有 wire 欄位一次定完」
│   ├── proto-rename-v7               「low｜proto 與套件改名為 v7，版本值仍是 6」
│   ├── version-errors                「high｜P1：版本 6→7、明確錯誤、凍結 pv7 版分析器」
│   ├── heartbeat-sync                「high（時鐘域局部 xhigh）｜P2：心跳對時」
│   ├── time-echo                     「high｜P3：時間回聲」
│   ├── drop-report ◆                 「high（列舉與上限局部 xhigh）｜P4：Gateway 丟棄回報」
│   ├── gateway-duties ◆              「high｜P5：職責分類與條文，不改行為」
│   ├── cause-records                 「high｜E2、動作拒收回覆、Leave 原因」
│   ├── hitbox-fields                 「high（驗證窗口局部 xhigh）｜hitbox 欄位，填等同現行的值」
│   ├── pv7-checkpoint                「medium（分析 high）｜行為改變前的整體回歸」
│   ├── a1-shot-step                  「high（局部 xhigh）｜A1：射擊照命令步判定，FireGate 退役」
│   └── pv7-merge                     「low｜四平台 L1 後合併，同步文件」
├── after-pv7
│   ├── tt2-trace                     「high（flush 與關閉局部 xhigh）｜Engine Trace，日誌契約的前提」
│   ├── log-contract ⏸                「ultracode（先問）＋high｜日誌 Data Contract（含原第 10 批、D58）」
│   ├── dead-letter-position          「medium｜P6：確認 D57」
│   ├── d44-regression-pv7 ◆          「medium（分析 high）｜丟棄回報上線後的 D44 回歸」
│   ├── incident-close ◆              「medium｜事故紀錄結案」
│   ├── observability-tools           「high｜相位決策進 trace、增量計數、加寬偵測器、遺失種子」
│   └── re-evaluate ⏸                 「medium｜用新數據重新評估相位追蹤與 D44，交使用者排序」
└── after-re-evaluate ⏸               「內容不變，評估後和結果一起排」
    ├── architecture                  「SDL 隔離、-fexperimental-library、code smell、分析器收斂、相位追蹤 golden」
    ├── measurement                   「Windows LAN、其餘觀測工具」
    ├── platform                      「關閉阻塞、LogFile、AssetWatcher、SIGINT、不穩的測試等」
    └── docs-close                    「系列紀錄、STABLE_BASELINE v7.1」
```

記號：✓ 完成、▶ 進行中、⏸ 等使用者決定、◆ 事故後遺症（v7.1 結束前全部結案）。

## 工時

估計沿用規劃草稿各批的數字（單位和 v7 各批的估計相同，例如 08c 估 20～25 h）。

| 區塊 | 工時 |
|---|---|
| prep | 約 3 h |
| pv7-line | 約 91 h |
| after-pv7 | 約 52～60 h |
| after-re-evaluate | 約 95～105 h，另加 re-evaluate 決定的修正（未估） |

停止規則：每批開始前寫出估計；實際超過 1.5 倍，或單一問題累計超過 2 小時，就停下回報。

## pv7-line 各批

同一條 PR 線，Client、Gateway、Match 三個角色在同一個 PR；每批是獨立 commit、各自驗收，最後一次合併。檔位見樹狀總覽。

| 代號 | 範圍 | 驗收（L1／L2／L3） | 工時 | 備註 |
|---|---|---|---|---|
| pv7-design | 一次定完所有 wire 欄位與語意：版本與錯誤格式；心跳與回聲的載體和時鐘域；丟棄回報的原因列舉（含 A3／A5 轉送或只計數）；E2、動作拒收回覆、Leave 原因；A1（epoch、命令步 S、射手看到的時刻）；hitbox（命中部位）；各欄位在 v7.1 填的值與驗證規則。產出 `protocol-v7.zh-Hant.md` 草案 | 只改文件／—／使用者核准草案 | 14 | 哪個欄位需要新量測才能決定，就停下問使用者。之後再改 wire 要先徵求同意 |
| proto-rename-v7 | `client_v6`／`runtime_v6` 改名為 v7（proto、Go 套件、CMake target、include），版本值仍是 6；照 v6 的先例（`708e877`→`f255bf1`） | 全量 CTest＋Go、digest 不變／—／— | 3 | Architecture Delta：Build Graph 的 target 名稱 |
| version-errors | ProtocolVersion 6→7（四處定義與一致性測試）；版本不符時寫出期望值與收到值（HTTP 409、IPC、Client）；驗收工具與突變升 pv7；凍結 pv7 版分析器 | 版本與不符測試、突變、`compare_authority_trees.py` 35／35、golden 30／30／短跑次確認新工具重現 v7 基準（只記錄）／— | 10 | 從這批起，凍結的 pv6 工具讀不了新跑次；之後的 L2 都用這套 |
| heartbeat-sync | runtime link 每秒心跳，用獨立計時器（不共用 token、nextSend、喚醒來源）；偏移、RTT 最小值、漂移每 10 秒寫進兩端日誌；契約禁止用在 250 ms／160 ms 的本機年齡判定，並用突變守住 | 測試、突變、digest 不變／同機長跑次，只記錄（含睡眠後的跳變）／— | 5 | 新增 RuntimeEnvelope 訊息 |
| time-echo | Client↔Gateway 的往返時間戳（載體依 `pv7-design`），RTT 最小值濾波，兩端日誌 | 測試、digest 不變／只記錄／跨機器在 Windows LAN 場次量 | 3 | |
| drop-report | Gateway 依原因累計丟棄數回報 Match：只帶身分、每位玩家每秒最多一則、有上限；Match 寫日誌；守恆測試擴充；跨語言 golden | 測試、golden、突變、digest 不變／注入跑次：回報數＝Gateway 計數（事前宣告）／— | 10 | D49 第二階段。合併後做 `d44-regression-pv7` |
| gateway-duties | 逐項分類 Gateway 的狀態（nextSend、nextActionDeadline、retiredSent、retirementAsked、帳本、lastResolved 副本）是執行還是決定；寫「Gateway 職責」正式條文，修正條文歧義；只做不改行為的整理 | Go 測試、race、digest 不變／—／— | 6 | 整理會改行為就停下回報。Architecture Delta：Gateway 的責任條文 |
| cause-records | 轉送的命令帶 Gateway 停留時間（只觀察，用突變守住不進 slack）、端到端 snapshot 年齡、動作拒收回覆、Leave 原因 | 測試、突變、digest 不變／只記錄／— | 6 | 有了對時與回聲才能判斷原因 |
| hitbox-fields | 命中部位一律填 Body；射手看到的時刻由 Client 照實填；Match 驗證並記錄，不改判定 | 編解碼與驗證測試、digest 不變／只記錄（這個時刻和 Match Tick 的差，給 v8 的射擊回溯參考）／— | 6 | 放在 A1 之前：A1 是線上唯一的行為改變，前面的批次都要不改行為 |
| pv7-checkpoint | 行為改變前的整體回歸：四平台 L1、digest；用凍結 pv7 工具和 v7 基準比較 | 全量／事前宣告＋D45 門檻來源檢查／— | 6 | 有差異時在線內的 commit 之間二分 |
| a1-shot-step | Match 在命令步 T(S) 判定射擊，趕不上時有界退回到達判定；Client 改用序號閘門，刪除 FireGate；combat golden 依宣告重產；新分析器檔 | 測試、突變、golden 依宣告更新／事前宣告＋D45：clean 下權威 Cooldown＝0、退回率／使用者的連點手感 | 20 | Architecture Delta：FireGate 退役、射擊判定語意改變。命中回饋可能慢約 20～37 ms〔推論〕 |
| pv7-merge | 四平台 L1 後合併；同步 HANDOFF、README | 四平台／—／— | 2 | |

## after-pv7 各批

順序是使用者排的（D90、D91）：日誌契約先做，D44 回歸與觀測工具放在 re-evaluate 之前，因為評估要靠它們產生的數據。

| 代號 | 範圍 | 驗收（L1／L2／L3） | 工時 | 備註 |
|---|---|---|---|---|
| tt2-trace | Engine 的 `GYO::Trace`：有上限的紀錄 sink、通知喚醒的背景寫檔、Base 的日誌 facade（[Engine 計畫](../../../architecture/plans/time-threads-trace/README.md) TT-2） | Engine 測試、突變、digest 不變／寫檔不進熱路徑（射擊 P99 增量 ≤1 ms，宣告前做 D45 檢查）／— | 10 | Engine 子系統和第一個消費端（`log-contract`）同一個 PR |
| log-contract | 日誌行的版本與驗證規則：08c 行族 v1 的去留、D58 暫定項的確認、原第 10 批的紀錄（拒絕原因、snapshot 年齡、Match 的裁決與結束紀錄等） | 跨語言 golden、凍結分析器仍可讀／只記錄／— | 16 | 紙上 6 h＋實作 10 h。日誌格式是 Data Contract |
| dead-letter-position | 確認 D57 是否仍成立：ingress 日誌和丟棄回報維持兩份契約、對齊同一套原則，或合成一份 | 只改文件／—／使用者決定 | 2 | |
| d44-regression-pv7 | 丟棄回報上線後的 D44 回歸；加入持續延遲（x1、ramp）的樣本 | —／事前宣告＋D45 檢查／— | 9～12 | 事故後遺症 |
| incident-close | 事故紀錄的行動項目表更新、狀態改為結案 | 只改文件／—／— | 2～4 | 在 `drop-report`、`gateway-duties`、`d44-regression-pv7` 之後 |
| observability-tools | 相位決策寫進 trace（觀測缺口 3）；late 修正改用增量計數、加寬 D44 偵測器；網路案例換不同的遺失種子；Gateway→Match 延遲注入改成正式模式 | 工具的單元測試、新舊各分析一次／—／— | 10～13 | re-evaluate 的前提 |
| re-evaluate | 用 pv7 之後的數據重新評估：相位追蹤（B6～B9）、延遲位移（A4）、偶發 epoch 重設（A5）、連線品質的參考年齡等；交出建議，由使用者排序 | —／—／使用者排序 | 3 | 缺工具時直接告訴使用者 |

## after-re-evaluate（內容不變，評估後再排）

| 代號（暫定） | 內容 | 工時 | 依賴、備註 |
|---|---|---|---|
| phase-tracking-golden | 相位追蹤的閉環 golden（B10） | 約 7 | 只要 re-evaluate 決定修正相位追蹤，就排在那些修正批之前 |
| sdl-isolation | IP-3、IP-4＋原第 12 批：產品、probe、產品測試不直接呼叫 SDL（不是移除 SDL） | 26～34 | 依賴 `tt2-trace`；IP-3 的公開介面要 1 次 xhigh 審查（v7 D39）；v8 的音效依賴 IP-3 |
| cxx-flag-removal | 移除 `-fexperimental-library`（`std::jthread`、`std::osyncstream`） | 4 | 依賴 `tt2-trace` |
| smell-cleanup | token 規則三份拷貝、被替代的序號存兩份、衝突檢查重疊、第 04 批審查遺留 | 約 20 | 哪個修正批直接需要（例如改 token 規則），就在那一批做（AGENTS.md §1） |
| analyzer-converge | `quad_evidence.py`、`command_evidence.py` 收斂成 N 人版 | 5 | 用既有的 pv6 語料新舊各分析一次 |
| lan15-windows | Windows LAN 場次；跨機器偏移與漂移（≥15 分鐘）；Windows 的晚醒、Tick 精度、App Nap 等 | 7～8 | 依賴 `time-echo`；要排朋友的時間 |
| observability-rest | 量測時間基準統一、probe 起點的幀相位、HUD 來源 tick 等 | 約 3 | |
| platform-fixes | 關閉時阻塞約 3 秒、`LogFile` 非同步、`AssetWatcher` 的時間基準、背景 Client 忽略 SIGINT、Gateway 測試在高負載下不穩 | 13～15 | `AssetWatcher` 未估；`LogFile` 和 `tt2-trace` 一起 |
| docs-close | 「時鐘到網路路徑」系列紀錄（第 ⑤ 集要等 `time-echo`）；STABLE_BASELINE v7.1；v7 略過的驗收要不要補 | 約 8＋未估 | |

## 跨分支依賴

樹狀總覽只表達層級；下面是跨分支的依賴。

- `version-errors` → pv7-line 之後所有的 L2（凍結 pv7 版分析器）
- `pv7-checkpoint` → `a1-shot-step`（行為改變排在整體回歸之後）
- `drop-report` → `d44-regression-pv7`（pv7 合併之後）
- `drop-report`、`gateway-duties`、`d44-regression-pv7` → `incident-close`
- `tt2-trace` → `log-contract` 的實作、`sdl-isolation`（IP-3 用到日誌 facade）、`cxx-flag-removal`
- `d44-regression-pv7`、`observability-tools`、`log-contract` → `re-evaluate`
- `time-echo` → `lan15-windows`、系列紀錄第 ⑤ 集
- `re-evaluate` 決定修正相位追蹤 → 先做 `phase-tracking-golden`

## 取消的項目

| 項目 | 決策 | 理由 |
|---|---|---|
| A1 的手感盲測 | D91 | 使用者：不用測 |
| M0、earliest 估晚修正（B11）、權威端早到就延後、FireGate 重推條件 1～11 | D90 | 採用 A1，FireGate 退役 |
| 輸入防偽造 | D90 | 還沒到那個規模 |
| v7 的 3 人 LAN 測試（pv6） | D91 | 先處理完問題 |
| v7 發行 `object_fps_pvp-v1.2.0` | D91 | 發行需要的 Windows 實機證據來自上一項；下一次發行在 v7.1 之後 |

## PR 線

| PR 線 | 內容 |
|---|---|
| 規劃 | 本目錄、同步 master、v8 README 的備註更正 |
| pv7-line | `pv7-design`～`pv7-merge`，三個角色同一個 PR |
| after-pv7 | `tt2-trace`～`re-evaluate`；Engine 的 TT-2 和它的第一個消費端同一個 PR |
| after-re-evaluate | 評估之後再定 |

PR 內只改 `docs/` 的推送沿用 L1（docs-increment）。合併前確認四平台 L1，用 `gh pr merge --merge`。
