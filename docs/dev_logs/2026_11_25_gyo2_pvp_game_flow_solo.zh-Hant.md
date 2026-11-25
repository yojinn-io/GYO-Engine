# 2026-11-25 遊戲流程框架第 2 段：單人（`v8-product-pvp-game-flow`）

Owner：產品（Object_FPS_PVP）。GYO2 主會話開的任務卡（遊戲流程框架）。作者 Opus 5.5／high。分支疊在第 1 段（#346）上。
根據：[事前宣告](../../games/object_fps_pvp/docs/game-flow-declaration.zh-Hant.md)第 3.3 節與第 4 節第 2 段。

## 做了什麼

1. **PvP 的 Local 端**（`world/PvpWorld.hpp` 的 `WorldEnd`、`LocalWorld`）：沒有 net，rollback Module 在但沒有策略（`authority` 依賴它）；World 自己是權威，「Match only」的 Fixed System（`AnimatePlayers`、`JudgeShots`、`RespawnPlayers`）不加 `net.OnServer`；不登記 `SeatPlayers`、`ControlOwnPlayer`、`FollowMatchArena`；新的 `SpawnLocalPlayer`（`Main/Update`，一次）在 `LocalSpawn` 指的出生點生本機玩家（帶 `PlayerInput`）。`MatchInfo` 照 Match 自己放。
2. **Client 的 `Solo` 槽**（`ClientSoloWorld`）：只在 `SoloGame` 登記遊戲；每一局用 `std::random_device` 選出生點。和 `Sim` 同一個 `ClientExchange`；`WriteLocalInput`（`FixedUpdate/First`）把交換來的操作直接寫進本機玩家的 `PlayerInput`（和 `WriteStepInput` 共用 `StepInputOf`）。`SnapshotOf` 讀 net 的 Resource 前先看 World 有沒有登記；沒有 net 的 World「連線」恆為真。借給自己的執行緒（`ownThreadWorlds = {"Solo", "Sim"}`）。
3. **單人房間**（`SoloRoom`，`ui/pvp_solo_room.json`）：商人、任務兩塊「準備中」；戰績（讀存檔）；出擊 → `SoloGame`；戻る → 主菜單。
4. **遊戲中的暫停與撤退**（`ui/pvp_game.json`，HUD 相機上的 `UiRoot`）：指標還給玩家（Tab、Esc）時顯示 `pause`（「撤退する」）；單人撤退把這一局結算進存檔（`match::Settled`：局數＋1、死亡＝這一局的生命數減一，死著再＋1）、回主菜單並顯示結果；多人撤退回大廳（大廳的 Sim World 沒有目標：連線關掉，Match 看到離開才寫存檔）。
5. **本機檔案**（`client/LocalFiles.hpp`）：使用者資料目錄的小文字檔，整檔讀寫（IO 的 `WriteFileAtomically`）。存檔 `<使用者資料目錄>/GYO/object_fps_pvp/<profile>/solo-save.json`，格式照 `match/PlayerSave.hpp`。`--profile NAME`（預設 `default`）。

## 驗證

- 本機（Intel Mac）`ctest -L "cpu|gpu"` 256/256。
- world：Local World 在給的出生點生玩家、沒有 net、照它寫的輸入移動；出生點的 index 取餘數。
- client：主菜單 → 單人房間（「記録はまだありません」）→ 出擊 → 自己的玩家到 Frame World、沒有 session → 點擊拿指標、W 前進 → Esc 出現暫停 → Enter 撤退 → 主菜單、存檔 `matches 1`、`deaths 0` → 再進房間「出撃 1 回・死亡 0 回」；每個場景的文件都顯示過（這一條先抓到 `pvp_game.json` 的 `play` canvas 少了 `focus_order`）。
- 突變：`pvp-local-spawn-fixed`、`pvp-local-input-ignored`、`pvp-solo-never-plays`、`pvp-solo-start-ignored`、`pvp-solo-not-saved`、`pvp-pause-not-shown`，加第 1 段的 `pvp-menu-multi-ignored`、`pvp-scene-sim-kept`：8/8 killed。

## 沒驗

- 死亡數不為 0 的結算（單人沒有敵人，測試走不到死亡；`DeathsOf` 只由程式審查看）。
- 真的視窗裡的單人一輪、暫停畫面：交使用者試玩。
