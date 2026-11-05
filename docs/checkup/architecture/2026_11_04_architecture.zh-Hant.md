# 架構漂移健檢（2026-11-04，聚焦）

基準：master `4918b1fa`（PR #142 合併後）。上一份報告的基準是 `6e78090b`（2026-11-01），這段期間 first-parent 合併了 16 個 PR（#125、#127～#140、#142、#143），只隔 3 天。
觸發條件：不是 README 的常規門檻（PR 數 16，未滿 20）。framework 整改線（PR #127→#131→#133→#134）完成時請求健檢；PR #142 新增了判斷標準 3、4 與步驟 6，也要第一次套用。
範圍（聚焦，不是全量）：framework 核心與綁定的 Design Invariants 與守衛是否真的無例外、沉進 Engine 的選擇窗與使用者檔案、三份 Design Invariants 的措辭、framework 線點名的四件事、新判斷 3／4 的第一次套用、以及上一份報告與 entity-model 卡留下的追蹤項。判斷標準與步驟見 [README](README.md)。

做法：架構會話照 README 步驟 1～7 自行掃描（high，Fable 5.1）；步驟 8（執行期的證據）由 1 位唯讀掃描員（Explore，模型與檔位繼承）讀基準以來改動的 4 份 HANDOFF 的新增段落與 53 份 dev_log。沒有對抗式審查：本報告只有觀察與建議，沒有新的修正方向要推翻。
除了註明已立案的項目，其餘都尚未立案。

## 結論

- **Engine 仍然沒有被產品入侵。** `engine/`（不含 `engine/config`）沒有產品名稱、玩法用詞或產品協議常數（唯一命中是 `GyopDatagram.hpp:14` 提到 Go gateway 的 framing，上次已有）；Engine 的 CMake 只連結 `GYO::*`、第三方 target 與系統庫；共通層沒有產品名稱分支。
- **上一份報告的 framework 問題（高）已解決。** 核心不再嵌入 SDL／ImGui／nlohmann：ImGui 與 JSON 各成綁定 target，守衛改成「核心禁止清單＋每個綁定一份正向清單」，例外清單已清空並由守衛禁止它回來（`framework_dependency_guard.py:177`）。選擇窗沉進 Engine 平台層（`SdlPlatform.hpp` 的 `ShowPathDialog`／`TakePathDialogAnswers`，DI 10），使用者檔案的探測與原子寫入沉進 `engine/io`（`UserFiles.hpp`），catalog 規則改用 Engine 公開的 `IsRelativeContentPath`，framework 的 `IsValidAssetPath` 與 `AssetRegistration` 已刪除。
- **第一次套用判斷 4（framework 是否變成轉接層）：沒有門面。** 核心的每一段公開介面都組合了兩個以上的 Engine 呼叫或加了自己的規則；但「兩個產品」這一半只有兩段成立（`RunHeadlessShell`、`ToUiTextInput`），其餘都只有一個使用者。其中 `FileErrorCode` 是 framework 自己不用、只有 ui_editor 用的型別，是 DI 5「一個產品自己需要的，直接向 Engine 拿」的漂移（第二節）。
- **三份 Design Invariants 的措辭已經不一致。** ui_editor DI 1 與 audio_editor DI 1 仍寫著 11-01 的寬版規則「a game and a tool, or two tools, both need comes through the framework first, even where Engine has it」；framework DI 5 在 11-04 收窄為「組合或改接」（PR #142）。兩份工具的 DI 要跟著改（Architecture Delta，使用者同意；守衛不測措辭，不涉 §14.4）。
- **第一次套用判斷 3（格式放在對的層）：只有一件放錯，而且已立案。** pvp 的 `sfx.json` 第 2 版裡的 `animation_events` 段是「動畫事件 → 聲音」的組合格式，住在產品裡；架構卡 `v8-arch-event-sounds-contract` 會把它改成 framework 的組合格式。其他格式（`gyo.anim_events`、`gyo.ui`、catalog v1 在 Engine；工具的工程檔在工具）都在對的層。
- **新的 Engine 缺口：沒有。** 上次列的三個缺口，選擇窗與 catalog 規則已解決；SDL renderer 的貼圖上傳仍是缺口，但 ui_editor 的原生例外現在寫明了結束條件（platform-sdl Replacement Criterion 2、ui_editor DI 1：「until Engine's SDL renderer has a texture API」），符合 `docs/architecture.md` Layers 節的檢查 5。
- **Gateway 的歸屬問題與上次相同**：`services/gyo_gateway` 這段期間沒有任何 commit，四項都維持。

## 這段期間的結構變化

- **framework 整改落地**（#127、#131、#133、#134）：`GYO::ToolFramework`（核心，無 SDL）、`GYO::ToolFrameworkWindow`（視窗外殼，只經 Engine 中立平台）、綁定 `GYO::ToolFrameworkImGui`（`framework/bindings/imgui/`，唯一連 `*Native` 的 target）與 `GYO::ToolFrameworkJson`（`framework/bindings/json/`）。核心公開介面 8 個標頭：`AbsolutePath`、`CatalogPlan`、`FrameworkError`、`HeadlessShell`、`ReadOnlyAssetCatalog`、`UiTextInput`、`WindowShell`；刪除 `CatalogRegistration`、`FileService`、`ImGuiSdlHost`、`PathDialog`、`SdlPathDialogs`。DI 改成 6 條＋Replacement Criteria 8 條；DI 3、DI 5 在 #142 改寫。
- **新的依賴邊**：`object_fps_pvp` Client → framework（`project.json` 宣告 component `FRAMEWORK`，只用 `ToUiTextInput`；#137）。`GyoBuild.cmake:56` 的 framework 條件從「有選工具」改成「有選工具或遊戲宣告 FRAMEWORK」；`GYO_KNOWN_COMPONENTS` 加 `FRAMEWORK`。方向正確（產品 → framework → Engine）。
- **Engine 平台層**：系統選擇窗（`v8-engine-platform-path-dialog`；DI 10、Replacement Criteria 7～9，mutants pd-*）；**Engine 開始帶 SDL 的 patch**（`third_party/sdl3/patches/`，owner 是 SDL 後端；第一個 patch 是 macOS 日文輸入法的 Return，`v8-tool-ui-editor-ime-enter`）。patch 的規則寫明「只修 SDL 自己的缺陷，不放 GYO 的行為」，有移除條件；記錄，不算漂移。
- **Engine io**：「User files」節（`ProbeFileStamp`、`WriteFileAtomically` 的 `createParentDirectories`、`FS::IsNativePathWithin`；`v8-engine-io-user-files`）。消費端：ui_editor（三者）、audio_editor（`createParentDirectories`）。
- **Engine audio**：播放控制（#139；`SoundHandle`、`Stop`／`StopAll`、`PlayheadOf`、`PlayheadBoard`）；DI 3、4、8、10、11 與 Replacement Criteria 6～8 新增或加嚴。點名的 Feature 是 audio_editor 的預覽（`plans/audio/CONTROL.md:7`），該消費端尚未合併（第二節）。
- **新的 `Design Invariants` 節**：Engine 側沒有新模組；產品側 `object_fps_pvp` 的 `assets.zh-Hant.md`、`entity-model.zh-Hant.md`、`player-presentation.zh-Hant.md`；audio 計畫的 `CONTROL.md`（修改案）。仍然沒有的：`net`、`math`、`error-handling`、Gateway。
- **根架構文件**：`docs/architecture.md` 新節「Layers: admission and format ownership」（#142）；本資料夾 README 的判斷標準 3、4 與步驟 6。
- **產品**：pv9（hitbox、hit_entity_id）、entity-model（PvpMatch 拆成元件欄）、素材版面標準化、音效、動畫權威。沒有新的穩定版本。

## 一、應收進 Engine 的功能

| 優先 | 功能 | 證據 | 建議的最小做法 |
|---|---|---|---|
| 中 | **SDL renderer 的貼圖上傳**（延續） | ui_editor 的資產預覽仍是全 repo 唯一的 `*Native` 例外（`tests/ui_editor/ui_editor_dependency_guard.py:5-7,34`：只允許 `SdlRendererNative.hpp` 與貼圖／renderer 的 `SDL_` 名稱）。這次把結束條件寫進了契約：platform-sdl Replacement Criterion 2、ui_editor DI 1「until Engine's SDL renderer has a texture API」。 | 不變：在 `SdlRenderer` 加「從像素建立貼圖」與釋放，不做材質系統。模型檢視工具（`v8-tool-model-viewer`，未開始）若也要顯示貼圖，就是第二個消費端，到時收。**未立案**。 |
| 低 | **`gyo.ui` 的版面公式公開**（延續） | `tools/ui_editor/src/GizmoMath.cpp:46-48` 仍有一份 `Layout`，對應 `engine/ui/src/UiRuntime.cpp:304` 匿名命名空間的 `ResolveDesignRect`；ui_editor DI 4 允許副本（只用來反解）。 | 不變：下次版面規則變動時公開函式並刪副本。**未立案**。 |

### 判斷過、目前不建議收進 Engine 的

- **蒙皮與姿勢**：Engine 的 `engine/model` 已有 `SamplePose`、`BlendPoses`、`SkinMesh`（`Animation.hpp:29,57,101`）；pvp 的 `PlayerPose.cpp` 只放產品自己的組合（慢跑權重、步距、`ComposeBody`、動作與身體的疊加）。層是對的。模型檢視工具到來時會有壓力，但壓力的形狀是「工具讀什麼格式」（第二節「格式的層」），不是 Engine 缺少蒙皮。
- **音效的事件 → 聲音對應**：使用者 2026-11-04 決定由 framework 持有組合格式，Engine 的 `gyo.anim_events` 不動；已開架構卡。
- **上一份報告列的選擇窗、catalog 規則**：已收進（Engine 平台層、`IsRelativeContentPath`）。

## 二、Engine 這一側的問題

### framework 層：公開介面逐段對照（判斷 4、`docs/architecture.md` Layers 節檢查 2）

| 介面 | 組合或改接的 Engine 能力 | 使用的產品 | 判定 |
|---|---|---|---|
| `RunHeadlessShell` | `Os::Console::Utf8ConsoleOutput` ＋ `CommandLineUtf8` ＋ 失敗回呼 | audio_editor、ui_editor（`Main.cpp`） | 成立 |
| `ToUiTextInput` | `Input::TextEvent` → `Ui::UiTextInput` 的改接 | object_fps_pvp（`PvpApplication.cpp:48`）、ui_editor（`PreviewAdapter.cpp`） | 成立 |
| `AbsolutePath` | `PathToUtf8` ＋ `MakeAbsolute` ＋ `PathFromUtf8` ＋ `lexically_normal`（`AbsolutePath.cpp`） | framework 自己的 `ReadOnlyAssetCatalog` ＋ ui_editor `DocumentSession` | 一個產品；留在核心是 framework-rework D-2 的決定（`docs/architecture.md:125`）。不是門面。 |
| `ReadOnlyAssetCatalog` | `ContentManifest` 的掛載＋唯讀視圖 | ui_editor | 一個產品（上次已記）。audio_editor 用 JSON 綁定登錄，不掛載。 |
| `CheckCatalogEntries`、`PlanRegistration`（`CatalogPlan`） | catalog 層級規則＋登錄政策，用 Engine 的 `RawCatalogEntry` 與 `IsRelativeContentPath` | 只經 JSON 綁定的 `RegisterAsset` → audio_editor（`SoundExport.cpp`） | 一個產品；11-01 審查定為核心的「結構」。模型檢視工具若也登錄就成立；若不登錄，下次重看。 |
| `RunWindowShell`、`RunImGuiShell` | SDL 平台＋RuntimeLoop＋ImGui 綁定的生命週期 | ui_editor（`EditorWindow.cpp`） | 一個產品；使用者 11-01 接受（framework-rework D-8），DI 5 寫明。 |
| `ReadCatalog`（JSON 綁定） | catalog v1 文字 → `RawCatalogEntry` | 沒有產品；只有 `CatalogStructureTests.cpp:64` | 公開但沒有消費端（內部由 `RegisterAsset` 用）。記錄。 |
| `FileErrorCode`／`FileError`（`FrameworkError.hpp:10-25`） | 沒有：framework 的程式碼沒有任何地方用它（`FileService` 刪除後留下） | ui_editor（`DocumentSession.hpp:58`、`EditorError.hpp`） | **漂移（低）**：DI 5「What one product alone needs, it takes from Engine directly」。這正是 framework 線點名的第一件事。 |

建議：`FileErrorCode` 搬回 ui_editor（`EditorError.hpp` 已有對照表），framework 的 `FrameworkError.hpp` 只留 `CatalogError`。小改；工具線的批。**未立案**。

### framework 層：Design Invariants 與守衛

| 嚴重度 | 項目 | 說明 |
|---|---|---|
| 中 | **三份 DI 的「共通功能先放 framework」措辭不一致** | framework DI 5（11-04，#142）：兩個以上產品「compose or adapt in the same way」才進 framework，只是共同呼叫同一個 Engine 能力不算。ui_editor DI 1（`docs/ui_toolchain.md`）與 audio_editor DI 1（`docs/audio_editor/architecture.md`）仍是 11-01 的寬版：「what a game and a tool, or two tools, both need comes through the framework first, even where Engine has it」。兩份要改成引用 framework DI 5，或照抄收窄後的句子。守衛（兩份 dependency guard）只測 include 與 target，不測這句話，所以不涉 §14.4；但 DI 的修改仍是 Architecture Delta。owner：工具線。 |
| 記錄 | 守衛與 DI 2 一致 | `framework_dependency_guard.py`：核心禁止 `SDL3/`、`imgui`、`backends/`、`nlohmann/`、`*Native.hpp`、無斜線的函式庫標頭（`:50`）、`SDL_`／`ImGui`／`nlohmann` 符號；每個綁定一份正向清單（`:60-79`），沒有清單的綁定目錄失敗（`:156`）；例外清單回來就失敗（`:177`）；DI 3 的產品 id 檢查讀兩份 registry（`:43`）。configure 時的連結檢查在 `tests/framework/CMakeLists.txt:82-`。上次指出的兩個洞（無斜線 include、前置宣告）已補。 |
| 記錄 | framework 線點名第二件：GyoBuild 的順序 | `GyoBuild.cmake` 先加 framework（`:57`）、再加工具（`:69`）、最後加 `tests/framework`（`:92`）。ui_editor 自己的 `NOT TARGET` 檢查（`tools/ui_editor/CMakeLists.txt:61-62`）在工具 configure 時先觸發，framework 的「綁定存在」檢查（`tests/framework/CMakeLists.txt:14-24`）因此只在沒有選工具時才會是第一個失敗。無害：兩個檢查都是 FATAL_ERROR，差別只在訊息。不建議改順序。 |
| 記錄 | framework 線點名第四件：IU-1 | 已由 `v8-input-ui-adapter` 完成（#135），`ToUiTextInput` 進核心；上表已列。 |

### 格式的層（判斷 3，第一次套用）

| 格式 | 所在層 | 判定 |
|---|---|---|
| `gyo.anim_events` v1、`gyo.ui` v1、catalog v1、WAV 片段 | Engine | 對：各描述一個資產；anim_events 不含事件名稱的意義 |
| `assets/object_fps_pvp/audio/sfx.json` v2 的 `animation_events` 段（事件名 → cue ＋ sync） | 產品（pvp） | **放錯層**：這是「Engine 資產如何組合」的格式。已立案：架構卡 `v8-arch-event-sounds-contract`（framework 的組合格式）＋之後的產品遷移批（已交 v8 規劃）。 |
| ui_editor 的工程檔、audio_editor 的工程檔 | 工具 | 對：工具自己的定義，沒有第二個產品讀 |
| pvp 的 `AnimationSetDefinition`、`CharacterPresentationDefinition`、`WeaponPresentationDefinition`（`apps/object_fps_pvp/include/RetroFPS/App/`） | 產品 | 對，**但要看住**：模型檢視工具（`v8-tool-model-viewer`，未開始）若直接讀這些產品私有格式，就是「產品私有格式被第二個產品讀取」的漂移。工具該讀的是 Engine 的模型資產＋`gyo.anim_events`＋framework 的 event_sounds。建議在該批的事前宣告寫明讀哪些格式。 |

### Engine 的部件與它的消費端（AGENTS §2）

| 嚴重度 | 項目 | 說明 |
|---|---|---|
| 記錄 | **音效播放控制 API 還沒有消費端** | `Mixer` 的 `SoundHandle`、`Stop`、`StopAll`、`PlayheadOf`（#139）在 `apps/`、`tools/`、`framework/` 沒有任何呼叫者；pvp 仍只用 `Play`。點名的 Feature 是 audio_editor 的預覽（F1 停止；`plans/audio/CONTROL.md:7-8`），該批在工具線，尚未合併。符合 §2（有點名的 Feature），不算漂移；下次健檢若消費端仍未出現，改列「只有介面的抽象」。 |
| 記錄 | OS 層以外的 OS 程式碼仍只有 `Waiter.cpp` | `engine/time/src/Waiter.cpp:11,54,129`；`os.md` 寫明凍結。`engine/os` 這段期間沒有改動，沒有新部件要對照。 |
| 低 | 「絕對路徑」的判斷仍有多份（延續） | 與上次相同，`io.md` 仍未寫明「契約字串層」與「問 OS 層」的分工。**未立案**。 |

### Gateway 的歸屬（延續，無變化）

`services/gyo_gateway` 自 `6e78090b` 起沒有 commit。四項維持：Design Invariants 未寫、限速常數、TCP frame 兩端歸屬、註解引用舊決策。**未立案**。

### 只有介面或沒有消費端的抽象

| 抽象 | 現況 |
|---|---|
| `Vfs`、`MountTable`、`IFileWatcher` | 仍只有 `tests/common/core/io` 在用 |
| `IRuntimePort`、`InputActionMap` | 仍只有未啟用產品（`object_fps`、`object_fps_v2`）與測試 |
| `CatalogFormat.hpp`、`BinaryLoader` | 仍沒有任何 include／消費端。framework 的 catalog 批沒有順手處理 `CatalogFormat.hpp`。 |
| `ReadCatalog`（framework JSON 綁定） | 新：只有測試在用（上表） |

### 其他

| 嚴重度 | 項目 | 說明 |
|---|---|---|
| 低 | **input-and-present 的兩個突變錨點失效**（framework 線點名第三件） | 本次驗證：`m7` 的 `find` 在 `engine/input/backend/sdl/src/SdlInput.cpp` 命中 0 次，`m-main-thread` 的 `find` 在 `engine/platform/sdl/src/SdlPlatform.cpp` 命中 2 次（`plans/input-and-present/scripts/mutations.json`）。`472225fe`（選擇窗）改了同檔的 m6a-g1／g2，沒有修這兩條。m-main-thread 守的是 platform-sdl DI 7 的主執行緒規則。owner：Engine 平台層；下次動 `SdlPlatform.cpp` 或 `SdlInput.cpp` 的批順手修。**未立案**。 |
| 低 | pvp 的 PvE 遺留標頭（10-30 使用者轉交，本次查證） | `apps/object_fps_pvp/include/RetroFPS/World/GridMap.hpp` 與 `Data/GameData.hpp` 的 `EnemyDefinition`／`EnemyCatalog`／`LevelDefinition`／`LevelCatalog`／`GameDataLoader`：`sources.cmake` 沒有對應的 `.cpp`，`GameDataLoader::`／`GridMap::` 沒有任何定義，pvp 的 `src/`、`gateway/`、測試都不用 `EnemyKind`／`FindByKind`／`GridMap`。活著的只有同檔的 `WeaponDefinition`（`WeaponState.hpp:3`、`WeaponShotGeometry.hpp:3` 為它 include）。產品內部的遺留，不越界；建議 pvp 把 `WeaponDefinition` 搬到武器自己的標頭，刪除其餘與 `World/GridMap.hpp`。owner：pvp。**未立案**。 |
| 中 | 對獨立消費端的破壞性變更（延續） | 這段期間 Engine 公開標頭沒有刪除任何函式（`git diff` 無減號的宣告）。未啟用產品的遷移清單狀態不變。 |
| 中 | 產品登錄資料仍放在 `engine/config`（延續） | 位置沒變。 |
| 低 | `net`、`math`、`error-handling` 沒有 Design Invariants；Gateway 沒有契約文件（延續） | 不變。 |
| 低 | 產品的文件放在 Engine 的文件資料夾（延續） | `docs/architecture/3d-assets.md`、`math.md:5` 不變。 |
| 低 | 同一個建置事實維護在兩處（延續） | `/Zc:preprocessor`：`GyoBuild.cmake:49-50` 與 `pipeline/CMakeLists.txt:34`。 |
| 低 | 突變執行器兩份、Engine 那份在 `docs/` 底下（延續） | 不變。pd-* 的突變也放在 `docs/architecture/plans/framework-rework/scripts/`，守的卻是 Engine 平台層的 DI 10：owner 與位置又多一處錯位。 |
| 記錄 | CI 沒有 TSan job（延續） | audio-control 的三批都靠本機 TSan（`plans/audio/README.md:51-53`）。並行元件再增一個（`PlayheadBoard`）。**未立案**。 |

## 三、執行期的證據（步驟 8）

掃描員讀了 4 份 HANDOFF 的新增段落與 53 份 dev_log（清單見附錄 C）。下面只列指向 Engine 缺口、平台差異或跨 owner 重複的項目；本會話核對了標 ✔ 的 file:line，其餘以掃描員的紀錄為準。產品內部的未結事項（pvp 的 worker 偶發失敗、HUD 檢查、授權紀錄、K3 突變清單的 6 條壞項等）屬於各 owner 的 HANDOFF，不在本報告重列。

| 類別 | 項目 | 證據 | owner | 判定 |
|---|---|---|---|---|
| Engine 缺口 | **視窗外殼沒有 live frame**：ui_editor 在 macOS 拖動視窗邊緣時畫面停住；pvp 用 `SdlPlatform::SetLiveFrameHandler` 自己處理，若搬上外殼就做不到。framework DI 6 寫明外殼「sets no live-frame handler」。 | ✔ `tools/plans/v8/HANDOFF.md:170`、`object_fps_pvp/plans/v8/HANDOFF.md:206`（(2)） | framework | 觀測到的壓力，兩個產品都要（一個已自己做、一個停住）；符合 DI 5「組合」的條件。候選：外殼加通用選項（Architecture Delta，改 DI 6）。**未立案**，等 pvp 決定是否搬上外殼。 |
| Engine 缺口 | **Engine 沒有量記憶體的 API**：量測工具用 `getrusage` 被 `os.boundary_guard` 擋下，改由 Python 包裝程式量。 | ✔ `dev_logs/2026_10_31_pvp_v8_anim_bottlenecks.zh-Hant.md:39-40` | engine（os） | 條件 2 成立（必須繞過），只有一個消費端且是量測。先記錄；第二個量測需求出現時收進 `engine/os`（`ProcessCpuTime` 旁）。**未立案**。 |
| Engine 側 | **動畫取樣跨平台不逐位元一致**（Slerp 等浮點運算；載入一致）：linux-x64＝macos-arm64、windows-x64、macos-x64 三組不同，原因未查（「不在本線範圍」）。後果：pvp 的跨平台 golden 縮小到不含動畫命中的情境；蒙皮網格不進 `animation_digest`。 | ✔ 同檔 `:31-35`；`dev_logs/2026_11_01_pvp_v8_hitbox_data_digest.zh-Hant.md:84` | engine（model） | Engine 這一側的問題：`engine/model` 沒有決定性的承諾，也沒有 Design Invariants 寫明「同平台可重現、跨平台不保證」。建議至少把這個事實寫進 `animation-events.md` 或新的 `model.md`，讓消費端不再各自發現。**未立案**。 |
| Engine 側 | `FixedTickRuntime` 一次最多補 5 步（83 ms），更長的停頓丟掉時間，Match 游標落後。pvp 選 (a) 不改，交給 B6。 | ✔ `dev_logs/2026_10_30_pvp_v8_connection_poor.zh-Hant.md:26` | engine（runtime）／pvp | 已知限界；pvp 的量測項目。記錄。 |
| 平台 | Engine 開始帶 SDL 的 patch（macOS 組字中的 Return）；未向上游回報。 | `dev_logs/2026_11_01_tool_v8_ui_editor_ime_enter.zh-Hant.md:89-107,209` | engine（platform-sdl） | 結構變化已列。建議補上游回報，否則移除條件永遠不成立。 |
| 平台 | 真實 OS 選擇窗在 CI 都不執行；Linux portal 要 pump；Windows 上同一請求回呼兩次（已以政策 (a) 處理）。 | `dev_logs/2026_11_01_framework_rework_path_dialog.zh-Hant.md:27,29` | engine | 限界，契約已寫明（platform-sdl DI 10）。 |
| 平台 | audio 特性測試的雜湊只在 macOS x86_64 錄過；board 的記憶體順序與對齊版 new 本機 TSan／ASan 才抓得到。 | `dev_logs/2026_10_31_engine_audio_control_baseline.zh-Hant.md:35-39`、`..._playhead.zh-Hant.md:39` | engine（audio） | 與「CI 沒有 TSan」同一件事。 |
| 平台 | ASan 樹不能選 PvP（PvP 要 SDL_GPU，和 `GYO_RENDER_DEVICE=NONE` 衝突）。 | `dev_logs/2026_10_31_engine_audio_control_clips.zh-Hant.md:40` | engine／pvp | 限界，記錄。 |
| 守衛 | GyoBuild 的 FRAMEWORK 條件 L1 抓不到（preset 都選工具）；遊戲可以不宣告 `FRAMEWORK` 直接連 `GYO::ToolFramework`（宣告與連結的對應只在 `*_COMPONENTS` 路徑檢查，SDL 後端 target 同類）。 | ✔ `dev_logs/2026_11_02_pvp_v8_input_ui_adapter_iu3.zh-Hant.md:59-60` | 共通層（build） | 兩個既有的洞。建議 ProjectComposition 加「只選遊戲」案例；繞過宣告的問題等素材／登錄的批。**未立案**。 |
| 中立 | `tests/framework/CatalogStructureTests.cpp:116` 的註解點名 audio_editor；framework 守衛不掃 `tests/`。 | ✔ | framework | DI 3 的字面只管 framework 的程式碼；測試裡點名是文件性質。低；改註解即可。 |
| 重複 | 三份相同的開場程式已由 `RunHeadlessShell` 收掉；Match（`match_main.cpp`）還沒採用。 | `dev_logs/2026_11_01_framework_rework_shell.zh-Hant.md:12`、`object_fps_pvp/plans/v8/HANDOFF.md:206` | pvp | 「pvp Client 搬上外殼」候選的一部分；pvp 列了 5 項要先決定。未排程。 |
| 重複 | 動畫與 HitPart 的檢查在 C++（Match＋Client）與 Go（Gateway）各一份。 | `dev_logs/2026_10_31_pvp_v8_anim_authority_wire.zh-Hant.md:14,36` | pvp／gateway | 設計如此（各有測試、wire 兩端）；與 Gateway 四項裡「TCP frame 兩端歸屬」同性質，一起看。 |
| 文件 | 4 份文件仍寫「遊戲不掛載 assets/common」，要不要改寫交給文件 owner。 | `dev_logs/2026_10_31_engine_v8_assets_common_retire.zh-Hant.md:20` | 圖書管理員 | 轉文件健檢。 |

## 四、entity-model 卡的追蹤項

| 項目 | 現況 |
|---|---|
| E-12（tick 中不刪實體）尚無程式 | 仍無：pvp 沒有 `inTick_` 或對應的 `GYO_ASSERT`。按計畫在第一個非玩家批加。 |
| `PlayerState` 仍混身體／游標／診斷欄位 | 仍混：`Pvp/Movement.hpp:159-178` 同時放位置與姿態、生命狀態、`movementSlack*` 診斷、`connectionQualityFailures`、`crouched`／`sprinting`。按計畫在第一個 AI 批拆。 |
| 改 `PvpMatch.cpp` 要跑突變守衛 | 守衛在：`tests/object_fps_pvp/mutations.json` 有 8 條以 `PvpMatch.cpp` 為目標。 |
| 移動迴圈反向無守衛 | 本次未查（靜態搜尋看不出，要讀 Match 的 tick 順序）。 |

## 上一份報告項目的現況

| 項目（11-01） | 當時 | 現況 | 證據 |
|---|---|---|---|
| 系統檔案選擇窗 | 中 | **已解決** | Engine 平台層 `ShowPathDialog`／`TakePathDialogAnswers`，DI 10；ui_editor 用（`tools/ui_editor/src/SdlPathDialogs.cpp`） |
| SDL renderer 的貼圖上傳 | 中 | 仍在，結束條件已寫進契約 | 第一節 |
| `gyo.ui` 的版面公式公開 | 低 | 仍在 | 第一節 |
| catalog 的路徑規則公開 | 低 | **已解決** | `ContentManifest.hpp` 的 `IsRelativeContentPath`；framework 的副本已刪 |
| framework 共通層直接嵌入具體元件 | 高 | **已解決** | 綁定 target＋守衛（第二節） |
| framework 重做 Engine 已有的規則 | 中 | **已解決** | `CatalogEntry = RawCatalogEntry`；路徑規則用 Engine 的 |
| framework 守衛的允許清單與原則相反 | 低 | **已解決** | 禁止清單＋正向清單；允許清單檔回來就失敗 |
| `GyoBuild.cmake` 以「有選工具」決定 framework | 記錄 | **已改** | 加 `GYO_ENABLE_FRAMEWORK`（component `FRAMEWORK`） |
| OS 層以外只剩 `Waiter.cpp` | 記錄 | 不變 | |
| OS 層的部件都有消費端 | 記錄 | 不變（OS 層無改動） | |
| 「絕對路徑」判斷有多份 | 低 | 仍在 | |
| Windows 無 SDL 程序的等待精度（Linux 未量） | 低 | 仍在 | 第三節 |
| MinGW 的差異、shader 工具 argv 為 ANSI 與 `fstream` | 記錄 | 仍在 | `pipeline/src/main.cpp:51,67` |
| Gateway 四項 | 中／中／低／低 | 全部仍在 | 無 commit |
| `Vfs`、`MountTable`、`IFileWatcher`、`IRuntimePort`、`InputActionMap`、`CatalogFormat.hpp`、`BinaryLoader` | 記錄 | 仍在 | 第二節 |
| 對獨立消費端的破壞性變更 | 中 | 仍在，無新案 | |
| 產品登錄資料在 `engine/config` | 中 | 仍在 | |
| `net`、`math`、`error-handling` 沒有 DI | 低 | 仍在 | |
| 產品文件在 `docs/architecture/` | 低 | 仍在 | |
| 同一個建置事實兩處 | 低 | 仍在 | |
| 突變執行器兩份 | 低 | 仍在，另多 pd-* 的位置錯位 | 第二節 |
| CI 沒有 TSan job | 記錄 | 仍在 | |
| `platform_sdl_concurrency` 只在 linux-x64 逾時 | 記錄 | 本次未查 | |
| 附錄 A：根 README 目錄表缺 `agents/` 與 `framework/` | — | 仍在 | `README.md`、`README.zh-Hant.md`、`README.en.md` 三份都沒有 |
| 附錄 A：計畫工具放在 `docs/`、沒有 `.gitattributes` | — | 仍在 | |
| 附錄 A：文件健檢最新一份 10-21 | — | 仍在 | `docs/checkup/documentation/` 無新報告 |
| 10-30 轉交：pvp 的 `GridMap.hpp` 遺留 | 未查證 | **已查證，確為遺留** | 第二節「其他」 |
| 10-31：v7 README 的「framework 層」一詞與工具層同名；`docs/architecture.md` 寫「不引入 umbrella framework」卻沒提工具層 | 轉工具線 | **已解決** | `docs/framework/architecture.md` 開頭寫明兩部分（tool part／game part，game part 引用 v7 README）；`docs/architecture.md:129` 已寫 framework 不是 umbrella |

## 附錄 A：流程與專案內部

- **評估的過程**（11-04）：「動畫事件 → 聲音」持有者的評估走了四輪（先在工具線的四個候選裡選、再比成熟引擎、再問 Bevy＋Editor、最後使用者提出 framework）。教訓已記：架構會話先給自己的全局見解與成熟方案的對照，不只在候選裡選。
- **ultracode 的規模**：這次健檢沒有用 workflow；聚焦範圍下一位掃描員夠用。
- **證據**：本報告只寫結論與 file:line，沒有另外的證據檔；突變錨點的驗證是一次性的 Python 讀取，未保存。

## 附錄 B：已確認無問題

- `engine/`（不含 `engine/config`）沒有產品名稱、玩法用詞或產品協議常數；`framework/` 沒有產品或工具名稱（守衛讀 registry）。
- Engine 的 CMake 只連結 `GYO::*`、第三方 target 與系統庫（`shell32`、`Threads::Threads`）；沒有循環。`GYO::Engine` 這段期間只多了 `UserFiles.cpp` 一個原始檔。
- 共通層（`build/cmake`、`build/*.py`、`build/acceptance/common`、`build/ci/common`、`tests/common`、`.github/workflows`、根 CMake）沒有依產品名稱分支；`build/acceptance/object_fps_pvp/` 裡的產品名稱屬於該產品自己的驗收。
- `apps/`、`tools/`、`framework/` 沒有直接的 `SDL_` 呼叫、沒有 `#include <SDL3/`、沒有 OS 巨集與 OS 標頭（唯一例外是 ui_editor 預覽的貼圖，守衛逐檔限制）。
- `engine/config` 的兩份 registry 沒有改動；pvp 的 `project.json` 以 component 宣告 FRAMEWORK，沒有改共通層。
- Engine 公開標頭沒有刪除任何函式。
- 新的 Engine io 與平台 API 都有消費端（ui_editor、audio_editor）；audio 的控制 API 有點名的 Feature。
- `docs/architecture.md:31,48,125,129` 已寫 framework 的位置、方向與「不是 umbrella」。

## 附錄 C：涵蓋範圍與限制

- 聚焦健檢：沒有重跑 10-27／11-01 的全量搜尋，只重做本報告範圍內的項目與上一份報告的未結項目。
- 依賴圖只用 CMake 的連結宣告靜態讀取，沒有在三個平台 configure。
- 三份 DI 的對照只看「共通功能先放 framework」與「可用任何 Engine 模組／SDL 只經 Engine」兩句；其餘條文沒有逐句對照。
- 判斷 3 的格式清單用 `docs/` 與 `assets/` 的已知格式列出，沒有搜尋所有 JSON。
- 步驟 8 由一位掃描員讀 HANDOFF 的新增段落（只看新增行，刪除的舊段落沒看）與 53 份 dev_log 全文；本會話只核對第三節標 ✔ 的 file:line。
- 未啟用產品沒有建置；`platform_sdl_concurrency` 的逾時與移動迴圈反向沒有查。
