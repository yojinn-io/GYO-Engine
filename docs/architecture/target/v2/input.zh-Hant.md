# 輸入（`engine/modules/input`）

更新：2026-11-24（`TextComposition`／`TextInput` 的組字游標改成位元組、欄位改名 `cursorByte`／`selectionBytes`，HANDOFF `v8-arch-gyo2-sdl-u-three-judgments` 第 3 件）；2026-11-23（新頁，SDL 線子批 W `v8-arch-gyo2-sdl-w`）。由來：[SDL 總宣告](sdl-declaration.zh-Hant.md)修訂 21（使用者 2026-11-23 動工前核准「SDL 照建議核准」）第 3.3、6.2、6.3、10.2 節的 W 列。照新 AGENTS（#296）不另寫事前宣告；設計取捨寫在本頁與 PR 說明。**Design Invariants 的最終文字在合併前核准定**。

Owner：架構。程式：`engine/modules/input/`（`GYO::Engine::Input`，target `GYO::Input`，登記單位 `InputModule`）。行為測試：`tests/common/input/`（ctest `input.module`）；後端寫進來的路徑在 `plugins/sdl_window/tests/`（ctest `sdl_window.events`）。

## 一個例子

Object_Connect 的玩家按住滑鼠左鍵拖線。`sdl_window` 在這一幀的 `Main`／`PreUpdate` 把 SDL 的按鍵事件寫成 `MouseButtonInput`、把移動寫成 `CursorMoved` 與 `MouseMotion`（排在 `InputSystems` 之前）。`input` 的 System 在同一個 stage 更新 `ButtonInput<MouseButton>`、`CursorPosition`、`AccumulatedMouseMotion`。遊戲的 System 在 `Update` 讀：左鍵 `Pressed`、游標在哪。要「游標被邊擋住仍有往外推的量」（G4）時，遊戲把 `window` 的 `WindowRequest.cursorGrab` 設成 `Locked`（系統游標藏起來、固定不動，SDL 的相對模式）、自己畫游標，從 `AccumulatedMouseMotion` 推位置——不鎖時 SDL 的相對量是絕對座標相減（`SDL_mouse.c:768`），游標停在邊上就是 0。玩家切到別的視窗：SDL 送失焦，`input` 收到 `KeyboardFocusLost`、放開所有按鍵；遊戲讀 `window` 的 `WindowState.focused` 決定要不要暫停。

## 1. 位置與依賴

```text
GYO::Base、GYO::Math、GYO::Time ◄── GYO::Kernel ◄── GYO::Orchestration ◄── GYO::Input
```

- 登記檔一列：`library_only=no`、`default=yes`、沒有 `depends`（不依賴 `window`、不依賴任何 Plugin）。沒有後端時只是空的資料（同 Bevy 的 `InputPlugin` 無條件在 `DefaultPlugins`，`bevy_internal/src/default_plugins.rs:14`）。
- 標頭：`Input.hpp`（型別、`Message`、狀態、請求、雜湊）、`InputModule.hpp`（登記單位、`InputOptions`、公開 set 名 `kInputSet`）。

## 2. 用法與後果

- **`Message`**（寫的一方：`sdl_window`、測試、網路注入）：`KeyboardInput`（實體鍵 `KeyCode`＝USB HID 的值、按下／放開、重複、無修飾時這個鍵在目前配置給的字、修飾鍵）、`KeyboardFocusLost`、`InputEventsDropped`（後端丟了幾個事件）、`MouseButtonInput`、`CursorMoved`（視窗的邏輯像素、左上原點、+Y 向下）、`MouseMotion`（相對移動量；`Locked` 時是裝置的量，G4；預設是後端的單位、不換成邏輯像素；後端可以讓產品選邏輯像素——`sdl_window` 的 `SDLWindowOptions::mouseMotion`＝`LogicalPixels` 時是邏輯像素、`Locked` 時也帶 OS 的指標速度與加速。前提：單位只有後端知道，遊戲換算不了，所以由 Engine 給選項，2026-11-24 `v8-arch-gyo2-relative-mouse-system-scale`）、`CursorEntered`／`CursorLeft`（G5）、`MouseWheel`（以「行」為單位）、`TextComposition`（組字中的字串〔UTF-8〕；選取的起點 `cursorByte` 與長度 `selectionBytes` 是字串的**位元組**、落在 code point 的邊界上，後端在自己的邊界換算——單位同 winit 給 Bevy 的（位元組，`bevy_text/src/text_edit.rs:21-25`；形狀是起點＋長度）；起點、長度各自可以是 -1＝沒給（沒給起點時長度也算沒給）。2026-11-24 由「照後端給的單位」改成位元組，HANDOFF `v8-arch-gyo2-sdl-u-three-judgments` 第 3 件）、`TextCommitted`（打出的字或確定的組字，UTF-8）。每個都帶發生的時刻（`Time::TimePoint`，SDL-3）。
- **寫的一方**排在 `Main`／`PreUpdate`、`before` `kInputSet`（使用者 `v8-arch-gyo2-sdl-input-writer-preupdate`）。不寫 `before` 不報錯：Kernel 對沒寫先後的照登記序排（Kernel 不對作者承諾這個順序，所以不釘成測試），狀態可能**安靜地晚一幀**。**讀狀態的一方**排在 `kInputSet` 之後（`Update`，或同 stage 寫 `after`）；`First`、`PreUpdate` 裡沒排在後面的 System 可能讀到上一輪的狀態（O-12 改後的「緩的面」(b)）。
- **狀態**（只由 `input` 的 System 寫，IN-2）：`ButtonInput<KeyCode>`、`ButtonInput<MouseButton>`（`Pressed`／`JustPressed`／`JustReleased`，同 Bevy 的 `ButtonInput<T>`）、`AccumulatedMouseMotion`、`AccumulatedMouseScroll`（這一幀的總和，沒有就是 0）、`CursorPosition`（離開視窗時 nullopt）、`TextInput`（組字中的內容）。五個 System 都在 `kInputSet`，照 Bevy 分開（`keyboard_input_system` 等，`bevy_input/src/lib.rs:117-165`）。
- **按鍵的規則**（IN-3）：按住的鍵再按（含 OS 的重複）不是新的按下；沒按住的鍵的重複（失焦時按著、回來還按著）照 Bevy 算按下（`keyboard.rs:187-190`）；沒按住的鍵放開什麼都不變（SDL 失焦時先送失焦、再補 key-up，`SDL_keyboard.c:354-356`）；這一幀有 `KeyboardFocusLost` → 放開所有按鍵（同 Bevy `keyboard.rs:199-203`，**滑鼠鍵不放**，同 Bevy）；有 `InputEventsDropped` → 放開所有按鍵與滑鼠鍵（丟了什麼不知道，可能是 key-up）。
- **跨型別的先後**：每種 `Message` 各自依序讀，兩種之間的先後看不到。失焦照 Bevy 在處理完這一幀的按鍵之後才放開（同一幀「失焦→回焦→再按」的鍵也會被放開，下一次按下才回來；這是 Bevy 也有的取捨）。游標的離開與移動、組字與確定用**時刻**比：離開（確定）的時刻不早於最後的移動（組字）就算離開（清空）。這要求後端給的時刻照送達的順序：`sdl_window` 在同一次泵裡讓時刻嚴格遞增（SDL 有的事件蓋 OS 的時刻、有的在入列時蓋，`SDL_events.c:1802`，兩種之間會倒過來）。
- **游標在視窗外的位置**：按著滑鼠鍵拖出視窗時，OS 讓視窗捕獲滑鼠，SDL 不送離開、`CursorMoved` 照送，位置可以是負的或超過視窗大小；放開鍵之後才離開。要視窗內的位置就自己夾（Bevy 在 `Window::cursor_position` 濾掉，`bevy_window/src/window.rs:631-646`；`input` 不知道視窗大小）。
- **`Unidentified` 的鍵**合成同一個（例：JIS 的 Yen、変換）：按住兩個、放開一個，就算兩個都放開。有點名的 Feature 時照 Bevy 的 `NativeKeyCode` 加原生碼（RC-IN 2）。
- **`TextInputRequest`**（請求，由 `world_ui`、ImGui 寫；後端在 `Main`／`Last` 讀、晚一幀生效）：要不要文字輸入、文字框的位置與大小（候選窗放在旁邊）、游標在框內的位置。**兩個寫者在同一個 World 時，後跑的蓋掉先跑的**——它是一般的 Resource，作者排先後（同 Bevy 的 `Window.ime_enabled` 也是誰最後寫算誰）。
- **新的 World**：準備函式放空的狀態（帶過去的值 O-7 留著）；後端在那個 World 的第一輪先寫目前的游標位置（SDL-7 例外 (2)）。換 World 時按住的鍵不補（OS 不會再送按下；它的放開是「沒按住的鍵放開」，什麼都不變）。
- **生成世界期間**（O-14）：照常寫、照常更新——輸入不因載入而丟（按住的鍵在載入後仍是按住的）；載入畫面要不要理會輸入由作者決定。
- **雜湊什麼都不寫**（全部型別）：OS 給的資料每台機器、每次執行都不同，不是兩次執行要一致的狀態（同 `CameraTargetSizes`，CM-5）。Kernel 的 `World::Summary` 仍然算每種 `Message` 的筆數與游標（`-kernel-summary-messages`），比摘要的測試（回滾、讀檔的等價）要在相同的輸入序列下比，或在比之前讓讀者讀完。
- **多個 World**：只有列了 `sdl_window` 的那個 World 直接收到 OS 的輸入。其他 World 要輸入時，作者在那個 World 讀 `Message`、轉到另一個 World、在那裡用 `before` `kInputSet` 的 System 寫同一種 `Message`（總宣告第 9.2 節第 2 條）；跨執行緒時經 O-13 (3) 的有鎖物件（PvP Client 移植那批）。同一條執行緒上的第二個 World 不在 O-13 的字面裡，已記在批次表的後續。
- **骨架關掉的 World**：用 `InputOptions{schedule, stage}` 指名。
- **不做的**：按鍵對應到動作（作者或之後的 Module；Bevy 本體也沒有，在生態的 leafwing）；手把；觸控。

## 3. 借自 Bevy 與偏離的地方

| 項 | Bevy | GYO | 理由 |
|---|---|---|---|
| `Message` 與雙軌（狀態＋依序事件） | `bevy_input` 的 `KeyboardInput`、`ButtonInput<T>`、`Accumulated*` | 同 | 照 Bevy |
| 公開 set、`PreUpdate` | `InputSystems`（`bevy_input/src/lib.rs:106`） | `kInputSet`、`Main`／`PreUpdate` | 照 Bevy（使用者 `-sdl-input-writer-preupdate`） |
| 游標的位置、進出 | 在 `bevy_window`（`CursorMoved` 等，`event.rs:192`、`:236`、`:253`；位置在 `Window::cursor_position`） | 在 `input` | GYO 的 `window` 資料不含輸入（總宣告 L-4），`world_ui` 只依賴 `input` |
| 輸入法 | `Ime` 事件在 `bevy_window`、`ime_enabled` 在 `Window` | `TextComposition`／`TextCommitted`、`TextInputRequest` 在 `input` | 使用者 P27 定的形（總宣告第 6.6 節） |
| 邏輯鍵 | `Key`（winit 的 logical key） | `KeyboardInput::character`（無修飾的字） | 夠快捷鍵用；打字走 `TextCommitted` |
| 時刻 | 不保留 | 每個 `Message` 帶 `TimePoint` | 使用者 P11（網路的預測要用） |
| 丟事件 | 沒有 | `InputEventsDropped` → 全部放開 | 後端的佇列有上限（SDL-7 例外 (1)） |
| 裝置的相對量 | winit 的 raw `DeviceEvent::MouseMotion`，任何時候都有（`bevy_winit/src/state.rs:459`） | `cursorGrab = Locked` 時才是裝置的量 | SDL 3.4 在非相對模式沒有 raw 的移動 | 一個請求欄位 |
| 相對量的單位 | winit 沒有選擇：`DeviceEvent::MouseMotion` 永遠是 raw、`CursorMoved` 是游標的位置 | 產品選（`sdl_window` 的 `mouseMotion`）：`Device`（預設，照 SDL 給的；`Locked` 時是 raw，用滑鼠轉視角）或 `LogicalPixels`（邏輯像素、帶 OS 的指標速度，遊戲自己畫游標時用） | 鎖住游標又要自己畫游標的遊戲（推過視窗邊）要的是游標的速度，不是裝置的計數；Windows 的 raw 是滑鼠計數，遊戲不知道單位換算不了 | 一個產品選項（預設不變） |

## Design Invariants

<a id="in-1"></a>
**IN-1 職責。** 登記輸入的 `Message`、狀態與 `TextInputRequest`；從 `Message` 推出狀態。不做：和 OS 說話（後端的）、按鍵對應動作、拿原始輸入決定遊戲（IN-5）。依賴只有 Base、Math、Time、Kernel、編排層。守衛：連結集（`gyo_expect_links(gyo_input …)`，`tests/common/input/CMakeLists.txt`）；審查。

<a id="in-2"></a>
**IN-2 狀態只從 `Message` 推出。** `ButtonInput<…>`、`Accumulated*`、`CursorPosition`、`TextInput` 只由 `kInputSet` 裡的 System 寫，內容只取自這一幀讀到的 `Message`；新 World 的初值是空的。守衛：`input.module` 各案；突變 `in-*`。

<a id="in-3"></a>
**IN-3 按鍵的規則。** 按住的鍵的重複不是按下（沒按住的鍵的重複是按下，同 Bevy）；沒按住的鍵放開不變；失焦放開所有按鍵（不放滑鼠鍵）；丟事件放開所有按鍵與滑鼠鍵。守衛：`input.module`「a repeat…」「a focus loss…」「dropped events…」；突變 `in-repeat-press`、`in-focus-ignored`、`in-dropped-ignored`。

<a id="in-4"></a>
**IN-4 寫的一方在 set 之前。** 寫 `input` 的 `Message` 的 System 排在同一張 schedule、同一個 stage、`before` `kInputSet`（`sdl_window` 照做）；沒排的後果是晚一幀、不報錯（不加守衛：AGENTS §3）。`TextInputRequest` 是一般的 Resource，兩個寫者時後跑的算。守衛：`sdl_window.events`「a key pushed before a frame…」（突變 `sw-writer-unordered`：拿掉 `before` → 失敗）；set 沒有成員時，寫的一方的 `before` 是 Kernel 的啟動錯誤（突變 `in-set-missing`）。

<a id="in-5"></a>
**IN-5 原始輸入不決定遊戲。** 判定的 System 讀作者換好的每 tick 遊戲輸入（回滾的 `Input<T>`），不讀本頁的 `Message` 與狀態（它們每台機器不同、雜湊不寫）。守衛：設計審查（AGENTS §3：誤用不加守衛）。

**Replacement Criteria**：RC-IN 1：跨型別的先後（例：同一幀「失焦→回焦→再按」）在實際的遊戲出錯 → 改成一個帶序號的總 `Message` 或給每個 `Message` 序號（Bevy 若改了一起看）。RC-IN 2：第二個後端（手把、網路）需要 `KeyCode` 以外的實體鍵 → 加 `Unidentified` 帶原生碼的形（同 Bevy `NativeKeyCode`）。
