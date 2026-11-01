# v8-framework-shell 第 0 關：外殼草圖（待核准）

日期：2026-11-01。依 PLAN 第 2.4 節「草圖先交核准」與 D-9（ultracode）。
**狀態：待使用者核准。** 核准前不寫程式、不改 `docs/framework/architecture.md` 與 `docs/architecture/platform-sdl.md`。

做法：獨立提案 3（引擎、檔位繼承主會話）、審查合成 1（繼承）、Sonnet 對抗式檢查 1（檔位繼承），全部唯讀。證據：使用者的 Mac，主 checkout `build/evidence/framework-rework/v8-framework-shell/`（`2026_11_01_sketch-workflow-output.json` 各 Agent 的結論、`…-journal.jsonl` 逐 Agent 回傳值、`…-script.js` 腳本）。

## 1. 三個提案

| 提案 | 形狀 | 審查分數 | 沒被選的原因／被接過來的部分 |
|---|---|---|---|
| A | PLAN 第 2.4 節補完：外殼擁有 RuntimeLoop，client 介面貼近現有 ImGuiSdlHost | 8.5 | 採用為骨架 |
| B | SDL3 main callbacks（init／iterate／event／quit） | 7.5 | SDL 的 callback 機制本身不能用：`SDL_QuitMainCallbacks` 會呼叫 `SDL_Quit`（違反 platform-sdl DI 8），generic 驅動只回 0／1（工具的結束碼 2、3、4 會丟）。自訂的 `ShellStep` 是和 Engine `RuntimeControl` 平行的詞彙（原則 4）。接過來的是兩個事實：live frame 和 operation 衝突；pvp 在 pump 前後包夾 SdlInput 並計時 |
| C | 程式庫式組合：產品自己跑迴圈，framework 只給 RAII 零件 | 6 | 和原則 2（framework 是 controller）與已核准的 D-8、DI 5 衝突；草稿的核心 CMake 條件會讓守衛失敗；跨物件的銷毀順序 framework 測不到。接過來的是：三份相同的主控台開場程式（ui_editor、audio_editor、match）是觀測到的壓力，正好由 `RunHeadlessShell` 收掉 |

## 2. 推薦草圖（A 為骨架）

```cpp
// ===== core GYO::ToolFramework: framework/include/gyo/framework/HeadlessShell.hpp =====
// Links unchanged: PUBLIC GYO::Engine, PRIVATE GYO::Os.
namespace Gyo::Tools::Framework {
// The command line as Engine::Os::Console::CommandLineUtf8 gives it: UTF-8, program name first.
using HeadlessMain = std::function<int(std::span<const std::string> arguments)>;
// Called instead of the body when the command line has no UTF-8 form; reason = Base::Describe(OsError).
using CommandLineFailure = std::function<int(std::string_view reason)>;
// Holds Os::Console::Utf8ConsoleOutput for the whole call, converts the command line, calls the body once
// and returns its value unchanged. A window shell may run inside the body. Catches nothing, logs nothing.
[[nodiscard]] int RunHeadlessShell(int argc, char* argv[], const HeadlessMain& body, const CommandLineFailure& failure);
}

// ===== core GYO::ToolFrameworkWindow (only if TARGET GYO::PlatformSDL): framework/include/gyo/framework/WindowShell.hpp =====
namespace Gyo::Tools::Framework {
struct WindowShellOptions final { Engine::Platform::Sdl::SdlPlatformOptions platform; };  // no copied fields
class WindowShellClient {
public:
    virtual ~WindowShellClient() = default;  // runs while the platform still exists
    // Inside PumpEvents' observer: queries only (platform-sdl DI 7). Sees every event before the pump decides Stop.
    virtual void OnEvent(const Engine::Platform::Sdl::PlatformEvent&) {}
    [[nodiscard]] virtual Engine::Runtime::RuntimeControl Update(const Engine::Runtime::FrameContext&) = 0;
    // Only after an Update that returned Continue, with the same FrameContext.
    [[nodiscard]] virtual Engine::Runtime::RuntimeControl Render(const Engine::Runtime::FrameContext&) = 0;
    // Read once after the loop ends, for any reason, before the client is destroyed.
    [[nodiscard]] virtual int ExitCode() const noexcept { return 0; }
};
// Called once, after the platform exists. nullptr: the client could not start (it logs why); exit code 1.
using WindowShellClientFactory =
    std::function<std::unique_ptr<WindowShellClient>(Engine::Platform::Sdl::SdlPlatform&)>;
// Creates the platform, then the client, and runs one Engine::Runtime::RuntimeLoop
// (ProcessEvents = PumpEvents with OnEvent as observer). No render device, no live-frame handler,
// no stop request, no log handler or SDL log bridge. Main thread. Returns 1 for its own failures,
// otherwise the client's code. Destroys the client before the platform, also while an exception unwinds.
[[nodiscard]] int RunWindowShell(const WindowShellOptions& options, const WindowShellClientFactory& factory);
}

// ===== ImGui binding GYO::ToolFrameworkImGui: framework/bindings/imgui/include/gyo/framework/imgui/ImGuiShell.hpp =====
// Neutral types only in the public header: no imgui.h, no SDL_, no *Native.
namespace Gyo::Tools::Framework {
struct ImGuiShellContext final {
    Engine::Platform::Sdl::SdlPlatform& platform;           // path dialogs (T1)
    Engine::Render::Backend::Sdl::SdlRenderer& renderer;    // the binding's own device, lent to the product
};
class ImGuiShellClient {   // as ImGuiSdlClient today (decision D-shell-2)
public:
    virtual ~ImGuiShellClient() = default;
    [[nodiscard]] virtual bool Initialize(const ImGuiShellContext&) = 0;  // after ImGui and both backends; false -> 1
    virtual void BeginFrame() {}
    virtual void Draw() = 0;
    [[nodiscard]] virtual bool WantsExit() const = 0;
    virtual void EndFrame() {}   // exactly once per BeginFrame, rendered or not
};
using ImGuiShellClientFactory = std::function<std::unique_ptr<ImGuiShellClient>(const ImGuiShellContext&)>;
// RunWindowShell with the binding's own client: SdlRenderer, the product factory, the ImGui context
// (IniFilename = LogFilename = nullptr, NavEnableKeyboard, system CJK chrome font, dark style), both SDL backends,
// then Initialize. Exit: 0 on WantsExit or window close; 1 when the renderer, a backend or the product fails
// to start, or Clear/Present fails. Logs through Base::Log ("framework.imgui", the window title as prefix).
[[nodiscard]] int RunImGuiShell(const WindowShellOptions& options, const ImGuiShellClientFactory& factory);
}
```

- **CMake**：核心 `framework/CMakeLists.txt` 加 `src/HeadlessShell.cpp`；`gyo_tool_framework_window` 放在 `if(TARGET GYO::PlatformSDL)` 裡，只連 `gyo_tool_framework` 與 `GYO::PlatformSDL`（不連 `GYO::RenderBackendSDL`，守住「外殼不建渲染裝置」）。綁定目錄以列舉加入（D-shell-4），建置條件寫在 `framework/bindings/imgui/CMakeLists.txt` 自己的開頭（和 `Legacy.cmake` 的條件等價）。綁定 PUBLIC 連 window target 與 `GYO::RenderBackendSDL`，PRIVATE 連 `GYO::Os`、兩個 `*Native`、`imgui::core`、`imgui::sdl3_sdlrenderer3`，不具名 `SDL3::SDL3`。全部名字都在守衛現有的清單內，例外清單不加行。
- **結束碼**：headless 原樣回傳 body 或 failure 的值；window 自己的失敗回 1，其餘回 client 的碼；ImGui 和現行宿主一致。
- **例外**：外殼都不 catch（D-shell-6）。observer 在 `SDL_PollEvent` 返回後才被呼叫，沒有 live frame，例外不穿過 C 的 stack。
- **log**：只用 Base::Log（`framework.shell`、`framework.imgui`；標題放訊息裡，不放 category，DI 3）。外殼不裝 handler、不持有 SdlLogBridge。行為變更：ImGui 綁定的訊息不再經 SDL 的輸出，Windows 的 debugger output 看不到，寫進 dev_log。
- **不加**：live frame、停止要求、pvp 專用掛鉤、最小化節流、HiDPI 縮放（D-shell-8）。舊的 `ImGuiSdlHost` 與 `GYO::ToolFrameworkGui` 不動。

## 3. 和 PLAN 第 2.4 節不同的地方

1. `HeadlessMain` 的 arguments 規定為 `CommandLineUtf8` 的結果，程式名在第一個（D-shell-3）。
2. 明寫 window shell 可以在 headless shell 的 body 裡跑：ui_editor 的 GUI 與 validate 共用一個 main（`tools/ui_editor/src/Main.cpp:76-104`）。
3. 綁定的建置條件放在綁定自己的 CMake，核心以列舉加入（D-shell-4）。放在核心會產生守衛的 target finding。
4. framework 新增 DI 6（外殼生命週期）與一條 Replacement Criterion，platform-sdl Replacement Criterion 2 在本批改寫（D-shell-5、D-shell-7）。
5. 工具側補上 PLAN 漏掉的一點：audio_editor 執行檔要自己具名 `GYO::ToolFramework`（現在只經 core 的 PRIVATE 連到，`tools/audio_editor/CMakeLists.txt:24-27,37`），守衛不用改。
6. pvp 候選說明多三項：live frame 和 operation 的衝突、pump 的前後包夾與計時、迴圈後的收尾工作（第 6 節）。
7. 驗收與突變比 PLAN 多（第 5 節）。

## 4. 要你決定的事

| 代號 | 問題 | 建議 | 主要理由與證據 |
|---|---|---|---|
| D-shell-1 | 外殼的形狀：A 反轉式（PLAN）、B callback 式、C 程式庫式 | A | 原則 2 與已核准的 D-8、DI 5；直接用 Engine 的 `RuntimeControl`（原則 4）。`docs/framework/architecture.md:72-81` |
| D-shell-2 | ImGui 綁定裡產品的建立與銷毀順序：(a) 工廠在 ImGui 之後、拿掉 Initialize、完全反序銷毀；(b) 照今天：先工廠、ImGui 建好後 Initialize、ImGui 先關再銷毀產品 | **(b)** | 審查者建議 (a)（對稱、產品的解構子能用 ImGui）。對抗式檢查指出 (a) 沒有觀測到的壓力，只是為了對稱，而且會擴大 T3a。我採 (b)：PLAN 核准的是「as ImGuiSdlClient today」，行為不變；ui_editor 的解構只銷毀貼圖，依賴的是 renderer 還在（`AssetPreviewContext.cpp:116-127`），(b) 也滿足。日後真的出現需要時，改動只在綁定與 T3a |
| D-shell-3 | arguments 是否含程式名 | 含 | 不另抄規則；audio_editor 的索引不必改。`engine/os/include/engine/os/Console.hpp:17-22` |
| D-shell-4 | 綁定目錄怎麼加入：(a) 列舉 `bindings/*`；(b) 明寫 `add_subdirectory(bindings/imgui)` | (a) | DI 2「the core never names it」；下一批的 JSON 綁定不必改核心。來路不明的目錄會被守衛與 configure 檢查擋下；漏建由第 5 節的存在檢查擋下 |
| D-shell-5 | DI 6 與新 Replacement Criterion 在本批寫入嗎 | 本批寫入（第 7 節條文） | AGENTS §3：置換條件在功能疊上去之前寫；T3a、T3b 跨會話依賴這些約定 |
| D-shell-6 | 外殼要不要 catch 例外 | 不 catch | error-handling.md 規則 6。已知風險：Windows 上 abort 的結束碼 3 和工具「3 = 檔案 I/O」撞號，尚未發生事故，只記錄 |
| D-shell-7 | platform-sdl Replacement Criterion 2 照第 7 節改寫 | 照改 | HANDOFF 已延到本批 |
| D-shell-8 | 最小化節流、HiDPI 縮放要不要放進 ImGui 綁定 | 不放，記為候選 | ui_editor 沒有回報過；加了是預測壓力 |

## 5. 驗收與突變（核准後在事前宣告逐條定稿）

- L1 三列綠；例外清單零新增行；守衛、selftest、configure 連結檢查通過。
- **存在檢查**（對抗式檢查 major 2）：`tests/framework/CMakeLists.txt` 在 configure 時確認：SDL 平台開著卻沒有 `GYO::ToolFrameworkWindow`，或 ImGui 與 SDL renderer 都開著卻沒有 `GYO::ToolFrameworkImGui`，就 FATAL_ERROR。否則條件寫錯時新測試會整組消失、L1 仍然是綠的。
- **headless**：argv 以 `CommandLineUtf8` 的結果交給 body（Windows 上用假 argv，和 `GetCommandLineW` 不同，所以三列都殺得掉「直接傳 argv」）；body 的值原樣回傳；POSIX 上非 UTF-8 走 failure 並原樣回傳它的值；每個回呼最多一次。
- **window**（dummy 驅動）：建平台失敗與工廠回 nullptr 都回 1 且不呼叫工廠／不跑迴圈；client 的結束碼；Stop 那一幀不 Render；Update／Render 交替且 frameIndex 相同；注入的事件先到 OnEvent；本視窗的關閉要求結束迴圈；client 解構時 video 還在、返回後已關；例外傳出時順序不變；**不設 live frame handler**（`RegisteredEventWatchesForTesting()` 為 0）、**不換 log handler**（執行前後 `SetLogHandler` 的值相同）。後兩項是對抗式檢查 major 1：DI 6 的禁止事項要有會失敗的測試。停止要求與主控台 guard 在 POSIX 上觀測不到，從 DI 6 拿掉，只寫在標頭說明。
- **ImGui**（dummy＋software，全新工作目錄）：3 幀後回 0、BeginFrame／Draw／EndFrame 各 3 次；`IniFilename`、`LogFilename` 為 nullptr，結束後沒有 `imgui.ini`，`LogToFile` 後也沒有 `imgui_log.txt`；Initialize 時 ImGui 已是 current，回 false 時回 1；產品解構時 renderer 還在；帶本視窗 ID 的滑鼠事件到 ImGui；`SDL_RENDER_DRIVER` 指定不存在的驅動時回 1 且不呼叫工廠；options 原樣傳到平台（從產品查得視窗尺寸）；關窗回 0。
- **有沒有真的畫**：在第 2 幀以後的 BeginFrame（NewFrame 之前）檢查字型貼圖為 OK。要先在 ImGui v1.92.5 確認成立，才在事前宣告列入；不成立就不列這條突變，在宣告時寫明（不在看結果後刪）。Clear／Present 拿掉的突變殺不到，寫成已知限界。
- **公開標頭中立**：一個只 include `ImGuiShell.hpp` 與 `WindowShell.hpp`、沒有 SDL 與 imgui include 路徑的編譯測試（前例 `platform_sdl_header_tests`）。
- **突變**：審查提出 18 條（argv、結束碼、銷毀順序、工廠、observer、核心連渲染裝置／Native／SDL 符號、imgui.ini、ImGui log、EndFrame、事件、綁定的 SDL 符號等），加上本節新增的存在檢查、live frame、log handler。條數與每條的 file、find、預期在核准後的事前宣告定稿，報告第一行做件數對帳。
- **L2**（本機一次）：`GYO_TOOLS` 為空、只選 audio_editor 各 configure 一次，後者沒有任何 SDL3 target 與兩個新 target。L1 沒有這兩個組態，寫成範圍的限界。

## 6. 工具側與 pvp

- **T3a（ui_editor，和 T1 一起）**：main 改用 `RunHeadlessShell`（命令列錯誤印 help 回 2），GUI 在 body 裡呼叫 `RunImGuiShell`；EditorClient 改實作 `ImGuiShellClient`，對話窗用 `context.platform`，預覽用 `NativeRenderer(context.renderer)` 並自己連 `GYO::RenderBackendSDLNative`；`ShowPathDialog` 只在 BeginFrame／Draw 呼叫；CMake 與守衛把 `GYO::ToolFrameworkGui` 換成 `GYO::ToolFrameworkImGui`（§14.4，工具線出示後核准）。
- **T3b（audio_editor）**：main 改用 `RunHeadlessShell`，索引不變；執行檔自己具名 `GYO::ToolFramework`。
- **pvp 候選說明**（外殼草圖核准後通知 pvp v8 owner；不是 Replacement Criterion）。pvp Client 採用 `RunWindowShell` 前要先改：
  1. 建置條件：framework 只在選了工具時加入（`build/cmake/GyoBuild.cmake:56-58`，framework DI 1），要改成產品在登錄資料宣告，屬 Architecture Delta。
  2. live frame（`PvpApplication.cpp:1377-1395`）：在 PumpEvents 裡跑，operation 會 assert；ImGui 綁定不能開。需要通用的外殼選項並在 Update／Render 契約加「這是 live frame」，另行核准，否則 pvp 放棄即時重繪。
  3. pump 的前後包夾與計時（:1098-1121）：逐事件的 OnEvent 做不到。放棄這個診斷，或外殼加通用的前後掛鉤（要核准）。
  4. 迴圈後的收尾（`connection.Leave()` 等，:1398-1401）：移到 client 的解構子（那時平台還在）。初始化失敗由回傳 bool 改成工廠回 nullptr。
  5. 現在就支援：GPU 裝置在工廠裡建（:1047）；平台前的內容載入在呼叫外殼前做（:950-956）；`imeComposition` 經 options 原樣帶（:1039）；`Fail()` 對應 `ExitCode()`；進入點的 catch 與 SdlLogBridge 由 pvp 保留；最小化節流留在 pvp。Match 最多只用 `RunHeadlessShell` 取代開場程式，外殼不提供停止要求。

## 7. 條文草案（英文，照原文件語言）

**docs/framework/architecture.md，新增 Design Invariant 6**

> 6. **Shell lifecycle.** RunHeadlessShell hands its body the command line as Engine's CommandLineUtf8 gives it (UTF-8, program name first) and returns the body's or the failure function's value unchanged. RunWindowShell creates the platform before its client and destroys the client before the platform, also while an exception unwinds; it owns the one RuntimeLoop, passes every event of a pump to OnEvent before that pump's Update, returns 1 for its own failures and otherwise the client's code, and sets no live-frame handler and no log handler. The ImGui binding's renderer is its own, lent to the product, and lives until the product is destroyed. No shell catches an exception, and shells log only through Base::Log. Guards: the headless, window-shell and ImGui-shell tests, the configure-time check that the shells are built where their Engine targets are, and the fw-shell-* and fw-imgui-* mutants.

**docs/framework/architecture.md，Replacement Criteria 新增**

> - Serving one product needs a callback or flag in a shell that no other product uses, or a client has to pump events or run a RuntimeLoop itself.

（對抗式檢查 minor：不舉 pvp 的例子，免得 pvp 一採用就觸發；pvp 的項目只在候選說明。）

**docs/architecture/platform-sdl.md，Replacement Criterion 2 改寫**

> 2. Apart from the framework's ImGui binding (GYO::ToolFrameworkImGui), and ui_editor's asset preview until the SDL renderer has a texture API, an active product needs a *Native target for a new capability: the neutral API is cut wrongly.

DI 5 不改字；DI 4 的「the host」改「the ImGui binding」與 Using it 的舊宿主段落，照 PLAN 留到 v8-framework-invariants；本批在 Using it 加新的進入點。`docs/architecture.md:90` 列了 framework 的 target 與 ImGui host，本批一併更新。

## 8. 對抗式檢查的處理

| 嚴重度 | 發現 | 處理 |
|---|---|---|
| major | DI 6 的禁止事項（live frame、log handler、停止要求、主控台 guard）沒有會失敗的測試或突變 | live frame 與 log handler 加測試與突變；停止要求與主控台 guard 在 POSIX 觀測不到，從 DI 6 拿掉 |
| major | 新測試以 `if(TARGET …)` 開關，條件寫錯時整組消失、L1 仍綠 | configure 時的存在檢查加一條突變 |
| minor | 突變條數前後不一（16／18） | 事前宣告定稿條數，件數對帳 |
| minor | ImGui 的失敗結束碼沒有測試 | `SDL_RENDER_DRIVER` 不存在的驅動回 1；options 原樣傳遞；Clear／Present 寫成已知限界 |
| minor | 「有沒有真的畫」的斷言可能不穩 | 改在第 2 幀後的 BeginFrame 檢查字型貼圖；先確認成立才宣告 |
| minor | 新 Replacement Criterion 舉了 pvp 的例子 | 拿掉例子 |
| minor | pvp 說明漏了迴圈後的收尾 | 第 6 節第 4 項 |
| minor | 草稿的標頭註解寫了工具名，抄進 framework 會觸發 DI 3 守衛 | 標頭只用中性字眼 |
| minor | 綁定公開標頭的中立性沒有守衛 | 只 include 公開標頭的編譯測試 |
| minor | 拿掉 Initialize、反轉銷毀順序沒有觀測到的壓力 | D-shell-2 改建議 (b)，照今天 |
| minor | 公開標頭的參數名 `main`、`docs/architecture.md:90` 沒列 | 參數改名 `body`；`docs/architecture.md` 列入本批 |

對抗式檢查確認無誤的事：守衛與 configure 檢查的推理、銷毀順序的安全性、事件路由與 operation 的 assert、舊宿主沒有 live frame（換新外殼不會失去即時重繪）、audio_editor 的 CMake 與守衛事實、SDL main callbacks 的兩個限制、pvp 的行號。

## 9. 估時

實作約 14～17 h（不含第 0 關與核准等待），PLAN 原估 12～16 h；略超是因為第 5 節多出的測試與突變。停止條件：總時數超過 25 h，或單一問題累計超過 2 h（例如某一列建不起 software renderer）。第 0 關實際約 1 h（workflow 約 28 分鐘加整理）。工具線另計：T3a 約 3～4 h（含 T1），T3b 少於 1 h。
