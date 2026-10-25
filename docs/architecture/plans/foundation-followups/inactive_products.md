# 基礎後續整理：未啟用產品的遷移清單

建立：2026-10-04（計畫落盤時）。基準：master `05042fa`。**本計畫各批都未開始；本文件目前只列「預計造成的遷移需求」，沒有任何量測。**

本計畫依範圍不修改 object_fps、object_fps_v2 與 object_fps_preview。各批改動 Engine 公開介面、共通測試或登錄資料後，這些產品重新啟用前需要先遷移；本文件是遷移清單。
重新啟用時，除了本文件，還要處理 [Math 的遷移清單](../math-foundation/inactive_products.md) 與 [Result 統一的遷移清單](../result-unification/inactive_products.md)。計畫與紀錄見 [README](README.md)、[HANDOFF](HANDOFF.md)。

[輸入與呈現計畫](../input-and-present/README.md)（IP）的遷移需求也記在本文件的「IP」節，不另建清單，避免同一件事寫在兩處。

## 1. 結論

| 項目 | 狀態 |
|---|---|
| 已量測的批次 | 無（全部未開始） |
| 新增的編譯失敗 TU | 未量測 |
| 新增的執行期語意變化 | 未量測 |

每批完成時，比照 Math 清單的方法（`git archive` 匯出 scratch、指向 scratch registry 啟用產品、`ninja -k 0`、逐 TU `-fsyntax-only`）量測，並把本節與對應節從「預計」改為實測結果。repo 只讀不寫。

## 2. 各批預計造成的遷移需求

### FF-1 Collision 判定語料

狀態：FF-1 完成（2026-10-05）。

- 無：只新增共通測試與比對腳本，Collision 的程式與公開介面沒有改。

### FF-2 共通測試的計時假設

狀態：FF-2 完成（2026-10-05）。

- `tests/object_fps/package_tools/test_gpu_smoke.py:55-60`：子程序須在 0.5 秒的 timeout 內輸出。重新啟用時比照 FF-2 對 `tests/common/ci/test_package_checks.py` 的修法（D6）：
  子程序輸出後寫就緒檔，測試以 `run_after_ready` 型的包裝讓 timeout 從就緒後才開始計時；runner 不改。
- 未啟用產品若使用 `AssetWatcher`：FF-2 修正了換算抖動造成的誤報 `Modified`，不需要遷移；`WatchedInfo::lastWriteTimeNs` 的值會差一個固定的時鐘差，不影響比較。

### FF-3 FNV-1a 收為一份

狀態：FF-3 完成（2026-10-05）。

- 無：2026-10-05 以 grep 確認 `apps`、`tools`、`tests`、`build/acceptance` 都沒有使用 `Detail::Fnv1a64`、`Asset::Detail` 或 `detail/Hash.hpp`；`AssetId`、`AssetType` 的公開用法與雜湊值不變（characterization 測試鎖住）。
- 未啟用產品若需要 FNV-1a，直接用 `engine/base/Fnv1a.hpp` 的 `Engine::Base::Fnv1a64`。

### FF-4 有限性檢查收斂

狀態：FF-4 完成（2026-10-05）。

- 無破損：只新增 render 的公開函式 `Engine::Render::IsFinite(const Color&)`（`render/RenderTypes.hpp`），沒有移除公開名稱。
- 可選：未啟用產品自己的 Color 有限性檢查可改用它（不影響編譯）。

### FF-5 Ui 與 ui_editor 的重複、`item_step` 驗證

狀態：FF-5 完成（2026-10-05）。

- 未啟用產品的 UI 資料（`assets/object_fps/ui/screens.json`、`assets/object_fps_v2/ui/screens.json`）：2026-10-05 的 scratch 比對中，兩者在新的 `item_step` 規則下都能載入，`gyo_ui_editor --validate` 也是 exit 0；layout 與 draw list 在 5 種 viewport 下與改動前逐位元相同。不需要遷移。
- Ui 新公開的 `FitDesignCanvas`、`ContainsUiPoint`、`AlignUiText`（`ui/UiRuntime.hpp`）屬加法，不需遷移。

### FF-6 共通層衛生與產品登錄

狀態：FF-6 完成（2026-10-05）。`object_fps_preview` 已在 `tools.csv` 停用。

- `object_fps_preview` 在 `engine/config/tools.csv` 改為停用（D11⑧）。重新啟用順序：先依各清單遷移並重新啟用 object_fps（preview link 依賴它的 `app_support`），再把 preview 的登錄改回啟用。
- preview 的輸入遷移見本文件「IP」節；Math、Result 的遷移見各自的清單。
- 未啟用產品的測試若自帶 characterization helper，不受影響；若重新啟用後要使用，改依賴 tests/common 的共通版本 `tests/common/support/CharacterizationBits.hpp`（`Engine::Test::Opaque`、`SameBits`、`UlpDistance`，連結 `gyo_test_support`）。

### FF-7 include 路徑風格徹底統一

狀態：FF-7 完成（2026-10-05）。

- 所有公開 header 改到 `engine/<m>/` 之下；不留別名，所以未遷移的 include 會直接編譯失敗。
- 對應（舊 → 新）：`render/` → `engine/render/`（含 `render/backend/sdl/`、`render/backend/sdl_gpu/`）、`model_renderer/` → `engine/render/model/`、`model/` → `engine/model/`（含 `model/backend/ufbx/`）、`text/` → `engine/text/`（含 `text/backend/sdl_ttf/`）、`ui/` → `engine/ui/`、`platform/sdl/` → `engine/platform/sdl/`、`input/backend/sdl/` → `engine/input/backend/sdl/`。檔名不變，target 名稱、namespace 不變。
- 2026-10-05 重新量測：94 行、36 個檔案，全部只需改 include 前綴：
  - `apps/object_fps` 9 檔、`apps/object_fps_v2` 12 檔；
  - `tests/object_fps` 4 檔、`tests/object_fps_v2` 3 檔；
  - `build/acceptance/object_fps` 4 檔、`build/acceptance/object_fps_v2` 3 檔；
  - `tools/object_fps_preview` 1 檔。
- 重新啟用時可以照上面的對應逐字取代：只取代「舊路徑恰好是被搬移的 Engine header」的 include，不要用前綴取代，以免改到產品自己的同名目錄。

### FF-8 GYOP 標頭 C++ 編解碼

狀態：FF-8 完成（2026-10-05）。

- 無：2026-10-05 grep `apps/object_fps`、`apps/object_fps_v2`、`tools/object_fps_preview` 沒有 GYOP、`wire::` 或 `Wire.hpp`。
- 未啟用產品若之後使用 GYOP，直接連結 `GYO::Net`（`engine/net/GyopDatagram.hpp`），版本與訊息種類由產品自己決定。

### FF-9 Collision 統一與公開合法性檢查

狀態：FF-9 完成（2026-10-07）。

- 數值：`RaycastAabb`、`VerticalCapsule` 版的 `RaycastCapsule` 與 `SweepSphereAgainstCapsule` 改為 double 實作；擦邊與容差邊界的命中可能翻轉，距離可能差數個 ULP。依賴這些結果的產品測試期望值可能要更新。
- 合法性（2026-10-07 FF-9 事前宣告）：height＝2r 的球形 `VerticalCapsule` 與長度 0 的 `Math::Capsule` 仍然合法；`min==max` 的 AABB 也不會被拒。
  - `object_fps_v2` 的兩處球形 `VerticalCapsule`（`src/Collision/CombatCollision.cpp:99-100`、`src/Gameplay/Enemy/EnemySystem.cpp:1155`）與球形頭部 hurt region（start == end）在 FF-9 之後仍然合法；FF-9 刪除 float 路徑後，這類膠囊原本可能在 float 路徑中止的問題也一併消除。
- 近平行射線：方向分量只有恰為 0 才算平行，近平行（|d_i|≤1e-6）不再被當成平行，視線判定可能由未中變為命中；影響 `EnemySystem.cpp:1130`、`:1164` 與兩個未啟用產品 `CombatCollision` 中的 `RaycastAabb`。
- 內容驗證：未啟用產品原本以 catch `std::invalid_argument` 驗證內容；Collision 改為 assert 後這招已失效（見 Result 清單）。重新啟用時改為在載入時呼叫公開 `IsValid`。

## 3. IP（輸入與呈現計畫）

IP 計畫的批次完成時，由該批更新本節（IP 夾不另建清單）。

### IP-1 輸入層：完整 scancode 與視窗互動事件

狀態：IP-1 完成（2026-10-05）。

- `tools/object_fps_preview/ViewmodelPreview.cpp:74-82` 直接讀 `SDL_SCANCODE_1..6` 與 `SDL_SCANCODE_ESCAPE`。重新啟用時改用 Engine 的 `Key::Digit1`～`Key::Digit6` 與 `Key::Escape`（`PhysicalInputFrame::Get(Key)` 或 `InputActionMap`）。preview 目前無法 link（見 FF-6），只能在 object_fps 重新啟用後驗證。
- 破損：無。`Key`、`MouseButton` 只在原有值之後追加；`PhysicalInputFrame` 新增 `events`（`std::vector`），既有的 `pressed`／`released`／`held` 語意不變；`NativeEventObserver` 保留。2026-10-05 grep：未啟用產品沒有依賴 `Key::Count`、`MouseButton::Count` 或 `PhysicalInputFrame` 的大小。

### IP-2 呈現不阻塞主迴圈

狀態：IP-2 完成（2026-10-05，縮小交付）。

- 破損：無。
  - 取得語意沒有改（仍是阻塞取得），`Skipped` 不會變常見。sdl_gpu 只在取得 ≥50 ms 時多一行診斷日誌。
  - `RuntimeLoop` 沒有 live frame 時，呼叫順序、frame 編號與 delta 都與先前相同。
- 選用：未啟用產品若要在縮放（macOS）或視窗移動／縮放（Windows）期間持續更新，可以呼叫 `SdlPlatform::SetLiveFrameHandler`，在處理函式中呼叫 `RuntimeLoop::RunLiveFrame`。
  - live frame 中，`Update`／`Render` 是在 `ProcessEvents` 尚未返回時被呼叫，沒有新的輸入；產品要確認不會重複消費輸入邊緣。
  - 提出需求的消費端的做法：live frame 中使用空的輸入幀。

### IP-3 SDL 隔離核心

狀態：完成（2026-10-22，ip3b～ip3f，含 L3）。

- ip3c、ip3d：`InputEvent` 多了 `timestamp`；輸入幀多了 `text`（`TextEvent`）與 `KeyModifiers`；`HandleNativeEvent` 必須帶時間。只有加法，未啟用產品不需改（`HandleNativeEvent` 的呼叫端除外，見下面的破損）。

- ui_editor（啟用中，依 D31 的 U2 只做機械修改）：`tools/ui_editor/src/ImGuiSdlRendererHost.cpp` 改用 Native 層的 `NativeWindow(platform)`、`NativeRenderer(renderer)`、`NativeEvent(event)`；`tools/ui_editor/CMakeLists.txt` 改連 `GYO::PlatformSDLNative`、`GYO::RenderBackendSDLNative`。破損：無（建置與 `editor` 標籤的測試通過）。
- **破損（重新啟用時一定要遷移）**：
  - `apps/object_fps{,_v2}/src/App/ObjectFpsRuntimeClient.cpp:233-234`：pump 的 observer 收 `const SDL_Event&`，而且只靠 Engine 標頭間接取得 SDL。改成 `const Engine::Platform::Sdl::PlatformEvent&`，交給 `SdlInput::HandleEvent`；需要 SDL 事件時用 `NativeEvent(event)` 並連 `GYO::PlatformSDLNative`。
  - `tests/object_fps_v2/App/DebugRuntimeTests.cpp:53-85`：`NativeWindow()` 成員函式已移除，改用 `Engine::Platform::Sdl::NativeWindow(platform)`；合成的 SDL 事件改用 `HandleNativeEvent(input, event)`（`SdlInputNative.hpp`，連 `GYO::InputBackendSDLNative`）。
  - `tools/object_fps_preview`：同上，pump 的 observer 與 `NativeWindow()`。
- ip3e（選用，重新啟用時建議遷移）：
  - 進入點：`apps/object_fps{,_v2}/main.cpp`、`tools/object_fps_preview/main.cpp` 的 `#include <SDL3/SDL_main.h>` 與 `int main` 可以改為 `#include "engine/platform/sdl/EntryPoint.hpp"`、`int GyoMain`，並連 `GYO::PlatformSDLMain`（例外記錄後回 1；Windows 上 argv 轉成 UTF-8）。不改也能建置。
  - 執行檔目錄：`SDL_GetBasePath()` 可以改為 `Engine::Platform::Sdl::BaseDirectory()`（回 `std::filesystem::path`，UTF-8 正確）。
  - 日誌：SDL 的輸出要進 Engine 的日誌時，建立 `Engine::Platform::Sdl::SdlLogBridge`；Base 的 handler 不得呼叫 SDL 的日誌函式。
- 變更摘要：後端的一般公開標頭（`SdlPlatform.hpp`、`SdlInput.hpp`、`SdlRenderer.hpp`）不再 include SDL；`NativeEventObserver` 改成 `EventObserver`（`const PlatformEvent&`）；`NativeWindow()`／`NativeRenderer()` 成員函式改成 Native 層的自由函式；`SdlInput::HandleEvent(const SDL_Event&)` 改成 `HandleEvent(const PlatformEvent&)`，自建事件用 `HandleNativeEvent`。

### IP-4 注入、觀測、查詢、PRIVATE 連結

狀態：ip4a～ip4c 完成（2026-10-22）；ip4d（突變全跑、L3）待做。

- ip4a、ip4b：只有加法。查詢、操作、注入、`EventWatch`、`SaveCaptureBmp` 都是新的 API，未啟用產品不需改。
- **破損（ip4c，重新啟用時一定要處理）**：後端改成 PRIVATE 連 SDL3 之後，一般目標不再傳遞 SDL 的 include 路徑與連結。直接 include `<SDL3/...>` 的未啟用目標，要自己連 `GYO::PlatformSDLNative`（或其他 `*Native`，或 `SDL3::SDL3`），否則找不到 SDL 標頭。目前的位置：
  - `apps/object_fps{,_v2}`：`main.cpp` 與 `src/App`；
  - `tests/object_fps{,_v2}`；
  - `tools/object_fps_preview`；
  - `build/acceptance/object_fps{,_v2}`（含 diagnostics）。
  連結 `*Native` 等於宣告直接使用 SDL（`platform-sdl.md` Design Invariant 2）。如果重新啟用時改用 Engine 的 API（`QueryWindow`、`SdlInputInjector`、`EventWatch`、進入點），可以不連。

## 4. TT（時間、執行緒與 Trace 計畫）

TT 計畫的批次完成時，由該批更新本節。

### TT-1 Time 與 Threads

狀態：TT-1 實作與 L1 完成（2026-10-08）。

- 破損：無。`RuntimeLoop` 的時鐘型別改為 `Engine::Time::MonotonicClock`，它就是 `std::chrono::steady_clock`，private 成員之外沒有介面變化；`FrameContext.deltaSeconds` 的定義不變。
- 連結：`GYO::Engine` 多了 PUBLIC 依賴 `GYO::Time`（只依賴 Base 與 OS 函式庫）；未啟用產品不需要改 CMake。
- 選用：未啟用產品若要等待或開角色執行緒，使用 `Engine::Time::Waiter` 與 `Engine::Threads::RoleThread`，不要直接用 `SDL_Delay` 或手寫 cv。

### TT-2 Trace

狀態：TT-2 實作與本機 L1 完成（2026-10-20），未合併。後續：2026-10-21 隨 PR #82 合併（`5ee499a`）。

- 破損：無。新增的 `GYO::Trace` 與 `Log.hpp` 都是新介面；SdlGpu 的慢 acquire 日誌從 `SDL_Log` 改為 `Engine::Base::Log`，預設輸出到 `std::cerr`（文字為 `[SDL_GPU] info: slow swapchain acquire …`）。依賴 SDL 日誌輸出這一行的未啟用產品，安裝自己的 handler 轉給 SDL 即可。
- 連結：只有用到 `GYO::Trace` 的目標需要連結它；`GYO::Engine` 不依賴它。
- 選用：未啟用產品若要背景寫紀錄檔，使用 `Engine::Trace::TraceSink`，不要自己開執行緒輪詢。


## 5. OS（OS 子系統計畫）

OS 子系統計畫（[os-subsystem](../os-subsystem/README.md)）的批次完成時，由該批更新本節。

### os-skeleton 建立 engine/os 與守衛

狀態：完成（2026-10-25，四平台 L1 通過），未合併（併進消費端的發行線 PR，D46）。

- 破損：無。新增 `GYO::Os`（只依賴 Base）與 `OsError`；沒有任何既有介面改變。
- 守衛：`os.boundary_guard` 這一批只掃 engine 與 tests/common。（os-guard-wide 實跑後更正：未啟用產品沒有任何發現，不需要 allowlist，見下方 os-guard-wide。）
- 選用：未啟用產品若要處理 OS 差異，等對應部件完成後使用 `engine/os`，不要自己寫 `#if`。

### io-contracts engine/io 的路徑契約

狀態：完成（2026-10-25，四平台 L1 通過），未合併（併進消費端的發行線 PR，D46）。

- 破損：無編譯破損。行為變更：
  - `AssetPathResolver` 在允許 scheme（預設）時，`file:///x`、`file://localhost/x` 現在是絕對路徑而被拒；過去會被當成 assets 根目錄下的相對路徑。未啟用產品的 shader bundle resolver 也用預設值，但它們的 manifest 沒有 file:// 條目。
  - `MountTable::Resolve` 拒絕含 `..` 段的請求。
- 選用：未啟用產品若要把原生絕對路徑交給 engine/io，用 `FileUriFromNativePath`／`NativePathFromFileUri`，不要自己拼 `file://` 字串。

### os-files-read 唯讀檔案原語與原生檔案系統

狀態：完成（2026-10-25，四平台 L1 通過），未合併（併進消費端的發行線 PR，D46）。

- 破損：無。新增 `Engine::Os::Files`（`GYO::Os`）與 `Engine::IO::FS::NativeFileSystem`；`GYO::Engine` 多了 PRIVATE 依賴 `GYO::Os`，未啟用產品不需要改 CMake。
- 選用：未啟用產品若要讀原生檔案，經 engine/io（`NativeFileSystem` 或掛上它的 `Vfs`），或直接用 `Engine::Os::Files`；路徑一律交 UTF-8（`PathToUtf8`，回傳 Result，D47），不要用 `path::string()`。

### os-guard-wide 守衛擴到產品、工具、產品測試與驗收程式

狀態：完成（2026-10-25，四平台 L1 通過），未合併。

- 破損：無。沒有改任何產品或 Engine 的程式碼。
- 守衛：`os.boundary_guard` 現在掃 engine、apps、tools、build/acceptance、tests；R4（直接開檔）也套用到 apps 與 tools。`apps/object_fps`、`apps/object_fps_v2`、`tests/object_fps`、`tests/object_fps_v2`、`build/acceptance/object_fps{,_v2}` 實跑都沒有發現，所以沒有 allowlist。重新啟用時若新增 OS 標頭、OS 巨集、列管的 OS API 或直接開檔，會被守衛擋下；真的需要時，在該產品自己的目錄放 `os_boundary_allowlist.txt`，只能列自己目錄下的檔案。
- 設定期檢查：產品的 target 與目錄不可以有識別 OS 的 compile definition（名稱含 `WIN32`、`APPLE`、`LINUX` 等 OS 字，或以 `$<PLATFORM_ID:...>` 為條件）。未啟用產品現有的 `NOMINMAX` 只設定 Windows 標頭、不識別 OS，可以保留。
- 選用：無。

### os-process-info 程序事實

狀態：完成（2026-10-25，四平台 L1 通過），未合併。

- 破損：無。新增 `Engine::Os::Process`（`GYO::Os`），沒有改任何既有介面。
- 選用：未啟用產品若需要執行檔路徑、CPU 時間或本地時間，用 `Engine::Os::Process`（`ExecutablePath`、`ProcessCpuTime`、`CurrentThreadCpuTime`、`ToLocalTime`），不要自己寫 `localtime_s`／`localtime_r` 或各平台的查法；時間戳的格式由產品自己決定。資源目錄仍用 platform/sdl 的 `BaseDirectory`，它和執行檔路徑是不同的事實。

### os-console-shutdown 命令列、主控台輸出、結束要求

狀態：完成（2026-10-26，四平台 L1 通過），未合併。

- 破損：無。新增 `Engine::Os::Console`（`GYO::Os`），沒有改任何既有介面。未啟用產品走 SDL 入口，argv 本來就是 UTF-8。
- 選用：未啟用產品若有不經 SDL 入口的主控台程式，用 `CommandLineUtf8` 取得 UTF-8 參數、`Utf8ConsoleOutput` 讓 Windows 主控台顯示 UTF-8、`InstallShutdownRequests`／`RequestedShutdown` 接結束要求；不要自己寫 `std::signal` 或 `SetConsoleCtrlHandler`。

### os-files-write 寫入原語

狀態：本機進行中（2026-10-26）；未合併。

- 破損：無。`NativeFileSystem` 多了寫入與建目錄（`Capabilities()` 的 canOpenWrite、canCreateDirectories 改成 true）；只用讀取的呼叫端不受影響。未啟用產品沒有直接開檔的站點。
- 選用：未啟用產品若要寫檔或建目錄，經 engine/io 的 `NativeFileSystem`（`Write | CreateIfMissing` 加 `Truncate` 或 `Append`；要和以前的文字模式 `std::ofstream` 相同的位元組時加 `Text`），原生路徑用 `FS::NativeFileUri` 轉成 URI；不要用 fstream 或 `std::filesystem` 的檔案操作。
