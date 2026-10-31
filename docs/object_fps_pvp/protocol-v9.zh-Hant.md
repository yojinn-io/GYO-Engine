# PvP Protocol v9：三部位命中、千分之一點、Match 的權威動畫狀態與回溯

更新：2026-10-31（`pv9-hitbox-declare`）。**狀態：定稿（`pv9-final-approve`，2026-10-31）：第 9 節 6 項照建議 (a)。** 範圍：hitbox-line 與 rewind-line 的 wire 需求一次推完（`pv9-scope-hitbox-and-rewind`）。計畫：[hitbox-plan](plans/v8/hitbox-plan.md) 第 2 版；回溯的設計沿用第 1 版（git `e33d53fc`）。推導：ultracode（三案 Opus 5.5、審查 Opus 5.5、對抗 Sonnet 5.5），紀錄見 [dev_log](../dev_logs/2026_10_31_pvp_v8_pv9_hitbox_declare.zh-Hant.md)。

本文件的 vN 指網路協議版本 pvN。pv9 取代 [protocol-v8](protocol-v8.zh-Hant.md)：沒有在此改寫的條款原樣沿用。程式行號以 `57a3aace` 為準。

## 0. 原則

- **pv9 只合併一次**：版本值 9、`HIT_PART_LEG`、千分之一點、`WorldSnapshot.animation`、`animation_digest` 同一次進 master；整條 hitbox-line 一個 PR，做完 `v8-product-hitbox-shot` 才合併（`pv9-single-merge` (a)）。protocol-v8 §2a 已規定：之後再新增或啟用預留，就是另一次 proto 變更。
- **每一項都有出處**：需要它的程式位置，或 hitbox-plan 第 2 節的成熟方案（附 ✓／△／✗）。推不出來的列在第 9 節，不寫成假設（pv8 假設 pv7 的 `HitPart` 夠用而漏了腿，`pv9-scope-hitbox-and-rewind` 的起因）。
- **⑤ 的對局規則不在 pv9**：protocol-v8 附錄 B 改名為 pv10，附錄 C 照舊只記形狀。pv8 預留的號碼（protocol-v8 §2a）一個都不挪用。
- **Gateway 照舊是無狀態的邊緣**：只多逐欄複製與單則訊息內的檢查，沒有新狀態、新 DropReason 或跨訊息的判斷（`state_whitelist_test.go` 不變）。

## 1. 版本

- 版本值 8→9，只接受 9（GYOP 標頭、`RuntimeEnvelope.protocol_version`）。
- 理由：單位、`BODY` 的範圍是既有欄位的語意改變，舊端讀到新值不會出錯，只會悄悄算錯；舊 Client 也會忽略新的動畫欄位、自己推步伐而和 Match 漂開。混版只能靠版本值拒收。

## 2. 部位（`HitPart`）

- 加 `HIT_PART_LEG = 3`（client 與 runtime 同號）。不設手臂的值，也不預留。
- `HIT_PART_BODY` 的範圍從「整顆站／蹲膠囊」（`ShotQuery.cpp:54-56`）改成「軀幹含骨盆」；`HIT_PART_HEAD` 不變。
- 射線穿過手臂時不停：繼續找後面的部位、世界或其他玩家。
- 要跟著改的地方（兩端對稱）：`Combat.hpp:47` 列舉、`:148-150` `ValidDecisionHitPart`、`:165-166` `ValidDamageRecord`；`IpcHost.cpp:178` 的 `HitPart_ARRAYSIZE == 3`；`adapter.go:169-171` `playerPart`；`ClientConnection.cpp:446`、`:644` 的解碼驗證；`PvpMatch.cpp:369`、`:375`（現在寫死 Body）。測試：`CombatTests.cpp:809` 把 3 當無效值，改用 4（收緊）。
- 出處：hitbox-plan §1（`v8-product-hit-rules-three-parts`）、§4 R4。三部位是我們的需求；Valorant 的三層部位 ✗（技術文章沒提），不當依據。

## 3. 單位：千分之一點

| 欄位 | 號碼 | 新值 | 說明 |
|---|---|---|---|
| `CombatRules.maximum_hp` | 1 | 100000 | 100 HP |
| `CombatRules.shot_damage` | 2 | 25000 | 軀幹的基本傷害 |
| `CombatRules.maximum_shot_damage` | 9 | 50000 | 頭（25000×2000/1000）；pv7 §7 本來就為倍率預留了這個欄位 |
| `CombatState.hp` | 2 | [0, maximum_hp] | |
| `ShotDecision.damage` | 7 | [0, maximum_shot_damage] | min(hp, 50000／25000／18750) |

- 欄位號與型別（uint32）都不變。部位倍率（頭 2000、軀幹 1000、腿 750）是 Match 的政策，**不上 wire**。
- 傷害＝shot_damage×permille(part)/1000，中間值用 uint64；建構時驗證能整除、結果 ≥1 且 ≤ maximum_shot_damage（hitbox-plan §9）。
- Client 的顯示：HUD 的 hp 顯示 ceil(hp/1000)，寫成 `hp/1000 + (hp%1000 != 0)`；射手的命中回饋 `weaponFeedback.lastDamage`（`PvpApplication.cpp:477`）也要換算。
- `damage_count ≤ maximum_hp`（`adapter.go:152`、`Combat.hpp:169`）在 100000 下幾乎不再約束，是放寬面，照 AGENTS §14.4 揭露（沿用 `v8-product-hit-rules-damage-count` (a)）。
- 驗收工具裡寫死的數值要跟著改：`combat_gui_evidence.py:25`、`:223`，`network_main.cpp:136`、`:148`、`:159`，`actions_test.go:88`，`adapter_test.go`／`server_test.go` 的 `testRules`。
- Welcome 的 `CombatRules` 變長約 6 B；Welcome 的最大值是靠加長 arena id 撐到 1200 B 的（`actions_test.go:135`），所以 arena id 的上限少約 6 字元。

## 4. Match 的權威動畫狀態（`WorldSnapshot.animation`）

### 4.1 為什麼要上 wire

Client 現在自己累積步伐相位、移動權重、jog 權重與跳躍階段（`PlayerPresentation.cpp:257-335`）。以 Valorant 為基底之後，判定姿勢由 Match 決定，Client 必須播同一組狀態，畫面上看到的才是被判定的姿勢（hitbox-plan §3、`v8-product-anim-state-on-wire`）。

成熟方案只證實「伺服器存位置與動畫狀態，回溯時用它」：Valorant ✓、CS △。**下面欄位的具體內容是我們依程式路徑推出來的**，不是照抄。

其他會影響姿勢的輸入，已經是權威欄位的純函式，不加欄位：瞄準＝f(`pitch`)；蹲姿 clip＝f(`crouched`)；死亡＝f(`life_state_tick`)；射擊、換彈、受擊＝f(CombatState 的各個 tick)（`PlayerPresentation.cpp:374-378`、`:381-383`、`:402-423`、`:464`）。idle 的時鐘改成 (t − `life_state_tick`)/60，不再因 epoch、不連續、瞬移而重設（`:262-288`、`:311`），所以也不必上 wire。

### 4.2 放在哪

新訊息 `PlayerAnimation`，放在 `WorldSnapshot` 的 `repeated animation`（client 8／runtime 7），和 `players` 一對一，帶 `player_id` 與 `life_generation`。

- 不放進 `PlayerState`：C++ 的 `PlayerState`（`Movement.hpp:160-183`）是 `StepMovement` 與預測的狀態，`SeedLead` 整份複製權威（`LocalPlayerPrediction.cpp:79`）；動畫狀態混進去會讓移動的 owner 多管一件事（AGENTS §3）。
- 先例：`CombatState` 也是獨立的 repeated 訊息，和 players 配對檢查（`adapter.go:79`、`:112-115`）。兩份 proto 的 `WorldSnapshot` 號碼本來就差一位（client 的 `gateway_hold_us`＝5 只在 client 端）。

### 4.3 欄位（client 與 runtime 同號）

| 號碼 | 欄位 | 型別 | 意義 |
|---|---|---|---|
| 1 | `player_id` | uint32 | 屬於哪位玩家 |
| 2 | `life_generation` | uint32 | 屬於哪條命；Client 只把同一條命的動畫和位置配在一起，重生後不沿用舊命的相位 |
| 3 | `gait_phase_q16` | uint32 | 步伐相位，不取模：每個 Tick 加 Δq16＝round(有號的水平位移 ÷ 一圈距離 × 65536)，一圈距離用這個 Tick 速度的精確 walk↔jog 比例（不是量化後的 `jog_weight`），累加 mod 2^32（`v8-product-anim-phase-definition` (a)，2026-10-31）；低 16 位是一圈內的位置。walk、jog、crouch_move、sprint 共用。倒退時相位減少，方向隱含在差值裡，`backward` 不上 wire |
| 4 | `move_weight_permille` | uint32 | idle↔步態（蹲姿時 crouch_idle↔crouch_move）的混合權重，[0, 1000] |
| 5 | `jog_weight_permille` | uint32 | walk↔jog 的速度混合權重，[0, 1000]，由權威水平速度決定 |
| 6 | `jump_phase` | enum `JumpPhase` | 下半身的跳躍階段 |
| 7 | `jump_phase_tick` | uint32 | 目前階段的 clip 時間零點（Authority Tick），見 4.4 |
| 8 | `sprint_weight_permille` | uint32 | jog↔sprint（`Sprint_Loop`）的混合權重，[0, 1000]；② 之前恆為 0，Client 與 Gateway 都拒收非 0（`pv9-sprint-animation` (a)）；② 的事前宣告定推進規則（照 4.4 的整數推進） |

新 enum `JumpPhase { JUMP_PHASE_UNSPECIFIED = 0; GROUNDED = 1; START = 2; AIRBORNE = 3; LAND = 4; }`，0 在 wire 上無效（`LifeState` 的先例，`adapter.go:90-103`）。

### 4.4 語意（Match 與 Client 共用的規則）

- **量化值就是權威值**：Match 的動畫狀態本身就存 wire 的整數（q16 相位、千分比、tick）。判定、回溯歷史、snapshot 發出的、Client 收到的，是同一組值；沒有只存在 Match 端的高精度狀態。
- **權重的整數推進**：每 Tick 的增量 (1/60)/transition 秒數必須是整數千分比。`transition_seconds` 0.1 s＝6 Tick，增量用「距離目標還差幾 Tick」換算（例如 `weight = 1000 × 已過 Tick / 6`，整數除法），不累加小數。載入時檢查 transition 秒數是 Tick 的整數倍。
- **`jump_phase_tick` 是 clip 時間的零點**，不是「偵測到換階段的 Tick」。照現行狀態機（`PlayerPresentation.cpp` 的 `jumpPhaseSeconds += span`）：
  - START：起跳的 Tick；
  - AIRBORNE：起跳 Tick + START 的跨度（0.1 s＝6 Tick）；沒有起跳、直接離地落下時是離地的 Tick；
  - LAND：著地的 Tick；GROUNDED：LAND 的零點 + LAND 的跨度，重生時等於 `life_state_tick`。
  - 載入時檢查 START、LAND 的跨度是 Tick 的整數倍。這條沒寫清楚的話，Client 與 Match 會各自詮釋，JumpLoop 的 clip 時間差 6 Tick（survey 量到起跳時頭偏 0.25 m 的原因）。
- **`jog_weight` 的保留與重設**：速度為 0 時保留上一個值；重生與重設時由當下速度重算。
- **死亡**：一對一保留；死亡期間的值凍結在死前最後一個 Tick，Client 不讀（死亡是全身 clip）。
- **內插**（契約的規則，不是 wire）：Client 的呈現與 Match 的回溯共用一個純函式。位置、pitch、權重用線性內插；yaw 用最短弧；相位用 int32 的有號差；離散值（`grounded`、`crouched`、`jump_phase`）取前一筆；以 Tick 為零點的動作用零點求值。
- **`backward` 的判斷**：Match 版不用 libm 的 sin／cos（用 yaw 的整數或多項式等價式），或接受「只同平台比對」（B3 已說取樣在平台間不一致）。在 `v8-product-anim-authority` 的事前宣告定。

### 4.5 Gateway 與 Client 的檢查（單則訊息內）

- `len(animation) == len(players)`，`player_id` 一一對應、不重複（新的區域 map 屬 `classPerCall`，有 `SnapshotForClient` 的 `seen` 先例）。
- `life_generation` 等於同一位玩家的 `PlayerState.life_generation`。
- 三個權重 ≤ 1000；② 之前 `sprint_weight_permille` == 0；`jump_phase` ∈ {1, 2, 3, 4}；`jump_phase_tick` ≤ snapshot 的 tick。
- 和同一則訊息的 `grounded` 交叉：START／AIRBORNE ⇒ `!grounded`；GROUNDED、LAND ⇒ `grounded`。死亡玩家不檢查這條。
- 兩端對稱：Gateway 的 `SnapshotForClient`，以及 Client 的 `ClientConnection.cpp` 解碼（`:592`、`:644` 附近），驗收工具的解碼端（`acceptance_protocol.hpp`、`network_main.cpp`、`worker_main.cpp`）也要跟上。
- **已知的後果**（`pv9-gateway-check-failure` (a)）：Gateway 的 `SnapshotForClient` 回錯誤時，`server.go:704-707` 呼叫 `runtimeFailed`，整個 Gateway 對所有玩家失效，不是只丟一份 snapshot。所以 Match 在寫進 snapshot 前用 Assert 守住同一組條件（Programmer Error，`docs/architecture/error-handling.md`）；Gateway 的錯誤處理不改。

### 4.6 動畫內容的校準（`pv9-animation-digest` (a)）

- `Ready.animation_digest`＝18、`Welcome.animation_digest`＝20（fixed64；避開 pv8 預留的 Ready 14～17、Welcome 16～19）。
- 內容：Match 載入的動畫定義的雜湊——判定與呈現用到的 clip 轉換後的 key（B3 的 H-load 的範圍）、各動作的跨度、transition 秒數、native speed 等 `presentation.json` 中影響姿勢的值。確切的位元組順序在 `v8-product-hitbox-data` 的事前宣告定，Match 與 Client 共用同一個函式。0 無效。
- Gateway：Ready→Welcome 逐欄複製；`ReadyFitsWelcome` 把它算進去。
- Client：收到 Welcome 時和自己載入的內容比對，不合就明確報錯（例如 `Match animation content differs from this client`），不進入對局。和移動規則的 D-5（不偵測）不同，理由是動畫內容不同時 Client 畫面上的姿勢不是被判定的姿勢。

## 5. 回溯（rewind-line）

沿用既有欄位，不加新欄位：

| 用途 | 欄位 | 規則 |
|---|---|---|
| 射手看到的時刻 τ | `ShotRequest.view_tick_q16`（9） | τ＝clamp(view_tick_q16/65536, J−R, J)；0 或無效時 τ＝J。R≤15 是 Match 常數（`maximum_reference_age_ms` 250 ms×60 Hz），由 lag-l2 的分位數決定（`v8-product-hitbox-rewind-r`） |
| 射手自己的時刻 | `movement_epoch`（7）、`command_sequence`（8） | 射手不回溯，用解析完 S 之後的狀態 |
| 逐目標的界限 | （Match 權威） | 再夾到該目標目前區段（`movement_epoch`、`life_generation`、`life_state`）的起點 |
| 回溯跨生命 | `ShotDecision` 的既有形狀 | 判 Miss：射線穿過那個目標，結果是既有的 `HIT_MISS`／`HIT_WORLD`／後面的其他玩家（`v8-product-hitbox-rewind-cross-life` (a)）。不新增 `ShotRejection` |
| 歷史 | （Match 內部） | 每 Tick 存位置、yaw、pitch、`crouched`、`grounded`、生命資料、CombatState 的動作 tick 與 `PlayerAnimation`；回溯時用 4.4 的內插重建姿勢（hitbox-plan §4 R6） |

- 目標在 τ 活著、到 J 已經死了（同一條命）時：判 Miss，和跨生命同一條規則。
- **接受的限制**：Client 漏收或 snapshot 被合併（`DROP_COALESCED_SNAPSHOTS`）時，Client 畫面上的兩筆 snapshot 隔了好幾個 Tick，它內插出的姿勢和 Match 在同一 τ 重建的姿勢不同（離散值更新得更晚、內插跨度更長）。Client 漏收目標新生命的前幾筆時，也會看到比 Match 晚的區段起點。這兩點接受為限制，`ShotRequest` 不加欄位（`pv9-rewind-client-view` (a)）；影子紀錄 v3 量漏收時的落差，量到問題再處理。
- 影子紀錄 `match-shots.jsonl`（產品內的 Data Contract，不是 wire）升 version 3：`view_lag` 以 J 計算（對齊 pv7 §7 的文字，`MatchShotTrace.hpp:230` 現在用 A），保留 A 的值供比較（`v8-product-hitbox-view-lag-base`）。

## 6. 大小（推論；`v8-product-hit-rules-leg-wire` 與 `v8-product-anim-authority` 以 `proto.Size` 實測重釘）

| 項目 | pv8 | pv9（推論） |
|---|---|---|
| 4 人 Snapshot 型別最大（含 24 B 標頭） | 822 B | 1022 B（每人 +50 B：8 欄×6 B＋外層 2 B） |
| 4 人 Snapshot 契約最大 | 748 B | 約 896 B（每人 +37 B；衝刺權重以 ② 之後的 1000 計） |
| Welcome | 1200 B（邊界） | CombatRules 約 +6 B、`animation_digest` +10 B（fixed64 加 tag，號碼 20 的 tag 是 2 B 時 +11 B）；arena id 上限相應縮短 |

都 ≤ 1200 B。要重釘的地方：`capacity_test.go:68`、`:71`，`worker_main.cpp:674`，`network_main.cpp:159`，`actions_test.go:135`。

`v8-product-hit-rules-permille` 的實測（2026-10-31）：型別最大以 MaxUint32 取 hp 與 damage，單位改了也不變；合法值的 4 人 Snapshot（`network_main.cpp:159`，hp＝100000）由 698 B 變成 **706 B**（每人的 hp 由 1 B 的 varint 變成 3 B），已重釘。其餘的釘不受單位影響，原值照舊通過。

## 7. Design Invariants 的變更（hitbox-plan §7，已核准的 Architecture Delta 的落地）

1. `network-architecture.zh-Hant.md:854`（觀測與模擬契約分開）：`view_tick_q16` 成為有界的判定輸入，只由 `ShotRewind` 單一入口讀；守衛 `test_no_decision_reads_the_view_tick` 改成全掃加白名單。照 AGENTS §14.4 **分兩段核准**（`v8-product-hitbox-view-invariant` (a)）：本批只同意改寫與意圖，守衛在 rewind 批第二次核准。
2. 新增一列：判定姿勢只由 Match 的權威動畫狀態（`WorldSnapshot.animation` 加權威欄位的純函式）與 Match 的形狀資料決定；Client 的呈現照 snapshot 播放，不參與判定。protocol-v7 §7「命中幾何只讀權威狀態；呈現動畫不參與」相應改寫。
3. State Authority 的移動：步伐相位、兩個權重、跳躍階段與它的零點，從 Client 的 `PlayerLocomotionState` 改成 Match 權威。Client 只保留呈現用的內插與 holding 凍結。
4. `PlayerPresentation` 目前**沒有** Design Invariants 節，要先新建（含 Replacement Criteria），再寫「遠端的動畫狀態來自 snapshot」。
5. `Combat.hpp:134-137`「受擊與射擊紀錄 Match 從不讀」改寫：上半身的射擊、換彈、受擊 clip 進判定姿勢，這些紀錄成為判定輸入（`pv9-upper-body-in-hitbox` (a)）。protocol-v7 §2、§7 的相應文字一起改。

## 8. 不在 pv9 的項目

- ⑤ 的對局規則、可靠事件串流（pv10，protocol-v8 附錄 B 改名）；附錄 C 的形狀。
- 部位倍率、R、形狀 JSON、Match 讀的動畫定義、姿勢歷史：Match 的政策或產品內容，不上 wire。
- 同一 Tick 內多發子彈時的姿勢順序（判定前固定一次，或每發後重算）：不影響 wire，在 `v8-product-hitbox-shot` 的事前宣告定。
- 動畫狀態進權威 digest 的版本：在 `v8-product-anim-authority` 定。
- 本機玩家自己的身體是否照 Match 的動畫播：呈現路徑沒有查到（遠端迴圈會跳過本機玩家），不影響 wire；`v8-product-anim-authority` 時確認。

## 9. 決定（`pv9-final-approve`，2026-10-31：全部照建議 (a)）

| # | 問題 | 選項 | 建議 |
|---|---|---|---|
| (A) `pv9-animation-digest` | Client 與 Match 的動畫內容（clip、跨度、transition）不同時，Client 畫面的姿勢不是判定的姿勢。要不要偵測 | (a) Ready／Welcome 加 `animation_digest`（fixed64，避開預留號碼），Client 不合就明確報錯；(b) 不偵測（照 pv8 對移動規則的 D-5） | (a)：載入轉換在四平台一致（B3 的 H-load），digest 做得出來；之後才加就要升 pv10；多 10 B |
| (B) `pv9-upper-body-in-hitbox` | 上半身的射擊、換彈、受擊 clip 進不進判定姿勢 | (a) 進：畫面＝判定；受擊與射擊紀錄成為判定輸入，改寫 `Combat.hpp:134-137`；(b) 不進：判定用的上半身只看瞄準 | (a)：照 Valorant「跑和 Client 相同的動畫」✓。兩個選項都不改 wire |
| (C) `pv9-sprint-animation` | ② 衝刺的動畫。UAL1 有 `Sprint_Loop`，但 `jog_weight` 夾在 [0, 1]、沒有 sprint 的混合 | (a) 現在加 `sprint_weight_permille`（號碼 8，[0, 1000]），② 之前恆為 0；(b) ② 用 `sprinting`（已在 wire）直接切 clip，不混合（蹲下的先例）；(c) ② 不加衝刺動畫，jog 加快播放 | (a)：② 是已排定的功能，照 `pv9-scope-hitbox-and-rewind` 的教訓現在推完；直接切 clip 會在步伐中途跳姿；每人多 6 B |
| (D) `pv9-rewind-client-view` | 漏收時 Client 看到的姿勢與 Match 重建的姿勢不同（第 5 節的限制） | (a) 接受，寫進契約；`ShotRequest` 不加欄位；(b) `ShotRequest` 加 Client 實際用的兩筆 snapshot tick（號碼要避開 B 層預留的 10～12） | (a)：CS 也只送一個時刻（Client 的指令時刻與內插量）△；Valorant 不要求兩邊逐位元一致；影子紀錄 v3 可以量漏收時的落差，量到問題再說 |
| (E) `pv9-single-merge` | pv9 的欄位一次進 proto，但 `anim-authority` 之前沒有行為 | (a) 整條 hitbox-line 一個 PR，做完 `anim-authority`、`hitbox-shot` 才合併 master；(b) 先合併 permille＋leg-wire，Match 填固定的合法動畫值、Client 暫不採用（pv8 的 `sprinting` 先例） | (a)：「pv9 只合併一次」與「PR 少開」；(b) 會讓 master 有一段「版本 9 但動畫欄位沒有意義」的時間 |
| (F) `pv9-gateway-check-failure` | 新檢查失敗時整個 Gateway 失效（4.5） | (a) 照現行慣例（runtime 失效），Match 寫入前用 Assert 守住同一組條件；(b) 新的檢查只丟這一份 snapshot（Gateway 的行為變更） | (a)：不改 Gateway 的錯誤處理；不合法的動畫狀態是 Match 的 Programmer Error，應該在 Match 端就停下 |
