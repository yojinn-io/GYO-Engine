# 總宣告：渲染管線線（`v8-arch-gyo2-render-pipeline`）

更新：2026-11-27（修訂 1：記入使用者的決定，第 12 節）。Owner：渲染管線線的任務卡（GYO2 主會話開）。
**狀態：總宣告；第 11 節的決定點已由使用者決定（第 12 節），合併等使用者說「合併」。** 第 13 節 A1 小宣告：修訂 1（2026-11-27，設計審查與純淨 Code Review 之後）。 第 14 節 A2 小宣告（2026-11-27；修訂 1：設計審查與純淨 Code Review 之後，第 14.10 節）。 第 15 節 A3 小宣告（2026-11-28）。 這一階段只寫宣告與分批，不寫實作程式；各批開工前另寫小宣告（判定、測試、突變）。

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
- **不做的**：自動重排、自動剔除沒人讀的 pass、資源 aliasing（Godot 的 `RenderingDeviceGraph`、URP 的 RenderGraph 有）。SDL_GPU 自己處理同一資源在 pass 之間的同步（cycling）；寫的人照順序放就對。「讀一個這一幀還沒被寫的 target」是契約（寫明、後端斷言），不加擋誤用的守衛（`no-guards-against-misuse`）。「讀的 target 必須這一幀寫過或明示沿用，兩者皆非就斷言」（使用者 2026-11-27，HANDOFF `v8-arch-gyo2-render-a2-target-reuse`；機制在第 14.11 節）。量到 target 的記憶體成為問題時再做 aliasing（RC-RP1）。
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
- **RP-3 GPU 的先後＝`RenderCommands` 的順序**：沒有第二個排序者；pass 讀的 target 在同一幀裡已被前面的 pass 寫過（契約，後端斷言）。「讀的 target 必須這一幀寫過或明示沿用，兩者皆非就斷言」（使用者 2026-11-27；第 14.11 節）。
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

### 13.3 設計（卡片判；修訂 1：設計審查與純淨 Code Review 之後）

1. **target 是 `RenderTextures` 的貼圖**（渲染 World）：`RenderTextures::CreateTarget(RenderTargetDesc{size, format, filter})` 回 `TextureId`，和圖片**同一張表**、共用一個計數器、不重用（RD-3）。pass 的目標是 `std::optional<TextureId>`（`nullopt`＝主目標），用 `target = id` 畫進去，之後的 draw 用 `texture = id` 取樣。照 Bevy：target 就是一張 `Image`（`Image::new_target_texture`，`bevy_image/src/image.rs:1279`），相機的 `RenderTarget::Image` 與 sprite 的圖是同一個 handle（`bevy_camera/src/camera.rs:896`）。新的要求 `TextureTargetCreate` 接在 `TextureOp` 的最後；`Update` 一個 target 是 Programmer Error（它沒有 CPU 像素）。
   - **大小**：`std::variant<TargetSize, FollowTarget>`；`FollowTarget{target}` 跟主目標（`nullopt`，預設）或同一張表裡另一個 target（設計審查 S-1：HDR 相機的中間圖要跟**相機的**目標，不是視窗；Bevy 的 `ViewTarget` 照相機自己的 `physical_target_size`，`bevy_render/src/view/mod.rs:1416`）。比例（半解析度）是之後在 `FollowTarget` 加欄位（RC-A1-2）。
   - **畫面跨幀保留**（S-2，照 Bevy）：沒給清除色的 pass 是 `LOAD`（`bevy_render/src/texture/texture_attachment.rs:52`；Godot 的 `VIEWPORT_CLEAR_NEVER`，`rendering_server_enums.h:514-518`）；只有剛建、或因大小變了重建之後的第一個 pass 清成透明黑。主目標照舊每幀第一個 pass 清（swapchain 的內容本來沒有定義）。
   - **格式**：target 現在只收 `Rgba8Unorm`（純淨審查 M3：sRGB 的 target 上清除色、頂點色的 bytes 與混合是什麼意思，要在 A4 的線性路徑一起定；現在收了就是一條沒定義、沒測的路）。
2. **來源 World 的 target 與對應**：
   - `RenderTargets`（render 的型別、來源 World 的 Resource，`world_render` 登記）：`Create(SourceTargetDesc{size, filter})` 回作者放進相機的 `CameraTarget`（從 1 起、不重用）。**主目標是 `Camera::kMainTarget`（0），其他相機目標 id 只有 `RenderTargets` 配**，別人不配、不寫那些 id 的大小（純淨審查 H2 的「一個擁有者」；作者亂寫一個 id 的大小是誤用，照 AGENTS §3 不加守衛，寫在 CM-5）。來源的 `size` 跟的是**相機目標**（主目標或先建的來源 target），所以是另一個型別 `SourceTargetDesc`；兩個 World 的 id 不同，不能共用一個 `RenderTargetDesc`。
   - `world_render` 的 `WriteRenderTargetSizes`（`PostUpdate`、在 `CameraUpdate` 之前）：照 id 順序把大小抄進 `CameraTargetSizes`——固定大小的 scale 1（Bevy `ImageRenderTarget::scale_factor` 的預設），跟別人的抄被跟的；放掉的拿掉。寫法經相機的 `WriteTargetSize`／`RemoveTargetSize`（S-3：「逐位元相同不寫」只有相機一處，`window_camera` 也改用它）。**這改了 render 頁第 7.2 節「`world_render` 不跑 System」那一句描述**（大地圖宣告第 13.1 節當 Architecture Delta 避開的那一條；設計審查判定不是使用者的決定）。放這裡的理由是依賴的方向：`window_camera` 另立一列，是因為 SDL 總宣告 L17 規定 `window` 與 `camera` 互不依賴（`sdl-declaration.zh-Hant.md:188`）；`render` 本來就依賴 `camera`，`world_render` 依賴 `render`，所以 `world_render` 加 `camera` 依賴沒有新的方向，另立一列只是多一個單位。`camera_render` 在渲染 World 只能唯讀來源（K.3-6），`camera` 讀不了 render 的型別（CM-2）。同名的 `world_time`、`world_asset` 也跑 System。
   - `camera_render` 的 `ExtractRenderTargets`（`Extract`、在 `ExtractCameras` 之前）：`SyncTargetTextures` 讓渲染 World 的 `TargetTextures` 跟上來源——第一次看到的 `CreateTarget`（跟來源 target 的，跟為它建的那張）；來源沒了的放掉。**身分**：同一張表裡 id 不重給、描述不變，所以同一個 id 就是同一個 target；`RenderTargets::Table()` 不同（被替換的來源 World 是新的一張；O-7 帶過去的複本是同一張）就把為舊表建的全部放掉（純淨審查 H2：原本用「id 與描述相同」猜，描述相同時會沿用舊 World 的畫面）。`ExtractCameras` 把相機的目標換成對應的貼圖 id（主目標還是主目標）；**來源已放掉 target 的相機不抽**（在相機算完之後才放的那一幀）。
   - 渲染 World 自己的中間 target（A4 的 HDR、A2 的深度、D1 的 bloom）直接 `CreateTarget`，不經來源。
3. **格式**：`TextureFormat{Rgba8Unorm, Rgba8UnormSrgb}`（Bevy／wgpu 的 `TextureFormat` 的兩個值）。`TextureDesc::format` 預設 `Rgba8Unorm`：既有的呼叫者（2D 的圖、ImGui、字形、3D 的 unlit）全部照舊照 bytes 取樣（RP-4、`sdl_gpu.pass3d` 不變）。`Rgba8UnormSrgb` 取樣時解碼成線性。**現在不做、位置已留**：`Rgba16Float`（A4）、深度格式（A2 的只寫深度 pass）、BC 壓縮（F1）——都是加 enum 值，後端加一列對應。加值的那一批要寫明 `TextureDesc`（CPU 像素，每像素 4 bytes）收哪些格式（設計審查 No-4）。
4. **後端**（`sdl_gpu`；純淨審查 H3）：
   - 每幀先拿視窗的畫面，再套用要求、照這一幀的大小建或重建 target（固定大小、主目標的、被跟的 target 現在的；照 id 順序，所以被跟的先定）、**一次**解析每個 pass 的目標（`ResolveTargets`：主目標或有貼圖的 target；其餘計一次沒有對應），上傳幾何與畫 pass 都照這張表。視窗縮小（沒有畫面）時 render target 照畫，只是沒有主目標。
   - target 是自己的一張表（`GPUTarget`），不和圖片（`GPUTexture`）混；每個 target 自己的深度貼圖。SDL 拒絕一個大小時記住那個大小，同一個大小不再試、不再計數。格式支援只在 `MakeTexture` 一處查（`SDL_gpu.h:4478`；Lavapipe 也查，不假設）。
   - 取樣自己這個目標的 draw（同一張貼圖同時讀寫，GPU 上沒有定義）跳過並計數。
   - `Capture` 那一幀，跟主目標的 target 照 `Capture` 的大小（下一次 `Submit` 再照視窗重建）。`Capture` 是測試與工具用的路，交替時多一次重建，結果是對的；不另外為它留一份（主目標的深度貼圖本來也是共用的）。
5. **clip-from-world 收一處**（純淨審查 H1；照 Bevy 的分法）：
   - `ComputedCamera` 回到只有投影與目標（CM-3 的文字不變；視點不可逆時換算函式照舊回 `InvalidData`）。
   - 新的推導元件 **`CameraView{valid, viewFromWorld, clipFromWorld}`**，由 `UpdateCameraViews` 在 set `CameraUpdate` 裡、`UpdateCameras` 與 `TransformPropagate` 之後寫（Bevy 的 `Frustum` 由 `update_frusta` 在 propagation 之後寫，`bevy_camera/src/visibility/mod.rs:503-507,627`）；值不同才寫。`ComputedCamera` 無效或視點沒有反矩陣時 `valid == false`；沒有 `GlobalTransform` 的相機沒有有效的 `CameraView`（不再有「當作在原點」的可選參數）。初始值由 `Camera` 給；宿主自己 `Insert` 組相機時要一起放（同 `ComputedCamera`，PvP 的 `ClientWorld.cpp`、`Scenes.cpp` 各加一行）。
   - visibility 的視錐、`Queue2d`、`Queue3d` 取 `CameraView::clipFromWorld`，`Queue3d` 的排序距離取 `viewFromWorld`（不再自己求反矩陣，原本的死分支沒了）。`ExtractedCamera::view` 從視點矩陣改成 `CameraView`（不再同時帶輸入與推導值）。
   - 換算函式（`WorldToViewport` 等）照 Bevy 收傳進來的 `GlobalTransform` 自己算（`bevy_camera/src/camera.rs:740-747`），不在「一個值」的範圍裡（設計審查 N-4）。
   - `CameraOptions` 要指 transform 的 schedule 與 stage（同 visibility）；不同 stage 時先後是骨架的順序（Kernel 只在同一個 stage 加順序邊，`Scheduler.cpp:272`），寫在相機頁，不加守衛。
   - 相機一動，變的是 `CameraView`；`ComputedCamera` 只在投影或目標變了才變。

**偏離 Bevy 的地方、理由與膠水**：

| Bevy | gyo2 | 理由 | 膠水 |
|---|---|---|---|
| 相機目標是來源的 `Image` handle，渲染側以 `AssetId` 為鍵（`render_asset.rs:393`） | 來源的 `RenderTargets` id；渲染 World 的 `TargetTextures` 對到 `RenderTextures` 的 id；換了表全部重建 | gyo2 的 asset 沒有執行期加入（`v8-arch-gyo2-mesh3d-held`），來源不能「新增一張圖」；渲染 World 的 id 由它自己的 `RenderTextures` 配（RD-3）；來源 World 會被替換 | 一個 Resource＋一個 extract System（同 `RenderImages` 的形狀） |
| `camera_system` 自己從 `Assets<Image>` 讀圖的大小（`bevy_render/src/camera.rs:292-298`） | `world_render` 經相機的 `WriteTargetSize` 把大小抄進 `CameraTargetSizes` | `camera` 不依賴 render（CM-2）；同 `window_camera` 抄視窗大小 | 一個 System |
| 目標圖的大小固定，跟視窗變要作者自己改（`camera.rs:292`）；`ViewTarget` 內部照相機目標的大小（`view/mod.rs:1416`） | `FollowTarget`：跟主目標或另一個 target | gyo2 的 pass 是資料，A4、D1、C4 的中間 target 要是看得到的 target | 無（後端多一個判斷） |
| 一張圖可開 sRGB／非 sRGB 兩種 view（wgpu `view_formats`） | 一張貼圖一種格式；target 只收 `Rgba8Unorm` | SDL_GPU 沒有 view（第 13.1 節）；sRGB target 的語意 A4 定 | 要兩種讀法時兩份貼圖（B4） |
| `Frustum`（`update_frusta`）與 `ViewUniform`（`prepare_view_uniforms`，`view/mod.rs:1230-1239`）各算一次 clip_from_world | `CameraView::clipFromWorld` 一個值，visibility 與兩條管線都取它 | 大地圖 #342 S-4：三處各算、各自可能錯；gyo2 的相機推導值本來就是被抽的元件 | 一個元件＋一個 System（形狀同 Bevy 的 `Frustum`）。TAA（D3）的 jitter 在渲染 World 改 `Pass3D::clipFromWorld`，不回寫相機（同 Bevy 在 prepare 時加 jitter，`view/mod.rs:1223-1235`） |

**別家對照**：Godot 的 viewport 自己有 RID，取樣要另拿 `viewport_get_texture`（`servers/rendering/rendering_server.h:577`），大小由 `viewport_set_size` 給（`:557`），清除方式由 `viewport_set_clear_mode` 給（`:574`）。gyo2 照 Bevy 用同一個 id（少一個 id 空間）。

### 13.4 判定（合併前）

- **T1**（cpu）：target 的 id 由資料側配、和圖片同一張表與計數器、不重用；`Find`／`FindTarget` 分得開；建立是一個依序的要求；target 可以跟另一個 target。來源的 `RenderTargets` 從 1 起、不重用；新的表是另一張、複本是同一張。
- **T2**（cpu）：來源 target 的大小進 `CameraTargetSizes`（固定的 scale 1；跟主目標或另一個 target 的抄被跟的；視窗變了跟著變；放掉的拿掉），相機在同一幀算得出來；大小經 `WriteTargetSize`，相同不寫、沒有不拿。
- **T3**（cpu）：渲染 World 對每個來源 target 有自己的 render target；同一個不重建；跟來源 target 的跟為它建的那張；換了表全部放掉重建；來源沒有了放掉；相機的 pass 畫在對應的 target；來源已放掉 target 的相機不畫（主目標也不畫）。
- **T4**（cpu）：`CameraView::clipFromWorld`＝`clipFromView × inverse(視點)`、`viewFromWorld`＝`inverse(視點)`；相機無效或視點沒有反矩陣時 `valid == false`；`UpdateCameraViews` 用這一幀 propagation 後的視點、動了才寫，`ComputedCamera` 不因為動了而寫；換算函式在視點沒有反矩陣時回 `InvalidData`。visibility、`Queue2d`、`Queue3d` 取同一個值。
- **T5**（gpu，`sdl_gpu.targets`）：畫進 8×8 的 target、在 2D pass 取樣到主目標，逐像素比；3D pass 也畫得進 target（它自己的深度）；新的 target 是透明黑，沒清除色的 pass 保留上一幀的畫面；放掉的 target 上的 pass 不畫、計一次；取樣自己這個目標的 draw 跳過、計數。
- **T6**（gpu）：`Rgba8UnormSrgb` 的 188／128 取樣成線性 128／55（±1）；`Rgba8Unorm` 照存的。
- **T7**（gpu）：跟主目標的 target 在 8×8 與 16×16 的 Capture 都和主目標一樣大（4×4 像素的方塊不被拉伸），重建不增加貼圖數；跟一個 4×4 target 的 target 是 4×4。
- **T8**：既有測試照過（`render.*`、`camera.module`、`sdl_gpu.pass2d`、`sdl_gpu.pass3d`、`ui.*`、`input.*`）；第 13.5 節的突變全部 killed。

**既有測試的改動**（只改輸入的做法與型別，判斷不變）：
- 管線測試的相機輔助函式（`Pipeline2dTests.cpp` 的 `Camera2d`、`Pipeline3dTests.cpp` 的 `Camera3d`、`VisibilityTests.cpp`、`Pass2DTests.cpp` 的兩處）用 `ViewOf` 給 `ExtractedCamera::view`（型別從視點矩陣改成 `CameraView`）；`[p3-clip-from-world]` 期待值的視點直接用同一個 `At(1, 0, -2)`。
- pass 的目標改成 `std::optional<TextureId>`：`target = 7` → `R::TextureId{7}`、`Camera3d(5, …)` → `R::TextureId{5}`、`UiPassTests.cpp` 的 `camera.target = 0` → `std::nullopt`。
- 宿主自己 `Insert` 組相機的測試（`PipelineWorldTests.cpp` 7 處、`VisibilityTests.cpp`、`WorldUiTests.cpp`、`CameraTests.cpp` 的 4 處）多放一個 `CameraView{}`（同它們已經放的 `ComputedCamera{}`）。

### 13.5 測試與突變

- 測試：`tests/common/render/RenderTests.cpp`（`render.data`）五案；`PipelineWorldTests.cpp`（`render.extract`）一案；`VisibilityTests.cpp`（`render.visibility`）一案；`tests/common/camera/CameraTests.cpp`（`camera.module`）三案；`plugins/sdl_gpu/tests/Pass2DTests.cpp` 的 `sdl_gpu targets:` 五案（ctest `sdl_gpu.targets`，標籤 `gpu`；每案用 `TargetCase` 在結束時放掉自己的貼圖，不靠別的案清乾淨）。
- 突變：`tests/common/render/target_mutations.json`，33 個（T1～T7 與審查的 S-1～S-3、H1、H3 各個獨立可錯的判斷；沒加的與理由在檔案的 `description`）。`Queue2d`、`Queue3d` 取同一個值由既有的 `p2-view-ignored`、`p3-view-ignored` 守。
- 數量的說明（合併前核准用）：改的條文是 CM-1、CM-5、新 CM-7、RD-3、新 RD-11，共 5 條，門檻 25；33 個超過的理由是 RD-11 是一條複合條文，綁了約 8 個獨立的決定（target 就是貼圖、跟別的目標、跨幀保留、回授、來源 id、大小副本、同步與表的身分、沒有對應的相機不抽、格式），README 基準 3 不設上限，只要求每個都寫出別的抓不到的錯（設計審查 (b) 逐個看過）。
- 既有突變（D184）：find 落在本批改過的程式裡的全部重跑，失效的改寫、守同一個性質；同一個改動的照 README 的合わせ方併成一個（`g3-unmapped-drawn`→`gp-unmapped-drawn`、`g3-colour-cleared`→`gp-load-always`、`rd-target-release-kept`→`rd-tex-release-kept-live`）；`camera_mutations.json` 的 `cm-target-always-zero`、`cm-letterbox-ignored` 回到 A1 前的 find（相機的程式回到原樣）。

### 13.6 RC

- **RC-A1-1**：來源的 sprite／網格要直接取樣來源的 target（小地圖當 `Sprite` 的圖）→ `ImageRef` 加一種「來源 target」，`RenderImages` 經 `TargetTextures` 對到貼圖（Bevy 的同一個 handle）；那時考慮把 `TargetTextures` 與同步移到 `render_world`（Bevy 的 `prepare_assets<GpuImage>` 在核心，設計審查 No-2）。現在作者在渲染 World 用自己的 pass 取樣。
- **RC-A1-2**：要 target 的一部分大小（半解析度的 SSAO、bloom 的 mip 鏈要被跟的目標的 1/2、1/4）→ `FollowTarget` 加比例（加法）。
- **RC-A1-3**：一幀裡多個不同大小的 3D target 讓每個 target 的深度貼圖成為記憶體的主要部分 → 深度改成依大小共用（RC-RP1 的 aliasing 那一層）。A2 讓深度成為看得到的 target 時一起看（設計審查 No-3）。
- **RC-A1-4**：多視窗、或第二個相機目標大小的擁有者出現 → `CameraTarget` 改成帶種類（Bevy 的 `RenderTarget` enum），或 id 分區（設計審查 No-5）。

### 13.7 不做

phase、全畫面 pass、只寫深度的 pass（A2）；除錯標記與計時（A3）；HDR、tonemapping（A4）；材質（B1）；mip 與壓縮（F1）；MSAA 的 target（D2）；「讀一個這一幀還沒被寫的 target」的後端斷言（RP-3，A2 定 pass 的讀寫宣告時一起做）。

## 14. A2 小宣告：camera phase 與 pass 種類（`v8-arch-gyo2-render-a2`）

2026-11-27。使用者說「A 段開始」（HANDOFF `v8-arch-gyo2-render-a-start`），主會話在 A1 之後交 A2。Opus／high（上段：公開介面 `CameraPasses`、`RenderPass`；pass 的資料形式）。小宣告寫完直接實作（主會話的指示：不等核准）。

讀的版本：本分支 `219a37f9`（A1＋審查修正，基底 gyo2 `172b7546`）；Bevy `6a911653f`；Godot `94fc09c`；SDL 3.4.0（`_deps/sdl3-src`）。

### 14.1 現況（程式，A2 前）與查到的事

- **`CameraPasses` 每台相機一個列表**（`Compose.hpp:20-22`）：`sprite_render`、`mesh_render`、`ui_render` 各自 `byCamera[i].push_back`（`SpriteRenderModule.cpp:92`、`MeshRenderModule.cpp:131`、`UiRenderModule.cpp:124`）。同一台相機裡的先後＝這些 System 跑的先後：UI 在最後，只因為 `QueueUiPasses` 排在 `PostUpdate`、`ComposeCameraPasses` 之前（`UiRenderModule.cpp:249`）。沒有「不屬於相機的 view」的位置。
- **pass 只有兩種**（`RenderCommands.hpp:193`）：`Pass2D`、`Pass3D`。pass 讀哪些 target 只隱含在 draw 的 `texture` 裡；寫哪個是 `target`。3D 的深度藏在後端（每個目標一張，`GPUDevice.cpp` 的 `GPUTarget::depth`，用途只有 `DEPTH_STENCIL_TARGET`），資料看不到（A1 設計審查 No-3）。
- **shader 怎麼被認得**：內建 shader 是 `sdl_gpu` 嵌進去的表（`EmbeddedShaders()`），後端在 `Create` 以名字找（`FindEmbeddedShader("pass2d.vert")` 等）；名字與原始檔寫在 `ShaderAbi.hpp`。shader 工具只產生一個固定名字的函式 `EmbeddedShaders()`（`ShaderTool.cpp` 的 `Source`）。測試用的 shader 只有工具自己的 `shader_tool/tests/broken.frag.hlsl`（給工具的拒絕測試）。
- **〔源〕Bevy 0.20 的 pass 與 phase**：`Core3dSystems { Prepass, MainPass, EarlyPostProcess, PostProcess }` 用 `chain_weak` 排先後（`bevy_core_pipeline/src/schedule.rs:49-55,69`）；`main_opaque_pass_3d`、`main_transparent_pass_3d` 在 `MainPass` 裡 chain，`tonemapping` 在 `PostProcess`（`core_3d/mod.rs:145-167`）；UI 是 `ui_pass.after(Core3dSystems::PostProcess).before(upscaling)`（`bevy_ui_render/src/lib.rs:302`）。`camera_driver` 先跑所有 `RootNonCameraView`（點光、聚光的陰影，`bevy_pbr/src/render/light.rs:2235,2328`），再照順序跑相機的 schedule（`schedule.rs:136-152`）。graph 物件的拿掉與理由：commit `f1f41547f`（#22144）的說明——slot 式的資源管理從沒被用過、Schedule 已涵蓋 graph 做的事。
- **〔源〕Bevy 的全畫面 pass**：`FullscreenShader`（`fullscreen_vertex_shader/mod.rs:8`），一個三角形、`draw(0..3)`，uv (0,0) 在左上（`fullscreen_vertex_shader.wesl:25-32`）；`FullscreenMaterial` 的綁定是「一張輸入貼圖＋sampler＋一個 uniform」（`fullscreen_material.rs:148-150`），預設在 `PostProcess`、`tonemapping` 之前（`:95-99`），不混合（`blend: None`，`:129`）。
- **〔源〕Bevy 的深度**：`CORE_3D_DEPTH_FORMAT = Depth32Float`（`core_3d/mod.rs:5`）；要取樣時加 `TEXTURE_BINDING`（`:749`）；`DepthPrepass` 讓深度先寫在 prepass（`prepass/mod.rs:65`、`ViewPrepassTextures` `:126`）。
- **〔源〕SDL_GPU**：深度格式只有 D16 同時保證能當 depth target 與取樣（`SDL_gpu.h:703,747`；D24／D32 二選一）；graphics pipeline 一定要有 fragment shader（`SDL_gpu.c:1042`，除錯模式斷言）。
- **〔源〕shader 的輸入位置**：DXC 照宣告順序編 location，不照 `TEXCOORDn` 的 n——只放 position 與 world 的頂點結構會讓 world 落到 uv 的 location（本批實測：只寫深度的 pass 的 prepass 什麼都沒畫；改用和 3D pass 同一個 `Mesh3DVertex` 後正確）。

### 14.2 前提：誰要解決

| 項 | 前提 | 理由 |
|---|---|---|
| 一台相機裡 pass 的先後與「不屬於相機的 view」先畫 | **〔Engine〕** | 各 Module 的 System 不知道彼此；只有合成的一方能定順序（Bevy 的 `camera_driver`） |
| 全畫面 pass、只寫深度的 pass 這兩種 pass | **〔Engine〕** | 後端要錄成 GPU 命令；作者伸不進 `sdl_gpu`（RD-1） |
| 深度是資料看得到的 target、格式由後端選 | **〔Engine〕** | 裝置支援哪種深度格式只有後端知道（SDL 只保證 D16） |
| pass 讀哪些 target（`ReadsOf`）、讀沒有畫面的 target 是錯 | **〔Engine〕**機制、**〔作者〕**排順序 | Engine 不重排（Q2）；先後由作者照順序放 |
| 某個後處理用哪個 shader、參數多少、放哪個 phase | **〔作者〕** | 遊戲的資料（第 4.2 節）；A2 不提供任何內建的全畫面效果 |
| 全畫面 pass 的 shader 從哪來 | **〔Engine〕**機制（名字）、**〔作者〕**的 shader 等 B2 | 共通內容管線、`ShaderData` 是 B2；材質的 `ShaderId` 是 B1 |

### 14.3 設計（卡片判）

1. **phase**（`Compose.hpp`）：`enum class CameraPhase { Prepass, Opaque, Transparent, PostProcess, Overlay }`（總宣告第 4.3 節的五個名字）；`CameraPasses::byCamera` 從 `vector<vector<RenderPass>>` 改成 `vector<CameraPhases>`，`CameraPhases` 是五個列表（`phases[CameraPhase::Opaque]`）。各 Module 放進的 phase：`mesh_render`→`Opaque`（它的一個 `Pass3D` 裡，混合的 draw 本來就排在不透明的之後；拆成兩個 pass 是之後有東西要插在中間時的事）、`sprite_render`→`Transparent`（Bevy 的 `Transparent2d`）、`ui_render`→`Overlay`（Bevy 的 `ui_pass` 在 `PostProcess` 之後）。同一個 phase 裡照放進去的先後，也就是 System 的先後（before／after）。`ComposePasses`：先放 `nonCameraViews`，再照相機順序放「清除、各 phase」。
   - **不屬於相機的 view**：`CameraPasses::nonCameraViews`（Bevy 的 `RootNonCameraView`）：所有相機之前；它的 pass 畫在自己的 target（陰影貼圖），所以不影響「目標上第一個相機清整個目標」的判斷（判斷照舊看 `out` 裡已經在的 pass 的目標）。和 `byCamera` 一樣在 `ExtractCameras` 與合成時清空。
2. **pass 的種類**（`RenderCommands.hpp`）：`RenderPass = variant<Pass2D, Pass3D, PassFullscreen, PassDepth>`（名字照既有的 `Pass2D`／`Pass3D` 的寫法）。
   - **`PassFullscreen{target, viewport, shader, inputs, parameters}`**：一個三角形蓋住 viewport（內建 `fullscreen.vert`，Bevy 的 `FullscreenShader`）；fragment shader 以 `inputs` 的順序在 sampler slot 0、1、… 取樣，`parameters`（bytes，float4 lane）是 fragment uniform slot 0（space3）；不混合、直接取代（Bevy 的 `blend: None`）。顏色的起點同 `Pass3D`（照目標現在的畫面）。ABI 在 `Fullscreen.hlsli`＋`ShaderAbi.hpp`（RD-4）。
   - **`PassDepth{target, viewport, clipFromWorld, clearDepth, vertices, indices, instances, draws}`**：沒有顏色、只寫 `target`（`Depth` 格式的 target）的深度；draw 用 `Draw3D`（深度與 cull 照用；貼圖與混合不用），pipeline 是 3D pipeline 的「只寫深度」那一支（內建 `depth.vert`／`depth.frag`：SDL 一定要 fragment shader）。
   - **`Pass3D::depth`**（`optional<TextureId>`）：有值時這個 pass 測試、寫入那張 `Depth` target（prepass 寫的那張；Bevy 的 `DepthPrepass` 之後主 pass 讀同一張深度），沒有值時照舊用目標自己的深度（Bevy 的 `ViewDepthTexture`）。那張深度要和目標一樣大（Programmer Error，後端斷言）。
3. **深度格式**：`TextureFormat::Depth`（A1 設計審查 No-3 的建議）。只給 render target（`CreateTarget`）；`TextureDesc` 的 CPU 像素是 4 bytes RGBA，`Depth` 是 Programmer Error（A1 審查 No-4：寫明 `TextureDesc` 收哪些格式）。`Depth` target 要 `Nearest`（Programmer Error；SDL／Vulkan 不保證深度能線性過濾，Bevy 的深度也綁 non-filtering sampler）。**具體格式由後端選**：D32F、D24、D16 之中第一個「能當 depth target 又能取樣」的（D16 有保證）；3D pass 自己的深度也用同一個格式（pipeline 只有一種深度格式）。取樣時第一個通道是深度。
4. **讀寫的宣告**：pass 寫的是 `target`（`TargetOf`，`PassDepth` 的是它的深度 target）與 `Pass3D::depth`；讀的是 `ReadsOf(pass)`：2D／3D 的 draw 的貼圖（白貼圖不算）、全畫面 pass 的 `inputs`；`PassDepth` 不讀（draw 的貼圖不用）；`Pass3D::depth` 是畫上去的 attachment，不是讀。資料本身就是宣告（另外加一個 `reads` 列表會和 draw 的貼圖不一致）。Godot 的 `RenderingDeviceGraph` 也是從命令推出讀寫（`servers/rendering/rendering_device_graph.h:183` 的 `ResourceTracker`），差別是它拿來自動排序，這裡只拿來檢查契約。
5. **RP-3 定稿（卡片判，讀法交使用者確認，見第 14.8 節）**：GPU 的先後＝`RenderCommands` 的順序；沒有第二個排序者、不剔除、不 aliasing。**pass 取樣的 render target 要有畫面**：自 `CreateTarget`（或因大小變了被後端重建）之後，某個 pass 畫過它——同一幀前面的 pass，或前幾幀（A1 的跨幀保留）；清除也算畫過。取樣一張沒有畫面的 target 是 Programmer Error，`sdl_gpu` 在錄那個 pass 之前對它的 `ReadsOf` 斷言。沒有 GPU 貼圖的（放掉、被拒）與 pass 自己的目標照 A1 的規則跳過並計數，不斷言。跟別的目標大小走的 target 在大小變的那一幀被重建、沒有畫面：要取樣它，就要每幀先畫（HDR、後處理的中間圖本來就是）。`Depth` target 沒有畫面時，第一個畫它的 pass 從清成 1 開始（同顏色 target 的透明黑）。
6. **全畫面 pass 的 shader 怎麼命名**：`PassFullscreen::shader` 是**後端帶著的 shader 的名字**（HLSL 檔名去掉 `.hlsl`），和現在 2D／3D 的內建 shader 被認得的方式一樣（`ShaderAbi.hpp` 的名字、`FindEmbeddedShader`）；後端沒有這個名字時那個 pass 不畫、計一次 `skippedDraws`（同沒有 GPU 貼圖的 draw）。shader 的 stage 不是 fragment、`inputs` 的數目和 shader 的 sampler 數不同、有沒有 `parameters` 和 shader 有沒有 uniform 不一致，是 Programmer Error（後端斷言）。**理由**：B1 的 `ShaderId`（資料側配，同 `TextureId`）要和材質、pipeline 的鍵一起設計；A2 先做一個 id 空間會把 B1 的形狀定下來。名字是最少的承諾：B1 時把 `std::string shader` 換成 `ShaderId`、內建的在啟動時配 id。**A2 不加任何內建的全畫面 fragment shader**：第一個是 A4 的 tonemapping；沒有消費者的內建（例：copy／blit）不先做。
7. **測試的 shader**：`plugins/sdl_gpu/tests/shaders/test_invert.frag.hlsl`、`test_shift.frag.hlsl`（照 `shader_tool/tests/broken.frag.hlsl` 的先例放在 Plugin 的測試目錄），由同一個 shader 工具在建置時編成**測試執行檔自己的表**：工具加 `--function <名字>`（預設 `EmbeddedShaders`），測試用 `--function TestShaders` 產生另一個函式；`GPUDevice::Create` 多一個參數 `more`（除了嵌進去的以外、全畫面 pass 可以指名的 shader；Plugin 不給）。**理由**：不把測試的 shader 嵌進 Plugin（產品帶著測試碼）；不在建置裡改寫產生的檔（脆弱）；`more` 是 adapter 內部介面（`src/GPUDevice.hpp`），Plugin 的公開介面不變。B2 的遊戲 shader 走 asset 與共通內容管線，`more` 那時改成 B2 的路或拿掉（RC-A2-3）。

**偏離 Bevy 的地方、理由與膠水**：

| Bevy | gyo2 | 理由 | 膠水 |
|---|---|---|---|
| 每台相機一個 schedule（`CameraRenderGraph`），phase 是 system set，pass 是直接錄 GPU 命令的 System | phase 是 `CameraPhases` 的五個列表；System 往列表放 pass 資料，`ComposePasses` 照 phase 串起來 | pass 是資料（RD-1），`sdl_gpu` 讀了才錄；不需要 Kernel 的「每台相機跑一次 schedule」 | 一層 phase（總宣告第 4.3 節原文） |
| `Prepass, MainPass, EarlyPostProcess, PostProcess`；UI 是 `PostProcess` 之後的一個 System | `Prepass, Opaque, Transparent, PostProcess, Overlay` | 總宣告第 4.3 節定的名字（使用者 Q2「照原文」）；`MainPass` 拆兩格讓 2D（透明）排在 3D（不透明）之後；`EarlyPostProcess` 沒有消費者，要時加一格 | 無 |
| `RootNonCameraView` 是帶自己 schedule 的實體 | `CameraPasses::nonCameraViews` 一個列表 | 同上（資料） | 一個欄位 |
| 深度固定 `Depth32Float` | `TextureFormat::Depth`，後端選 D32F／D24／D16 | SDL 只保證 D16 能當 depth target 又能取樣 | 後端一個選擇迴圈 |
| 全畫面效果是一個 `FullscreenMaterial` 型別（shader＋uniform 型別） | `PassFullscreen{shader 名字, inputs, parameters bytes}` | 材質機制是 B1；A2 只做 pass 的資料 | B1 換成 `ShaderId` |
| 讀一張沒寫過的貼圖讀到 0（wgpu 會把資源清成 0〔知〕） | 讀一張沒有畫面的 target 是 Programmer Error（RP-3） | 總宣告第 4.3 節與 Q2 的原文是「契約、後端斷言」；SDL_GPU 不保證新貼圖的內容 | 一個斷言 |

### 14.4 判定（合併前）

- **P1**（cpu）：一台相機裡照 `Prepass → Opaque → Transparent → PostProcess → Overlay` 合成、同一個 phase 照放進去的先後；相機照順序；合成後每個列表都空了。
- **P2**（cpu）：`nonCameraViews` 在所有相機之前；它畫在自己的目標上時，第一個相機照樣清整個主目標。
- **P3**（cpu，`render.extract`）：在 `Update` 排在管線 System 之前放的 `PostProcess` pass，合成後在 `mesh_render` 的 `Pass3D` 與 `sprite_render` 的 `Pass2D` 之後；`sprite_render` 的 pass 在 `mesh_render` 之後（它的 System 先跑）；`nonCameraViews` 最先。`render.extract` 既有的案原樣通過。
- **P4**（cpu）：`TargetOf` 對四種 pass、`ReadsOf`（白貼圖不算、全畫面的 `inputs` 照順序、`PassDepth` 不讀、`Pass3D::depth` 不是讀）。
- **P5**（cpu）：`Depth` target 建得出來、`Linear` 的 `Depth` target 與 `Depth` 的 `TextureDesc` 是 Programmer Error。
- **P6**（cpu，`sdl_gpu.shaders`）：內建的 `depth.vert`、`depth.frag`、`fullscreen.vert` 的資源數和 `ShaderAbi.hpp` 一致。
- **P7**（gpu，`sdl_gpu.phases`）：兩個全畫面 pass 串接（反色→平移 2 像素）的像素；參數到得了 shader；下一幀取樣上一幀畫的 target（保留的畫面）；後端沒有那個 shader 時不畫、計一次。
- **P8**（gpu）：只寫深度的 pass 寫進 `Depth` target；之後的 `Pass3D`（`depth` 指它、`clearDepth == false`）被擋住的地方不畫、沒擋住的畫、並寫入自己的深度；2D pass 取樣它得到 0.25／0.5／1（±1/255）。
- **P9**（gpu，`sdl_gpu.unwritten`）：取樣一張沒有畫面的 target 是後端的斷言（死亡測試：自己的 handler 印標籤並結束行程；正常的執行裡這一案 `skip`，ctest 用 `--no-skip` 跑）；P7 的下一幀證明有畫面的不斷言。
- **P10**：既有測試照過（`render.*`、`camera.module`、`sdl_gpu.*`、`ui.*`）；第 14.5 節的突變全部 killed。

**既有測試的改動**（只改輸入的做法與型別，判斷不變）：
- `Pipeline3dTests.cpp` 的合成案：`passes.byCamera[0].push_back(...)` → `passes.byCamera[0][R::CameraPhase::Opaque].push_back(...)`（2D 的那格 `Transparent`）；`[cp-emptied]` 看同一格。
- `Pass2DTests.cpp` 的 2D 管線案：`slots.byCamera[0].push_back` → `slots.byCamera[0][R::CameraPhase::Transparent].push_back`。

### 14.5 測試與突變

- 測試：`RenderTests.cpp`（`render.data`）二案；`Pipeline3dTests.cpp`（`render.pipeline3d`）一案；`PipelineWorldTests.cpp`（`render.extract`）一案；`SDLGPUTests.cpp`（`sdl_gpu.shaders`）一案；新 `plugins/sdl_gpu/tests/PhaseTests.cpp`：`sdl_gpu phases:` 三案（ctest `sdl_gpu.phases`，標籤 `gpu`）、`sdl_gpu unwritten:` 一案（ctest `sdl_gpu.unwritten`，標籤 `gpu`，`PASS_REGULAR_EXPRESSION` 看成功的標籤，失敗的標籤不同）。
- 突變：`tests/common/render/phase_mutations.json`，現在 58 個（P1～P9、修訂 1、第 14.11～14.14 節各個獨立可錯的判斷；沒加、退役的與理由在檔案的 `description`）。
- 數量的說明（合併前核准用）：改的條文是 RD-4、RD-11、新 RD-12、RP-3，共 4 條（第 14.13 節另改 SDL-5），門檻 20；58 個超過的理由：第 14.11～14.14 節每一節的機制（資料側的大小、更新模式、沿用、收據、兩份畫面）各有幾個獨立的決定，每個一個突變（README 基準 1、3），最初的 23 個是 RD-12 綁了 phase 的先後、不屬於相機的 view、三個 Module 各自的 phase、兩種新 pass、讀的宣告這幾個獨立的決定（README 基準 3 不設上限，只要求每個都寫出別的抓不到的錯）。
- 既有突變（D184）：find 落在本批改過的程式裡的全部重跑（`GPUDevice.cpp` 的 3D 錄製與 pipeline、目標的建立；`ComposePasses`；`Render.cpp` 的 `Create`／`CreateTarget`；`ExtractCameras`），失效的改寫、守同一個性質；被改過的測試殺的（`cp-*`、`gp-pipe-*`）也重跑。結果記在 dev_log。

### 14.6 RC

- **RC-A2-1**：要在 `Opaque` 與 `Transparent` 的 3D draw 之間插 pass（transmission、SSR 要不透明的結果當輸入）→ `Queue3d` 拆成兩個 `Pass3D`，透明的放 `Transparent`（Bevy 的 `main_opaque_pass_3d`／`main_transparent_pass_3d`）。
- **RC-A2-2**：要 `EarlyPostProcess`（Bevy 在 `PostProcess` 之前的那一格：例如 DOF、motion blur 在 tonemapping 之前）而同一個 phase 裡 System 的 before／after 不夠用 → `CameraPhase` 加一格（加法）。
- **RC-A2-3**：B1／B2 有了 `ShaderId` 與 `ShaderData` → `PassFullscreen::shader` 換成 `ShaderId`；`GPUDevice::Create` 的 `more` 換成 B2 的路或拿掉。
- **RC-A2-4**：一個 view 要同時寫顏色與深度以外的 target（normal、motion vector 的 prepass：C4、D3）→ pass 加 MRT（多個顏色 target）。
- RC-A1-3（每個 target 的深度共用）照舊；`Pass3D::depth` 讓作者自己共用一張。

### 14.7 不做

除錯標記與計時（A3）；HDR、tonemapping 與任何內建的全畫面效果（A4）；材質與 `ShaderId`（B1）、遊戲 shader（B2）；陰影本身（C1）；MRT（RC-A2-4）；MSAA（D2）；自動重排、剔除、aliasing（Q2）。

### 14.8 和總宣告原文不同、交使用者確認的一點（已決定：第 14.11 節）

- **RP-3 的「這一幀」**：總宣告第 4.3 節與第 9 節寫「讀一個**這一幀**還沒被寫的 target」是契約。A1 照 Bevy 讓 target 的畫面跨幀保留（第 13.3 節第 1 點、設計審查 S-2），所以「這一幀沒寫、前幾幀寫過」的 target 有畫面（例：每兩幀更新一次的監視器畫面），讀它是對的；真正沒有定義的是「建好（或重建）之後還沒畫過」。本批照後者斷言（第 14.3 節第 5 點）。照字面（這一幀沒寫就斷言）會讓 A1 的跨幀保留只剩「在上面疊畫」一種用途。Bevy 沒有這個斷言（wgpu 把新資源清成 0〔知〕）。
- **使用者的決定**（2026-11-27，HANDOFF `v8-arch-gyo2-render-a2-target-reuse`）：上面的讀法不採用，(A)／(B)／(C) 都不選。原條文不改、改機制：明示的「沿用」算這一幀寫過，條文加一句「讀的 target 必須這一幀寫過或明示沿用，兩者皆非就斷言」；重建的那一幀強制重畫。機制（資料側定大小、相機的更新模式）在第 14.11 節（使用者 2026-11-27「照建議，主要是解法符合邏輯，而不是補丁。」，HANDOFF `v8-arch-gyo2-render-a2-target-size-data-side`）。第 14.3 節第 5 點與第 14.10 節第 3 點的 RP-3 以第 14.11 節為準。

### 14.9 SDL 的斷言在測試裡不開對話框（主會話的指示，A2 實作中）

- **發生的事**：本機（macOS、Metal）跑 A2 的突變時，突變 `gd-depth-target-colour-usage`（`Depth` target 用顏色 target 的用途）讓 `SDL_CreateGPUTexture` 的驗證斷言（`SDL_gpu.c:1274-1276`：深度格式的用途只能有 `DEPTH_STENCIL_TARGET` 與 `SAMPLER`），SDL 的預設 handler 開了對話框（`SDL_PromptAssertion`，`SDL_assert.c:129-220`），要使用者按。**A2 本身的程式沒有這個問題**：深度格式只以 `DEPTH_STENCIL_TARGET | SAMPLER`（`Depth` target）或 `DEPTH_STENCIL_TARGET`（目標自己的深度）建；那是故意寫錯的突變。
- **為什麼 Metal 走到 SDL 的斷言**：建貼圖前的 `SDL_GPUTextureSupportsFormat` 在 Metal 只查「深度用途要深度格式」，不查反過來（`SDL_gpu_metal.m:4176-4180`），所以對 D32F＋`COLOR_TARGET` 回 true；Vulkan 把用途換成 `VkImageUsage` 問驅動（`SDL_gpu_vulkan.c:10992-11001`），會回 false，後端算一次被拒、不進 SDL 的驗證。**Linux L1 不會發生**：一是 Vulkan 先拒；二是突變不在 CI 跑（D184）；三是 A2 還沒推、還沒經過 L1。
- **做法**（SDL 自己的機制）：`plugins/sdl/tests/adapters/sdl/UnattendedAssertions.cpp` 在 main 之前呼叫 `SDL_SetAssertionHandler`（`SDL_assert.h:589`），handler 把條件與位置印到 stderr、回 `SDL_ASSERTION_ABORT`（SDL 會 `SDL_Quit` 再 abort，`SDL_assert.c:123-127`）；`SDL_Quit` 不會換回預設 handler（`:387`）。做成 OBJECT library `gyo_sdl_unattended_assertions`，連進每個會啟動 SDL 的測試執行檔（`gyo_sdl_tests`、`gyo_sdl_audio_tests`、`gyo_sdl_gpu_tests`、`gyo_sdl_window_tests`）；它在 Plugin 的測試 adapter 目錄（SDL-1）。跑產品執行檔的 ctest（`sdl_window.example`、`window_mode.example`、`object_connect.main`、`object_fps_pvp.main`、`object_fps_pvp.main_connect`）沒有測試的 main，改用 SDL 的 hint：環境變數 `SDL_ASSERT=abort`（`SDL_hints.h:4587-4609`；預設 handler 讀它、不開視窗，`SDL_assert.c:177-196`）。這是讓測試能無人跑完，不是擋誤用的守衛。
- **檢查**：每個執行檔一案「handler 已裝好」（`[sdl-assert-unattended]`）；`sdl.unattended_assertion`（cpu）在子行程強制一個 SDL 斷言（平常 `skip` 的一案，同時把 hint 設成 abort 當第二層網：handler 沒裝時也不開對話框，只是少了 handler 的字、ctest 失敗），由 `ExpectUnattendedAbort.cmake` 判「行程結束、印出 handler 的字」（ctest 對訊號結束的行程不看輸出一律算失敗，所以用腳本判）。突變 `sdl-unattended-handler-not-installed`（`sdl_mutations.json`）killed。

### 14.10 修訂 1：設計審查與純淨 Code Review 之後（2026-11-27）

審查兩份（主 checkout `build/evidence/v8-arch-gyo2-render-a2/`，不在 git）：每批的設計審查（Opus／high，Blocker 0、Should 2、Nit 5、Note 6）、純淨 Code Review（Fable／high，高 3、中 6、低 8）。逐項的處理與一句理由在 dev_log。改了的形狀（第 14.3 節的對應點以這裡為準）：

1. **mesh 類 pass 共用**（純淨 H3）：`MeshGeometry{viewport, clipFromWorld, vertices, indices, instances}` 是 `Pass3D` 與 `PassDepth` 的共同部分（繼承，所以 `pass.draws`、`pass.instances` 的寫法不變）；`MeshDraw{mesh, 範圍, depthTest, depthWrite, cull}` 是 draw 的共同部分，`Draw3D : MeshDraw` 多 `texture`、`blend`。`PassDepth::draws` 是 `MeshDraw`：沒有用不到的欄位。後端的 mesh pipeline 以「有沒有顏色輸出（`MeshColour{format, blend}`）」為鍵的一部分，只寫深度＝沒有顏色輸出（同一個概念一個名字；混合不再把只寫深度的 pipeline 分成兩條，純淨 M6）。
2. **寫的宣告**（純淨 M1）：`WritesOf(pass)`＝`PassWrites{colour, colourTarget, depthTarget}`（顏色：`PassDepth` 以外都畫；深度：`Pass3D::depth`、`PassDepth::target`）。`TargetOf` 由它推出（後端開 render pass 的那個 attachment）。用在三處：`ComposePasses` 判「目標上第一個相機」（前面的相機的 pass 畫過顏色的目標，後面的相機不再當第一個——之前只看相機的目標，前一台相機的 `PostProcess` 寫進的 target 會被後一台整張清掉：先寫成失敗的測試 `[cp-written-not-first]` 再修）；後端「取樣自己畫的東西」的 draw 跳過（`Pass3D` 取樣自己的深度 target 也算）；`FirstUnwrittenRead`。
3. **RP-3 的檢查改成 render 的純函式**（純淨 M5；行為暫不改，等使用者選 (A)／(B)／(C)）：`FirstUnwrittenRead(passes, drawn, noPicture)` 回「第一個取樣沒有畫面的 target 的 pass」；後端在解析完目標、錄任何東西之前，以這一幀還沒有畫面的 target 呼叫它並斷言。測試不用 GPU（`render.data` 的 `[rd-unwritten-*]`）。`Submit` 要先拿到 swapchain 才知道跟主目標的 target 的大小（重建與否），所以在取 command buffer 之後、錄之前，不在取之前。「沒有畫面 → 第一次寫時清」收成後端一個函式 `BeginWrite`（顏色、`Pass3D::depth`、`PassDepth` 三處都經它）。選 (B) 時改的是這兩處：後端把 `noPicture` 的 target 先清，而不是斷言。
4. **格式分兩個型別**（純淨 M3、設計 N-4／S-2）：`TextureFormat{Rgba8Unorm, Rgba8UnormSrgb}` 給貼圖，`RenderTargetFormat{Rgba8Unorm, Depth}` 給 target：「Depth 的貼圖」與「sRGB 的 target」寫不出來，原本的兩個斷言拿掉。`Depth` target 不看 `filter`，後端一律用最近的 texel 取樣（AGENTS §1.10）。
5. **phase**（純淨 M2、設計 N-2）：`CameraPhase` 從 0 起（和專案其他 enum 不同，理由：它是索引，`CameraPhase{}` 也是一個 phase），`kCameraPhaseCount` 由最後一個值推出；`CameraPhases` 的列表私有、`InOrder()` 給合成用。五個名字不變（設計審查 (c)）。
6. **`nonCameraViews` 改名 `beforeCameras`**（純淨 L3、設計 N-5）：它是「在所有相機之前畫的 pass」；註解改成點光、聚光的陰影（Bevy 的 `RootNonCameraView`，`bevy_pbr/src/render/light.rs:2235,2328`；方向光的 cascade 跟著相機，C1 多半放相機的 `Prepass`）。只有 `ComposePasses` 清它（`ExtractCameras` 只做每台相機的格子）。`ComposePasses` 搬到 `Compose.cpp`。
7. **全畫面 pass**（純淨 H2，只修便宜的；形狀照設計審查 (b) 保留到 B1）：**到 A4 的 tonemapping 之前，Engine 沒有任何產品可用的全畫面 fragment shader**（只有測試的）；`more` 的 shader 也算進建裝置時的格式交集；`more` 與嵌進去的重名、`more` 自己重名是 Programmer Error（不再默默遮住）；`parameters` 要是 16 bytes 的倍數（Programmer Error）。其餘（id 登記、經 Plugin 選項給 `more`）交 B1／B2（RC-A2-3）。
8. **其他**：`GPUDevice::Create` 的內建 shader 是一張（原始檔名 → 成員）的表，名字由 `ShaderAbi::ShaderName(k*Source)` 推出（純淨 L1）；`RenderPass` 的分派都用窮舉的 overloaded visitor（純淨 M4）。

第 14.8 節（RP-3 的讀法）與 `gt-unwritten-*` 照舊，等使用者的決定。

### 14.11 RP-3 的機制：資料側定大小、相機的更新模式、明示沿用（使用者的決定）

2026-11-27。兩個使用者的決定：一、RP-3 原條文不改、改機制，條文加一句「讀的 target 必須這一幀寫過或明示沿用，兩者皆非就斷言」（HANDOFF `v8-arch-gyo2-render-a2-target-reuse`；原話「我其實想維持原條文，需要改的是機制。……」，主會話轉）；二、機制照卡片的建議（HANDOFF `v8-arch-gyo2-render-a2-target-size-data-side`；原話「照建議，主要是解法符合邏輯，而不是補丁。」）。上段（render 的公開介面、相機的元件）。

**例子**：走廊的監視器畫面（一個跟視窗大小走的 target、一台相機）每兩幀拍一次。第 1 幀相機畫進 target、螢幕的材質取樣它；第 2 幀相機不畫，`RenderCommands` 裡有一句「沿用 target」，螢幕照樣取樣它（算這一幀寫過）。使用者拖動視窗：target 的大小變了、內容沒了，那一幀相機一定畫，不會沿用、也不會斷言。哪一幀誰寫了哪個 target，都在資料裡看得到。

**前提**：

| 項 | 前提 | 理由 |
|---|---|---|
| target 的大小、「這一幀新建或重建」 | **〔Engine〕** | 要在合成之前知道，才能把「沿用」換成「畫」；只有資料側（`RenderTextures`）在合成之前定得了 |
| 相機哪一幀畫、哪一幀沿用 | **〔Engine〕**機制、**〔作者〕**選模式 | 作者選 Always／Once／Disabled；強制重畫是 Engine 的事 |
| 作者自己的 pass 畫的 target 要沿用 | **〔作者〕** | 作者在自己的 System 放一句 `PassReuse`（同 Engine 放的那一句） |

**設計**：

1. **大小在資料側定**：`RenderTextures` 對每個 target 照描述算大小（固定、主目標的、另一個 target 的；照 id 順序），每次變了發一個 `TextureTargetSize{id, size}` 要求；後端只照要求建（重建）貼圖，不再自己解析 follow（拿掉 `EnsureTargets`）。主目標的大小由 `camera_render` 在 extract 從來源的 `CameraTargetSizes[kMainTarget]`（`window_camera` 寫的視窗大小）給（`SetMainSize`）。同 Bevy：`prepare_view_targets` 照抽出的相機目標大小建貼圖；同 Godot：viewport 的大小是場景的資料（`servers/rendering/renderer_viewport.cpp:1135-1141`）。
2. **畫面（picture）是資料**：target 建好、或大小變了，就沒有畫面；一幀的 `RenderCommands` 有 pass 畫它（顏色或深度），那一幀結束時就有了（`render_world` 的 Cleanup 呼叫 `KeepPictures`）。沿用不給畫面（沒有畫面的 target 沿用了也還是沒有）。
3. **明示沿用 `PassReuse{target}`**：`RenderPass` 的一種，什麼都不畫；對後面取樣的 pass 而言，那個 target 算這一幀寫過。Engine 放（下一點）或作者自己放。
4. **相機的更新模式 `TargetUpdate{Always, Once, Disabled}`**（`render` 的元件，`world_render` 登記；Godot SubViewport 的 `render_target_update_mode`，`scene/main/viewport.h:917-921`）：沒有元件＝Always。`camera_render` 在 extract 時：Disabled 的相機、它的 target **有畫面** → 放一句 `PassReuse`、不抽這台相機（管線不排它的 pass）；target 沒有畫面（新建、重建、從沒畫過）→ 照常抽、照常畫（強制重畫）。Once＝這一幀畫，下一幀起 Disabled：`world_render` 在 Last 記下哪些相機是 Once（渲染 World 抽到的就是這個狀態），下一幀的 First 把它們改成 Disabled（Godot 在畫完後把模式寫回 `UPDATE_DISABLED`，`renderer_viewport.cpp:982-983`）。主目標的畫面不跨幀保留（視窗的畫面每幀未定），所以主目標上的相機一定畫——不是例外，是「沒有畫面就畫」那一條。
5. **RP-3 的檢查只看資料**：`FirstUnwrittenRead(passes, targets)`：一個 pass 取樣某個 render target，而這一幀前面沒有 pass 畫它、也沒有沿用 → 後端在錄之前斷言。後端畫不出來的（沒有貼圖）不影響判斷——條文講的是程式。

**偏離 Bevy／Godot 與膠水**：

| Bevy／Godot | gyo2 | 理由 | 膠水 |
|---|---|---|---|
| Bevy：相機 `is_active = false` 時目標圖照舊（隱含的跨幀保留）；圖重建後內容沒了、不強制重畫 | 沿用是明示的一句 `PassReuse`；重建那一幀強制重畫 | 使用者的條文：每一幀誰寫了哪個 target 看得到，錯的順序是斷言、不是黑畫面 | 一種 pass、一個元件、`camera_render` 的一個判斷 |
| Godot：`UPDATE_DISABLED` 改大小時只重配、不重畫（`renderer_viewport.cpp:831`、`:1135-1141`） | 沒有畫面就畫 | 同上；「沿用」只對有畫面的 target 有意義 | 同上 |
| A1（卡片判）：後端照 swapchain／Capture 解析 follow 的大小 | 資料側解析，後端照要求建 | 「重建」要在合成前就是資料；同 Bevy 的 prepare_view_targets | `RenderTextures` 一個解析函式、一種要求 |

Unreal 的 SceneCapture（`bCaptureEveryFrame`、`bCaptureOnMovement`、`CaptureScene()`）是同一類「作者決定哪一幀拍」〔知，本機沒有原始碼〕。

**比過、沒選的**：(A)／(B)／(C)（第 14.8 節）；「沿用 T＋附上重畫用的 pass，後端決定」（不用動大小的解析，但每幀照樣抽取、排 pass，只省 GPU）；「每幀真的複製」（使用者原本的想法：每幀多讀寫一整張、兩份記憶體，重建時複製的來源也沒了，一樣要強制重畫）。

**使用者接受的代價**：推翻 A1 的卡片判「後端定 follow 的大小」；跟視窗走的 target 照來源 World 知道的視窗大小，可能比 swapchain 晚一幀；A1 的 follow 測試與突變換成新的輸入（守同一個性質）；重建時強制重畫是 Bevy／Godot 之外的膠水。

**A1 條文的變化**（RD-11）：跨幀保留對「疊畫」不變（沒有清除色的 pass 從上一幀的內容開始，Bevy 的 `LoadOp::Load`）；**取樣**要這一幀畫過或沿用；新建（或重建）時第一個 pass 從透明黑開始照舊；follow 的大小由資料側解析（`Capture` 的大小不再決定 target 的大小）。

**判定**：
- **U1**（cpu，`render.data`）：大小在資料側：固定的建好就有大小、跟另一個的跟它、跟主目標的在主目標大小知道後才有；每次變了一個要求、一樣的不發；主目標大小未知時保留原大小；新建、改大小沒有畫面，畫過的有，沿用不給畫面。
- **U2**（cpu，`render.extract`）：Disabled 的相機第一幀（沒有畫面）畫；之後放一句沿用、不畫；視窗變大（target 重建）那一幀畫；Once 在被看到的那一幀畫、下一幀起 Disabled（`world_render` 改的）。
- **U3**（cpu／gpu）：`FirstUnwrittenRead`：這一幀前面畫過或沿用的不算；之後才畫的算；沿用的 target 取樣到上一幀的畫面（`[gf-reused-sampled]`，Lavapipe）；前一幀畫過、這一幀沒畫也沒沿用就取樣 → 斷言（`sdl_gpu.unwritten`，子行程）。
- 既有測試的改動（只改輸入，性質不變）：A1 的 `render.data` 一案的要求清單多一個大小要求；`sdl_gpu.targets` 的 `TargetCase::Frame` 照 `Capture` 的大小呼叫 `SetMainSize`（跟主目標的 target 由資料側定大小）；A2 自己的 `gf-kept-sampled` 改成先沿用再取樣（`gf-reused-sampled`）、死亡測試改成「前一幀畫過、這一幀沒畫也沒沿用」。

**突變**：`phase_mutations.json` 新增 10 個（U1～U3，id 見 dev_log）、退役 1 個（`rd-unwritten-drawn-ignored`：判斷只看資料、沒有 drawn 旗標了）；A1 的 `gt-target-not-resized`、`gt-follows-other-as-main` 改寫到資料側（同一個性質）。

**RC**：
- **RC-A2-5**：視窗大小比 swapchain 晚一幀成為可見的問題 → 後端把這一幀的 swapchain 大小回報給資料側（下一幀生效），或 `window_camera` 改在取 swapchain 之後寫。
- **RC-A2-6**：要「每 N 幀更新一次」這種模式 → `TargetUpdate` 加值（加法；作者現在用 Once 自己排）。

### 14.12 第 14.11 節的修訂：設計審查與純淨 Code Review 之後（2026-11-27）

審查兩份（`build/evidence/v8-arch-gyo2-render-a2/design-review-rp3.zh-Hant.md`、`pure-code-review-rp3.zh-Hant.md`，不在 git）。使用者的三點與那一句不變；改的是做法（逐項在 dev_log）：

1. **沿用是 target 的事，模式是相機的**（設計 S-1 的 (i)、純淨 High 2）：每張 target 一個判斷——它的相機這一幀**都不畫**（Disabled 且 target 有畫面、inactive、投影或視點無效），而且它有畫面 → 這一幀沿用它；只要有一台要畫 → 它的相機全部照常畫（等待中的也畫）。沒有畫面又沒人畫 → 不沿用，取樣它照使用者那一句斷言。**和 Godot 的衝突點**：Godot 的模式屬於 viewport（＝target）、一個 viewport 一台相機（`scene/main/viewport.h:916-922`）；GYO 照使用者把模式放在相機上（Bevy 的多台相機共用一個目標），所以判斷放在 target。膠水：`camera_render` 的一個「每張 target 有沒有人畫」的表。
2. **明示沿用是 `RenderCommands::retained`**（純淨 Medium 3）：不再是 `RenderPass` 的一種；`PassWrites`、`TargetOf`、後端的幾何與目標解析都沒有沿用的特例了。仍是 `RenderCommands` 裡寫明的一句（使用者要的「明示」）。`camera_render` 的沿用經 `ExtractedCameras::retained`、由合成放進去；作者自己的 System 直接放。
3. **沿用一張沒有畫面的 target 不算寫過**（設計 S-2）：`FirstUnwrittenRead(commands, textures)` 只把有畫面的 retained 當作寫過。
4. **B-1：深度 target 的大小只在資料側比**：`FirstDepthOfAnotherSize(commands, textures)`（資料側的大小，主目標用 `RenderTextures::MainSize`）由後端在畫之前斷言；GPU 上主目標是視窗的畫面，可能比資料側晚一幀，大小不同時後端**跳過那個 pass 的 draw、計數**，不斷言（Bevy 不會遇到：它的深度接在照抽出的大小建的中間圖上，`bevy_core_pipeline/src/core_3d/mod.rs:667,695,709`）。
5. **大小的要求送到套用為止**（設計 S-3 的一半、純淨 Medium 6）：`RenderTextures` 記住後端套用過的大小（收據的大小，`Apply`；第 14.13 節），沒套用的每幀送一次（level-triggered，同 A1 的 `EnsureTargets` 能自己回復）。
6. **「有畫面」跟著一份幀的紀錄**（設計 S-3、純淨 High 1）：`RenderTextures::Apply` 是畫面與大小的唯一入口。這一節寫成時紀錄由資料側照這一幀的資料做（`AssumedReport`），因為後端寫回渲染 World 會改 SDL-5；使用者選 (a) 之後換成後端的收據 `RenderFrameReceipt`（第 14.13 節），`AssumedReport` 拿掉。
7. **RP-3 照第 4.3 節原文「後端斷言」**：判斷是 render 的純函式；後端（`sdl_gpu` 的 Plugin，`RenderContracts.hpp` 的 `RequireRenderContracts`）在畫之前斷言。純淨 Medium 5「改在 render 側檢查」不採用：使用者保留第 4.3 節的原文。
8. **Once 的前提**（設計 S-4、純淨 Medium 4）：兩步做法（Last 記下、下一個 First 改成 Disabled）的前提是「渲染 World 對來源的每一輪恰好抽一次，抽在來源的 Last 與下一個 First 之間」——預設的 `RunFrame`（每個 World 依序跑一輪，`Product.cpp` 的 `RunFrameSteps`）成立，兩個 World 誰先跑都成立。來源被借出（`Lend`，O-13）時由持有者自己跑，來源的輪數和渲染 World 的抽取不一定一比一，Once 可能沒畫就變成 Disabled〔未確認：借出的 World 能不能被抽取〕（RC-A2-8）。比過的另一個做法「渲染 World 看模式的變化、不寫回作者的元件」沒選：要在渲染 World 存每台相機的跨幀狀態，那是 RD-6／render 頁第 7.3 節與 RC-RD 5 留給 D3 的事。**Godot 的對照更正**：`renderer_viewport.cpp:982-983` 把 `UPDATE_ONCE` 改成 `UPDATE_DISABLED` 的是**渲染伺服器那一份**狀態，場景端的 `SubViewport::update_mode` 不變（`scene/main/viewport.cpp:5739-5747`）；GYO 改的是作者的元件（來源 World 的 `TargetUpdate`），因為渲染側不能寫回（RP-2；Kernel 的 Extract 只有 `Read<T>`）。膠水：`SeenOnce`＋兩個 System。Godot 的預設是 `UPDATE_WHEN_VISIBLE`（`viewport.h:919`），GYO 預設 Always（同 Bevy）。

**新 RC**：
- **RC-A2-7**：〔已解，第 14.13 節〕SDL-5 的決定之後，後端回報這一幀實際畫的 target 與套用的大小，換掉 `AssumedReport`。
- **RC-A2-8**：來源被借出、或來源與渲染 World 的輪數不一比一 → Once 改成渲染側看模式的變化（要渲染 World 存相機的跨幀狀態，和 RC-RD 5 一起解）。

### 14.13 幀的收據 `RenderFrameReceipt`（SDL-5 (a)，2026-11-27）

**使用者的決定**（HANDOFF `v8-arch-gyo2-sdl5-render-frame-receipt`，經主會話）：「其實（a）的解法不錯，那個 Resource 就是 cache 或者 buffer 一類的東西。」「那叫receipt 好了。」「叫 FrameReceiptBuffer 把，然後用註釋寫明功能。如何？」，最後「RenderFrameReceipt 好了，選 （a）」。比過的 (b)、(c) 沒選：收據是唯一的真相、修在根源；寫回只在渲染 World 之內，模擬 World 不動，K.3-6 不受影響。`FrameReceiptBuffer` 沒選：render 裡的 buffer 多指 GPU 的緩衝，同組的名字是「Render＋它是什麼」（`RenderCommands`、`RenderTextures`）。

1. **SDL-5 改了**（sdl 頁）：後端可以寫 render 定義的一個 Resource，只記「這一幀實際做成的事」。`GPUSubmit` 多一個 `ResMut<RenderFrameReceipt>`，其餘照舊只讀。
2. **收據的功能**（`RenderTextures.hpp` 的註解寫同一句）：後端每幀寫、資料側讀完就清的緩衝，記這一幀實際畫了的 render target 與實際套用的大小，一個 target 一筆（同一個 target 畫兩次記一次；同一幀改兩次大小記後一個）。
3. **同一幀讀**：後端在 Submit 寫，`render_world` 的 Cleanup（同一輪、Submit 之後）`Apply` 再清空。這對正確性有關係：下一幀的抽取（`ExtractCameras` 看 `HasPicture`）要看到這一幀的結果；若到下一幀才讀，下一幀的判斷會用上上一幀的畫面。
4. **後端什麼時候記**（`GPUDevice.cpp`）：建好 target 的貼圖時記大小（SDL 拒絕的不記：資料側下一幀再要求，直到套用；後端每次都再試、每次都計 `refusedAsks`）；pass 畫過的 target 在 command buffer 交給 GPU 之後才記（交不出去時後端自己那份「有畫面」也拿掉，兩份一致）。上傳失敗的那一幀不錄任何 pass（既有的做法），所以沒有畫過的；拿不到 command buffer 時什麼都沒套用、什麼都不記。
5. **從外面驗證**：三種失敗在後端那一側都引發不了，不加 Plugin 的測試專用鉤子就做不到（只為測試開的入口也是產品的一部分，不加）。(1)「SDL 拒絕大小」：試過比裝置最大的 2D 貼圖還寬的 target——Lavapipe 報 `maxImageDimension2D` 16384，65536×1 照樣建成（`refusedAsks` 0、沒有錯誤訊息，Linux 主機 `4860574e`）；SDL 的驗證只對 cube 貼圖查 2D 的大小，而且整段只在 debug mode 跑（`SDL_gpu.c` 的 `SDL_CreateGPUTexture`：`if (device->debug_mode)` 之內，`MAX_2D_DIMENSION` 只用在 cube 與 cube array）；超過上限是 Vulkan 的未定義行為，靠記憶體不夠來拒絕要真的吃掉記憶體。(2)「上傳失敗」只在一幀超過 4 GiB、或 SDL 映射不了傳輸緩衝時發生；(3)「交 command buffer 中途失敗」只在驅動拒絕時發生。這三種收據的資料側反應用 headless 的替身測（`[rd-receipt-failed-*]`、`[rd-size-asked-until-applied]`、`[re-receipt-*]`），後端那一側只有審查。
6. **沒有後端就沒有畫面**：只有 `render_world` 的產品（工具、headless 測試）沒人寫收據，target 永遠沒有畫面、大小每幀再要求；不更新的相機每幀都畫。這是「沒畫成就沒有畫面」本身，不是例外。headless 的測試用一個「全部做成」的替身寫收據（`tests/common/render/Receipts.hpp`）。
7. **沒有需要例外的地方**。留著的已知缺口（14.11 之前就有，設計審查 Note-1）：拿不到 command buffer 的那一幀，這一幀的建立要求（`TextureCreate`、`TextureTargetCreate`）被 Cleanup 清掉，後端不知道那個 target；之後它的大小要求每幀被拒、計數〔target 的部分由第 14.14 節第 4 點修掉〕。收據只管「做成的事」，沒有記「要求有沒有被收下」；要收下的要求也進收據是另一個改動，不在這一批（RC-A2-9）。

**新 RC**：
- **RC-A2-9**：拿不到 command buffer、或整幀沒套用要求的情形在實機出現 → 收據加上「收下的要求」，沒收下的建立要求留到下一幀（資料側的要求變成送到收下為止）。

### 14.14 第 14.13 節的修訂：設計審查與純淨 Code Review 之後（2026-11-28）

審查兩份（`build/evidence/v8-arch-gyo2-render-a2/design-review-receipt.zh-Hant.md`、`pure-code-review-receipt.zh-Hant.md`，不在 git）。使用者的 RP-3 一句「讀的 target 必須這一幀寫過或明示沿用，兩者皆非就斷言」與 SDL-5 (a) 不變；改的是機制（逐項在 dev_log）：

1. **要了新的大小，舊的套用就不算數**（設計 B-1）：後端照新大小重建前先放掉舊貼圖（被拒時就沒有貼圖），所以資料側在大小變了的那一刻拿掉 `applied`（同 Bevy 有新版本時先拿掉舊的，`bevy_render/src/render_asset.rs:449-452`）。否則「拉大被拒、拉回原來的大小」時資料側以為舊的還在、不再要求。
2. **後端的畫面也是「交給 GPU 了才算」**（設計 S-1、純淨 M1）：pass 畫過的 target 先記在這一幀的帳上（`FrameDraws`，`plugins/sdl_gpu/src/FrameDraws.hpp`，不含 SDL），交出去之後才在後端那一份與收據記成有畫面；交不出去時兩份都不動（沒交出去，貼圖上還是交出去前的內容）。第 14.13 節第 4 點的「兩份一致」從此在所有情形成立。
3. **程式的事實與 GPU 的事實分開**（純淨 H1；AGENTS 第 1 節第 9 條：Assert 只給程式的誤り）。比過的兩個方向：
   - (i)「沒有確認過畫面就強制重畫」：不更新的相機本來就這樣（沒有畫面就畫）；但 inactive 的相機（作者關掉的）不能替它畫，作者自己的 System 放的沿用也沒有相機可以重畫——只用 (i) 解不了，GPU 的失敗仍會在之後的幀變成斷言。
   - (ii) **採用**：RP-3 判的是 `RenderCommands` 對資料側紀錄的錯。資料側在一幀結束時記下這一幀的 pass 畫了哪些 target（`RenderTextures::WasDrawn`：程式畫過它、在它現在的大小），沿用一張程式畫過的 target 就算寫過；**GPU 有沒有真的拿到那一幀**是後端的實況：後端那裡沒有畫面（這一幀也沒畫）的 target 被取樣時，跳過那個 draw、計 `skippedDraws`（同 B-1 的前例「深度大小不同 → 跳過、計數」，也同「取樣沒有 GPU 貼圖的 → 跳過、計數」）。收據的畫面（`HasPicture`）只用來決定「能畫的相機要不要重畫」：不更新的相機的 target 沒有確認過的畫面就再畫（(i) 留在它原本的位置）。
   - inactive 的相機：它的 target 只要程式畫過就被沿用（`camera_render` 的沿用看 `WasDrawn`），GPU 沒拿到那一幀時取樣它的 draw 被跳過、計數；作者再把相機打開，它就畫。不需要例外：每張 target 的判斷仍是「它的相機這一幀都不畫 → 沿用」，「畫過」用程式的事實、「要不要重畫」用 GPU 的事實。
4. **大小沒套用的 target 整個再要求**（純淨 M2 的一部分）：資料側對「要了大小、收據沒有」的 target，下一幀先送它的建立（`TextureTargetCreate`）再送大小；後端對已經有的 target 再建立什麼都不做（id 不重給）。拿不到 command buffer 而丟掉的建立就這樣補回來，SDL-5 的「沒套用就每幀再要求，直到套用」對 target 成立。和既有的「大小送到套用為止」是同一個機制，只是把建立一起送。`TextureCreate`／`TextureUpdate`／mesh 的要求不在這一批改（仍是送出就清，拿不到 command buffer 的那一幀會丟；RC-A2-9）。收據加上「拒絕＋原因」、作者自選重試策略是擴大範圍，不做（Note：`RenderFrameReceipt` 只記做成的事，「沒記」同時是沒有後端、這一幀失敗、SDL 拒絕三種；SDL 拒絕的大小每幀再試、每幀計數）。
5. **一幀結束只有一個入口**（純淨 M4）：`RenderTextures::EndFrame(commands, receipt)`（同 `RenderImages::EndFrame`、`RenderModelMeshes::EndFrame`）：記下程式畫了什麼、讀收據、清掉這一幀的要求、排下一幀的要求，順序在裡面；`Apply`、`ClearPending` 變成私有。`render_world` 的 Cleanup 與所有測試都走它；收據由 Cleanup 在之後清空。
6. **headless 的後端替身共用**（純淨 M3）：`tests/common/render/Receipts.hpp` 的 `AllDoneBackend`（掛在 Submit stage、寫「全部做成」的收據的 System，`AddAllDoneBackend` 掛上）；要畫成的 headless 測試明確選用它。`WorldUiTests`、object_connect 的 `RuntimeTests`、object_fps_pvp 的 `ClientTests` 在 Submit 掛的替身沒有寫收據：它們不建 render target（`CreateTarget`、`RenderTargets`、`TargetUpdate` 都沒有用到），收據只管 target，所以不受影響，不改。
7. **命名**（設計 N-3、純淨 L2）：收據的寫入改成動詞 `RecordDrawn`／`RecordApplied`。

**新 RC**：
- **RC-A2-10**：作者要對被拒的大小降級、放棄或退避 → 收據加上「拒絕＋原因」，重試策略交作者選（和 RC-A2-9 一起）。


## 15. A3 小宣告：GPU 除錯與計時（`v8-arch-gyo2-render-a3`）

2026-11-28。主會話在 A2 之後交 A3（與 A4 並行，各在自己的工作樹）。Opus／high（下段：除錯資訊；**不改資料形式**——`RenderCommands`、`CameraPasses` 的形狀不動）。小宣告寫完直接實作（主會話的指示：不等核准）。

讀的版本：本分支 `859258d6`（A2 與審查修正，PR #370 的 head）；Bevy `6a911653f`；Godot `94fc09c`；SDL 3.4.0（建置抓下來的 `_deps/sdl3-src`）。

**使用者的決定**（第 12 節 Q1 (a)，原話「全部照推薦」）：每個 pass 加除錯標記（RenderDoc／Xcode／PIX 擷取時看每個 pass 的 GPU 時間）＋Engine 報每個 pass 的 CPU 時間與整幀的 GPU 時間（fence）；每個 pass 的 GPU 時間等 SDL 加 timestamp query（RC-RP5）。不選：(b) adapter 寫三份原生 timestamp；(c) 每個 pass 各等一個 fence（串行化）。

### 15.1 現況（程式，A3 前）與查到的事

- **沒有任何計時與標記**：`sdl_gpu` 的 adapter 每個 pass 錄一個（或幾個）render pass（`GPUDevice.cpp` 的 `RecordPasses`），不呼叫 `SDL_PushGPUDebugGroup`／`SDL_InsertGPUDebugLabel`。整幀的時間只有 `sdl_gpu.budget` 從外面量 `Capture` 的牆上時間（render 頁第 2 節「幀時間的預算」）。`Submit` 用 `SDL_SubmitGPUCommandBuffer`（沒有 fence）；`Capture` 用 `SDL_SubmitGPUCommandBufferAndAcquireFence` 並等它（讀回要等）。
- **pass 沒有名字**：`RenderPass` 是 `variant<Pass2D, Pass3D, PassFullscreen, PassDepth>`（`RenderCommands.hpp:248`），沒有標籤欄位；`CameraPasses` 的 phase 與相機在 `ComposePasses` 之後就攤平成一個列表。
- **〔源〕SDL 3.4.0**：
  - 標記：`SDL_InsertGPUDebugLabel`、`SDL_PushGPUDebugGroup`、`SDL_PopGPUDebugGroup`（`SDL_gpu.h:2959,2989,3007`），掛在 command buffer 上；在 render pass 之中 push 的群組在 Metal 會變成那個原生 pass 的群組，要在同一個 pass 裡 pop（`:2977-2980`）。各後端：Vulkan 只在 loader 有 `VK_EXT_debug_utils` 時呼叫 `vkCmdBeginDebugUtilsLabelEXT`，沒有就什麼都不做（`src/gpu/vulkan/SDL_gpu_vulkan.c:5577-5605`；那個擴充是「optional」，`:11127`；RenderDoc 的層會提供它）；**D3D12 要 `WinPixEventRuntime.dll`**：建裝置時 `SDL_LoadObject`，沒有就印一次警告、之後 push／pop 什麼都不做（`src/gpu/d3d12/SDL_gpu_d3d12.c:94,2165-2201,8862-8881`）——**不加這個依賴**：放不放那個 DLL 是發版與開發者機器的事，沒有它畫面照常、只是 PIX 裡沒有群組名；Metal 在沒有 encoder 時 push 到 command buffer 本身（`src/gpu/metal/SDL_gpu_metal.m:1315-1349`）。在 debug mode 下只多一個「command buffer 還沒送出」的檢查（`src/gpu/SDL_gpu.c:1466-1523`）。
  - fence：`SDL_SubmitGPUCommandBufferAndAcquireFence`、`SDL_WaitForGPUFences`、`SDL_QueryGPUFence`（不阻塞）、`SDL_ReleaseGPUFence`（`SDL_gpu.h:4356,4414,4431,4447`）；Vulkan 的 fence 就是那個 command buffer 的 `inFlightFence`，送出的路徑和不拿 fence 的一樣（`SDL_gpu_vulkan.c:10574-10583`），查詢是 `vkGetFenceStatus`（`:9596-9614`）。fence 只說「做完了沒」，**不帶時刻**。一次可以有幾幀在 GPU 上：預設 2（`SDL_gpu.h:4142`）。
  - **沒有 timestamp query**（第 4.7 節）。
- **〔源〕Bevy**：`RenderDiagnosticsPlugin`（`bevy_render/src/diagnostic/mod.rs:64`）。pass 的名字是寫 pass 的程式自己給的常數：`main_opaque_pass_3d` 同時是 wgpu render pass 的 `label`（擷取工具裡看到的名字）和計時的 span（`bevy_core_pipeline/src/core_3d/main_opaque_pass_3d_node.rs:68,75,112`）。span 的 CPU 時間是開始與結束時的 `Instant::now()`（`diagnostic/internal.rs:459-482`），GPU 時間是 span 兩端寫的 timestamp（`:573-640`）；沒有 timestamp 的平台（Metal、WebGPU）**只記 CPU 時間**（`mod.rs:60-62`）。結果是**幾幀之後**才拿得到：送出的幀排在 `submitted_frames`，每幀開頭不阻塞地看哪些已經讀得回來（`internal.rs:88-101`）。路徑是 `render/<span>/elapsed_cpu`、`elapsed_gpu`（`:515-521`），進 `DiagnosticsStore`（`mod.rs:727`）。
- **〔源〕Godot**：`capture_timestamp(name)` 同時記 CPU 的 tick 與 GPU 的 timestamp（`servers/rendering/rendering_device.cpp:8971-8984`）；結果在那一格 frame 再被用到、先等它的 fence 之後才讀（`_begin_frame`，`:8305-8308,8345-8351`），所以晚 `get_frame_delay()` 幀（`rendering_device.h:1931-1935,1946`）。除錯標記是 `draw_command_begin_label`／`end_label`（`:1963-1965`）。viewport 的 `measure_render_time` 用兩個 timestamp 給整個 viewport 的 CPU 與 GPU 時間（`servers/rendering/rendering_server.h:627-629`；`renderer_viewport.cpp:342-345,1609,1616`）。

### 15.2 前提：誰要解決

| 項 | 前提 | 理由 |
|---|---|---|
| 擷取工具裡每個 pass 有名字 | **〔Engine〕** | GPU 命令只在 `sdl_gpu` 的 adapter 錄；作者伸不進去（RD-1） |
| 每個 pass 錄製花的 CPU 時間 | **〔Engine〕** | 同上：錄製在 adapter 裡 |
| 整幀的 GPU 時間 | **〔Engine〕** | fence 在 adapter 裡；作者拿不到 command buffer |
| 拿這些數字做什麼（平均、上限、畫在畫面上、降畫質） | **〔作者〕** | 只告訴外面發生了什麼（`errors-inform-not-prescribe`）；Engine 不平滑、不判預算 |
| pass 的語意名字（「太陽的 cascade 2」） | **〔作者〕**，但資料裡沒有地方放 | 不改資料形式（A3 是下段）；名字從 pass 在資料裡的位置推（第 15.3 節第 1 點），語意名字是 RC-A3-1 |

### 15.3 設計（卡片判）

1. **pass 的名字從資料推出、和 `RenderCommands::passes` 一一對應**：`PassLabel(pass, index)`（`plugins/sdl_gpu/src/PassLabels.hpp`，不含 SDL）＝「`#<index> <種類>[ <shader>] -> main`」或「`-> target <id>`」。種類是 variant 的型別名（`Pass2D`、`Pass3D`、`PassFullscreen`、`PassDepth`），全畫面 pass 多帶 shader 的名字（第 14.3 節第 6 點：它就是 shader 的識別）。index 讓名字唯一、而且就是「第幾個 pass」，所以「名字對到哪個 pass」不用另外查表。例：`#0 PassDepth -> target 3`、`#1 Pass3D -> main`、`#2 PassFullscreen test_invert.frag -> target 5`。
2. **除錯群組**：adapter 錄每個有目標的 pass 時，在 render pass 之外 `SDL_PushGPUDebugGroup(cmd, 名字)`、錄完 `SDL_PopGPUDebugGroup`（一個 pass 可能錄成幾個 render pass——例如沒有深度可用時的空 pass——都在同一個群組裡；在 pass 之外 push／pop，所以 Metal 上是 command buffer 的群組，`SDL_gpu.h:2977-2980`）。每幀計數（`FrameTiming::debugGroups`），測試只驗次數（擷取工具裡的樣子 CI 看不到）。copy pass（上傳）與「沒有 pass 時清主目標」那一個不加群組（不是 `RenderCommands` 的 pass）。**一律開**：不看 debug mode（Bevy 的 pass label 也一律給；沒有擷取工具時 Vulkan 不做事、D3D12 沒有 DLL 時不做事）。
3. **每個 pass 的 CPU 時間**：adapter 錄那個 pass 前後的 `std::chrono::steady_clock`（Bevy 的 `Instant::now()`，`internal.rs:459-482`）——是**錄製**（編碼命令）的時間，不含產生那個 pass 的 System（那是 Kernel 的事）。沒有目標的 pass（沒有對應、或這一幀上傳失敗、什麼都不錄）`recorded == false`、時間 0。
4. **整幀的 GPU 時間（fence）**：每一幀（`Submit` 與 `Capture`）都用 `SDL_SubmitGPUCommandBufferAndAcquireFence` 送出，記下送出的時刻；之後**不阻塞**地查（`SDL_QueryGPUFence`）：`Submit` 的開頭、拿到 swapchain 之後、`Capture` 等完 fence 之後。看到 signaled 時記「送出到看到做完」的時間，放掉 fence。**這是上界**：fence 沒有時刻，所以它含排在前面的幀的 GPU 工作與「多久之後才去看」。`Capture` 本來就等它的 fence（讀回要等），所以那一幀的值是「送出到等完」；畫到視窗的幀是下一次 `Submit` 才看，粒度是一幀（GPU 跟得上時 ≈ 幀間隔；GPU 落後時是兩、三幀）。**不串行化 GPU**（(c) 被拒）：pass 之間不插 fence，`Submit` 也不等自己的 fence（等了會讓 CPU 和 GPU 不能重疊，那是另一種串行化）。每個 pass 的 GPU 時間要 timestamp：RC-RP5；那之前看擷取工具（第 2 點的群組名）。
5. **報到哪裡**：`SDLGPUStatus`（Product 的 buffer，同其他計數）多兩個：`LastFrameTiming()`（最近錄的一幀：`frame` 號碼、每個 pass 的 `PassTiming{name, recorded, cpu}`、`debugGroups`）與 `LastGPUFrameTime()`（`optional<GPUFrameTime{frame, atMost}>`：最近看到做完的一幀與它的上界；沒有看到過時沒有值）。幀的號碼是裝置錄過的幀數（`Submit` 與 `Capture` 都算，從 1 起），兩者用它對得上。只印、不判（同 `sdl_gpu.budget`）。**不寫進渲染 World**：SDL-5 只讓後端寫收據（使用者 2026-11-27 的決定），計時不是「這一幀做成的事」，不為它改 SDL-5。adapter 的介面（`src/GPUDevice.hpp`）多兩個同名的 getter；`Submit`／`Capture` 的簽名不變（A4 同時在改測試的呼叫）。

**偏離 Bevy 的地方、理由與膠水**：

| Bevy | gyo2 | 理由 | 膠水 |
|---|---|---|---|
| pass 的名字是寫 pass 的程式給的常數（`main_opaque_pass_3d_node.rs:68,75`） | 從 pass 在 `RenderCommands` 的位置、種類、目標推出 | GYO 的 pass 是資料（RD-1），錄的是 `sdl_gpu`；A3 不改資料形式，資料裡沒有名字 | 一個純函式；語意名字 RC-A3-1 |
| 每個 span 的 GPU 時間（timestamp，Vulkan／DX12），Metal 只有 CPU | 每個 pass 只有 CPU 時間；整幀一個 GPU 時間的上界（fence） | SDL_GPU 3.4 沒有 timestamp（使用者 Q1 (a)） | fence 的帳（一個小列表），RC-RP5 換掉 |
| 結果進 `DiagnosticsStore`（有歷史、平滑） | 只有最近一幀，在 `SDLGPUStatus` | 平均、歷史是作者的事（第 15.2 節）；GYO 沒有共通的 diagnostics 設施 | 無 |
| 結果幾幀之後到（`internal.rs:88-101`） | 同（`Submit` 的 GPU 時間下一幀才知道） | 同一個理由：不等 GPU | 無 |
| 選用：`RenderDiagnosticsPlugin` 要自己加（`mod.rs:60-64`）；Godot 的 `measure_render_time` 要 viewport 開（`rendering_server.h:627-629`） | 一律開（修訂 15.8，設計審查 N5） | 成本是每個 pass 讀兩次時鐘、寫一次名字（緩衝重用）、推彈一個群組，每幀一個 fence；沒有擷取工具時群組不做事；AGENTS §1.10 既定值方便。開關等量到成本再加（RC-A3-3） | 無 |

**別家對照**：Godot 的 `capture_timestamp` 是作者指名的點（不是每個 pass），結果晚 `get_frame_delay()` 幀；viewport 的 `measure_render_time` 是整個 viewport 一個值——和這裡的「整幀一個值」同一個粒度，只是它有 timestamp。

### 15.4 判定（合併前）

- **T1**（cpu，`sdl_gpu.passlabels`）：四種 pass 的名字帶 index、種類、目標（主目標或 target id）、全畫面 pass 的 shader；同一幀裡的名字兩兩不同；`TimingOf` 每個 pass 一格、順序同 `passes`、名字是 `PassLabel`、都還沒錄。
- **T2**（gpu，`sdl_gpu.timing`）：`Capture` 一幀（主目標上的 2D pass、一個沒有對應的 pass、主目標上的 3D pass）之後，`LastFrameTiming()` 每個 pass 一格、名字＝`PassLabel(passes[i], i)`；有目標的 `recorded` 且 CPU 時間 > 0，沒有對應的沒錄、時間 0；`debugGroups`＝錄的 pass 數；第二次 `Capture` 的 `frame` 多 1。
- **T3**（gpu，`sdl_gpu.timing`）：`Capture` 之後 `LastGPUFrameTime()` 有值、`frame` 是同一幀、`atMost` > 0（只印數值）。
- **T4**（gpu，`sdl_gpu.frames`，會開視窗：L1 跑，這張卡不跑）：`SDLGPUStatus::LastFrameTiming()` 的格數等於最後一幀的 pass 數；跑了幾幀之後 `LastGPUFrameTime()` 有值、不晚於最後一幀。
- **T5**：既有測試照過（`sdl_gpu.*`、`render.*`）；第 15.5 節的突變全部 killed（會開視窗的除外，寫明）。

**既有測試的改動**：沒有改判斷；`sdl_gpu.frames` 只加 T4 的檢查。

**和第 5.1 節驗收的對應**（修訂 15.8，設計審查 S4）：第 5.1 節寫「cpu：每個 pass 的名字與 CPU 時間出現在 `SDLGPUStatus`」。名字在 cpu 驗（T1，純函式）；**CPU 時間要有裝置才量得到（錄製的時間），所以移到 gpu 驗**（T2，經 `GPUDevice`）；`SDLGPUStatus` 的轉送只在會開視窗的 T4（`sdl_gpu.frames`，L1 的 Linux 在 Xvfb＋Lavapipe 跑 `-L gpu`；這張卡沒跑）。

### 15.5 測試與突變

- 測試：新 `plugins/sdl_gpu/tests/TimingTests.cpp`：`sdl_gpu pass labels:`（ctest `sdl_gpu.passlabels`，label `cpu`）、`sdl_gpu timing:`（ctest `sdl_gpu.timing`，label `gpu`，`-s` 印出每個 pass 的 CPU 時間與整幀的上界）。
- 突變：新 `tests/common/render/gpu_debug_mutations.json`（T1～T4 各個獨立可錯的判斷；id 與數量見 dev_log，沒加的與理由在檔案的 `description`）。這一批不改 Design Invariants 的條文（下段），所以「條文數×5」是 0；每個突變的 `guards` 指本節的判定，`what` 寫它抓到、別的抓不到的錯（README 基準 3）。
- 既有突變（D184）：find 落在本批改過的程式裡的（`GPUDevice.cpp` 的 `RecordPasses`、`DrawFrame`、`Submit`、`Capture`、`~State`；`SDLGPUPlugin.cpp` 的 `SDLGPUStatus`）重跑；被改過的測試殺的（`sdl_gpu.frames` 的 `gp-frames-*`，會開視窗）不跑、寫明。

### 15.6 RC

- **RC-A3-1**：作者要擷取工具裡的語意名字（「cascade 2」「bloom 降採樣 3」），而位置推的名字不夠用 → pass 加一個可選的標籤（資料形式的改動，上段；Bevy 的 pass label）。`PassLabel` 那時先用標籤。
- **RC-A3-2**：整幀的上界太粗（要分辨 GPU 落後一幀還是兩幀以內的數字，或畫到視窗的幀要精確值）而 SDL 還沒有 timestamp（RC-RP5）→ 另一條執行緒等 fence 並記時刻（SDL-4 要先確認 `SDL_WaitForGPUFences` 能不能在主執行緒以外呼叫〔未確認〕）。
- RC-RP5 照舊：SDL_GPU 加了 timestamp query → 每個 pass 的 GPU 時間，群組的兩端寫 timestamp（Bevy 的 `pass_span`）。

### 15.7 不做

每個 pass 的 GPU 時間（RC-RP5）；adapter 直接呼叫原生 timestamp（Q1 (b)）；每個 pass 一個 fence（Q1 (c)）；歷史、平均、預算判定（作者的事）；`WinPixEventRuntime` 的依賴；draw 層級的標記（`SDL_InsertGPUDebugLabel`：沒有消費者，要時加）；把計時寫進渲染 World（SDL-5）；Object_PvP 的整體幀時間量測（重 GPU 量測，先問使用者）。

### 15.8 第 15 節的修訂：設計審查與純淨 Code Review 之後（2026-11-28）

審查兩份（主 checkout `build/evidence/v8-arch-gyo2-render-a3/design-review-a3.zh-Hant.md`、`pure-code-review-a3.zh-Hant.md`，不在 git）：設計審查（Opus／high）Blocker 0、Should 4、Nit 6、Note 6；純淨 Code Review（Fable／high）高 1、中 7、低 8。逐項的處理與理由在 dev_log。使用者的 Q1 (a) 不變。

**等使用者決定、這一輪不動**（主會話交使用者）：設計 S1＝純淨 H1——畫到視窗的幀，整幀 GPU 時間的上界實際上約等於「多久之後才去看」（GPU 跟得上時≈這一幀剩下的 CPU 時間，落後時≈VSync 間隔的倍數），而型別叫 `GPUFrameTime`、getter 叫 `LastGPUFrameTime`，會被讀成「GPU 花了多久」。Q1 (a) 的前提「fence 給得出整幀的 GPU 時間」只在 `Capture`（等自己的 fence）成立。選項：(i) 照現狀明講；(ii) 照實際意思改名；(iii) 提前 RC-A3-2（另一條執行緒等 fence，和 SDL-4「`GPUDevice` 每個進入點斷言主執行緒」衝突）。和它綁在一起的改名、區間欄位（純淨 H1 的建議 1）都等決定。

改了的：

1. **T3 判「從哪一刻算起」**（設計 S2）：`Capture` 前後量牆上時間 `wall`，判 `0 < atMost ≤ wall`、各 pass 的 CPU 時間和 ≤ `wall`（兩端都在 `Capture` 之內，所以是從外面判的上界，不是寫死的數字）。突變 `tm-gpu-from-epoch`、`tm-cpu-from-epoch`。
2. **種類的名字由 render 給、窮舉**（設計 S3、純淨 M4）：`render` 的 `PassKindName(pass)`（`RenderCommands.hpp`，`TargetOf` 旁，`Overloaded` 的窮舉 visitor：多一個 pass 種類時編譯失敗）；`PassLabel` 只排版。這是加一個純函式，不改資料形式；和 A4 的 `RenderCommands.hpp` 改動不相鄰。
3. **CPU 驗收的對應**（設計 S4）：第 15.4 節補一段。
4. **名字與帳本重用**（純淨 M1 的一半）：`BeginTiming(timing, frame, commands)` 在上一幀的 `FrameTiming` 上寫（`resize`、名字 `assign` 進既有的字串），每幀不重新配置。**開關不加**（理由在 15.3 的偏離表；RC-A3-3）。
5. **時間型別**（純淨 M7 的一半）：`PassTiming::cpu`、`GPUFrameTime::atMost` 改成 `Engine::Time::Duration`，adapter 讀 `Engine::Time::Now()`（engine 的時間基準；`sdl_gpu` 多連 `GYO::Time`）。**時鐘不注入**：T3 的 `wall` 上界從外面就驗得到起點，注入要在 `GPUDevice::Create` 多一個只給測試的接縫，而且精確值會綁住「讀幾次時鐘」。
6. **getter 回值**（純淨 M3、設計 N3）：`LastFrameTiming()` 從參考改成 `std::optional<FrameTiming>`（複本；第一幀之前、沒有裝置時沒有值），和 `LastGPUFrameTime()` 同一種表示「沒有」；`Shutdown` 之後不懸空。
7. **帳本明確傳遞**（設計 N4、純淨 M2、L2、L4）：`BeginFrame`（編號＋`BeginTiming`）與看 fence 移到 `Submit`／`Capture`；`DrawFrame`／`RecordPasses` 收 `FrameTiming&`，`RecordPasses` 斷言「每個 pass 一格」（程式的誤り）；交給 GPU 收成一個函式 `Hand`（取時刻、帶 fence 送出、`Commit`、記帳），`Submit` 與 `Capture` 共用。上傳失敗那條路傳一份空的帳。
8. **看 fence 的迴圈**（純淨 L3）：`erase_if` 的有副作用的述詞改成明寫的迴圈。
9. **註解**（設計 N1、N6、純淨 L1、L6）：`recorded == false` 也包括跳過的幀；拿不到 command buffer 的幀不錄、`LastFrameTiming` 停在前一幀（看 `frame`）；`cpu` 是錄製那段的牆上時間（Bevy 的 `elapsed_cpu` 同義）；查不到的 fence 留到裝置結束。
10. **T4 不要求視窗上一定看到做完的幀**（純淨 M7 的另一半）：視窗上不等 GPU，「到最後一幀為止看到某一幀做完」邏輯上不保證；只在有值時判它是已錄的幀。確定的情況由 `Capture` 的 T3 驗。

**不改的**（理由在 dev_log）：`PassTiming` 改放結構化的事實（純淨 M5：名字就是擷取工具裡的群組名，種類與目標在同一個位置的 `RenderCommands` 裡）；拿掉 `debugGroups`（純淨 M6、設計 N2：第 5.1 節的驗收是「只驗呼叫次數」，它是除錯標記在擷取工具之外唯一看得到的事實；計數是 `DebugGroup` 自己數的，只刪 `SDL_PushGPUDebugGroup` 一行的錯只有擷取工具看得到）；交出之後才編號（純淨 L1：帳本描述的是錄的幀，交不出去的那一幀照樣錄過）；整幀的錄製合計（純淨 L7：第 5.1 節要的是每個 pass）；`inFlight` 的上限（純淨 L8、設計 N6：裝置壞了時送出也失敗，不再加 fence）；診斷關掉時走沒有 fence 的送出（純淨 L5：隨開關）。

**新 RC**：
- **RC-A3-3**：量到診斷（名字、時鐘、群組、fence）成為幀時間的可見部分，或有作者要在發版關掉 → `sdl_gpu` 加一個開關（預設開，AGENTS §1.10），關掉時不推群組、不量、用沒有 fence 的送出。
