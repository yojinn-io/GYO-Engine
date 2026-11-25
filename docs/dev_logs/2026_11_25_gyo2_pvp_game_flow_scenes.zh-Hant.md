# 2026-11-25 遊戲流程框架第 1 段：主菜單與場景切換（`v8-product-pvp-game-flow`）

Owner：產品（Object_FPS_PVP）＋`sdl_window` 的 runner。GYO2 主會話開的任務卡（遊戲流程框架）。作者 Opus 5.5／high。
根據：[事前宣告](../../games/object_fps_pvp/docs/game-flow-declaration.zh-Hant.md)第 3.1、3.2 節與第 4 節第 1 段。

## 做了什麼

1. **`sdl_window` 的兩幀之間掛鉤**（`SDLWindowOptions::betweenFrames`，`SDLWindowBetweenFrames{wanted, run}`）：runner 每幀之後問 `wanted`；為真時收回 `ownThreadWorlds`（各自跑完這一輪）、呼叫 `run`（產品在裡面 `ReplaceWorld`）、再借出。`run` 的 Err（`OrchestrationError`）照 `FailedExit` 結束 runner。live frame 裡不換。
2. **Client 一個 Product**（`include/object_fps_pvp/client/Scenes.hpp`、`src/client/Scenes.cpp`）：`Sim`（只在 `MultiGame` 登記遊戲與 `NetConnectTarget`；其他場景只剩單位，`StepSchedule` 照 RB-E10 宣告）、`Frame`（每個場景自己的 `build`；多了 `world_ui`）、`Render`（多了 `ui_render`，不換）。場景：`Menu`、`Lobby`、`MultiGame`。`SceneDirector`（Product 一份）記現在的場景；場景的 System 寫 `SceneRequest`；`ChangeScene` 在兩幀之間換（Sim 的角色變時換 Sim，再換 Frame；`ClientExchange::Reset` 讓舊場景的畫面與操作不流到新的）。`FrameNotes`（`CarryOver`）帶過場景：`--frames` 的預算、文件顯示了沒、內容不符、上次連線結束的理由。
3. **主菜單**（`assets/object_fps_pvp/ui/pvp_menu.json`，canvas `menu`）：シングルプレイ（第 2 段前顯示「準備中です」）、マルチプレイ（→ 大廳）、設定（第 5 段前「準備中です」）、終了（關窗）。
4. **大廳**改成場景（`LobbyWorld.cpp` 的 `BuildLobby`）：送出位址 → `MultiGame`（連那個位址）；「戻る」→ 主菜單。遊戲場景的連線結束或內容不符 → 帶理由回大廳（`LeaveOnEnd`，取代 `CloseOnEnd` 的關窗）。
5. **程式**（`src/client_main.cpp`）：一個 Product 跑到視窗關掉；`--connect` 從 `MultiGame` 開始；`--frames` 照舊（文件場景要顯示過文件；內容不符回 1）。

## 驗證

- `sdl_window.events` 30/30（新 3 案：`wanted` 為假不呼叫 `run`；為真時借出的 World 收回、換掉、再借出且不在主執行緒；`run` 的錯誤結束 runner）。
- `object_fps_pvp.client` 12/12（大廳三案改成場景：大廳 → 有效位址進 `MultiGame`、Sim 有目標；主菜單 → 大廳 → 主菜單，Render World 同一個、`ExtractSourceSwaps` 加一、主菜單的 Sim 沒有遊戲也沒有連線；終了關窗；Match 拒絕 → 回大廳、視窗不關、Render 不換、大廳顯示理由）。
- 本機（Intel Mac）`ctest -L "cpu|gpu"`：第一次 255/256（`naming.guard`：標頭裡前向宣告了別的命名空間的 `Product`，N1），改成 include 之後相關 9 項全過。
- 突變：`sw-between-*` 4/4、`pvp-lobby-*`／`pvp-scene-*`／`pvp-menu-*`／`pvp-session-end-ignored` 10/10 killed。

## 沒驗

- 真的視窗裡換場景（視窗不關、畫面不閃）：交使用者試玩。
