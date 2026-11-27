# 事前宣告：遊戲流程框架（`v8-product-pvp-game-flow`）

更新：2026-11-26（修訂 7：啟動畫面（`v8-product-pvp-playtest-fixes`）；修訂 6：使用者的兩個決定（`v8-arch-gyo2-decisions-2026-11-26-night`）：3.3 單人房間和多人同一個房間畫面、3.6 解析度改實體像素；修訂 5：新增 3.7「畫面跳轉的兩層」（主會話轉達使用者的關切）；3.6 照第 5 段實作與審查改寫；修訂 4：第 4 段的設計審查之後，3.5 的就緒與開始照實作改寫；修訂 3：第 3 段的設計審查之後，3.4 的 session 與離開房間、列表與選房照實作改寫；修訂 2：第 2 段的設計審查之後，3.1 的 Solo 單位、3.3 的生玩家與暫停照實作改寫；修訂 1：第 1 段實作與設計審查之後，3.2 的 `run` 型別、3.4 的連線目標的給法、第 1 段的槽數照實作改寫，見文末「修訂紀錄」）。Owner：PvP 的遊戲流程框架任務卡（GYO2 主會話開）。**狀態：寫完就動工**（新 AGENTS：只有推翻使用者的決定或動 AGENTS §1 才先問）。

根據（使用者的決定，HANDOFF 全文）：
- `v8-arch-gyo2-game-flow-and-gateway`：主菜單（單人、多人、設置、退出）；單人與多人的流程。
- `v8-arch-gyo2-net-connect-target-user`：World＝場景（主菜單、大廳、房間、遊戲）；單人的遊戲 World 不帶 `NetConnectTarget`，多人的在「開始」拿到 connect 時才帶。
- `v8-arch-gyo2-direct-connect-and-kick-old`：遊戲 UDP 直連 Match，Gateway 只走 HTTP；資安先不做。
- `v8-arch-gyo2-loading-screen-and-relevance`：loading 畫面（含進度條）蓋到就緒；遊戲開始前 Match 不送其他玩家的位置（net 的相關性）。
- `v8-arch-gyo2-settings-resolution-fullscreen`、`v8-arch-gyo2-window-mode-vsync-start`：解析度固定四種、窗口／無邊框全屏、VSync；存使用者資料目錄。
- `v8-arch-gyo2-disconnect-rules`：模式規則，本卡只要不衝突。
- 交接說明 [`gateway-client-handoff.zh-Hant.md`](gateway-client-handoff.zh-Hant.md)（外部協議 v1）；R1b 宣告 `docs/architecture/target/v2/sdl-extract-replace-declaration.zh-Hant.md` 第 8 節（PvP 怎麼用它）。

讀的版本：`origin/gyo2` ac0c8958。

---

## 0. 前提表

| # | 前提 | 誰定的 | 掛在上面的決定 |
|---|---|---|---|
| P1 | World＝場景；換場景＝同一個 Product 換前台 World，渲染 World 留著、視窗不關 | 使用者（net-connect-target-user、R1b 選 (B)） | 3.1、3.2 |
| P2 | `ReplaceWorld` 照**同一份描述**重新登記（O-8）；描述的 `units`、`options` 不變，只有產品的 `build`、準備函式與 `extra` 能隨場景不同 | 使用者（O-8） | 3.1 的四個 World 槽、3.3 的 Local World 另一個槽 |
| P3 | 一個 Product 只有一個帶 net 角色的 World（net D-3）；角色是描述的選項 | 使用者（net 宣告） | 單人另一個沒有 net 的槽（3.3） |
| P4 | Sim 在自己的執行緒（#338，`ownThreadWorlds`）；借出的 World 不能替換（O-13：`WorldLent`） | 使用者（O-13）、#338 | 3.2 的 runner 掛鉤要先收回再換 |
| P5 | 單人不連任何伺服器 | 使用者（net-connect-target-user） | 3.3：單人不是「本機 Match」 |
| P6 | 多人「開始」後第一幀就連 Match，邊連邊載入 | 使用者回「沒錯」（game-flow-and-gateway 的卡的對應） | 3.5：Match 等「全員載完」靠輸入裡的就緒位元，不靠延後連線 |
| P7 | Client 只讀存檔，Match 寫（多人）；單人存在本機 | Gateway 宣告 D9、交接說明第 4 節 | 3.3、3.4 |

---

## 1. 範圍

**做**：主菜單；單人房間（商人、任務只留位置）→ 遊戲（隨機出生點）→ 撤離 → 結算存檔（本機）→ 回主菜單；多人：Gateway 的 session、大廳（房間列表、建房：模式與地圖、加入）、房間（成員、準備、房主開始）→ 遊戲 → 撤離（離開 Match，Match 寫存檔）→ 回大廳；loading 畫面與就緒；Match 開始前的相關性；設置（解析度、窗口／無邊框全屏、VSync、存檔與啟動時套用）。

**不做**（只留位置與資料流）：商人、任務、合作／生存的規則與結算（撤離後的「結算」只做到存檔現有的 `matches`、`deaths`）、快速配對、推送（照交接說明用輪詢）。

---

## 2. 現況（`origin/gyo2` ac0c8958）

- `games/object_fps_pvp/src/client_main.cpp:181-234`：大廳是自己的 Product（`RunLobby`），送出位址後關窗，`RunClient`（`:237-321`）另建 Client 的 Product；斷線後再建大廳。**視窗關掉再開**（R1b 宣告第 8 節說的問題）。
- `ClientSimWorld`／`ClientFrameWorld`／`ClientRenderWorld`（`src/client/ClientWorld.cpp:1453-1484`）：Sim 帶 PvP 的 client World＋net；Frame 是看見、聽見、操作的那一層（`sdl_window`）；Render 從 Frame 抽。Frame 沒有 `world_ui`（HUD 是文字轉圖的 sprite）。
- `sdl_window` 的 runner（`plugins/sdl_window/src/SDLWindowPlugin.cpp:326-370`）：一開始把 `ownThreadWorlds` 借出，之後只有 pump＋`RunFrame`，**沒有在兩幀之間替換 World 的地方**；`LentWorlds`（`:222-323`）在匿名命名空間。產品要自己寫 runner（`installRunner = false`）就得重寫借出的執行緒與 `UntilNextRound` 的等待。
- `Product::ReplaceWorld`（`engine/orchestration/include/engine/orchestration/Product.hpp:181`）：照描述重新登記；R1b 讓有讀者的來源也能換（讀者留著、改讀新的）。
- `PvpWorld`（`src/world/PvpWorld.cpp:857-947`）：兩種端（Match＝server、Client），`NetEntry`、`OnServer`／`OnClient` 條件；沒有不帶 net 的端。
- net：`PeerVisibility`（`engine/modules/net/include/engine/net/Replication.hpp:454-485`）已有「只看某些實體」；client→server 只有輸入（`NetEntry::Input`），沒有 client 訊息。
- `AssetView::Ready(handles)`（`engine/modules/world_asset/include/engine/world_asset/AssetView.hpp:129`）：一組 Handle **連同依賴**的 `loaded`、`failed`、`ids`——進度可以照它算，不用補 Engine。
- `SDLGPUStatus`（`plugins/sdl_gpu/include/plugins/sdl_gpu/SDLGPUPlugin.hpp:38-70`）：`FramesShown`、`SkippedDraws` 等，在 Product 的 buffer。
- UI：`gyo.ui` v2 有 `panel`、`image`、`text`、`button`、`horizontal_slider`、`fixed_step_list`（列表，`itemTemplate`）、`text_field`，一份文件多個 canvas（`engine/modules/ui/include/engine/ui/UiDocument.hpp:136-207`）。
- Match 不知道房間有誰：matches 服務只給 `--capacity`（`services/matches/local.go:57-66`）。

---

## 3. 設計

### 3.1 Client 的 Product：四個 World 槽、場景表

照描述順序：

| World | 單位（描述，不隨場景變） | 依場景的 `build` |
|---|---|---|
| `Solo` | `entity_ids world_time rollback authority transform physics_3d`（**沒有 net**；rollback 在但沒有策略：`authority` 依賴它），借給自己的執行緒 | 單人遊戲：PvP 的 Local 端（3.3）＋交換；其他場景：空 |
| `Sim` | 現在的（PvP client World＋net＋rollback），借給自己的執行緒 | 多人遊戲：現在的內容＋`NetConnectTarget`；其他場景：空（沒有目標＝不連，#332 回到沒有目標時關 socket） |
| `Frame` | 現在的＋`world_ui`；`sdl_window` | 每個場景各自的 UI 與 System（下表） |
| `Render` | 現在的＋`ui_render`；`sdl_gpu`；`extractFrom = "Frame"` | 不換（R1b：留著、改讀新的 Frame） |

場景（`client::Scene`）：`Splash`（啟動畫面，修訂 7）、`Menu`（含設置的 canvas）、`SoloRoom`、`SoloGame`、`Lobby`、`Room`、`MultiGame`。
- 為什麼單人另一個槽（P2、P3）：net 的角色、rollback 的策略是描述的選項；單人不能帶 net 角色當 client（沒有權威），也不能在同一個槽裡拿掉它。Unity 的寫法正是兩種 World：選單與單人 `CreateLocalWorld`（`com.unity.netcode/Runtime/ClientServerWorld/ClientServerBootstrap.cs:98`），多人才 `CreateClientWorld`（`:339`）。空場景的 World 只剩單位本身，每步沒有東西跑。
- Frame 每個場景都登記 Render 要抽的型別（R1b 宣告第 8 節第 3 點）：單位登記的不變；產品自己的型別（遊戲的 HUD、角色等）各場景都登記，只有 System 與實體隨場景。
- 「現在要哪一個場景」放在 Product 一份的共用處（O-15）：`ConfigureClient` 建的 `SceneDirector`（現在的場景與它的參數），各 World 的 `build` 讀它。Frame 的 System 要換場景時寫 Frame World 的 Resource `SceneRequest`；runner 掛鉤（3.2）在兩幀之間讀它、更新 `SceneDirector`、替換 World。

### 3.2 換場景的時機：`sdl_window` runner 的「兩幀之間」掛鉤（Engine 改動）

**前提（Engine 要解決還是作者能處理）**：替換只能在兩次入口之間、World 都在 Product 手上時做（O-8、O-13）。借出的執行緒歸 runner 管，作者在 `sdl_window` 的 runner 裡沒有地方插；自己寫 runner 得重寫借出的執行緒與等待（`LentWorlds`、`UntilNextRound`），兩份會分岔。所以這是只有 runner 做得到的機制，加在 `sdl_window`。

別家：Godot 的 `change_scene_to_node` 只記下待換的場景，主迴圈在一幀處理完之後 `_flush_scene_change`（`godot/scene/main/scene_tree.cpp:726-727`、`:1691-1704`）——換場景在主迴圈的兩幀之間。Bevy 是同一個 World 的 State 轉換（`bevy_state`，`DespawnOnExit`，`crates/bevy_state/src/state_scoped.rs:145`），沒有換 World；GYO 的 World＝場景（P1）對應 Godot 的形。

介面（`plugins/sdl_window/include/plugins/sdl_window/SDLWindowPlugin.hpp`）：

```cpp
struct SDLWindowBetweenFrames final {
    // After each frame, on the main thread, with every World in the Product's hands except the lent ones:
    // whether the product wants to replace Worlds now (reads its own state; called every frame).
    std::function<bool(Engine::Orchestration::Product&)> wanted;
    // Then, with the lent Worlds returned: the product's replacements. An Err ends the runner (Failure).
    // The Worlds of ownThreadWorlds are lent again after it.
    std::function<Engine::Base::Result<void, Engine::Orchestration::OrchestrationError>(Engine::Orchestration::Product&)> run;
};
// SDLWindowOptions::betweenFrames (none: no replacement, as today).
```

- runner：`RunFrame` 之後、等待之前，`wanted` 為真就 `ReturnAll`（收回、join），有 World 壞了照現在的出口結束；呼叫 `run`；再 `Lend(ownThreadWorlds)`。live frame（OS 的 modal 迴圈）裡不換。
- 收回借出的 World 要等它跑完這一輪（最多一步）；換場景不是每幀的事，代價可接受。
- 測試（`sdl_window.events`，dummy video driver）：(a) `wanted` 為假時不呼叫 `run`；(b) 為真時 `run` 裡 `ReplaceWorld` 一個借出的 World 成功（沒收回會回 `WorldLent`）、之後它又在別的執行緒跑；(c) `run` 回 Err → runner Failure 帶那段文字。突變：不收回就呼叫 `run`；`run` 之後不再借出；忽略 `run` 的 Err。

### 3.3 單人：PvP 的 Local 端

- `world::WorldOptions` 加端的種類：`Match`、`Client`、`Local`。`Local` 的描述沒有 net 與 rollback；權威策略照舊（它自己就是權威）。
- `BuildWorld`：`Local` 不經 `NetEntry`、不登記 `SeatPlayers`／`ControlOwnPlayer`／`FollowMatchArena`；「Match only」的 Fixed System（`JudgeShots`、`RespawnPlayers`）在 Local 不加 `OnServer` 條件（本來就是權威）。新的 `SpawnLocalPlayer`（準備函式）：本機玩家在**隨機**出生點（`std::random_device` 選 index；單人的亂數不是判定資料，不需要決定性）；`LocalInput`（`FixedUpdate/First`）：交換來的操作寫進玩家的 `PlayerInput`。
- 交換：`Solo` 用和 `Sim` 一樣的 `ClientExchange`（畫面的那一半不分單人多人）。
- 撤離：遊戲中 Esc 開暫停的 canvas（繼續、撤離）。撤離＝`SceneRequest{Menu}`＋這一局的結算（離開過的局數＋1、這局的死亡數）寫本機存檔 `<使用者資料目錄>/<profile>/solo-save.json`，格式照 `match/PlayerSave.hpp`（同一份存檔格式，同一個 parse／serialize）。單人房間顯示讀到的存檔（「讀檔」）。
- 單人房間：~~商人、任務只放兩塊「準備中」的面板；按「開始」進 `SoloGame`，地圖用 content 的第一個（之後由任務決定）。~~（修訂 6）和多人的房間同一個畫面（`pvp_room.json`，canvas `solo_room`：同樣的面板、成員、商人與任務的「準備中」、模式與地圖兩列），差別只在按鈕：多人是「準備」「開始」「退出」，單人是「開始」「マップ」「戻る」。~~模式固定「生存」（只顯示）~~（修訂 8）模式用已安裝模式清單（`modes/modes.json`）的第一個（只顯示它的名字）；地圖在已安裝的 arena 之間切換，「開始」→ `SceneChange{SoloGame, map, mode}`（大廳建房時選的地圖同樣經 `SceneChange::map` 進遊戲，3.4）；loading 的「中止」回單人房間時帶回那張圖。

### 3.4 多人：Client 接 Gateway

- `client::GatewayClient`（純函式＋小狀態，邏輯庫）：每個請求的 `HttpRequest`（路徑、Bearer、`Idempotency-Key`、JSON body）與回應的解析（`Room`、`RoomSummary`、`Connect`、`Session`、錯誤碼），照交接說明第 2 節。冪等鍵：每個操作一把（亂數 hex），重試同一把。
- 呼叫：Frame World 的 Resource 持有進行中的 `Http::HttpCall`，System 每幀 `Poll`（交接說明第 3 節）。逾時由 Client 決定：5 秒丟掉、顯示錯誤（Gateway 的錯誤碼原樣顯示成玩家的話，不替使用者做處理）。
- session：第一次用的時候 `POST /v1/session`；token 與到期放 `SceneDirector`（跨場景）；401 的三種碼重拿一次。裝置 id：`<使用者資料目錄>/<profile>/device-id`，第一次用高熵亂數（32 bytes hex）產生（交接說明 L7）；`--profile NAME` 讓同一台 Mac 開兩個 Client。
- 大廳（`Lobby`）：每秒 `GET /v1/rooms`；列表（`fixed_step_list`，最多 8 列，點一列＝加入）；建房（~~模式 coop／survival 切換~~（修訂 8）模式在 content 已安裝的模式之間切換、顯示模式檔的名字，地圖在 content 的 arena 之間切換）；顯示戰績（`GET /v1/saves/me`：局數、死亡）；回主菜單。
- 房間（`Room`）：每 0.75 秒 `GET /v1/rooms/{id}`；成員與準備；準備切換；房主的「開始」；離開（`POST leave`，回大廳）。`state == "started"` 且有自己的 `connect` → `SceneRequest{MultiGame, connect}`（同一份 GET 的 connect，2 分鐘內）。409／503 的錯誤碼顯示在狀態列。
- 遊戲（`MultiGame`）：`SceneChange::connect` 放 `NetConnectTarget{server, userId, DecodeHex(token)}`，`Sim` 的 `build` 讀 `SceneDirector` 的它、在準備函式放入。斷線（Refused、逾時、`ClosedByRemote`）→ 回大廳並顯示理由，**不自動重連**（交接說明第 4 節）。撤離＝`NetService::Close`＋Flush（現在 `RunClient` 關窗時做的）→ 回大廳，大廳過幾秒再讀一次存檔（Match 收到離開才寫）。
- `--connect A.B.C.D:P` 留著（測試與手動）：直接進 `MultiGame`，沒有 token（沒有金鑰的 Match 收）。直接輸入位址的舊大廳（#330，`LobbyWorld`、`lobby/AddressText`）由 Gateway 大廳取代、刪掉（第 3 段）；第 1 段先把它當成一個場景接上，保持每段都能玩。
- `--gateway A.B.C.D:P`（預設 `127.0.0.1:7700`）。

### 3.5 loading 與就緒；Match 的開始與相關性

**就緒**（遊戲場景的 Frame）：四項全成立才拿掉 loading 的 canvas：
1. 資產：遊戲場景要的 Handle（地圖模型、角色、viewmodel、武器、貼圖、聲音、字型）的 `AssetView::Ready(...)` `complete`（失敗照現在顯示錯誤、回上一個場景）。進度＝`(loaded + failed) / ids.size()`（連同依賴）；位元組級不做（資產都是本機檔，量小；沒有使用者點名）。
2. 預熱：資產齊了之後，畫面在 loading 底下照常畫遊戲場景（貼圖、網格上傳、pipeline 第一次建），`SDLGPUStatus::FramesShown` 前進兩幀且這兩幀 `SkippedDraws` 沒增加。讀 `SDLGPUStatus` 的是 runner 掛鉤的 `wanted`（主執行緒、兩幀之間，O-12 的「runner 直接讀寫」），寫成 Frame World 的 Resource `DrawProgress`；沒有 `sdl_gpu`（測試）時視為已預熱。
3. 網路初始狀態：自己的玩家到了（`ClientView::ownPlayer != 0`）、`MatchInfo` 的地圖對得上（單人：本機玩家生出來）。
4. 第一幀：1～3 成立之後再畫完一幀。

就緒之後 Frame 在操作裡帶「就緒」，loading 換成「等待其他玩家」直到 Match 開始，然後拿掉。進度條用 `horizontal_slider`（綁 binding、沒有 action：只顯示）。

**Match 的開始**：
- `MatchInfo` 加 `phase`（`Waiting`、`Playing`），照常複製。`Waiting` 時 `MovePlayers`、`UseWeapons` 不吃輸入（兩端一樣：Client 的預測照同一個 phase）；`JudgeShots`、`RespawnPlayers` 不動。
- `PlayerInput` 加 `ready`（Client 就緒後一直帶 1）。Match 在 `Waiting`：等待的使用者（`--expect-users 7,9`）**都有玩家且最新的輸入 `ready`**，或從第一個連進來起超過 `--start-deadline-seconds`（預設 60；有人當掉不擋住其他人）→ `Playing`。沒有 `--expect-users`（手動起的 Match、`--connect`、測試）：一開始就 `Playing`（和現在一樣）。單人（Local）：等的是本機玩家。
- 相關性：`Waiting` 時 Match 給每個 Peer `PeerVisibility{everything = false, entities = {它控制的玩家}}`；`Playing` 時拿掉（看見全部）。用 net 現有的 C-VIS1，不改 net。
- 掉線規則（`v8-arch-gyo2-disconnect-rules`）：不衝突——Match 開始後新玩家本來進不來（房間鎖了、token 只給成員）；掉線的玩家重連時 phase 已是 `Playing`，直接看見全部。
- Go：rooms 在開始時把成員的 userId 交給 matches（`Request.Users`），`local.go` 加 `--expect-users`。

### 3.6 設置

- `Menu` 場景的 `settings` canvas：解析度（四種，左右切換）、視窗模式（窗口／無邊框全屏）、VSync（開／關）、套用、返回。
- 解析度是**實體像素**（修訂 6，使用者：「改」）：`window` 的 `ResizeWindow` 與 `WindowSettings::size` 是邏輯像素，所以送 `WindowSizeFor(解析度, WindowState::scaleFactor)`（四捨五入）。程式先照設定的數字開視窗；視窗第一次說出 scale factor（`WindowState::known`）、或 scale 變了（換到別的倍率的螢幕）時，`FitWindow` 再送一次。scale 1 的螢幕不多送。不動 Engine。
- 套用：解析度送 `window::ResizeWindow`；比例照 Engine 現有的（UI 的 fit、相機的 `Letterbox`），本卡不改。視窗模式與 VSync 照「視窗模式＋VSync」卡的介面；那張卡合併之前，UI 與存檔先做，套用那兩項只記在存檔、不送（PR 寫明）。
- 存檔：`<使用者資料目錄>/settings.json`（`{"version":1,"resolution":[w,h],"windowMode":"windowed"|"borderless","vsync":true}`，JSON Module）；啟動時讀回、用在 `WindowSettings`；讀不到或不認得 → 預設（1280×720、窗口、VSync 開）並在 log 說一次。設定不分 profile（同一台機器同一份）。

### 3.7 畫面跳轉的兩層

使用者問：「跳轉有解法了麼。之前 master 頁面跳轉都是一堆的 if else」。跳轉分兩層，各自只做一件事；沒有「每一幀先問現在是哪個畫面、再決定做什麼」的總開關。

| 層 | 用什麼 | 負責什麼 | 誰觸發 |
|---|---|---|---|
| 場景之間 | 換前台 World（`ReplaceWorld`，R1b；`client/Scenes.hpp` 的 `SceneRequest` → 兩幀之間 `ChangeScene`） | 每個場景是自己的 World，只登記自己的 System 與實體；`Solo`／`Sim` 只在要玩的場景帶遊戲（3.1） | 場景的 System 寫 `SceneRequest` |
| 同一個場景的頁面 | Engine 的 `state_machine` Module（Bevy 的 States；`client/Pages.hpp`） | 頁面是一個 enum；每個值有 enter／exit schedule：進頁面放它的 UI 根、離開拿掉；頁面的 System 用 `InState<頁面>` 決定跑不跑 | `StateMachineRequest<頁面>::Request(值)` |

| 跳轉 | 層 |
|---|---|
| 啟動畫面 → 主菜單；主菜單 → 單人房間、大廳；單人房間 → 遊戲、主菜單；大廳 ↔ 房間；房間 → 遊戲；遊戲 → 主菜單（單人撤退、中止）、大廳（多人撤退、中止、連線結束）；大廳 → 主菜單 | 場景（換 World） |
| 主菜單 ↔ 設定（`MenuPage::Main`／`Settings`） | 頁面 |
| 遊戲的 loading → 遊戲 ↔ 暫停（`GamePage::Loading`／`Playing`／`Paused`） | 頁面 |

- UI 的按鈕（`world_ui` 的 `UiAction`）直接對到「寫一個 `SceneRequest`」或「送一個頁面的 `Request`」：在它所在的頁面的 System 裡（`InState` 擋住別頁），中間不經總開關。不是按鈕的轉換（loading 就緒且開始、指標被拿走或還給玩家）也是那一頁的 System 送一個 `Request`。
- 一個頁面的請求在下一幀的套用點生效（state_machine 的 apply stage，`Main/PreUpdate` 之後，同 Bevy 的 `StateTransition`）；換場景在兩幀之間。
- 和 master 比：master 的 `apps/object_fps_pvp/src/Pvp/PvpApplication.cpp`（1453 行）把大廳、連線、遊戲放在同一個物件，一個 `ConnectionPhase` 欄位在每幀的程式裡逐處判斷（`:208`、`:301-303`、`:315`、`:354`、`:880`），按鈕在一個 `switch (entry->action)` 裡再用 `if (inLobby)` 擋（`:316-325`）。這裡每個場景與頁面只登記自己的 System，不在就不跑。
- 同一個頁面內的按鈕仍依 action 名分派（`UiAction` 帶的是字串 id，U 的設計：ui 頁第 3 節「動作」）；那是「這個按鈕做什麼」，不是跳轉。

---

## 4. 分段

每段一個 draft PR，後一個疊在前一個上，base `gyo2`；標題 `feat(object_fps_pvp): 說明（v8-product-pvp-game-flow）`。

| 段 | 範圍 | 測試 | 突變（只跑相關的） |
|---|---|---|---|
| 1 主菜單與場景切換 | `sdl_window` 的 `betweenFrames`（3.2）；Client 改成一個 Product（第 1 段三個槽，`Solo` 在第 2 段加）、`SceneDirector`、`SceneRequest`；主菜單（單人、多人、設置、退出；單人與設置先顯示「準備中」）；多人＝舊的位址大廳當場景 → 遊戲 → 斷線回大廳；`--connect` 照舊 | sdl_window 的三案；client 測試：菜單→大廳→遊戲→斷線回大廳，Render 沒重建（`ExtractSourceSwaps` 加一、同一個 Render World）、Sim 換了才連；`main`／`main_connect`（gpu） | `betweenFrames` 三個；換場景不換 Sim；菜單場景 Sim 帶目標 |
| 2 單人 | Local 端（3.3）、`Solo` 槽、單人房間、隨機出生點、暫停與撤離、本機存檔 | world 測試：Local 生玩家、能動、能死與重生；client 測試：主菜單→單人→遊戲→撤離→存檔寫了、再進讀得到、全程沒有 socket（`NetService::UdpAddress` 空） | Local 帶 net；不寫存檔；出生點固定 |
| 3 多人接 Gateway | `GatewayClient`、session 與裝置 id、大廳、房間、開始 → `MultiGame`、撤離與回大廳、戰績；刪舊位址大廳 | 邏輯測試：請求的組成與回應的解析（每個錯誤碼）；client 測試：假 Gateway（`gyo_test_loopback_listener`）走建房→準備→開始→帶上 connect；network：真的 services＋Match，一個 headless Client 走完（`-L network`） | 不帶 Bearer；不帶冪等鍵；`ClosedByRemote` 自動重連 |
| 4 loading 與就緒 | 就緒四項、進度條、等待其他玩家；`MatchInfo.phase`、`PlayerInput.ready`、Match 的開始與 `PeerVisibility`；Go 的 `--expect-users` | world 測試：Waiting 時另一個 Client 收不到別人的玩家、不能動；全員 ready → Playing、看得到；期限；client 測試：資產沒齊時 loading 在、進度遞增 | 不設 visibility；ready 不看；Waiting 時能動 |
| 5 設置 | settings canvas、`settings.json` 讀寫、啟動套用、解析度 `ResizeWindow`；視窗模式與 VSync 照那張卡（合併後接上） | 邏輯測試：讀寫往返、壞檔回預設；client 測試：套用送出 `ResizeWindow` | 不存；啟動不讀 |

每段：設計審查（Opus／high，審查者只拿成果物與這份宣告的原始要求）、本機 `ctest -L "cpu|gpu"`（網路與 Go 另跑 `-L "go|network"`）、手動 dispatch L1、回報主會話 PR／head／run／審查結論。

## 5. 協調

- PvP 卡（遠端內插、後座力）：改遊戲 World 的 System。本卡第 1 段只動描述與 `build` 的入口，第 2、4 段動 `BuildWorld`、`PlayerInput`、`MatchInfo`：已 SendMessage 說明；後合的 merge 跟上。
- 視窗模式＋VSync 卡：本卡第 1 段只動 `SDLWindowOptions` 與 runner（`RunWindow`、`LentWorlds`），不碰 backend；第 5 段用它的介面。
- 改名卡（#341）：先合的話 merge 跟上（`DatagramAddress` → `OS::SocketAddress`）。

## 6. 估時（推定）

第 1 段 10～14 h、第 2 段 8～12 h、第 3 段 10～14 h、第 4 段 10～14 h、第 5 段 5～8 h；合計 43～62 h。

## 7. 卡片自己判的（可以推翻）

- 單人用另一個 World 槽（不是本機 Match）：P3、P5；Unity 同形。
- 換場景的掛鉤放 `sdl_window` 的 runner（不是產品自寫 runner、不是編排層新入口）：3.2 的前提。
- Match 等「全員載完」用輸入的就緒位元（不是延後連線、不是新的 client 訊息）：P6；net 沒有 client 訊息，加它是 Engine 的新機制，沒有必要。
- 開始前凍結移動：沒有它「開始」沒有意義（先載完的人可以先跑位）。這是 PvP 這款遊戲的規則，放在遊戲裡、不在 Engine。
- 進度不做位元組級；設定不分 profile；單人地圖用第一個。

## 修訂紀錄

- 修訂 1（第 1 段實作、設計審查 S4 之後）：
  - 3.2：`run` 回 `Result<void, OrchestrationError>`，不是 `std::string`。`Base::Result` 的錯誤要是帶碼的錯誤（`CodedError`），而產品在 `run` 裡做的就是 `ReplaceWorld`，它的錯誤（含壞掉時的 `WorldFailure`）原樣帶出最直接；產品自己的錯誤用 `PrepareFailed` 加 `reporterCode`。
  - 3.4：連線目標不經 `ReplaceRequest::extra`，改由 `Sim` 的 `build` 讀 `SceneDirector` 現在場景的 `connect`、放進準備函式。換場景時的參數都在 `SceneDirector` 一處，`ChangeScene` 不必知道每個場景要什麼。
  - 第 4 節第 1 段：先做三個槽；`Solo` 隨第 2 段加。
- 修訂 2（第 2 段實作、設計審查之後）：
  - 3.1：`Solo` 的單位加 `rollback`（沒有策略，什麼都不記錄）：`authority` 依賴它（O-E3）。
  - 3.3：本機玩家由 `SpawnLocalPlayer`（`Main/Update`，沒有玩家時生一次）生，不在準備函式：生實體要經 `entity_ids` 的分配器（`K::Spawn`），準備函式沒有那條路。死亡數由 Solo World 照 Match 的同一條規則（`CountDeath`：沒看過的 respawn tick）數（`LocalDeaths`），Frame 撤退時只拿它的數字結算——結算是權威端的事（審查 S1）。
  - 3.3：暫停（指標還給玩家）時單人的 Solo World 停下（world_time 的 `TimeControl::paused`，經交換帶過去）；多人不停（Match 不等人），暫停畫面的標題寫「メニュー（対戦は続いています）」。暫停畫面有「続ける」「撤退する」（審查 S2）。
  - 第 4 節第 2 段的突變「Local 帶 net」換成「Local 把權威端的 System 掛上 `net.OnServer`」（`pvp-local-authority-conditioned`）：沒有 net 角色的 World 帶 net 單位什麼也不登記（G-1），那個突變守不到行為；守的是「權威端的 System 在 Local 也跑」。
- 修訂 3（第 3 段實作、設計審查之後）：
  - 3.4：`GatewayClient` 在 client 函式庫（它用 `GYO::Http` 與 Kernel 的 Resource；邏輯庫不連這兩個），請求的組成與回應的解析是純函式、測試直接叫。
  - 3.4：session 不放 `SceneDirector`：`GatewayClient` 本身是 Frame World 的 Resource，`CarryOver` 帶過每個場景（進行中的呼叫也帶著），第一個場景放入（裝置 id 讀或第一次產生；拿不到時每個呼叫立刻以 `no_device` 結束並說明）。
  - 3.4：只有玩家自己撤退時離開房間（撤退在遊戲場景 `Ask` leave，呼叫隨 `GatewayClient` 帶到大廳完成）；連線自己結束（被拒、逾時、`ClosedByRemote`）時不離開：玩家還是房間的成員，Match 還在時房間會給新的 connect（掉線規則「位子保留」；審查 B1）。
  - 3.4：列表最多 6 列（文件的 `max_items`），選房是 ▲▼＋「参加する」（`fixed_step_list` 的樣板不能放按鈕）；選中的房間以 id 記，列表更新後找回位置。房間收到 404、410、`not_member` 回大廳。
- 修訂 4（第 4 段實作、設計審查之後）：
  - 3.5 第 1 點：資產失敗**不**擋住遊戲、不回上一個場景：失敗的那一個由它自己的地方顯示（沒有那個模型、沒有那個聲音），整局不能玩比少一樣東西更糟；loading 的「中止」讓玩家自己回去。就緒每一幀重算到 `ready` 為止：Client 先畫自己的第一張地圖、跟著 Match 換地圖時，新模型的載入讓這一步重來（審查 B1）。進度＝頂層 Handle 中完成的比例（失敗也算），只增不減。
  - 3.5 第 2 點：預熱的「畫了幾幀」叫 `DrawnFrames`（Frame World 的 Resource），由程式經 Frame World 的交換函式（進）從 `SDLGPUStatus` 寫，不經 runner 掛鉤的 `wanted`；兩幀沒有略過的 draw，或資產完成後最多 120 幀（略過一直有時不卡住）。overlay 的文件出來之前，相機什麼都不看、世界相機清成黑色（審查 S6）。
  - 3.5「Match 的開始」：`MatchInfo` 多帶 `startTick`，開始前不動以「tick < startTick」判（Client 重算開始前後的步時答案和 Match 一樣；審查 S7）。等待的使用者去重。
  - 3.5：loading 期間點擊與 Tab 不拿指標（給 loading 畫面的「中止」）；「中止」：多人照撤退（離開房間），單人回單人房間、不結算。
  - 3.5：進度條用 `horizontal_slider`：`gyo.ui` 的 slider 一定有 action、在 focus 順序裡（UI 的驗證），所以它可以被聚焦與拖動，發出的 `pvp.progress` 沒有人讀。只顯示的元素是 Engine（U）的事，有人點名時再加。
  - 就緒用每一步的輸入帶（`PlayerInput::ready`）：net 沒有 client→server 的訊息（只有 `ServerMessage`），Unity 是用 RPC（`NetworkStreamInGame`）。要不要補 client 訊息是 net 的 Engine 題目，記成後續。
- 修訂 5（第 5 段實作、設計審查之後；3.7 是主會話轉達使用者的關切）：
  - 3.7 新增。設定頁、遊戲的 loading／暫停改用 `state_machine`（`MenuPage`、`GamePage`）：原本設定是換 canvas、loading 與暫停是每幀照條件挑 canvas（`ShowPause`），改成頁面的 enter／exit 放與拿 UI 根、頁面的 System 用 `InState`。
  - 3.6：解像度按一下換下一種（繞回），不是左右切換：只有四種，一顆鍵夠；要左右時加一顆「上一種」即可。
  - 3.6：視窗模式是 window 的 `WindowMode`（設定直接用它，檔案仍寫 `"windowed"`／`"borderless"`）；適用時送 `SetWindowMode` 與 `ResizeWindow`（每次都送，視窗不一定在那個大小）；進設定頁時，畫面一列照視窗實際的 `WindowState::mode`（玩家用 OS 的按鈕切過時）；無邊框時解像度一列標「（ウィンドウ時）」（全屏取螢幕的大小，選的是回到視窗時的大小）。VSync：Render World 的 Extract 把設定抄進 render 的 `Presentation`（`CopyPresentation`）。
  - 3.4：房間的地圖帶進遊戲：`SceneChange::map`（房間的 `map`，arena 的 id）；Sim、Solo 的 `BuildWorld` 與 Frame 的 `GameRules` 照它在 `GameContent` 找 arena 的 index（`WorldOptions::arena`）；沒有那張圖或沒帶（單人房間、`--connect`）用 Client 自己的（`ClientOptions::arena`），Match 開始後照舊跟隨 Match 的（`FollowMatchArena`）。大地圖卡要的接點。
  - ~~3.6：解像度是視窗的**邏輯像素**（`WindowSettings::size`、`ResizeWindow` 的單位）。Windows 的顯示縮放 125% 時 1920×1080 的邏輯視窗比 1080p 螢幕大。要不要改成實體像素（照 `WindowState::scaleFactor` 換算）是使用者的決定（他定的是「送現有的 `ResizeWindow`」），回報主會話問。~~ → 修訂 6 改成實體像素。
- 修訂 6（使用者的兩個決定，`v8-arch-gyo2-decisions-2026-11-26-night`）：
  - 3.6：解析度改實體像素，產品照 `WindowState::scaleFactor` 換算（`WindowSizeFor`、`FitWindow`），不動 Engine。macOS Retina（scale 2）上 1920×1080 的視窗佔畫面的一半大小，這是實體像素的意思。
  - 3.3：單人房間改成多人的房間畫面（同一份文件、同樣的面板），按鈕不同（單人「開始」、多人「準備」）；模式固定「生存」；地圖在房間裡選，經 `SceneChange::map` 進遊戲。舊的 `pvp_solo_room.json` 刪掉。多人房間的標題只留「部屋 #id」，模式與地圖移到兩列（兩個 canvas 同一個排法）。
  - 3.3／3.7：暫停（使用者實機試玩：「剛進入遊戲時立刻跳出暫停」）。原因：`GamePage::Playing` 的 `PauseOnRelease` 判的是「指標不是遊戲的」，而遊戲開始時指標本來就還不是遊戲的（第一下點擊才拿），所以 loading 一結束就進暫停。改成判「這一幀要暫停」：指標被交還（Tab、Esc、視窗失去焦點）或按了 Esc；遊戲開始時照舊顯示遊戲，第一下點擊拿指標（不是射擊）。
  - 3.4：「選地圖沒用」（同一次試玩）。使用者：「我看到的都是原始的測試地圖」：試玩的整合版只有 `pvp_training_v1`、`pvp_corners_v1`、`biome_trial`，250 m 的 Extraction 在 #354、還沒合併。選圖的每一層（房間的 map → matches 的 `--arena` → Match 玩那張，services.log 第二場 `pvp_corners_v1` → `SceneChange::map` → Client 的 arena）都接上了，不改程式。另外屬實但不是這次原因的一件：`pvp_training_v1` 與 `pvp_corners_v1` 的幾何是同一份（只差出生點），兩張之間切換畫面不變；要不要改地圖資料由主會話問使用者。`--verbose` 的 log 加「選的地圖」與「遊戲玩的 arena」兩行，給下次試玩當依據。
  - 3.5：`#342`（大地圖）之後 arena 的畫面是每個 region 一個 Mesh3d、模型由 Client 持有，資產那一步不再等 arena 的模型（`ShowArena` 同一幀就換好）；`#348` 的 `ModelRef` 不用另外處理。
- 修訂 7（`v8-product-pvp-playtest-fixes`，LAN 實機場次的回饋 6「啟動時沒有標識」；使用者的決定「先用文字」）：
  - 3.1：新增場景 `Splash`：程式不帶 `--connect` 時的第一個場景（`ClientStart::first` 的預設仍是 `Menu`，測試照舊從主菜單開始）。Frame World 只放 `ui/pvp_splash.json` 的 `splash` canvas（黑底、文字「GYO」與「ENGINE」）；文件排好之後 `ClientStart::splashSeconds`（預設 2 秒）到了、或按任何鍵與滑鼠鍵，就 `SceneRequest{Menu}`。秒數從**排好**算起，載入慢不會把標識吃掉。換成圖檔只改那份文件（`text` 換 `image`），不改程式。
  - 3.7：跳轉照「場景之間」那一層（換 World），沒有另一套導航；啟動畫面只出現在程式的第一個場景，回主菜單（撤退、離開大廳）不會再看到。
- 修訂 8（模式配置 `v8-arch-gyo2-mode-config` 的 M1，使用者的決定「全部照推薦，空殼刪掉」，HANDOFF `v8-arch-gyo2-mode-config-m1`）：
  - 3.3／3.4：房間的「模式」改成模式配置的模式（`assets/object_fps_pvp/modes/`，[模式配置宣告](../../../docs/architecture/target/v2/mode-config-declaration.zh-Hant.md)第 3 節）。`coop`／`survival` 兩個佔位拿掉，等它們有自己的模式檔再出現；現在只有「核心 PvP」（`core_pvp`）。大廳建房在已安裝的模式之間切換（顯示模式檔的 `name`），單人用清單的第一個；房間服務的模式清單照地圖的做法讀 Match 的 content（`--content`，或 `--modes`），matches 起 Match 時帶 `--mode`；房間開始時經 `SceneChange::mode` 進遊戲。
