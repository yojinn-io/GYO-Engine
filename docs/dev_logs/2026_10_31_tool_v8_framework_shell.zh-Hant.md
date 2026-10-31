# 工具 v8：v8-tool-framework-shell（framework 層的工具部分）

日期：2026-10-31。分支 `claude/v8-tool-framework-shell`（切自 master `3d5fe1a5`）。檔位 medium～high，引擎 Opus 5.5。估 3～5 h；從「開始」到測試與突變全過約 35 分鐘（22:04～22:39，牆上時間，含 Design Invariants 的關卡與等待回覆，以及新建置樹第一次編譯 DXC 的約 15 分鐘）。

**狀態：** 完成（Mac 上的測試與突變）。Windows、Linux 由 CI 的 L1 驗證；ui_editor 的視窗（宿主、系統選擇窗）只有 GUI 能看，交給使用者在 macOS 上確認。

相關文件：[Design Invariants](../framework/architecture.md)、[規劃](../tools/plans/v8/assets.md)（`v8-tool-framework-scope`、`v8-tool-framework-principle`、`v8-tool-framework-top-level`）、[docs/architecture.md](../architecture.md)、[ui_toolchain.md](../ui_toolchain.md)「Design Invariants」的 ui_editor 節。

## 做了什麼

建立 framework 層的工具部分。起初放在 `tools/framework/`；PR 開出後依使用者的決定搬到頂層 `framework/`（見「搬到頂層」）。它不是產品、不登錄，只在選了工具時建置。依賴方向是「遊戲或工具 → framework → Engine」，目前只有工具用它。行為不變：ui_editor 與音效工具的測試沒有改判定，全部通過。

- **兩個程式庫**：
  - `GYO::ToolFramework`（不含 SDL）：`FileService`、`ReadOnlyAssetCatalog`（從 ui_editor 搬來）、`CatalogRegistration`（從音效工具搬來）、選擇窗的中立型別 `PathDialog`、錯誤型別 `FrameworkError.hpp`（`FileError`、`CatalogError`）。
  - `GYO::ToolFrameworkGui`：`ImGuiSdlHost`（從 ui_editor 的 `ImGuiSdlRendererHost.cpp` 改成通用宿主）、`SdlPathDialogs`。只在建置裡有 SDL 平台、SDL renderer 與 ImGui 的 SDL 後端時建立。
- **介面的改動**（搬家需要的，已核准）：
  - 宿主：工具實作 `ImGuiSdlClient`（`Initialize`、`BeginFrame`、`Draw`、`WantsExit`、`EndFrame`），`RunImGuiSdlHost(options, factory)` 建視窗、renderer、ImGui 與系統字型，再用 factory 建 client。初始化與解構的順序和原本相同（client 在 ImGui 之前建立；ImGui 先關，再解構 client，最後是 renderer 與視窗）。字型的 log 用視窗標題當前綴，ui_editor 的輸出文字不變。
  - 選擇窗：請求帶種類（開檔、存檔、開資料夾）、過濾器、預設位置與不透明的 tag，回覆帶回同一個 tag。SDL 回呼的生命週期處理搬進 framework，過濾器的字串由每個待回覆的請求保管。ui_editor 保留自己的用途列舉，`tools/ui_editor/src/SdlPathDialogs.cpp` 只負責「用途 → 種類與 UI JSON 過濾器」。
  - catalog 寫入：`RegisterSound` 改成 `RegisterAsset({id, type, path})`，型別由呼叫端傳入；空的型別拒絕。音效工具傳 `"sound"`，並把 framework 的錯誤換回自己的代碼（結束碼不變）。
  - ui_editor 的 `EditorError.hpp`：`FileError`、`CatalogError` 改成 framework 型別的別名；`PathDialogOutcome` 也是別名。ui_editor 其他程式只加上 `Framework::` 前綴。
- **建置與登錄**（Architecture Delta，已核准）：
  - `build/cmake/GyoBuild.cmake`：有選工具時，在 Engine 之後加入 `framework`；`BUILD_TESTING` 時加入 `tests/framework`。
  - `build/cmake/GyoAppRegistry.cmake`：`framework` 加進保留名稱（產品與工具共用的檢查）。`tests/common/cmake/AppRegistryTests.cmake` 的保留名稱迴圈加上 `framework`，`ToolRegistryTests.cmake` 加一條工具名為 `framework` 時失敗的案例。
- **Design Invariants**：[docs/framework/architecture.md](../framework/architecture.md)，5 條與 Replacement Criteria（使用者核准）。`docs/ui_toolchain.md` 的 ui_editor 第 1～3 條各加一句指向 framework（核准）。
- **守衛**：
  - 新增 `tests/framework/framework_dependency_guard.py`：不是產品（沒有 `project.json`、`add_executable`、安裝 target，登錄裡沒有 `framework`）、允許清單（include 與 target，禁止 `engine/input/` 與 SDL_GPU）、中立（讀 `tools.csv` 與 `projects.csv` 的 id，framework 的檔案出現就失敗）。自我測試 12 個。
  - 工具的守衛（§14.4，使用者核准）：ui_editor 與音效工具都允許 `gyo/framework/` 與 `GYO::ToolFramework`（ui_editor 另加 `GYO::ToolFrameworkGui`）。收緊的部分：ui_editor 拿掉 `backends/imgui_impl_sdl`、`GYO::PlatformSDL`、`GYO::PlatformSDLNative`、`GYO::RenderBackendSDL`、`GYO::RenderBackendSDLNative`、`imgui::sdl3_sdlrenderer3`；音效工具拿掉 `nlohmann/` 與 `nlohmann_json`。被放寬的性質（不連結遊戲、`GYO::Input`、SDL_GPU）由 framework 的守衛對 framework 自己的程式與 CMake 繼續檢查。
- **測試的搬移**：catalog 寫入的 5 個 case 從 `tests/audio_editor` 搬到 `tests/framework/FrameworkCoreTests.cpp`（改成帶型別，另加「型別是呼叫端的」與空型別）；選擇窗收件匣的測試從 `tests/ui_editor/EditorCoreTests.cpp` 搬過來（改成 tag）。判定沒有放寬：舊的期待值逐條保留。
- **其他引用**：`docs/architecture/plans/os-subsystem/scripts/mutations.json` 裡兩個指向 `tools/ui_editor/src/FileService.cpp` 的突變改成新路徑。舊計畫文件與 dev_log 裡的舊路徑是當時的紀錄，不改。

## 搬到頂層（v8-tool-framework-top-level）

PR #123 開出後，架構會話指出兩件事：owner 名 `framework` 和 [v7 README](../object_fps_pvp/plans/v7/README.md) 定義的 framework 層（Engine 與遊戲之間、承載遊戲的組合方式）撞名；`docs/architecture.md` 寫「不引入 umbrella framework」，卻沒有提到這一層。使用者回答「這個是遊戲和工具共用的。」，並選了「同一層，現在就搬到頂層」。

- `tools/framework/` → `framework/`（`git mv`，內容不變）。新的頂層目錄是 Architecture Delta（AGENTS §10），由這個選擇核准。
- [Design Invariants](../framework/architecture.md) 跟著改（同一個選擇核准）：開頭說明兩個部分（現有的工具部分、還沒建的遊戲部分，照 v7 README 的啟動條件抽出）；第 1 條「不是工具」改成「不是產品」（兩份登錄都沒有它）；第 2 條依賴改成「遊戲或工具 → framework → Engine」，framework 不 include `apps/` 或 `tools/`；第 5 條「兩個以上工具」改成「兩個以上產品」，遊戲部分不預先放入。
- `docs/architecture.md`：目錄樹加 `framework/`；說明頂層 `framework/` 的依賴方向；模組表加「Framework, tool part」一列，UI editor 的依賴寫上它；「不引入 umbrella framework」那句補上 framework 層不是 umbrella 的理由（沒有產品必須經過它、不用它的產品照樣成立、遊戲部分等第二個遊戲才抽出）。
- 守衛：掃描範圍改成 `framework/`；不是產品的檢查涵蓋兩份登錄；自我測試加「include 遊戲的標頭會失敗」（12 個）。
- namespace 維持 `Gyo::Tools::Framework`、target 維持 `GYO::ToolFramework`／`GYO::ToolFrameworkGui`：它們就是工具部分。遊戲部分日後另取自己的 namespace 與 target。
- 搬移後重新驗證：ctest 63／63；只選音效工具的樹 55／55；不選工具的樹沒有 framework；路徑改了的突變重跑 16／16（framework 14、os 的 FileService 2）。

## 驗證

- 新建置樹 `build/target/_build/framework`（`GYO_APPS=`、`GYO_TOOLS=AUTO`＝ui_editor＋audio_editor、`BUILD_TESTING=ON`）：完整 ctest 63／63 通過，含 `gyo_framework.*`、`gyo_ui_editor.*`、`gyo_audio_editor.*`、`cmake.AppRegistry`、`cmake.ToolRegistry`、`cmake.ProjectComposition`。
- 只選音效工具的樹（`build/target/_build/audio`，沒有 SDL renderer 與 ImGui）：`GYO::ToolFrameworkGui` 不建立，建置成功，ctest 55／55 通過。
- 不選工具的樹（`build/target/_build/core`）：重新設定後，建置圖裡沒有 `framework`（Design Invariant 1：沒有用到它的產品就不建）。

## 突變

用既有的執行器 `docs/architecture/plans/time-threads-trace/scripts/run_mutations.py` 在 `framework` 樹上跑，全部被抓到：

- `docs/tools/plans/v8/scripts/framework_mutations.json`：14／14。
  - 從音效工具搬來的 catalog 突變 4 個（改名 `fw-…`，目標改成 `gyo_framework_tests`）。
  - 新的：型別被忽略、空型別不拒絕、收件匣順序顛倒、取走後不清空。
  - 守衛：framework 的 CMake 出現 `GYO::Input`、include 工具的標頭、程式出現登錄的工具名、加了執行檔；ui_editor 又直接連 `GYO::PlatformSDLNative`（收緊的部分）；音效工具又連 `nlohmann_json`（收緊的部分）。
- `docs/architecture/plans/os-subsystem/scripts/mutations.json` 裡 `FileService.cpp` 的兩個（`ui-x1`、`ui-x2`，路徑改了）：2／2。
- `docs/tools/plans/v8/scripts/audio_editor_mutations.json`（音效工具的檔案改了，整份重跑；catalog 的 4 個已移出）：13／13。

## 已知的限制與之後

- 宿主與 SDL 選擇窗沒有自動測試（要視窗）。ui_editor 的「Browse…」與開啟、匯出對話框需要在 GUI 上確認。
- `FileService`、`ReadOnlyAssetCatalog` 的測試仍在 `tests/ui_editor`（經 ui_editor 的行為測到）。沒有搬，是因為它們同時測 ui_editor 的規則（匯出不能進入掛載的資產根目錄等）。
- 音效工具的視窗（`v8-tool-audio-editor-edit`）會用 `GYO::ToolFrameworkGui`，到時候它的守衛要再加這個 target（同樣要你核准）。
