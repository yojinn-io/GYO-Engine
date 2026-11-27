# 2026-11-27 渲染管線線 A1：render target（`v8-arch-gyo2-render-a1`）

Owner：渲染管線線のカード。分支 `claude/gyo2-render-a`（從 `origin/gyo2` `172b7546`）。作者 Opus 5.5／high（子 Agent）。宣告：[渲染管線宣告](../architecture/target/v2/render-pipeline-declaration.zh-Hant.md)第 13 節（A1 小宣告）；render 頁第 3 節、RD-3、RD-11；相機頁 CM-1、CM-3、CM-5；SDL 頁第 7 節。

## 前提

- 使用者：總宣告（#369）合併後說「A 段開始」（HANDOFF `v8-arch-gyo2-render-a-start`），主會話交 A1；小宣告寫完直接實作、不等核准（主會話的指示）。
- A1 的範圍（總宣告第 5 節）：畫到貼圖、貼圖格式與色彩空間、clip-from-world 收一處、`RenderCommands` 開頭註解。上段（公開介面、camera 的推導值條文）。
- 〔Engine〕target 的 id、GPU 貼圖、畫進去再取樣、跟視窗大小走、target 的大小進相機的推導值、世界到 clip 只算一次；〔作者〕哪張圖是彩色（解碼）、哪張是資料，用 target 做什麼。

## 做了什麼

1. **render target**（`RenderTextures.hpp`、`Render.cpp`）：`CreateTarget(RenderTargetDesc{size, format, filter})` 回 `TextureId`，和圖片共用計數器、不重用（RD-3）；pass 以 `target = id.value` 畫進去、draw 以 `texture = id` 取樣（同 Bevy 的 `Image::new_target_texture`）。`size` 沒給＝跟主目標走。新要求 `TextureTargetCreate`；`FindTarget`。
2. **格式**：`TextureFormat{Rgba8Unorm, Rgba8UnormSrgb}`，`TextureDesc::format` 預設 `Rgba8Unorm`（既有的呼叫者全部照舊）。`Rgba16Float`（A4）、深度（A2）、壓縮（F1）沒加，是加 enum 值與後端一列。
3. **SDL_GPU 能不能對一張貼圖開兩種格式的 view**（總宣告第 4.4 節的未確認點）：**不能**。`SDL_GPUTextureCreateInfo` 只有一個 `format`（`SDL_gpu.h:1769-1781`），沒有建 view 的 API，綁定 `SDL_GPUTextureSamplerBinding{texture, sampler}`（`:2142`）不帶格式；Vulkan 後端不開 `MUTABLE_FORMAT`（`SDL_gpu_vulkan.c:5607-5640`）。Bevy（wgpu `view_formats`，`bevy_image/src/image.rs:30-50`）、Godot（`texture_create_shared`＋`shareable_formats`→`MUTABLE_FORMAT`，`rendering_device_driver_vulkan.cpp:2316-2317`）都做得到。判：要兩種讀法時建兩份（B4 在 `RenderImages` 以圖＋格式為鍵）。
4. **來源 World 的 target**（新 `RenderTargets.hpp`、`RenderTargets.cpp`）：
   - `RenderTargets`（`world_render` 登記）：`Create` 回 `CameraTarget`（從 1 起、不重用）。
   - `world_render` 新的 System `WriteRenderTargetSizes`（`PostUpdate`、`CameraUpdate` 之前）：大小抄進 `CameraTargetSizes`（固定的 scale 1；跟主目標的抄 target 0；值不同才寫；放掉的拿掉）。`world_render` 的登記列加 `camera` 依賴、新 `WorldRenderOptions`。**這改了 render 頁「`world_render` 不跑 System」那一條**（大地圖宣告第 13.1 節當 Architecture Delta 沒改的那一條），比過的選項與理由在宣告第 13.3 節第 2 點。
   - `camera_render` 新的 `ExtractRenderTargets`（`ExtractCameras` 之前）：`SyncTargetTextures` 維持 `TargetTextures`（來源 id → 渲染 World 的 `TextureId`）；`ExtractCameras` 把相機的目標換成對應的 id，來源沒有的 id 的相機不抽。
5. **後端**（`GPUDevice.cpp`）：每個 pass 畫在自己的目標上；每個目標自己的「這一幀第一個 pass」與自己的深度貼圖；跟主目標的 target 在拿到主目標大小後建、大小變了重建；格式對到 SDL 的兩種；建之前 `SDL_GPUTextureSupportsFormat` 查格式與用途。`RecordPasses` 拆成 `RecordPasses`（目標的選擇）＋`RecordPass2D`＋`RecordPass3D`。
6. **clip-from-world 收一處**：`ComputedCamera::clipFromWorld`（`clipFromView × inverse(GlobalTransform)`），`UpdateCameras` 排在 `TransformPropagate` 之後寫；沒有視點＝原點；視點沒有反矩陣時 `valid == false`；`Compute` 多一個可選的 `view` 參數。visibility、`Queue2d`、`Queue3d` 改讀它。
7. **註解**：`RenderCommands.hpp` 開頭（「R2a 只有一種 pass、3D 在 R2b」）改成現在的樣子；`Camera.hpp` 的 `CameraTarget` 註解（「R2a、R2b 對其他 id」）同。
8. **文件**：宣告第 13 節；render 頁（第 2、3、7.2、7.4、8.3、8.6 節、RD-3、新 RD-11、範囲の限界）；相機頁（第 2 節、CM-1、CM-3、CM-5）；SDL 頁第 7 節。

## 測試

- 新：
  - `render.data` 五案：target 的 id（共用計數器、不重用、`Find`／`FindTarget` 分開、要求的順序）、格式、錯的 target 要求（Programmer Error）、來源的 `RenderTargets` 與 `TargetSizeOf`、`SyncTargetTextures`／`PassTargetOf`。
  - `render.extract` 一案：來源的固定與跟主目標的 target、相機在同一幀算得出來並畫在渲染 World 的 target 上、沒有對應的相機不畫、視窗變大跟著變、放掉後大小與渲染 World 的 target 都放掉。來源先建一個再放掉，讓來源的 id（2、3）和渲染 World 的（1、2）不同（第一次跑時兩邊碰巧相等，`re-target-not-mapped` 存活；改了之後 killed）。
  - `render.visibility` 一案：相機站在 z 50 時視錐跟著視點。
  - `camera.module` 兩案：`Compute` 的 `clipFromWorld`、原點、沒有反矩陣；`UpdateCameras` 用這一幀 propagation 後的視點、動了才寫。
  - 新 ctest `sdl_gpu.targets`（標籤 `gpu`）四案：8×8 target 畫 2D 再在主目標取樣（逐像素）、同一個 target 畫 3D（它自己的深度）、`Rgba8UnormSrgb` 的 188／128 取樣成 128／55（±1）而 `Rgba8Unorm` 照存、跟主目標的 target 在 8×8 與 16×16 都和主目標一樣大、target 第一個沒清除色的 pass 從透明黑開始（前一幀留的紅不見）。
- 改了的既有測試（判斷不變）：`Pipeline2dTests.cpp` 的 `Camera2d`、`Pipeline3dTests.cpp` 的 `Camera3d` 把視點也交給 `Compute`（管線改讀 `computed.clipFromWorld`）。算出來的矩陣和以前在管線裡算的是同一個運算、逐位元相同（`[p3-clip-from-world]` 逐位元比對照樣通過）；斷言一行都沒改。

## 驗證（本機 macOS Intel、Metal，preset `test`）

- 建置：依賴借另一個工作樹（`gyo2-big-map-s4c`）抓好的 `_deps`（`FETCHCONTENT_SOURCE_DIR_*`），shader 工具借同一個工作樹建好的 `gyo_sdl_gpu_shader_tool`（`GYO_SHADER_TOOL_EXECUTABLE`；本批沒改 shader 與 shader 工具）。全量建置成功（無警告；`GPUDevice.cpp` 的 `-Wmissing-field-initializers` 已修）。
- `ctest -L 'cpu|shader'`：254 個裡 253 通過。失敗的 `cmake.PluginSelection` 是借用依賴造成的（巢狀配置的 `FETCHCONTENT_FULLY_DISCONNECTED` 要求這個建置自己有 `yyjson-src`），同 S6 的紀錄，和本批無關。
- GPU（Metal，8×8～16×16 讀回，是測試、不是量測）：`sdl_gpu.pass2d`、`sdl_gpu.pass3d`、`sdl_gpu.frames`、`sdl_gpu.targets`、`sdl_gpu.budget`（既有的只印不判的那一案，跟著 `gpu` 標籤一起跑；Metal 5.8／6.4 ms）全部通過。
- 突變（D184）：見下一節。

## 突變

- 新 `tests/common/render/target_mutations.json`：27 個，全部 killed（`gt-` 的 5 個在 Metal 上）。
  - 寫了又拿掉的一個：`cm-clip-from-world-before-propagate`（拿掉 `UpdateCameras` 的 `after TransformPropagate`）存活——Kernel 對沒宣告先後的衝突照登記順序排，`camera` 依賴 `transform`，所以 propagation 本來就先跑。順序照 visibility 的寫法明寫，不算突變（理由在清單的 `description`）。
  - 第一次跑時 `rd-target-no-ask`、`rt-size-follows-ignored` 是 wrong_failure（先停在沒有標籤的 `REQUIRE`）：把那兩個 `REQUIRE` 改成帶標籤的 `REQUIRE_MESSAGE`，重跑 killed。
  - 數量：改的條文 4 條（CM-3、CM-5、RD-3、新 RD-11）×5＝20 < 27。超過的理由：A1 是三件事（target、格式、clip-from-world），每個獨立可錯的判斷一個（README 基準 3）；合併前審查時說明。
- 既有突變：
  - find 失效的 16 個：15 個改寫到新程式、守同一個性質，`g3-unmapped-drawn` 併入 `gp-unmapped-drawn`（2D 與 3D 的 pass 現在走同一條路到目標，同一個改動讓兩邊都畫到主目標；cases 與 expect 照 README 的合わせ方）。
  - find 落在改過的程式裡、或被改過的測試（`Camera2d`／`Camera3d`）殺的：`render_mutations.json` 21 個（改寫的與 find 在改過的程式裡的）＋`p2-`／`p3-`／`cp-`／`mh-` 其餘 54 個、`instancing_mutations.json` 13 個（`in-g3-instance-base-not-advanced`＋CPU 的 12 個）、`camera_mutations.json` 4 個、`visibility_mutations.json` 1 個。
  - 結果：93 個全部 killed（`gp-`／`g3-`／`in-g3-` 在 Metal 上）。加上新的 27 個，本批共跑 120 個、120 killed。

## 沒做／沒跑

- A2（phase、全畫面 pass、只寫深度的 pass、RP-3 的「讀沒寫過的 target」斷言）、A3、A4（`Rgba16Float`、HDR）、B1、F1（mip、壓縮）、D2（MSAA 的 target）。
- 來源的 sprite／網格直接拿來源的 target 當圖（RC-A1-1）、target 的比例大小（RC-A1-2）、每個 target 的深度貼圖共用（RC-A1-3）。
- 沒跑：Linux（Vulkan／Lavapipe）、Windows（D3D12）上的 `sdl_gpu.targets`——交給 L1。重 GPU 量測沒做。
