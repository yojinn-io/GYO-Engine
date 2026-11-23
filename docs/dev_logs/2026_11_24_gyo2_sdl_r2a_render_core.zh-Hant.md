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

## 卡片判的（理由在 render 頁、sdl 頁第 7 節；可推翻）

| 題 | 定法 |
|---|---|
| 貼圖的 id 誰配 | 資料側（`RenderTextures`），從 1 起、不重用、0 是白貼圖（照 Bevy 的 `AssetId`；master 是裝置配 handle） |
| 一幀的資料何時清 | `Cleanup` stage（幀尾，同 Bevy）；幀頭清會丟掉開局提的貼圖要求 |
| `GPUSubmit` 的錯誤 | 不是 System 的錯誤；`SDLGPUStatus` 計數（送出／跳過／失敗的幀、被拒的貼圖要求、跳過的 draw、沒有對應的目標） |
| 換 World 時的貼圖 | 每個準備好的 World 一個號碼；別的 World 送出時才放掉前一個的貼圖（替換失敗時什麼都不放） |
| 顏色空間 | 2D pass 照 bytes 混合（UNORM），同 ImGui；3D 的線性空間交 R2b（RC-RD 3） |
| extract schedule | R2a 不排；第一個讀來源 World 的子批加 |
| shader id、實例數 | 沒有消費者，R2b 加 |
| shader 的嵌法 | 建置時編、嵌進 `sdl_gpu`（Dear ImGui 的 SDL_GPU 後端同）；P22 的讀法使用者定 (a)（見下） |
| 命名 | `gpu`→`GPU` 進縮寫表（`SDLGPU`、`GPUSubmit`） |
| 幀時間的預算 | Object_Connect 大小的一幀經 `Capture` 8 ms 以內（只印不判） |

## 交使用者的

- **shader 編譯器的下載**：使用者選 (A) 照 master、同意五項（2026-11-24，經主會話，原話「照建議」）。比過的 (B)：編好的 shader 當產生檔放進 repo。
- **P22 的讀法**：使用者選 (a)（2026-11-24，經主會話，原話「照建議」）——內建 shader 跟著 Plugin 走、`sdl_gpu` 自己編與嵌；遊戲 shader 的「共通管線找編譯器」由第一個有遊戲 shader 的批做。比過的 (b)：內建 shader 也走共通內容管線、執行時讀資產。
- 兩頁 Design Invariants（RD-1～RD-4、SDL-4、SDL-5）：照新 AGENTS §2.6 沒有推翻使用者決定的條文，不逐條核准（主會話 2026-11-24）。

## 合併 gyo2 與跟上（2026-11-24）

- A（#306，426add81）合進 gyo2 後 merge：縮寫表與 `naming_guard` 的 `ACRONYMS` 合起來（`sdl`、`spsc`、`gpu`）；`config/plugins.csv` 照登記順序 `sdl_audio` 在 `sdl_gpu` 前；sdl 頁的第 6 節 `sdl_audio`、第 7 節 `sdl_gpu`，SDL-3 取 A 的、SDL-4／SDL-5 取 R2a 的。
- os.md 的 R4 與 DI 2 拿掉「the host shader tool」的例外，code page 那一段改指 `sdl_gpu` 的 shader 工具（主會話：字面跟上已經做的事）。
- A 的 naming 守衛（Plugin 的測試整檔在 Plugin 的 namespace）查出 `sdl_gpu` 的兩個測試檔 8 件 → 照 `sdl_window` 包進 `GYO::Plugins::SDLGPU`；A 的突變 `nm-acronyms-off`、`nm-acronym-sdl-dropped` 的 find 跟著縮寫表改，2/2 killed。
- merge 後本機 `ctest -L 'cpu|gpu'` 234/234。
- 給 K 的 `ImDrawIdx`（16／32 位元）：主會話記進批次表、K 開卡時寫進 brief。

## 設計審查（Opus／high，作者同為 Opus；Fable 暫停）

blocker 1、should 7、nit 7：

- **B1**：shader 工具不建輸出的上層目錄，Linux／Windows 的 `sdl_gpu.shader_tool.compiles` 會失敗（macOS 剛好被 Metal 的工作目錄建了）→ `Publish` 先建目錄。
- **S1** 同一幀先上傳再放掉：放掉延到 copy pass 錄完之後。**S2** 上傳失敗時這一幀新建的貼圖沒有像素 → 放掉、計入被拒。**S3** 位元組數改 64 位元，超過 4 GiB 的一幀算失敗（原本 `GrowTo` 會無窮迴圈）。**S4** 換 World 的貼圖沒測 → `sdl_gpu.frames` 補替換失敗與成功兩段、`SDLGPUStatus::TextureCount()`、突變 3 個。**S5** 幀時間的預算補宣告；P22 送主會話。**S6**「ImGui 的 draw list 原樣交得進來」改寫（16 位元索引、callback）。**S7** 測試的裝置開 SDL 的驗證。
- nit：文件的清除時點、被拒的文字進 `LastFailure`、`SdlError`→`SDLError`、shader 工具的拒絕測試改比文字、Linear 取樣的像素測試、快取 key 加 `GyoProcessCodePage.cmake`、`Capture` 的目標不帶 SAMPLER。順手：連續的 draw 只在 pipeline／貼圖改變時才重綁。

## 查證（本機 macOS Intel，`test` preset，RelWithDebInfo）

- 全量建置 0 錯誤；`ctest -L cpu` 222/222（審查修正前）；審查修正後 `render.*`、`sdl_gpu.*` 7/7（含 label `gpu` 的 `sdl_gpu.pass2d`、`sdl_gpu.frames`、`sdl_gpu.budget`，Metal）；主機建置的 `sdl_gpu.shader_tool.*` 3/3。
- DXC 在本機從原始碼建（重用主 checkout 的 master 時期原始碼，`FETCHCONTENT_SOURCE_DIR_*`，沒有新下載）。
- 突變 `docs/architecture/target/v2/scripts/render_mutations.json`：36/36 killed（gp- 在本機 Metal 跑）。第一輪 2 個 wrong_failure：`rd-clip-y-not-turned` 的 expect 指錯 CHECK（view 原點在 0 時左上角不變，改看右下角）、`rd-tex-ids-from-0` 先被 RD-3 的斷言擋下（expect 加上斷言文字）。
- os 守衛、naming 守衛、orchestration 守衛：乾淨。
- L1：見 PR。Windows（Direct3D 12）實機沒跑。
