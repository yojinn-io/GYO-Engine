# framework 整改：批次計畫（第 1 版草案）

更新：2026-11-01。基準：origin/master `6e78090b`（與 `ebfe3225` 只差 `docs/checkup/`）。
**狀態：第 1 版，使用者 2026-11-01 核准**（含 README 的 D-1～D-9，全部採建議）。

規劃方式：ultracode（核對現況 1、獨立提案 3、審查合成 1、Sonnet 對抗式檢查 1）。對抗式檢查提出 1 個 blocker、9 個 major／minor，本版已逐條處理（第 7 節）。
證據：使用者的 Mac，主 checkout `build/evidence/framework-rework/`：`2026_11_01_plan-workflow-output.json`（各 Agent 的結論）、`2026_11_01_plan-workflow-journal.jsonl`（逐 Agent 回傳值）。三個提案的原文只在 journal 裡。

## 1. 批次總覽

```text
framework-rework
├── v8-framework-rework-plan         medium｜本文件與 README、HANDOFF（核准即完成）
├── v8-framework-guard-ratchet       high｜framework 守衛一次裝好＋例外清單；DI 2、DI 5、Replacement Criteria 提前改寫
├── v8-engine-platform-path-dialog   high＋xhigh 審查（回呼、信箱、生命週期）｜Engine 平台層的選擇窗
├── v8-engine-io-user-files          high＋xhigh 審查（路徑包含判定）｜engine/io 補使用者檔案的缺口
├── v8-framework-shell               high（草圖先核准）｜無視窗外殼、有視窗外殼、ImGui 綁定
├── v8-framework-catalog-structure   high｜登錄拆成核心結構 API 與 JSON 綁定；cases.json 對齊
├── v8-framework-catalog-extra-fields  high｜D-6：更新條目時丟掉額外欄位的缺陷，先 Red 後 Green
└── v8-framework-invariants          high（文件與完整突變 medium）｜刪舊件、例外清單清空、DI 4 與文件定稿
```

| 代號 | 白話 | 依賴 | 估時 | PR |
|---|---|---|---|---|
| v8-framework-rework-plan | 定計畫 | 無 | 2～3 h | A |
| v8-framework-guard-ratchet | 先把守衛的規則裝好，舊違規列進只減不增的清單 | plan | 7～9 h | A |
| v8-engine-platform-path-dialog | 選擇窗的 OS 半進 Engine | plan | 12～16 h | A |
| v8-engine-io-user-files | 檔案戳記、原子寫建父目錄、路徑包含判定進 engine/io | plan | 6～8 h | A |
| v8-framework-shell | 產品外殼與 ImGui 綁定 | guard-ratchet | 12～16 h | A |
| v8-framework-catalog-structure | catalog 登錄的結構與 JSON 文字分開 | guard-ratchet | 8～12 h | A |
| v8-framework-catalog-extra-fields | 修更新條目時丟掉額外欄位的缺陷 | catalog-structure | 2～3 h | A |
| v8-framework-invariants | 工具切換後收尾：刪舊件、文件定稿 | 以上全部＋工具線 T1、T2、T3a、T3b 合併 | 6～9 h | B |

合計約 57～78 h，不含核准等待與工具線（工具線另約 20～30 h）。任一批超過估時 1.5 倍，或單一問題累計超過 2 h，就停下回報（記憶：時間盒）。

### 和 README 第一版草案不同的地方（都有證據）

- **刪除全部移到最後一批。** 工具還在用 framework 的 `PathDialog`（`tools/ui_editor/include/gyo/ui_editor/PathDialog.hpp:3`）、`GYO::ToolFrameworkGui`（`tools/ui_editor/CMakeLists.txt:49-60` 找不到就 FATAL_ERROR）、舊的 `RegisterAsset`（`tools/audio_editor/src/SoundExport.cpp:90`）。所以第 1～5 批只做加法，舊件不動，等工具切換後才刪。
- **T1 不能在選擇窗那批合併後就開始。** 舊宿主只交出 `SDL_Window*`／`SDL_Renderer*`（`ImGuiSdlHost.hpp:20-23`），工具拿不到 `SdlPlatform&`。預設 T1 與 T3a 一起做（決策 D-4）。
- **守衛與 Replacement Criteria 提前。** AGENTS §3 要求 Replacement Criteria 在堆功能前寫。
- **外殼與 catalog 拆成兩批。** 兩者的使用者、核准項目和工具側交接都不同；仍在同一條 PR。
- **跨執行緒的信箱歸 Engine，不歸工具**（決策 D-3）。
- **JSON 綁定要自己具名連 nlohmann。** Engine 的 nlohmann 是 PRIVATE（`engine/CMakeLists.txt:76`），不會傳遞。
- **cases.json 不用補案例。** 已有 54 個案例，含 `duplicate-id-across-catalogs`、`shader-overlaps-asset`、`asset-path-control`。真正的缺口是 framework 沒有測試讀它，而且每個案例都是整套內容。
- **engine/io 那批改 high。** 它碰到 io.md 的 DI 2、DI 3 與 RC 3。

## 2. 每批的內容與驗收

共通的完成條件（每批都有，下面不重複）：
- dev_log 與該批同一變更（`docs/dev_logs/2026_MM_DD_framework_rework_<批>.zh-Hant.md`），寫結論與判定用的數值；證據放使用者 Mac 主 checkout 的 `build/evidence/framework-rework/<批代號>/`，dev_log 寫明機器與路徑。README 的一覽同步更新。
- 有突變的批次：結果出來前宣告每條突變的 file、find、預期；報告第一行寫「件數の突き合わせ」（宣告數＝執行數＝killed 數，stale 為 0）。依 D184 只跑相關的。
- L1 驗收只看 ci-windows、ci-linux、ci-macos 三列。core preset 的 `GYO_TOOLS` 為空（`CMakePresets.json`），不建 framework，所以 core 列只用來證明 Engine 側沒壞。
- 合併後用 SendMessage 通知工具線：合併 commit、公開介面的變更、工具側要改的地方。

### 2.1 v8-framework-guard-ratchet（high）

1. `tests/framework/framework_dependency_guard.py` 依目錄判定：`framework/bindings/<name>/` 是綁定，其餘都是核心。
   - 核心 include：禁止 `SDL3/`、`imgui` 開頭（有無斜線都算，補 `:70-71` 的洞）、`backends/`、`nlohmann/`、`engine/**/native/` 與 `*Native.hpp`（補 `:29` 前綴 `engine/` 放行 Native 標頭的洞）。保留正向清單，和禁止規則取交集，避免未知前綴被放過。
   - 核心符號：禁止 `\bSDL_\w+`、`\bImGui\w*`、`nlohmann`、`struct SDL_`（補 `:25` 只掃 GPU 符號的洞）。
   - 核心 CMake：禁止 `SDL3::*`、`imgui::*`、`nlohmann_json::*`、`GYO::*Native`。
   - 現有禁止項（game、`GYO::Input`、SDL_GPU、已登錄 id）照舊。
2. 兩個綁定的正向清單在這批寫好，第 4、5 批就不必改守衛。
   - `bindings/imgui`：include 允許 `gyo/framework/`、`engine/` 的中立標頭、`engine/platform/sdl` 與 `engine/render/backend/sdl` 的 Native 標頭、`imgui.h`、`backends/imgui_impl_sdl3.h`、`backends/imgui_impl_sdlrenderer3.h`；原始碼不得有 `SDL_` 符號，不得具名 `SDL3::SDL3`。
   - `bindings/json`：include 允許 `gyo/framework/`、`engine/` 的中立標頭、`nlohmann/`；禁止任何 Native 標頭。
3. 例外清單 `tests/framework/framework_guard_allowlist.txt`：逐條記「path、規則、次數上限」，次數只准減少。增加就失敗；減少屬收緊，Agent 自己改、事後報告（§14.4）。自我測試：條目已不再違規（stale）時失敗；新違規不在清單時失敗；已列檔案裡多出一個違規時失敗。
4. 舊的 gui target 從 `framework/CMakeLists.txt` 搬到獨立的 CMake 檔，讓 `framework/CMakeLists.txt` 本身零例外（第 4、5 批要在那裡加新 target）。
5. configure 時的連結閉包檢查，比照 platform-sdl 的 G3（`tests/common/platform_sdl/CMakeLists.txt:60-94`）：核心 target 的連結閉包不得有 SDL3、imgui、nlohmann、`*Native`；Engine 的 nlohmann 以 `$<LINK_ONLY:>` 出現可接受。
6. `docs/framework/architecture.md` 改寫 DI 2、DI 5、Replacement Criteria（條文草案見第 4 節）。編號保持 1～5，`docs/ui_toolchain.md:74-83` 的引用不失效。
7. fw-* 突變目錄搬到本資料夾 `scripts/framework_mutations.json`；`docs/tools/plans/v8/scripts/audio_editor_mutations.json` 的說明同步。新突變一律錨在第 6 批之後仍存在的檔案（`ReadOnlyAssetCatalog.cpp`、`framework/CMakeLists.txt` 等）。

驗收：L1 三列綠，含 `gyo_framework.dependency_guard` 與它的自我測試、configure 檢查。
突變（事前宣告，全部 killed）：核心 include `<imgui.h>`、核心標頭 `struct SDL_Window;`、核心 include Native 標頭、核心連 nlohmann、ImGui 綁定呼叫 `SDL_`、JSON 綁定 include `imgui.h`、JSON 綁定 include `SdlPlatformNative.hpp`、清單 stale、清單外新違規、已列檔案多一個 `SDL3::SDL3`、連結閉包帶 SDL3。
§14.4：出示 1～5 項。「沒有放寬」的證明是舊自我測試的失敗案例與舊目錄中被殺的 fw-guard-* 突變在新判定下仍失敗或被殺，由提案者以外的審查者用實際資料核對。

### 2.2 v8-engine-platform-path-dialog（high；回呼、信箱、生命週期與 noexcept 做 xhigh 審查＋別的引擎做一次對抗式檢查）

介面草圖（中立標頭 `SdlPlatform.hpp`，不得有 `SDL_` 符號，G1／G2 守）：

```cpp
enum class PathDialogKind : std::uint8_t { OpenFile = 1, SaveFile, ChooseFolder };
struct PathDialogFilter final { std::string name; std::string pattern; };   // "json", "wav;wave", "*"; UTF-8
struct PathDialogRequest final {
    std::uint64_t tag{};                    // the caller's; handed back unchanged, never read
    PathDialogKind kind{PathDialogKind::OpenFile};
    std::vector<PathDialogFilter> filters;  // ignored for ChooseFolder
    std::string defaultLocation;            // UTF-8; empty: the OS chooses
};
enum class PathDialogOutcome : std::uint8_t { Chosen = 1, Cancelled, Failed };
struct PathDialogAnswer final { std::uint64_t tag{}; PathDialogOutcome outcome{PathDialogOutcome::Failed};
                                std::string path; std::string error; };
// SdlPlatform:
void ShowPathDialog(PathDialogRequest request);                       // operation (DI 7), main thread
[[nodiscard]] std::vector<PathDialogAnswer> TakePathDialogAnswers();  // main thread, no SDL call
```

Native 標頭的測試縫（比照 `ClockReaders`，`SdlPlatformNative.hpp:29-36`；正式路徑也經過它）：

```cpp
using PathDialogCallback = void(SDLCALL*)(void*, const char* const*, int) noexcept;  // static_assert target
struct PathDialogLauncher final {
    void (*show)(PathDialogCallback callback, void* userdata, SDL_Window* window, PathDialogKind kind,
                 const SDL_DialogFileFilter* filters, int filterCount, const char* defaultLocation);
};
void ShowPathDialogWith(SdlPlatform& platform, PathDialogRequest request, const PathDialogLauncher& launcher);
```

- 實作：平台持有 `shared_ptr` 信箱（mutex、closed 旗標、答案串列）。Show 時預先配好節點，節點擁有 filter 與預設位置的字串（`SDL_dialog.h:145-146` 要求活到回呼）。呼叫 SDL 時不持鎖，因為回呼可能同步發生。
- 回呼跳板 noexcept：只捕捉 `std::bad_alloc`（改成 Failed），鎖內 splice 節點（不配置、不丟），信箱已 closed 就丟棄，不碰平台，不呼叫 `GYO_ASSERT`（測試的斷言處理器會丟例外，在 noexcept 裡等於 terminate）。
- 平台銷毀時鎖內設 closed；之後才到的回呼仍安全，信箱由未回覆的節點共同持有。
- 「kind 對應哪個 `SDL_Show*`」抽成純函式，單元測試並加突變，補上假 launcher 跑不到真實 show 的缺口。
- 用 Take 輪詢，不把答案放進 PumpEvents 的事件：放進事件會讓 `PlatformEvent` 對 SdlInput 以外公開欄位，觸發 platform-sdl RC 3。
- 測試（`platform_sdl_tests`，dummy 驅動，只經假 launcher）：同步回呼、另一執行緒回呼、平台銷毀後回呼、`files==nullptr`→Failed、`files[0]==nullptr`→Cancelled、tag 與到達順序、每個答案只交出一次、filter 指標活到回呼、observer 內 Show 觸發 `GYO_ASSERT`。跨執行緒案例不放進已知在 linux 會逾時的 `platform_sdl_concurrency` 篩選。
- platform-sdl.md：Contracts 新增「Path dialogs」；新增 DI 10；DI 7 補「ShowPathDialog 是 operation」；DI 4 補「launcher 縫不寫輸入 frame」；Replacement Criteria 2 的例外改成「ImGui 綁定，以及 ui_editor 的資產預覽（直到 Engine 有貼圖 API）」；Ownership 加選擇窗的信箱。
- framework 的 `SdlPathDialogs`、`PathDialog` 不動。

驗收：L1 三列的 `platform_sdl_tests`、`platform_sdl_header_tests`（G1／G2）、G3 configure 都綠；`gyo_ui_editor.*`、`gyo_framework.*` 不變並通過。
另加一次本機 ASan／UBSan 執行，含只有 sanitizer 抓得到的兩條突變（filter 提早釋放、信箱不共享）；CI 沒有 sanitizer，DI 10 的守衛欄寫明「這兩條只由本機 sanitizer 守」。
突變（事前宣告，全部 killed）：丟答案、tag 對調、順序、Take 不清空、Cancelled 當 Failed、observer 斷言、kind 對應、noexcept（預期建置失敗，對 `PathDialogCallback` 做 static_assert）、中立標頭加 `SDL_DialogFileFilter`（G2）。
範圍的限界（一路帶到 T1 的事前宣告）：真實 OS 對話窗的回呼執行緒、Linux portal 需要 pump、平台或父視窗銷毀時的行為，CI 都量不到，由 T1 的 L3 實機確認，Windows 優先。

工具側 T1（ui_editor，預設與 T3a 一起）：`SdlPathDialogs` 改收 `SdlPlatform&`，只留用途↔tag 對照與每幀套用；`PathDialog.hpp` 不再 include framework；CMake 具名 `GYO::PlatformSDL`，守衛放寬（§14.4）；ui_toolchain.md DI 1 改寫。`pathDialogOpen_`（`EditorApp.cpp:128,173-174`）要有「答案不來」時的恢復規則（DI 10 只保證最多一個答案）；T1 的 L3 測選檔、取消、非 ASCII 路徑、對話窗開著時關視窗、答案不來。

### 2.3 v8-engine-io-user-files（high；`IsNativePathWithin` 的語義做 xhigh 審查）

只補 Engine 缺的能力，路徑一律 UTF-8，錯誤一律 `IoError`：

```cpp
// engine/io/include/engine/io/helpers/UserFiles.hpp
struct FileStamp final { bool exists{}; std::uint64_t sizeBytes{}; std::int64_t mtimeNs{}; std::uint64_t contentHash{}; };
IoResult<FileStamp> ProbeFileStamp(FS::IFileSystem& fs, const Path::Uri& uri, std::size_t maxBytes);  // missing → exists=false
// engine/io/include/engine/io/helpers/WriteFileAtomically.hpp（擴充）
struct WriteAtomicallyOptions final { bool createParentDirectories = false; };
IoResult<void> WriteFileAtomically(FS::IFileSystem&, const Path::Uri&, Base::ConstSpan<std::byte>, const WriteAtomicallyOptions&);
// engine/io/include/engine/io/fs/NativePaths.hpp
IoResult<bool> IsNativePathWithin(std::string_view utf8Candidate, std::string_view utf8Root);  // via Os::Files::ResolvedPath
```

- 不新增：ReadTextFile（用 `ReadAllBytes`，呼叫者傳 maxBytes；不用 `ReadAllTextUtf8`，它剝 BOM）、PathExists（用 `IFileSystem::Exists`）。AbsolutePath 的去處見決策 D-2。
- `ProbeFileStamp` 一定由呼叫者給 maxBytes。舊實作讀整檔沒有上限，預設 64 MB（`FileAllCommon.hpp:26`）會讓大檔變成錯誤。
- 新舊語義不同的地方先寫對照測試，不宣稱行為不變：舊 `IsPathWithin` 先 `lexically_normal` 再只解析 parent，新的解析整條路徑。最後一段是符號連結、含 `..` 與符號連結、不存在的父目錄、大小寫、UNC 都要列。有差異就交使用者決定採哪個，寫進 dev_log。
- framework 的 `FileService` 這批不改。
- io.md：Contracts 新增「User files」；RC 3 句尾記一句「`IsNativePathWithin` is such a helper」，正規化地點的清單不變。

驗收：L1 三列與 core 列綠；`tests/common/core/io` 新案例（非 ASCII、檔案不存在、對象是目錄、`/a/bc` 不在 `/a/b` 內、符號連結、Windows 大小寫、長路徑與 UNC、建父目錄）；`os.boundary_guard` 不變並通過。
突變（全部 killed）：包含判定改字串前綴、不解析、不存在當錯誤、雜湊、不建父目錄。

工具側 T2：ui_editor 的 `DocumentSession` 改呼叫 engine/io；audio_editor 的讀寫改用 `ReadAllBytes` 與 `WriteFileAtomically`（暫存檔名會變），CMake 具名 `GYO::Engine`，守衛加 `engine/io/` 與 `GYO::Engine`（放寬，§14.4）。

### 2.4 v8-framework-shell（high；關卡 0：草圖先交核准）

```cpp
// 核心 GYO::ToolFramework（任何組態都會建，不帶 SDL）
using HeadlessMain = std::function<int(std::span<const std::string> arguments)>;  // UTF-8
using CommandLineFailure = std::function<int(std::string_view reason)>;
[[nodiscard]] int RunHeadlessShell(int argc, char* argv[], const HeadlessMain&, const CommandLineFailure&);

// 核心 GYO::ToolFrameworkWindow（條件：TARGET GYO::PlatformSDL；只連核心與中立的 GYO::PlatformSDL）
struct WindowShellOptions final { Engine::Platform::Sdl::SdlPlatformOptions platform; };  // no copied fields
class WindowShellClient { public: virtual ~WindowShellClient() = default;
    virtual void OnEvent(const Engine::Platform::Sdl::PlatformEvent&) {}          // queries only (DI 7)
    [[nodiscard]] virtual Engine::Runtime::RuntimeControl Update(const Engine::Runtime::FrameContext&) = 0;
    [[nodiscard]] virtual Engine::Runtime::RuntimeControl Render(const Engine::Runtime::FrameContext&) = 0;
    [[nodiscard]] virtual int ExitCode() const noexcept { return 0; } };
using WindowShellClientFactory = std::function<std::unique_ptr<WindowShellClient>(Engine::Platform::Sdl::SdlPlatform&)>;
[[nodiscard]] int RunWindowShell(const WindowShellOptions&, const WindowShellClientFactory&);  // no render device, no live frame

// ImGui 綁定 GYO::ToolFrameworkImGui（條件：IMGUI 且 SDL_RENDERER）
struct ImGuiShellContext final { Engine::Platform::Sdl::SdlPlatform& platform; Engine::Render::Backend::Sdl::SdlRenderer& renderer; };
class ImGuiShellClient { /* Initialize / BeginFrame / Draw / WantsExit / EndFrame, as ImGuiSdlClient today */ };
[[nodiscard]] int RunImGuiShell(const WindowShellOptions&, const ImGuiShellClientFactory&);
```

- 外殼不建立渲染裝置：client 拿到 `SdlPlatform&` 後自己建。這樣 SdlRenderer（ui_editor）與 SdlGpuRenderDevice（pvp）都接得上，不為 pvp 預先加掛鉤。
- `WindowShellOptions` 直接帶 Engine 的 `SdlPlatformOptions`，不複製欄位（pvp 要設 `imeComposition`，`PvpApplication.cpp:1039`）。
- 不加 live frame、停止要求、pvp 專用掛鉤（§1）。這些列進給 pvp owner 的候選說明，寫成「採用前要先改的項目」：live frame（`PvpApplication.cpp:1377-1395`）、GPU 裝置（:1047）、平台前的內容載入（:954-956）、停止要求（`match_main.cpp:118`）、framework DI 1 與 `build/cmake/GyoBuild.cmake:56` 的建置條件。
- ImGui 綁定：從 `ImGuiSdlHost.cpp` 複製字型、後端初始化與關閉、幀順序；`IniFilename`／`LogFilename` 設 nullptr；handle 只經 PRIVATE 的 `*Native`；`SDL_Log*` 改 Base log（行為變更，寫進 dev_log）。
- 舊的 `ImGuiSdlHost` 與 `GYO::ToolFrameworkGui` 不動。

驗收：L1 三列綠。`RunHeadlessShell`：argv 以 UTF-8 交給 main、命令列錯誤走回呼、結束碼原樣回傳。`RunWindowShell`（dummy 驅動）：factory 失敗回 1、client 結束碼、client 先於平台銷毀。ImGui 冒煙測試（dummy＋software renderer，前例 `tests/common/runtime_sdl/CMakeLists.txt:5`）：3 幀後結束碼 0、工作目錄沒有 imgui.ini；某平台建不起 renderer 就在事前宣告縮小平台並寫成限界。新目錄零例外。
L2（一次，本機）：`GYO_TOOLS` 為空、只選 audio_editor 各 configure 一次，確認後者沒有任何 SDL3 target。L1 沒有這兩個組態，寫成範圍的限界。
突變（全部 killed）：argv 不轉 UTF-8、client 晚於平台銷毀、factory 失敗的結束碼、寫 imgui.ini、ImGui 銷毀順序。

工具側 T3a（ui_editor）：GUI 用 `RunImGuiShell`，cli 用 `RunHeadlessShell`（命令列錯誤印 help 回 2）；預覽用 `NativeRenderer(context.renderer)`，ui_editor 自己連 `GYO::RenderBackendSDLNative`（platform-sdl RC 2 的新例外）；守衛加 `GYO::ToolFrameworkImGui`、`GYO::RenderBackendSDLNative`，拿掉 `GYO::ToolFrameworkGui`（§14.4）；突變 `ui-guard-native-target` 改寫。audio_editor 的 Main 改 `RunHeadlessShell`（併入 T3b）。

### 2.5 v8-framework-catalog-structure（high；關卡 0：決策 D-1）

- 核心（不帶 nlohmann）：條目型別用 Engine 的 `RawCatalogEntry`（欄位順序與 `AssetRegistration` 相同）；`RegistrationPolicy`（預設兩項都拒絕）；`PlanRegistration` 回傳新增或更新與 previousPath，並查單一 catalog 內的重複 id。路徑規則依 D-1。
- JSON 綁定 `GYO::ToolFrameworkJson`（隨核心建，自己具名 PRIVATE 連 nlohmann）：v1 合法性交給 `CatalogParser::Parse`（v1 規則只剩一份來源）；保留位元組的改寫從舊碼複製；寫完重新解析並比對全文，這是 DI 4 的位元組保留驗證，不是第二份 v1 規則。公開便利函式 `RegisterAsset(text, entry, policy)`，`SoundExport.cpp:90-92` 的呼叫形狀幾乎不變。
- `tests/framework` 讀 `cases.json`，用 `ContentManifest::Parse` 分類，只取「單一 catalog、沒有 shader bundle」的案例；適用與排除件數在實作前重算並事前宣告。
- 舊的 `CatalogRegistration`、它的 6 個測試、fw-catalog-* 與 fw-path-dot-dot 突變都保留，到第 6 批刪檔時一起退役。這批只新增，新 API 有自己的測試與突變。理由：在 T3b 前，audio_editor 還走舊 API，CLI 測試不查位元組保留。
- 已知缺陷（更新既有條目時丟掉額外欄位，`CatalogRegistration.cpp:84,189,208` 對照 cases.json 的 `valid-custom-type-and-metadata`）：這批照現行行為搬，由下一小批 v8-framework-catalog-extra-fields 依 §14.3 修（D-6）。

驗收：L1 三列與 core 列綠；cases.json 子集全部一致；`asset-path-control` 在舊規則先失敗（Red，存紀錄）、新規則通過；新 API 對 `convert_taken_path` 情境回拒絕；`gyo_audio_editor.cli` 的 `convert_taken_path` 仍回 4；`AssetContentContractTests` 照常通過。
突變（全部 killed）：拿掉控制字元檢查、改寫時動到其他條目、新 API 版的政策與重複 id。

工具側 T3b（audio_editor）：CMake 具名 `GYO::ToolFrameworkJson`，參數改 `RawCatalogEntry`，政策用預設值；守衛加 `GYO::ToolFrameworkJson`（§14.4）；`ae-guard-json-target` 改寫；audio_editor DI 3 的「unique ids」改寫成「catalog 內唯一；跨 catalog 與 shader 重疊在載入時由 ContentManifest 判定」。

### 2.6 v8-framework-invariants（high；文件與完整突變 medium）

前提：T1、T2、T3a、T3b 都已合併。開工前 `git grep` 確認 `tools/`、`apps/`、`framework/src`、`tests/` 都不再用 `ImGuiSdlHost`、`SdlPathDialogs`、`PathDialogInbox`、`FileService`、舊 `RegisterAsset`／`AssetRegistration`、`GYO::ToolFrameworkGui`。

1. 刪舊件與舊的 gui CMake 檔；`FrameworkError` 只留還有使用者的部分。
2. 例外清單清空，守衛改為「清單必須為空」（比照 platform-sdl G2）。
3. `docs/framework/architecture.md` 的 DI 4 與 Using it；`docs/architecture.md:48,90,246`。
4. 突變定版：舊 fw-catalog-*、fw-inbox-* 退役（由新 API 的突變與 pd-* 取代），其餘改錨。
5. 依 D184，PR 線合併前在背景完整跑一次突變。
6. 這條線的總 dev_log，README 完成清單，通知架構會話。

驗收：L1 三列綠、例外清單為空、configure 檢查無例外；`git grep -nE 'SDL_|nlohmann|imgui' framework/include framework/src` 無結果；完整突變的件數對帳。§14.4：清空屬收緊，仍出示 1～5 項並核准（DI 守衛）。

## 3. 範圍外（只記錄）

- framework/ 不在 `os_boundary_guard` 的 ROOTS（`tests/common/os/os_boundary_guard.py:26`）；加入屬 Engine OS 守衛的收緊，另案。
- `CatalogFormat.hpp` 沒有任何檔案 include（健檢 :80）。
- `ReadOnlyAssetCatalog::Mount` 不經 ContentManifest（`ReadOnlyAssetCatalog.cpp:38-66`），用它掛載時不查 shader 重疊與跨 catalog 重複。
- target 與命名空間（`Gyo::Tools::Framework`、`GYO::ToolFramework*`）改名，等遊戲真正使用時再處理。
- SDL renderer 的貼圖 API、pvp 搬上外殼、遊戲 framework 的其他部分、Gateway、未啟用產品、素材結構（README「不做」）。

## 4. Design Invariants 草案（英文，照原文件語言；逐批核准後才寫入）

**docs/framework/architecture.md**（DI 2、DI 5、Replacement Criteria 在 guard-ratchet；DI 4、Using it 在 invariants）

- DI 2：Dependencies point one way: game or tool → framework → engine. The framework includes nothing from apps/ or tools/, and the engine never names it; it links no game, no GYO::Input and no SDL_GPU. The core (everything outside framework/bindings/) includes no SDL3/, imgui*, backends/, nlohmann/ or *Native header, names no SDL_, ImGui or nlohmann symbol, and links no SDL3::*, imgui::*, nlohmann_json::* or GYO::*Native: it reaches SDL only through Engine's neutral targets. A binding is its own target under framework/bindings/<name>/ and the core never names it. Only the ImGui binding links GYO::*Native, PRIVATE; no framework target names SDL3::SDL3 or an SDL_ symbol. Guards: the dependency guard (core rules, one positive list per binding, an allowlist whose counts only go down and that is empty after v8-framework-invariants) and the configure-time link check.
- DI 3：保留；刪「dialog filters」例子；補 Registration policy is chosen by the caller's options, never by which product calls.
- DI 4（invariants 批）：The ImGui binding writes no side file (imgui.ini, ImGui's log). The catalog mount is read-only. Writing a catalog changes only the entry with the caller's id; the JSON binding leaves every other byte as it was, the result is re-read and checked as data, and the file is replaced atomically. Catalog-level rules follow the catalog-level cases of tests/common/fixtures/asset_contract/cases.json; cross-catalog and shader-bundle rules are decided at load by ContentManifest.（選 b-lite 時加：the path rule is Engine's.）
- DI 5：The core is the product shell: it puts Engine capabilities together and hands control to the product (a headless and a windowed shell, content mounting, registration policy). It holds no product's UI, document format or rules, and names no concrete UI or data-format library; such a library enters only as a binding. A capability Engine lacks is added to Engine, not to the framework. Otherwise only what two or more products share enters. The game part enters only on the condition above. 註：有視窗外殼目前只有 ui_editor 一個使用者，是使用者接受的預測壓力（決策 D-8）。
- Replacement Criteria：保留「要服務產品就得知道該產品的意義」「每新增一個工具都要改核心的公開介面」；新增「核心要直接依靠具體的 UI 或 JSON 函式庫才能服務呼叫者」「綁定模組要知道哪個產品在呼叫才能運作」「有視窗外殼要為 client 建立或選擇渲染裝置」「framework 抄了 Engine 的規則，而沒有共用 fixture 檢查」；刪「工具需要 ImGui on SDL renderer 以外的 GUI」。pvp 採用時要改的項目不寫成 Replacement Criteria，寫在 pvp 候選說明裡。
- 「換掉 ImGui」的實際範圍：換綁定，加上工具自己的 Draw 程式；核心外殼與 Engine 不動。比原則 3 的字面弱，要寫進 DI 5 的說明。

**docs/architecture/platform-sdl.md**（path-dialog 批）

- DI 10：At most one answer is handed out per request, and exactly one when the OS calls back; answers are handed out only on the main thread by TakePathDialogAnswers, in callback order, each once. SDL's dialog callback is noexcept, never touches the platform and never asserts; its mailbox outlives the platform, and an answer that arrives after the platform is destroyed is dropped. Filters and the default location live until the callback. The platform keeps no count of open dialogs for any decision and never refuses, merges or deduplicates a request. Known limit: SDL 3.4 has no way to cancel a dialog, and on Linux an answer may need the program to keep pumping events. Guards: ShowPathDialogWith tests with a fake launcher; mutants pd-*; the filter-lifetime and shared-mailbox mutants only under a local sanitizer run.
- DI 7 補：ShowPathDialog is an operation. DI 4 補：The path dialog launcher seam replaces only the request to the OS; it writes no input frame.
- Replacement Criteria 2：Apart from the framework's ImGui binding, and ui_editor's asset preview until the SDL renderer has a texture API, an active product needs a *Native target for a new capability ...

**docs/architecture/io.md**（io 批）：DI 不變；Contracts 新增「User files」；RC 3 句尾記一句。

## 5. 守衛策略

建議 ratchet（決策 D-5）：規則在 guard-ratchet 批一次裝好，舊違規進只減不增的例外清單，最後一批清空。§14.4 的核准關卡集中在 guard-ratchet 與 invariants 兩批；中途清單只會減少，屬收緊，事後報告。
另一案是最後才收緊：少一段暫時機制（約 2 h），但第 4、5 批的新程式碼只受有洞的舊守衛保護（例如 `<imgui.h>` 抓不到），返工集中在最後。

工具側的守衛（工具線提出，各自 §14.4 核准）：ui_editor 在 T1 加 `GYO::PlatformSDL`，T3a 加 `GYO::ToolFrameworkImGui`、`GYO::RenderBackendSDLNative` 並拿掉 `GYO::ToolFrameworkGui`；audio_editor 在 T2 加 `engine/io/`、`GYO::Engine`，T3b 加 `GYO::ToolFrameworkJson`。

## 6. Architecture Delta

| 代號 | 內容 | 依賴的變化 | Ownership 的變化 |
|---|---|---|---|
| AD1 | Engine 平台層新增選擇窗（path-dialog 批） | ui_editor → `GYO::PlatformSDL`（新增，方向正確）；framework 不再碰 SDL 對話窗 | 跨執行緒信箱與生命週期歸 Engine；用途對照與每幀套用歸 ui_editor |
| AD2 | engine/io 新增使用者檔案輔助（io 批） | audio_editor 具名 `GYO::Engine`（原本經 framework 的 PUBLIC 間接取得） | 使用者路徑的能力歸 Engine；路徑型別轉換歸工具 |
| AD3 | framework 拆核心與綁定、新增產品外殼（shell、catalog、invariants 批） | 核心只連 Engine 中立 target；只有 ImGui 綁定 PRIVATE 連 `*Native` | 外殼與登錄政策歸核心；字型、ImGui 後端、JSON 改寫歸綁定；v1 合法性回到 Engine `CatalogParser` |
| AD4 | 只在 D-1 選 b-lite 或 (b)：engine/asset 公開既有規則 | 不變 | C++ 的路徑規則只剩一份 |
| AD5 | framework 守衛結構（guard-ratchet 批） | 不變 | 突變目錄的 owner 從 docs/tools/plans/v8 移到本計畫 |

每項的 AGENTS §3 七點說明在證據檔 `2026_11_01_plan-workflow-output.json` 的 `architectureDeltas`；核准時逐項出示。

## 7. 對抗式檢查的處理

| 嚴重度 | 發現 | 處理 |
|---|---|---|
| blocker | 刪 FileService 會弄壞核心的 `ReadOnlyAssetCatalog`（`ReadOnlyAssetCatalog.cpp:8,29,46,64` 用 `AbsolutePath`），工具與核心的路徑等同判定可能分歧 | 決策 D-2；invariants 批的前置 grep 涵蓋 framework/src 與 tests/ |
| major | 不記次數的例外清單會遮住已列檔案的新違規 | 改成逐條記次數、只減不增；舊 gui target 搬出 `framework/CMakeLists.txt`；加突變 |
| major | DI 10「恰好一個答案」與已知限界矛盾，工具的 `pathDialogOpen_` 會卡住 | 改「最多一個；OS 回呼時恰好一個」；T1 要有恢復規則與 L3 案例 |
| major | 生命週期類突變 CI 抓不到；真實 show 一次都不跑 | 本機 ASan／UBSan 列入完成條件；kind 對應抽純函式加突變 |
| major | noexcept 突變照原設計殺不到 | Native 標頭自訂帶 noexcept 的回呼型別 |
| major | 舊 `CatalogRegistration` 在 T3b 前沒有測試守 | 舊測試與舊突變留到最後一批才退役 |
| major | FileService 轉呼叫後「行為不變」不成立 | FileService 這批不改；先寫新舊對照測試，差異交使用者決定；`ProbeFileStamp` 一定傳 maxBytes |
| major | core 列不建 framework，驗收寫錯 | 驗收只看 ci 三列；可移除性改由 L2 兩次 configure |
| major | 視窗外殼與 pvp 對不上，pvp 寫成 Replacement Criteria 是預測壓力 | options 直接帶 `SdlPlatformOptions`；pvp 的項目移到候選說明 |
| minor | 綁定清單允許 `engine/` 等於放行 Native | JSON 綁定禁 Native；ImGui 綁定限定兩個 Native 目錄；加突變 |
| minor | 新突變沒寫錨點，可能錨在會刪的檔案 | 事前宣告逐條寫 file、find、預期，錨在留下的檔案 |
| minor | 「第二個解析器消失」說過頭 | 改寫成「v1 規則只剩一份來源」 |
| minor | 已知缺陷被指給工具線，但程式碼屬 framework | 決策 D-6 |
| minor | 各批沒寫 dev_log 與件數對帳 | 第 2 節共通條件 |

## 8. 決策

見 README 的「決策」節與 HANDOFF 的決策紀錄。
