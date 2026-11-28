# 頭頂 UI（名字與血條）：事前宣告（`v8-arch-gyo2-overhead-ui`）

任務卡，2026-11-27。這一階段只寫事前宣告，交使用者核准；核准前不寫實作程式。#367（模式配置，`v8-arch-gyo2-mode-config`）改的是同一個遊戲的檔案，**它合併之前不動程式**，實作等主會話通知。作者 Opus 5.5／high。

行號以 `gyo2` 85df84da 為準；標「#367」的是 #367 分支（`claude/gyo2-mode-config-declaration`）上的檔案；標「#375」的是門卡的宣告（`claude/gyo2-interact-door-e11d33` 9984c4aa，修訂 2，`interact-door-declaration.zh-Hant.md`）。

- **修訂 1**：使用者改了 O1：**敵方玩家不顯示血條，怪物顯示血條，參考 Overwatch**。
- **修訂 2**：使用者補了三點：**顯示名字（來源 (N1)），隊友也顯示名字；驗證用測試地圖裡的靶子**。靶子是 Scene 的物件，格式照門卡（#375 第 5 節），不另發明。決定的原話在第 9 節。
- **修訂 3**：使用者定了 O6～O8（第 9.3 節，HANDOFF `v8-arch-gyo2-overhead-ui-o6-o8`）：**隊友只顯示名字、不顯示血條**（狀態之後用第三人稱動作表現，這次不做）；**靶子放 `pvp_arena`＋`sandbox` 模式配置＋門的 scene 檔**；**`JudgeShots` 只記打到的實體，怎麼處理由各組自己決定**（不加 `ShotHitKind`）。門卡的 P2 改成「卸下／保留兩種預設」，第 4.3、6 節照它重對一次，多了一個決策點 O9（第 8 節）。用詞照門卡：模式檔與它的條目一律寫「模式配置」。修訂 2 的版本是 ec3e19af。

---

## 0. 前提與結論先講

### 0.1 已定的前提（這份宣告不推翻）

| 前提 | 出處 |
|---|---|
| 路線 (A)：新功能加在 Object_PvP，由模式配置決定這一場掛哪些系統；玩法組固定登記、由模式條件決定跑不跑 | HANDOFF `v8-arch-gyo2-object-pvp-route-a`；#367 宣告 2.3 |
| 最終產品是有怪物的撤離射擊；Object_PvP 驗證核心 PvP 交互與地圖 | HANDOFF `v8-arch-gyo2-tarkov-is-reference` |
| **敵方玩家不顯示血條**（受傷的資訊靠命中標記、受擊動作、音效）；**怪物顯示血條，參考 Overwatch** | HANDOFF `v8-arch-gyo2-overhead-ui-o1`（第 9 節） |
| **顯示名字**，來源＝房間裡的「Player 12」（(N1)）；**隊友也顯示名字**；**驗證用測試地圖裡的靶子** | HANDOFF `v8-arch-gyo2-overhead-ui-names-targets`（第 9 節） |
| **隊友只顯示名字、不顯示血條**；隊友的狀態之後用第三人稱動作表現（敵我都看得到），這是作者的規則、這次不做；**靶子放 `pvp_arena`＋`sandbox` 模式配置＋門的 scene 檔**；**`JudgeShots` 只記打到的實體與距離，靶子等由自己的組讀 `ShotRecord` 處理**，不在 `ShotHitKind` 為每種東西加值 | HANDOFF `v8-arch-gyo2-overhead-ui-o6-o8`（第 9.3 節） |
| Map＝毛坯房、Scene＝裝修（會互動、有狀態的東西），Scene 是另一個配置檔；scene 物件＝`{id, set, data}`，由玩法組的 `readScene` 解析；兩端各自放好、網路用 net 的 `Placed` 認得 | HANDOFF `v8-arch-gyo2-map-scene-split`；#375 第 4、5 節 |
| Engine 只給機制；Product 的詞不進 Engine；錯誤用 `Result` 只告知；不對誤用加守衛；ECS 相關以 Bevy 為主參考 | AGENTS §1、批次表〈每批的設計審查〉第 5 項 |
| 使用者原則：「行為要統一、語意要一致」 | 卡片 brief（主會話轉交） |

### 0.2 結論（一段話）

**Engine 不改。** 頭頂 UI 做在 Client 的 Frame World，畫在**螢幕空間**，用 HUD 已經在用的那一套：HUD camera（2D、1280×720）上的 `Sprite`（文字圖）與 `Shape2d`（血條）。每幀：(1) 每一種「會有頭頂 UI 的東西」由它自己的**來源 System** 寫一個遊戲層的 component `Overhead`（錨點、名字、血量比例、顯示的方針）；(2) 共用的 `ShowOverheads` 依方針決定透明度，再用 `Camera::WorldToViewport`（世界 camera）與 `Camera::ViewportToWorld2d`（HUD camera）換成 HUD 座標擺上去。被牆擋住、在鏡頭後面、在畫面外就不顯示；**不設距離上限**。

來源有兩種：

- **玩家**：其他玩家顯示名字（`PlayerLabel(userId)`，和房間同一個字串），**不顯示血條**；看得到就顯示。**敵人和隊友一樣**（隊友也只有名字），所以不用分關係（第 3.2 節）。核心 PvP 也看得到名字，模式配置不改。
- **靶子**（代替還沒有的怪物）：玩法組 `targets`，scene 物件，有血量、會被打、打壞了過幾秒復原。血條照 Overwatch：**交戰中**（自己剛打中、或血量低）才顯示，之後淡出；標成 `always` 的一直顯示（Boss 的代替）。放在門卡的 `sandbox` 模式配置與 `pvp_arena` 的 scene 檔。靶子要能被子彈打到：**`JudgeShots` 把有 `Shootable` 的 collider 也當成候選，打到時在 `ShotRecord` 記下那個實體**；扣不扣血、扣多少由 `targets` 組自己讀 `ShotRecord` 決定（第 3.5 節，O8 (a′)）。靶子依賴門卡的 `Placed` 與 scene，排在門線的那兩步之後。

---

## 1. 現況（照程式核對）

| 項目 | 程式 | 對這張卡的意思 |
|---|---|---|
| `WorldToViewport` | `engine/modules/camera/include/engine/camera/Camera.hpp:176`（Bevy `world_to_viewport`）；鏡頭後面是 `PastFarPlane`、比 near 近是 `PastNearPlane`，失敗不配置記憶體（`:161`） | 直接用；失敗就隱藏 |
| `ViewportToWorld2d` | `Camera.hpp:183`（Bevy `viewport_to_world_2d`） | viewport 座標 → HUD camera 的 2D 座標，不自己寫換算 |
| HUD | HUD camera `kHudCamera`：`ScalingFixed{1280, 720}`、order 2、只看 `kHudLayer`（`games/object_fps_pvp/src/client/ClientWorld.cpp:1908-1914`）；文字用 `TextPicture` 畫成圖、文字不變就不重畫（`:310-320`、`:374`）；`ShowHud`（`:953`） | 頭頂 UI 用同一個 camera、同一個文字圖快取的做法 |
| gyo.ui 文件 | 元件的位置只來自文件（`UiRectTransform`，`engine/modules/ui/include/engine/ui/UiDocument.hpp`）；binding 只能改文字、顏色、slider 的值、清單的項目 | **gyo.ui 不能每幀把元件擺到任意位置**，所以頭頂 UI 不走 gyo.ui（第 2.3 節） |
| 射擊的判定 | `JudgeShots`（`games/object_fps_pvp/src/world/PvpWorld.cpp:470-544`）：對 `Walls`（所有沒有 `ChildOf` 的 collider，`:252`）打 `CastRay`，回傳**打到的實體**（`EntityRayHit{entity, hit}`，`engine/modules/physics_3d/include/engine/physics_3d/SpatialQuery.hpp:45-48`），但只取距離；結果只有 `Miss`／`World`／`Player`（`sim/CombatRules.hpp:54`），`World` 的 `target` 是 0 | 靶子是 collider，現在子彈打到它只會是 `World`、不知道打到誰（第 3.5 節） |
| 自己打中了誰 | `ShotRecord`（`world/Components.hpp:70-77`）：`kind`、`target`，已複製；Codec 不檢查 `kind` 的值（`PvpWorld.cpp:169-181`）；命中標記看它（`ShowHud` 的 `h.marker.Observe`） | 「自己剛打中它」看同一份資料，和命中標記一致 |
| 血量的顯示 | `PlayerCombat::hp`（milli-points，`Components.hpp:52`）、`CombatRules::maximumHp`（`sim/CombatRules.hpp:21`）、`DisplayPoints`（無條件進位，`:35`） | 靶子也用 milli-points 與 `DisplayPoints` 的規則，和自己的 HUD 一致 |
| 名字 | 沒有顯示名稱；`Owner` 只有 `userId`（`Components.hpp:100`）；房間畫面寫 `"Player " + userId`（`src/client/RoomScene.cpp:70`） | (N1)：抽成 `PlayerLabel(userId)`，房間與頭頂共用 |
| 隊伍 | **沒有**：Object_PvP 沒有隊伍的概念，其他玩家都是敵人 | 隊友頭頂和敵人一樣只有名字（O6 (b)），有隊伍時頭頂 UI 不用改 |
| 頭的位置 | 判定形狀有 `HitPart::Head`（`src/character/CharacterRig.cpp:219`）；`PlaceHitShapes`（`:329`）；`PoseCharacters` 每幀算其他玩家的姿勢（`ClientWorld.cpp:758`） | 玩家的錨點＝`Head` 的中心＋偏移 |
| 遮蔽 | `PW::Blocked(walls, a, b)`（`world/PvpWorld.hpp:76`）；聲音用它（`ClientWorld.cpp:1076-1082`） | 頭頂 UI 用同一個判斷 |
| Scene（門線，**已核准動工、未合併**） | #375：`assets/object_fps_pvp/scenes/<名字>.json`，物件 `{id, set, data}`；`GameplaySet::readScene`；`EntityId`＝`kSceneEntities` 起點＋`id`；`sceneDigest` 只算模式配置開著的組；net 的 `Placed`（附上；看不見時照預設**卸下**複製來的 component，或作者選**保留**最後的值，兩種都標 `NotReceiving`，P2）；開賽前 Peer 看得到所有 `Placed` 的實體；新的模式配置 `sandbox`（`modes.json` 的第二個）、`pvp_arena` 的 scene；Frame World 經 `WorldSnapshot` 拿門的狀態（#375 第 3～5、8 節） | 靶子照同一套：另一個組 `targets`，同一個 scene 檔、同一個模式配置（第 3.5 節） |

---

## 2. 成熟方案

查證等級：**源碼**（本機 `~/Code/GitHome`）、**官方**（官方文件或 patch notes）、**△**（二手資料：新聞、第三方指南、論壇）、**未確認**（憑記憶、沒找到出處）。

### 2.1 Bevy（主參考，源碼 `bevy` 6a911653f）

| 什麼 | 位置 |
|---|---|
| `world_to_viewport`：NDC z < 0 是 `PastFarPlane`、> 1 是 `PastNearPlane`，Y 翻成向下 | `crates/bevy_camera/src/camera.rs:543-590` |
| `viewport_to_world_2d`；範例把 `world_to_viewport` 與 `viewport_to_world_2d` 來回換、互相驗證 | `camera.rs:713`；`examples/2d/2d_viewport_to_world.rs:29-38` |
| 引擎自己的用法：camera controller 用 `world_to_viewport` 算錨點在螢幕上的大小，讓指示圖示保持固定像素大小 | `crates/bevy_camera_controller/src/pan_orbit_camera/extensions/anchor_indicator.rs:61-71` |
| UI 的排版在 PostUpdate、排在 `CameraUpdateSystems` 之後 | `crates/bevy_ui/src/lib.rs:151-163` |

Bevy 本身沒有名牌或血條的元件；社群的做法是「每幀用 `world_to_viewport` 算位置，寫 UI `Node` 的絕對位置」，**未確認**（社群 crate 不在本機）。

### 2.2 Godot（對照，源碼 `godot` 94fc09c）

| 什麼 | 位置 |
|---|---|
| 螢幕空間：`Camera3D::unproject_position`、`is_position_behind` | `scene/3d/camera_3d.cpp:548`、`:462-466` |
| 世界空間：`Label3D` 的 `billboard_mode`、`FLAG_FIXED_SIZE`、`FLAG_DISABLE_DEPTH_TEST` | `scene/3d/label_3d.h:45-46,139`；固定大小在材質的 shader 裡做（`scene/resources/material.cpp:1341`） |

### 2.3 螢幕空間 vs 世界空間

| | 螢幕空間（HUD 的 2D 層） | 世界空間（billboard） |
|---|---|---|
| 大小 | 固定像素，遠近一樣清楚 | 隨距離變小；要固定大小，得每幀縮放或在 shader 做（Godot 的 `FLAG_FIXED_SIZE`） |
| 文字 | HUD 的文字圖，清楚 | 文字貼在 3D 四邊形上，斜看、遠看會糊 |
| 被擋住 | 要自己判斷（用 `PW::Blocked`） | 深度測試自動擋（也會被其他身體擋） |
| GYO 現有的機制 | **都有**：`WorldToViewport`、`ViewportToWorld2d`、HUD camera、`Sprite`、`Shape2d` | 沒有 billboard 的 drawable；要用 `Mesh3d` 做四邊形、每幀轉向 |
| Overwatch | 名字與血條固定大小、畫在頭頂（2.4） | — |

**判定：螢幕空間。** 理由：Engine 已有全部的機制；固定大小、清楚的名字與血條是 Overwatch 的樣子；擋住的判斷和聲音用同一個。

不用 gyo.ui：gyo.ui 的位置寫死在文件裡（第 1 節）；要支援就要改 `gyo.ui` 資料契約（版本 3）。這次不做，只有這一個使用者（AGENTS §1）。

### 2.4 Overwatch：血條什麼時候出現

Overwatch 是 PvP 的英雄射擊，沒有「怪物」；使用者要的是**把它對敵方英雄的血條規則，用在我們的怪物（這次是靶子）上**。

| 規則 | 內容 | 查證 |
|---|---|---|
| 血量低時顯示 | 2024-07-09（第 11 季中）patch notes：敵人在 Low Health 門檻時顯示血條，預設開 | **官方**（<https://overwatch.blizzard.com/en-us/news/patch-notes/live/2024/07/>，頁面本身沒有開；文字見搜尋摘要與兩家轉載：Dexerto <https://www.dexerto.com/overwatch/overwatch-2-players-are-loving-this-game-changing-new-enemy-hp-feature-2824295/>、OW Cavalry <https://x.com/OWCavalry/status/1810745929693286554>）。**門檻是多少，沒查到** |
| 打中之後顯示 | 自己打中的敵人顯示血條；2018 年起「自己做出的物件造成傷害」也算 | △（搜尋摘要引用的舊 patch notes，原頁未開）；「打中才顯示」的基本規則本身 **未確認**（所有來源都把它當前提） |
| 瞄準時顯示 | 第三方指南說瞄準敵人時一定顯示；Custom Game 的說明說預設可見、可關。**兩份互相矛盾** | △（<https://fortunerapps.com/overwatch-2/how-to-customize-hud-in-overwatch-2>，抓取時 404，內容來自搜尋摘要；<https://overwatch.fandom.com/wiki/Custom_Game>） |
| 被擋住 | 打中過的敵人躲到掩體後，看不到它的血條；看得到模型時才看得到 | △（官方論壇的玩家討論 <https://us.forums.blizzard.com/en/overwatch/t/make-health-bars-visible/316809>） |
| 顯示多久、怎麼淡出 | 沒查到秒數 | **未確認** |
| 距離上限 | 沒查到 | **未確認**（「Motion Detection 25 m」是 Hacks & Hijinx 模式的修飾，不是一般規則，官方 2025-06 patch notes） |
| 隊友 | 有「隊友頭頂血條」的開關；PvE 與部分活動把隊友血量放在畫面左下 | △（Dexerto <https://dexerto.com/overwatch/hidden-overwatch-2-update-adds-highly-requested-feature-players-have-waited-ages-for-3001067>；第三方指南） |
| 名字 | 隊友頭頂有名字、敵人沒有 | **未確認**（憑記憶）。使用者決定敵方玩家也顯示名字（第 9 節），和這一點不同 |
| 血條的樣子 | 每 25 HP 一格；白＝生命、黃＝護甲、藍＝護盾 | **未確認**（憑記憶；Fandom 的 Hit points 頁回 402） |
| PvE 的敵人 | 沒查到 | **未確認** |

**取用的概念**（只取有出處的部分，秒數與門檻由我們的資料定）：

1. **交戰中才顯示血條**：自己剛打中它（△），或它血量低（官方）；沒有再打中就淡出。
2. **被擋住就不顯示**（△，和 GYO 聲音的遮蔽同一個判斷）。
3. **不設距離上限**：沒找到 Overwatch 有距離上限；使用者也指出「遠了看不到血條、卻看得到人，覺得怪」。
4. **不取「瞄準時顯示」**：來源互相矛盾，而且要每幀對判定形狀打一條選取射線；之後實機覺得需要再加（只動 `ShowOverheads` 的方針，第 4.2 節）。
5. Boss 一直顯示：Overwatch 沒有對應（未確認），這是 PvE 的常見做法；靶子用 `"overhead": "always"` 代替（3.5）。

---

## 3. 資料：誰有頭頂 UI、顯示什麼

**前提：作者能處理的**（遊戲層的 component 與資料檔）。Engine 不知道「名字」「血量」「靶子」「怪物」。

### 3.1 標記：`Overhead` component（Frame World，遊戲層）

```
enum class OverheadPolicy { Always, Engaged };
struct Overhead {          // games/object_fps_pvp，Frame World 才有
    Math::Vec3 anchor;     // 這一幀頭頂的世界座標（已加上偏移）
    std::string label;     // 空：不顯示名字
    std::optional<float> health;  // 0～1；none：不顯示血條
    OverheadPolicy bar = OverheadPolicy::Engaged;  // 血條的方針；名字一律看得到就顯示
};
```

- **誰寫**：每一種東西自己的**來源 System**。這次兩個：`TagPlayerOverheads`（其他玩家）、`TagTargetOverheads`（靶子）。之後的怪物加 `TagMonsterOverheads`，`ShowOverheads` 不用改。**有 `Overhead` 就是「要顯示頭頂 UI」**，和 Bevy「有 component 就被 query 到」同一個做法。
- **不是決策資料**、不複製、`HashValue` 不寫東西（和 `ClientHud` 一樣）。放在 Frame World 裡那個對象的實體上：對象不見了，`Overhead` 跟著不見。
- 為什麼不讓 `ShowOverheads` 直接讀血量的 component：那樣它就要認得每一種東西的血量（玩家的 `PlayerCombat`、靶子的 `TargetState`、之後怪物的、G6 之後的部位血量）。來源 System 把各自的資料翻成同一個形狀，顯示的規則只有一套（行為統一）。

### 3.2 名字與誰顯示什麼

名字的來源（(N1)，已定）：把 `RoomScene.cpp:70` 的 `"Player " + userId` 抽成 `PlayerLabel(userId)`，房間與頭頂都呼叫它。同一個人在房間與場上叫同一個名字（語意一致）。真的顯示名稱（identity＋wire）等「玩家要能取名字」成為功能時另開卡，只換 `PlayerLabel` 的來源。

| 對象 | 名字 | 血條 | 狀態 |
|---|---|---|---|
| 敵方玩家 | `PlayerLabel`，看得到就顯示 | **不顯示** | 已定（第 9 節）；這張卡做 |
| 隊友 | `PlayerLabel`，看得到就顯示 | **不顯示**（O6 (b)） | 已定（第 9.3 節）。**現在沒有隊伍**；頭頂和敵人一樣，所以有隊伍時 `TagPlayerOverheads` 不用改 |
| 自己 | — | — | 不顯示 |
| 靶子 | scene 的 `name`（有寫才顯示） | `Engaged`，或 scene 寫 `always` | 這張卡做（3.5） |
| 怪物 | 定義資料的名字（有寫才顯示） | `Engaged`，Boss `Always` | 怪物那張卡 |

- 核心 PvP 的效果：其他玩家頭上出現「Player N」，沒有血條。這只是 Client 的表現，不動判定，權威 digest 不變；**模式配置不改**（`core_pvp.json`、`rulesDigest` 不動）。
- 「某個模式配置不顯示名字」（例：之後的撤離玩法）現在沒有人要，**不做**；要時加一個玩法組，`TagPlayerOverheads` 掛它的條件就好（#367 的機制已經在）。
- **隊友的狀態**（O6 (b)，只記方向，這次不做）：用第三人稱的動作表現（受傷時換不同的 idle 等），**敵我都看得到**，同一個角色在每個人眼裡播同一個動作（和 Match 判定用的姿勢只有一份一致）。這是作者的規則：可以寫死在程式裡（使用者說不推薦），也可以放進模式配置；Object_PvP 由我們當作者自己選。「Object_PvP 放進模式配置」是 side chat 的讀法，使用者沒有反對、也沒有明說，到實作那張卡再確認。要等有隊伍、E1（#377）、姿勢組（D4）、`LimbHealth`。頭頂 UI 這邊沒有要配合的：隊友與敵人頭頂都只有名字。

### 3.3 錨點

- 玩家（之後的人形怪）：`Head` 判定形狀的中心（`PlaceHitShapes`），往上加 `ui/overhead.json` 的 `headClearance`（公尺）。它跟著動畫（蹲、跳、倒地），和「打到頭」的頭是同一個點。走路時會跟著頭上下晃幾公分。`PoseCharacters` 把這一幀算好的 model-space 姿勢留在 `ClientCharacters`，`TagPlayerOverheads` 排在它後面讀，不重算。
- 靶子：長方體頂面的中心，往上加同一個 `headClearance`。
- 算錨點是來源 System 的事；`ShowOverheads` 只拿 `anchor`。

### 3.4 外觀：`ui/overhead.json`

照 `ui/hit_feedback.json`，Client 自己讀，不進比對值：`headClearance`、文字大小、名字的顏色、血條的寬高、底色、血量色、低血量色、名字與血條的間距、`lowHealth`（0～1）、`holdSeconds`、`fadeSeconds`。`lowHealth` 算外觀而不是規則：它只決定「要不要把已經在 Client 的血量畫出來」，不改任何判定。

### 3.5 靶子：玩法組 `targets`（scene 物件）

**前提：作者能處理的**（遊戲層的玩法組與 scene 資料；Engine 的 `Placed` 由門線補）。

**放哪裡（O7 (a)，已定）**：

| 什麼 | 放哪 | 理由 |
|---|---|---|
| 地圖 | `pvp_arena`（現有的小場地） | 測試地圖；大地圖要跑很遠才看得到 |
| 模式配置 | 門卡新增的 `sandbox`（`modes.json` 的第二個）多開 `targets` 組 | 「試東西的模式配置」只有一個：門與靶子一起試；核心 PvP 不開，回歸基準不動 |
| 資料 | 門卡的 `scenes/pvp_arena.json`，同一個檔多幾個 `set: "targets"` 的物件 | 同一種 scene 格式、同一個檔（#375 第 5 節）；一張 arena 只引用一個 scene |

scene 物件的例子（格式照 #375 5.1；`id` 的分配見下）：

```json
{ "id": 101, "set": "targets",
  "data": { "at": [4.0, 0.0, 12.0], "yaw": 0.0, "size": [0.6, 1.8, 0.3], "health": 150,
            "resetSeconds": 3.0, "overhead": "engaged", "color": [0.8, 0.2, 0.2] } },
{ "id": 103, "set": "targets",
  "data": { "at": [0.0, 0.0, 18.0], "yaw": 0.0, "size": [1.2, 2.4, 0.4], "health": 600,
            "resetSeconds": 5.0, "overhead": "always", "name": "大きな的", "color": [0.3, 0.3, 0.8] } }
```

| 欄位 | 意思 | 錯誤（讀取失敗，寫哪一個物件、哪一欄） |
|---|---|---|
| `at`、`yaw` | 底面中心的位置、朝向（照 `PlayerBody::yaw`） | 缺、不是數字 |
| `size` | 長方體的寬、高、厚（公尺） | 不是正數 |
| `health` | 最大血量（點，內部換成 milli-points） | 不是正整數 |
| `resetSeconds` | 打壞後幾秒復原 | 負數 |
| `overhead` | `engaged`／`always` | 別的字 |
| `name` | 頭頂的名字（選填） | 太長（64 位元組） |
| `color` | 畫面的顏色 | 不是 0～1 的三個數 |

- 第一批：普通的靶子兩個（`engaged`、沒有名字）＋大的一個（`always`、有名字），共三個，放在出生點看得到的地方。`id` 從 101 開始，避開門用的小號（和門卡約定，第 6 節）。

**資料與 System**：

| 名字 | 種類 | 內容 | 決策資料？ | 複製？ |
|---|---|---|---|---|
| `TargetState` | component（靶子） | `hp`（milli-points）、`brokenTick`（0：沒壞） | 是（`authority.Component<TargetState>`） | `Replicate`，Codec 版本 1 |
| `TargetShape` | component（靶子） | scene 讀到的 `size`、`health`、`resetSeconds`、`overhead`、`name`、`color` | 否（進 `sceneDigest`） | 否（兩端從 scene 放） |
| `Shootable` | component（marker） | 沒有欄位：子彈打到它要記下打到誰 | 否 | 否（兩端從 scene 放） |

- **靶子的實體**：模式配置開著 `targets` 時，每一端在 `Prepare` 從 scene 放（照 #375 的門）：`EntityId`＝`kSceneEntities` 起點＋`id`；`TargetState`（滿血）、`TargetShape`、`Shootable`、`Physics3d::Collider{Cuboid}`、`Transform`；有網路時加 `Placed`。靶子不動，collider 進 `Walls`，擋子彈、擋走路（和門一樣，核心的移動不用改）。
- **看不見時**：照門，用預設（卸下），不放 `WhenHidden`；這一批的靶子實際上不會被藏（#375 3.2：開賽前給所有 `Placed` 的實體，開賽後全部同步）。
- **命中靶子怎麼判（O8 (a′)，已定）**：`JudgeShots` 只回答「最先打到哪個實體、多遠」；打到之後怎麼處理，由那個實體所屬的組自己決定。和 avian（`RayHitData`：entity、距離、法線，`avian/src/spatial_query/ray_caster.rs:403-414`）、Godot（`RayCast3D::get_collider`，`godot/scene/3d/physics/ray_cast_3d.cpp:89`）一樣，一槍只判一次。
  - **`JudgeShots`**：`CastRay(walls)` 本來就回傳打到的實體（第 1 節）。最近的命中是牆、而且那個實體有 `Shootable` 時，`ShotRecord::target` 記那個實體；沒有 `Shootable`（地圖的牆、門）或打到地板，`target` 照舊是 0。`kind` 照舊是 `World`（「先碰到的不是玩家」）。玩家照舊：用姿勢擺判定形狀（PV2），扣血在 `JudgeShots` 裡。
  - **不加 `ShotHitKind` 的值**（`sim/CombatRules.hpp:54` 照舊 `Miss`／`World`／`Player`）：之後的怪物、箱子也是 `Shootable`，`JudgeShots` 不用認得它們。`kind` 說「核心有沒有替它扣血」（`Player` 有、`World` 沒有），`target` 說「打到誰」。
  - **`ChooseShotOutcome`**：world 的候選從 `std::optional<float>` 改成帶實體的 `{distance, entity}`（地板、沒有 `Shootable` 的牆是 0）；`ShotOutcome::player` 改名 `target`（不只是玩家）。比較規則不變：最近的贏、同距離時 world 贏。
  - **`ShotRecord` 的 Codec 升版本**（1 → 2）：位元組的排法不變，但 `World` 帶 `target` 是新的意思；照決定升版本，讓兩端用同一個意思讀。
  - **`ShotRecord::damage`** 對靶子是 0：`ShotRecord` 只有 `JudgeShots` 寫（一個寫者），靶子扣了多少看 `TargetState`。
  - **`DamageTargets`**（`targets` 組，FixedUpdate，`JudgeShots` 之後；Match、Local）：讀這一步的每個 `ShotRecord`（`tick` 是現在、`kind` 是 `World`、`target` 有 `TargetState`），扣 `ShotDamage(combat, Body, hp)`。打壞的（`brokenTick` 不是 0）不扣。
  - 核心 PvP 沒有 `Shootable`，`target` 一律 0，結果和改前相同；權威 digest 逐位元組相同，用 A/B 證明（報告只差 `ShotRecord` 的 Codec 版本）。
  - 比過的：(a) 加一個 `ShotHitKind::Object`（打到 `Shootable` 的一律用它）；(b) 靶子的組自己再打一次射線（同一槍判兩次，可能矛盾）。(a) 和 (a′) 判的是同一件事，差在 `kind` 多不多一個值：(a′) 讓 `kind` 只說「核心有沒有扣血」、`target` 只說「打到誰」，各管一件（HANDOFF 把 (a) 記成「每種東西一個 enum」，卡片的 (a) 原本是共用一個 `Object`；兩種讀法都輸給 (a′)，不影響決定）。
- **`ResetTargets`**（FixedUpdate，`DamageTargets` 之後）：`hp` 到 0 → 記 `brokenTick`；過了 `resetSeconds` → 回滿血、`brokenTick` 清 0。打壞時 collider 留著（打壞的靶子還擋子彈，簡單；要不要倒下是之後的事）。
- **Client**：`WorldSnapshot` 加靶子（`targets{id, TargetState}`），照門（#375 第 8 節）。Sim 側的 System 只放「有 `TargetState`、沒有 `NotReceiving`」的靶子，所以卸下與保留在 Frame 看起來一樣：**沒有那一筆＝沒在收資料**。Frame World 從 scene 知道形狀，每個靶子一個 `Mesh3d` 長方體（門的共用 helper，第 6 節）；打壞時變暗；沒在收資料時留著上一個樣子。
- **命中標記**：`kind=World` 而且 `target` 不是 0（打到 `Shootable` 的東西）也顯示命中標記，和打中玩家一樣的顏色。「打中會被打的東西就有標記」是同一個行為（行為統一）；命中標記出現的那一槍，就是讓靶子的血條出現的那一槍。打壞的靶子也出標記：子彈確實打到它，扣不扣血是組的事。

---

## 4. 每幀做的事（頭頂 UI）

**前提：作者能處理的**（遊戲層的 System）。

### 4.1 System 與排程

| System | 何時 | 做什麼 |
|---|---|---|
| `TagPlayerOverheads` | Main／Update，`PoseCharacters` 之後 | 每個**身體有畫出來**、活著的其他玩家：寫 `Overhead{anchor, PlayerLabel(userId), health=none}`；死了的拿掉 |
| `TagTargetOverheads` | Main／Update | 每個靶子：寫 `Overhead{anchor, name, health＝DisplayPoints 的比例, bar＝scene 的 overhead}`；這一幀的 `WorldSnapshot::targets` 沒有它（沒在收資料）時 `health＝none`：不畫舊的血量，名字照舊 |
| `NoteOwnHits` | Main／Update | 看自己的 `ShotRecord`：`tick` 是新的、`target` 不是 0（`Player`，或打到 `Shootable` 的 `World`）→ 記下「`target` 在這個時間被自己打中」（`OwnHits` Resource，依實體 id）；目標不見了就刪紀錄 |
| `ShowOverheads` | Main／PostUpdate，camera 的 `kUpdateSet` 與 transform 的 `kPropagateSet` 之後、visibility 的 `kCheckSet` 之前 | 每個有 `Overhead` 的實體：透明度（4.2）→ 隱藏（4.3）→ 換座標（4.4）→ 擺它的名牌實體（名字一個 `Sprite`、血條一個 `Shape2d`，在 `kHudLayer`）；目標不見了就移除名牌實體 |

- 排在 PostUpdate 的理由：要用這一幀的 camera（`PlaceCamera` 在 Update 改 camera 的 `Transform`，`GlobalTransform` 與 `ComputedCamera` 在 PostUpdate 才更新）。Bevy 的 UI 排版也在 PostUpdate、`CameraUpdateSystems` 之後（2.1）。
- 名牌實體沒有 parent，`ShowOverheads` 同時寫 `Transform` 與 `GlobalTransform`（和 `Place()` 同一個做法），不等下一幀的 propagate。這一幀就畫在新位置，用測試確認（第 5 節 C1）。
- 名牌實體：`OverheadPart{target}`（照 `CharacterPart`）；文字圖照 HUD 的快取，名字不變就不重畫。

### 4.2 透明度（純函式 `present::OverheadAlpha`）

- **名字**：看得到（4.3 沒有隱藏）就是 1。
- **血條**：`Always` → 1。`Engaged`：血量 ≤ `lowHealth` → 1；否則看最後一次自己打中它的時間 t：`now − t ≤ holdSeconds` → 1，之後在 `fadeSeconds` 內線性降到 0；從沒打中過 → 0。
- 名字和血條都是 0 時不擺名牌（也不打遮蔽的射線）。秒數與門檻是我們的值（Overwatch 的沒查到，2.4）。

### 4.3 隱藏的條件（純函式 `present::PlaceOverhead`）

回傳「HUD 座標」或「隱藏的原因」：

| # | 條件 | 判斷 | 原因 |
|---|---|---|---|
| 1 | 不是要顯示的對象 | 沒有 `Overhead`（自己、死了的玩家） | — |
| 2 | 在鏡頭後面、比 near 近 | `WorldToViewport` 失敗（`PastFarPlane`／`PastNearPlane`） | `Behind` |
| 3 | 在畫面外 | 換出來的 viewport 座標不在 viewport 裡 | `OffScreen` |
| 4 | 被擋住 | `PW::Blocked(walls, camera 位置, 錨點)`：牆來自 Sim World 的地圖（和聲音用的同一份 `WorldSnapshot::walls`）；第 4 步起加上 scene 的門與靶子（見下，O9） | `Occluded` |

- **沒有距離條件**（2.4 第 3 點）。camera 的 far（150 m）以外本來就換不出來，而且那時候身體也畫不出來，所以「看得到人卻看不到名字」不會發生。
- 被擋住時，血條淡出的計時照走：從牆後走出來，還在 `holdSeconds` 內就會再出現。
- 順序由便宜到貴：先 2、3，最後才打射線。射線的成本：玩家與靶子只有幾個，不管；怪物多時再看（第 8 節）。
- **擋住的東西（照門卡修訂 2 重對）**：
  - 不看其他身體：和聲音一樣，身體不擋。
  - `WorldSnapshot::walls` 只有地圖的 `BoxSet`（`ClientExchange.hpp:46`、`ClientWorld.cpp:1529`）；門與靶子是各自的 `Cuboid`，不在裡面。**只看它的話，關著的門後面的人，名字會透過門顯示出來**：看不到人卻看得到名字，是使用者說「怪」的反面，在 PvP 裡也等於透視。
  - 所以第 4 步（門線合併後）把擋的東西改成「**Frame 畫出來的擋物**」：地圖的牆＋scene 的門與靶子，**照 Frame World 畫它的姿勢**（門轉到一半就照一半）。「看得到模型才看得到名字」（Overwatch 的 △，2.4）就是看畫出來的東西。
  - **卸下／保留不用另外處理**：兩種預設都留著實體，Frame 的門與靶子停在上一個樣子（#375 3.2「Client 的門沒有 `DoorState`（卸下時）就不擺」），畫出來的就是擋的。和門卡的焦點射線不同：焦點是「按 E 對著哪個」的判定，用 Sim 傳來的 `DoorState`；名字是畫面，用畫出來的姿勢。
  - **聲音要不要一起換是 O9**（第 8 節）：聲音的遮蔽現在也只看地圖的牆；名字換了、聲音不換，「被門擋住」對兩者的意思就不同。
- 不夾到畫面邊緣。`WorldToViewport` 的失敗是「這一次沒換」，不是錯誤；`ShowOverheads` 不回傳錯誤（E2）。

### 4.4 座標

`WorldToViewport(世界 camera, 錨點)` → viewport 的邏輯像素（左上原點、Y 向下）→ `ViewportToWorld2d(HUD camera, 那個點)` → HUD 的 2D 座標（中心原點、Y 向上）。兩個 camera 在同一個 target、都沒有 viewport，是同一個像素。不自己寫「÷寬×1280」的換算。HUD camera 是 `ScalingFixed{1280, 720}`，視窗不是 16:9 時 HUD 會被拉長，名牌和現有的 HUD 文字一樣（現況，不在這張卡改）。

### 4.5 血條的樣子

比例＝`DisplayPoints(hp) / DisplayPoints(最大值)`，還有血的不會變成空條。分格（Overwatch 每 25 HP 一格，未確認）**這次不做**；有需要再加 `segmentPoints`。

---

## 5. 外部可驗的測試

| # | 測試 | 判定 | 在哪 |
|---|---|---|---|
| P1 | `PlaceOverhead` 的位置：正前方 d 公尺的點 → HUD 的 (0, 0)；往右偏一個已知角度 → 由 FOV 手算的 x；往上 → y 變大。另外和「`WorldToViewport`→`ViewportToWorld2d`」來回驗證（照 Bevy 範例） | 誤差 < 0.01 HUD 單位 | `tests/object_fps_pvp/OverheadTests.cpp`（logic，cpu） |
| P2 | 隱藏：鏡頭後面 → `Behind`；畫面外 → `OffScreen`；一面牆在中間 → `Occluded`，牆移開 → 顯示；錨點貼著牆的這一面 → 顯示；140 m 遠、沒有牆 → 顯示（沒有距離條件） | 原因逐一相符 | 同上 |
| P3 | 透明度：名字一律 1；血條 `Always` → 1；`Engaged` 從沒打中 → 0；打中後 `holdSeconds` 內 → 1；淡出的一半 → 0.5；之後 → 0；再打中 → 1；血量 ≤ `lowHealth` → 1 | 相等 | 同上 |
| P4 | 比例 `DisplayPoints` 的規則（1 milli-point → 1/100，不是 0）；`PlayerLabel` 和房間畫面同一個字串 | 相等 | 同上 |
| P5 | `ui/overhead.json` 與靶子的 scene `data`：缺欄位、型別錯、負數、範圍外 → 讀取失敗並說哪一欄 | 錯誤訊息相符 | 同上 |
| W1 | `JudgeShots`：打到 `Shootable` 的 collider → `kind=World`、`target`＝它、`damage`＝0、`distance`＝命中距離；打到沒有 `Shootable` 的牆、或打到地板 → `World`、`target`＝0；玩家在靶子前面 → `Player`；靶子在玩家前面 → 靶子、玩家不扣血。`ChooseShotOutcome` 的同距離規則照舊（world 贏） | 相符 | `tests/object_fps_pvp/world/`（cpu） |
| W2 | `DamageTargets`／`ResetTargets`：扣血＝`ShotDamage(Body)`；到 0 → 壞；壞了不再扣；`resetSeconds` 後回滿；同一步兩人打同一個 → 各扣一次（照 EntityId 順序）；`target` 是沒有 `TargetState` 的實體 → 不扣 | 相符 | 同上 |
| W3 | 模式配置沒開 `targets`：沒有靶子的實體、沒有 `TargetState` 的格子、`kind=World` 的 `ShotRecord` 的 `target` 一律 0 | 相符 | 同上 |
| C1 | Client 整合（`ClientTests.cpp` 的 `Setup`）：對手在視野裡 → 頭上有名牌，文字＝`PlayerLabel(對手的 userId)`、**沒有血條**；位置＝錨點換算的值；**同一幀**轉動視角，名牌的 `GlobalTransform` 跟著變；自己沒有 | 相等 | `object_fps_pvp.client.*`（cpu） |
| C2 | 對手被打中很多次、血量變了 → 仍然沒有血條（敵方玩家不顯示血條的決定） | 相符 | 同上 |
| C3 | 視角轉 180° → 隱藏；對手走到牆後 → 隱藏；對手離開 → 名牌實體被移除 | 相符 | 同上 |
| C4 | `sandbox` 模式配置、`pvp_arena`：`engaged` 的靶子沒打 → 沒有血條；自己打中 → 出現、寬度照比例、命中標記也出現；`holdSeconds＋fadeSeconds` 之後 → 消失；`always` 的靶子一進場就有血條與名字；`WorldSnapshot::targets` 少了某個靶子 → 它只有名字、沒有血條 | 相符 | 同上 |
| C5 | 遮蔽（第 4 步）：對手站在關著的門後 → 隱藏；門開了（照 Frame 畫的姿勢轉到不擋的角度）→ 顯示；靶子在中間 → 隱藏；門帶 `NotReceiving` 時照它停著的姿勢判 | 相符 | 同上 |
| A/B | #367 的核心 PvP 腳本：改前、改後權威 digest 逐位元組相同（`JudgeShots` 的改動在核心 PvP 不改變任何結果）；報告只差 `ShotRecord` 的 Codec 版本 | 相同 | macOS 本機（Linux 照 #367 的做法，主機開著時） |

- 實機看一次（名字晃不晃、牆角閃不閃、字的大小、靶子的血條）：交使用者 playtest。CI 沒有 GPU 的畫面比對；本機不做 GPU 量測。

### 5.1 突變（D184：新的，find 在改動範圍裡）

預定的方向（id `pvp-overhead-*`、`pvp-target-*`，實際在各步定）：

- 鏡頭後面不隱藏（`WorldToViewport` 的失敗被當成 (0,0)）；畫面外不隱藏；遮蔽的判斷拿掉。
- `Engaged` 當成 `Always`；淡出不降；低血量的條件拿掉或方向反了。
- 敵方玩家寫了血條；自己也寫 `Overhead`；死了不拿掉。
- 自己的命中紀錄記錯對象（記成自己，或不看 `kind`）。
- 比例用 milli-points（1 milli-point 時變 0）。
- Y 方向翻錯；`headClearance` 沒加上去；目標離開後名牌實體沒移除。
- `JudgeShots` 不看 `Shootable`（所有牆都記 `target`）；`Shootable` 的 `target` 記成 0；`DamageTargets` 不看 `tick`（舊的 `ShotRecord` 每步重扣）、打壞的也扣；`ResetTargets` 不復原。
- 擋物只用地圖的牆（門、靶子不擋）；門照 `DoorState` 而不是畫出來的姿勢；`targets` 少了那一筆時還畫舊的血條。

---

## 6. 和門線（#375）的對齊

| 項目 | 這張卡 | 門卡 |
|---|---|---|
| scene 格式、`readScene`、`kSceneEntities`、`sceneDigest`、`Placed` | **照用**，不改 | 做（#375 第 2、4 步） |
| `sandbox` 模式配置、`scenes/pvp_arena.json` | 加一個 `targets` 組、加三個物件（`id` 101～） | 建檔（#375 第 5 步），門用小號（目前 `id` 1） |
| 物件 `id` 的分配 | 101 起 | 1 起。**寫的人之間的約定，不是格式**：reader 不檢查各組的號段；之後由工具分配；寫進 scene 的資料契約頁（#375 9e0a29c5，5.1） |
| 長方體的 `Mesh3d` | 用門的 helper | 門先做成共用的 helper（#375 第 6 步）：參數是大小、顏色、原點在長方體的哪裡（門＝門軸那一邊，靶子＝底面中心） |
| `WorldSnapshot` 帶 scene 物件的狀態 | `targets`：`std::vector<{EntityId id; TargetState}>` | `doors`：`std::vector<{EntityId id; DoorState}>`。每組一個 vector、照 `id` 由小到大，由那一組的 Sim 側 System 填（#375 9e0a29c5，第 8 節） |
| 開賽前 Peer 看得到 `Placed` 的實體 | 靶子自動適用 | `ShowPlayers` 改一行（#375 3.2） |
| `JudgeShots` | 有 `Shootable` 的 collider 記 `target`（O8 (a′)）；不加 `ShotHitKind` 的值 | 不改（門經 `Walls` 自動擋子彈；門沒有 `Shootable`，仍是 `World`、`target`＝0） |
| 看不見時（P2） | 照門：預設卸下、不放 `WhenHidden`；Sim 側不把沒在收資料的靶子放進 `targets`，Frame 只看「有沒有那一筆」 | 兩種預設＋`NotReceiving`（#375 1.5、4.2） |
| 頭頂 UI 的遮蔽 | 第 4 步起看 Frame 畫出來的門與靶子（4.3） | 門的姿勢在 Frame 怎麼擺（#375 3.2、第 8 節），照用 |
| 世界點 → HUD 座標 | 第 4.4 節的函式 | 互動提示如果也要，共用它 |

門卡已確認修訂 2 的這張表（2026-11-28，#375 9e0a29c5）；修訂 3 改的三列（`JudgeShots`、P2、遮蔽）照門卡修訂 2（e0732917、9984c4aa）的寫法，門卡那一側不用改。確認時的內容：沒有矛盾；門沒有 `Shootable`，打到仍是 `World`；靶子沒有 `Interactable`，按 E 的判定只把它當成擋住射線的牆。`ShotRecord` 的 Codec 由這張卡的第 3 步升，#375 不改 `ShotRecord`。（當時寫的是 `Object`；O8 改成 (a′) 後，門照樣是 `World`、`target`＝0。）#375 的 D2（互動鍵的 wire）和靶子無關。

---

## 7. 分批

一條線、一個 PR（宣告這個 draft PR 之後接著放實作）。第 1、2 步只等 #367；第 3、4 步還要等門線的 `Placed`、scene、`sandbox`（#375 第 2、4、5 步）合併。

```
v8-arch-gyo2-overhead-ui（一個 PR，開向 gyo2）
├─ 第 1 步｜Opus/high  ｜上段｜present/Overhead（位置、隱藏、透明度、比例）、ui/overhead.json、PlayerLabel；P1～P5（靶子的 data 除外）
├─ 第 2 步｜Opus/high  ｜下段｜Frame World：Overhead、OwnHits、NoteOwnHits、ShowOverheads、TagPlayerOverheads（名字）；C1～C3
│  ── 等門線的 Placed、scene、sandbox 合併 ──
├─ 第 3 步｜Opus/high  ｜上段｜targets 組：readScene、TargetState、Shootable、JudgeShots 記 Shootable 的實體、DamageTargets、ResetTargets、sandbox 與 pvp_arena 的物件；W1～W3、A/B
├─ 第 4 步｜Opus/high  ｜下段｜Client：靶子的網格、WorldSnapshot、TagTargetOverheads、打到 Shootable 的命中標記、畫出來的擋物；C4、C5
└─ 第 5 步｜Opus/medium｜下段｜突變、A/B（macOS、Linux）、設計審查、純淨 Code Review、dev_log
```

| 步 | 內容 | 上段／下段（根據） | 實作者 | 審查者 | 檢驗者 |
|---|---|---|---|---|---|
| 1 | 第 3.2、3.4、4.2～4.5 節：`present/Overhead.hpp`＋`.cpp`；`assets/object_fps_pvp/ui/overhead.json`；`PlayerLabel` 抽出、`RoomScene` 改用它 | **上段**：新的內容檔（資料形式） | Opus／high | Opus／high（第 5 步一起） | Opus／high |
| 2 | 第 3.1、3.3、4.1 節（玩家的部分）：`ClientWorld.cpp` | 下段：Client 的表現，不動 wire、權威、模式配置 | Opus／high（要先懂 Frame World 與 HUD） | Opus／high | Opus／high |
| 3 | 第 3.5 節的 Match 側與資料 | **上段**：核心 System（`JudgeShots`、`ChooseShotOutcome`）、新的複製 component 與 Codec、`ShotRecord` 的 Codec、scene 的新組 | Opus／high | Opus／high | Opus／high |
| 4 | 第 3.5 節的 Client 側、`TagTargetOverheads`、4.3 的擋物（O9 選 (a) 時含聲音） | 下段：Client 的表現 | Opus／high | Opus／high | Opus／high |
| 5 | 第 5.1 節的突變；A/B；設計審查（Opus／high，唯讀，報告放主 checkout `build/evidence/gyo2-overhead-ui/`）；純淨 Code Review（Fable／high，只給 git 範圍）；dev_log | 下段（執行與紀錄）；檢驗上段的成果，不用小模型 | Opus／medium | — | Opus／medium |

- 模型都不超過主會話（Opus）；Fable 只在純淨 Code Review（navigator 規則）。
- **時間**：第 1 步約半天，第 2 步約一天，第 3 步約一天，第 4 步約半天，第 5 步約半天（Linux 的 A/B 約 30 分鐘，要主機開著）。超過 1.5 倍就停下回報。
- 門線如果晚，第 1、2 步做完時可以先停下，讓主會話決定先合併還是等（合併照舊只在使用者說「合併」時）。
- **停下回報的條件**：第 3 步開工時門線的 `Placed`、scene、`sandbox` 還沒合併；A/B 不相同；`CastRay` 對旋轉的 `Cuboid` 有缺陷（門卡也列了同一條）。

---

## 8. 要使用者決定的點

O6～O8 已定（第 9.3 節）。修訂 3 多一個：

| # | 問題 | 選項 | 推薦 |
|---|---|---|---|
| **O9** | 第 4 步讓門與靶子擋住頭頂的名字（4.3）時，**聲音的遮蔽要不要一起換** | (a) **一起換**：名字與聲音都看「Frame 畫出來的擋物」（地圖的牆＋門＋靶子）(b) 只換名字，聲音照舊只看地圖的牆 | **(a)**：「被門擋住」對看與聽是同一個意思（語意一致）；門關著時槍聲照現在的悶聲規則變小（`ClientWorld.cpp:1096`）。代價：改到既有的聲音功能（這張卡原本不碰），`ClientWorld.cpp` 幾行，用 C5 的同一組場景加聲音的測試。(b) 的話，關著的門後的槍聲聽起來像沒有門 |

卡片自己判的（不是決策點，列出來讓你知道；要改就說）：

- 螢幕空間，不做世界空間的 billboard；不走 gyo.ui（第 2.3 節）。Engine 不改。
- 標記＝遊戲層的 `Overhead`，由各種東西自己的來源 System 寫；顯示只有一套（第 3.1 節）。
- 照 Overwatch 取的：血條交戰中才顯示（自己剛打中、或血量低）、被擋住不顯示、淡出；**不取**「瞄準時顯示」（2.4 第 4 點）。
- **不設距離上限**；名字看得到就顯示（2.4 第 3 點、使用者的第 1 句話）。
- 「自己剛打中」看 `ShotRecord`，和命中標記同一份資料；打中靶子也出命中標記（第 3.5 節）。
- 核心 PvP 也顯示其他玩家的名字；模式配置不改（第 3.2 節）。
- 遮蔽：第 1、2 步只看地圖的牆（和聲音一樣）；第 4 步起加上 Frame 畫出來的門與靶子（第 4.3 節；聲音是 O9）。
- 靶子沒在收資料時只畫名字、不畫舊的血量（第 3.5、4.1 節）。
- 打到 `Shootable` 的 `ShotRecord::damage` 是 0，扣多少看 `TargetState`（`ShotRecord` 只有 `JudgeShots` 寫，第 3.5 節）。
- 外觀、低血量門檻、秒數放 `ui/overhead.json`（第 3.4 節）；血條不分格（第 4.5 節）。
- 打壞的靶子留著 collider、過 `resetSeconds` 回滿（第 3.5 節）。

## 9. 使用者的決定（2026-11-27～28）

都是主會話轉交。

### 9.1 O1（HANDOFF `v8-arch-gyo2-overhead-ui-o1`）

**原話**（逐字）：

1. 「這個很微妙，我想了想有血條的 FPS其實不多。關鍵是如果遠了看不到血條，但是又能看到人，覺得怪怪的。」
2. 看完 side chat 的比較之後：「可以，玩家沒有血條，而怪物有。參考Overwatch。」

| # | 決定 | 比過的選項 | 理由 |
|---|---|---|---|
| O1 | 敵方玩家不顯示血條（受傷的資訊靠命中標記、受擊動作、音效）；怪物顯示血條，參考 Overwatch；距離、遮擋、淡出照 Overwatch 的實際行為核對（第 2.4 節） | 修訂前的 (a) `core_pvp` 開、(b) 另建模式開、(c) 只開名字；side chat 另提的「瞄準或命中時才顯示」（Apex、The Division 一派） | 寫實射擊多半不給敵方玩家血條；有距離上限時會「看得到人、看不到狀態」；怪物（尤其 Boss）顯示血條是 PvE 的常見做法 |
| O2 | (N1)：玩家的名字用和房間同一個字串 | (N2) 真的顯示名稱 | 第 3.2 節 |

### 9.2 名字、隊友、驗證（HANDOFF `v8-arch-gyo2-overhead-ui-names-targets`）

**原話**（逐字）：「顯示名字，隊友也要顯示。驗證的話，在測試地圖裡建幾個靶子。」

| # | 決定 | 比過的選項 | 本文 |
|---|---|---|---|
| O3 | 顯示名字，來源 (N1)；隊友也顯示名字。隊友的血條使用者沒有說 → O6 | 修訂 1 的 O3 (a)（敵方玩家與一般怪不顯示名字）、(b)、(c) | 3.2 |
| O4 | 驗證用測試地圖裡的靶子，代替還沒有的怪物；靶子是 Scene 的物件，格式和門卡對齊 | 修訂 1 的 (V1) 測試裡插 `Overhead`、(V2) 另開訓練假人卡、(V3) 和怪物卡一起 | 3.5、第 6 節；放法 → O7 |

修訂 2 跟著動的：名字的表（3.2，敵方玩家顯示名字）、靶子（3.5）、門線的對齊（第 6 節）、測試（W1～W3、C1～C4、A/B 回來了）、分批（多了第 3、4 步，等門線）。

### 9.3 O6～O8（HANDOFF `v8-arch-gyo2-overhead-ui-o6-o8`，2026-11-28，side chat）

宣告的動工前核准還沒給。

| # | 決定 | 原話（逐字） | 比過的選項 | 理由 | 本文 |
|---|---|---|---|---|---|
| O6 | **(b)**：隊友頭頂只顯示名字、不顯示血條。隊友的狀態用第三人稱動作（不同的 idle 等）表現，敵我都看得到；這是作者的規則，Object_PvP 由我們當作者自己選。這次不實作（等有隊伍、E1 #377、姿勢組 D4、`LimbHealth`） | 「我選（b），但是可以用第三人稱的 idle 狀態，現實不同的任務動畫，來表現隊友的狀態。」「兩邊都看得到。但是，我得說，這個不是寫死的機制，而是模式配置的規則。看作者的想法來定。」「這是看作者的實作，方式，他可以寫死在程式裡（不推薦就是了），也可以放在模式配置裡。看作者自己的決策。只是 Object_PvP 我們自己就是作者，所以這個方案是我們自己做的。」 | (a) 名字＋常駐血條（卡片原推薦）；(c) 只在 `Engaged` 時顯示；只讓隊友看到狀態動作（同一角色在不同人眼裡播不同動畫，和 Match 判定的姿勢只有一份不一致） | 身體語言不是 UI；撤離射擊靠觀察，敵人看得出重傷、看不出數字。「Object_PvP 放進模式配置」是 side chat 的讀法，使用者沒有反對、也沒有明說 | 3.2 |
| O7 | **(a)**：靶子放 `pvp_arena`、開 `sandbox` 模式配置、和門共用同一個 scene 檔 | 「可以。」 | (b) 另建 `targets` 模式配置；(c) 放大地圖 | 第 3.5 節的表 | 3.5 |
| O8 | **(a′)**：`JudgeShots` 把有 `Shootable` 的 collider 也當成候選，只記打到的實體與距離；靶子、之後的怪物、箱子等各由自己的組讀 `ShotRecord` 處理；不在 `ShotHitKind` 為每種東西加值；玩家的判定（PV2）照舊；`ShotRecord` 的 Codec 升版本，核心 PvP 的結果不變、用 A/B 證明 | 先問「別家的做法呢？」，看完對照後：「照推薦」 | 卡片的 (a) 加 `ShotHitKind::Object`；(b) 靶子的組自己再打一次射線 | 別家一槍只判一次、判定只回答打到哪個實體：avian `RayHitData`（`ray_caster.rs:403-414`）、Godot `RayCast3D::get_collider`（`ray_cast_3d.cpp:89`）為源碼；Unreal `ApplyDamage`→`TakeDamage`、Source `FireBullets`→`TraceAttack` 是記憶、未確認 | 3.5 |

這一輪都在遊戲層，Engine 不改。修訂 3 跟著動的：第 0、1 節、3.2（隊友）、3.5（O8 的做法、看不見時、Client）、4.1、4.3（擋物照門卡修訂 2 重對）、第 5 節（W1～W3、C4、C5、突變）、第 6～8 節（O9）。

## 10. 沒查到、未確認的

- Overwatch：「打中才顯示」的基本規則、顯示秒數與淡出、距離上限、低血量門檻的數值、敵我名字、血條分格與顏色、PvE 敵人的血條，都沒有找到官方說明（第 2.4 節逐條標了等級）。2024-07 的 patch notes 頁面本身沒有打開，文字來自搜尋摘要與兩家轉載。
- Bevy 社群的名牌做法：憑記憶。
- PostUpdate 裡同時寫 `Transform` 與 `GlobalTransform`，這一幀就畫在新位置、不被 visibility 的剔除擋掉：從排程推，第 2 步用 C1 確認。
- 門線（#375）已核准動工（修訂 2），還沒合併；第 3、4 步照它目前的宣告寫，它改了就跟著改。
- 擋物要每幀對門與靶子的 `Cuboid` 打射線（照 Frame 畫的姿勢）：門與靶子只有幾個，不量；`CastRay` 對旋轉的 `Cuboid` 有沒有缺陷，第 7 節的停止條件已列。
- 很多怪物時每幀的射線成本：沒量，等怪物那張卡。

**還沒開始**：核准只針對這份宣告；實作等 #367 合併、主會話通知。
