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

- **和 L1 比**（run 45993881896，head `334ecc08`）：linux-x64 的 CPU 255/255、主機建置的 shader 3/3、core 236/236，和 Linux 主機一致；macos-arm64 成功（只建置）。linux-x64 的「Run Linux GPU tests」失敗，但**沒跑到任何 GPU 測試**：開頭的 `xvfb-run … vulkaninfo --summary` 連不上 Xvfb（`vulkan-info.log`：「XCB failed to connect to the X server due to error:1」、`AppCreateXcbSurface failed to establish connection`），`set -euo pipefail` 讓這一步在 `ctest -L gpu` 之前結束（沒有 `gpu-tests.log`）。和 A2 的程式無關；A2 的 GPU 案在 L1 上還沒跑過，要重跑那一列。windows-x64 寫這段時還在跑。

## 設計審查與修正（同日）

兩份審查，報告在主 checkout 的 `build/evidence/v8-arch-gyo2-render-a2/`（不在 git）：
- 每批的設計審查（Opus／high）：Blocker 0、Should 2、Nit 5、Note 6。
- 純淨 Code Review（Fable／high，只看程式）：高 3、中 6、低 8。

修正是另一個 commit（不 amend）。小宣告加第 14.10 節（修訂 1）。**RP-3 的行為不改**：取樣一張建立／重建後還沒畫過的 target 時 (A) 斷言（現在）、(B) 取樣到透明黑／1、不斷言（兩份審查的建議，同 Bevy／wgpu）、(C) 照字面「這一幀沒寫過」就斷言——主會話已交使用者，等回覆；第 14.8 節與 `gt-unwritten-*` 照舊。這次只把檢查改成一處、改起來小（下面 M5）。

### 設計審查

| 項 | 處理 | 一句理由 |
|---|---|---|
| S-1 RP-3 要交 (A)／(B)／(C) | 交使用者（主會話） | 推翻總宣告第 4.3 節原文的選項只有使用者能定；程式先改成決定後只動一處 |
| S-2 `rd-depth-linear-allowed`、`rd-depth-pixels-allowed` 和「誤用不測」矛盾 | 兩個都退役 | M3 之後兩個斷言守的組合寫不出來（型別分開、Depth 不看 filter），沒有東西可守 |
| N-1 CMake 註解指向不存在的檔 | 採用 | 改指 `SDL_ASSERT=abort`、宣告第 14.9 節 |
| N-2 `kCameraPhaseCount` 手動同步 | 採用 | 由最後一個值推出（M2） |
| N-3 `Fullscreen.hlsli` 的 uv 說明 | 採用 | 寫成 viewport 的四角（沒有 viewport 時是整個目標） |
| N-4 `Depth` 要 `Nearest` | 採用 | Depth 不看 `filter`，後端一律最近的 texel（AGENTS §1.10） |
| N-5 `nonCameraViews` 寫成「光的陰影」 | 採用 | 改成點光、聚光；方向光的 cascade 跟著相機（`light.rs:2235,2328`） |
| No-1～No-6 | 記錄 | 接受；No-2（`PassDepth` 重複）由純淨 H3 解，No-5（每幀配 vector）只在有沒有畫面的 target 時才走 |

### 純淨 Code Review

| 項 | 處理 | 一句理由 |
|---|---|---|
| H1 resize 就能觸發斷言 | 交使用者（同 S-1） | 是 RP-3 的 (A)／(B) 之選 |
| H2 全畫面 pass 沒有正式的 shader、名字是字串、`more` 是測試接縫 | 部分 | 形狀照設計審查 (b) 留到 B1 的 `ShaderId`（RC-A2-3），A4 帶第一個內建；修便宜的：`more` 算進格式交集、重名是 Programmer Error（L5）、`parameters` 要 16 bytes 的倍數（L4）、宣告寫明「A4 之前沒有產品可用的全畫面 shader」。拒絕：現在做 shader 登記（那是 B1 的設計）、經 Plugin 選項給 `more`（產品沒有要給的 shader，會變成只為測試開的公開介面） |
| H3 `PassDepth` 複製 `Pass3D` | 採用 | `MeshGeometry`（兩者共用）＋`MeshDraw`（`Draw3D : MeshDraw`）：`PassDepth` 沒有用不到的欄位；後端 `RecordMeshes` 收 `MeshGeometry` 與 `span<const Draw>`（只有 draw 的型別是模板參數：`Draw3D` 綁貼圖，`MeshDraw` 不綁）；「只寫深度」只剩一個說法：沒有顏色輸出（`std::optional<MeshColour>`） |
| M1 寫沒有完整建模 | 採用 | `WritesOf`（顏色＋深度）；`ComposePasses` 的 bug 先寫成失敗的測試（`[cp-written-not-first]`，修正前失敗、修正後通過）再修；`Pass3D` 取樣自己的深度 target 跳過（`[gd-own-depth-skipped]`）；fresh 收成 `BeginWrite` 一個函式 |
| M2 `CameraPhase` | 部分 | 從 0 起、由最後一個值推出數目、列表私有＋`InOrder()`；五個名字與封閉的 enum 不改（設計審查 (c)、No-6：phase 內用 System 排序） |
| M3 一個 enum 兩個子集、Depth 的 filter | 採用 | `TextureFormat` 與 `RenderTargetFormat` 分開；`SDLTextureFormat(Depth)` 的 INVALID 哨兵沒了。A1 的「sRGB target 是 Programmer Error」那一行測試拿掉：那個值寫不出來了（不是為了讓測試過） |
| M4 分派不窮舉 | 採用 | `WritesOf`、`ReadsOf`、`GeometryOf`、`RecordPasses` 都用 overloaded visitor |
| M5 RP-3 夾在錄製中間 | 採用 | `render` 的純函式 `FirstUnwrittenRead`，CPU 測試四案；後端在錄之前呼叫一次（`Submit` 要先拿 swapchain 才知道重建與否，所以在取 command buffer 之後、錄之前）；依據仍是「pass 有提到」（寫明的契約） |
| M6 depth-only pipeline 的鍵含 blend | 採用（沒有失敗的測試） | `MeshDraw` 沒有 blend，鍵是（顏色輸出、depthTest、depthWrite、cull）；看不到 pipeline 的數目、外面量不到，所以不先寫測試 |
| L1 `Create` 的 if 階梯與字面值 | 採用 | 一張（原始檔名 → 成員）的表；名字由 `ShaderAbi::ShaderName(k*Source)` 推出 |
| L2 私有深度也要求可取樣 | 不改 | 一種深度格式讓 pipeline 只有一種深度格式；試過的驅動都拿 D32F 兩用；量到精度問題再分（設計 No-4） |
| L3 名字、兩個清空的主人、`ComposePasses` 的位置 | 採用（平行迴圈不改） | `beforeCameras`；只有 `ComposePasses` 清；搬到 `Compose.cpp`。三個管線的平行索引迴圈各一行，不值得一個抽象 |
| L4 `parameters` 不查大小 | 採用 | 16 bytes 的倍數（Programmer Error）；shader 的 uniform 大小反射裡沒有，只能查到這裡 |
| L5 `FindShader` 重名 | 採用 | 建裝置時斷言 |
| L6 測試 fixture 重複、四個 `TestMain` | 不改 | `Pass2DTests`／`Pass3DTests` 的 helper 各不同，搬動是改既有測試、沒有判斷上的好處；`PhaseTests` 的裝置要帶 `more`。四個「handler 已裝好」是每個執行檔的連結檢查（設計 No-3） |
| L7 `GPUDevice.cpp` 太大、第三份 pipeline cache | 不改 | 拆檔與共用 cache 不在 A2；A3／B1 動到 pipeline 的鍵時一起看 |
| L8 `--function` 的 namespace、關鍵字 | 不改 | 只有測試用它；關鍵字當函式名會編譯失敗，不會靜靜錯 |

### 修正後的驗證

- 本機（macOS Intel，只跑 cpu）：全量建置成功；`ctest -L 'cpu|shader'` 254/255（`cmake.PluginSelection` 是借用依賴的環境問題，同前）；cpu 突變 62/62 killed（phase、target、render 的 `cp-*`／`rd-*`）。
- Linux 主機（`502342bb`，Lavapipe、xvfb、不開視窗；設定同上一節）：`sdl_gpu.phases`、`unwritten`、`targets`、`pass2d` 4/4，pass3d（去掉 `GPUSubmit` 一案）11/11；GPU 突變 61/61 killed（D184 的 49 個＋phase 的 12 個，含改寫的 `gt-unwritten-*`）；`ctest -L 'cpu|shader'` 255/255。
  - 第一次跑 cpu 時 `sdl.build` 失敗：我的腳本把 `SDL_VIDEODRIVER=x11` 匯出給整個執行（GPU 用的），環境變數蓋過那一案自己設的 `dummy` hint（`SDL_SetHint` 回 false）。不帶它重跑 255/255；L1 的 cpu 那一步本來就不帶。和程式無關。
- RP-3 的「明示重用」（使用者的決定，HANDOFF `v8-arch-gyo2-render-a2-target-reuse`）：評估後停下、交主會話——強制重畫要資料側在合成前知道 target 這一幀被重建，現在跟別人大小走的 target 是後端在 `Submit` 時才定大小（`GPUDevice.cpp` 的 `EnsureTargets`），要改成資料側定大小才做得到。第 14.8 節、RP-3 條文與 `gt-unwritten-*` 的語意等核准後一起改。

## RP-3 的機制：資料側定大小、相機的更新模式、明示沿用（同日，使用者的決定）

- 使用者：RP-3 原條文不改、改機制（「我其實想維持原條文，需要改的是機制。……」，HANDOFF `v8-arch-gyo2-render-a2-target-reuse`）；卡片評估後停下報告「重建時強制重畫要資料側先知道大小」，使用者「照建議，主要是解法符合邏輯，而不是補丁。」（HANDOFF `v8-arch-gyo2-render-a2-target-size-data-side`）。條文加的一句是使用者的原話：「讀的 target 必須這一幀寫過或明示沿用，兩者皆非就斷言」。宣告第 14.11 節（小宣告）、第 4.3 節與 RP-3 各加這一句、第 14.8 節記下決定。
- 做了什麼：
  1. `RenderTextures`：`SetMainSize`、每個 target 在資料側解析大小（`ResolveSizes`，照 id 順序），變了發 `TextureTargetSize`；`HasPicture`／`MarkDrawn`；`render_world` 的 Cleanup 呼叫 `KeepPictures`。
  2. `camera_render`：`ExtractRenderTargets` 從來源的 `CameraTargetSizes[kMainTarget]` 給主目標大小；`ExtractCameras` 對 Disabled、target 有畫面的相機放 `PassReuse`、不抽。
  3. `render`：`PassReuse`、`TargetUpdate{Always, Once, Disabled}`；`PassWrites::reused`；`FirstUnwrittenRead(passes, targets)` 只看資料。
  4. `world_render`：登記 `TargetUpdate`；`NoteOnceUpdates`（Last）與 `SettleOnceUpdates`（First）把 Once 在被看到的下一幀改成 Disabled。
  5. `sdl_gpu`：拿掉 `EnsureTargets`（後端不再解析 follow 的大小），照 `TextureTargetSize` 建、重建；`PassReuse` 不錄東西；RP-3 的斷言照資料。
- 寫的時候的一件事：Once 一開始只在 First 改成 Disabled，宿主在兩幀之間設 Once 時，First 在渲染 World 抽到之前就改了，Once 一次也沒畫（`[re-update-once-drawn]` 失敗）。改成兩步：Last 記下（渲染 World 抽到的就是那時的狀態）、下一幀的 First 只改記下的。這是 Once 的定義本身（看到的那一幀畫），不是例外；兩個 World 誰先跑都成立。
- 沒有需要例外的地方：主目標上的 Disabled 相機一定畫，是「沒有畫面就畫」那一條（主目標的畫面不跨幀保留），不是另立的規則。沿用一張後端沒有畫面的 target（例：上一幀上傳失敗，`FramesFailed` 已經報了）取樣到的是未定的內容；資料側照它畫過的紀錄判斷，不另外處理。
- 測試：`render.data` 二案（大小與畫面、沿用與 RP-3 的資料判斷）；`render.extract` 一案（Disabled 第一幀畫、之後沿用、視窗變大那一幀畫、Once）；`sdl_gpu.phases` 的沿用後取樣；`sdl_gpu.unwritten` 改成「前一幀畫過、這一幀沒畫也沒沿用」。既有測試只改輸入：`render.data` 的要求清單多一個大小要求（A1 的案）、`sdl_gpu.targets` 的 `TargetCase::Frame` 照 `Capture` 的大小呼叫 `SetMainSize`。
- 突變：`phase_mutations.json` 新增 10 個（`rd-reuse-not-written`、`rd-size-picture-kept`、`rd-picture-not-marked`、`rd-picture-by-reuse`、`re-update-disabled-ignored`、`re-update-picture-ignored`、`re-update-once-reused`、`re-main-size-not-set`、`wr-once-never-settled`、`wr-once-settled-unseen`），退役 `rd-unwritten-drawn-ignored`；A1 的 `gt-target-not-resized`、`gt-follows-other-as-main` 改寫到資料側（`render.data`）；`gt-unwritten-not-asserted`、`rd-writes-3d-depth-dropped`、`rd-unwritten-drawing-ignored` 的 find 跟著改。

### 第 14.11 節的驗證

- 本機（macOS Intel，只跑 cpu）：全量建置成功；`ctest -L 'cpu|shader'` 254/255（`cmake.PluginSelection` 是借用依賴的環境問題，同前）；cpu 突變 82/82 killed（phase、target、render 的 `cp-`／`rd-`／`re-`、`vs-camera-list-not-extracted`）；runner 結束後確認每個突變的 find 原文都在（沒有殘留）。
- Linux 主機（`b1b173a5`，bundle 送過去；Lavapipe、xvfb、不開視窗；cpu 那一步不帶 `SDL_VIDEODRIVER`）：`ctest -L 'cpu|shader'` 255/255；`sdl_gpu.phases`、`unwritten`、`targets`、`pass2d` 4/4；pass3d（去掉 `GPUSubmit` 一案）11/11；GPU 突變 59/59 killed（D184 的 GPU 既有突變加 phase 的 GPU 突變，不含會開視窗的 3 個）。

## RP-3 機制的設計審查與純淨 Code Review（同日）

- 設計審查（Opus／high，`build/evidence/v8-arch-gyo2-render-a2/design-review-rp3.zh-Hant.md`）：Blocker 1、Should 4、Nit 5、Note 6。純淨 Code Review（Fable／high，只給範圍 `5f73da5e..8db63abf`，`pure-code-review-rp3.zh-Hant.md`）：高 2、中 4、低 4。兩份都不在 git。
- 主會話的指示：(1) 後端回報這一幀實際畫了什麼（S-3、純淨 High 1 的根本解）要後端寫渲染 World，會改 SDL-5（`sdl_gpu` 只讀），交使用者決定 (a)/(b)/(c)；這一輪不接後端的寫入，資料側的 `RenderFrameReport`／`Apply` 留著。(2) 第 4.3 節「後端斷言」照字面：判斷是 render 的純函式、後端呼叫並斷言；不加 render 側的檢查 stage。(3) 其他照做。
- 宣告第 14.12 節記下改了什麼與新 RC（RC-A2-7、RC-A2-8）。

| 指摘 | 結果 | 怎麼做／理由 |
|---|---|---|
| 設計 B-1 跟主目標走的深度在視窗改大小時斷言 | 採用 | 先寫失敗的測試：Linux 主機上 `b1b173a5` 加一案（主目標的畫面 8×8、資料側 16×16），舊碼 `GYO_ASSERT failed: t.size == on.size && "a 3D pass's depth target has its target's size"`（`GPUDevice.cpp:1078`），SIGABRT。改成：深度的大小在資料側比（`FirstDepthOfAnotherSize`，後端斷言），GPU 上大小不同（主目標的畫面比資料側晚一幀）時跳過那些 draw、計入 `skippedDraws`（`[gd-depth-size-skipped]`） |
| 設計 S-1／純淨 High 2 同一張 target 多台相機、「不畫」有多種狀態 | 採用 (i) | 每張 target 一個判斷（宣告第 14.12 節第 1 點）；inactive、無效的相機也算「不畫」。和 Godot 的衝突點與膠水寫在宣告 |
| 設計 S-2 沿用一張沒有畫面的 target 不斷言 | 採用 | `FirstUnwrittenRead` 只把有畫面的 retained 當寫過；`sdl_gpu.unwritten` 改成這一種 |
| 設計 S-3／純淨 High 1 「有畫面」不跟後端的實況 | 一半採用、一半等使用者 | `RenderTextures::Apply(RenderFrameReport)` 成為畫面與大小的唯一入口；紀錄現在由資料側做（`AssumedReport`），後端畫不成的那一幀仍當畫過（代價與 RC-A2-7 在宣告）。後端的實況等 SDL-5 的決定 |
| 設計 S-4／純淨 Medium 4 Once 的前提、沒有握手 | 寫明，不改做法 | 前提寫進宣告；`Lend` 的情形〔未確認〕＋RC-A2-8。比過「渲染側看模式的變化」：要渲染 World 存每台相機的跨幀狀態，那是 render 頁第 7.3 節與 RC-RD 5 留給 D3 的事，這一批不開。Godot 的對照更正（改的是伺服器那一份，場景端的 `update_mode` 不變） |
| 純淨 Medium 3 `PassReuse` 不是 pass | 採用 | `RenderCommands::retained`；`PassWrites` 回到三欄，後端沒有沿用的分支 |
| 純淨 Medium 5 RP-3 不該由後端執行 | 不採用 | 使用者保留第 4.3 節原文「後端斷言」。判斷留在 render 的純函式；斷言從 adapter（`GPUDevice.cpp`）移到 Plugin 的 `RequireRenderContracts`（`RenderContracts.hpp`），headless 的資料測試直接呼叫它 |
| 純淨 Medium 6 大小只送變更、沒有回饋 | 採用 | 記住套用過的大小，沒套用的每幀再要求（level-triggered，`[rd-size-asked-again]`） |
| 純淨 Low 7 兩張表、follow 推導兩次 | 不改 | 兩張表各是一種東西（貼圖／target）；來源側的 `TargetSizeOf` 是相機要的投影大小，渲染側的是貼圖的大小，跟隨者的領頭被放掉時兩邊都退回「沒有大小」（`[rd-size-leader-released]`） |
| 純淨 Low 8 `ApplyAsks` 的 `get_if` 鏈、未知 id 無聲 | 一半採用 | 未知 id 的 `TextureTargetSize` 計入 `refusedAsks`（同 `TextureUpdate`）。改成 visitor 不做：既有突變的 find 都在這幾行，改寫會讓它們全部要重寫，判斷上沒有好處 |
| 純淨 Low 9 命名、written 不去重 | 採用命名 | `KeepPictures` 拿掉（`Apply`）；後端的 `fresh` 改叫 `picture`（和資料側同名同向）；`noteStage`／`onceStage` 改叫 `noteOnceStage`／`settleOnceStage`。written 不去重：一幀的 pass 數小，線性就好 |
| 純淨 Low 10 測試 | 採用 | 「沒畫」改看 `Retains()`；`pending.size() == 6` 旁寫出六個是哪些；第 1、2 項的情況各加案（沿用沒有畫面的、inactive 的相機的 target 被取樣、多台相機共用一張） |
| 設計 N-1 §14.5 的數量 | 採用 | 41 個與理由 |
| 設計 N-2、N-3 缺突變 | 採用 | `rd-size-same-asked`、`gd-size-ask-ignored` |
| 設計 N-4 `TargetOf` 變寬 | 隨 Medium 3 消失 | 沿用不再是 pass |
| 設計 N-5 每個後端各自呼叫 | 記下 | 第二個後端時再看（同 Medium 5 的理由） |
| 設計 Note-2 `HasPicture(optional)` | 採用 | 主目標一律 false |
| 設計 Note-3 inactive 的相機 | 採用 | 隨 S-1：有畫面就沿用；camera 頁寫一句 |
| 設計 Note-4 沒有相機的產品 | 採用 | render 頁第 3 節「自己呼叫 `SetMainSize`」 |

- 等使用者（SDL-5）的測試：後端畫不成的那一幀之後沿用（要後端的實況）。

## 沒做／沒跑

- Mac（Metal）上的重跑（上一節的已知缺口）；RADV（Xvfb 下建不了裝置）；會開視窗的 GPU 案與突變（`sdl_gpu.frames`、`GPUSubmit` 的案、`gp-frames-never-shown`、`g3-frames-never-shown`、`gp-present-not-recorded`）。
- Windows（D3D12）上的 `sdl_gpu.phases`、`sdl_gpu.unwritten`——交給 L1（Windows 只建置，GPU 案要有 GPU 的機器）。重 GPU 量測沒做。
- RC-A2-1～4（第 14.6 節）；A3、A4、B1、B2、C1。
