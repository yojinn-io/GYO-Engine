# 總宣告：渲染管線線（`v8-arch-gyo2-render-pipeline`）

更新：2026-11-27（修訂 1：記入使用者的決定，第 12 節）。Owner：渲染管線線的任務卡（GYO2 主會話開）。
**狀態：總宣告；第 11 節的決定點已由使用者決定（第 12 節），合併等使用者說「合併」。** 這一階段只寫宣告與分批，不寫實作程式；各批開工前另寫小宣告（判定、測試、突變）。

**核心一句**（每一項都拿來對照）：**清單照原樣全做。Engine 只給「只有 Engine 做得到」的機制（畫到哪裡、pass 的先後、材質怎麼接 shader、光與影怎麼算）；PBR、tonemapping、後處理這些是 Engine 給的「預設規則」，作者可以換掉；遊戲自己的 shader、材質參數、哪個模式開哪些效果，是遊戲的資料。**

**用詞**：標「**卡片判**」的是這張卡照命名規範、別家原始碼與已定原則自己判的，不是使用者的話。前提標記：**〔Engine〕**＝只有 Engine 做得到；**〔作者〕**＝作者（遊戲或工具）能處理。查證等級：**〔源〕**＝讀過原始碼、附行號；**〔文〕**＝讀過標頭或文件的說明；**〔知〕**＝憑記憶、沒有查證。

## 0. 根據與讀的版本

- 工作樹 `gyo2-render-pipeline-2384e5`，基底 `origin/gyo2` 00738c36。
- 使用者的清單：[批次表](rebuild-batch-plan.zh-Hant.md)〈渲染管線線（全部要做，待開卡）〉（`v8-arch-gyo2-render-pipeline-scope`、`-render-after-big-map`）。
- 已定、這條線要對進去的：K.3-6（模擬的 World 與渲染的 World 分開，渲染的只經 Extract 唯讀拿資料；[render 頁](render.zh-Hant.md)第 5、7.2 節）、P22 的讀法 (a)（遊戲 shader 走共通內容管線，`v8-arch-gyo2-sdl-r2a-builtin-shaders-in-plugin`）、路線 (A)（新功能加在 Object_PvP、由模式配置決定開哪些，`v8-arch-gyo2-object-pvp-route-a`、`-mode-config-start`）、Bevy 為主參考（`v8-arch-gyo2-hierarchy-bevy-and-primary-reference`）、本機重 GPU 量測先問（`warn-before-heavy-local-runs`）。
- 別家原始碼：Bevy `6a911653f`（本機，0.20.0-dev，2026-11-10）、Godot `94fc09c`（本機，4.8 beta）；SDL 3.4.0 的 `SDL_gpu.h`（gyo2 建置目錄 `build/target/_build/test/_deps/sdl3-src`）。Unity SRP 只有記憶，標〔知〕。
- 美術規格草稿：主 checkout `build/evidence/art-spec-draft/art-asset-spec.zh-Hant.md`（不在 git，只讀）第 4、5 節的四項（第 7 節）。

## 1. 現況（gyo2 程式，2026-11-27）

| 清單的項目 | 現況 | 程式位置 |
|---|---|---|
| 光源、材質模型 | **沒有**。3D 是 unlit：貼圖×實例顏色 | `engine/modules/render/shaders/mesh3d.frag.hlsl`、`ShaderAbi.hpp:49-95`（「unlit」）；`Vertex3D` 帶法線但不綁（`RenderCommands.hpp:119-126`） |
| 陰影、skybox、IBL、SSAO | 沒有 | — |
| 遊戲自己的 shader | 沒有。只有 Plugin 內建的 2D 與 3D 兩組，建置時編好嵌進 `sdl_gpu` | `plugins/sdl_gpu/src/adapters/sdl/ShaderTool.cpp`、`src/EmbeddedShaders.hpp`；[SDL 頁](sdl.zh-Hant.md)第 7 節「shader 工具」 |
| 材質資料、熱重載 | 沒有材質系統。模型的 `Material` 只有 `baseColor`、`baseColorImage`，由作者自己讀了填進 `Mesh3d` 的 `color`／`image`；asset 沒有熱重載（RC-AS4） | `engine/modules/model/include/engine/model/ModelData.hpp:66-77`；`games/object_fps_pvp/src/client/ClientWorld.cpp:748`（`MaterialColor`）；[asset 頁](asset.zh-Hant.md) RC-AS4 |
| tonemapping、exposure／gamma | 沒有（PvP 盤點列了 exposure／gamma，R2b 沒做） | [render 頁](render.zh-Hant.md)「範囲の限界」；`games/object_fps_pvp/docs/porting-inventory.zh-Hant.md:82` |
| bloom、霧、color grading、AA | 沒有 | — |
| 粒子、decal、拖尾 | 沒有（2D 可用 `Shape2d` 每幀給頂點，[render 頁](render.zh-Hant.md)第 7.9 節） | — |
| 視錐剔除 | **有**（大地圖 S2）：`visibility` Module 的 `CheckVisibility`、`Aabb`、`VisibilityRange` | `engine/modules/visibility/src/VisibilityModule.cpp:28`；`Drawables.hpp:202` |
| instancing | **有**（大地圖 S6）：同鍵的 `Mesh3d` 自動成為一個 draw 的實例 | `Pipeline3d.hpp:144-153`（`Queue3d`）；`ShaderAbi.hpp:62-74` |
| LOD | 機制的一半有：`VisibilityRange` 的距離切換；代理網格是大地圖 S7（延後）；漸變（crossfade）沒有 | `Drawables.hpp:202`；[大地圖宣告](big-map-declaration.zh-Hant.md)第 8 節 S7 |
| mipmap、貼圖壓縮 | 沒有：貼圖只有 RGBA8 UNORM | `plugins/sdl_gpu/src/adapters/sdl/GPUDevice.cpp:356`；`engine/modules/image/include/engine/image/PngLoader.hpp:16` |
| render target | 沒有：`CameraTarget::id` 只有 0（主目標）有對應，其他 id 的 pass 不畫、計數 | `engine/modules/camera/include/engine/camera/Camera.hpp:52-58`；[render 頁](render.zh-Hant.md)第 2 節 `target` |
| 多 pass／render graph | 一半：`RenderCommands::passes` 是照順序畫的 2D／3D pass 列表；`camera_render` 的 `CameraPasses` 每台相機一格、`ComposeCameraPasses` 照相機順序寫入。沒有 pass 之間讀寫貼圖的關係、沒有全畫面 pass | `RenderCommands.hpp:196-206`；`Compose.hpp:20-21`；`CameraRenderModule.cpp:73` |
| GPU 計時與除錯 | 沒有。只有 `sdl_gpu.budget` 經 `Capture` 量整幀（只印） | [render 頁](render.zh-Hant.md)第 2 節「幀時間的預算」 |

另外兩件記在批次表、這條線順手收：`RenderCommands.hpp:5-9` 開頭註解過時（Object_Connect #321 審查）；clip-from-world 在 visibility、`Pipeline2d`、`Pipeline3d` 三處各算一次（大地圖 #342 S-4，收斂要讓 `ComputedCamera` 多帶 clip-from-world，`Camera.hpp:122`）。兩件都排進 A1（第 5 節）。

## 2. 清單逐項：照原樣全做、各歸哪一批

```text
渲染管線                                         批次（第 5 節）
│
├─ 光照與陰影
│  ├─ 光源：方向光、點光、聚光；環境光／天空光   B4 render-lights-pbr
│  ├─ 材質模型：Lambert／Blinn-Phong 或 PBR       B4（建議 PBR＝StandardMaterial 當預設；Blinn-Phong 走 B2 的遊戲 shader）
│  ├─ 陰影：cascaded shadow map、點光 cube shadow  C1 render-shadow-csm、C2 render-shadow-point-spot
│  └─ 環境：skybox、IBL、AO（SSAO）               C3 render-sky-ibl、C4 render-ssao
│
├─ shader 與材質系統
│  ├─ 遊戲自己的 shader（P22）                    B2 render-game-shader
│  └─ 材質資料：參數、對應 shader、熱重載         B1 render-material、B3 render-hot-reload
│
├─ 後處理
│  ├─ tonemapping＋exposure／gamma                A4 render-hdr-tonemap
│  ├─ bloom、霧、color grading                    D1 render-bloom-fog-grading
│  └─ 抗鋸齒：MSAA 或 FXAA／TAA                   D2 render-msaa-fxaa、D3 render-taa（三個都做）
│
├─ 效果
│  ├─ 粒子                                        E1 render-particles
│  ├─ 貼花（decal）                               E3 render-decals
│  └─ 軌跡／拖尾                                  E2 render-trails
│
├─ 效能
│  ├─ 視錐剔除                                    已做（大地圖 S2）
│  ├─ instancing                                  已做（大地圖 S6）
│  ├─ LOD                                         距離切換已做（S2）；代理網格＝大地圖 S7（延後，歸大地圖線）；漸變 F2 render-lod-fade
│  └─ mipmap、貼圖壓縮                            F1 render-texture-compress
│
└─ 基礎設施
   ├─ render target                               A1 render-target
   ├─ 多 pass／render graph                       A2 render-phases
   └─ GPU 計時與除錯                              A3 render-gpu-debug（每個 pass 的 GPU 時間受 SDL_GPU 限制，第 4.7 節、Q1）
```

「或」的兩題（材質模型、抗鋸齒）：抗鋸齒三個都做（MSAA、FXAA、TAA 是不同取捨，作者照相機選）；材質模型 PBR 當預設、Blinn-Phong 由作者用遊戲 shader 寫（Q3，使用者已定，第 12 節）。

## 3. 別家對照

Bevy 是主參考；Godot 當第二家；Unity SRP 只有記憶。

| 題 | Bevy（`6a911653f`）〔源〕 | Godot（`94fc09c`）〔源〕 | Unity SRP〔知〕 | GYO（本宣告） |
|---|---|---|---|---|
| render graph | **已拿掉 graph 物件**：「Replace `RenderGraph` with systems」（#22144，commit `f1f41547f`，2026-01-28）——每台相機一個 schedule（`CameraRenderGraph`，`bevy_render/src/camera.rs:184`），`camera_driver` 照相機順序跑（`bevy_core_pipeline/src/schedule.rs:136`），pass 是 system set `Prepass → MainPass → EarlyPostProcess → PostProcess`（`:50`）；根是 `RenderGraph` schedule（`bevy_render/src/renderer/mod.rs:48`）；陰影這種不屬於相機的 view 是 `RootNonCameraView` | `RenderingDeviceGraph`：裝置層記錄所有命令、自動追蹤資源（`ResourceTracker`）、自動排序與插 barrier（`servers/rendering/rendering_device_graph.h:49,183`；`rendering_device.h:1764`） | URP 的 RenderGraph：pass 宣告讀寫、自動剔除沒人用的 pass、資源 aliasing | 照 Bevy：沒有 graph 物件。**誰產生 pass** 由渲染 World 的 schedule 排；**GPU 的先後**＝`RenderCommands` 的順序；pass 宣告讀哪些 target；同步交給 SDL_GPU（cycling，`SDL_gpu.h:309-329`）（第 4.3 節、Q2） |
| render target | `RenderTarget`（Window／Image／TextureView，`bevy_camera/src/camera.rs:896`）；每台相機的 `ViewTarget` 有兩張主貼圖互換給後處理（`bevy_render/src/view/mod.rs:794`、`post_process_write` `:1043`） | `SubViewport`＋`viewport_get_texture`（`servers/rendering/rendering_server.h:577`） | `RTHandle`〔知〕 | 照 Bevy：相機的目標 id 對到資料側配的 render target；後處理用兩張互換 |
| 材質 | `Material` trait（vertex／fragment shader＋`AsBindGroup`，`bevy_pbr/src/material.rs:148`）；`StandardMaterial` 是 `bevy_pbr` 內的一個實作（`pbr_material.rs:27`） | `ShaderMaterial`／`StandardMaterial3D`、`ORMMaterial3D`（`scene/resources/material.h:913,926`）；`shader_create`、`material_create`（`rendering_server.h:167,183`） | URP Lit（metallic 流程）〔知〕 | 照 Bevy：Engine 給材質的機制＋`StandardMaterial` 當預設；遊戲寫自己的 shader 與參數（第 4.5 節） |
| shader 從哪來 | WGSL／WESL 執行時經 naga 編；`file_watcher` 熱重載（`bevy_asset/src/lib.rs:62`） | 執行時編 Godot shader 語言 | 離線編譯＋變體〔知〕 | **離線編**（P22：共通內容管線找 `sdl_gpu` 的工具）；熱重載換的是編好的檔（第 4.6 節） |
| 光照 | clustered forward；`DirectionalLight`、`PointLight`、`SpotLight`（`bevy_light/src/directional_light.rs:73`、`point_light.rs:49`）、`AmbientLight`／`GlobalAmbientLight`（`ambient_light.rs:12,62`）；`apply_pbr_lighting`（`bevy_pbr/src/render/pbr_functions.wesl:326`） | Forward+（clustered，`renderer_rd/forward_clustered/`） | URP Forward+〔知〕 | forward、每個 view 一份有上限的光源表，**不做 clustering**（RC-RP2；卡片判，第 10 節） |
| 陰影 | CSM：`CascadeShadowConfig`（`bevy_light/src/cascade.rs:26`）、最多 4 層（`bevy_pbr/src/render/light.rs:242`）、`DirectionalLightShadowMap` 預設 2048（`directional_light.rs:193,204`）；點光 cube（`PointLightShadowMap`，`point_light.rs:180`）；`extract_lights`／`prepare_lights`（`light.rs:320,1038`） | `LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS`、`LIGHT_OMNI_SHADOW_CUBE`（`servers/rendering/rendering_server_enums.h:282,288`）；shadow atlas（`rendering_server.h:353`） | 同類〔知〕 | 照 Bevy |
| 環境 | `Skybox`（`bevy_light/src/probe.rs:244`）、`EnvironmentMapLight`（預先濾好的 diffuse＋specular 貼圖，`probe.rs:116`）、`ScreenSpaceAmbientOcclusion`（`bevy_pbr/src/ssao/mod.rs:117`，要 depth＋normal prepass） | `environment_set_ssao`（`rendering_server.h:686`） | 同類〔知〕 | 照 Bevy；IBL 的預先濾波由工具烘 |
| tonemapping／exposure | `Tonemapping`（預設 `TonyMcMapface`，`bevy_render/src/view/mod.rs:291,332`）、`Exposure{ev100}`（`bevy_camera/src/camera.rs:236`）、`Hdr` 標記（`bevy_camera/src/components.rs:91`）、`ColorGrading`（`view/mod.rs:498`） | `ENV_TONE_MAPPER_*`（`rendering_server_enums.h:686-688`，預設 linear〔知〕） | Volume＋Tonemapping〔知〕 | 照 Bevy 的形狀；預設值在 A4 的小宣告定（第 5 節） |
| bloom／霧 | `Bloom`（`bevy_post_process/src/bloom/settings.rs:33`）、`DistanceFog`（`bevy_pbr/src/fog.rs:56`，在材質 shader 裡算） | `environment_set_glow`、`environment_set_fog`（`rendering_server.h:672,702`） | Volume〔知〕 | 照 Bevy |
| 抗鋸齒 | `Msaa`（預設 `Sample4`，`view/mod.rs:256-260`）、`Fxaa`（`bevy_anti_alias/src/fxaa/mod.rs:58`）、`TemporalAntiAliasing`（要 depth＋motion vector prepass，`taa/mod.rs:114-116`） | `VIEWPORT_MSAA_*`、`VIEWPORT_SCREEN_SPACE_AA_FXAA`（`rendering_server_enums.h:543-553`） | 同類〔知〕 | 三個都做；MSAA 的預設值在 D2 定 |
| 粒子 | 核心沒有；生態 bevy_hanabi（GPU）〔知〕 | `CPUParticles3D`、`GPUParticles3D`（含 `trail_enabled`，`scene/3d/cpu_particles_3d.h:40`、`gpu_particles_3d.h:86`）；`particles_create`（`rendering_server.h:449`） | VFX Graph〔知〕 | CPU 模擬在渲染 World、實例化畫；GPU 版是 RC（第 5 節 E1） |
| decal | `ForwardDecal`（一個四邊形網格＋材質，要 `DepthPrepass`，`bevy_pbr/src/decal/forward.rs:57-65`）；`ClusteredDecal`（要 bindless，`bevy_light/src/cluster/mod.rs:235`、`bevy_pbr/src/decal/clustered.rs:1-10`） | `Decal`（clustered，`rendering_server.h:384`） | URP Decal Projector〔知〕 | 照 Bevy 的 `ForwardDecal`（SDL_GPU 沒有 bindless，clustered 是 RC） |
| mipmap／壓縮 | `CompressedImageFormats::BC`（`bevy_image/src/image.rs:2404,2431`）、KTX2／DDS loader（`ktx2.rs`、`dds.rs`）、離線壓縮的 `CompressedImageSaver`（`compressed_image_saver/mod.rs:112`）；執行時產 mip（`bevy_core_pipeline/src/mip_generation/mod.rs:234`） | 匯入時轉 VRAM 壓縮（桌面 S3TC／BPTC，`editor/import/resource_importer_texture.cpp:136`；法線圖另一個選項 `:276`） | 匯入設定〔知〕 | 照 Godot 的「匯入（打包）時轉」：工具把 PNG 轉成帶 mip 的 KTX2（BC7／BC5）（第 7 節） |
| LOD 漸變 | `VISIBILITY_RANGE_DITHER`（`bevy_pbr/src/render/mesh.rs:3126`） | `visibility_range` 的 fade〔知〕 | LOD Group 的 crossfade〔知〕 | 照 Bevy 的 dither |
| GPU 計時 | `DiagnosticsRecorder` 用 wgpu 的 timestamp query（`bevy_render/src/diagnostic/internal.rs:45`、`begin_time_span` `:188`） | `capture_timestamp`、`get_captured_timestamp_gpu_time`（`rendering_device.h:1931,1934`） | Profiler／Frame Debugger〔知〕 | **SDL_GPU 3.4.0 沒有 timestamp query**（`SDL_gpu.h` 沒有這種 API；有 `SDL_PushGPUDebugGroup`、`SDL_InsertGPUDebugLabel`，`:2959,2989`）。做法交使用者（Q1） |

## 4. 設計的骨架

### 4.1 兩個 World 與 Extract（K.3-6）

例子：PvP 的 Client，遊戲 World 有太陽（`DirectionalLight`＋`CascadeShadowConfig`）、幾盞走廊的點光、世界相機帶 `Hdr`、`Tonemapping`、`Exposure`、`Bloom`；撞牆時遊戲在命中點生成一個 `ForwardDecal` 實體、一個火花的 `ParticleEmitter`。每幀：渲染 World 的 `Extract` 把光、相機的後處理設定、材質引用、發射器、decal 抽成渲染側的資料（唯讀、不寫回）→ 渲染 World 自己的 System 排陰影 pass、相機的各 phase、模擬粒子 → `ComposeCameraPasses` 寫進 `RenderCommands` → `sdl_gpu` 在 `Submit` 畫。Match 不帶任何渲染的 Module。

- **來源側**（遊戲 World）：新的元件與 Resource（光、材質引用、相機的後處理設定、發射器、decal）都由 `world_render` 登記（同現在的 `Mesh3d`、`Aabb`，[render 頁](render.zh-Hant.md)第 7.2 節）。它們是作者的資料、不進一致性雜湊（同 `Drawables.hpp` 的現行做法）。
- **渲染側**：只經 Extract 讀（`Kernel::Extract<Kernel::Read<…>>`，`MeshRenderModule.cpp:68` 的形狀）。**粒子的模擬狀態在渲染 World**：它是畫面、不是被判定的狀態，Match 不跑；來源只放發射器（同 bevy_hanabi 的分工〔知〕）。
- **跨幀的渲染側狀態**（陰影貼圖、TAA 的歷史、粒子）：現在的相機與網格在渲染 World 每幀重建（[render 頁](render.zh-Hant.md)第 7.3 節、RC-RD 5）。陰影貼圖與粒子以「光／發射器的來源實體」為鍵的 Resource 保留；TAA 觸發 RC-RD 5（相機要跨幀的歷史），在 D3 照 RC-RD 5 解四個衝突點。來源被替換（R1b）時，這些以舊實體為鍵的資料整份丟掉（同 `RenderImages` 對新 World 的做法）。

### 4.2 Engine 的機制與遊戲的資料：分界

| 東西 | 誰 | 放哪 | 前提 |
|---|---|---|---|
| render target 的 id、格式、大小；pass 讀寫哪些 target | Engine | `render`（資料與中性邏輯，RD-1） | 〔Engine〕後端要在一幀裡排出先後 |
| 每台相機的 phase、全畫面 pass、陰影 view 的排法 | Engine | `render`＋`camera_render`、新的渲染側 Module | 〔Engine〕 |
| 材質的機制：shader id、參數區塊、貼圖槽、pipeline 的鍵 | Engine | `render` | 〔Engine〕作者寫不出 pipeline 的接法 |
| `StandardMaterial`（PBR）、tonemapping 曲線、bloom、FXAA、SSAO 的內建 shader | Engine 給的**預設規則**（AGENTS §1.1、§1.10：預設方便、可換） | HLSL 原始碼在 `render/shaders/`（RD-1、RD-4），由 `sdl_gpu` 編好嵌進去（P22 (a) 的內建那一條路） | 〔作者〕可用自己的材質／全畫面 pass 換掉 |
| GPU 物件、pipeline、shader 工具、格式支援的查詢 | Plugin `sdl_gpu` | `plugins/sdl_gpu/src/adapters/sdl/` | 外部組件只在 adapter（AGENTS §1.4） |
| 遊戲自己的 shader（HLSL）與它的參數排法 | 遊戲 | `games/object_fps_pvp/shaders/`（例），經共通內容管線編 | 〔作者〕 |
| 材質參數的值、哪個網格用哪個材質 | 遊戲 | 資料檔（JSON asset）＋遊戲的 System | 〔作者〕 |
| 哪個模式開陰影、bloom、粒子品質 | 遊戲 | 模式配置（`v8-arch-gyo2-mode-config`）＋設定 | 〔作者〕 |
| PNG → 帶 mip 的壓縮貼圖、IBL 預先濾波、LOD 代理網格 | 工具 | `tools/`，只經資料形式交接（AGENTS §1.7） | 〔作者〕；Game 沒有工具也能跑（讀未壓縮的 PNG） |

Product 的詞不進 Engine：元件名照 Bevy（`DirectionalLight`、`ForwardDecal`、`ParticleEmitter`），不出現「彈孔」「子彈」。

### 4.3 多 pass／render graph：照 Bevy 0.20 的「render graph as systems」

**卡片判**（Q2 給使用者確認讀法）：使用者清單寫的「Bevy 的 render graph」，Bevy 本身已經在 2026-01 拿掉（第 3 節第 1 列）。理由寫在那個 commit：Schedule 已經涵蓋 graph 在做的事（排先後、一起執行），graph 的 slot 資源管理從沒被用過；要資源管理也該用 ECS 做。GYO 的 Kernel 本來就是 schedule＋stage，照現在的 Bevy 做最少膠水。

- **CPU 這一側**：`CameraPasses` 每台相機的一格，從「一個 pass 列表」變成**照 phase 分的列表**（`Prepass`、`Opaque`、`Transparent`、`PostProcess`、`Overlay`；Bevy 的 `Core3dSystems`）。各 Module 的 System 把自己的 pass 放進對的 phase；同一 phase 內的先後由 System 的排序（set、before／after）決定。陰影這種不屬於相機的 view 在所有相機之前（Bevy 的 `RootNonCameraView`）。
- **GPU 這一側**：仍是 `RenderCommands::passes` 的順序。新的 pass 種類：**全畫面 pass**（shader＋輸入的貼圖或 target＋參數 bytes＋輸出 target；後處理與 tonemapping 用）、**只寫深度的 pass**（陰影、prepass）。每個 pass 宣告寫哪個 target、讀哪些 target。
- **不做的**：自動重排、自動剔除沒人讀的 pass、資源 aliasing（Godot 的 `RenderingDeviceGraph`、URP 的 RenderGraph 有）。SDL_GPU 自己處理同一資源在 pass 之間的同步（cycling）；寫的人照順序放就對。「讀一個這一幀還沒被寫的 target」是契約（寫明、後端斷言），不加擋誤用的守衛（`no-guards-against-misuse`）。量到 target 的記憶體成為問題時再做 aliasing（RC-RP1）。
- **和 Bevy 的差別與膠水**：Bevy 的 pass 是直接錄 GPU 命令的 System；GYO 的 pass 是資料（RD-1，`sdl_gpu` 讀了再錄），所以 phase 是**資料上的格子**，不是每台相機一個 schedule。衝突點：Bevy 的作者加 pass＝加一個 System；GYO 的作者加 pass＝加一個 System 往格子放資料（同樣是一個 System）。膠水：`CameraPasses` 多一層 phase。不需要 Kernel 的新功能（不用「每台相機跑一次 schedule」）。

### 4.4 顏色的路徑（RC-RD 3 在這條線觸發）

現在 2D 與 3D 都照 bytes 混合、目標是 UNORM（[render 頁](render.zh-Hant.md)第 2 節「顏色」、RC-RD 3）。打光必須在線性空間，所以：

- 3D 相機帶 `Hdr`（Bevy 同名、opt-in）時畫進線性的 RGBA16F 中間 target，相機的最後一個 phase 用 tonemapping 的全畫面 pass 把它轉成 sRGB 編碼的 bytes 寫到輸出（shader 裡做編碼）。**不帶 `Hdr` 的 3D 相機**在材質 shader 裡直接 tonemap＋編碼（Bevy 的 `TONEMAP_IN_SHADER`，`bevy_pbr/src/render/mesh.rs:491,3108`）。
- 2D 與 UI 照舊在輸出上照 bytes 混合——輸出仍是 UNORM、不改 swapchain（`GPUDevice.cpp:976` 取 swapchain 的格式、SDR composition）。所以 2D pass 的 ABI 不變（RD-4），RC-RD 3 的「重定目標格式與 2D 的混合空間」只動 3D 那一半。
- **貼圖要帶色彩空間**：彩色圖（`_D`、`_E`）取樣時要解碼成線性（`R8G8B8A8_UNORM_SRGB`，`SDL_gpu.h:824`），`_N`、`_ORM` 不解碼。`RenderTextures::Create` 加色彩空間（A1）。2D 照舊用 UNORM 取樣（bytes），同一張圖給 2D 與 3D 用時由後端各開一個 view〔未確認：SDL_GPU 能不能對同一張貼圖開不同格式的 view，A1 查；不能就建兩份〕。
- **偏離 Bevy**：Bevy 的輸出是 sRGB 格式、UI 在線性空間混合；GYO 保留 2D 的 bytes 混合（同 Dear ImGui、SDL_Renderer），衝突點只在「同一張圖 3D 與 2D 顏色看起來一樣」——兩邊最後都是 sRGB bytes，所以一樣。膠水：tonemapping pass 多一步編碼。

### 4.5 材質系統

- **機制**（`render`）：`ShaderId`（資料側配，同 `TextureId`，RD-3 的做法）；材質＝（shader、參數 bytes、貼圖槽的 `TextureId` 列表、混合／深度／cull）；`Draw3D` 帶材質，pipeline 的鍵加 shader；instancing 的鍵（IN-1）加材質。現在的 unlit 改寫成第一個內建材質（行為不變、既有 `render.pipeline3d`／`sdl_gpu.pass3d` 的測試原樣通過）。
- **來源側**：`MeshMaterial3d`（Bevy 同名的分開元件，`Mesh3d` 不變）指一個材質 asset 或作者持有的值（`ModelRef`／`ImageRef` 的先例）。模型檔裡的 `Material`（`ModelData.hpp:66`）是資料，作者讀了轉成材質（Bevy 讀 glTF 時直接產 `StandardMaterial`，`bevy_pbr/src/gltf.rs:13`；GYO 先由作者轉，要自動轉時是 model 那一側的事）。
- **參數的排法**：內建材質的 ABI 寫在 `ShaderAbi.hpp`（RD-4）；遊戲材質的 ABI 寫在和 shader 放一起的描述檔，shader 工具從編好的 SPIR-V 讀出資源數與 uniform 大小比對（同 `sdl_gpu.shaders` 的做法），對不上是建置錯誤。

### 4.6 遊戲 shader（P22）與熱重載

- **編譯**：產品在 CMake 宣告自己的 HLSL（`gyo_game_add_shaders` 之類，名字在 B2 定）；共通內容管線從已選 Plugin 的登記資料找到 `sdl_gpu` 的 shader 工具（P22 (a) 的原話）；產出各平台的 blob＋反射資料，當 asset 放進產品的內容目錄。沒選 `sdl_gpu` 的產品（Match）不編、不報錯（SDL-2 的精神）。
- **執行時**：`ShaderData` 經 asset／`world_asset` 載入，渲染 World 經來源的 `AssetView` 讀（同圖、模型）。
- **熱重載**（清單點名，觸發 asset 的 RC-AS4）：asset 加 `Modified`＋檔案監看（Bevy 的 `file_watcher`，`bevy_asset/src/lib.rs:62`）；換的是**編好的** blob 與材質檔。重新編譯由建置命令做（例：`cmake --build … --target <產品>_shaders`），遊戲行程不呼叫編譯器——shader 工具是主機的建置工具（[SDL 頁](sdl.zh-Hant.md)第 7 節），塞進遊戲行程會讓 Product 依賴 DXC。開發用、發版關掉（Bevy 也是 feature）。asset 那一側的改動在 B3 照 asset 頁的規則寫小宣告。

### 4.7 SDL_GPU 的限制（查 `SDL_gpu.h` 3.4.0）

| 要的 | SDL_GPU | 影響 |
|---|---|---|
| render target、MSAA resolve、RGBA16F、sRGB 格式、cube／2D array、比較取樣、compute、mip 產生 | 有（`SDL_GPU_TEXTUREUSAGE_COLOR_TARGET` `:907`、`SDL_GPU_STOREOP_RESOLVE` `:657`、`R16G16B16A16_FLOAT` `:797`、`TEXTURETYPE_CUBE`／`2D_ARRAY` `:924-926`、`enable_compare` `:1618`、`SDL_CreateGPUComputePipeline` `:2604`、`SDL_GenerateMipmapsForGPUTexture`） | 清單都做得出來 |
| BC 壓縮格式 | 有（`BC1`～`BC7`，`:777-782`、`BC7_RGBA_UNORM_SRGB` `:830`），要用 `SDL_GPUTextureSupportsFormat` 查 | Apple Silicon 的 BC 支援〔知〕、Lavapipe 的 BC 支援〔未確認〕：F1 開工時先查 CI 的 `vulkan-info.log` |
| GPU timestamp query | **沒有** | 每個 pass 的 GPU 時間拿不到（Q1） |
| bindless | 沒有 | clustered decal（Bevy）做不了 → `ForwardDecal`；材質一次綁一組 |
| 除錯標記 | 有（`SDL_PushGPUDebugGroup`、`SDL_InsertGPUDebugLabel`，`:2959,2989`；D3D12 要 WinPixEventRuntime） | RenderDoc／Xcode／PIX 的擷取裡看得到每個 pass 的名字 |

## 5. 分批

**順序與主會話的建議比**：主會話的建議（未定）是「地基 → 光照、tonemapping、遊戲 shader → 陰影、粒子、後處理、decal」。本宣告大致照它，改三處（卡片判，理由附）：

1. **tonemapping 移進地基（A4）**：它是第一個全畫面 pass，也是線性 HDR 路徑的出口；光照（B4）一做出來就要它，否則 HDR 的值沒地方轉。
2. **材質機制（B1）在光照（B4）之前**：PBR 是「第一個內建的光照材質」。先做光照再補材質系統，就是先把 PBR 寫死在 pipeline 裡、之後再拆（`no-shit-mountain`）。
3. **貼圖壓縮（F1）提早**：只依賴 A1 的貼圖格式；美術規格的壓縮與 mip 那一項等它，可以和 B 段並行。

```text
v8-arch-gyo2-render-pipeline
├── A 地基
│   ├── A1 render-target          畫到貼圖、貼圖格式與色彩空間、clip-from-world 收一處、RenderCommands 註解
│   ├── A2 render-phases          相機的 phase、全畫面 pass、只寫深度的 pass、讀寫宣告
│   ├── A3 render-gpu-debug       pass 名字的除錯標記、CPU 側的每 pass 時間、整幀 GPU 時間（Q1）
│   └── A4 render-hdr-tonemap     Hdr、tonemapping、Exposure、gamma（PvP 的 exposure／gamma）
├── B 材質與光照
│   ├── B1 render-material        材質機制；unlit 改成第一個內建材質
│   ├── B2 render-game-shader     P22：共通內容管線、遊戲 shader、ShaderData asset
│   ├── B3 render-hot-reload      asset 的 Modified＋監看；shader blob 與材質檔
│   └── B4 render-lights-pbr      方向／點／聚光、環境光、StandardMaterial、法線圖、tangent
├── C 陰影與環境
│   ├── C1 render-shadow-csm      方向光的 cascaded shadow map
│   ├── C2 render-shadow-point-spot 點光 cube、聚光
│   ├── C3 render-sky-ibl         skybox、IBL（工具烘預先濾波）
│   └── C4 render-ssao            depth＋normal prepass、SSAO
├── D 後處理
│   ├── D1 render-bloom-fog-grading bloom、DistanceFog、ColorGrading
│   ├── D2 render-msaa-fxaa       MSAA（含 resolve）、FXAA
│   └── D3 render-taa             motion vector prepass、jitter、歷史（RC-RD 5）
├── E 效果
│   ├── E1 render-particles       發射器（來源）＋CPU 模擬（渲染 World）＋實例化、加色混合
│   ├── E2 render-trails          拖尾（面向相機的帶）
│   └── E3 render-decals          ForwardDecal
└── F 效能
    ├── F1 render-texture-compress mip、KTX2（BC7／BC5）、轉檔工具
    └── F2 render-lod-fade        VisibilityRange 的 dither 漸變（代理網格仍是大地圖 S7）
```

### 5.1 批次表

上段的根據照 [navigator](../../../../agents/navigator.md)〈上段與下段〉：公開介面、資料形式。審查者是每批的設計審查（批次表〈每批的設計審查〉），純淨 Code Review（Fable／high）由主會話照規則排。估時是粗估（未量），只用來排序。

| 批 | 段別（根據） | 實作者／審查者／驗證者（模型／檔位） | 前置 | 驗收（CI 上外部檢查） | 估 |
|---|---|---|---|---|---|
| A1 render-target | 上段（公開介面：`RenderCommands`、`CameraTarget` 的對應、`RenderTextures` 的格式；camera 推導值的條文） | Opus／high、Opus／high、Opus | — | cpu：target id 由資料側配、不重用（同 RD-3）；clip-from-world 三處取同一個值。gpu（Lavapipe）：畫進 8×8 target、再在 2D pass 取樣到主目標，逐像素比；sRGB 貼圖取樣出線性值；目標跟視窗大小變 | 12～18 h |
| A2 render-phases | 上段（公開介面、pass 的資料形式） | Opus／high、Opus／high、Opus | A1 | cpu：同一台相機各 phase 的先後、陰影 view 在相機之前、`ComposeCameraPasses` 的順序（`render.extract` 現有案原樣通過）。gpu：兩個全畫面 pass 串接（反色→平移）的像素；只寫深度的 pass 之後取樣深度 | 14～20 h |
| A3 render-gpu-debug | 下段（除錯資訊；不改資料形式） | Opus／high、Opus／high、Opus | A2 | cpu：每個 pass 的名字與 CPU 時間出現在 `SDLGPUStatus`（或 trace），名字與 `RenderCommands` 的 pass 一一對應。gpu：整幀 GPU 時間 > 0。除錯標記本身在 CI 看不到（要 RenderDoc），只驗呼叫次數 | 6～10 h |
| A4 render-hdr-tonemap | 上段（公開介面：相機的元件；顏色路徑） | Opus／high、Opus／high、Opus | A2 | cpu：每條 tonemapping 曲線的純函式對照 Bevy 的式子（`bevy_core_pipeline/src/tonemapping.wesl`）。gpu：RGBA16F 寫 4.0 → 輸出 byte 和 CPU 參考值差 ≤ 1；不帶 `Hdr` 的相機像素和現在一樣（`sdl_gpu.pass3d` 原樣通過） | 12～16 h |
| B1 render-material | 上段（公開介面、pipeline 的鍵、IN-1） | Opus／high、Opus／high、Opus | A1 | cpu：材質進 pipeline 的鍵與 instancing 的鍵；unlit 改成材質後 `render.pipeline3d` 全部原樣通過。gpu：兩個參數不同的材質畫出預期像素 | 16～24 h |
| B2 render-game-shader | 上段（資料形式：shader 的描述與 ABI；建置的登記） | Opus／high、Opus／high、Opus | B1 | shader（Linux 主機建置）：測試用的遊戲 shader 編得過、描述與反射對不上時建置失敗。cpu：`ShaderData` 經 asset 載入。gpu：遊戲 shader 畫出預期像素。SDL-2 的拿掉 Plugin 檢查照樣通過 | 16～24 h |
| B3 render-hot-reload | 上段（asset 的公開介面：`Modified`） | Opus／high、Opus／high、Opus | B2 | cpu：改暫存目錄裡的 blob／材質檔 → 下一個 `Modified`、同一個 Handle 拿到新值；發版設定沒有監看；計時用 CTest 的逾時（`portable-verification`） | 10～16 h |
| B4 render-lights-pbr | 上段（公開介面：光的元件、`StandardMaterial`；資料形式：`Vertex3D` 加 tangent） | Opus／high、Opus／high、Opus | B1、A4 | cpu：光源的抽取與上限、`StandardMaterial` 的參數打包、tangent 對照 MikkTSpace 的參考值、法線用 inverse-transpose。gpu：方向／點／聚光／環境光各一個 16×16 的受光面，像素和 CPU 參考 BRDF 差在容許內；法線圖翻轉受光的一側 | 24～36 h |
| C1 render-shadow-csm | 上段（公開介面） | Opus／high、Opus／high、Opus | B4、A2 | cpu：cascade 的切法對照 Bevy（`cascade.rs:26` 起）。gpu：箱子擋太陽、地面的影子像素比亮處暗、cascade 交界處連續 | 18～26 h |
| C2 render-shadow-point-spot | 上段（公開介面） | Opus／high、Opus／high、Opus | C1 | gpu：點光六個方向各擋一個箱子都有影子；聚光一張 | 12～18 h |
| C3 render-sky-ibl | 上段（公開介面；cubemap 的資料形式；工具的輸出格式） | Opus／high、Opus／high、Opus | B4、A1 | cpu：cubemap 的面順序；工具輸出的 mip 數。gpu：skybox 六個方向的像素；均勻白色環境下漫反射＝albedo（解析解） | 16～24 h |
| C4 render-ssao | 上段（公開介面；prepass 的 phase） | Opus／high、Opus／high、Opus | A2、B4 | gpu：牆角的 AO 比空地暗；prepass 的深度等於主 pass 的深度 | 16～24 h |
| D1 render-bloom-fog-grading | 上段（公開介面） | Opus／high、Opus／high、Opus | A4 | cpu：bloom 的 mip 鏈大小、霧與 grading 的純函式。gpu：一個很亮的像素擴散到鄰近；遠處的像素混向霧色 | 14～20 h |
| D2 render-msaa-fxaa | 上段（公開介面；target 的 sample 數） | Opus／high、Opus／high、Opus | A4 | gpu：斜邊的像素在 MSAA 下是中間值、resolve 後的 target 可取樣；FXAA 讓斜邊變平滑（像素對照） | 12～18 h |
| D3 render-taa | 上段（RC-RD 5：相機在渲染 World 的跨幀狀態；Extract 的對應） | Opus／high、Opus／high、Opus（四個衝突點的設計可提 xhigh，要同意） | C4、D2 | cpu：jitter 序列固定；鏡頭切換時歷史重設；來源替換時歷史丟掉。gpu：靜止場景數幀後收斂到 MSAA 參考 | 20～30 h |
| E1 render-particles | 上段（公開介面：發射器；`BlendMode::Add`） | Opus／high、Opus／high、Opus | B1、A4 | cpu：發射數、壽命、渲染 World 外沒有模擬狀態（Match 不帶）。gpu：加色混合的像素是和；實例化成一個 draw | 18～26 h |
| E2 render-trails | 下段（用 E1 與 `Pass3D` 自己的頂點；只加一個元件）＊加新的 pass 種類時改上段 | Opus／high、Opus／high、Opus | E1 | cpu：點列 → 面向相機的帶（寬度、朝向）。gpu：線段沿途的像素 | 8～12 h |
| E3 render-decals | 上段（公開介面） | Opus／high、Opus／high、Opus | B4、C4 | gpu：牆上的 decal 只改盒子內、表面上的像素；邊緣照深度淡出 | 12～18 h |
| F1 render-texture-compress | 上段（資料形式：壓縮貼圖的契約；工具的輸出） | Opus／high、Opus／high、Opus | A1 | 工具：PNG → KTX2 的 mip 數、格式、尺寸不是 4 的倍數時失敗。cpu：loader 讀出 mip 與格式。gpu：取樣值和參考解碼的差在容許內；縮小時取到對的 mip；不支援的格式走明確的退路（Lavapipe 不支援 BC 時，這條要另寫怎麼在 CI 驗） | 16～24 h |
| F2 render-lod-fade | 上段（`VisibilityRange` 的語意：margin 裡從「兩個都畫」變成漸變） | Opus／high、Opus／high、Opus | B1 | gpu：margin 中間兩個 LOD 各占的像素比例接近漸變係數 | 8～12 h |

合計粗估 290～430 h（未量）。

- **並行**：A3 與 A4 可並行；B2、B3 與 B4 在 B1 之後可並行（碰同檔時協調）；F1 在 A1 之後任何時候；C 與 D、E 在各自的前置之後可並行。照 `split-large-task-integrator-first`：改基盤介面（`RenderCommands`、`CameraPasses`）的 A1、A2、B1 不和別批並寫。
- **PR**：照 `fewer-prs`，建議一段一條線（A、B、C、D、E、F 各一個 PR，段內各批各自 commit）；由主會話定。
- **每批的小宣告**：開工前寫判定、測試、突變（D184：每批只跑相關突變）；合併前照批次表〈每批的設計審查〉審查。
- **Object_PvP 的接法**（路線 (A)）：每批做完，由 PvP 卡在 Object_PvP 的模式配置接上（例：「核心 PvP」模式先不開光照，畫面照舊）；這條線不改 PvP 的遊戲程式，除了驗收要的最小場景。

## 6. 驗收方法（CI）

- **cpu**（三平台 L1）：資料與純函式——排序、id、鍵、曲線、切法、打包。和現在的 `render.data`、`render.pipeline3d`、`render.extract` 同一種。
- **gpu**（Linux L1 的 Lavapipe，`ctest -L gpu`，`.github/workflows/cross-platform.yml:183`）：`sdl_gpu` 的 `Capture` 讀回小 target（8×8～16×16）逐像素比；浮點路徑（HDR、光照）比 CPU 參考值、容許差寫在各批的小宣告。Windows、macOS 的 GPU 像素不在 CI（現狀；`ci-is-primary-standard`），各批的實機看法只在 CI 做不到時加。
- **shader**（Linux 主機建置）：遊戲 shader 與內建 shader 的編譯與 ABI 對照（`sdl_gpu.shader_tool.*` 的延伸）。
- **幀時間**：每批照 `sdl_gpu.budget` 的做法加「只印、不判」的量測。Object_PvP 的整體幀時間（S5 當回歸基準）是本機重 GPU 量測，**每次都先問**（`warn-before-heavy-local-runs`）。

## 7. 美術規格的四項（建議與由哪一批定）

| 項目 | 建議 | 由哪一批定 | 根據 |
|---|---|---|---|
| 法線貼圖 `_N` | **要**。切線空間、**OpenGL 慣例（Y+，綠色朝上）**、8-bit PNG、linear；tangent 用 MikkTSpace（模型沒帶時由 loader 產生）。打包時壓成 BC5（兩個通道、Z 在 shader 重建） | B4（要不要、慣例、tangent），F1（壓縮格式） | glTF 與 Bevy 預設 Y+、DirectX 的圖要翻（`flip_normal_map_y`，`bevy_pbr/src/pbr_material.rs:427-431`）；Bevy 產 tangent 用 MikkTSpace（`bevy_mesh/src/mesh.rs:2195`、`mikktspace.rs:79`）；Godot 匯入時法線圖另走壓縮選項（`resource_importer_texture.cpp:276`） |
| `_RMA` 合併貼圖 | **要**，但**通道照 glTF 的 ORM：R＝AO、G＝roughness、B＝metallic**，後綴改 `_ORM`（Q4，使用者已定，第 12 節） | B4 | Bevy：`occlusion_texture` 讀 R、`metallic_roughness_texture` 讀 G（roughness）與 B（metallic），文件說常見做法就是三個合一張（`pbr_material.rs:174,446-456`）；Godot `ORMMaterial3D`：`ROUGHNESS = orm_tex.g`、`METALLIC = orm_tex.b`、`AO = orm_tex.r`（`scene/resources/material.cpp:1724-1725,1942`）。「RMA」照字面是 R＝roughness，和兩家都相反，美術照名字排就會錯 |
| UV1（lightmap） | **現在不要求**。清單裡沒有烘焙光照；環境光由 `AmbientLight`＋IBL（C3）＋SSAO（C4）給。規格寫「不需要；有也不會壞（loader 忽略）」 | 不在這條線；RC-RP4（烘焙光照成為點名的 Feature 時，靜態場景物件要 UV1） | Bevy 的 `Lightmap` 用 `ATTRIBUTE_UV_1`（`bevy_pbr/src/lightmap/mod.rs:88`、`bevy_mesh/src/mesh.rs:394`）；Godot `LightmapGI` 用 UV2（`scene/3d/lightmap_gi.h:150`、`ARRAY_TEX_UV2` `rendering_server_enums.h:132`）。體素地圖的 AO 可由烘焙工具寫進頂點，不需要 UV1 |
| 貼圖壓縮格式 | **PNG 交件**；打包時由工具轉成 **KTX2＋預先算好的 mip**：`_D`、`_E` → BC7（sRGB）、`_ORM` → BC7（linear）、`_N` → BC5；遊戲執行時不轉。沒經工具的 PNG 照樣能讀（開發用，執行時產 mip）。尺寸照現在的「2 的冪次」（BC 的 4×4 區塊與 mip 都要） | F1 | 三個桌面平台都走 BC（`SDL_gpu.h:777-782,830`；Apple Silicon 支援 BC〔知〕，F1 用 `SDL_GPUTextureSupportsFormat` 確認）；Bevy 的 `CompressedImageFormats::BC`、KTX2／DDS loader（`bevy_image/src/image.rs:2431`、`ktx2.rs`）；Godot 在匯入時轉 S3TC／BPTC（`resource_importer_texture.cpp:136`） |

## 8. 不做／留給別條線

- LOD 代理網格（大地圖 S7，延後；歸大地圖線）、遮擋剔除、GPU 剔除＋indirect（RC-IN2）。
- 烘焙光照（lightmap）、GI、反射探針的即時更新、volumetric、SSR、DOF、motion blur、OIT、meshlet（Bevy 有，清單沒點名）。
- HDR 顯示器輸出（swapchain 的 HDR composition）。
- 2D 的光照、2D 的後處理（現在的 2D pass 不動；2D 相機要後處理時另議）。
- `voxel_map` 的頂點壓縮（要遊戲 shader，B2 之後由大地圖線或 PvP 判要不要做）。
- Object_PvP 的畫面調整本身（PvP 卡照模式配置接）。

## 9. Design Invariants（候選；各批小宣告時定稿）

- **RP-1 機制在 Engine、規則可換**：內建的 PBR、tonemapping、後處理都經公開的材質／全畫面 pass 機制接上；作者用同一個機制換掉它們，不需要改 Engine。
- **RP-2 渲染側只讀來源**（K.3-6）：光、材質引用、後處理設定、發射器、decal 只經 Extract 讀；渲染 World 的狀態（陰影貼圖、粒子、TAA 歷史）不寫回、不進雜湊。
- **RP-3 GPU 的先後＝`RenderCommands` 的順序**：沒有第二個排序者；pass 讀的 target 在同一幀裡已被前面的 pass 寫過（契約，後端斷言）。
- **RP-4 2D 的顏色路徑不變**：2D pass 照 bytes 混合、ABI 不變（RD-4）；線性空間只在 3D 相機的中間 target。
- RD-1（`render` 不含 GPU 物件、不依賴 Plugin）照舊；新的 shader 原始碼放 `render/shaders/`，遊戲的放產品自己的目錄。

## 10. Replacement Criteria（候選）

- **RC-RP1** target 的記憶體（中間 target、陰影、歷史）在 Object_PvP 的設定下超過預算 → 資源 aliasing（Godot／URP 的 graph 那一層）。
- **RC-RP2** 一個 view 的光源數超過上限、或光源表的成本成為主要部分 → clustered forward（Bevy、Godot 的做法）。
- **RC-RP3** 粒子的 CPU 模擬或上傳成為幀時間的主要部分 → GPU compute 模擬（SDL_GPU 有 compute）。
- **RC-RP4** 烘焙光照成為點名的 Feature → lightmap、UV1 列入美術規格。
- **RC-RP5** SDL_GPU 加了 timestamp query → A3 改成每個 pass 的 GPU 時間（Q1 已選 (a)）。
- **RC-RP6** SDL_GPU 有 bindless 或要大量 decal → clustered decal。
- 既有的 RC-RD 3（本線 A4 觸發，第 4.4 節）、RC-RD 5（D3 觸發）、RC-IN1（B1：自訂 shader 要每實體 uniform 時的 `NoAutomaticBatching`）在各批照原文解。

## 11. 交使用者的事

只列和別家不同、或會推翻你之前決定的地方；其他照「卡片判」做，可推翻。

1. **Q1 GPU 計時（每個 pass 花多少時間）**：SDL_GPU 3.4.0 沒有 timestamp query（Bevy、Godot 都靠它，第 3 節最後一列）。
   - (a) **建議**：每個 pass 加除錯標記（RenderDoc／Xcode／PIX 擷取時看得到每個 pass 的 GPU 時間）＋Engine 自己報每個 pass 的 CPU 時間與整幀的 GPU 時間（fence）；每個 pass 的 GPU 時間等 SDL 加 query（RC-RP5）。清單這一項照做，只是「每個 pass 的 GPU 時間」在遊戲內要靠外部工具。
   - (b) 在 `sdl_gpu` 的 adapter 裡直接呼叫 Vulkan／D3D12／Metal 的 timestamp：遊戲內就有數字，但 adapter 要寫三份原生程式〔未確認 SDL 3.4 能不能拿到原生的命令緩衝〕。
   - (c) 每個 pass 各自送出、各等一個 fence：拿得到數字，但會把 GPU 排成串行，量到的不是真的時間。
2. **Q2 「Bevy 的 render graph」的讀法**：Bevy 已在 2026-01 把 render graph 換成 schedule（第 4.3 節）。建議照現在的 Bevy：沒有 graph 物件，pass 的產生由 schedule 排、GPU 照 `RenderCommands` 的順序。和 Godot／URP 不同的是不自動重排、不自動剔除 pass、不做資源 aliasing（RC-RP1）。
3. **Q3 材質模型**：清單寫「Lambert／Blinn-Phong 或 PBR」。建議 **PBR（`StandardMaterial`）當預設**（Bevy 的預設；美術規格已經有 `_RMA`，就是 PBR 的貼圖）；Blinn-Phong 不內建，要的話作者用 B2 寫遊戲 shader。
4. **Q4 `_RMA` 的通道**：建議照 glTF／Bevy／Godot 的 ORM（R＝AO、G＝roughness、B＝metallic），後綴改 `_ORM`（第 7 節）。這是改你草稿裡的命名。

**卡片判、可推翻的（和 Bevy 不同的地方，供知悉）**：光照不做 clustering（RC-RP2）；2D 照 bytes 混合、輸出不換 sRGB 格式（第 4.4 節）；shader 離線編、熱重載換編好的檔（第 4.6 節）；decal 只做 forward（RC-RP6）；粒子在渲染 World 用 CPU 模擬（RC-RP3）。順序和主會話建議的三處差別在第 5 節開頭。

## 12. 使用者的決定（2026-11-27）

**原話**：「全部照推薦，空殼刪掉」。使用者先說「全部展示出來我過一遍」，主會話把第 11 節各題的選項與推薦逐題列出後，使用者這樣回覆。**出處**：HANDOFF `v8-arch-gyo2-render-pipeline-q1-q4`（gyo2 `3825f51b`），經主會話轉交。

| 題 | 決定 | 比過的選項 | 影響 |
|---|---|---|---|
| Q1 GPU 計時 | **(a)**：每個 pass 加除錯標記（外部擷取工具看每個 pass 的 GPU 時間）＋Engine 報每個 pass 的 CPU 時間與整幀的 GPU 時間（fence）；每個 pass 的 GPU 時間等 SDL 加 query（RC-RP5） | (b) adapter 寫三份原生 timestamp；(c) 每個 pass 各等一個 fence（串行化） | A3 維持下段、照第 5.1 節的驗收 |
| Q2 render graph | **照現在的 Bevy**（render graph 已換成 schedule）：沒有 graph 物件、不自動重排或剔除 pass、不做資源 aliasing（RC-RP1） | Godot／URP 式的自動重排與 aliasing | 第 4.3 節照原文；RP-3 定為 DI 候選 |
| Q3 材質模型 | **PBR（`StandardMaterial`）當預設**；Blinn-Phong 不內建，作者用 B2 寫遊戲 shader | Lambert／Blinn-Phong 內建 | B1、B4 照原文 |
| Q4 合併貼圖的通道 | **ORM（R＝AO、G＝roughness、B＝metallic），後綴 `_ORM`** | 維持 `_RMA` 的通道順序 | B4 照原文；使用者的美術規格草稿那一列已由主會話改成 `_ORM` |

**卡片判、使用者沒有推翻的**：光照不做 clustering（RC-RP2）；2D 照 bytes 混合（第 4.4 節）；shader 離線編、熱重載換編好的檔（第 4.6 節）；decal 只做 forward（RC-RP6）；粒子在渲染 World 用 CPU 模擬（RC-RP3）。這些照原文做，觸發各自的 RC 時再議。

## 13. A1 小宣告：render target（`v8-arch-gyo2-render-a1`）

2026-11-27。使用者說「A 段開始」（HANDOFF `v8-arch-gyo2-render-a-start`，gyo2 `172b7546`），主會話交 A1。Opus／high（上段：`RenderCommands`、`CameraTarget` 的對應、`RenderTextures` 的格式、camera 推導值的條文）。小宣告寫完直接實作（主會話的指示：不等核准）。

讀的版本：gyo2 `172b7546`；Bevy `6a911653f`；Godot `94fc09c`；SDL 3.4.0（建置抓下來的 `_deps/sdl3-src`）。

### 13.1 現況（程式，A1 前）與 SDL 查到的事

- **render target 沒有**：`CameraTarget::id` 只有 0 有對應（`Camera.hpp:52-58`）；`sdl_gpu` 只畫 target 0 的 pass、別的計數（`GPUDevice.cpp:483,508,639,648`）；`camera_render` 把來源的 id 原樣抄進 `ExtractedCamera::target`（`CameraRenderModule.cpp:47`）。
- **貼圖只有一種格式**：`TextureDesc` 沒有格式（`RenderTextures.hpp:37`），後端一律 `R8G8B8A8_UNORM`（`GPUDevice.cpp:356`）。3D 的深度貼圖只有一張、跟著最後一個畫 3D 的目標的大小（`:154`）。
- **clip-from-world 算三次**：visibility（`VisibilityModule.cpp:57-59`，來源 World）、`Queue2d`（`Pipeline2d.cpp:133-135`，渲染 World）、`Queue3d`（`Pipeline3d.cpp:206-208`，渲染 World），都是 `clipFromView × inverse(GlobalTransform)`；`UpdateCameras` 不讀 `GlobalTransform`（`CameraModule.cpp:46`）。
- **〔源〕SDL_GPU 3.4.0 不能對同一張貼圖開別的格式的 view**（第 4.4 節的未確認點）：`SDL_GPUTextureCreateInfo` 只有一個 `format`（`SDL_gpu.h:1769-1781`），API 裡沒有建 view 的函式（建立、命名、釋放貼圖只有 `:2801,2938,3022`）；綁定是 `SDL_GPUTextureSamplerBinding{texture, sampler}`（`:2142`），沒有格式。Vulkan 後端建 image 時只加 `CUBE_COMPATIBLE`／`2D_ARRAY_COMPATIBLE`，沒有 `VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT`（`src/gpu/vulkan/SDL_gpu_vulkan.c:5607-5640`）。對照：Bevy 經 wgpu 的 `view_formats` 給 sRGB 變體（`bevy_image/src/image.rs:30-50`）；Godot 的 `texture_create_shared`（`servers/rendering/rendering_device.h:470`）靠 `shareable_formats` 開 `MUTABLE_FORMAT`（`drivers/vulkan/rendering_device_driver_vulkan.cpp:2316-2317`）。**判**：同一張圖要「解碼」與「照存的」兩種讀法時建兩份貼圖（`RenderImages` 以圖＋格式為鍵，B4 用到時做）；A1 只讓 `RenderTextures` 帶格式。

### 13.2 前提：誰要解決

| 項 | 前提 | 理由 |
|---|---|---|
| target 的 id、GPU 貼圖、pass 畫進去再被取樣 | **〔Engine〕** | 作者伸不進後端；id 由資料側配才能讓 `RenderCommands` 保持純資料（RD-3 同理） |
| target 的大小進相機的推導值 | **〔Engine〕** | `camera` 不認得 render 的型別（CM-2），來源 World 要有人把大小抄進 `CameraTargetSizes`（同 `window_camera`） |
| 跟視窗大小走的 target | **〔Engine〕** | 只有後端知道這一幀主目標的大小（swapchain、Capture） |
| 貼圖取樣時要不要 sRGB 解碼 | **〔Engine〕機制、〔作者〕選** | 格式是 Engine 給的欄位；哪張圖是彩色（`_D`、`_E`）、哪張是資料（`_N`、`_ORM`）是作者／素材的事 |
| clip-from-world 一處 | **〔Engine〕** | 三個讀者都在 Engine 內 |
| 用 target 做小地圖、監視器畫面 | **〔作者〕** | 作者建 target、放相機、在自己的 pass 取樣 |

### 13.3 設計（卡片判）

1. **target 是 `RenderTextures` 的貼圖**（渲染 World）：`RenderTextures::CreateTarget(RenderTargetDesc{size, format, filter})` 回 `TextureId`，和圖片共用一個計數器、不重用（RD-3）。pass 用 `target = id.value` 畫進去，之後的 draw 用 `texture = id` 取樣。`size` 沒給＝跟主目標（target 0）一樣大，後端每幀照主目標的大小建（變了就重建）。照 Bevy：target 就是一張 `Image`（`Image::new_target_texture`，`bevy_image/src/image.rs:1279`），相機的 `RenderTarget::Image` 與 sprite 的圖是同一個 handle（`bevy_camera/src/camera.rs:896`）。新的要求 `TextureTargetCreate` 接在 `TextureOp` 的最後；`Update` 一個 target 是 Programmer Error（它沒有 CPU 像素）。
2. **來源 World 的 target 與對應**：
   - `RenderTargets`（render 的型別、來源 World 的 Resource，`world_render` 登記）：`Create(desc)` 回作者放進相機的 `CameraTarget`（從 1 起、不重用）。
   - `world_render` 的 `WriteRenderTargetSizes`（`PostUpdate`、在 `CameraUpdate` 之前；**改了 render 頁第 7.2 節「`world_render` 不跑 System」這一條**，大地圖宣告第 13 節把它當 Architecture Delta、沒有改。卡片判：比過的另一個選項是像 `window_camera` 那樣另立一列膠水（多一列、每個用 target 的來源 World 都要多帶）；大小的副本是 target 的擁有者那一側的事，`world_render` 正是來源側登記 `RenderTargets` 的那一列，所以放這裡）：每個 target 的大小抄進 `CameraTargetSizes`——固定大小的 scale 1（Bevy `ImageRenderTarget::scale_factor` 的預設），跟主目標的抄 target 0 的；值不同才寫；放掉的 target 拿掉大小。所以 `world_render` 的登記列多依賴 `camera`。
   - `camera_render` 的 `ExtractRenderTargets`（`Extract`、在 `ExtractCameras` 之前）：`SyncTargetTextures` 讓渲染 World 的 `TargetTextures` 跟上來源——第一次看到、或描述變了（被替換的來源 World 從 1 重配）就 `CreateTarget`（舊的放掉）；來源沒有了就放掉。`ExtractCameras` 把相機的目標換成對應的貼圖 id（0 還是 0）；**來源沒有這個 target 的相機不抽**（沒有東西可畫）。
   - 渲染 World 自己的中間 target（A4 的 HDR、A2 的深度、D1 的 bloom）直接 `CreateTarget`，不經來源。
3. **格式**：`TextureFormat{Rgba8Unorm, Rgba8UnormSrgb}`（Bevy／wgpu 的 `TextureFormat` 的兩個值）。`TextureDesc::format` 預設 `Rgba8Unorm`：既有的呼叫者（2D 的圖、ImGui、字形、3D 的 unlit）全部照舊照 bytes 取樣（RP-4、`sdl_gpu.pass3d` 不變）。`Rgba8UnormSrgb` 取樣解碼成線性、畫進去時編碼。target 也帶格式（同一個 enum）。**現在不做、位置已留**：`Rgba16Float`（A4）、深度格式（A2 的只寫深度 pass）、BC 壓縮（F1）——都是加 enum 值，後端加一列對應；誰要誰加（主會話的指示：只做 A1 驗收要的與免費的）。
4. **後端**（`sdl_gpu`）：每個 pass 畫在自己的目標上（0＝視窗或 Capture 的圖；別的＝那張 target）；一個目標這一幀的第一個 pass 沒給清除色時清成透明黑（同 target 0）；每個 target 有自己的深度貼圖（3D pass 畫進 target 時用它的大小）；建貼圖前用 `SDL_GPUTextureSupportsFormat` 查格式與用途（`SDL_gpu.h:4478`；Lavapipe 也一樣查，不假設），不支援就算 refused。沒有圖的目標（沒建、或 SDL 拒絕）的 pass 照舊計數 `UnmappedPasses`。
5. **clip-from-world 收一處**：`ComputedCamera::clipFromWorld`＝`clipFromView × inverse(GlobalTransform)`，由 `UpdateCameras`（排在 `TransformPropagate` 之後，讀這一幀的視點）寫；沒有 `GlobalTransform` 的相機當在原點；視點沒有反矩陣（scale 0）時 `valid == false`。`Compute` 多一個可選的 `view` 參數。visibility、`Queue2d`、`Queue3d` 都讀這一個值（`Queue3d` 另外用視點的反矩陣算排序距離，不是 clip）。相機動了 `ComputedCamera` 就變（`Changed` 照實亮）；不動不寫（CM-3 照舊）。

**偏離 Bevy 的地方、理由與膠水**：

| Bevy | gyo2 | 理由 | 膠水 |
|---|---|---|---|
| 相機目標是來源的 `Image` handle，渲染側以 `AssetId` 為鍵（`render_asset.rs:393`） | 來源的 `RenderTargets` id；渲染 World 的 `TargetTextures` 對到 `RenderTextures` 的 id | gyo2 的 asset 沒有執行期加入（`v8-arch-gyo2-mesh3d-held`），來源不能「新增一張圖」；渲染 World 的 id 由它自己的 `RenderTextures` 配（RD-3） | 一個 Resource＋一個 extract System（同 `RenderImages` 的形狀） |
| `camera_system` 自己從 `Assets<Image>` 讀圖的大小（`bevy_render/src/camera.rs:292-298`） | `world_render` 把大小抄進 `CameraTargetSizes` | `camera` 不依賴 render（CM-2）；同 `window_camera` 抄視窗大小 | 一個 System |
| 目標圖的大小固定，跟視窗變要作者自己改（`camera.rs:292`） | `size` 不給＝跟主目標走，後端每幀對齊 | A4 的 HDR、D1、C4 的中間 target 都要跟相機的目標一樣大；Bevy 的這些是 `ViewTarget` 內部建的（`view/mod.rs:794`），gyo2 的 pass 是資料，要一個看得到的 target | 無（後端多一個判斷） |
| 一張圖可開 sRGB／非 sRGB 兩種 view（wgpu `view_formats`） | 一張貼圖一種格式 | SDL_GPU 沒有 view（第 13.1 節） | 要兩種讀法時兩份貼圖（B4） |
| `clip_from_world` 算兩次：`update_frusta`（`bevy_camera/src/projection.rs:72`，主 World）、`prepare_view_uniforms`（`bevy_render/src/view/mod.rs:1230-1239`，渲染 World） | `ComputedCamera::clipFromWorld` 一個值，三個讀者都取它 | 大地圖 #342 S-4：三處各算、各自可能錯；gyo2 的相機推導值已經是被抽的元件，帶過去不另算 | `UpdateCameras` 排在 propagation 之後（Bevy 的 `update_frusta` 也在它之後，`bevy_camera/src/visibility/mod.rs:503-507`）。TAA（D3）的 jitter 在渲染 World 改 `Pass3D::clipFromWorld`，不回寫相機（同 Bevy 在 prepare 時加 jitter，`view/mod.rs:1223-1235`） |

**別家對照**：Godot 的 viewport 自己有 RID，取樣要另拿 `viewport_get_texture`（`servers/rendering/rendering_server.h:577`），大小由 `viewport_set_size` 給（`:557`）。gyo2 照 Bevy 用同一個 id（少一個 id 空間）。

### 13.4 判定（合併前）

- **T1**（cpu）：target 的 id 由資料側配、和圖片共用計數器、不重用；`Find`／`FindTarget` 分得開；建立是一個依序的要求。來源的 `RenderTargets` 從 1 起、不重用。
- **T2**（cpu）：來源 target 的大小進 `CameraTargetSizes`（固定的 scale 1；跟主目標的抄 target 0 的、視窗變了跟著變；放掉的拿掉），相機在同一幀算得出來。
- **T3**（cpu）：渲染 World 對每個來源 target 有自己的 render target；同一個不重建；描述變了重建、舊的放掉；來源沒有了放掉；相機的 pass 畫在對應的 target；來源沒有的 id 的相機不畫。
- **T4**（cpu）：`ComputedCamera::clipFromWorld`＝`clipFromView × inverse(視點)`、沒有視點時＝`clipFromView`、視點沒有反矩陣時 `valid == false`；`UpdateCameras` 用這一幀 propagation 後的視點；相機動了才寫。visibility、`Queue2d`、`Queue3d` 取同一個值。
- **T5**（gpu，`sdl_gpu.targets`）：畫進 8×8 的 target、在 2D pass 取樣到主目標，逐像素比；3D pass 也畫得進 target（它自己的深度）；target 第一個沒清除色的 pass 從透明黑開始；放掉後沒有 GPU 貼圖。
- **T6**（gpu）：`Rgba8UnormSrgb` 的 188／128 取樣成線性 128／55（±1）；`Rgba8Unorm` 照存的。
- **T7**（gpu）：跟主目標的 target 在 8×8 與 16×16 的 Capture 都和主目標一樣大（4×4 像素的方塊不被拉伸），重建不增加貼圖數。
- **T8**：既有測試照過（`render.*`、`camera.module`、`sdl_gpu.pass2d`、`sdl_gpu.pass3d`）；第 13.5 節的突變全部 killed。

**既有測試的改動**（只改輸入的做法，判斷不變）：`Pipeline2dTests.cpp` 的 `Camera2d`、`Pipeline3dTests.cpp` 的 `Camera3d` 兩個輔助函式把視點也交給 `Compute`（同 `UpdateCameras`），因為管線改讀 `computed.clipFromWorld`；斷言一行都沒改。

### 13.5 測試與突變

- 測試：`tests/common/render/RenderTests.cpp`（`render.data`）五案；`PipelineWorldTests.cpp`（`render.extract`）一案；`VisibilityTests.cpp`（`render.visibility`）一案；`tests/common/camera/CameraTests.cpp`（`camera.module`）兩案；`plugins/sdl_gpu/tests/Pass2DTests.cpp` 的 `sdl_gpu targets:` 四案（新 ctest `sdl_gpu.targets`，標籤 `gpu`）。
- 突變：`tests/common/render/target_mutations.json`，27 個（T1～T7 各個獨立可錯的判斷；沒加的與理由在檔案的 `description`）。`Queue2d`、`Queue3d` 取同一個值由既有的 `p2-view-ignored`、`p3-view-ignored` 守（改寫成「管線不取 `computed.clipFromWorld`」）。
- 既有突變（D184）：find 失效的 16 個：15 個改寫到新程式、守同一個性質，1 個併入（`render_mutations.json` 的 `rd-tex-reuse`、`rd-tex-release-kept-live`、`gp-no-scissor`、`gp-release-kept`、`gp-unmapped-drawn`、`gp-viewport-not-set`、`gp-scissor-whole-target`、`p2-view-ignored`、`p3-view-ignored`、`g3-colour-cleared`、`g3-first-kept`、`g3-base-not-advanced`；`instancing_mutations.json` 的 `in-g3-instance-base-not-advanced`；`camera_mutations.json` 的 `cm-target-always-zero`、`cm-letterbox-ignored`；`g3-unmapped-drawn` 和 `gp-unmapped-drawn` 是同一個改動了——2D 與 3D 的 pass 走同一條路到目標——照 README 的合わせ方併成一個）。find 落在改過的程式裡的、與被改過的測試輔助函式（`Camera2d`、`Camera3d`）殺的也重跑。

### 13.6 RC

- **RC-A1-1**：來源的 sprite／網格要直接取樣來源的 target（小地圖當 `Sprite` 的圖）→ `ImageRef` 加一種「來源 target」，`RenderImages` 經 `TargetTextures` 對到貼圖（Bevy 的同一個 handle）。現在作者在渲染 World 用自己的 pass 取樣。
- **RC-A1-2**：要 target 的一部分大小（半解析度的 SSAO、bloom 的 mip 鏈要主目標的 1/2、1/4）→ `RenderTargetDesc` 加比例（加法）。
- **RC-A1-3**：一幀裡多個不同大小的 3D target 讓每個 target 的深度貼圖成為記憶體的主要部分 → 深度改成依大小共用（RC-RP1 的 aliasing 那一層）。

### 13.7 不做

phase、全畫面 pass、只寫深度的 pass（A2）；除錯標記與計時（A3）；HDR、tonemapping（A4）；材質（B1）；mip 與壓縮（F1）；MSAA 的 target（D2）；「讀一個這一幀還沒被寫的 target」的後端斷言（RP-3，A2 定 pass 的讀寫宣告時一起做）。
