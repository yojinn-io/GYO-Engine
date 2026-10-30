# PvP Protocol v8：蹲下、衝刺與移動參數的校準

更新：2026-10-30（重開為草稿）。**狀態：草稿（2026-10-30 曾核准定稿 `pv8-design-final-approve`，同日因 `pv8-scope-reopen` 重開）。本文件的蹲下與衝刺條文保留，新的範圍（多通道、序號欄位、動作、物品、背包、Boss、技能、任務、房間規則）在 [pv8 需求問卷](plans/v8/pv8-requirements.md) 之後的設計加入。** 由 v8 的 `pv8-design` 產出（[設計紀錄](plans/v8/pv8-design.md)，[v8 交接](plans/v8/HANDOFF.md) pv8-scope-crouch-sprint、pv8-design-ultracode、pv8-design-first-picks、pv8-design-air-calibration-draft、pv8-design-air-calibration、v8-product-ads-cancel、v8-product-crouch-pose、v8-product-crouch-jump-blocked、pv8-design-remaining-picks）。
本文件的 vN 指網路協議版本 pvN；遊戲版本與協議版本分開編號（pv8-design-remaining-picks）。pv8 取代 [protocol-v7](protocol-v7.zh-Hant.md)：pv7 中沒有在此改寫的條款，pv8 原樣沿用；改寫的條款標明原出處。程式行號以 `62fd7ebf` 為準。

## 0. 原則

- **pv8 是 v8 的一次 wire 變更**（pv8-scope-crouch-sprint，沿用 v7.1 的做法 (a)）：本文件的欄位與語意在 pv8-line 一次加入、一次合併。合併之後，任何新增、刪除、改號、改型別或改語意，都要先徵求使用者同意。
- **只為 ①② 的功能**：蹲下（v8 ①）與衝刺（v8 ②）。ADS 已取消（v8-product-ads-cancel）。對局規則、多武器、多位攻擊者、選地圖不在 pv8（§7）。每個欄位都寫出它支撐的功能（AGENTS.md §2）。
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
- 4 人 Snapshot 的型別最大只剩 25 B。之後要在 PlayerState 或 CombatState 加欄位，先看本節；超過時，先把 `sprinting` 移出 wire，不改本節的定義（pv8-design-remaining-picks，D-2）。

## 7. 不在 pv8 的項目

下列項目刻意不上 wire。之後要做，就是需要使用者同意的 wire 變更：

1. ADS（v8-product-ads-cancel 取消）。
2. 衝刺體力（pv8-design-remaining-picks，D-6）。
3. 連續的蹲伏進度、轉換計時、Stance enum、趴下、側身、滑鏟。
4. `movement_rules_revision` 與混版偵測（pv8-design-remaining-picks，D-5）。
5. `ShotRequest` 帶射手姿態；因衝刺拒絕射擊的 `ShotRejection` 新值；新的 DropReason 值。
6. v8-product-hitbox-rewind 專用欄位：pv8 假設 pv7 的 `HitPart`、`view_tick_q16`、`maximum_shot_damage` 與 PlayerState 的 `crouched` 就夠（Match 內部保存姿態歷史），這個假設在 hitbox 批用實際程式路徑核對。
7. 對局規則（計分、勝負、回合、隊伍、出生保護）：到 v8 ⑤ 再升協議（v8-product-flow-after-tools、pv8-scope-crouch-sprint）。
8. 多武器與射速、多位攻擊者的受擊方向、建房時選地圖。
9. protocol-v7 §10 的其餘項目。

## 8. 範圍與證明

- commit 的順序：(1) `pv8-rename`（只改名，兩樹權威 digest 0 差分）；(2) v8-product-crouch 本體（§2～§6、版本改 8）。三個角色在同一個 PR 合併。
- 權威 digest：`DigestVersion` 1→2，`EncodePlayer` 加 `crouched`、`sprinting`；v1 欄位表保留可選。既有 35 個情境用 v1 欄位做同機兩樹比對（scale 1、3），站立路徑要 0 差分（事前宣告）。
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
- 突變（至少）：站立分支也走參數化算式；拿掉 `CanPlaceCharacterBody`；Neutral 也清 `crouch_held`；SeedLead 不繼承姿態；蹲著時仍然可以跳（站不起來時）；預測改用本機常數；模型縮放改用蹲姿高度；Gateway 刪掉任一個新欄位的複製。

## 附錄：Architecture Delta（pv8-design-remaining-picks，D-8 同意）

- runtime 與 client 的 Data Contract：MovementCommand、PlayerState、Ready、Welcome 的新欄位；版本 7→8。
- 新的產品內 Data Contract：Match 端的移動規則資料（§3）。
- Build Graph：proto、Go 套件、CMake target 改名（`pv8-rename`，單獨一個 commit）。
- Gateway：只多逐欄複製；沒有新狀態、新判斷、新 DropReason；`state_whitelist_test.go` 不變。
- 沒有新的 Top-level 目錄、子系統或跨 owner 依賴；Engine 與 `services/gyo_gateway` 不改；依賴方向與 owner 不變。
