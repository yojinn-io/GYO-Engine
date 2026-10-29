# PvP v8 分批計畫

更新：2026-10-29（`v8-plan-sync`：範圍、順序、分批、檔位與引擎，D243～D246）；初版 2026-10-07。Owner：`object_fps_pvp`。
**狀態：`pv8-design` 第 1 版草案完成，等使用者決定（[pv8-design](pv8-design.md)）。**記錄器是 [HANDOFF](HANDOFF.md)。

版本號：v8＝遊戲版本，pvN＝網路協議版本（見 [v6 交接](../v6/HANDOFF.md) D20）。v7.1 停在 pv7；**pv8 在 v8**（D246）。
與前面版本的分工：[v7](../v7/README.md) 交付執行緒、時間、網路；[v7.1](../v7.1/README.md) 交付 pv7 與事故善後（v1.2.0 已發行，D242）；v8 補完一個基礎 FPS 的功能與工具。

## 定位

- **v8 的核心是補完遊戲功能與工具**（D243）。2026-10-29 盤點 origin/master 的程式碼：移動只有走與跳；只有一把武器；沒有音效（Engine 沒有 Audio 模組）；沒有計分與對局結束（無止盡的死鬥）；Esc 直接離開對局；地圖是手寫 JSON，沒有地圖與資產的編輯器。
- **順序**（D244、D245）：① 基礎功能 → ② 體驗優化 → ③ 手感時序（v7.1 交過來的）→ ④ 工具 → ⑤ 流程項目。①② 完成才像一個基礎 FPS；③ 是優化；④ 是 Engine 的穩定；⑤ 排在工具之後。
- **塔科夫方向不在 v8**（D243）：v8 只收集它的玩法需求（下面「網路協議升級」一項），多通道、interest management 等協議升級之後另開。
- **pv8 是一次 wire 變更**（D246，沿用 v7.1 的做法 (a)）：蹲下、衝刺的欄位在 `pv8-design` 一次定完，隨蹲下的 PR 合併；衝刺先填預設值，② 只填行為（ADS 已取消，D251）。⑤ 的對局規則（計分、勝負）**不**進 pv8（AGENTS §2：契約要說出它支撐的功能，⑤ 還太遠），到 ⑤ 時再升一次協議。
- **推導順序**照 AGENTS §2「Feature → Contract → Architecture」。不以玩法為理由在 Engine 加入產品專用的知識（AGENTS §6）。

## 樹狀總覽

格式：「檔位｜引擎｜簡述」。引擎照 [變速箱規則](../../../../agents/effort-gearbox.md)「エンジン（モデル）」：主會話的引擎與檔位由使用者決定，下面寫的是建議；子 agent 的引擎由 Claude 設定，開始前說明。「Opus 5.5」＝目前主會話的引擎；「Sonnet 5.5」用在唯讀偵察與對抗式檢查（對抗式檢查盡量用和提案者不同的引擎）。估時是第一版，誤差可能到 ±50%，每批開始前重估。

```
v8-plan
├── v8-plan-sync ✓                 「medium｜Opus 5.5｜寫本計畫、HANDOFF，記錄 D243～D246」
│
├── ① 基礎功能
│   ├── audio-line                 「PR 線：Engine 音效＋產品音效」
│   │   ├── au1-plan               「high｜Opus 5.5｜建 Engine 計畫夾、定 GYO::Audio 的介面與 Design Invariants」
│   │   ├── au1                    「high（排程局部 xhigh）｜Opus 5.5｜中立混音器、時間戳排程、SDL 後端」
│   │   └── product-sfx            「high｜Opus 5.5｜射擊、命中、受擊、換彈、腳步；資產來源先定」
│   ├── pv8-line                   「PR 線：pv8 契約＋蹲下，三角色同一個 PR」
│   │   ├── pv8-design             「ultracode｜案 3×Opus 5.5、審 1×Opus 5.5、對抗 1×Sonnet 5.5｜蹲下、衝刺的欄位一次定完（ADS 取消，D251）」
│   │   └── crouch                 「high（預測重播局部 xhigh）｜Opus 5.5｜碰撞高度、視角、遠端動畫、權威事前宣告」
│   └── hitbox-line                「PR 線：部位 hitbox（pv7 已有欄位）」
│       ├── hitbox-data            「high｜Opus 5.5｜骨骼→部位形狀的 Data Contract，先手寫 JSON」
│       ├── hitbox-rewind          「high（回溯局部 xhigh）｜Opus 5.5｜Match 的權威姿勢歷史、射擊照開槍時刻回溯」
│       └── hit-rules              「medium｜Opus 5.5｜部位倍率（數值由使用者決定）」
│
├── ② 體驗優化                     「pv8 欄位已在 ① 合併，這裡只填行為」
│   └── sprint                     「high｜Opus 5.5｜衝刺；改變權威結果，事前宣告」
│
├── ③ 手感時序                     「v7.1 交過來；基準在 pv8 合併後重新取得」
│   ├── analyzer-105               「medium｜Opus 5.5｜門檻 105 與視窗 30 改共用常數＋核對測試」
│   ├── phase-golden               「high｜Opus 5.5｜B10：相位追蹤的 golden」
│   ├── correct-assert             「medium｜Opus 5.5｜決定 Correct() 的前提是否改成 assert」
│   ├── b6-plan                    「ultracode｜案 3×Opus 5.5、審 1×Opus 5.5、對抗 1×Sonnet 5.5｜撤回多加的領先」
│   ├── b6                         「xhigh｜Opus 5.5｜B6 實作」
│   └── a4                         「ultracode 規劃＋xhigh｜Opus 5.5｜延遲位移；需要 WAN 資料」
│
├── ④ 工具                         「各自開工時規劃，現在不估」
│   ├── ui-editor-widgets          「high｜Opus 5.5｜按鈕與輸入框元件化，連同大廳；輸入法組字」
│   ├── voxel-map-editor           「ultracode 規劃｜同上｜Editor → Data Contract → Application」
│   └── assets-editor              「ultracode 規劃｜同上｜hitbox 形狀等；接手 ① 手寫的 JSON」
│
└── ⑤ 流程項目                     「排在工具之後」
    ├── match-rules                「ultracode（契約）＋high｜Opus 5.5｜計分、計分板、擊殺訊息、勝負、結算回大廳；再升一次協議」
    ├── spawn-layout               「high｜Opus 5.5｜出生分散與出生保護；事前宣告＋digest」
    ├── pause-menu                 「high｜Opus 5.5｜Esc 開選單，不直接離開」
    ├── display-settings           「high｜Opus 5.5｜IP-5＋解析度、視窗模式、FPS 上限、settings.json（D41、D76）」
    ├── warmup-hidden              「high｜Opus 5.5｜預熱幀不讓玩家看到」
    └── player-look                「medium｜Opus 5.5｜角色顏色與名牌、屍體手上的槍」
```

## 各批明細

| 代號 | 檔位｜引擎 | 估時 | 內容 | 依賴與注意 |
|---|---|---|---|---|
| `au1-plan` | high｜Opus 5.5 | 3～5 h | 依 v7 D28 在開始時建立 Engine 計畫夾；中立混音器與時間戳排程的介面、owner、Design Invariants 與 Replacement Criteria（AGENTS §3） | 依賴 TT-1（v7 完成）與 IP-3（PR #85 完成，2026-10-23）。`engine/asset` 已有 `SoundLoader` |
| `au1` | high（排程局部 xhigh）｜Opus 5.5 | 12～18 h | `GYO::Audio`＋SDL 後端 | 局部 xhigh 用 `xhigh-reviewer`（引擎繼承） |
| `product-sfx` | high｜Opus 5.5 | 8～12 h | 在事件真正發生的地方以 Engine 時間戳觸發：射擊、命中確認、受擊、換彈、遠端射擊、腳步 | 音效資產的來源（v7 D38）開工前決定。L2：`SDL_AUDIO_DRIVER=disk` 30 FPS 連射，起音間隔對應射擊 Tick |
| `pv8-design` | ultracode｜案 3×Opus 5.5、審 1×Opus 5.5、對抗 1×Sonnet 5.5 | 6～10 h | 姿態（蹲下）、衝刺的 wire 欄位與語意一次定完（ADS 取消，D251；設計見 [pv8-design](pv8-design.md)），產出 `protocol-v8.zh-Hant.md` 草案 | 只放 ①② 的功能（D246）。版本不符要明確報錯（同 pv7 的 version-errors） |
| `crouch` | high（預測重播局部 xhigh）｜Opus 5.5 | 16～24 h | 碰撞高度（站起時頭上有牆不能站）、視角高度、遠端動畫；pv8 欄位實作並合併 | 改變權威結果：事前宣告，同機兩樹 digest 比對；三角色同一個 PR |
| `hitbox-data` | high｜Opus 5.5 | 6～10 h | 骨骼對應的形狀與部位標記（通用格式）＋部位語意（產品資料），先手寫 JSON | 通用格式的 owner 在規劃時定（v8 候選表「PvP 骨骼 hitbox」）；④ 的 Assets Editor 之後接手 |
| `hitbox-rewind` | high（回溯局部 xhigh）｜Opus 5.5 | 20～30 h | Match 保存權威姿勢歷史，照射手開槍時看到的遠端時刻回溯；填 pv7 已預留的欄位（v7.1 D87、D89） | 蹲下之後做：蹲下會改變姿勢 |
| `hit-rules` | medium｜Opus 5.5 | 2～4 h | 部位倍率 | 倍率由使用者決定 |
| `sprint` | high｜Opus 5.5 | 8～12 h | 衝刺速度與限制 | 改變權威結果：事前宣告 |
| `analyzer-105`、`phase-golden`、`correct-assert`、`b6-plan`、`b6`、`a4` | 見樹 | 3～4、6～10、1～2、4～6、16～24 h；A4 開工時估 | 下面「v7.1 交過來的項目」 | pv8 改了移動與預測的路徑，B6 與 A4 的基準在 pv8 合併後重新取得 |
| ④ 三項 | 見樹 | 開工時估 | `ui-editor-widgets`（v8 候選表「大廳 UI 的元件化」）、`voxel-map-editor`（未決事項 b）、`assets-editor` | 工具照 AGENTS §5「手動 → Script → Tool」、§8 Data Contract |
| `match-rules` | ultracode（契約）＋high｜Opus 5.5 | 32～48 h | 對局狀態、計分、計分板、擊殺訊息、結束與勝負、結算回大廳 | 要再升一次協議（D246） |
| `spawn-layout` | high｜Opus 5.5 | 10～16 h | v8 候選表「正式的 4 人地圖」 | 改變 2 人的出生位置與驗收幾何 |
| `pause-menu` | high｜Opus 5.5 | 6～10 h | Esc 開選單 | 和 `display-settings` 的選單共用 |
| `display-settings` | high｜Opus 5.5 | 22～32 h | 下面「已決定的項目」的前兩項（D41、D76），含 Engine IP-5 | Engine 計畫 [輸入與呈現](../../../architecture/plans/input-and-present/README.md) |
| `warmup-hidden` | high｜Opus 5.5 | 6～10 h | v8 候選表「預熱幀不得讓玩家看到」 | 驗收可沿用 `black_map_watch.py` |
| `player-look` | medium｜Opus 5.5 | 6～9 h | 角色顏色與名牌、屍體手上的槍 | 只改呈現 |

合計（④ 與 A4 除外）約 195～296 h：① 73～113、② 8～12、③ 30～46、⑤ 82～125（ADS 取消後，D251）。

## PR 線

PR 以功能線為單位、盡量少開；文件同步併進同一條線的 PR。

- ①：`audio-line`、`pv8-line`、`hitbox-line` 三條。`audio-line` 不碰 wire，可以和 `pv8-line` 並行；`hitbox-line` 排在 `pv8-line` 之後。
- ②：`sprint` 一條（ADS 取消，D251）。③：一條（`b6`、`a4` 若太大再分）。④：每個工具一條。⑤：`match-rules` 一條，其餘依開工時的分組。

## 不在 v8 的項目

| 項目 | 去處 | 理由 |
|---|---|---|
| 塔科夫的協議升級（多通道、interest management、差分） | v8 之後另開 | D243：v8 只寫需求 |
| 多武器與射速 | 未排定 | 武器 id 要改 wire，不在 pv8（D246） |
| 受擊方向對多位攻擊者 | 未排定 | 要改 wire，不在 pv8 |
| 建房時在 Client 選地圖 | 未排定 | 要改 wire 與 Match／Gateway，不在 pv8 |
| 單機劇情＋聯機選項 | 未排定 | 規模大，可能另立計畫 |
| 驗收工具的三個問題、品質窗口失敗原因、睡眠後重連、I-3 與 outage-resync P2 的兩個例外 | 等使用者決定（HANDOFF「等你決定」） | 不屬於玩法 |

## 開發紀錄

| 批次 | dev_log |
|---|---|
| `v8-plan-sync` | [2026-10-29](../../../dev_logs/2026_10_29_pvp_v8_plan_sync.zh-Hant.md) |

---

以下是 2026-10-22 版的規劃輸入，內容不變；其中的候選已在上面分到各批或「不在 v8 的項目」。

## 範圍（2026-10-22 版）

- 做：規則、地圖、出生、回合、命中與傷害、角色辨識等直接影響遊玩的調整。
- 不做：v7 負責的架構與平台工作；不以玩法為理由在 Engine 加入產品專用的知識（AGENTS §6）。

## 已決定的項目

- 遊戲內的畫面設定 UI（[v7 交接](../v7/HANDOFF.md) D41，2026-10-09 使用者決定）：玩家在遊戲中選擇解析度、視窗模式與 FPS 上限，包含設定選單、套用後未確認就還原，以及選擇的保存（`settings.json`：版本與驗證規則屬 Data Contract，可以手寫）。
  - UI 與下一項的命令列選項共用限幀與視窗模式機制；兩者的先後在 v8 規劃時決定（v7 的第 13 批原本要先提供命令列選項，已隨 D76 移到 v8）。
  - Engine 的 `UserDataDirectory` 與 `engine/io` 的 `AtomicWriteFile`（原本在 IP-5）隨這項一起做。
- 解析度、視窗模式與 FPS 上限的選擇（[v7 交接](../v7/HANDOFF.md) D76，2026-10-14 使用者以「照建議」核准；原 v7 的 P5＝IP-5＋第 13 批，v7 任務 6）：
  - 內容（v7 的原計畫）：玩家以產品 Client 的命令列選項選擇解析度、視窗模式與 FPS 上限（例如 `--resolution`、`--window-mode`、`--fps <上限>`）；指定的值不能用時退回預設並記一行日誌；HUD 依解析度縮放，移除寫死的 1280×720；L1 含選項解析與限幀的節拍；L3 含 30 FPS 的手感（v7 D41）。細節見 v7 交接「P5：解析度與 FPS 的選擇」。
  - Engine 的部分是 IP-5（顯示器與模式列舉、視窗模式、像素密度），見 Engine 計畫 [輸入與呈現](../../../architecture/plans/input-and-present/README.md)。
  - 理由（D76）：這是玩家功能，設定 UI 與保存已經在 v8（D41），一起做比較自然。
- 音效（[v7 交接](../v7/HANDOFF.md) D76，2026-10-14 使用者以「照建議」核准；原 v7 的 P6＝AU-1＋第 14 批，v7 任務 5）：
  - 內容（v7 的原計畫）：Engine 新增 `GYO::Audio`（中立混音器、時間戳排程）＋SDL 後端（AU-1，Engine 計畫夾在開始時建立，v7 D28）；本產品在事件真正發生的地方以 Engine 時間戳觸發射擊、命中確認、受擊、換彈、遠端射擊的聲音；L2 以 `SDL_AUDIO_DRIVER=disk` 在 30 FPS 連射，檢查起音間隔對應射擊 Tick。細節見 v7 交接「P6：音效」；音效資產的來源見 v7 交接 D38（待確認）。
  - 理由（D76）：和網路與重構無關；使用者決定音效不是 v7 的發行條件。
  - 依賴：AU-1 需要 TT-1（v7 已完成）與 IP-3。IP-3（SDL 隔離）原本移到 v7.1，但沒有在 v7.1 完成，目前在 v7.1 的「re-evaluate 之後」那組（[v7.1 穩定基線](../v7.1/STABLE_BASELINE.md)），尚未排定。

- 網路協議升級（需求；2026-10-25 使用者，v7.1 期間的技術討論）：
  - 需求：「迷你版逃離塔科夫」要有背包、裝備、人物屬性、PvP、PvE（Boss 戰、小怪）、物品收集。現在的 pv7 契約滿足不了，協議必須升級（新的 pvN）。
  - 為什麼 pv7 不夠：每個 Tick 送一份完整 snapshot、必須塞進一個資料包（下面「預計壓力」第 4 項），而且每個 Client 都收全部實體。玩家維持 4 人、只把地圖變大還撐得住；小怪、投射物、地上的物品會讓 snapshot 塞不下；背包只該給本人看，不該送給所有人。
  - 討論出的方向（未定案，v8 規劃時再決定）：
    - 多個邏輯通道，底下都走 UDP；需要可靠的通道在 UDP 上自己做編號、ACK、重送、去重（現有的動作通道就是這種做法，可以當樣板）。
    - 高頻狀態（玩家、怪物、Boss 的位置）：每個 Client 一份、只放附近的（interest management），再加差分。
    - 世界事件（物品生成、被撿走、Boss 換階段）：可靠的事件串流，只在變化時送。
    - 私人狀態（背包、裝備、屬性）：只送給本人，變化時才送。
    - 玩家動作（撿物品、使用道具、攻擊）：擴充現有的動作通道。
    - 各通道依需要定頻率；Tick 維持 60 Hz，不以提高 Tick 率解決容量問題。
    - 權威只有一個擁有者：所有判定留在 Match 的模擬執行緒；I/O 執行緒只寫輸入佇列、讀輸出佇列（AGENTS §3「狀態的權威」）。
  - 要使用者決定的架構點：可靠串流與差分基準需要記狀態，放在 Match 還是 Gateway。放在 Gateway 會讓它變成有狀態，違背 Gateway 原本「無狀態邊緣」的設計。
  - 前提：照「功能 → 契約 → 架構」（AGENTS §2），先寫具體的玩法需求：同時存在的小怪、投射物、地上物品的數量；背包與裝備的格數與內容；人物屬性；Boss 的規模；PvP 與 PvE 的組合方式。目前只有「迷你版逃離塔科夫」這個方向，沒有具體需求。
  - 相關：下面「預計壓力」第 4、5、6 項，「未決事項」a。

## 規劃方式

- v8 開始時依變速箱規則規劃：先列出要做的玩法項目並由使用者排序，再分批。
- 玩法調整多半會改變權威結果或協議。沿用 v6 的做法：事前宣告預期變化，用同機兩樹的 digest 比對證明，改 wire 時三個角色同一個 PR。
- 手感類的項目以 L3（使用者實際遊玩）為主要驗收；可量化的部分（例如重生後多久被擊殺）先定義指標再調整。

## 候選（未定案；來源與現況）

| 候選 | 來源 | 備註 |
|---|---|---|
| 正式的 4 人地圖：出生點分散（例如四角）、出生保護 | v6 第 16 批（D23③）：為了不改變 2 人的權威結果與驗收幾何，第 16 批把新出生點放在既有兩點之間的線段上，4 人時容易出生即被擊殺 | 會改變 2 人的出生位置與驗收幾何，需要權威事前宣告與驗收工具改寫。使用者（2026-10-15，v7 第 16 批 L3）：重生點沒有保護；玩家不移動、佔著重生點時，之後加入或重生的人可能疊在一起。很多 FPS 都有這種情況，不確定算不算問題，放到 v8 看要不要處理。 |
| 多武器與射速 | 使用者（2026-10-15，v7 第 16 批 L3-3）：「連擊手感好了很多，但是不知道後面如果換武器就不知道了」 | v7 的手感改善來自網路與時序，不是槍械參數（`cooldownTicks` 10 不變）。v7.1 採用 A1：射擊照命令步判定，v7 的本機射擊閘 FireGate 已退役（[v7.1 交接](../v7.1/HANDOFF.md) D90、D137；推導經過見 [v7 第 09 批](../v7/09-firegate-c2.md)），換武器不必重推它的常數。A1 的冷卻保留上限 K＝min(K_max, cooldownTicks)、K_max＝2 Tick，射速改成資料驅動也成立（D136，[聯網架構](../../network-architecture.zh-Hant.md) §7「射擊判定的冷卻保留與命令步」）。做多武器時要先定兩件事：(1) 武器資料的權威在 Match（載入、驗證，經 `CombatRules` 告訴 Client），Client 本機的資料不得決定射速；(2) 射速的單位建議直接存 Tick（60 Hz 下只能是 3600÷Tick 發／分，例如 MP5 的 800 發／分只能取 4 或 5 Tick），若存發／分要把換算與取整寫進契約。武器 id 等欄位不在 pv7（D87），要改 wire 先徵求使用者同意。全自動武器上線時，用「故障情境＋全自動」的量測重新評估 K_max。來源：[v7 STABLE_BASELINE](../v7/STABLE_BASELINE.md) 已知限制第 10 項 |
| 建房時在 Client 選地圖 | v6 第 17 批（D25）：Client 已能安裝多張地圖並依 Match 選擇；建房時選地圖還需要多房間或換地圖的 Match／Gateway、建房請求帶地圖 id（協議變更）與大廳選單 | 第 17 批的多地圖安裝是前提 |
| 角色外觀區分（顏色、名牌） | v6 第 16 批的已知限制：4 人時遠端外觀相同 | 呈現為主；若需要隊伍資訊則牽涉 wire |
| CS 式開局／回合準備期 | v6 交接第 9 項（候選）：全員凍結、無敵、倒數 | 需要 Match 的回合狀態與契約變更 |
| 屍體手上的世界手槍掉落或隱藏 | v6 交接（候選，`PlayerPresentation.cpp`） | 呈現 |
| 受擊方向對多位攻擊者的處理 | v6 第 16 批的已知限制：只指向最後一位攻擊者 | 第 09 批的受擊欄位只有最後攻擊者；更多資訊需要改 wire |
| PvP 骨骼 hitbox、爆頭倍率 | 列在 [v7 候選](../v7/README.md)（2026-10-07 使用者提出） | pv7 只做了 hitbox 的契約支持（命中部位、射手開槍時看到的遠端時刻等欄位，目前填等同現行的值；[v7.1 交接](../v7.1/HANDOFF.md) D87、D89）。hitbox 的實作在 v8，包括 Match 端權威姿勢與射擊回溯；玩法規則（部位倍率等）也在 v8 決定。骨骼對應的形狀與部位標記屬通用格式（Engine 計畫，Assets Editor 編輯），部位倍率等產品語意留在 PvP 的資料 |
| 單機劇情＋聯機選項 | v6 交接（v7 之後的產品方向候選） | 規模大，可能另立計畫 |
| 大廳 UI 的元件化：按鈕與位址輸入框做成元件，並修改 ui_editor | 使用者（2026-10-22，v7.1 `sdl-isolation` 的 ip3f L3）：輸入「:」時忽然卡住（「不知道是不是輸入法的問題」），「操作手感怪怪的」；「v8排一個任務，組件化這些按鈕和輸入框，順便修改uieditor」 | 現況：大廳的位址輸入在 `PvpApplication.cpp` 內手寫（`HandleTextEditing`、`AppendAddress`），按鈕也是產品自己處理。規劃時要定：(1) 元件放在哪個 owner（Engine 的 `engine/ui` 與 ui_editor 是共通層，不得帶產品知識；輸入框的編輯政策目前屬消費端，[input.md](../../../architecture/input.md) Design Invariant 4）；(2) 輸入法的組字（SDL 的 TEXT_EDITING，目前不顯示，推論是「:」卡住的原因，未重現）；(3) 手感的另一個來源是 IP-2 已知的 drawable 取得停頓（約 66 ms，修改前也有），屬執行緒分離，不在這個任務 |
| 預熱幀不得讓玩家看到（需求） | 使用者（2026-10-23，v7.1 `black-map-observation` 之後）：「這個的確是需求，未來我建立大地圖之後，這個問題可能會被放大」；「預熱是需要的，問題在於它暴露給了使用者」 | 現況：`InitializeGraphics` 的暖機迴圈（`PvpApplication.cpp:912-923`）以一般的 `Renderer::Render` 取得、繪製並送上螢幕，暖機幀背景純黑（清除色 `{0,0,0,1}`）、沒有地圖與 HUD，只有人物與武器。產品在暖機後直接進入迴圈，實測（O4，每 0.5 s 截圖，連續啟動 5 次）看不到；GUI 測試的 timing probe 會讓它停在螢幕上約 2.5 s（v7.1 決定只記錄、不改 probe）。地圖變大、預熱變長之後，玩家看到的時間可能變長。規劃時要定：(1) 需求本身（可見的上限，例如 0 幀，或改成有意義的載入畫面）；(2) 修法（暖機用正常的清除色；第一個正常幀之前不顯示視窗——macOS 上不可見視窗的 drawable 取得可能卡住，IP-2 的已知風險；或顯示載入畫面）；(3) 驗收可以沿用 `black_map_watch.py` 與 `black_frame.py`。和下面「預計壓力」第 3 項（同步載入）、第 8 項相關 |

## v7.1 交過來的項目（2026-10-21，v7.1 `re-evaluate`，D154、D155；`after-re-evaluate`，D158）

v7.1 的 `re-evaluate` 用 pv7 的數據判斷為「還需要」、交給 v8 的項目。細節與證據在 [v7.1 re-evaluate](../v7.1/re-evaluate.md) 與 [v7.1 after-re-evaluate](../v7.1/after-re-evaluate.md)（標 D158 的 4 列）。

| 項目 | 為什麼還需要 | 規劃時要注意 |
|---|---|---|
| B6：延遲結束後撤回多加的領先（**重新規劃**，不沿用 09b 的 S／M） | 一次 45～60 ms、持續 150 ms 的延遲，會留下約 +25 ms、約 4 秒的延遲（pv7 補量 4/8 個單位）。原因是 late 修正之後的重新擷取在 slew 結束時就決定，用到的還是延遲期間的樣本 | 09b 的 S／M 估計到不了 150 ms 的邊界。先定義玩家可見的目標（撤回時間、不增加 held 與重設），用 schema 3 的 `phase_decision` 補齊「放出相對 slew 結束」的曲線，再比較候選（S、M、放出後依正值樣本撤回、縮短 late 後的決定視窗）。相位追蹤不在 digest 裡，先做 phase-tracking-golden。時序邏輯，規劃用 ultracode 或 xhigh（先問使用者）。09b 1-7 第 3 點一起處理。**v7.1 outage-resync 之後（PR #90）**：B6 的基準要在它合併後重新取得。它改了相位追蹤的入口與 slack 的來源：被替代序號只存一份、保留 600 序號；晚到超過 N_o（333,333 µs）的第一份副本不產生樣本，改成 Resync；恢復時 `LocalPlayerPrediction::Resume`（`resume_reset`）。它也留下兩個交給 B6 的已知例外：只上行中斷後，以及 Match 停頓後恢復時，多出的領先量要等 Backlog 清掉（network-architecture §7「連線中斷與重新對時」的已知項目）。`Correct()`、`reacquiring_`、deadband、p90 視窗沒有改 |
| A4：延遲位移（產生→執行約 38 ms） | 玩家感受得到；A1 之後射擊也照命令步判定。手感上的取捨，不是缺陷 | 改領先會改 backlog 穩態（30 Tick 合計 60→30），碰到 D44 的停止條件；需要 WAN 資料與 phase-tracking-golden |
| N-analyzer-literal-105 | backlog 門檻 105 與視窗 30 在 Match 與 4 個分析器的 6 處手動維持（AGENTS.md §9） | 改成共用常數，加從 `Movement.hpp` 解析並核對的測試；動到凍結分析器，下一個 L2 重新凍結（v7.1 analyzer-converge 後：`quad_evidence.py` 的那一處隨 `command_metrics` 刪除，分析器剩 `command_evidence.py` 2 處、`gameplay_evidence.py` 2 處、`gameplay_soak_evidence.py` 1 處） |
| B10：相位追蹤的 golden（D158） | 權威 digest 的 `RunLink` 不經 TrackSlack，35 個情境的相位樣本都是 0，相位追蹤改壞了沒有測試會失敗 | 排在 B6 之前（即上面的 phase-tracking-golden） |
| N-correct-invariant-assert（D158） | `Correct()` 的前提（前一次修正已 slew 完）只寫在註解，D68 ① 要不要改成 assert 一直沒決定 | B6 動到 late 修正之後的流程，動手前決定 |
| E3：量測工具的時間基準統一（D158） | 09b 的「70～87 ms 邊界」用 t_frame 量，凍結的 `measure.py` 用 t_c，兩者差 13.7～24.1 ms | 只在 B6 要拿 09b 的數字對照時需要；只用 `phase_decision` 的時刻就不需要 |
| E5：probe 起點的幀相位（D158） | probe 起點綁在 join 之後，不能跨建置比較 b、a＋b、`legal_*` | A4 改領先時做前後對照才需要 |

v7.1 決定取消、不交給 v8 的：B7（slack 到達即發布）、A5（backlog 105 與 300 ms 型重設：照設計保留，連同門檻 105 重推與快速正向回收）。重開條件見 v7.1 `re-evaluate.md`。

## 大地圖與塔科夫類玩法的預計壓力（未觀測，規劃輸入）

- 2026-10-09 使用者在側聊提出：之後預計做 GridMap 與「迷你版逃離塔科夫」。
- 以下都是**預計的壓力，還沒有觀測到**。依 AGENTS §2，開工前要先量到實際的壓力才動手；本節只記錄，不分批、不定檔位、不改計畫。
- v8 的範圍仍然不做 v7 的架構工作。但這些壓力是玩法帶來的架構需求，開工時架構的部分可能另立 Engine 計畫或批次。
- 目前 v7 的三角色分離（主、模擬、網路）不需要改。最先出現的壓力多半是演算法問題（O(N) 的寫法），不是執行緒的數量。
- 程式證據的行號以 2026-10-09（`claude/pvp-v7-p1b`）為準，開工時重新核對。

### 1. 碰撞是線性掃描

- 程式證據：
  - `apps/object_fps_pvp/src/Collision/CharacterCollision.cpp`：`MoveCharacterBody`（:69）先做最多 8 輪的穿透修正（:76），每輪掃過全部 `walls`（:83）；再做最多 5 輪的掃掠（:103），每輪也掃過全部 `walls`（:119）。`CanPlaceCharacterBody` 同樣逐面檢查（:59）。
  - `apps/object_fps_pvp/src/Pvp/Movement.cpp`：站立判定逐面掃描（:36）；每一步呼叫 `MoveCharacterBody`（:59）。
  - `apps/object_fps_pvp/src/Pvp/ShotQuery.cpp`：射線逐面檢查（:42）。
- 成本落在哪條執行緒：同一段程式跑在三條執行緒。
  - Match 的 Tick：每位玩家每個 Tick 走一步（`PvpMatch.cpp:420` 的 `StepMovement`）；射擊判定（`PvpMatch.cpp:312` 的 `QueryShot`）只在 Match。
  - Client 的模擬角色：每一步（`LocalPlayerPrediction.cpp:309`），以及 Reconcile 時重播待確認的命令（:204）。
  - 主執行緒：`InterpolateLocalPresentation` 每次放置呈現位置都做一次掃掠（`LocalPlayerPrediction.cpp:370`）。
- GridMap 會讓牆從幾十面變成幾千格。
- 修法方向：空間索引，以格子座標直接查附近的格子。這解決的是演算法問題，加執行緒沒有用。
- 開工前要收集的證據：Match 的 Tick 處理時間、模擬角色每一步的時間，和牆數的關係。
- owner 的候選：通用的格子空間索引放 Engine（Collision），地圖資料放產品。

### 2. 繪製每幀逐格、逐牆提交，沒有剔除

- 程式證據：`apps/object_fps_pvp/src/Pvp/PvpApplication.cpp` 的地板每一格送一次繪製（:566-579），每面牆送兩個 box（:580-586）。
- 成本落在哪條執行緒：全部在主執行緒。
- 修法方向：靜態格子按區塊合併成網格或改用 instancing，再依區塊剔除。舊的 `object_fps` 已有 `MapGeometryGenerator`（`apps/object_fps/src/Rendering/MapGeometryGenerator.cpp`），可以參考。
- 開工前要收集的證據：主執行緒的 prepare／render 時間、`longest_frame_ms`。
- owner 的候選：區塊網格的建構可以升到 Engine；地圖內容與配色留在產品。

### 3. 同步載入卡住主執行緒

- 程式證據：`engine/asset/include/engine/asset/AssetManager.hpp:36-37` 的 Async 只是擬似 async：在主執行緒的 `Update`（`engine/asset/src/AssetManager.cpp:25-27`、`:226-232`）每幀處理 `maxLoadsPerFrame = 2` 件；`engine/asset/` 沒有任何同步保護（沒有 mutex）。產品在 `PvpApplication.cpp:941-942` 每幀呼叫 `BeginFrame`／`Update`。
- 成本落在哪條執行緒：主執行緒。地圖和資產變大之後，同步載入會讓事件 pump 停好幾秒，macOS 會判定視窗沒有回應。
- 修法方向：背景 worker 做讀檔、解壓、解碼，主執行緒在 `Update` 收完成的結果。交給 GPU 的上傳要先查證 SDL GPU 的執行緒規則。這就是 [v7 候選](../v7/README.md)中「非同步資產載入（Job 的第一個實際需求）」的具體化。
- 開工前要收集的證據：載入時的主執行緒停頓時間、資產大小與件數。
- owner 的候選：Engine（Asset）。

### 4. snapshot 的大小

- 程式證據：UDP 一個封包上限 1200 bytes（`apps/object_fps_pvp/include/RetroFPS/Pvp/Wire.hpp:32`、`apps/object_fps_pvp/protocol/README.md:19`）；現在每個 snapshot 都帶全部玩家。
- 成本落在哪條執行緒：Match 組 snapshot、網路角色收送；主要是頻寬與封包上限。
- 修法方向：加入 AI、戰利品、門之後，需要 interest management（只送附近的實體）與差量壓縮。這是協議變更（新的 pvN）。
- 開工前要收集的證據：snapshot 的大小分布與實體數的關係。
- owner 的候選：產品（協議與 Match）。

### 5. Match 的邏輯負擔

- 內容：AI 在格子上尋路（A*）、戰利品與背包的權威、撤離計時。
- 成本落在哪條執行緒：Match 的 Tick 執行緒。
- 修法方向：尋路交給 worker，結果在指定的 Tick 套用，維持權威的決定性。伺服器端最可能在這裡第一次需要 Job 系統。
- 開工前要收集的證據：Tick 處理時間與 AI 數量、尋路次數的關係。
- owner 的候選：玩法規則在產品；Job 系統在 Engine（建在 Threads 上）。

### 6. 跨局保存（倉庫）

- 內容：需要後端服務與 Data Contract（版本、驗證規則），和執行緒無關。
- 開工前要收集的證據：保存的資料範圍與一致性要求（玩法決定之後才能定）。
- owner 的候選：產品（服務與 Data Contract）。

### 7. Render 執行緒的拆分

- 時機：排在第 2、3 項之後；主執行緒量到長幀才考慮。
- 限制：SDL 3.4 規定 swapchain 的 claim／acquire／present 與事件 pump 只能在視窗執行緒，所以能拆的只有錄製：render 執行緒畫到離屏貼圖，主執行緒負責事件、貼到 swapchain、present。
- 代價：多一次拷貝，延遲可能多一幀；command buffer 能否跨執行緒錄製要先查證。
- 開工前要收集的證據：第 2、3 項處理後，主執行緒仍有的長幀與其內容。
- owner 的候選：Engine（Render）。

### 8. 預熱與載入期間的畫面

- 2026-10-23 使用者提出：大地圖之後，預熱幀被看見的問題可能會被放大（候選「預熱幀不得讓玩家看到」）。
- 程式證據：暖機幀由 `InitializeGraphics` 送上螢幕（`PvpApplication.cpp:912-923`）；暖機之後若有長時間不 Render 的工作（同步載入大地圖、建構網格），螢幕會一直停在最後送出的那一幀，這是作業系統的正常行為。
- 觀測：v7.1 `black-map-observation`（[計畫](../v7.1/black-map-observation.md)）證實 GUI 測試看到的黑屏就是暖機幀，產品目前看不到。
- 開工前要收集的證據：大地圖的預熱與載入時間；暖機後到第一個正常幀的時間（`black_map_watch.py` 可直接量）。
- owner 的候選：產品（何時、顯示什麼）；如果需要「先畫載入畫面再載入」的通用流程，可能牽涉 Engine 的 Runtime／Render。

### 未決事項（由使用者決定）

- a. 迷你塔科夫是新產品，還是 `object_fps_pvp` 的演進？如果是新產品，`ClientSimulation`、`MatchRuntimeHost`、三角色編排、碰撞索引都會有第二個使用者，符合 [v7 README](../v7/README.md) 寫的 framework 層啟動條件，也牽涉 Product Boundary（AGENTS §7）。
- b. GridMap 要沿用舊的 2D 格子，還是重做成在 3D 格子上擺放網格模型（類似 Godot 的 GridMap）？
  - 舊的 2D 格子：`object_fps`、`object_fps_v2` 的 `GridMap`／`GridMapLoader`／`GridCollision`／`MapGeometryGenerator`（這兩個產品在 `engine/config/projects.csv` 都是 `enabled=0`）；`apps/object_fps_pvp/include/RetroFPS/World/GridMap.hpp` 也有一份。
  - 3D 格子需要編輯器，要走 Editor → Data Contract → Application（AGENTS §8）。
- c. 建議的 owner（只是建議，未決）：GridMap 先放在產品內，只把通用部分（格子空間索引、區塊網格的建構）升到 Engine；等第二個遊戲用同樣的方式組合，再抽 framework。

## 依賴

- 起點是 [v7.1 穩定基線](../v7.1/STABLE_BASELINE.md)（2026-10-21）：v7 的執行緒分離與時間服務、v7.1 的 pv7 都已完成，手感相關的時序不會再因它們改變。
- 第 16 批之後房間是 4 人；v8 的玩法以 4 人為前提。
- 上面「預計壓力」的修法多數建立在 v7 的 Time／Threads 之上，例如 Job 系統建在 Threads 上，worker 數＝核心數扣掉角色執行緒。
