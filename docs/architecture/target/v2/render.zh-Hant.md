# 渲染（`engine/modules/render`）與 `render_world`

更新：2026-11-24（新頁，SDL 線子批 R2a `v8-arch-gyo2-sdl-r2a`）。由來：[SDL 總宣告](sdl-declaration.zh-Hant.md)修訂 21（使用者 2026-11-23 動工前核准「SDL 照建議核准」，HANDOFF `v8-arch-gyo2-sdl-declaration-approved`）第 3.3、6.1、9.2、10.2 節的 R2a 列；Q7＝(a)（`-plugin-in-world-rule-removed`）、讀法 2（`-sdl-system-calls-via-adapter`）。後端 `sdl_gpu` 寫在 [SDL 的 Plugin 頁](sdl.zh-Hant.md)第 7 節。**Design Invariants 的最終文字在合併前核准定**。

Owner：架構。程式：`engine/modules/render/`（`GYO::Engine::Render`，target `GYO::Render`，函式庫）、`engine/modules/render_world/`（`GYO::Engine::RenderWorld`，target `GYO::RenderWorld`，登記單位 `RenderWorldModule`）。行為測試：`tests/common/render/`（ctest `render.data`、`render.world`）、`plugins/sdl_gpu/tests/`（`sdl_gpu.pass2d` 等）。

## 一個例子

Object_Connect 的選單畫一塊半透明的黑底、上面一張按鈕圖：遊戲（之後經 R2b 的可畫元件與 Extract，今天在渲染 World 裡用一個 System）在 `Update` 往 `RenderCommands` 放一個 2D pass——`view` 是 1280×720 的遊戲座標，第一個 draw 用白貼圖 `TextureId{}`、頂點色 `{0,0,0,160}` 畫一個矩形，第二個 draw 用按鈕圖的 `TextureId`、`Alpha` 混合。按鈕圖在開場時經 `RenderTextures::Create` 拿到 id（像素照 `image` 的排法）。這一幀的 `Submit` stage，`sdl_gpu` 的 `GPUSubmit` 先建按鈕圖的 GPU 貼圖、再照順序畫兩個 draw、送到視窗；`Cleanup` stage 把 pass 與貼圖的要求清掉，貼圖本身留著。視窗被縮成 2560×1440：`view` 不變，畫面整個放大兩倍；裁切框也跟著放大。

## 1. 位置與依賴

```text
GYO::Base、GYO::Math ◄── GYO::Kernel（只為 HashValue）◄── GYO::Render
GYO::Render、GYO::Orchestration ◄── GYO::RenderWorld
GYO::Render、GYO::RenderWorld、GYO::SDL ◄── GYO::SDLGPU（plugins/sdl_gpu）
```

- `render`：`library_only=yes`、沒有 `depends`。R2a 不依賴 `transform`、`camera`（總宣告第 10.1 節：R2b 才加，只取型別）。不含 SDL、不含 GPU 物件、沒有後端介面（B'）。
- `render_world`：渲染側的單位（`default=no`、`depends=render`），放在要畫的那個 World。
- 沒有後端（沒選 `sdl_gpu`）時資料照樣每幀做好、清掉，沒有人畫（總宣告第 9.2 節第 3 條、Q4）。

## 2. `RenderCommands`：這一幀要畫什麼（資料）

`RenderCommands{passes}` 是渲染 World 的 Resource（`render_world` 登記），每個 `Pass2D` 照順序畫。R2a 只有一種 pass：**2D pass**（貼圖、混合、裁切）。

| 欄位 | 意思 |
|---|---|
| `target` | 畫到哪個目標，用 `camera` 的 id 空間（`CameraTarget::id`）：0 是主目標（有視窗時是視窗）。其他 id 對到什麼是後端的事；R2a 的 `sdl_gpu` 只對 0，別的 pass 不畫、計數 |
| `clear` | 先清成這個顏色。某個目標這一幀的第一個 pass 沒給時清成透明黑（視窗的畫面在幀開始時是未定的）；之後的 pass 沒給就疊在已有的畫面上 |
| `view` | 拉滿整個目標的那個矩形（view 單位，y 向下）。沒給＝目標自己的像素（1 單位＝1 像素、原點在左上角的像素） |
| `vertices`、`indices` | 這個 pass 所有 draw 的頂點與索引各一份。`Vertex2D`＝位置、uv、顏色（20 bytes，和 Dear ImGui 的 `ImDrawVert` 同一排法）；索引是 32 位元——ImGui 預設的 `ImDrawIdx` 是 16 位元，K 定 `#define ImDrawIdx unsigned int` 或轉換；ImGui 的 `ImDrawCallback`（draw 之間插自己的指令）表達不了，K 用到時再定 |
| `draws` | 照順序畫；後面的蓋在前面的上面。`Draw2D`＝貼圖、混合、裁切、`indexOffset`／`indexCount`／`vertexOffset`（索引加 `vertexOffset` 指到這個 pass 的頂點） |

- **座標**：view 單位、y 向下；uv (0,0) 是貼圖的左上角、(1,1) 是右下角，和 `image` 的 `ImageData` 一樣（第 0 列在最上面、不翻轉；I #301 交來的約定）。**上下只在 `ViewToClip` 轉一次**（y 向下的 view → y 向上的 clip space；SDL_GPU 的 NDC、viewport、貼圖座標各後端已經統一，`SDL_gpu.h` 的 Coordinate System 一節），uv 不轉（master 坑 17 的單一邊界）。
- **顏色**：照存的 bytes（彩色圖是 sRGB 編碼）、alpha 是 straight。2D pass 直接拿 bytes 混合，同 Dear ImGui、SDL_Renderer；目標與貼圖都是 UNORM。3D 要在線性空間打光時（R2b）另定，見範囲の限界。
- **混合**：`Alpha`（straight alpha：顏色＝src×a＋dst×(1−a)，alpha＝a＋dst.a×(1−a)；Bevy sprite 的 `BlendState::ALPHA_BLENDING`，`bevy_sprite_render/src/render/mod.rs:289`）、`Opaque`（取代，含 alpha；Bevy 的 `AlphaMode2d::Opaque`，`bevy_sprite_render/src/mesh2d/material.rs:264`）。Object_Connect 的 renderer 只用一般的 alpha 混合（`PuzzleRenderer.cpp:493-502`）。
- **裁切**：`clip`（view 單位）讓**中心在裡面**的像素通過——左、上邊含，右、下邊不含（光柵化自己的規則，所以對齊整像素的裁切是精確的），再切到目標裡（`ClipToPixels`）。沒給＝整個目標。
- **白貼圖**：`TextureId{}`（0）一直都在、不建也不放，畫出來就是頂點色（不貼圖的純色形狀，Object_Connect 盤點的 R2a 項）。
- **R2a 不帶的欄位**：總宣告 P27 列的 shader id、實例數——沒有消費者（2D pass 只有一組內建 shader、沒有實例），R2b（3D、自訂 shader）有需要時加。頂點色以外的 tint、相機、視區也是 R2b。
- **幀時間的預算**（總宣告第 10.2 節 R2a 列「幀時間的預算先宣告」）：Object_Connect 大小的一幀——1280×720、一個 pass、2,000 個各自一次 draw 的半透明矩形——經 `sdl_gpu` 的 `Capture`（套用、上傳、畫、讀回並等 GPU，比畫到視窗多了讀回）**8 ms 以內**（60 Hz 一幀的一半）。量法：`sdl_gpu.budget`（label `gpu`，只印、不判：GPU 與 driver 決定它，Lavapipe 比任何玩家的 GPU 都慢）。本機（macOS Intel、Metal、不開 SDL 的驗證）2026-11-24 量三次：4.89、4.97、5.25 ms。超過時看 RC-RD 2。
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

- **登記**：`render` 的 `RenderCommands`、`RenderTextures`（新 World 是空的）。雜湊函式什麼都不寫（渲染 World 不是被判定的狀態）。
- **stage**：在 `RenderWorldOptions::schedule`（預設 `Main`）的 `after`（預設 `Last`）之後插兩個 stage：`Submit`（後端畫、送出；Bevy 的 `RenderSystems::Render`，`bevy_render/src/lib.rs:231`）、`Cleanup`（`ClearRenderFrame` 清掉這一幀的 pass 與貼圖要求；Bevy 的 `RenderSystems::Cleanup`，`:233`）。寫 `RenderCommands` 的 System 放在 `Submit` 之前的任何 stage。
- **清在幀尾、不在幀頭**：Bevy 也是在 `Cleanup` 清。若在 `First` 清，開局清單或準備函式提的貼圖要求會在第一個 `Submit` 前被清掉。
- **extract schedule**：R2a 不排進每幀清單（R2a 用一個 World 加「出」的 System 驗，總宣告第 10.1 節）。第一個讀來源 World 的子批（K 的 draw list、R2b）加：`render_world` 把 extract schedule 插在 `Main` 之前（總宣告第 6.1 節第 3 點）；到時要定沒有 `extractFrom` 的渲染 World（R2a 這種）怎麼辦（不然是 O-E9）。

## 6. 比過的

| 題 | Bevy | 其他 | GYO R2a |
|---|---|---|---|
| 渲染的資料 | RenderApp 的實體＋phase item（`bevy_render`） | master：固定的 scene＋overlay 佇列 `RenderQueue`（`engine/render/include/engine/render/RenderQueue.hpp`）；Dear ImGui：draw list（頂點、索引、裁切、貼圖 id） | 一份 `RenderCommands`（pass → draws），形狀照 ImGui 的 draw list，K 直接交得進來；排序、合批的邏輯（R2b）是 `render` 的函式，產出還是這份資料 |
| 送出 | `render_system` 跑 render graph、submit、present（`bevy_render/src/renderer/mod.rs:82`），在 `RenderSystems::Render` | master：`Renderer` 呼叫 `IRenderDevice`（後端介面在 `engine/render`） | `sdl_gpu` 的 `GPUSubmit` 在 `Submit` stage（Q7＝(a)）；`engine/` 沒有後端介面（B'） |
| 貼圖的 id | 資料側的 `AssetId`（`render_asset.rs:220`、`prepare_assets` `:393`） | master：裝置配 handle | 資料側配（第 3 節） |
| 每幀清空 | `Cleanup`／`PostCleanup` | — | `Cleanup` stage |

偏離 Bevy 的地方：沒有 render graph 與 phase（R2a 只有一種 pass，R2b 再對照 Bevy 的 graph／phase 定，總宣告第 10.2 節 R2b 列）；渲染 World 的資料是一個 Resource、不是實體（實體對應在 R2b 逐種判，總宣告第 6.1 節第 8 點）。膠水量：無（`RenderCommands` 是後端讀的資料）。

## Design Invariants

- **RD-1 `render` 只有資料與中性的邏輯**：`RenderCommands`、`RenderTextures`、換算函式、HLSL 原始碼與 ABI；沒有 SDL、沒有 GPU 物件、沒有後端介面、不依賴任何 Module 或 Plugin（B'）。守衛：連結檢查（`tests/common/render/CMakeLists.txt` 的 `gyo_expect_links(gyo_render GYO::Base GYO::Kernel GYO::Math)`、`render_world` 同）；SDL 的原始碼掃描在 K 之後立（總宣告第 9.1 節），之前由每批的設計審查看。
- **RD-2 一幀的資料在 `Submit` 被看到、在 `Cleanup` 清掉**：`Submit` 在 `after` 那個 stage 之後、`Cleanup` 在 `Submit` 之後；`ClearRenderFrame` 清 pass 與貼圖要求、不動貼圖本身。守衛：`render.world`（`[rw-submit-after-last]`、`[rw-submit-sees]`、`[rw-cleanup-*]`、`[rw-prepare-ask-kept]`）。
- **RD-3 貼圖的 id 由資料側配、不重用；白貼圖是 0**：守衛：`render.data`（`[rd-tex-*]`）。
- **RD-4 shader 的兩邊一起改**：`ShaderAbi.hpp` 與 `Pass2D.hlsli` 是同一份 ABI；`sdl_gpu` 從編好的 SPIR-V 讀出的資源數和 `ShaderAbi.hpp` 的一致。守衛：`sdl_gpu.shaders`（`[sg-abi-*]`）、`render.data` 的 `[rd-abi-*]`、`sdl_gpu.pass2d` 的像素比對（頂點排法錯了顏色就不對）。

## Replacement Criteria

- **RC-RD 1**：第二個非 SDL 的後端（或 SDL_Renderer 之類的另一條路）要 `RenderCommands` 露出 SDL_GPU 才有的概念才畫得出來 → 資料切錯了，重切（RC-SDL 1 的 render 那一半）。
- **RC-RD 2**：一幀的 `Vertex2D`／索引的上傳成為幀時間的主要部分（R2b 的幀時間預算先宣告，計測）→ 改成保留式的頂點緩衝（Bevy 的 batch／retained），不是改格式。
- **RC-RD 3**：3D 在線性空間打光（R2b）和 2D pass 照 bytes 混合在同一個目標上畫不出一致的顏色 → 重定目標格式與 2D 的混合空間。

## 範囲の限界

- 只有 2D pass、只有內建的一組 shader；shader id、實例數、相機、視區、合成、3D 在 R2b。R2b 的幀時間預算（3D、合成）在 R2b 另宣告。
- 顏色照 bytes 混合（sRGB 編碼的值直接線性內插）：半透明的邊緣比線性空間混合暗一點，同 ImGui 與多數 2D 引擎；R2b 帶 3D 時重看（RC-RD 3）。
- 貼圖只有 RGBA8；沒有 mipmap、沒有壓縮格式、沒有渲染到貼圖（R2b 的合成、疊層需要時加）。
- 一個 Product 只有一個 World 列 `sdl_gpu`（一個視窗、一張貼圖表）；多個渲染 World 各有一個 `GPUSubmit` 時一個視窗一幀只能送出一次——沒有點名的 Feature，有了再定（總宣告第 10.2 節 R2a 列）。
- 沒有 extract schedule（第 5 節）。
