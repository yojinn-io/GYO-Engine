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

## 設計審查與修正（同日）

兩份審查，報告在主 checkout 的 `build/evidence/v8-arch-gyo2-render-a3/`（不在 git）：
- 每批的設計審查（Opus／high，`design-review-a3.zh-Hant.md`）：Blocker 0、Should 4、Nit 6、Note 6。
- 純淨 Code Review（Fable／high，範圍 `859258d6..a91ee2e5`，`pure-code-review-a3.zh-Hant.md`）：高 1、中 7、低 8。

修正是另一個 commit（不 amend）。小宣告加第 15.8 節。

**先寫的測試**：
- `PassKindName`（S3、M4）：`RenderTests.cpp` 的「what each kind of pass」一案加 `[rd-pass-kind]`，在 render 還沒有這個函式時建置失敗（`RenderTests.cpp:268:35: error: no member named 'PassKindName' in namespace 'GYO::Engine::Render'`，本機）。
- T3 的 `wall` 上界（S2）、`BeginTiming` 重寫上一幀（M1）、`LastFrameTiming()` 第一幀之前沒有值（M3）：這幾個是加強測試或改形狀，不是重現既有的錯；在舊的程式上 `wall` 的兩條本來就成立（守的錯由新突變 `tm-*-from-epoch` 示範），其餘跟著新的介面寫。沒有「先失敗」可記的錯。

### 設計審查

| 項 | 處理 | 一句理由 |
|---|---|---|
| S1 視窗幀的值≈幀間隔，名字會被誤讀 | **已定 (ii)：改名**（使用者「照建議」，修訂 2） | `FrameSeenDone{frame, after, within}`／`LastFrameSeenDone()`；見下面「修訂 2」與宣告第 15.9 節 |
| S2 `atMost > 0` 驗不到起點 | 採用 | `Capture` 前後的 `wall`：`0 < atMost ≤ wall`、CPU 時間和 ≤ `wall`；突變 `tm-gpu-from-epoch`、`tm-cpu-from-epoch` |
| S3 `PassLabel` 不窮舉 | 採用 | 種類名字改由 render 的 `PassKindName`（`Overloaded` 窮舉）給，和純淨 M4 一起；只加純函式、不改資料形式，和 A4 的 `RenderCommands.hpp` 改動不相鄰 |
| S4 第 5.1 節的 cpu 驗收移到 gpu 沒寫 | 採用 | 第 15.4 節加一段 |
| N1 `recorded == false` 漏了跳過的幀 | 採用 | 公開標頭的註解加跳過的幀、拿不到 command buffer 時 timing 停在前一幀 |
| N2 `debugGroups` 可推出、只數自己 | 部分 | 留著（見純淨 M6）；宣告與註解寫明它數的是 `DebugGroup`，只刪 `SDL_PushGPUDebugGroup` 一行的錯只有擷取工具看得到（突變目錄的「沒加」） |
| N3 回傳內部參考 | 採用 | 隨純淨 M3 改成 `optional` 的複本 |
| N4 `timing` 的隱藏耦合 | 採用 | 隨純淨 M2：`FrameTiming&` 明確傳進 `DrawFrame`／`RecordPasses`，斷言每個 pass 一格；編號移到 `BeginFrame` |
| N5 Bevy／Godot 是選用 | 採用（記下） | 偏離表加一列；開關不加、RC-A3-3（見純淨 M1） |
| N6 `inFlight` 沒有上限 | 記下 | 註解寫明查不到的 fence 留到裝置結束；裝置壞了時送出也失敗，不再加 fence |
| Note 1～5 | 記下 | 照讀；Note 4 的 A4 合併衝突點（`tests/CMakeLists.txt` 一行、sdl 頁相鄰兩項、宣告的狀態行與章節）合併時兩邊都留 |
| Note 6 Mac 上 os 測試是卡住 | 照實記、轉主會話 | 見下面「os 的逾時」；A3 不修 os |

### 純淨 Code Review

| 項 | 處理 | 一句理由 |
|---|---|---|
| H1 `Submit` 路徑的 `atMost` 由「什麼時候去看」決定 | **已定：改名＋區間**（同設計 S1，修訂 2） | 建議 1 的區間（`after`、`within`）與改名一起做；建議 3（拿出公開 status）沒選 |
| M1 診斷一律開、每幀重建字串 | 部分 | 帳本重用（`BeginTiming` 在上一幀的 `FrameTiming` 上寫、名字 `assign` 進既有的字串）；開關不加：沒有量到壓力（AGENTS §2），群組在沒有擷取工具時不做事，加開關會讓送出有兩條路（L5）；量到時 RC-A3-3 |
| M2 隱含的不變量、三種擁有方式 | 部分 | 帳本明確傳給 `DrawFrame`／`RecordPasses`（斷言每個 pass 一格），上傳失敗那條路給空帳；帳本仍由裝置持有、getter 取（改成和 `FrameTally` 一樣由呼叫方傳入要改 `Submit`／`Capture` 的簽名，A4 同時在改所有呼叫；這一輪不改） |
| M3 兩個 getter 不一致、參考懸空 | 採用 | 兩個都回 `optional` 的值；`Shutdown` 之後沒有值 |
| M4 visitor 不窮舉、種類名在 backend | 採用 | 同設計 S3 |
| M5 名字壓成字串、格式成了契約 | 不改 | 名字就是擷取工具裡的群組名（使用者要的「每個 pass 的名字」），種類與目標在同一個位置的 `RenderCommands` 裡；格式是宣告第 15.3 節第 1 點定的 |
| M6 `debugGroups` 是衍生值 | 不改（理由） | 第 5.1 節的驗收是「除錯標記只驗呼叫次數」，這是擷取工具之外唯一看得到的事實；拿掉就只剩測試專用的接縫，那更不該加 |
| M7 時鐘寫死、測試對非同步下斷言 | 部分 | 型別與讀法改成 `Engine::Time`（`Duration`、`Now()`）；T4 不要求視窗上一定看到做完的幀；時鐘不注入：`wall` 上界從外面驗起點，注入要在 `Create` 多一個測試接縫、精確值會綁住讀時鐘的次數 |
| L1 編號在交出之前 | 不改（寫明） | 帳本描述的是錄的幀；交不出去的那一幀照樣錄過，`LastGPUFrameTime` 只有交出去的幀 |
| L2 `DrawFrame` 多了職責、看兩輪 | 採用 | 編號與看 fence 移到 `Submit`／`Capture`；兩輪各自的理由寫在 `Submit` |
| L3 `erase_if` 述詞有副作用 | 採用 | 明寫的迴圈 |
| L4 交出的程式寫兩次 | 採用 | `Hand`（時刻、帶 fence 送出、`Commit`、記帳），`Submit`／`Capture` 共用 |
| L5 成功與否由 fence 決定 | 不改 | 三個後端帶 fence 的送出和不帶的是同一條（設計 Note 1）；隨 RC-A3-3 |
| L6 命名 | 部分 | `cpu` 留著（使用者的話是「每個 pass 的 CPU 時間」，Bevy 也叫 `elapsed_cpu`），註解寫明是錄製那段的牆上時間；`TimingOf` 改成 `BeginTiming`，留在 `PassLabels.hpp`（帳本是名字做的）；`recorded` 與 `unmappedPasses` 寫在註解 |
| L7 沒有整幀合計 | 不改 | 第 5.1 節要的是每個 pass；整幀的牆上時間作者在 `Submit` 外面量得到 |
| L8 `inFlight` 沒有上限 | 記下 | 同設計 N6 |

### 突變

- `gpu_debug_mutations.json` 現在 22 個：
  - 新加：`tm-timing-cpu-kept`、`tm-timing-groups-not-reset`（取代 `tm-groups-carried-over`）、`tm-cpu-from-epoch`、`tm-timing-not-begun`（斷言）、`tm-timing-none-given`、`tm-hand-not-tracked`（取代 `tm-capture-not-tracked`）、`tm-gpu-from-epoch`。
  - 退役：`tm-groups-carried-over`、`tm-capture-not-tracked`、`tm-submit-not-tracked`（理由在檔案的 `description`）。
  - 改寫 find（程式改了，守同一個性質）：`tm-label-no-index`、`tm-label-target-ignored`、`tm-label-shader-dropped`、`tm-timing-not-named`、`tm-timing-starts-recorded`、`tm-frames-not-counted`（expect 加 `[tm-one-per-pass]`：沒有編號時第一幀沒有帳）、`tm-unmapped-recorded`、`tm-cpu-not-measured`、`tm-group-not-pushed`、`tm-capture-not-looked-at`、`tm-gpu-older-kept`、`tm-gpu-time-zero`、`tm-status-timing-not-given`；`tm-label-kind-wrong` 移到 render 的 `PassKindName`（`render.data`）。
- 既有突變（D184）：find 在改動範圍的 `g3-base-not-advanced`；被改過的測試殺的 `rd-reads-white`、`rd-reads-inputs-dropped`、`rd-writes-3d-depth-dropped`（「what each kind of pass」一案加了一行）。

### 修正後的驗證

- 本機（macOS Intel，只跑 cpu；`bf1940b3`）：全量建置成功（新的警告沒有；`Render.cpp:225` 的 `-Wmissing-field-initializers` 是 A2 就有的）；`ctest -L 'cpu|shader' -j 6`（load 約 4.8）259 個裡 258 通過，失敗的只有 `cmake.PluginSelection`（借用依賴的環境問題）。cpu 突變 11/11 killed（新的 cpu 8 個、D184 的 `rd-reads-white`、`rd-reads-inputs-dropped`、`rd-writes-3d-depth-dropped`）；runner 前確認每個 find 都在、恰好一處，結束後原文都在。
- Linux 主機（`bf1940b3`，bundle 送過去；Lavapipe `llvmpipe (LLVM 20.1.2, 256 bits)`、xvfb、不開視窗；cpu 那一步不帶 `SDL_VIDEODRIVER`）：`ctest -L 'cpu|shader'` 259/259；`sdl_gpu.pass2d`、`targets`、`phases`、`contracts`、`framedraws`、`unwritten`、`timing` 7/7；pass3d（去掉 `GPUSubmit` 一案）11/11；突變 25/25 killed（新目錄裡不開視窗的 21 個、D184 的 4 個）。
  - `sdl_gpu.timing -s`：`#0 Pass2D -> main` 28.2 µs、`#1 Pass2D -> target 999` 沒錄、`#2 Pass3D -> main` 12.6 µs；`frame 1 done on the GPU within 0.334678 ms of its submission (the Capture took 0.419226 ms)`。
- 沒跑：`sdl_gpu.frames`（T4，會開視窗）與 `tm-status-timing-not-given`；Metal、D3D12 照舊沒驗。

### os 的逾時（設計 Note 6；A3 不修 os，轉主會話）

- 本機（macOS Intel）的 `case-timing` 紀錄有 7 份逾時的，**每一份都卡在同一個案例**：`OS wake signal loses no signal sent while the waiter checks its flag`（`gyo_os_tests` 3 份：前面 84 個案例都結束；`gyo_os_wake_fallback_tests` 4 份：前面 7 個案例都結束；那個案例只有 `case start`、沒有 `case end`）。當時的 load 是 78、99、131、133、195、301。不只是變慢，是在那一案停住、60 s 逾時。
- 負荷低的時候（load 約 4.5～4.8，審查修正之後）：`ctest -L 'cpu|shader' -j 6` 裡兩個都通過；再單獨跑 `gyo_os_tests` 0.80 s、`os.wake_fallback` 0.52 s，都通過。
- Linux 主機（同一份程式）兩輪都通過。判斷：高負荷下那一案會停住（可能是潛在的競爭條件在高負荷下顯形）〔未確認：沒有讀 os 的程式、沒有重現〕，請 os 的負責會話看 L1 macos-arm64 的 cpu 結果。

（「驗證」一節裡第一次寫的「負荷造成，未確認」以這一段為準。）

## 修訂 2：看到做完的幀照實際意思改名（同日，使用者的決定）

- 使用者（經主會話轉 side chat，HANDOFF `v8-arch-gyo2-render-a3-s1`）：原話「照建議」，選 S1＝H1 的 (ii)：照實際意思改名，H1 建議的區間欄位一起做，RC-A3-2 留著。比過的 (i)（只寫清楚：名字仍會被讀成 GPU 花了多久）、(iii)（提前 RC-A3-2：和 SDL-4 衝突、SDL 能否在別的執行緒等 fence 未確認）沒選。宣告第 15.9 節。
- **定的名字**：型別 `FrameSeenDone{frame, after, within}`、getter `LastFrameSeenDone()`（`SDLGPUStatus` 與 `GPUDevice`）。
  - 理由：同組的 `FrameTiming`↔`LastFrameTiming()` 是「型別名＝getter 去掉 Last」、沒有 `GPU` 前綴；「Seen」說出量到的是看到的時刻。
  - `within`：使用者例子裡的字，看到做完的那一次看。`after`：上一次看到還沒做完的那一次看，是區間的下界；沒有一次看到還沒做完時是 0。兩者都從交出算起，型別是 `Engine::Time::Duration`。
- **做了什麼**：
  1. fence 的帳從 adapter 拆成不含 SDL 的 `FramesInFlight<Fence>`（`plugins/sdl_gpu/src/FramesInFlight.hpp`，同 `FrameDraws` 的先例），提供 `Hand`／`Look`／`Clear`／`LastSeenDone`。adapter 把 `SDL_QueryGPUFence`、`SDL_ReleaseGPUFence` 交給它，看的時機不變。
  2. 拆出來的理由：區間的下界只有在「看了還沒做完」時才有值。`Capture` 等完才看，所以不開視窗就驗不到下界；拆成純的帳之後，可以用排好的時刻從外面驗。
  3. 文件：宣告第 15.3 節第 5 點、T3、T4，第 15.8 節的舊名旁加注；SDL 頁第 7 節「除錯標記與計時」。render 頁沒有用到舊名。
- **先失敗**：測試先改成新名字，並加 `sdl_gpu frames in flight:` 一案。舊碼建置失敗（本機）：
  - `TimingTests.cpp:25:10: fatal error: 'FramesInFlight.hpp' file not found`
  - `Pass2DTests.cpp:430:28: error: no member named 'FrameSeenDone' in namespace 'GYO::Plugins::SDLGPU'`
  - `Pass2DTests.cpp:430:58: error: no member named 'LastFrameSeenDone' in 'GYO::Plugins::SDLGPU::SDLGPUStatus'`
- **測試**：
  - 新 ctest `sdl_gpu.framesinflight`（label `cpu`，T6）：排好的時刻驗區間 `{1, 4 ms, 12 ms}`、`{2, 2 ms, 10 ms}`；同一次看到兩幀做完時留最新的；做完的 fence 放掉一次；`Clear` 放掉還在飛的。
  - `sdl_gpu.timing`（T3）多一條 `after == 0`。
  - `sdl_gpu.frames`（T4）的檢查改成有值時 `after < within`。
- **突變**（改名批，D184 只跑 find／expect 受影響的）：
  - 新加一個 `tm-seen-after-dropped`：區間的下界是新的行為，沒有別的突變守它。
  - `tm-gpu-older-kept`、`tm-gpu-time-zero` 跟著帳搬到 `FramesInFlight.hpp`，改用 cpu 的案例（`tm-gpu-time-zero` 的 expect 改成 `[tm-seen-within]`）。
  - `tm-hand-not-tracked` 的 find 改成 `inFlight.Hand(...)`。
  - 目錄共 23 個。`Clear` 現在由 `[tm-seen-cleared]` 看得到；description 裡「沒加」的那一條跟著改，改名批不為它加突變。

### 修訂 2 的驗證

- 本機（macOS Intel，只跑 cpu；`4b2fe934`）：全量建置成功、沒有新警告。
  - `SDL_ASSERT=abort ctest -L 'cpu|shader' -j 6`（load 約 23）：260 個裡 258 通過（新的 `sdl_gpu.framesinflight` 通過）。失敗的兩個：
    - `cmake.PluginSelection`：借用依賴的環境問題。
    - `sdl.unattended_assertion`：我把 `SDL_ASSERT=abort` 給了整個 ctest。這一案要看自己 handler 印的字，環境變數讓 SDL 的預設 handler 先處理，所以失敗（`[sdl-assert-no-handler-text]`）。不帶環境變數單獨跑通過；帶著單獨跑會重現。和程式無關；產品執行檔的 ctest 本來就各自帶 `SDL_ASSERT=abort`。
  - cpu 突變 3/3 killed（`tm-gpu-older-kept`、`tm-gpu-time-zero`、`tm-seen-after-dropped`，都在 `sdl_gpu.framesinflight`）。runner 前確認目錄裡 23 個 find 都在、恰好一處。
- **Linux 主機**（`4b2fe934`；Vulkan 用 Lavapipe `llvmpipe (LLVM 20.1.2, 256 bits)`，在 xvfb 下跑；不是實體 GPU）：第一次送 bundle 時主機連不上（`scp: Connection closed`、`ssh: ... Host is down`，約 20 分鐘），主會話確認恢復後跑 `run.sh a3r2`：
  - 建置成功；`ctest -L 'cpu|shader'`：260/260。
  - 不開視窗的 GPU（`sdl_gpu.pass2d`、`targets`、`phases`、`contracts`、`framedraws`、`unwritten`、`timing`）：7/7。`sdl_gpu.timing` 含 T3 的 `after == 0` 與新名字。
  - GPU 突變：目錄 `host_a3r2.json`（不開視窗的全部 22 個，含 find 改過的 `tm-hand-not-tracked`、`tm-capture-not-looked-at`、`tm-gpu-from-epoch`、新的 `tm-seen-after-dropped`）：22/22 killed。
  - bundle 用完已刪；只用 `~/Workspace/render-card-a3`，沒改機器設定。
- 沒跑：`sdl_gpu.frames`（T4，會開視窗）與 `tm-status-timing-not-given`；Metal、D3D12。

## 沒做／沒跑

- **Metal（Mac）上沒驗**：本機只跑 cpu。Metal 特有的行為——debug group 在 pass 之外 push 到 command buffer 本身（`SDL_gpu_metal.m:1315-1349`）、Xcode 的擷取裡群組怎麼顯示、Metal 的 fence——都沒有在 Mac 上跑過（已知缺口）。macOS 的 L1 是 macos-arm64、只建置。
- **Windows（D3D12）沒驗**：`WinPixEventRuntime.dll` 不在時 SDL 的警告與 no-op、PIX 裡的群組名，都只讀了 SDL 的原始碼。
- 擷取工具（RenderDoc／Xcode／PIX）裡實際看到的群組名：CI 與這張卡都沒有擷取工具，只驗次數（`[tm-groups]`）。
- 會開視窗的 `sdl_gpu.frames`（T4）與它的突變 2 個、`gp-frames-*`；RADV（Xvfb 下建不了裝置，同 A2）。
- 重 GPU 量測（Object_PvP 的整體幀時間）沒做；`sdl_gpu.budget` 也沒跑。
- 視窗幀的 `FrameSeenDone` 在 Mac／Windows 上的值；精確的 GPU 時間（RC-RP5、RC-A3-2）。
