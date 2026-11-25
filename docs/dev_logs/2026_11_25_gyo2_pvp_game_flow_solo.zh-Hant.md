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

## 設計審查（Opus／high，作者同模型，審查者只拿成果物與原始要求）

blocker 1、should 4、nit 12。

- B1（宣告的「Local 能死與重生」「全程沒有 socket」沒做）→ world 新案：Local World 的玩家死了會重生、`lives` 2、死亡只數一次；client 單人案每幀查 `UdpAddress()` 是空的；突變「Local 帶 net」換成 `pvp-local-authority-conditioned`（宣告修訂 2 寫理由）。
- S1（結算在 Frame 用鏡像另算一套死亡數）→ `CountDeath`（Match 的存檔與 Local 共用）、Solo World 的 `CountLocalDeaths`／`LocalDeaths`，`ClientView::deaths` 帶給 Frame；`DeathsOf` 拿掉。
- S2（「一時停止」但單人照走）→ 單人暫停時 Solo World 的 `TimeControl::paused`；多人的標題改成「メニュー（対戦は続いています）」；加「続ける」（`pvp.resume`，下一幀當成點擊拿指標；拿著指標時不再當點擊，否則會開槍）。
- S3（死亡數與多局累加沒守住）→ 單人案走兩局：第一局死一次，第二局後 `matches 2`、`deaths 1`；突變 `pvp-solo-game-number-fixed`、`pvp-local-deaths-not-counted`、`pvp-pause-not-stopping`。
- S4（新的對外 API 用 out-param）→ `LocalFiles` 回 `Result<…, IOError>`；單人存檔是 `SoloSave.hpp` 的 `Result<…, SoloSaveFailure>`。`PvpWorld.cpp` 的 `ReadText`（讀內容檔，world 函式庫）不併：兩個函式庫、用途不同。
- nit：N1 拿掉不可能的守衛；N3 註解與宣告對齊；N4 `Fixed` 的參數改名 `onServer`、rollback 的 `StepSchedule` 併進 `if (networked)`；N5 designated initializer；N6 `GameScene` 拿掉；N7 `ShowPause` 只寫 overlay、多人不算存檔路徑；N9 `PlayerSave.hpp` 註解；N10 測試用 `SoloSavePath`；N12 突變改名 `pvp-solo-input-ignored`。不改：N2（宣告修訂 2 寫了為什麼是 System）、N5 的出生點注入（測試以 `LocalSpawn` 給定出生點的 world 案守）、N8（註解寬度）、N11（點「撤退する」那一下也拿了指標：換場景時游標放開，實機看）。
- 審查後：`ctest -L "cpu|gpu"` 256/256；突變第 2 段 10/10 killed。

## 沒驗

- 真的視窗裡的單人一輪、暫停畫面：交使用者試玩。
