# 2026-11-24 SDL 線子批 R2a：`render`、`render_world` 與 `sdl_gpu`（`v8-arch-gyo2-sdl-r2a`）

Owner：架構。GYO2 主會話開的任務卡（使用者「開 R2a」）。分支 `claude/gyo2-sdl-r2a-render-core-26af90`（開向 `gyo2`）。作者 Opus 5.5／high。
根據：[SDL 總宣告](../architecture/target/v2/sdl-declaration.zh-Hant.md)修訂 21（使用者 2026-11-23 動工前核准「SDL 照建議核准」）第 3.3、6.1、9.1、9.2、10.2 節的 R2a 列；Q7＝(a)（`-plugin-in-world-rule-removed`）、讀法 2（`-sdl-system-calls-via-adapter`）；I（#301）交來的像素排法；P（#300）交來的 shader 工具的 os 守衛地位。照新 AGENTS 不另寫事前宣告；取捨寫在 [render](../architecture/target/v2/render.zh-Hant.md) 與 [sdl](../architecture/target/v2/sdl.zh-Hant.md) 第 7 節。

## 做了什麼

1. **`render` Module**（`engine/modules/render`，函式庫、沒有依賴）：`RenderCommands`（2D pass：目標、清除色、view、頂點、索引、draw〔貼圖、混合、裁切、偏移〕）、`RenderTextures`（CPU 像素的貼圖：id 由資料這一側配、建立／局部更新／放掉的要求）、中性的換算（`ViewToClip`、`ViewOf`、`ClipToPixels`）、2D pass 的 HLSL 與 `ShaderAbi.hpp`。
2. **`render_world`**（渲染側、`default=no`、`depends=render`）：登記兩個 Resource、在 `Main` 的 `Last` 之後插 `Submit`、`Cleanup` 兩個 stage、`Cleanup` 清這一幀的資料。
3. **`sdl_gpu` Plugin**：`Configure` 建裝置、claim 視窗（`sdl_window` 的不透明把手）、建 shader 與 sampler；`Build` 登記 `threadBound` 的把手、把 `GPUSubmit` 掛在 `Submit`；`GPUSubmit` 經 adapter 套用貼圖要求、上傳、畫、送出；錯誤只計數（`SDLGPUStatus`）。SDL 只在 `src/adapters/sdl/`。
4. **shader 工具**（`plugins/sdl_gpu/shader_tool/`＋`src/adapters/sdl/ShaderTool.cpp`）：照 master 的 `engine/render/shaders/pipeline` 縮成最小，建置時把 HLSL 編成平台的格式、嵌進 `sdl_gpu`。
5. **CI**：L1 的 Linux 列加「Run Linux GPU tests」（Lavapipe＋Xvfb、`ctest -L gpu`）；shader 工具的快取 key 改成 v4、含工具的原始碼。
6. **命名**：縮寫表加 `gpu`→`GPU`（`GYO::Plugins::SDLGPU`、`GPUSubmit`），naming_guard 的 `ACRONYMS` 同步。
7. **文件**：新頁 render；sdl 頁第 1 節的列、第 7 節 `sdl_gpu` 與 shader 工具、SDL-4、SDL-5、範囲の限界；rebuild-naming 的縮寫表。

（以下「卡片判的」「查證」「設計審查」在驗證後補。）
