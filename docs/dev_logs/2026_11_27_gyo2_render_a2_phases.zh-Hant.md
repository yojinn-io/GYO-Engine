# 2026-11-27 渲染管線線 A2：camera phase 與 pass 種類（`v8-arch-gyo2-render-a2`）

Owner：渲染管線線のカード。分支 `claude/gyo2-render-a`（A1 `219a37f9` 之上）。作者 Opus 5.5／high（子 Agent）。宣告：[渲染管線宣告](../architecture/target/v2/render-pipeline-declaration.zh-Hant.md)第 14 節（A2 小宣告）；render 頁第 2、3、4、8.2 節、RD-4、RD-11、新 RD-12；SDL 頁第 7 節。

## 前提

- 使用者：總宣告（#369）合併後說「A 段開始」（HANDOFF `v8-arch-gyo2-render-a-start`），主會話在 A1 之後交 A2；小宣告寫完直接實作、不等核准（主會話的指示）。
- A2 的範圍（總宣告第 5 節）：相機的 phase、全畫面 pass、只寫深度的 pass、讀寫宣告。上段（公開介面 `CameraPasses`、`RenderPass`；pass 的資料形式）。
- 使用者 Q2：照現在的 Bevy（render graph 換成 schedule）：沒有 graph 物件、不自動重排或剔除、不 aliasing；第 4.3 節照原文。
- 〔Engine〕phase 的先後、不屬於相機的 view、兩種新 pass、深度格式的選擇、讀的宣告與 RP-3 的斷言；〔作者〕後處理用哪個 shader、參數、放哪個 phase、pass 的先後。

## 做了什麼

1. **phase**（`Compose.hpp`、`Pipeline3d.cpp` 的 `ComposePasses`）：`CameraPhase{Prepass, Opaque, Transparent, PostProcess, Overlay}`、`CameraPhases`（五個列表）、`CameraPasses{nonCameraViews, byCamera}`。合成：`nonCameraViews` → 相機順序（清除 → 五個 phase）。`mesh_render`→`Opaque`、`sprite_render`→`Transparent`、`ui_render`→`Overlay`（各一行）。`ExtractCameras` 也清 `nonCameraViews`。
2. **pass 的種類**（`RenderCommands.hpp`、`Render.cpp`）：`PassFullscreen{target, viewport, shader, inputs, parameters}`、`PassDepth{target, viewport, clipFromWorld, clearDepth, 幾何, draws}`、`Pass3D::depth`；`RenderPass` 四種；`TargetOf` 移到 `.cpp`；新 `ReadsOf`；`ViewportOf` 的兩個多載。
3. **深度格式**（`RenderTextures.hpp`）：`TextureFormat::Depth`：只給 target、要 `Nearest`；`TextureDesc` 不收（兩個斷言）。
4. **shader**（`engine/modules/render/shaders/`）：`Fullscreen.hlsli`、`fullscreen.vert.hlsl`（Bevy 的全畫面三角形）、`depth.vert.hlsl`、`depth.frag.hlsl`；`ShaderAbi.hpp` 的對應常數。`plugins/sdl_gpu/CMakeLists.txt` 多嵌三個。
5. **後端**（`GPUDevice.cpp`）：深度格式選「能當 depth target 又能取樣」的第一個；`GPUTarget::isDepth`；`Depth` target 的用途；3D 與只寫深度的 pass 共用 `RecordMeshes`（`Pipeline3DFor` 多一個 `depthOnly`）；`RecordPassFullscreen`（以名字找 shader、pipeline 以（shader、格式）為鍵）；`RequirePictures`（RP-3 的斷言）；`Create` 多 `more`。
6. **shader 工具**（`ShaderTool.cpp`）：`--function <名字>`（預設 `EmbeddedShaders`，要是 C++ 識別字）。
7. **測試的 shader**：`plugins/sdl_gpu/tests/shaders/test_invert.frag.hlsl`、`test_shift.frag.hlsl`，由工具 `--function TestShaders` 編成測試執行檔的表（`TestShaders.hpp`）。
8. **文件**：宣告第 14 節（與第 9 節 RP-3 的一句）；render 頁（第 2、3、4、8.2 節、RD-4、RD-11、新 RD-12）；SDL 頁第 7 節；各 Module 標頭的註解。

## 測試

- 新：
  - `render.data` 二案：`Depth` target（建得出、`Linear` 與 `TextureDesc` 的 `Depth` 是 Programmer Error）；四種 pass 的 `TargetOf`／`ReadsOf`。
  - `render.pipeline3d` 一案：`nonCameraViews` 最先、它在自己的目標上時第一個相機照樣清整個主目標、五個 phase 的順序（反著放進去）、相機的順序、全部清空。
  - `render.extract` 一案：在 `Update` 排在兩個管線 System 之前放的 `PostProcess` pass 合成在 `Pass3D` 與 `Pass2D` 之後；同一台相機的 2D（`Transparent`）在 3D（`Opaque`）之後，雖然它的 System 先跑；`nonCameraViews` 最先。
  - `sdl_gpu.shaders` 一案：`depth.vert`、`depth.frag`、`fullscreen.vert` 的資源數對 ABI。
  - 新 ctest `sdl_gpu.phases`（標籤 `gpu`）三案：兩個全畫面 pass 串接（反色→平移 2 像素）逐像素、下一幀取樣上一幀的畫面；沒有那個 shader 時不畫、計一次；只寫深度的 pass → `Pass3D`（`depth` 指它、保留深度）被擋住與沒擋住的像素 → 2D 取樣深度得 0.25／0.5／1（±1）。
  - 新 ctest `sdl_gpu.unwritten`（標籤 `gpu`）：取樣沒有畫面的 target 時斷言（這一案平常 `skip`，ctest 帶 `--no-skip`；自己的 handler 印成功的標籤並以 0 結束，`PASS_REGULAR_EXPRESSION` 看那個標籤，失敗的標籤是另一個）。
- 改了的既有測試（判斷不變）：`Pipeline3dTests.cpp` 的合成案、`Pass2DTests.cpp` 的 2D 管線案——放 pass 的地方從 `byCamera[i]` 改成 `byCamera[i][CameraPhase::…]`；斷言一行都沒改（`[cp-emptied]` 看同一格）。
- 寫測試時自己的錯（記下來，不是改既有測試）：
  - 只寫深度的 pass 第一次什麼都沒畫：`depth.vert` 的輸入結構只放 position 與 world（`TEXCOORD0`、`2`～`5`），DXC 照宣告順序編 location，world 落到 uv 的 location。改成和 3D 同一個 `Mesh3DVertex`；render 頁第 4 節記下這件事。
  - 深度案原本預期右半是清除的 1：`Pass3D` 在沒被擋住的地方也寫自己的深度 0.5。預期改成三塊（0.25、0.5、1），多驗了「3D pass 寫進那張深度」。

## 驗證（本機 macOS Intel、Metal，preset `test`）

- 建置：依賴照 A1 借另一個工作樹（`gyo2-big-map-s4c`）的 `_deps`。shader 工具：本批改了工具（`--function`），借來的工具沒有它，所以用同一個工作樹主機建置的函式庫與標頭（shadercross、SPIRV-Cross、SDL、`libdxcompiler.dylib`）把本分支的 `ShaderTool.cpp` 手動編、連結成 `build/target/_build/test/a2-shader-tool/gyo_sdl_gpu_shader_tool`，以 `GYO_SHADER_TOOL_EXECUTABLE` 指它（沒有下載）。新的 shader 全部編得過（metallib；另外手動 metallib＋spirv 試編也過）。全量建置成功，無新警告。
- 工具的 `shader` 測試（主機建置裡的 ctest，本機的借用配置裡沒有）照它們的命令手動跑：編 2D pass 回 0、`broken.frag.hlsl` 回 1 且訊息相符、不寫輸出、`glsl` 回 1；`--function 9bad` 回 1。
- `ctest -L 'cpu|shader'`：254 個裡 253 通過。失敗的 `cmake.PluginSelection` 同 A1，是借用依賴的環境問題（`FETCHCONTENT_FULLY_DISCONNECTED` 要求 `yyjson-src`）。
- GPU（Metal，8×8 讀回，是測試、不是量測）：`ctest -L gpu` 11/11（`sdl_gpu.pass2d`、`pass3d`、`frames`、`targets`、`phases`、`unwritten`、`budget`，與 `window_mode.example`、`object_connect.main`、`object_fps_pvp.main`、`object_fps_pvp.main_connect`）。
- **在使用者的畫面上開過視窗**：上面那次 `ctest -L gpu` 包含 `sdl_gpu.frames`、`sdl_gpu.pass3d` 的 `GPUSubmit` 一案（`settings.hidden = false`，顯示視窗）、`window_mode.example`、`object_connect.main`、`object_fps_pvp.main*`——當時沒有先問。之後照主會話的指示：會開視窗、對話框或佔畫面的本機 GPU 執行，先讀測試確認、報告、等許可再跑。

## 突變

- 新 `tests/common/render/phase_mutations.json`：23 個。
  - CPU 的 13 個全部 killed（`cp-*`、`re-phase-*`、`rd-reads-*`、`rd-depth-*`、`sg-phase-depth-uniforms`）。
  - GPU（Metal）跑到一半被停（見下一節的事件）：`gf-parameters-not-pushed`、`gf-unknown-not-counted`、`gd-pass3d-depth-cleared`、`gd-depth-pass-fresh-kept`、`gd-depth-target-colour-usage` killed；`gf-uv-flipped` **存活**、`gd-pass3d-depth-ignored` **wrong_failure**；`gd-depth-target-as-colour`、`gt-unwritten-not-asserted`、`gt-unwritten-always` 沒跑完／沒跑。
  - `gf-uv-flipped` 存活的原因：串接的兩個全畫面 pass 用同一個頂點 shader，uv 錯了會被翻兩次、互相抵消。`sdl_gpu.phases` 第一案加一個「只有一個全畫面 pass、讀 2D pass 畫的 target」的檢查（`[gf-uv-down]`），突變的 expect 改指它。**改了之後還沒在 GPU 上跑**（等許可）。
  - `gd-pass3d-depth-ignored` 的 wrong_failure：測試失敗、輸出沒有對上 `[gd-tested]`；runner 不留輸出，原因未確認，要再跑一次看輸出（等許可）。
  - `gd-depth-target-colour-usage`：Metal 上會走到 SDL 的斷言（下一節），expect 改成「`[gd-made]` 或 SDL 的訊息」。
  - 數量：改的條文 4 條（RD-4、RD-11、新 RD-12、RP-3）×5＝20 < 23；理由在宣告第 14.5 節。之後的結果見「Linux 主機」一節。
- 既有突變（D184）：find 落在改過的程式裡、或被改過的測試殺的，共 66 個（scratchpad 的清單，不進 git）。
  - find 失效的 4 個改寫、守同一個性質（3D 的錄製搬進 `RecordMeshes`）：`g3-depth-not-cleared`、`g3-depth-always-cleared`（`pass.clearDepth` → `clearDepth`）、`g3-texture-ignored`、`g3-texture-missing-not-counted`（貼圖的綁定移進 `if (textured)`）。
  - `cp-clear-after` 的 replace 用了舊的 `byCamera[i].push_back`（build_failed）：改成放進相機的最後一個 phase，守同一個性質（清除在相機的 pass 之後），重跑 killed。
  - CPU 的 14 個：14 killed（含改寫後的 `cp-clear-after`）。
  - GPU 的 52 個：**沒跑**（等許可；其中 `gp-frames-never-shown`、`g3-frames-never-shown`、`gp-present-not-recorded` 的案會開視窗）。

## 事件：SDL 的斷言開了對話框（同日，主會話轉來）

- 使用者看到的：本機跑 `gyo_sdl_gpu_tests -tc=sdl_gpu phases: a depth-only*` 時，SDL 開了模式對話框「Assertion failure at SDL_CreateGPUTexture_REAL (SDL_gpu.c:1274), triggered 2 times: For depth textures: usage cannot contain any flags except for DEPTH_STENCIL_TARGET and SAMPLER」，使用者按了 Abort。
- 查到的：那是突變的執行，不是 A2 的程式。runner 的紀錄是 `gd-depth-target-colour-usage: killed` 之後停在下一個（`gd-depth-target-as-colour`）。前一個突變把 `Depth` target 的用途換成顏色 target 的 `COLOR_TARGET | SAMPLER`：Metal 的 `SDL_GPUTextureSupportsFormat` 不拒（`SDL_gpu_metal.m:4176-4180` 只查反方向），SDL 的驗證（`SDL_gpu.c:1274-1276`，裝置以除錯模式建）斷言，預設 handler 開對話框（`SDL_assert.c:129-220`）。`PID 56035` 是不是同一個突變，從紀錄分不出來（〔未確認〕）。我用 SIGINT 停了 runner（它的 `finally` 還原了原始檔，確認過 find 的原文在）、再結束那個測試行程。
- A2 的程式：深度格式只以 `DEPTH_STENCIL_TARGET | SAMPLER`（`kDepthTargetUsage`）或 `DEPTH_STENCIL_TARGET`（`EnsureDepth`）建，沒有違反那條規則；沒有要修的地方，所以沒有「重現 → 失敗的測試 → 修」的對象（AGENTS §2.5）。要修的是「測試會開需要人的對話框」，做法與檢查在宣告第 14.9 節。
- Linux L1 為什麼抓不到：Vulkan 的 `SDL_GPUTextureSupportsFormat` 照驅動回答（`SDL_gpu_vulkan.c:10992-11001`），D32F＋`COLOR_ATTACHMENT` 會被拒、算一次被拒的要求，不進 SDL 的驗證；突變不在 CI 跑；而且 A2 還沒推、還沒經過 L1。
- 修正（和 A2 同一個 commit：改到的檔案有一半和 A2 相同）：`gyo_sdl_unattended_assertions`（`SDL_SetAssertionHandler`：印出、abort），連進 4 個測試執行檔；跑產品執行檔的 5 個 ctest 帶 `SDL_ASSERT=abort`。第一版想在 `tests/common/support` 用環境變數設 hint，`os.boundary_guard` 擋了平台巨集（R2 `_WIN32`），改成在 Plugin 的測試 adapter 目錄用 SDL 自己的 API。
- 檢查：每個執行檔一案 `[sdl-assert-unattended]`（cpu，四個都過）；`sdl.unattended_assertion`（cpu）強制一次 SDL 斷言，行程以 abort 結束並印出 handler 的字（`Subprocess aborted`，過）；突變 `sdl-unattended-handler-not-installed` killed（第一次 wrong_failure：CMake 把長訊息折行，expect 改成開頭不折的標籤 `[sdl-assert-no-handler-text]`）。

## Linux 主機（Vulkan）上的 GPU 驗證（同日，使用者「測試可以放Linux嗎？」，經主會話）

- 使用者決定剩下的 GPU 項目不在這台 Mac 跑，改在 Linux 主機（192.168.3.20，Ubuntu 24.04）。在自己的目錄 `~/Workspace/render-card-a2/` 從 GitHub clone `claude/gyo2-render-a` 的 `334ecc08`；preset `test`＋`gcc-14`／`g++-14`（同 L1 的 Linux 列），建置自己抓依賴、shader 工具照正常的主機建置從原始碼建（DXC 等），全量建置成功。沒有改那台機器的設定、沒有用 sudo。
- **裝置的選法**：同 L1（`cross-platform.yml:171-177`）：Vulkan loader 只給一個 ICD——`VK_DRIVER_FILES`／`VK_ICD_FILENAMES`＝`/usr/share/vulkan/icd.d/lvp_icd.json`（或 `radeon_icd.json`）；`SDL_VIDEODRIVER=x11`、`xvfb-run`（不開看得到的視窗）。`vulkaninfo --summary` 在同一個環境下確認：`llvmpipe (LLVM 20.1.2, 256 bits)`／`AMD Radeon Graphics (RADV REMBRANDT)`。
- **測試**（不開視窗的：`sdl_gpu.phases`、`sdl_gpu.unwritten`、`sdl_gpu.targets`、`sdl_gpu.pass2d`，與 `sdl_gpu pass3d:` 去掉會開視窗的 `GPUSubmit` 一案）：
  - **Lavapipe**：ctest 4/4、pass3d 11 案 11 過。
  - **RADV**：在 Xvfb 下建不了裝置（`SDL_CreateGPUDevice: No supported SDL_GPU backend found!`；Vulkan 的訊息「No DRI3 support detected - required for presentation」：SDL 的 Vulkan 後端要能呈現，Xvfb 沒有 DRI3）。用桌面的真實 display 會碰到使用者的工作階段，沒有做。所以 RADV 沒跑成；突變全部用 Lavapipe（同 L1）。這次失敗也確認了新的 handler：每個案以 abort 結束、沒有對話框。
  - cpu：`ctest -L 'cpu|shader'` 255/255（Linux 沒有本機的借用依賴問題，`cmake.PluginSelection` 也過）；主機建置的 `sdl_gpu.shader_tool.*`（label `shader`）3/3。
- **突變（Lavapipe）**：
  - 新的 GPU 5 個：`gf-uv-flipped`（加了 `[gf-uv-down]` 之後）killed、`gd-pass3d-depth-ignored`（expect 改成 `[gd-sampled-scene]` 之後）killed、`gt-unwritten-not-asserted` killed、`gt-unwritten-always` killed；`gd-depth-target-as-colour` 第一次 wrong_failure：那個 target 向 SDL 要「沒有格式」，SDL 的除錯檢查斷言（`SDL_gpu.c:927`「Invalid texture format enum」），unattended handler 結束行程（Mac 上這裡也會開對話框）。expect 加上這句，重跑 killed。
  - `gd-pass3d-depth-ignored` 的原因：突變讓場景用目標自己的深度，那張深度沒被寫過又被載入（`clearDepth == false`），Metal 上讀成 0、場景整個不畫，所以左半照樣是藍的、`[gd-tested]` 成立，失敗的是 `[gd-tested-in-front]` 與 `[gd-sampled-scene]`。不管那張深度裡是什麼，深度 target 中間一塊都停在 1（場景沒寫進去），所以 expect 改指 `[gd-sampled-scene]`。
  - D184 的 GPU 49 個（不含會開視窗的 3 個，它們的程式 A2 沒改）：47 killed；`gt-target-not-mapped`、`gt-target-drawn-on-main`（A1 的）wrong_failure：A2 之後，那個 target 沒被畫過就被取樣，RP-3 的斷言先結束了那一案。expect 加上 `(RP-3)`（守同一個性質：那個 pass 沒畫進 target），重跑 killed。
  - 合計：新的 23 個全部 killed（cpu 13、Metal 5、Lavapipe 5）；D184 既有的 cpu 14＋GPU 49 全部 killed；`sdl-unattended-handler-not-installed` killed。
- **已知缺口：Metal 上的行為現在沒有驗證**。這批 GPU 驗證跑在 Linux／Vulkan，不是 Metal：`[gf-uv-down]` 與改過 expect 的突變沒在 Metal 跑；Metal 特有的行為——`SDL_GPUTextureSupportsFormat` 不拒「深度格式＋顏色用途」（`SDL_gpu_metal.m:4176-4180`）、沒寫過又被載入的深度讀成 0——只有這次在 Mac 上看到的紀錄，沒有在 Mac 上重跑確認。macOS 的 L1 是 macos-arm64，只建置、不跑 GPU。

## 沒做／沒跑

- Mac（Metal）上的重跑（上一節的已知缺口）；RADV（Xvfb 下建不了裝置）；會開視窗的 GPU 案與突變（`sdl_gpu.frames`、`GPUSubmit` 的案、`gp-frames-never-shown`、`g3-frames-never-shown`、`gp-present-not-recorded`）。
- Windows（D3D12）上的 `sdl_gpu.phases`、`sdl_gpu.unwritten`——交給 L1（Windows 只建置，GPU 案要有 GPU 的機器）。重 GPU 量測沒做。
- RC-A2-1～4（第 14.6 節）；A3、A4、B1、B2、C1。
