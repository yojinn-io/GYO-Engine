# 頭頂的名字與血條：事前宣告（`v8-arch-gyo2-overhead-ui`）

任務卡，2026-11-27。這一階段只寫事前宣告，交使用者核准；核准前不寫實作程式。#367（模式配置，`v8-arch-gyo2-mode-config`）改的是同一個遊戲的檔案，**它合併之前不動程式**，實作等主會話通知。作者 Opus 5.5／high。

行號以 `gyo2` 85df84da 為準；標「#367」的，是 #367 分支（`claude/gyo2-mode-config-declaration`）上的檔案。

---

## 0. 前提與結論先講

### 0.1 已定的前提（這份宣告不推翻）

| 前提 | 出處 |
|---|---|
| 路線 (A)：新功能加在 Object_PvP，由模式配置決定這一場掛哪些系統 | HANDOFF `v8-arch-gyo2-object-pvp-route-a` |
| 模式配置：模式檔的 `sets` 列出開哪些玩法組（gameplay set），各組自己讀它那一塊設定，進 `rulesDigest`，兩端比對 | #367 `mode-config-declaration.zh-Hant.md` 第 2.4、3.2、3.3 節 |
| 最終產品是有怪物的撤離射擊；Object_PvP 驗證核心 PvP 交互與地圖 | HANDOFF `v8-arch-gyo2-tarkov-is-reference` |
| Engine 只給機制；Product 的詞不進 Engine；錯誤用 `Result` 只告知；不對誤用加守衛；ECS 相關以 Bevy 為主參考 | AGENTS §1、批次表〈每批的設計審查〉第 5 項 |
| 使用者原則：「行為要統一、語意要一致」 | 卡片 brief（主會話轉交） |

### 0.2 結論（一段話）

**Engine 不改。** 做在 Client 的 Frame World，畫在**螢幕空間**，用的是 HUD 已經在用的那一套：HUD camera（2D、1280×720）上的 `Sprite`（文字圖）與 `Shape2d`（血條）。每幀：(1) 每一種「會有頭頂 UI 的東西」由它自己的來源 System 寫一個遊戲層的 component `Overhead`（錨點、名字、血量比例）；(2) 一個共用的 `ShowOverheads` 用 `Camera::WorldToViewport`（世界 camera）換成 viewport 座標，再用 `Camera::ViewportToWorld2d`（HUD camera）換成 HUD 座標，擺上去。隱藏的條件：自己、死了、在鏡頭後面、在畫面外、超過距離、被牆擋住（**和聲音的遮蔽同一個判斷** `PW::Blocked`）。顯示什麼（名字、血條、距離）由**模式**決定（玩法組 `overhead` 的設定），長什麼樣子由 `ui/overhead.json` 決定。之後的怪物只要加一個寫 `Overhead` 的來源 System。

---

## 1. 現況（照程式核對）

| 項目 | 程式 | 對這張卡的意思 |
|---|---|---|
| `WorldToViewport` | `engine/modules/camera/include/engine/camera/Camera.hpp:176`（Bevy `world_to_viewport`）；鏡頭後面是 `PastFarPlane`、比 near 近是 `PastNearPlane`，失敗不配置記憶體（`:161`） | 直接用；失敗就隱藏 |
| `ViewportToWorld2d` | `Camera.hpp:183`（Bevy `viewport_to_world_2d`） | viewport 座標 → HUD camera 的 2D 座標，不自己寫換算 |
| HUD | HUD camera `kHudCamera`：`ScalingFixed{1280, 720}`、order 2、只看 `kHudLayer`（`games/object_fps_pvp/src/client/ClientWorld.cpp:1908-1914`）；文字用 `TextPicture` 畫成圖、文字不變就不重畫（`:310-320`、`:374`）；`ShowHud`（`:953`） | 頭頂 UI 用同一個 camera、同一個文字圖快取的做法 |
| gyo.ui 文件 | 元件的位置只來自文件（`UiRectTransform`，`engine/modules/ui/include/engine/ui/UiDocument.hpp`）；binding 只能改文字、顏色、slider 的值、清單的項目 | **gyo.ui 不能每幀把元件擺到任意位置**，所以頭頂 UI 不走 gyo.ui（第 2.3 節） |
| 血量 | `PlayerCombat::hp`（milli-points，已複製給每個 Client，`world/Components.hpp:52`）；最大值 `CombatRules::maximumHp`（`sim/CombatRules.hpp:21`）；顯示用 `DisplayPoints`（無條件進位，活著的玩家不會顯示 0，`:35`） | 血條比例用 `DisplayPoints`，和自己的 HUD「HP N」同一個數（語意一致） |
| 名字 | **沒有顯示名稱**：identity 服務只有 device → userId（`services/identity/identity.go`）；`Owner` 只有 `userId`（`Components.hpp:100`）；房間畫面寫 `"Player " + userId`（`src/client/RoomScene.cpp:70`） | 第 3.2 節、決策點 O2 |
| 頭的位置 | 角色的判定形狀（`hitbox.json`）有 `HitPart::Head`（`src/character/CharacterRig.cpp:219`）；`PlaceHitShapes` 從姿勢算出世界位置（`:329`）；`PoseCharacters` 每幀算出其他玩家的姿勢，但只放在函式裡（`ClientWorld.cpp:758`） | 錨點＝頭的判定形狀的中心，往上加一段偏移（第 3.3 節） |
| 遮蔽 | `PW::Blocked(walls, a, b)`：兩點間有沒有牆（`world/PvpWorld.hpp:76`）；聲音已經用它判斷其他玩家的聲音被牆擋住（`ClientWorld.cpp:1076-1082`） | 頭頂 UI 用同一個判斷 |
| 誰會出現 | 開局前 Match 用 `PeerVisibility` 不讓其他玩家出現（`PvpWorld.cpp:747`）；其他玩家的身體由 `AddCharacters`／`PoseCharacters` 畫 | 頭頂 UI 跟著「身體有沒有畫出來」，不另訂一套 |
| 模式 → Frame World | #367：Frame World 的 `Prepare` 拿到這一場的模式（`ModeOf`），`GameRulesOf(game, mode, arena)`；Kernel 加了 `ResourceExists<R>`（Bevy `resource_exists`，#367 `engine/ecs/kernel/include/engine/kernel/Conditions.hpp`） | Frame World 依模式放不放設定的 Resource，System 掛 `ResourceExists` |

---

## 2. 成熟方案

查證等級：**源碼**（本機 `~/Code/GitHome`）、**官方文件**、**未確認**（憑記憶）。

### 2.1 Bevy（主參考，源碼 `bevy` 6a911653f）

| 什麼 | 位置 |
|---|---|
| `world_to_viewport`：NDC z < 0 是 `PastFarPlane`、> 1 是 `PastNearPlane`，Y 翻成向下 | `crates/bevy_camera/src/camera.rs:543-590` |
| `viewport_to_world_2d`；範例把 `world_to_viewport` 與 `viewport_to_world_2d` 來回換、互相驗證 | `camera.rs:713`；`examples/2d/2d_viewport_to_world.rs:29-38` |
| 引擎自己的用法：camera controller 用 `world_to_viewport` 算錨點在螢幕上的大小，讓指示圖示保持固定的像素大小 | `crates/bevy_camera_controller/src/pan_orbit_camera/extensions/anchor_indicator.rs:61-71` |
| UI 的排版在 PostUpdate、排在 `CameraUpdateSystems` 之後（螢幕空間 UI 要用這一幀的 camera） | `crates/bevy_ui/src/lib.rs:151-163` |
| 依距離隱藏：`VisibilityRange`（start／end margin） | `crates/bevy_camera/src/visibility/range.rs:80-96` |

Bevy 本身**沒有**名牌或血條的元件；社群的做法是「一個 System 每幀用 `world_to_viewport` 算位置，寫 UI `Node` 的絕對位置」（螢幕空間），或用一張朝向 camera 的四邊形（世界空間）。這一段是**未確認**（社群 crate 不在本機）。引擎給的機制就是上面那幾個函式。

### 2.2 Godot（對照，源碼 `godot` 94fc09c）

| 什麼 | 位置 |
|---|---|
| 螢幕空間：`Camera3D::unproject_position`（世界 → 螢幕）、`is_position_behind`（在 near 平面後面） | `scene/3d/camera_3d.cpp:548`、`:462-466` |
| 世界空間：`Label3D` 的 `billboard_mode`、`FLAG_FIXED_SIZE`（不隨距離變小）、`FLAG_DISABLE_DEPTH_TEST`（不被牆擋） | `scene/3d/label_3d.h:45-46,139`；固定大小在材質的 shader 裡做（`scene/resources/material.cpp:1341`） |

Godot 兩種都給：要螢幕空間就用 `unproject_position` 擺 2D 的 Control；要世界空間就用 `Label3D`，而且為了讓它「像螢幕空間」，另外給了固定大小與關掉深度測試兩個旗標。

### 2.3 兩種做法的比較

| | 螢幕空間（HUD 的 2D 層） | 世界空間（billboard） |
|---|---|---|
| 大小 | 固定像素，遠近一樣清楚 | 隨距離變小；要固定大小，得每幀縮放或在 shader 做（Godot 的 `FLAG_FIXED_SIZE`） |
| 文字 | HUD 的文字圖，清楚 | 文字貼在 3D 四邊形上，斜看、遠看會糊 |
| 被擋住 | 要自己判斷（這裡用 `PW::Blocked`） | 深度測試自動擋（也會被其他玩家的身體擋） |
| GYO 現有的機制 | **都有**：`WorldToViewport`、`ViewportToWorld2d`、HUD camera、`Sprite`、`Shape2d` | 沒有 billboard 的 drawable；要用 `Mesh3d` 做四邊形、每幀轉向，文字圖當貼圖，混色與深度要照 `Drawables.hpp` 的選項 |
| FPS 的慣例 | 名牌、血條多半是這種（**未確認**：Unreal 的 `UWidgetComponent` 有 Screen／World 兩種 space，FPS 常用 Screen；憑記憶） | 常見於第三人稱、MMO 的頭頂名字 |

**判定：螢幕空間。** 理由：(1) Engine 已經有全部的機制，世界空間要做 billboard，會變成 Engine 的工作；(2) FPS 的名牌要清楚、固定大小，世界空間要再加兩個旗標才做得到（Godot 的做法）；(3) 擋住的判斷用 `PW::Blocked`，和聲音同一個（第 4 節）。

不用 gyo.ui：gyo.ui 的位置是文件裡寫死的（第 1 節）。Bevy 的 UI 是 ECS 的 `Node`，每幀改位置是本來的用法；GYO 的 gyo.ui 是文件，要支援就要改資料契約（`gyo.ui` 版本 3）。**這次不做**：HUD 的 2D 層已經夠用，只有這一個使用者（AGENTS §1）；之後如果有第二個需要「跟著世界點走的 gyo.ui 元件」的功能，再當 Engine 的題目。

---

## 3. 資料：誰有頭頂 UI、顯示什麼

**前提：作者能處理的**（遊戲層的 component 與資料檔）。Engine 不知道「名字」「血量」。

### 3.1 標記：`Overhead` component（Frame World，遊戲層）

```
struct Overhead {          // games/object_fps_pvp，Frame World 才有
    Math::Vec3 anchor;     // 這一幀頭頂的世界座標（已加上偏移）
    std::string label;     // 空：不顯示名字
    std::optional<float> health;  // 0～1；none：不顯示血條
};
```

- **誰寫**：每一種東西自己的**來源 System**。這次只有一個：`TagPlayerOverheads`（其他玩家）。之後的怪物加 `TagMonsterOverheads`，從怪物的定義資料讀名字與錨點，`ShowOverheads` 不用改。這就是「資料怎麼標要顯示頭頂 UI」：**有 `Overhead` 就顯示**，和 Bevy「有 component 就被 query 到」同一個做法。
- **不是決策資料**、不複製、`HashValue` 不寫東西（和 `ClientHud` 一樣）。放在 Frame World 的玩家副本實體上：玩家離開、實體被移除，`Overhead` 跟著不見。
- **為什麼不讓 `ShowOverheads` 直接讀 `PlayerCombat`**：那樣它就只認得玩家；怪物的血量是另一個 component（G6 之後還可能有部位血量）。來源 System 把各自的資料翻成同一個形狀，顯示的規則只有一套（行為統一）。

### 3.2 名字從哪來

現在沒有顯示名稱（第 1 節）。選項（**決策點 O2**）：

| | 做法 | 代價 |
|---|---|---|
| (N1) | **和房間畫面同一個字串**：把 `RoomScene.cpp:70` 的 `"Player " + userId` 抽成一個函式 `PlayerLabel(userId)`，房間與頭頂都呼叫它；來源是 `Owner::userId`（已複製） | 名字是數字；有真名之後只改這一個函式的來源 |
| (N2) | 真的顯示名稱：identity 服務存名字、入場時帶給 Match、複製一個 `PlayerName` component | 動服務、wire、存檔；是另一張卡的量 |

**推薦 (N1)**：同一個人在房間與在場上叫同一個名字（語意一致）；(N2) 等「玩家要能取名字」成為功能時另開卡，屆時只換 `PlayerLabel` 的來源。

### 3.3 錨點：頭的判定形狀

- 錨點＝角色判定形狀裡 `Head` 那一個的中心（`PlaceHitShapes`），往上加 `ui/overhead.json` 的 `headClearance`（公尺）。
- 為什麼用判定形狀：它跟著動畫（蹲下、跳、倒地）走，和「打到頭」的頭是同一個點（語意一致）；不另訂一條「頭有多高」的規則。
- 代價：走路時頭會上下晃，名牌跟著晃幾公分；近距離看得出來。如果實機覺得晃，改成「身體 capsule 的頂端」（`feet + BodyHeight(crouched)`）只改來源 System。
- 實作：`PoseCharacters` 把這一幀算好的 model-space 姿勢留在 `ClientCharacters`（現在只留 `locomotion`），`TagPlayerOverheads` 排在它後面讀，不重算姿勢。
- 怪物：沒有 `Head` 判定形狀的，由怪物的定義資料給錨點（那是怪物那張卡的事）。

### 3.4 顯示什麼：模式決定；長什麼樣：表現檔決定

| 放哪裡 | 內容 | 理由 |
|---|---|---|
| **模式檔**：玩法組 `overhead` 的設定，例 `"sets": {"overhead": {"label": true, "health": true, "range": 60}}` | 顯示名字嗎、顯示血條嗎、最遠幾公尺 | 看得到敵人的名字與血量是**資訊**，影響玩法（撤離射擊多半不給，核心 PvP 可以給）；放模式裡就進 `rulesDigest`，同一場的每個 Client 一樣 |
| **`ui/overhead.json`**（新檔，照 `ui/hit_feedback.json`） | `headClearance`、文字大小、血條寬高、顏色、名字與血條的間距 | 外觀，不影響玩法，和 `hit_feedback.json` 同類 |

- 玩法組 `overhead` 在 Sim／Match 的登記是空的（它沒有決策資料、沒有 System）；它只用模式配置的「設定區塊＋digest」那一半。Frame World 的 `Prepare` 從 `game->modes[mode].sets` 找這一組：開著就放 `OverheadRules` Resource，沒開就不放。`TagPlayerOverheads` 與 `ShowOverheads` 掛 `ResourceExists<OverheadRules>`（Bevy `resource_exists`）。
- 模式沒開這一組：一個頭頂 UI 都沒有，HUD 的實體也不生成。
- 之後有隊伍時（協力），可以把設定分成 `allies`／`enemies` 兩塊（例：隊友隔牆也看得到）。**這次不做**，只寫在這裡當延伸的位置。

---

## 4. 每幀做的事與隱藏的條件

**前提：作者能處理的**（遊戲層的 System）。

### 4.1 System 與排程

| System | 何時 | 做什麼 |
|---|---|---|
| `TagPlayerOverheads` | Main／Update，在 `PoseCharacters` 之後 | 每個**身體有畫出來**的其他玩家：寫 `Overhead`（錨點、`PlayerLabel(userId)`、`DisplayPoints(hp) / DisplayPoints(maximumHp)`）；死了的：拿掉 `Overhead` |
| `ShowOverheads` | Main／PostUpdate，在 camera 的 `kUpdateSet` 與 transform 的 `kPropagateSet` 之後、visibility 的 `kCheckSet` 之前 | 每個有 `Overhead` 的實體：判斷隱藏（4.2）→ 換座標 → 擺它的名牌實體（一個 `Sprite`＋一個 `Shape2d`，在 `kHudLayer`）；目標不見了就移除名牌實體 |

- 排在 PostUpdate 的理由：要用這一幀的 camera（`PlaceCamera` 在 Update 改 camera 的 `Transform`，`GlobalTransform` 與 `ComputedCamera` 在 PostUpdate 才更新）。Bevy 的 UI 排版也在 PostUpdate、`CameraUpdateSystems` 之後（2.1）。
- 名牌實體沒有 parent，`ShowOverheads` 同時寫它的 `Transform` 與 `GlobalTransform`（和 `Place()` 同一個做法，`ClientWorld.cpp` 的 Helpers），不等下一幀的 propagate。**這一幀就畫在新位置**要用測試確認（第 5 節 C1）。
- 名牌實體：`OverheadPart{target}`（照 `CharacterPart`）；文字圖照 HUD 的快取，名字不變就不重畫。

### 4.2 隱藏的條件（全部在一個純函式裡，好測）

`present::PlaceOverhead(...)` 回傳「HUD 座標」或「隱藏的原因」：

| # | 條件 | 判斷 | 原因 |
|---|---|---|---|
| 1 | 自己 | 來源 System 不給自己寫 `Overhead` | — |
| 2 | 死了、身體沒畫出來 | 同上（跟著 `PoseCharacters` 畫不畫） | — |
| 3 | 在鏡頭後面、比 near 近 | `WorldToViewport` 失敗（`PastFarPlane`／`PastNearPlane`） | `Behind` |
| 4 | 在畫面外 | 換出來的 viewport 座標不在 viewport 裡 | `OffScreen` |
| 5 | 太遠 | camera 到錨點的距離 > 模式的 `range` | `TooFar` |
| 6 | 被牆擋住 | `PW::Blocked(walls, camera 位置, 錨點)`：牆來自 Sim World 的地圖（和聲音用的同一份 `walls`） | `Occluded` |

- 判斷的順序是由便宜到貴：先 3～5，最後才打射線。玩家只有幾個，射線的成本可以不管；之後怪物多時，距離先篩掉大部分，**沒量過**，到那時再看。
- 只看牆、不看其他玩家的身體：和聲音的遮蔽一樣。
- 不夾到畫面邊緣（那是目標指示器的做法，不是名牌）。
- 隱藏是立刻的，不做淡出。牆角來回時如果會閃，再加（實機看）。
- `WorldToViewport` 的失敗是「這一次沒換」，不是錯誤；`ShowOverheads` 不回傳錯誤（E2：這一個名牌，不是 World）。

### 4.3 座標

`WorldToViewport(世界 camera, 錨點)` → viewport 的邏輯像素（左上原點、Y 向下）→ `ViewportToWorld2d(HUD camera, 那個點)` → HUD 的 2D 座標（中心原點、Y 向上）。兩個 camera 在同一個 target、都沒有 viewport，所以同一個像素。不自己寫「÷寬×1280」的換算：HUD camera 換了 scaling 也不會錯。

HUD camera 是 `ScalingFixed{1280, 720}`，視窗不是 16:9 時 HUD 會被拉長；名牌和現有的 HUD 文字一樣被拉長（現況，不在這張卡改）。

---

## 5. 外部可驗的測試

| # | 測試 | 判定 | 在哪 |
|---|---|---|---|
| P1 | `PlaceOverhead` 的位置：正前方 d 公尺的點 → HUD 的 (0, 0)；往右偏一個已知角度 → 由 FOV 手算的 x；往上加 `headClearance` → y 變大。另外和「`WorldToViewport`→`ViewportToWorld2d`」互相驗證（照 Bevy 範例的來回換） | 誤差 < 0.01 HUD 單位 | `tests/object_fps_pvp/OverheadTests.cpp`（logic，cpu） |
| P2 | 隱藏：鏡頭後面 → `Behind`；畫面外 → `OffScreen`；超過 `range` → `TooFar`，剛好在 `range` 內 → 顯示；一面牆在中間 → `Occluded`，牆移開 → 顯示；錨點貼著牆的這一面 → 顯示（`Blocked` 的「只碰到遠端」規則） | 原因逐一相符 | 同上 |
| P3 | 血條比例：`DisplayPoints(hp)/DisplayPoints(max)`；hp＝1 milli-point → 1/100（不是 0）；`PlayerLabel` 和房間畫面的字串同一個 | 相等 | 同上 |
| P4 | 設定：`ui/overhead.json` 缺欄位、型別錯、負數 → 讀取失敗並說哪一欄；模式的 `overhead` 區塊同樣；區塊不同 → `rulesDigest` 不同 | 錯誤訊息相符 | 同上 |
| C1 | Client 整合（`ClientTests.cpp` 的 `Setup`，Match＋Client 記憶體連線）：對手在視野裡 → 有一組名牌實體在 `kHudLayer`，位置＝對手頭的錨點換算的值；**同一幀**轉動視角，名牌的 `GlobalTransform` 跟著變 | 相等 | `object_fps_pvp.client.*`（cpu） |
| C2 | Match 改對手的 `hp` → 幾幀內血條的寬度照比例變；對手死了 → 名牌消失；對手離開 → 名牌實體被移除 | 相符 | 同上 |
| C3 | 視角轉 180° → 名牌隱藏；對手走到牆後 → 隱藏；走出來 → 顯示 | 相符 | 同上 |
| C4 | 模式沒開 `overhead` → 跑很多幀都沒有任何名牌實體；自己的玩家永遠沒有 | 相符 | 同上 |
| A/B | #367 的核心 PvP 腳本：改前、改後權威 digest 逐位元組相同（這張卡不動 Sim／Match；`core_pvp` 開這一組時只有 `rulesDigest` 變） | 相同 | macOS 本機 |

- 實機看一次（名牌晃不晃、牆角閃不閃、字的大小）：交使用者 playtest。CI 沒有 GPU 的畫面比對。
- 本機不做 GPU 的量測。

### 5.1 突變（D184：新的，find 在改動範圍裡）

預定的方向（id `pvp-overhead-*`，實際在各步定）：

- 鏡頭後面不隱藏（`WorldToViewport` 的失敗被當成 (0,0)）。
- 畫面外不隱藏。
- 距離的比較用 `>=` 改成永遠通過、或拿掉。
- 遮蔽的判斷拿掉。
- 自己也寫 `Overhead`。
- 死了不拿掉 `Overhead`。
- 血條比例用 milli-points（hp＝1 時變 0）。
- Y 方向翻錯（直接用 viewport 的 y）。
- `headClearance` 沒加上去。
- 目標離開後名牌實體沒移除。
- 模式沒開也顯示（條件拿掉）。

---

## 6. 分批

一條線、一個 PR（宣告這個 draft PR 之後接著放實作）。#367 合併、主會話通知後才開始。

```
v8-arch-gyo2-overhead-ui（一個 PR，開向 gyo2）
├─ 第 1 步｜Opus/high｜上段｜present/Overhead（純函式：位置、隱藏、比例）、ui/overhead.json、玩法組 overhead 的設定區塊、PlayerLabel；P1～P4
├─ 第 2 步｜Opus/high｜下段｜Frame World：Overhead component、TagPlayerOverheads、ShowOverheads、名牌實體與文字快取、core_pvp 的設定（照 O1）；C1～C4
└─ 第 3 步｜Opus/medium｜下段｜突變、A/B、設計審查、純淨 Code Review、dev_log
```

| 步 | 內容 | 上段／下段（根據） | 實作者 | 審查者 | 檢驗者 |
|---|---|---|---|---|---|
| 1 | 第 3.2、3.4、4.2、4.3 節：`games/object_fps_pvp/include/object_fps_pvp/present/Overhead.hpp`＋`src/present/Overhead.cpp`；`assets/object_fps_pvp/ui/overhead.json`；`GameSets()` 加 `overhead`（Sim／Match 的登記是空的，只有 `readMode`）；`PlayerLabel` 抽出、`RoomScene` 改用它 | **上段**：新的內容檔與模式檔的區塊（資料形式），進 `rulesDigest` | Opus／high | Opus／high（第 3 步一起） | Opus／high |
| 2 | 第 3.1、3.3、4.1 節：`ClientWorld.cpp` 的 System 與 Resource；`PoseCharacters` 留姿勢；`core_pvp.json` 依 O1 | 下段：Client 的表現，不動 wire、權威、協議 | Opus／high（要先懂 Frame World 與 HUD） | Opus／high | Opus／high |
| 3 | 第 5.1 節的突變（新的＋find 在改動範圍的）；A/B（macOS）；設計審查（Opus／high，唯讀，報告放主 checkout `build/evidence/gyo2-overhead-ui/`）；純淨 Code Review（Fable／high，只給 git 範圍）；dev_log | 下段（執行與紀錄）；檢驗上段的成果，不用小模型 | Opus／medium | — | Opus／medium |

- 模型都不超過主會話（Opus）；Fable 只在純淨 Code Review（navigator 規則）。
- **時間**：第 1 步約半天，第 2 步約一天，第 3 步約半天。超過 1.5 倍就停下回報。
- **衝突**：`gyo2-interact-door`（門的互動）也可能改 `ClientWorld.cpp` 的 HUD（互動提示）。誰先合併，另一個照 merge-base 跟上；如果它也要「世界點 → HUD 座標」，共用第 4.3 節的那一個函式，不各寫一份。

---

## 7. 要使用者決定的點

| # | 問題 | 選項 | 推薦 |
|---|---|---|---|
| **O1** | 核心 PvP 模式開不開頭頂 UI | (a) **`core_pvp` 開**（名字＋血條、`range` 60 m）：你一進遊戲就看得到；`core_pvp` 的 `rulesDigest` 會變，權威 digest 不變（A/B 證明）(b) `core_pvp` 不開，另建一個模式檔開它：核心 PvP 的資料完全不動，但要在房間選那個模式才看得到 (c) `core_pvp` 只開名字、不開血條 | **(a)**：這是你要看的功能；它只是表現，不動判定，回歸基準（權威 digest）照樣成立。之後的撤離模式不開它，就是模式配置該做的事 |
| **O2** | 名字從哪來 | (N1) 和房間同一個字串 `PlayerLabel(userId)`（「Player 12」）(N2) 真的顯示名稱（identity＋wire，另開卡） | **(N1)**（第 3.2 節） |

卡片自己判的（不是決策點，列出來讓你知道；要改就說）：

- 螢幕空間，不做世界空間的 billboard；不走 gyo.ui（第 2.3 節）。Engine 不改。
- 標記＝遊戲層的 `Overhead` component，由各種東西自己的來源 System 寫；顯示只有一套（第 3.1 節）。
- 錨點＝頭的判定形狀中心＋偏移（第 3.3 節）。
- 顯示什麼放模式（玩法組 `overhead`），外觀放 `ui/overhead.json`（第 3.4 節）。
- 隱藏：自己、死了、鏡頭後面、畫面外、太遠、被牆擋（和聲音同一個 `PW::Blocked`）；立刻隱藏、不淡出、不夾邊（第 4.2 節）。
- 血條比例用 `DisplayPoints`，和自己的 HUD 數字一致（第 1 節）。

## 8. 沒查到、未確認的

- Bevy 社群的名牌做法（螢幕空間的 `Node`、或世界空間的四邊形）：社群 crate 不在本機，憑記憶。
- Unreal `UWidgetComponent` 的 Screen／World space 與 FPS 的慣例：憑記憶。
- PostUpdate 裡同時寫 `Transform` 與 `GlobalTransform`，這一幀就畫在新位置、不被 visibility 的剔除擋掉：從排程推，第 2 步用 C1 確認。
- 很多怪物時每幀的射線成本：沒量，等怪物那張卡。

**還沒開始**：核准只針對這份宣告；實作等 #367 合併、主會話通知。
