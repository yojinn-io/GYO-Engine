# 2026-11-25 遊戲流程框架第 3 段：多人接 Gateway（`v8-product-pvp-game-flow`）

Owner：產品（Object_FPS_PVP）。GYO2 主會話開的任務卡（遊戲流程框架）。作者 Opus 5.5／high。分支疊在第 2 段（#347）上。
根據：[事前宣告](../../games/object_fps_pvp/docs/game-flow-declaration.zh-Hant.md)第 3.4 節與第 4 節第 3 段；[Client 端交接說明](../../games/object_fps_pvp/docs/gateway-client-handoff.zh-Hant.md)。

## 做了什麼

1. **Gateway 的 Client**（`include/object_fps_pvp/client/Gateway.hpp`、`src/client/Gateway.cpp`）：外部協議 v1 的請求（session、房間列表、建房、讀房、加入、準備、開始、離開、讀存檔）與回應的解析；`GatewayClient` 一次一個呼叫（`GYO::Http`），先開 session，401 時重開一次再試；5 秒沒有回答就算 `timeout`。每個變更一把亂數冪等鍵。裝置 id 在玩家目錄的 `device-id`（32 bytes 亂數 hex，第一次產生）。session 在場景之間經 `GatewayState`（`CarryOver`）帶著。
2. **大廳**（`LobbyScene.cpp`，`ui/pvp_lobby.json` 整個換掉）：每秒列房間、每五秒讀戰績（`GET /v1/saves/me`）；▲▼ 選房、參加；建房（模式 協力／生存、地圖＝安裝的 arena）；從遊戲回來時先 `leave` 那個房間；`status`（玩家做的事）與 `notice`（Gateway 的狀態）分兩行。
3. **房間**（`RoomScene.cpp`，`ui/pvp_room.json`）：每 0.75 秒讀房；準備切換、房主開始、退出；`started` 且有自己的 `connect` → `MultiGame`（`NetConnectTarget{server, userId, token}`）；404／410 → 回大廳並顯示理由。商人、任務只有位置。
4. **遊戲裡的玩家是連線的使用者**（`LocalUserOf`）：`Sim` 與 `Frame` 的 `LocalUser` 用 `connect.userId`（Gateway 的 user id），單人用 Client 自己的。端到端測試先抓到這一條（用 `--user` 的亂數 id 時，Match 給的玩家永遠不是「自己的」）。
5. **直接輸入位址的舊大廳**（#330）刪掉：`LobbyWorld.hpp`、`lobby/AddressText.hpp` 與它的測試、突變 `pvp-lobby-filter-off`、`pvp-lobby-any-address`。`ParseMatchAddress` 移到 `Scenes.hpp`（`--connect`、`--gateway`、房間的 connect 用）。
6. **程式**：`--gateway A.B.C.D:P`（預設 `127.0.0.1:7700`）；`--profile` 也決定裝置 id。

## 驗證

- 本機（Intel Mac）`ctest -L "cpu|gpu"` 256/256；`ctest -L "go|network"` 5/5（新 `object_fps_pvp.gateway_client`：Go 的 `TestTheRealClientPlaysThroughTheGateway` 在一個行程裡起全部服務、真的 Match 行程，跑 Client 測試程式的 `network:` 一案：建房→準備→開始→經 UDP 連上 Match、自己的玩家到了→撤退→大廳讀到「出撃 1 回」，約 10 秒）。`go vet`、`go test ./services/...` 全過。
- client 測試（假 Gateway：`gyo_test_loopback_listener` 上的 HTTP）：請求的組成與回應的解析；大廳列房與戰績、建房→準備→開始→`MultiGame` 的 connect（位址、user、token 位元組）、每個呼叫帶 token、每個變更帶鍵、回大廳先 leave；列出的房間可加入；session 過期時重開一次；沒人回答的 Gateway 顯示接不上。
- 突變：`pvp-gw-no-bearer`、`pvp-gw-no-key`、`pvp-gw-no-reopen`、`pvp-gw-user-own`、`pvp-gw-start-ignored`、`pvp-gw-leave-not-sent`、`pvp-gw-join-ignored`，加改寫的 `pvp-lobby-quit-ignored`、`pvp-lobby-ui-not-drawn`、`pvp-scene-end-stays`、`pvp-scene-sim-kept`：11/11 killed。

## 沒驗

- 兩個 Client（兩個 `--profile`）經 Gateway 進同一場：端到端測試只有一個 Client。交使用者實機驗收。
- 真的視窗裡的大廳與房間畫面：交使用者試玩。
