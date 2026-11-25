# 2026-11-25 遊戲流程框架第 4 段：loading 與就緒、Match 開始前的相關性（`v8-product-pvp-game-flow`）

Owner：產品（Object_FPS_PVP）。GYO2 主會話開的任務卡（遊戲流程框架）。作者 Opus 5.5／high。分支疊在第 3 段（#349）上。
根據：[事前宣告](../../games/object_fps_pvp/docs/game-flow-declaration.zh-Hant.md)第 3.5 節與第 4 節第 4 段；使用者 `v8-arch-gyo2-loading-screen-and-relevance`。

## 做了什麼

1. **Match 的開始**（`world/PvpWorld.hpp`）：`MatchInfo::phase`（`Waiting`／`Playing`，照常複製）；`PlayerInput::ready`；`StartMatch`（權威端，`FixedUpdate/Update`，`MovePlayers` 之前）：等待的使用者都有玩家且輸入 `ready`，或第一個玩家來了之後超過期限 → `Playing`。`Waiting` 時 `MovePlayers`、`UseWeapons` 拿不到輸入（不動、不轉、不開槍；兩端同一條，Client 照複製來的 phase 預測）。Match 沒有等待的使用者（手動起、`--connect`、測試）時一開始就 `Playing`；Local World 等的是本機玩家、沒有期限。
2. **相關性**：`ShowPlayers`（Match，`Main/Update`，`SeatPlayers` 之後）：`Waiting` 時每個 Peer 的 `PeerVisibility` 只有它控制的玩家（別人的位置不送），`Playing` 時拿掉（看見全部）。net 現有的 C-VIS1，net 不改。
3. **Match 與 Go**：`--expect-users ID,…`、`--start-deadline-seconds N`（預設 60）；matches 的 `Spec.Players`（rooms 開始時已經給成員）→ `local.go` 加 `--expect-users`。
4. **Client 的就緒**（`TrackReadiness`）：(1) 遊戲的資產連同依賴（`AssetView::Ready`：角色、武器、viewmodel、貼圖、聲音、字型、地圖模型、overlay 文件；失敗也算完成，不卡住）→ 進度 0～0.8；(2) 預熱：資產齊了之後 `DrawnFrames::shown` 前進兩幀且這期間沒有略過的 draw（程式經 Frame World 的交換函式從 `SDLGPUStatus` 寫進；沒有 GPU 的測試不等）；(3) 自己的玩家從 Match（單人：本機）到了；(4) 再畫完一幀 → `ready`，經操作的 `StepInput::ready` 進 `PlayerInput::ready`。
5. **loading 畫面**（`pvp_game.json` 的 `loading` canvas）：不透明的底、進度條（`horizontal_slider` 綁 `progress`）、現在在做什麼（資料、描畫準備、連線、第一個畫面、等其他玩家），蓋到 `ready` 且 `Playing`；「中止」：多人照撤退（離開房間回大廳），單人回單人房間、不結算。單人的暫停只在開始之後才停 Solo World 的時間。

## 驗證

- 本機（Intel Mac）`ctest -L "cpu|gpu"` 256/256、`ctest -L "go|network"` 5/5、`go test ./services/...` 全過。
- world：兩個 Client、等待 {1,2}：開始前各自只看得到自己、一個準備好的 Client 走路不動；兩個都準備好 → `Playing`、互相看得到、能走；只來一個時期限到了也開始。
- client：loading 畫面在第一幀、進度單調遞增且經過 0～1 之間、載完畫完自己的玩家到了才 `ready`、Match 那邊這個 Client 的玩家 `ready == 1`、之後畫面拿掉；單人一輪先 loading 再開始。端到端（`object_fps_pvp.gateway_client`）：Match 等房間的成員準備好才開始，Client 在開始後走暫停畫面撤退。
- 突變：`pvp-start-ignores-ready`、`pvp-start-not-hidden`、`pvp-start-not-frozen`、`pvp-start-no-deadline`、`pvp-loading-not-shown`、`pvp-loading-not-told`，加 find 改了的 `pvp-pause-not-shown`、`pvp-pause-not-stopping`、`pvp-gw-evacuate-no-leave`：9/9 killed。

## 沒驗

- 真的 GPU 上的預熱（`DrawnFrames` 從 `SDLGPUStatus` 來的那條）、畫面上 loading 蓋住的樣子與進度條：交使用者試玩。
