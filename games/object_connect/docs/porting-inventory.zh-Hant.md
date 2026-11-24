# Object_Connect 移植盤點表

來源：`~/Code/GitHome/Object_Connect`（HEAD `ca4482c`，C++20，Windows／MSVC 限定，KamataEngine／DirectX 12）。
移植目標：`games/object_connect/`（遊戲源碼）、`assets/object_connect/`（資產）、`tests/object_connect/`（遊戲自己的測試）。
GYO2 基準：`gyo2` `7be54ccb`（2026-11-23）。這份表屬於產品；刪掉 Object_Connect 時跟著刪（AGENTS §1-8）。

讀法：每一項寫「原本是什麼 → 在 GYO2 對到什麼 → 狀態」。「Engine」指 Engine 的單位（Module、Kernel、core）；「遊戲」指 `games/object_connect/` 自己的程式（AGENTS §1-1：替わりうる規則は作者が選ぶ）。

## 1. 原專案的組成

| 層 | 原檔 | 行數（約） | 依賴 |
|---|---|---|---|
| core（無引擎） | `Data/Csv`、`Data/PuzzleCatalogLoader`、`Data/PuzzleData.hpp`、`Puzzle/PuzzleBoard`、`Geometry/Geometry2D`、`Tentacle/BloodTentacle`、`Tentacle/RibbonStrip`、`Game/GameFlow`、`Game/FinalResults`、`Game/MusicSequencePolicy.hpp`、`Math/Vec2`、`Math/Color`、`Text/TextLayout`、`Text/Utf8` | 4,700 | 只有標準庫（`Csv::Load` 與 catalog 的 `Load` 碰檔案系統） |
| runtime | `Core/Application`、`Core/FrameTimer`、`Game/Game`、`Input/InputSystem`、`Rendering/*`（PuzzleRenderer、GameUiRenderer、MouseCursorRenderer、TextureHandleRegistry）、`Text/FontSystem`、`Audio/GameAudio` | 5,900 | KamataEngine（WinApp、DirectXCommon、Sprite、Input〔DirectInput〕、Audio〔XAudio2〕）、ImGui（只在 Debug、與 `imstb_truetype` 字型點陣化）、DirectXTex、D3D12、Win32 |
| 測試 | `tests/*`（自製的 `TestContext`，沒有測試框架） | 3,800 | core；`FontSystemLifecycleTests`、`GameAudioTests` 連 runtime |

外部組件：KamataEngine（學校提供的 DX12 框架，預建 `.lib`）、ImGui、DirectXTex、DXC、Windows SDK。**全部不帶進 GYO2**：視窗、輸入、畫面、音訊裝置、字型點陣化都改用 GYO2 的單位（下表）。移植後遊戲自己用的外部組件只有 fast_float（解析浮點，見 2.1 的 `Data/*` 列）。

## 2. 逐項對照

### 2.1 遊戲自己的（搬進 `games/object_connect/`）

| 原本 | 內容 | 在 GYO2 | 狀態 |
|---|---|---|---|
| `Puzzle/PuzzleBoard` | 解謎規則：可當來源／目標的節點、接續數與長度預算、重複／循環／dead 節點擋線、拖曳預覽、收回、解開判定、`wrapEdges` 的穿邊路徑 | 遊戲的 `logic` 函式庫（`src/puzzle/`） | **PR 1 已搬**，原測試全過 |
| `Geometry/Geometry2D` | AABB、線段與 AABB 的交點、穿邊（環面）路徑與繞數 | 遊戲（`src/geometry/`）。Engine 的 `core/math` 有 `Vec2`、2D 的 `Rect`（包含、交集）與 3D 的 `Aabb`／`Segment`，沒有 2D 線段對矩形的進入時刻；穿邊（環面）路徑與繞數是這款遊戲的規則，不進 Engine。之後 World 層可把 AABB 換成 `Math::Rect`，搬邏輯時不改 | **PR 1 已搬** |
| `Tentacle/BloodTentacle` | 血管的 Verlet 粒子鏈（固定步 1/120 s 的累加器、約束迭代、跟隨延遲） | 遊戲（`src/tentacle/`）。不是物理 Module 的事：一條鏈的模擬是遊戲的表現規則（SDL 總宣告第 6.4 節也這樣寫） | **PR 1 已搬** |
| `Tentacle/RibbonStrip` | 把中心線轉成三角形帶的頂點（寬度變化、像素格對齊、顏色） | 遊戲（`src/tentacle/`）；輸出的頂點之後交 R2b 的「來源側每幀給頂點的 2D 形狀」 | **PR 1 已搬**（頂點怎麼送進渲染等 R2b） |
| `Data/Csv`、`Data/PuzzleCatalogLoader`、`Data/PuzzleData.hpp` | 嚴格的 CSV 讀取；`levels.csv`／`nodes.csv`／`maps/*.csv` 三種表組成關卡目錄；欄位驗證、預設值、舊欄位相容與警告 | 遊戲（`src/data/`）。只留從字串解析（`Parse`）；讀檔改走 asset（2.2 的 `asset` 列）。原本的 `Load`、`Csv::Load`、「貼圖檔存在嗎」的檢查碰檔案系統，違反 os 守衛 R4（runtime 程式不直接開檔），拿掉。浮點欄位原本用 `std::from_chars`（浮點版在 macOS 13.3 的 deployment target 沒有），改經遊戲自己的 adapter `src/adapters/fast_float/` 呼叫 fast_float（`third_party/` 已有，json Module 也用；json 的 J-1 只管 Engine，守衛不掃產品目錄），語意同 `chars_format::general` | **PR 1 已搬**（`Parse` 部分）；讀檔的接法在 PR 2 |
| `Game/GameFlow` | 選單狀態（主選單、選關、遊玩、暫停、解開、最終結果）、選項移動、確認後的命令 | 遊戲。畫面的值改用 Engine 的 `state_machine`（`StateMachine<GameScreen>`，請求在下一次 apply 生效）；選項在 Resource `MenuSelection`；`GameFlow` 只加一個「從畫面與選項接著跑」的建構子，System 每幀用它跑一次選單（原測試不動） | **PR 1 原樣搬**；**PR 2 接上 state machine** |
| `Game/FinalResults` | 一輪遊玩的通關紀錄、結果卡的頁數 | 遊戲（`src/game/`）；PR 2 成為 Resource | **PR 1 已搬** |
| `Game/MusicSequencePolicy.hpp` | 什麼時候重播 BGM 的規則 | 遊戲 | **PR 1 已搬** |
| `MouseCursorRenderer.hpp` 裡的 `ResolveMouseCursorIcon`、`ShouldUseCustomMouseCursor` | 游標要用哪張圖、什麼時候換掉系統游標 | 遊戲（`game/MouseCursorPolicy.hpp`，從 renderer 標頭拆出、內容不變） | **PR 1 已搬** |
| `Math/Vec2`、`Math/Color` | 2D 向量、RGBA | 遊戲（`include/object_connect/math/`）。Engine 的 `core/math` 有 `Vec2`／`Vec4`（`Color` 可對 `Vec4`），但 PuzzleBoard 等大量程式用原型別；換型別是之後 World 層的事，不在搬邏輯時一起改 | **PR 1 已搬** |
| `Game/Game`（`Game::Impl`） | 一幀的流程：讀輸入 → 失焦／Esc 取消拖曳 → UI 命中 → GameFlow → 套用命令（開關卡、重試、下一關）→ 更新 PuzzleBoard → 音效事件 → 游標 | `world` 函式庫（`include/object_connect/world/`）：Resource `Catalog`、`Playfield`（固定 1280×720）、`FrameInput`、`MenuSelection`、`Session`、`RunResults`；Message `MenuStepped`、`LineHoldStarted`／`LineHoldEnded`、`MusicRestarted`、`QuitChosen`；`Main`／`Update` 依序五個 System（CancelDragOnLeave → StepMenu → ApplyMenuCommand → SimulateBoard → TickSolvedTimer）；一幀的時間是 `world_time` 的 `ActiveTime::delta`。輸入取樣、UI 命中、翻頁、游標、音效呼叫留在外面：`FrameInput` 進、Message 出 | **PR 2 已寫**，headless 測試 `object_connect.world`（9 個情境） |
| `Rendering/GameUiRenderer` 的命中判定與翻頁 | 選單、選關（5×2 一頁）、結果卡的版面與滑鼠命中 | 遊戲。版面與命中是遊戲的規則；畫字用 `text`、畫矩形用 R2a／R2b。**不需要 U**（沒有文字框、沒有通用控件）；U 來了可以改用、不是前提 | **移植實作第 1 段已搬**：版面、命中、選關與結果的翻頁搬成 `logic` 的 `ui/MenuLayout`（`MenuPages`；原 `Draw` 裡移動頁數的部分成為 `Present`，每幀結束時呼叫）；**第 3 段**畫出來（`runtime/UiDrawing`，第 10 節） |
| `Rendering/PuzzleRenderer` | 背景、血管三角形帶、節點 sprite（含光暈、脈動、inactive 55% 亮度）、dead 節點的程序化外觀 | 遊戲的「每幀產生畫面資料」System | **移植實作第 3 段已畫**：`runtime/PuzzleDrawing`，用 R2a 的 `Pass2D`（每幀自給頂點），不必等 R2b（第 10 節） |
| `Core/Application`、`Core/FrameTimer` | `WinMain`、KamataEngine 的初始化與主迴圈、幀時間 | 產品的 `main` 建 `Product`、runner 由 `sdl_window` 給；幀時間由 `world_time` | **移植實作第 1 段已做**：`src/main.cpp`（第 8 節） |

### 2.2 對到 GYO2 已有的單位

| 需要的能力 | 原本 | GYO2 | 狀態 |
|---|---|---|---|
| ECS、System、Resource、Message | 沒有（手寫一幀的流程） | Kernel（`engine/ecs/kernel`）、`orchestration` 的 `Product` | 已有 |
| 畫面狀態 | `GameFlow::screen_` | `state_machine`（`StateMachine<S>`＋`StateMachineRequest<S>`、enter／exit schedule） | PR 2 用了（只用值與請求；enter／exit schedule 等畫面與音效接上再看要不要用） |
| 一幀的時間 | `FrameTimer`（`QueryPerformanceCounter`） | `world_time`（`VirtualTime` 等 Resource） | 已有 |
| 讀資料檔 | `std::ifstream` | `asset`（catalog Data Contract、`TextLoader`）＋`world_asset`（`AssetRequests`／`AssetView`）；`assets/object_connect/content.json`（建置層生成的 `GYO_GAME_CONTENT_MANIFEST`） | **移植實作第 1 段已接**：遊戲自己的 loader 經 `LoadContext::ReferencePath` 宣告引用（第 8 節） |
| 節點與游標的 PNG | DirectXTex／KamataEngine `TextureManager` | `image`（子批 I，PR #301 待合併）的 PNG loader | **移植實作第 1 段已接**：產品的 `configure` 登記 `PngLoader`；節點圖經 `nodes.csv` 的 loader 宣告引用，游標 3 張直接請求 |
| 日文字型、排版 | `FontSystem`（`imstb_truetype`）＋`TextLayout`／`Utf8`（左到右、無 shaping） | `text`（子批 T，已合併：FreeType＋HarfBuzz、換行、圖集） | 已有；**移植實作第 1 段已接**字型（`FontLoader`、`UiText` 的 `TextContext`）。原 `Text/*` **不搬**：GYO2 的 `text` 涵蓋（且多了 shaping）；原 `TextLayoutTests`／`Utf8Tests`／`LazyGlyphTests`／`FontSystemLifecycleTests` 跟著不搬 |
| 音效、BGM | `GameAudio`（XAudio2 經 KamataEngine：載 WAV、播放、迴圈、改音量、查是否在播） | `audio`（`Mixer`、`Clip`、`WavLoader`）；裝置輸出在子批 A（`sdl_audio`） | **移植實作第 2 段已接**（第 9 節）；G1、G2 由 A（#306）補上 |
| 正交相機、2D 座標 | 固定 1280×720 的像素座標 | `transform`＋`camera`（X1，已合併：正交投影、2D marker） | 已有；**第 3 段**直接用 `Pass2D::view`（1280×720 拉滿視窗；視窗不可縮放，HiDPI 是等比例），沒有用 camera；黑邊（等比例＋置中）要等 R2b 的 `Pass2D::viewport`／`KeepAspect`，不可縮放時用不到 |
| 記錄 | `OutputDebugStringA` | `logging` | 已有 |
| 存檔 | 沒有（通關紀錄只在一輪遊玩中、不寫檔） | `save_version` | **不需要** |
| 網路、權威、回滾 | 沒有 | `net`、`authority`、`rollback` | **不需要**（單機；不帶也成立） |
| 物理 | 沒有物理引擎；Verlet 鏈是遊戲邏輯 | 物理 2D | **不需要** |

### 2.3 要等的子批

| 子批 | Object_Connect 用到的 |
|---|---|
| P（plugins／SDL3） | `sdl_window`、`sdl_gpu`、`sdl_audio` 的前提 |
| W（window／input） | 鍵盤（W/S/↑/↓/Enter/Esc 的「剛按下」）、滑鼠位置與左鍵三態、滾輪、焦點（失焦取消拖曳並暫停）、游標在不在視窗內、隱藏系統游標、視窗不可縮放；**缺口 G3～G5** |
| R2a（render core） | 2D pass：貼圖 sprite、不貼圖的純色三角形、半透明混合、畫的先後 |
| R2b（render pipelines） | 已記的兩件：來源側每幀給頂點的 2D 形狀（血管）、固定解析度的縮放；游標畫在 HUD 文字之上 |
| A（audio output） | 混音器輸出到裝置 |
| U（ui） | 不需要（2.1 `GameUiRenderer` 列） |

## 3. 缺口（GYO2 沒有、Object_Connect 需要）

| # | 缺的能力 | Engine 的機制還是遊戲的事 | 原本怎麼用 | 影響的子批 | 參考 |
|---|---|---|---|---|---|
| G1 | **播放中改音量**（壓低 BGM、淡出音效） | Engine（混音器的機制）。`audio` 的 `PlayRequest` 只在開始時給增益，`Submitter` 只有 `Play`／`Stop`／`StopAll` | 拖線時 BGM 在 0.05 s 內壓到 0.4、放開 0.25 s 回復；放開時拉線音 0.05 s 淡出（`GameAudio.cpp:27-30`、`:385`、`:407`） | audio 線（A 或另開小批）；**A（#306）已補：`Submitter::SetGain`（ramp）** | kira 的 `set_volume(…, Tween)`；Bevy `AudioSink::set_volume` |
| G2 | **迴圈播放** | Engine（混音器的機制）。`PlayRequest` 註明「One sound, played once」；`Playhead` 規定「never for decisions」，遊戲不能靠查播放位置接下一段 | BGM：序奏播完接迴圈段（`GameAudio.cpp:338`、`:349` 用 `IsPlaying` 判斷）。（更正：拉線音不是迴圈，`BeginLineHold` 以 `loop=false` 播，`GameAudio.cpp:158`） | audio 線；**A（#306）已補：`PlayRequest::loop`** | kira 的 `loop_region`；Bevy `PlaybackMode::Loop`。不改 Engine 的替代：遊戲用 `at` 排下一段的開始時刻（要 clip 的長度），每段結束前補排——可行但每個遊戲都要自己寫 |
| G3 | **把系統游標移到指定位置（warp）** | Engine（`window` 的請求資料） | `wrapEdges` 關卡（20 關中 9 關）拖線碰到視窗邊緣時，把游標移到對邊（`InputSystem.cpp:87-115` 的 `SetCursorPos`），讓線穿過邊界繼續拉 | W | Bevy `Window::set_cursor_position`；SDL `SDL_WarpMouseInWindow` |
| G4 | **滑鼠的相對移動量（游標被邊緣擋住時仍有）** | Engine（`input` 的 `Message`） | 游標卡在邊上時，絕對座標不動，用 DirectInput 的相對量判斷「還在往外拉」（`InputSystem.cpp:200-230`） | W | Bevy `MouseMotion`（raw delta）；SDL `SDL_MouseMotionEvent::xrel` |
| G5 | **游標進出視窗** | Engine（`input` 或 `window` 的 `Message`） | 只在「有焦點且游標在視窗內」時隱藏系統游標、畫自訂游標（`ShouldUseCustomMouseCursor`） | W | Bevy `CursorEntered`／`CursorLeft`；SDL `SDL_EVENT_WINDOW_MOUSE_ENTER`／`LEAVE` |
| G6 | 視窗不可縮放 | Engine（`window` 建窗的設定）；SDL 總宣告的 `window` 列有「改視窗的請求」，沒寫「建窗時可不可縮放」 | `SetSizeChangeMode(kNone)`（`Application.cpp:124`） | W | Bevy `Window::resizable`；SDL `SDL_WINDOW_RESIZABLE` |
| — | 遊戲層的 2D 幾何、穿邊、Verlet 鏈、CSV 解析、選單版面與命中 | 遊戲 | — | — | 不進 Engine |

**SDL 總宣告 R2b 已記的兩件核對**：(1) 動態頂點——血管的三角形帶（`RibbonStrip`）＋矩形、圓、外框的純色三角形，**沒漏**；(2) 固定解析度的縮放——原本視窗不可縮放（G6），所以原遊戲其實沒有遇到「不同視窗大小」；GYO2 若預設可縮放或碰到 DPI 縮放，縮放仍要做，**沒漏**，但滑鼠換回遊戲座標（`viewport_to_world_2d`）也要涵蓋 **G3 的反方向**（遊戲座標 → 視窗座標，warp 時用；`InputSystem.cpp:87-108`）。其餘第 6.4 節列的（正交相機、sprite、純色三角形、半透明、畫的先後、日文 HUD、游標、失焦、滾輪）都對得上。

## 4. 資產

使用者 2026-11-23 確認（經主會話）：美術、關卡資料、音訊都是使用者本人做的，可以放進 repo；`k025g1030-design` 是使用者的學校帳號；音訊選 (a)，4 個 wav 原樣進 git。授權說明在 `assets/object_connect/LICENSES/README.md`。

| 檔（`assets/object_connect/` 下，相對路徑照原本） | 大小 | 授權 | 狀態 |
|---|---|---|---|
| `fonts/BIZUDPGothic-Regular.ttf` | 4.3 MB | SIL OFL 1.1（`LICENSES/BIZUDPGothic-OFL.txt`）；SHA-256 與原專案記錄的相同 | **已放（PR 2）** |
| `assets/textures/node/*.png`（7）、`assets/textures/mouse/*.png`（3） | 40 KB | 使用者本人 | **已放（PR 2）** |
| `audio/*.wav`（4） | 25 MB | 使用者本人 | **已放（PR 2）**，原樣 |
| `data/levels.csv`、`data/nodes.csv`、`data/maps/stage_01..20.csv` | 96 KB | 使用者本人 | **已放（PR 2）** |
| `data/maps/archive/prototype_0{1,2}.csv` | — | 使用者本人 | 不放（沒有關卡引用的原型） |
| `white1x1.png`、`kamata.ico`、`axis/*`、`shaders/*.hlsl` | — | KamataEngine 的範本資源 | **不搬**（GYO2 的 render 自己有 shader；`axis` 是 3D 範本） |

路徑照原本，所以 CSV 不改（`nodes.csv` 寫的是 `assets/textures/node/heart.png`，以 `assets/object_connect/` 為根）。格式：PNG 經 `image`（I）、WAV 經 `audio` 的 `WavLoader`、CSV 經 `asset` 的 `TextLoader`。`content.json`（asset catalog）在掛 main 執行檔時才需要（`GyoContent.cmake`），W 那段一起寫。

## 5. PR 1 拿掉的測試

| 測試 | 原因 | 現況 |
|---|---|---|
| `PuzzleCatalogTests` 的 `TestBundledCatalogs`、`PuzzleBoardTests` 的 `TestBundledStageOneIntegration` | 讀磁碟上的關卡資料（`Load` 已拿掉） | **PR 2 接回**：測試自己讀 `assets/object_connect/` 的檔（`tests/object_connect/BundledData.cpp`；tests 不受 R4 限制），交給 `Parse`，斷言照原本 |
| `PuzzleCatalogTests` 的 `TestIndependentDiskLoad` | 測的是 `Load` 本身 | 不回來（`Load` 不存在了；讀檔之後經 asset） |
| `Text/*`、`FontSystemLifecycleTests` | 原 `Text/*` 不搬（GYO2 `text`） | 不回來 |
| `GameAudioTests` | `GameAudio` 要改寫在 `audio` 上，且有 G1、G2 | **移植實作第 2 段**：依新寫法改寫成 `object_connect.runtime` 的三個聲音情境（第 9 節） |

## 6. 搬法的取捨（卡片自己判的）

- **namespace 保留 `object_connect`**：命名規範 N17「產品用自己的根 `<產品>::`」，目錄 `games/object_connect` ↔ namespace `object_connect` ↔ target `gyo_object_connect-logic`。標頭路徑改成 repo 的慣例（小寫目錄：`object_connect/puzzle/PuzzleBoard.hpp`）。
- **錯誤的回報方式**：遊戲函式庫內照原本（`bool`＋`std::string& error`、內部用例外）。AGENTS §1-9 的 `Result` 是 Engine 的設計根；遊戲內部的寫法是作者的（§1-1）。PR 2 的 World 層（System 的回傳）照 Kernel 的 `SystemResult`。
- **測試框架**：沿用原本的 `TestContext`（遊戲自己的），不改寫成 doctest；ctest 名 `object_connect.logic`，標籤 `cpu`。
- **先搬純邏輯、再寫成 System**：純邏輯原樣搬、原測試全過，確認移植沒有改壞規則；PR 2 才把 `Game::Impl` 的一幀拆成 System／Resource，差異只在接線。

## 7. PR 2：World 層的時序差（卡片自己判的）

- 原本 `GameFlow` 立刻換畫面；state machine 在下一幀的 apply（`Main`／`PreUpdate` 之後）才換。每個 System 在原本讀「換過的畫面」的地方讀「這一幀結束時的畫面」（`EffectiveScreen`：這一幀的請求，沒有就是目前值），所以一幀內決定的事和原本相同；新畫面從下一幀起才是目前值。例：Esc 暫停的那一幀，原本與現在都不再更新 PuzzleBoard。
- `Session::solvedElapsedSeconds` 的累加照原本（解開的那一幀從 0 加上那一幀的時間）。
- `StartPuzzle` 等原本丟例外的地方，改成 System 回 `KernelError`（`SystemFailed`）：World 壞掉、Product 結束，由 `main` 決定怎麼報（AGENTS §1-9：只告訴外面發生了什麼）。
- 測試把 `TimeSampling` 設成 `ManualDelta` 0.1 s，每幀時間固定；Message 由 `Main`／`Last` 的記錄 System 讀。

## 8. 移植實作第 1 段：主程式、輸入、資產（卡片自己判的）

PR：移植實作第 1 段（W、T、I 合併後）。畫面要等 R2a，聲音要等 A。

| 項 | 做法 | 理由 |
|---|---|---|
| 主程式 | `src/main.cpp`：預設 Module＋`sdl_window`（`gyo_product_units(... PLUGINS sdl_window)`）；`WindowSettings` 1280×720、`resizable=false`（G6）；`AssetOptions` 的內容根＝執行檔旁的 `assets/object_connect`（`GyoContent.cmake` 放的位置）；asset 的執行緒是產品自己開的 `WorkerPool`（2 條）；`--frames N`（CI 用）與 `--verbose`（還沒有畫面時，把畫面、游標、拖曳、warp 的變化寫進 log） | 照產品登記的 `games/` 形（#294）與 `examples/sdl_window` |
| 資料的載入 | 兩個遊戲自己的 loader：`levels.csv` → `LevelTable`（宣告 `map_path` 欄的檔為 text 的引用）、`nodes.csv` → `NodeTable`（宣告 `texture_path` 欄的檔為 image 的引用）；表裡的路徑是從內容根算的（原版的 `resourceRoot`），loader 換成相對於表本身的引用（`ReferenceFromTable`）。`asset_catalog.json` 列出全部檔；CSV 的地圖用 `"format": "txt"` 交給 `TextLoader` | 比「System 讀 levels 再照命名規則拼 id」少一個命名約定；表與它指的檔一起 Complete（asset 宣告 5.17，同 glTF 宣告貼圖） |
| 什麼時候開始玩 | 載入放在 World 生成期間（`WhileGenerating`，沒好就 `Hold()`）；遊戲的 System 加 `AfterGenerating()`（`BuildWorld` 新增 `WorldOptions`，測試不帶條件時照舊）；任何一個檔失敗或關卡是空的 → System 回 `SystemFailed`，`main` 記一行、結束碼 1 | 原版 `Game::Initialize` 失敗就不開始；GYO 的「先載完再開始」機制是 world-gen（O-14），不另做載入畫面 |
| 輸入 | `ReadPlayerInput`（`Update`，在 `CancelDragOnLeave` 前）：`input` 的 `ButtonInput`／`CursorPosition`／`AccumulatedMouse*` 與 `window` 的 `WindowState` → `FrameInput`。鍵：W/↑、S/↓、Enter/NumpadEnter、Esc 的「剛按下」；左鍵三態；滾輪一格＝120（原版的 `WHEEL_DELTA`）。沒有焦點時不讀鍵、鍵與滾輪（原版 `Sample`），但這一幀仍照原版 `Update` 走過 UI（失焦算「啟動」，清掉滾輪的零頭）。「游標在視窗內」照原版 `PtInRect` 用範圍判斷（按住左鍵時 `CursorPosition` 會給視窗外的位置）；游標不在視窗內時選單不做命中（原版的滑鼠位置跟著游標出去，命中不到） | 原版 `InputSystem::Sample` 與 `Game::Impl::Update` 開頭照搬 |
| 座標 | 視窗的邏輯像素 → 遊戲座標（1280×720）：照比例換算（`ClientToPlayfield`）。視窗不可改大小，所以一般是 1:1；HiDPI 由 `window` 的邏輯像素吸收 | 原版 `ToLogicalPosition` |
| 穿邊（G3、G4） | `input/PointerWrap`（`logic`，不碰視窗）：原版 `InputSystem` 的穿邊整段；要 warp 時送 `window` 的 `WarpCursor`（邏輯像素）。相對移動只看正負號（游標停在邊上時判斷「還在往外推」）。原版的 `SetCursorPos` 會回報成功與否，請求資料不會：warp 後那一軸若出現超過視窗一半、往原處的跳動，當成 OS 沒套用 warp（基準移到游標實際的位置、指標不動），不當成玩家的移動 | 原版行為照搬，視窗呼叫換成請求資料；沒套用時不讓穿邊每幀重複觸發（設計審查 should） |
| 游標（G5） | `PresentMenus` 算 `CursorAppearance`（哪張圖、要不要換掉系統游標＝有焦點且在視窗內）；第 3 段起遊戲畫自己的游標（`DrawFrame`），同時 `WindowRequest::cursorVisible=false` | 原版 `MouseCursorRenderer`（隱藏系統游標、畫 32×32 的圖） |
| 選單分頁 | `PresentMenus`（`Update`，在 `TickSolvedTimer` 後）以這一幀結束時的畫面呼叫 `MenuPages::Present`；`QuitChosen` → `WindowRequest::close` | 原版 `Draw` 在 `Update` 之後移動頁數，下一幀的翻頁看「上次畫的畫面」 |

**已知的限制**：視窗貼著螢幕邊時，游標被螢幕擋住後 `MouseMotion` 是 0（不是 `Locked` 時 SDL 由絕對位置算，`input` 頁 G4），所以推不過那個邊；原版用 DirectInput 的相對量推得過。要完全照原版，得在拖線時切 `CursorGrab::Locked`（遊戲的游標第 3 段已經畫得出來），但 `PointerWrap` 要改成由相對量追蹤指標（`Locked` 時絕對位置不再跟著動），第 3 段沒做，仍是已知限制。視窗沒貼邊時，按住左鍵時 OS 讓游標出得了視窗，照絕對位置就能穿邊。

## 9. 移植實作第 2 段：聲音（卡片自己判的）

PR：移植實作第 2 段（A #306 合併後）。

| 項 | 做法 | 理由 |
|---|---|---|
| 混音器與提交口 | 產品帶 `sdl_audio`；它在自己的 `Configure` 建 `Mixer` 放進 Product 的共用處、登記 `WavLoader`、開預設裝置。產品的 `configure`（`runtime::ConfigureProduct`）從共用處找 `Mixer`、建一個 `Submitter`，包成 `SoundPort` 放回共用處；World 的 `GameSound` 只持指標 | `Mixer` 的說明要提交口「放在比生產者重建活得久的地方」；共用處活到所有 World 之後 |
| 沒有混音器或裝置 | 沒有 `Mixer`（不帶 `sdl_audio`）→ 記一行 Info、靜音照跑；裝置開不了（`AudioOutput::Failure`）→ `main` 記一行 Warning、照跑 | 原版音訊不可用時記警告、關掉聲音（`GameAudio::Initialize`） |
| 音檔 | 4 個 wav 照原版是**可有可無**：載入時一起等、失敗的記 Warning、那個聲音不出聲；其餘內容失敗才是啟動失敗 | 原版 `loadOptional` |
| BGM | 生成結束後的第一幀播序奏（0.9），迴圈段（0.4、`loop`）排在序奏的長度之後（`PlayRequest::loop` 的說明的做法）；`MusicRestarted` → 停掉兩者、重排 | 原版用 `IsPlaying` 查序奏播完再接；GYO 的 `Playhead` 不准拿來做決定，A 的寫法是事先排好 |
| 壓低 BGM（G1） | 拿到線時目標 0.4、放開時 1.0；對序奏與迴圈段各送一次 `SetGain`。ramp 照混音器實際的時間線算（`PlanDuck`）：混音器大約在送出時接手、把正在跑的 ramp 凍結在那裡，延遲 `scheduleDelay` 後才開始新的 ramp；長度＝剩下的距離 ÷ 原版的速率（攻 0.05 s、放 0.25 s 走完整段 0.6）。時間用 Engine 的時間基準（混音器用的那個），不用遊戲時間。中途重播 BGM 時，新的兩個聲部以那時的增益開始、再補一次 `SetGain` 接完剩下的 ramp | 原版每幀以固定速率移動。**和原版的差別**：比延遲（60 ms）還短的一次拿線，壓低還沒開始就被取代，BGM 不會沉下去（原版會沉一點）；其他情形曲線相同、只晚 60 ms（混音器所有請求的共同延遲） |
| 拉線音、放開音 | 拿到線：切掉上一次的（淡出中的也切）、播拉線音（0.65、不迴圈）；放手：拉線音 `SetGain` 到 0、ramp 0.05 s，0.05 s 後 `Stop`（淡出中的另外記著）；是「放開」（不是取消）時再播放開音（2.0） | 原版 `BeginLineHold`／`EndLineHold`／`UpdateHoldFade` |
| 時刻 | 每個請求的 `at` 是送出時的 `Time::Now()`（Engine 時間基準）；混音器自己加 `scheduleDelay`（預設 60 ms） | audio DI 3：不用可縮放的遊戲時間 |

**沒處理的（審查 nit）**：同一幀若先後有「放手」與「拿線」兩種 Message，System 先處理全部拿線再處理放手（現在沒有會這樣送的路徑）；World 被替換時舊的迴圈段不會停（這款遊戲不替換 World）；兩個 World 共用一個提交口會違反單一使用者（只有一個 World）。

## 10. 移植實作第 3 段：畫面（卡片自己判的）

PR：移植實作第 3 段（R2a #315 合併後）。

| 項 | 做法 | 理由 |
|---|---|---|
| 用 R2a 還是等 R2b | 直接用 R2a：遊戲的 World 帶 `render_world`，`DrawFrame`（`Main/Last`，生成結束後）每幀組一個 `Pass2D` 寫進 `RenderCommands`，`sdl_gpu` 在 `Submit` 畫。血管的三角形帶就是每幀自給的頂點，不必等 R2b 的 `Shape2d` | R2a 的 `Pass2D` 本來就是每幀的頂點＋索引；R2b 卡也確認這條可行（主會話轉來的說明） |
| 座標 | `Pass2D::view = {0,0,1280,720}`，畫面拉滿視窗（不可縮放，HiDPI 等比例） | 原版的 back buffer 就是 1280×720 |
| 順序 | 原版 `Game::Impl::Draw` 的順序：關卡（背景 → 血管外框帶 → 血管核心帶 → 肉的畫素 → 程序化的死節點與骨斑 → 死節點的圖 → 光暈 → 脈動 → 互動節點的佔位方塊 → 互動節點的圖）→ UI 方塊（`GameUiRenderer::Draw` 的順序）→ 全部文字 → 游標 | 照原版 |
| 顏色 | 原版畫在 sRGB 的 render target、顏色是線性值；`ToColor8` 把線性值做 sRGB 編碼成位元組（貼圖的染色也一樣） | R2a 的 2D pass 拿位元組原樣混合（render 頁第 2 節）。**差別**：混合在編碼後的位元組上做，不是線性空間，半透明的覆蓋層比原版稍暗 |
| 文字 | `text` 的 `TextContext`（BIZ UDPGothic，`ja`）每幀 Shape→Layout→Rasterize 進 `GlyphAtlas`；新頁整頁 `Create`、舊頁照 `ChangesSince` 的矩形 `Update`（Alpha8 → RGBA，白色＋覆蓋率當 alpha）；行高 1.0（原版是字型的 ascent－descent＋line gap，這個字型正好是 1.0；Engine 預設 1.2 會讓 Top 對齊的字低 0.1 倍字高）；對齊照原版 `TextLayout.cpp:64-97`（每行用自己的寬度水平對齊；Top＝版面頂端在錨點、Middle＝中線在錨點）；字形位置取整數像素 | 原版 `FontSystem`（imstb_truetype）不搬 |
| 貼圖 | 節點圖：`nodes.csv` 與各地圖 CSV 的 `texture_path` 都經 loader 宣告引用（地圖改成遊戲自己的型別 `MapTable`），生成結束後的第一幀一次做成貼圖；游標 3 張；最終結果的器官圖用 `organ.png`。濾波 Linear | 地圖 CSV 也指定 4 張器官圖（liver、lung、stomach、kidney），原版從路徑直接讀 |
| 頂點上限 | 拿掉原版的 65,536 頂點預算與「放不下就丟掉血管副本」的保留邏輯 | `Pass2D` 的清單會長，沒有固定上限 |
| 幀間隔 | `SDLWindowOptions::minFrameInterval` 4 ms | 顯示畫面時等垂直同步；視窗最小化時拿不到畫面、不會等，這個下限讓迴圈不空轉 |
| 測試 | `object_connect.main` 改 gpu 標籤（視窗不隱藏：X11 上隱藏視窗拿不到畫面，每幀都被跳過）；`--frames` 模式下沒有任何一幀顯示、或有畫壞的幀／跳過的繪製／被拒的貼圖時結束碼 1（SDL dummy 驅動沒有 GPU 裝置；L1 在 Linux 的 Lavapipe＋Xvfb 跑）；`object_connect.runtime` 在 `Submit` 階段讀 `RenderCommands`，當資料檢查 | 畫素級的比對是 `sdl_gpu.pass2d` 的事 |

## 11. 移植實作第 4 段：R2b 之後的形（卡片自己判的）

PR：移植實作第 4 段（R2b #319、#320 合併後）。R2b 卡交來兩條路：(A) 維持單一 World、要黑邊時設 `Pass2D::viewport`；(B) 拆成遊戲 World＋渲染 World（extract），用 `Shape2d`／`Sprite`＋`Camera2d`＋`Letterbox`。

| 項 | 做法 | 理由 |
|---|---|---|
| 選哪條 | **(A)**：單一 World、`DrawFrame` 自己組 `Pass2D`（第 3 段的形），只換掉「固定解析度的縮放」 | 動態頂點（血管）第 3 段已用 `Pass2D` 的每幀頂點做好；(B) 要把文字、游標、選單的畫法搬進渲染 World，畫面不會變，只多一層搬運。R2b 的 `Shape2d`／`Sprite` 在這款遊戲用不到 |
| 固定解析度的縮放 | `FitPlayfield`：用 camera 的純函式 `Compute`＋`Letterbox{1280,720}`（`ScalingFixed` 同尺寸）從視窗的實際像素算出置中、等比例的最大矩形；`Pass2D::viewport` 設成它（第一個 pass 清整個目標＝黑邊），`view` 照舊 1280×720 | 用 Engine 的機制，不在遊戲裡自己寫 letterbox 的算式；不建相機實體（單一 World 不需要相機的系統，`Compute` 是純函式） |
| 指標的換算 | 滑鼠位置先扣掉視區的左上角（邏輯像素）、再照視區大小換成遊戲座標；穿邊的「邊」是遊戲畫面的邊（黑邊算在外面）；warp 的目標加回左上角。游標「在不在視窗內」仍以整個視窗判斷 | 第 1 段的 `ClientToPlayfield` 本來就是線性換算，換成以視區為準 |
| 游標 | 遊戲的游標另起一個 `Pass2D`（不清除、沒有 viewport、view＝整個視窗的邏輯像素），畫在遊戲畫面的 pass 之後，大小照遊戲畫面的倍率 | 遊戲畫面的 pass 只畫在 viewport 裡，游標進了黑邊會被切掉，而系統游標已經藏起來（設計審查 should） |
| 什麼時候看得到黑邊 | 視窗不可縮放（G6），一般整個視窗就是 1280×720（HiDPI 等比例）；螢幕放不下或縮放比例除不盡時才有黑邊 | — |
