# 頭頂 UI（名字與血條）：事前宣告（`v8-arch-gyo2-overhead-ui`）

任務卡，2026-11-27。這一階段只寫事前宣告，交使用者核准；核准前不寫實作程式。#367（模式配置，`v8-arch-gyo2-mode-config`）改的是同一個遊戲的檔案，**它合併之前不動程式**，實作等主會話通知。作者 Opus 5.5／high。

行號以 `gyo2` 85df84da 為準；標「#367」的，是 #367 分支（`claude/gyo2-mode-config-declaration`）上的檔案。

**修訂 1（2026-11-27）**：使用者改了 O1 的方向（第 9 節）：**敵方玩家不顯示血條，怪物顯示血條，參考 Overwatch**。機制照原設計；顯示與否改成依對象的種類；模式配置不再為這張卡改（第 3.4 節）。

---

## 0. 前提與結論先講

### 0.1 已定的前提（這份宣告不推翻）

| 前提 | 出處 |
|---|---|
| 路線 (A)：新功能加在 Object_PvP，由模式配置決定這一場掛哪些系統 | HANDOFF `v8-arch-gyo2-object-pvp-route-a` |
| 最終產品是有怪物的撤離射擊；Object_PvP 驗證核心 PvP 交互與地圖 | HANDOFF `v8-arch-gyo2-tarkov-is-reference` |
| **敵方玩家不顯示血條**（受傷的資訊靠命中標記、受擊動作、音效）；**怪物顯示血條，參考 Overwatch**；名字（O2）照 (N1) | HANDOFF `v8-arch-gyo2-overhead-ui-o1`（第 9 節） |
| Engine 只給機制；Product 的詞不進 Engine；錯誤用 `Result` 只告知；不對誤用加守衛；ECS 相關以 Bevy 為主參考 | AGENTS §1、批次表〈每批的設計審查〉第 5 項 |
| 使用者原則：「行為要統一、語意要一致」 | 卡片 brief（主會話轉交） |

### 0.2 結論（一段話）

**Engine 不改，模式配置也不改。** 做在 Client 的 Frame World，畫在**螢幕空間**，用 HUD 已經在用的那一套：HUD camera（2D、1280×720）上的 `Sprite`（文字圖）與 `Shape2d`（血條）。每幀：(1) 每一種「會有頭頂 UI 的東西」由它自己的**來源 System** 寫一個遊戲層的 component `Overhead`（錨點、名字、血量比例、顯示的方針）；(2) 一個共用的 `ShowOverheads` 依方針決定顯不顯示、透明度多少，再用 `Camera::WorldToViewport`（世界 camera）與 `Camera::ViewportToWorld2d`（HUD camera）換成 HUD 座標擺上去。方針照 Overwatch：**交戰中**（自己剛打中它、或它血量低）才顯示，沒有再打中就淡出；被牆擋住、在鏡頭後面、在畫面外就不顯示；**不設距離上限**（看得到就看得到狀態）。Boss 這類由定義資料標成**一直顯示**。敵方玩家沒有來源 System，所以什麼都不顯示。

---

## 1. 現況（照程式核對）

| 項目 | 程式 | 對這張卡的意思 |
|---|---|---|
| `WorldToViewport` | `engine/modules/camera/include/engine/camera/Camera.hpp:176`（Bevy `world_to_viewport`）；鏡頭後面是 `PastFarPlane`、比 near 近是 `PastNearPlane`，失敗不配置記憶體（`:161`） | 直接用；失敗就隱藏 |
| `ViewportToWorld2d` | `Camera.hpp:183`（Bevy `viewport_to_world_2d`） | viewport 座標 → HUD camera 的 2D 座標，不自己寫換算 |
| HUD | HUD camera `kHudCamera`：`ScalingFixed{1280, 720}`、order 2、只看 `kHudLayer`（`games/object_fps_pvp/src/client/ClientWorld.cpp:1908-1914`）；文字用 `TextPicture` 畫成圖、文字不變就不重畫（`:310-320`、`:374`）；`ShowHud`（`:953`） | 頭頂 UI 用同一個 camera、同一個文字圖快取的做法 |
| gyo.ui 文件 | 元件的位置只來自文件（`UiRectTransform`，`engine/modules/ui/include/engine/ui/UiDocument.hpp`）；binding 只能改文字、顏色、slider 的值、清單的項目 | **gyo.ui 不能每幀把元件擺到任意位置**，所以頭頂 UI 不走 gyo.ui（第 2.3 節） |
| 自己打中了誰 | `ShotRecord`：Match 對自己最後一槍的判定，`target` 是實體 id（`world/Components.hpp:70-77`），已複製；命中標記就是看它（`ShowHud`，`ClientWorld.cpp` 的 `h.marker.Observe`） | 「自己剛打中它」照同一份資料，和命中標記一致；`target` 是實體 id，不分玩家或怪物 |
| 血量 | 玩家：`PlayerCombat::hp`（milli-points，`Components.hpp:52`）、`CombatRules::maximumHp`（`sim/CombatRules.hpp:21`）、顯示用 `DisplayPoints`（無條件進位，`:35`）。**怪物還沒有** | 怪物的血量 component 是怪物那張卡的；血條比例照 `DisplayPoints` 的規則（活著的不會顯示 0） |
| 名字 | **沒有顯示名稱**：identity 服務只有 device → userId（`services/identity/identity.go`）；`Owner` 只有 `userId`（`Components.hpp:100`）；房間畫面寫 `"Player " + userId`（`src/client/RoomScene.cpp:70`） | O2 (N1)：要顯示玩家名字時用同一個字串（第 3.2 節） |
| 頭的位置 | 角色的判定形狀（`hitbox.json`）有 `HitPart::Head`（`src/character/CharacterRig.cpp:219`）；`PlaceHitShapes`（`:329`）；`PoseCharacters` 每幀算其他玩家的姿勢（`ClientWorld.cpp:758`） | 有 `Head` 判定形狀的對象，錨點＝它的中心＋偏移（第 3.3 節） |
| 遮蔽 | `PW::Blocked(walls, a, b)`（`world/PvpWorld.hpp:76`）；聲音用它判斷其他玩家的聲音被牆擋住（`ClientWorld.cpp:1076-1082`） | 頭頂 UI 用同一個判斷 |
| 怪物 | **還沒有**（不在 `gyo2`） | 第一批的驗證對象：決策點 O4（第 7 節） |

---

## 2. 成熟方案

查證等級：**源碼**（本機 `~/Code/GitHome`）、**官方**（官方文件或 patch notes）、**△**（二手資料：新聞、第三方指南、論壇）、**未確認**（憑記憶、沒找到出處）。

### 2.1 Bevy（主參考，源碼 `bevy` 6a911653f）

| 什麼 | 位置 |
|---|---|
| `world_to_viewport`：NDC z < 0 是 `PastFarPlane`、> 1 是 `PastNearPlane`，Y 翻成向下 | `crates/bevy_camera/src/camera.rs:543-590` |
| `viewport_to_world_2d`；範例把 `world_to_viewport` 與 `viewport_to_world_2d` 來回換、互相驗證 | `camera.rs:713`；`examples/2d/2d_viewport_to_world.rs:29-38` |
| 引擎自己的用法：camera controller 用 `world_to_viewport` 算錨點在螢幕上的大小，讓指示圖示保持固定像素大小 | `crates/bevy_camera_controller/src/pan_orbit_camera/extensions/anchor_indicator.rs:61-71` |
| UI 的排版在 PostUpdate、排在 `CameraUpdateSystems` 之後（螢幕空間 UI 要用這一幀的 camera） | `crates/bevy_ui/src/lib.rs:151-163` |

Bevy 本身沒有名牌或血條的元件；社群的做法是「一個 System 每幀用 `world_to_viewport` 算位置，寫 UI `Node` 的絕對位置」，**未確認**（社群 crate 不在本機）。

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
| Overwatch | 血條固定大小、畫在頭頂（2.4） | — |

**判定：螢幕空間。** 理由：Engine 已有全部的機制；固定大小、清楚的血條是 Overwatch 的樣子；擋住的判斷和聲音用同一個。

不用 gyo.ui：gyo.ui 的位置是文件裡寫死的（第 1 節）；要支援就要改 `gyo.ui` 資料契約（版本 3）。這次不做，只有這一個使用者（AGENTS §1）。

### 2.4 Overwatch：血條什麼時候出現

Overwatch 是 PvP 的英雄射擊，沒有「怪物」；使用者要的是**把它對敵方英雄的血條規則，用在我們的怪物上**。

| 規則 | 內容 | 查證 |
|---|---|---|
| 血量低時顯示 | 2024-07-09（第 11 季中）patch notes：敵人在 Low Health 門檻時顯示血條，預設開 | **官方**（<https://overwatch.blizzard.com/en-us/news/patch-notes/live/2024/07/>，頁面本身沒有開；文字見搜尋摘要與兩家轉載：Dexerto <https://www.dexerto.com/overwatch/overwatch-2-players-are-loving-this-game-changing-new-enemy-hp-feature-2824295/>、OW Cavalry <https://x.com/OWCavalry/status/1810745929693286554>）。**門檻是多少，沒查到** |
| 打中之後顯示 | 自己打中的敵人顯示血條；2018 年起「自己做出的物件（砲台等）造成傷害」也算 | △（搜尋摘要引用的舊 patch notes，原頁未開）；「打中才顯示」的基本規則本身是 **未確認**（憑記憶，所有來源都把它當前提） |
| 瞄準時顯示 | 第三方指南說瞄準敵人時血條一定顯示；另一份 Custom Game 的說明說預設可見、可關。**兩份互相矛盾** | △（<https://fortunerapps.com/overwatch-2/how-to-customize-hud-in-overwatch-2>，抓取時 404，內容來自搜尋摘要；<https://overwatch.fandom.com/wiki/Custom_Game>） |
| 被擋住 | 打中過的敵人躲到掩體後，看不到它的血條；看得到模型時才看得到 | △（官方論壇的玩家討論 <https://us.forums.blizzard.com/en/overwatch/t/make-health-bars-visible/316809>，不是官方說明） |
| 顯示多久、怎麼淡出 | 沒查到秒數 | **未確認** |
| 距離上限 | 沒查到 | **未確認**（「Motion Detection 25 m」是 Hacks & Hijinx 模式的修飾，不是一般規則，官方 2025-06 patch notes） |
| 隊友 | 有「隊友頭頂血條」的開關；PvE 與部分活動把隊友血量放在畫面左下 | △（Dexerto <https://dexerto.com/overwatch/hidden-overwatch-2-update-adds-highly-requested-feature-players-have-waited-ages-for-3001067>；第三方指南） |
| 名字 | 隊友頭頂有名字、敵人沒有 | **未確認**（憑記憶） |
| 血條的樣子 | 每 25 HP 一格；白＝生命、黃＝護甲、藍＝護盾 | **未確認**（憑記憶；Fandom 的 Hit points 頁回 402，沒讀到） |
| PvE 的敵人（Null Sector、Junkenstein） | 沒查到 | **未確認** |

**取用的概念**（只取有出處的部分，秒數與門檻由我們的資料定）：

1. **交戰中才顯示**：自己剛打中它（官方的「低血量」＋△ 的「打中」），沒有再打中就淡出。
2. **血量低時顯示**（官方）。
3. **被擋住就不顯示**（△，和 GYO 聲音的遮蔽同一個判斷）。
4. **不設距離上限**：沒找到 Overwatch 有距離上限；使用者也指出「遠了看不到血條、卻看得到人，覺得怪」。
5. **不取「瞄準時顯示」**：來源互相矛盾，而且要每幀對判定形狀打一條選取射線；之後實機覺得需要再加（只動 `ShowOverheads` 的方針，第 4.2 節）。
6. Boss 一直顯示：Overwatch 沒有對應（未確認），這是 PvE 的常見做法，用定義資料的方針 `Always` 表達（3.1）。

---

## 3. 資料：誰有頭頂 UI、顯示什麼

**前提：作者能處理的**（遊戲層的 component 與資料檔）。Engine 不知道「名字」「血量」「怪物」。

### 3.1 標記：`Overhead` component（Frame World，遊戲層）

```
enum class OverheadPolicy { Engaged, Always };
struct Overhead {          // games/object_fps_pvp，Frame World 才有
    Math::Vec3 anchor;     // 這一幀頭頂的世界座標（已加上偏移）
    std::string label;     // 空：不顯示名字
    std::optional<float> health;  // 0～1；none：不顯示血條
    OverheadPolicy policy = OverheadPolicy::Engaged;
};
```

- **誰寫**：每一種東西自己的**來源 System**。怪物那張卡加 `TagMonsterOverheads`：從怪物的定義資料讀名字（有的才寫）、錨點、方針（一般怪 `Engaged`、Boss `Always`）。`ShowOverheads` 不用改。**有 `Overhead` 就是「要顯示頭頂 UI」**，和 Bevy「有 component 就被 query 到」同一個做法。
- **敵方玩家沒有來源 System**（第 9 節的決定）：不寫 `Overhead`，什麼都不顯示。
- **不是決策資料**、不複製、`HashValue` 不寫東西（和 `ClientHud` 一樣）。放在 Frame World 裡那個對象的副本實體上：對象離開、實體被移除，`Overhead` 跟著不見。
- 為什麼不讓 `ShowOverheads` 直接讀血量的 component：那樣它就要認得每一種東西的血量（玩家的 `PlayerCombat`、怪物的另一個、G6 之後可能有部位血量）。來源 System 把各自的資料翻成同一個形狀，顯示的規則只有一套（行為統一）。

### 3.2 名字

O2 已定 (N1)：**要顯示玩家名字時**，用和房間畫面同一個字串。把 `RoomScene.cpp:70` 的 `"Player " + userId` 抽成 `PlayerLabel(userId)`，房間與頭頂都呼叫它。真的顯示名稱（identity＋wire）等「玩家要能取名字」成為功能時另開卡，只換 `PlayerLabel` 的來源。

**誰顯示名字**還沒定。建議（**決策點 O3**，第 7 節）：

| 對象 | 名字 | 血條 | 理由 |
|---|---|---|---|
| 敵方玩家 | 不顯示 | 不顯示（已定） | 寫實射擊的主流；名字和血條一樣會變成「隔很遠也知道是誰」的資訊 |
| 一般怪物 | 不顯示 | `Engaged` | 名字對一般怪沒有資訊量；打中它時要知道還剩多少 |
| Boss、精英 | 顯示（定義資料的名字） | `Always` | PvE 的常見做法：知道在打誰、還剩多少 |
| 隊友 | 顯示 `PlayerLabel`；血條待定 | — | **現在沒有隊伍**，等協力模式那張卡再定；Overwatch 有隊友頭頂血條的開關（△） |

照這個建議，**這張卡做完，遊戲裡看不到任何頭頂 UI**：玩家都是敵人，怪物還沒有。機制的驗收靠測試（O4）。

### 3.3 錨點

- 有 `Head` 判定形狀的對象（玩家、之後的人形怪）：錨點＝`Head` 的中心（`PlaceHitShapes`），往上加 `ui/overhead.json` 的 `headClearance`（公尺）。它跟著動畫（蹲、跳、倒地），和「打到頭」的頭是同一個點（語意一致）。走路時會跟著頭上下晃幾公分。
- 沒有 `Head` 的怪物：由怪物的定義資料給錨點（那張卡的事）。
- 算錨點是來源 System 的事；`ShowOverheads` 只拿 `anchor`。

### 3.4 模式配置：這張卡不改

修訂前的設計是「玩法組 `overhead` 的設定決定顯示名字、血條、距離」，會讓 `core_pvp` 的 `rulesDigest` 變。修訂後：

- 敵方玩家什麼都不顯示，**核心 PvP 不需要任何設定**，`core_pvp.json` 與 `rulesDigest` 不動。
- 怪物的方針（`Engaged`／`Always`、有沒有名字）是**怪物定義資料**的一部分；那份資料要不要進比對值，是怪物那張卡照 #367 的規則決定的。
- 「某個模式要把頭頂 UI 全關」（例：硬派模式）現在沒有人要，**不做**；有了再加一個玩法組，`ShowOverheads` 掛條件就好（#367 的機制已經在）。
- 外觀（大小、顏色、淡出秒數、低血量門檻）放 `ui/overhead.json`（照 `ui/hit_feedback.json`），Client 自己讀，不進比對值。低血量門檻算外觀而不是規則的理由：它只決定「要不要把已經複製到 Client 的血量畫出來」，不改任何判定。

---

## 4. 每幀做的事

**前提：作者能處理的**（遊戲層的 System）。

### 4.1 System 與排程

| System | 何時 | 做什麼 |
|---|---|---|
| 來源 System（怪物那張卡起） | Main／Update，在它的姿勢算好之後 | 寫 `Overhead` |
| `NoteOwnHits` | Main／Update | 看自己的 `ShotRecord`：`tick` 是新的、`kind` 是命中 → 記下「`target` 在這個時間被自己打中」（`OwnHits` Resource，依實體 id）。**和命中標記看同一份資料**：命中標記出現的那一槍，就是讓血條出現的那一槍。目標離開就刪掉它的紀錄 |
| `ShowOverheads` | Main／PostUpdate，在 camera 的 `kUpdateSet` 與 transform 的 `kPropagateSet` 之後、visibility 的 `kCheckSet` 之前 | 每個有 `Overhead` 的實體：方針與透明度（4.2）→ 隱藏（4.3）→ 換座標（4.4）→ 擺它的名牌實體（一個 `Sprite`＋一個 `Shape2d`，在 `kHudLayer`）；目標不見了就移除名牌實體 |

- 排在 PostUpdate 的理由：要用這一幀的 camera（`PlaceCamera` 在 Update 改 camera 的 `Transform`，`GlobalTransform` 與 `ComputedCamera` 在 PostUpdate 才更新）。Bevy 的 UI 排版也在 PostUpdate、`CameraUpdateSystems` 之後（2.1）。
- 名牌實體沒有 parent，`ShowOverheads` 同時寫 `Transform` 與 `GlobalTransform`（和 `Place()` 同一個做法），不等下一幀的 propagate。這一幀就畫在新位置，用測試確認（第 5 節 C1）。
- 名牌實體：`OverheadPart{target}`（照 `CharacterPart`）；文字圖照 HUD 的快取，名字不變就不重畫。
- 沒有任何 `Overhead` 時，三個 System 什麼都不做，不需要模式條件。

### 4.2 方針與透明度（純函式 `present::OverheadAlpha`）

| 方針 | 顯示 |
|---|---|
| `Always` | 一直是 1 |
| `Engaged` | 血量 ≤ `lowHealth`（`ui/overhead.json`）→ 1；否則看最後一次自己打中它的時間 t：`now − t ≤ holdSeconds` → 1，之後在 `fadeSeconds` 內線性降到 0；從沒打中過 → 0 |

- 0 就不擺名牌（也不打遮蔽的射線）。
- 秒數與門檻是我們的值（Overwatch 的沒查到，2.4）。

### 4.3 隱藏的條件（純函式 `present::PlaceOverhead`）

回傳「HUD 座標」或「隱藏的原因」：

| # | 條件 | 判斷 | 原因 |
|---|---|---|---|
| 1 | 不是要顯示的對象 | 沒有 `Overhead`（敵方玩家、自己） | — |
| 2 | 在鏡頭後面、比 near 近 | `WorldToViewport` 失敗（`PastFarPlane`／`PastNearPlane`） | `Behind` |
| 3 | 在畫面外 | 換出來的 viewport 座標不在 viewport 裡 | `OffScreen` |
| 4 | 被牆擋住 | `PW::Blocked(walls, camera 位置, 錨點)`：牆來自 Sim World 的地圖（和聲音用的同一份 `walls`） | `Occluded` |

- **沒有距離條件**（2.4 第 4 點）。camera 的 far（150 m）以外本來就換不出來（`PastFarPlane`），而且那時候身體也畫不出來，所以「看得到人卻看不到狀態」不會發生。
- 被擋住時淡出的計時照走：從牆後走出來，還在 `holdSeconds` 內就會再出現。
- 順序由便宜到貴：先 2、3，最後才打射線；透明度是 0 的不判斷。射線的成本：沒量，怪物多時再看（第 8 節）。
- 只看牆、不看其他身體：和聲音的遮蔽一樣。不夾到畫面邊緣。
- `WorldToViewport` 的失敗是「這一次沒換」，不是錯誤；`ShowOverheads` 不回傳錯誤（E2：這一個名牌，不是 World）。

### 4.4 座標

`WorldToViewport(世界 camera, 錨點)` → viewport 的邏輯像素（左上原點、Y 向下）→ `ViewportToWorld2d(HUD camera, 那個點)` → HUD 的 2D 座標（中心原點、Y 向上）。兩個 camera 在同一個 target、都沒有 viewport，所以是同一個像素。不自己寫「÷寬×1280」的換算。

HUD camera 是 `ScalingFixed{1280, 720}`，視窗不是 16:9 時 HUD 會被拉長；名牌和現有的 HUD 文字一樣（現況，不在這張卡改）。

### 4.5 血條的樣子

`ui/overhead.json`：寬、高、底色、血量色、低血量色、和名字的間距、文字大小、`headClearance`、`lowHealth`、`holdSeconds`、`fadeSeconds`。比例＝`DisplayPoints(hp) / DisplayPoints(最大值)`，活著的不會變成空條。分格（Overwatch 每 25 HP 一格，未確認）**這次不做**：怪物的血量級距還不知道，有了再加一個 `segmentPoints` 欄位。

---

## 5. 外部可驗的測試

驗證對象照 O4 推薦的 (V1)：**測試直接在 Client 的 Frame World 裡，給對手玩家的副本插一個 `Overhead`**（當作「某一種會顯示的東西」），遊戲本身不加任何來源 System。

| # | 測試 | 判定 | 在哪 |
|---|---|---|---|
| P1 | `PlaceOverhead` 的位置：正前方 d 公尺的點 → HUD 的 (0, 0)；往右偏一個已知角度 → 由 FOV 手算的 x；往上 → y 變大。另外和「`WorldToViewport`→`ViewportToWorld2d`」來回驗證（照 Bevy 範例） | 誤差 < 0.01 HUD 單位 | `tests/object_fps_pvp/OverheadTests.cpp`（logic，cpu） |
| P2 | 隱藏：鏡頭後面 → `Behind`；畫面外 → `OffScreen`；一面牆在中間 → `Occluded`，牆移開 → 顯示；錨點貼著牆的這一面 → 顯示；140 m 遠、沒有牆 → 顯示（沒有距離條件） | 原因逐一相符 | 同上 |
| P3 | 透明度：`Always` → 1；`Engaged` 從沒打中 → 0；打中後 `holdSeconds` 內 → 1；淡出的一半 → 0.5；之後 → 0；再打中 → 1；血量 ≤ `lowHealth` → 1 不論打中與否 | 相等 | 同上 |
| P4 | 血條比例 `DisplayPoints` 的規則（hp＝1 milli-point → 1/100，不是 0）；`PlayerLabel` 和房間畫面的字串同一個 | 相等 | 同上 |
| P5 | `ui/overhead.json`：缺欄位、型別錯、負數、`lowHealth` 不在 0～1 → 讀取失敗並說哪一欄 | 錯誤訊息相符 | 同上 |
| C1 | Client 整合（`ClientTests.cpp` 的 `Setup`，Match＋Client 記憶體連線）：對手的副本有 `Overhead{policy=Always}` → 有一組名牌實體在 `kHudLayer`，位置＝錨點換算的值；**同一幀**轉動視角，名牌的 `GlobalTransform` 跟著變 | 相等 | `object_fps_pvp.client.*`（cpu） |
| C2 | `policy=Engaged`：沒打中 → 沒有名牌；Match 寫自己的 `ShotRecord` 命中對手（照命中標記的測試寫法）→ 出現；`holdSeconds＋fadeSeconds` 之後 → 消失 | 相符 | 同上 |
| C3 | 視角轉 180° → 隱藏；對手走到牆後 → 隱藏；對手離開 → 名牌實體被移除 | 相符 | 同上 |
| C4 | **遊戲本身**：對手玩家（沒有插 `Overhead`）跑很多幀、打中很多次，都沒有名牌實體（敵方玩家不顯示的決定）；自己也沒有 | 相符 | 同上 |

- 這張卡不動 Sim、Match、模式資料：權威 digest 不會變，**不跑 A/B**；`object_fps_pvp.*` 既有測試全過即可。
- 遊戲裡看不到效果（O3 的建議下），實機要等怪物；本機不做 GPU 量測。

### 5.1 突變（D184：新的，find 在改動範圍裡）

預定的方向（id `pvp-overhead-*`，實際在各步定）：

- 鏡頭後面不隱藏（`WorldToViewport` 的失敗被當成 (0,0)）。
- 畫面外不隱藏。
- 遮蔽的判斷拿掉。
- `Engaged` 當成 `Always`。
- 淡出不降（`holdSeconds` 之後仍是 1）。
- 低血量的條件拿掉、或比較的方向反了。
- 自己的命中紀錄記錯對象（記成自己，或不看 `kind`）。
- 血條比例用 milli-points（hp＝1 時變 0）。
- Y 方向翻錯（直接用 viewport 的 y）。
- `headClearance` 沒加上去。
- 目標離開後名牌實體沒移除。

---

## 6. 分批

一條線、一個 PR（宣告這個 draft PR 之後接著放實作）。#367 合併、主會話通知後才開始。

```
v8-arch-gyo2-overhead-ui（一個 PR，開向 gyo2）
├─ 第 1 步｜Opus/high｜上段｜present/Overhead（純函式：位置、隱藏、透明度、比例）、ui/overhead.json、PlayerLabel；P1～P5
├─ 第 2 步｜Opus/high｜下段｜Frame World：Overhead、OwnHits、NoteOwnHits、ShowOverheads、名牌實體與文字快取；C1～C4
└─ 第 3 步｜Opus/medium｜下段｜突變、設計審查、純淨 Code Review、dev_log
```

| 步 | 內容 | 上段／下段（根據） | 實作者 | 審查者 | 檢驗者 |
|---|---|---|---|---|---|
| 1 | 第 3.2、4.2～4.5 節：`games/object_fps_pvp/include/object_fps_pvp/present/Overhead.hpp`＋`src/present/Overhead.cpp`；`assets/object_fps_pvp/ui/overhead.json`；`PlayerLabel` 抽出、`RoomScene` 改用它 | **上段**：新的內容檔（資料形式） | Opus／high | Opus／high（第 3 步一起） | Opus／high |
| 2 | 第 3.1、4.1 節：`ClientWorld.cpp` 的 component、Resource、System | 下段：Client 的表現，不動 wire、權威、協議、模式資料 | Opus／high（要先懂 Frame World 與 HUD） | Opus／high | Opus／high |
| 3 | 第 5.1 節的突變（新的＋find 在改動範圍的）；設計審查（Opus／high，唯讀，報告放主 checkout `build/evidence/gyo2-overhead-ui/`）；純淨 Code Review（Fable／high，只給 git 範圍）；dev_log | 下段（執行與紀錄）；檢驗上段的成果，不用小模型 | Opus／medium | — | Opus／medium |

- 模型都不超過主會話（Opus）；Fable 只在純淨 Code Review（navigator 規則）。
- **時間**：第 1 步約半天，第 2 步約一天，第 3 步約半天。超過 1.5 倍就停下回報。
- **衝突**：`gyo2-interact-door`（門的互動）也可能改 `ClientWorld.cpp` 的 HUD（互動提示）。誰先合併，另一個照 merge-base 跟上；如果它也要「世界點 → HUD 座標」，共用第 4.4 節的那一個函式。

---

## 7. 要使用者決定的點

| # | 問題 | 選項 | 推薦 |
|---|---|---|---|
| **O3** | 誰顯示名字（第 3.2 節的表） | (a) **敵方玩家不顯示；一般怪不顯示；Boss、精英顯示；隊友等協力模式那張卡再定** (b) 怪物一律顯示名字 (c) 現在就定隊友：名字一直顯示（穿牆與否另定） | **(a)**：一般怪的名字沒有資訊量、只增加畫面雜訊；隊伍還不存在，現在定的規則沒有東西可以驗 |
| **O4** | 第一批用什麼對象驗證（怪物還沒有） | (V1) **測試裡給對手的副本插 `Overhead`**：只驗機制，遊戲裡看不到 (V2) 另開一張「訓練假人」的卡：arena 資料放幾個不動的目標，Match 判定命中、有血量、會複製（等於怪物的第一步：目標實體、血量、命中判定都要動 Match 與 wire）；這張卡給它寫來源 System，遊戲裡看得到 (V3) 這張卡先不做，和第一張怪物卡一起做 | **(V1)**：這張卡只做 Client 的表現，(V2) 會把它拉進 Match、wire、權威（上段）；機制先用測試鎖住，怪物卡只要加一個來源 System。如果你想先在遊戲裡看到，選 (V2)，我把假人寫成另一張卡的宣告 |

卡片自己判的（不是決策點，列出來讓你知道；要改就說）：

- 螢幕空間，不做世界空間的 billboard；不走 gyo.ui（第 2.3 節）。Engine 不改。
- 標記＝遊戲層的 `Overhead`，由各種東西自己的來源 System 寫；顯示只有一套（第 3.1 節）。
- 照 Overwatch 取的：交戰中才顯示（自己剛打中、或血量低）、被擋住不顯示、淡出；**不取**「瞄準時顯示」（來源矛盾，2.4 第 5 點）。
- **不設距離上限**（2.4 第 4 點、使用者的第 1 句話）。
- 「自己剛打中」看 `ShotRecord`，和命中標記同一份資料（第 4.1 節）。
- 模式配置不改；低血量門檻、秒數、外觀放 `ui/overhead.json`（第 3.4 節）。
- 血條比例用 `DisplayPoints` 的規則；分格這次不做（第 4.5 節）。

## 8. 沒查到、未確認的

- Overwatch：「打中才顯示」的基本規則、顯示秒數與淡出、距離上限、低血量門檻的數值、敵我名字、血條分格與顏色、PvE 敵人的血條，都沒有找到官方說明（第 2.4 節逐條標了等級）。2024-07 的 patch notes 頁面本身沒有打開，文字來自搜尋摘要與兩家轉載。
- Bevy 社群的名牌做法：憑記憶。
- PostUpdate 裡同時寫 `Transform` 與 `GlobalTransform`，這一幀就畫在新位置、不被 visibility 的剔除擋掉：從排程推，第 2 步用 C1 確認。
- 很多怪物時每幀的射線成本：沒量，等怪物那張卡。

## 9. 使用者的決定（2026-11-27）

主會話轉交，出處 HANDOFF `v8-arch-gyo2-overhead-ui-o1`。

**原話**（逐字）：

1. 「這個很微妙，我想了想有血條的 FPS其實不多。關鍵是如果遠了看不到血條，但是又能看到人，覺得怪怪的。」
2. 看完 side chat 的比較之後：「可以，玩家沒有血條，而怪物有。參考Overwatch。」

| # | 決定 | 比過的選項 | 理由 |
|---|---|---|---|
| O1 | 敵方玩家不顯示血條（受傷的資訊靠命中標記、受擊動作、音效）；怪物顯示血條，參考 Overwatch；距離、遮擋、淡出照 Overwatch 的實際行為核對（第 2.4 節） | 修訂前的 (a) `core_pvp` 開、(b) 另建模式開、(c) 只開名字；side chat 另提的「瞄準或命中時才顯示」（Apex、The Division 一派） | 寫實射擊多半不給敵方玩家血條；有距離上限時會「看得到人、看不到狀態」；怪物（尤其 Boss）顯示血條是 PvE 的常見做法 |
| O2 | (N1) 不受影響：要顯示玩家名字時用和房間同一個字串 | (N2) 真的顯示名稱 | 第 3.2 節 |

這次改寫跟著動的：模式配置那一段（第 3.4 節，不再改 `core_pvp`）、名字的建議（O3）、驗證對象（O4）、測試與突變、分批（拿掉 A/B）。

**還沒開始**：核准只針對這份宣告；實作等 #367 合併、主會話通知。
