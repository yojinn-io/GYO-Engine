# 2026-11-26 房間裡「準備」按不下去的調查（`v8-product-pvp-room-ready-lag`）

Owner：產品（Object_FPS_PVP）。GYO2 主會話轉來的使用者實機回報，遊戲流程框架任務卡處理。作者 Opus 5.5／high。

## 回報

使用者原話：「現在有UI卡頓的問題，現象是當B加入到A建立的房間之後。會出現A無法點擊準備按鈕的情況。過了幾十秒才恢復，但是也是時好時壞。」
環境：gyo2 431d1b7d，`acceptance_macos.sh` 開兩個 Client（Mac 一台）；log 在主 checkout 的 `build/evidence/gyo2-pvp-game-flow/20261126-044558/`。那次的 log 只有場景與連線，房間階段看不出任何東西。A 在進房間前玩了三次單人（含 `kiriyu_extraction_250m`），B 沒有。

## 查了什麼

1. **兩個 Client＋真的服務（headless，沒有繪製）**：新案 `network-room:`（`services/stack/client_test.go` 的 `TestTwoClientsInARoom`）。A 從主菜單先在 250 m 地圖玩一場單人、撤退、進大廳開房；B 在大廳加入；A 用滑鼠點「準備する」（按下、幾幀後放開）來回六次，每次點之前把焦點從 B 換到 A，之後換回去。
   - A 的 ready 呼叫：從按下到回應 25～52 ms；房間（B 每 0.75 秒輪詢）0.2～0.7 秒內看到；之後 2 秒沒有被蓋回去。
   - 在這些條件下**沒有重現**：Client 的呼叫排隊（一次一個、先進先出）、房間的輪詢、rooms 服務的鎖、先玩過 250 m 單人留下的狀態、焦點在兩個視窗之間來回，這些在 headless 下都不造成卡住。沒涵蓋的：真的繪製（GPU、Metal 的 drawable）、真的 OS 事件（游標離開／進入視窗、事件佇列滿）。
2. **UI 是否每次輪詢就重建**：`world_ui` 每幀把 `UiBindings` 交給同一個 `UiRuntime::Update`；只有文件換了才重做 runtime（`engine/modules/world_ui/src/WorldUiModule.cpp` 的 `Prepare`）。輪詢只改 bindings，不是原因。
3. **HTTP 是否同步卡主執行緒**：`HttpCall::Start` 用非阻塞的 `StreamSocket::Connect`，`Poll` 只收送已到的資料（`engine/modules/http/src/Http.cpp:243-282`）。不是原因。
4. **macOS 被遮住的視窗**：SDL 的 Metal 後端每幀呼叫 `[layer nextDrawable]`（`_deps/sdl3-src/src/gpu/metal/SDL_gpu_metal.m:3908`），沒有 drawable 時最多等 1 秒。實機量測（本機 Intel Mac，兩個真的 Client 視窗疊在一起，`--frames`，主菜單）：後面的 A 跑 29 秒只有兩幀慢（377 ms、1085 ms，都在焦點切換前後），沒有持續變慢。主菜單裡「被遮住就一直卡」不成立；房間與玩過 250 m 地圖之後未量。證據：`build/evidence/gyo2-pvp-room-ready/20261126-050134-occlusion/`、`20261126-050155-occlusion-long/`。
5. **macOS 的第一下點擊**：SDL 的 Cocoa 視窗 `acceptsFirstMouse` 預設回 NO（`SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH` 沒設，`_deps/sdl3-src/src/video/cocoa/SDL_cocoawindow.m` 的 `acceptsFirstMouse`）：不在前面的視窗，第一下點擊只讓它到前面、不交給程式。兩個視窗之間來回操作時，每換一次視窗第一下就「點不下去」。和「時好時壞」相符，和「幾十秒才恢復」不完全相符。**未確認**。
6. **設計審查指出、未驗證的可能原因**（給下一次實機看 log）：事件佇列（`plugins/sdl_window/src/EventQueue.hpp`，1024 格）在慢幀時被游標移動塞滿、點擊被丟；滑鼠鍵卡在「按著」（失焦不放開滑鼠鍵：`engine/modules/input/src/InputModule.cpp`），之後的按下都不是 just pressed。

## 找到並修掉的一件

**還在路上的房間讀取把「準備」蓋回去**（審查指出，FakeGateway 決定性重現）：玩家按準備時若有一個房間的讀取正在路上（每 0.75 秒一次），它的答案（玩家按之前的房間）先回來，`asked` 被清掉，按鈕變回「準備する」，直到 ready 的答案回來。localhost 上通常只有一兩幀；幀慢時看得見，玩家再按一次就送出相反的請求。
- 失敗的案：「準備之前問的房間讀取不讓按鈕變回去」（FakeGateway 扣住讀取、按準備、放開讀取：修前按鈕變回「準備する」）。
- 修法（`RoomScene.cpp`）：`asked` 只在所有 ready 呼叫都有答案（成功或被拒）時放掉；房間的讀取不再放掉它（連按兩下時，第一個答案也不放掉）。突變 `pvp-room-ready-undone-by-read`。
- 這件能不能說明「幾十秒」：未確認（需要幀慢到看得見）。

## 做了什麼（log）

- `--verbose` 的 log（每個場景）：場景、慢幀（超過 100 ms，連同場景）、視窗焦點、後端丟掉的事件數（佇列滿）、Frame World 收到的左鍵按下與放開（位置；按下時輸入狀態是不是 just pressed、游標位置知不知道；遊戲場景不記，那裡一下就是一槍）、UI action、UI 文件的 canvas 排好與重做（runtime 的文件換了）、每個 Gateway 呼叫（method、target、tag、是不是 session、狀態或錯誤碼、在佇列裡等了多久、花了多久，0.1 ms）。
- 下次實機點了沒反應時看 log：
  - 沒有 `mouse: pressed` → OS／SDL 沒把點擊交來（第一下點擊、或 `input: N events dropped`）；
  - 有 pressed 但 `just pressed no` 或 `cursor unknown` → 輸入狀態擋住；
  - 有 pressed、沒有 `ui: pvp.ready` → UI 沒認（位置）；
  - 有 `ui: pvp.ready` → `gateway: POST /v1/rooms/…/ready` 等了多久、花了多久；
  - 前後有沒有 `slow frame`。
- `GatewayClient::TakeCalls`（`GatewayCallRecord`，最多留 64 筆）：log 的來源。`waited` 是那個呼叫從 Ask 到輪到它的時間（它的 session 與重試共用），`took` 是那次交換的時間。

## 設計審查（Opus／high，作者同模型，審查者只拿成果物與原始要求）

blocker 0、should 8、nit 9。

- S1（「沒有重現」的結論說太滿，e2e 沒走使用者的路徑：先玩過單人、250 m、焦點來回）→ e2e 改成 A 先在 250 m 玩一場單人、每次點擊前後換焦點；上面 1 寫明涵蓋與沒涵蓋的條件。仍沒重現。
- S2（同一個 root 的 runtime 換了 log 抓不到）→ 記 runtime 的文件指標，換了就記「laid out again」。
- S3（「點了沒反應」分不出原因）→ 加記放開、丟掉的事件數、按下時的 just pressed 與游標。`DrawnFrames.skipped` 沒加（`slow frame` 已能看出幀卡住）。
- S4（`waited` 把自己的 session 時間算進去）→ `waited` 改成只算排隊（輪到它的時刻固定下來），註解寫明一個呼叫的幾次交換共用。
- S5（`[pvp-gw-calls-waited]` 太弱）→ 斷言 `waited ≥` 前兩次交換的 `took`；突變 `pvp-gw-calls-asked-overwritten`。
- S6（失敗路徑的記錄沒守）→ 逾時案加記錄的檢查；突變 `pvp-gw-calls-timeout-not-kept`。
- S7（e2e 量的不是「按下 → rooms 收到」）→ 從按下開始，用 A 自己的 ready 呼叫記錄（`waited + took`）量；B 的視角留給「沒被蓋回去」。
- S8（重複）→ `network:` 案改用 `NetworkClient`；Go 抽出 `runClientCase`。
- 可能原因 3（還在路上的讀取把準備蓋回去）→ 上面「找到並修掉的一件」。
- N2 → 遊戲場景不記滑鼠；輪詢照記（主會話要求「HTTP 請求的送出、回應、耗時」）。N4 → method 與 target 在送出時記下，不再重組 session 的請求。N5 → 沒有 device id 的呼叫也記。N6 → 0.1 ms。N7 → `network`／`network-room` 的案（`[pvp-room-ready-*]`、`[pvp-gw-e2e-*]`）是 network 標籤、要真的服務，不跑突變。N8 → 鍵盤那一輪的數字刪掉（現在的 e2e 只用滑鼠）。
- 不改：N1（Tab 次數與點擊座標寫死：失敗時的訊息會說哪一步沒到）、N3（不開 `--verbose` 也留 64 筆：量小，換成只在 verbose 時接的 sink 要多一條接線）、N9（房間的案不開 Match 但起了 `matches.Local`：`Config` 要 Provider）。

## 第二次實機測試之後（主會話轉來，log：`build/evidence/gyo2-pvp-game-flow/20261126-052334/`）

這一輪「準備」兩邊都正常（A：`ui: pvp.ready` → `POST …/ready -> 200 in 32 ms`）。看出三件：

### 1. 焦點切換時的慢幀（0.7～1.2 秒）

使用者看了 log：「我看過了，應該是切換的問題。」接著更正：「不不不，之前一個是window，一個是全屏。」

- **量到的位置**：臨時在 runner（`service.Pump`、`product.RunFrame`、`WaitForEvent`）和 `sdl_gpu` 的 `Submit` 加計時。慢的都在 `SDL_WaitAndAcquireGPUSwapchainTexture`（Metal 的 `[layer nextDrawable]` 等 window server 還 drawable，上限 1 秒：`SDL_gpu_metal.m:3908`）；pump 只在啟動時 130～225 ms。
- **更正之前的解讀**：本機的設定檔（`~/Library/Application Support/GYO/object_fps_pvp/settings.json`，一台機器一份、兩個 Client 共用）從 01:31 起是 `"windowMode":"borderless"`。所以 05:01～05:36 的量測其實是**兩個都在無邊框全屏**（SDL 在 macOS 預設用 fullscreen Spaces：`SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES` 預設 "1"，`_deps/sdl3-src/include/SDL3/SDL_hints.h:3678`），看到的 1074 ms 是 Space 的切換，不是「視窗被蓋住」。使用者第一次回報時兩個 Client 也都讀同一份設定：都是全屏、各在自己的 Space。
- **對照組**（各用自己的 HOME 給各自的設定，不動使用者的設定檔）：
  - 兩個都是窗口（`20261126-055529-e1-win-win-noskip/`）：後開的 B 蓋住 A 的那一刻，A 取 swapchain 等 453 ms；啟動時各一幀 ~390 ms；其餘沒有慢幀。
  - A 全屏、B 窗口（`20261126-055624-e2-full-win-noskip/`）：開 B 時 macOS 不切 Space（B 一直 not focused），A 沒有慢幀。**Space 之間的切換沒能自動做出來**（要切前景的程式需要輔助使用權限），未量。
- **臨時試過的 `sdl_gpu` 改法**（被遮住、最小化、隱藏的視窗不取 swapchain，wgpu 的 `Occluded`／Bevy 跳過那一幀：`bevy_render/src/view/window/mod.rs:524`）：兩個都全屏時，之後的切換不再卡（被遮住的 1294 幀都跳過），但切過去那一刻的 ~1 秒還在；兩個都窗口時看不出差別（`20261126-055549-e1-win-win-skip/`）。效果在使用者的情況（Spaces）沒證實，**不放進這個 PR**，列成選項。
- **選項**（Engine 的行為，交主會話問使用者）：見 PR 與給主會話的回報。Bevy 的無邊框全屏是 winit 的 `Fullscreen::Borderless`（`bevy_winit/src/winit_windows.rs:86`、`system.rs:339`）；winit 在 macOS 上走不走 Spaces，本機沒有 winit 的原始碼，未確認。
- log：`window: starts windowed|fullscreen at WxH`、`window: windowed|fullscreen`（`WindowModeChanged`）：下次分得出哪個 Client 是全屏。

### 2. 非房主按「開始」沒有回饋

原因：被拒（`403 not_owner`）的話寫在狀態列，但下一次成功的房間讀取（0.75 秒內）就把它蓋回邀請的話。改成：
- 「開始」只給房主：知道房主之後（第一次讀取起），不是房主時按鈕寫「開始（部屋主のみ）」，按了不問 Gateway，狀態列說「開始できるのは部屋主だけです」（和 Gateway 的 `not_owner` 同一句）。
- 狀態列顯示最新的一句：玩家動作的話（成功的讀取不清掉它：被拒的理由看得到）或讀取出的問題（它會取代動作的話，下一次成功的讀取清掉）；兩者都沒有時是邀請的話。
- 突變 `pvp-room-start-asked-anyway`、`pvp-room-said-overwritten`、`pvp-room-start-label`。

### 3. 點擊有沒有命中按鈕

A 在大廳 (714,322) 點兩下都沒有 UI 動作。log 的 `mouse: pressed` 加「hits '元素' of canvas '…'」或「hits nothing」（照 world_ui 的算法：視窗的點換成 viewport 的實體像素，`HitTestUiLayout` 只看可互動的元素）。實機上的結果未驗（要使用者點）。

## 第二次設計審查（Opus／high，範圍 `34457ca8..81e5abc8`，審查者只拿成果物與原始要求）

blocker 0、should 5、nit 9。確認這個範圍沒有 Engine 的改動。

- S1（玩家動作的話永遠蓋過讀取的問題：「開始しています…」之後連線目標讀不出來就看不到，比改前退步）→ 狀態列顯示最新的一句：讀取出問題時清掉動作的話。新檢查 `[pvp-room-problem-shown]`（FakeGateway 讓房間讀取回 503）；突變 `pvp-room-problem-hidden`。
- S2（`readyCalls` 沒有測試）→ 新案「答案回來前按兩次準備，結果照第二次」（FakeGateway 扣住 ready）；突變 `pvp-room-ready-count-ignored`。
- S3（Gateway 拒絕動作的話沒有測試）→ 同一案：房主沒準備就開始，409 `not_all_ready` 的話撐過兩次讀取；突變 `pvp-room-refusal-cleared`。
- S4（命中用了事件的位置，world_ui 用的是 `CursorPosition`）→ 改用游標位置（不知道時寫明）；bindings 是場景改過之後的，註解寫明可能和 world_ui 那時不同。
- S5（「重做」只靠文件指標：同一個 root 拿掉再放回、或同一位址的新文件抓不到）→ 每幀以現有的 root 為準，不在的就忘掉：放回時再記一次「laid out」。
- N1 → `not_owner` 的話統一成「開始できるのは部屋主だけです」，本地的拒絕也用 `GatewayErrorText`。N4 → `waited` 的註解照實寫（從 Ask 之後第一次 Update 起算）。N5 → bindings 另開 query（沒有 bindings 的 root 也看得到）。N6 → 命中列出每個收輸入的 root 的元素。N8 → e2e 的 ready 記錄先收下，再檢查狀態碼。N9 → FakeGateway 自己數被扣住的呼叫（`Held()`），測試等它，不再和讀取的時機賽跑。
- 不改：N2（第一次讀取之前加入者看到「開始」：按了會問 Gateway、得到同一句話；要更早知道房主得讓大廳把房間帶過來，之後需要再做）、N3（房主換人後舊的話留到下一個動作）、N7（遊戲場景的 overlay 按鈕點擊不記：UI action 有記）。

## 使用者的決定與主迴圈心跳

使用者：「這個東西繼續觀察吧，我當時碰到的卡頓是完全無響應。」Spaces 的兩個選項都不做、Engine 不改；第一次的「完全沒反應、幾十秒」不是 Space 切換的 ~1 秒，原因未找到。

- **主迴圈心跳**（`--verbose`，`client_main.cpp`，產品自己的標記，不動 Engine）：迴圈在每一段開始時記下「現在在哪一段、從何時起」——Frame World（它的 exchange in：這一輪的第一件事）、Render World 到畫之前（它的 exchange in，在 extract 之前）、`sdl_gpu` 的 `GPUSubmit`（uploads、acquire、draw、present：`Submit` stage 裡、`GPUSubmit` 之前）、換場景（`betweenFrames.run` 的前後）、其餘的兩幀之間（`Cleanup` stage 之後：window 的 event pump、等待；等待受 `minFrameInterval` 4 ms 限制，超過 1 秒的幾乎只會是 pump）。另一條執行緒每 200 ms 看一次：一段超過 1 秒就當下記一行「one part of the frame for N s, in <段>」（迴圈一直不回來也留得下），之後每 5 秒再記，回來時記「going again after about N s」（最多多算 200 ms）。每段都不到 1 秒、加起來超過 1 秒的幀不在這裡記，由 `slow frame` 事後記。
- 實測（臨時把門檻降到 300 ms，兩個全屏 Client）：卡住的當下記到「in sdl_gpu's GPUSubmit (uploads, acquire, draw, present)」，回來時「going again after about 1.2 s」（`20261126-063417-heartbeat2-300ms-check/`）。門檻 1 秒時，1.08 秒的停頓可能落在兩次檢查之間而不記（`20261126-062443-heartbeat-full-full/`）。

## 第三次設計審查（Opus／high，範圍 `81e5abc8..901aa353`，審查者只拿成果物與原始要求）

blocker 0、should 2、nit 9。確認沒有 Engine 的改動。

- S1（Frame World 與 Render World 的標記在 stage 的尾端：world.build 在各 unit 之後登記，同一 stage 照登記順序；exchange in、asset apply、各 extract 卡住會歸到錯的段）→ 這兩個標記改成 `ExchangeIn`（每個 World 這一輪的第一件事，Render World 的 extract 在它之後）。
- S2（讀不出連線目標時清掉動作的話沒有測試）→ 新案「開始了的房間給的連線目標讀不出來，就說出來（蓋過『開始しています…』）」（FakeGateway 的 `BadConnect`）；突變 `pvp-room-connect-problem-hidden`。
- N1 → `Mark` 先存時間再存段。N2 → 「going again after about」。N3 → 段名「sdl_gpu's GPUSubmit (uploads, acquire, draw, present)」。N4 → 換場景單獨一段（`betweenFrames.run` 的前後）。N6 → system 名 `object_fps_pvp.Heartbeat.*`。N7 → `std::atomic<Part>`（enum class）。N8 → 看門狗一開始就記一次「the start」（啟動時卡住也看得到）。N9 → 用詞改成「one part of the frame」，上面寫明和「兩幀之間超過 1 秒」的差別。
- 不改：N5（看門狗的判斷沒有單元測試：`client_main` 沒有測試目標，實機以降低門檻驗過）。

## 驗證

- 本機（Intel Mac）`ctest -L "cpu|gpu"` 262/262、`-L "go|network"` 5/5（`object_fps_pvp.gateway_client` 跑兩個 Go 測試：一個 Client 的，和兩個 Client 的）。
- 突變：`pvp-gw-*`、`pvp-room-*`、`pvp-lobby-*` 82/82 killed（`pvp-room-ready-undone-by-read` 的 find 跟著程式改了一次，重跑 killed）。

## 沒驗

- 使用者的現象本身：沒有重現。等使用者用這個分支的 `acceptance_macos.sh` 再試一次、帶回 log。
- 第一下點擊（上面 5）要不要改：要改就是 `sdl_window` 設 `SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH`（Engine 的 Plugin，所有產品都受影響），交使用者決定。
