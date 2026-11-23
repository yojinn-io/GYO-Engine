# 2026-11-23 SDL 線子批 W：`input`、`window` 與 `sdl_window`（`v8-arch-gyo2-sdl-w`）

Owner：架構。GYO2 主會話開的任務卡（使用者「開 W、A、3D 物理、PvP 移植準備」）。分支 `claude/gyo2-sdl-w-window-input-7718bb`（開向 `gyo2`）。作者 Opus 5.5／high。
根據：[SDL 總宣告](../architecture/target/v2/sdl-declaration.zh-Hant.md)修訂 21（使用者 2026-11-23 動工前核准「SDL 照建議核准」）第 3.3、6.2、6.3、9.1、9.2、10.2 節的 W 列；Object_Connect 盤點（PR #302）的 G3～G6。照新 AGENTS（#296）不另寫事前宣告；取捨寫在 [input](../architecture/target/v2/input.zh-Hant.md)、[window](../architecture/target/v2/window.zh-Hant.md)、[sdl](../architecture/target/v2/sdl.zh-Hant.md) 三頁與 PR 說明。

## 做了什麼

1. **`input` Module**（`engine/modules/input`，`default=yes`、沒有依賴）：`Message` 11 種（鍵盤、失焦、丟事件、滑鼠鍵、游標移動、相對移動〔G4〕、進出視窗〔G5〕、滾輪、組字、確定）、狀態（`ButtonInput<KeyCode>`／`<MouseButton>`、`AccumulatedMouseMotion`／`Scroll`、`CursorPosition`、`TextInput`）、`TextInputRequest`；公開 set `InputSystems`（`Main`／`PreUpdate`），五個狀態 System 照 Bevy 分開。`KeyCode` 用 USB HID 的值。
2. **`window` Module**（`default=yes`、沒有依賴）：`WindowSettings`（產品選項；可不可改大小＝G6）、`Message` 4 種、`WindowState`／`Clipboard`／`WindowRequest`、一次性的 `WarpCursor`（G3）與 `CopyToClipboard`；公開 set `WindowSystems`；`closeWhenRequested` 時收到關閉要求就設 `WindowRequest.close`。
3. **`window_camera`**（`default=yes`、`depends=window;camera`）：X1 交來的膠水。`WindowState` 知道大小時寫進 `CameraTargetSizes{0}`，值不同才寫；新 World 第一輪就有（後端在第一輪寫目前的大小）。
4. **`sdl` 函式庫**：`ClockPair`（時鐘配對與換算的純函式，照 master 修過的版本）、`NativeWindows`（不透明把手）；`GYO::SDL` 從 INTERFACE 改成 STATIC、不連 SDL。
5. **`sdl_window` Plugin**：`Configure` 建窗、放把手與 `SDLWindowPump`、裝 runner（可不裝）；`Build` 登記 `threadBound` 的把手、兩個寫者（`PreUpdate`、`before` 兩個 set）與套用請求的 System（`Last`）；SDL 只在 `src/adapters/sdl/`。SDL-7 的對應表寫在 sdl 頁第 5 節。
6. **examples**：一個 example 一個目錄、照目錄加入；新 `examples/sdl_window`（Escape 或 `--frames N` 關窗）；`plugin_removal.py` 拿掉帶 `sdl*` 的 example 目錄（P 交來的）。
7. **文件**：新頁 input、window；sdl 頁補 `sdl_window`、SDL-3、SDL-4、SDL-7 與對應表；orchestration O-12 照核准清單第 4 件改字（「進」可以在 `PreUpdate`＋`before` 擁有者的公開 set、登記的可以是 Plugin）。

## W 要定的題，怎麼定的（卡片判；理由在三頁）

| 題 | 定法 |
|---|---|
| 多個 World 都要讀輸入 | 只有列 `sdl_window` 的 World 直接收；其他 World 由作者轉（總宣告 9.2 第 2 條）；同執行緒第二個 World 已在批次表後續 |
| 多個 World 的請求怎麼合 | 只讀列 `sdl_window` 的 World 的請求；`TextInputRequest` 兩個寫者時後跑的算（一般 Resource，作者排先後，同 Bevy） |
| 新 World 的初始狀態 | 準備函式放空的；`sdl_window` 在那個 World 的第一輪先寫目前的大小、焦點、剪貼簿、游標（SDL-7 例外 (2)）；按住的鍵不補 |
| 生成世界期間的事件 | 照常寫、照常更新（輸入不因載入而丟） |
| 佇列上限與丟棄 | 預設 1024（不大於 Kernel 的 `Message` 上限）；一個 SDL 事件的 `Message` 全進或全不進；丟了寫 `InputEventsDropped`，`input` 放開所有按鍵與滑鼠鍵 |
| 請求 System 的錯誤 | 不當 System 的錯誤；不記、下一幀重試、計數（`SDLWindowPump`） |
| `SDL_EVENT_QUIT` 與 os 的關機請求 | SDL 不裝訊號處理、最後一個視窗關了不 quit；quit → `WindowCloseRequested{Application}`；runner 第一次看到 os 的旗標 → `{Shutdown}` |
| 關窗與結束迴圈 | `WindowCloseRequested` →（`closeWhenRequested` 或遊戲）設 `WindowRequest.close` → `Last` 關窗 → runner 跑完這一幀結束；產品可關掉 `closeWhenRequested` 擋下 |
| 骨架關掉的 World | `InputOptions`、`WindowOptions`、`WindowCameraOptions`、`SDLWindowWorldOptions` 指名 |
| 時間戳 | sdl 函式庫的 `ClockPair`、SDL-3 |
| 視窗大小進 `CameraTargetSizes` | 新的小單位 `window_camera`（X1 的兩個候選之一） |

## 交使用者的

- **時間取樣的形**（總宣告：W 不自己判）：(a) 交換函式「進」／(b) 照 Bevy 留在 System（今天的程式）。已轉主會話問使用者（卡片傾向 (b)）。**本批沒有動 world_time**，等回覆。
- 三頁 Design Invariants（IN-、WN-、SDL-3／4／7）與 O-12 的最終文字：合併前核准。

## 設計審查（Opus／high，作者同為 Opus；Fable 暫停）

blocker 1、should 8、nit 5，全部處理：

- **B1**（屬實，`SDL_mouse.c:768`：非相對模式的 `xrel` 是絕對座標相減，游標被擋住就是 0）：加 `WindowRequest.cursorGrab`（照 Bevy 的 `CursorGrabMode`），`Locked`＝SDL 的相對模式時 `MouseMotion` 才是裝置的量；G4 的用法改寫（遊戲鎖住游標、自己畫）；`MouseMotion` 不乘邏輯比例。
- **S1**（SDL 的視窗事件在入列時蓋時刻、移動事件蓋 OS 的時刻）：同一次泵裡時刻嚴格遞增（`OrderTimes`），組字與確定的平手一起解。突變 `sw-times-unordered`。
- **S2**（拖曳時滑鼠被捕獲、位置在視窗外）：照實寫進 input 頁（Bevy 濾掉；`input` 不知道視窗大小）。
- **S3**（改大小的快取不知道玩家拉過）：改成一次性的 `ResizeWindow`。
- **S4**（視窗事件被輸入擠掉；關機請求被丟就永久失效）：視窗事件另一個上限（預設 256）；關機請求進了佇列才算看過。突變 `sw-window-capacity-shared`。
- **S5**：組字的單位照實寫（SDL 在 Windows、macOS 是 UTF-16）、-1 保留。**S6**：滾輪不乘 −1（照 Bevy）。**S7**：IN-3 改字（沒按住的鍵的重複是按下，同 Bevy）。
- **S8**：座標換算抽成純函式並測、SDL-E1 加測試（突變 `sw-coord-inverted`、`sw-e1-not-reported`）；拿掉釘 Kernel 平手順序的測試（總宣告 10.2 W 列：不釘 Kernel 不承諾的 tie-break）。「值不同才呼叫」沒有計數測試，寫進範囲の限界。
- nit：第一輪的快照拆成視窗與游標兩個查詢（剪貼簿只讀一次）；`Unidentified` 合成同一個鍵寫進 input 頁；README 在拿掉 Plugin 後的斷連結不處理。

## 和 A（sdl_audio）的協調

- 命名：SDL 是縮寫（A 提、W 同意）：`GYO::Plugins::SDLWindow`、`GYO::SDLWindow`、`SDLWindowPlugin`、`GYO::Plugins::SDL`、`GYO::SDL`；P 的 `SdlBuildTests.cpp` 改名 `SDLBuildTests.cpp`。naming_guard 的 Plugin 檢查在 A；W 的測試照 A 的守衛整檔包進 Plugin 的 namespace。
- SDL 的全域狀態（A 寫進 sdl 頁第 2 節）：只管自己的子系統、不呼叫 `SDL_Quit`；hint 讀完還原。W 指出 `SDL_HINT_NO_SIGNAL_HANDLERS` 在 EVENTS 初始化時讀、AUDIO 隱含 EVENTS，A 已加成 sdl* 共通規則。

## 查證（本機 macOS Intel，`test` preset，RelWithDebInfo）

- merge `origin/gyo2`（23a43378，含 I #301）之後：全體建置 0 錯誤、`ctest -L cpu` 216/216。
- 新的 ctest：`input.module`、`window.module`、`sdl.clock`、`sdl_window.events`（SDL dummy 驅動、`SDL_PushEvent`）、`sdl_window.example`（example 實際用 runner 跑 30 幀後自己關窗、回 0）。`sdl_window.events` 不帶環境變數直接跑也過（測試自己設 dummy 驅動）。
- 突變 `docs/architecture/target/v2/scripts/window_input_mutations.json`：34/34 killed。第一輪 2 個 wrong_failure，查明後改 expect：`in-set-missing` 被 Kernel 的啟動檢查擋下（set 沒有成員時 `before` 是啟動錯誤）；`sw-snapshot-missing` 在第一個 World 被 dummy 驅動建窗時送的大小事件蓋住，改用替換後的 World 判。
- `build.ci` 的 `test_plugin_removal.py` 16 件；`plugin_removal.py copy` 在本機拿掉 `sdl`、`sdl_window`、`examples/sdl_window`。
- L1 三平台：見 PR #308。
