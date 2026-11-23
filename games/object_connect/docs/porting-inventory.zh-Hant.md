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
| `Game/GameFlow` | 選單狀態（主選單、選關、遊玩、暫停、解開、最終結果）、選項移動、確認後的命令 | 遊戲。畫面的值改用 Engine 的 `state_machine`（`StateMachine<GameScreen>`，請求在下一次 apply 生效），選項與命令留在遊戲的 System | **PR 1 先原樣搬**（純邏輯與原測試）；PR 2 改寫成 state machine＋System |
| `Game/FinalResults` | 一輪遊玩的通關紀錄、結果卡的頁數 | 遊戲（`src/game/`）；PR 2 成為 Resource | **PR 1 已搬** |
| `Game/MusicSequencePolicy.hpp` | 什麼時候重播 BGM 的規則 | 遊戲 | **PR 1 已搬** |
| `MouseCursorRenderer.hpp` 裡的 `ResolveMouseCursorIcon`、`ShouldUseCustomMouseCursor` | 游標要用哪張圖、什麼時候換掉系統游標 | 遊戲（`game/MouseCursorPolicy.hpp`，從 renderer 標頭拆出、內容不變） | **PR 1 已搬** |
| `Math/Vec2`、`Math/Color` | 2D 向量、RGBA | 遊戲（`include/object_connect/math/`）。Engine 的 `core/math` 有 `Vec2`／`Vec4`（`Color` 可對 `Vec4`），但 PuzzleBoard 等大量程式用原型別；換型別是之後 World 層的事，不在搬邏輯時一起改 | **PR 1 已搬** |
| `Game/Game`（`Game::Impl`） | 一幀的流程：讀輸入 → 失焦／Esc 取消拖曳 → UI 命中 → GameFlow → 套用命令（開關卡、重試、下一關）→ 更新 PuzzleBoard → 音效事件 → 游標 | 拆成遊戲的 System 與 Resource（目錄、目前的 PuzzleBoard、通關紀錄、`StateMachine<GameScreen>`）；一幀的時間從 `world_time` 拿 | PR 2 |
| `Rendering/GameUiRenderer` 的命中判定與翻頁 | 選單、選關（5×2 一頁）、結果卡的版面與滑鼠命中 | 遊戲。版面與命中是遊戲的規則；畫字用 `text`、畫矩形用 R2a／R2b。**不需要 U**（沒有文字框、沒有通用控件）；U 來了可以改用、不是前提 | 等 R2a／R2b（命中判定的純邏輯可先搬，PR 2 判） |
| `Rendering/PuzzleRenderer` | 背景、血管三角形帶、節點 sprite（含光暈、脈動、inactive 55% 亮度）、dead 節點的程序化外觀 | 遊戲的「每幀產生畫面資料」System；接 R2b 的可畫元件 | 等 R2b |
| `Core/Application`、`Core/FrameTimer` | `WinMain`、KamataEngine 的初始化與主迴圈、幀時間 | 產品的 `main` 建 `Product`（例：`examples/orchestration/product_main.cpp`）、runner 由 `sdl_window` 給；幀時間由 `world_time` | 等 W |

### 2.2 對到 GYO2 已有的單位

| 需要的能力 | 原本 | GYO2 | 狀態 |
|---|---|---|---|
| ECS、System、Resource、Message | 沒有（手寫一幀的流程） | Kernel（`engine/ecs/kernel`）、`orchestration` 的 `Product` | 已有 |
| 畫面狀態 | `GameFlow::screen_` | `state_machine`（`StateMachine<S>`＋`StateMachineRequest<S>`、enter／exit schedule） | 已有（PR 2 用） |
| 一幀的時間 | `FrameTimer`（`QueryPerformanceCounter`） | `world_time`（`VirtualTime` 等 Resource） | 已有 |
| 讀資料檔 | `std::ifstream` | `asset`（catalog Data Contract、`TextLoader`）＋`world_asset`（`AssetRequests`／`AssetView`）；`assets/object_connect/content.json`（建置層生成的 `GYO_GAME_CONTENT_MANIFEST`） | 已有；關卡目錄跨三種檔、`levels.csv` 用路徑指向 `maps/*.csv`，PR 2 判「每個 CSV 一個 text asset、System 組目錄」或「遊戲自己的 loader 經 `LoadContext::Reference` 宣告引用」 |
| 節點與游標的 PNG | DirectXTex／KamataEngine `TextureManager` | `image`（子批 I，PR #301 待合併）的 PNG loader | 等 I 合併；資產要先過授權（第 4 節） |
| 日文字型、排版 | `FontSystem`（`imstb_truetype`）＋`TextLayout`／`Utf8`（左到右、無 shaping） | `text`（子批 T，已合併：FreeType＋HarfBuzz、換行、圖集） | 已有。原 `Text/*` **不搬**：GYO2 的 `text` 涵蓋（且多了 shaping）；原 `TextLayoutTests`／`Utf8Tests`／`LazyGlyphTests`／`FontSystemLifecycleTests` 跟著不搬 |
| 音效、BGM | `GameAudio`（XAudio2 經 KamataEngine：載 WAV、播放、迴圈、改音量、查是否在播） | `audio`（`Mixer`、`Clip`、`WavLoader`）；裝置輸出在子批 A（`sdl_audio`） | 部分已有，缺口見第 3 節 G1、G2 |
| 正交相機、2D 座標 | 固定 1280×720 的像素座標 | `transform`＋`camera`（X1，已合併：正交投影、2D marker） | 已有；縮放模式與換算是 R2b（第 3 節核對） |
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
| G1 | **播放中改音量**（壓低 BGM、淡出音效） | Engine（混音器的機制）。`audio` 的 `PlayRequest` 只在開始時給增益，`Submitter` 只有 `Play`／`Stop`／`StopAll` | 拖線時 BGM 在 0.05 s 內壓到 0.4、放開 0.25 s 回復；放開時拉線音 0.05 s 淡出（`GameAudio.cpp:27-30`、`:385`、`:407`） | audio 線（A 或另開小批） | kira 的 `set_volume(…, Tween)`；Bevy `AudioSink::set_volume` |
| G2 | **迴圈播放** | Engine（混音器的機制）。`PlayRequest` 註明「One sound, played once」；`Playhead` 規定「never for decisions」，遊戲不能靠查播放位置接下一段 | BGM：序奏播完接迴圈段（`GameAudio.cpp:338`、`:349` 用 `IsPlaying` 判斷）；拉線音迴圈 | audio 線 | kira 的 `loop_region`；Bevy `PlaybackMode::Loop`。不改 Engine 的替代：遊戲用 `at` 排下一段的開始時刻（要 clip 的長度），每段結束前補排——可行但每個遊戲都要自己寫 |
| G3 | **把系統游標移到指定位置（warp）** | Engine（`window` 的請求資料） | `wrapEdges` 關卡（20 關中 9 關）拖線碰到視窗邊緣時，把游標移到對邊（`InputSystem.cpp:87-115` 的 `SetCursorPos`），讓線穿過邊界繼續拉 | W | Bevy `Window::set_cursor_position`；SDL `SDL_WarpMouseInWindow` |
| G4 | **滑鼠的相對移動量（游標被邊緣擋住時仍有）** | Engine（`input` 的 `Message`） | 游標卡在邊上時，絕對座標不動，用 DirectInput 的相對量判斷「還在往外拉」（`InputSystem.cpp:200-230`） | W | Bevy `MouseMotion`（raw delta）；SDL `SDL_MouseMotionEvent::xrel` |
| G5 | **游標進出視窗** | Engine（`input` 或 `window` 的 `Message`） | 只在「有焦點且游標在視窗內」時隱藏系統游標、畫自訂游標（`ShouldUseCustomMouseCursor`） | W | Bevy `CursorEntered`／`CursorLeft`；SDL `SDL_EVENT_WINDOW_MOUSE_ENTER`／`LEAVE` |
| G6 | 視窗不可縮放 | Engine（`window` 建窗的設定）；SDL 總宣告的 `window` 列有「改視窗的請求」，沒寫「建窗時可不可縮放」 | `SetSizeChangeMode(kNone)`（`Application.cpp:124`） | W | Bevy `Window::resizable`；SDL `SDL_WINDOW_RESIZABLE` |
| — | 遊戲層的 2D 幾何、穿邊、Verlet 鏈、CSV 解析、選單版面與命中 | 遊戲 | — | — | 不進 Engine |

**SDL 總宣告 R2b 已記的兩件核對**：(1) 動態頂點——血管的三角形帶（`RibbonStrip`）＋矩形、圓、外框的純色三角形，**沒漏**；(2) 固定解析度的縮放——原本視窗不可縮放（G6），所以原遊戲其實沒有遇到「不同視窗大小」；GYO2 若預設可縮放或碰到 DPI 縮放，縮放仍要做，**沒漏**，但滑鼠換回遊戲座標（`viewport_to_world_2d`）也要涵蓋 **G3 的反方向**（遊戲座標 → 視窗座標，warp 時用；`InputSystem.cpp:87-108`）。其餘第 6.4 節列的（正交相機、sprite、純色三角形、半透明、畫的先後、日文 HUD、游標、失焦、滾輪）都對得上。

## 4. 資產

| 檔 | 大小 | 授權 | 狀態 |
|---|---|---|---|
| `fonts/BIZUDPGothic-Regular.ttf` | 4.3 MB | SIL OFL 1.1（原專案 `LICENSES/README.md`、`BIZUDPGothic-OFL.txt`） | 可放；要一起帶授權文字 |
| `assets/textures/node/*.png`（7）、`assets/textures/mouse/*.png`（3） | 40 KB | 原專案 `LICENSES/README.md` 寫明「None of these notices grants additional rights to the game's code, artwork or audio」，沒有寫作者與授權 | **待使用者確認**（已轉主會話） |
| `audio/*.wav`（4） | 25 MB | 同上 | **待使用者確認**；也要定 25 MB 的 WAV 要不要原樣進 git |
| `data/levels.csv`、`data/nodes.csv`、`data/maps/stage_01..20.csv` | 96 KB | 同上 | **待使用者確認** |
| `white1x1.png`、`kamata.ico`、`axis/*`、`shaders/*.hlsl` | — | KamataEngine 的範本資源 | **不搬**（GYO2 的 render 自己有 shader；`axis` 是 3D 範本） |

格式：PNG 經 `image`（I）、WAV 經 `audio` 的 `WavLoader`、CSV 經 `asset` 的 `TextLoader`，都登記在 `assets/object_connect/` 的 catalog。

## 5. PR 1 拿掉的測試（之後會回來）

| 測試 | 原因 | 何時回來 |
|---|---|---|
| `PuzzleCatalogTests` 的 `TestIndependentDiskLoad`、`TestBundledCatalogs` | 讀磁碟上的關卡資料（`Load` 已拿掉；資料待授權） | 資產搬進來、讀檔接上 asset 之後（PR 2 或之後），改用 asset 讀同一份資料、斷言照原本 |
| `PuzzleBoardTests` 的 `TestBundledStageOneIntegration` | 同上 | 同上 |
| `Text/*`、`FontSystemLifecycleTests` | 原 `Text/*` 不搬（GYO2 `text`） | 不回來 |
| `GameAudioTests` | `GameAudio` 要改寫在 `audio` 上，且有 G1、G2 | audio 那一段移植時，依新寫法重寫 |

## 6. 搬法的取捨（卡片自己判的）

- **namespace 保留 `object_connect`**：命名規範 N17「產品用自己的根 `<產品>::`」，目錄 `games/object_connect` ↔ namespace `object_connect` ↔ target `gyo_object_connect-logic`。標頭路徑改成 repo 的慣例（小寫目錄：`object_connect/puzzle/PuzzleBoard.hpp`）。
- **錯誤的回報方式**：遊戲函式庫內照原本（`bool`＋`std::string& error`、內部用例外）。AGENTS §1-9 的 `Result` 是 Engine 的設計根；遊戲內部的寫法是作者的（§1-1）。PR 2 的 World 層（System 的回傳）照 Kernel 的 `SystemResult`。
- **測試框架**：沿用原本的 `TestContext`（遊戲自己的），不改寫成 doctest；ctest 名 `object_connect.logic`，標籤 `cpu`。
- **先搬純邏輯、再寫成 System**：純邏輯原樣搬、原測試全過，確認移植沒有改壞規則；PR 2 才把 `Game::Impl` 的一幀拆成 System／Resource，差異只在接線。
