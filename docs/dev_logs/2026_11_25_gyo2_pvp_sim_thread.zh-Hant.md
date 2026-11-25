# 2026-11-25 PvP Client 拆成 Sim／Frame／Render（`v8-product-pvp-port-sim-thread`，第 4 段遊戲部分）

Owner：產品（`games/object_fps_pvp`）。PvP 移植實作卡（GYO2 主會話開的任務卡）做。作者 Opus 5.5／high。
根據：[第 4 段的宣告](../architecture/target/v2/live-frame-sim-thread-declaration.zh-Hant.md)第 4.3 節「遊戲」與第 5 節 J5、J6、J7；Engine 部分（`Product::Lend`、`ownThreadWorlds`）在 #331。主會話 2026-11-25：「請現在開始做第 4 段的遊戲部分…疊在 #332 的分支 a33951a9 上…R5 的實機驗收（拖視窗時模擬照跑、不被判斷線）也寫進測試和回報。」

## 做了什麼

1. **三個 World**（`ClientWorld.hpp`、`.cpp`）：
   - `Sim`＝`world::ClientWorld`（PvP World、`net` 的 K3 client、`rollback`、`authority`、`physics_3d`、`world_time`）改名，加 `SimControls`、`ClientSessionEnd` 與三個 System：`PrepareOwnInput`（自己的 Peer 的 `PeerInput`，原本在 `ReadControls` 裡）、`WriteStepInput`（讀 `SimControls`：按住的鍵與視角照最後一次的，按下只給這一步）、`NoteSessionEnd`（只記理由，不碰視窗）。
   - `Frame`：`entity_ids`（`kFrameIds` 2'000'001～3'000'000，Match 與 Sim 的範圍之外）、`world_time`（`fixedStep = false`）、`transform`、`world_asset`、`camera`、`input`、`window`、`window_camera`、`world_render`（＋`sdl_window`）。登記 PvP 的五個元件（`Owner`、`PlayerBody`、`PlayerCombat`、`HitRecord`、`ShotRecord`，同名）與 `GameRules`、`LocalUser`、`ContentRef`，當 Sim 的玩家的複本；`ReadControls`、相機、HUD、角色、viewmodel、聲音照舊，加 `CloseOnEnd`（`ClientView::ended` 時關窗）。
   - `Render`：`extractFrom = "Frame"`。
2. **交換**（`ClientExchange`，`ConfigureClient` 在 Product 的 buffer 建一份，兩半各一把 `std::mutex`）：
   - `Sim` 的「出」：`SnapshotOf`——每個有 `Owner` 的實體（`Owner` 與其他四個元件，有就帶）、`GameRules` 的競技場 id 與 walls digest、`ClientView`（tick、步長、`latestConfirmed`、連線、`OwnPlayer`、`ContentMismatch`、結束與理由）。只留最新一份。
   - `Frame` 的「進」：`ApplySnapshot`——版本比上次新才套；元件不同才寫（`operator==`，讓 Frame 的 System 看得到「變了」）；Sim 沒有的玩家 `Despawn`；競技場換了就從 `ContentRef` 找來換（找不到不動，Sim 的 `ContentMismatch` 會說）；`ClientView` 整份換。
   - `Frame` 的「出」：`Controls::Take()` 交給 `AddControls`（按住的鍵、視角取最新，按下 OR 累計）。`Sim` 的「進」：`TakeControls` 取走（按下清掉）並 `Merge` 進 `SimControls`。
3. **Frame 沒有 rollback**：拿掉所有 `RB::KeepThroughRollback`（相機、HUD、競技場的畫、角色與 viewmodel 的部件）。
4. **讀 Sim 資料的 System 改讀 `ClientView`**：`PoseCharacters`、`PoseViewmodel`（tick、步長、`latestConfirmed`）、`ShowHud`（連線、內容不符）、`StatusOf`（Frame 的；`playing` 用 `ClientView::ownPlayer`：`world::OwnPlayer` 要看 `PlayerInput`，Frame 沒有）。
5. **`client_main`**：Worlds＝Sim（帶 `NetConnectTarget`）、Frame（`WatchProgram` 改讀 `ClientView`）、Render；`SDLWindowOptions::ownThreadWorlds = {"Sim"}`。跑完讀 Frame 的 `StatusOf`，`NetService` 的 `Close`＋`Flush` 用 Sim 的 tick（runner 在返回前已 `Return`）。
6. **測試**：`ClientTests` 的 Setup 建 Sim＋Frame＋Render（都在測試的執行緒；`Client()`＝Frame、新增 `Sim()`）；`PW::OwnPlayer` 改從 Sim 讀。新案 J5。

## 兩條執行緒都碰的東西（宣告 4.3 第 4 項）

| 物件 | 誰碰 |
|---|---|
| `ClientExchange` | Sim 的執行緒（取控制、放快照）與主執行緒（放控制、取快照）；兩半各一把鎖，不互等 |
| `NetService`（Product 的 buffer） | 交出期間只有 Sim 的執行緒（Frame、Render 沒有 `net`）；`Return` 之後主執行緒在 `client_main` 裡 `Close`＋`Flush` |
| `AssetStore`、混音器（`SoundPort`）、`SDLWindowPump`、`NativeWindows` | 只有主執行緒（Sim 沒有 `world_asset`、`sdl_window`，也不放聲音） |
| `GameContent`（`shared_ptr<const>`） | 兩邊都讀、沒人寫（Sim 的 `ContentRef` 與 Frame 的 `ContentRef`、`ClientOptions`） |
| 每個 World 的 `ModuleBuffer` | 照 CCR-5（各自的 World） |
| `Base::Log` | 兩邊都寫；契約是「任何執行緒、可同時」（`engine/core/base/include/engine/base/Log.hpp:39`，handler 是 `std::atomic`） |

## 行為的差別

- 在同一條執行緒上（測試、`ownThreadWorlds` 空）時，一幀的順序是 Sim（進、輪、出）→ Frame（進、輪、出）：這一幀讀的鍵在下一幀的 Sim 才用，輸入多一幀延遲；畫面拿到的是這一幀 Sim 的結果。交出之後兩邊各跑各的，順序不再有意義。
- 既有的 client 測試沒有因為這一幀改等待的幀數（J6：全部原樣通過，含一步四幀的 4／4 點擊）。

## 驗證

- 本機（Intel Mac）：`ctest -R object_fps_pvp` 6/6（`logic`、`world`、`client`、`main`、`main_connect`、`match`）。
- J5 新案 `client: J5 while the main thread stops for 250 ms…`：測試自己的「模擬執行緒」跑 Sim 的租約、Match 與 MemoryLink（MemoryLink 兩端要在同一條執行緒），主執行緒跑 Frame 的幀、中間睡 250 ms。本機一次的數字：停住期間 163 輪、tick 207→369（每輪一步）、Match 取用這個 Client 的輸入 162 次、缺 0、按住 W 的玩家在 Match 上走了。交還之後仍連著、Match 上仍有玩家。
- 突變：新的 `pvp-exchange-press-not-accumulated`（交換只留最新的控制、按下被蓋掉）、`pvp-step-press-repeats`（`WriteStepInput` 不清按下）、`pvp-frame-mirror-kept`（Sim 沒有的玩家 Frame 不刪）與 `ClientWorld.cpp` 相關的既有 6 個（`pvp-press-lost`、`-cursor-not-locked`、`-character-not-shown`、`-hit-direction-not-drawn`、`-hit-no-shake`、`-session-end-ignored`）共 9/9 killed。
- **沒驗**：
  - J7 實機（拖視窗角、拖標題列時畫面照跑、Match 不判斷線、自己的玩家在對手畫面上照常動）：交使用者。`client_main` 的 `ownThreadWorlds = {"Sim"}` 沒有自動測試會殺（`main_connect` 只跑幀數、不量拖動），由 J7 驗。
  - ThreadSanitizer：沒跑。
