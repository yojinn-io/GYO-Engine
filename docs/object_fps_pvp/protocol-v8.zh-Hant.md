# PvP Protocol v8：蹲下、衝刺、移動參數的校準與序號欄位的縮窄

更新：2026-10-30（`pv8-contract-round2`：加入第 2 輪的 A 層與擴充附錄）。**狀態：定稿（`pv8-final-approve-round2`，2026-10-30）。附錄 B、C 是設計，不是 pv8 的條文。** 第 1 輪（蹲下、衝刺、參數校準）由 `pv8-design` 第 1 輪產出；第 2 輪（序號欄位的縮窄、`match_id` 的刪除、號碼的預留、擴充附錄）由 `pv8-design` 第 2 輪產出（[pv8-round2](plans/v8/pv8-round2.md)，`pv8-round2-picks`）。決策代號見 [v8 交接](plans/v8/HANDOFF.md)。
本文件的 vN 指網路協議版本 pvN；遊戲版本與協議版本分開編號（pv8-design-remaining-picks）。pv8 取代 [protocol-v7](protocol-v7.zh-Hant.md)：pv7 中沒有在此改寫的條款，pv8 原樣沿用；改寫的條款標明原出處。程式行號以 `62fd7ebf` 為準。

## 0. 原則

- **pv8 是 v8 的一次 wire 變更**（pv8-scope-crouch-sprint，沿用 v7.1 的做法 (a)）：本文件的欄位與語意在 pv8-line 一次加入、一次合併。合併之後，任何新增、刪除、改號、改型別或改語意，都要先徵求使用者同意。
- **pv8 上 wire 的範圍是第 2 輪的 A 層**（`pv8-version-split` (a)）：
  - 蹲下（v8 ①）與衝刺（v8 ②）的欄位（§2～§5）。ADS 已取消（v8-product-ads-cancel）。
  - 序號欄位的縮窄、`Welcome.match_id` 的刪除（§2a），支撐的功能是「在單一資料包內放得下 ⑤ 之後的低頻狀態」；縮窄本身不改變任何行為。
  - B、C 層要用的欄位號以 `reserved` 預留（§2a，`pv8-reserve-numbers` (a)），只防撞號，不產生行為。
- **B 層在 v8 ⑤ 時升 pv9**（附錄 B），C 層只記形狀（附錄 C）。每個欄位都寫出它支撐的功能（AGENTS.md §2）。
- **衝刺的欄位先放、行為在 ②**：pv8 生效時，衝刺的欄位固定為預設值；② 只改 `StepMovement` 與移動規則資料的數值，不再改 wire。
- **Gateway 仍是無狀態的邊緣**（protocol-v7 §0、§2a）：pv8 只讓 Gateway 多做逐欄複製，沒有新狀態、新的跨訊息判斷或新的 DropReason。
- **判定狀態只有 Match 是權威**：`crouched`、`sprinting` 只由 Match 經 `StepMovement` 產生；Client 的值只是預測，Reconcile 時整份換成權威的 PlayerState。

## 1. 版本（改寫 protocol-v7 §1 的「版本值」）

- `ProtocolVersion`＝8，只接受 8。四處定義一起改（`Wire.hpp:15`、`adapter.go:17`、`acceptance_protocol.hpp:6`、`acceptance_util.py:8`），`test_protocol_version.py` 檢查四者一致；突變 `09-version-python` 的 find／replace 同步改。
- `pv8-rename` 先把 `client_v7.proto`／`runtime_v7.proto` 改名為 `client_v8.proto`／`runtime_v8.proto`，package、Go 套件（`clientv8`／`runtimev8`）、CMake target、C++ include 與 namespace 一起改；這時版本值仍是 7。用專案內的 protoc 36.2.0 與 protoc-gen-go v1.36.11 重產，證明差異只有名稱。檔名跟協議版本，不跟遊戲版本（pv8-design-remaining-picks；這次兩者剛好都是 8）。
- 版本位置不變式、各位置的行為、版本不符時的告知，照 protocol-v7 §1，只把 7 換成 8。
- pv7 的分析器維持凍結；`run_quad` 的 mixed 模式在 v8-product-crouch 批改寫成 pv8 版，或明寫退役。

## 2. 新的 wire 欄位

兩份 proto 同號（MovementCommand、PlayerState 本來就同號）。C++ struct 的新欄位一律加在最後：`IpcHost.cpp:617`、`ClientConnection.cpp:608-610`、`AuthorityDigest.cpp:286` 用位置式 brace-init，加錯位置仍能編譯。

| 訊息 | 欄位 | 號 | 型別 | 語意 | 支撐的功能 | 生效時的值 |
|---|---|---|---|---|---|---|
| `MovementCommand` | `crouch_held` | 7 | bool | 這一步玩家按住蹲鍵。按住型意圖，不是邊緣；鍵位用切換還是按住只在 Client 決定 | ① 蹲下 | 有行為 |
| `MovementCommand` | `sprint_held` | 8 | bool | 這一步玩家按住衝刺鍵 | ② 衝刺 | Client 送 false；Match 照收、不理會 |
| `PlayerState` | `crouched` | 20 | bool | 解析到 `last_resolved_command` 之後的權威姿態，true＝蹲姿膠囊 | ① 蹲下；v8-product-hitbox-rewind 的姿態歷史 | 有行為 |
| `PlayerState` | `sprinting` | 21 | bool | Match 實際套用了衝刺（意圖加上限制之後的結果），是跨步狀態 | ② 衝刺（跨步規則、遠端動畫） | Match 恆填 false |

- `PlayerState` 的欄位 16 仍是 reserved。
- 4 個 bool 在 wire 層不加範圍規則；旗標的任何組合在 wire 層都接受，由 `StepMovement` 推導。
- ② 之前收到 `sprint_held`＝true：照收、不理會，結果和 false 逐位元相同。不拒收，是因為拒收要在 Gateway 加依內容的判斷，到 ② 又要拿掉。
- Client 收到 `sprinting`＝true 也接受，不當成無效 snapshot。

## 2a. 序號欄位的縮窄與號碼的預留（第 2 輪 A 層）

### 原則

- 只縮窄有界的計數；不引入新的繞回。GYOP 標頭的 u32 傳輸序號維持既有的串行比較。
- varint 編碼下，縮窄只改**帳面**的最大值，不改實際流量。值不變時權威 digest 不變。
- 用盡的**行為**照各處原樣（拋 `overflow_error`、`SequenceExhausted` 重設、預測器凍結、`Action IDs exhausted`、HTTP 503），但每一處的**條件**都要改成對產品常數 `WireCounterMax`＝`UINT32_MAX`（`Wire.hpp`）判斷，見下面的「用盡的位置」。只換常數而不改條件，tick 到 2^32 時會先撞上編碼邊界的 Assert，player_id 會在 Gateway 被截斷而撞號。C++ 內部型別維持 `std::uint64_t`（`pv8-seq-cpp-types` (b)），在 wire 的編碼邊界（IpcHost 的編碼、ClientConnection 的請求編碼）以 `GYO_ASSERT` 檢查 ≤`WireCounterMax`（Programmer Error）。
- 接收端：protobuf 對 u32 欄位收到超過 32 位元的 varint 會截斷。這不構成新的攻擊面：Client 本來就能直接送任何 u32 值，Match 對這些值做的範圍與一致性檢查照舊。混版由版本值嚴格拒收（§1）。

### 改成 uint32 的欄位（client 與 runtime 的同名欄位一起改）

| 訊息 | 欄位 |
|---|---|
| `WorldSnapshot` | `tick` |
| `PlayerState` | `player_id`、`last_resolved_command`、`movement_epoch`、`life_generation`、`life_state_tick`、`respawn_tick`、`movement_slack_sequence` |
| `CombatState` | `player_id`、`next_allowed_shot_tick`、`life_generation`、`reload_action_id`、`reload_start_tick`、`reload_end_tick`、`last_shot_action_id`、`last_shot_tick`、`last_damage_tick`、`last_attacker_id` |
| `MovementCommand` | `sequence` |
| `PlayerInput` | `movement_epoch`、`life_generation`、`observed_authority_tick` |
| `ShotRequest` | `action_id`、`observed_authority_tick`、`life_generation`、`movement_epoch`、`command_sequence` |
| `ShotDecision` | `action_id`、`resolved_tick`、`target_id`、`life_generation`、`target_life_generation` |
| `ActionBatch`／`ActionResults` | `acknowledged_through`／`retired_through` |
| `Welcome` | `player_id` |
| `CombatRules` | `cooldown_ticks`、`reload_ticks`、`respawn_ticks` |
| runtime 各訊息 | `player_id`：PlayerJoin、PlayerLeave、JoinResult、PlayerEvicted、PlayerInput、ActionBatch、ActionResults、`RuntimeError`（`runtime_v7.proto:123`）、`GatewayDropReport`（`:294`，0 表示 Gateway 本身的串流） |

完整的逐欄清單在 `pv8-seq-narrow` 開工時從兩份 proto 機械列出，與本表核對；本表漏列的 uint64 欄位，預設**保留** u64，要改就先補進本表。

- Go 端的型別要跟著改，不只是逐欄複製：`adapter.go` 的 `DecodeInput`／`DecodeActions` 的 `playerID uint64` 參數、`previous uint64`、各處以 player_id 為鍵的 `map[uint64]`、`validDamageRecord(tick uint64)`，以及 `drop_report.go`、`runtime_link.go` 的 `map[uint64]`（2026-10-30 對抗式檢查）。編譯器會逼出這些位置，但它們都在 `pv8-seq-narrow` 的範圍內。

### 保留 uint64 的欄位

- `view_tick_q16`：乘上 65536 之後，u32 只到 65,536 tick（約 18 分鐘）；`MatchShotTrace.hpp:84-87` 與突變 `v71hf-view-tick-unbounded` 守住它。
- `ClockSample` 的 `*_us`（u32 只到 71.6 分鐘）。
- `arena_digest`（fixed64）。
- 只增不減的診斷計數：`refused_batches`、`DropCount.total`、`report_sequence`、`samples_omitted`、`DropSample` 的 7 個欄位。

### 刪除

- `Welcome.match_id`：Gateway 寫死 1（`server.go:616`），proto 欄位、Go、C++、Python 都沒有讀者（Client 只讀 `player_id`，`ClientConnection.cpp:409-414`）。號碼 2 改為 `reserved 2` 與 `reserved "match_id"`。
- HTTP 契約一起改：`GET /rooms`（`server.go:260`）與 join 回覆（`server.go:369`）的 JSON `match_id` 都刪除。要同步改的測試與文件：`server_test.go:202`（`credentials.MatchID`）、`adapter.go:286` 與 `actions_test.go:31`、`:123`（`MatchId: math.MaxUint64`）、`network-architecture.zh-Hant.md:460`、`:513`、`:524`。

### 上限的根據（需求問卷的典型值）

| 欄位群 | 需求 | u32 的上限 |
|---|---|---|
| tick | 一個 Match 程序最長 7 天不重啟（1-1） | 2^32−1 tick＝828 天（60 Hz），118 倍餘裕 |
| player_id | 一個 Match 程序生命期內 65,535 位玩家（1-2） | Gateway 的遞增計數；用盡回 503 `player_ids_exhausted`（`server.go:358`） |
| action_id | 一位玩家一局 100,000 個動作（1-3） | 用盡報 `Action IDs exhausted`（`ClientConnection.cpp:1011`） |
| epoch、life、命令序號 | 同 tick 的量級 | 用盡照既有規則（`PvpMatch.cpp` 的 `overflow_error` 與 `SequenceExhausted`） |

### 用盡的位置（`pv8-seq-ceiling` 逐處改，各有測試）

| 位置 | 現在的條件 | 新的條件 | 行為（不變） |
|---|---|---|---|
| `PvpMatch.cpp:19-23` `FutureTick` | 相加超過 uint64 上限 | 相加超過 `WireCounterMax`（`respawn_tick`、`reload_end_tick` 等由它算出） | 拋例外 |
| `PvpMatch.cpp:151-152` | `tick_` 等於 uint64 上限 | 等於 `WireCounterMax` | `Authority tick exhausted` |
| `PvpMatch.cpp:270-273` | life、epoch 等於 uint64 上限 | 等於 `WireCounterMax` | `overflow_error` |
| `PvpMatch.cpp:422` `ContiguousPending`（`pv8-seq-ceiling` 補列） | 迴圈上限 `sequence < u64max` | 達到 `WireCounterMax` 就停 | 計數停在上限 |
| `PvpMatch.cpp:431-432` | epoch 等於 uint64 上限 | 等於 `WireCounterMax` | `overflow_error` |
| `PvpMatch.cpp:458` `Tick`（`pv8-seq-ceiling` 補列） | `tick_` 等於 uint64 上限 | 達到 `WireCounterMax` | `Invalid match tick` |
| `PvpMatch.cpp:477-478` | `lastResolvedCommand` 等於 uint64 上限 | 等於 `WireCounterMax` | `SequenceExhausted` 重設 |
| `LocalPlayerPrediction.cpp` 的命令序號、epoch、life 的增量處 | uint64 上限 | `WireCounterMax` | 凍結（`pv8-seq-ceiling` 開工時逐處列出） |
| `ClientConnection.cpp:1011` | `numeric_limits<ActionId>::max()`（`ActionId` 是 uint64） | `WireCounterMax` | `Action IDs exhausted` |
| `gateway/server.go:358` | `nextPlayer == ^uint64(0)` | `nextPlayer == math.MaxUint32`（或更保守的上限） | HTTP 503 `player_ids_exhausted` |

- 這張表是 `pv8-seq-ceiling` 的範圍：wire 不動，先把條件換掉並為每一處加測試，事前宣告 36 個情境 0 差分。
- 截斷的判讀的根據：Gateway 不做跨訊息的值比較（`DecodeInput` 的 `command.Sequence <= previous`、`DecodeActions` 的去重都在單一 datagram 內、在解碼之後），所以「送 2^32＋k」等於「送 k」，而 k 本來就能送。Match 把 u32 的值擴張成 u64 比較是安全的方向。

### 預留的號碼（`pv8-reserve-numbers` (a)）

pv8 的 proto 以 `reserved` 預留下列號碼，只防撞號，不產生行為。號碼依各 proto 目前的下一個空號配；client 與 runtime 不同號的訊息分開列。

| 訊息 | client 號 | runtime 號 | 預定給 | 層 |
|---|---|---|---|---|
| `PlayerState` | 22 | 22 | `team` | B |
| `PlayerState` | 23～24 | 23～24 | 施放中的技能與結束 tick | C |
| `WorldSnapshot` | 6 | 5 | `MatchStatus` | B |
| `WorldSnapshot` | 7 | 6 | `BossState` | C |
| `Welcome`／`Ready` | 16／— | —／14 | `RoomRules` | B |
| `Welcome`／`Ready` | 17～19／— | —／15～17 | 技能、大招、背包的規則 | C |
| `ActionBatch` | 3 | 6 | `events_acknowledged_through` | B |
| `ActionResults` | 5 | 6 | `repeated MatchEvent events` | B |
| `ShotRequest` | 10～12 | 10～12 | `oneof detail`（物品、技能、互動） | B |
| `ShotDecision` | 12～13 | 12～13 | `cooldown_end_tick`、`item_instance_id` | B |
| `ShotRejection`（enum） | 10～17 | 10～17 | 非射擊動作的拒收原因 | B |
| `ShotHitKind`（enum） | 3 | 3 | `HIT_NPC` | C |
| `EvictionReason`（enum） | — | 3 | `EVICTION_EVENT_BACKLOG` | B |
| `RuntimeEnvelope`（oneof） | — | 22、23 | 世界實體、私人狀態 | C |

- 以 `reserved` 預留的號碼，之後真的使用時要先移除該 `reserved` 行；這是一次 proto 變更，照樣要使用者同意（pv9）。
- 第 2 輪合成稿把 `MatchStatus` 寫成 `WorldSnapshot` 第 4 號，和 client 既有的 `match_hold_us`（4）撞號；本表依兩份 proto 的實際最大號更正（2026-10-30 主會話核對）。

## 3. 移動參數的校準（Ready → Welcome，pv8-design-air-calibration）

沿用 `jump_height`、`gravity` 的先例（`ClientConnection.cpp:560-570`、`LocalPlayerPrediction.cpp:39-42`）：Match 在 Ready 下發，Gateway 複製到 Welcome，Client 進地圖時檢查並採用。

| 欄位 | Ready 號 | Welcome 號 | 型別 | 語意 | 支撐的功能 |
|---|---|---|---|---|---|
| `crouch_body_height` | 10 | 12 | float | 蹲姿膠囊的高度 | ① 蹲下 |
| `crouch_eye_height` | 11 | 13 | float | 蹲姿的眼高 | ① 蹲下 |
| `crouch_speed_scale` | 12 | 14 | float | 蹲姿的水平移速倍率 | ① 蹲下 |
| `sprint_speed_scale` | 13 | 15 | float | 衝刺的水平移速倍率；生效時為 1.0 | ② 衝刺 |

- **驗證**（Match 載入規則資料時、Client 收到 Welcome 時各做一次，規則相同；不合就明確報錯，不退回預設值）：
  - 四個值都是有限值。
  - 0 < `crouch_eye_height` ≤ `crouch_body_height` < 該 arena 的 `body_height`；`crouch_body_height` ≥ 2·`radius`。
  - 0 < `crouch_speed_scale` ≤ 1；0 < `sprint_speed_scale` ≤ 3（上限是本文件的提案，v8-product-crouch 批的事前宣告時可以調整）。
- **採用**：Client 把四個值和 `jump_height`、`gravity` 放在同一個 `MovementRules`，預測時用 Match 給的值，不用本機的值。同一 session 內改變就是錯誤：`Match movement rules changed within a session`（同 `jump_height`）。
- **Gateway**：多做 4 個欄位的逐欄複製。`ReadyFitsWelcome`（`adapter.go:279`，確認 Welcome 塞得進一個資料包）要把這 4 個欄位算進去。
- **不進 `arena_digest`**：Arena 內容與正規化不變。

### 移動規則資料（新的產品內 Data Contract）

- Match 端的一份產品資料（檔名與位置在 v8-product-crouch 批決定，例如 `assets/object_fps_pvp/movement_rules.json`），帶 `version` 與上面的驗證規則；可以手寫（AGENTS §8）。
- 只有 Match 讀。Client 不安裝、不讀；Client 只認 Welcome 的值。
- 調整倍率只要更新 Match 的規則資料，Client 不必重新安裝地圖內容。
- 混版（① 的 Client 對 ② 的 Match）不偵測（pv8-design-remaining-picks：D-5 (a)，遊戲還沒上線）。

## 4. 移動規則（`StepMovement` 的一步）

`StepMovement` 是 Client 預測、Client 重播、Match 權威三處共用的同一個函式（`Movement.hpp:166-167`）。一步之內的順序是契約的一部分：

1. 驗證命令；alive 時寫 yaw、pitch。
2. 解析姿態（v8-product-ads-cancel、v8-product-crouch-jump-blocked）：
   - alive 時看 `crouch_held`，地面與空中都可以切換。
   - 本步要跳（alive、`jump_requested`、本步開始時 grounded）時，不論 `crouch_held`，目標姿態是站姿（蹲著跳：直接轉為跳躍）。
   - 站變蹲：腳的位置不動，頭往下縮（空中也一樣，蹲跳不會跳得更高）。
   - 蹲變站：在**目前位置**以站立膠囊做 `CanPlaceCharacterBody`，不通過就維持蹲姿。禁止靠 `MoveCharacterBody` 的初始穿透推出來站起。
   - 死亡時不解析：`crouched` 不變，`sprinting` 為 false。
3. 解析 `sprinting`：pv8 生效時恆為 false；規則由 ② 事前宣告。
4. 依姿態算水平位移（蹲姿乘 `crouch_speed_scale`；衝刺乘 `sprint_speed_scale`）→ 依姿態做 supported → 跳躍：條件是 alive、`jump_requested`、grounded，且第 2 步之後是站姿。蹲著按跳但站不起來時不跳、維持蹲姿 → 重力 → 依姿態做 `MoveCharacterBody` → 重算 grounded。

- 依姿態取高度只有一個函式，5 個呼叫點共用：supported、`MoveCharacterBody`、`InterpolateLocalPresentation`、`ShotQuery` 的眼高、受擊膠囊。站立分支直接回傳 `arena.bodyHeight` 與 `arena.eyeHeight`，保持既有的浮點運算順序，站立路徑的權威結果逐位元不變。
- 重生（`PvpMatch.cpp:264-286`）：`crouched`＝false、`sprinting`＝false。出生點的選擇用站立高度。
- epoch 輪替：姿態保留。
- 蹲姿不改變模型（v8-product-crouch-pose）：呈現用現有角色的蹲姿動畫；模型縮放照舊用站立的 `bodyHeight`，蹲姿高度只用在碰撞、支撐、受擊膠囊與眼高。

## 5. 缺命令時的替補與 Client 的責任

- 替補（改寫 `PvpMatch.cpp:477-496` 的規則）：
  - Held：複製上一個實際命令，`jump_requested` 照舊清除，`crouch_held`、`sprint_held` 保留。
  - Neutral：移動軸與 `sprint_held` 歸零，`crouch_held` 保留。
  - （2026-10-30 更正：先前寫的「epoch 輪替後、第一個實際命令前的替補會把蹲著的玩家拉起來」不成立。Match 在新 epoch 收到第 1 個命令之前不推進該玩家（`PvpMatch.cpp:473-476` 的 `started`），清空的命令不會被拿去替補；突變 `reset-substitution-stands` 存活證實了這點，該規則刪除。）
- Client：
  - `SeedLead` 的領先命令繼承權威姿態：`crouch_held`＝`authority.crouched`、`sprint_held`＝false（`LocalPlayerPrediction.cpp:74-82` 目前寫死 `{seq,0,0,yaw,pitch}` 並送給 Match）。重生後 `authority.crouched` 是 false，之後的實際命令照當下的按鍵。
  - 命令過期、操控者不符或 controls=false 時：`crouch_held` 保留上一次的意圖，`sprint_held` 為 false。
  - 新狀態不放在預測器的成員變數：命令側隨 pending 重播，狀態側隨 snapshot 整份取代。
  - 眼高與遠端蹲姿的過渡只在呈現層做時間平滑，不進判定。
  - 已知：停頓後重新種子（reseed）時，姿態可能抖動一次。

## 6. 大小預算（改寫 protocol-v7 §8）

Client 面向的 UDP 訊息（含 24 B 標頭）在契約最大值與型別最大值下都要 ≤1200 B。2026-10-30 以 `proto.Size` 實測：pv7 的生成碼，pv8 的新欄位以 protowire 附加，編碼結果相同。實測用的暫時測試在量完後刪除。

| 訊息 | pv7 | pv8 |
|---|---|---|
| 4 人 Snapshot，契約最大 | 1073 B | **1101 B**（PlayerState payload 122→128 B，跨過 127 B，長度前綴 1→2 B，每人 +7 B） |
| 4 人 Snapshot，型別最大 | 1151 B | **1175 B**（PlayerState payload 136→142 B，每人 +6 B；餘裕 25 B） |
| PlayerInput，12 個命令（全部欄位非預設） | 477 B | **525 B** |
| Welcome | 約 254 B | 約 274 B（4 個 float） |

- 型別最大的定義：所有 uint32 取上限；enum 取 MaxInt32；`movement_slack_us` 取 MinInt32；bool 都是 true；其他欄位同契約最大的測試（`capacity_test.go:34`）。
- 負的 enum 值會編成 10 B，但不會出現在 Client 面向的 wire 上：Gateway 驗證 `LifeState` 與 `HitPart` 之後，用列舉常數重新組出 snapshot（`adapter.go:88-106`、`:143`）。
- protocol-v7 §8 寫的型別最大 1155 B，和本次實測的 1151 B 差 4 B。pv7 沒有釘住型別最大的測試，原定義無法核對；pv8 以本節的定義與實測為準，並新增測試釘住它。
- 要釘住的測試：`capacity_test.go` 改成 45＋264×N（＝1101）；`worker_main.cpp:609` 改成 1101；新增型別最大 1175 的 `proto.Size` 斷言；新增 12 個命令的 PlayerInput 最大值斷言（525）。
- 上表是第 1 輪（只加蹲下與衝刺）的實測。

### 縮窄之後（§2a，推論，`pv8-seq-narrow` 實測後改寫本節）

| 訊息 | 第 1 輪實測 | 縮窄後（推論） |
|---|---|---|
| 4 人 Snapshot，契約最大 | 1101 B | 約 740 B（PlayerState payload 約 91 B，長度前綴回到 1 B） |
| 4 人 Snapshot，型別最大 | 1175 B | 約 822 B（每個 u64→u32 欄位省 5 B；長度前綴的變化要實測），餘裕約 378 B |
| PlayerInput，12 個命令 | 525 B | 約 450 B |
| ActionBatch 8 筆 | ≤680 B | 約 475 B |
| ActionResults 8 筆 | 約 603 B | 約 403 B |
| Welcome | 約 274 B | 約 263 B（刪 `match_id`） |

- 推論的算法：u64 最大值的 varint 是 10 B（加 tag 11 B），u32 最大值是 5 B（加 tag 6 B）。型別最大的 822 B 用「減 8 個長度前綴」推導，對抗式檢查指出這一步沒有證據，所以只當估計。
- `pv8-seq-narrow` 要用 `proto.Size` 實測，把本節改成實測值並釘成測試（`capacity_test.go`、`worker_main.cpp`、`network_main.cpp`、`actions_test.go` 的 Welcome 邊界，三處數值一致）。
- 第 1 輪定的退路（型別最大超過 1200 B 時先把 `sprinting` 移出 wire，`pv8-design-remaining-picks` 的 D-2）在縮窄後不會觸發，保留為規則。

## 7. 不在 pv8 的項目

下列項目刻意不上 wire。之後要做，就是需要使用者同意的 wire 變更。B 層的項目在 v8 ⑤ 時以 pv9 上 wire（附錄 B）：

1. ADS（v8-product-ads-cancel 取消）。
2. 衝刺體力（pv8-design-remaining-picks，D-6）。
3. 連續的蹲伏進度、轉換計時、Stance enum、趴下、側身、滑鏟。
4. `movement_rules_revision` 與混版偵測（pv8-design-remaining-picks，D-5）。
5. `ShotRequest` 帶射手姿態；因衝刺拒絕射擊的 `ShotRejection` 新值；新的 DropReason 值。
6. v8-product-hitbox-rewind 專用欄位：pv8 假設 pv7 的 `HitPart`、`view_tick_q16`、`maximum_shot_damage` 與 PlayerState 的 `crouched` 就夠（Match 內部保存姿態歷史），這個假設在 hitbox 批用實際程式路徑核對。
7. 對局規則（計分、勝負、回合、隊伍、出生保護）、可靠事件串流、非射擊動作：pv9（附錄 B，`pv8-version-split` (a)）。
8. 地上物品、容器、背包、裝備、Boss、小怪、技能、狀態效果、投射物、任務、防透視、遠處降頻、8 人房：只記形狀（附錄 C，`pv8-req-v8-scope` (a)）。
9. 多武器與射速、多位攻擊者的受擊方向、建房時選地圖。
10. protocol-v7 §10 的其餘項目。

## 8. 範圍與證明

- pv8-line 的批次與順序（`pv8-round2`）：
  1. `pv8-rename`：只改名，兩樹權威 digest 0 差分。
  2. `pv8-seq-ceiling`：不改 wire，`WireCounterMax` 取代 uint64 上限檢查，各用盡邊界的測試；事前宣告 36 個情境 0 差分。
  3. `pv8-seq-narrow`：§2a 的縮窄、刪除與預留，§6 的實測與釘位，protoreflect 覆蓋，凍結探針在 L1 實跑。
  4. `v8-product-crouch` 的 wire 接入：§2～§5、版本改 8。和 3 沒有先後依賴，後落地的一批負責重釘大小。
  三個角色在同一個 PR 合併。
- 權威 digest：`DigestVersion` 2 已在 v8-product-crouch 的不碰 wire 部分完成（`347fa9ba`）：`crouched`、`sprinting` 只在 true 時符號化，站姿情境的位元組與既有 golden 不變；J1（36 個情境 × scale 1、3，0 差分）通過。縮窄（2、3）的證明同樣是 36 個情境 0 差分。
- 新情境（事前宣告）：
  - 基本：`crouch-hold`、`crouch-stand-blocked`（合成的懸空天花板地圖）、`crouch-walk-under`、`crouch-shot`。
  - 替補與重設：`crouch-held-neutral-gap`、`crouch-epoch-reseed`、`crouch-respawn`、`crouch-dead-release`。
  - 跳躍與空中：`crouch-airborne`、`crouch-jump`、`crouch-jump-blocked`、`crouch-jump-hold`。
- 守衛：
  - Go：protoreflect 的欄位覆蓋測試（含 Ready→Welcome 的 4 個欄位）；client／runtime 同名訊息的描述子逐號比對。
  - C++：IpcHost、ClientConnection 往返全欄相等。
  - 大小：§6 的四個斷言。
  - pv8 生效到 ② 之前：所有情境的 `sprinting` 恆為 false；`sprint_held` 送 true 與 false 的 digest 相同。② 要放寬時照 AGENTS §14.4。
  - 移動規則資料：每一條驗證規則的違規都拒收；Client 收到違規的 Welcome 參數、或同一 session 內參數改變時報錯；預測用 Welcome 的值。
  - 呈現：蹲下前後模型的縮放相同。
- 縮窄的守衛：每個縮窄欄位以 `MaxUint32` 經 adapter 往返；`reserved 2` 存在、`match_id` 不在任何讀取路徑；各用盡邊界的測試；凍結的 `action_probe.py`、`impaired_network.py` 在 pv8 的 L1 實跑（凍結工具要改時照 AGENTS §14.4）；`backpressure_probe.py` 開頭位元組隨版本值改變，要先確認它屬不屬於凍結群組。
- 突變（至少）：站立分支也走參數化算式；拿掉 `CanPlaceCharacterBody`；Neutral 也清 `crouch_held`；SeedLead 不繼承姿態；蹲著時仍然可以跳（站不起來時）；預測改用本機常數；模型縮放改用蹲姿高度；Gateway 刪掉任一個新欄位的複製；`WireCounterMax` 改回 u64 上限；刪掉編碼 Assert；`view_tick_q16` 誤改成 u32；大小的等式改成 ≤。

## 附錄：Architecture Delta（pv8-design-remaining-picks，D-8 同意）

- runtime 與 client 的 Data Contract：MovementCommand、PlayerState、Ready、Welcome 的新欄位；§2a 的型別縮窄、`match_id` 的刪除與號碼的預留；版本 7→8。
- 產品常數 `WireCounterMax`（`Wire.hpp`），取代各處的 uint64 上限。
- 新的產品內 Data Contract：Match 端的移動規則資料（§3）。
- Build Graph：proto、Go 套件、CMake target 改名（`pv8-rename`，單獨一個 commit）。
- Gateway：只多逐欄複製；沒有新狀態、新判斷、新 DropReason；`state_whitelist_test.go` 不變。
- 沒有新的 Top-level 目錄、子系統或跨 owner 依賴；Engine 與 `services/gyo_gateway` 不改；依賴方向與 owner 不變。

## 附錄 B：pv9 的擴充（第 2 輪 B 層，v8 ⑤ 時上 wire）

本附錄是設計，不是 pv8 的條文。上 wire 時升 pv9（`pv8-version-split` (a)），各批開工時另做事前宣告與核准。號碼已在 §2a 預留。

### B-1 可靠事件串流（Match → Client）

- **形狀**：搭在每位玩家的 ActionResults 框架上，多一個 `repeated MatchEvent events`。`MatchEvent{event_id u32, tick u32, oneof：MatchPhase、Kill、ScoreChanged、Roster}`。ACK 搭在 `ActionBatch.events_acknowledged_through`。
- **事件號**：每位玩家每個 session 從 1 起，連續，只由 Match 配發。Client 只接受等於下一個期待值的事件，其餘丟棄並計數。
- **權威與送達狀態**（AGENTS §3）：
  - 事件是 `PvpMatch::Step` 的不可變輸出；`PvpMatch` 不知道送達（network-architecture 的 Design Invariants (8)）。
  - 外送箱（下一個事件號、待確認 ≤256、已確認游標）放在 `MatchRuntimeHost`：sim 與 IPC 兩條執行緒的交會點，和現有的動作 ACK 同一處。link 重建時隨 `RequestReset` 清空。
  - 重送排程併進 `IpcHost::ActionLane`（可重建、不參與判定）：先放裁決，再從最小的未確認事件號開始放。
  - Gateway 用既有的 results slot，不加第二個槽；只做逐欄複製與單則檢查（同一框內事件號嚴格遞增且非零、事件數 ≤上限、序列化後 ≤1176 B），不檢查跨框的連續性。
- **溢位**：待確認超過上限時，以 `EVICTION_EVENT_BACKLOG` 逐出（死信不能是無聲的）。上限 256 要用 `pv8-obs-bytes` 的實測確認。
- **上線前必須先解決的事**（第 2 輪對抗式檢查的 must 2～4）：
  1. 下行傳輸序號的 stale 規則（`ClientConnection.cpp:543` 對所有 Type 共用）會讓事件框架被 snapshot 超車而丟掉；results slot 是 latest-wins，被取代也會丟。要決定 stale 是否依 Type 區分（`pv8-client-stale-by-type`，先用 `pv8-obs-bytes` 量超車比例），並寫出最壞延遲。
  2. 串流在 Resync、epoch 輪替、link 重建、短線重連時的語意：Welcome 帶串流起點或 generation，Client 在那時重置期待值；兩條路徑各有測試。做不到就維持只留形狀。
  3. 事件 ACK 的驗證：不超過已配發值、不倒退、和動作的退役互不阻塞；Gateway 漏複製這個欄位時要被覆蓋測試抓到。
  5. `IpcHost` 的 `LanePending()`（`IpcHost.cpp:195`）與 `LaneDue()` 是 `ScheduleWrite` 與 `Write` 共用的單一判斷；事件進入 `ActionLane` 時兩者要一起改，否則會「有事件待送卻不喚醒」或「無限喚醒」。
  6. Gateway 的 results slot 取代舊框時計入 DropReason 34（`action_delivery.go:51`）；事件框被取代算不算丟棄，在 pv9 的設計決定。
  7. IPC link 重建＝整場 Match 重設（`MatchRuntimeHost.cpp:762-787` 的 `RequestReset`／`ClearState`）；Client 短線重連不經過它，第 2 點要分兩條路徑寫。
  4. 每種事件寫出可見性（全員、本隊、本人）並有測試；私人事件不得出現在別人的框架。
- **守衛**：送達參數不改變權威 digest；丟包、重複、亂序、被超車時，Client 套用的事件序列和 Match 產生的完全相同（收支對得上）。
- **支撐的功能**：⑤ `v8-product-match-rules` 的擊殺訊息、結算（必須到達的事件）。

### B-2 對局狀態與房間規則

- `WorldSnapshot.MatchStatus`（client 6／runtime 5）：階段、階段結束 tick、隊伍分數（≤2）、每人的擊殺／死亡／分數；全員可見的低頻狀態，latest-wins。
- `PlayerState.team`（22）。
- `RoomRules`（Welcome 16／Ready 14）：模式、時間上限（tick，u32）、分數上限、友軍傷害、重生規則、隊伍數。由 Match 的規則資料（帶 version 與驗證，AGENTS §8）經 Ready 下發；同一 session 內改變就是錯誤（同 §3 的先例）。建房的人自訂規則延後（需要多房間與房主身分）。
- **支撐的功能**：⑤ 的計分板、階段、隊伍顏色、勝負。

### B-3 動作種類

- `ShotRequest.oneof detail`（10～12：物品、技能、互動）；`ActionKind` 只加有功能批的種類；`ShotRejection` 10～17；`ShotDecision` 12～13。
- 非射擊動作在交接後的下一個 Tick 裁決，不用 A1 的 E／S 到期規則，避免視窗 32 的行頭阻塞；兩人同時撿同一件時依交接順序，後到的回 `ITEM_GONE`。這條和射擊判定的 Design Invariants（E、S 是不可變請求內容）的關係，要在該批的契約寫清楚。
- `ShotRequest`／`ShotDecision` 的改名（ActionRequest／ActionDecision）在這一批決定（`pv8-action-message-name`）。
- **支撐的功能**：第一個非射擊動作的功能批。

## 附錄 C：只記形狀的項目（第 2 輪 C 層）

v8 沒有排定對應的功能（`pv8-req-v8-scope` (a)）。下面只記錄方向，開工時重新設計。

| 項目 | 形狀 |
|---|---|
| 地上物品、容器、屍體 | 出現與消失走可靠事件；加入時送一次基準（典型 200 件約 7.3 kB，約 0.33 s 送完，推論）；容器內容只送給開啟的人 |
| 背包與裝備 | 私人事件（全量基準加差量），只進擁有者的串流；外觀裝備公開；佔格大小放在產品的 catalog（Data Contract） |
| Boss、小怪 | Boss 狀態放 snapshot（latest-wins），換階段、擊倒走事件；小怪走每人 snapshot（見下）；前提是產品有 PvE |
| 技能、狀態效果、投射物、放置物 | 施放是動作；冷卻與充能走私人事件；施放中的技能放 snapshot；投射物用生成事件加共用的純軌跡規則 |
| 任務 | 私人事件；任務定義放在 catalog |
| 防透視、遠處降頻、20 隻 AI、8 人房 | Match 每 Tick 一則框架，裡面是每位玩家一份子訊息，Gateway 在單一框架內依 player_id 分送（是否算 Gateway 允許類別的擴充，由使用者判定）；相關集合與 priority accumulator 放在 Match（可重建、不參與判定）；守衛「interest 參數不改變權威 digest」 |
| 不採用 | 開放 GYOP 標頭的 Channel（會改 `engine/net` 與 `services/gyo_gateway`）、新增上行 Type（會吃 session 的 120/s 桶，最壞只剩約 6/s）、Gateway 的第二個槽、HTTP→IPC 的 RoomCreate |

