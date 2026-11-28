# 2026-11-28 渲染管線線 A3：GPU 除錯與計時（`v8-arch-gyo2-render-a3`）

Owner：渲染管線線のカード。分支 `claude/gyo2-render-a3`（`claude/gyo2-render-a` 的 `859258d6`＝PR #370 的 head 之上；A4 同時在另一個工作樹，之後兩條都 merge 回 `claude/gyo2-render-a`）。作者 Opus 5.5／high（子 Agent）。宣告：[渲染管線宣告](../architecture/target/v2/render-pipeline-declaration.zh-Hant.md)第 15 節（A3 小宣告）；[SDL 頁](../architecture/target/v2/sdl.zh-Hant.md)第 7 節「除錯標記與計時」。

## 前提

- 使用者 Q1 (a)（總宣告第 12 節，原話「全部照推薦」）：每個 pass 加除錯標記＋Engine 報每個 pass 的 CPU 時間與整幀的 GPU 時間（fence）；每個 pass 的 GPU 時間等 SDL 的 timestamp query（RC-RP5）。不選 (b)（adapter 三份原生 timestamp）、(c)（每個 pass 一個 fence）。
- A3 是下段：不改資料形式（`RenderCommands`、`CameraPasses` 不動）。小宣告寫完直接實作（主會話的指示）。
- 〔Engine〕pass 的名字與除錯群組、錄製的 CPU 時間、fence；〔作者〕拿數字做什麼（平均、預算、降畫質）。

## 做了什麼

1. **小宣告**：宣告第 15 節（現況與查到的事、前提、設計、判定 T1～T5、測試與突變、RC-A3-1／2、不做）。Bevy 的 `RenderDiagnosticsPlugin`（名字是 pass 的常數、CPU 用 `Instant`、結果幾幀後到）與 Godot 的 `capture_timestamp`、`measure_render_time` 附檔名:行號；SDL 3.4 的 debug group 與 fence 的實作逐後端查過（Vulkan 沒有 `VK_EXT_debug_utils` 時不做事、D3D12 要 `WinPixEventRuntime.dll`、Metal 在 pass 外 push 到 command buffer）。
2. **pass 的名字**（`plugins/sdl_gpu/src/PassLabels.hpp`，不含 SDL）：`PassLabel(pass, index)`＝「`#<index> <種類>[ <shader>] -> main`／`-> target <id>`」；`TimingOf(frame, commands)` 每個 pass 一格。名字從資料推（資料裡沒有名字）。
3. **adapter**（`GPUDevice.cpp`）：`RecordPasses` 每個有目標的 pass 包一個 `DebugGroup`（RAII：push＋計數／pop，在 render pass 之外）、量錄製的 `steady_clock`；`DrawFrame` 開頭數幀、建這一幀的 `timing`、先看前面的 fence。`Submit` 改用 `SDL_SubmitGPUCommandBufferAndAcquireFence`（不等），開頭也看一次 fence；`Capture` 的 fence 交給同一份帳（`Track`），等完後 `LookAtFences` 放掉它。`~State` 放掉還沒看到做完的 fence。
4. **報告**：`SDLGPUStatus::LastFrameTiming()`、`LastGPUFrameTime()`（公開型別 `PassTiming`、`FrameTiming`、`GPUFrameTime` 在 `SDLGPUPlugin.hpp`）；`GPUDevice` 多兩個同名 getter。`Submit`／`Capture` 的簽名不變（A4 同時在改測試的呼叫）。
5. **文件**：SDL 頁第 7 節加「除錯標記與計時」一項，`GPUSubmit` 那一項的「交給 GPU 之後」改成帶 fence 的送出。

## 測試

- 先寫失敗的測試：`TimingTests.cpp` 與 `PassLabel`／getter 的空殼先建好（一時的分支 `a3-stub-temp`，`20abd5de`，不合併、只在本機）：
  - 本機 `sdl_gpu.passlabels`：`TimingTests.cpp:53/54/56/57` 四個名字不對、`:64` `REQUIRE( timing.passes.size() == 2 )` 不成立，`test cases: 1 | 0 passed | 1 failed`。
  - Linux（Lavapipe）`sdl_gpu.timing`：`TimingTests.cpp:103: FATAL ERROR: REQUIRE( first.passes.size() == commands.passes.size() ) is NOT correct!`。
- 新 ctest：`sdl_gpu.passlabels`（label `cpu`，T1）、`sdl_gpu.timing`（label `gpu`，`-s`，T2、T3）。`sdl_gpu.frames` 加 `[gp-frames-timing]`、`[gp-frames-gpu-time]`（T4；會開視窗，這張卡沒跑，L1 跑）。
- 既有測試的判斷沒改。

## 驗證

- **本機（macOS Intel，只跑 cpu；`cfa31b14`）**：依賴照 A2 借 `gyo2-big-map-s4c` 的 `_deps`、shader 工具借 `gyo2-render-pipeline-2384e5` 的 `a2-shader-tool`（A3 沒改工具）。全量建置成功、無警告。`ctest -L 'cpu|shader' -j 8`：259 個裡 256 通過；`cmake.PluginSelection` 是借用依賴的環境問題（同 A2）；`gyo_os_tests`、`os.wake_fallback` 逾時（60 s）——單獨重跑也逾時，當時這台 Mac 的 load 是 133（其他會話在建置；case-timing 記錄 `load=133.83`），卡在 `OS wake signal loses no signal sent while the waiter checks its flag` 一案。A3 沒碰 os；同一份程式在 Linux 主機通過（下一項）。runner 之後再單獨重跑一次（load 約 300）仍逾時。〔判斷：負荷造成；低負荷下沒有重跑確認，未確認〕
- **Linux 主機（`cfa31b14`；Ubuntu 24.04，gcc-14，preset `test`；GitHub clone `claude/gyo2-render-a` 的 `859258d6` 再 fetch bundle；`~/Workspace/render-card-a3/`）**：
  - **Vulkan 跑在 Lavapipe**（`VK_DRIVER_FILES`／`VK_ICD_FILENAMES`＝`lvp_icd.json`，`vulkaninfo`：`llvmpipe (LLVM 20.1.2, 256 bits)`），xvfb 下、不開視窗；cpu 那一步不帶 `SDL_VIDEODRIVER`。
  - `ctest -L 'cpu|shader'`：`100% tests passed out of 259`。
  - GPU（不開視窗的）：`sdl_gpu.pass2d`、`targets`、`phases`、`contracts`、`framedraws`、`unwritten`、`timing` 7/7；pass3d（去掉 `GPUSubmit` 一案）`test cases: 11 | 11 passed`。
  - `sdl_gpu.timing -s` 印出的值（只印、不判）：`#0 Pass2D -> main` 錄、CPU 13.785 µs；`#1 Pass2D -> target 999` 沒錄、0；`#2 Pass3D -> main` 錄、16.731 µs；`frame 1 done on the GPU within 0.210475 ms of its submission`（Capture 等自己的 fence，所以是「送出到等完」）。

## 突變

- 新 `tests/common/render/gpu_debug_mutations.json`：18 個。`guards` 指第 15 節的判定（這一批不改 DI 條文：下段；理由在宣告第 15.5 節與檔案的 `description`）。
  - cpu 6 個（`tm-label-*`、`tm-timing-*`）：本機 6/6 killed、Linux 6/6 killed。
  - gpu 10 個（`tm-frames-not-counted`、`tm-unmapped-recorded`、`tm-groups-carried-over`、`tm-recorded-not-marked`、`tm-cpu-not-measured`、`tm-group-not-pushed`、`tm-capture-not-tracked`、`tm-capture-not-looked-at`、`tm-gpu-older-kept`、`tm-gpu-time-zero`）：Linux（Lavapipe）10/10 killed。
  - 會開視窗的 2 個（`tm-submit-not-tracked`、`tm-status-timing-not-given`，`sdl_gpu.frames`）：**沒跑**。
  - 跑之前確認每個 find 都在、恰好一處；Python 用 `-B` 並清 `__pycache__`。
- 既有突變（D184）：find 落在改過的程式（`GPUDevice.cpp`、`SDLGPUPlugin.cpp`、兩個標頭；改動的行前後 2 行內）的只有 `g3-base-not-advanced`（`render_mutations.json`，`RecordPasses` 的幾何偏移）：Linux killed。所有目錄裡指這幾個檔的突變 find 都還在、恰好一處（沒有失效的）。被改過的測試殺的 `gp-frames-*`（`sdl_gpu.frames`，會開視窗）沒跑。
- Linux 的 runner 合計 17/17 killed（新的 16＋`g3-base-not-advanced`）。

## 沒做／沒跑

- **Metal（Mac）上沒驗**：本機只跑 cpu。Metal 特有的行為——debug group 在 pass 之外 push 到 command buffer 本身（`SDL_gpu_metal.m:1315-1349`）、Xcode 的擷取裡群組怎麼顯示、Metal 的 fence——都沒有在 Mac 上跑過（已知缺口）。macOS 的 L1 是 macos-arm64、只建置。
- **Windows（D3D12）沒驗**：`WinPixEventRuntime.dll` 不在時 SDL 的警告與 no-op、PIX 裡的群組名，都只讀了 SDL 的原始碼。
- 擷取工具（RenderDoc／Xcode／PIX）裡實際看到的群組名：CI 與這張卡都沒有擷取工具，只驗次數（`[tm-groups]`）。
- 會開視窗的 `sdl_gpu.frames`（T4）與它的突變 2 個、`gp-frames-*`；RADV（Xvfb 下建不了裝置，同 A2）。
- 重 GPU 量測（Object_PvP 的整體幀時間）沒做；`sdl_gpu.budget` 也沒跑。
- 設計審查與純淨 Code Review（主會話排）。
