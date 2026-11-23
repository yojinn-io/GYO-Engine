# SDL 的 Plugin（`plugins/sdl*`）

更新：2026-11-23（子批 W `v8-arch-gyo2-sdl-w`：`sdl` 函式庫的共用型別、第 5 節 `sdl_window`、SDL-3、SDL-4、SDL-7 與對應表、examples 的拿法；**最終文字在 W 的合併前核准**）；2026-11-23（新頁，子批 P `v8-arch-gyo2-sdl-p`）。由來：[SDL 總宣告](sdl-declaration.zh-Hant.md)修訂 21（使用者 2026-11-23 動工前核准「SDL 照建議核准」，HANDOFF `v8-arch-gyo2-sdl-declaration-approved`）第 9.1、10.2、12 節。本頁是 `plugins/sdl*` 的 owner 文件：子批 P 開頁、寫目錄的形、取得、選擇與 SDL-1、SDL-2；W（`sdl_window`）、A（`sdl_audio`）、R2a（`sdl_gpu`）各自補自己那一列與 SDL-3～SDL-7（草案在總宣告第 12 節）。

Owner：架構。

## 一個例子

Match（伺服器）不帶任何 `sdl*` 的列：configure 不加 `plugins/sdl`，SDL 不下載、不建，產品裡沒有 SDL。KeepTool 帶 `sdl_window`、`sdl_gpu`：建置層加這兩個目錄，也沿依賴加 `sdl`（函式庫），由 `sdl` 抓 SDL 3.4.0、上 GYO 的補丁。把整組 `sdl*` 從 repo 拿掉，Engine、所有 Module、headless 的測試照樣建得起、測得過（SDL-2）。

## 1. 列與依賴

一列＝一個目錄＝一個 target＝一個單位（`config/plugins.csv`）。依賴只有 `plugins/sdl*` → `engine/modules/*` 與 `sdl` 函式庫；Engine 不 include `plugins/`，Module 不依賴 Plugin（登記檔讀取時的 FATAL_ERROR）。

| 列 | 角色 | 誰建 | 現況 |
|---|---|---|---|
| `sdl` | 函式庫（`library_only`）：抓 SDL3、上補丁；`sdl*` 之間共用的、我們的詞寫的型別：不透明的視窗把手 `NativeWindows`、時鐘配對 `ClockPair`（`plugins/sdl/include/plugins/sdl/`） | P、W | 有（target `GYO::SDL`，連 `GYO::Time`，不連 SDL） |
| `sdl_window` | 來源側：Runner、Window、InputEvents、Ime、剪貼簿（第 5 節） | W | 有（target `GYO::SDLWindow`） |
| `sdl_gpu` | 渲染側：Gpu、GpuSubmit | R2a | 還沒有 |
| `sdl_audio` | `Configure`：AudioOutput | A | 還沒有 |

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

- adapter 給 System 用的介面標頭放在 `src/adapters/sdl/` 外（例：`plugins/sdl_gpu/src/GpuDevice.hpp`），由 adapter 的 `.cpp` 實作（SDL-1）。
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

改 `plugins/sdl/CMakeLists.txt` 的 URL 與 `URL_HASH`、本節的表、`plugins/sdl/tests/adapters/sdl/SDLBuildTests.cpp` 的版本；每個補丁重新上、上不去時照上表的移除條件。下載新版本要使用者同意（版本、來源、大小、SHA-256）。

## 5. `sdl_window`

`plugins/sdl_window/`（`GYO::Plugins::SDLWindow`，登記單位 `SDLWindowPlugin`，標頭 `plugins/sdl_window/SDLWindowPlugin.hpp`）。依賴 `sdl;input;window`。SDL 只在 `src/adapters/sdl/WindowBackend.cpp` 與私有的 `Translate.hpp`；給單位、System、runner 用的介面是 `src/WindowBackend.hpp`（我們的詞）。

- **`Configure`**（產品一次）：讀 `window` 的 `WindowSettings` 與自己的 `SDLWindowOptions`，經 adapter 起 SDL 的 video、建窗；放進 Product 的 buffer：`SDL::NativeWindows`（給 `sdl_gpu`）、`SDLWindowPump`（給自己的 runner 用的產品）；`installRunner`（預設 true）時裝 runner。建不了窗是啟動錯誤 `SDL-E1`（沒有 World 被建）。`OnDestroy`（所有 World 之後）關窗、停 video。
- **`Build`**（列它的那個 World）：登記把手 Resource `SDLWindowHandle`（`threadBound`，雜湊不寫）；`SDLWindowWriteInput`、`SDLWindowWriteWindow` 掛 `Main`／`PreUpdate`，各自 `before` `InputSystems`、`WindowSystems`（使用者 `-sdl-input-writer-preupdate`）；`SDLWindowApplyRequests` 掛 `Main`／`Last`。準備函式在準備 World 的執行緒放把手（新 World 的第一輪先寫目前的狀態）。骨架關掉的 World 用 `SDLWindowWorldOptions`。
- **runner**（照 Bevy 的 winit runner）：每幀「看一次 os 的關機請求 → 經 adapter 泵事件 → `RunFrame`」，`WindowRequest.close` 被套用後跑完那一幀就結束、回 Success；`RunFrame` 失敗回 Failure（文字同預設 runner）。`minFrameInterval` 大於 0 時，一幀太快就經 adapter 等 OS 的事件到期滿（還沒有畫面的程式不空轉；有了 `sdl_gpu` 由送出畫面等垂直同步）。runner 不看事件的內容。
- **佇列**（`src/EventQueue.hpp`，不公開）：元素就是 `input`、`window` 的 `Message`；一個 SDL 事件的 `Message` 全進或全不進；輸入與視窗各一個上限（`queueCapacity` 預設 1024、`windowQueueCapacity` 預設 256；都不大於 Kernel 的 `Message` 上限，一幀的寫入不會溢出 Kernel 的緩衝；滑鼠灌爆輸入那份也擠不掉失焦、關閉）；os 的關機請求進了佇列才算看過（滿了下一幀再試）；滿了丟掉並計數，輸入那半的寫者寫 `InputEventsDropped{count}`（`input` 因此放開所有按鍵與滑鼠鍵）。`SDLWindowPump::DroppedEvents()` 是累計。
- **請求**（`Last`）：`TextInputRequest`、`WindowRequest` 的欄位和上次 SDL 接受的不同才呼叫 adapter（同 Bevy 的 `CachedWindow`）；被拒時不記、下一幀重試、計數（`SDLWindowPump::RefusedRequests()`、`LastRefusal()`）——**不當 System 的錯誤**（回錯誤會讓 World 壞、Product 結束，O-11；照「告訴外面發生了什麼」）。`WarpCursor`、`CopyToClipboard` 是 `Message`，每筆呼叫一次。`close` 最後套用。
- **座標**：SDL 的視窗座標乘「像素密度／顯示縮放」得到邏輯像素（Windows 150 % 是 1/1.5；Retina 是 1）；請求反過來除。建窗時 SDL 的大小是視窗座標，所以 Windows 縮放下照邏輯大小再設一次。
- **SDL 的 hint**（整個 process；設了、讀完就還原）：`SDL_HINT_NO_SIGNAL_HANDLERS=1`（訊號歸 os；EVENTS 初始化時讀，VIDEO 隱含 EVENTS）與 `drawComposition` 時的 `SDL_HINT_IME_IMPLEMENTED_UI=composition`（Windows 上程式自己畫組字；起 video 時讀一次；ImGui 不畫組字，K 定）在 `SDL_InitSubSystem(VIDEO)` 前設、之後 `SDL_ResetHint`；`SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE=0`（關窗是遊戲的要求、不是 SDL 的 quit）在視窗活著時都要，關窗、`SDL_QuitSubSystem(VIDEO)` 之後還原。只管 VIDEO、不呼叫 `SDL_Quit`（A 的「SDL 的全域狀態」同一套規則）。
- **時刻的順序**：同一次泵裡，一個事件的時刻不晚於前一個時就改成前一個加 1 ns（`OrderTimes`）——SDL 有的事件蓋 OS 的時刻、有的在入列時蓋（`SDL_events.c:1802`），送達的順序才是真的；`input` 用時刻分兩種 `Message` 的先後。
- **游標的抓法**（`WindowRequest.cursorGrab`）：`Locked`＝`SDL_SetWindowRelativeMouseMode`（`MouseMotion` 是裝置的量，G4）、`Confined`＝`SDL_SetWindowMouseGrab`。
- **一個 Product 只有一個 World 列 `sdl_window`**（不加守衛）：佇列只有一份，第二個 World 的寫者拿不到；兩個 World 的請求各讀一次，狀態式的請求後跑的蓋掉先跑的。其他 World 的輸入由作者轉（總宣告第 9.2 節第 2 條）。

### SDL-7 的對應表

每種 SDL 事件對應 0 個或固定的幾個 `Message`（`Translate`，`src/adapters/sdl/Translate.hpp`）。視窗事件的視窗 id 不是自己的視窗 → 0 個。時刻照 SDL-3 換算。

| SDL 事件 | `Message` |
|---|---|
| `KEY_DOWN`／`KEY_UP` | `input` `KeyboardInput`（scancode → `KeyCode`〔同為 USB HID 的值，`input` 沒命名的 → `Unidentified`〕、`down`、`repeat`、無修飾時的字〔`SDL_GetKeyFromScancode`〕、SDL 給的 `mod`） |
| `TEXT_EDITING` | `input` `TextComposition`（`start`、`length` 照 SDL 給的，-1＝沒給） |
| `TEXT_INPUT` | `input` `TextCommitted` |
| `MOUSE_MOTION` | `input` `CursorMoved`（`x`、`y` 換成邏輯像素）＋`MouseMotion`（`xrel`、`yrel` 照 SDL 給的），兩個 |
| `MOUSE_BUTTON_DOWN`／`UP` | `input` `MouseButtonInput`（左、中、右、X1→Back、X2→Forward；其他鍵 → 0 個） |
| `MOUSE_WHEEL` | `input` `MouseWheel`（`x`、`y` 照 SDL 給的，macOS 的自然捲動不反過來，同 Bevy） |
| `WINDOW_MOUSE_ENTER`／`LEAVE` | `input` `CursorEntered`／`CursorLeft` |
| `WINDOW_FOCUS_GAINED` | `window` `WindowFocused{true}` |
| `WINDOW_FOCUS_LOST` | `window` `WindowFocused{false}`＋`input` `KeyboardFocusLost`，兩個 |
| `WINDOW_PIXEL_SIZE_CHANGED` | `window` `WindowResized`（事件的像素大小、向 SDL 查的顯示縮放） |
| `WINDOW_DISPLAY_SCALE_CHANGED` | `window` `WindowResized`（向 SDL 查的像素大小與縮放） |
| `WINDOW_CLOSE_REQUESTED` | `window` `WindowCloseRequested{Window}` |
| `QUIT` | `window` `WindowCloseRequested{Application}` |
| `CLIPBOARD_UPDATE` | `window` `ClipboardChanged`（向 SDL 讀的文字，超過 `clipboardLimit` 截在 UTF-8 的邊界） |
| 其他 | 0 個 |
| （不是 SDL 事件）runner 第一次看到 os 的關機請求 | `window` `WindowCloseRequested{Shutdown}` |
| （例外 (2)）新 World 的第一輪 | `window` `WindowResized`、`WindowFocused`、`ClipboardChanged`（目前的值）；游標在視窗內時 `input` `CursorEntered`＋`CursorMoved` |

`WINDOW_RESIZED`（視窗座標的大小）不寫：邏輯大小由像素大小與縮放算出，和 `PIXEL_SIZE_CHANGED` 重複。

## Design Invariants

- **SDL-1 SDL 只在 `plugins/sdl*` 的 adapter 目錄**：`SDL3/` 的標頭只被 `plugins/sdl*` 的 `src/adapters/sdl/` 與 `tests/adapters/sdl/` 底下的檔案 include（adapter 目錄裡可以有私有標頭，給 adapter 的幾個 `.cpp` 共用 SDL 的型別）；adapter 目錄外的檔案不 include adapter 目錄裡的任何檔案——adapter 給 System 用的介面標頭（我們的詞，不含 SDL）放在 adapter 目錄外，由 adapter 的 `.cpp` 實作；所以 SDL 不會經遞移的 include 漏到 System。System、runner、單位的 `Configure`／`Build`／`Check` 的程式裡沒有 SDL（建視窗、開裝置、claim 視窗也呼叫 adapter）；`engine/` 底下沒有 SDL。Plugin 之間的交接（視窗登錄表）用不透明的把手，型別定在 `sdl` 函式庫、不露 SDL 的型別（收窄使用者 11-13 的 (iii)，總宣告第 14 節核准清單第 6 件）。SDL3 對 `sdl*` 的 target 都是 PRIVATE 連結；`GYO::SDL` 本身不帶 SDL。守衛：原始碼掃描在子批 K 的 scenario 通過後立（總宣告第 9.1 節）；在那之前、以及 adapter 目錄外前置宣告 SDL 型別這種掃描判不了的，由每批的設計審查看。
- **SDL-2 沒有 SDL 也成立**：拿掉 `sdl*` 的 Plugin 與登記檔的列（與帶它們的產品），Engine、所有 Module、headless 的測試仍 configure、build、通過，SDL 不被下載、不被建。沒有產品選 `sdl*`、`GYO_PLUGINS` 也沒點名時，同樣不下載、不建。守衛：CI 的 core 步驟（L1 的 Linux 列、打包工作流程的 toolchain 列）——checkout 自己的建置與測試跑完之後，`build/ci/common/plugin_removal.py remove` 在 checkout 裡拿掉 `sdl*` 的列與目錄與帶它們的產品（暫存區刪除，經 git 列檔的測試照樣能跑），以 `core` preset、`GYO_PLUGINS=ALL` configure、build、ctest，`plugin_removal.py check` 確認 `gyo-build.json` 的 `plugins` 沒有 `sdl*`、`_deps` 沒有抓 SDL、CMakeCache 沒有 SDL 自己的選項（`SDL_*`，抓 vendored 或 ExternalProject 加進來的 SDL），最後 `restore` 把 checkout 還原成 HEAD。在 checkout 本身做、不在副本做：編譯快取的命中若跨兩棵原始碼樹，object 裡的 `std::source_location` 會是另一棵樹的路徑（L1 第二輪在 Windows 的 sccache＋MSVC 上碰到，`authority.authority_sources` 失敗）；同一個路徑編譯，快取的 key 也和以前一樣。本機要不動自己的樹時用 `copy`（HEAD 的 git 工作樹）；`cmake.PluginSelection`（整個 repo 什麼都沒選時不加任何 Plugin 目錄；configure 而已，沿用父建置抓好的來源、不連網）；`cmake.UnitRegistry`（選擇：依賴補齊、`ALL`、拒絕非 Plugin 的名字、產品登記）；`build.ci` 的 `test_plugin_removal.py`。突變：`docs/architecture/target/v2/scripts/sdl_mutations.json`。
- **SDL-3 一條時間線**：輸入事件的時刻是 `Time::TimePoint`；SDL 的時刻不出 adapter。每次泵取一組配對（依序讀 engine 鐘、SDL 鐘、engine 鐘，engine 取中點；`plugins/sdl/include/plugins/sdl/ClockPair.hpp`），事件的 SDL 時刻 `t` 換成 `min(中點 + (t − SDL 讀數), 送達時的 Now())`，`t == 0` 是送達時刻；差以 2^64 取模（master `SdlPlatform.cpp:645-674`、坑 8、9 修過的版本）。保證：(1) 不晚於送達；(2) 同一次泵裡非零且不遞減的 SDL 時刻換算後不遞減；(3) `t == 0` → 送達時刻。不保證全序與跨兩次泵的先後（誤差約 ±bracket/2）。送進混音器的時刻、runner 的取樣也在這條時間線（A、時間取樣那一題）。守衛：`sdl.clock`（假讀取函式、精確等式）、`sdl_window.events`「an event's time…」；突變 `sc-*`。
- **SDL-4 主執行緒**：視窗、事件泵，以及 World 裡經 adapter 呼叫 SDL 的 System（輸入法、剪貼簿、改視窗；R2a 的 `GpuSubmit`）只在建視窗的執行緒；adapter 每個進入點斷言 `SDL_IsMainThread`，把手 Resource 標 `threadBound`（DI-KS 14）。由 runner 安排，不由 Kernel 的啟動檢查（P19）。守衛：adapter 的斷言；審查。
- SDL-5（渲染只讀）由 R2a 寫進本頁。SDL-6 已撤，號碼不重用。
- **SDL-7 `sdl_window` 只翻譯、不持狀態**：整個 `sdl_window`（單位、runner、adapter、System）把 SDL 事件照第 5 節的對應表翻成 `input`、`window` 的 `Message`，把請求翻成 adapter 的呼叫。事件的方向不持有按鍵、焦點、組字的狀態，不合成對應表以外的 `Message`，不做失焦清鍵、去重、合併；鍵盤事件照帶 SDL 的 `mod`（和按鍵事件同一個來源，SDL 3.4 的平台後端自己對齊修飾鍵，總宣告第 6.6 節）；runner 不看事件的內容。**兩個例外**：(1) 佇列滿了丟棄並計數（`InputEventsDropped`）；(2) 新建或替換的 World 的第一輪先寫目前的狀態（對應表末兩列）。請求的方向記的「上次 SDL 接受的值」是自己輸出側的快取、不算副本；向 SDL 查目前的值（大小、縮放、剪貼簿、游標）不算持有。守衛：`sdl_window.events`——在 SDL 的 dummy 驅動用 `SDL_PushEvent` 餵一串事件，比寫出的 `Message` 序列與對應表（合成、丟事件〔沒計數〕、合併都會讓序列不同）、佇列上限設小時丟棄的數與計數；「持有但不輸出」的狀態與「runner 看事件內容」序列抓不到，由設計審查看。突變 `sw-*`。

## Replacement Criteria

總宣告第 12 節 RC-SDL 1～4（整條線）；各子批補自己的。

## 範囲の限界

- **`sdl_window`（W）**：SDL 的 dummy 驅動沒有 `SetWindowResizable`（`SDL_video.c:3126`），執行中改「能不能改大小」在 CI 看不到（建窗時不能改大小有測）；`ResizeWindow`、`cursorGrab` 在 dummy 驅動上同樣看不到效果；「值不同才呼叫」沒有計呼叫次數的測試；輸入法的組字、候選窗只能實機驗（K）；三平台的修飾鍵卡住（Bevy 點名的三種情形）沒有實測，SDL 3.4 的平台後端自己對齊（總宣告第 6.6、15 節），實機場次時一起測。`WarpCursor` 在 dummy 驅動上不可觀測（只確認不被拒）。live frame（OS 的 modal 迴圈中畫圖）不在本批。

- `remove` 要 checkout 的追蹤檔沒有改動（否則拒絕、請用 `copy`），`restore` 把所有追蹤檔放回 HEAD；中途失敗時 checkout 停在拿掉的狀態（CI 的那個 job 已經失敗）。`copy` 建的是 HEAD 的工作樹：本機還沒 commit 的改動不在裡面。
- **examples**：一個 example 一個目錄、各有 `CMakeLists.txt`，`examples/CMakeLists.txt` 照目錄加入、不寫名字；`plugin_removal.py` 拿掉 CMake 檔提到被拿掉的 Plugin 的 `examples/<名>`（W；`build.ci` 的 `test_plugin_removal.py`「an example directory…」）。
- 「帶 SDL 的產品」以產品目錄（與它的 `tests/`、`build/acceptance/`、`assets/` 底下同名目錄）的 CMake 檔裡有沒有 `sdl*` 的名字判（`#` 註解不算；多行的 `#[[ ]]` 註解只去掉第一行，後面的名字會多報、不會漏報）；產品與 example 目錄以外的 CMake 檔還提到被拿掉的名字時，`remove`／`copy` 失敗並列出檔案。
- SDL-2 的 CI 步驟只在 Linux（L1）與打包的 toolchain 列跑；Windows、macOS 的 L1 列建的是帶 SDL 的組態。
- 本機重用的建置目錄若留著以前抓的 `_deps/sdl3-*`，`check` 會報錯：用新的建置目錄。`copy` 會在本機 repo 登記一個 worktree（`git worktree remove` 拿掉）。
- master 留下的舊「component」機制（`GyoComponents.cmake` 的 `SDL_*`、`GYO_ENABLE_SDL3`，`GyoRuntimeDeployment.cmake` 的 `SDL3-shared`，`core` preset 的 `GYO_BUILD_SDL_*`）和 `plugins/sdl` 沒有接上；`GyoRuntimeDeployment` 在產品 configure 時查 `TARGET SDL3-shared`，早於 Plugin 加入，永遠是 false。留給重審產品登記機制的那批（Object_Connect 移植）處理，這批不動。
