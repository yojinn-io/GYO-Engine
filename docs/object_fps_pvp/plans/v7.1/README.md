# PvP v7.1 hotfix 分批計畫

更新：2026-10-21（after-re-evaluate 的判斷與批次草案，D158；v7.1 成為穩定基線，D157）；初版 2026-10-15。Owner：`object_fps_pvp`。
**狀態：v7.1 完成並成為穩定基線**（[STABLE_BASELINE](STABLE_BASELINE.md)，D157）：pv7-line 以 PR #78 合併（`0fabb52`），after-pv7 以 PR #82 合併（`5ee499a`）；樹狀總覽的各批都已完成。剩下 `after-re-evaluate`：判斷已確定（D158，[after-re-evaluate.md](after-re-evaluate.md)），批次已核准（D159），進行中；docs-close 的系列紀錄（G2）還沒決定。

版本號：v7.1＝遊戲版本（v7 之後、v8 之前），pvN＝網路協議版本（[v6 交接](../v6/HANDOFF.md) D20）。v7 停在 pv6；**pv7 在 v7.1**。
本目錄和 `v5`～`v8` 同層，是計畫文件，不是新的 Architecture Concept。
盤點與每一項的處置見 [清單核對結果](INVENTORY.md)。2026-10-14 的種子清單（從 v7 移入的 13 項與重構候選 A～G）已逐項併入該文件；原文見 git 歷史 `c742836`。

## 定位

- **v7.1 是 hotfix**（D90）。事故後遺症、檢測功能、遺留功能混在一起，形成一個死循環：沒有正確的檢測數據，就無法評估事故和故障；評估不了，事故怎麼計算都解決不了。所以**先檢測、再評估**：版本與資訊分離、對時、回聲做在前面，丟棄回報接著做，其餘修正等有了數據再重新評估。
- **目的**（D87）：做功能之前，先把架構與契約穩定下來；v8 只做功能和 editor。v8 目前看得到的契約需求只有**骨骼 hitbox**，pv7 只給它契約支持，實作在 v8。
- **pv7 是唯一一次 wire 變更**（做法 (a)，D90）：所有 wire 欄位在 `pv7-design` 一次定完，照下面的順序在同一條 PR 線分批實作，最後一次合併。v7.1 的情況特殊，分次升版沒有意義。
- **Gateway 回到無狀態邊緣**（D94，[設計事故 2026-10-15](Incident/2026-10-15-gateway-state-drift.md)、[方向評估](gateway-direction.md)）：Gateway 只翻譯、逐則檢查、原樣轉送；內部訊息保留身分並冪等，去重、帳本與結果重送由 Match 負責。這是設計前提的改變，排在檢測（P1～P4）之後、A1 之前，以三個行為批分別完成。
- **推導順序**照 AGENTS.md §2「Feature → Contract → Architecture」：契約和架構的工作都要說出它支撐的功能。

## 樹狀總覽

```
v71-plan                              「v7.1 hotfix：先檢測、再評估；wire 一次設計、一次合併」
├── prep
│   └── plan-sync-docs ✓              「low｜同步 master（PR #75～77），寫 plans/v7.1」
├── pv7-line                          「同一條 PR 線，三角色，最後一次合併」
│   ├── pv7-design ✓                  「high＋xhigh 檢查｜所有 wire 欄位與 IPC 送達語意一次定完」
│   ├── proto-rename-v7 ✓             「low｜proto 與套件改名為 v7，版本值仍是 6」
│   ├── version-errors ✓              「high｜P1：版本 6→7、明確錯誤、凍結 pv7 版分析器」
│   ├── heartbeat-sync ✓             「high（時鐘域局部 xhigh）｜P2：心跳對時」
│   ├── time-echo ✓                   「high｜P3：時間回聲」
│   ├── drop-report ✓◆                「high（列舉與上限局部 xhigh）｜P4：丟棄回報，先照現行出口」
│   ├── edge-results-resend ✓         「high（通道計時局部 xhigh）｜Match 在 ACK 前重送結果」
│   ├── edge-input-passthrough ✓◆     「high（局部 xhigh）｜★輸入原樣轉送，刪副本與拒絕線」
│   ├── edge-results-passthrough ✓    「high｜★結果原樣轉送，刪 Gateway 帳本與 A3」
│   ├── edge-actions-passthrough ✓    「high｜★動作原樣轉送，刪 link 視窗、A5、30 Hz 持有（L2 在 edge-rate-limit 一起跑）」
│   ├── edge-analyzers ✓              「medium（守恆式 high）｜改寫並重新凍結 ingress 守恆式」
│   ├── edge-contract ✓◆              「medium（守衛 high）｜P5：無狀態邊緣的約束與守衛」
│   ├── edge-rate-limit ✓             「high｜K1：限速改 token bucket（D121），跑 edge-actions-passthrough 的 L2」
│   ├── cause-records ✓               「high｜E2、動作拒收回覆、Leave 原因」
│   ├── hitbox-fields ✓               「high（驗證窗口局部 xhigh）｜hitbox 欄位＋E、S 影子紀錄」
│   ├── pv7-checkpoint ✓              「medium（分析 high）｜A1 前的整體回歸」
│   ├── a1-shot-step ✓                「high（局部 xhigh）｜★A1：射擊照命令步判定，FireGate 退役」
│   └── pv7-merge ✓                   「low｜四平台 L1 後合併，同步文件」
├── after-pv7
│   ├── tt2-trace ✓                   「high（flush 與關閉局部 xhigh）｜Engine Trace，日誌契約的前提」
│   ├── log-contract ✓                「ultracode（先問）＋high｜日誌 Data Contract（含原第 10 批、D58）」
│   ├── dead-letter-position ✓        「medium｜P6：確認 D57」
│   ├── d44-regression-pv7 ✓◆         「medium（分析 high）｜丟棄回報上線後的 D44 回歸」
│   ├── incident-close ✓◆             「medium｜事故紀錄結案」
│   ├── observability-tools ✓         「high｜相位決策進 trace、增量計數、加寬偵測器、遺失種子」
│   └── re-evaluate ✓                 「medium｜用新數據重新評估相位追蹤與 D44，交使用者排序」
└── after-re-evaluate ▶               「D158 判斷還需要的項目；D159 核准，worker-probe-repair 提到最前面」
    ├── 0 worker-probe-repair ✓       「high｜修好 worker probe（決策點 C）」
    ├── 1 系統與引擎
    │   ├── sdl-isolation-plan ✓      「ultracode｜IP-3／IP-4 的 PLAN、公開介面審查、遷移清單（D160 核准）」
    │   ├── sdl-isolation ⏸           「high｜產品、probe、產品測試不直接呼叫 SDL（ip3a 關卡）」
    │   ├── substitution-single-store ✓ 「→ outage-resync（PR #90）的 single-store-refactor、retention-600 完成」
    │   ├── silent-branch-asserts     「high｜無聲分支改成 assert 或計數」
    │   └── cxx-flag-removal          「high｜不再需要 experimental 程式庫」
    ├── 2 觀測工具
    │   ├── contract-readers          「medium｜compare_reports 收進工具目錄、host_lock 鍵進日誌契約」
    │   ├── trace-no-overwrite        「high｜重啟 Match 不覆寫前一段 trace」
    │   └── analyzer-converge         「high｜兩個命令分析器收斂成 N 人版」
    ├── 3 測試
    │   ├── token-tests               「high｜token 的存活突變、測試模型與產品連動」
    │   └── client-path-tests         「high｜世代檢查與他版計數的正向測試」
    └── 4 發行
        ├── windows-long-path         「high｜深資料夾也能載入 shader」
        ├── quick-close               「high｜關閉 Client 不再卡 3 秒」
        ├── wake-reconnect            「high｜睡眠醒來不斷線；短於 T_max 的睡眠＝中斷已由 outage-resync 處理（D172 D-11 (w1)），其餘仍在本批」
        └── lan15-windows             「high｜Windows LAN 場次」
```

記號：✓ 完成、◐ 實作與 L1 完成、L2 延後、▶ 進行中、⏸ 等使用者決定、◆ 事故後遺症（v7.1 結束前全部結案）、★ 行為改變（各自一批，事前宣告並前後對照）。

## 工時

估計沿用規劃草稿各批的數字（單位和 v7 各批的估計相同，例如 08c 估 20～25 h）。

| 區塊 | 工時 |
|---|---|
| prep | 約 3 h |
| pv7-line | 約 178～193 h〔推論〕（README 初版估 91 h；pv7-design 重估 111 h；D94 改為無狀態邊緣後為 170～179 h；§2a 的 xhigh 檢查後為 177～191 h，使用者已接受（D96）；再加 GYOP 序號欄位的 1～2 h） |
| after-pv7 | 約 54～64 h |
| after-re-evaluate | 約 73～102 h（D158、D159） |

停止規則：每批開始前寫出估計；實際超過 1.5 倍，或單一問題累計超過 2 小時，就停下回報。

## pv7-line 各批

同一條 PR 線，Client、Gateway、Match 三個角色在同一個 PR；每批是獨立 commit、各自驗收，最後一次合併。檔位見樹狀總覽。

| 代號 | 範圍 | 驗收（L1／L2／L3） | 工時 | 備註 |
|---|---|---|---|---|
| pv7-design | 一次定完所有 wire 欄位與語意，包括 §2a 的 IPC 送達語意（D94）；產出 `protocol-v7.zh-Hant.md` 草案；IPC 送達語意做 1 次 xhigh 對抗式檢查 | 只改文件／—／使用者核准草案 | 19～20 | 哪個欄位需要新量測才能決定，就停下問使用者。之後再改 wire 要先徵求同意 |
| proto-rename-v7 | `client_v6`／`runtime_v6` 改名為 v7（proto、Go 套件、CMake target、include），版本值仍是 6；照 v6 的先例（`708e877`→`f255bf1`） | 全量 CTest＋Go、digest 不變／—／— | 3 | Architecture Delta：Build Graph 的 target 名稱 |
| version-errors | ProtocolVersion 6→7；版本不符時寫出期望值與收到值（HTTP、IPC、Client）；驗收工具與突變升 pv7；凍結 pv7 版分析器（ingress 的部分守恆式列為事前宣告的例外，到 `edge-analyzers` 重新凍結） | 版本與不符測試、突變、digest 兩樹比較與 golden／短跑次確認新工具重現 v7 基準（只記錄）／— | 12 | 從這批起，凍結的 pv6 工具讀不了新跑次 |
| heartbeat-sync | runtime link 每秒心跳，用獨立計時器；偏移、RTT 最小值、漂移每 10 秒寫進兩端日誌；活性規則（診斷訊框不刷新）；D32 的結構隔離與差分測試 | 測試、突變、digest 不變／同機長跑次，只記錄（含睡眠後的跳變）／— | 9 | 新增 RuntimeEnvelope 訊息 |
| time-echo | Client↔Gateway 的往返時間戳（搭在 Hello／Welcome），RTT 最小值濾波，兩端日誌 | 測試、digest 不變／只記錄／跨機器在 Windows LAN 場次量 | 4 | |
| drop-report | Gateway 依原因累計丟棄數回報 Match；**先照現行出口實作**（含之後會刪除的過渡原因），edge 批刪掉對應程式時一起縮減；優雅關閉送 final、回報日誌 | 測試、golden、突變、digest 不變／注入跑次：Match 收到的回報與 Gateway 寫出的回報依 report_sequence 逐則相等（事前宣告）／— | 13～16 | D49 第二階段。排在 edge 批之前（守住 D90），edge 批的前後對照因此有丟棄回報可用 |
| edge-results-resend | Match 的結果通道改成在 Client ACK 之前以 ≤30 Hz 重送（循環游標），退役補送，拒收觸發送出；Gateway 這時仍有帳本、會冪等合併 | IpcHost 測試（改寫 08b 的「每個裁決只送一次」）、digest 不變／—／— | 7～8 | Client 看到的不變 |
| edge-input-passthrough | ★輸入一個資料包對一則 IPC 訊息、原樣轉送；刪除 `lastResolved` 副本、命令表、路由、拒絕線、已寫出去重、截短、D1、D2；link 改成依到達順序的有界 FIFO；解碼成功就提交活性 | Go 測試、突變；被刪路徑的 ingress v1 鍵照樣輸出 0／事前宣告＋D45：命令證據、替代率、連線品質與逐出（含 Gateway→Match 延遲注入）、Match 的副本計數與負載（host mutex 等待、Tick 晚醒）／— | 20～26 | 事故根因元件在這批刪除。Match 每個 clean-60 跑次要多處理約 3.84k 個重複命令（原寫 1.15 萬是除錯了跑次數，見 gateway-direction），負載上升就停下重評 |
| edge-results-passthrough | ★結果原樣轉送給 Client；刪除 session 帳本、retiredSent、retirementAsked、結果節拍與 A3；Gateway 只記每位玩家上次送出結果的時刻，守 30 Hz | Go 測試、突變／事前宣告＋D45：裁決到 Client 的延遲（含 `edge-results-resend` 的「新裁決最多晚 1 個間隔」，D107）、送出間隔 ≤30 Hz（含 host-ipc 停頓案例）、射擊時序與權威 Cooldown＝0／— | 12～16 | 排在動作直通之前，避免帳本在沒有 A3 時合併請求 |
| edge-actions-passthrough | ★動作批原樣轉送；刪除 link 動作視窗、A5、30 Hz 持有與重送 | Go 測試、突變／事前宣告＋D45：射擊到達與 action_latency、權威 Cooldown＝0、Expired 率、被閘門擋下的點擊數／— | 14～17 | FireGate 的 spread 在這批到 A1 之間會變得過度保守 |
| edge-analyzers | 改寫 ingress 分析器的 Gateway 守恆式並重新凍結；同步突變目錄 | 新舊各分析一次／—／— | 5～6 | |
| edge-contract | 把無狀態邊緣的約束寫進 `network-architecture.zh-Hant.md` 的 `Design Invariants` 節與 protocol-v7；Go 狀態欄位的白名單守衛（遞迴走訪型別、掃描套件變數）與突變 | Go 測試、突變、digest 不變／—／— | 4 | 取代 `gateway-duties`。依內容的判斷靠審查，並由 AGENTS.md 連到條文（事故行動 4，文件會話） |
| edge-rate-limit | edge 系列收尾（D119）：重新測試 K1（Gateway 停頓後的固定窗口限速丟棄），對照 `edge-contract` 的 Gateway 狀態白名單評估方案 (a)（token bucket），採用就實作；然後跑一次涵蓋 `edge-actions-passthrough` 與 K1 的 L2（新宣告，兩棵樹帶同一個限速修正） | 重放測試、突變／事前宣告＋D45／— | 8～12 | K1 的證據在 HANDOFF 規劃紀錄 2026-10-18 05:02 條目 |
| cause-records | dwell（一對一）、Client 的 GYOP 序號（只作診斷，D96）、snapshot 的兩段停留、動作拒收回覆（ActionResults）、Leave 原因 | 測試、突變、digest 不變／只記錄／— | 8～9 | 有了對時與回聲才能判斷原因 |
| hitbox-fields | 命中部位一律填 Body；射手看到的時刻由 Client 照實填；加 A1 的 E、S 與影子紀錄（D-2）；Match 驗證並記錄，不改判定 | 編解碼與驗證測試、digest 不變／只記錄／— | 8 | |
| pv7-checkpoint | A1 前的整體回歸：四平台 L1、digest；先扣掉三個 edge 批事前宣告的差異帶，再用凍結 pv7 工具和 v7 基準比較；產出 K 與 S 取法的影子資料 | 全量／事前宣告＋D45／— | 8 | 宣告必須把影子紀錄的對接寫成判定（D127）：對接鍵在 (player, E, life, S) 之外加上連線或時間順序，處理重連；對接率：每個 on_time 路徑的裁決都要對得到 resolved 事件，對不到就是工具無效 |
| a1-shot-step | ★Match 在命令步 T(S) 判定射擊，趕不上時有界退回到達判定；Client 改用序號閘門，刪除 FireGate；combat golden 依宣告重產；K 的值交使用者決定 | 測試、突變、golden 依宣告更新／事前宣告＋D45：clean 下權威 Cooldown＝0、退回率／使用者的連點手感 | 20 | Architecture Delta：FireGate 退役、射擊判定語意改變。命中回饋可能慢約 20～37 ms〔推論〕 |
| pv7-merge | 四平台 L1 後合併；同步 HANDOFF、README | 四平台／—／— | 2 | |

## after-pv7 各批

順序是使用者排的（D90、D91）：日誌契約先做，D44 回歸與觀測工具放在 re-evaluate 之前，因為評估要靠它們產生的數據。

| 代號 | 範圍 | 驗收（L1／L2／L3） | 工時 | 備註 |
|---|---|---|---|---|
| tt2-trace | Engine 的 `GYO::Trace`：有上限的紀錄 sink、通知喚醒的背景寫檔、Base 的日誌 facade（[Engine 計畫](../../../architecture/plans/time-threads-trace/README.md) TT-2） | Engine 測試、突變、digest 不變／寫檔不進熱路徑：每筆紀錄的生產者成本 P99 × 每 Tick 最多 8 筆 ≤1 ms、host 鎖等待的粗閘門、移動延遲不退步（D140；原本的「射擊 P99 增量 ≤1 ms」跨建置不能比較，見 [tt2-trace](tt2-trace.md) 第 11 節）／— | 10 | Engine 子系統和第一個消費端（`log-contract`）同一個 PR |
| log-contract | 日誌行的版本與驗證規則：08c 行族 v1 的去留、D58 暫定項的確認、原第 10 批的紀錄（拒絕原因、snapshot 年齡、Match 的裁決與結束紀錄等） | 跨語言 golden、凍結分析器仍可讀／只記錄／— | 16 | 紙上 6 h＋實作 10 h。日誌格式是 Data Contract |
| dead-letter-position | 確認 D57 是否仍成立：ingress 日誌和丟棄回報維持兩份契約、對齊同一套原則，或合成一份 | 只改文件／—／使用者決定 | 2 | |
| d44-regression-pv7 | pv7 上線後的 D44 回歸，範圍涵蓋丟棄回報、三個 edge 行為批與 A1；加入持續延遲（x1、ramp）的樣本 | —／事前宣告＋D45 檢查／— | 10～14 | 事故後遺症 |
| incident-close | 兩份事故紀錄（2026-10-10 悄悄丟命令、2026-10-15 Gateway 狀態漂移）的行動項目表更新、狀態改為結案 | 只改文件／—／— | 3～5 | 在 `drop-report`、`edge-contract`、`d44-regression-pv7` 之後，而且設計事故的行動 4（AGENTS.md）已由文件會話處理 |
| observability-tools | 相位決策寫進 trace（觀測缺口 3）；late 修正改用增量計數、加寬 D44 偵測器；網路案例換不同的遺失種子；Gateway→Match 延遲注入改成正式模式 | 工具的單元測試、新舊各分析一次／—／— | 10～13 | re-evaluate 的前提 |
| re-evaluate | 用 pv7 之後的數據重新評估：相位追蹤（B6～B9）、延遲位移（A4）、偶發 epoch 重設（A5）、連線品質的參考年齡等；交出建議，由使用者排序 | —／—／使用者排序 | 3 | 缺工具時直接告訴使用者 |

## after-re-evaluate（D158）

INVENTORY 處置為「re-evaluate 之後」的 37 項，加上 HANDOFF「未結事項」補列的 6 項，逐項判斷還需不需要繼續（[after-re-evaluate.md](after-re-evaluate.md)）：還需要 20 列，排成上面樹狀總覽的 15 批；交給 v8 4 項（B10、N-correct-invariant-assert、E3、E5）；有條件 3 項；取消、已解決、已被取代共 15 項。順序依使用者的優先序：系統與引擎 → 觀測工具 → 測試 → 發行。各批的內容、驗收、估時與決策點見該文件第 9 節。

舊的分組（phase-tracking-golden、smell-cleanup、observability-rest、platform-fixes 等）由這份判斷取代；docs-close 的 STABLE_BASELINE 已寫（D157）、v7 略過的驗收不補（D157），系列紀錄（G2）還沒決定。

## 跨分支依賴

樹狀總覽只表達層級；下面是跨分支的依賴。

- `version-errors` → pv7-line 之後所有的 L2（凍結 pv7 版分析器）
- `pv7-checkpoint` → `a1-shot-step`（行為改變排在整體回歸之後）
- `drop-report` → `d44-regression-pv7`（pv7 合併之後）
- `drop-report` → 三個 edge 行為批（edge 批的前後對照要有丟棄回報可用）
- `edge-results-resend` → `edge-results-passthrough` → `edge-actions-passthrough`（結果先、動作後）
- `drop-report`、`edge-contract`、`d44-regression-pv7`、事故行動 4（文件會話）→ `incident-close`
- `tt2-trace` → `log-contract` 的實作、`sdl-isolation`（IP-3 用到日誌 facade）、`cxx-flag-removal`
- `d44-regression-pv7`、`observability-tools`、`log-contract` → `re-evaluate`
- `time-echo` → `lan15-windows`、系列紀錄第 ⑤ 集
- `re-evaluate` 決定修正相位追蹤 → 先做 `phase-tracking-golden`（D158：B10 跟 B6 交給 v8）
- `sdl-isolation-plan` → `sdl-isolation`
- `worker-probe-repair` → `token-tests`、`client-path-tests`
- `contract-readers`、`trace-no-overwrite`、`windows-long-path`、`quick-close`、`wake-reconnect` → `lan15-windows`

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
| after-re-evaluate | sdl-isolation 兩批一條、系統其餘＋觀測＋測試（含 worker-probe-repair）一條、發行一條（D159） |

PR 內只改 `docs/` 的推送沿用 L1（docs-increment）。合併前確認四平台 L1，用 `gh pr merge --merge`。

## 開發紀錄

每批完成時，做那一批的會話把 dev_log 和該批放在同一個變更提交，並加進這份清單（AGENTS.md §15）。下列依批次順序；路徑相對於本目錄。

- [pv7-design：pv7 的契約一次定完，Gateway 回到無狀態邊緣](../../../dev_logs/2026_10_16_pvp_v71_pv7_design.zh-Hant.md)
- [proto-rename-v7、version-errors：協議改名為 v7，版本 6→7，版本不符時說清楚](../../../dev_logs/2026_10_16_pvp_v71_version.zh-Hant.md)
- [heartbeat-sync、time-echo：Match↔Gateway 心跳對時，Client↔Gateway 時間回聲](../../../dev_logs/2026_10_17_pvp_v71_clock.zh-Hant.md)
- [drop-report：Gateway 把丟了什麼、為什麼丟，回報給 Match](../../../dev_logs/2026_10_16_pvp_v71_drop_report.zh-Hant.md)
- [edge-results-resend、edge-input-passthrough、edge-results-passthrough：Gateway 回到無狀態邊緣之一，Match 在 ACK 前重送結果、輸入與結果原樣轉送](../../../dev_logs/2026_10_17_pvp_v71_edge_input_results.zh-Hant.md)
- [edge-actions-passthrough、edge-analyzers、edge-contract：動作原樣轉送、守恆分析器重新凍結、無狀態邊緣的約束與守衛](../../../dev_logs/2026_10_18_pvp_v71_edge_actions_contract.zh-Hant.md)
- [edge-rate-limit：限速改成 token bucket（K1），並跑 edge-actions-passthrough 的 L2](../../../dev_logs/2026_10_18_pvp_v71_edge_rate_limit.zh-Hant.md)
- [cause-records、hitbox-fields：原因紀錄（E2、動作拒收回覆、Leave 原因）與 hitbox 欄位、E 和 S 的影子紀錄](../../../dev_logs/2026_10_18_pvp_v71_cause_hitbox.zh-Hant.md)
- [pv7-checkpoint：A1 之前的整體回歸，v7 基準對現在 232 項 0 停止](../../../dev_logs/2026_10_19_pvp_v71_pv7_checkpoint.zh-Hant.md)
- [a1-shot-step＋pv7-merge：射擊照命令步判定，FireGate 退役，pv7-line 合併](../../../dev_logs/2026_10_20_pvp_v71_a1_merge.zh-Hant.md)
- [log-contract：日誌 Data Contract v1，凍結分析器不會靜默讀錯](../../../dev_logs/2026_10_20_pvp_v71_log_contract.zh-Hant.md)（Engine 側的 tt2-trace 見 [Engine TT-2](../../../dev_logs/2026_10_20_engine_tt2.zh-Hant.md)）
- [dead-letter-position＋incident-close：死信維持兩份契約，兩份事故紀錄結案](../../../dev_logs/2026_10_21_pvp_v71_dead_letter_incident.zh-Hant.md)
- [d44-regression-pv7：pv7 上線後的 D44 回歸，290 個 case-run 加長測 0 次](../../../dev_logs/2026_10_21_pvp_v71_d44_regression.zh-Hant.md)
- [observability-tools：相位決策寫進 trace、增量計數、加寬偵測器、遺失種子](../../../dev_logs/2026_10_21_pvp_v71_observability.zh-Hant.md)
- [re-evaluate：用 pv7 之後的數據判斷相位追蹤與 D44 的項目還要不要繼續](../../../dev_logs/2026_10_21_pvp_v71_re_evaluate.zh-Hant.md)
- [worker-probe-repair、sdl-isolation-plan：修好 worker probe，排定 SDL 隔離的計畫](../../../dev_logs/2026_10_21_pvp_v71_probe_sdl_plan.zh-Hant.md)
- [sdl-isolation：產品、probe、產品測試不再直接呼叫 SDL（Engine IP-3／IP-4 與產品側 IP-4，L2 通過）](../../../dev_logs/2026_10_23_pvp_v71_sdl_isolation.zh-Hant.md)（PR #85，`054b08f`；Engine 側見 [IP-3](../../../dev_logs/2026_10_22_engine_ip3.zh-Hant.md)、[IP-4](../../../dev_logs/2026_10_22_engine_ip4.zh-Hant.md)）
- [black-map-observation O1：黑屏分類工具 `black_frame.py`](../../../dev_logs/2026_10_23_pvp_v71_black_map_o1.zh-Hant.md)
- [black-map-observation O2：視窗觀測外掛 `black_map_watch.py`](../../../dev_logs/2026_10_23_pvp_v71_black_map_o2.zh-Hant.md)
- [black-map-observation O3：已知樣本確認觀測，假說 H1（暖機幀）修正後成立](../../../dev_logs/2026_10_23_pvp_v71_black_map_o3.zh-Hant.md)
- [black-map-observation O4：L2 的 10 種案例與產品 Client 的觀測跑次，黑屏只在 timing 類露出](../../../dev_logs/2026_10_23_pvp_v71_black_map_o4.zh-Hant.md)
- [outage-resync：斷線恢復＝重新對時（C13 (b)＋UDP 中斷狀態）](../../../dev_logs/2026_10_23_pvp_v71_outage_resync.zh-Hant.md)（L2 通過、剩 L3；PR #90，分支 `claude/pvp-v71-outage-resync`，合併前連結在本分支上無效）

