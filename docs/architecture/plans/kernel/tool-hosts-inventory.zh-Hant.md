# 四個工具 host 骨架的盤點：`v8-arch-tool-hosts-inventory`

更新：2026-11-13。Owner：架構（架構師會話開出的任務卡；本卡會話 Fable／medium，讀碼與對照 high）。**只讀；不改程式、不改守衛、不改契約。**
給 `v8-arch-plugins-editor`（遷移計畫 §3.24）與工具四批（§3.25～3.28）的事前宣告當起點。切法一節（§8）是**候選，不是決定**；決定在 plugins-editor 批。

對象：`tools/ui_editor`、`tools/voxel_map_editor`、`tools/model_viewer`、`tools/audio_editor` 的 host 骨架，以及它們直接用的 `framework/` 型別。程式以 master `d0d8563f` 為準。

## 0. 件数の突き合わせ與判定（看結果前宣告）

事前宣告（判定與數字）在開始後、掃描前以 `send_message` 送 parent，回「session parent not found」（`list_sessions linked:true` 回空，與 `v8-arch-guards-inventory` 同一情形），所以未經 parent 核定就寫了正文；請使用者在本會話核定，核定後補記於此。

| # | 判定（宣告） | 結果 |
|---|---|---|
| 1 | 覆蓋：骨架 10 項 × 4 工具 ＝ 40 格，每格寫檔與函式或「無此功能」；空格 0 才成立 | **成立**：40／40（§2 的十個表，每表 4 列；「無此功能」8 格：⑦ 設定與最近檔 4、④ 檔案對話 voxel、⑤ 預覽 readback audio、⑥ 資產掛載 voxel、⑧ 關窗提示 audio；⑩ 的「不起執行緒」4 列有交換點與時鐘的答案，不算空格） |
| 2 | 依賴邊：每工具一列，(a) CMake `target_link_libraries` 命名的外部 target 去重數，(b) `#include` 映射出的 target 數；兩者附命令與命中數；(a)＝(b) 且集合相同＝一致，否則逐個 target 解釋才算「不一致（已解釋）」；掃描前正例先命中 | **件数の突き合わせ：不一致（已解釋）**——四工具都是 (b) ＞ (a)：ui_editor 15／13、voxel 14／12、model_viewer 18／13、audio 12／10；多出的 11 個 target 全部是「include 到、但經 framework 或 `GYO::Engine` 的 PUBLIC 連結傳遞而未在工具 CMake 直接命名」，反向（連了但沒 include）0 個（§4）。正例：framework ImGui binding 的 include 5 個目標、CMake 3 個目標先命中；第一次 CMake 正例**未命中**（註解裡的右括號截斷了 `target_link_libraries` 的引數），修正掃描器後才命中（§4.0） |
| 3 | 零修改：`git diff --stat origin/master` 恰 4 檔（本報告、dev_log、kernel README 一行、HANDOFF 一行） | **成立**（dev_log 列實際輸出） |

## 1. 讀法與範圍

- **骨架 10 項**（卡片列的）：① 行程入口 ② 視窗與事件 pump ③ ImGui 接法 ④ 檔案對話與選擇窗 ⑤ 預覽 readback ⑥ 資產掛載 ⑦ 設定與最近檔 ⑧ 關窗提示 ⑨ 錯誤顯示 ⑩ 交換點／執行緒。
- **共用程度**的用語：「四同」＝四個工具用同一份程式（framework 型別）或逐字相同；「三同一異」；「各自一份」＝同一件事各寫一份，內容不同（§7 另列）；「無」＝該工具沒有這個功能。
- 讀了哪些檔：四工具的 `Main.cpp`、視窗 client（`EditorWindow.cpp` ×2、`ViewerWindow.cpp`、`AudioEditorWindow.cpp`）、`*Unavailable.cpp`、`FileAccess.*`、`PreviewMeasure.cpp` ×2、`PreviewScene.*`、`PreviewRenderer.*`、`PreviewSounds.cpp`、`PreviewPlayer.hpp`、ui_editor 的 `EditorApp.hpp`、`EditorAppDialogs.cpp`、`PathDialog.*`、`SdlPathDialogs.*`、`AssetPreviewContext.hpp`（.cpp 只讀 Initialize／Mount／Upload 部分）、`PreviewAdapter.hpp`，與 `EditorApp.cpp` 的 host 相關函式（Initialize、ApplyPendingAssetChanges、ApplyPathDialogResults、RequestAssetMount、CloseRequested、DrawStatusBar、DrawMainMenu 的 Quit）；framework 的 `WindowShell.*`、`HeadlessShell.*`、`ImGuiShell.*`、`Preview.*`、`ReadOnlyAssetCatalog.hpp`、三個 `CMakeLists.txt`；四工具的 `CMakeLists.txt`、`project.json`；四支工具守衛與 framework 守衛的清單常數；`GyoComponents.cmake`、`GyoBuild.cmake`、`ToolRegistryTests.cmake` 的對應行。
- 行數（`wc -l`）：視窗 client ui_editor 104（另 `EditorApp*.cpp` 約 2,400）、voxel 1,135、model_viewer 1,024、audio 579；`Main.cpp` 108／371／206／158；framework `WindowShell.cpp` 69、`HeadlessShell.cpp` 20、`ImGuiShell.cpp` 492、`Preview.cpp` 113。

四工具今天都站在同一條 framework 鏈上：`main` → `RunHeadlessShell` → 工具的 `Run` → `RunXxxWindow`／`RunEditorGui` → `RunImGuiShell` → `RunWindowShell`（`RuntimeLoop` ＋ `SdlPlatform::PumpEvents`）→ `ImGuiWindowClient`（ImGui context、兩個 SDL 後端、字型、文字輸入）→ 工具的 `ImGuiShellClient`。

## 2. 骨架逐項

### ① 行程入口

| 工具 | 怎麼做（檔：函式） | 用的 framework 型別 |
|---|---|---|
| ui_editor | `Main.cpp:101-107` `main` → `RunHeadlessShell(argc, argv, Run, <lambda：印 reason＋CommandLineHelp()，回 2>)`；`Run`（`:76-97`）用 `ParseCommandLine` 得 `EditorMode` {Help, Validate, Gui}，Gui → `RunEditorGui`（`EditorWindow.cpp:93-102`：`WindowShellOptions` 標題「GYO UI Editor」1600×900 可調 → `RunImGuiShell`，factory 建 `EditorClient`） | `HeadlessShell`、`WindowShellOptions`、`RunImGuiShell` |
| voxel_map_editor | `Main.cpp:369-371` `main` → `RunHeadlessShell(argc, argv, Run, Usage)`；`Run`（`:259-365`）字串比對 13 個子命令（build／verify／inspect／import-boxes／edit／measure-preview／build-map／verify-map／inspect-map／check-markers／measure-map／--help），`edit` → `RunEditorWindow`（`window/EditorWindow.cpp:1124-1133`：「GYO Voxel Map Editor」1500×900） | 同上 |
| model_viewer | `Main.cpp:202-206` → `RunHeadlessShell(argc, argv, Run, Usage)`；`Run`（`:142-198`）5 個子命令（models／info／roundtrip／window／measure-preview）＋`Options` 小解析器；`window` → `RunViewerWindow`（`ViewerWindow.cpp:1013-1022`：「GYO Model Viewer」1400×860） | 同上 |
| audio_editor | `Main.cpp:154-158` → `RunHeadlessShell(argc, argv, Run, Usage)`；`Run`（`:132-150`）3 個子命令（info／convert／edit）；`edit` → `RunAudioEditorGui`（`AudioEditorWindow.cpp:568-577`：「GYO Audio Editor」1200×820） | 同上 |

- **共用程度**：`main` 一行進 `RunHeadlessShell` **四同**（framework）；`RunXxxWindow` 的 10 行（選項＋`RunImGuiShell`＋factory）**四同形、各自一份**（只差標題與尺寸）；子命令分派與 `Usage` 文字**各自一份**（這是產品知識，§6）；失敗函式：三個傳 `Usage`，ui_editor 傳 lambda（三同一異）。
- **CLI 變體的存根** `*Unavailable.cpp` **四同形、各自一份**：ui_editor `EditorGuiUnavailable.cpp` 回 3、voxel `EditorWindowUnavailable.cpp` 回 2（兩個函式）、model_viewer `ViewerWindowUnavailable.cpp` 回 3（兩個函式）、audio `AudioEditorGuiUnavailable.cpp` 回 2。**退出碼不一致**（3／2／3／2）、訊息各寫一遍——同一件事四種寫法的小例子。
- CMake：gui 變體以 `if(GYO_TOOL_VARIANT STREQUAL "gui")` 加視窗原始檔與 target 存在檢查（`foreach(_target IN ITEMS …) if(NOT TARGET …) FATAL_ERROR`），四份結構相同、清單不同（§4）。

### ② 視窗與事件 pump

| 工具 | 怎麼做 | 用的 framework 型別 |
|---|---|---|
| ui_editor | 自己不寫 pump。`EditorClient : ImGuiShellClient`（`EditorWindow.cpp:31-89`）；事件經 framework 的 `OnEvent` 進 `SdlInput`＋ImGui 後端，client 只在 `BeginFrame` 讀 `input_->Frame()`（`:61`） | `RunWindowShell`（`WindowShell.cpp:44-67`：`SdlPlatform::Create` → factory → `LoopAdapter` → `RuntimeLoop::Run`；`ProcessEvents` ＝ `PumpEvents` 帶 `OnEvent` 觀察者，Stop 且 client 不同意關就 Continue `:28-33`）；`ImGuiWindowClient::OnEvent`（`ImGuiShell.cpp:403-406`） |
| voxel_map_editor | 同上，`EditorClient`（`window/EditorWindow.cpp:79-1120`）；不讀 `ImGuiShellInput`，輸入全走 `ImGui::GetIO()` | 同上 |
| model_viewer | 同上，`ViewerClient`（`ViewerWindow.cpp:64-1009`）；輸入全走 ImGui | 同上 |
| audio_editor | 同上，`AudioEditorClient`（`AudioEditorWindow.cpp:79-564`）；輸入全走 ImGui | 同上 |

- **共用程度**：**四同**（framework 一份）。沒有一個工具自己 pump、自己建 `RuntimeLoop` 或設 live-frame handler（framework DI 6 不設；工具線 HANDOFF 記過 macOS 拖邊時畫面停住）。
- **例外（2／4 自寫 Host 形狀）**：量測模式自己開平台、自己 `PumpEvents`、不經 shell：model_viewer `PreviewMeasure.cpp:37-42、:96`（`SdlPlatform::Create`＋`SdlRenderer::Create`＋每幀 `PumpEvents()`）、voxel `window/PreviewMeasure.cpp:120、:205、:302`。兩份結構相同（開平台 → 開預覽 → N 幀計時 → 寫 CSV），內容不同（量的東西不同）。
- 關窗請求的流程在這一層：`PumpEvents` 回 Stop → `LoopAdapter::ProcessEvents` 問 `CloseRequested()` → `ImGuiWindowClient::CloseRequested` 轉問工具 client（`ImGuiShell.cpp:449`）。工具端的回答見 ⑧。

### ③ ImGui 接法

| 工具 | 怎麼做 | 用的 framework 型別 |
|---|---|---|
| ui_editor | client 實作 `Initialize／BeginFrame／Draw／WantsExit／CloseRequested／EndFrame`（`EditorWindow.cpp:37-82`）；`Draw` → `EditorApp::Draw`：**五個固定位置的 ImGui 視窗**（Hierarchy／Canvas／Inspector／Diagnostics & Action Log／Status，`EditorApp.cpp:238-267` 以 `GetMainViewport()` 算位置）＋主選單；**唯一用 `ImGuiShellInput` 的工具**：`BeginFrame` 設 `SetPhysicalInput(&input_->Frame())`，`Draw` 後 `TakeTextInputRequest` → `input_->RequestTextInput(rect, cursor)`（`:61、:68-72`）；`ImGui::` 呼叫 430 處 | `ImGuiShellClient`、`ImGuiShellContext`、`ImGuiShellInput`；context／兩個 SDL 後端／系統 CJK 字型／`IniFilename=nullptr`／幀順序（`input.EndFrame→Take→BeginFrame→client.BeginFrame→NewFrame×3→client.Draw→Composition.Draw→(WantsExit)→Render→Apply 文字輸入→Clear→RenderDrawData→Present→client.EndFrame`，`ImGuiShell.cpp:408-447`）全在 framework |
| voxel_map_editor | 同一組虛函式（`EditorWindow.cpp:91-150`）；`Draw`：**一個全視窗主窗**「Voxel Map Editor」`NoDecoration|NoMove|NoSavedSettings|NoNavInputs`（`:108-113`），左側欄 380 px 可拉＋右側預覽；`InputString`（`:60-70`，`std::string` 版 InputText）；ImGui 176 處 | 同上；不用 `ImGuiShellInput` |
| model_viewer | 同一組虛函式（`ViewerWindow.cpp:79-167`）；`Draw`：一個全視窗主窗「Model Viewer」`NoDecoration|NoMove|NoSavedSettings`（`:141-145`），左側欄 360 px 可拉；`InputString`（`:50-60`，**與 voxel 逐字相同**）；ImGui 139 處 | 同上；不用 `ImGuiShellInput` |
| audio_editor | 同一組虛函式（`AudioEditorWindow.cpp:83-148`，無 `CloseRequested`／`EndFrame` 覆寫）；`Draw`：一個全視窗主窗「Audio Editor」`NoDecoration|NoMove|NoSavedSettings|MenuBar|NoBringToFrontOnFocus`（`:121-127`）；文字欄用 `std::array<char,1024>` ＋ `SetField／Field`（`:44-57`）；ImGui 87 處 | 同上；不用 `ImGuiShellInput` |

- **共用程度**：ImGui 的生命週期、後端、字型、side file 禁止、幀順序 **四同**（framework）。工具端：全視窗單一主窗 **三同一異**（ui_editor 五窗）；主窗旗標三份各不同；`InputString` 2／4 逐字重複；字串欄位的做法三種（`std::string`＋resize 回呼、`std::array<char,1024>`＋helper、`std::array<char,1024>` 直接 `InputText`）。
- 目標架構對照：runtime-composition §3b 要 ImGui plugin 拆兩半（文件 World 的輸入與 widget System／渲染 World 的 draw list 繪製）；今天「一個函式從 NewFrame 到 Present」就是 `ImGuiWindowClient::Update`＋`Render`，四工具都靠它，沒有一個工具碰 context 或後端（`imgui_impl_*` 只在 framework，守衛禁止）。

### ④ 檔案對話與選擇窗

| 工具 | 怎麼做 | 用的 framework／Engine 型別 |
|---|---|---|
| ui_editor | 三層：介面 `PathDialogs`（`PathDialog.hpp:40-49`：Show／TakeResults／Abandon，可為 null → 手打路徑）；記帳 `PathDialogRequests`（`PathDialog.cpp:5-21`：`++lastTag_`、`waiting_` 用途、`Accept` 只認最新 tag、`Failed` 不結束、`Abandon`）；平台轉接 `SdlPathDialogs`（`SdlPathDialogs.cpp:62-78`：`PathDialogRequestFor` 把 5 個用途（OpenDocument／ExportDocument／AssetFolder／AssetRootField／CatalogField）換成 `PathDialogRequest`＋過濾器）。回覆在 `BeginFrame` 經 `EditorApp::ApplyPathDialogResults`（`EditorApp.cpp:126-171`）套用；`pathDialogOpen_` 鎖住選單項、狀態列「Stop waiting」→ `Abandon`（`:406-417`）。**CCR-1 合規** | Engine `SdlPlatform::ShowPathDialog／TakePathDialogAnswers`（platform-sdl DI 10）；framework 無此型別 |
| voxel_map_editor | **無此功能**：路徑只來自 CLI（`edit <grid> <map-dir>`），視窗內無任何系統對話；存檔走 Ctrl+S／按鈕 | — |
| model_viewer | 寫在 client 內：`DrawContent`（`ViewerWindow.cpp:621-627`）送 `PathDialogRequest{tag = kContentDialog(常數 1), OpenFolder}`；`BeginFrame`（`:112-119`）取全部回覆、只看 `tag == kContentDialog`、Chosen → `Guarded(MountContent)`、Failed → 錯誤字串。**無等待狀態、無 Abandon**；兩次點擊送兩個請求、每個回覆都套用。**CCR-1 不合規**（登記頁已記；改造期間不修） | Engine 同上 |
| audio_editor | 寫在 client 內：`ShowDialog`（`AudioEditorWindow.cpp:229-256`：`++lastTag_`、`dialog_ = purpose`、5 個用途 OpenSource／OpenProject／SaveProject／AssetRoot／Catalog）；`ApplyDialogAnswers`（`:262-290`，`BeginFrame` 呼叫）：只認 `dialog_` 有值且 `tag == lastTag_`、Failed 只寫狀態不結束、Chosen／Cancelled 結束；「Stop waiting」按鈕 `dialog_.reset()`（`:136-140`）。**CCR-1 合規** | Engine 同上 |

- **共用程度**：3／4 有，**三種實作**（audit framework-tools 觀察 5；CCR-1 已登記）：記帳規則相同（CCR-1）但型別、放置、過濾器各一份；用途列舉與過濾字串是產品知識（§6）。voxel 無。
- 搬進共用 plugin 時可共用的是：tag 配發與「只認最新」記帳、`Failed` 不結束、Abandon、「Stop waiting」的 UI 片段、回覆在幀邊界套用的時點；留在工具的是用途列舉、過濾器、回覆之後做什麼。

### ⑤ 預覽 readback

| 工具 | 怎麼做 | 用的 framework 型別 |
|---|---|---|
| ui_editor | **沒有 3D readback**。兩條別的路：(a) `AssetPreviewContext`（`AssetPreviewContext.cpp:155-205`）在 `Initialize` 收 **原生 `SDL_Renderer*`**（`EditorWindow.cpp:41` 經 `NativeRenderer(context.renderer)`），解碼 catalog 資產後 `SDL_CreateTexture／SDL_UpdateTexture` 成視窗 renderer 的貼圖（`:46-67`），LRU 256 在 `BeginFrame` 修剪；是四工具中**唯一直接 include `SDL3/SDL.h` 的檔**（`:4`，ui_toolchain DI 1 的「唯一原生用法」）。(b) `PreviewAdapter` 用 `ImDrawList` 畫 `GYO::Ui` 的評估結果（`PreviewAdapter.hpp:21-23`），每幀 `UiRuntime::Update`。都在 ImGui 的 SDL renderer 上，無離屏裝置 | `ImGuiShellContext::renderer`（借用的 renderer）；Engine `RenderBackendSDLNative`、`AssetSdlImage`、`TextBackendSDLTTF`、`AssetManager`（`:104`） |
| voxel_map_editor | `PreviewScene`（`window/PreviewScene.cpp`）包 `OffscreenPreview`：`Create` 給著色器路徑 `BaseDirectory()/shaders/builtin` 與 `--gpu-driver`（`EditorWindow.cpp:95-103`）；每幀 `DrawPreview`（`:564-624`）：算面板尺寸×`DisplayFramebufferScale`（`:579-582`）→ `scene_->Render`（`BeginFrame`→逐 chunk `Submit`→`EndFrame` 回 `TextureReadback`，`PreviewScene.cpp:97-118`）→ `Framework::UploadFrame(renderer_, texture_, frame)` → `ImGuiTextureId` → `ImGui::Image`（`:595-603`）→ 同一幀內 Fly／Pick／Navigate／Edit／Overlay；解構釋放 `texture_`（`:85`） | `OffscreenPreview`、`UploadFrame`／`PreviewTexture`、`ImGuiTextureId`、`OrbitCamera`（`PreviewCamera.hpp`） |
| model_viewer | `PreviewRenderer`（`PreviewRenderer.cpp`）包 `OffscreenPreview`＋Engine `ModelRenderer`（`:100-121`，另建骨架方塊 mesh）；`Initialize`（`ViewerWindow.cpp:79-97`）同樣用 `BaseDirectory()/shaders/builtin`＋`--gpu-driver`；每幀 `DrawPreview`（`:908-946`）：尺寸算法 **與 voxel 逐字相同**（`:912-915`）→ `preview_->Render`（`UpdatePose`→`BeginFrame`→`Submit`→`EndFrame`，`PreviewRenderer.cpp:155-181`）→ `UploadFrame` → `ImGuiTextureId` → `ImGui::Image` → 滑鼠拖動改相機；解構釋放貼圖（`:70`） | 同上（`OrbitCamera` 經 `PreviewRenderer.hpp:26`） |
| audio_editor | **無此功能**（無 3D 預覽）。波形以 `ImDrawList` 直接畫（`AudioEditorWindow.cpp:376-441`）；「預覽」指試聽（見 ⑩） | — |

- **共用程度**：離屏裝置／Renderer／readback／上傳 **二同**（framework 一份：`Preview.cpp:43-111`、`PreviewTexture.cpp`）；兩個包裝（`PreviewScene`、`PreviewRenderer`）**各自一份、內容不同**（chunk mesh＋palette 貼圖 vs. `ModelRenderer` instance＋骨架）；呼叫端的尺寸計算、上傳、`ImGui::Image`、貼圖釋放 **2／4 逐字相同**；CMake 的 `gyo_<tool>_shaders` 複製 target 2／4 逐字相同（`voxel CMakeLists.txt:74-79`、`model_viewer CMakeLists.txt:67-72`）；ui_editor 一條自己的原生貼圖路；audio 無。
- 目標架構對照：runtime-composition §3b 「readback 消失、預覽視窗＝draw list 的貼圖 id、由渲染 World 解析」。今天的回流點就是 `EndFrame → TextureReadback → UploadFrame → ImGuiTextureId(原生指標)`，兩個工具各一份呼叫、framework 一份機制；preview.md DI 4（PV2／PV5）守的正是這條（guards-inventory §2.10 PV 列）。ui_editor 的原生 `SDL_Texture` 路徑不是 readback，但同樣把「貼圖 id＝原生指標」寫進文件 World 側的程式。

### ⑥ 資產掛載

| 工具 | 怎麼做 | 用的 framework 型別 |
|---|---|---|
| ui_editor | `EditorApp::catalog_`（`EditorApp.hpp:94`）；`Initialize` 從 CLI 掛（`EditorApp.cpp:75-86`：`MountCatalog` → `MountRoot` 或 `Mount(catalog, root)`）；視窗內「Mount Read-Only Content」對話（`EditorAppDialogs.cpp:109-156`：單一 catalog 或整個 root，Browse 走 ④）→ `RequestAssetMount` 排隊（`:184-189`）→ **下一幀邊界** `ApplyPendingAssetChanges`（`:106-124`）掛載並 `previewAssets_.Mount(catalog)`（`:571`）；`AssetPreviewContext` 內部用 Engine `AssetManager`（`AssetPreviewContext.cpp:104`，framework-delete 對帳表 ui_editor 列要歸零的那個）；`--validate` 也掛（`Main.cpp:44-56`）。**唯讀**（ui_toolchain DI 3） | `ReadOnlyAssetCatalog`（`MountRoot／Mount／Unmount／Assets／Find／ParsedCatalog`） |
| voxel_map_editor | **無此功能**：不掛 catalog；開 `gyo.voxel_grid` 檔與 map 資料夾（`OpenEditor(gridPath, mapDirectory)`，`EditorWindow.cpp:158`），存檔經 `SaveEditor`；DI 5「不寫 catalog、產品檔、arena」 | — |
| model_viewer | `ModelContent`（`ModelContent.cpp:37-40`）包 `ReadOnlyAssetCatalog`，`Mount(root, catalog?)`；`Initialize` 從 CLI 掛（`ViewerWindow.cpp:104-106`）、「Open content folder...」對話後 `MountContent`（`:206-223`，先關事件檔、清音效、關 session）；寫新檔後 `RefreshContent` 重掛（`:227-238`）；**寫 catalog** 經 framework `RegisterAsset`（`EventFiles.cpp:86`） | `ReadOnlyAssetCatalog`、`RegisterAsset`（json binding）、`EventSounds`／`BindEventSounds` |
| audio_editor | **不掛**：匯出時以打字的 asset root＋catalog 路徑（`DrawExport`，`AudioEditorWindow.cpp:511-530`）直接 `ExportSound` → `Framework::RegisterAsset`（`SoundExport.cpp:63`）寫入 catalog（audio_editor DI 3：只寫自己登記的那條） | `RegisterAsset`（json binding）、`CatalogPlan`（在 `SoundExport` 內） |

- **共用程度**：唯讀掛載 2／4（同一 framework 型別，包裝各一份：ui_editor 直接持有＋排隊套用、model_viewer 包成 `ModelContent`）；寫入登記 2／4（同一 framework 函式）；voxel 無；掛載時點各異（ui_editor 幀邊界排隊、model_viewer 當場、audio 匯出當場）。
- 目標架構對照：audit §10.1 asset-catalog plugin（Resource＋`Io:file` System）；target/assets 的 `Assets<T>`／`AssetInbox`。ui_editor 的 `AssetManager` 使用是 tool-ui 批（§3.28）的「改走 `Assets<T>`」對象。

### ⑦ 設定與最近檔

| 工具 | 怎麼做 |
|---|---|
| ui_editor | **無此功能**。無設定檔、無最近檔、無視窗狀態存檔（framework DI 4：`IniFilename=nullptr`、`LogFilename=nullptr`，`ImGuiShell.cpp:276-277`）；起始檔、輸出檔、asset root 全部來自 CLI（`CommandLineOptions`）；視窗尺寸寫死（`EditorWindow.cpp:96-97`）；`designSizeLocked_`、`showGrid_` 等是執行期狀態，不存檔（`EditorApp.hpp:117-125` 註解「never saved」） |
| voxel_map_editor | **無此功能**。CLI 給 grid 與 map-dir；`--gpu-driver` 每次指定；尺寸寫死 |
| model_viewer | **無此功能**。CLI 給 `--asset-root／--asset-catalog／--gpu-driver`；尺寸寫死 |
| audio_editor | **無此功能**。CLI 給一個檔（project 或 wav）；專案檔是編輯內容（DI 4），不是工具設定；尺寸寫死 |

- 掃描：`git grep -n -i -P 'recent|imgui\.ini|IniFilename|SDL_GetPrefPath|PrefPath|AppData|XDG' -- tools framework` 只命中 framework 關閉 ini 的兩行（證據 `greps.txt`）。
- **共用程度**：四同（都沒有）。一個 `tool-settings` 候選 plugin 今天 **0 個使用者**（§8）。

### ⑧ 關窗提示

| 工具 | 怎麼做 |
|---|---|
| ui_editor | `EditorClient::CloseRequested` → `EditorApp::CloseRequested`（`EditorApp.cpp:276-280`）：`session_.IsDirty()` 為真就 `showDiscardDialog_ = true` 回 false；下一幀 `DrawDialogs` 開 modal「Discard unsaved changes?」（`EditorAppDialogs.cpp:158-174`）：**兩鍵**「Discard and close」（`wantsExit_ = true`）／「Keep editing」。File ▸ Quit 走同一條（`EditorApp.cpp:317` `wantsExit_ = CloseRequested()`）。另一個提示「Output changed」（存檔目標在外部改過：Overwrite／Cancel，`:176-197`） |
| voxel_map_editor | `CloseRequested`（`EditorWindow.cpp:146-150`）：`session_->Modified()` 就 `openClose_ = true` 回 false；`DrawClosePrompt`（`:1049-1070`）modal「Unsaved map」**三鍵**「Save and close」（存了才 `wantsExit_`）／「Discard」／「Keep editing」；`promptOpen_` 期間 `Shortcuts` 停用（`:252`）。另一個提示「File changed」（Reload from disk／Overwrite／Cancel，`:1025-1047`）。解構子對未存變更寫 Warning log（`:84-89`） |
| model_viewer | `CloseRequested`（`ViewerWindow.cpp:162-167`）：先 `CommitPending()`，`AnyUnsaved()` 就 `Guarded([this]{ wantsExit_ = true; })` 回 false；`Guarded`（`:171-180`）把動作存成 **`std::function` 跨幀 closure** `pendingAction_`、開 modal「Discard unsaved events?」（`:182-204`）**兩鍵**「Discard」（清 history／sounds 後執行 closure）／「Keep editing」。同一機制也守「開別的模型」「關檔」「掛別的 content」（`:115、:635、:774`）。磁碟外部變更只顯示錯誤字串（`ChangedOnDisk`），無提示。解構子寫 Warning log（`:69-77`） |
| audio_editor | **無此功能**：不覆寫 `CloseRequested`（預設回 true，`ImGuiShell.hpp:70`），關窗立即結束；專案無 dirty 追蹤、無提示；File ▸ Quit 直接 `quit_ = true`（`AudioEditorWindow.cpp:356`） |

- **共用程度**：framework 的「關窗先問 client、不同意就繼續跑」**四同**；工具端 3／4 有、**三種實作**（兩鍵／三鍵含存檔／兩鍵加跨幀 closure 守多種動作）；「磁碟上變了」提示 2／4 各一份（voxel 三鍵、ui_editor 兩鍵）；audio 無。這是架構師保留的第二項「必須真的進共用 plugin」。
- 要統一時的行為差異（進 plugins-editor 宣告，§1.3 行為差異）：是否提供「Save and close」；提示是否也守「開別的檔」（model_viewer 守、其他不守）；audio 要不要開始追蹤 dirty。

### ⑨ 錯誤顯示

| 工具 | 怎麼做 |
|---|---|
| ui_editor | 單一狀態列字串 `statusMessage_`（`EditorApp.hpp:104`，`DrawStatusBar` `EditorApp.cpp:400-424`，顯示 Catalog 狀態與 Unsaved／Saved）＋「Diagnostics & Action Log」面板（`diagnostics_`、`actionLog_`）；初始化失敗 `Engine::Base::Log(Error, "ui_editor", …)`（`EditorWindow.cpp:21-28`）；被忽略的對話回覆 `Log(Warning, "ui_editor.dialogs", …)`（`EditorApp.cpp:131`）；`--validate` 走 `std::cerr`。`Base::Log` 呼叫 2 處 |
| voxel_map_editor | 三個字串 `error_`／`status_`／`previewError_`（`EditorWindow.cpp:1088-1090`），紅字 `ImVec4(1, 0.4F, 0.4F, 1)` 在側欄 File 區與預覽上方（`:371-372、:565`）；`Report(error)` 把 `Describe` 寫進 `error_`；`Base::Log(Error, "voxel_map_editor", …)` 只在 Initialize 與解構（2 處） |
| model_viewer | **分區字串**：`contentError_`、`previewError_`、`eventsError_`／`eventsStatus_`、`soundsError_`／`soundsStatus_`、`audioWarning_`、`session_.Error()`，各畫在自己的區塊，紅 `ImVec4(1, 0.4F, 0.4F, 1)`、黃 `(1, 0.8F, 0.3F, 1)`；`Base::Log(Error, "model_viewer", …)` 2 處（預覽／音訊啟動失敗、解構） |
| audio_editor | 單一字串 `status_`（`AudioEditorWindow.cpp:562`，底部 `TextWrapped`）＋ `audioProblem_`、`player_->Problem()` 紅字 `(1.0f, 0.5f, 0.4f, 1.0f)`；`Base::Log(Error, "audio_editor", …)` 1 處（Initialize） |

- **共用程度**：樣式同形（字串成員＋`TextColored` 紅字；初始化失敗才進 `Base::Log`，分類＝工具名）、**各自一份**；紅色常數三份（兩份相同、一份不同）；錯誤型別各工具自己的 `*Error.hpp`（`EditorError`、`VoxelMapError`、`ModelViewerError`、`AudioEditorError`），都以 `Engine::Base::Describe` 轉字串。framework 自己用 `framework.shell`／`framework.imgui` 分類（`WindowShell.cpp:20`、`ImGuiShell.cpp:40`）。

### ⑩ 交換點／執行緒

| 工具 | 自起執行緒 | 用到的交換點（跨執行緒或跨幀的佇列） | 時鐘 |
|---|---|---|---|
| ui_editor | 無 | (a) 對話回覆佇列 `TakePathDialogAnswers`（平台側收 SDL 回呼，幀邊界取出；`SdlPathDialogs.cpp:70`）；(b) 輸入快照 `ShellInput::Take`→`Frame()`（framework，`ImGuiShell.cpp:323-326`）；(c) 排隊的掛載請求（同執行緒、跨幀，`ApplyPendingAssetChanges`） | 無（不讀時鐘） |
| voxel_map_editor | 無 | (a) GPU readback 同步等待（`OffscreenPreview::EndFrame` → `ReadFrame`，`Preview.cpp:105`）；(b) `EditSession::TakeChanges` → `MapPreview::Apply` → `PreviewScene::Apply`（同執行緒、跨幀的變更佇列，`EditorWindow.cpp:210-220`） | ImGui 的 `io.DeltaTime`（`:654`，時鐘在 ImGui 的 SDL3 後端） |
| model_viewer | 無 | (a) 對話回覆佇列（`ViewerWindow.cpp:112`）；(b) GPU readback 同步等待；(c) 音訊 `Submitter` → Mixer 執行緒（Engine `SdlAudioOutput` 起的，`PreviewSounds.cpp:39`）：`SoundPlayback::Update` 每幀排程、60 ms 延遲（`:426`）；(d) 跨幀 closure `pendingAction_`（`:1006`） | `std::chrono::steady_clock`（`:107、:120`）＋ `Engine::Time::Now()`（`:426`、`PreviewSounds.cpp:68`） |
| audio_editor | 無 | (a) 對話回覆佇列（`AudioEditorWindow.cpp:263`）；(b) 音訊 `Submitter` → Mixer 執行緒（`SdlAudioOutput::Open`，`:96`）：`PreviewPlayer::Update` 每幀補送滿佇列或缺 slot 的請求（`PreviewPlayer.hpp:42`）；mixer 是位置的權威、player 只顯示（`:27-29`，GYO::Audio DI 4） | `Engine::Time::Now()`（`:117、:331、:335`） |

- 掃描：`git grep -n -P '\b(std::thread|std::jthread|std::mutex|std::atomic|std::async|std::condition_variable|std::future|<thread>|<mutex>|<atomic>)' -- tools framework` 退出碼 1（0 命中）；正例 `git grep -c -P '\bstd::thread\b' -- engine` 命中 3 檔（`Mixer.cpp`、`SdlGpuRenderDevice.cpp`、`RoleThread.hpp`）。
- **共用程度**：「工具不起執行緒、所有跨執行緒交換在 Engine 的佇列」**四同**；音訊交換點 2／4 兩種包裝（`PreviewPlayer` 一次一聲、`PreviewSounds`＋`SoundPlayback` 排程）；時鐘來源三種（audit 觀察 6）；跨幀 closure 只 model_viewer。
- 目標架構對照：這些交換點在 target 分別對應 `PlatformEvents`（boundary）、`FrameTime`（external）、Submitter Resource、Extract（readback 消失）。

## 3. 共用程度彙總

| # | 項目 | framework 一份（四同） | 工具端的共用程度 | 工具端差異的具體內容 |
|---|---|---|---|---|
| ① | 行程入口 | `RunHeadlessShell` | `RunXxxWindow` 四同形各一份；存根四同形各一份 | 標題與尺寸；存根退出碼 3／2／3／2；失敗函式三同一異 |
| ② | 視窗與 pump | `RunWindowShell`、`LoopAdapter` | 四同（不自寫）；量測模式 2／4 自寫 | 量測迴圈量的東西不同 |
| ③ | ImGui 接法 | `ImGuiWindowClient`、`ImGuiScope`、字型、幀序 | 全視窗單窗三同一異；`InputString` 2／4 逐字同；文字欄三法 | ui_editor 五窗；旗標三種；只有 ui_editor 用 `ImGuiShellInput` |
| ④ | 檔案對話 | 無（Engine `SdlPlatform`） | 3／4 有，**三種記帳**；voxel 無 | 用途列舉、過濾器、Abandon 有無、等待狀態有無（CCR-1） |
| ⑤ | 預覽 readback | `OffscreenPreview`、`UploadFrame`、`ImGuiTextureId` | 2／4 同路、包裝各一份；ui_editor 另一路；audio 無 | chunk+palette vs. ModelRenderer+骨架；尺寸計算與上傳 2／4 逐字同 |
| ⑥ | 資產掛載 | `ReadOnlyAssetCatalog`、`RegisterAsset` | 唯讀掛 2／4 包裝各一份；寫登記 2／4；voxel 無 | 掛載時點（幀邊界排隊 vs. 當場）；ui_editor 另有 `AssetManager` |
| ⑦ | 設定與最近檔 | （DI 4 禁 ini） | 四同：都無 | — |
| ⑧ | 關窗提示 | `CloseRequested` 的詢問 | 3／4 有，**三種實作**；audio 無 | 兩鍵／三鍵含存檔／跨幀 closure；守哪些動作 |
| ⑨ | 錯誤顯示 | `Base::Log` 分類 | 樣式同形各一份 | 單字串 vs. 分區字串；紅色常數三份 |
| ⑩ | 交換點／執行緒 | `ShellInput`、pump | 四同不起執行緒；音訊 2／4 兩種；時鐘三種 | `PreviewPlayer` vs. `SoundPlayback`；`steady_clock`／`Time::Now`／`io.DeltaTime` |

## 4. 依賴邊逐工具

### 4.0 掃描方法與正例

- 掃描器：主 checkout `build/evidence/tool-hosts-inventory/scan_tool_hosts.py`（Python，`-I` 執行；只讀）。(a) CMake：`tools/<t>/CMakeLists.txt` 去掉 `#` 註解後，每個 `target_link_libraries(<target> …)` 內命名的 `GYO::*`、`imgui::*`、`nlohmann_json::*`、`SDL3::*`，去重。(b) include：`git grep -n -P '^\s*#include\s*[<"]' -- tools/<t>`，每個路徑依 alias 表（`git grep -n -P 'add_library\((GYO|imgui)::\w+\s+ALIAS' -- engine framework third_party`，36 個 target 的目錄）做最長前綴映射；`engine/asset`、`engine/io`、`engine/runtime` 落 `GYO::Engine` 傘（`engine/CMakeLists.txt:26-30`）；`*Native.hpp` 映 `*Native` target；`gyo/framework/imgui/`→`ToolFrameworkImGui`、`gyo/framework/json/`→`ToolFrameworkJson`、`Preview*.hpp`→`ToolFrameworkPreview`、其餘 `gyo/framework/`→`ToolFramework`；`imgui.h`→`imgui::core`、`backends/`→`imgui::sdl3_sdlrenderer3`、`nlohmann/`、`SDL3/`。
- 正例（掃描器啟動時自查，未命中即退出碼 2）：`framework/bindings/imgui/src/ImGuiShell.cpp` 的 include 必須映出 `imgui::core`、`imgui::sdl3_sdlrenderer3`、`GYO::PlatformSDLNative`、`GYO::RenderBackendSDLNative`、`GYO::InputBackendSDL`；`framework/bindings/imgui/CMakeLists.txt` 必須掃出 `imgui::sdl3_sdlrenderer3`、`GYO::PlatformSDLNative`、`GYO::ToolFrameworkWindow`。**第一次執行 CMake 正例未命中**：`target_link_libraries(... # (Design Invariant 2) ...)` 註解內的右括號讓正規式提前結束，只掃出 3 個 PUBLIC 項；加入去註解後命中。這一次失敗是正例規則（AGENTS §14.2）攔下的掃描器缺陷，記在這裡。
- 輸出：`scan.json`（全部邊與命中數）、`<tool>_include_hits.txt`（每個 target 的 `檔:行 路徑`）、`cmake_edges.dot`（CMake 連結圖）。commit `d0d8563f`。

### 4.1 每工具的邊

| 工具 | (a) CMake 直接命名的外部 target（去重） | (b) include 映出的 target（命中數） | 只在 (b) 的 target 與原因 |
|---|---|---|---|
| ui_editor | **13**：`GYO::Engine`、`ToolFramework`、`Ui`、`nlohmann_json`、`Math`、`Os`（core）；`ToolFramework`、`Os`（exe）；`RenderBackendSDLNative`、`AssetSdlImage`、`TextBackendSDLTTF`、`imgui::core`、`PlatformSDL`、`Math`、`Input`、`Os`（preview）；`ToolFrameworkImGui`、`PlatformSDL`、`RenderBackendSDLNative`（exe gui）。邊 19 條 | **15**：Engine 16、nlohmann 13、Math 12、Base 9、Os 9、ToolFramework 9、imgui::core 6、Ui 4、AssetSdlImage 1、Input 1、PlatformSDL 1、RenderBackendSDLNative 1、TextBackendSDLTTF 1、ToolFrameworkImGui 1、**SDL3 直接 1** | `GYO::Base`（`engine/base/*.hpp` 9 處）：`GYO::Engine` PUBLIC 連 `GYO::Base`（`engine/CMakeLists.txt:72`）；`SDL3`（`AssetPreviewContext.cpp:4`）：經 `GYO::RenderBackendSDLNative` INTERFACE 帶入 `SDL3::SDL3`（`engine/render/backend/sdl/CMakeLists.txt:24-31`），是 ui_toolchain DI 1 寫明的唯一原生用法 |
| voxel_map_editor | **12**：`Base`、`Engine`、`Math`、`VoxelMesh`、`MapMarkers`、`nlohmann_json`（core）；`ToolFramework`、`nlohmann_json`（exe）；`ToolFrameworkPreview`、`ToolFrameworkImGui`、`RenderBackendSDL`、`PlatformSDL`、`imgui::core`（window）。邊 13 條 | **14**：Base 14、Math 11、VoxelMesh 7、ToolFrameworkPreview 5、Engine 4、nlohmann 3、PlatformSDL 2、RenderBackendSDL 2、MapMarkers 1、Render 1、RenderBackendSDLGPU 1、ToolFramework 1、ToolFrameworkImGui 1、imgui::core 1 | `GYO::Render`（`PreviewScene.hpp:9`）、`GYO::RenderBackendSDLGPU`（`PreviewScene.cpp:4`）：`GYO::ToolFrameworkPreview` PUBLIC 連兩者（`framework/CMakeLists.txt:54-57`） |
| model_viewer | **13**：`Base`、`Engine`、`Model`、`ToolFramework`、`AssetUfbx`、`ToolFrameworkJson`（core）；`ToolFramework`（exe）；`ModelRenderer`、`ToolFrameworkPreview`、`Audio`、`AudioBackendSDL`（preview）；`ToolFrameworkImGui`、`RenderBackendSDL`、`imgui::core`（exe gui）。邊 14 條 | **18**：Model 15、Base 14、Engine 8、ToolFramework 4、ToolFrameworkPreview 4、PlatformSDL 3、Render 3、ToolFrameworkJson 3、Audio 2、RenderBackendSDL 2、AssetUfbx 1、AudioBackendSDL 1、Math 1、ModelRenderer 1、RenderBackendSDLGPU 1、Time 1、ToolFrameworkImGui 1、imgui::core 1 | `GYO::Math`、`GYO::Time`：`GYO::Engine` PUBLIC 連 `Base Math Time`（`engine/CMakeLists.txt:72`）；`GYO::PlatformSDL`、`GYO::Render`、`GYO::RenderBackendSDLGPU`：`ToolFrameworkPreview` PUBLIC（`framework/CMakeLists.txt:54-57`）、`ToolFrameworkImGui` PUBLIC `ToolFrameworkWindow` PUBLIC `PlatformSDL`（`bindings/imgui/CMakeLists.txt:16-19`、`framework/CMakeLists.txt:43`） |
| audio_editor | **10**：`Base`、`Audio`、`Engine`、`Os`、`ToolFramework`、`ToolFrameworkJson`、`nlohmann_json`（core）；`Engine`、`ToolFramework`、`ToolFrameworkImGui`、`AudioBackendSDL`、`imgui::core`（exe）。邊 12 條 | **12**：Audio 8、Base 7、Engine 6、Time 2、AudioBackendSDL 1、Os 1、PlatformSDL 1、ToolFramework 1、ToolFrameworkImGui 1、ToolFrameworkJson 1、imgui::core 1、nlohmann 1 | `GYO::PlatformSDL`（`AudioEditorWindow.cpp:21`，對話框型別）：經 `ToolFrameworkImGui` → `ToolFrameworkWindow` → `PlatformSDL` 的 PUBLIC 鏈（工具 CMake 註解 `:56-58` 寫明「不命名它」）；`GYO::Time`（`MonotonicClock.hpp`）：`GYO::Engine` PUBLIC |

- **件数の突き合わせ：不一致（已解釋）**。只在 (b) 的 11 個 target 全部由 PUBLIC 連結傳遞解釋；只在 (a) 的 0 個。意思：每個工具都 include 了它沒有直接命名的 Engine／framework 標頭，依賴正向清單（工具守衛）是以「連結命名」為單位守的，**include 面比連結面寬 2～5 個 target**。搬進 plugin 時，plugin 自己的正向清單（runtime-composition §4）要以 include 面為準，否則會漏。
- **第三方邊**（直接）：`imgui::core` 4／4（`imgui.h`；`backends/` 0／4，只在 framework）；`nlohmann_json` 3／4 直接（ui_editor 13 處含公開標頭 `EditorApp.hpp:12`、voxel 3、audio 1；model_viewer 0，經 `ToolFrameworkJson`）；`SDL3` 直接 1／4（ui_editor 一檔）；`SDL3::*` target 0／4。
- **Engine 邊的交集**（四工具都有）：`GYO::Base`、`GYO::Engine`、`GYO::ToolFramework`、`GYO::ToolFrameworkImGui`、`imgui::core`；三個有預覽／對話的另加 `PlatformSDL`（include 面）。只有一個工具有的：`Ui`、`Input`、`TextBackendSDLTTF`、`AssetSdlImage`、`RenderBackendSDLNative`（ui_editor）；`VoxelMesh`、`MapMarkers`（voxel）；`Model`、`ModelRenderer`、`AssetUfbx`（model_viewer）。

## 5. 守的性質來自哪份文件；搬進共用 plugin 會撞到的守衛

守衛的逐列盤點已在 [guards-inventory.zh-Hant.md](guards-inventory.zh-Hant.md)（PR #198，2026-11-13 已合併 master）；本節只對照 host 骨架的 10 項，**不重做**，引用其節號。

| 項 | 性質的來源 | 守衛（今天） | 搬進共用 plugin 會撞到 |
|---|---|---|---|
| ①② 行程入口、pump、關窗詢問、生命週期順序 | framework DI 6（`docs/framework/architecture.md`；**已廢止、隨 framework-delete 刪**） | `tests/framework/HeadlessShellTests.cpp` H1～H3、`WindowShellTests.cpp` W1～W4、`ImGuiShellTests.cpp` Q3（關窗）；fw-shell-*／fw-close-* 突變（guards-inventory §3 第 11 列 33 條：重列 29） | 守衛掃 `framework/`，plugin 在 `plugins/` 不被掃；framework 刪除後 `tests/framework` 的 CMake link check 硬失敗 → 必須同批刪（guards-inventory §2.10 DI 2 綁定列） |
| ③ ImGui 接法（side file、文字輸入、組字） | framework DI 2、4（廢止）；ui_toolchain ui_editor DI 1、2 | `ImGuiShellTests` I1～I11、T2；`ShellHeaderTests`；`ui_editor_dependency_guard.py` 禁 `imgui_internal.h`（四支都禁） | 工具守衛的 `ALLOWED_TARGETS` 無任何 plugin target（§2.11 (a)）；`ALLOWED_INCLUDE_PREFIXES` 只有 `gyo/<tool>/`、`gyo/framework/`、`engine/`（voxel 另 `window/`）→ 第一個 `#include "gyo/plugins/..."` 或連 plugin target 的 commit 就失敗；每條新邊都是 DI 守衛變更（§14.4 交使用者） |
| ④ 檔案對話記帳 | platform-sdl DI 10（Engine，不動）；**CCR-1**（`docs/cross-component-rules.md`，owner 今天是 Engine 條文、target owner 寫 framework `PathDialogRequests<Purpose>`，批 `v8-tool-path-dialog-rule-handover` 已暫停） | 平台側 `tests/common/platform_sdl/PathDialogTests.cpp`；消費端只有 ui_editor `tests/ui_editor/EditorCoreTests.cpp` 的規則測試；audio／model_viewer 無 | CCR-1 的 target owner 欄指向 framework → 若進 editor-dialogs plugin，登記頁的 target owner 要改（圖書管理員維護）；ui_editor 的規則測試要隨型別搬 |
| ⑤ 預覽 readback | preview.md DI 1～5（資料契約 DI 不廢止、owner 隨 plugin 搬；**DI 4 被 runtime-composition §3b 取代**）；voxel DI 7（W6／W7）；model_viewer DI 5、RC 1 | `tests/framework/PreviewTests.cpp` PV1～PV7、`framework_preview_mutations.json` 25 條；voxel W6／W7 | PV2／PV5 的判定本身是 readback 形狀——搬進 plugin 不改判定就把 readback 帶進渲染 World（guards-inventory §2.10 PV 列、計畫 §3.24「重寫不搬」）；voxel 守衛的 core／window 兩段切法（`WINDOW_ONLY_INCLUDES` 含 `gyo/framework/Preview`、`WindowShell`）在逼舊的 library 切法（§2.11 voxel 結構列） |
| ⑥ 資產掛載 | framework DI 4（唯讀 mount、`RegisterAsset` 只改一條；廢止）；ui_toolchain DI 3；audio_editor DI 3；model_viewer DI 4；catalog 資料契約（C1～C7、E1～E4 不廢止） | catalog 結構測試與 fw-cat-*／fw-extra-* 突變；`framework_catalog_mutations.json` 12 條（重列 10） | 同 ③ 的正向清單；`AssetManager` 的使用（ui_editor）由 assets-core／framework-delete 對帳 |
| ⑦ 設定與最近檔 | framework DI 4「不寫自己的檔」（`imgui.ini`）；ui_toolchain DI 2 無 side file | `ImGuiShellTests` I2 | 一個會寫設定檔的 `tool-settings` plugin 直接違反 framework DI 4 與 ui_editor DI 2 的「無 side file」；今天 0 個使用者，不是守衛問題而是沒有 Feature（§8） |
| ⑧ 關窗提示 | framework DI 6 的 close request（`v8-framework-close-request`）；各工具無 DI 條文（是行為） | `ImGuiShellTests` Q3、`WindowShellTests`；工具端：無專門測試（grep `tests/{audio_editor,model_viewer,voxel_map_editor}` 的 TEST_CASE 無 close／unsaved／dialog 命中；ui_editor 的在 EditorCoreTests） | 無守衛會擋；但 audio 今天沒有提示，統一後是行為變更（計畫 §3.24 宣告寫明） |
| ⑨ 錯誤顯示 | 無條文（各工具 `*Error.hpp` 經 error-handling.md 的 Result 契約） | 無 | 無 |
| ⑩ 交換點／執行緒 | GYO::Audio DI 4（mixer 是位置權威）；platform-sdl DI 10；J-9（`engine/` 無 `<thread>`，不含 tools） | `os_boundary_guard.py` R1～R4 的 `RUNTIME_ROOTS = engine, apps, tools`（**沒有 `plugins`**，§2.4） | plugin 內若直接開檔或起執行緒，OS 邊界守衛不掃（空轉）；plugins-foundation 要改掃描根（DI 守衛變更） |
| 共通：framework 的存在 | framework DI 1（不是產品） | `framework_dependency_guard.py` ratchet（`REMOVED_ALLOWLIST` 不得回來、`CORE["targets"]` 正向清單 `:55-74`）；`GyoComponents.cmake:5` `FRAMEWORK` component；`GyoBuild.cmake:55-58、91-93` `GYO_ACTIVE_TOOLS OR GYO_ENABLE_FRAMEWORK`；`ToolRegistryTests.cmake:100-102` 保留名 `framework` | 過渡期四工具同時連 framework 與 plugin（半遷移）守衛不擋（§2.11 (c)：`ToolFramework*` 項不刪就可並存）；framework 刪除前 `GYO_ACTIVE_TOOLS` 讓 framework 與其守衛在任何工具啟用時都 configure（Dummy 形狀，§2.12）；三者全部在 framework-delete 退役（§2.12、§2.13） |

## 6. 每個工具專屬、不該進共用 plugin 的東西（產品知識）

| 工具 | 專屬（留在工具自己的 plugin 或 main） |
|---|---|
| ui_editor | `gyo.ui` 文件的編輯模型（`DocumentSession`、`UndoStack`、`EditorDocumentEdits`、`ElementSchema`、`ElementClipboard`、`GizmoMath`、`UiDocumentBridge`）、`PreviewAdapter`（`GYO::Ui` 評估結果的繪製與 action 記錄，DI 5）、`AssetPreviewContext`（字型與貼圖解碼，`GYO::TextBackendSDLTTF`／`AssetSdlImage`）、對話用途 5 個與過濾器「UI JSON」、五窗版面、`--validate` 模式、診斷面板 |
| voxel_map_editor | `EditSession`／`MapPreview`／`PreviewScene` 的內容（chunk mesh、palette）、Goxel 式工具與鍵（`Tool`、`EditMode`、`Shortcuts`）、`FlyCamera`、`Pick`／`Visibility`、13 個 CLI 子命令、量測模式、三鍵關窗（含存檔）、「File changed」三鍵提示、`gyo.voxel_grid` 讀寫 |
| model_viewer | `ModelSession`／`PlaybackClock`、事件時間線（`EventHistory`、拖動、閃爍）、`gyo.anim_events` 與 `gyo.event_sounds` 的開存（經 Engine／framework 驗證）、`PreviewRenderer` 的骨架繪製、`PreviewSounds`＋`SoundPlayback` 排程、`Guarded` 守多種動作、CLI 的 models／info／roundtrip |
| audio_editor | WAV 解碼、裁剪與增益（`Edit`、`Conversion` 的 `-ffp-contract=off`）、專案檔、`SoundExport`／`RegisterAsset`、波形繪製、`PreviewPlayer`（一次一聲、mixer 兩 slot）、對話用途 5 個與過濾器「WAV」 |

跨工具但**不是 host 骨架**的共用（函式庫，與層無關；audit §10.1）：`EventSounds`／`BindEventSounds`（model_viewer、PvP）、`CatalogPlan`／`CatalogText`（audio、model_viewer）、`UiTextInput`（ui_editor、PvP）、`OrbitCamera`（voxel、model_viewer）、`AbsolutePath`。

## 7. 兩個以上工具重複、但內容不同的程式（framework 老問題的證據）

這些是「同一件事各寫一份」，framework 沒有收（或收了一半）：

| 重複的事 | 工具 | 具體差異 | 出處 |
|---|---|---|---|
| 檔案對話記帳 | ui_editor、audio、model_viewer | 三種型別與放置；mv 無等待狀態 | ④；CCR-1；audit 觀察 5 |
| 關窗提示 | ui_editor、voxel、model_viewer | 兩鍵／三鍵含存檔／跨幀 closure 守多種動作；audio 無 | ⑧；audit 觀察 3 |
| 「磁碟上變了」提示 | ui_editor、voxel | Overwrite／Cancel vs. Reload／Overwrite／Cancel；mv 只顯示錯誤 | ⑧ |
| CLI 變體存根 | 四工具 | 退出碼 3／2／3／2、訊息各寫 | ① |
| `RunXxxWindow` 的 10 行 | 四工具 | 標題、尺寸 | ① |
| 全視窗主窗的旗標 | voxel、mv、audio | 三組不同旗標 | ③ |
| `InputString` | voxel、mv | 逐字相同（可直接共用）；audio 與 ui_editor 用 `std::array<char,1024>` | ③ |
| 預覽尺寸計算、`UploadFrame`、`ImGuiTextureId`、`ImGui::Image`、貼圖釋放 | voxel、mv | 逐字相同的 6＋4 行各一份；framework 只收了 `UploadFrame`，呼叫端沒收 | ⑤ |
| 著色器路徑 `BaseDirectory()/shaders/builtin`＋`--gpu-driver`＋CMake 複製 target | voxel、mv（含兩個量測模式共 4 處） | 逐字相同 | ⑤ |
| 量測模式的 Host 迴圈 | voxel、mv | 結構相同、量的東西不同 | ② |
| `FileAccess`（`ReadAll`／`WriteReplacing` 包 engine/io） | audio、mv、voxel（ui_editor 在 `DocumentSession.cpp:47-96`） | 同一 10 行各一份，錯誤型別各工具自己的；mv 另加 `Probe`／`StampOfWritten`，voxel 另加 `FileExists` | audit 觀察 3 |
| 錯誤字串＋紅字 | 四工具 | 單字串／分區字串；紅色常數兩種 | ⑨ |
| 時鐘取樣 | audio、mv、voxel | `Time::Now`／`steady_clock`＋`Time::Now`／`io.DeltaTime` | ⑩；audit 觀察 6 |
| 試聽的 Submitter 包裝 | audio、mv | `PreviewPlayer`（一次一聲、重試） vs. `PreviewSounds`＋`SoundPlayback`（排程、版本號） | ⑩ |
| 解構子對未存變更寫 Warning | voxel、mv | 兩份相同形狀；ui_editor、audio 無 | ⑧ |

## 8. plugin 切法候選（**候選，不是決定**；決定在 `v8-arch-plugins-editor`）

每個候選列：哪些工具用、搬進去要改什麼、不搬的代價。與 audit §10.4 的表對齊；本節只加 host 骨架讀出來的細節。

| 候選 | 收進去的現況 | 哪些工具用 | 搬進去要改什麼 | 不搬的代價 |
|---|---|---|---|---|
| **sdl-window-runner**（計畫 §3.20 plugins-frame） | `WindowShell`（`RunWindowShell`、`LoopAdapter`）、`SdlPlatform` 的 pump 與 live frame、`SdlInput` 後端、對話回覆寫進 boundary（`PlatformEvents`） | 4／4（PvP Client 也用） | 四工具的 `RunXxxWindow` 改成 `project.json` plugins 欄＋main 只 Run；`WindowShellOptions`（標題、尺寸）改成 Resource 或登記資料；量測模式 2 份自寫迴圈改成合成 Runtime（或留 harness、宣告寫明） | 四工具各自寫 pump（今天沒有，不搬＝留在 framework，framework-delete 不開） |
| **headless-runner**（§3.11） | `HeadlessShell`（UTF-8 命令列、主控台）、`match_main` 迴圈 | 4／4 的 CLI 模式＋Match | 四個 `main` 的一行改成 Run；`Usage` 失敗函式三同一異要統一簽名 | 同上 |
| **imgui**（§3.24） | `ImGuiShell` 兩半：文件 World 的 System（context、輸入換幀、`NewFrame`、組字顯示、文字輸入所有權）與渲染 World 的 System（`RenderDrawData`、`Present`）；系統 CJK 字型；`IniFilename=nullptr`；`ImGuiTextureId` 改成 draw list 的貼圖 id 由渲染 World 解析 | 4／4 | 四工具的 `Draw` 從虛函式改成 System（H.4.3 v1：收成少數大 System）；`ImGuiShellInput` 的三個用法（ui_editor）改成 Resource；`InputString` 2 份、全視窗主窗 3 份可一起收成小工具函式（函式庫，不是 plugin 責任） | 四工具沒有 ImGui；或各自持 context（違 I-5 全域明列例外） |
| **editor-dialogs**（§3.24；架構師保留：必須進） | 選擇窗記帳（CCR-1：tag、只認最新、Failed 不結束、Abandon、「Stop waiting」片段）＋關窗提示（dirty 詢問、下一幀 modal、`wantsExit`）＋「磁碟上變了」提示 | 對話 3／4、關窗提示 3／4、磁碟變更 2／4 | 三種記帳統一成一份 Resource＋System（用途仍由工具給：`Purpose` 當泛型參數或資料）；三種關窗提示統一：**要決定**「Save and close」有無、是否守「開別的檔」、audio 是否開始追蹤 dirty（行為差異，宣告寫明）；CCR-1 登記頁 target owner 改成此 plugin；ui_editor 的規則測試搬過來成 plugin 測試；model_viewer 缺陷在統一時自然消失＝行為變更，宣告寫明交使用者 | 「framework 的老問題換名」：三份記帳、三份提示搬進三個工具 plugin，runtime-composition RC「Host 的協調邏輯在兩個以上 host 重複」第一天就成立 |
| **offscreen-preview**（§3.24） | `OffscreenPreview`（裝置、`Renderer`、著色器 bundle）、`PreviewScene`／`PreviewRenderer` 的「render target＋場景合成」部分；readback／`UploadFrame`／原生貼圖 id **消失**；著色器路徑與 `--gpu-driver` 收成 Resource | 2／4（voxel、mv；ui_editor 的 `AssetPreviewContext` 是另一種：2D 貼圖解碼到視窗 renderer，候選歸 imgui plugin 的貼圖供應或 asset 側，不歸此） | 兩個包裝改成渲染 World 的 System（Extract 幾何與相機）；兩份逐字相同的呼叫端（尺寸、上傳、Image、釋放）歸零；PV2／PV5 重寫不搬；voxel W6／W7 的觀測手段改寫 | 兩工具各自持離屏裝置；readback 形狀留下（H-6 衝突） |
| **asset-catalog**（§3.20） | `ReadOnlyAssetCatalog`、`CatalogPlan`／`CatalogText`（`RegisterAsset`） | 唯讀 2／4、寫登記 2／4 | ui_editor 的幀邊界排隊掛載與 model_viewer 的當場掛載統一成一種（Loading 或 `Io:file` System）；ui_editor 的 `AssetManager` 改 `Assets<T>`（tool-ui 批） | 兩份包裝；catalog 資料契約 owner 無處落 |
| **event-sounds**（§3.20） | `EventSounds`、Mixer 與 `Submitter` 接線 | model_viewer＋PvP | `PreviewSounds`／`SoundPlayback` 的排程部分是工具專屬（留）；接線（開輸出、建 submitter）與 audio_editor 的 `PreviewPlayer` 同一段（`SdlAudioOutput::Open`＋`CreateSubmitter`）可收 | 兩份接線 |
| **tool-settings**（卡片提到） | （無現況） | **0／4** | — | 無代價：今天沒有 Feature；依 AGENTS §1 不為「將來可能」建 plugin。若之後出現（最近檔、視窗狀態），先碰 framework DI 4／ui_editor DI 2 的「無 side file」，要先改條文 |
| **measure-harness**（不在 audit 表） | 兩個量測模式的 Host 迴圈與 CSV 寫出 | 2／4 | 若 runner plugin 能跑「無視窗、固定幀數、計時」的合成 Runtime，量測模式就是一個 plugin 選擇；否則留 harness（宣告寫明是 Host 形狀） | 兩份 Host 迴圈留著＝兩套手排順序留著（README「每個 host 一套手排順序」） |

共用 plugin 之外、建議收成**函式庫**（與層無關）的逐字重複：`InputString`、`FileAccess` 的 `ReadAll`／`WriteReplacing`（錯誤型別要中立）、紅字常數、CLI 存根的訊息格式。

## 9. 範囲の限界

- 只讀碼、未執行任何工具或測試；守衛的行為引用 guards-inventory 與守衛原始碼的清單常數，沒有跑守衛。
- ui_editor 的 `EditorApp*.cpp`（約 2,400 行）只讀 host 相關函式與對話檔；面板內容未逐行讀。`AssetPreviewContext.cpp` 只讀 Initialize／Mount／Upload／BeginFrame 部分。
- 依賴邊的 include 映射依 alias 表的目錄前綴；`engine/ui` 下 `Ui` 與 `UiRenderer` 同目錄一律映 `GYO::Ui`（四工具都沒連 `UiRenderer`，不影響結論）。(a) 只數 `target_link_libraries`，`foreach(_target IN ITEMS …)` 的存在檢查不算邊。
- 第三方的傳遞邊（`SDL3::SDL3` 經 `*Native`、`imgui::sdl3_sdlrenderer3` 經 framework）只在工具直接命名或 include 時計入；framework 自己的邊見 `framework/CMakeLists.txt`，不在本卡。
- parent 會話找不到，事前宣告未經核定（§0）。

## 證據與出處

- 主 checkout `build/evidence/tool-hosts-inventory/`（git 管理外，本機 macOS）：`scan_tool_hosts.py`、`scan.json`、`{ui_editor,voxel_map_editor,model_viewer,audio_editor}_include_hits.txt`、`cmake_edges.dot`、`greps.txt`（執行緒、設定、對話、關窗、ImGui 計數、時鐘、catalog 的 `git grep` 原始輸出）、`migration-plan.snapshot.md`、`guards-inventory.snapshot.md`、`snapshots.rev`。
- 讀過的文件：kernel `HANDOFF.md` 決定表（`v8-arch-framework-retire`、`v8-arch-plugin-selection-data`、`v8-arch-docs-legacy`）；`target/runtime-composition.zh-Hant.md` §1、§3b、§4、DI 候選；遷移計畫（分支 `claude/arch-migration-plan`）§2 取捨、§3.11、§3.20、§3.24～3.29、§4 Delta-2／Delta-5；`audit.zh-Hant.md` §1.2、§3.1、§3.2、§10；`audit-part-framework-tools.zh-Hant.md` 全文；`docs/framework/architecture.md` DI 1～6 與 RC、`preview.md`、`event-sounds.md` 的 legacy 標示；`docs/audio_editor/architecture.md`、`docs/model_viewer/architecture.md`、`docs/voxel_map_editor/architecture.md` 的 DI 與 RC；`docs/ui_toolchain.md` ui_editor DI 1～5；`docs/cross-component-rules.md` CCR-1；`docs/tools/plans/v8/HANDOFF.md` 現況（2026-11-11 停線）；AGENTS §6、§7、§9、§10。
