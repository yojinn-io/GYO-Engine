# SDL 的 Plugin（`plugins/sdl*`）

更新：2026-11-28（SDL-5 改成「只讀渲染 World，只寫這一幀的收據 `RenderFrameReceipt`」，第 7 節的 `GPUSubmit` 跟著改，A2 宣告第 14.13、14.14 節）；2026-11-27（全畫面 pass、只寫深度的 pass、`Depth` target、RP-3 的斷言、shader 工具的 `--function`，渲染管線線 A2 `v8-arch-gyo2-render-a2`：第 7 節的 `Configure`、`GPUSubmit`、3D pass、shader 工具）；2026-11-27（render target 與貼圖格式，渲染管線線 A1 `v8-arch-gyo2-render-a1`：第 7 節的 `GPUSubmit`、3D pass）；2026-11-25（視窗模式與 VSync `v8-arch-gyo2-window-mode-vsync`，[事前宣告](window-mode-vsync-declaration.zh-Hant.md)：第 5 節的「視窗模式」、SDL-7 對應表的 `ENTER_FULLSCREEN`／`LEAVE_FULLSCREEN` 與例外 (2) 的模式、第 7 節的 present mode）；2026-11-24（SDL-7 表的 `TEXT_EDITING`：起點與長度換成位元組、依 video driver 選單位，第「升版」節加一條重查，`v8-arch-gyo2-sdl-u` 的後續）；2026-11-24（子批 R2a `v8-arch-gyo2-sdl-r2a`：第 1 節 `sdl_gpu` 列、第 7 節 `sdl_gpu` 與 shader 工具、SDL-4 的 `GPUSubmit`、SDL-5）；2026-11-23（子批 A `v8-arch-gyo2-sdl-a`：第 6 節 `sdl_audio`、SDL-3 的音訊一半、Plugin 的名字、SDL 的全域狀態、`GYO::Sdl` 改 `GYO::SDL`）；2026-11-23（子批 W `v8-arch-gyo2-sdl-w`：`sdl` 函式庫的共用型別、第 5 節 `sdl_window`、SDL-3、SDL-4、SDL-7 與對應表、examples 的拿法；**最終文字在 W 的合併前核准**）；2026-11-23（新頁，子批 P `v8-arch-gyo2-sdl-p`）。由來：[SDL 總宣告](sdl-declaration.zh-Hant.md)修訂 21（使用者 2026-11-23 動工前核准「SDL 照建議核准」，HANDOFF `v8-arch-gyo2-sdl-declaration-approved`）第 9.1、10.2、12 節。本頁是 `plugins/sdl*` 的 owner 文件：子批 P 開頁、寫目錄的形、取得、選擇與 SDL-1、SDL-2；W（`sdl_window`）、A（`sdl_audio`）、R2a（`sdl_gpu`）各自補自己那一列與 SDL-3～SDL-7（草案在總宣告第 12 節）。

Owner：架構。

## 一個例子

Match（伺服器）不帶任何 `sdl*` 的列：configure 不加 `plugins/sdl`，SDL 不下載、不建，產品裡沒有 SDL。KeepTool 帶 `sdl_window`、`sdl_gpu`：建置層加這兩個目錄，也沿依賴加 `sdl`（函式庫），由 `sdl` 抓 SDL 3.4.0、上 GYO 的補丁。把整組 `sdl*` 從 repo 拿掉，Engine、所有 Module、headless 的測試照樣建得起、測得過（SDL-2）。

## 1. 列與依賴

一列＝一個目錄＝一個 target＝一個單位（`config/plugins.csv`）。依賴只有 `plugins/sdl*` → `engine/modules/*` 與 `sdl` 函式庫；Engine 不 include `plugins/`，Module 不依賴 Plugin（登記檔讀取時的 FATAL_ERROR）。

| 列 | 角色 | 誰建 | 現況 |
|---|---|---|---|
| `sdl` | 函式庫（`library_only`）：抓 SDL3、上補丁；`sdl*` 之間共用的、我們的詞寫的型別：不透明的視窗把手 `NativeWindows`、時鐘配對 `ClockPair`（`plugins/sdl/include/plugins/sdl/`） | P、W | 有（target `GYO::SDL`，連 `GYO::Time`，不連 SDL） |
| `sdl_window` | 來源側：Runner、Window、InputEvents、Ime、剪貼簿（第 5 節） | W | 有（target `GYO::SDLWindow`） |
| `sdl_gpu` | 渲染側：GPU、GPUSubmit、建置時的 shader 工具（第 7 節） | R2a | 有（target `GYO::SDLGPU`） |
| `sdl_audio` | `Configure`：AudioOutput（第 6 節） | A | 有（target `GYO::SDLAudio`） |

各列的內容、依賴與兩個 World 的接法見總宣告第 9.1、9.2 節。一列的依賴在登記檔寫：例 `sdl_window` 的 `depends` 是 `sdl;input;window`。

## 2. 目錄的形

```text
plugins/<名>/
  CMakeLists.txt          # target gyo_<名>、alias GYO::<名>；SDL3::SDL3-static 以 PRIVATE 連結
  include/…               # 給依賴者的標頭：我們的詞，不 include SDL3/
  src/…                   # System、單位、runner：不 include SDL3/
  src/adapters/sdl/…      # 只有這裡的 .cpp（與這裡的私有標頭）include SDL3/
  tests/CMakeLists.txt    # BUILD_TESTING 時加入；這個 Plugin 沒被選就不存在
  tests/adapters/sdl/…    # 直接呼叫 SDL 的測試
```

- adapter 給 System 用的介面標頭放在 `src/adapters/sdl/` 外（例：`plugins/sdl_gpu/src/GPUDevice.hpp`），由 adapter 的 `.cpp` 實作（SDL-1）。
- **名字**（命名規範 N17 的「`GYO::Plugins::<plugin>` 由第一個 plugin 批定」，A 定）：SDL 是縮寫，照縮寫表寫 `SDL`（一大一小不准；`Json` 是唯一的例外）。目錄 `plugins/<名>` ↔ namespace `GYO::Plugins::<從目錄名推>`（同 Engine 的單位：以 `_` 分段、首字母大寫、縮寫段照表）↔ target `gyo_<名>`／`GYO::<推出的名>`：`sdl_audio` ↔ `GYO::Plugins::SDLAudio` ↔ `GYO::SDLAudio`；單位型別 `<推出的名>Plugin`（N20）；含入路徑 `plugins/<名>/…`。Plugin 的測試也在它的 namespace 裡（測試在 Plugin 的目錄，守衛照目錄判）。守衛：`naming.guard` 把 `plugins/` 底下每個目錄當一個單位查（`ACRONYMS` 加 `sdl`）。
- **SDL 的全域狀態**（`sdl*` 共用；A 定，設計審查 S6）：SDL 是行程全域的。(1) 每個 `sdl*` 只用 `SDL_InitSubSystem`／`SDL_QuitSubSystem` 管自己用的子系統（參照計數），**不呼叫 `SDL_Quit`**：`SDL_Quit` 會把所有子系統一起收掉（例：音訊的 `SDL_QuitAudio` 會 `SDL_DestroyAudioStream` 所有裝置 stream，`SDL_audio.c:1110-1116`），之後 `sdl_audio` 停裝置時就是對已釋放的 stream 操作。(2) 第一次 `SDL_InitSubSystem` 的執行緒被 SDL 記成主執行緒（`SDL.c:289-290`；AUDIO 隱含 EVENTS）：`Configure` 在呼叫 `Product::Build` 的執行緒上跑，所以那條執行緒就是 SDL 的主執行緒，有視窗的 runner 要在同一條執行緒（SDL-4）。(3) hint 是行程全域的：設了要在用完時還原（`SDL_ResetHint`），環境變數優先於程式設的值。(4) 訊號歸 os 一家（os.md DI 8）：任何 `sdl*` 在 `SDL_InitSubSystem` 之前設 `SDL_HINT_NO_SIGNAL_HANDLERS=1`、之後還原——AUDIO、VIDEO 都隱含 EVENTS（`SDL.c:401-405`），EVENTS 初始化時沒有這個 hint 就裝自己的 SIGINT／SIGTERM handler（`events/SDL_quit.c:150`），誰先初始化都一樣（W 指出）。守衛：審查（測試要碰訊號 API，os 守衛 R3 不准）。
- Plugin 的測試放在自己的目錄、跟著列走；名字以 Plugin 名開頭（例：`sdl.build`）。`plugins/` 是執行時的程式：os 守衛的 R1～R4 都管它，它的測試也受 R4（os.md DI 1、2；`tests/` 底下的豁免不延伸到 `plugins/<名>/tests/`）；Windows 上它的執行檔套 UTF-8 的 code page（os.md DI 11）。

## 3. 選擇：只建被選的 Plugin

- 建置層（`build/cmake/GyoBuild.cmake`）在所有產品、測試、examples 之後，加入被選的 Plugin 目錄：產品在 `gyo_product_units(... PLUGINS …)` 列出的，加上快取變數 `GYO_PLUGINS` 點名的（登記檔的名字，或 `ALL`），連同它們依賴的 Plugin，照登記順序（`gyo_plugin_selection`，`GyoUnitRegistry.cmake`）。產品先 configure、以名字 link Plugin 的 target，CMake 在全部目錄 configure 完之後才解析；link 一個沒被選的 Plugin，generate 時 CMake 自己報 target 不存在。Plugin 自己的測試裡也可以用 `gyo_product_units(... PLUGINS <自己>)`。
- 預設 `GYO_PLUGINS` 空：只建產品帶的。`test` 系列的 preset（含 L1 的 `ci-<os>`）設 `ALL`：三個平台都建 SDL、跑 Plugin 自己的測試。`core` preset 設空。
- `gyo-build.json` 的 `plugins` 記這次加入的 Plugin。

## 4. SDL 的取得

| 項目 | 值 |
|---|---|
| 版本 | SDL 3.4.0（tag `release-3.4.0`，commit `a962f40bbba175e9716557a25d5d7965f134a3d3`，2026-01-01 發布） |
| 來源 | 官方 release 的原始碼 archive `https://github.com/libsdl-org/SDL/releases/download/release-3.4.0/SDL3-3.4.0.tar.gz`（15,567,602 bytes） |
| 驗證 | `URL_HASH SHA256=082cbf5f429e0d80820f68dc2b507a94d4cc1b4e70817b119bbb8ec6a69584b8`（GitHub 的 asset digest） |
| 放哪 | FetchContent，建置輸出的 `_deps/sdl3-*`；不放 `plugins/` 底下、不放 `third_party/`（`third_party/` 在登記檔之前無條件加入，放那裡刪不掉 SDL，總宣告第 9.1 節） |
| 建法 | 靜態（`SDL_STATIC=ON`、`SDL_SHARED=OFF`）；不建 SDL 自己的測試、examples、install |

**靜態的理由**：產品旁邊不必再帶一個 SDL3 的動態函式庫，Windows 的測試不必設 DLL 搜尋路徑。master 用動態（`GyoRuntimeDeployment.cmake` 的 `SDL3-shared` 複製）；要改回動態時，打包（子批 K 的 tools 線）一起處理。

**GYO 的編譯選項**：`plugins/` 在 `add_compile_options`（`-ffp-contract=off`、MSVC 的 `/Zc:preprocessor`，`GyoBuild.cmake`）之後加入，FetchContent 的 SDL 也吃到這些選項。L1 三平台（含 MSVC）建得起是 P 的驗收之一。

### SDL 的補丁

`plugins/sdl/patches/*.patch` 照檔名順序上（`plugins/sdl/cmake/ApplyPatches.cmake`；已上的跳過）。原始碼是 release archive 解開的、在建置輸出裡（可能在 GYO 的 git 工作樹之內），所以 `git apply` 用 `GIT_CEILING_DIRECTORIES` 停在原始碼目錄、當一般的 patch 程式用。用 `FETCHCONTENT_SOURCE_DIR_SDL3` 指定的原始碼不經 patch 步驟：configure 時檢查補丁的標記，沒上就是 FATAL_ERROR。

| 補丁 | 缺陷 | 移除條件 |
|---|---|---|
| `gyo-cocoa-ime-composing-keydown` | macOS：組字中，輸入法處理一個鍵卻沒改組字時，SDL 照樣送出按下（日文輸入法的第一個 Return 只確定候選字），程式中途收到 Return。補丁讓組字中按下的鍵不送給程式，除非輸入法把它當命令轉出（`doCommandBySelector:`），同 winit。由來：master `f775acb2`（`v8-tool-ui-editor-ime-enter`） | SDL 上游修好；或升版後補丁上不去（先在新版重現缺陷，再移植補丁） |

### 升版

改 `plugins/sdl/CMakeLists.txt` 的 URL 與 `URL_HASH`、本節的表、`plugins/sdl/tests/adapters/sdl/SDLBuildTests.cpp` 的版本；每個補丁重新上、上不去時照上表的移除條件。另外重查 `sdl_window` 的 adapter 依賴的 SDL 實作：Windows 的輸入法位置是 UTF-16（`SDL_windowskeyboard.c` 的 `IME_SendEditingEvent`，和 SDL 文件寫的「UTF-8 字元」相反；3.4.0 驗過），其他後端是 code point——改了就改 `EditingUnitOf`（`plugins/sdl_window/src/adapters/sdl/Translate.hpp`）。下載新版本要使用者同意（版本、來源、大小、SHA-256）。

## 5. `sdl_window`

`plugins/sdl_window/`（`GYO::Plugins::SDLWindow`，登記單位 `SDLWindowPlugin`，標頭 `plugins/sdl_window/SDLWindowPlugin.hpp`）。依賴 `sdl;input;window`。SDL 只在 `src/adapters/sdl/WindowBackend.cpp` 與私有的 `Translate.hpp`；給單位、System、runner 用的介面是 `src/WindowBackend.hpp`（我們的詞）。

- **`Configure`**（產品一次）：讀 `window` 的 `WindowSettings` 與自己的 `SDLWindowOptions`，經 adapter 起 SDL 的 video、建窗；放進 Product 的 buffer：`SDL::NativeWindows`（給 `sdl_gpu`）、`SDLWindowPump`（給自己的 runner 用的產品）；`installRunner`（預設 true）時裝 runner。建不了窗是啟動錯誤 `SDL-E1`（沒有 World 被建）。`OnDestroy`（所有 World 之後）關窗、停 video。
- **`Build`**（列它的那個 World）：登記把手 Resource `SDLWindowHandle`（`threadBound`，雜湊不寫）；`SDLWindowWriteInput`、`SDLWindowWriteWindow` 掛 `Main`／`PreUpdate`，各自 `before` `InputSystems`、`WindowSystems`（使用者 `-sdl-input-writer-preupdate`）；`SDLWindowApplyRequests` 掛 `Main`／`Last`。準備函式在準備 World 的執行緒放把手（新 World 的第一輪先寫目前的狀態）。骨架關掉的 World 用 `SDLWindowWorldOptions`。
- **runner**（照 Bevy 的 winit runner）：每幀「看一次 os 的關機請求 → 經 adapter 泵事件 → `RunFrame`」，`WindowRequest.close` 被套用後跑完那一幀就結束、回 Success；`RunFrame` 失敗回 Failure（文字同預設 runner）。`minFrameInterval` 大於 0 時，一幀太快就經 adapter 等 OS 的事件到期滿（還沒有畫面的程式不空轉；有了 `sdl_gpu` 由送出畫面等垂直同步——`Presentation` 是 `AutoNoVsync` 時不等，第 7 節）。runner 不看事件的內容。**runner 不取樣**：總宣告第 9.1 節 Runner 列的「泵事件 → 取樣 → `RunFrame`」與 SDL-3 草案的「runner 的取樣」照使用者 2026-11-23 的選擇（(b) 照 Bevy 留在 System，`v8-arch-gyo2-time-sampling-in-system`）拿掉取樣，取樣在 world_time 的 `AdvanceTime`。總宣告第 6.1 節第 7 點（渲染 World 不帶 world_time、內插要的量經 `ExtractRes` 從來源讀）照原文成立，由 R1a、R2a 照用；「這一件和取樣一起在 W 定」那句就此定案。（總宣告本身照原樣保留，開頭的後記指到這裡。）
- **live frame**（R3；〔`v8-product-pvp-port-live-frame`，[事前宣告](live-frame-sim-thread-declaration.zh-Hant.md)第 4.1 節〕〔定案：使用者 2026-11-25「可以合了 #330、#331」（主會話先說明「合併即定案」），HANDOFF `v8-arch-gyo2-pvp-lend-lobby-merged`〕）：OS 的 modal 迴圈（macOS 的 live resize、Windows 拖標題列與邊框）卡住泵的時候，SDL 以約 60 Hz 送 `SDL_EVENT_WINDOW_EXPOSED`（`data1 = 1`），允許從 event watch 重畫（`SDL_events.h:139-140`、`SDL_video.c:4302-4312`）。adapter 在 `SetLiveFrame` 第一次給函式時 `SDL_AddEventWatch`、隨 adapter 在主執行緒拆掉（master 坑 5）；watch 只在「這個事件、這個視窗、主執行緒（坑 6）、泵或等事件進行中、不重入」全部成立時，先把**現在的**視窗大小寫成 `WindowResized` 進佇列（每次都寫；排隊的 resize 事件要等泵返回，坑 3；SDL-7 的例外 (3)，使用者 2026-11-25，`v8-arch-gyo2-sdl7-live-frame-resize`），再呼叫函式。runner 的函式＝`RunFrame`；它回錯時記下、泵這一輪照樣處理完，之後 runner 回 Failure；例外存起來，泵或等事件返回後再丟（坑 4）。函式整段在 SDL 的 watcher 鎖裡（別的執行緒的 `SDL_PushEvent` 都等它）：不能在主執行緒等會推 SDL 事件的執行緒，也不能呼叫 `SetLiveFrame`。live frame 裡 `ApplyWindowRequests` 不呼叫 adapter：狀態式的請求由下一個 modal 外的幀套用，一次性的（`SetWindowMode`、`WarpCursor`、`ResizeWindow`、`CopyToClipboard`）存在 Service、下一個 modal 外的幀照順序先套用。`SDLWindowOptions::liveFrame`（預設 true）；自己的 runner 用 `SDLWindowPump::SetLiveFrame`。Linux 沒有這種 modal 迴圈。守衛：`sdl_window.events`「the live frame runs from SDL's live-resize expose…」「the runner's live frame…」「the live frame first writes the window's size now…」（用 `SDL_RunOnMainThread` 在泵裡送 expose）；突變 `sw-live-*`。按下視窗角到開始拖動之間 macOS 不送回呼（坑 2），畫面那段會停；模擬不停靠下一條。
- **自己的執行緒的 World**（R5、O-13；〔`v8-product-pvp-port-sim-thread`，同宣告第 4.3 節〕〔定案：使用者 2026-11-25「可以合了 #330、#331」（主會話先說明「合併即定案」），HANDOFF `v8-arch-gyo2-pvp-lend-lobby-merged`〕）：`SDLWindowOptions::ownThreadWorlds` 列的 World，runner 在遊戲初始化之後 `Product::Lend`，各開一條 `Threads::RoleThread`（`gyo-world-<名字>`）：推一輪 → 等到「這一輪開始的時刻＋下一個固定步還差的時間」（world_time 的 `FixedTime::accumulator.UntilNextStep()` 除以 `VirtualTime::scale`，最多一秒；沒有 world_time、暫停或倍率 0 時等 `ownThreadIdle`）→ 直到停止請求。推一輪回錯（World 壞了，或交換函式的 E2）或有例外離開一輪時，那條執行緒記下、設「已停」旗標後停；主迴圈每圈看到就結束 runner。主執行緒照舊泵事件、`RunFrame`（跳過交出的）。runner 每一條出口都先停止、join、`Return`；之後模擬執行緒的例外在主執行緒照樣丟出，它的錯誤或 Product 的 `Failure()` 讓 runner 回 Failure。依賴：`sdl_window` 的 CMake 直接私下連 `GYO::WorldTime`、`GYO::Threads`（只讀交出的 World 的時間型別）。**不**寫進 `config/plugins.csv` 的 `private_depends`：那一欄的 Module 是 World 的單位依賴，O-E3 會要求每個用 `sdl_window` 的 World 也帶 `world_time`（2026-11-25 #331 合併後大廳的 World 因此起不來，已改回）。守衛：`sdl_window.events`「a World of ownThreadWorlds runs on its own thread while the main thread is held…」（主執行緒停 250 ms，交出的 World 照跑）、「a lent World with world_time waits for its next fixed step…」、「a lent World's exchange failure (E2) ends the runner…」、「an exception that leaves a lent World's round goes on from the runner…」；突變 `sw-own-thread-*`。
- **佇列**（`src/EventQueue.hpp`，不公開）：元素就是 `input`、`window` 的 `Message`；一個 SDL 事件的 `Message` 全進或全不進；輸入與視窗各一個上限（`queueCapacity` 預設 1024、`windowQueueCapacity` 預設 256；都不大於 Kernel 的 `Message` 上限，一幀的寫入不會溢出 Kernel 的緩衝；滑鼠灌爆輸入那份也擠不掉失焦、關閉）；os 的關機請求進了佇列才算看過（滿了下一幀再試）；滿了丟掉並計數，輸入那半的寫者寫 `InputEventsDropped{count}`（`input` 因此放開所有按鍵與滑鼠鍵）。`SDLWindowPump::DroppedEvents()` 是累計。
- **請求**（`Last`）：`TextInputRequest`、`WindowRequest` 的欄位和上次 SDL 接受的不同才呼叫 adapter（同 Bevy 的 `CachedWindow`）；被拒時不記、下一幀重試、計數（`SDLWindowPump::RefusedRequests()`、`LastRefusal()`）——**不當 System 的錯誤**（回錯誤會讓 World 壞、Product 結束，O-11；照「告訴外面發生了什麼」）。`SetWindowMode`、`ResizeWindow`、`WarpCursor`、`CopyToClipboard` 是 `Message`，每筆經 adapter 一次，照這個順序（模式在大小之前）；被拒時計數、不重試。例外：全屏中的 `ResizeWindow` 不呼叫 SDL，記下來（後到的蓋掉先到的），回窗口後才給（下面「視窗模式」）。`close` 最後套用。
- **座標**：SDL 的視窗座標乘「像素密度／顯示縮放」得到邏輯像素（Windows 150 % 是 1/1.5；Retina 是 1）；請求反過來除。建窗時 SDL 的大小是視窗座標，所以 Windows 縮放下照邏輯大小再設一次。
- **SDL 的 hint**（整個 process；設了、讀完就還原）：`SDL_HINT_NO_SIGNAL_HANDLERS=1`（訊號歸 os；EVENTS 初始化時讀，VIDEO 隱含 EVENTS）與 `drawComposition` 時的 `SDL_HINT_IME_IMPLEMENTED_UI=composition`（Windows 上程式自己畫組字；起 video 時讀一次；ImGui 不畫組字，K 定）在 `SDL_InitSubSystem(VIDEO)` 前設、之後 `SDL_ResetHint`；`SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE=0`（關窗是遊戲的要求、不是 SDL 的 quit）在視窗活著時都要，關窗、`SDL_QuitSubSystem(VIDEO)` 之後還原；`mouseMotion`＝`LogicalPixels` 時的 `SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE=1` 同樣在視窗活著時都設著、關窗後還原。只管 VIDEO、不呼叫 `SDL_Quit`（A 的「SDL 的全域狀態」同一套規則）。
- **時刻的順序**：同一次泵裡，一個事件的時刻不晚於前一個時就改成前一個加 1 ns（`OrderTimes`）——SDL 有的事件蓋 OS 的時刻、有的在入列時蓋（`SDL_events.c:1802`），送達的順序才是真的；`input` 用時刻分兩種 `Message` 的先後。
- **視窗模式**（`v8-arch-gyo2-window-mode-vsync`，[事前宣告](window-mode-vsync-declaration.zh-Hant.md)第 4.2 節；使用者 2026-11-25「開，無邊框」）：`WindowSettings::mode`＝`BorderlessFullscreen` 時建窗帶 `SDL_WINDOW_FULLSCREEN`；`SetWindowMode` 經 adapter 呼叫 `SDL_SetWindowFullscreen`；GYO 從不設 `SDL_SetWindowFullscreenMode`，所以 SDL 的全屏一直是桌面的模式＝無邊框、不換螢幕的顯示模式（再設一次 `nullptr` 在 macOS 的 Spaces 轉場中會 `SDL_SyncWindow`、卡住主執行緒，`SDL_video.c:2126-2165`）。切回窗口時 SDL 自己還原窗口的位置與大小。結果不看呼叫的回傳，看 SDL 的 `ENTER_FULLSCREEN`／`LEAVE_FULLSCREEN`（macOS 的 Spaces 是非同步；玩家按綠色按鈕也走這條）→ `WindowModeChanged`；全屏時的大小照常由 `PIXEL_SIZE_CHANGED` 回報。**全屏中的 `ResizeWindow`**：SDL 在全屏中的 `SDL_SetWindowSize` 各平台不一樣（Windows 丟掉、`SDL_windowswindow.c:937`；macOS 會去改全屏的視窗；dummy 直接回報新大小），所以「SDL 的旗標是全屏」或「要了全屏、旗標還沒變」（macOS 非同步進入的空檔；旗標變成全屏或看到 `LEAVE_FULLSCREEN`〔含進入被拒〕就不再算）時 adapter 把大小記下（請求側的快取，同 `DeferredRequests`，不是事件側的狀態），`ApplyWindowRequests` 每幀結尾在兩者都不成立時才 `SDL_SetWindowSize`（macOS 非同步離開時等旗標清掉之後）。視窗是不是全屏一律問 SDL、不記「遊戲最後要的模式」：玩家也會自己離開全屏（設計審查 B1）。Bevy 沒有這層統一（任何模式都直接 `request_inner_size`，`bevy_winit/src/system.rs:400`，結果看平台）；不統一的話每個遊戲都得自己「等 `WindowModeChanged{Windowed}` 再送大小」、還要處理 macOS 的非同步。建窗就是全屏時，`WindowSettings::size` 一律記成回窗口時的大小（2026-11-25 這台 Mac 的實機驗收抓到：macOS 上以全屏建的窗離開 Space 時回到建窗時的螢幕大小，`SDL_cocoawindow.m:1578-1584` 只在有 pending 時設大小；dummy 上 SDL 核心會還原，CI 抓不到）。守衛：`sdl_window.events`「entering and leaving fullscreen…」「SetWindowMode makes the window borderless fullscreen…」（dummy 驅動上顯示的視窗：SDL 核心照常送 ENTER／LEAVE、還原窗口的大小）、「a window made borderless fullscreen…」（含替換後的 World 第一輪知道是全屏）、「after the player left the game's fullscreen…」（測試直接呼叫 SDL 模擬玩家離開）、「a SetWindowMode sent in a live frame…」；突變 `sw-map-enter-ignored`、`sw-snapshot-mode-missing`、`sw-made-windowed`、`sw-mode-never-fullscreen`、`sw-size-not-held`、`sw-held-size-dropped`、`sw-flush-not-called`、`sw-live-mode-dropped`。
- **游標的抓法**（`WindowRequest.cursorGrab`）：`Locked`＝`SDL_SetWindowRelativeMouseMode`（`MouseMotion` 是裝置的量，G4）、`Confined`＝`SDL_SetWindowMouseGrab`。
- **相對量的單位**（`SDLWindowOptions::mouseMotion`，2026-11-24 `v8-arch-gyo2-relative-mouse-system-scale`）：`Device`（預設）照 SDL 給的——相對模式下 Windows 是 raw input 的滑鼠計數、沒有指標速度與加速（`SDL_windowsevents.c:564`、`:596`；`SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE` 預設關，`SDL_mouse.c:111`、`:733-738`），macOS 是 Cocoa 已經加速過的 points（`SDL_cocoamouse.m:599`、`:609`），X11、Wayland 是未加速的量（`SDL_x11xinput2.c:91-97` 的 `raw_values`、`SDL_waylandevents.c:1194-1200` 的 `dx_unaccel`）；非相對模式是游標在視窗座標上的移動。`LogicalPixels`：視窗活著時設 `SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE=1`（SDL 經 hint callback 隨時讀，所以持續設著、關窗時還原），`MouseMotion` 的 `xrel`、`yrel` 乘上每個視窗座標的邏輯像素數（同 `CursorMoved`）。前提：單位只有後端知道，遊戲換算不了。對照：winit 的 `DeviceEvent::MouseMotion` 永遠是 raw（`bevy_winit/src/state.rs:459`），沒有這個選擇；GYO 多一個選項，是因為 SDL 3.4 在非相對模式沒有 raw，而鎖住游標又自己畫游標的遊戲要的是游標的速度。外部測試：`sdl_window.events` 的 [sw-motion-*]。dummy 驅動的視窗座標就是邏輯像素（係數 1），所以「Translate 有沒有把選項用上」由純函式 `MotionDelta` 的測試、`Translate` 這個參數沒有預設值（漏傳就編譯不過）與審查守。環境變數 `SDL_MOUSE_RELATIVE_SYSTEM_SCALE` 有設時照 SDL 的規則以它為準（第 2 節「SDL 的全域狀態」(3)）。
- **一個 Product 只有一個 World 列 `sdl_window`**（不加守衛）：佇列只有一份，第二個 World 的寫者拿不到；兩個 World 的請求各讀一次，狀態式的請求後跑的蓋掉先跑的。其他 World 的輸入由作者轉（總宣告第 9.2 節第 2 條）。

### SDL-7 的對應表

每種 SDL 事件對應 0 個或固定的幾個 `Message`（`Translate`，`src/adapters/sdl/Translate.hpp`）。視窗事件的視窗 id 不是自己的視窗 → 0 個。時刻照 SDL-3 換算。

| SDL 事件 | `Message` |
|---|---|
| `KEY_DOWN`／`KEY_UP` | `input` `KeyboardInput`（scancode → `KeyCode`〔同為 USB HID 的值，`input` 沒命名的 → `Unidentified`〕、`down`、`repeat`、無修飾時的字〔`SDL_GetKeyFromScancode`〕、SDL 給的 `mod`） |
| `TEXT_EDITING` | `input` `TextComposition`（`start`、`length` 換成字串的位元組：SDL 文件寫「UTF-8 字元」＝code point，Cocoa、Wayland、X11、IBus、Fcitx 都給 code point，只有 Windows 給 IMM 的 UTF-16 位置〔3.4.0 驗過、升版重查〕——adapter 在建視窗時照 `SDL_GetCurrentVideoDriver()` 選單位換算；起點、長度各自可以是 -1＝沒給） |
| `TEXT_INPUT` | `input` `TextCommitted` |
| `MOUSE_MOTION` | `input` `CursorMoved`（`x`、`y` 換成邏輯像素）＋`MouseMotion`（`xrel`、`yrel` 照 SDL 給的；`mouseMotion`＝`LogicalPixels` 時換成邏輯像素），兩個 |
| `MOUSE_BUTTON_DOWN`／`UP` | `input` `MouseButtonInput`（左、中、右、X1→Back、X2→Forward；其他鍵 → 0 個） |
| `MOUSE_WHEEL` | `input` `MouseWheel`（`x`、`y` 照 SDL 給的，macOS 的自然捲動不反過來，同 Bevy） |
| `WINDOW_MOUSE_ENTER`／`LEAVE` | `input` `CursorEntered`／`CursorLeft` |
| `WINDOW_FOCUS_GAINED` | `window` `WindowFocused{true}` |
| `WINDOW_FOCUS_LOST` | `window` `WindowFocused{false}`＋`input` `KeyboardFocusLost`，兩個 |
| `WINDOW_PIXEL_SIZE_CHANGED` | `window` `WindowResized`（事件的像素大小、向 SDL 查的顯示縮放） |
| `WINDOW_DISPLAY_SCALE_CHANGED` | `window` `WindowResized`（向 SDL 查的像素大小與縮放） |
| `WINDOW_ENTER_FULLSCREEN`／`LEAVE_FULLSCREEN` | `window` `WindowModeChanged{BorderlessFullscreen}`／`{Windowed}`（GYO 只做無邊框全屏，SDL 的全屏一律記成它） |
| `WINDOW_CLOSE_REQUESTED` | `window` `WindowCloseRequested{Window}` |
| `QUIT` | `window` `WindowCloseRequested{Application}` |
| `CLIPBOARD_UPDATE` | `window` `ClipboardChanged`（向 SDL 讀的文字，超過 `clipboardLimit` 截在 UTF-8 的邊界） |
| 其他 | 0 個 |
| （不是 SDL 事件）runner 第一次看到 os 的關機請求 | `window` `WindowCloseRequested{Shutdown}` |
| （例外 (2)）新 World 的第一輪 | `window` `WindowResized`、`WindowFocused`、`WindowModeChanged`、`ClipboardChanged`（目前的值；模式看 SDL 的旗標）；游標在視窗內時 `input` `CursorEntered`＋`CursorMoved` |
| （例外 (3)）`WINDOW_EXPOSED`（`data1 = 1`，live frame 中：這個視窗、主執行緒、泵或等事件進行中、不重入） | `window` `WindowResized`（當下的值，向 SDL 查），在 live frame 的一幀之前（使用者 2026-11-25，`v8-arch-gyo2-sdl7-live-frame-resize`） |

`WINDOW_RESIZED`（視窗座標的大小）不寫：邏輯大小由像素大小與縮放算出，和 `PIXEL_SIZE_CHANGED` 重複。

## 6. `sdl_audio`：音訊輸出

〔子批 A `v8-arch-gyo2-sdl-a`；總宣告第 7 節、第 10 節的 A。〕

**一個例子**：Object_Connect 帶 `sdl_audio`。產品啟動時，`sdl_audio` 在自己的 `Configure` 建一個混音器放進 Product 的共用處、把音效 loader 登記進 asset、開預設的播放裝置。遊戲的表現層 System 經提交口送「播哪個 clip、什麼時刻」，SDL 的音訊執行緒每個 block 叫一次 `Mixer::Render`。在沒有音效卡的機器上，裝置開不了：產品照樣跑、照樣送聲音（計數、不出聲），遊戲讀共用處的 `AudioOutput` 知道為什麼。關掉產品時，裝置在所有 World 之後、共用處之前停。

**角色**：只做 `Configure` 的單位（總宣告第 3.3 節）：產品帶著、不放進任何 World（放進也沒有東西登記）。`depends`＝`sdl;audio`（都是函式庫）。

**`Configure` 做的事**（照這個順序）：

1. **混音器**：產品給了 `AudioOutputMixer`（`ProductDescription::options`，一個 `shared_ptr<Mixer>`）就播產品的、不建；否則照 `AudioOutputOptions::mixer` 建一個，`Create<Mixer>` 放進 Product 的共用處，其他單位與 World 的 build 用 `Find<Mixer>()` 找（總宣告第 7 節的 (i)，(ii) 當覆寫）。設定超出混音器的限界是啟動錯誤 `SA-E1`（產品寫錯設定，不是機器的狀況）。
2. **音效 loader**：共用處有 asset 的 `AssetLoaderRegistry`（產品帶 `world_asset`）時登記 `WavLoader`（audio 頁第 1 節「SDL 那一批有建混音器的登記單位時，改由它登記」，使用者 2026-11-23 核准）。同一個 loader 再登記不變，所以產品自己也登記無妨；被拒是啟動錯誤 `SA-E2`。沒有 asset 的產品不登記、不報錯。
3. **裝置**：開預設的播放裝置（`AudioOutputOptions::deviceSampleFrames` 是給 SDL 的 block 大小提示，0＝平台的預設），把 `AudioOutput` 放進共用處。**開不了不是啟動錯誤**：`AudioOutput::Failure()` 說為什麼（`AudioOutputFailure`，N25 的 E2：這個輸出沒做成，其他都健康；碼 `SubsystemUnavailable`、`DeviceUnavailable`、`DeviceNotStarted`，detail 是 SDL 的訊息），產品照跑、提交口照收（計數、不出聲）。「告訴外面發生了什麼、不替外面決定」：要不要提示玩家、要不要結束，是作者讀了決定。Bevy 同形：開不了裝置只警告、`AudioOutput.stream` 是 `None`，遊戲照跑（`bevy_audio/src/audio_output.rs:16-31`）。
4. **停**：`OnDestroy` 停裝置。O-15：回呼在所有 World 之後、共用處之前跑，所以裝置先停，`sdl_audio` 建的混音器（共用處）與產品給的混音器（選項，活到共用處之後）都比最後一次回呼活得久（audio DI 7）。

**adapter**（SDL-1、`-sdl-system-calls-via-adapter`）：`src/AudioDevice.hpp` 是我們的詞（開、停、統計），`src/adapters/sdl/AudioDevice.cpp` 是 `sdl_audio` 裡唯一 include SDL 的檔案。開：`SDL_InitSubSystem(AUDIO)` → `SDL_OpenAudioDeviceStream(預設播放裝置, S16／2ch／48000, 回呼)` → `SDL_ResumeAudioStreamDevice`（裝置的格式與預設裝置的切換由 SDL 的 stream 處理）。回呼在 SDL 的音訊執行緒：每個 block 開頭讀一次 `Time::Now()` 當 anchor（SDL-3、audio DI 3），一次最多算 4096 frame、分幾次時下一次的 anchor 加上已算的時間；不丟例外、不配置、不上鎖、不記日誌（audio DI 7；SDL 自己的 `SDL_PutAudioStreamData` 可能配置）。只持混音器的指標、不持所有權。停：`SDL_DestroyAudioStream`（在 SDL 的鎖下解除綁定，返回後不再有回呼）→ `SDL_QuitSubSystem(AUDIO)`（參照計數，多個產品各開各的不互相關掉）。照 master 修過的做法（`engine/audio/backend/sdl/src/SdlAudioOutput.cpp`，origin/master），差別：master 的輸出擁有混音器，這裡只借（audio 頁第 4 節）；拿掉 master 的 anchor 紀錄（只給 master 的起音紀錄用，沒有讀者）。

**和別家比**：

| | 誰擁有混音器 | 停的先後 | 開不了裝置 |
|---|---|---|---|
| GYO `sdl_audio` | 共用處（或產品），輸出只借 | `OnDestroy` 停裝置 → 共用處解構 | 回報在共用處，不是啟動錯誤 |
| Bevy `bevy_audio` | 開裝置的 `AudioOutput` Resource 擁有 rodio 的 sink（`audio_output.rs:16-31`） | Resource 解構時 | `warn!`、`stream: None`，照跑 |
| kira | `AudioManager::new` 把 renderer 交給後端（`crates/kira/src/manager.rs:70-92`），後端擁有它 | `CpalBackend` 的 `Drop` 停 stream（`backend/cpal/desktop.rs:158-166`） | `AudioManager::new` 回 `Err`，作者決定 |
| SoLoud | `Soloud` 物件自己是混音器，`init` 開後端（`include/soloud.h:230`） | `deinit`（`src/core/soloud.cpp:192`） | `init` 回錯誤碼 |
| miniaudio | `ma_device_init` 開裝置、作者的回呼混音（`miniaudio.h:137-149`） | `ma_device_uninit` | 回 `ma_result` |

**`scheduleDelay` 的重新推導**（總宣告第 7 節；判定在量之前寫）：

- **量什麼**：延遲至少要蓋過「事件到下一幀送出」（30 FPS 的消費者最多 33.3 ms）＋「送出到下一次回呼」（回呼的最長間隔）。量的是每個平台、預設播放裝置、SDL 預設 block 大小下的回呼最長間隔：`gyo_sdl_audio_block_record 10`（不是 CTest，要真的裝置），每台跑兩次，取大的。
- **判定**：需要的延遲＝33.3 ms＋最長間隔；預設值＝各平台需要的延遲的最大值加 5 ms 的餘裕，往上取到 5 ms 的倍數。比 60 ms 大就改大；小於 60 ms 時，三個平台都量到才改小（只量到一部分不改小：沒量到的平台可能更大）。
- **量到的**：（見下表；`v8-arch-gyo2-sdl-a` 本機）

| 平台 | 機器、驅動、預設輸出 | 指定 | device_frames | 最長間隔（兩次） | 需要的延遲 |
|---|---|---|---|---|---|
| macOS 26.7.1（Intel） | MacBook Pro、coreaudio、LG HDR 4K（DisplayPort） | 不指定（SDL 預設） | 1024 | 21.463／21.470 ms | 33.3＋21.5＝54.8 ms |
| 同上 | 同上 | 480 | 480 | 10.831 ms（一次） | 44.1 ms |
| 同上 | 同上 | 256 | 256 | 10.863 ms（一次） | 44.2 ms |
| Windows | **未量**（主戰場；這張卡的機器上沒有） | | | | |
| Linux | **未量**（CI 沒有音訊裝置，dummy／disk 驅動的節拍不是真的） | | | | |

- **結論**：量到的只有 macOS：54.8＋5＝59.8 → **60 ms，預設值不變**（判定的「比 60 大才改大」不成立；只量到一個平台，不改小）。master 的量測（macOS 54.8 ms、Windows 44.4 ms）只當比較基準：macOS 這次和 master 同一台、同一個數。Windows、Linux 要在實機上各跑兩次 `gyo_sdl_audio_block_record 10`；任一個的需要的延遲＋5 ms 超過 60 ms 就照判定改大。
- macOS 的 CoreAudio 節拍下限約 10.8 ms：指定 256 也只到 10.9 ms（同 master）。

**範囲の限界**：

- 只有預設的播放裝置；選裝置、裝置清單、錄音不做（沒有點名的 Feature）。預設裝置被拔掉時，SDL 的 stream 自己跟到新的預設裝置；跟不上時（沒有裝置了）不出聲，`AudioOutput` 不改狀態（不輪詢）。
- `AudioOutput` 不隨裝置中途的狀況改變；`Stats()` 只供顯示與量測。
- 回呼統計是 relaxed 原子、一個一個讀，期間的快照不一致；不拿來相減。
- `sound`／`wav` 由 `sdl_audio` 登記 `WavLoader`，在產品的 `configure` 之前；asset 對同一組（型別、格式）拒絕第二個 loader（`FormatTaken`），所以帶 `sdl_audio` 的產品不能換成自己的 `wav` loader（Bevy 允許重複登記、只警告，`bevy_asset/src/server/loaders.rs:65-67`）。沒有點名的 Feature 要換；要換時在 `AudioOutputOptions` 加一個不登記的選項。
- `OnDestroy` 停裝置和共用處的反序解構今天給出同一個先後（`AudioOutput` 在 `Mixer` 之後 Create、先解構），測試分不出拿掉 `OnDestroy` 的差別，沒有守衛；留著是因為停的先後不該依賴 Create 的順序（總宣告第 7 節、audio 頁第 4 節）。
- `scheduleDelay` 的 Windows、Linux 量測未做；合併前要不要先量，由使用者定。

## 7. `sdl_gpu`

`plugins/sdl_gpu/`（`GYO::Plugins::SDLGPU`，登記單位 `SDLGPUPlugin`，標頭 `plugins/sdl_gpu/SDLGPUPlugin.hpp`）。依賴 `sdl;render;render_world`（不寫 `sdl_window`：它常在另一個 World，O-E3 逐 World 檢查會要求渲染 World 也列它；視窗經 Product 的共用處交接）。SDL 只在 `src/adapters/sdl/GPUDevice.cpp` 與 shader 工具 `src/adapters/sdl/ShaderTool.cpp`；給單位與 System 用的介面是 `src/GPUDevice.hpp`、`src/EmbeddedShaders.hpp`（我們的詞）。資料的格式在 [render 頁](render.zh-Hant.md)。

- **`Configure`**（產品一次）：從 Product 的 buffer 找 `SDL::NativeWindows`（`sdl_window` 放的不透明把手），經 adapter 建 GPU 裝置（照嵌進來的 shader 格式選 driver）、claim 視窗、建 2D、3D、只寫深度、全畫面 pass 的內建 shader、sampler、白貼圖，選 3D pass 與 `Depth` target 的深度格式。**裝置在 `Configure` 建**（總宣告第 10.2 節：替換 World 時 `Build` 重跑、不重建裝置）。沒有視窗（產品沒帶 `sdl_window`，或它建窗失敗）是啟動錯誤 `SDL-E2`；裝置建不起來是 `SDL-E3`（訊息帶 `VideoFailed`／`DeviceFailed`／`ClaimFailed`／`SetupFailed` 與 SDL 的文字）。放 `SDLGPUStatus` 進 Product 的 buffer。`OnDestroy` 放掉裝置——登記在 `sdl_window` 之後、所以先跑（反序），視窗還在時就放開它。
- **`Build`**（要畫的那個 World；在主執行緒）：登記把手 Resource `SDLGPUHandle`（`threadBound`，雜湊不寫）、把 `GPUSubmit` 掛在 `render_world` 的 `Submit` stage（`RenderWorldOptions::schedule`）。準備函式給每個 World 一個號碼：換了一個 World 在送出時，先放掉前一個 World 的貼圖與 mesh（新 World 的 id 從 1 重來）；替換失敗、舊 World 繼續畫時號碼沒變，什麼都不放。
- **`GPUSubmit`**（照 Bevy 的 `render_system`，`bevy_render/src/renderer/mod.rs:82`；Q7＝(a)、讀法 2）：讀渲染 World、只寫這一幀的收據 `RenderFrameReceipt`（SDL-5）；經 adapter 依序：套用這一幀的貼圖要求（建、局部更新、放掉）與 mesh 要求（`RenderMeshes::Pending()`：建、放掉）、上傳頂點與索引（一個 copy pass）、拿 swapchain 的畫面（`SDL_WaitAndAcquireGPUSwapchainTexture`，等垂直同步）、照 pass 的順序每個 `Pass2D`／`Pass3D` 一個 render pass（2D 每個 draw 設裁切、綁貼圖與 sampler）、送出。**render target**（渲染管線線 A1，[render 頁](render.zh-Hant.md)第 3 節）：每幀先拿視窗的畫面（縮小時沒有：render target 照樣畫，只是不送到視窗），再套用要求、照 `TextureTargetSize` 建或重建 target（`COLOR_TARGET | SAMPLER`；大小由資料側的 `RenderTextures` 解析，A2 第 14.11 節；不認得的 id 算被拒的要求），然後**一次**解析每個 pass 的目標（主目標或有貼圖的 target；其餘算一次沒有對應），上傳幾何與畫都照這張表。target 是自己的一張表（不和圖片的混在一起），每個有自己的深度貼圖；畫面跨幀保留，只有剛建或重建後的第一個 pass 清成透明黑。取樣自己這個目標的 draw 跳過、計數。**A2**：`Depth` target 用裝置的深度格式、用途 `DEPTH_STENCIL_TARGET | SAMPLER`；只寫深度的 pass 畫在它上面（沒有顏色 target），`Pass3D::depth` 指它時用它當深度（資料側要和目標一樣大，見下面 RP-3；GPU 上主目標的畫面可能比資料側的大小晚一幀，兩者不同時跳過那個 pass 的 draw、計數，不斷言——第 14.12 節 B-1）；`Depth` target 以外的 pass 畫在 `Depth` target 上、或反過來，是 Programmer Error（斷言）。**全畫面 pass**：以名字找 shader（嵌進來的，或 `GPUDevice::Create` 的 `more`：測試自己的表），pipeline 的 key 是（shader、目標格式），沒有頂點輸入、畫一個三角形（`SDL_DrawGPUPrimitives` 3 個頂點），`inputs` 照順序綁在 fragment sampler 0、1、…，`parameters` 推到 fragment uniform 0；找不到 shader、輸入沒有 GPU 貼圖或是自己的目標、pipeline 建不起來時不畫、計一次跳過的 draw。**RP-3**：Plugin 的 `GPUSubmit` 在交給 adapter 之前呼叫 `RequireRenderContracts`（`src/RenderContracts.hpp`）：它 `ReadsOf` 的 render target 要這一幀畫過、或在 `RenderCommands::retained` 裡且有畫面（`render` 的 `FirstUnwrittenRead`），3D pass 的深度 target 在資料側要和目標一樣大（`FirstDepthOfAnotherSize`），否則斷言（Programmer Error；只看資料，沒有 GPU 也能測：`sdl_gpu.contracts`、`sdl_gpu.unwritten`）。沿用的 target 什麼都不錄，畫面照上一幀。**收據**（A2 第 14.13 節）：adapter 在建好 target 的貼圖時記下套用的大小（SDL 拒絕的不記：資料側下一幀再要求）；pass 畫過的 target 先記在這一幀的帳上（`FrameDraws`，不含 SDL），command buffer 交給 GPU 之後（`SDL_SubmitGPUCommandBufferAndAcquireFence` 成功；A3 起每幀帶 fence）才在 adapter 自己那份與收據都記成有畫面；交不出去時兩份都不動（第 14.14 節；上傳失敗的那一幀不錄 pass，沒有畫過的），`Capture` 同樣寫。取樣一張 adapter 這裡沒有畫面、這一幀也沒畫的 target（程式畫過、GPU 沒拿到那一幀）的 draw 跳過、計數，不斷言（第 14.14 節）。已經有的 target 再要求建立（資料側對大小沒套用的 target 整個再要求）什麼都不做。貼圖的格式照 `TextureFormat` 對到 `R8G8B8A8_UNORM`／`R8G8B8A8_UNORM_SRGB`，建之前（只在一處）用 `SDL_GPUTextureSupportsFormat` 查格式與用途，不支援算被拒的要求（同一個大小只算一次）。
- **present mode**（VSync；`v8-arch-gyo2-window-mode-vsync`，[事前宣告](window-mode-vsync-declaration.zh-Hant.md)第 4.3 節；使用者 2026-11-25「VSync 要」）：`GPUSubmit` 每幀讀渲染 World 的 `render` `Presentation`（[render 頁](render.zh-Hant.md)第 2 節），和上次要的不同才經 adapter 改 swapchain（同 Bevy 的 `present_mode_changed`，`bevy_render/src/view/window/mod.rs:220`）。選法是不含 SDL 的純函式 `ChoosePresentMode`（`src/PresentModes.hpp`）：`AutoVsync`＝`VSYNC`；`AutoNoVsync`＝`IMMEDIATE`→`MAILBOX`→`VSYNC`，取視窗支援的第一個（`SDL_WindowSupportsGPUPresentMode`；Bevy 的退回鏈去掉 SDL_GPU 沒有的 FIFO-relaxed；master 只有 IMMEDIATE→VSYNC，`SdlGpuRenderDevice.cpp:308-312`）。`SDL_SetGPUSwapchainParameters` 用 claim 時的 composition（SDR）。實際的模式是 `SDLGPUStatus::ActivePresentMode()`（`Vsync`／`Immediate`／`Mailbox`）；改 swapchain 的次數是 `PresentModeSets()`（每次都等 GPU 閒下來：每個變化一次、不是每幀）；SDL 拒絕時計 `RefusedPresentModes()`、記 `LastFailure()`，**不重試**到 `Presentation` 再變。**新 World**（替換）的 `Presentation` 是預設 `AutoVsync`，swapchain 跟著改回 VSYNC：要保留就照 O-7 帶過去，或由遊戲的設定重寫（設計審查 S1 選 (a)：渲染 World 的資料是真相，同 `window` 的請求；`sdl_gpu` 不寫 `Presentation`，渲染 World 裡它只寫這一幀的收據，SDL-5）。**VSync 關的後果**：`GPUSubmit` 不再等螢幕，runner 的主迴圈也不等（`minFrameInterval` 預設 0）——幀率只受 CPU／GPU 限制、吃滿兩者；要上限的產品設 `SDLWindowOptions::minFrameInterval`（開窗時定）。本機 Metal：VSync 關＝`Immediate`。守衛：`sdl_gpu.present`（退回順序的表，label `cpu`）、`sdl_gpu.frames`「…VSync off and on again」（label `gpu`）；突變 `sg-present-*`、`gp-present-*`（含 `gp-present-every-frame`）。
- **3D pass**（`Pass3D`；目標的規則同 2D：0 或 render target，其他算進沒有對應的 pass）：**深度格式**在建裝置時選一次，照 D32_FLOAT、D24_UNORM、D16_UNORM 的順序取第一個 `SDL_GPUTextureSupportsFormat` 說能當深度目標**又能取樣**的（A2：`Depth` target 也用它；SDL 保證 D16 兩者都行，`SDL_gpu.h:703,747`）；深度貼圖每個目標一張，照目標的大小建、大小變了重建。每個 `Pass3D` 開頭把深度清成 1（每個 pass 有自己的深度，同 Bevy 的每台相機）；顏色照目標原樣載入，只有它是這一幀目標上的第一個 pass 時清成透明黑（同沒有 clear 的 `Pass2D`）。viewport 同 2D 的切法（`ViewportOf`）。每個 draw 推 vertex uniform `clipFromWorld × world`、fragment uniform 顏色、綁貼圖（`TextureId{}` 是白貼圖）。**pipeline 的 key** 是（目標格式、blend、depthTest、depthWrite、cull）：depthTest 時比較用 LESS_OR_EQUAL，不測時 ALWAYS 且不寫深度；cull Back 用 SDL 的 BACK、正面是 COUNTER_CLOCKWISE（畫面上逆時針為正面，8×8 的像素測試確認過）。**mesh**：每個 `MeshId` 一組自己的頂點與索引 buffer，在這一幀的 copy pass 上傳，放掉時等 copy pass 記完才放（同貼圖）；`mesh {}` 的 draw 用 pass 自己的頂點與索引，和 2D 的放在同一組每幀的動態 buffer（照 pass 順序接著放）。**計數**：建不起來的 mesh 要求（含上傳失敗那一幀建的）算 `RefusedMeshAsks()`；指到沒有 GPU 貼圖或 mesh 的 draw、pipeline 建不起來的 draw、深度貼圖建不起來時那個 pass 的 draw，都算跳過的 draw。
- **除錯標記與計時**（渲染管線線 A3，[渲染管線宣告](render-pipeline-declaration.zh-Hant.md)第 15 節；使用者 Q1 (a)）：adapter 錄每個有目標的 pass 時，在 render pass 之外包一個除錯群組（`SDL_PushGPUDebugGroup`／`SDL_PopGPUDebugGroup`），名字是不含 SDL 的純函式 `PassLabel`（`src/PassLabels.hpp`；種類的名字是 render 的 `PassKindName`）：「`#<在 RenderCommands::passes 的位置> <種類>[ <全畫面 pass 的 shader>] -> main`／`-> target <id>`」——資料裡沒有 pass 的名字（A3 不改資料形式），位置讓名字唯一、也就是「第幾個 pass」。RenderDoc、Xcode、PIX 擷取時看得到每個 pass 與它的 GPU 時間；D3D12 要 `WinPixEventRuntime.dll`（沒有時 SDL 在建裝置時警告一次、群組什麼都不做；不加這個依賴），Vulkan 要 loader 有 `VK_EXT_debug_utils`（擷取工具的層會給）。錄每個 pass 的 CPU 時間（`Engine::Time::Now()` 的牆上時間；是錄製的時間，不含產生 pass 的 System）。每一幀用 `SDL_SubmitGPUCommandBufferAndAcquireFence` 送出、不等它；`Submit` 的開頭與拿到 swapchain 之後、`Capture` 等完自己的 fence 之後，不阻塞地查（`SDL_QueryGPUFence`），看到做完就記「送出到看到做完」——**整幀 GPU 時間的上界**（fence 沒有時刻：含前面的幀的 GPU 工作與多晚去看；畫到視窗的幀粒度約一幀）。每個 pass 的 GPU 時間要 timestamp query（SDL 3.4 沒有，RC-RP5）。報在 `SDLGPUStatus::LastFrameTiming()`（`optional` 的複本，第一幀之前沒有；最近錄的一幀：`frame` 號碼、每個 pass 的名字、有沒有錄、CPU 時間、群組數）與 `LastGPUFrameTime()`（最近看到做完的一幀與上界）；只印、不判，平均與預算是作者的事。不寫進渲染 World（SDL-5 只讓後端寫收據）。守衛：`sdl_gpu.passlabels`（label `cpu`）、`sdl_gpu.timing`（label `gpu`）、`sdl_gpu.frames` 的 `[gp-frames-timing]`、`[gp-frames-gpu-time]`；突變 `tests/common/render/gpu_debug_mutations.json`。
- **錯誤怎麼出去**（總宣告第 10.2 節的題；照「告訴外面發生了什麼」，`errors-inform-not-prescribe`）：**沒有一種是 System 的錯誤**——System 回錯誤會讓 World 壞、Product 結束（O-11），而視窗縮小、某張貼圖建不起來都不是結束的理由。`SDLGPUStatus` 計數：送出的幀、跳過的幀（視窗沒給畫面：縮小、隱藏、被蓋住；貼圖要求照樣套用）、失敗的幀（SDL 拒絕某一步，`LastFailure()` 是最後一次的文字）、被拒的貼圖要求與 mesh 要求、因此跳過的 draw、目標沒有對應的 pass。SDL_GPU 沒有「裝置遺失」的事件或復原介面；裝置壞了會表現成每幀失敗，產品看 `FramesFailed()` 決定怎麼辦。
- **一個 Product 只有一個 World 列 `sdl_gpu`**（不加守衛）：一個視窗、一張貼圖表、一張 mesh 表。

### shader 工具

總宣告第 10.2 節 R2a 列「最小的 shader 管線（HLSL 在 `render`、host 工具在 `sdl_gpu`、ABI、共通管線找編譯器）」。照 master 的 `engine/render/shaders/pipeline`（三平台 CI 跑過）搬進 `sdl_gpu`、縮成最小：

- **流程**：建置時，`plugins/sdl_gpu/CMakeLists.txt` 以 ExternalProject 另建一個**主機**專案 `plugins/sdl_gpu/shader_tool/`（不進交叉編譯的圖；交叉編譯時用 `GYO_SHADER_TOOL_EXECUTABLE` 指定現成的工具），得到 `gyo_sdl_gpu_shader_tool`；它把 `render` 的 HLSL（`GYO::Render` 的 `GYO_RENDER_SHADER_DIR`）編成這個平台的格式、從 SPIR-V 讀出資源數，寫成一個 C++ 原始碼（建置目錄裡的 `generated/EmbeddedShaders.cpp`，定義 `EmbeddedShaders()`；`--function <名字>` 時定義那個名字的函式，A2：測試用它把自己的 shader `tests/shaders/*.hlsl` 編成另一張表 `TestShaders()`，交給 `GPUDevice::Create` 的 `more`，Plugin 不帶測試的 shader），`sdl_gpu` 把它編進去（Dear ImGui 的 SDL_GPU 後端也是把 shader 的 bytes 放在原始碼裡）。執行時不讀 shader 檔、不需要資產路徑。
- **格式**（`GYO_SHADER_FORMATS=AUTO`，同 master）：Windows＝DXIL＋SPIR-V（Direct3D 12 或 Vulkan）、Linux＝SPIR-V、macOS＝Metal library（MSL 經 Apple 的 `xcrun metal`／`metallib` 離線編譯；有 `CMAKE_OSX_DEPLOYMENT_TARGET` 時帶 `-mmacosx-version-min`）。
- **編譯器**：HLSL → SPIR-V、SPIR-V → DXIL 用 DXC，SPIR-V → MSL 用 SPIRV-Cross，都經 SDL_shadercross（照 SDL_GPU 的 register 慣例對位）。
- **P22 的讀法**（使用者 2026-11-24 選 (a)，經主會話，原話「照建議」）：使用者 11-13 的「shader 的編譯步驟留在共通的內容管線，共通管線從已選 plugin 的登記資料找到編譯器」指**遊戲內容的 shader**。內建的 2D shader 跟著 Plugin 走，由 `sdl_gpu` 自己編、自己嵌；第一個有遊戲 shader 的批（R2b 或遊戲移植）做「共通管線從已選 Plugin 的登記資料找到 `sdl_gpu` 的編譯工具」。比過的 (b)：照 master 把內建 shader 也當一個 bundle 走共通內容管線、執行時從資產目錄讀（每個用 `sdl_gpu` 的產品與測試都要組資產、帶路徑）。理由：內建 shader 是 Plugin 的一部分，產品不帶 `sdl_gpu` 就不需要它。出處：總宣告第 9.1 節「要改這一句就在 R2a 的宣告交使用者」。
- **相對 master 拿掉的**：bundle 的 spec 與 manifest、nlohmann_json、執行時讀 shader 檔、遊戲的 shader bundle（`gyo_add_shader_bundle`）——R2a 只有內建的一組；遊戲自己的 shader 在需要它的子批（R2b）再加。
- **os 守衛的地位**：工具在 `plugins/` 底下，是執行時的程式（os.md DI 2 的 R1～R4 全管，沒有豁免）：讀寫檔、開行程都經 SDL（`SDL_LoadFile`、`SDL_SaveFile`、`SDL_CreateProcess`），不用 fstream 與 `std::filesystem` 的檔案操作；它的執行檔在 Windows 套 UTF-8 的 code page（主機專案自己呼叫 `gyo_os_process_code_page`，同 master）。os.md DI 2 與 R4 字面裡的「the host shader tool」豁免已經沒有對象（P 的交接、總宣告第 16 節 (5)），R2a 拿掉（主會話 2026-11-24：字面跟上已經做的事，不必問使用者）。
- **測試**：主機專案的 `sdl_gpu.shader_tool.*`（label `shader`；L1 在 Linux 從主機建置目錄跑）：編得過 `render` 的 2D pass、編不過的 shader 與未知的格式都回 1、不寫輸出。主建置的 `sdl_gpu.shaders`（label `cpu`）比嵌進來的資源數與 `render` 的 ABI（RD-4；2D、3D、只寫深度與全畫面的 shader 都比）。 **SDL 的斷言不開對話框**（A2，[渲染管線宣告](render-pipeline-declaration.zh-Hant.md)第 14.9 節）：會啟動 SDL 的測試執行檔都連 `plugins/sdl/tests/adapters/sdl/` 的 `gyo_sdl_unattended_assertions`（`SDL_SetAssertionHandler`：印出、abort），跑產品執行檔的 ctest 帶 `SDL_ASSERT=abort`；`sdl.unattended_assertion` 在子行程強制一次斷言確認。

| 外部組件 | 版本（同 master 的 pin） | 來源與驗證 |
|---|---|---|
| DXC（Windows） | v1.9.2602 預建版 | `https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602/dxc_2026_02_20.zip`，SHA256 `a1e89031421cf3c1fca6627766ab3020ca4f962ac7e2caa7fab2b33a8436151e` |
| DXC（x86-64 Linux） | v1.9.2602 預建版 | `…/v1.9.2602/linux_dxc_2026_02_20.x86_64.tar.gz`，SHA256 `a1d3e3b5e1c5685b3eb27d5e8890e41d87df45def05112a2d6f1a63a931f7d60` |
| DXC（macOS） | libsdl-org fork commit `2c84a1c5ab7091608c97df6ba5ccf46e71c322eb`（含 submodule），從原始碼建 | git（官方沒有 Apple 的預建版） |
| SPIRV-Cross | commit `1a6169566c73d3da552748fc372fe2bbb856e46e` | codeload tarball，SHA256 `0f295b214b164e42a1d21537c8da7b44569806c16220dda9798558edfaacd11e` |
| SDL_shadercross | commit `e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba` | codeload tarball，SHA256 `342bb6a8e734745eb5951f25c87fa7aad62f46b3736def8681d9fa7ad046887f`（只編 `src/SDL_shadercross.c`，不跑上游會抓浮動依賴的安裝程式） |
| SDL | 3.4.0 | 同第 4 節的 release archive 與 SHA-256（主機專案自己的一份：關掉 video、audio、GPU，不上補丁） |

全部放在主機建置目錄（`GYO_SHADER_HOST_BUILD_DIR`，預設 `<build>/host-tools`）；`FETCHCONTENT_SOURCE_DIR_<名>`（`SDL3`、`GYO_SPIRVCROSS`、`GYO_SHADERCROSS`、`GYO_DXC`、`GYO_DXC_SOURCE`）指定現成的原始碼時照用。CI 的快取（`shader-tools-v4-…`）存整個主機建置目錄：key 先是 pin（`Dependencies.cmake` 的 hash）、再是工具自己的原始碼；只改工具時以 `restore-keys` 取回同一組 pin 的建置，DXC 等已經是新的、只重建工具；pin 改了才全部重建（macOS 從原始碼建 DXC 很久）。**下載這些是使用者同意的**（2026-11-24，經主會話，原話「照建議」：選 (A) 照 master、同意 DXC 兩個預建版、macOS 的 DXC 原始碼、SPIRV-Cross、SDL_shadercross 五項；比過的 (B) 是把編好的 shader 當產生檔放進 repo）。

## Design Invariants

- **SDL-1 SDL 只在 `plugins/sdl*` 的 adapter 目錄**：`SDL3/` 的標頭只被 `plugins/sdl*` 的 `src/adapters/sdl/` 與 `tests/adapters/sdl/` 底下的檔案 include（adapter 目錄裡可以有私有標頭，給 adapter 的幾個 `.cpp` 共用 SDL 的型別）；adapter 目錄外的檔案不 include adapter 目錄裡的任何檔案——adapter 給 System 用的介面標頭（我們的詞，不含 SDL）放在 adapter 目錄外，由 adapter 的 `.cpp` 實作；所以 SDL 不會經遞移的 include 漏到 System。System、runner、單位的 `Configure`／`Build`／`Check` 的程式裡沒有 SDL（建視窗、開裝置、claim 視窗也呼叫 adapter）；`engine/` 底下沒有 SDL。Plugin 之間的交接（視窗登錄表）用不透明的把手，型別定在 `sdl` 函式庫、不露 SDL 的型別（收窄使用者 11-13 的 (iii)，總宣告第 14 節核准清單第 6 件）。SDL3 對 `sdl*` 的 target 都是 PRIVATE 連結；`GYO::SDL` 本身不帶 SDL。守衛：原始碼掃描在子批 K 的 scenario 通過後立（總宣告第 9.1 節）；在那之前、以及 adapter 目錄外前置宣告 SDL 型別這種掃描判不了的，由每批的設計審查看。
- **SDL-2 沒有 SDL 也成立**：拿掉 `sdl*` 的 Plugin 與登記檔的列（與帶它們的產品），Engine、所有 Module、headless 的測試仍 configure、build、通過，SDL 不被下載、不被建。沒有產品選 `sdl*`、`GYO_PLUGINS` 也沒點名時，同樣不下載、不建。守衛：CI 的 core 步驟（L1 的 Linux 列、打包工作流程的 toolchain 列）——checkout 自己的建置與測試跑完之後，`build/ci/common/plugin_removal.py remove` 在 checkout 裡拿掉 `sdl*` 的列與目錄與帶它們的產品（暫存區刪除，經 git 列檔的測試照樣能跑），以 `core` preset、`GYO_PLUGINS=ALL` configure、build、ctest，`plugin_removal.py check` 確認 `gyo-build.json` 的 `plugins` 沒有 `sdl*`、`_deps` 沒有抓 SDL、CMakeCache 沒有 SDL 自己的選項（`SDL_*`，抓 vendored 或 ExternalProject 加進來的 SDL），最後 `restore` 把 checkout 還原成 HEAD。在 checkout 本身做、不在副本做：編譯快取的命中若跨兩棵原始碼樹，object 裡的 `std::source_location` 會是另一棵樹的路徑（L1 第二輪在 Windows 的 sccache＋MSVC 上碰到，`authority.authority_sources` 失敗）；同一個路徑編譯，快取的 key 也和以前一樣。本機要不動自己的樹時用 `copy`（HEAD 的 git 工作樹）；`cmake.PluginSelection`（整個 repo 什麼都沒選時不加任何 Plugin 目錄；configure 而已，沿用父建置抓好的來源、不連網）；`cmake.UnitRegistry`（選擇：依賴補齊、`ALL`、拒絕非 Plugin 的名字、產品登記）；`build.ci` 的 `test_plugin_removal.py`。突變：`docs/architecture/target/v2/scripts/sdl_mutations.json`。
- **SDL-3 一條時間線**：輸入事件的時刻是 `Time::TimePoint`；SDL 的時刻不出 adapter。每次泵取一組配對（依序讀 engine 鐘、SDL 鐘、engine 鐘，engine 取中點；`plugins/sdl/include/plugins/sdl/ClockPair.hpp`），事件的 SDL 時刻 `t` 換成 `min(中點 + (t − SDL 讀數), 送達時的 Now())`，`t == 0` 是送達時刻；差以 2^64 取模（master `SdlPlatform.cpp:645-674`、坑 8、9 修過的版本）。保證：(1) 不晚於送達；(2) 同一次泵裡非零且不遞減的 SDL 時刻換算後不遞減；(3) `t == 0` → 送達時刻。不保證全序與跨兩次泵的先後（誤差約 ±bracket/2）。送進混音器的時刻（A）也在這條時間線：`sdl_audio` 的回呼在每個裝置 block 開頭讀一次 `Time::Now()` 當 anchor（audio DI 3），一個 block 分幾次算時，後一次的 anchor 是前一次的加上已算的 frame 數換成的時間；World 時間的取樣由 world_time 的 `AdvanceTime` 讀時間基準（使用者 2026-11-23 選照 Bevy 留在 System，`v8-arch-gyo2-time-sampling-in-system`；runner 不取樣）。守衛：`sdl.clock`（假讀取函式、精確等式）、`sdl_window.events`「an event's time…」；突變 `sc-*`。音訊這一半的守衛：`sdl_audio.dummy` 的「maps a play from the anchor of the block that takes it」（只在開裝置時讀一次 anchor 的寫法，起播會晚上一秒；突變 `sdla-anchor-once`）；「分幾次算時後一次的 anchor」由審查守（dummy、disk 的 block 都小於一次的 4096 frame）。
- **SDL-4 主執行緒**：視窗、事件泵、swapchain 的取得與送出，以及 World 裡經 adapter 呼叫 SDL 的 System（輸入法、剪貼簿、改視窗、`GPUSubmit`）只在建視窗的執行緒；adapter 每個進入點斷言 `SDL_IsMainThread`（`sdl_window` 的 `WindowBackend`、`sdl_gpu` 的 `GPUDevice`），把手 Resource（`SDLWindowHandle`、`SDLGPUHandle`）標 `threadBound`（DI-KS 14）。由 runner 安排，不由 Kernel 的啟動檢查（P19）。守衛：adapter 的斷言；審查。
- **SDL-5 渲染只讀，只寫這一幀的收據**：`sdl_gpu` 的 `GPUSubmit` 讀渲染 World（`Res<RenderCommands>`、`Res<RenderTextures>`、`Res<RenderMeshes>`、`Res<Presentation>`、自己的把手）；唯一寫的是 render 定義的一個 Resource `RenderFrameReceipt`：後端每幀寫、資料側讀完就清的緩衝，記這一幀**實際**畫了的 render target 與**實際**套用的大小，一個 target 一筆。`render_world` 的 Cleanup 在同一幀讀進 `RenderTextures`（畫過的有畫面；套用的大小不再要求，沒套用的每幀再要求，直到套用）然後清空。不寫別的 Resource、不寫來源 World；它在 GPU 那一側做的事（貼圖、緩衝、送出）都在 World 外的 `Service`。使用者 2026-11-27 選 (a)：「其實（a）的解法不錯，那個 Resource 就是 cache 或者 buffer 一類的東西。」，名字「RenderFrameReceipt 好了，選 （a）」（HANDOFF `v8-arch-gyo2-sdl5-render-frame-receipt`，經主會話；比過的 (b)、(c) 沒選：收據是唯一的真相、修在根源；寫回只在渲染 World 之內，模擬 World 不動，K.3-6 不受影響；`FrameReceiptBuffer` 沒選：render 裡的 buffer 多指 GPU 的緩衝，同組的名字是 `RenderCommands`／`RenderTextures`）。渲染 World 讀來源 World 受 DI-KS 13 約束（全文只在 Kernel 頁；R2a 沒有 Extract）。守衛：`GPUSubmit` 寫的參數只有 `ResMut<RenderFrameReceipt>`（Kernel 的存取宣告；寫的參數會出現在 System 的簽名，審查看）。SDL-6 已撤，號碼不重用。
- **SDL-7 `sdl_window` 只翻譯、不持狀態**：整個 `sdl_window`（單位、runner、adapter、System）把 SDL 事件照第 5 節的對應表翻成 `input`、`window` 的 `Message`，把請求翻成 adapter 的呼叫。事件的方向不持有按鍵、焦點、組字的狀態，不合成對應表以外的 `Message`，不做失焦清鍵、去重、合併；鍵盤事件照帶 SDL 的 `mod`（和按鍵事件同一個來源，SDL 3.4 的平台後端自己對齊修飾鍵，總宣告第 6.6 節）；runner 不看事件的內容。**三個例外**：(1) 佇列滿了丟棄並計數（`InputEventsDropped`）；(2) 新建或替換的 World 的第一輪先寫目前的狀態（對應表末兩列）；(3) live frame 進入前先寫視窗目前的大小（`WindowResized`，每次向 SDL 查當下的值、不記上一次的大小；排隊的 resize 事件要等泵返回才處理，master 坑 3；對應表末列。使用者 2026-11-25「照建議 (a)」，HANDOFF `v8-arch-gyo2-sdl7-live-frame-resize`：這條要擋的是 `sdl_window` 自己記狀態、自己編事件，每次查當下的值不違反原意）。請求的方向記的「上次 SDL 接受的值」是自己輸出側的快取、不算副本；向 SDL 查目前的值（大小、縮放、剪貼簿、游標）不算持有。守衛：`sdl_window.events`——在 SDL 的 dummy 驅動用 `SDL_PushEvent` 餵一串事件，比寫出的 `Message` 序列與對應表（合成、丟事件〔沒計數〕、合併都會讓序列不同）、佇列上限設小時丟棄的數與計數；「持有但不輸出」的狀態與「runner 看事件內容」序列抓不到，由設計審查看。突變 `sw-*`。

## Replacement Criteria

總宣告第 12 節 RC-SDL 1～4（整條線）；各子批補自己的。

## 範囲の限界

- **`sdl_gpu`（R2a）**：GPU 的測試（label `gpu`：`sdl_gpu.pass2d`、`sdl_gpu.frames`）在 L1 只在 Linux 的 Lavapipe 跑；Windows（Direct3D 12）與 macOS（Metal）的 L1 列只建不跑，本機在 macOS（Metal）跑過，Windows 的實機沒跑。半透明混合的像素允許差 1（各 GPU 的捨入）；其他像素比確切的值。隱藏的視窗不給 swapchain 的畫面（每幀都跳過，照契約不是錯誤），所以 `sdl_gpu.frames` 與 `sdl_gpu.pass3d` 的幀測試用**顯示的**視窗、像 runner 一樣泵事件，要求**至少一幀送到視窗**（`[gp-frames-shown]`、`[g3-frames-shown]`；R2b 的 3D 段加——之前全部跳過也會通過，是 Object_Connect 移植的 Linux CI 抓到的）；視窗上的像素沒有比（swapchain 讀不回來；畫法和 `Capture` 共用同一段程式）。present mode 只有 `AutoVsync`／`AutoNoVsync`（`Fifo`、`Immediate`、`Mailbox` 的指定值沒有消費者，加值不破壞相容）；「VSync 關時幀率不被鎖」、Windows（D3D12）上 `AutoNoVsync` 實際取到哪一個，只能實機看（`sdl_gpu.frames` 只確認模式換了、幀照常送到）。HDR、多視窗、render to texture 沒有做。
- **`sdl_window`（W）**：SDL 的 dummy 驅動沒有 `SetWindowResizable`（`SDL_video.c:3126`），執行中改「能不能改大小」在 CI 看不到（建窗時不能改大小有測）；`cursorGrab` 在 dummy 驅動上同樣看不到效果（`ResizeWindow` 看得到：dummy 的 `SetWindowSize` 直接送 RESIZED，`SDL_nullvideo.c:64-66`）；「值不同才呼叫」沒有計呼叫次數的測試；輸入法的組字、候選窗只能實機驗（K）；三平台的修飾鍵卡住（Bevy 點名的三種情形）沒有實測，SDL 3.4 的平台後端自己對齊（總宣告第 6.6、15 節），實機場次時一起測。`WarpCursor` 在 dummy 驅動上不可觀測（只確認不被拒）。live frame（OS 的 modal 迴圈中畫圖）不在本批。**視窗模式**：dummy 驅動沒有真的全屏（SDL 核心改旗標、送 ENTER／LEAVE 與大小），macOS 的 Spaces 動畫與非同步離開、Windows 的無邊框、回窗口的位置、綠色按鈕、Windows 縮放下建窗就是全屏時回窗口的大小（dummy 的縮放是 1）、macOS 非同步進入的空檔（送全屏的同一幀送大小：全屏畫面不變、回窗口後才生效；dummy 是同步的，「要了全屏、旗標還沒變」這一支 CI 守不到）只能實機驗（這台 Mac＋Windows）。

- `remove` 要 checkout 的追蹤檔沒有改動（否則拒絕、請用 `copy`），`restore` 把所有追蹤檔放回 HEAD；中途失敗時 checkout 停在拿掉的狀態（CI 的那個 job 已經失敗）。`copy` 建的是 HEAD 的工作樹：本機還沒 commit 的改動不在裡面。
- **examples**：一個 example 一個目錄、各有 `CMakeLists.txt`，`examples/CMakeLists.txt` 照目錄加入、不寫名字；`plugin_removal.py` 拿掉 CMake 檔提到被拿掉的 Plugin 的 `examples/<名>`（W；`build.ci` 的 `test_plugin_removal.py`「an example directory…」）。
- 「帶 SDL 的產品」以產品目錄（與它的 `tests/`、`build/acceptance/`、`assets/` 底下同名目錄）的 CMake 檔裡有沒有 `sdl*` 的名字判（`#` 註解不算；多行的 `#[[ ]]` 註解只去掉第一行，後面的名字會多報、不會漏報）；產品與 example 目錄以外的 CMake 檔還提到被拿掉的名字時，`remove`／`copy` 失敗並列出檔案。
- SDL-2 的 CI 步驟只在 Linux（L1）與打包的 toolchain 列跑；Windows、macOS 的 L1 列建的是帶 SDL 的組態。
- 本機重用的建置目錄若留著以前抓的 `_deps/sdl3-*`，`check` 會報錯：用新的建置目錄。`copy` 會在本機 repo 登記一個 worktree（`git worktree remove` 拿掉）。
- master 留下的舊「component」機制（`GyoComponents.cmake` 的 `SDL_*`、`GYO_ENABLE_SDL3`，`GyoRuntimeDeployment.cmake` 的 `SDL3-shared`，`core` preset 的 `GYO_BUILD_SDL_*`）和 `plugins/sdl` 沒有接上；`GyoRuntimeDeployment` 在產品 configure 時查 `TARGET SDL3-shared`，早於 Plugin 加入，永遠是 false。留給重審產品登記機制的那批（Object_Connect 移植）處理，這批不動。
