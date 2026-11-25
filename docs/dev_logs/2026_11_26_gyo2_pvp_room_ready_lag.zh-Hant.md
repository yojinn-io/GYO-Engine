# 2026-11-26 房間裡「準備」按不下去的調查（`v8-product-pvp-room-ready-lag`）

Owner：產品（Object_FPS_PVP）。GYO2 主會話轉來的使用者實機回報，遊戲流程框架任務卡處理。作者 Opus 5.5／high。

## 回報

使用者原話：「現在有UI卡頓的問題，現象是當B加入到A建立的房間之後。會出現A無法點擊準備按鈕的情況。過了幾十秒才恢復，但是也是時好時壞。」
環境：gyo2 431d1b7d，`acceptance_macos.sh` 開兩個 Client（Mac 一台）；log 在主 checkout 的 `build/evidence/gyo2-pvp-game-flow/20261126-044558/`。那次的 log 只有場景與連線，房間階段看不出任何東西。A 在進房間前玩了三次單人（含 `kiriyu_extraction_250m`），B 沒有。

## 查了什麼

1. **Client 的邏輯、輪詢與 Gateway（headless）**：新案「B 加入 A 的房間；A 立刻準備，而且一直是準備」（`network-room:`，兩個完整的 Client＋真的服務，`services/stack/client_test.go` 的 `TestTwoClientsInARoom`）。
   - 鍵盤 Enter：A 的準備 0.03 秒內就到房間。
   - 滑鼠點「準備する」（按下、幾幀後放開）來回六次：每次 0.20～0.72 秒到房間（B 每 0.75 秒輪詢一次才看得到），之後 2 秒內沒有被輪詢蓋回去。
   - **沒有重現**。所以 Client 的呼叫排隊（一次一個、先進先出）、房間的輪詢、`asked` 的處理、rooms 服務的鎖都不是原因（至少在 headless、沒有繪製的情況）。
2. **UI 是否每次輪詢就重建**：`world_ui` 每幀把 `UiBindings` 交給同一個 `UiRuntime::Update`，不重建 runtime（`engine/modules/world_ui/src/WorldUiModule.cpp` 的 `UpdateUi`）。不是原因。
3. **HTTP 是否同步卡主執行緒**：`HttpCall::Start` 用非阻塞的 `StreamSocket::Connect`，`Poll` 只收送已到的資料（`engine/modules/http/src/Http.cpp:243-282`）。不是原因。
4. **macOS 被遮住的視窗**：SDL 的 Metal 後端每幀呼叫 `[layer nextDrawable]`（`_deps/sdl3-src/src/gpu/metal/SDL_gpu_metal.m:3908`），沒有 drawable 時最多等 1 秒。實機量測（本機 Intel Mac，兩個真的 Client 視窗疊在一起，`--frames`，主菜單）：後面的 A 跑 29 秒只有兩幀慢（377 ms、1085 ms，都在焦點切換前後），沒有持續變慢。**「被遮住就一直卡」不成立**（主菜單；房間與玩過 250 m 地圖之後的狀態未量）。證據：`build/evidence/gyo2-pvp-room-ready/20261126-050134-occlusion/`、`20261126-050155-occlusion-long/`。
5. **macOS 的第一下點擊**：SDL 的 Cocoa 視窗 `acceptsFirstMouse` 預設回 NO（`SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH` 沒設，`_deps/sdl3-src/src/video/cocoa/SDL_cocoawindow.m:2207-2215`）：不在前面的視窗，第一下點擊只讓它到前面、不交給程式。兩個視窗之間來回操作時，每換一次視窗第一下就「點不下去」。和「時好時壞」相符，和「幾十秒才恢復」不完全相符。**未確認**。

## 做了什麼

- `--verbose` 的 log（每個場景）：場景、慢幀（超過 100 ms，連同場景）、視窗焦點、Frame World 收到的滑鼠按下（位置）、UI action、UI 文件的 canvas 排好（重建）、每個 Gateway 呼叫（method、target、tag、是不是 session、狀態或錯誤碼、在佇列裡等了多久、花了多久）。下次實機：點了沒反應時，看 log 有沒有 `mouse: pressed` → 有沒有 `ui: pvp.ready` → `gateway: POST /v1/rooms/…/ready` 等了多久、花了多久、前後有沒有 `slow frame`，就分得出是 OS 沒把點擊交來、UI 沒認、還是呼叫卡住。
- `GatewayClient::TakeCalls`（`GatewayCallRecord`，最多留 64 筆）：log 的來源。
- 新案兩個：`network-room:`（上面 1，留著當回歸守衛）、「每個呼叫都記下：問了什麼、答了什麼、等了多久、花了多久」。

## 驗證

- 本機（Intel Mac）`ctest -L "cpu|gpu"` 262/262、`-L "go|network"` 5/5（`object_fps_pvp.gateway_client` 跑兩個 Go 測試：原本的一個 Client，和新的兩個 Client）。
- 突變：`pvp-gw-calls-not-kept`、`pvp-gw-calls-no-wait` 2/2 killed。

## 沒驗

- 使用者的現象本身：沒有重現。等使用者用這個分支的 `acceptance_macos.sh` 再試一次、帶回 log。
- 第一下點擊（上面 5）要不要改：要改就是 `sdl_window` 設 `SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH`（Engine 的 Plugin，所有產品都受影響），交使用者決定。
