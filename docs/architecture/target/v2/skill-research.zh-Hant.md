# 技能系統與冷卻：研究（`v8-arch-gyo2-skill-research`）

研究卡，2026-11-27。只研究、只改文件；交出決策點，不寫程式。使用者的決定由主會話記錄（HANDOFF），本文件不寫「使用者的決定」一節。

## 0. 前提、範圍、判斷依據

### 0.1 已定的前提（這份研究不推翻它們）

| 前提 | 出處 |
|---|---|
| 最終產品是有怪物的撤離射擊，塔科夫只是參考之一 | HANDOFF `v8-arch-gyo2-tarkov-is-reference` |
| 路線 (A)：技能等新功能都加在 Object_PvP，由模式配置決定開不開；「核心 PvP」模式＝現在的規則，當回歸基準 | HANDOFF `v8-arch-gyo2-object-pvp-route-a` |
| 模式配置：玩法組（gameplay set）在每個模式、每一端都登記，跑不跑由帶值的 run condition 決定；規則表由模式檔的 `rules` 選；模式 id＋規則 digest 進內容比對 | HANDOFF `v8-arch-gyo2-mode-config-*`；PR #367 `mode-config-declaration.zh-Hant.md` 第 0.2、3、4、6 節 |
| 規則類欄位（移動速度、跳高、重力、墜落傷害）要從 arena 搬進模式規則表（`v8-arch-gyo2-arena-rules-split`），排在 #367 之後、W1 之前 | HANDOFF `v8-arch-gyo2-mode-config-u3`；批次表〈後續小事〉 |
| 會影響移動的狀態效果會進預測與回滾，成本比只動 Match 的後果高一級；(Z4) 另成一批 | `limb-damage-research.zh-Hant.md:257`；HANDOFF `v8-arch-gyo2-limb-damage-decisions` |
| 網路：Match 權威＋Client 預測自己的玩家＋rollback；權威狀態進 digest 比對 | `authority.zh-Hant.md`、`rollback.zh-Hant.md` |
| 不參考 master 的權威／回滾算法與數據 | 記憶 `gyo2-no-master-algorithms-data` |

使用者的例子：瞬間移動、移動加速、2 段跳、蓄力跳；有冷卻的是主動技能，被動技能沒有冷卻。使用者之前說：「增加連跳（按住空格），如果後期有修改跳躍效果的 SKILL 可以直接修改」。

### 0.2 判斷依據

1. Engine 只給機制；技能是什麼、怎麼疊加、冷卻多長，都是遊戲規則，由作者選。每題開頭寫一句前提：**Engine 要解決（只有 Engine 做得到）還是作者能處理**。
2. ECS 相關以 Bevy 為主參考；偏離時寫衝突點與膠水量。
3. 核心 PvP 模式的行為要留得住（A/B 權威 digest 相同，#367 第 5 節）。
4. 不加防誤用的守衛；契約以外的設計由每批的設計審查與純淨 Code Review 看。
5. 查證等級：**源碼**（本機 `~/Code/GitHome`，附檔名與行號）、**官方文件**（附 URL）、**△ 第三方**（社群文件、非官方站）、**未確認**。

### 0.3 gyo2 現況一覽（後面各題共用）

| 什麼 | 現況 | 位置 |
|---|---|---|
| 移動、跳躍 | `StepBody` 一個函式做視角、蹲、跳、落下、沿牆滑動；只能從地面起跳（`input.jump && grounded && !crouched`），沒有空中跳 | `games/object_fps_pvp/src/world/PvpWorld.cpp:305-375`（起跳 `:338-341`） |
| 數值來源 | 移動速度、跳高、重力在 `ArenaContent`；蹲、衝刺倍率、`hold_jump` 在 `StanceRules`（`rules/movement_rules.json` v2，模式檔的 `movement`） | `include/object_fps_pvp/sim/Arena.hpp:32-41`、`include/object_fps_pvp/sim/MovementRules.hpp:13-20`、`assets/object_fps_pvp/rules/movement_rules.json` |
| 速度 | `MoveSpeed(arena, rules, crouched)`：一處算出 | `MovementRules.hpp:33` |
| **連跳（按住空格）** | **在 Client 算**：`ClientWorld.cpp:586` `input.jump = sim::StepJumps(step.jump, step.jumpHeld, rules)`，輸入只送一個 `jump` 位元；Match 的 sim 看不到「按住」 | `src/client/ClientWorld.cpp:538-539,586`；`MovementRules.hpp:37-41` |
| 輸入 | `PlayerInput`：`jump`、`fire`、`reload` 是「按下」（只算一步；網路沿用上一筆輸入時清掉，`PvpWorld.cpp:217-222`），`crouch` 是按住；Codec 版本 2；`PlayerInput` 是判定的輸入（`{.input = true}`），**進權威 digest** | `include/object_fps_pvp/world/Components.hpp:103-117`；`PvpWorld.cpp:93-104,197-224,1096` |
| 冷卻的記法 | **已經是「結束的 tick」**：`PlayerCombat::nextShotTick`、`reloadEndTick`；判定 `now >= nextShotTick`，起動時 `now + TicksFor(秒)`。「開始的 tick」只用在 `lifeTick`、`lastShotTick`（給畫面與聲音） | `Components.hpp:51-64`；`PvpWorld.cpp:427-455` |
| 秒→tick | `TicksFor(seconds, stepNanoseconds)`：無條件進位、至少 1 | `include/object_fps_pvp/sim/CombatRules.hpp:44` |
| 權威與同步 | `AuthorityEntry::Component<T>` 代為登記進 Kernel＋回滾，進 digest；`NetEntry::Replicate<T>` 帶版本號的 Codec；Client 預測自己的玩家（`MovePlayers`、`UseWeapons` 兩端都跑），其他人取 Match 的值 | `PvpWorld.cpp:1091-1133`；`authority.zh-Hant.md` 第 2 節；`rollback.zh-Hant.md` 一個例子 |
| 回滾的方式 | `StateRollbackStrategy`：逐實體逐元件比對權威值和歷史，不同就放回那一步、寫回輸入、重跑 | `rollback.zh-Hant.md` 一個例子；`PvpWorld.cpp:1225-1227` |
| 模式配置（#367，OPEN） | `GameplaySet{name, build, readMode, readArena}`；System 進 `pvp.<name>.fixed`／`.frame` 兩個 Kernel set，條件 `pvp.mode.<name>`；關著的組不放 component、不同步、不進 digest；`core_pvp.json` 的 `sets` 是空的 | PR #367 `games/object_fps_pvp/include/object_fps_pvp/world/GameplaySets.hpp`、`Modes.hpp`、`assets/object_fps_pvp/modes/core_pvp.json` |
| Kernel 的 Query | 沒有 Bevy 的 `Option<&T>` 項；可用第二個 `Query` 的 `Get(id)`（沒有就回 `nullptr`）達到同樣效果 | `engine/ecs/kernel/include/engine/kernel/Params.hpp:48-53,306-316`；Bevy `crates/bevy_ecs/src/query/fetch.rs:3695`（`Option<T>`）、`:3939`（`Has<T>`） |

### 0.4 對主會話初步方向的修正（查程式後）

| 初步方向 | 查到的 | 影響 |
|---|---|---|
| 「現有的 `PlayerCombat` 記的是開始的 tick」 | 判定用的冷卻已經是結束的 tick（`nextShotTick`、`reloadEndTick`）；開始的 tick 只給畫面 | 技能冷卻用結束的 tick 是**沿用現有寫法**，不是新寫法（第 3.2 節） |
| 「後期有修改跳躍效果的 SKILL 可以直接修改」 | 連跳的規則在 Client 算成一個位元；sim 不知道「按住」，也分不出「這一步剛按下」和「一直按著」 | 2 段跳（要分出空中的「按下」）、蓄力跳（要知道按了多久）、改連跳的技能，都要**先把跳躍規則搬進 sim**，輸入改送原始的「按下」與「按住」（決策點 K1） |
| 「瞬移要由 Match 檢查目的地能不能站」 | GYO 的預測是兩端跑同一個 System：Client 預測時用同一個 `Fits` 檢查（`PvpWorld.cpp:278`），Match 的結果自動是權威，預測錯就回滾。不需要 GAS 那種「Client 請求、Server 另外驗證」的第二條路 | 瞬移不另立驗證流程（第 3.4 節） |

---

## 1. 骨架：技能、效果、冷卻、修飾、標籤

### 1.1 五個概念各自是什麼、放哪一層

| 概念 | 前提 | 是什麼 | 放哪一層 |
|---|---|---|---|
| **技能定義**（skill definition） | **作者能處理**：哪些技能、數值多少是遊戲的資料 | 資料：id、種類（主動／被動）、效果種類、數值、冷卻秒數、蓄力的上下限 | 遊戲的規則表（模式檔 `rules` 的一種，例 `skills`）；解析後的值進規則 digest（同 `StanceRules`，#367 第 3.3 節），兩端各讀自己的檔、不同步 |
| **效果**（effect） | **作者能處理** | 程式：每個效果種類一段 sim 程式，讀定義裡的數值。分四種（1.2） | `games/object_fps_pvp/src/sim/`（純函式）＋`src/world/`（System） |
| **冷卻**（cooldown） | **作者能處理**：記法與長度是遊戲規則；Engine 已給 `Tick`、回滾、digest | 狀態：每個技能欄位一個「可以再用的 tick」 | 玩家實體上的 component（例 `SkillSlots`），authority＋`Replicate` |
| **修飾**（stat modifier） | **作者能處理**：哪些數值能被改、怎麼疊加是遊戲規則 | 「某個數值 +x 或 ×y」的資料；被動的修飾從定義來（常駐），有時效的修飾是狀態（結束的 tick） | 計算是 sim 的純函式（第 4 節）；有時效的修飾存在 `SkillSlots` 裡 |
| **標籤**（tag） | **作者能處理**；Engine 已有 marker component（`With<T>`／`Without<T>`，`Params.hpp:50-51`） | GAS 用來表達「冷卻中」「被沉默」「不能和某技能並用」 | **第一批不做**（決策點 K5）；需要時做成 component 裡的位元，不用增刪 marker component（預測中增刪 component 的問題見 2.3） |

結論：**五個都在遊戲層**，Engine 不新增（第 6 節）。

### 1.2 效果的四種（拿使用者的例子對）

| 種類 | 例子 | 寫什麼 | 何時算 |
|---|---|---|---|
| (E1) 瞬時效果 | 瞬間移動 | 當步改狀態（`PlayerBody::feet`）一次 | 按下技能的那一步 |
| (E2) 有時效的修飾 | 移動加速（例：3 秒內 ×1.5） | 記下修飾的結束 tick；數值每一步由第 4 節的函式算 | 起動時記；到期自動失效 |
| (E3) 常駐修飾（被動） | 移動速度 ×1.2、空中多跳 1 次 | 不存狀態：裝了就生效，從「裝了哪些技能」＋定義算出 | 每一步算 |
| (E4) 蓄力 | 蓄力跳 | 記下開始按住的 tick；放開時用持續的 tick 數算強度 | 按住時記、放開時發動 |

2 段跳是 (E3)：數值「空中可跳次數」基準 0，被動 +1；另要一個狀態「這次離地後已用了幾次」（落地歸零）。

### 1.3 一個例子（移動加速，從按鍵到畫面）

1. Client 第 100 步：玩家按 Q（技能欄位 0）。輸入 `skillPressed = 0b0001` 送出，並在本地照常預測。
2. 兩端的 `UseSkills`（`skills` 玩法組，固定步、在 `MovePlayers` 之前）：欄位 0 的 `readyTick <= 100`，發動。`readyTick = 100 + TicksFor(冷卻秒)`；在修飾欄位寫「移動速度 ×1.5，結束 `100 + TicksFor(3)`」。
3. 同一步的 `MovePlayers`：算速度時呼叫 `EffectiveMovement(基準, 裝備, 有時效的修飾, now)`，得到 ×1.5 的速度。
4. Match 第 100 步得到同樣的結果；`SkillSlots` 複製回 Client。若 Client 預測錯（例：Match 判定這一步玩家已死），回滾 Module 比對 `SkillSlots` 不同，放回第 100 步重跑。
5. 畫面：技能圖示的冷卻條讀 `readyTick - now`；加速的特效讀「修飾的開始 tick」（和 `lastShotTick` 給聲音同一種做法），不在 sim 裡發事件。

---

## 2. 對照：別家怎麼做

### 2.1 Unreal GAS 與 Lyra（本機沒有源碼，只用官方文件）

| 主題 | 做法 | 出處（查證等級） |
|---|---|---|
| Ability／Effect／Attribute | Ability＝可重複觸發的動作，定義做什麼、成本、條件；GameplayEffect＝改 Attribute 與 tag 的不可變資產（runtime 是 Spec）；Attribute 是 float，分 base 與 current | [Understanding GAS](https://dev.epicgames.com/documentation/en-us/unreal-engine/understanding-the-unreal-engine-gameplay-ability-system)、[Gameplay Effects](https://dev.epicgames.com/documentation/unreal-engine/gameplay-effects-for-the-gameplay-ability-system-in-unreal-engine?lang=en-US)、[Attributes and Attribute Sets](https://dev.epicgames.com/documentation/unreal-engine/gameplay-attributes-and-attribute-sets-for-the-gameplay-ability-system-in-unreal-engine)（官方文件） |
| 冷卻 | 冷卻是一個 GameplayEffect；`CommitAbility` 套用成本與冷卻；Lyra 的 `GA_Grenade` 用 `GE_Grenade_Cooldown`、`GA_Hero_Dash` 用 `GE_HeroDash_Cooldown`，剩餘時間由能力廣播給 UI | [Using Gameplay Abilities](https://dev.epicgames.com/documentation/unreal-engine/using-gameplay-abilities-in-unreal-engine)、[Abilities in Lyra](https://dev.epicgames.com/documentation/unreal-engine/abilities-in-lyra-in-unreal-engine)（官方文件） |
| 冷卻＝effect＋tag | 冷卻 GE 給擁有者一個 tag，能力檢查這個 tag；`GetCooldownGameplayEffect`、`GetCooldownTags` | API 頁存在但內容由 JS 載入、讀不到；論壇與社群文件這樣寫（△ 第三方，[論壇](https://forums.unrealengine.com/t/gameplay-ability-system-dynamic-cooldowns/522829)） |
| base／current | Instant 的 GE 改 base；有時效的 GE 改 current，結束時撤銷；例：base 100、×0.7 → 70，base 變 110 時 current 77 | Understanding GAS、Attributes（官方文件） |
| 疊加公式 | `((Base + Add) × Multiply ÷ Divide × CompoundMultiply) + FinalAdd`；有 Override 時直接回 Override；evaluation channel 分段 | [gas-reference（聲稱照 UE 5.7 源碼）](https://gas-reference.pages.dev/reference/modifier-formula/)（△ 第三方）；`EGameplayModOp`、`FAggregatorModChannel` API 頁讀不到內容（未確認） |
| 預測 | 能力在 Client 先啟動，Server 拒絕時撤銷；大多數有時效的 GE 能隨被拒的能力回滾，**Instant 的屬性與 tag 變化不能**；Execution Policy：Local Predicted／Local Only／Server Initiated／Server Only | Understanding GAS、Using Gameplay Abilities（官方文件） |
| 預測鍵 | GE 只在有有效 prediction key 時在 Client 套用；**不預測**：GE 的移除、週期效果（DoT） | [FPredictionKey（UE 4.27 API）](https://docs.unrealengine.com/4.27/API/Plugins/GameplayAbilities/FPredictionKey/index.html)（官方 API，UE4 版；UE5 是否相同未確認） |
| 被動 | Lyra 用「On Spawn 啟動的能力」（例 `GA_Weapon_AutoReload`）；常駐狀態也可用 Infinite 的 GE | Abilities in Lyra、Gameplay Effects（官方文件） |
| 授予與輸入 | `ULyraAbilitySet`（資料資產：能力＋等級＋input tag、GE、Attribute Set），由 Experience 的 PawnData、`GameFeatureAction_AddAbilities` 或裝備授予；輸入用 input tag（例 `InputTag.Ability.Dash`）觸發；`ActivationPolicy`：OnInputTriggered／WhileInputActive／OnSpawn | Abilities in Lyra、[Lyra Input Settings](https://dev.epicgames.com/documentation/unreal-engine/lyra-input-settings-in-unreal-engine)（官方文件） |

**GAS 和 GYO 的根本差別**：GAS 的預測是「Client 先做、Server 確認或撤銷」，要 prediction key 把兩端的副作用對起來，所以才有「哪些能預測、哪些不能」的清單。GYO 是**輸入驅動的狀態回滾**：兩端跑同一個 System、吃同一筆輸入（輸入有歷史，`authority.zh-Hant.md` 第 4 節），預測錯就把整個狀態放回去重跑。所以 GYO **不需要 prediction key**，也沒有「Instant 的效果不能回滾」的限制——瞬移改的 `PlayerBody` 本來就在回滾範圍裡。GAS 的價值在**資料的切法**（能力／效果／屬性分開、冷卻是有時效的效果、base 與 current 分開），這部分可以取。

### 2.2 lightyear（Bevy 生態的網路，源碼 `lightyear` 736e1695）

| 主題 | 做法 | 出處 |
|---|---|---|
| 預測的冷卻 | `Weapon { last_fire_tick: Option<Tick> }`，登記 `.replicate().predict()`；發射時 `tick - last_fire_tick` 換算成時間，比最小間隔；**記開始的 tick** | `demos/projectiles/src/protocol.rs:92-94,167`；`demos/projectiles/src/shared.rs:215-223`（源碼） |
| 重跑時照樣跑 | 發射的 observer 在回滾重跑時也跑；`Weapon` 被預測，所以重跑前冷卻狀態已還原 | `shared.rs:203-207`（源碼） |
| 不預測的冷卻 | 另一個例子用 Server 本地的 `GrabCooldown { until_secs }`（結束時間、浮點秒），註明不複製 | `examples/timeline_switch/src/server.rs:27-31,100-118`（源碼） |

和 GYO 一樣：冷卻是被預測的 component，回滾時自動還原；判定在兩端跑同一段程式。

### 2.3 Unity Netcode for Entities（源碼鏡像 `com.unity.netcode` 7.2.0）

| 主題 | 做法 | 出處 |
|---|---|---|
| 預測迴圈 | `PredictedSimulationSystemGroup` 兩端都跑，是「準決定性」的步；收到快照後從最舊的 tick 重跑 | `Documentation~/intro-to-prediction.md:93,113`；`prediction-n4e.md:13-19`（源碼內文件） |
| 只做一次的事 | `NetworkTime.IsFirstTimeFullyPredictingTick` 擋住只該發生一次的事（預測生成、特效與聲音） | `prediction-n4e.md:27` |
| tick 的比較 | `NetworkTick.TicksSince`、`IsNewerThan`（環繞安全的比較） | `Runtime/PredictionTicking/NetworkTick.cs:150,184` |
| 「上次發射的 tick」 | 測試裡把 `lastFire` 放在 command（輸入）資料 | `Tests/Editor/Physics/LagCompensationTests.cs:48,289,448` |
| **預測中增刪 component** | 預測中移除再加回被複製的 component，備份與快照對不上，值可能還原成預設；建議**避免增刪被複製的 component** | `Documentation~/prediction-details.md:108-168`（「Mitigation」`:162-168`） |
| 回滾上限 | 輸入佇列 64 步 | `prediction-details.md:13` |

對 GYO 的意義：**有時效的修飾、技能狀態用固定大小的欄位放在一個 component 裡**，不要每個 buff 增刪一個 component 或生成一個實體。GYO 的回滾能還原 component 的有無嗎？`StateRollbackStrategy` 逐實體逐元件比對（`rollback.zh-Hant.md`），增刪在預測中的行為本研究沒查到測試，**未確認**；固定欄位就不必依賴它。

### 2.4 Bevy 生態

| 套件 | 做法 | 對 GYO | 出處（查證等級） |
|---|---|---|---|
| `leafwing_abilities` 0.13.0（2026-11-22，bevy 0.19） | `Abilitylike`（通常是無資料的 enum）、`CooldownState<A>`（`HashMap<A, Cooldown>`＋全域冷卻）、`ChargeState`、資源池（Life／Mana）；`Cooldown { max_time, elapsed_time }` 用 `Res<Time>` 的 delta 在 `PreUpdate` 累加；**不跑效果**：效果由使用者自己的 System 寫，`trigger` 只扣冷卻、次數與成本 | 「定義／效果分開」與 GYO 一致；但**用幀時間、浮點秒、HashMap**，沒有提到網路、決定性、回滾 → 不能照搬，只取形狀（次數 charges、全域冷卻是可參考的選項） | [docs.rs](https://docs.rs/leafwing_abilities)、`src/leafwing_abilities/cooldown.rs`、`systems.rs`、`plugin.rs`（docs.rs 上的源碼） |
| `bevy_gauge` 0.6.0（2026-11-10，bevy 0.20） | 依賴圖的屬性系統（`"MaxHealth" => "Vitality * 10.0 + 50.0"`）；每個節點用 `Sum` 或 `Product`（每個修飾算 `1 + 值`）歸約；**修飾值先用 `f32::total_cmp` 排序再歸約**，為了不同套用順序在兩台機器上逐位元相同 | 唯一明說決定性的套件；排序歸約是「同一數值被多處改」的一種解法 | [docs.rs crate](https://docs.rs/crate/bevy_gauge/latest)、`node/struct.AttributeNode.html`、`node/enum.ReduceFn.html`（官方文件） |
| `bevy_stat_query` 0.5.0 | 只用無序的運算（加、乘、min、max、or）；有整數、分數與捨入的數值型別；查詢時才算、不快取 | 「只用可交換的運算」＝順序無關；整數型別合 GYO 的千分比 | [docs.rs](https://docs.rs/bevy_stat_query)（官方文件） |
| `immediate_stats` 0.6.0 | `Stat { base: i32, bonus: i32, multiplier: f32 }`，`(base + bonus) × multiplier`；**每幀把修飾重設**，buff 每幀重新套 | 「每步從基準重算」，不在原值上累乘 | [docs.rs](https://docs.rs/immediate_stats)（官方文件） |

其他 GAS 式的 Bevy 套件：web-fetch 子代理沒有搜尋工具，沒查（**未確認**）。結論：**Bevy 生態沒有同時處理冷卻、修飾與網路回滾的成熟套件**；能取的是 `leafwing_abilities` 的「定義與效果分開、效果由使用者 System 寫」和 `bevy_gauge`／`bevy_stat_query`／`immediate_stats` 的疊加做法。

### 2.5 共同點與 GYO 的取捨

| 共同點 | 取不取 | 理由 |
|---|---|---|
| 定義是資料、效果是程式（GAS、leafwing、Lyra 的 AbilitySet） | 取 | 和主會話初步方向一致 |
| 冷卻是有時效的狀態，在預測範圍內（GAS 的冷卻 GE 被預測、lightyear 的 `Weapon` 被預測） | 取 | GYO 的回滾自動涵蓋 |
| 數值不在原值上累改，而是「基準＋修飾集合」每次算（GAS 的 base／current、immediate_stats 每幀重設、bevy_gauge 依賴圖） | 取 | 第 4 節 |
| 疊加用可交換的運算或排序歸約（bevy_stat_query、bevy_gauge） | 取 | 第 4 節 |
| prediction key、「能預測／不能預測」清單（GAS） | 不取 | GYO 是狀態回滾，兩端同一個 System（2.1 末段） |
| 冷卻用幀時間或浮點秒（leafwing、lightyear timeline_switch） | 不取 | GYO 的冷卻已是 tick（`nextShotTick`），決定性與回滾 |
| tag 系統（GAS 的階層 tag、bevy_gauge 的 tag） | 先不取 | 第一批的例子用不到；需要時放位元（決策點 K5） |
| 每個 buff 一個 component 或實體（GAS 的 Active GE 容器是陣列） | 不取 | Unity 指出預測中增刪的問題（2.3） |

---

## 3. 同步、預測、回滾

**前提：作者能處理。** Engine 已給：權威登記（進 Kernel、進回滾、進 digest）、帶版本號的複製、元件輸入的歷史、狀態回滾。技能只要照現有玩家狀態的做法登記。

### 3.1 狀態放哪裡

| 資料 | 種類 | 放哪裡 | 進 digest | 同步 |
|---|---|---|---|---|
| 技能定義 | 內容 | 規則表 `skills`（模式檔的 `rules`） | 進**規則 digest**（內容比對，不是權威 digest） | 不同步，兩端各讀 |
| 裝了哪些技能（loadout） | 第一批：內容（模式固定）；之後：狀態（玩家自選） | 第一批：`sets.skills` 的設定；之後：component `SkillLoadout` | 內容時進規則 digest；狀態時進權威 digest | 狀態時 `Replicate` |
| 冷卻、蓄力開始、有時效的修飾、空中已跳次數 | 狀態 | component `SkillSlots`（固定大小：N 個欄位，每欄 `readyTick`、`chargeStartTick`；M 個修飾欄 `{定義索引, startTick, endTick}`；`airJumpsUsed`） | **進權威 digest**（`AuthorityEntry::Component`） | `Replicate`（帶版本號的 Codec） |
| 有效數值（速度、跳高、空中可跳次數） | 衍生 | 不存；每步由第 4 節的函式算 | 不進（從上面幾項算得出） | 不同步 |
| 輸入 | 判定的輸入 | `PlayerInput` 加技能欄位的「按下」與「按住」位元 | 進（現在 `PlayerInput` 就進） | 輸入通道 |

「`SkillSlots` 只在 `skills` 玩法組開著時放到玩家身上」：核心 PvP 不開，沒有格子，權威 digest 不變（#367 第 4 節第 2 點）。

### 3.2 冷卻的記法

| | 做法 | 好處 | 代價 | 誰這樣做 |
|---|---|---|---|---|
| (T1) **結束的 tick**（`readyTick`） | 起動時 `now + TicksFor(冷卻秒 × 修飾)`；判定 `now >= readyTick` | 和 `nextShotTick` 同一種寫法；「冷卻重置」（例：擊殺時清冷卻）只要 `readyTick = now`；判定只讀一個欄位 | 冷卻縮短的修飾只在起動時套用（冷卻中途裝上不影響這一次） | GYO `PlayerCombat`；GAS 的冷卻 GE 有時效（剩餘時間由 effect 算） |
| (T2) 開始的 tick（`castTick`） | 判定 `now - castTick >= TicksFor(冷卻 × 修飾)` | 冷卻縮短中途也生效 | 每次判定都要算修飾；「重置」要寫成把 `castTick` 往前推 | lightyear `last_fire_tick` |
| (T3) 剩餘 tick 倒數 | 每步減一 | 直觀 | 每步都要寫（每步都改狀態、每步都要比對）；跳步、重跑時容易錯 | leafwing（用幀時間累加） |

**推薦 (T1)**：沿用現有寫法；畫面要的「開始的 tick」另記一欄（同 `lastShotTick`），給特效與聲音用，不參與判定。

### 3.3 預測與回滾的流程

1. **自己的玩家**：Client 照常預測 `UseSkills`、`MovePlayers`；Match 的 `SkillSlots`、`PlayerBody` 複製回來後，`StateRollbackStrategy` 比對，不同就放回那一步、寫回輸入、重跑（`rollback.zh-Hant.md` 一個例子）。技能狀態不需要另外的機制。
2. **別人的玩家**：Client 不預測，取 Match 的值；技能特效讀複製來的 `SkillSlots`（例：修飾的 `startTick` 變了就播加速特效）。
3. **只發生一次的事**（聲音、特效）：不在 sim 裡發事件，由畫面讀狀態的「開始 tick」判斷（同 `lastShotTick`、`HitRecord::tick` 的做法，`include/object_fps_pvp/present/HitFeedback.hpp:107`）。Unity 用 `IsFirstTimeFullyPredictingTick` 擋重跑時重播（2.3）；GYO 讀狀態的做法在重跑時天然不重播（同一個 tick 值）。預測錯時特效可能已播又被撤回——和現在開槍的預測一樣，接受。
4. **輸入**：技能的按下與按住走 `PlayerInput`，有歷史（伺服器權威代為登記 `rollback.Input`），重跑時寫回，蓄力的「按了多久」由 `chargeStartTick` 與當步的按住位元重現。

### 3.4 瞬間移動要不要 Match 另外檢查

**前提：作者能處理。** 不另外檢查：`Blink` 效果在兩端是同一段程式——沿視線找目的地、用 `Fits`（`PvpWorld.cpp:278`）檢查站不站得下、站不下就縮短或不發動。Client 預測和 Match 用同一份牆（`wallsDigest` 已比對過），結果相同；不同的只有別的玩家的位置（Client 看到的是內插後的別人），這和現在的移動預測是同一類誤差，由回滾修正。

畫面要處理的：別人瞬移時，內插不能從 A 平滑拉到 B（`src/present/Interpolation.cpp`）；要一個「這一步是瞬移」的標記（`SkillSlots` 裡瞬移欄位的 `startTick`），內插看到就直接跳過去。這是畫面的事，不進 sim 的判定。

### 3.5 和肢體傷害 (Z4) 的關係

(Z4)「腿受傷變慢、手受傷換彈變慢」和技能的修飾是**同一個問題**：都是「某個來源讓移動或換彈的數值變了」。建議 (Z4) 做的時候也走第 4 節的同一個函式（來源多一種：部位狀態），不要另寫一套「腿傷 ×0.7」。這樣 `MoveSpeed` 只有一個入口，技能、傷、裝備的重量都是它的修飾來源。

---

## 4. 修飾怎麼疊加、怎麼避免同一個數值被多處各自改

**前提：作者能處理。** 疊加公式是遊戲規則（GAS、PoE 式、bevy_gauge 各不同）；Engine 只要提供 Hasher 與決定性的執行順序（已有）。

### 4.1 避免多處各自改：一個入口、每步從基準重算

- 每個能被改的數值（移動速度、跳高、空中可跳次數、冷卻倍率…）**只有一個函式算有效值**，例 `EffectiveMovement(基準規則, 裝備的被動, 有時效的修飾, now)`；`MovePlayers`、`UseSkills`、畫面都呼叫它。
- **從不在原值上累改**（不寫 `speed *= 1.2`）：每步從基準重算，修飾過期就自然消失，不會「乘了忘了除」或浮點漂移。這是 immediate_stats 的「每幀重設」和 GAS 的「current 由 base 與有效 GE 算出」的共同做法（2.4、2.1）。
- 現在 `MoveSpeed(arena, rules, crouched)` 已經是單一入口（`MovementRules.hpp:33`），擴充它即可；arena 規則欄位拆出之後，基準來自模式規則表。
- 不加守衛擋「有人另外改速度」（不加防誤用的守衛）；由設計審查看「有沒有第二個入口」。

### 4.2 疊加的選項

| | 公式 | 好處 | 代價 | 誰 |
|---|---|---|---|---|
| (S1) **加總式** | `有效值 = (基準 + ΣAdd) × (1000 + ΣMul‰) / 1000` | 兩個 Σ 都是整數加法，**和修飾的順序無關**；只有最後一次浮點運算；千分比和 `CombatRules` 的 `headPermille` 同寫法；+20% 和 +30% 是 +50%，設計者好算 | 兩個 ×1.5 是 ×2.0 不是 ×2.25；要「真正相乘」的效果寫不出來 | GAS 同一 channel 的乘法修飾相加（△ 第三方說法，未確認）；PoE 的 increased；bevy_stat_query 只用無序運算 |
| (S2) 連乘 | `(基準 + ΣAdd) × Π(Mul)` | 「再快 50%」對任何速度都是 50% | 浮點連乘和順序有關，要固定順序（例：照欄位序）或像 bevy_gauge 先排序 | bevy_gauge `Product`；immediate_stats 的 `*=` |
| (S3) GAS 式分段 | Add→Mul→Div→FinalAdd，Override 短路，可多個 channel | 表達力最大 | 規則最多、設計者最難推算；第一批的例子用不到 | GAS（△ 第三方） |

**推薦 (S1)**：使用者的四個例子都寫得出來（加速 ×1.5、被動 ×1.2、空中次數 +1、蓄力是另一條路）；順序無關所以不用排序；之後要「真正相乘」時在 (S1) 後面加一段 Π（不改既有資料的意思）。最後的值照現在一樣存 float、算進 sim；跨平台不要求逐位元一致（#367 `mode-config-declaration.zh-Hant.md` 第 5 節的說明：權威 digest 裡的浮點跨平台、跨編譯器不保證相同；差異由 Match 權威修正），但同一平台、同一輸入要相同，(S1) 天然滿足。

### 4.3 整數的數值

「空中可跳次數」這類整數：`基準 + ΣAdd`，不乘；上限由定義或規則表給。冷卻倍率用 (S1) 的 Mul 部分，結果再 `TicksFor`（已是無條件進位、至少 1）。

---

## 5. 和模式配置（PR #367）的關係

**前提：作者能處理。** #367 已給「玩法組＋帶值的 run condition＋規則表」；技能照它掛，不需要新的機制。

| 掛什麼 | 怎麼掛 | 核心 PvP 模式 |
|---|---|---|
| 規則表 `skills` | 模式檔 `rules: { "skills": "rules/skills.json" }`；定義的解析值進 `rulesDigest` | 不列 |
| 玩法組 `skills` | `GameplaySet{ name = "skills", build = 登記 SkillSlots（authority＋Replicate）與 System, readMode = 讀 loadout }`；System `UseSkills` 進 `pvp.skills.fixed`，在 `MovePlayers` 之前；玩家生成時（組開著才）放 `SkillSlots` | 不開：沒有 `SkillSlots`、`UseSkills` 不跑、digest 不變 |
| 移動讀修飾 | `MovePlayers` 多一個 `Query<Read<SkillSlots>>`，`Get(id)` 為 `nullptr` 時用基準（Bevy 寫 `Option<&SkillSlots>`；GYO 的膠水是一個 Query＋一行判斷） | `Get` 永遠是 `nullptr`，結果和今天相同；但 `MovePlayers` 的存取多一項 → 一致性指紋改變（所有模式相同，兩端照樣連得上） |
| 模式檔例子 | `"sets": { "skills": { "loadout": ["blink", "sprint_boost", "double_jump"] } }` | `"sets": {}` |

和 **arena 規則欄位拆出**的關係：技能的修飾改的是「基準」，基準該在模式規則表（`movement`）。建議**拆出在前**，技能在後；反過來的話，技能先讀 `ArenaContent` 的速度與跳高，拆出時再改一次讀的地方（小，但是重工）。

---

## 6. 要不要新增 Engine 的機制

**前提：只有 Engine 做得到的才放 Engine。** 逐項看技能要的能力：

| 要的能力 | Engine 有沒有 | 位置 |
|---|---|---|
| 依模式開關一組 System 與 component | 有（#367 的帶值 run condition＋Kernel set） | PR #367 `engine/ecs/kernel/include/engine/kernel/Conditions.hpp` |
| 狀態進回滾、進 digest | 有（`AuthorityEntry::Component`） | `authority.zh-Hant.md` 第 2 節 |
| 帶版本號的複製 | 有（`NetEntry::Replicate`） | `PvpWorld.cpp:1124-1128` |
| 元件輸入的歷史（重跑時寫回按住／按下） | 有 | `authority.zh-Hant.md` 第 4 節 |
| 決定性的 tick | 有（`WT::Tick`、`TicksFor` 在遊戲） | `PvpWorld.cpp:427-433` |
| 「這個實體有沒有某 component」的可選讀取 | 沒有 `Option<T>` 項；**第二個 Query＋`Get` 可達到** | `Params.hpp:306-316` |
| 預測生成實體（例：丟出去的陷阱、投射物技能） | **未確認**：批次表〈後續小事〉記著「預設都帶 entity_ids 與回滾時，配發器沒有登記成可回滾」「網路複製的外來編號也要先動配發器的範圍」 | 批次表〈後續小事〉；lightyear 用 prespawn hash（`demos/projectiles/src/shared.rs:224`）、Unity 用 predicted spawn（`prediction-details.md:53-85`） |

**結論：使用者的四個例子都不需要新增 Engine 的機制。** 唯一可能的缺口是「會生成實體的技能」（投射物、陷阱、召喚），它碰到的是既有的配發器缺口，不是技能特有的；到有這種技能時，和網路複製那批一起看。`Option<T>` 查詢項只是方便，不是缺口（膠水一行），不建議為技能加。

---

## 7. 選項、推薦、分批、決策點

### 7.1 選項總表

| 題 | 選項 | 推薦 | 理由 |
|---|---|---|---|
| K1 跳躍規則在哪算 | (a) 搬進 sim：輸入送原始的 `jumpPressed`、`jumpHeld`，`StepJumps` 在 `StepBody` 裡判；(b) 維持 Client 算，技能另加位元 | **(a)** | 2 段跳要分「空中剛按下」與「一直按著」、蓄力要知道按住多久，(b) 每加一種都要在 Client 多算一種；規則在 Client 算也違反「權威在 Match」（Match 只信 Client 給的結果）。代價：`PlayerInput` 改格式（Codec v3）、`PlayerInput` 進 digest，核心 PvP 的 A/B 要改比法（7.3） |
| K2 冷卻記法 | (T1) 結束 tick／(T2) 開始 tick／(T3) 倒數 | **(T1)** | 3.2 |
| K3 疊加公式 | (S1) 加總式／(S2) 連乘／(S3) GAS 式 | **(S1)** | 4.2 |
| K4 裝備（loadout）從哪來 | (a) 第一批由模式固定（`sets.skills.loadout`），之後換成玩家自選時改成 component；(b) 現在就做 `SkillLoadout` component＋選擇介面 | **(a)** | 玩家自選要接存檔或背包（另一條線）；`SkillSlots` 的形狀不因來源改變，換來源只動「誰寫 loadout」 |
| K5 標籤 | (a) 不做，用明確欄位；(b) `SkillSlots` 裡放位元標籤；(c) GAS 式階層 tag | **(a)** | 四個例子用不到；「冷卻中」就是 `readyTick > now`。等到有「沉默」「互斥」再開 (b) |
| K6 輸入的形狀 | (a) 固定 N 個欄位，每欄一個按下＋一個按住位元；(b) 一個「使用技能 N」的位元組 | **(a)**，N＝4 | 蓄力要「按住」，(b) 表達不了同時按住一個、按下另一個；位元比位元組省、Codec 固定 |
| K7 欄位數與修飾欄數 | 固定大小（例：技能 4、有時效的修飾 4）／可變 | **固定** | Unity 指出預測中增刪的問題（2.3）；固定大小的 Codec 與 hash 最簡單；滿了怎麼辦（新的蓋最舊的或不發動）寫進定義 |
| K8 (Z4) 是否共用修飾函式 | 共用／(Z4) 另寫 | **共用** | 3.5 |

### 7.2 分批

```text
技能線
├─ S0 前置：arena 規則欄位拆出（已排，模式配置卡）        上段｜Opus／high
├─ S1 跳躍規則搬進 sim＋輸入送原始位元                     上段｜Opus／high
├─ S2 技能骨架＋移動加速（主動）＋移動速度（被動）         上段｜Opus／high
├─ S3 2 段跳（被動）＋蓄力跳（主動）                        下段｜Opus／high
└─ S4 瞬間移動（主動）＋別人瞬移時的內插                    下段｜Opus／high
```

| 代號 | 內容 | 上段／下段（根據） | 實作者／審查者／驗證者 | 驗收 |
|---|---|---|---|---|
| S0 | `v8-arch-gyo2-arena-rules-split`（已排） | 上段（資料格式） | 照該批 | 照該批 |
| S1 | `PlayerInput` 加 `jumpHeld`（`jump` 改成純「按下」），`StepJumps` 移進 `StepBody`；`hold_jump` 的意思不變；Codec v3 | **上段**：判定輸入的資料格式與 wire | Opus／high；設計審查 Opus／high；純淨 Code Review Fable／high；驗證者 Opus／high | 既有 `object_fps_pvp.*` 不改期望值；核心 PvP 的行為 A/B（7.3）；回滾情境：按住空格連跳時預測錯能還原 |
| S2 | 規則表 `skills`、玩法組 `skills`、`SkillSlots`（冷卻 (T1)、修飾欄）、`EffectiveMovement`（(S1)）、輸入的技能位元；效果：(E2) 移動加速、(E3) 移動速度被動 | **上段**：新的資料格式（規則表、`SkillSlots` 的 Codec）與權威登記 | 同 S1 | 核心 PvP A/B 相同；開技能的測試模式：冷卻、到期、預測錯回滾、別人看得到加速；規則 digest 不同時 `ContentMismatch` |
| S3 | `airJumpsUsed`、空中可跳次數（(E3)）、蓄力（(E4)：`chargeStartTick`、放開時算跳躍初速） | 下段（S2 已定好 `SkillSlots` 的形狀；只加欄位就要升 Codec，屬資料格式→若要改形狀則上段） | Opus／high；純淨 Code Review Fable／high | 2 段跳在連跳（按住）下不會在空中自動用掉；蓄力的強度在重跑時相同 |
| S4 | (E1) 瞬移：目的地查找＋`Fits`；內插的「瞬移」標記 | 下段（遊戲邏輯與畫面） | Opus／high；純淨 Code Review Fable／high | 撞牆時縮短；預測錯回滾；別人瞬移時畫面不拉線 |

檔位都用 high（使用者 2026-11-19：xhigh 只在需要調查時用）。S1～S4 都有 CI 與 A/B，不需要再升。

### 7.3 核心 PvP 的 A/B 在 S1 之後要怎麼比

#367 的 A/B 比每一步的權威 digest，`PlayerInput` 在裡面（`mode-config-declaration.zh-Hant.md` 第 5 節）。S1 改了 `PlayerInput` 的欄位，digest 一定不同。可選：

- (a) S1 的 A/B 比「扣掉 `PlayerInput` 的 digest」（`PlayerBody`、`PlayerCombat`、`ShotRecord`、`HitRecord`），並逐步比對新舊 `PlayerInput` 的對應（舊的 `jump` ＝ 新的 `StepJumps(jump, jumpHeld)`）；
- (b) S1 之後重取基準，A/B 從 S2 起用新的基準。

推薦 (a)：證明「搬位置不改行為」，(b) 只證明「之後不再改」。這是 S1 宣告裡的細節，列在這裡是因為它影響回歸基準的定義。

### 7.4 要使用者決定的點

| # | 問題 | 選項 | 推薦 |
|---|---|---|---|
| **K1** | 跳躍規則（含連跳）搬進 sim、輸入改送原始的按下與按住？ | (a) 搬／(b) 維持 Client 算 | (a)；代價是輸入格式改版、A/B 的比法改（7.3） |
| **K2** | 冷卻用結束的 tick？ | (T1)／(T2)／(T3) | (T1)，沿用 `nextShotTick` |
| **K3** | 修飾的疊加公式？ | (S1) 加總式／(S2) 連乘／(S3) GAS 式 | (S1) |
| **K4** | 第一批的技能裝備由模式固定？ | (a) 模式固定／(b) 現在就做玩家自選 | (a) |
| **K5** | 標籤先不做？ | (a)／(b)／(c) | (a) |
| **K6** | 輸入用固定 4 欄、每欄按下＋按住？ | (a)／(b) | (a)，N＝4（數字可改） |
| **K7** | 技能欄與修飾欄固定大小？ | 固定／可變 | 固定 |
| **K8** | 肢體傷害 (Z4) 做時共用同一個修飾函式？ | 共用／另寫 | 共用（這題可以等開 (Z4) 時再定） |
| **時機** | 技能線排在哪裡？ | 例：arena 規則拆出之後、W1 之前或之後 | 本卡不推薦（排程是主會話與使用者的事）；只提醒 S0 在 S1 之前、S1 在 S2 之前 |

K2、K5、K6、K7 照 AGENTS §2.6 是細節、可由實作卡自判；列出來是因為它們決定 `SkillSlots` 與 `PlayerInput` 的 wire 形狀（上段），使用者想改時改得最便宜的時機是現在。

---

## 8. 沒查到、未確認的

| 項目 | 狀態 |
|---|---|
| GAS 的 API 參考頁（`UGameplayAbility` 的冷卻函式、`EGameplayModOp`、`FAggregatorModChannel`）內容 | 頁面由 JS 載入，讀不到；疊加公式只有第三方來源 |
| GAS 社群文件（tranek GASDocumentation） | GitHub 回 503，沒讀到 |
| UE5 的 `FPredictionKey` 「不預測」清單是否和 UE 4.27 相同 | 未確認 |
| 冷卻 GE 存的是「開始＋時長」還是剩餘時間 | 未確認 |
| Bevy 生態有沒有其他 GAS 式套件 | 沒搜尋（子代理沒有搜尋工具） |
| GYO 的 `StateRollbackStrategy` 能不能正確還原「預測中增刪的 component」 | 沒查測試；固定欄位的設計不依賴它 |
| 不存在的 Resource 會不會被複製 | #367 自己列為未確認（`Replication.cpp:299-305`），技能不用 Resource，不受影響 |

## 9. 對其他文件與批次的影響（如果採用推薦）

- 批次表〈後續小事〉：加「技能線（S1～S4）」一列，前置 S0；S1 會改 `PlayerInput`，排在其他也改 `PlayerInput` 的批（例：W1 的武器切換輸入）之前或併在一起，免得 Codec 連升兩次。
- 肢體傷害 (Z4)：開那批時照 K8 走同一個修飾函式。
- `MovementRules.hpp:37-41` 的註解「The step's input carries the answer as its jump press」會在 S1 改掉。
