# 渲染（`engine/modules/render`）與 `render_world`

更新：2026-11-25（VSync `v8-arch-gyo2-window-mode-vsync`，[事前宣告](window-mode-vsync-declaration.zh-Hant.md)第 4.3 節：第 2 節末的 `Presentation`、第 5 節的登記、RD-10）；2026-11-24（R2b 的 3D 段 `v8-arch-gyo2-sdl-r2b-3d`：第 8 節的 3D pass、合成、`mesh_render`、CPU 蒙皮，`RenderCommands::passes` 改成 2D／3D 的 variant，RD-8、RD-9）；2026-11-24（R2b 的 2D 段 `v8-arch-gyo2-sdl-r2b-2d`：`Pass2D::viewport`、extract schedule、第 7 節的 2D 管線、RD-5～RD-7；新頁是 SDL 線子批 R2a `v8-arch-gyo2-sdl-r2a`）。由來：[SDL 總宣告](sdl-declaration.zh-Hant.md)修訂 21（使用者 2026-11-23 動工前核准「SDL 照建議核准」，HANDOFF `v8-arch-gyo2-sdl-declaration-approved`）第 3.3、6.1、9.2、10.2 節的 R2a 列；Q7＝(a)（`-plugin-in-world-rule-removed`）、讀法 2（`-sdl-system-calls-via-adapter`）。後端 `sdl_gpu` 寫在 [SDL 的 Plugin 頁](sdl.zh-Hant.md)第 7 節。Design Invariants 照新 AGENTS §2.6：沒有推翻使用者決定的條文，不逐條核准（主會話 2026-11-24）。

Owner：架構。程式：`engine/modules/render/`（`GYO::Engine::Render`，target `GYO::Render`，函式庫）、`engine/modules/render_world/`（`GYO::Engine::RenderWorld`，target `GYO::RenderWorld`，登記單位 `RenderWorldModule`）；R2b 的 2D 段加 `world_render`、`camera_render`、`sprite_render`（第 7 節）。行為測試：`tests/common/render/`（ctest `render.data`、`render.world`、`render.pipeline2d`、`render.extract`）、`plugins/sdl_gpu/tests/`（`sdl_gpu.pass2d` 等）。

## 一個例子

Object_Connect 的選單畫一塊半透明的黑底、上面一張按鈕圖：遊戲（之後經 R2b 的可畫元件與 Extract，今天在渲染 World 裡用一個 System）在 `Update` 往 `RenderCommands` 放一個 2D pass——`view` 是 1280×720 的遊戲座標，第一個 draw 用白貼圖 `TextureId{}`、頂點色 `{0,0,0,160}` 畫一個矩形，第二個 draw 用按鈕圖的 `TextureId`、`Alpha` 混合。按鈕圖在開場時經 `RenderTextures::Create` 拿到 id（像素照 `image` 的排法）。這一幀的 `Submit` stage，`sdl_gpu` 的 `GPUSubmit` 先建按鈕圖的 GPU 貼圖、再照順序畫兩個 draw、送到視窗；`Cleanup` stage 把 pass 與貼圖的要求清掉，貼圖本身留著。視窗被縮成 2560×1440：`view` 不變，畫面整個放大兩倍；裁切框也跟著放大。

## 1. 位置與依賴

```text
GYO::Base、GYO::Math、GYO::Kernel（只為 HashValue）、asset／image／world_asset／transform／camera（只取型別）◄── GYO::Render
GYO::Render、GYO::Orchestration ◄── GYO::RenderWorld
GYO::Render ◄── GYO::WorldRender（來源側）
GYO::Render、GYO::RenderWorld ◄── GYO::CameraRender ◄── GYO::SpriteRender（渲染側）
GYO::Render、GYO::RenderWorld、GYO::SDL ◄── GYO::SDLGPU（plugins/sdl_gpu）
```

- `render`：`library_only=yes`；`depends=asset;image;world_asset;transform;camera`——R2b 加的，**只取型別**（可畫元件的圖、來源的 `AssetView`、`GlobalTransform`、相機；總宣告第 3.3 節「函式庫可以指向有單位的列，只取型別」、第 10.1 節）。不含 SDL、不含 GPU 物件、沒有後端介面（B'）。
- `render_world`：渲染側的單位（`default=no`、`depends=render`），放在要畫的那個 World。
- `world_render`（來源側）、`camera_render`、`sprite_render`（渲染側）：第 7.2 節。
- 沒有後端（沒選 `sdl_gpu`）時資料照樣每幀做好、清掉，沒有人畫（總宣告第 9.2 節第 3 條、Q4）。

## 2. `RenderCommands`：這一幀要畫什麼（資料）

`RenderCommands{passes}` 是渲染 World 的 Resource（`render_world` 登記），每個 pass 照順序畫。R2a 只有一種 pass：**2D pass**（貼圖、混合、裁切）；R2b 的 3D 段把 `passes` 改成 `std::variant<Pass2D, Pass3D>` 的序列（`RenderPass`，第 8 節），2D 與 3D 照相機的順序交錯。

| 欄位 | 意思 |
|---|---|
| `target` | 畫到哪個目標，用 `camera` 的 id 空間（`CameraTarget::id`）：0 是主目標（有視窗時是視窗）。其他 id 對到什麼是後端的事；R2a 的 `sdl_gpu` 只對 0，別的 pass 不畫、計數 |
| `clear` | 先把**整個目標**（不只 `viewport`；同 GPU 的清除）清成這個顏色——黑邊就是這樣來的。2D 管線只把它給一個目標這一幀的第一個相機（第 7.4 節）。某個目標這一幀的第一個 pass 沒給時清成透明黑（視窗的畫面在幀開始時是未定的）；之後的 pass 沒給就疊在已有的畫面上 |
| `viewport` | （R2b）這個 pass 畫在目標的哪一塊像素（相機的視區；黑邊）。沒給＝整個目標；超出目標的部分切掉 |
| `view` | 拉滿 `viewport` 的那個矩形（view 單位，y 向下）。沒給＝`viewport` 自己的像素（1 單位＝1 像素、原點在 `viewport` 左上角的像素） |
| `vertices`、`indices` | 這個 pass 所有 draw 的頂點與索引各一份。`Vertex2D`＝位置、uv、顏色（20 bytes，和 Dear ImGui 的 `ImDrawVert` 同一排法）；索引是 32 位元——ImGui 預設的 `ImDrawIdx` 是 16 位元，K 定 `#define ImDrawIdx unsigned int` 或轉換；ImGui 的 `ImDrawCallback`（draw 之間插自己的指令）表達不了，K 用到時再定 |
| `draws` | 照順序畫；後面的蓋在前面的上面。`Draw2D`＝貼圖、混合、裁切、`indexOffset`／`indexCount`／`vertexOffset`（索引加 `vertexOffset` 指到這個 pass 的頂點） |

- **座標**：view 單位、y 向下；uv (0,0) 是貼圖的左上角、(1,1) 是右下角，和 `image` 的 `ImageData` 一樣（第 0 列在最上面、不翻轉；I #301 交來的約定）。**上下只在 `ViewToClip` 轉一次**（y 向下的 view → y 向上的 clip space；SDL_GPU 的 NDC、viewport、貼圖座標各後端已經統一，`SDL_gpu.h` 的 Coordinate System 一節），uv 不轉（master 坑 17 的單一邊界）。
- **顏色**：照存的 bytes（彩色圖是 sRGB 編碼）、alpha 是 straight。2D pass 直接拿 bytes 混合，同 Dear ImGui、SDL_Renderer；目標與貼圖都是 UNORM。3D 要在線性空間打光時（R2b）另定，見範囲の限界。
- **混合**：`Alpha`（straight alpha：顏色＝src×a＋dst×(1−a)，alpha＝a＋dst.a×(1−a)；Bevy sprite 的 `BlendState::ALPHA_BLENDING`，`bevy_sprite_render/src/render/mod.rs:289`）、`Opaque`（取代，含 alpha；Bevy 的 `AlphaMode2d::Opaque`，`bevy_sprite_render/src/mesh2d/material.rs:264`）。Object_Connect 的 renderer 只用一般的 alpha 混合（`PuzzleRenderer.cpp:493-502`）。
- **裁切**：`clip`（view 單位）讓**中心在裡面**的像素通過——左、上邊含，右、下邊不含（光柵化自己的規則，所以對齊整像素的裁切是精確的），再切到 `viewport` 裡（`ClipToPixels`）。沒給＝整個 `viewport`。`sdl_gpu` 把 `viewport` 交給 GPU 的 viewport（Bevy 的 `set_camera_viewport`，`bevy_render/src/render_phase/draw_state.rs:686`），裁切框交給 scissor。
- **白貼圖**：`TextureId{}`（0）一直都在、不建也不放，畫出來就是頂點色（不貼圖的純色形狀，Object_Connect 盤點的 R2a 項）。
- **R2a 不帶的欄位**：總宣告 P27 列的 shader id、實例數——沒有消費者（2D pass 只有一組內建 shader、沒有實例），R2b 的 3D 段（3D、自訂 shader）有需要時加。視區是 R2b 的 2D 段加的 `viewport`；相機不進 `RenderCommands`（2D 管線在 CPU 把頂點換成 `viewport` 的像素，第 7.4 節）。
- **幀時間的預算**（總宣告第 10.2 節 R2a 列「幀時間的預算先宣告」）：Object_Connect 大小的一幀——1280×720、一個 pass、2,000 個各自一次 draw 的半透明矩形——經 `sdl_gpu` 的 `Capture`（套用、上傳、畫、讀回並等 GPU，比畫到視窗多了讀回）**8 ms 以內**（60 Hz 一幀的一半）。量法：`sdl_gpu.budget`（label `gpu`，只印、不判：GPU 與 driver 決定它，Lavapipe 比任何玩家的 GPU 都慢）。本機（macOS Intel、Metal、不開 SDL 的驗證）2026-11-24 量三次：4.89、4.97、5.25 ms。超過時看 RC-RD 2。
- **呈現方式 `Presentation`**（使用者 2026-11-25「VSync 要」）：渲染 World 的 Resource（`render_world` 登記，新 World 放預設；不在 `RenderCommands` 裡，`Cleanup` 不清它）——主目標（目標 0＝視窗）的畫面怎麼送上螢幕。欄位 `presentMode`：`AutoVsync`（預設；等螢幕更新、不撕裂）、`AutoNoVsync`（不等：立即，不支援時 mailbox，再不行退回 `AutoVsync`）——Bevy 的 `PresentMode` 裡會自己退回的兩個值（`bevy_window/src/window.rs:1229`；退回鏈 `bevy_render/src/view/window/mod.rs:882-911`）。遊戲中改它即時生效：後端（`sdl_gpu`）每幀讀、變了才改 swapchain、實際取到的模式由後端回報（[SDL 的 Plugin 頁](sdl.zh-Hant.md)第 7 節）。**偏離 Bevy**：Bevy 的 `present_mode` 在 `Window` 上、`bevy_render` 抽出視窗來讀；GYO 的視窗不是實體、`window` 在列 `sdl_window` 的 World，渲染 World 沒有視窗（[window 頁](window.zh-Hant.md)偏離表第 1 列），`sdl_gpu` 只讀渲染 World（SDL-5）；present mode 是 swapchain 的性質、swapchain 歸渲染這一側，所以放渲染 World。衝突點：設定放在遊戲 World 的雙 World 遊戲，要在渲染 World 的 `Extract` 用 `ExtractRes<自己的設定>` 寫 `Presentation`（一個 System）；單一 World 直接寫。膠水量：一個 Resource。`Fifo`、`Immediate`、`Mailbox` 的指定值沒有消費者，不帶（加值不破壞相容）。
- **契約**：draw 的索引範圍超出 pass 的 `indices` 是 Programmer Error（後端斷言）；索引值指到 pass 外的頂點不檢查（每個索引都查是逐值的守衛，Bevy／wgpu 也不查；由寫的一方保證）。

## 3. `RenderTextures`：CPU 像素的貼圖（id 由資料這一側配）

總宣告第 10.2 節 R2a 列的「CPU 像素的貼圖：建立與局部更新、id 誰配」。使用者：K 的 ImGui 字型圖集、U 的字形圖集、圖片。

- **id 誰配**：`RenderTextures`（渲染 World 的 Resource）在 `Create` 時給 id、從 1 起、只增不重用（`TextureId{}` 是白貼圖）。後端拿 id 當鍵建自己的 GPU 物件。這樣 `RenderCommands` 是純資料、和後端無關，測試不需要後端就能比內容。同 Bevy：貼圖的 id 是資料那一側的 `AssetId`，渲染側 `RenderAssets` 用它當鍵（`bevy_render/src/render_asset.rs:220`）。master 是裝置配 handle（`IRenderDevice::CreateTexture` 回 `TextureHandle`，`engine/render/include/engine/render/IRenderDevice.hpp:16`），資料就得等後端回話——不照 master。
- **要求**：`Create`（大小、取樣方式、全部像素）、`Update`（一塊區域與它的像素）、`Release`；照順序記在 `Pending()`，後端在畫之前照順序套用。像素照 `image` 的排法（RGBA8、straight alpha、第 0 列在最上面、列與列之間不留空）。`Update`／`Release` 一個不在的 id、區域超出、像素的大小不對，是 Programmer Error。
- **取樣**：`Linear`（預設）、`Nearest`（點陣圖、照原大小畫的字）。邊緣 clamp。
- **生命**：要求留到這一幀畫完（`Cleanup` stage 由 `render_world` 清）；貼圖留到 `Release`。在 `Submit` 之前的任何地方（含開局、準備函式）提的要求都會被那一幀的後端看到。
- **後端做不到時**（貼圖太大、記憶體不夠）：那個 id 沒有 GPU 貼圖，用它的 draw 跳過，兩者都計數（`sdl_gpu` 的 `SDLGPUStatus`）；不是 System 的錯誤。
- **新的 World**：新的 `RenderTextures` 從 1 重新配；後端看到換了 World 就先放掉前一個 World 的貼圖（`sdl_gpu` 第 7 節）。

## 4. Shader 與 ABI

- HLSL 原始碼放在 `render`（`engine/modules/render/shaders/`：`pass2d.vert.hlsl`、`pass2d.frag.hlsl`、共用的 `Pass2D.hlsli`），C++ 這一側是 `ShaderAbi.hpp`；兩邊一起改（RD-4）。
- ABI：頂點輸入 location 0／1／2＝位置（float2，offset 0）、uv（float2，8）、顏色（4 bytes normalized，16），stride 20；頂點的 uniform slot 0＝`ViewTransform`（一個 float4：xy＝scale、zw＝offset）；片段的 sampler slot 0＝draw 的貼圖。register space 照 SDL_GPU 的 HLSL 慣例（頂點 uniform 在 space1，片段的貼圖、sampler 在 space2）。只用 float4 的 lane，各 API 不會排得不一樣（同 master 的 `gyo.raster.v1`）。
- 編譯不在 `render`：`sdl_gpu` 的 shader 工具在建置時編成平台要的格式、嵌進 `sdl_gpu`（[SDL 的 Plugin 頁](sdl.zh-Hant.md)第 7 節）。`render` 不認得任何 shader 格式。

## 5. `render_world`

照 Bevy 的 RenderApp（`bevy_render/src/lib.rs`）：

- **登記**：`render` 的 `RenderCommands`、`RenderTextures`（新 World 是空的）、`Presentation`（新 World 是預設 `AutoVsync`；帶過來的值留著）。雜湊函式什麼都不寫（渲染 World 不是被判定的狀態）。
- **stage**：在 `RenderWorldOptions::schedule`（預設 `Main`）的 `after`（預設 `Last`）之後插兩個 stage：`Submit`（後端畫、送出；Bevy 的 `RenderSystems::Render`，`bevy_render/src/lib.rs:231`）、`Cleanup`（`ClearRenderFrame` 清掉這一幀的 pass 與貼圖要求；Bevy 的 `RenderSystems::Cleanup`，`:233`）。寫 `RenderCommands` 的 System 放在 `Submit` 之前的任何 stage。
- **清在幀尾、不在幀頭**：Bevy 也是在 `Cleanup` 清。若在 `First` 清，開局清單或準備函式提的貼圖要求會在第一個 `Submit` 前被清掉。
- **extract schedule**（R2b 的 2D 段加）：World 的描述有 `extractFrom` 時，`render_world` 宣告 extract schedule `Extract`（一個同名的 stage）、插在 `RenderWorldOptions::schedule`（預設 `Main`）之前（總宣告第 6.1 節第 3 點；Bevy 的 `ExtractSchedule` 由 `SubApp::extract` 在渲染的 schedule 之前跑，`bevy_render/src/lib.rs:438`）。`camera_render`、`sprite_render`（之後 K 的 draw list）把 extract 的 System 掛在這裡。**沒有 `extractFrom` 的 World 不宣告**：自己的 System 寫 `RenderCommands` 的 World（R2a 的測試、Object_Connect 移植的單一 World）照舊，不會因為帶了 `render_world` 就變成 O-E9。判斷靠編排層新給的 `WorldSetup::ExtractFrom()`（O-12 的描述欄位，讓單位看得到；`orchestration.extract` 的 `[or-ex-setup-*]`）。

## 6. 比過的

| 題 | Bevy | 其他 | GYO R2a |
|---|---|---|---|
| 渲染的資料 | RenderApp 的實體＋phase item（`bevy_render`） | master：固定的 scene＋overlay 佇列 `RenderQueue`（`engine/render/include/engine/render/RenderQueue.hpp`）；Dear ImGui：draw list（頂點、索引、裁切、貼圖 id） | 一份 `RenderCommands`（pass → draws），形狀照 ImGui 的 draw list，K 直接交得進來；排序、合批的邏輯（R2b）是 `render` 的函式，產出還是這份資料 |
| 送出 | `render_system` 跑 render graph、submit、present（`bevy_render/src/renderer/mod.rs:82`），在 `RenderSystems::Render` | master：`Renderer` 呼叫 `IRenderDevice`（後端介面在 `engine/render`） | `sdl_gpu` 的 `GPUSubmit` 在 `Submit` stage（Q7＝(a)）；`engine/` 沒有後端介面（B'） |
| 貼圖的 id | 資料側的 `AssetId`（`render_asset.rs:220`、`prepare_assets` `:393`） | master：裝置配 handle | 資料側配（第 3 節） |
| 每幀清空 | `Cleanup`／`PostCleanup` | — | `Cleanup` stage |

偏離 Bevy 的地方：沒有 render graph 與 phase（R2a 只有一種 pass，R2b 再對照 Bevy 的 graph／phase 定，總宣告第 10.2 節 R2b 列）；渲染 World 的資料是一個 Resource、不是實體（實體對應在 R2b 逐種判，總宣告第 6.1 節第 8 點）。膠水量：無（`RenderCommands` 是後端讀的資料）。

## 7. 2D 管線（R2b 的 2D 段，`v8-arch-gyo2-sdl-r2b-2d`）

總宣告第 10.2 節 R2b 列拆成兩段（卡片照 L17 判，主會話開卡時指示「Object_Connect 的 2D 需求先做」）：**2D 段**（本節：讀相機、合成、渲染 World 的實體對應〔2D 的種類〕、疊層、讀 `GlobalTransform`、Object_Connect 的兩件）與 **3D 段**（`r2b-3d`：3D 場景、蒙皮、viewmodel 相機；PvP）。

### 7.1 例子

Object_Connect 用兩個 World 時：遊戲 World 帶 `transform`、`camera`、`world_asset`、`world_render`；渲染 World 帶 `render_world`、`camera_render`、`sprite_render`、`sdl_gpu`，描述寫 `extractFrom = "Game"`。作者在遊戲 World 生成相機（`Camera2d`、`Transform`、`Letterbox{1280, 720}`，投影改 `ScalingFixed{1280, 720}`）、每個節點一個 `Sprite`（圖是 `Handle<ImageData>`，z=2）、每條血管一個 `Shape2d`（`TriangleStrip`，每幀由 Verlet 粒子鏈換一次頂點，z=1）。HUD 用第二個相機（`order=1`、`ClearColorConfig::None`、`RenderLayers{2}`），HUD 的東西也放 `RenderLayers{2}`。每幀：遊戲 World 跑完 → 渲染 World 的 `Extract` 抽出相機與 2D 物件、第一次看到的圖建成貼圖 → `Update` 的 `Queue2dPasses` 每個相機產一個 `Pass2D`（相機的視區、清除色、z 從小到大）→ `Submit` 由 `sdl_gpu` 畫。視窗拉成 2560×1600：`camera` 算出的視區是 (0, 80) 2560×1440，上下各 80 像素是清除色（黑）；滑鼠的視窗座標照舊用 `ViewportToWorld2d` 換回遊戲座標（相機頁第 2 節）。

### 7.2 單位與 World 的組法

| 列 | 角色 | 做什麼 |
|---|---|---|
| `world_render` | 來源側（`default=no`，依賴 `render`） | 登記可畫元件 `Sprite`、`Shape2d`、`RenderLayers`、相機的 `ClearColorConfig` 與 Resource `ClearColor`（沒有才放：不透明黑）。不跑 System（總宣告第 3.3 節命名 `world_<x>`） |
| `camera_render` | 渲染側（依賴 `render`、`render_world`） | `ExtractCameras`（`Extract` schedule）：重建 `ExtractedCameras`——啟用中、`ComputedCamera` 有效、有 `GlobalTransform` 的相機，照 `order`、再照來源實體排；帶目標、視區與投影、視點、2D／3D 標記、清除色（`ClearColorConfig` 對來源的 `ClearColor`）、`RenderLayers` |
| `sprite_render` | 渲染側（依賴 `render`、`render_world`、`camera_render`） | `ExtractSprites2d`（`Extract`）：重建 `Extracted2dItems`、經 `RenderImages` 建貼圖；`Queue2dPasses`（`SpriteRenderOptions`，預設 `Main`／`Update`）：每個 2D 相機一個 `Queue2d` 的 pass，接在 `RenderCommands` 後面 |

- **讀相機的 extract 另立一列**（總宣告第 10.1 節「定的時候回頭驗 KeepTool」）：放進 `render_world` 的話，每個帶 `render_world` 的渲染 World 都要求來源登記相機的型別（R-E4）；KeepTool 的渲染 World 只抽 ImGui 的 draw list。驗過：來源只有 `entity_ids`、渲染 World 只有 `render_world` 時建得起來（`render.extract` 的 `[re-keeptool]`）。
- **來源要帶的**：`transform`、`camera`、`world_render`、`world_asset`（`sprite_render` 經 `ExtractRes<AssetView>` 讀圖；漏了是 R-E4）。都只在來源 World，渲染 World 不必帶它們。
- **圖從來源的 `AssetView` 讀**：遊戲 World 這一幀拉到的圖，渲染 World 同一幀就畫得出來；用渲染 World 自己的 `AssetView` 會晚一幀（它的 extract 在自己的 `First` 之前）。

### 7.3 渲染 World 的實體對應：逐種判（總宣告第 6.1 節第 8 點）

| 種類 | Bevy | GYO（2D 段） | 理由 |
|---|---|---|---|
| 相機 | 有對應實體（`SyncToRenderWorld`，`bevy_render/src/camera.rs:69`；`extract_cameras`，`:486`） | **每幀暫時的資料**（`ExtractedCameras`，一個 Resource） | 2D 段的相機在渲染 World 沒有自己的狀態（沒有貼圖目標、沒有 TAA 的歷史）；有了再改成以來源實體為鍵的 Resource |
| sprite | 每幀暫時（`ExtractedSprites`，`bevy_sprite_render/src/render/mod.rs:353`） | 每幀暫時（`Extracted2dItems`） | 同 Bevy |
| 每幀給頂點的 2D 形狀 | （Bevy 的 `Mesh2d` 是素材） | 每幀暫時 | 作者每幀換頂點（總宣告第 6.4 節 (1) 「每幀用暫時的資料」） |
| 圖的 GPU 貼圖 | `RenderAssets<GpuImage>`，以素材 id 為鍵（`render_asset.rs:393`） | `RenderImages`，以**圖**為鍵（素材 id＋life，或作者持有的 `ImageData` 的位址） | 留著的是圖，不是實體 |

所以 2D 段**沒有渲染側的實體**，總宣告列的四個衝突點（對照表放哪、增刪的通知誰清、同步要動兩個 World、元件被拿掉的通知）在 2D 段都不出現：每幀重建，來源刪掉的實體下一幀就不在（`[re-gone]`）。來源被替換（R1b）時也沒有以舊實體為鍵的對照要作廢；`RenderImages` 的鍵是素材 id 與 life，換 World 不受影響。3D 段的網格（Bevy 是以來源實體為鍵的 `RenderMeshInstances`）在 3D 段再判。

**圖的貼圖什麼時候放掉**：素材的貼圖留著，直到同一個素材 id 以新的 life 再被要求（它的 Handle 全放掉之後再要求，asset 給新的 life；舊 life 的圖不可能再出現，所以建新貼圖時放掉舊的，`[p2-image-new-life]`）或渲染 World 結束（Bevy 在素材被移除時放）；作者持有的圖（字形圖集）某一幀沒有任何物件用它就放掉（`ImageData` 不可變：新圖是新的值、新的位址；`RenderImages` 拿著 `shared_ptr`，位址不會被重用）。素材還沒載好的物件這一幀跳過（同 Bevy）。

### 7.4 排序、合批、座標

- **相機的先後**：`order` 小的先畫，同 `order` 照來源實體（Bevy 照 `order` 與目標排、同值的先後不定，`camera.rs:836`；GYO 用實體讓它確定）。每個相機一個 `Pass2D`：`target`＝相機的目標 id、`viewport`＝`ComputedCamera::viewport`。
- **清除**（照 Bevy；3D 段起由 `camera_render` 的合成做，第 8.2 節）：一個目標這一幀**第一個**畫的相機清整個目標（只有 `clear` 的 `Pass2D`，黑邊）；之後的相機有清除色時**只清自己的視區**——蓋滿視區、清除色的不透明矩形（`Opaque`），所以分割畫面的第二個相機不會把第一個相機的半邊清掉（`[re-split-*]`、`[cp-*]`）；`None` 的相機什麼都不清、疊在前面的畫面上。Bevy 的視窗輸出每幀只在第一次使用時清（`bevy_render/src/texture/texture_attachment.rs:408-433` 的 `is_first_call`），每個相機的畫面放回輸出時用 scissor 限在自己的視區（`bevy_core_pipeline/src/upscaling/node.rs:91-97`）；GYO 沒有每個相機的中間貼圖，用矩形得到同樣的結果。
- **2D 物件的先後**：只畫 `RenderLayers` 和相機有交集的；照位置的 z 從小到大（後畫的蓋在前面；Bevy `Transparent2d` 的 sort key＝z，`bevy_sprite_render/src/render/mod.rs:504`；和 X1 的 `Default2d` 一致：z 大的在前），同 z 照來源實體、同一個實體（同時有 `Sprite` 與 `Shape2d`）照抽出的順序（sprite 先）——每個平台都一樣（Bevy 是 `sort_unstable`，先後不定）。位置在近平面之前或遠平面之後的整個不畫（只看位置，不切三角形）。
- **合批**：相鄰、同貼圖、同混合的物件併成一個 draw（Bevy 的 sprite 也是相鄰同圖併批，`mod.rs:615`）。不跨貼圖重排（會改變半透明的先後）。
- **座標**：每個頂點在 CPU 經「物件的 `GlobalTransform` → 相機的 clip-from-world」換成 clip space，再換成 `viewport` 的像素（y 向下）；`Pass2D::view` 不給。上下在這裡轉一次（clip 的 y 向上 → 像素的 y 向下），之後 `ViewToClip` 再轉回去——兩次都在換算函式裡，資料路徑沒有翻轉（master 坑 17 的單一邊界：uv 照 `image` 的第 0 列在上，sprite 的上緣接 v=0）。不加 shader 的矩陣：2D pass 的 ABI 不變（RD-4），K 的 draw list 照用；CPU 的成本見第 7.5 節與 RC-RD 4。
- **`Shape2d`**：`TriangleList` 或 `TriangleStrip`（strip 在 `TriangleListOf` 展開，奇數三角形對調前兩點，保持朝向）；沒給索引＝頂點照順序。索引指到頂點外不檢查（同第 2 節的契約）。
- **`Sprite`**：大小＝`customSize`，否則 `rect` 的大小，否則圖的像素大小（沒圖是 1×1）；`anchor` 照 Bevy（−0.5～0.5，0 是中心）；`flipX`／`flipY`；`color` 乘上圖的顏色；`filter`（點陣圖用 `Nearest`）。

### 7.5 幀時間的預算

和第 2 節同一個預算：Object_Connect 大小的一幀（1280×720、一個相機、2,000 個半透明方塊）經 `Queue2d`（排序、頂點換算、合批）再 `Capture`，**8 ms 以內**。量法：`sdl_gpu.budget` 的 `[gp-budget-pipeline]`（只印、不判）。本機（macOS Intel、Metal）2026-11-24：直接組 `Pass2D` 4.70 ms、經 `Queue2d` 5.37 ms（多 0.67 ms 是 CPU 的排序與換算）。**沒有量的**：`ExtractSprites2d`（每個物件複製素材 id 的字串、查一次 `RenderImages`、`Shape2d` 的頂點複製一份）——真的遊戲接上兩個 World 時量。超過時看 RC-RD 2、RC-RD 4。

### 7.6 疊層（HUD）

照 Bevy：HUD 是第二個 2D 相機——`order` 比世界的相機大、`ClearColorConfig::None`、`RenderLayers` 用另一層，HUD 的 sprite、形狀、文字放同一層。沒有另一條「疊層管線」。螢幕座標的 HUD：HUD 相機用 `ScalingWindowSize`（或和世界相機同一個 `ScalingFixed`＋`Letterbox`，HUD 跟著縮），`viewportOrigin` 改 (0, 1) 就是左上原點（y 仍向上）。游標畫在 HUD 文字之上：游標放 z 更大的位置或第三個相機。

**文字**：2D 段沒有 `Text2d` 元件。作者用 `text` 排版、點陣化進自己持有的圖集（`std::shared_ptr<const ImageData>`），每個字形一個 `Shape2d` 的四邊形（或整段一個）。字形的位置要在**最終目標的像素**上排（`text.zh-Hant.md`「使用 `text` 的一方要照做的」第 1 點），所以縮放的 HUD 要用乘過縮放的字級排、再除回去，否則字會糊——一個 `Text2d` 元件（Bevy 的 `Text2d`）在 U 或有需要的遊戲再加。

### 7.7 比過的

| 題 | Bevy | 其他 | GYO（2D 段） |
|---|---|---|---|
| 渲染 World 怎麼拿到來源的資料 | `ExtractSchedule`，每幀抽；相機同步實體，sprite 每幀重建 | master：遊戲每幀往 `RenderQueue` 提交 scene＋overlay（`engine/render/include/engine/render/RenderQueue.hpp`，固定兩層、沒有相機的 order） | Extract；2D 的種類全部每幀重建（第 7.3 節） |
| 疊層 | 第二個相機＋`RenderLayers`＋`ClearColorConfig::None` | master：固定的 overlay 佇列；Godot：`CanvasLayer`〔知〕 | 照 Bevy |
| 固定解析度的黑邊 | 沒有內建：作者改 `Camera.viewport`（或 `ScalingMode::AutoMin` 露出多的區域） | Godot：專案設定的 stretch aspect `keep`〔知〕；master：UI 的 `FitDesignCanvas`（等比、置中，`engine/ui/include/engine/ui/UiRuntime.hpp:31`，origin/master） | `camera` 的 `Letterbox` 元件（推導值的視區，不寫作者的 `Camera.viewport`：它進雜湊，視窗大小每台機器不同）；相機頁第 2 節 |
| 2D 的先後 | z（`FloatOrd`），同值不定 | Godot：`z_index` 再照樹的順序〔知〕 | z，同值照實體 |
| 頂點換算 | GPU（view uniform＋每個 sprite 的仿射） | master：sprite 在後端換算 | CPU（2D pass 的 ABI 不變；RC-RD 4） |

偏離 Bevy 的地方與膠水量：相機在渲染 World 沒有實體（每幀暫時的資料；膠水：無，`ExtractedCameras` 就是 Bevy `ExtractedCamera` 的欄位）；頂點在 CPU 換算（膠水：`Queue2d` 一個函式）；`RenderLayers` 只有 32 層（Bevy 不限；要更多時換成位元組陣列，加法）；`ClearColor` 預設黑（Bevy 是深灰）。

### 7.8 Object_Connect 的兩件與兩條路

總宣告第 6.4 節的兩件：(1) **每幀給頂點的 2D 形狀**＝`Shape2d`（`TriangleStrip` 給血管的帶、`TriangleList` 給矩形與圓、不貼圖時是白貼圖）；(2) **固定解析度的縮放**＝`camera` 的 `Letterbox`＋`ScalingFixed`（黑邊，`[re-resize]`）或只用 `ScalingFixed`（拉伸），滑鼠換回遊戲座標用 `ViewportToWorld2d`（含視區偏移），warp 用 `WorldToViewport`（盤點表第 3 節「G3 的反方向」）。

移植卡的第 3 段（單一 World、遊戲自己組 `Pass2D`、`view`＝1280×720）用 R2a 就畫得出來；要黑邊時設 `Pass2D::viewport`（值用 `camera::Compute`＋`Letterbox` 算）。改成兩個 World（本節的路）時，排序、黑邊、HUD 由管線做。選哪條是移植卡的判斷（經主會話轉）。

### 7.9 VFX（R7，預見不實作）

粒子、拖尾、閃光這類效果的形狀和 Object_Connect 的血管一樣是「每幀給頂點」：2D 先用 `Shape2d`（每幀重建，第 7.3 節）就做得出來。預見的需求與接點：(1) 大量粒子時 CPU 換算超過預算 → RC-RD 4 的 GPU 換算或實例化（`RenderCommands` 加實例數，總宣告 P27 已列）；(2) 加色混合（`BlendMode::Add`，加一個值、`sdl_gpu` 多一個 pipeline）；(3) 自訂 shader（遊戲的 shader）→ 第一個帶遊戲 shader 的批做「共通內容管線從已選 Plugin 的登記資料找到 `sdl_gpu` 的編譯工具」（P22 的讀法 (a)，SDL 頁第 7 節）。2D 段沒有遊戲 shader（Object_Connect 原專案的 shader 都是 KamataEngine 內建的 sprite／shape），所以 P22 在 2D 段不觸發。

## 8. 3D 管線與合成（R2b 的 3D 段，`v8-arch-gyo2-sdl-r2b-3d`）

PvP 的盤點（`games/object_fps_pvp/docs/porting-inventory.zh-Hant.md` 第 2.3 節 R2b 列、PV5、PV7）要的：世界相機（near 0.05／far 150）、viewmodel 用另一個相機並清深度、不測深度的世界疊層、半透明、貼圖、蒙皮的角色、2D 的 HUD 疊在最上面。

### 8.1 例子

PvP 的 Client：遊戲 World 有世界相機（`Camera3d`、`order 0`）、viewmodel 相機（`Camera3d`、`order 1`、`ClearColorConfig::None`、`RenderLayers{2}`、近平面 0.01）、HUD 相機（`Camera2d`、`order 2`、`None`、`RenderLayers{4}`）。每個角色一個實體：`Mesh3d{model, mesh}`＋`SkinPose`（遊戲照 tick 取樣、`ComputeModelSpace` 算出每個節點的模型空間矩陣，使用者定的第 2 層由遊戲做）。手臂與槍放 `RenderLayers{2}`。每幀：`Extract` 抽出相機、網格、sprite → `Update` 的 `Queue3dPasses`、`Queue2dPasses` 把每個相機的 pass 放進它的格子 → `PostUpdate` 的 `ComposeCameraPasses` 照相機順序寫進 `RenderCommands`：清整個目標 → 世界的 `Pass3D` → viewmodel 的 `Pass3D`（深度重新清空，所以手臂不會被牆擋住）→ HUD 的 `Pass2D`。

### 8.2 合成

`camera_render` 的 `CameraPasses`：每個抽出的相機一格（`ExtractCameras` 清空）；`sprite_render`、`mesh_render` 把那個相機的 pass 放進去；`ComposeCameraPasses`（`CameraRenderOptions`，預設 `Main`／`PostUpdate`，在管線的 `Update` 之後、`Submit` 之前）照相機順序寫進 `RenderCommands`，每個相機的清除在它的 pass 之前（第 7.4 節的清除規則，`Compose.hpp`）。沒有合成的話，哪個管線先跑就先畫：HUD 的 2D 相機會畫在世界的 3D 相機之前。Bevy 的相機也照 order 排（`sort_cameras`，`bevy_render/src/camera.rs:836`），每個相機跑自己的 phase。

### 8.3 `Pass3D` 與 `mesh_render`

- **資料**：`Pass3D{target, viewport, clipFromWorld, vertices, indices, draws}`；`Draw3D{mesh, indexOffset, indexCount, vertexOffset, world, color, texture, blend, depthTest, depthWrite, cull}`；`Vertex3D`＝位置、法線、uv（32 bytes；3D pass 只綁位置與 uv，法線留給之後打光）；`RenderMeshes`（`render_world` 登記）：資料側配 id 的保留網格，同 `RenderTextures`（Bevy 的 `RenderAssets<RenderMesh>`）。`MeshId{}` 的 draw 用 pass 自己的頂點（蒙皮後的、每幀變的）。
- **深度**：預設每個 `Pass3D` 的深度從清空開始、顏色不清（相機的清除是它前面的那個 pass）。等於 Bevy 每個相機有自己的深度：viewmodel 相機在世界相機之後畫、自己的深度，就是盤點表 PV7 的「清深度」。作者可以讓一個 3D 相機**接著用**前面的相機留下的深度：相機實體放 `Camera3dDepth{clear = false}`（Bevy 的 `Camera3dDepthLoadOp::Load`，`bevy_camera/src/components.rs:60-72`；`Pass3D::clearDepth`）——這一幀沒有 pass 寫過的深度是未定的，同 Bevy。
- **半透明不寫深度**（照 Bevy：`AlphaMode::Blend` 不寫深度，`bevy_pbr/src/render/mesh.rs:3531-3547`）：`Mesh3d::depthWrite` 沒給時，不透明寫、半透明不寫，交錯的半透明網格不會互相擋住；作者要時明給 `true`。
- **shader**：無光照（unlit）——貼圖×`color`，同 master（盤點表 R2a 列「unlit 的 shader」）。顏色照 bytes（同 2D pass；RC-RD 3 不觸發，因為不打光）。HLSL 在 `render/shaders/mesh3d.*.hlsl`，ABI 在 `ShaderAbi.hpp`（RD-4）。
- **先後**（RD-8）：不透明、測深度的近到遠（被近的擋住的像素少算；**偏離 Bevy**：Bevy 的 `Opaque3d` 是 binned phase，照 pipeline、材質、網格分組，不照距離，`bevy_core_pipeline/src/core_3d/mod.rs:172-188`、`:275`——照距離排的代價是 pipeline 與貼圖可能來回切換，沒有量；PvP 的 draw 數少），半透明、測深度的遠到近（Bevy 的 `Transparent3d`），不測深度的（世界疊層：方向指示、命中標記）最後、遠到近；距離＝物件原點在視空間的 z。
- **`Mesh3d`**（來源側，`world_render` 登記）：模型素材的 `Handle`、網格索引、`color`、`image`（`ImageRef`，沒給是白）、混合、深度測試／寫入、`CullMode`（`Back`＝螢幕上順時針的不畫，逆時針是正面，同 Bevy 的 `FrontFace::Ccw`；預設 `None`）。模型自己的材質（`Material::baseColor`、貼圖檔名）是資料，作者讀了填進 `color`／`image`——材質系統（Bevy 的 `StandardMaterial`）不在本批。
- **實體對應**（總宣告第 6.1 節第 8 點，逐種判的 3D 那一種）：網格每幀暫時（`Extracted3dItems`）；保留的是**模型網格**（以素材 id、life、網格索引為鍵的 `RenderModelMeshes`，RD-9）。Bevy 的 `RenderMeshInstances` 以來源實體為鍵、留每實例的 GPU 資料；GYO 的 world 矩陣與材質每個 draw 推一次 uniform，沒有每實例的 GPU 狀態要留，所以不需要對應實體——四個衝突點在 3D 段也不出現（RC-RD 5 寫了什麼時候要改）。
- **沒有蒙皮的網格**放在它的節點上（`SkinPose` 合模型的節點數就用它，否則 rest pose；`PlaceMesh`），整個模型再由實體的 `GlobalTransform` 放。

### 8.4 蒙皮（骨骼動畫第 3 層）：CPU

`-skeletal-animation-layers` 留給 R2b 判「GPU 或 CPU」。**判：CPU**（`SkinVertices`：每個頂點＝Σ 權重×節點的模型空間矩陣×inverse bind×位置，照 model 頁第 2.1 節的式子；法線用矩陣的旋轉部分、再正規化）。

| | CPU（選） | GPU（Bevy） |
|---|---|---|
| 做法 | 每幀在渲染 World 算頂點、放進 pass 自己的頂點上傳 | joint palette 放 storage buffer、頂點帶 joints／weights、vertex shader 混合 |
| PvP 的量 | 同時最多 4 個身體＋1 個 viewmodel、每頂點 4 個權重（盤點表第 4.6 節）——CPU 的量小 | 一樣畫得出 |
| 要多的 | 無（3D pass 一組 shader、一種頂點格式） | 第二組 shader 與頂點格式、storage buffer、shader 工具多檢查一種資源 |
| master | CPU（`ModelRenderer.cpp:85,131`），PvP 已經這樣跑 | — |

理由：來源側的介面（`SkinPose`＝每個節點的模型空間矩陣）兩種做法都一樣，上層的遊戲、網路、動畫都不依賴它；換成 GPU 只動 `mesh_render` 與 `sdl_gpu` 的內部（RC-RD 6）。所以先選少的那一個，之後換也不會被上層的程式卡住（建機制時要提防的是「保守的選項被上層蓋住、沒人回頭改」，這一題的上層不碰它）。

### 8.5 比過的

| 題 | Bevy | master | GYO（3D 段） |
|---|---|---|---|
| 一幀的組成 | 每個相機自己的 render graph 子圖、照 order | 固定的 scene → viewmodel → overlay（`RenderQueue`、`MeshLayer`） | 照相機 order 合成（第 8.2 節）；viewmodel 是一個普通的相機 |
| viewmodel 的深度 | 每個相機自己的深度 | `MeshLayer::ViewModel` 前清深度 | 每個 `Pass3D` 自己清深度 |
| 蒙皮 | GPU | CPU | CPU（第 8.4 節） |
| 網格的保留 | `RenderAssets<RenderMesh>`、`RenderMeshInstances` | 後端的 handle | `RenderMeshes`（資料側 id）、`RenderModelMeshes`（模型網格為鍵） |
| 著色 | PBR（`StandardMaterial`） | unlit＋exposure／gamma | unlit；exposure／gamma 不在本批（見範囲の限界） |

偏離 Bevy 與膠水量：沒有 PBR 與材質系統（PvP 是 unlit；有點名的產品再加）；CPU 蒙皮（膠水：`SkinVertices` 一個函式）；網格實例不留 GPU 狀態（每個 draw 推 uniform；大量相同網格時 RC-RD 2 的實例化）；不透明照距離排、不照 pipeline 分組（第 8.3 節）；法線用整個矩陣換（不是 inverse-transpose：非等比縮放時法線會歪，unlit 用不到，打光時改）。

**和直接寫 `RenderCommands` 的一方的先後**：`ComposeCameraPasses` 在 `PostUpdate`。同一個 World 裡在 `Update` 直接往 `RenderCommands` 放 pass 的一方（例：ImGui 的繪製那半、R2a 式的遊戲自己的 pass）會壓在所有相機**底下**，而且第一個相機因此不再是「目標上第一個」——它的清除變成只清視區，黑邊留著那一方畫的東西。要蓋在相機之上就排在 `PostUpdate` 之後、`Submit` 之前（例：`Last`）。

## Design Invariants

- **RD-1 `render` 只有資料與中性的邏輯**：`RenderCommands`、`RenderTextures`、可畫元件、2D 管線的函式、換算函式、HLSL 原始碼與 ABI；沒有 SDL、沒有 GPU 物件、沒有後端介面、不依賴任何 Plugin（B'）；依賴的 Module（asset、image、model、world_asset、transform、camera）只取型別（R2b）；其中 `transform` 是給渲染側的單位（`camera_render`、`sprite_render` 讀 `GlobalTransform`）經 `render` 拿到型別用的，`render` 自己的程式不 include 它——不是多餘的依賴。守衛：連結檢查（`tests/common/render/CMakeLists.txt` 的 `gyo_expect_links`：`gyo_render` 是 Base、Kernel、Math 與上面六個；`render_world`、`world_render`、`camera_render`、`sprite_render` 各自列）；SDL 的原始碼掃描在 K 之後立（總宣告第 9.1 節），之前由每批的設計審查看。
- **RD-2 一幀的資料在 `Submit` 被看到、在 `Cleanup` 清掉**：`Submit` 在 `after` 那個 stage 之後、`Cleanup` 在 `Submit` 之後；`ClearRenderFrame` 清 pass 與貼圖要求、不動貼圖本身。守衛：`render.world`（`[rw-submit-after-last]`、`[rw-submit-sees]`、`[rw-cleanup-*]`、`[rw-prepare-ask-kept]`）。
- **RD-3 貼圖的 id 由資料側配、不重用；白貼圖是 0**：守衛：`render.data`（`[rd-tex-*]`）。
- **RD-4 shader 的兩邊一起改**：`ShaderAbi.hpp` 與 `Pass2D.hlsli` 是同一份 ABI；`sdl_gpu` 從編好的 SPIR-V 讀出的資源數和 `ShaderAbi.hpp` 的一致。守衛：`sdl_gpu.shaders`（`[sg-abi-*]`）、`render.data` 的 `[rd-abi-*]`、`sdl_gpu.pass2d` 的像素比對（頂點排法錯了顏色就不對）。

- **RD-5 extract schedule 只在有 `extractFrom` 的 World**（R2b）：`render_world` 在那種 World 宣告 `Extract`、插在畫面的 schedule 之前；沒有 `extractFrom` 的 World 沒有它。守衛：`render.extract`（`[re-extract-before-main]`、`[re-no-extract-without-source]`、`[re-keeptool]`）、`orchestration.extract`（`[or-ex-setup-*]`）。
- **RD-6 抽出的 2D 資料每幀重建，留著的只有以圖為鍵的貼圖**（R2b，第 7.3 節）：`ExtractedCameras`、`Extracted2dItems` 每次 extract 整個換掉；`RenderImages` 以素材（id＋life）或作者持有的圖為鍵，同一張圖只建一次；作者持有的圖沒人用就放掉，素材的留著。守衛：`render.extract`（`[re-gone]`、`[re-dynamic]`、`[re-texture-once]`、`[re-held-released]`、`[re-asset-*]`：真的 PNG 經 world_asset，載好的那一幀就畫）、`render.pipeline2d`（`[p2-image-*]`）。
- **RD-7 相機的先後、清除與 2D 的先後**（R2b，第 7.4 節）：一個目標第一個相機清整個目標，之後的相機只清自己的視區；相機照 `order`、再照來源實體；一個相機只畫 `RenderLayers` 有交集的物件，照位置的 z 從小到大、同 z 照來源實體；近平面之前、遠平面之後的不畫；相鄰同貼圖同混合才合批。守衛：`render.pipeline2d`（`[p2-z-*]`、`[p2-layers]`、`[p2-near-far]`、`[p2-batch*]`、`[p2-map-*]`、`[p2-viewport]`、`[p2-clear-*]`）、`render.extract`（`[re-camera-order]`、`[re-hud]`、`[re-split-*]`）、`sdl_gpu.pass2d`（`[gp-pipe-*]`，Lavapipe 上的像素）。

- **RD-8 3D 的先後**（R2b 的 3D 段，第 8.3 節）：一個 3D 相機一個 `Pass3D`，深度從清空開始、顏色不清；先畫不透明、測深度的（近到遠），再畫半透明、測深度的（遠到近），最後畫不測深度的（遠到近）；同距離照來源實體；只畫 `RenderLayers` 有交集的。守衛：`render.pipeline3d`（`[p3-*]`）、`sdl_gpu.pass3d`（`[g3-*]`，Lavapipe 上的像素）。
- **RD-9 模型網格以模型網格為鍵保留**（第 8.3 節）：沒有蒙皮的模型網格第一次畫時建成 `RenderMeshes` 的網格、以（素材 id、life、網格索引）為鍵；同一個素材的新 life 放掉舊的；蒙皮的網格每幀在 CPU 算、放在 pass 自己的頂點。守衛：`render.pipeline3d`（`[p3-mesh-*]`、`[p3-skin-*]`、`[p3-place-*]`）、`render.extract`（`[re-3d-*]`）。

- **RD-10 呈現方式是渲染 World 的資料、`Cleanup` 不清**（第 2 節末）：`render_world` 登記 `Presentation`、新 World 放預設；遊戲改的值留到它再改；後端讀它、不寫它。守衛：`render.world`（`[rw-present-default]`、`[rw-present-kept]`）、`sdl_gpu.frames`（`[gp-present-*]`）。

## Replacement Criteria

- **RC-RD 1**：第二個非 SDL 的後端（或 SDL_Renderer 之類的另一條路）要 `RenderCommands` 露出 SDL_GPU 才有的概念才畫得出來 → 資料切錯了，重切（RC-SDL 1 的 render 那一半）。
- **RC-RD 2**：一幀的 `Vertex2D`／索引的上傳成為幀時間的主要部分（R2b 的幀時間預算先宣告，計測）→ 改成保留式的頂點緩衝（Bevy 的 batch／retained），不是改格式。
- **RC-RD 3**：3D 在線性空間打光（R2b）和 2D pass 照 bytes 混合在同一個目標上畫不出一致的顏色 → 重定目標格式與 2D 的混合空間。
- **RC-RD 4**（R2b）：2D 管線在 CPU 換算頂點（`Queue2d`）成為幀時間的主要部分（`[gp-budget-pipeline]` 超過第 7.5 節的預算）→ 換算移到 GPU（2D pass 帶 view 矩陣、物件帶仿射或實例），改 ABI 時 `ShaderAbi.hpp` 與 `Pass2D.hlsli` 一起改（RD-4）。
- **RC-RD 6**（R2b 的 3D 段）：CPU 蒙皮（第 8.4 節；joint palette 是每個網格算一次、沒有蒙皮的實體每幀重算 rest pose）成為幀時間的主要部分（蒙皮的頂點數×實體數），或一幀要上傳的蒙皮頂點超過預算 → 改成 GPU 蒙皮（joint palette 放 storage buffer、頂點帶 joints／weights，同 Bevy）；來源側的介面（`SkinPose`）不變。
- **RC-RD 5**（R2b）：相機在渲染 World 需要跨幀的狀態（貼圖目標、TAA 的歷史、遮擋查詢）→ 相機改成以來源實體為鍵的 Resource 或對應實體，第 7.3 節的四個衝突點在那時解。

## 範囲の限界

- 內建的 shader 只有 2D pass 與 unlit 的 3D pass；沒有 shader id、實例數、打光、材質系統。3D 段**沒有**：後處理（PvP 盤點的 exposure／gamma——master 的 `SceneColorTransform`；要的時候是在 `Pass3D` 之後加一個全畫面的 pass，交 PvP 移植時定）、MSAA、陰影、視錐剔除、3D 的幀時間預算（3D 段沒有量；PvP 的場景接上時宣告與量測）。模型網格的保留（`RenderModelMeshes`）與素材的貼圖一樣留到渲染 World 結束或同一個素材換 life——換地圖但不換 World、又放掉舊模型的 Handle 時，舊的 GPU buffer 不會放（素材卸載的通知有需要時再接）。深度貼圖在目標大小改變時重建，沒有測試（`sdl_gpu.pass3d` 都是 8×8）。
- 2D 段沒有：`Text2d` 元件（第 7.6 節）、可見性元件（Bevy 的 `Visibility`；不畫就拿掉元件或換層）、視錐剔除（只看位置的近遠平面）、相機畫到貼圖（其他目標 id 照第 2 節，後端不畫、計數）、加色混合（第 7.9 節）。
- 顏色照 bytes 混合（sRGB 編碼的值直接線性內插）：半透明的邊緣比線性空間混合暗一點，同 ImGui 與多數 2D 引擎；R2b 帶 3D 時重看（RC-RD 3）。
- 貼圖只有 RGBA8；沒有 mipmap、沒有壓縮格式、沒有渲染到貼圖（R2b 的合成、疊層需要時加）。
- 一個 Product 只有一個 World 列 `sdl_gpu`（一個視窗、一張貼圖表）；多個渲染 World 各有一個 `GPUSubmit` 時一個視窗一幀只能送出一次——沒有點名的 Feature，有了再定（總宣告第 10.2 節 R2a 列）。
- 一個來源的 `AssetView` 拿不到的圖（另一個 Product 的素材）不畫；圖的貼圖不做 mipmap。
