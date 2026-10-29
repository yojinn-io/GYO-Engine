# pv8-design：pv8 契約的設計（蹲下、衝刺、ADS）

更新：2026-10-29（第 2 版：依 D250 改寫 D-3、D-4 的部分）；第 1 版 2026-10-29。Owner：`object_fps_pvp`。**狀態：草案。D-1、D-3、D-4 已決定（HANDOFF D248～D250）；等使用者決定 D-2、D-5～D-8（文末「等你決定」）。** 決定後實測型別最大，再寫 `protocol-v8.zh-Hant.md` 定稿。
上層：[v8 README](README.md)、[HANDOFF](HANDOFF.md)（D246、D247）。前一版契約：[protocol-v7](../../protocol-v7.zh-Hant.md)。

## 過程

- ultracode（D247）：2 位唯讀偵察（Sonnet 5.5，medium：wire 與契約、玩法程式）→ 3 個獨立方案（Opus 5.5：最小加法、風險優先、功能優先）→ 1 位審查（Opus 5.5）評分與合成 → 1 次對抗式檢查（Sonnet 5.5）。agent 只讀不寫，讀的是本分支的 worktree。
- 評分（滿分 30）：最小加法 26、風險優先 24、功能優先 19。以最小加法為基底，移植了風險優先的決定性規則與守衛測試，以及功能優先的 Arena 倍率。
- 對抗式檢查：must 4、should 8、note 4。處置見下面「對抗式檢查的處置」。
- **所有大小數字都是手算，沒有建置或執行任何測試。** 型別最大在定稿前用 `proto.Size` 實測。

## 草案

### wire 欄位（6 個 bool，兩份 proto 同號）

| 訊息 | 欄位 | 號 | 語意 | 支撐的功能 | ② 之前的值 |
|---|---|---|---|---|---|
| `MovementCommand` | `crouch_held` | 7 | 這一步按住蹲鍵（按住型意圖；切換或按住只在 Client 決定） | ① crouch | crouch 合併時就有行為 |
| `MovementCommand` | `sprint_held` | 8 | 這一步按住衝刺鍵 | ② sprint | Client 送 false；Match 照收、不理會 |
| `MovementCommand` | `aim_held` | 9 | 這一步按住瞄準鍵 | ② ads | 同上 |
| `PlayerState` | `crouched` | 20 | 解析到 `last_resolved_command` 之後的權威姿態（蹲姿膠囊） | ① crouch；hitbox-rewind 的姿態歷史 | crouch 合併時就生效 |
| `PlayerState` | `sprinting` | 21 | Match 實際套用衝刺（意圖＋限制的結果），跨步狀態 | ② sprint（「只能在地面開始、空中可以延續」這類規則；遠端動畫） | Match 恆填 false |
| `CombatState` | `aiming` | 15 | Match 判定的 ADS 狀態 | ② ads（遠端姿勢；ADS 影響命中時的依據） | Match 恆填 false |

- C++ struct 的新欄位一律加在最後：`IpcHost.cpp:617`、`ClientConnection.cpp:608-610`、`AuthorityDigest.cpp:286` 用位置式 brace-init，加錯位置仍能編譯。
- `PlayerState` 16 仍是 reserved。`CombatState` 15 是最後一個 1 B tag。

### 移動參數的校準：Match 在進地圖時下發（D-4，D250）

沿用 `jump_height`、`gravity` 的先例：Match 在 Ready／Welcome 下發，Client 進地圖時檢查範圍並採用，同一 session 內改變就明確報錯（`ClientConnection.cpp:560-570`、`LocalPlayerPrediction.cpp:39-42`）。

| 訊息 | 欄位（號碼在定稿時配） | 型別 | 語意 | 支撐的功能 | ② 之前的值 |
|---|---|---|---|---|---|
| Ready（runtime，接在 9 之後）、Welcome（client，接在 11 之後） | `crouch_body_height` | float | 蹲姿膠囊高度 | ① crouch | 規則資料的值 |
| 同上 | `crouch_eye_height` | float | 蹲姿眼高 | ① crouch | 規則資料的值 |
| 同上 | `crouch_speed_scale` | float | 蹲姿移速倍率 | ① crouch | 規則資料的值 |
| 同上 | `sprint_speed_scale` | float | 衝刺移速倍率 | ② sprint | 1.0 |
| 同上 | `aim_speed_scale` | float | ADS 移速倍率 | ② ads | 1.0 |

- **來源**：Match 端一份產品的移動規則資料（例如 `assets/object_fps_pvp/movement_rules.json`，名稱在 crouch 批決定），有版本與驗證規則，屬 Data Contract（AGENTS §8），可以手寫。只有 Match 讀；Client 不安裝它，也不讀它。
- **不進 `arena_digest`**：Arena 內容不變（不做 Arena v2）。調倍率只要更新 Match 的規則資料，Client 不必重新安裝地圖內容。
- **驗證**（Match 載入時和 Client 收到 Welcome 時各做一次，規則相同）：五個值都是有限值；0 < `crouch_eye_height` ≤ `crouch_body_height` < 該 arena 的 `body_height`；`crouch_body_height` ≥ 2·radius；三個倍率 > 0，上限在定稿時定。不合就明確報錯，不退回預設值。
- **Client 的採用**：和 `MovementRules{jump_height, gravity}` 放在一起，預測時用 Match 給的值，不用本機的值。同一 session 內改變就是錯誤（同 `jump_height`）。
- **Gateway**：Ready→Welcome 多 5 個欄位的逐欄複製，同 `jump_height` 的現行做法，沒有新判斷。
- **大小**：Welcome 和 Ready 各多約 25 B（5×(1 B tag＋4 B)）。Welcome 遠低於 1200 B，實測時一起確認。

### 權威與驗證

- `crouched`、`sprinting` 只由 Match 經 `StepMovement` 產生（預測、重播、權威共用同一個函式）；`aiming` 由 Match 在移動步之後推導。
- Gateway 只多做逐欄複製（`adapter.go` 的 `DecodeInput`、`EqualCommand`、`SnapshotForClient` 與 CombatState 的複製），沒有新狀態、新判斷、新的 DropReason。`state_whitelist_test.go` 不變。
- 6 個 bool 欄位在 wire 層不加範圍規則；旗標組合（蹲＋衝刺等）在 wire 層全部接受，由 `StepMovement` 推導。Welcome 的 5 個參數照上一節驗證。
- ② 之前收到 true：照收、不理會（拒收要在 Gateway 加依內容的判斷，到 ② 又要拿掉）。

### 一步之內的順序與狀態轉換

1. 驗證命令 → alive 時寫 yaw、pitch。
2. 解析姿態（D-3，D250）：**alive 且本步開始時 grounded（`state.grounded`）**才看 `crouch_held`；空中（跳起或走下平台）維持離地時的姿態，空中的蹲鍵與放開都不生效，落地後的下一步才依 `crouch_held` 切換。站變蹲時腳的位置不動；蹲變站要在**目前位置**以站立膠囊做 `CanPlaceCharacterBody`，不通過就維持蹲姿。禁止靠 `MoveCharacterBody` 的初始穿透推出來站起。
3. 解析 `sprinting`（② 之前恆為 false）。
4. 依姿態算水平位移（套用倍率）→ 依姿態做 supported → 跳躍：條件是 alive、`jump_requested`、grounded，且**第 2 步解析後不是蹲姿**（D-3：蹲著按跳無效，不會先站起再跳；同一步放開蹲鍵且成功站起時可以跳）→ 重力 → 依姿態做 `MoveCharacterBody`（掃掠會擋住站姿走進低處）→ 重算 grounded。
- 5 個膠囊呼叫點（supported、`MoveCharacterBody`、`InterpolateLocalPresentation`、`ShotQuery` 的眼高、受擊膠囊）都用同一個「依姿態取高度」的函式。站立分支直接回傳 `arena.bodyHeight`，保持既有的浮點運算順序。
- 死亡：`crouched` 不變（屍體維持死亡時的姿態），`sprinting` 設為 false。
- 重生（`PvpMatch.cpp:264-286`）：明文設 `crouched`＝false、`sprinting`＝false；`aiming` 由 `CombatState{id}` 重設。出生點的選擇用站立高度。
- epoch 輪替：姿態保留（`PvpMatch.cpp:465` 的註解）。
- 替補（`PvpMatch.cpp:477-496`）：
  - Held 保留三個旗標；Neutral 清掉移動軸與 `sprint_held`，保留 `crouch_held`、`aim_held`；jump 照舊清除。
  - **補充（主會話核對）**：`ResetMovementEpoch` 會清空 `lastActualCommand`（`PvpMatch.cpp:433`）。如果直接用清空後的命令替補，epoch 輪替後、第一個實際命令到達前，替補的 `crouch_held` 是 false，蹲著的玩家會被悄悄拉起來。規則：清空之後還沒收到實際命令時，替補的 `crouch_held` 取 `state.crouched`。這一條要先寫成 Red 測試。
- Client：
  - `SeedLead` 的領先命令要繼承權威姿態：`crouch_held`＝`authority.crouched`，`sprint_held`＝false，`aim_held` 取本機最後已知的值。目前它寫死 `{seq,0,0,yaw,pitch}` 並送給 Match（`LocalPlayerPrediction.cpp:74-82`），不改的話權威端會站起 2 Tick。重生後 `authority.crouched` 是 false，之後的實際命令照當下的按鍵。
  - 命令過期、操控者不符或 controls=false 時：蹲保留上一次的意圖，衝刺與瞄準歸 false。
  - 新狀態不放在預測器的成員變數（不學 `pendingJump_`）：命令側隨 pending 重播，狀態側隨 snapshot 整份取代。
  - 眼高與遠端蹲姿的過渡只在呈現層做時間平滑，不進判定；手感由 L3 確認。

### 版本與遷移

- `ProtocolVersion` 7→8，四處定義一起改（`Wire.hpp:15`、`adapter.go:17`、`acceptance_protocol.hpp:6`、`acceptance_util.py:8`），由 `test_protocol_version.py` 檢查。突變 `09-version-python` 的 find／replace 同步改。
- 沿用 pv7 §1 的機制，只換常數；三個版本位置不變。
- proto 檔名跟協議版本（pvN），不跟遊戲版本：`client_v8`、`runtime_v8` 的 8 是 pv8。這次兩者剛好都是 8。
- commit 的順序：
  1. `proto-rename-v8`：只改名，版本值仍是 7；證明重產的差異只有名稱，兩樹 digest 0 差分。
  2. crouch 本體：6 個 bool、Ready／Welcome 的 5 個參數、移動規則資料、版本值改 8、姿態行為。三個角色一起上，pv7 的分析器維持凍結。
- 第 1 版的 `arena-v2` commit 隨 D-4 的改寫取消：Arena 內容與 `arena_digest` 都不變，「站立路徑 0 差分」與「arena digest 會變」的矛盾也跟著消失。

### 大小（手算，定稿前實測）

| 項目 | pv7 | pv8 |
|---|---|---|
| 4 人 snapshot，契約最大 | 1073 B | 1109 B（每人 +9 B） |
| 4 人 snapshot，型別最大（估計） | 約 1151 B（protocol-v7 §8 寫 1155 B，差 4 B 未解釋） | 約 1187～1191 B，只剩 9～13 B |
| `PlayerInput`（12 個命令） | 約 477 B | 約 549 B |
| Welcome／Ready | — | 各多約 25 B，遠低於上限 |

型別最大的餘裕很緊。實測後若超過 1200 B，處理方式見 D-2。

### 權威 digest 與測試

- `DigestVersion` 1→2：`EncodePlayer` 加 `crouched`、`sprinting`，`EncodeCombat` 加 `aiming`；v1 欄位表保留可選。既有 35 個情境用 v1 欄位做兩樹比對（scale 1、3），站立路徑要 0 差分（事前宣告）。
- 新情境（事前宣告）：`crouch-hold`、`crouch-stand-blocked`（合成的懸空天花板地圖；現有地圖沒有）、`crouch-walk-under`（蹲著走到低處下方後放開蹲鍵）、`crouch-held-neutral-gap`、`crouch-epoch-reseed`（預期有一次姿態抖動，不承諾無抖動）、`crouch-respawn`（按住蹲鍵時重生）、`crouch-dead-release`（死亡期間送 `crouch_held`＝false）、`crouch-shot`，以及 D-3 的三個：`crouch-airborne`（跳起後按蹲、走下平台時按蹲或放開，空中姿態不變，落地後才切換）、`crouch-jump-ignored`（蹲著按跳，不站起也不跳）、`crouch-release-jump`（同一步放開蹲鍵並按跳：站得起來就跳，被擋就不跳）。
- 移動規則資料與校準：資料的驗證（每一條規則的違規都拒收）；Client 收到違規的 Welcome 參數時明確報錯；同一 session 內參數改變時報錯；Client 預測用的是 Welcome 的值，不是本機的值（突變：改回本機常數，測試必須失敗）；Gateway 的欄位覆蓋守衛包含 Ready→Welcome 的 5 個欄位。
- 守衛：
  - Go：protoreflect 欄位覆蓋測試（每個欄位填不同的非預設值，翻譯後逐欄相等；刪掉任一個複製，測試必須失敗）；client／runtime 同名訊息的描述子逐號比對。
  - C++：IpcHost、ClientConnection 往返全欄相等。
  - 大小：`capacity_test.go:46` 改成 45＋266×N；`worker_main.cpp:609` 改成 1109；新增型別最大的 `proto.Size` 斷言 ≤1200；新增 12 個命令全為 true 的 `PlayerInput` 最大值測試。
  - ② 之前：所有情境的 `sprinting`、`aiming` 恆為 false，送 true 與送 false 的 digest 相同。② 要放寬這個守衛時，照 AGENTS §14.4 說明。
- 突變：站立分支也走參數化算式；拿掉 `CanPlaceCharacterBody`；Neutral 也清 `crouch_held`；替補不取 `state.crouched`；SeedLead 不繼承姿態。

### 不在 pv8

連續的蹲伏進度與轉換計時；Stance enum；`movement_rules_revision`（D-5）；衝刺體力（D-6）；ADS 的 FOV、viewmodel、靈敏度（只在 Client）；`ShotRequest` 帶射手姿態；因衝刺或 ADS 拒絕射擊的新 `ShotRejection`；wire 層的旗標組合驗證；新的 DropReason；對局規則；多武器、多位攻擊者、選地圖；趴下、側身、滑鏟；movement trace 的姿態鍵（crouch 批另外決定，屬 log-contract）。

### 估時

- `pv8-design` 收尾（實測型別最大、寫 `protocol-v8.zh-Hant.md`）：2～3 h。
- `crouch`：19～25 h，比 README 原估 16～24 h 的上限多 1 h（D-3 的三個情境、Welcome 的校準）。
  - `proto-rename-v8`：1 h。
  - 欄位落地、重產、adapter 與守衛、版本值：5～7 h。
  - 姿態與站起判定、替補與 SeedLead、重生：5～7 h。
  - 移動規則資料與 Ready／Welcome 的校準：2～3 h（取代第 1 版的 `arena-v2` 1～2 h）。
  - digest 與事前宣告：3～4 h。
  - 相機與遠端動畫：3～4 h。

## 對抗式檢查的處置

| # | 嚴重度 | 指出的問題 | 處置 |
|---|---|---|---|
| 1 | must | 死亡時 `StepMovement` 沒有 alive 條件擋姿態；重生區塊沒有重設 `crouched` | 接受：姿態解析只在 alive 時做；死亡時凍結；重生明文重設。加 `crouch-dead-release`、`crouch-respawn` |
| 2 | must | 重生後 SeedLead 與按住蹲鍵的關係沒有寫 | 接受：寫明重生後繼承 `authority.crouched`（＝false），之後照按鍵 |
| 3 | must | `lastActualCommand` 可能把上一生命的 `crouch_held` 帶進新 epoch | 不成立：`ResetMovementEpoch` 會清空它（`PvpMatch.cpp:433`）。由此發現反方向的缺口（epoch 輪替後替補會拉起蹲著的玩家），補成「替補取 `state.crouched`」 |
| 4 | must | 站起判定用移動前還是移動後的位置 | 接受：在目前位置（移動前）判定，之後的掃掠用新姿態的膠囊；加 `crouch-walk-under` |
| 5 | should | 空中蹲下、蹲著跳沒有規範 | 列為 D-3，D250 決定 |
| 6 | should | 姿態預測錯誤時，位置修正不會觸發，眼高會跳 | 接受：眼高在呈現層做時間平滑，手感交 L3 |
| 7 | should | 「hitbox-rewind 不需要新欄位」沒有程式路徑支撐 | 接受：降為假設，在 hitbox 批用實際程式路徑核對 |
| 8 | should | ② 之前沒有守衛能發現 Match 誤填 `sprinting`、`aiming` | 接受：加「恆為 false」的守衛 |
| 9 | should | 型別最大的 4 B 差未解釋 | 接受：列進實測項目 |
| 10 | should | 倍率放在 Arena，② 每次調倍率都會讓已安裝的 arena 失效 | 隨 D-4 的改寫（D250）解消：參數不進 Arena |
| 11 | should | proto 檔名要跟協議版本還是遊戲版本 | 接受：跟協議版本 |
| 12 | should | 「35 個情境 0 差分」和「Arena v2 改變 arena_digest」互相矛盾 | 隨 D-4 的改寫（D250）解消：不做 Arena v2 |
| 13～16 | note | 最大值測試用全 true；CombatState 複製的行號；reseed 會有一次抖動 | 接受，寫進上面的測試與情境 |

## 等你決定

| # | 問題 | 選項 | 建議 |
|---|---|---|---|
| D-1（**已決定：(a)，D248、D250**） | `aiming` 放在哪裡、ADS 的移速怎麼推導 | (a) `CombatState`=15，移速只看 `aim_held`＋PlayerState，換彈不影響移速；(b) `PlayerState`=22，型別最大再多約 4 B；(c) 同 (a)，但移速也看換彈，要把換彈狀態放進 PlayerState | (a)：最省預算，預測一致。如果 ② 一定要「換彈時不能 ADS 移動」，改選 (b)，而且要在定稿前決定 |
| D-2 | 型別最大實測超過 1200 B 時 | (a) `sprinting` 移出 wire，遠端靠速度推，② 不做跨步規則；(b) 重新定義 protocol §8 的型別最大；(c) 接受較小的餘裕 | 先實測；超過時選 (a)，不改 §8 |
| D-3（**已決定，D250**） | 空中能不能蹲下或站起；蹲著能不能跳 | 第 1 版的選項：(a) 空中可以切換、蹲著可以跳；(b) 空中不能切換姿態；(c) 蹲著不能跳 | **決定：(b)＋(c)**。姿態只在著地時切換，空中（含走下平台）維持離地時的姿態；蹲著按跳無效。見「一步之內的順序」第 2、4 步 |
| D-4（**已決定，D250**） | 蹲下參數與倍率放在哪裡 | 第 1 版的選項：(a) Arena 內容 v2；(b) 產品常數；(c) Ready/Welcome 下發 | **決定：(c) 的校準**。Match 端的移動規則資料，在進地圖時經 Ready／Welcome 下發，Client 檢查並採用；不進 `arena_digest`。見「移動參數的校準」 |
| D-5 | 要不要在 Ready/Welcome 加 `movement_rules_revision`（偵測 ① 和 ② 混版） | (a) 不加；(b) 加，Welcome 多 2～6 B | (a)：它支撐的是部署安全，不是玩家功能 |
| D-6 | 衝刺要不要有體力 | (a) 不要；(b) 要，就得在定稿前加進 PlayerState | (a) |
| D-7 | proto 要不要改名成 v8 | (a) `proto-rename-v8` 單獨一個 commit；(b) 保留 v7 檔名 | (a)：照先例，約 1 h，可逐位元驗證 |
| D-8 | 同意 pv8 的 wire 變更（6 個 bool、Ready／Welcome 的 5 個參數、版本 8、新的移動規則資料）作為 Architecture Delta：Runtime 與 Data Contract 的變更，Gateway 只多逐欄複製，依賴方向與 owner 不變 | 同意／修改 | 同意 |
