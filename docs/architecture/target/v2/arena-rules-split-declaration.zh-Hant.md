# arena 規則拆出：事前宣告（`v8-arch-gyo2-arena-rules-split`）

任務卡「arena 規則拆出」，2026-11-28。這一階段只做事前宣告，交使用者核准；核准前不寫實作程式。作者 Opus 5.5／high。

出處：HANDOFF `v8-arch-gyo2-mode-config-u3`（原則）、`v8-arch-gyo2-map-scene-split`（Map／Scene 分層）、`v8-arch-gyo2-interact-door-decisions`、`v8-arch-gyo2-overhead-ui-o6-o8`（O7：靶子在 `pvp_arena` 的 scene）、`v8-arch-gyo2-object-pvp-route-a`、`v8-arch-gyo2-arena-rules-split-start`；`mode-config-declaration.zh-Hant.md`（#367）第 3、6、11 節；批次表 `rebuild-batch-plan.zh-Hant.md` 第 89 行；門線宣告 `interact-door-declaration.zh-Hant.md`（分支 `claude/gyo2-interact-door-e11d33`）第 5、6 節。

---

## 0. 前提與結論先講

### 0.1 已定的前提（這份宣告不推翻）

| 前提 | 出處 |
|---|---|
| 遊戲規則不綁定地圖；地圖適應規則，不由地圖決定規則 | `v8-arch-gyo2-mode-config-u3`（使用者原話） |
| Map＝毛坯房（靜態、烘焙過）；Scene＝裝修（門、箱子、NPC 等會互動、有狀態的東西），另一個配置檔；arena＝一個 map＋一個 scene | `v8-arch-gyo2-map-scene-split` |
| 模式配置檔選規則表（`rules`：種類 → 檔案）與玩法組（`sets`）；規則表的值進 `rulesDigest` | `mode-config-declaration.zh-Hant.md` 3.2、3.3 |
| 角色的骨架、動畫、命中形狀進 `characterDigest`（U1 (b)） | 同上 11.7 |
| 路線 (A)：核心 PvP 是回歸基準，結果逐位元一致，用權威 digest 的 A/B 證明 | `v8-arch-gyo2-object-pvp-route-a`、#367 第 5 節 |
| 這一批是 arena 格式唯一的寫者；`pvp_arena` 的 `scene` 值由門線填 | 門線宣告第 6 節 |

### 0.2 結論

**前提：這是作者能處理的**（遊戲的資料契約；Engine 只讀 JSON、算 hash，不改 Engine）。

1. arena 檔（version 4 → **5**）只留地圖類：`id`、`width`／`depth`（邊界）、`map`、`floor`、`kill_height`、`spawns`，加選填的 **`scene`**。
2. 規則類進模式配置檔的規則表：移動速度、跳高、重力進 `movement`（`rules/movement_rules.json`，version 2 → **3**）；墜落傷害進新的規則表種類 **`damage`**（`rules/damage_rules.json`，version 1，G6 之後也放部位傷害）。
3. 身體尺寸（身高、半徑、眼高）進角色資料 `characters/player/character.json`（version 1 → **2**）的 `body`，進 `characterDigest`。
4. #367 留下的 arena `sets` 區塊：建議拿掉（決策點 A）；`arenaDigest` 改成比 arena 自己的值。
5. 六張 arena 的規則類與身體欄位全部同值（移動速度 3、身高 1.8、半徑 0.25、眼高 1.6、跳高與重力用預設 0.6／18、墜落 `{4.0, 15}`），所以搬過去的值和現在完全相同；核心 PvP 的權威 digest 逐位元相同是預期，A/B 驗證。

---

## 1. 成熟方案（源碼）

| 對照 | 看到的 | 位置 |
|---|---|---|
| **avian**（Bevy 生態的物理，`e496f4a`） | 重力是全域 Resource `Gravity`，由 App 插入，不屬於地圖 | `avian/src/dynamics/integrator/mod.rs:140-163` |
| | 角色控制器範例：移動設定（加速度、跳躍衝量、重力、終端速度）是**角色實體上的 component**；碰撞膠囊也在角色上；環境（glb）另外載入，不帶任何移動規則 | `avian/crates/avian3d/examples/kinematic_character_3d/plugin.rs:49-77`、`main.rs:43-57`（玩家）、`main.rs:69-76`（環境） |
| **lightyear**（Bevy 生態的網路，`736e1695`） | 角色的膠囊是常數，放在角色的 bundle；最高速度是移動函式裡的常數 | `lightyear/examples/avian_3d/src/shared.rs:20-21,66-85,197` |
| | 重力在共用的 plugin 裡設定（全遊戲一個值） | `lightyear/examples/fps/src/shared.rs:69` |
| **Godot**（`94fc09c`） | 預設重力是專案設定（全遊戲），每個 World 的 space 都套它 | `servers/physics_3d/physics_server_3d.cpp:554`、`scene/resources/3d/world_3d.cpp:61` |
| | 局部的重力改寫是放在場景裡的 `Area3D`（一個物件，不是關卡的欄位） | `scene/3d/physics/area_3d.cpp:39-45,84`；body 拿到的是合計值 `scene/3d/physics/physics_body_3d.cpp:186-190` |
| | 速度、跳躍速度寫在角色的腳本裡（`CharacterBody3D` 的範本） | `modules/gdscript/editor/script_templates/CharacterBody3D/basic_movement.gd:6-7,13` |
| | 無限大的地板是一種碰撞形狀 `WorldBoundaryShape3D`，放在關卡的場景裡 | `scene/resources/3d/world_boundary_shape_3d.cpp:113` |
| **Unity Netcode** | `PhysicsStep`（含重力）是每個物理 World 一份的單例設定，不是每個角色 | `com.unity.netcode/Runtime/Physics/PredictedPhysicsSystemGroup.cs:185-186`（註解）。Unity Physics 本體不在本機：`PhysicsStep` 是在 SubScene 裡 author 的，是記憶、**未確認** |
| **Unreal**（本機沒有源碼，全部是記憶、**未確認**） | `AWorldSettings::KillZ` 與重力是每個關卡的設定；`UCharacterMovementComponent` 的 `MaxWalkSpeed`、`JumpZVelocity`、`CrouchedHalfHeight` 與膠囊在角色上；`APlayerStart` 放在關卡裡，`AGameModeBase::ChoosePlayerStart` 選哪一個 | — |
| **GYO 現況** | 六張 arena 必須共用一個身高（`games/object_fps_pvp/src/world/PvpWorld.cpp:1087-1093`），角色的縮放由它算（`PvpWorld.cpp:1094`、`src/character/CharacterRig.cpp:203-207`）：身體事實上已經是角色的 | — |

**共同點**：身體的形狀在角色上（avian、lightyear、Godot、Unreal 都一樣）；重力是全遊戲或全 World 的一個值（avian、lightyear、Godot、Unity），局部改寫是放在場景裡的物件（Godot `Area3D`）；地圖不帶移動規則（avian 的環境 glb）。**分歧**：移動速度與跳躍，avian、Godot、Unreal 放在角色上，不在「模式」上（決策點 B）；Unreal 的重力與 KillZ 是關卡設定（記憶）——重力那一半正是使用者原則不要的「地圖決定規則」。

---

## 2. 欄位分類表與依據

判準：**換一張地圖、規則還是同一套嗎？** 是 → 不屬於 arena。**這個值是由地形決定的嗎？** 是 → 屬於 arena（地圖適應規則：規則說「掉出去會死」，地圖說「多低算掉出去」）。

| 欄位（現在 arena v4） | 去哪 | 依據 |
|---|---|---|
| `movement_speed` | 模式配置的 `movement` 表 | 規則；蹲下、衝刺的倍率（`crouch_speed_scale`、`sprint_speed_scale`）已經在這張表（`sim/MovementRules.hpp:12-21`），基準和倍率放一起，不跨檔相乘。別家放角色上：決策點 B |
| `jump_height` | `movement` 表 | 同上（`hold_jump` 已經在這張表） |
| `gravity` | `movement` 表 | 全遊戲／全 World 一個值（avian `Gravity`、Godot 專案設定、Unity `PhysicsStep`）；跳躍速度 `sqrt(2gh)` 由它與跳高算出，同表。局部重力（之後如果要）是 scene 的物件（Godot `Area3D` 的做法），不是 arena 欄位 |
| `fall_damage` | 新的 `damage` 表 | 傷害規則；G6 的部位傷害也在 `damage` 表（#367 宣告 3.2、6），之後腿部吃墜落傷害時在同一張表，不必再搬一次。計算要重力（`v²/2g`），從 `movement` 表讀 |
| `body_height`、`radius`、`eye_height` | 角色資料 `character.json` 的 `body` | 別家全部在角色上；GYO 已經強制所有 arena 同一身高、角色縮放由它算 |
| `width`、`depth`（隱形牆） | 留 arena | 地形的範圍。夾住腳的式子 `[radius, width − radius]` 用角色的半徑，在執行期合起來 |
| `map` | 留 arena | 毛坯房本身 |
| `spawns` | 留 arena | 位置由地形決定（站在地上、不在牆裡）；Unreal 的 `APlayerStart` 也在關卡裡，選哪一個是規則（GYO 的 `sim::ChooseSpawn`，程式）。不放 scene：不會互動、沒有狀態。之後分隊伍或撤離的出生點時再看 |
| `floor`（界線不清） | **留 arena** | 它是「這張地圖沒有地面」的代用地形（一個無限大的地板），不是規則；Godot 同類的 `WorldBoundaryShape3D` 是放在關卡裡的碰撞形狀。理想上屬於 Map，但 Map 是工具烘焙的產物、arena 是手寫的組合檔，放 arena 不必動地圖工具 |
| `kill_height`（界線不清） | **留 arena** | 值由地形決定：`kiriyu_town_graybox` 是 −13，其他 −10（地形最低處不同）。「低於它就死」這條規則在程式裡（`sim::BelowKillHeight`）。Unreal 的 `KillZ` 也是關卡設定（記憶、未確認） |
| `cell_size` | **刪掉** | 讀檔程式從來沒讀（`src/sim/Arena.cpp:59-121`）；格子大小是地圖自己的（`maps-src/*.voxel_grid.json`、region 檔）。「該不該存在」→ 不該 |
| `sets` | 決策點 A | 第 4 節 |
| （新）`scene` | arena，選填 | 門線第 6 節；第 3.1 節 |

**連帶：蹲下的身體**（`crouch_body_height`、`crouch_eye_height`，現在在 `movement` 表）：照同一個判準，它是身體的形狀，應該和站姿的身體放一起；但它是 #367 與 master 的蹲下批就放在規則表的，搬動是推翻已定的放法：決策點 C。

**連帶：型別名**：`movement` 表的內容不再只是姿勢，`sim::StanceRules` 改名 `sim::MovementRules`（和檔名 `MovementRules.hpp`、規則種類 `movement` 一致），`GameRules::stance` → `movement`。這是命名規範內的事，卡片自己判。

---

## 3. 新格式與版本號

### 3.1 arena 檔（`maps/*.json`，version 5）

```json
{
  "version": 5,
  "id": "pvp_training_v1",
  "width": 20,
  "depth": 20,
  "map": "object_fps_pvp.map.pvp_arena",
  "floor": 0,
  "kill_height": -10,
  "spawns": [ {"position": [5, 0, 5], "yaw": 0}, "…" ]
}
```

| 欄位 | 必填 | 意思 |
|---|---|---|
| `id` | 是 | 1～64 字元（不變） |
| `width`、`depth` | 是 | 邊界（不變） |
| `map` | 是 | 地圖的 catalog id（不變） |
| `floor` | 否 | 地板平面的高度；沒有＝地圖自己的地面 |
| `kill_height` | 是 | 低於它就死 |
| `spawns` | 是 | 2～64 個 |
| **`scene`** | 否 | 這張 arena 用哪個 scene 檔：相對 content 根的路徑，**寫法和模式配置檔的 `rules` 相同**（`/` 分隔、每段非空、不是 `.`／`..`、不含 `\` 與 `:`；`src/world/Modes.cpp:28-41` 的 `ContentPath`）。**沒有這個欄位＝沒有裝修**。例：`"scene": "scenes/pvp_arena.json"` |

- `ContentPath` 從 `Modes.cpp` 的匿名 namespace 搬到 `data/JsonFields`（logic 函式庫），arena 與模式配置檔共用一份，不複製。
- `ArenaContent::scene` 是 `std::optional<std::string>`。**這一批只讀路徑的形**；scene 檔存不存在、內容對不對、`sceneDigest`，是門線第 5 步（門線宣告 5.2、5.4）。
- `pvp_arena.json` 這一批**不填** `scene`（門線填）。
- version 4 的檔：讀取失敗（`unsupported arena version`），不做相容讀法（Client 與 Match 從同一個 commit 建，沒有舊檔要讀）。

### 3.2 `movement` 表（`rules/movement_rules.json`，version 3）

```json
{
  "version": 3,
  "movement_speed": 3,
  "jump_height": 0.6,
  "gravity": 18,
  "crouch_body_height": 1.2,
  "crouch_eye_height": 1.0,
  "crouch_speed_scale": 0.5,
  "sprint_speed_scale": 1.0,
  "hold_jump": true
}
```

- 新加 `movement_speed`、`jump_height`、`gravity`：選填，預設 3、0.6、18（和現在 arena 的預設相同；AGENTS §1.10）。核心 PvP 的檔把三個值明寫出來。
- `crouch_body_height`、`crouch_eye_height`：照決策點 C；推薦 (b) 時從這張表拿掉、搬到角色。

### 3.3 `damage` 表（`rules/damage_rules.json`，version 1，新）

```json
{ "version": 1, "fall_damage": {"safe_height": 4.0, "points_per_metre": 15} }
```

- 模式配置檔的 `rules` 加 `"damage": "rules/damage_rules.json"`；`damage` 是選填的種類（沒有＝落地不傷），`movement` 仍是必填。
- `fall_damage` 的欄位與檢查和現在 arena 的相同。G6 之後在同一張表加部位傷害的欄位（version 升）。

### 3.4 角色資料（`characters/player/character.json`，version 2）

```json
{
  "version": 2,
  "model_asset_id": "…",
  "animation_set_asset_id": "…",
  "animation_binding": { "…": "…" },
  "body": { "height": 1.8, "radius": 0.25, "eye_height": 1.6 }
}
```

- `body` 選填，各欄選填，預設 1.8／0.25／1.6（和現在相同）；決策點 C 推薦 (b) 時加 `crouch_height`、`crouch_eye_height`（預設 1.2／1.0）。
- 型別 `sim::BodySize` 放 sim（`sim::BodyHeight`、`EyeHeight`、`MoveSpeed` 這些規則函式要用）；`LoadCharacterRig` 讀它，不再收 `bodyHeight` 參數（縮放用自己讀到的身高）。
- 現在的讀檔程式不檢查 `character.json` 的 version；這一批開始檢查（2 以外讀取失敗）。

### 3.5 模式配置檔（version 1，不變）

只多認得一個規則種類 `damage`。`core_pvp.json` 的 `rules` 加 `damage`。門線新增的 `sandbox.json` 也要列 `damage`，否則那個模式落地不傷（合併後轉告門卡，第 7 節）。

### 3.6 比對值（`MatchInfo`，Codec 版本不變）

| 比對值 | 改後涵蓋 | 改了什麼 |
|---|---|---|
| `rulesDigest` | 模式本身＋`movement` 表（多了速度、跳高、重力）＋`damage` 表（新）＋`CombatRules` | 規則類欄位**開始被比對**（現在 arena 的這些值兩端不同也不會被發現：`GameRules` 與 `MatchInfo` 的 `HashValue` 是空的，`PvpWorld.cpp:106-107`，arena 的值不進任何比對值） |
| `characterDigest` | 加上 `BodySize` | 身體**開始被比對** |
| `arenaDigest` | 決策點 A 推薦 (a)：arena 自己的值（`map`、`width`、`depth`、`floor`、`kill_height`、每個 spawn 的位置與 yaw）；不含 `id`（另比）與 `scene`（scene 的內容由門線的 `sceneDigest` 比） | 同一個 arena id、不同的邊界或出生點，現在不會被發現（Client 的預測照自己的邊界走），改後是 `MismatchKind::ArenaData` |

- `MatchInfo` 的欄位與 wire 不變（`arenaDigest` 仍是一個 u64），所以 `MatchInfoCodec` 的版本、protocol hash 不變。
- `ArenaData` 的原因字串改成說「邊界、地板、kill height、出生點」（`PvpWorld.cpp:850-851`、Client 的顯示 `src/client/ClientWorld.cpp:112`）。

---

## 4. `sets` 區塊（#367 留下的）

**現況**：arena 檔可帶 `sets`（組名 → 組自己讀的區塊；`world/GameplaySets.hpp:47-56` 的 `readArena`、`:91-92` 的 `ReadArenaSets`），模式開著那一組時放進 World（`PvpWorld.cpp:1276-1278`）、進 `arenaDigest`（`PvpWorld.cpp:1026-1038`）。原本預定的使用者：門的位置、撤離點、怪物的出生區（#367 宣告 3.2 第 185 行、6「門」那一列）。**現在沒有任何產品的組用它**；只有測試的 probe 組（`tests/object_fps_pvp/world/GameplaySetTests.cpp:121`）。

**判斷**：原本預定的三種都是「放在地圖上的東西」，照 Map／Scene 分層都是 scene 的物件（門已經改放 scene）。剩下能放在 arena `sets` 的只有「這張地圖的某一組設定」——那就是地圖決定規則，正是原則不要的。所以它沒有該留的用途：決策點 A 推薦拿掉。

---

## 5. 讀檔錯誤

全部是讀取失敗：`nullopt`＋`error`（`LoadGameContent` 在前面加檔名），只說發生了什麼。

| 檔 | 情形 |
|---|---|
| arena v5 | 不是物件；version 不是 5；`id` 缺、空、超過 64；`width`／`depth` 缺或不是有限正數；`map` 缺或空；`floor` 不是有限數；`kill_height` 缺、不是有限數、或不低於 `floor`；`spawns` 不是陣列、不是 2～64 個；spawn 的位置不是 3 個有限數、yaw 不是有限數；spawn 低於 `floor`、不高於 `kill_height`、或在 `[0, width] × [0, depth]` 之外；`scene` 不是字串或不是 content 裡的路徑 |
| arena 讀檔移出去的檢查 | **出生點在邊界內（含半徑）**：現在 `ParseArena` 用 arena 自己的半徑檢查 `[radius, width − radius]`（`src/sim/Arena.cpp:101-104`）。半徑改由角色給，這一條移到 `LoadGameContent`（讀完角色之後，逐張 arena 檢查）：「spawn N of arena 'X' is outside the bounds for the character's radius」 |
| `movement` v3 | version 不是 3；`movement_speed`、`jump_height`、`gravity` 不是有限正數；`2 × gravity × jump_height` 不是有限數；（現有的）蹲下與衝刺的檢查 |
| `damage` v1 | version 不是 1；`fall_damage.safe_height` 不是有限正數；`points_per_metre` 不是 0～2³²−1 的整數毫點（和現在相同） |
| `character.json` v2 | version 不是 2；`body` 的各欄不是有限正數；眼高高於身高；身高短於兩個半球（`2 × radius`）；（C (b) 時）蹲下的身高與眼高的檢查（現在 `StanceFits` 的那幾條，`src/sim/MovementRules.cpp:35-42`） |
| 模式配置檔 | 不認得的規則種類（認得 `movement`、`damage`）；沒有 `movement`；（C (a) 時）規則表的蹲下身體不合角色的身體——現在是逐張 arena 檢查（`src/world/Modes.cpp:177-181`），改成對角色檢查一次 |
| 拿掉的錯誤 | 「the arenas do not share one body height」（`PvpWorld.cpp:1088-1093`）：身體只有一個來源，這種情形不再存在；（A (a) 時）arena 的 `sets` 的各種錯誤 |

---

## 6. 核心 PvP 不變的證明、測試、突變

### 6.1 A/B（權威 digest）

照 #367 第 5 節：主 checkout `build/evidence/gyo2-mode-config/ab.py`（`gyo_object_fps_pvp_core_script --report --check`）。

| 項目 | 方法 | 判定 |
|---|---|---|
| 權威 digest | base＝這條線分出的 `gyo2` commit，head＝改後；同一台 macOS 建兩次、跑同一支腳本，逐位元組比 | 完全相同 |
| report（協議與指紋） | 同一支程式的 `report` 行 | **完全相同**（沒有 Codec、登記、決策資料的改動；`MatchInfo` 的 wire 不變） |
| Linux | 同上，在 Linux 主機（要主機開著，約 30 分鐘） | 完全相同。**建議做**（上段的資料格式），使用者說主機可用時跑；沒跑就寫未跑 |
| S5 | **不重量**：讀圖、建牆的程式不改（只有 arena 的欄位來源變）。主 checkout 的 S5 合成內容（`build/evidence/gyo2-big-map/s5/content.py`）寫的是 v4 的 arena 檔，這一批順手改成 v5（不在 git） | — |

證據放主 checkout `build/evidence/gyo2-arena-rules-split/`。

### 6.2 測試（外部可驗；本機＋L1 三平台）

- **既有測試不改期望值**。用到 arena 規則或身體欄位的測試（`tests/object_fps_pvp/RulesTests.cpp:38-107`、`world/WorldTests.cpp:685-743`、`world/LocalTests.cpp`），只改「從哪裡讀這個值」（`movement` 表、`damage` 表、角色的 `body`），斷言照舊。
- arena 讀檔（`tests/object_fps_pvp/ArenaTests.cpp`）：v5 的正例；v4 被拒；第 5 節每一種錯誤一案；`scene` 讀到、沒有時是空、壞路徑（空段、`..`、`\`、`:`、絕對路徑）被拒；舊的規則欄位不再被讀（v5 檔裡寫了 `movement_speed` 也不影響任何值）。
- `movement` v3、`damage` v1、`character.json` v2 的讀取與每一種錯誤。
- 內容讀取：出生點在邊界內但貼邊、角色半徑較大時被 `LoadGameContent` 拒（臨時 content 資料夾，照 `world/WorldTestSupport.hpp` 的 `ModeFolder`）；模式配置沒有 `damage` 時落地不傷。
- 比對值：`movement` 表的速度、跳高、重力、`damage` 表的每一欄各改一個值，`rulesDigest` 都變；角色 `body` 的每一欄改一個值，`characterDigest` 都變；（A (a)）arena 的 `map`、邊界、`floor`、`kill_height`、spawn 各改一個值，`arenaDigest` 都變，而 `scene` 改了不變；Client 與 Match 的 arena 只差一個出生點 → `MismatchKind::ArenaData`、原因看得出來。

### 6.3 相關突變（D184：新的、find 在改動範圍、殺它的測試被改）

- **新增**（約 15 個）：arena version 5 的檢查；`scene` 的路徑檢查（放過 `..`）；`LoadGameContent` 的出生點半徑檢查拿掉；`MoveSpeed`／`JumpSpeed`／`Fall` 改讀預設值（不讀表）；`LandingDamage` 忽略 `damage` 表；`BodyHeight`／`EyeHeight` 不讀角色；`character.json` 的身體檢查；`rulesDigest` 不收速度／重力／墜落；`characterDigest` 不收身體；（A (a)）`arenaDigest` 不收出生點、邊界。
- **改寫 find**（既有的）：`pvp-arena-version`、`pvp-bounds-at-edge`、`pvp-bounds-open`、`pvp-fall-safe-ignored`、`pvp-floor-everywhere`、`pvp-landing-speed-lost`、`pvp-rules-digest-no-stance`、`pvp-spawn-in-wall-unchecked`、`pvp-character-digest-*`（find 文字跟著改名或改讀處變的才改）。
- **拿掉**（A (a)）：`pvp-set-arena-block-always`、`pvp-set-arena-undigested`（被測的程式不存在了）。
- 只跑上面這些與殺它們的測試被改到的；整條線的完整重跑不在這一批（D184）。

---

## 7. 和三張卡的檔案重疊、合併順序

| 卡 | 現況（2026-11-28 查） | 會重疊的檔（這一批改的 × 那張卡計畫改的） |
|---|---|---|
| 門 `local_dfce2da4`（#375，draft，實作中；分支目前只有宣告） | 第 5 步等這一批的 `scene` 欄 | `world/PvpWorld.cpp`（`Prepare` 放組資料的那段 `:1271-1278`：這一批拿掉 arena 區塊的迴圈，門線在同處放 scene 物件）、`world/GameplaySets.hpp/.cpp`（這一批拿掉 `readArena`，門線在旁邊加 `readScene`）、`world/PvpWorld.hpp`（`GameContent`：拿掉 `arenaSets`、門線加 scene 資料；`MismatchKind`：門線在 `ArenaData` 後加 `Scene`，相容）、`world/Components.hpp`（`GameRules`；門線改 `PlayerInput`、`MatchInfo`）、`client/ClientWorld.cpp`（`ArenaData` 的字串；門線第 0 步大量改名）、`assets/.../maps/pvp_arena.json`（這一批改整個檔；門線加 `scene` 值）、`modes/`（這一批改 `core_pvp.json`；門線加 `sandbox.json` 與 `modes.json`）、`docs/mutations.json`、`tests/.../world/WorldTestSupport.hpp`、`GameplaySetTests.cpp` |
| 頭頂 UI `local_82adf4b0`（#372，draft，只有宣告） | 第 1、2 步只等 #367；第 3 步等門線 | `world/PvpWorld.cpp`（O8 (a′) 改 `JudgeShots`；這一批改它上面的 `ShotRay`、`BodyCapsule` 讀身體的兩行 `:474-482`）、`character/CharacterRig.cpp`（頭的錨點讀 `HitPart::Head`；這一批改 `LoadCharacterRig` 讀身體）、`docs/mutations.json`、`docs/architecture/target/v2/README.md`（各加一行） |
| E1＋E2 `local_080686b2`（#377，draft） | 只改 `engine/modules/model` 與文件 | **沒有程式的重疊**（宣告 K-15：遊戲層不改）。文件：無（HANDOFF、批次表由主會話寫） |

**建議的合併順序**：

1. **這一批先合**。門線的第 5 步（scene）和頭頂 UI 的第 3 步（靶子在 scene）都要 `scene` 欄；W1 排在這一批之後。這一批預計約一天半（第 8 節），門線第 0～4 步也要幾天，時間上不卡門線。
2. 這一批合併後，門卡與頭頂 UI 卡各自 `git merge origin/gyo2`（不 rebase）再接著做；衝突在上表的地方，都是幾行的鄰接改動。主會話轉告兩張卡兩件事：`sandbox.json` 的 `rules` 要列 `damage`（3.5）；arena 的 `sets` 與 `readArena` 已拿掉（A (a) 時），門線照它的宣告本來就不讀。
3. #377 與這一批互不影響，誰先都可以。
4. 為了讓 2 的衝突小：這一批不順手整理上表那些檔的其他地方（例：`Setup` 的位置參數、`ReadText` 三份，#367 審查記下的 P10、P11 不在這一批）。

---

## 8. 批次

```
v8-arch-gyo2-arena-rules-split（一個 PR，開向 gyo2；這份宣告的 draft PR 接著放實作）
├─ 第 1 步｜Opus/high  ｜上段｜格式與讀取：arena v5（scene、拿掉規則與身體、cell_size、sets）、movement v3、damage v1、character v2、
│                              ContentPath 共用、LoadGameContent／LoadModes、GameRules、sim 的函式、三個比對值、assets、測試、突變表
├─ 第 2 步｜Opus/medium｜下段｜A/B（macOS；Linux 看主機）、相關突變、dev_log、本宣告的實作結果
└─ 第 3 步｜Opus/high＋Fable/high｜審查：設計審查（Opus/high）、純粹的 Code Review（Fable/high，只給 git 範圍）、處置、dispatch 三平台 L1
```

| 步 | 上段／下段（根據） | 實作者 | 審查者 | 檢證者 |
|---|---|---|---|---|
| 1 | **上段**：資料格式（四種檔）與內容比對值 | Opus／high（本卡） | Opus／high（設計審查）；純粹的 Code Review Fable／high | Opus／high |
| 2 | 下段（執行與紀錄）；檢驗上段的成果，不用小模型 | Opus／medium | — | Opus／high |
| 3 | 審查與處置 | 處置 Opus／high | 同第 1 步 | Opus／high |

- 除純粹的 Code Review 的 Fable，模型都不超過主會話（Opus）。Fable 不能用時用 Opus，報告寫明。
- **時間**：第 1 步約 6 小時；第 2 步約 2 小時（建兩次＋突變）；第 3 步約 2 小時。超過 1.5 倍就停下回報。
- **停下回報的條件**：A/B 不相同；有既有測試的期望值非改不可；門線要的 arena 介面超出 `scene` 一欄；決策點的答案改變第 2 節的分類。

---

## 9. 要使用者決定的點

只列和別家不同、或推翻已定的。

| # | 問題 | 選項 | 推薦 | 證據 |
|---|---|---|---|---|
| **A**（推翻 #367 已核准的 3.2「arena 也能帶組的區塊」與 11.2 的 `readArena`） | arena 的 `sets` 區塊留不留 | **(a) 拿掉**：`sets`、`readArena`、`ReadArenaSets`、`GameContent::arenaSets` 與放進 World 的那段；`arenaDigest` 改成比 arena 自己的值（3.6），`MatchInfo` 的 wire 不變 (b) 留著，給「某張地圖的某一組設定」；`arenaDigest` 另外也收 arena 自己的值 | **(a)**。機制：原本預定的使用者（門、撤離點、怪物出生區）都是放在地圖上的東西，照 Map／Scene 分層進 scene；剩下的用途是「地圖決定規則」。取捨：之後真有「只屬於這張地圖、不是物件」的資料時要再加欄位，但那時先問它是不是規則；(b) 留一個沒有使用者、又和原則相反的入口 | 第 4 節；`GameplaySets.hpp:47-56,91-92`、`PvpWorld.cpp:1026-1038,1276-1278`；門線宣告第 6 節末 |
| **B**（和別家不同） | 移動速度、跳高放哪 | **(a) 模式配置的 `movement` 表**（主會話在 HANDOFF `-mode-config-u3` 的分法） (b) 角色資料（avian、Godot、Unreal 的放法） | **(a)**。機制：現在只有一個角色；蹲下、衝刺的倍率與 `hold_jump` 已經在 `movement` 表，基準和倍率同表才不跨檔相乘；「這一場跑多快」是模式的玩法（例：一個快節奏的模式配置）。取捨：別家放角色上，是因為它們有多個角色、各自不同；GYO 之後有怪物或不同角色時，角色可以帶一個倍率（或模式配置的表以角色分），那時再加，不必搬回來 | 第 1 節：avian `plugin.rs:49-77`、Godot `basic_movement.gd:6-7`；GYO `sim/MovementRules.hpp:12-21` |
| **C**（推翻已定的放法：蹲下的身體在規則表，#367 的 `movement` 表、源自 master 的蹲下批） | 蹲下的身高與眼高放哪 | (a) 留在 `movement` 表；`StanceFits` 改成對角色檢查一次 **(b) 搬到角色的 `body`**（`crouch_height`、`crouch_eye_height`）；`movement` 表只剩「怎麼動」（速度與倍率、跳、重力、`hold_jump`） | **(b)**。機制：和站姿同一個判準——身體的形狀是角色的（Unreal 的 `CrouchedHalfHeight` 也在角色上，記憶、未確認）；搬了以後「蹲下的身體比站著矮、不短於兩個半球」只在一個檔裡檢查，跨檔的 `StanceFits` 拿掉。取捨：(a) 少動一個檔，但留一個跨「模式配置 × 角色」的形狀檢查；值不變，A/B 不受影響 | `sim/MovementRules.hpp:12-34`、`src/sim/MovementRules.cpp:35-42`、`src/world/Modes.cpp:177-181` |

卡片自己判、不交決定的（使用者可推翻）：重力進 `movement` 表；墜落傷害進新的 `damage` 表；`floor`、`kill_height`、`spawns`、邊界留 arena；`cell_size` 刪掉；`StanceRules` 改名 `MovementRules`；三個比對值的涵蓋（3.6）；不做 v4 的相容讀法。

### 9.1 使用者的決定（2026-11-28，動工前核准）

主會話轉交。原話：「照推薦，A、B、C 都核准，開工」。

| # | 決定 | 比過的選項 | 理由（本文） |
|---|---|---|---|
| A | **(a)** 拿掉 arena 的 `sets` 區塊（`sets`、`readArena`、`ReadArenaSets`、`GameContent::arenaSets` 與放進 World 的那段）；`arenaDigest` 改成比 arena 自己的值。**這推翻 #367 已核准的部分**：`mode-config-declaration.zh-Hant.md` 3.2「arena 檔也可以帶玩法組的一塊資料」（第 185 行）、6「門」那一列的「從 arena 檔的 `sets.doors` 生成門」、11.2 的 `readArena` 與 3.3 第 5 項的 `arenaDigest`（組的區塊）。門已經改放 scene（`v8-arch-gyo2-map-scene-split`） | (b) 留著，給「某張地圖的某一組設定」 | 第 4 節 |
| B | **(a)** 移動速度與跳高放進模式配置的 `movement` 表。**和別家不同**（avian、Godot、Unreal 放在角色上） | (b) 角色資料 | 第 9 節 B |
| C | **(b)** 蹲下的身高與眼高搬到角色的 `body`（`crouch_height`、`crouch_eye_height`）；`movement` 表只留「怎麼動」，跨檔的 `StanceFits` 拿掉 | (a) 留在 `movement` 表 | 第 9 節 C |

卡片自己判的項目（上一段），使用者沒有推翻，照本文進行。

---

## 10. 沒查到、未確認的

- Unreal 的 `KillZ`、重力、`CharacterMovementComponent`、`PlayerStart`／`ChoosePlayerStart`：本機沒有源碼，記憶、**未確認**。
- Unity Physics 的 `PhysicsStep` 在 SubScene 裡 author：本機只有 Netcode 的註解，**未確認**。
- Lyra 的 Experience 能不能改移動速度（GameplayEffect）：沒有源碼，未查。
- 主 checkout 外有沒有別的 v4 arena 檔的產生者：repo 內只有 `assets/object_fps_pvp/maps/*.json` 與測試；主 checkout 的 S5 `content.py` 已知（6.1）。其他機器上的證據腳本未查。

---

## 11. 實作結果

dev_log：`docs/dev_logs/2026_11_28_gyo2_arena_rules_split.zh-Hant.md`。證據：主 checkout `build/evidence/gyo2-arena-rules-split/`（`ab.py`、`ab/`、`linux.sh`、`linux-ab.txt`、`mutations-run1.log`、`design-review.md`、`pure-code-review.md`）。

### 11.1 commit

| commit | 內容 |
|---|---|
| `3c5248a7` | 第 1 步全部（格式、讀取、比對值、A 的拿掉、assets、測試、突變表、dev_log） |
| （本 commit） | 第 3 步：設計審查的處置（11.4）、Linux A/B、本節 |

### 11.2 和本宣告不同的地方

- `ArenaContent` 留在 `sim/Arena.hpp`；`BelowKillHeight` 從 `MovementRules.hpp` 搬到這裡（它問的是 arena 的值）；多一個 `SpawnInBounds(arena, spawn, radius)`，讀檔（半徑 0）與 `LoadGameContent`（角色的半徑）共用一個式子。
- `GameRules` 不另存身體：系統讀 `GameRules::character->bodySize`（身體只有一個來源）。
- `data::Positive`／`PositiveOr` 從 `Arena.cpp` 的匿名 namespace 搬到 `data/JsonFields`（arena、`movement`、`damage`、`body` 四個讀者共用）。
- 讀檔錯誤的字句：`width`／`depth` 缺少時是「missing member width」（原本是「width and depth are required」）；`safe_height` 缺少時是「missing member safe_height」。情形和第 5 節相同。
- 兩個 world 測試名的「arena's」改成「rules'」（速度與跳高不再是 arena 的）；mutations 的 `cases` 用前綴比對，不受影響。

### 11.3 檢驗

- 本機（macOS x86_64，preset `test`）：`object_fps_pvp.*` 16 項全過（cpu 8、network 4、gpu 2、go 2）；`asset.*`、`world_asset.*` 12 項全過。突變跑完、建置還原後再跑一次 `object_fps_pvp.*`，16 項全過。
- **A/B（macOS）**：base `d2329a80`、head `3c5248a7` 的前身（同內容，commit 前的工作樹）：權威 digest 1257 行逐位元組相同，report 沒有差異（第 6.1 節預期的「完全相同」）。
- **突變**（第 6.3 節）：新增 24 個、改寫 find 6 個、拿掉 2 個，加上殺它們的測試被改到的既有 28 個，共 58 個：**58/58 killed**。過程中 `pvp-mode-unknown-rule-kind` 一次 `wrong_failure`（不認得的種類被放過時，那張表被當成 `damage` 讀、以「unsupported version」失敗，不是原本期待的字串），`expect` 改成實際的失敗訊息後重跑 killed。
- **Linux A/B**（Ryzen 7 6800H，g++ 13.3；`linux.sh`：重用 #367 在主機上的兩棵建置樹，只換 checkout，增量建置各約 95 s）：base `d2329a80`、head `3c5248a7`，權威 digest 1257 行與 report 66 行**整個檔逐位元組相同**。

### 11.4 審查的處置

設計審查（Opus／high，唯讀；範圍 `d2329a80..3c5248a7`）沒有 blocker。純粹的 Code Review（Fable／high）的編號寫 P1～。

| # | 內容 | 處置 |
|---|---|---|
| D1 | 出生點半徑的錯誤只寫檔名，第 5 節寫「spawn N of arena 'X'」 | 採用：程式改成也寫 arena id（`… spawn 0 of arena 'edge' is outside the bounds for the character's radius`）；測試跟著改 |
| D2 | `ArenaData` 的原因少了 map（Client 的字串還少了 kill height），比 digest 涵蓋的窄 | 採用：原因寫「map, bounds, floor, kill height, spawns」；Client「データ（地図・範囲・床・落下死の高さ・出生点）」 |
| S1 | `character.json` → rig 的身體、version 檢查、縮放沒有測試也沒有突變 | 採用：world 測試「the character's body is read from its file into the rig」（臨時 content：不是預設值的身體讀進 rig、模型照身高縮放、讀者拒的身體與 version 1 都讀不進來）；突變 `pvp-character-body-unset`、`-version-unchecked`、`-scale-constant`。臨時 content 的做法收成 `ContentFolder`（出生點半徑那一案共用） |
| S2 | 第 5 節的 arena 錯誤不是每一種都有一案 | 採用：id 缺、空、65 字元（64 可）；spawns 不是陣列、65 個；位置不是 3 個數、有非有限值；yaw 非有限、缺；突變 `pvp-arena-id-unbounded` |
| N1 | `LandingDamage` 收整個 `MovementRules` 只為了重力 | 採用：改收 `float gravity`（同 `Fall`）；`DamageRules.hpp` 不再依賴 `MovementRules.hpp` |
| N2 | `ParseBodySize` 重新解析整個角色檔 | 不採：sim 的公開標頭不帶 JSON 型別（JSON 是 logic 函式庫的私有依賴），讀文字是 sim 的讀者共同的形（`ParseArena`、`ParseMovementRules`）；version 由讀整個檔的 `LoadCharacterRig` 檢查 |
| N3 | digest 的 domain tag 沒升 | 採用：`rules/2`、`character/2`（雜湊的內容變了；值本來就不同，A/B 不受影響） |
| N4 | 規則種類比兩次字串 | 不採：兩種；G6 加第三種時再看要不要表 |
| N5 | 重排的註解超過 120 字元 | 採用：`ContentHash.hpp` 那兩段重排 |

處置後：本機 `object_fps_pvp.*` 16 項全過；A/B（macOS）權威 digest 仍 1257 行逐位元組相同、report 沒有差異。
