# 架構漂移健檢（2026-11-01）

基準：master `ebfe3225`（PR #124 合併後）。上一份報告的基準是 `37d533d`（2026-10-27），這段期間 first-parent 合併了 26 個 PR（#96～#124），只隔 6 天。
觸發條件：PR 數超過 20，並新增兩個 Engine 模組（`os`、`audio`）與一個最上層目錄（`framework/`）。使用者另外要求對 framework 層做審查。
判斷標準與步驤見 [README](README.md)。

做法：架構會話照 README 步驟 1～7 自行掃描，步驟 7（執行期的證據）由 1 位唯讀掃描員讀各計畫 HANDOFF 與 dev_log；framework 的修正方向另有 1 位對抗式審查者（Sonnet）嘗試推翻。
除了註明已立案的項目，其餘都尚未立案。

## 結論

- **Engine 仍然沒有被產品入侵。** `engine/`（不含 `engine/config`）與 `services/` 沒有產品名稱、玩法常數或產品協議常數；`engine/model` 新增的 `gyo.anim_events` 格式不含任何事件名稱，事件的意義由呼叫者給。Engine 的 target 到不了產品、工具或 framework。
- **上一份報告的 OS 問題大多已解決。** `engine/os` 合併（PR #104）：Windows 長路徑、UTF-8 路徑與命令列、程序資訊、停止要求、原子寫檔都收進 Engine；`engine/io` 的 `IFileSystem` 有了原生實作；pvp、ui_editor、audio_editor 都改用它們。pvp 的 C++ 仍然沒有直接的 SDL 呼叫。
- **這段期間最大的架構變化是 framework 層**（PR #123）。它的依賴方向正確（工具 → framework → Engine），但內容不符合使用者的原則：共通層直接嵌入了 ImGui、SDL 與 nlohmann，而且重做了 Engine 已有的 catalog 規則。修正方向見第三節，已交工具線。
- **新出現的 Engine 缺口有三個**：檔案選擇窗、SDL renderer 的貼圖上傳、`gyo.ui` 的版面公式。三個都是工具為了取得能力而繞過 Engine 的抽象。
- **Gateway 的歸屬問題與上次相同**：這段期間 `services/gyo_gateway` 沒有任何 commit，Design Invariants 仍然沒有寫。

## 這段期間的結構變化

- **新的 Engine 模組**：`os`（#104；Console、Files、Process、Thread、Shutdown，20 條 Design Invariants）、`audio`（混音器＋SDL 後端；SDL 後端連結 SDL3 PRIVATE，沒有 Native target）。
- **新的最上層目錄**：`framework/`（#123，先建在 `tools/framework/`，同一個 PR 內搬到頂層）。commit 已標為 Architecture Delta，`docs/architecture.md` 與根 README 的目錄表都已更新。
- **Engine 核心的依賴**：`GYO::Engine` 新增 PRIVATE 連結 `GYO::Os`；`engine/asset` 改經 `engine/io` 的原生檔案系統開檔，不再用 `std::ifstream`。
- **新的 `Design Invariants` 節**：Engine 的 `audio.md`、`animation-events.md`、`io.md`、`os.md`、`threads.md`、`time.md`；工具的 `framework/architecture.md`、`audio_editor/architecture.md`、`ui_toolchain.md`。上一份報告說 Time、Threads 的條文「未合併」，現在已在 master。還沒有的：`net`、`math`、`error-handling`、Gateway。
- **產品**：pv8（蹲姿、hitbox 契約欄位）、音效、第三人稱武器、大廳文字欄位。沒有新的穩定版本。
- **CI**：macos-x64（Rosetta）已從 L1 拿掉（`2026_10_31_ci_speed`）。

## 一、應收進 Engine 的功能

| 優先 | 功能 | 證據 | 建議的最小做法 |
|---|---|---|---|
| 中 | **系統檔案選擇窗** | 全 repo 唯一的實作在 framework，直接呼叫 `SDL_ShowOpenFileDialog`／`SaveFileDialog`／`OpenFolderDialog`，而且需要 `SDL_Window*`（`framework/src/SdlPathDialogs.cpp:55-61`）。<br>這正是 `platform-sdl.md` Replacement Criteria 第 2 條描述的情況：產品為了新能力去用 `*Native`，代表中立 API 切錯了。<br>使用者 2026-10-31 已定方向：「Engine 沒有就直接增強 Engine 的能力」。 | 中立介面放在 Engine 平台層的非原生 API，SDL 實作放在 SDL 後端；Engine 的介面比照剪貼簿，只有 OS 那一半（種類、過濾器、預設位置、tag、答案交回主執行緒）；請求的用途對照與 inbox 等流程留在工具（使用者 2026-11-01 定案）。SDL 的回呼可能在別的執行緒、也可能在呼叫返回之前觸發（同檔 `:51-52` 的註解），Engine 要把答案交回主執行緒。要修改 `platform-sdl.md` 的契約與 Design Invariants（含 Replacement Criteria 第 2 條），屬 Architecture Delta。**已交工具線**（第三節）。 |
| 中 | **SDL renderer 的貼圖上傳** | ui_editor 的資產預覽直接呼叫 `SDL_CreateTexture`、`SDL_UpdateTexture`、`SDL_SetTextureBlendMode`／`ScaleMode`、`SDL_DestroyTexture`（`tools/ui_editor/src/AssetPreviewContext.cpp`，10 處），公開標頭也帶 `SDL_Texture*`（`AssetPreviewContext.hpp:11,18,55`）。<br>`engine/render/backend/sdl` 的 `SdlRenderer.hpp` 沒有任何貼圖 API。 | 只有一個消費端，但 Engine 有缺口（條件 2）。建議與選擇窗一起判斷：若 framework 修正後 ui_editor 仍需要 `RenderBackendSDLNative`，就是這一項造成的。最小做法是在 `SdlRenderer` 加「從像素建立貼圖」與釋放，不做材質系統。**未立案**。 |
| 低 | **`gyo.ui` 的版面公式公開** | ui_editor 的 `GizmoMath` 自己抄了一份 `ResolveDesignRect`，因為 Engine 的版本在 `UiRuntime.cpp:304` 的匿名命名空間裡（`2026_10_30_tool_v8_ui_invariants.zh-Hant.md:22`）。ui_editor 的不變式 4 允許副本，條件是守衛保證一致。 | 條件 4（契約歸 Engine，但只實作了一部分）。副本有守衛，不急；下次 `gyo.ui` 的版面規則變動時，順手公開函式並刪副本。**未立案**。 |
| 低 | **catalog 的路徑規則公開** | framework 抄了 Engine 的 `IsRelativeContentPath`（`engine/asset/src/ContentManifest.cpp:35`，檔內 static）成 `IsValidAssetPath`（`framework/src/CatalogRegistration.cpp:136`），而且已經分歧：Engine 拒絕控制字元、檢查與 shader bundle 的重疊，framework 都沒有。 | 公開既有規則，不新增規則。這是 framework 修正的一部分（第三節）。 |

### 判斷過、目前不建議收進 Engine 的

- **ImGui 宿主與 JSON 讀寫**：使用者明示它們不是 Engine 的職責（「UI、3D 模型都不應該是 Engine 的職責」）。它們屬於 framework 的抽象與可插拔的實作模組，見第三節。
- **Match 的 Tick 晚醒最多 136 ms**（`2026_10_30_pvp_v8_product_sfx.zh-Hant.md:73`）：只觀察到一次，原因未寫；在 Windows 沒有 SDL 的程序上，Engine 等待的晚醒已記為已知限界（D231）。先當產品的量測項目，不先動 Engine。
- **模型載入後軸向差 90°**（`2026_10_31_pvp_v8_nailer_tps.zh-Hant.md:31,58`）：原因未查，分不出是 Engine 的載入器還是資產匯出設定。記錄，等查明。
- **上一份報告列的 CPU 時間、本地時間、停止要求**：已隨 `engine/os` 收進（`ProcessCpuTime`、`ToLocalTime`、`InstallShutdownRequests`，pvp 已使用）。

## 二、Engine 這一側的問題

### framework 層（新）

| 嚴重度 | 項目 | 說明 |
|---|---|---|
| 高 | **共通層直接嵌入具體元件** | `framework/src/ImGuiSdlHost.cpp:11-15` include SDL3 與 ImGui 的 SDL 後端，並直接呼叫 `SDL_Log`／`SDL_LogWarn`／`SDL_LogError`；`SdlPathDialogs.cpp` 直接呼叫 SDL；`CatalogRegistration.cpp:3,106` 用 nlohmann 自己解析 catalog。公開標頭 `ImGuiSdlHost.hpp:7-8,21-22` 與 `SdlPathDialogs.hpp:8,19` 暴露 `SDL_Window*`／`SDL_Renderer*`。<br>使用者的原則（2026-10-31）：framework 是通用建置，可以處理 UI，但要抽象出來，不直接嵌入 ImGui 或特定的 JSON 解析器；SDL 一律經 Engine。<br>進入共通層的判斷用了「兩個工具都用」，而實際上每一塊目前都只有一個使用者（`git grep`：宿主、選擇窗、`FileService`、`ReadOnlyAssetCatalog` 只有 ui_editor；`RegisterAsset` 只有 audio_editor）。 |
| 中 | **重做 Engine 已有的規則**（AGENTS §9） | 條目型別 `AssetRegistration`（`CatalogRegistration.hpp:11`）與 Engine 的 `RawCatalogEntry`（`CatalogParser.hpp:13`）是同一件事；路徑規則見第一節；catalog v1 的合法性有兩個解析器。framework 接受的登錄，可能讓 `ContentManifest::LoadCatalogs` 在載入時失敗。 |
| 低 | **守衛的允許清單與原則相反** | `tests/framework/framework_dependency_guard.py:29` 允許 `SDL3/`、`backends/imgui_impl_sdl`、`nlohmann/`。修正時改成禁止清單（核心），綁定模組各有自己的正向清單。 |
| 記錄 | 建置條件 | `GyoBuild.cmake:56` 以「有選工具」決定是否加入 framework。遊戲部分出現時這個條件要改；現在沒有問題。 |

修正方向與批次見第三節。

### OS 差異與檔案

| 嚴重度 | 項目 | 說明 |
|---|---|---|
| 記錄 | **OS 層以外剩下的 OS 程式碼只有 `Waiter.cpp`** | `engine/time/src/Waiter.cpp:11,54,129` 的 `__APPLE__` 分支與 `GetLastError`（`:144,153`）。`os.md:37` 寫明等待後端留在 `engine/time`（os-wait-backend 凍結，D78）。符合文件，不算漂移；下次要動等待後端時再收。 |
| 記錄 | **OS 層的部件都有消費端** | 逐一對照 `engine/os` 公開 API 與消費端：只有 `AnsiCodePage` 沒有 OS 層以外的呼叫者（它用在 OS 層自己的錯誤訊息）。沒有預先擴大。 |
| 低 | **「絕對路徑」的判斷仍有多份** | `engine/io/src/path/PathUtils.cpp:20`（`IsAbsoluteLike`）、同檔 `:275`（`IsAbsolutePathLike`）、`engine/os` 的 `Files::IsAbsolutePath`、`ShaderLibrary.cpp:51`（拒絕任何 `:`）。比上次多了一份，但 `engine/os` 這份是唯一真正問 OS 的；其餘三份是契約字串層面的檢查，用途不同。建議在 `io.md` 寫明兩層的分工，不合併。**未立案**。 |
| 低 | Windows 無 SDL 程序的等待精度 | 實機數據已有：cv 與 `sleep_until` 晚醒約 15.6 ms，開了視窗降到 1～2 ms（`2026_10_28_engine_os_windows_run.zh-Hant.md:184`，D228）；D231 記為已知限界。Linux 仍未量。**已記錄，未立案**。 |
| 記錄 | MinGW 的差異 | `lexically_normal` 吃掉 UNC 開頭、`remove_all` 在 `\\?\` 長路徑卡死、不支援 symlink（`os-subsystem/HANDOFF.md:977-983`）。等 build-baseline 的支援矩陣決定 MinGW 是否為支援目標。 |
| 記錄 | shader 工具與驗收探針的 argv 在 Windows 仍是 ANSI（`os-subsystem/HANDOFF.md:995`） | 範圍外，未結。shader 工具也仍用 `std::ifstream`／`ofstream`（`pipeline/src/main.cpp:51,67`），它不連結 `GYO::Os`。 |

### Gateway 的歸屬（延續，無變化）

這段期間 `services/gyo_gateway` 沒有任何 commit。上一份報告的四項都維持：Design Invariants 未寫、限速 120 包／秒仍是套件常數（`session.go:50`）、TCP frame 兩端歸屬不同、註解仍引用 v7.1／K1／D121（`session.go:48`）。pv8 的設計在未合併分支上，合併前後都應該回頭看這四項。**未立案**。

### 只有介面或沒有消費端的抽象

| 抽象 | 現況 |
|---|---|
| `IFileSystem` | **已解決**：`NativeFileSystem` 實作（`engine/io/src/fs/NativeFileSystem.cpp`），asset、trace、pvp、framework、audio_editor 都在用。 |
| `Vfs`、`MountTable` | 仍然只有測試在用；這段期間沒有擴充。維持「第一個消費端出現前不擴充」。 |
| `IFileWatcher` | 仍然沒有實作；`AssetWatcher` 改經 `NativeFileSystem::Stat` 輪詢（`AssetWatcher.cpp:159`），不再直接用 `std::filesystem`。兩套並存的問題仍在。 |
| `IRuntimePort` | 仍只剩未啟用產品；`docs/architecture.md:96` 仍把它寫成 runtime 的邊界。與未啟用產品的去留一起決定。 |
| `InputActionMap` | 同上，只有未啟用產品。 |
| `CatalogFormat.hpp` | 仍然沒有任何檔案 include 它。Engine 內部小修，建議併進 framework 的 catalog 批（那一批正好要公開 catalog 規則）。 |
| `SoundLoader` | **已解決**：pvp 使用（`PvpApplication.cpp`）。`BinaryLoader` 仍無消費端。 |
| Math 的部分公開介面 | 這段期間 Math 沒有改動。 |

### 其他

| 嚴重度 | 項目 | 說明 |
|---|---|---|
| 中 | **對獨立消費端的破壞性變更**（延續） | 這段期間沒有新的公開 API 破壞性變更。未啟用產品的遷移清單狀態不變。preview 工具仍停用。**待決定**：與 10-04 相同。 |
| 中 | 產品登錄資料仍放在 `engine/config`（延續） | 位置沒變。 |
| 低 | `net`、`math`、`error-handling` 沒有 Design Invariants；Gateway 沒有任何契約文件 | `net` 的建議同上次：至少寫一條「與 Go 的 framing 位元組一致，由 `tests/common/net` 的向量守護」。 |
| 低 | 產品的文件放在 Engine 的文件資料夾（延續） | `docs/architecture/3d-assets.md` 仍是 object_fps（v1）的資產文件，三份根 README 的第 73 行仍連到它；`math.md:5` 仍點名 `object_fps_pvp`。素材標準化任務卡（2026-10-31）已決定不搬它、只在新文件裡指向它。 |
| 低 | 同一個建置事實維護在兩處（延續） | `/Zc:preprocessor` 仍在 `GyoBuild.cmake:49-50` 與 `pipeline/CMakeLists.txt:32-34` 各設一次。 |
| 低 | 突變執行器有兩份 | `docs/architecture/plans/time-threads-trace/scripts/run_mutations.py`（Engine 計畫用，跨計畫重用）與 `tests/object_fps_pvp/run_mutations.py`（產品自有，說明文寫明是 product-owned）。owner 不同，不算重複；但 Engine 那份仍放在 `docs/` 底下，和上次一樣接近該移到 `build/` 或 `tests/common` 的時點。 |
| 記錄 | CI 沒有 TSan job | Audio 的 SPSC 環的記憶體順序突變只有本機 TSan 抓得到（`audio/HANDOFF.md:130`）。Engine 的並行元件變多（Threads、Trace、Audio），建議列為 CI 的候選項目。**未立案**。 |
| 記錄 | `platform_sdl_concurrency` 只在 linux-x64 CI 逾時 | 原因未查明，看門狗已合併（PR #99）。只記錄。 |

## 三、framework 修正方向（使用者要求的審查）

使用者 2026-10-31 的原則（原話）：
- 「Engine和SDL是可插拔的模組化，和IMGUI和JSON一樣，數據和結構分離是算法抽象應該依靠結構，而不是數據。」
- 「tool相當於UI，而framework相當於controller。tool整理好數據之後，調用framework，得到返回的結果。」
- 「framework在GYO中就是一個通用建置，他可以處理UI的東西，但不是直接嵌入，而是抽象出來。」
- 「SDL的問題你可以直接用Engine的能力」「Engine 沒有就直接增強 Engine 的能力，架構設計要保持一致性」。
- 「ImGui的問題和Json一樣，他不是Engine的職責」。

分層：

| 層 | 負責 | 例子 |
|---|---|---|
| Engine | 能力 | 視窗、渲染、事件、檔案、log、選擇窗（新增）、catalog 的格式與規則 |
| framework 核心 | 通用功能，只依靠結構與自己定義的介面 | 登錄的判斷、檔案存取、唯讀 catalog、宿主的介面 |
| 綁定模組（framework 底下，獨立 target） | 把一個具體元件接到核心的介面上，可以替換；由 components 選用 | ImGui 宿主、catalog 的 JSON 文字讀寫 |
| 工具（產品） | 自己的 UI 與資料 | 編輯器畫面（ImGui 呼叫）、工程檔 |

去處：

| 現在 | 去處 |
|---|---|
| `ImGuiSdlHost` | 介面放核心；ImGui 實作做成綁定模組；log 改走 `engine/base/Log.hpp`；SDL handle 只經 `*Native` |
| `SdlPathDialogs`（SDL 呼叫、回呼、交回主執行緒） | Engine 平台層，與剪貼簿同級（第一節第一項）；使用者 2026-11-01 定案 |
| `PathDialog` 型別、inbox、用途對照 | 工具（ui_editor）：流程那一半是工具的；framework 不再持有選擇窗的任何部分 |
| `CatalogRegistration` 的判斷 | 核心，只處理結構，用 Engine 的 `RawCatalogEntry` |
| `CatalogRegistration` 的 JSON 改寫 | JSON 綁定模組；寫回前用 Engine 的解析器與規則再驗一次 |
| `IsValidAssetPath`、`ParseCatalog` | 刪除，改用 Engine 公開的規則 |
| `FileService`、`ReadOnlyAssetCatalog` | 不變 |
| 工具的畫面程式、工程檔 | 不變；這次不做 UI 元件庫（單一用例，AGENTS §1） |

建議批次（代號與檔位由工具線規劃後交使用者；順序經對抗式審查修正，守衛收緊排最後，否則收緊後的守衛會立刻失敗）：

```text
framework-fix
├── v8-engine-platform-path-dialog       high｜Engine 平台層的選擇窗＋ui_editor 切換＋刪除 framework 的 SdlPathDialogs
├── v8-tool-framework-imgui-binding      high｜宿主介面草圖（先核准）→ ImGui 綁定模組，log 走 Engine
├── v8-tool-framework-catalog-structure  high｜登錄改成「結構＋JSON 綁定」，規則的單一來源依使用者選項
└── v8-tool-framework-invariants         high｜Design Invariants 改寫＋守衛補洞與收緊＋突變搬家
```

守衛（AGENTS §14.4）：所有 Design Invariants 守衛的變更，無論收緊或放寬，都要出示 1～5 項並經使用者核准。現有守衛有兩個洞要一起補：沒有斜線的 include（例如 `<imgui.h>`）被跳過（`tests/framework/framework_dependency_guard.py:70`）；只掃 `SDL_*GPU*` 符號，標頭前置宣告 `struct SDL_Window;` 會通過。ui_editor 改連回 `GYO::PlatformSDL` 是放寬（PR #123 剛拿掉）。

catalog 規則的單一來源有兩個選項，要使用者選：(a) 規則留在 framework 核心，用既有的共用 fixture（`build/content_contract.py`、`tests/common/fixtures/asset_contract/cases.json`，`docs/architecture.md:246`）對齊，補上 shader bundle 重疊與重複 id 的案例，不動 Engine；(b) Engine 公開涵蓋全部規則的記憶體內驗證函式（Architecture Delta）。架構會話建議 (a)，它沿用既有機制，也符合「Engine 保持純粹」。

要使用者核准的：上面兩項，加上平台層選擇窗的契約（要對照 `platform-sdl.md` 的 DI 6、7、8、9；放在 `engine/platform/sdl`，不新開中立目錄）、宿主介面的草圖、framework 的新 Design Invariants、Engine 批次是否用 ultracode。

### 對抗式審查

1 位審查者（Sonnet）嘗試推翻第 1 版方向，7 項主要發現全部成立並已併入：批次順序做不完；§14.4 的判斷有誤；守衛有洞；宿主介面沒定義且與 ui_editor 的預覽相衝；單一來源漏掉既有的 fixture 機制、只公開路徑函式修不掉 bundle 重疊與重複 id；選擇窗契約漏了 DI 6、8 與回呼的 `noexcept`；JSON 綁定沒有可用的 component。
推翻不了的：所有 file:line（1 處 nit）、Engine 與 framework 的規則已分歧、每一塊只有一個使用者、選擇窗放 SDL 後端不違反 DI 1、2、9。
完整草稿（第 2 版）在架構會話的 scratchpad，交工具線時隨訊息送出；正式內容以本節與工具線的計畫文件為準。

## 上一份報告項目的現況

| 項目（10-27） | 當時 | 現況 | 證據 |
|---|---|---|---|
| 不依賴 SDL 的程序服務 | 中 | **已解決** | `engine/os` 的 `ExecutablePath`、`CommandLineUtf8`、`Utf8ConsoleOutput`、`InstallShutdownRequests`；pvp Match、ui_editor、audio_editor 都在用 |
| 原子寫檔與根目錄判斷 | 中 | **已解決** | `engine/io` 的 `WriteFileAtomically`、`os` 的 `RenameReplacing`、`SamePathComponent`；framework 的 `FileService` 與 audio_editor 使用 |
| CPU 時間、本地時間、停止要求 | 低 | **已解決** | `ProcessCpuTime`、`ToLocalTime`、`RequestedShutdown`，pvp 使用 |
| Windows 長路徑 | 高 | **已解決** | asset 經 `engine/io` 開檔；實機驗收通過（`2026_10_27_pvp_v71_d201_windows.zh-Hant.md:101`）；深路徑套件檢查在 `build/acceptance/object_fps_pvp/content_deep_path.py`，由產品驗收執行 |
| Engine 沒有負責 OS 差異的模組 | 中 | **已解決** | `engine/os` 與 `os.md` 的 20 條 Design Invariants；OS 層以外只剩 `Waiter.cpp`，文件寫明 |
| 檔案抽象只有介面 | 中 | **已解決** | `NativeFileSystem` |
| 路徑字串的編碼不一致 | 中 | **已解決** | 全部經 `PathToUtf8`／`PathFromUtf8`；shader 工具除外（見上） |
| Windows 無 SDL 與 Linux 的等待精度沒量 | 中 | 部分：Windows 已量並記為限界；Linux 仍未量 | D228、D231 |
| Windows 絕對路徑判斷有三份 | 低 | 仍在，變四份 | 見第二節 |
| 時間基準跨睡眠的行為 | 低 | **已解決** | `time.md:17` 寫明各平台時鐘對睡眠的行為 |
| Trace 的 CRLF | 記錄 | **已解決** | `trace.md` 已寫 |
| Gateway 四項 | 中／中／低／低 | 全部仍在，無變化 | 見第二節 |
| `Vfs`、`MountTable`、`IFileWatcher`、`IRuntimePort`、`InputActionMap`、`CatalogFormat.hpp`、`BinaryLoader` | 記錄 | 仍在 | 見第二節 |
| `SoundLoader` | 記錄 | **已解決** | pvp 使用 |
| 對獨立消費端的破壞性變更 | 中 | 仍在，無新案 | |
| 產品登錄資料在 `engine/config` | 中 | 仍在 | |
| Time、Threads、Net 沒有 Design Invariants | 低 | Time、Threads **已解決**；Net 仍在 | |
| 產品文件在 `docs/architecture/` | 低 | 仍在 | 素材任務卡決定不搬 |
| 同一個建置事實兩處 | 低 | 仍在 | |
| Metal 取得 drawable 停頓 | 低 | 仍在，無新資料 | |
| pvp 的 `Explain`／`ExplainUi` | 更正項 | **已解決** | pvp 已無 `Explain`，`Base::Describe` 23 處 |
| 附錄 A：計畫工具放在 `docs/` | — | 仍在 | |
| 附錄 A：`agents/` 不在根 README 目錄表 | — | 仍在 | README.zh-Hant.md 第 8～21 行沒有 `agents/`，也沒有 `framework/` |
| 附錄 A：沒有 `.gitattributes` | — | 仍在 | |
| 附錄 A：ui_editor 寫死字型目錄 | — | **已解決** | `Os::Files::FontDirectories` |

## 附錄 A：流程與專案內部

- **framework 的審查過程**：架構會話在 10-31 的一天內對工具線發了五則互相矛盾的訊息，原因是把使用者的補充原則誤讀成推翻結論，在使用者確認前就改寫。已記為教訓；本報告第三節是唯一有效的版本。
- **根 README 的目錄表**沒有 `agents/` 與 `framework/`。`docs/architecture.md` 已有 framework，三份 README 沒有。
- **證據的保存規則**（PR #122）：正式文件不以會消失的位置當證據所在，證據檔不進 repo，存在主 checkout 的 `build/evidence/`。本報告沿用：只寫結論與 file:line，沒有另外的證據檔。
- **文件健檢**：`docs/checkup/documentation/` 最新一份是 10-21，期間沒有新報告。
- **Rosetta**：macos-x64 已從 L1 拿掉，上次附錄 B 的「只在 Rosetta 出現的斷言」不再有觀測管道。

## 附錄 B：已確認無問題

- `engine/`（不含 `engine/config`）與 `services/` 沒有產品名稱、玩法常數或協議常數；`framework/` 沒有產品或工具名稱（Design Invariant 3 成立）。
- Engine 的 CMake 只連結 `GYO::*`、第三方 target 與系統庫；沒有循環；`GYO::Engine → GYO::Os` 是新邊，方向正確。
- 共通層（`build/cmake`、`build/*.py`、`build/acceptance/common`、`build/ci/common`、`tests/common`、`.github/workflows`、根 CMake）沒有依產品名稱分支。`GyoBuild.cmake:56` 的 framework 條件是依「層」分支，不是依產品。
- pvp 的 C++ 沒有直接的 SDL 呼叫；pvp 的檔案存取全部經 `engine/io`（`NativeFiles.hpp` 的註解寫明，`git grep` 證實沒有 `fstream`）。
- `engine/audio` 的 SDL 後端連結 SDL3 PRIVATE、沒有 Native target；pvp 經 `GYO::Audio` 播放，沒有碰 SDL 音效。
- `gyo.anim_events` 的格式在 `engine/model`，不含任何事件名稱。
- Go 這一側方向正確：產品以 replace 依賴 `services/`，`services/` 沒有反向依賴。
- `engine/os` 的每個公開部件都有 OS 層以外的消費端（`AnsiCodePage` 除外，它用於自己的錯誤訊息）。

## 附錄 C：涵蓋範圍與限制

- 依賴圖只用 CMake 的連結宣告靜態讀取，沒有在三個平台 configure；Windows 或 Linux 才有的條件 target 沒有涵蓋。
- 玩法用詞用固定詞表搜尋。
- framework 的審查以 master 的程式為準；工具線尚未合併的 `-edit` 分支沒有看。
- 步驟 7 由一位掃描員讀 HANDOFF 的新增段落，沒有讀全文。
- 未啟用產品沒有建置。
