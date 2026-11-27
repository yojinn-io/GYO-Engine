# 肢體傷害的規則與判定：研究（`v8-arch-limb-damage-research`）

研究卡，2026-11-27。只研究、只改文件；交出決策點，不寫程式，不替使用者寫「使用者的決定」。

## 0. 前提、範圍、判斷依據

### 0.1 已定的前提（這份研究不推翻它們）

| 前提 | 出處 |
|---|---|
| 最終產品是塔科夫型撤離射擊＋怪物；Object_PvP 只驗核心 PvP 交互與地圖 | HANDOFF `v8-arch-gyo2-object-pvp-route-a` |
| 路線 (A)：新功能都加在 Object_PvP，由 arena／模式資料開關；現行規則留成「核心 PvP」模式，S5 量測當回歸基準 | 同上 |
| 肢體傷害採 (3)：手臂是獨立部位，有自己的倍率和血量；Engine 網路協議不改 | HANDOFF `v8-arch-gyo2-limb-damage` |
| #363：D5 (b) Match 也跑 IK；D6 Match 用 crossfade 中的姿勢擺判定形狀；D7 PV3 之後再做，做時存擺好的判定形狀 | HANDOFF `v8-arch-gyo2-anim-layers-decisions` |
| D9：手臂一定加判定，不設門檻；量測拿來調膠囊半徑和倍率 | HANDOFF `v8-arch-gyo2-anim-d9-no-threshold` |
| 10-31「手臂不設判定」（`v8-product-hit-rules-three-parts`）的理由是「沒有塔科夫級的肢體傷害」；這個前提已不成立。這裡只引用決定與理由，不引用 master 的算法、常數、數據 | #363 第 10.1 節 |

### 0.2 判斷依據

1. Engine 只給機制；部位、倍率、血量、傷害轉移都是遊戲規則，由作者（遊戲）選。
2. 核心 PvP 模式的行為要留得住（S5 的回歸基準）。
3. 參考成熟方案：網路與 ECS 看 lightyear、Unity Netcode for Entities、bevy_replicon、Bevy（生態：avian）；部位的切法看 Source 引擎與塔科夫類遊戲（只有二手資料）。
4. 查證等級：**源碼**（本機 `~/Code/GitHome`，附檔名與行號）、**官方文件**、**△ 社群 wiki／報導**、**未確認**。

### 0.3 gyo2 現況一覽（後面各題共用）

| 什麼 | 現況 | 位置 |
|---|---|---|
| 部位 | `enum class HitPart : std::uint8_t { None, Body, Head, Leg }` | `games/object_fps_pvp/include/object_fps_pvp/sim/CombatRules.hpp:16` |
| 倍率 | 頭 2000‰、軀幹 1000‰、腿 750‰，寫在 `CombatRules` 的預設值（沒有從資料讀） | `CombatRules.hpp:25-27` |
| 傷害 | `ShotDamage`：`switch (part)` 取倍率，封頂後不超過目標剩下的血 | `games/object_fps_pvp/src/sim/CombatRules.cpp:13-24` |
| 同距的優先 | `PartRank`：頭＞軀幹＞腿 | `CombatRules.cpp:39-47` |
| 血量 | 一個總血量 `PlayerCombat::hp`（milli-points） | `games/object_fps_pvp/include/object_fps_pvp/world/Components.hpp:52-53` |
| 形狀 | `hitbox.json`：頭 1 球、軀幹 2 膠囊（`spine_01→spine_03`、`pelvis→spine_01`）、腿 4 膠囊，共 7 個；沒有手臂 | `assets/object_fps_pvp/characters/player/hitbox.json` |
| 形狀的部位名 | 載入時只認 `head`／`body`／`leg`，其他名字報錯 | `games/object_fps_pvp/src/character/CharacterRig.cpp:219-220` |
| 判定 | 每一發，對每個活著的目標：推姿勢→`ComposeBody`→`ModelSpace`→`PlaceHitShapes`→逐個膠囊打射線，取最近（同距看 `PartRank`） | `games/object_fps_pvp/src/world/PvpWorld.cpp:470-546`（迴圈 `495-521`） |
| 交換內容 | `ShotRecord`、`HitRecord` 都帶 `part`；登記 `Replicate`，Codec 逐欄位寫，版本號 1 | `Components.hpp:70-88`、`PvpWorld.cpp:169-192`、`PvpWorld.cpp:1124-1128` |
| 權威 hash | `HashValue` 把 `part` 當 u8 hash；兩個元件都登記成 authority component | `PvpWorld.cpp:81,89`、`PvpWorld.cpp:1092-1095` |
| Codec 版本號 | 進 protocol hash；版本號不同的兩端連不上 | `engine/modules/net/include/engine/net/Codec.hpp:104-112` |
| enum 的解碼 | 讀底層整數後直接 cast，不檢查範圍 | `Codec.hpp:57-61` |
| Client 的讀者 | 受擊片段只分頭與其他；命中標記只看是不是頭 | `games/object_fps_pvp/src/sim/PlayerAnimation.cpp:171`、`games/object_fps_pvp/src/client/ClientWorld.cpp:972` |
| 內容比對 | Client 與 Match 只比 arena id 與牆的 digest；`hitbox.json`、`CombatRules` 不比 | `Components.hpp:148-155`、`PvpWorld.cpp:810-820` |
| 模式 | 還沒有「模式」這個資料；`arenas.json` 只列 arena 檔 | `assets/object_fps_pvp/maps/arenas.json` |

---

## 1. 部位清單要不要資料化

**前提**：這是**作者（遊戲）能處理的**。部位是遊戲的列舉或資料，交換時是遊戲自己的 component 和 Codec；Engine 的網路只搬 bytes、管版本號（`Codec.hpp:104-112`），不認得部位。所以這一題**不產生 Engine 的批次**。

### 1.1 現況：`HitPart` 寫死時，加一個部位要動哪裡

| # | 位置 | 加 `Arm` 要做的 |
|---|---|---|
| 1 | `CombatRules.hpp:16` | enum 加值 |
| 2 | `CombatRules.hpp:25-27` | 加 `armPermille` |
| 3 | `CombatRules.cpp:13-24`、`39-47` | `ShotDamage`、`PartRank` 的 `switch` 各加一個 case |
| 4 | `CharacterRig.cpp:219-220` | 認得 `arm` |
| 5 | `PvpWorld.cpp:169-192` | `ShotCodec`、`HitCodec` 的版本號 1→2（bytes 沒變，意思變了：舊 Client 收到 4 會照 `Codec.hpp:57-61` 直接 cast） |
| 6 | 測試、突變 | `tests/object_fps_pvp/RulesTests.cpp:140-197`、`world/WorldTests.cpp:183,492`；`games/object_fps_pvp/docs/mutations.json:64,182,2878` 的 find 字串 |
| — | `PlayerAnimation.cpp:171`、`ClientWorld.cpp:972` | 不必改（`Arm` 走「其他」） |

**代價本身不大**（約 6 處）。真正的問題在路線 (A)：

- 核心 PvP 模式要**留住現在的判定**：沒有手臂形狀（或有但不判）、3 個部位、一個總血量。撤離模式要 7 個部位。**同一個 `hitbox.json` 要在兩個模式下有不同的結果**，所以「哪些形狀判、算哪個部位」無論如何都要由模式決定。只加 enum 值解決不了這件事。
- 從 4 個再到 7 個（胸、腹、左右手、左右腳）時，`Body`、`Leg` 會變成「只有核心模式用的群組值」，和 `Thorax`、`LeftLeg` 這些細的值混在同一個 enum 裡。每多一個部位，上表 1～6 再走一遍。

### 1.2 別家怎麼做

| 誰 | 做法 | 查證 |
|---|---|---|
| **lightyear**（avian 整合） | 框架不定義部位。回溯的單位是「一個 collider entity」：每個要回溯的 collider 有自己的 `LagCompensationHistory`，查詢回傳被打中的 entity（`crates/integration/avian/src/lag_compensation/history.rs:69-72`、`query.rs:212-243`）。要分部位，就是每個部位一個 collider entity，部位是掛在那個 entity 上的 component（作者的資料） | 源碼 |
| **avian**（Bevy 生態的物理） | 子 collider 用 `ColliderOf` 關聯到同一個 rigid body（`avian/src/collision/collider/collider_hierarchy/mod.rs:41-53`）。一個角色多個子 collider，部位資訊放在子 entity 上 | 源碼 |
| **Unity Netcode for Entities** | 回溯整個 `CollisionWorld`（`Runtime/Physics/PhysicsWorldHistory.cs:41-57,118`）；官方範例打到的是 `Entity`，沒有部位的概念（`Tests/Editor/DocCodeSamples/server-rewind.cs`）。部位要作者自己從 collider entity 推 | 源碼、官方文件 |
| **bevy_replicon**、**Bevy** | 沒有 hitbox 或部位的機制；replicon 的 README 只指向第三方的 `bevy_rewind`（`bevy_replicon/README.md:58-60`） | 源碼 |
| **Source 引擎**（CS、TF2、Garry's Mod） | **固定的細部位列舉**（hitgroup：generic 0、head 1、chest 2、stomach 3、左右手 4/5、左右腳 6/7、gear 10）；**模型資料**把每個 hitbox 指到一個 hitgroup；**遊戲規則**決定每個 hitgroup 的倍率（Garry's Mod 的 `ScalePlayerDamage` hook） | 官方文件（https://developer.valvesoftware.com/wiki/Team_Fortress_2/Scripting/Script_Functions/Constants）、△ 社群 wiki（https://wiki.facepunch.com/gmod/Enums/HITGROUP）；SDK 原始檔本卡沒在本機查 |

共同點：

1. **框架都不定義部位**；部位是作者的資料。
2. 有部位的遊戲裡，最久經考驗的是 Source 的形狀：**「解剖上的部位」是一個少量、固定的列舉，掛在模型資料上；「規則上的部位」（倍率、血量）是規則的事**。兩者分開，換規則不必改模型，換模型不必改規則。

### 1.3 選項

把「部位」拆成兩件事來看：

- **解剖標籤**：這個形狀在身體的哪裡（頭、胸、腹、左上臂…）。屬於**角色**（`hitbox.json`），和模式無關。
- **規則部位**：這一發算哪個部位、乘多少、扣哪一格血、會不會死。屬於**模式**。

| | 做法 | 好處 | 代價 |
|---|---|---|---|
| (1a) | **維持寫死的 enum，加 `Arm`**；模式只決定「手臂形狀判不判」 | 改動最少（1.1 節的 6 處）；Client 照舊 | 到 7 個部位時 enum 混著群組值和細的值；每加一個部位走一遍 1.1 節；倍率、血量仍寫在程式裡，模式要換倍率就要改程式 |
| (1b) | **固定的解剖標籤 enum（Source 的 hitgroup 形狀）＋模式資料的規則表**：`hitbox.json` 的每個形狀標一個解剖標籤（頭、胸、腹、左右上臂、左右前臂…或左右手、左右腳）；模式資料把標籤對到規則部位，帶倍率、血池、同距優先、表現標籤（頭／其他）；交換內容傳解剖標籤 | 解剖標籤的意思在兩端編進程式，**不必比內容**就能解讀；核心 PvP 模式＝「胸＋腹→軀幹、左右腳→腿、手臂→不判」，留得住現在的結果；撤離模式只換資料 | 一次性的改造比 (1a) 大：`hitbox.json` 的 `body`／`leg` 改成細的標籤（核心模式用表合回去）、`ShotDamage`／`PartRank` 改查表、Codec 升版本號；加新的解剖標籤（例如脖子）仍要改程式，但很少發生 |
| (1c) | **全部資料化**：部位清單本身（名字、數量）在模式資料；交換內容傳「在清單裡的索引」 | 加部位完全不改程式 | 索引的意思由資料決定：**Client 和 Match 的部位清單必須相同**，要把模式資料的 digest 放進比對（現在只比 arena 與牆，`PvpWorld.cpp:810-820`）；解碼不檢查範圍（`Codec.hpp:57-61`），Client 要處理超出清單的索引；Client 的受擊片段、命中標記也要改成看資料的表現標籤 |

各選項對 1 題要求的影響點：

| 影響點 | (1a) | (1b) | (1c) |
|---|---|---|---|
| `ShotRecord`／`HitRecord` 的 Codec | bytes 不變（u8），版本號升 | bytes 不變（u8），版本號升一次 | bytes 不變（u8），版本號升一次；意思隨模式資料變 |
| `Replicate` 登記（`PvpWorld.cpp:1124-1128`） | 不變 | 不變；部位血量另加 `LimbHealth`（第 3 題），只在用它的模式掛 | 同 (1b) |
| 權威 hash（`PvpWorld.cpp:81,89`） | 不變（仍 hash u8） | 不變 | 不變；但兩端的清單不同時 hash 照樣一致、意思卻不同，所以要靠內容比對擋 |
| Client 受擊片段（`PlayerAnimation.cpp:171`）、命中標記（`ClientWorld.cpp:972`） | 不必改 | 改成看規則表的表現標籤，或維持「是不是頭標籤」 | 必須改成看資料的表現標籤 |
| 內容比對 | 不必 | 規則表影響的只有 Match 算的傷害（傷害值本身已在 `ShotRecord::damage` 裡複製），Client 不必比；但 UI 要顯示部位血量時要知道血池清單 → 模式要比 | **必須**把部位清單放進比對 |

### 1.4 和「模式資料開關」的銜接

批次表〈後續小事〉的「模式資料開關」（`rebuild-batch-plan.zh-Hant.md:88`）還沒設計。這一題給它的需求：

1. **模式要有身分，兩端要一致**：模式 id＋規則 digest 放進 `MatchInfo`（和 `wallsDigest` 同一種做法，`Components.hpp:148-155`），不一致就當內容不符（`PvpWorld.cpp:810-820`）。(1b) 下為了 UI 的血池清單需要；(1c) 下為了解讀部位索引必須。
2. **模式帶一張傷害規則表**（倍率、血池、致命與否、轉移係數、同距優先、表現標籤），就是現在寫死在 `CombatRules` 裡的那幾個數字的去處（`CombatRules.hpp:25-27`）。
3. **模式決定掛哪些 component**：`LimbHealth` 只在撤離模式掛（路線 (A) 的「不用的模式不掛」）。
4. **核心 PvP 模式的規則表＝現在的數字**，S5 的結果要一致。

順序照批次表：模式資料開關先，肢體傷害（G6）後。

### 1.5 推薦：(1b)

1. 解剖和規則分開，是 Source 這種有十多年部位傷害經驗的做法；三個網路框架也都把部位留給作者的資料（1.2）。
2. 路線 (A) 本來就要求「同一份角色資料、不同模式不同規則」；(1a) 做不到這件事，最後還是要補一張表。
3. (1c) 的彈性（加部位不改程式）用不太到：人形角色的解剖部位很少變；代價（內容比對、範圍檢查、Client 改看資料）卻是現在就要付。
4. 解剖標籤的粒度建議照最細的需求先定一次：**頭、胸、腹、左上臂、左前臂、右上臂、右前臂、左大腿、左小腿、右大腿、右小腿**（和 11 個形狀一一對應），規則表再把它們合成 7 個（撤離）或 3 個（核心）部位。合併的方向是資料，拆開的方向才要改程式。

### 1.6 決策點

- **L1**：部位清單用 (1a)、(1b)、(1c) 哪一個？（推薦 (1b)）
- **L1'**（採 (1b) 時）：解剖標籤的粒度：照 11 個形狀（上臂、前臂分開），還是 7 個（手、腳不分上下）？推薦 11 個：形狀本來就是 11 個，合併是資料的事；之後若要「打前臂只扣手」以外的規則（例如護甲只護上臂），不必再改程式。

---

## 2. 延遲補償（PV3）的時機

**前提**：這是**作者（遊戲）能處理的**。Engine 已經給了機制：Client 每格輸入帶畫面的時間 `ViewTime`（`engine/modules/net/include/engine/net/Replication.hpp:396-404`），Match 消耗輸入時把它寫進 `PeerInput<T>::lastView`（`Replication.hpp:559`）。存什麼歷史、怎麼插值、回溯多久，是遊戲的規則（port 宣告 PV3：「之後做回溯時不用改 wire」，`games/object_fps_pvp/docs/port-declaration.zh-Hant.md:53`）。所以這一題**不產生 Engine 的批次**。

### 2.1 現況

- Match 用**當 tick** 的位置與姿勢判（`PvpWorld.cpp:495-521`）；`ViewTime` 有送，Match 沒讀（`port-declaration.zh-Hant.md:134`）。
- Client 看到的對手落後。落後量（推論，沒量）：
  - 對手的畫面時間＝Match 現在的 tick 估計 − 半個 RTT − 內插延遲（`ClientWorld.cpp` 的 `InterpolateRemotes` 註解，約 `1582-1588` 行）。
  - 內插延遲＝max(送出間隔 1 tick × 1.2, 5 ms)＋jitter × 4＋1 tick（`games/object_fps_pvp/include/object_fps_pvp/present/Interpolation.hpp:26-33`）；64 Hz、jitter 0 時約 2.2 tick ≈ 34 ms。
  - 自己的輸入要提前送到 Match（約半個 RTT＋輸入的餘裕）。
  - 合起來，判定時的對手和畫面上的對手差 ≈ **RTT＋內插延遲＋輸入餘裕**。LAN（RTT 幾 ms）約 50 ms；RTT 60 ms 約 110 ms（輸入餘裕當 1 tick，未確認）。
- 對手橫移的速度上限是 3 m/s（各 arena 的 `movement_speed`，例 `assets/object_fps_pvp/maps/pvp_arena.json:7`），所以畫面和判定差 **LAN 約 0.15 m、RTT 60 ms 約 0.33 m**（推論）。
- 對照形狀的大小：頭的球半徑 0.184、軀幹膠囊 0.18、小腿 0.105（`hitbox.json`）；手臂膠囊的半徑還沒量（G5），以人體比例推約 0.05～0.08（推論）。**在 LAN 上，橫移中的手臂（直徑約 0.1～0.16 m）就已經小於落差**：沒有提前量就打不到；頭（直徑 0.37 m）還容得下。
- 站著不動的目標沒有這個落差，只剩姿勢的跨平台差（PV6）；但 PV6 在 LAN 上比 0.15 m 小得多（推論：PV6 是浮點差，不是幾個 tick 的位移）。**所以「看到打中卻沒算」在手臂上的主因是沒有回溯，不是 PV6**。

### 2.2 別家怎麼做

| 誰 | 做法 | 查證 |
|---|---|---|
| **lightyear** | 每個 collider 存 (Position, Rotation, AABB) 歷史，預設 35 tick（`history.rs:30-46`）；查詢先打「歷史 AABB 的包絡」當 broadphase，中了才在兩 tick 間 lerp／slerp 打真的形狀（`history.rs:121-145,176-230`、`query.rs:181-243,260-309`）；源碼明寫形狀假設不變、只取位姿（`query.rs:300`）。範例從射手的 `InterpolationDelay` 取回溯量（`examples/fps/src/server.rs:120-160`） | 源碼 |
| **Unity Netcode for Entities** | 每 tick 存一份 `CollisionWorld`，環形最多 32 份、預設 16（`PhysicsWorldHistory.cs:118`、`Runtime/Physics/LagCompensationConfig.cs:17-28`）；官方範例回溯量＝命令的 `InterpolationDelay`＋1 tick 的畫面延遲（`server-rewind.cs` 的 `additionalRenderDelay`）。官方文件：回溯只涵蓋物理狀態，其他影響結果的狀態（例如無敵）要自己追；歷史建議限在 250～500 ms，太長會讓被打的人「躲到牆後還被打中」（`Documentation~/server-rewind.md:27,37`） | 源碼、官方文件 |
| **Lyra** | Client 先判、伺服器確認（#363 第 5.2 節） | 官方文件 |
| **塔科夫** | 命中由哪一端判，本卡沒找到可靠出處 | 未確認 |

共同點：有回溯的都存**擺好的碰撞形狀**、限制回溯的長度、用 broadphase 省掉不必要的精確測試。#363 的 D7 已經選了這條（存擺好的判定形狀）。

### 2.3 選項

| | 做法 | 好處 | 代價 |
|---|---|---|---|
| (P1) | **維持 D7**：G6 之後再說，沒有排程 | 不加批次；Match 照現在只在有人開槍時擺姿勢 | 手臂在橫移時實際上要提前量才打得到（2.1）；撤離模式的手感會被「手臂很難打、打了沒算」主導，G5 的量測（調半徑與倍率）會被落差污染：調出來的數字是在補延遲，不是在補形狀 |
| (P2) | **排在 G6 之後、撤離模式第一次實機試玩之前**；回溯開不開由模式資料決定（核心 PvP 關，撤離開） | 核心 PvP 的 S5 基準不動；G5 可以分兩次量（回溯關／開），把「形狀不準」和「延遲」分開；G6 先做完，回溯存的就是最後的 11 個形狀 | 多一個上段的批次（判定）；Match 每 tick 要擺所有玩家的姿勢（第 5 題）；要定回溯上限（Unity 建議 250～500 ms）與超過時的處理 |
| (P3) | **提前到 G6 之前** | 手臂一加進來就是對的手感 | 回溯先存 7 個形狀，G6 再改成 11 個；G3（Match 跑 IK）之前存的姿勢沒有 IK，存了也要再改；排程上卡在 E2、G3 之前，會把 W 線往後推 |
| (P4) | Client 判、伺服器確認（Lyra） | Client 看到什麼就算什麼 | PV6：Match 拿自己的姿勢驗，手臂在骨鏈末端、誤差最大，邊緣誤判最多；#363 的 R5 已不推薦 |

### 2.4 推薦：(P2)

1. 手臂比頭細一半以上，沒有回溯時在 LAN 上就已經小於落差（2.1 的推論）；撤離模式要做部位血量，手臂扣不到血會讓 (3) 的設計看不出效果。
2. (P3) 會讓回溯的內容改兩次（7→11 個形狀、加 IK）；(P2) 等 G3、G6 完成再做，一次到位。這和 D7「PV3 之後再做」的方向相同，只是把「之後」從「沒有排程」改成「撤離模式試玩之前」。
3. 回溯開關放進模式資料，核心 PvP 模式照舊，S5 的回歸基準不受影響（路線 (A)）。

### 2.5 決策點

- **L2**：PV3 排在哪裡：(P1) 維持沒有排程、(P2) G6 之後、撤離模式試玩之前、(P3) G6 之前？（推薦 (P2)）
- **L2'**（採 (P2)、(P3) 時）：回溯的上限（tick 數或 ms）由模式資料帶，還是先寫一個固定值？推薦資料帶，預設值參考 Unity 的 250～500 ms；實際值用 LAN 場次量（第 5 題 M6）。

---

## 3. 傷害模型

**前提**：這是**作者（遊戲）能處理的**。部位血量、倍率、歸零後果全是遊戲規則；同步用的是遊戲自己的 component＋`Replicate`（HANDOFF `v8-arch-gyo2-limb-damage`）。**不產生 Engine 的批次**。

### 3.1 現況

- 一個總血量 `PlayerCombat::hp`；傷害＝`shotDamage × 部位倍率`、封頂、不超過剩下的血；歸零就死（`CombatRules.cpp:13-24`、`PvpWorld.cpp:532-540`）。
- 沒有部位血量、沒有傷害轉移、沒有斷肢的狀態效果。

### 3.2 別家怎麼做（都是二手資料）

| 遊戲 | 部位與血量 | 歸零之後 | 查證 |
|---|---|---|---|
| 塔科夫 | 7 個部位各有血量（頭、胸、腹、左手、右手、腳）；總和 440 的說法常見 | 頭或胸歸零即死；其他部位歸零（「黑掉」）後，再打到那裡的傷害乘一個係數、分給其他部位（社群 wiki 寫腹 1.05、手 0.49、腳 0.7，頁面沒標資料的版本）；斷手：換彈、拔槍、瞄準變慢，瞄準會晃；斷腳：走得慢、不能跑 | △ 社群 wiki（https://wikiwiki.jp/eft/%E4%BD%93%E5%8A%9B ，2026-07-01 更新，沒標資料版本）、△ 報導（https://www.gamepressure.com/escape-from-tarkov/health-and-injuries/z1cf2f ）；各部位的血量數字本卡沒查到可靠出處，**未確認** |
| Arena Breakout: Infinite | 和塔科夫同型：部位各有血量，總和 440，頭與胸致命 | 部位壞掉有狀態效果，要特定藥品 | △ 報導（https://charlieintel.com/games/arena-breakout-infinite-health-system-explained-324912 ）；數字在不同來源互相矛盾，**未確認** |
| CS:GO／CS2 | 只有總血量；部位只影響倍率；手臂和胸同為 1× | 無部位後果 | △（#363 第 10.3 節） |
| Source 引擎 | 部位（hitgroup）只是倍率的鍵；血量模型由遊戲寫 | 由遊戲寫 | 官方文件、△（第 1.2 節） |

### 3.3 選項：血量的形狀

| | 做法 | 好處 | 代價 |
|---|---|---|---|
| (H1) | **總血量＋部位血量並存**：一發的傷害（乘倍率後）同時扣總血量和該部位的血；總血量歸零才死；部位歸零只帶後果 | 死亡條件和現在一樣（`PlayerCombat::hp`），核心 PvP 不必動；`LimbHealth` 只是多一層 | 部位血量對生死沒有直接作用，只靠狀態效果有意義；和塔科夫的手感不同（打頭不必然一槍、打腳也會直接累積到死） |
| (H2) | **只有部位血量＋致命部位**（塔科夫型）：每個部位一格血；致命部位（頭、胸）歸零即死；非致命部位歸零後，再打到它的傷害乘轉移係數、分到其他部位 | 和最終產品的參考對齊；部位的戰術意義最大（護頭胸、打腳讓對方跑不動） | 死亡條件變了：撤離模式不再用 `PlayerCombat::hp` 判死，要由規則表決定；轉移的分法（平均分？按比例？只分給沒黑的部位？）要定，社群 wiki 也只有推測 |
| (H3) | **資料決定血池**：規則表把每個規則部位指到一個血池，血池帶最大值、致命與否、轉移係數；核心 PvP＝「所有部位指到同一個致命血池」，撤離＝「7 個部位各自一池，頭、胸致命」 | (H1) 以外的兩種都能用同一套程式表達：核心 PvP 的結果和現在相同，撤離可以是 (H2)；換規則只換資料 | 程式比 (H2) 多一層「部位→血池」；核心模式要確認和現在逐位元一樣（S5、權威 hash） |

### 3.4 選項：歸零的後果

| | 後果 | 代價與注意 |
|---|---|---|
| (Z1) | 只記錄（UI 顯示黑掉） | 最小；部位血量對戰鬥沒有作用 |
| (Z2) | 傷害轉移（係數在規則表） | 只動 Match 的傷害計算；同步的只有結果（`LimbHealth`） |
| (Z3) | 致命部位歸零即死 | (H2)／(H3) 才有 |
| (Z4) | 狀態效果：斷手（換彈、瞄準變慢）、斷腳（不能跑、變慢） | **會進預測**：自己的移動和換彈是 Client 預測、會回滾的 sim；`LimbHealth` 要登記成 authority component＋`Replicate`（和 `PlayerCombat` 一樣，`PvpWorld.cpp:1092-1095,1124-1128`），`MovePlayers`、換彈的時間要讀它，兩端要逐位元一致。代價比 (Z1)～(Z3) 大一級，建議另成一批 |

### 3.5 選項：左右、胸腹

| 問題 | 分 | 不分 |
|---|---|---|
| 左右手、左右腳分開 | 形狀本來就左右分開（`hitbox.json` 的 `thigh_l`／`thigh_r`）；塔科夫左右手的狀態效果不同（△）；`LimbHealth` 多 2 格 | 少 2 格；但打左手扣到右手的血，表現上說不通 |
| 軀幹拆成胸、腹 | 軀幹本來就是兩個膠囊（`spine_01→spine_03`、`pelvis→spine_01`），拆只是改標籤；(H2) 下才有意義：胸致命、腹不致命（塔科夫 △） | (H1) 下拆不拆只差倍率；不拆就少一格 |

### 3.6 和護甲的關係（只點出，不展開）

- 護甲要寫「護哪些部位」，所以**部位要有穩定的名字**讓護甲資料引用；(1b)／(1c) 的資料化天然支援，(1a) 要再寫一張對照。
- 塔科夫的手臂護甲能擋「穿過肩膀進胸」的子彈（△ 攻略站 https://www.ggrecon.com/guides/escape-from-tarkov-best-armour-armour-hitboxes/ ）：護甲和第 4 題的「手臂擋不擋」、武器的穿透數值綁在一起。
- 護甲是部位的屬性還是裝備的屬性、穿透怎麼算，等武器與背包線再定。

### 3.7 推薦

1. **血量的形狀：(H3)**，撤離模式用 (H2) 的設定（7 個部位各一池，頭、胸致命），核心 PvP 用「一個致命血池」。理由：最終產品是塔科夫型，(H2) 是目標；(H3) 讓核心 PvP 和撤離共用同一套程式，路線 (A) 的「一套程式承壓」才成立。(H1) 不推薦：它讓部位血量只能靠狀態效果說話，和使用者選 (3) 的目的（手臂有自己的血量而且有意義）不合。
2. **歸零的後果：先 (Z2)＋(Z3)**；(Z4) 狀態效果另成一批，因為它進 Client 的預測與回滾，判定的檢查方式不同。
3. **左右分、胸腹拆**（撤離模式）：形狀已經是分開的，代價只是資料與 `LimbHealth` 多幾格。
4. 轉移的分法先照「分給還沒黑的部位、按各部位的最大血量比例」這種可以寫成資料的規則；具體分法和係數是調數值的事，G5 之後再調。

### 3.8 決策點

- **L3**：血量的形狀：(H1)、(H2)、(H3)？（推薦 (H3)，撤離模式用 (H2) 的設定）
- **L4**：歸零的後果先做哪些：(Z1)～(Z4)？（推薦 (Z2)＋(Z3)，(Z4) 另一批）
- **L5**：左右分不分？胸腹拆不拆？（推薦都分）

---

## 4. D10：手臂擋不擋後面的部位

**前提**：這是**作者（遊戲）能處理的**。同一目標內取哪個形狀是遊戲的判定規則（`PvpWorld.cpp:513-519`）。**不產生 Engine 的批次**。

### 4.1 現況

- 對每個目標，取射線上**最近**的形狀；同距看 `PartRank`（`PvpWorld.cpp:513-519`、`CombatRules.cpp:39-47`）。現在沒有手臂，所以舉槍時手臂擋在胸前的情形，算的是胸。
- #363 第 7 節的 D10 推薦 (a) 擋，理由是手臂有血量，不擋會讓手臂很少扣血。

### 4.2 和傷害模型的關係

| 傷害模型 | 擋 | 不擋 |
|---|---|---|
| (H1) 總血量 | 只換倍率：手臂倍率比胸低時，擋＝對被打的人有利 | 手臂只在「只中手臂」時扣血，部位血量很少動 |
| (H2)／(H3) 致命部位 | **手臂替胸擋子彈**：非致命部位吸收傷害，黑掉之後再轉移（係數 < 1 時仍比直接進胸少）。這是塔科夫類的戰術面（△，3.6） | 舉槍時手臂完全不吸傷害，打胸就是打胸；手臂血量只在側面被打時才動 |

### 4.3 別家怎麼做

| 遊戲 | 做法 | 查證 |
|---|---|---|
| CS:GO（2016 起） | 不擋：射線打到手臂後繼續進頭，算頭 | △（#363 第 10.3 節） |
| 塔科夫 | 手臂會擋；有手臂護甲能防止子彈穿過肩膀進胸；玩家說法：子彈可以穿過手臂再進胸、穿過時損失傷害與穿透 | △ 攻略站（3.6 的連結）；穿透的細節只有玩家討論，**未確認** |

### 4.4 選項

| | 做法 | 好處 | 代價 |
|---|---|---|---|
| (a) | **擋**：照現行規則取最近 | 判定規則不用改（只要 `PartRank` 能從規則表讀到手臂的優先序）；配 (H2)／(H3) 時手臂有實際的保護作用 | 舉槍時頭和胸常被手臂擋，對射手不利；和 CS 的方向相反 |
| (b) | **不擋**：同目標的其他形狀有中，就算那個；只中手臂才算手臂 | 對射手有利（CS 型） | 和 (3) 的目的衝突：手臂的血量很少動；判定要改成「排除手臂後再取最近」 |
| (c) | **穿透**：先扣手臂，再照穿透量進後面的部位 | 最接近塔科夫（△） | 要武器有穿透數值，現在沒有；判定要改成沿射線收集多個形狀 |
| (d) | **模式決定**：規則表帶「這個部位擋不擋」 | 核心 PvP 不受影響（它本來就不判手臂）；撤離可以選 (a)，之後換 (c) | 多一個欄位；預設值還是要選 (a) 或 (b) |

### 4.5 推薦：(a)，寫成 (d) 的資料欄位，撤離模式預設擋；(c) 等武器有穿透數值再議

1. 配推薦的 (H3)／(H2)，手臂替胸擋子彈是部位血量有意義的主要來源；(b) 會讓 (3) 的設計失去大半效果（#363 的理由仍成立）。
2. (a) 不改判定的取法，只改優先序的來源；寫成資料欄位幾乎不加成本，之後換 (c) 時也不必再改資料格式。

### 4.6 決策點

- **L6**（＝D10）：手臂擋不擋：(a)、(b)、(c)、(d)？（推薦 (a)，用 (d) 的欄位寫）

---

## 5. Match 的成本

**前提**：這是**作者（遊戲）能處理的**。成本來自遊戲自己的判定迴圈；用到的 Engine 部分（model 的取樣、E2 的 two-bone IK、物理的射線對膠囊）都是已定的純函式。量測若發現 model 的取樣本身是瓶頸，才會回到 Engine（那時另開）。

### 5.1 現在一發的工作量（從程式推，沒量）

對**每個射手、每個活著的其他目標**（`PvpWorld.cpp:495-521`）：

| 步驟 | 工作 | 位置 |
|---|---|---|
| 1 | `AnimationInputOf` | `PvpWorld.cpp:505-506` |
| 2 | `ComposeBody`：站姿時 `SampleClip` 6～7 次（Idle、Walk、Jog、AimNeutral、Aim 上或下、動作片段 2 次）＋2～3 次 `Blend`＋上半身逐骨合成；每次都走過**整副骨架的所有 node**；函式裡新配置 4 個 vector | `CharacterRig.cpp:244-316` |
| 3 | `ModelSpace`：整副骨架 | `CharacterRig.cpp:318-321` |
| 4 | `PlaceHitShapes`：7 個形狀 | `CharacterRig.cpp:329-347` |
| 5 | 射線對膠囊 7 次 | `PvpWorld.cpp:514-519` |

注意兩件事：

- **沒有 broadphase**：每個目標都先擺好整副姿勢再測，不管射線離他多遠。
- **同一 tick 多個射手時，每個射手都重算每個目標的姿勢**（沒有每 tick 的快取）。

4 人、射速上限 6 發／秒（`cooldownSeconds = 1/6`，`CombatRules.hpp:30`）時，最壞每秒 4×6×3＝72 次姿勢；平常遠少於此。

### 5.2 加手臂之後（推論）

| 項目 | 增加 | 相對大小 |
|---|---|---|
| IK（D5 (b)，G3） | 每次姿勢多一次 two-bone IK（左手；右手握槍是片段本身）＋手臂鏈的模型空間重算 | 小：幾次向量運算與一段鏈；和 6～7 次整副取樣相比應可忽略（推論） |
| 形狀 7→11 | `PlaceHitShapes` 多 4 個、射線測試多 4 次（＋57%） | 小：射線對膠囊是常數時間的幾何 |
| 再拆部位 | (1b) 下解剖標籤 11 個＝形狀 11 個；規則部位怎麼合併不加形狀 | 0 |

結論（推論）：**加手臂本身不改變 Match 的成本級別**；成本由「每次判定要擺幾次整副姿勢」決定。

### 5.3 做 PV3 之後（推論）

| | 現在（當 tick） | PV3 (b)（存擺好的形狀） |
|---|---|---|
| 擺姿勢的次數 | 開槍時，射手數×目標數 | **每 tick 每個玩家 1 次**：64 Hz × N（4 人＝256 次／秒，比現在最壞的 72 次多） |
| 記憶體 | 0 | 11 形狀 × 7 float × 4 B ＝ 308 B／人／tick；回溯 250～500 ms（16～32 tick）≈ 5～10 KB／人 |
| 每發的查詢 | 7→11 個形狀 × 目標數 | 同左，加兩 tick 間的插值；配 broadphase 可只測射線附近的目標 |

撤離模式人數多、還有怪物時，(b) 的成本隨「有判定形狀的實體數 × tick 率」線性增加。怪物要不要用同一套形狀與回溯，是怪物線的事，這裡只點出。

### 5.4 可以先做的省法（別家做法，不是本卡的決定）

1. **broadphase**：lightyear 先打「歷史 AABB 的包絡」，中了才做精確測試（`history.rs:121-145`、`query.rs:181-212`）。gyo2 可以先用「身體的包圍膠囊」測，中了才擺姿勢。這在 PV3 之前也有效。
2. **每 tick 的姿勢快取**：同一 tick 多個射手共用目標的姿勢；PV3 (b) 本身就是這件事。

### 5.5 量測項（本卡不跑；都是小的 micro benchmark，不是重量測）

| # | 量什麼 | 在哪 | 給誰用 |
|---|---|---|---|
| M1 | `ComposeBody`＋`ModelSpace` 一次的時間（Release，Linux） | Match 的機器 | 5.1、5.3 的乘數 |
| M2 | two-bone IK 一次（含鏈的模型空間重算） | 同上 | G3 |
| M3 | 射線對 7／11 個膠囊 | 同上 | G6 |
| M4 | `JudgeShots` 一 tick 的最壞情形（4 人同 tick 開槍），手臂有／無 | 同上 | G6 的回歸 |
| M5 | PV3 (b) 每 tick 擺所有玩家：N＝4、8、16 | 同上 | L2 的排程、撤離模式的人數上限 |
| M6 | LAN 場次的實際落差（RTT、內插延遲、輸入餘裕） | LAN 場次 | 2.1 的推論、回溯上限（L2'） |
| M7 | 手臂膠囊的半徑與包覆（D9）、Client 與 Match 的手臂形狀差（PV6） | G5 | 倍率與半徑 |

M1～M5 都是 CPU 的短時間量測，不需要 GPU 或大記憶體；排在 G5 或 G6 裡一起做即可。

### 5.6 決策點

這一題沒有要使用者定的；量測項排進哪一批照批次表的順序由主會話排。broadphase 要不要和 G6 一起做，屬於實作細節，開 G6 時由那張卡判斷。

---

## 6. 決策點總表

| # | 問題 | 選項 | 推薦 |
|---|---|---|---|
| L1 | 部位清單資料化 | (1a) enum 加 `Arm`／(1b) 固定解剖標籤＋模式規則表／(1c) 全資料化 | (1b) |
| L1' | 解剖標籤粒度（(1b) 時） | 11 個（上臂、前臂分開）／7 個 | 11 個 |
| L2 | PV3 的時機 | (P1) 維持沒排程／(P2) G6 之後、撤離試玩之前，模式開關／(P3) G6 之前 | (P2) |
| L2' | 回溯上限 | 模式資料帶／固定值 | 模式資料帶 |
| L3 | 血量的形狀 | (H1) 總＋部位／(H2) 只有部位＋致命部位／(H3) 資料決定血池 | (H3)，撤離用 (H2) 的設定 |
| L4 | 歸零的後果 | (Z1) 記錄／(Z2) 轉移／(Z3) 致命／(Z4) 狀態效果 | 先 (Z2)＋(Z3)，(Z4) 另一批 |
| L5 | 左右、胸腹 | 分／不分 | 都分（撤離模式） |
| L6 | D10 手臂擋不擋 | (a) 擋／(b) 不擋／(c) 穿透／(d) 模式欄位 | (a)，以 (d) 的欄位寫 |

## 7. 對其他文件與批次的影響（如果採用推薦）

- **#363 第 8 節的 G6**：「`HitPart::Arm`」改成「解剖標籤＋規則表」；`hitbox.json` 的 `body`／`leg` 換成細的標籤，`CharacterRig.cpp:219-220` 跟著改。G6 的其他內容（手臂形狀、Match 擺手臂、同目標規則）不變。
- **批次表〈後續小事〉**：
  - 模式資料開關那一列（`rebuild-batch-plan.zh-Hant.md:88`）：加 1.4 節的四點需求（模式身分＋digest、傷害規則表、掛哪些 component、核心模式＝現在的數字）。
  - 肢體傷害那一列（`rebuild-batch-plan.zh-Hant.md:89`）：`HitPart::Arm` 改成 (1b) 的寫法；`LimbHealth` 的形狀照 (H3)。
  - 新增兩列：PV3（照 L2）、斷肢的狀態效果（照 L4 的 (Z4)）。
- **port 宣告 PV3**（`games/object_fps_pvp/docs/port-declaration.zh-Hant.md:50-53`）：「這一卡不做」的理由對核心 PvP 仍成立；撤離模式的排程照 L2。改不改由負責的會話判斷。

以上的文件修改都不在本卡；本卡只交這份研究。

## 8. 沒查到、未確認的

- 塔科夫各部位的血量數字、黑掉後傷害轉移的分法與係數（社群 wiki 有係數，沒標版本，分法是推測）。
- 塔科夫的命中在哪一端判。
- CS2 是否保留 2016 的「穿過手臂進頭算頭」。
- Source SDK 的 `shareddefs.h` 原始檔（本機沒有；hitgroup 的值來自 Valve 開發者 wiki 與 Garry's Mod wiki）。
- 手臂膠囊的半徑、gyo2 骨架的 node 數、輸入的提前量：2.1、5.1 的推論都依賴它們，量測項 M1、M6、M7 會補上。
