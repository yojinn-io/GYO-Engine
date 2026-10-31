# hitbox-line 規劃（v8-product-hitbox-plan）

更新：2026-10-31。Owner：`object_fps_pvp`。**狀態：草案，等你決定（下面「等你決定」的 13 項）。** 起因：`v8-product-hitbox-plan-ultracode`（[HANDOFF](HANDOFF.md)）。已定的前提：`v8-product-hit-rules-v2-values`、`v8-product-hit-rules-limb-wire`、`v8-product-hit-rules-permille`。

## 過程

- ultracode（run `wf_7b0fc672-291`，約 20 分，7／7 個 agent 回傳）：2 位唯讀偵察（Sonnet 5.5，medium：Match 端、動畫端）→ 3 個方案（Opus 5.5：還原 v2、決定性與風險優先、最小契約優先）→ 1 位審查（Opus 5.5）→ 1 次對抗式檢查（Sonnet 5.5，high）。
- 第一次 run（`wf_c70a45ea-cdc`）在偵察中途停下：使用者轉來「其他遊戲怎麼處理 hitbox」的參考（CS、PUBG／Fortnite、Overwatch、Valorant），加進方案、審查、對抗式檢查的 prompt 後重跑；該參考出自另一個 agent 的知識、沒有逐一查證，只當參考。
- 評分（滿分 30）：最小契約 25、決定性 23、還原 v2 14。以「最小契約」（B-flat）為基底，移植「決定性」的倍率驗證、分步 digest、平手規則，與「還原 v2」的量測批、合成 fixture、Replacement 路徑。
- agent 的原始輸出留在會話的工作檔（`hitbox-plan-result.json`），不放進 repo。**全部是設計，沒有實作。**

## 結論

### 做法：B-flat（固定姿勢的 11 段骨骼膠囊）

- 每種姿態（站、蹲）各一張 11 段的形狀表，以身體座標（腳底為原點、+z 朝前）手寫成 JSON，`assets/object_fps_pvp/hitbox.json`，由 PvP 持有。每段 `{id, part: head|body|limb, a, b, radius}`，`a＝b` 是球。
- Match **不**連 `GYO::Model`，不讀 FBX、動畫或骨骼名；判定只用權威的位置、yaw、`crouched`、`lifeState`，加上 Match 自己的有界姿勢歷史（回溯用）。執行期的運算量和現在的單顆膠囊同級，射線用既有的 `Engine::Collision::RaycastCapsule`（`Collision.hpp:65-70`），不需要新的引擎基元。
- 為什麼不選 A（Match 取樣完整動畫）：要把 `GYO::Model`、UFBX、約 23.8 MB 的動畫帶進 Match，並把 Client 呈現層依路徑累積的步伐相位（`PlayerPresentation.cpp:178-304`）重新定義成權威狀態；Slerp 用到 acos／sin，含命中的情境就離開跨平台 golden。pv7 §7 也寫明命中幾何只讀權威狀態、呈現動畫不參與。
- 為什麼不選 A′（權威步態相位上 wire＋離線烘焙表）：三種 Architecture Delta 疊在一起（估 70～100 h），而「畫面和判定的落差」還沒量過；在量不到的問題上排大改造，就是 v7.1 估到 400 h 的教訓。A 與 A′ 寫進 Replacement Criteria，量到落差才提案。
- 為什麼不選 C（部位簡化形狀）：四肢對不上 v2 的 8 段 0.75。

### 玩家會看到的

| 部位 | 單發（基本 25 點＝25000） | 擊倒發數（100000） |
|---|---|---|
| 頭 | 50000 | 2 |
| 軀幹、骨盆 | 25000 | 4 |
| 四肢 | 18750 | 6 |

- HUD 顯示 `ceil(hp/1000)`（建議），活著時不會顯示 0。受擊動畫：HEAD 用 `Hit_Head`，BODY 與 LIMB 用 `Hit_Chest`。
- **落差（推論，未量）**：跑動時畫面上的手腳在擺動，判定停在參考姿勢；跳躍縮腿、上身隨 pitch 前傾都不進判定。頭與軀幹接近身體軸，落差較小。
- **輪廓變窄（對抗式檢查 should）**：現在的判定是半徑 0.25 的單顆膠囊；改成 11 段之後，輪廓邊緣（軀幹 0.19 之外到 0.25 之間、段與段之間的縫）原本的命中會變成 Miss，命中率會下降。survey 要量 11 段聯集對舊膠囊的覆蓋率。
- **回溯**：打移動中的目標時，判定的是射手畫面上的位置，最多回溯 R 個 Tick；代價是被打的一方可能已躲進掩體才中彈（最多晚 R，且不超過 250 ms）。R 同時是「偏袒射手」的政策上限與濫用上限：惡意 Client 可以固定把時刻填成 J−R。

### 決定性

- 只要求 Match 自己可重現；跨平台逐位元一致只要求 golden 子集（零角度）；不要求 Client 與 Match 逐位元一致。Client 不做命中射線（`src` 內只有 `PvpMatch.cpp` 與 `ShotQuery.cpp` 用到 `QueryShot`／`RaycastCapsule`），所以這個前提不會讓玩家看到預測錯誤。
- **待查（對抗式檢查 should）**：合成 Arena 的出生點 yaw 是 π（`fixtures/arena.json`），剛加入或重生、還沒收到移動命令的目標用自己的 yaw 做形狀旋轉時會用到 libm 的 sinf／cosf。declare 批要逐一檢查 `combat-kill-respawn`、`combat-cross-life`、`combat-rejoin` 是否在這個狀態下射擊，必要時讓 digest 對進入形狀旋轉的目標 yaw 也套零角度檢查。

## Architecture Delta（AGENTS §3，要你同意）

1. **新的產品內 Data Contract**：`hitbox.json`（version 1），owner `object_fps_pvp`，只有 Match 讀；嚴格載入（仿 `MovementRulesFile`：每個鍵必填、不准多鍵、違規就停下 Match）；`--hitbox` 參數；登記 `asset_catalog.json` 與 `test_asset_catalog.py`。載入時檢查站姿各段頂端（`y＋radius`）≤ 1.8、蹲姿 ≤ 1.2（例：頭球中心 1.65＋半徑 0.16＝1.81 會被拒收，要調）；不檢查半徑對 0.25 的關係（手臂可能超出）。
2. **State Authority**：PvpMatch 新增每位 Participant 一個 R＋1 Tick 的姿勢環形緩衝（位置、yaw、`crouched`、`lifeState`、life、epoch）。這是 Match 自己權威的狀態；Client 的 `PlayerLocomotionState` 仍只是呈現，Match 不讀，Client 不上傳姿勢。歷史在 Join、Leave、重置、連線世代換掉時的建立與清除，rewind 批的事前宣告要寫並用測試鎖住。
3. **Design Invariants**：
   - (a) network-architecture 的 Design Invariants 表第 8 列「觀測與模擬契約分開」：`view_tick_q16` 改寫成「pv7 §7 定義的有界判定輸入，只由單一入口讀取、夾在 [J−R, J]」，其他診斷欄位仍不得驅動判定。
   - (b) 新增一列：判定姿勢由 Match 的 hitbox 資料加權威狀態定義，呈現姿勢不參與。
   - (c) hitbox Component 的 Replacement Criteria（在 data 批之前寫好）：每加一種姿態或動作狀態（衝刺、跳躍、持槍）就得新增並重驗一組形狀；survey 量到的四肢最大偏差（公尺）或輪廓覆蓋率超過**事前宣告的門檻**。成立時停下，交你在 A′ 與 A 之間選。條件只用 survey 量得到的量（對抗式檢查 should：「畫面命中、判定未中的比例」目前沒有量測工具，不列為條件）。
4. **守衛**（`test_diagnostic_isolation.py` 的 `test_no_decision_reads_the_view_tick`）：改成掃整個 `src` 與 `include`，`viewTickQ16`／`view_tick` 只准出現在白名單（IpcHost 的解碼、`MatchShotTrace`、`ShotRewind.cpp`、測試）；`ShotRewind` 對外只回傳 τ，PvpMatch 不碰原始欄位；夾取用行為測試守住。這是 Design Invariants 的守衛，照 AGENTS §14.4 **分兩段核准**：declare 批只同意第 8 列的改寫與意圖；rewind 批先寫出新守衛與突變檢查、經另一位審查者核對 1～5，再請你第二次核准，之後才合併。
5. **共用純規則**：把 `SnapshotTimeline.hpp:137-139` 的插值抽成 domain 的純函式 `InterpolatePose`，Client 與 Match 共用（依賴方向本來就是 Client → domain）。
6. **survey 的量測工具**：要載入 FBX 與重定向，照 AGENTS §8 做成獨立的專用工具目標，登記 owner 與對 `object_fps_pvp` 的依賴，不嵌進遊戲程式。
7. **不建 Engine 的通用骨骼形狀格式**：① 只做 PvP 產品的 JSON，通用格式交給 `v8-tool-assets-editor` 規劃（只有一個 consumer；Engine 不承擔工具職能）。這和 README 的「骨骼對應的形狀屬通用格式（Engine 計畫）」原文不一致，見「等你決定」。

沒有新的 Top-level 目錄；match_domain 不加依賴；依賴方向不變。

## 契約變更（寫進新版協議）

- **wire**：兩份 proto 的 `HitPart` 加 `HIT_PART_LIMB = 3`（同號）；跟著改 `IpcHost.cpp:177-178` 的 static_assert、`Combat.hpp:148-150`／`:165-166` 的白名單、`adapter.go:169-170` 的 `playerPart`（沒加的話，含 LIMB 的整批結果會被 Gateway 判無效，而且只在 LIMB 命中時才出現）。測試裡把 3 當無效值的地方改用 4（收緊）。
- **單位**：`maximum_hp`＝100000、`shot_damage`＝25000、`maximum_shot_damage`＝50000；欄位號與型別（uint32）不變，數值語意改變。
- **傷害公式**：`damage = min(hp, shot_damage × permille(part) / 1000)`，中間值 uint64（避免之後調倍率時溢位；現在 100000 × 2000 不會溢位）；建構時驗證能被 1000 整除、結果 ≥1、≤ `maximum_shot_damage`，所以不需要捨入規則。
- **倍率**：以 HitPart 為鍵（HEAD 2000、BODY 1000、LIMB 750），屬 Match 的政策，不上 wire。
- **平手規則**：距離 → 等距時世界優先 → 最小 PlayerId → 同一目標內 HEAD＞BODY＞LIMB → 段 id 字典序；和 JSON 的排列順序無關。
- **回溯跨生命時（對抗式檢查 must）**：τ 與 J 的目標生命不同時，這一發怎麼判，要明確寫進協議（見「等你決定」`v8-product-hitbox-rewind-cross-life`），並配 Client 與 Gateway 都接受的測試、寫明 HitFeedback 的行為。
- **`view_lag` 的基準**：程式（`MatchShotTrace.hpp:225-232`）用 A、pv7 §7 的文字是 J；建議統一成 J（τ 的界限以 J 為準），影子紀錄升版本 3、保留 A 的欄位比較。
- **`damage_count ≤ maximum_hp`**（`ValidDamageRecord`、`adapter.go:152`）：在 100000 下幾乎不再約束，是放寬面，照 §14.4 揭露（見「等你決定」）。
- **版本**：建議升 pv9（兩者都改了既有欄位的語意，要靠版本值拒收混版）；原本 ⑤ 的附錄 B 改名為 pv10，交叉引用由圖書管理員同步檢查。檔名跟協議版本走（`pv8-design-remaining-picks`），所以要另一批 `pv9-rename`（proto、Go 套件、CMake target），版本定義 4 處（`Wire.hpp:17`、`adapter.go:17`、`acceptance_protocol.hpp`、`acceptance_util.py`，`test_protocol_version.py` 守住，對抗式檢查已驗證）。

## 分批（草案，含對抗式檢查的修正）

```
hitbox-line
├── v8-product-hitbox-survey            「high｜Opus 5.5｜獨立的量測工具：節點唯一性、站蹲參考幀尺寸、各 clip 對固定姿勢的偏差、輪廓覆蓋率（只報告）」
├── v8-product-hitbox-lag-shadow        「medium｜Opus 5.5｜影子紀錄 v3：view_lag 改以 J 計算、保留 A」
├── v8-product-hitbox-lag-l2            「medium｜Sonnet 5.5｜L2 收 view_lag 分布、snapshot 缺口、夾取比例（收支行）」
├── pv9-hitbox-declare                  「ultracode｜案 Opus 5.5、對抗 Sonnet 5.5｜事前宣告：protocol-v9 草稿、Design Invariants（第一段核准）、門檻、R 的選取規則、各批 digest 預期與 §14.4 範本」
├── pv9-rename                          「low｜Sonnet 5.5｜proto、Go 套件、CMake target 改名為 v9（版本值不變）」
├── v8-product-hit-rules-permille       「high｜Opus 5.5｜千分之一點：常數、uint64 傷害函式、HUD ceil、C++／Go／驗收工具字面值、DigestVersion 3、golden 重產、版本值 9」
├── v8-product-hit-rules-limb-wire      「high｜Opus 5.5｜HIT_PART_LIMB、白名單、playerPart、往返測試；Match 還不產生 LIMB」
├── v8-product-hitbox-data              「high｜Opus 5.5｜hitbox.json 契約與嚴格載入器、--hitbox、合成 fixture、Design Invariants 與 Replacement Criteria 文件」
├── v8-product-hitbox-shot              「high（平手與順序局部 xhigh）｜Opus 5.5｜逐段判定、ShotHit 帶部位、倍率傷害；combat-*、host-play、host-poor-connection 照宣告重產」
├── v8-product-hit-presentation         「medium｜Opus 5.5｜受擊動畫依部位；驗收工具改成依部位推算期望傷害」
├── v8-product-hitbox-rewind            「high（τ、夾取、生命判斷局部 xhigh）｜Opus 5.5｜姿勢歷史、ShotRewind 單一入口、InterpolatePose、新守衛（第二段核准）、rewind golden」
└── v8-product-hitbox-acceptance        「medium｜Sonnet 5.5｜L1、相關突變＋背景完整跑、L2 驗證爆頭／四肢／回溯（收支行）」
```

| 代號 | 檔位｜引擎 | 估時 | 依賴 |
|---|---|---|---|
| `v8-product-hitbox-survey` | high｜Opus 5.5 | 3～5 h | pv8-line（已合併） |
| `v8-product-hitbox-lag-shadow` | medium｜Opus 5.5 | 1～2 h | 無 |
| `v8-product-hitbox-lag-l2` | medium｜Sonnet 5.5 | 2～3 h＋一次 L2 場次 | lag-shadow；可和後面並行 |
| `pv9-hitbox-declare` | ultracode｜Opus 5.5、對抗 Sonnet 5.5 | 4～6 h | survey、你回覆本文件 |
| `pv9-rename` | low｜Sonnet 5.5 | 1～2 h | declare |
| `v8-product-hit-rules-permille` | high｜Opus 5.5 | 10～16 h | pv9-rename |
| `v8-product-hit-rules-limb-wire` | high｜Opus 5.5 | 3～5 h | permille（同一個 PR） |
| `v8-product-hitbox-data` | high｜Opus 5.5 | 8～12 h | declare（可和 permille 並行） |
| `v8-product-hitbox-shot` | high（局部 xhigh）｜Opus 5.5 | 10～14 h | data、limb-wire |
| `v8-product-hit-presentation` | medium｜Opus 5.5 | 3～5 h | shot |
| `v8-product-hitbox-rewind` | high（局部 xhigh）｜Opus 5.5 | 20～30 h | shot、lag-l2（R）、守衛的第一段核准 |
| `v8-product-hitbox-acceptance` | medium｜Sonnet 5.5 | 3～5 h | 前面全部 |

- **合計約 68～105 h**（對抗式檢查把 rewind 提回 README 的 20～30 h、permille 提到 10～16 h，並加上 pv9-rename、lag-shadow 拆批與測試改動）。README 原本的三批是 28～44 h，**超過 1.5 倍，照 time-box 規則要你重新核准範圍**。rewind 拆成另一條 PR 線時，① 的部位判定線約 45～70 h，回溯線約 23～35 h。
- 每批完成時 dev_log 與 HANDOFF 和該批同一變更提交（AGENTS §15）。
- 波及的測試（對抗式檢查 should，寫進 data 與 shot 批的範圍）：`ShotQueryTests.cpp` 的 31 處 `QueryShot`（合成 Arena）、`CrouchTests.cpp:115-120`、約 112 處 PvpMatch／MatchRuntimeHost 的建構點（建構子要不要給測試用的預設形狀，宣告時說明為什麼不違反「同一事實只維護一處」）、`network_main.cpp:159` 的合法值大小釘（hp 變大，約 +2 B／人，推論）。
- golden 重產照 §14.4 的格式寫 1～5；兩樹比對證明「只有 hp、damage、hitPart 不同」就是「沒有寬鬆面」的證據。declare 批先寫好範本。
- 引擎：除了 Sonnet 5.5 的 lag-l2、pv9-rename、acceptance 與對抗式檢查，其餘照主會話（Opus 5.5）；declare 批開頭再列一次給你核准。

### digest 分步預告（對抗式檢查 must 已修正）

| 批次 | 預期改變 | 證明 |
|---|---|---|
| permille | 全部 golden（每個 Tick 都編 hp）；DigestVersion 3 加 hitPart | 兩樹比對：只有 hp、damage、hitPart 不同；舊基準保留 |
| limb-wire | 0 差分（Match 還不產生 LIMB） | 往返測試 |
| shot | `combat-*`、`host-play`、`host-poor-connection` 的 damage、hitPart、擊殺與重生時程（`host-*` 也有射擊：`AuthorityDigest.cpp:640-662`）；`move-*`、`prediction-*` 0 差分 | 預期序列在宣告前用合成 fixture 算出；MatchRuntimeHost 要能注入 fixture |
| rewind | 既有情境 0 差分（view 為 0 或無效時 τ＝J，宣告前確認各情境的值） | 新增 rewind golden 情境 |

## 對抗式檢查的處置（主會話的判讀）

| # | 嚴重度 | 指出的問題 | 處置 |
|---|---|---|---|
| 1 | must | shot 批預告 `host-*` 0 差分，但 `host-play`／`host-poor-connection` 有射擊 | 接受，改預告（上表） |
| 2 | must | 守衛放寬在 §14.4 的順序上走不通：核准時還沒有新守衛與突變結果 | 接受，改成兩段核准（Architecture Delta 第 4 點） |
| 3 | must | 回溯跨生命時「不扣血」的裁決內容沒定義 | 接受，列為你的決定 `v8-product-hitbox-rewind-cross-life` |
| 4 | should | Replacement Criteria 的「畫面命中、判定未中比例」沒有量測工具 | 接受，條件改用 survey 量得到的量，門檻在 declare 宣告 |
| 5 | should | 守衛只掃 6 個檔，傳整個 ShotRequest 就能繞過；夾取無法用文字比對守住 | 接受，改成全掃加白名單、行為測試守夾取 |
| 6 | should | 升 pv9 的波及面：檔名、附錄 B 與 README 的 pv9 字樣 | 接受，加 `pv9-rename` 批與交叉引用清單 |
| 7 | should | 漏列的測試與資料登記 | 接受，寫進 data 與 shot 批的範圍 |
| 8 | should | 輪廓變窄、命中率下降沒寫；高度檢查的定義不清 | 接受，「玩家會看到的」與 Architecture Delta 第 1 點已補 |
| 9 | should | 出生 yaw＝π 時目標形狀旋轉會用到 libm | 接受，列為 declare 批的檢查項 |
| 10 | should | 估時偏低；dev_log 只在最後一批 | 接受，總估改 68～105 h，每批都寫 dev_log |
| 11 | should | survey 嵌在遊戲程式違反 §8；兩種性質的變更混在一批 | 接受，survey 做成獨立工具目標，lag-shadow 另拆一批 |
| 12 | should | golden 重產沒寫成 §14.4 的格式 | 接受，declare 批先寫範本 |
| 13 | note | R 是政策上限也是濫用上限；L2 的 RTT 不等於實際 | 接受，寫進「玩家會看到的」與 declare 的宣告 |
| 14 | note | 姿勢歷史的生命週期沒寫 | 接受，rewind 批的宣告加一節 |
| 15 | note | `network_main.cpp` 的合法值大小釘會變；「現在會溢位」的說法不對 | 接受，已改寫理由並列入波及清單 |
| 16 | note | acceptance 批要有收支行；引擎要在 declare 列出核准 | 接受 |

## 等你決定

| # | 問題 | 選項 | 建議 |
|---|---|---|---|
| `v8-product-hitbox-model` | 判定姿勢用哪種做法 | (a) B-flat：站、蹲各一張固定 11 段表；(b) A′：權威步態相位上 wire＋離線烘焙表；(c) A：Match 取樣完整動畫 | (a)；A′、A 寫進 Replacement Criteria |
| `v8-product-hitbox-plan-scope` | 估時 68～105 h 超過原本的 28～44 h | (a) 接受，一條 PR 線；(b) rewind 拆成另一條 PR 線，① 先交部位判定；(c) 重新檢視前提與範圍 | (b)：部位判定不依賴 R 的實測 |
| `pv9-hitbox-version` | LIMB 與單位這次 wire 變更的版本 | (a) 升 pv9，⑤ 的附錄 B 改名 pv10；(b) 等 ⑤ 一起升（hitbox 延後）；(c) 留在 pv8（混版時語意錯亂） | (a) |
| `v8-product-hitbox-view-invariant` | 改寫 Design Invariants 第 8 列、放寬並補強守衛（分兩段核准） | (a) 同意；(b) 不同意，不做回溯（R＝0） | (a) |
| `v8-product-hitbox-rewind-r` | R 怎麼決定 | (a) lag-l2 收分布，取事前宣告的分位數加餘裕（上限 15 Tick）；(b) 直接用 15；(c) 先用 8 再調 | (a) |
| `v8-product-hitbox-rewind-cross-life` | 回溯到的時刻目標是上一條命時，這一發怎麼判 | (a) 判 Miss（穿過）；(b) 判命中但傷害 0、不寫受擊紀錄 | (a)：不產生「命中但沒傷害」的畫面回饋，wire 也不用改驗證 |
| `v8-product-hitbox-view-lag-base` | `view_lag` 以 A 還是 J 為準 | J（改程式）／A（改契約文字） | J |
| `v8-product-hit-rules-multiplier` | 倍率放哪、以什麼為鍵 | 鍵：HitPart（3 值）或段（11 值）；位置：CombatRules 常數或產品資料檔 | HitPart 為鍵，放 CombatRules 常數，和形狀分開 |
| `v8-product-hit-rules-damage-count` | `damage_count ≤ maximum_hp` 幾乎不再約束 | (a) 公式不變（揭露放寬面），Match 側另加更嚴的測試；(b) 最小傷害上 wire，Client 與 Gateway 一起收緊 | (a) |
| `v8-product-hit-rules-hud` | HUD 的 HP 顯示 | ceil／floor／一位小數 | ceil |
| `v8-product-hitbox-format-owner` | 骨骼形狀要不要先定 Engine 的通用格式 | (a) ① 只做 PvP 的 JSON，通用格式交給 assets editor；(b) 先在 Engine 定 | (a) |
| `v8-product-hitbox-determinism-premise` | 跨平台決定性的要求 | (a) 只要求 Match 可重現，跨平台只要求 golden 子集；(b) 所有情境跨平台一致（要自製三角函數，屬 Engine Math 變更） | (a) |
| `v8-product-hitbox-shape-bound` | 形狀要不要落在權威移動膠囊的高度內 | (a) 載入時檢查頂端（y＋radius）；(b) 只寫文件 | (a) |
