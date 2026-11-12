# framework 的 3D 預覽元件（`v8-framework-preview-component`）

更新：2026-11-05。Owner：framework（`framework/`）。卡片由架構師會話開出；批次代號照 PR #152 的計畫表
（[voxel-map-editor.md](../../../tools/plans/v8/voxel-map-editor.md)），不另取。
**狀態：完成（2026-11-06）。宣告與守衛的判定變更都已核准；PR 等使用者決定合併。**（2026-11-12 圖書管理員補註：後續：PR #157 已合併，`2cd57244`） 先讀本頁，再讀 [交接](HANDOFF.md)。

## 代號對照

| 代號 | 白話 |
|---|---|
| `v8-framework-preview-component` | 整批：把 model_viewer 的 3D 預覽裡「畫面外 GPU 裝置＋Engine `Renderer`＋讀回＋上傳成 SDL 貼圖」這段中立的組合抽成 framework 的元件，model_viewer 同一批改用；給 voxel 地圖編輯器當第二個使用者 |
| `v8-framework-preview-guard` | 本批裡守衛正向清單的變更（AGENTS.md §14.4 的 1～5），單獨一道核准關卡 |
| D-pv-N | 本批要使用者決定的事項（見「待決」） |

## 來源

- 決定（voxel 線的 [HANDOFF](../../../tools/plans/v8/voxel-map-editor-HANDOFF.md)）：
  `v8-tool-voxel-layers`（3D 預覽元件＝framework，第二個組合者，framework DI 5）、
  `v8-tool-voxel-preview-framework`（PR #146 合併後抽，model_viewer 同一批改用）、
  `v8-framework-preview-component-owner`（交給架構會話＝framework 的 owner 做）。
- 前提：PR #146（model_viewer）已合併進 master（`54948874` 之前）。本批基底是本機 master `54948874`（宣告時誤寫為 `a9ccad7d`；#152 的合併在開 PR 前併入）。
- 規則：[framework 架構](../../../framework/architecture.md) 的 Design Invariants 1～6 與 Replacement Criteria；
  `docs/architecture.md`「Layers: admission and format ownership」的檢查 2（每個新 framework piece 要說出它接合哪幾個
  Engine 能力、哪兩個產品用）；上一次架構健檢（[2026-11-04](../../../checkup/architecture/2026_11_04_architecture.zh-Hant.md)）
  的判斷 4（framework 不是門面）。

## 現況（本機 master `54948874`）

- 來源程式：`tools/model_viewer/include/gyo/model_viewer/PreviewRenderer.hpp` 與 `src/PreviewRenderer.cpp`（約 230 行）。
  裡面混著兩種東西：
  - **中立的組合**：載入 Engine 的 shader bundle（`ShaderLibrary::AppendBundle`）→ `SdlGpuRenderDevice::CreateOffscreen`
    → `Renderer::Initialize`；每幀 `RenderQueue` 設相機 → `Renderer::Render` → `SdlGpuRenderDevice::ReadFrame`；
    `UploadFrame`（依幀尺寸建或換 `SdlRenderer` 的串流貼圖＋`UploadTexture`）；`OrbitCamera`（由 yaw／pitch／distance
    算出 `PerspectiveCamera3D`）。
  - **model_viewer 自己的**：`ModelRenderer` 的 `ModelResource`／`ModelInstance`、骨架細盒（`SubmitSkeleton`）、
    `FrameModel`（依 `ModelAsset` 與 `Pose` 取景）、`PreviewOptions.skeleton`、`ModelViewerErrorCode::PreviewFailed`。
- 使用端：`ViewerWindow.cpp`（視窗）、`PreviewMeasure.cpp`（P1 量測 `measure-preview`）、
  `tests/model_viewer/ModelViewerPreviewSmoke.cpp`（V2／V3，標籤 `gpu`）。
- 第二個使用者的需求（voxel 線）：每幀提交自己的一個或多個 `MeshView`（區塊 mesh）＋調色盤貼圖，unlit、順時針為正面；
  相機由客戶端直接給 `PerspectiveCamera3D`，不一定用 `OrbitCamera`；不需要 Model。
- **Engine 缺口檢查（本批的停下條件之一）：沒有缺口。** `MaterialDesc.shader` 預設 `builtin/unlit`；
  `PipelineDesc.clockwiseFrontFace` 預設 true（`RenderDeviceTypes.hpp`「+Z-forward, +Y-up projection presents outward
  mesh faces clockwise」）；`Renderer.cpp:220` 非 `doubleSided` 的 mesh 剔背面。voxel 要的「順時針為正面、剔背面、unlit」
  就是 Engine 的預設，不需要改 Engine，也不在 framework 補。
- framework 現況：核心 `GYO::ToolFramework`（無 SDL）、`GYO::ToolFrameworkWindow`（只在 `GYO::PlatformSDL` 存在時建）、
  綁定 `GYO::ToolFrameworkImGui`／`GYO::ToolFrameworkJson`。守衛 `tests/framework/framework_dependency_guard.py`
  （核心一張正向清單、每個綁定一張）＋ `tests/framework/CMakeLists.txt` 的 configure 時連結檢查。
  Engine 的 `SdlRenderer` 已有 `CreateStreamingTexture`／`UploadTexture`／`ReleaseTexture`，ImGui 綁定已有
  `ImGuiTextureId`，所以上傳不需要 `*Native`。

## 範圍

### 抽到 framework（新 target `GYO::ToolFrameworkPreview`）

核心的一部分（不是綁定）：只在 `GYO::RenderBackendSDLGPU` 與 `GYO::RenderBackendSDL` 都存在時建，比照
`GYO::ToolFrameworkWindow`；`tests/framework/CMakeLists.txt` 加「有這兩個 Engine target 卻沒有本 target 就 FATAL_ERROR」。
由客戶端建立並持有；`RunWindowShell`／`RunImGuiShell` 不碰它（Replacement Criteria「外殼不替客戶端建立或選擇 render device」）。

標頭都在 `framework/include/gyo/framework/`，namespace `Gyo::Tools::Framework`：

| 標頭 | 內容 | 接合的 Engine 能力（檢查 2） | 兩個使用者 |
|---|---|---|---|
| `Preview.hpp` | `PreviewOptions`（shader bundle 路徑、初始大小、driver，都由呼叫端給）；`OffscreenPreview::Create(platform, options)`；`BeginFrame(width, height, camera, clearColor)` → `RenderQueue*`（包在 `Result` 裡）；`EndFrame()` → `TextureReadback`；`Device()` → 畫面外裝置（客戶端用它建 mesh／貼圖） | `ShaderLibrary::AppendBundle`（經 `NativeFileAssetSource`＋`AssetPathResolver`）＋ `SdlGpuRenderDevice::CreateOffscreen` ＋ `Renderer::Initialize`；每幀 `SetFrameSize` ＋ `RenderQueue::Reset`／`SetCamera` ＋ `Renderer::Render` ＋ `ReadFrame`；解構時 `Renderer::Reset` 先於裝置 | model_viewer（骨骼動畫預覽）、voxel_map_editor（區塊 mesh 預覽，`v8-tool-voxel-map-editor-window`） |
| `PreviewTexture.hpp` | `PreviewTexture`（句柄＋尺寸）；`UploadFrame(SdlRenderer&, PreviewTexture&, const TextureReadback&)` | `SdlRenderer::ReleaseTexture`／`CreateStreamingTexture`／`UploadTexture`：依幀尺寸決定建、換或沿用，再上傳 | 同上（兩者都在 ImGui 綁定的 `SdlRenderer` 上顯示） |
| `PreviewCamera.hpp` | `OrbitCamera`（target、yaw、pitch、distance、fov）→ `Camera()` 得 `PerspectiveCamera3D` | 把繞點相機的參數改接成 Engine 的相機約定（Z→Y→X 的 Euler、+Z 向前；near／far 由 distance 推） | model_viewer；voxel 編輯器可用可不用（**見 D-pv-1**） |

- 元件的 API 以「開一幀（給尺寸與 `PerspectiveCamera3D`）→ 客戶端拿 `RenderQueue&` 自己提交 → 結束一幀並讀回」為中心。
  客戶端拿到的是 Engine 的 `RenderQueue`／`IRenderDevice`，提交介面是 Engine 的（`MeshSubmission`、`MaterialDesc`），
  元件不加自己的旗標、不知道畫的是什麼（DI 3）。
- 錯誤型別：`FrameworkError.hpp` 加 `PreviewErrorCode { StartFailed, DrawFailed, UploadFailed }`（`PreviewError`）；
  Engine 的原因放 detail（`Base::CauseDetail`）。三個碼都有測試會斷言，不多加。
- 「門面」逐一檢查（Replacement Criteria）：`Create` 接合 3 個 Engine 呼叫；`BeginFrame` 接合 3 個；`EndFrame` 接合 2 個；
  `UploadFrame` 接合 3 個並帶尺寸判斷；`OrbitCamera::Camera` 是改接；`Device()` 是對持有物的存取，不是呼叫。
  原本 model_viewer 的 `Resize` 只轉呼叫 `SetFrameSize`，是門面，**不搬**（尺寸改成每幀由 `BeginFrame` 帶，見 D-pv-2）。

### 留在 model_viewer

`ModelRenderer` 的資源與實例、骨架細盒、`FrameModel`（依賴 `ModelAsset`）、`PreviewOptions.skeleton`、
`ModelViewerErrorCode::PreviewFailed`。model_viewer 的 `PreviewRenderer` 改成「持有 framework 的 `OffscreenPreview`，
每幀 `BeginFrame` → 提交模型與骨架 → `EndFrame`」。搬走的 `OrbitCamera`、`PreviewTexture`、`UploadFrame` 從 model_viewer
刪除，不留相容層；`ViewerWindow`、`PreviewMeasure`、smoke 測試改用 `Framework::` 的。

### 不做

- 不改 Engine（Render、SDL GPU、SDL renderer）；Engine 缺口檢查已過，沒有要補的。
- 不為 voxel_map_editor 寫任何程式（它的 window 批在它自己的線）；不改 PR #152 的文件。
- 不改 Gateway、pv10、pvp；不動 ui_editor 的 `*Native` 例外；不做 `FileErrorCode` 搬家等 framework 無關的整理。
- 不改 framework 的 DI 1～6 條文；`docs/framework/architecture.md` 只在 tool part 的清單與「Using it」加用法。
  model_viewer 的 DI 條文也不改，只改 `docs/model_viewer/architecture.md`「Window」節對 `PreviewRenderer`／`UploadFrame` 的描述。

## 待決（請在核准宣告時一併決定；每項都有建議）

| 代號 | 問題 | 建議 | 另一個選項 |
|---|---|---|---|
| D-pv-1 | `OrbitCamera` 今天只有 model_viewer 用；voxel 編輯器說相機要能直接給 `PerspectiveCamera3D`。搬到 framework 嗎？ | **搬**（照架構師的委託與 `v8-tool-voxel-layers`）：它不是 Engine 的門面（Engine 沒有繞點相機），是改接；voxel 編輯器要做轉盤式預覽時就是第二個使用者。契約文件寫明「今天一個使用者」，健檢時重看 | 留在 model_viewer；voxel 需要時再搬（代價：到時再動 framework 的公開介面一次） |
| D-pv-2 | 幀尺寸：元件要不要有 `Resize`？ | **不要**。`BeginFrame(width, height, …)` 每幀帶尺寸，元件每幀呼叫 `SetFrameSize`（Engine 那邊只記下請求值，尺寸沒變就不重建，所以元件不必保存尺寸的副本、不必判斷有沒有變，符合 AGENTS §3「狀態的權威」）。兩個工具都省掉「想要的尺寸 vs. 現在的尺寸」那段簿記 | 保留 `Resize`（它只轉呼叫一個 Engine 函式，是 Replacement Criteria 的門面；兩個就觸發條件） |
| D-pv-3 | `Device()` 回傳什麼？ | **`SdlGpuRenderDevice&`**（標頭只前置宣告；客戶端要 `IRenderDevice&` 時直接用，它是基底）。model_viewer 的 smoke 測試用 `Device().ReadFrame()` 核對「交出的幀就是裝置最後一幀」（V2），回傳基底就測不到，等於放寬 V2 | 回傳 `IRenderDevice&`，V2 的核對改寫（放寬，要走 §14.4） |
| D-pv-4 | 守衛怎麼放寬？ | **(b) 核心的 CMake 連結改成每個 target 一張清單**：`gyo_tool_framework` 只能連今天那幾個；`gyo_tool_framework_window` 只能連核心＋`GYO::PlatformSDL`；`gyo_tool_framework_preview` 才能連 `GYO::Render`、`GYO::RenderBackendSDLGPU`、`GYO::RenderBackendSDL`。文字裡（註解、`if(TARGET …)`）可以提到這些名字。這樣既有突變 `fw-shell-core-render`（「視窗外殼連了 render device」）仍殺得掉，DI 6「外殼不建 render device」繼續有守衛 | (a) 只把四個名字加進核心那一張清單：最小改動，但 `fw-shell-core-render` 變成等價突變（殺不掉），「視窗外殼不連 render device」只剩審查在守，等於多放寬一面 |

D-pv-4 不論選哪個，§14.4 的 1～5 在實作後、突變與另一位檢證者（Opus 5.5／high，唯讀）驗證後再正式交你核准（關卡 2）。
下面「守衛變更」先寫預計的內容。

## Architecture Delta（AGENTS.md §3 的 7 項）

1. **需要它的 Feature**：voxel 地圖編輯器的 3D 預覽（`v8-tool-voxel-map-editor-window`）與 model_viewer 既有的預覽。
2. **現在的問題**：「畫面外裝置＋Renderer＋讀回＋上傳」這段組合住在 model_viewer 裡；第二個工具要用就得複製一份
   （DI 5：兩個產品以同樣方式組合 Engine 能力 → 進 framework）。
3. **變化的邊界**：framework 核心多一個 target（`GYO::ToolFrameworkPreview`）與三個公開標頭；model_viewer 的
   `PreviewRenderer` 縮成模型專屬的部分。
4. **影響的 Module／Product／Tool**：`framework/`、`tests/framework/`、`tools/model_viewer/`、`tests/model_viewer/`、
   `docs/framework/`、`docs/model_viewer/architecture.md`（Window 節）、`docs/tools/plans/v8/scripts/model_viewer_mutations.json`。
5. **Dependency Direction**：model_viewer → framework（preview）→ Engine。framework 核心第一次連 render 的 target
   （`GYO::Render`、`GYO::RenderBackendSDLGPU`、`GYO::RenderBackendSDL`，都不是 `*Native`，DI 2 允許）。Engine 不變，仍不認識 framework。
6. **Ownership**：畫面外預覽的組合、`OrbitCamera`、`PreviewTexture`／`UploadFrame` 的 owner 從 model_viewer 改為 framework。
7. **為什麼不能更小**：複製進 voxel_map_editor 會有兩份約 150 行的相同組合，同一個錯要修兩次（框架 DI 5 的「相同組合」正是這個）；
   放進 Engine 則讓 Engine 持有「工具預覽」這種組合（Engine 只放能力；上傳那段還跨了兩個 backend）。

## 驗收判定（結果看之前先宣告；不放寬）

試行次數：除了 P1，都是決定性的，**不重試**；失敗就修（§14.3 先 Red 後 Green）或停下回報。

### framework 的新元件（`tests/framework/PreviewTests.cpp`，每條規則一組測試＋相關突變）

| 判定 | 規則 | 標籤 | 突變（`scripts/framework_preview_mutations.json`） |
|---|---|---|---|
| PV1 | `Create`：bundle 路徑錯 → `StartFailed`，detail 有 Engine 的原因；路徑對 → 成功 | `gpu;editor`（本機 Metal） | `pv-create-ignores-bundle-error` |
| PV2 | 每幀各自的：畫一個紅色正方形，再把相機轉開，第二幀中央不是紅的；每幀交出的 bytes 和 `Device().ReadFrame()` 的相同 | `gpu;editor` | `pv-stale-frame`（`EndFrame` 交出第一次讀回的）、`pv-no-render`（`EndFrame` 不呼叫 `Renderer::Render`）、`pv-queue-not-reset`（`BeginFrame` 不清上一幀的提交） |
| PV3 | 尺寸跟著 `BeginFrame`：給 40×30 → 幀是 40×30；再給 61×45 → 61×45 | `gpu;editor` | `pv-size-ignored` |
| PV4 | 相機是 `BeginFrame` 給的：正對時正方形在中央，轉開時中央是背景 | `gpu;editor` | `pv-camera-ignored` |
| PV5 | `UploadFrame`：第一次建貼圖；同尺寸沿用同一個句柄；尺寸變了釋放舊的、建新的；貼圖畫出來的像素和幀逐一相同（測試經 `*Native` 讀回，比照 ImGui 綁定的 T2） | `cpu;editor`（dummy video、software renderer，L1） | `pv-upload-keeps-old-size`、`pv-upload-skipped`、`pv-upload-new-every-call` |
| PV6 | `OrbitCamera::Camera`：位置＝target − forward·distance；pitch 正值往下看；near／far 隨 distance | `cpu;editor`（L1） | `pv-orbit-pitch-sign`、`pv-orbit-distance-sign`、`pv-orbit-near-clip` |
| PV7 | 守衛：元件的檔案不具名任何登錄的產品 id；核心規則不變（`SDL3/`、`*Native`、`SDL_`、imgui、nlohmann 仍失敗）；本 target 有它的 Engine target 卻沒建時 configure 失敗 | `cpu;editor`（L1） | `fw-guard-preview-target-dropped`、`fw-guard-preview-native-listed`、`fw-link-preview-missing`、`fw-link-preview-native` |

- 既有 framework 測試與守衛全綠；全量 CTest（`cpu|shader`）全綠。
- 既有突變裡會變等價或錨點被改的（D184 重跑範圍：新的、find 在改動範圍、殺它的測試被改）：
  - `fw-guard-unlisted-engine-target` 用 `GYO::RenderBackendSDLGPU` 當「清單外的 Engine target」的例子，放寬後要改用別的
    （預計 `GYO::AudioBackendSDL`；意圖與預期訊息不變）。
  - `fw-shell-core-render`：選 D-pv-4 (b) 時預期訊息改成「`gyo_tool_framework_window` links `GYO::RenderBackendSDL`」；選 (a) 時變等價，要刪（是放寬，寫進 §14.4）。
  - `mvp-stale-frame`（錨在搬走的程式）改錨到 framework 成 `pv-stale-frame`；model_viewer 的 smoke 仍核對 V2，性質沒有少守。
  - 其餘 find 落在 `framework/CMakeLists.txt`、`tests/framework/framework_dependency_guard.py`、`tools/model_viewer/CMakeLists.txt`、
    `tests/model_viewer/model_viewer_dependency_guard.py` 的既有突變，實作後逐條預檢（find 恰好一次）並重跑。
- 新突變與重跑的全部要 killed、stale 0；預檢：每條 find 恰好出現一次。

### model_viewer（既有判定不改）

- `gyo_model_viewer.core`、`.cli`、`.dependency_guard`、`.dependency_guard_selftest`（`cpu`）通過；`gyo_model_viewer.preview`（smoke，`gpu`）
  在本機通過。smoke 的 V2／V3 判定不變（紅色正方形、轉開、`SameAsDevice`、`TextureHolds`、新尺寸、無模型、骨架）；
  只把 `Resize(w, h)` 改成把尺寸交給 `Render`，型別改成 `Framework::` 的。
- 守衛正向清單加 `GYO::ToolFrameworkPreview`；model_viewer 不再具名的 target（預計 `GYO::RenderBackendSDLGPU`）從清單拿掉（收緊，事後報告）。
- `model_viewer_mutations.json` 的 `mvp-*`、`mv-guard-*` 重跑（D184）。

### P1 重跑（model_viewer 計畫的原宣告，門檻不放寬）

- 命令：`gyo_model_viewer measure-preview --asset-root assets/object_fps_pvp --csv <evidence>/p1-frames.csv`。
- PvP catalog 的 4 個 `model` 資產，各播最長的片段 600 幀（沒有片段的用預設姿勢，和 #146 相同），1280×720，Metal。
- 量的範圍：每幀「取樣＋`ModelRenderer` 更新＋`BeginFrame`＋提交＋`EndFrame`（含等 GPU 的讀回）＋`UploadFrame`」，和 #146 同一段。
- **門檻：每個模型的 p95 ≤ 16.7 ms**。試行 1 次，不重試。件數突き合わせ：每個模型 600 筆，少了就是「不一致」。
- 建置 RelWithDebInfo（和 #146 的量測相同，結果可比；不比 Release 快）。
- 結果和 #146 的數值並列（[model-viewer-trial.md](../../../tools/plans/v8/model-viewer-trial.md)：p95 5.957／4.726／5.214／4.150 ms）。
  只看門檻；和 #146 的差異寫出來，不當判定。
- 不過 → 不放寬，停下照 model_viewer 的 Replacement Criteria 1 回報。

### 守衛變更（`v8-framework-preview-guard`，AGENTS.md §14.4，預計內容；關卡 2 交核准）

1. **守的性質**：framework 核心的 CMake 只能具名正向清單裡的 target；新的 Engine 依賴邊要先寫進守衛才過（每條邊都有紀錄）。
2. **放寬面**（以前失敗、以後通過）：核心的 CMake 文字出現 `GYO::Render`、`GYO::RenderBackendSDLGPU`、`GYO::RenderBackendSDL`、
   `GYO::ToolFrameworkPreview`；選 (b) 時再加上「`gyo_tool_framework_preview` 的連結行連這三個 Engine target」。
3. **收緊面**（以前通過、以後失敗）：選 (b) 時，`gyo_tool_framework` 連 `GYO::PlatformSDL`、`gyo_tool_framework_window` 連任何
   render target、任何核心 target 連別的核心 target 以外的 framework target，都失敗（以前只要名字在核心清單裡就過）。選 (a) 時沒有收緊面。
4. **其他判定是否仍守**：`SDL3/`、`*Native`、`SDL_`、imgui、nlohmann 的規則不動；configure 時的連結檢查不動（它看的是 SDL3／imgui／nlohmann／`*Native`，
   不看 Engine target，所以本批不放寬它，只加「有 Engine target 卻沒建 preview target 就失敗」）。「視窗外殼不建 render device」
   在 (b) 由每 target 清單＋`fw-shell-core-render` 守；在 (a) 只剩審查。
5. **根據與證據**：守衛自測的 Red／Green、PV7 的突變、檢證者（Opus 5.5／high，唯讀）對 1～4 的確認（新舊守衛各跑合成輸入比對），
   都存到證據目錄。

## 證據位置

使用者的 Mac，主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/framework-preview/`（git 不追蹤）：
configure／build／ctest 的輸出、守衛的 Red／Green、突變輸出（`mutations_*.txt`）、`verifier/`、P1 的 `p1-frames.csv`／`p1-summary.txt`／`commit.txt`。
審査者與検証者的原始回報在會話暫存區，未保存；結論寫進 HANDOFF 與 dev_log。
文件只寫結論與判定用的數值。

## 模型／檔位（`agents/effort-gearbox.md`、`agents/navigator.md`）

上段（framework 公開介面、Design Invariants、守衛正向清單）。

| 角色 | 模型／檔位 | 備注 |
|---|---|---|
| 實作者 | Opus 5.5／high（主會話） | 計畫表的指定；不需要比主會話更強的模型 |
| 審査者 | Opus 5.5／high（子 Agent，唯讀） | code review：Architecture Drift、門面、DI |
| 検証者（§14.4） | Opus 5.5／high（子 Agent，唯讀） | 用合成輸入比對新舊守衛，確認 1～4 |

## 估時（開工前）

| 步驟 | 估 |
|---|---|
| 事前宣告（本頁、HANDOFF） | 1 h |
| 元件、測試 PV1～PV7、突變目錄 | 3～4 h |
| 守衛變更、自測、突變、検証者、§14.4 的 1～5 | 1～1.5 h |
| model_viewer 改用、smoke、P1 重跑 | 1～1.5 h |
| 契約文件、architecture.md、dev_log、審査、PR | 1 h |
| 合計 | 7～9 h（計畫表 6～10 h；超過 1.5 倍＝13.5 h，或單一問題累計 >2 h 就停下回報） |

## 關卡

1. 事前宣告核准（本頁與 D-pv-1～4）。核准前不動程式碼。
2. 守衛變更的 §14.4 核准（實作、突變、検証者之後）。核准前不合併、不開 PR 的合併。
3. PR 開出 → 通知 voxel 會話與架構師會話；合併由使用者決定（repo 不允許 auto-merge）。

## 完成

| 批 | 日期 | dev_log |
|---|---|---|
| `v8-framework-preview-component` | 2026-11-06 | [3D 預覽元件](../../../dev_logs/2026_11_05_framework_v8_preview_component.zh-Hant.md) |
