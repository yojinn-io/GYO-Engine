# 渲染（`engine/modules/render`）與 `render_world`

更新：2026-11-28（A4 修訂 1，宣告第 16.8 節：Hdr 相機每台一張中間圖、每個目標一張共用深度、tonemapping 對不是第一台的相機混合、`PassFullscreen::blend`、`ColourSpace` 的規則、`Exposure` 移到 B4、RD-13 拆成 RD-13～RD-15）；2026-11-28（HDR 與 tonemapping：`Hdr`、`Tonemapping`、`Exposure`、`Rgba16Float`、tonemapping pass，渲染管線線 A4 `v8-arch-gyo2-render-a4`，[渲染管線宣告](render-pipeline-declaration.zh-Hant.md)第 16 節：第 2 節的顏色與全畫面 pass、第 3 節的格式、第 4 節、第 8.2 節、RD-4、RD-11、新 RD-13、RC-RD 3、範囲の限界）；2026-11-27（target 的大小在資料側定、相機的更新模式 `TargetUpdate`、明示沿用 `PassReuse`，渲染管線線 A2 `v8-arch-gyo2-render-a2`，[渲染管線宣告](render-pipeline-declaration.zh-Hant.md)第 14.11 節：第 2 節的 RP-3、第 3 節的 render target、RD-11、RD-12）；2026-11-27（camera phase、全畫面 pass、只寫深度的 pass、讀的宣告與 RP-3，渲染管線線 A2 `v8-arch-gyo2-render-a2`，[渲染管線宣告](render-pipeline-declaration.zh-Hant.md)第 14 節：第 2 節、第 3 節的 `Depth`、第 4 節、第 8.2 節、RD-4、RD-11、新 RD-12）；2026-11-27（render target、貼圖格式、clip-from-world 收一處，渲染管線線 A1 `v8-arch-gyo2-render-a1`，[渲染管線宣告](render-pipeline-declaration.zh-Hant.md)第 13 節：第 2 節的 `target` 與顏色、第 3 節、第 7.2 節、第 7.4 節的座標、第 8.6 節、RD-3、RD-11、範囲の限界）；2026-11-26（交給 render 的模型與每幀上傳上限，大地圖線 `v8-arch-gyo2-big-map-memory`，[大地圖宣告](big-map-declaration.zh-Hant.md)第 17 節：第 8.3 節的 `ModelRef`、RD-9）；2026-11-26（instancing，大地圖線 S6 `v8-arch-gyo2-big-map-s6`，[大地圖宣告](big-map-declaration.zh-Hant.md)第 16 節：第 8.3 節的資料、第 8.7 節、IN-1～IN-3）；2026-11-25（VSync `v8-arch-gyo2-window-mode-vsync`，[事前宣告](window-mode-vsync-declaration.zh-Hant.md)第 4.3 節：第 2 節末的 `Presentation`、第 5 節的登記、RD-10）；2026-11-24（R2b 的 3D 段 `v8-arch-gyo2-sdl-r2b-3d`：第 8 節的 3D pass、合成、`mesh_render`、CPU 蒙皮，`RenderCommands::passes` 改成 2D／3D 的 variant，RD-8、RD-9）；2026-11-24（R2b 的 2D 段 `v8-arch-gyo2-sdl-r2b-2d`：`Pass2D::viewport`、extract schedule、第 7 節的 2D 管線、RD-5～RD-7；新頁是 SDL 線子批 R2a `v8-arch-gyo2-sdl-r2a`）。由來：[SDL 總宣告](sdl-declaration.zh-Hant.md)修訂 21（使用者 2026-11-23 動工前核准「SDL 照建議核准」，HANDOFF `v8-arch-gyo2-sdl-declaration-approved`）第 3.3、6.1、9.2、10.2 節的 R2a 列；Q7＝(a)（`-plugin-in-world-rule-removed`）、讀法 2（`-sdl-system-calls-via-adapter`）。後端 `sdl_gpu` 寫在 [SDL 的 Plugin 頁](sdl.zh-Hant.md)第 7 節。Design Invariants 照新 AGENTS §2.6：沒有推翻使用者決定的條文，不逐條核准（主會話 2026-11-24）。

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

`RenderCommands{passes}` 是渲染 World 的 Resource（`render_world` 登記），每個 pass 照順序畫。R2a 只有一種 pass：**2D pass**（貼圖、混合、裁切）；R2b 的 3D 段把 `passes` 改成 `std::variant<Pass2D, Pass3D>` 的序列（`RenderPass`，第 8 節），2D 與 3D 照相機的順序交錯；A2 加**全畫面 pass**（`PassFullscreen`）與**只寫深度的 pass**（`PassDepth`），`RenderPass` 是四種的 variant（下面「pass 的種類」）。

| 欄位 | 意思 |
|---|---|
| `target` | 畫到哪個目標（`std::optional<TextureId>`，A1）：`nullopt` 是主目標（有視窗時是視窗；`Capture` 時是那張圖）；有值是 `RenderTextures` 的 render target（第 3 節）。`camera_render` 把相機的 `CameraTarget` 換成它對到的 target。沒有圖的目標（不是 target 的 id、已放掉、或後端建不出來）的 pass 不畫、每幀計一次；pass 裡取樣自己這個目標的 draw 跳過、計數（同一張貼圖同時讀寫，GPU 上沒有定義） |
| `clear` | 先把**整個目標**（不只 `viewport`；同 GPU 的清除）清成這個顏色——黑邊就是這樣來的。2D 管線只把它給一個目標這一幀的第一個相機（第 7.4 節）。某個目標這一幀的第一個 pass 沒給時清成透明黑（視窗的畫面在幀開始時是未定的）；之後的 pass 沒給就疊在已有的畫面上 |
| `viewport` | （R2b）這個 pass 畫在目標的哪一塊像素（相機的視區；黑邊）。沒給＝整個目標；超出目標的部分切掉 |
| `view` | 拉滿 `viewport` 的那個矩形（view 單位，y 向下）。沒給＝`viewport` 自己的像素（1 單位＝1 像素、原點在 `viewport` 左上角的像素） |
| `vertices`、`indices` | 這個 pass 所有 draw 的頂點與索引各一份。`Vertex2D`＝位置、uv、顏色（20 bytes，和 Dear ImGui 的 `ImDrawVert` 同一排法）；索引是 32 位元——ImGui 預設的 `ImDrawIdx` 是 16 位元，K 定 `#define ImDrawIdx unsigned int` 或轉換；ImGui 的 `ImDrawCallback`（draw 之間插自己的指令）表達不了，K 用到時再定 |
| `draws` | 照順序畫；後面的蓋在前面的上面。`Draw2D`＝貼圖、混合、裁切、`indexOffset`／`indexCount`／`vertexOffset`（索引加 `vertexOffset` 指到這個 pass 的頂點） |

- **座標**：view 單位、y 向下；uv (0,0) 是貼圖的左上角、(1,1) 是右下角，和 `image` 的 `ImageData` 一樣（第 0 列在最上面、不翻轉；I #301 交來的約定）。**上下只在 `ViewToClip` 轉一次**（y 向下的 view → y 向上的 clip space；SDL_GPU 的 NDC、viewport、貼圖座標各後端已經統一，`SDL_gpu.h` 的 Coordinate System 一節），uv 不轉（master 坑 17 的單一邊界）。
- **顏色**：照存的 bytes（彩色圖是 sRGB 編碼）、alpha 是 straight。2D pass 直接拿 bytes 混合，同 Dear ImGui、SDL_Renderer；目標與貼圖預設都是 UNORM。貼圖可以是 `Rgba8UnormSrgb`（A1，第 3 節）：取樣時解碼成線性、畫進這種 target 時編碼——給之後在線性空間打光的 3D（B4）；2D 的管線不建這種貼圖（RP-4）。**顏色空間**（A4，`ColourSpace`，`RenderTextures.hpp`；[渲染管線宣告](render-pipeline-declaration.zh-Hant.md)第 16.8 節第 2 點）：一個 target 上的顏色代表什麼，由 `ColourSpaceOf(RenderTargetFormat)` 一處決定，清除色、2D、3D 一致：bytes 的 target（`Rgba8Unorm`、主目標）照 bytes；線性的 target（`Rgba16Float`：帶 `Hdr` 的 3D 相機的中間圖）上，每個顏色先換成它代表的光再用、再混合——`Color8`（清除色、頂點色、實例色）與照 bytes 取樣的 texel 解碼，取樣即線性光的 texel（`Rgba8UnormSrgb` 的貼圖、`Rgba16Float` 的 target，`SampledAsLinear`）照原值，不解碼兩次（Bevy 把顏色換成線性給 main texture，`bevy_render/src/view/mod.rs:1442-1450`）。中間圖經 tonemapping pass 的曲線、clamp、sRGB 編碼變回 bytes，寫在 8-bit 的目標上；沒有 sRGB 格式的 target。
- **混合**：`Alpha`（straight alpha：顏色＝src×a＋dst×(1−a)，alpha＝a＋dst.a×(1−a)；Bevy sprite 的 `BlendState::ALPHA_BLENDING`，`bevy_sprite_render/src/render/mod.rs:289`）、`Opaque`（取代，含 alpha；Bevy 的 `AlphaMode2d::Opaque`，`bevy_sprite_render/src/mesh2d/material.rs:264`）。Object_Connect 的 renderer 只用一般的 alpha 混合（`PuzzleRenderer.cpp:493-502`）。
- **裁切**：`clip`（view 單位）讓**中心在裡面**的像素通過——左、上邊含，右、下邊不含（光柵化自己的規則，所以對齊整像素的裁切是精確的），再切到 `viewport` 裡（`ClipToPixels`）。沒給＝整個 `viewport`。`sdl_gpu` 把 `viewport` 交給 GPU 的 viewport（Bevy 的 `set_camera_viewport`，`bevy_render/src/render_phase/draw_state.rs:686`），裁切框交給 scissor。
- **白貼圖**：`TextureId{}`（0）一直都在、不建也不放，畫出來就是頂點色（不貼圖的純色形狀，Object_Connect 盤點的 R2a 項）。
- **R2a 不帶的欄位**：總宣告 P27 列的 shader id、實例數——沒有消費者（2D pass 只有一組內建 shader、沒有實例），R2b 的 3D 段（3D、自訂 shader）有需要時加。視區是 R2b 的 2D 段加的 `viewport`；相機不進 `RenderCommands`（2D 管線在 CPU 把頂點換成 `viewport` 的像素，第 7.4 節）。
- **幀時間的預算**（總宣告第 10.2 節 R2a 列「幀時間的預算先宣告」）：Object_Connect 大小的一幀——1280×720、一個 pass、2,000 個各自一次 draw 的半透明矩形——經 `sdl_gpu` 的 `Capture`（套用、上傳、畫、讀回並等 GPU，比畫到視窗多了讀回）**8 ms 以內**（60 Hz 一幀的一半）。量法：`sdl_gpu.budget`（label `gpu`，只印、不判：GPU 與 driver 決定它，Lavapipe 比任何玩家的 GPU 都慢）。本機（macOS Intel、Metal、不開 SDL 的驗證）2026-11-24 量三次：4.89、4.97、5.25 ms。超過時看 RC-RD 2。
- **呈現方式 `Presentation`**（使用者 2026-11-25「VSync 要」）：渲染 World 的 Resource（`render_world` 登記，新 World 放預設；不在 `RenderCommands` 裡，`Cleanup` 不清它）——主目標（目標 0＝視窗）的畫面怎麼送上螢幕。欄位 `presentMode`：`AutoVsync`（預設；等螢幕更新、不撕裂）、`AutoNoVsync`（不等：立即，不支援時 mailbox，再不行退回 `AutoVsync`）——Bevy 的 `PresentMode` 裡會自己退回的兩個值（`bevy_window/src/window.rs:1229`；欄位 `Window::present_mode` 在 `:166`；退回鏈 `bevy_render/src/view/window/mod.rs:882-911`）。遊戲中改它即時生效：後端（`sdl_gpu`）每幀讀、變了才改 swapchain、實際取到的模式由後端回報（[SDL 的 Plugin 頁](sdl.zh-Hant.md)第 7 節）。**新 World** 是預設值（替換時 VSync 回到開）：要保留就照 O-7 帶過去，或由遊戲的設定重寫——Bevy 的 `present_mode` 在 `Window` 實體上、換場景不重設，GYO 的渲染 World 被替換時資料跟著換（設計審查 S1 選 (a)）。**偏離 Bevy**：Bevy 的 `present_mode` 在 `Window` 上、`bevy_render` 抽出視窗來讀；GYO 的視窗不是實體、`window` 在列 `sdl_window` 的 World，渲染 World 沒有視窗（[window 頁](window.zh-Hant.md)偏離表第 1 列），`sdl_gpu` 只讀渲染 World（SDL-5）；present mode 是 swapchain 的性質、swapchain 歸渲染這一側，所以放渲染 World。衝突點：設定放在遊戲 World 的雙 World 遊戲，要在渲染 World 的 `Extract` 用 `ExtractRes<自己的設定>` 寫 `Presentation`（一個 System）；單一 World 直接寫。膠水量：一個 Resource。`Fifo`、`Immediate`、`Mailbox` 的指定值沒有消費者，不帶（加值不破壞相容）。
- **契約**：draw 的索引範圍超出 pass 的 `indices` 是 Programmer Error（後端斷言）；索引值指到 pass 外的頂點不檢查（每個索引都查是逐值的守衛，Bevy／wgpu 也不查；由寫的一方保證）。
- **pass 的種類**（A2，[渲染管線宣告](render-pipeline-declaration.zh-Hant.md)第 14.3 節）：
  - `PassFullscreen{target, viewport, shader, inputs, parameters}`：一個三角形蓋住 viewport（內建 `fullscreen.vert`，Bevy 的 `FullscreenShader`），fragment shader 照 `inputs` 的順序取樣、`parameters`（bytes，float4 lane）是它的 uniform；顏色直接取代、不混合；起點照目標現在的畫面。`shader` 是後端帶著的 shader 的**名字**（HLSL 檔名去掉 `.hlsl`）；後端沒有那個名字時不畫、計一次 `skippedDraws`。`inputs` 的數目、有沒有 `parameters` 要和 shader 一致（Programmer Error，後端斷言）。名字是 B1 的 `ShaderId` 之前的形狀（RC-A2-3）。`blend`（A4 修訂 1）：`Opaque`（預設，取代）或 `Alpha`（straight alpha 疊上，同 2D 的 draw；Bevy 對不是目標上第一台的相機的 upscaling）。Engine 內建的全畫面 fragment shader 只有 A4 的 `tonemapping.frag`（第 8.2 節）。
  - `PassDepth`：沒有顏色、畫 `Depth` target 的深度（陰影、prepass）。它和 `Pass3D` 共用 `MeshGeometry{viewport, clipFromWorld, vertices, indices, instances}`；它的 draw 是 `MeshDraw`（mesh、範圍、depthTest、depthWrite、cull），`Pass3D` 的 `Draw3D : MeshDraw` 多貼圖與混合。
  - `Pass3D::depth`：有值時這個 pass 測試、寫入那張 `Depth` target（prepass 寫的；要和目標一樣大，Programmer Error），沒有值時照舊用目標自己的深度。
- **明示沿用**（A2，第 14.11～14.14 節）：`RenderCommands::retained` 裡的 target 這一幀不畫、用上一幀的畫面；**程式畫過的**（`WasDrawn`：某一幀的 pass 在它現在的大小畫過）才算這一幀寫過（從沒畫過的沿用不算）。GPU 沒拿到畫它的那一幀時，後端跳過取樣它的 draw、計數，不是 Programmer Error。`camera_render` 沿用「它的相機這一幀都不畫」的 target（`TargetUpdate`、inactive、無效，第 8.2 節）；作者自己的 System 也可以放。
- **讀與寫**（RP-3）：pass 寫的是 `WritesOf(pass)`（顏色 target——`PassDepth` 沒有——與深度 target：`Pass3D::depth`、`PassDepth::target`；取樣自己寫的東西的 draw 跳過、計數）；讀的是 `ReadsOf(pass)`——draw 的貼圖（白貼圖不算）、全畫面 pass 的 `inputs`；`PassDepth` 不讀。GPU 的先後就是 `passes` 的順序：不重排、不剔除、不 aliasing（使用者 Q2）。**「讀的 target 必須這一幀寫過或明示沿用，兩者皆非就斷言」**（使用者 2026-11-27）：`render` 的 `FirstUnwrittenRead` 找出第一個違反的 pass（只看資料；沿用一張程式從沒畫過的不算寫過），後端（`sdl_gpu` 的 `RequireRenderContracts`）在畫之前斷言（第 4.3 節「後端斷言」）。`Pass3D::depth` 的大小也只在資料側比（`FirstDepthOfAnotherSize`）；GPU 上主目標的大小可能晚一幀，不同時跳過那些 draw、計數。

## 3. `RenderTextures`：CPU 像素的貼圖（id 由資料這一側配）

總宣告第 10.2 節 R2a 列的「CPU 像素的貼圖：建立與局部更新、id 誰配」。使用者：K 的 ImGui 字型圖集、U 的字形圖集、圖片。

- **id 誰配**：`RenderTextures`（渲染 World 的 Resource）在 `Create` 時給 id、從 1 起、只增不重用（`TextureId{}` 是白貼圖）。後端拿 id 當鍵建自己的 GPU 物件。這樣 `RenderCommands` 是純資料、和後端無關，測試不需要後端就能比內容。同 Bevy：貼圖的 id 是資料那一側的 `AssetId`，渲染側 `RenderAssets` 用它當鍵（`bevy_render/src/render_asset.rs:220`）。master 是裝置配 handle（`IRenderDevice::CreateTexture` 回 `TextureHandle`，`engine/render/include/engine/render/IRenderDevice.hpp:16`），資料就得等後端回話——不照 master。
- **要求**：`Create`（大小、取樣方式、全部像素）、`Update`（一塊區域與它的像素）、`Release`；照順序記在 `Pending()`，後端在畫之前照順序套用。像素照 `image` 的排法（RGBA8、straight alpha、第 0 列在最上面、列與列之間不留空）。`Update`／`Release` 一個不在的 id、區域超出、像素的大小不對，是 Programmer Error。
- **取樣**：`Linear`（預設）、`Nearest`（點陣圖、照原大小畫的字）。邊緣 clamp。
- **生命**：要求留到這一幀畫完（`Cleanup` stage 由 `render_world` 清）；貼圖留到 `Release`。在 `Submit` 之前的任何地方（含開局、準備函式）提的要求都會被那一幀的後端看到。
- **後端做不到時**（貼圖太大、記憶體不夠）：那個 id 沒有 GPU 貼圖，用它的 draw 跳過，兩者都計數（`sdl_gpu` 的 `SDLGPUStatus`）；不是 System 的錯誤。
- **新的 World**：新的 `RenderTextures` 從 1 重新配；後端看到換了 World 就先放掉前一個 World 的貼圖（`sdl_gpu` 第 7 節）。
- **格式**（A1）：`TextureDesc::format`＝`Rgba8Unorm`（預設；照存的 bytes 取樣）或 `Rgba8UnormSrgb`（R、G、B 是 sRGB 編碼，取樣時解碼成線性；彩色圖 `_D`、`_E` 給 3D 用）。像素的排法兩種一樣（4 bytes）。SDL_GPU 3.4 一張貼圖只有一種格式、不能開別的格式的 view（[渲染管線宣告](render-pipeline-declaration.zh-Hant.md)第 13.1 節），同一張圖兩種讀法都要時是兩張貼圖。Bevy 的彩色圖預設 sRGB（`ImageLoaderSettings::is_srgb`）；這裡預設 UNORM，因為 2D 照 bytes 混合（RP-4）。render target 的格式是另一個型別 `RenderTargetFormat{Rgba8Unorm, Depth}`（A2 修訂 1：sRGB 的 target、Depth 的貼圖寫不出來；sRGB 的 target 上清除色與混合是什麼意思，A4 定線性路徑時再定）。**`Depth`**（A2）：深度，近 0、遠 1；不看 `filter`，一律以最近的 texel 取樣（驅動不一定能線性過濾深度）；後端在 D32F、D24、D16 裡選第一個能當 depth target 又能取樣的（SDL 只保證 D16，`SDL_gpu.h:703,747`；Bevy 固定 `Depth32Float`）；取樣時第一個通道是深度；沒有畫面時第一個畫它的 pass 從清成 1 開始。**`Rgba16Float`**（A4）：每個通道 16-bit 浮點的線性光，超過 1 的值保留（Bevy 的 HDR main texture）；照存的取樣；上面的顏色的意思見第 2 節「顏色空間」。`TextureFormat`（CPU 像素）不加浮點：`TextureDesc` 的像素一律 4 bytes（浮點像素的圖片是 C3）。壓縮（F1）由要的那一批加。
- **render target**（A1）：`CreateTarget(RenderTargetDesc{size, format, filter})` 給一個 id（和圖片在同一張表、共用計數器、不重用）；pass 以 `target = id` 畫進去，之後的 draw 以 `texture = id` 取樣（Bevy 的 `Image::new_target_texture`：一張圖同時是相機的目標與 sprite 的圖）。`size` 是固定的 `TargetSize`，或 `FollowTarget{target}`——跟主目標（`nullopt`，預設）或同一張表裡另一個 target 一樣大。**大小在資料側定**（A2，第 14.11 節）：`RenderTextures` 照描述算出每個 target 的大小（主目標的由 `camera_render` 在 extract 從來源的 `CameraTargetSizes[kMainTarget]` 給，`SetMainSize`），每次變了發一個 `TextureTargetSize` 要求，後端照要求建、重建；`Capture` 的大小不決定 target 的大小。建好或大小變了就沒有畫面（`HasPicture`）；後端在 Submit 把這一幀實際做成的事寫進 `RenderFrameReceipt`（`render_world` 登記；實際畫了的 target、實際套用的大小，一個 target 一筆；SDL-5、A2 第 14.13 節），`render_world` 的 Cleanup 在同一幀交給 `RenderTextures::Apply` 再清空：畫過的有畫面、套用過的大小不再要求，沒套用的每幀再要求。沒有後端（或後端這一幀沒畫成）就沒有畫面。沒有相機的 render World 自己呼叫 `SetMainSize`。以前（A1）由後端照 swapchain 的大小解析（HDR 相機的中間圖跟相機的目標，Bevy 的 `ViewTarget` 也照相機自己的目標大小，`bevy_render/src/view/mod.rs:1416`）。**畫面跨幀保留**：沒給清除色的 pass 畫在上一幀留下的畫面上（Bevy 的 `LoadOp::Load`，`texture_attachment.rs:52`；Godot 的 `VIEWPORT_CLEAR_NEVER`）；只有剛建好或剛重建（大小變了）之後的第一個 pass 從透明黑開始（那時內容本來沒有定義）。主目標照舊：每幀第一個 pass 清。它沒有 CPU 像素：`Update` 一個 target 是 Programmer Error。`Release` 對 target 一樣；跟著它的 target 從此沒有畫面。要求是 `TextureTargetCreate`，和其他要求同一個順序。
- **來源 World 的 target**（A1；`RenderTargets.hpp`）：相機在來源 World，所以作者在來源建 target——`RenderTargets::Create(SourceTargetDesc{size, filter})`（`world_render` 登記的 Resource）回 `CameraTarget`（從 1 起、不重用）。**`kMainTarget`（0）以外的相機目標 id 只有它配**，別人不配、也不寫那些 id 的大小（相機頁 CM-5）。`size` 是固定的，或跟另一個相機目標（主目標或先建的來源 target）。`world_render` 的 `WriteRenderTargetSizes` 把每個 target 的大小照 id 順序抄進 `CameraTargetSizes`（經相機的 `WriteTargetSize`：固定的 scale 1，跟的抄被跟的；放掉的拿掉；`PostUpdate`、`CameraUpdate` 之前），相機同一幀就算得出來。渲染 World 的 `camera_render` 在 extract 用 `SyncTargetTextures` 為每個來源 target 建一張自己的 target（`TargetTextures`：來源 id → `TextureId`；跟來源 target 的，跟它對到的那張）：同一張表裡 id 不重給、描述不變，所以同一個 id 就是同一個 target；**換了一張表**（`RenderTargets::Table()`：新的 `RenderTargets`＝被替換的來源 World；O-7 帶過去的複本是同一張）就把為舊表建的全部放掉，被重配的 id 不會畫到舊 World 的畫面。來源放掉的放掉。相機的 pass 畫在那張上；來源已放掉 target 的相機（在相機算完之後才放）不抽。渲染 World 自己的中間 target（HDR、深度、後處理）直接 `CreateTarget`。

## 4. Shader 與 ABI

- HLSL 原始碼放在 `render`（`engine/modules/render/shaders/`：`pass2d.vert.hlsl`、`pass2d.frag.hlsl`、共用的 `Pass2D.hlsli`；3D 的 `mesh3d.*`、`Mesh3D.hlsli`；A4 的線性 target 上的 `pass2d_linear.frag.hlsl`、`mesh3d_linear.frag.hlsl` 與共用的 `LinearColour.hlsli`（片段 uniform：這個 draw 的貼圖是不是線性取樣）、tonemapping pass 的 `tonemapping.frag.hlsl` 與曲線 `Tonemapping.hlsli`、sRGB 的轉換 `Color.hlsli`（C++ 那一側是 Math 的 `ColorSpace.hpp`）；方法的值是 `include/engine/render/TonemappingMethods.h`（C++ 與 HLSL 共用的一份 `#define`）；A2 的只寫深度 `depth.vert.hlsl`、`depth.frag.hlsl`（3D pass 的頂點輸入與 uniform，沒有顏色；SDL 一定要 fragment shader）與全畫面的 `fullscreen.vert.hlsl`、`Fullscreen.hlsli`（沒有頂點輸入；fragment 的 `FullscreenVarying`、貼圖與 sampler 在 space2 照 `inputs` 的順序、參數是 space3 的 b0）），C++ 這一側是 `ShaderAbi.hpp`；兩邊一起改（RD-4）。頂點結構要照 location 的順序宣告全部輸入（DXC 照宣告順序編 location，不照 `TEXCOORDn` 的 n）。
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
| `world_render` | 來源側（`default=no`，依賴 `render`、`camera`） | 登記可畫元件 `Sprite`、`Shape2d`、`RenderLayers`、可見性的元件 `Aabb`、`NoFrustumCulling`、`VisibilityRange` 與相機的推導值 `CulledEntities`（`visibility` 寫、`camera_render` 要抽，所以型別要在來源 World 登記；第 8.6 節）、相機的 `ClearColorConfig` 與 Resource `ClearColor`（沒有才放：不透明黑）、render target 的 `RenderTargets`（沒有才放：空的；A1）。跑一個 System：`WriteRenderTargetSizes`（`WorldRenderOptions`，預設 `Main`／`PostUpdate`、`CameraUpdate` 之前；第 3 節） |
| `camera_render` | 渲染側（依賴 `render`、`render_world`） | `ExtractRenderTargets`（`Extract`、`ExtractCameras` 之前；A1）：讓 `TargetTextures` 跟上來源的 `RenderTargets`（第 3 節）。`ExtractCameras`（`Extract` schedule）：重建 `ExtractedCameras`——啟用中、`ComputedCamera` 與 `CameraView` 有效的相機，照 `order`、再照來源實體排；帶目標（`CameraTarget` 對到的 pass target；來源沒有那個 target 的相機不抽）、視區與投影、`CameraView`（世界到 clip、視空間；A1 起代替視點的矩陣）、2D／3D 標記、清除色（`ClearColorConfig` 對來源的 `ClearColor`）、`RenderLayers`；相機的 `CulledEntities` 抽進 `ExtractedCamera::culled`（沒有就是空的，第 8.6 節） |
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
- **座標**：每個頂點在 CPU 經「物件的 `GlobalTransform` → 相機的 clip-from-world（`CameraView::clipFromWorld`，相機頁 CM-7；A1 起不在這裡另算）」換成 clip space，再換成 `viewport` 的像素（y 向下）；`Pass2D::view` 不給。上下在這裡轉一次（clip 的 y 向上 → 像素的 y 向下），之後 `ViewToClip` 再轉回去——兩次都在換算函式裡，資料路徑沒有翻轉（master 坑 17 的單一邊界：uv 照 `image` 的第 0 列在上，sprite 的上緣接 v=0）。不加 shader 的矩陣：2D pass 的 ABI 不變（RD-4），K 的 draw list 照用；CPU 的成本見第 7.5 節與 RC-RD 4。
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

`camera_render` 的 `CameraPasses`：每個抽出的相機一格（`ExtractCameras` 清空），每格是五個 **phase** 的列表（A2，`CameraPhase`）：`Prepass`（只寫深度）、`Opaque`（`mesh_render` 的 `Pass3D`，它的混合 draw 在不透明的之後）、`Transparent`（`sprite_render` 的 `Pass2D`）、`PostProcess`（全畫面 pass）、`Overlay`（`ui_render` 的 UI）；另有 `beforeCameras`：不屬於相機的 view 的 pass（點光、聚光的陰影貼圖，Bevy 的 `RootNonCameraView`；方向光的 cascade 跟著相機）。`ComposeCameraPasses`（`CameraRenderOptions`，預設 `Main`／`PostUpdate`，在管線的 `Update` 之後、`Submit` 之前）先寫 `beforeCameras`，再照相機順序寫：清除（第 7.4 節的清除規則，`Compose.hpp`；「目標上第一個相機」＝前面沒有相機在這個目標、`out` 裡也沒有 pass 在它上面畫過顏色），再照 phase 的順序、同一個 phase 照放進去的先後（System 的 before／after）。所以後處理放在 `PostProcess` 就在場景之後，不管它的 System 先跑還是後跑。**相機的更新模式** `TargetUpdate{Always, Once, Disabled}`（A2，第 14.11～14.14 節；仿 Godot SubViewport 的 `render_target_update_mode`，但模式在相機上、判斷在 target；`world_render` 登記，沒有元件＝Always）：每張 target 一個判斷——它的相機這一幀都不畫（Disabled 且有畫面、inactive、無效）而程式畫過它 → 沿用（`ExtractedCameras::retained`，合成放進 `RenderCommands::retained`）；有一台要畫 → 全部照畫；target 沒有後端確認過的畫面（新建、大小變了、從沒畫過、GPU 沒拿到那一幀）就畫（強制重畫）。inactive 的相機（Bevy 的 `is_active = false`）也是「不畫」：程式畫過它的 target 就沿用（GPU 沒拿到那一幀時，後端跳過取樣、計數）。主目標的畫面不跨幀保留，主目標上的相機一定畫。Once 在被看到的那一幀畫：`world_render` 在 Last 記下 Once 的相機、下一幀的 First 改成 Disabled。沒有合成的話，哪個管線先跑就先畫：HUD 的 2D 相機會畫在世界的 3D 相機之前。Bevy 的相機也照 order 排（`sort_cameras`，`bevy_render/src/camera.rs:836`），每個相機跑自己的 schedule，phase 是 system set `Prepass`、`MainPass`、`EarlyPostProcess`、`PostProcess`（`bevy_core_pipeline/src/schedule.rs:49-55`），UI 在 `PostProcess` 之後；這裡的 pass 是資料，所以 phase 是列表（[渲染管線宣告](render-pipeline-declaration.zh-Hant.md)第 14.3 節的偏離表）。

**HDR 與 tonemapping**（A4，[渲染管線宣告](render-pipeline-declaration.zh-Hant.md)第 16 節、修訂 1 第 16.8 節）：相機的元件 `Hdr`、`Tonemapping{method}`（`Drawables.hpp`，`world_render` 登記；照 Bevy；`Exposure` 在 B4 和打光一起加）。`camera_render` 的 `ExtractHdr`（`ExtractCameras` 之後）抽每台相機的曲線，並給這一幀要畫的、帶 `Hdr` 的 3D 相機**一張自己的**中間圖（`Rgba16Float`、`Nearest`、跟著它的目標的大小），給這種相機畫的目標**一張深度**，那個目標上的每台 3D 相機（帶不帶 Hdr）都測試、寫入它（`ExtractedCamera::depth`→`Pass3D::depth`；Bevy 的 `ViewDepthTexture` 以目標為鍵）；`HdrTargets` 記，沒有相機用的放掉。phase 畫在哪裡由 `PhaseTarget(camera, phase)` 決定：Hdr 相機的 `Prepass`～`PostProcess` 在中間圖、`Overlay` 在目標。`Hdr` 在 2D 相機上不看（RP-4）。合成時目標的清除照不帶 Hdr 的規則（之後帶清除色的 Hdr 相機不必清 viewport）；中間圖每一幀清整張（清除色；沒有時透明黑）；`PostProcess` 之後、`Overlay` 之前放 tonemapping pass（`TonemappingPass`：`tonemapping.frag` 在相機的 viewport 裡、在自己的像素讀中間圖，曲線→clamp→sRGB 編碼；**目標上第一個相機取代、之後的 alpha 混合**，Bevy 的 upscaling）——所以 Hdr 相機在它沒畫到的地方留著前面相機畫的東西（背景、前一台的 UI），`ClearColorConfig::None` 也是疊畫。Hdr 相機的 `PostProcess` 在 D1 的互換之前讀改不了中間圖（RC-A4-2）。曲線＝Bevy 不需要 LUT 的全部：`None`、`Reinhard`、`ReinhardLuminance`（**預設**：Bevy 在 LUT 曲線之前的預設；Bevy 現在的 `TonyMcMapface` 要 3D LUT，RC-A4-1）、`AcesFitted`、`SomewhatBoringDisplayTransform`、`KhronosPbrNeutral`。不帶 `Hdr` 的相機像素不變：unlit 不 tonemap（Bevy 的 `TONEMAP_IN_SHADER` 給 B4 的打光材質；這一點與預設曲線等使用者，宣告第 16.8 節開頭）。

### 8.3 `Pass3D` 與 `mesh_render`

- **資料**：`Pass3D{target, viewport, clipFromWorld（相機的 `CameraView::clipFromWorld`）, vertices, indices, instances, draws}`；`Draw3D{mesh, indexOffset, indexCount, vertexOffset, instanceOffset, instanceCount, texture, blend, depthTest, depthWrite, cull}`；`Instance3D{world, color}`（每個實例的位置與顏色，第 8.7 節）；`Vertex3D`＝位置、法線、uv（32 bytes；3D pass 只綁位置與 uv，法線留給之後打光）；`RenderMeshes`（`render_world` 登記）：資料側配 id 的保留網格，同 `RenderTextures`（Bevy 的 `RenderAssets<RenderMesh>`）。`MeshId{}` 的 draw 用 pass 自己的頂點（蒙皮後的、每幀變的）。
- **深度**：預設每個 `Pass3D` 的深度從清空開始、顏色不清（相機的清除是它前面的那個 pass）。等於 Bevy 每個相機有自己的深度：viewmodel 相機在世界相機之後畫、自己的深度，就是盤點表 PV7 的「清深度」。作者可以讓一個 3D 相機**接著用**前面的相機留下的深度：相機實體放 `Camera3dDepth{clear = false}`（Bevy 的 `Camera3dDepthLoadOp::Load`，`bevy_camera/src/components.rs:60-72`；`Pass3D::clearDepth`）——這一幀沒有 pass 寫過的深度是未定的，同 Bevy。
- **半透明不寫深度**（照 Bevy：`AlphaMode::Blend` 不寫深度，`bevy_pbr/src/render/mesh.rs:3531-3547`）：`Mesh3d::depthWrite` 沒給時，不透明寫、半透明不寫，交錯的半透明網格不會互相擋住；作者要時明給 `true`。
- **shader**：無光照（unlit）——貼圖×`color`，同 master（盤點表 R2a 列「unlit 的 shader」）。顏色照 bytes（同 2D pass；RC-RD 3 不觸發，因為不打光）。HLSL 在 `render/shaders/mesh3d.*.hlsl`，ABI 在 `ShaderAbi.hpp`（RD-4）。
- **先後**（RD-8）：不透明、測深度的照**各自最近的實例**排、近到遠（同鍵的併成一個 draw，第 8.7 節的 IN-2）（被近的擋住的像素少算；**偏離 Bevy**：Bevy 的 `Opaque3d` 是 binned phase，照 pipeline、材質、網格分組，不照距離，`bevy_core_pipeline/src/core_3d/mod.rs:172-188`、`:275`——照距離排的代價是 pipeline 與貼圖可能來回切換，沒有量；PvP 的 draw 數少），半透明、測深度的遠到近（Bevy 的 `Transparent3d`），不測深度的（世界疊層：方向指示、命中標記）最後、遠到近；距離＝物件原點在視空間的 z。
- **`Mesh3d`**（來源側，`world_render` 登記）：模型（`ModelRef`：素材的 `Handle`、作者持有的 `shared_ptr<const ModelData>`——執行期產生的幾何，例如大地圖的 region；照 `ImageRef` 的先例，Bevy 是 `Assets<Mesh>::add`，gyo2 的 asset 沒有執行期加入，`v8-arch-gyo2-mesh3d-held`——，或交給 render 的 `shared_ptr<HandedModel>`：GPU 網格做好後 render 拿走沒有蒙皮的網格的頂點與索引，Bevy 的 `RenderAssetUsages::RENDER_WORLD`＋`take_gpu_data`，大地圖宣告第 17 節）、網格索引、`color`、`image`（`ImageRef`，沒給是白）、混合、深度測試／寫入、`CullMode`（`Back`＝螢幕上順時針的不畫，逆時針是正面，同 Bevy 的 `FrontFace::Ccw`；預設 `None`）。模型自己的材質（`Material::baseColor`、貼圖檔名）是資料，作者讀了填進 `color`／`image`——材質系統（Bevy 的 `StandardMaterial`）不在本批。
- **實體對應**（總宣告第 6.1 節第 8 點，逐種判的 3D 那一種）：網格每幀暫時（`Extracted3dItems`）；保留的是**模型網格**（以素材 id、life、網格索引為鍵，或作者持有的模型以位址、網格索引為鍵的 `RenderModelMeshes`，RD-9）。Bevy 的 `RenderMeshInstances` 以來源實體為鍵、留每實例的 GPU 資料；GYO 的 world 矩陣與顏色是每幀重寫的實例資料（第 8.7 節），沒有跨幀的每實例 GPU 狀態要留，所以不需要對應實體——四個衝突點在 3D 段也不出現（RC-RD 5 寫了什麼時候要改）。
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

### 8.6 可見性（視錐剔除與距離；大地圖線 S2，`v8-arch-gyo2-big-map-s2`）

宣告：[大地圖宣告](big-map-declaration.zh-Hant.md)第 13、14 節。照 Bevy 的 `bevy_camera` visibility（`check_visibility_cpu_culling`、`check_visibility_ranges`）。

- **元件**（`Drawables.hpp`，`world_render` 登記，不進雜湊）：`Aabb`（實體本地座標的盒）、`NoFrustumCulling`、`VisibilityRange`（照 Bevy 的兩段 margin＋`useAabb`；看得到＝距離在 `[startMargin.start, endMargin.end)`；漸變要 shader，現在 margin 裡兩個 LOD 都畫）、相機上的 `CulledEntities`。
- **`visibility` Module**（來源側，`default=no`，依賴 render、transform、camera）：`CheckVisibility` 在 frame／PostUpdate、`TransformPropagate` 與 `CameraUpdate` 之後，公開 set `CheckVisibility`。對每個啟用中、有效的 3D 相機，把受管（有 `Aabb` 或 `VisibilityRange`）而看不到的實體寫進該相機的 `CulledEntities`（EntityId 升冪；只在內容改變時寫；第一份用延遲指令插入）。盒子經 `GlobalTransform` 的八個角換成世界 AABB，用 `Math::Intersects(Frustum, Aabb)`；視錐由相機的 `CameraView::clipFromWorld` 做（A1，和管線取同一個值；相機頁 CM-7）。
- **抽取與排隊**：`camera_render` 抽成 `ExtractedCamera::culled`；`Queue3d` 跳過清單裡的實體。`RenderLayers` 照舊。2D 不剔除。

**Design Invariants**

- **VS-1 一處決定**：哪些實體受剔除管、看不看得到，只由 `visibility` 的 `CheckVisibility` 決定；renderer 只問「在不在相機的清單裡」。守衛：審查；突變 `vs-queue-filter-off`、`vs-camera-list-not-extracted`。
- **VS-2 選用**：沒帶 `visibility` 的 World，相機沒有清單，什麼都不剔除；沒有 `Aabb` 也沒有 `VisibilityRange` 的實體永遠畫。守衛：「without the visibility Module…」案、`[vs-frustum]` 的無盒實體。
- **VS-3 只在變了才寫**：`CulledEntities` 內容不變就不寫，`Changed` 只在變了才亮。守衛：`[vs-write-changes]`、突變 `vs-write-every-frame`。

**Replacement Criteria**：RC-VS1（要剔除很多不知道盒子的網格 → 照 Bevy 的 `calculate_bounds` 自動算）、RC-VS2（玩法要讀「這一幀有沒有被看到」→ `ViewVisibility`）、RC-VS3（量到被剔除的項目在抽取的成本超過預算，例如 CPU 蒙皮 → 把剔除移到抽取前）。大地圖宣告第 14 節 S-4（clip-from-world 收成一處）在渲染管線線 A1 做了（相機頁 CM-7）。

### 8.7 instancing（大地圖線 S6，`v8-arch-gyo2-big-map-s6`）

宣告：[大地圖宣告](big-map-declaration.zh-Hant.md)第 16 節。照 Bevy 的自動合批（`bevy_render/src/batching/`）。作者那一側不加元件：同一個模型網格、同一張圖、同樣的混合／深度／cull 的 `Mesh3d` 自動成為一個 draw 的實例。

- **資料**：`Instance3D{world, color}`（68 bytes）；`Pass3D::instances`；每個 `Draw3D` 畫 `[instanceOffset, instanceOffset + instanceCount)` 的實例，畫一個東西＝一個實例。
- **ABI**：頂點 slot 0 每頂點、slot 1 每實例（location 2～5＝world，`Matrix4` 照存放順序的四欄；6＝顏色）；vertex uniform 是 pass 的 `clipFromWorld`；沒有 fragment uniform。
- **`Queue3d`**：鍵＝`MeshId`、`TextureId`、混合、深度測試、深度寫入、cull。
  - 不透明的同鍵進一個 bin，bin 在最近成員的位置，bin 內近到遠。
  - 半透明與不測深度的只併相鄰同鍵的。
  - 自己帶頂點的（CPU 蒙皮）不併。
- **`sdl_gpu`**：實例每幀上傳到一個 instance buffer；slot 1 每個 pass 綁一次，每個 draw 用 `first_instance`＝`instanceOffset` 指出範圍（同 Bevy 的 `draw_indexed(.., batch_range)`；instance-rate 輸入在 Vulkan、D3D12、Metal 都照它開始，只有 shader 讀內建的實例編號時才不一致，這裡不讀）。

**Design Invariants**

- **IN-1 鍵**：只有鍵相同的項目成為同一個 draw 的實例；world 與顏色跟著實例。守衛：`render.pipeline3d`（`[in-*-apart]`、`[in-one-draw]`、`[in-own-apart]`）、`render.extract`（`[in-scene-*]`）；突變 `in-batch-off`、`in-key-*`、`in-own-joined`。
- **IN-2 順序**：不透明的 bin 在最近成員的位置、bin 內近到遠；排序過的組只併相鄰的；GPU 照實例順序畫。守衛：`[in-bin-near-first]`、`[in-bins-by-nearest]`、`[in-blend-neighbours-*]`、`[g3-instances-order]`；突變 `in-blend-binned`、`in-bin-far-first`。
- **IN-3 實例的位置**：每個 draw 從它自己的第一個實例開始（`first_instance`），下一個 pass 從前一個 pass 的實例之後開始。守衛：`sdl_gpu.pass3d`（`[g3-instances-*]`、`[g3-pass-depth-cleared]`）；突變 `in-g3-*`、`in-instance-world-lost`、`in-instance-colour-lost`。

**Replacement Criteria**：RC-IN1（自訂 shader 要每實體的 uniform → `NoAutomaticBatching`）、RC-IN2（draw 數或實例上傳成為主要成本 → 保留式實例或 GPU 剔除＋indirect）、RC-IN3（每實例一個實體的成本超過預算 → 一個實體多個實例的元件，Godot `MultiMesh` 的形狀）。

### 8.5 比過的

| 題 | Bevy | master | GYO（3D 段） |
|---|---|---|---|
| 一幀的組成 | 每個相機自己的 render graph 子圖、照 order | 固定的 scene → viewmodel → overlay（`RenderQueue`、`MeshLayer`） | 照相機 order 合成（第 8.2 節）；viewmodel 是一個普通的相機 |
| viewmodel 的深度 | 每個相機自己的深度 | `MeshLayer::ViewModel` 前清深度 | 每個 `Pass3D` 自己清深度 |
| 蒙皮 | GPU | CPU | CPU（第 8.4 節） |
| 網格的保留 | `RenderAssets<RenderMesh>`、`RenderMeshInstances` | 後端的 handle | `RenderMeshes`（資料側 id）、`RenderModelMeshes`（模型網格為鍵） |
| 著色 | PBR（`StandardMaterial`） | unlit＋exposure／gamma | unlit；exposure／gamma 不在本批（見範囲の限界） |

偏離 Bevy 與膠水量：沒有 PBR 與材質系統（PvP 是 unlit；有點名的產品再加）；CPU 蒙皮（膠水：`SkinVertices` 一個函式）；網格實例不留 GPU 狀態（每幀的實例重寫一次，同 Bevy 的 CPU 路徑；第 8.7 節）；不透明照距離排、不照 pipeline 分組（第 8.3 節）；法線用整個矩陣換（不是 inverse-transpose：非等比縮放時法線會歪，unlit 用不到，打光時改）。

**和直接寫 `RenderCommands` 的一方的先後**：`ComposeCameraPasses` 在 `PostUpdate`。同一個 World 裡在 `Update` 直接往 `RenderCommands` 放 pass 的一方（例：ImGui 的繪製那半、R2a 式的遊戲自己的 pass）會壓在所有相機**底下**，而且第一個相機因此不再是「目標上第一個」——它的清除變成只清視區，黑邊留著那一方畫的東西。要蓋在相機之上就排在 `PostUpdate` 之後、`Submit` 之前（例：`Last`）。

## Design Invariants

- **RD-1 `render` 只有資料與中性的邏輯**：`RenderCommands`、`RenderTextures`、可畫元件、2D 管線的函式、換算函式、HLSL 原始碼與 ABI；沒有 SDL、沒有 GPU 物件、沒有後端介面、不依賴任何 Plugin（B'）；依賴的 Module（asset、image、model、world_asset、transform、camera）只取型別（R2b）；其中 `transform` 是給渲染側的單位（`camera_render`、`sprite_render` 讀 `GlobalTransform`）經 `render` 拿到型別用的，`render` 自己的程式不 include 它——不是多餘的依賴。守衛：連結檢查（`tests/common/render/CMakeLists.txt` 的 `gyo_expect_links`：`gyo_render` 是 Base、Kernel、Math 與上面六個；`render_world`、`world_render`、`camera_render`、`sprite_render` 各自列）；SDL 的原始碼掃描在 K 之後立（總宣告第 9.1 節），之前由每批的設計審查看。
- **RD-2 一幀的資料在 `Submit` 被看到、在 `Cleanup` 清掉**：`Submit` 在 `after` 那個 stage 之後、`Cleanup` 在 `Submit` 之後；`ClearRenderFrame` 清 pass 與貼圖要求、不動貼圖本身。守衛：`render.world`（`[rw-submit-after-last]`、`[rw-submit-sees]`、`[rw-cleanup-*]`、`[rw-prepare-ask-kept]`）。
- **RD-3 貼圖的 id 由資料側配、不重用；白貼圖是 0**：render target 和圖片共用同一個計數器（A1）。守衛：`render.data`（`[rd-tex-*]`、`[rd-target-*]`）；突變 `rd-tex-*`、`rd-target-own-counter`（`tests/common/render/target_mutations.json`）。
- **RD-4 shader 的兩邊一起改**：`ShaderAbi.hpp` 與 `Pass2D.hlsli`（3D 的 `Mesh3D.hlsli`、全畫面的 `Fullscreen.hlsli`）是同一份 ABI；`sdl_gpu` 從編好的 SPIR-V 讀出的資源數和 `ShaderAbi.hpp` 的一致。A4 的曲線（`Tonemapping.hlsli`）與測試的 CPU 參考（`tests/common/render/TonemappingReference.hpp`）是同一份式子；方法的值兩邊都取 `TonemappingMethods.h`。守衛：`sdl_gpu.shaders`（`[sg-abi-*]`、`[g3-abi-*]`、`[sg-phase-*]`、`[sg-hdr-*]`）、`render.data` 的 `[rd-abi-*]`、`sdl_gpu.pass2d`／`sdl_gpu.phases`／`sdl_gpu.hdr` 的像素比對（頂點排法或 uv 的方向錯了顏色就不對）。

- **RD-5 extract schedule 只在有 `extractFrom` 的 World**（R2b）：`render_world` 在那種 World 宣告 `Extract`、插在畫面的 schedule 之前；沒有 `extractFrom` 的 World 沒有它。守衛：`render.extract`（`[re-extract-before-main]`、`[re-no-extract-without-source]`、`[re-keeptool]`）、`orchestration.extract`（`[or-ex-setup-*]`）。
- **RD-6 抽出的 2D 資料每幀重建，留著的只有以圖為鍵的貼圖**（R2b，第 7.3 節）：`ExtractedCameras`、`Extracted2dItems` 每次 extract 整個換掉；`RenderImages` 以素材（id＋life）或作者持有的圖為鍵，同一張圖只建一次；作者持有的圖沒人用就放掉，素材的留著。守衛：`render.extract`（`[re-gone]`、`[re-dynamic]`、`[re-texture-once]`、`[re-held-released]`、`[re-asset-*]`：真的 PNG 經 world_asset，載好的那一幀就畫）、`render.pipeline2d`（`[p2-image-*]`）。
- **RD-7 相機的先後、清除與 2D 的先後**（R2b，第 7.4 節）：一個目標第一個相機清整個目標，之後的相機只清自己的視區；相機照 `order`、再照來源實體；一個相機只畫 `RenderLayers` 有交集的物件，照位置的 z 從小到大、同 z 照來源實體；近平面之前、遠平面之後的不畫；相鄰同貼圖同混合才合批。守衛：`render.pipeline2d`（`[p2-z-*]`、`[p2-layers]`、`[p2-near-far]`、`[p2-batch*]`、`[p2-map-*]`、`[p2-viewport]`、`[p2-clear-*]`）、`render.extract`（`[re-camera-order]`、`[re-hud]`、`[re-split-*]`）、`sdl_gpu.pass2d`（`[gp-pipe-*]`，Lavapipe 上的像素）。

- **RD-8 3D 的先後**（R2b 的 3D 段，第 8.3 節）：一個 3D 相機一個 `Pass3D`，深度從清空開始、顏色不清；先畫不透明、測深度的（照各自最近的實例近到遠，見 IN-2），再畫半透明、測深度的（遠到近），最後畫不測深度的（遠到近）；同距離照來源實體；只畫 `RenderLayers` 有交集的。守衛：`render.pipeline3d`（`[p3-*]`）、`sdl_gpu.pass3d`（`[g3-*]`，Lavapipe 上的像素）。
- **RD-9 模型網格以模型網格為鍵保留**（第 8.3 節；2026-11-26 加交出的模型與每幀上限）：交給 render 的模型（`HandedModel`）以它自己的位址為鍵；它的每個沒有蒙皮、有幾何的網格都做好的那一幀拿走頂點與索引，留 nodes 與蒙皮的網格，GPU 網格放掉後畫不回來（同 Bevy）。`MeshRenderOptions::meshBytesPerFrame`（Bevy 的 `RenderAssetBytesPerFrame`，預設不限）是一幀內新網格位元組的軟上限：做到超過為止，超過的那一個仍做，之後的留到下一幀、那一幀不畫。守衛：`render.pipeline3d`（`[p3-bytes-*]`、`[p3-handed-*]`）、`render.extract`（`[mh-handed-*]`）。沒有蒙皮的模型網格第一次畫時建成 `RenderMeshes` 的網格、以（素材 id、life、網格索引）為鍵；同一個素材的新 life 放掉舊的；作者持有的模型（`ModelRef` 的 `shared_ptr`）以（位址、網格索引）為鍵，保留期間握著它，在沒有任何 `Mesh3d` 畫它的那一幀放掉（`ExtractMeshes3d` 的結尾呼叫 `EndFrame`，release 跟著那一幀的 asks 送給後端；鍵與 holder 由 `ResolveModel` 一處產生，同 `ResolveImage`）。和 Bevy 的差別：Bevy 在最後一個 Handle 丟掉時才放 GPU 網格，就地修改 `Assets<Mesh>` 會觸發重傳；這裡是一幀沒畫就放，也不察覺就地修改——作者要換幾何就給新的 `ModelData`（持有的要當作不可變）；蒙皮的網格每幀在 CPU 算、放在 pass 自己的頂點。守衛：`render.pipeline3d`（`[p3-mesh-*]`、`[p3-skin-*]`、`[p3-place-*]`）、`render.extract`（`[re-3d-*]`）。

- **RD-11 render target 與格式**（A1，第 3 節）：target 是 `RenderTextures` 的貼圖（和圖片同一張表），pass 以它的 id 為目標、draw 以它的 id 取樣、取樣自己這個目標的 draw 跳過並計數；跟別的目標的 target 每幀和被跟的一樣大；render target 的內容跨幀保留（沒有清除色的 pass 疊在上一幀的內容上），只有剛建或剛重建後的第一個 pass 從透明黑開始；取樣要這一幀畫過或明示沿用（A2，RP-3）；大小由資料側的 `RenderTextures` 解析、以 `TextureTargetSize` 要求後端（A2，第 14.11 節）；主目標以外的相機目標 id 只由來源的 `RenderTargets` 配，大小由 `world_render` 經相機的 `WriteTargetSize` 抄進 `CameraTargetSizes`（`CameraUpdate` 之前），渲染 World 為每個建一張自己的 target（同一個不重建、換了表全部重建、沒了放掉），相機畫在那張上、沒有對應的相機不抽；貼圖的格式照存、`Rgba8UnormSrgb` 取樣解碼成線性；target 的格式是 `RenderTargetFormat`：`Rgba8Unorm`、`Depth`（A2：深度以最近的 texel 取樣、後端選具體格式）與 `Rgba16Float`（A4：線性的光，清除色解碼），和貼圖的 `TextureFormat` 分開；沒有 sRGB 格式的 target。守衛：`render.data`（`[rd-target-*]`、`[rd-format-*]`、`[rt-*]`）、`render.extract`（`[wr-target-*]`、`[re-target-*]`）、`sdl_gpu.targets`（`[gt-*]`，Lavapipe 上的像素）；突變 `tests/common/render/target_mutations.json`。

- **RD-12 phase、pass 的種類與讀寫的宣告**（A2，第 2 節、第 8.2 節；[渲染管線宣告](render-pipeline-declaration.zh-Hant.md)第 14 節）：合成照「不屬於相機的 view → 相機順序（清除 → `Prepass` → `Opaque` → `Transparent` → `PostProcess` → `Overlay`）」，同一個 phase 照放進去的先後；`mesh_render` 放 `Opaque`、`sprite_render` 放 `Transparent`、`ui_render` 放 `Overlay`；全畫面 pass 以名字指 shader、照順序取樣 `inputs`、參數是它的 uniform、取代顏色；只寫深度的 pass 畫 `Depth` target、`Pass3D::depth` 測試並寫入它；`ReadsOf` 是 draw 的貼圖（白的不算）與全畫面的 `inputs`；`WritesOf` 是顏色 target 與深度 target，「目標上第一個相機」與「取樣自己寫的東西」都照它。GPU 的先後＝`passes` 的順序（RP-3）：取樣這一幀沒畫過也沒沿用的 render target 是 Programmer Error、後端斷言；`PassReuse` 算寫過；`TargetUpdate` 不畫的幀由 `camera_render` 放沿用，target 沒有畫面時照畫。守衛：`render.data`（`[rd-pass-*]`、`[rd-depth-target]`）、`render.pipeline3d`（`[cp-phase-*]`、`[cp-non-camera-*]`）、`render.extract`（`[re-phase-*]`）、`sdl_gpu.shaders`（`[sg-phase-*]`）、`sdl_gpu.phases`（`[gf-*]`、`[gd-*]`，Lavapipe 上的像素）、`sdl_gpu.unwritten`（`[gt-unwritten-asserted]`）；突變 `tests/common/render/phase_mutations.json`。

- **RD-13 Hdr 相機的 target**（A4，第 8.2 節；[渲染管線宣告](render-pipeline-declaration.zh-Hant.md)第 16、16.8 節）：這一幀要畫的、帶 `Hdr` 的 3D 相機各有一張自己的中間圖（`Rgba16Float`、`Nearest`、跟著它的目標的大小）；這種相機畫的目標有一張深度（`Depth`），那個目標上的每台 3D 相機都用它；沒有相機用的放掉；同一台相機在同一個目標上時跨幀留著；`Hdr` 在 2D 相機上不看。守衛：`render.data`（`[tm-sync-*]`）、`render.pipeline3d`（`[p3-hdr-*]`）、`render.extract`（`[re-hdr-*]`）；突變 `tests/common/render/hdr_mutations.json`。

- **RD-14 合成與 tonemapping pass**（A4，第 8.2 節）：Hdr 相機的 `Prepass`～`PostProcess` 在它的中間圖（`PhaseTarget`）；中間圖每一幀清整張（清除色或透明黑），所以 tonemapping 讀它之前寫過（RP-3）；目標上的清除同不帶 Hdr 的相機（第一個清整張；之後帶清除色的 Hdr 相機不清 viewport）；tonemapping pass 在 `PostProcess` 之後、`Overlay` 之前，以相機的曲線、在 viewport 裡、在自己的像素讀中間圖、寫 sRGB 編碼的 bytes，目標上第一個相機取代、之後的 alpha 混合；曲線是 Bevy 的式子（無 LUT 的六條），預設 `ReinhardLuminance`；Hdr 相機的 `PostProcess` 在 D1 的互換之前讀改不了中間圖（RC-A4-2）。守衛：`render.data`（`[tm-*]`）、`render.pipeline3d`（`[cp-hdr-*]`）、`sdl_gpu.shaders`（`[sg-hdr-*]`）、`sdl_gpu.hdr`（`[gh-curve-*]`、`[gh-pixel-mapped]`、`[gh-viewport-only]`、`[gh-ui-kept]`、`[gh-split-kept]`、`[gh-blend-over]`，Lavapipe 上的像素）；突變同上。

- **RD-15 顏色空間**（A4，第 2 節「顏色空間」）：target 上的顏色代表什麼只由 `ColourSpaceOf(RenderTargetFormat)` 決定：bytes 的 target 照 bytes；線性的 target 上每個顏色先換成光——`Color8` 與照 bytes 取樣的 texel 解碼、線性取樣的 texel（`SampledAsLinear`）照原值——清除色、2D、3D 一致；後端照它選 `*_linear.frag`（pipeline 的鍵明寫顏色空間）、每個 draw 推貼圖是不是線性取樣。守衛：`render.data`（`[tm-colour-space]`、`[tm-sampled-linear]`）、`sdl_gpu.hdr`（`[gh-2d-linear]`、`[gh-srgb-texture-once]`、`[gh-bytes-texture-decoded]`、`[gh-unlit-linear]`、`[gh-clear-decoded]`）；突變同上。

- **RD-10 呈現方式是渲染 World 的資料、`Cleanup` 不清**（第 2 節末）：`render_world` 登記 `Presentation`、新 World 放預設（帶過去的值留著，O-7）；遊戲改的值留到它再改；後端讀它、不寫它，變了才套用。守衛：`render.world`（`[rw-present-registered]`、`[rw-present-default]`、`[rw-present-kept]`）、`sdl_gpu.frames`（`[gp-present-*]`：含 `[gp-present-only-on-change]`、`[gp-present-new-world]`）；突變 `rw-present-not-put`、`gp-present-every-frame`。

## Replacement Criteria

- **RC-RD 1**：第二個非 SDL 的後端（或 SDL_Renderer 之類的另一條路）要 `RenderCommands` 露出 SDL_GPU 才有的概念才畫得出來 → 資料切錯了，重切（RC-SDL 1 的 render 那一半）。
- **RC-RD 2**：一幀的 `Vertex2D`／索引的上傳成為幀時間的主要部分（R2b 的幀時間預算先宣告，計測）→ 改成保留式的頂點緩衝（Bevy 的 batch／retained），不是改格式。
- **RC-RD 3**：3D 在線性空間打光（R2b）和 2D pass 照 bytes 混合在同一個目標上畫不出一致的顏色 → 重定目標格式與 2D 的混合空間。〔A4 觸發，3D 那一半已解（[渲染管線宣告](render-pipeline-declaration.zh-Hant.md)第 16 節）：帶 `Hdr` 的 3D 相機畫在線性的中間圖、tonemapping pass 編碼回 bytes；目標仍是 UNORM、2D 照 bytes（RP-4）。2D 那一半留給 RC-A4-4。〕
- **RC-RD 4**（R2b）：2D 管線在 CPU 換算頂點（`Queue2d`）成為幀時間的主要部分（`[gp-budget-pipeline]` 超過第 7.5 節的預算）→ 換算移到 GPU（2D pass 帶 view 矩陣、物件帶仿射或實例），改 ABI 時 `ShaderAbi.hpp` 與 `Pass2D.hlsli` 一起改（RD-4）。
- **RC-RD 6**（R2b 的 3D 段）：CPU 蒙皮（第 8.4 節；joint palette 是每個網格算一次、沒有蒙皮的實體每幀重算 rest pose）成為幀時間的主要部分（蒙皮的頂點數×實體數），或一幀要上傳的蒙皮頂點超過預算 → 改成 GPU 蒙皮（joint palette 放 storage buffer、頂點帶 joints／weights，同 Bevy）；來源側的介面（`SkinPose`）不變。
- **RC-RD 5**（R2b）：相機在渲染 World 需要跨幀的狀態（貼圖目標、TAA 的歷史、遮擋查詢）→ 相機改成以來源實體為鍵的 Resource 或對應實體，第 7.3 節的四個衝突點在那時解。

## 範囲の限界

- 內建的 shader 只有 2D pass 與 unlit 的 3D pass；沒有 shader id、打光、材質系統。3D 段**沒有**：後處理（A4 起有 HDR 與 tonemapping，第 8.2 節；PvP 盤點的 exposure／gamma 調整是 Bevy 的 `ColorGrading`，D1）、MSAA、陰影、視錐剔除、3D 的幀時間預算（3D 段沒有量；PvP 的場景接上時宣告與量測）。模型網格的保留（`RenderModelMeshes`）與素材的貼圖一樣留到渲染 World 結束或同一個素材換 life——換地圖但不換 World、又放掉舊模型的 Handle 時，舊的 GPU buffer 不會放（素材卸載的通知有需要時再接）。深度貼圖每個目標一張（主目標一張、每個 render target 一張；A1），在目標大小改變時重建，沒有測試（`sdl_gpu.pass3d`、`sdl_gpu.targets` 都是 8×8）。
- 2D 段沒有：`Text2d` 元件（第 7.6 節）、可見性元件（Bevy 的 `Visibility`；不畫就拿掉元件或換層）、視錐剔除（只看位置的近遠平面）、加色混合（第 7.9 節）。
- 顏色照 bytes 混合（sRGB 編碼的值直接線性內插）：半透明的邊緣比線性空間混合暗一點，同 ImGui 與多數 2D 引擎；R2b 帶 3D 時重看（RC-RD 3）。
- 貼圖只有 `Rgba8Unorm`、`Rgba8UnormSrgb` 兩種格式（A1），target 是 `Rgba8Unorm`、`Depth`（A2）、`Rgba16Float`（A4）；沒有 mipmap、沒有壓縮格式（F1）。`Capture` 那一幀，跟主目標的 target 照 `Capture` 的大小（下一次 `Submit` 再照視窗的大小重建；測試與工具用，交替時多一次重建）。來源的 sprite 不能直接拿來源的 target 當圖（渲染管線宣告 RC-A1-1）。
- 一個 Product 只有一個 World 列 `sdl_gpu`（一個視窗、一張貼圖表）；多個渲染 World 各有一個 `GPUSubmit` 時一個視窗一幀只能送出一次——沒有點名的 Feature，有了再定（總宣告第 10.2 節 R2a 列）。
- 一個來源的 `AssetView` 拿不到的圖（另一個 Product 的素材）不畫；圖的貼圖不做 mipmap。
