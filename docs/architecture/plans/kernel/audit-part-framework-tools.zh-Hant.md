# 部分報告：`v8-arch-kernel-audit`，範圍 ② framework-tools

更新：2026-11-11。Owner：架構（架構師會話的審查子代理，Opus／high）。**只讀；不改程式、不改契約。** 主會話合併三份部分報告並逐列核對後，才成為 `audit.zh-Hant.md` 的一部分。

## 範圍與件数の突き合わせ

- 範圍：`framework/*`（掃描依目錄分成 `bindings`、`include`、`src` 三個模組）、`tools/*`（`voxel_map_editor`、`audio_editor`、`model_viewer`、`ui_editor`、`object_fps_preview`）。
- 掃描：`python3 …/plans/kernel/scripts/audit_scan.py --repo <本 worktree> --out /Users/karasu/Code/Source/GYO-Engine/build/evidence/kernel-audit/framework-tools --roots framework tools`（2026-11-11，master `1cff5413`，本機 macOS）。
- CSV（主 checkout，git 不追蹤）：`build/evidence/kernel-audit/framework-tools/audit_hits.csv`（180 筆命中）、`audit_summary.csv`（8 個模組）。下文 `hits:N` ＝ `audit_hits.csv` 的第 N 行（第 1 行是表頭）。
- **件数の突き合わせ：一致。掃描模組數 8 ＝ 主表列數 7 ＋ 未審列數 1**（`tools/object_fps_preview`）。主表之後的「細分表」是同一批模組依元件再拆，不計入件數。
- 讀了哪些檔：事前宣告 `audit-declaration.zh-Hant.md`；規格 `spec.zh-Hant.md` §G（含 G.4）、§H（H.4.3、H.6、H.7、H.8）、§I（I.1～I.6）、§K.3、§L（L.1～L.5）；`mechanism.zh-Hant.md` §1；`target/README.md`、`target/limits.zh-Hant.md`、`target/runtime-composition.zh-Hant.md`；`determinism.md` §2、§4。程式：framework 全部 26 個原始檔逐檔讀；`docs/framework/architecture.md` 的 Design Invariants 與 Replacement Criteria；四個工具的視窗 client（`EditorWindow.cpp`、`AudioEditorWindow.cpp`、`ViewerWindow.cpp`、ui_editor 的 `EditorWindow.cpp`／`EditorApp.hpp`）、`Main.cpp`、`FileAccess.cpp`、`PreviewMeasure.cpp`，以及掃描命中所在的檔案（`PreviewPlayer`、`PreviewScene`、`Editing.hpp`、`MapBuild.cpp`、`Synthetic.cpp`、`Playback.cpp`、`EventSoundFiles.cpp`、`PreviewAdapter.cpp`、`AssetPreviewContext.cpp`、`SdlPathDialogs.cpp`）。工具的其餘葉子檔只看公開 API 形狀與掃描結果，未逐行讀（標在各列）。

## 判法的適用（先說明，主會話合併時要對齊）

事前宣告寫「Q1 為是 → 組織層（重寫或 Host／服務）」。規格 I.3 本身只把 Q1 當作「留」的排除條件，「包／重寫」由 (i) 跨 tick 參考、(ii) 全域可變狀態、(iii) 自起執行緒三問決定；L.1 也把 `input`、`asset`、`ui`、`render` 等 Q1 為是的模組判成「包」。照宣告字面，凡有 `BeginFrame`／`EndFrame` 的元件（例如 `OffscreenPreview`）都會變成組織層，與 L.1 的判法不一致。

本報告採用的讀法：**Q1 為是，而且該元件決定其他元件的呼叫順序（擁有迴圈、依序呼叫別人的 Update／BeginFrame）→ 組織層（Host 或重寫）；Q1 為是，但只是每幀被呼叫一次、本身不排序別人 → 交給 Q2～Q4 決定包或重寫。** 依 §14 換兩種讀法都判過的元件，表中寫明兩種結果。此差異列為觀察 1，不改宣告與規格。

## 主表（每個掃描模組一列）

| 模組 | 行數（summary） | Q1 生命週期／更新迴圈 | Q2 跨 tick 指標或參考 | Q3 全域可變狀態（含時鐘、亂數） | Q4 自起執行緒碰 World 資料 | zone 初判 | 判定 | 理由 | 工時粗估 | 受 LIM 影響 |
|---|---|---|---|---|---|---|---|---|---|---|
| `framework/bindings` | 987 | **是**。`ImGuiWindowClient::Update`／`Render`（`ImGuiShell.cpp:408-447`；hits:13～20）以寫死的順序呼叫 input 的 `EndFrame`→`Take`→`BeginFrame`、client 的 `BeginFrame`→ImGui `NewFrame`→`Draw`→`Render`→文字輸入 `Apply`→`Clear`→`RenderDrawData`→`Present`→client `EndFrame`；`ImGuiScope::Start`（`:265`，hits:6）與解構（`:253-263`）管 ImGui context 與兩個 SDL 後端的生命週期 | 有，指向 Host 擁有的物件，不是 World 資料：`ImGuiWindowClient::platform_`（`:454`，hits:21）、`ShellInput::input_`（`:367`，hits:7）；`ImeComposition` 經 ImGui 的 `Platform_ImeUserData` 存自己的指標（`:161`，掃描無命中） | **是，掃描無命中＋人工確認有**：ImGui 的全域 context（`ImGui::CreateContext()` `:269`；`:267` 斷言一個行程一個）；所有 `ImGui::` 呼叫經它。json binding（`CatalogText.cpp`、`EventSoundsText.cpp`）掃描無命中＋人工確認無 | 掃描無命中（Q4 欄 0）＋人工確認無（framework 全目錄無 `std::thread`／`atomic`／`mutex`） | 不適用（Host）；json 為 B | **Host**（ImGui 骨架重寫）；json binding **留** | ImGui binding 是 ImGui 工具的幀排序者，正是 H-7 要消掉的「host 依序呼叫模組」形狀；ImGui context 是第三方全域狀態，照 H.4.3 以 effect `Gpu:imgui` 序列化承載。json binding 是純解析與序列化 | 12～24 h（json 0） | LIM-3（全域狀態 L5 不在頁保護範圍） |
| `framework/include` | 530 | **是**（同 `framework/src` 的元件標頭）：`WindowShellClient::Update`／`Render`（`WindowShell.hpp:26-28`，hits:30）；`OffscreenPreview::Create`／`BeginFrame`／`EndFrame`（`Preview.hpp:37`、`:52`、`:62`，hits:26～28） | 掃描命中的都是型別別名或參數，不是成員：`HeadlessMain`／`CommandLineFailure`（`HeadlessShell.hpp:12`、`:15`，hits:24、25）、`SoundSeconds`（`EventSounds.hpp:45`，hits:23）、`WindowShellClientFactory`（`WindowShell.hpp:45`，hits:31）、`clipboard` 參數（`UiTextInput.hpp:29`，hits:29）。人工確認：無跨幀成員指標 | 掃描無命中＋人工確認無（只有 `inline constexpr`，`EventSounds.hpp:21-22`） | 掃描無命中＋人工確認無 | 依元件，見細分表 | **Host**（主成分 `WindowShell`）；其餘見細分表 | 本列是 `framework/src` 同一組元件的標頭；判定與工時併入 `framework/src` 列 | 併入 `framework/src` | 同 `framework/src` |
| `framework/src` | 613 | **是**。`LoopAdapter::ProcessEvents`（`WindowShell.cpp:28-33`，hits:37）在 `PumpEvents` 裡把事件交 client，並處理 close request；`Update`／`Render` 轉給 client（`:34-35`，hits:38）；`RunWindowShell` 擁有 `RuntimeLoop`（`:61-63`，hits:42）。`OffscreenPreview::Create`／`BeginFrame`／`EndFrame`（`Preview.cpp:43`、`:80`、`:97`，hits:32～35）擁有 GPU 裝置 | 有，指向 Host 物件：`LoopAdapter::platform_`／`client_`（`WindowShell.cpp:38-39`，hits:39、40）。`OffscreenPreview::Impl` 擁有裝置與 queue（值與 `unique_ptr`，`Preview.cpp:27-32`），不參考外部資料；`ReadOnlyAssetCatalog` 全為值成員（`ReadOnlyAssetCatalog.hpp:47-50`） | 掃描無命中＋人工確認：無可變全域。`AbsolutePath` 經 `Os::Files::MakeAbsolute` 讀行程的目前目錄（`AbsolutePath.cpp:18`，OS 狀態，只讀）；`HeadlessShell` 以 RAII 切換主控台 UTF-8（`HeadlessShell.cpp:11`） | 掃描無命中＋人工確認無 | 不適用（Host）；`OffscreenPreview` 為 B（渲染 World）；其餘 B | **Host**（`WindowShell` 重寫成視窗 host 骨架）；`OffscreenPreview`、`ReadOnlyAssetCatalog` **包**；其餘**留** | `WindowShell` 擁有迴圈與 pump，是組織層；它 include `IRuntimeClient.hpp` 與 `RuntimeLoop.hpp`（`WindowShell.cpp:9-10`），在 H-7 的掃描清單內。重寫會改 framework 的 DI 6（觀察 2） | 21～42 h（`WindowShell` 16～32、`OffscreenPreview` 4～8、`ReadOnlyAssetCatalog` 1～2） | LIM-3（包的元件） |
| `tools/audio_editor` | 1852 | **是**。client 的 `Initialize`（`AudioEditorWindow.cpp:83`，hits:46）建 Mixer 與 `SdlAudioOutput`（`:85`、`:96`，hits:47、48）；`BeginFrame`（`:115-118`，hits:49、50）依序跑選擇窗回覆與 `PreviewPlayer::Update`。`PreviewPlayer::Update`／`Stop`（`PreviewPlayer.hpp:41`、`:43`，hits:43、44）是每幀被呼叫的補送 | 有，指向外部服務與 Host 物件：`PreviewPlayer::mixer_`（`PreviewPlayer.hpp:63`，hits:45）指向 `SdlAudioOutput` 擁有的 Mixer（I.1 的外部系統）；`platform_`（`AudioEditorWindow.cpp:533`，hits:52）。編輯核心（`Edit`、`Conversion`、`WavFile`、`ProjectFile`、`SoundExport`）掃描無命中 | **是**：`Engine::Time::Now()`（`AudioEditorWindow.cpp:117`、`:331`、`:335`，人工確認；掃描的 clock 樣式不含 `Time::Now`）；ImGui 全域 context（86 處 `ImGui::`，掃描無命中＋人工確認）。`std::mt19937_64`（`Conversion.cpp:64`，hits:55）是函式內區域、以參數 seed，不是全域狀態；檔案寫入經 `WriteFileAtomically`（`FileAccess.cpp:38`；spec L.1：時鐘與亂數） | 掃描無命中＋人工確認無；音訊回呼執行緒由 Engine 的 `SdlAudioOutput` 建立（`engine/audio/backend/sdl/src/SdlAudioOutput.cpp:40-47`），只碰 Mixer，不是本工具起的 | B | **Host**（client 重寫成 Editor host）；`PreviewPlayer` **包**；編輯核心**留** | client 擁有幀內順序（組織層）。`PreviewPlayer` 的跨幀指標指向外部服務 Mixer，不指向 World 資料；`Submitter` 綁第一個呼叫它的執行緒，成為綁 Main 的 Resource | 8～16 h | LIM-3 |
| `tools/model_viewer` | 3453 | **是**。client 的 `Initialize`（`ViewerWindow.cpp:79`，hits:98）建預覽與音訊（`:90`、`:98`，hits:99、100）；`BeginFrame`（`:111-138`，hits:102）依序跑選擇窗回覆→牆鐘差→`PlaybackClock::Advance`（`:125`，hits:104）→事件閃爍→`UpdateSounds`。量測模式 `PreviewMeasure.cpp:96-114` 有自己的 `PumpEvents` 迴圈（hits:83、86） | **是**：`pendingAction_`（`ViewerWindow.cpp:1006`，hits:113）跨幀保存捕獲 `this` 的 closure（由 `Guarded` 存入，`:171-178`；例如 `:115` 的 `[this, path]`），等使用者在下一幀以後確認才執行。另有 Host 物件參考 `platform_`／`renderer_`（`:948-949`，hits:110、111） | **是**：`steady_clock::now()`（`ViewerWindow.cpp:107`、`:120`，hits:101、103；成員 `last_` `:963`，hits:112）、`Engine::Time::Now()`（`:426`；`PreviewSounds.cpp:68`，人工確認）；ImGui 全域 context（134 處）。`EventSoundFiles.cpp:13` 的 `<chrono>`（hits:69）只用於型別換算（`:176-188`、`:220`），不讀時鐘，掃描誤報 | 掃描無命中＋人工確認無（音訊執行緒同 audio_editor，屬 Engine） | B | **Host**（client 重寫成 Editor host）；預覽與音訊**包**；文件與檔案**留**（細分表） | client 擁有幀內順序，且有跨幀 closure（Q2 是），遷移時要改成資料（待決動作的種類加參數）。牆鐘改由 Host 的 `FrameTime`（external）提供。`:112-113` 的選擇窗記帳缺陷見觀察 5 | 16～32 h | LIM-3 |
| `tools/ui_editor` | 6260 | **是**。client 的 `BeginFrame`（`EditorWindow.cpp:57-62`，hits:139、140）依序跑選擇窗結果→資產變更→`AssetPreviewContext::BeginFrame`→設輸入指標；`EndFrame`（`:79-82`，hits:141、142）。`PreviewAdapter` 每幀呼叫 Engine 的 `UiRuntime::Update`（`PreviewAdapter.cpp:348`，hits:148）、文件變更時 `Initialize`（`:292`，hits:147） | **是**：`EditorApp` 存下 `AssetPreviewContext& previewAssets_`（`EditorApp.hpp:91`，hits:122）與 `PathDialogs* dialogs_`（`:92`，hits:123），兩者都是同一 host 的兄弟物件，遷移後會是同一 World 的 Resource。`AssetPreviewContext` 的 `impl_->renderer`（`AssetPreviewContext.cpp:173`，掃描無命中＋人工確認）跨幀存 `SDL_Renderer*`。`physicalInput_`（`EditorApp.hpp:96`，hits:124）只在一幀內有效（`EditorWindow.cpp:61` 設、`:80` 清），不算跨 tick；`ClipScope::drawList_`（`PreviewAdapter.cpp:135`，hits:146）是區塊內 RAII，不算 | **是**：ImGui 全域 context（6 個檔共 424 處，掃描無命中＋人工確認）；`SdlTtfTextRasterizer::Create`（`AssetPreviewContext.cpp:156`，hits:127）經 Engine text 後端的全域初始化（spec L.1 `text` 列）。函式內 `static const`（`UndoStack.cpp:42`、`ElementSchema.cpp:54`）初始化後不變，不算可變。存檔經 `WriteFileAtomically`（`DocumentSession.cpp:94`） | 掃描無命中＋人工確認無 | B | **Host**（client 重寫成 Editor host）；`EditorApp` **重寫**（介面：拆掉存下的兄弟參考）；其餘見細分表 | client 是組織層。`EditorApp` 以建構子注入、跨幀持有另一個將成 Resource 的物件，照判法偏重寫該層介面（改成 System 參數取得）；內部 ImGui 繪製大半可留在一兩個 `Gpu:imgui` System 裡 | 32～56 h | LIM-3 |
| `tools/voxel_map_editor` | 5967 | **是**。`EditorClient::Draw`（`EditorWindow.cpp:107-140`）依序跑 `Shortcuts`→`ReconcileDrafts`→側欄→`DrawPreview`；`DrawPreview`（`:564-625`）依序跑 `SyncPreview`（`TakeChanges`→`MapPreview::Apply`，`:210-220`）→`PreviewScene::Apply`／`Render`（`:571`、`:589`）→`UploadFrame`→`ImGui::Image`→`Fly`→`Pick`→`Navigate`→`Edit`→`EndDrag`→`DrawOverlay`。`Initialize`（`:91`，hits:157）建 `PreviewScene`（`:97`，hits:159）。量測模式 `PreviewMeasure.cpp:204-205`、`:302` 有自己的 `PumpEvents` 迴圈（hits:171、174） | client 有 Host 物件參考 `platform_`／`renderer_`（`EditorWindow.cpp:1072-1073`，hits:163、164）。核心否：`EditSession`、`MapPreview` 的成員全是值（`Editing.hpp:115-121`、`:213-215`）；`ShapeCache::grid_`（`MapBuild.cpp:59`，hits:152）只活在一次烘焙呼叫內（`MapBuild.cpp:344`、`:369` 區域建構）；`Tools` 的函式以參數收 `EditSession&`（`Tools.hpp:61-72`），不存下 | **是（只在 client 與量測）**：ImGui 全域 context（170 處）；`io.DeltaTime`（`EditorWindow.cpp:654`）是 ImGui 的 SDL3 後端從時鐘算的幀時間（掃描看不到）。量測：`steady_clock`（`MapMeasure.cpp:29`、`PreviewMeasure.cpp:214`、`:303`，hits:155、172～176）。`std::minstd_rand(20261106)`（`Synthetic.cpp:19`、`PreviewMeasure.cpp:47`，hits:156、166）是函式內區域、固定 seed。存檔經 `WriteFileAtomically`（`FileAccess.cpp:39`） | 掃描無命中＋人工確認無 | B | **Host**（`EditorClient` 重寫成 Editor host，H.4.3）；`EditSession`、`MapPreview`、`PreviewScene` **包**；葉子**留** | 工具線給的起點成立：迴圈在 client，葉子（Build／MapBuild／Visibility／FlyCamera／Tools／Editing 的演算法）不持有跨幀參考也無全域狀態。預覽的 readback 回到 ImGui 是 H.4.3 沒寫清的方向（觀察 4） | 24～48 h | LIM-3 |
| `tools/object_fps_preview` | 144 | **未審** | — | — | — | — | **未審** | 使用者 K.3 第 8 項決定隨 `object_fps` 移除（`v8-arch-remove-object-fps`） | — | — |

工時的前提：H.4.3 的 v1 形狀（文件與 `EditSession` 先當 Resource，ImGui 繪製收成少數幾個大 System）為下限；把編輯工具拆成多個 System、`EditorApp` 全面拆分為上限。不含 Kernel 本體與 framework 標準 ScheduleDef 以外的工作。合計：framework 33～66 h，工具 80～152 h。

## 細分表（同一批模組依元件再拆；不計入件數）

### framework

| 元件 | 所在 | Q1／Q2／Q3／Q4 摘要（證據見主表或此處） | 判定 | spec L.2 初判 | 差異與原因 |
|---|---|---|---|---|---|
| `WindowShell`、`WindowShellClient` | include＋src | Q1 是（擁有 `RuntimeLoop`、pump、close request；`WindowShell.cpp:28-35`、`:61-63`）／Q2 Host 物件參考／Q3 否／Q4 否 | Host（重寫成視窗 host 骨架） | 重寫，成為視窗 host 骨架與標準 ScheduleDef | 同。補充：今天只有工具用它（觀察 2） |
| `HeadlessShell` | include＋src | Q1 否（呼叫 body 一次，`HeadlessShell.cpp:9-18`）／Q2 否／Q3 主控台模式以 RAII 切換（OS 狀態）／Q4 否 | Host（原樣，行程入口） | 留（行程入口） | 只是標籤：它不由 System 呼叫，是行程入口，歸 Host；不需改，工時 0 |
| `ImGuiShell`（binding） | bindings/imgui | Q1 是且排序別人（`ImGuiShell.cpp:408-447`）／Q2 Host 物件參考／Q3 ImGui 全域 context（`:267-269`）／Q4 否 | Host（骨架重寫）；ImGui context ＝ `Gpu:imgui` effect 的承載 | 包 | L.2 只看到全域 context；但 binding 也是 ImGui 工具的幀排序者，順序（輸入換幀、文字輸入在 `ImGui::Render` 後套用 `:429`、Present 後才 client `EndFrame` `:440`）要搬進標準 ScheduleDef 的 stage 序 |
| `OffscreenPreview` | include＋src | Q1 是但只是每幀被呼叫的括號 API（`Preview.cpp:80-111`），擁有裝置（`Create` `:43-76`）／Q2 否／Q3 否／Q4 否 | 包（渲染 Runtime 的 Resource，親和 `GraphicsContextOwner`；生命週期交 Host） | 包 | 同。照宣告字面（Q1 是→組織層）會判「Host／服務」；照 I.3 與 L.1 的讀法判「包」，兩種結果都列出（觀察 1） |
| `PreviewCamera`（`OrbitCamera`） | include＋src | 四否（純函式，`PreviewCamera.cpp:9-22`） | 留 | 包（併在 Preview 內） | L.2 把它與 Preview 併成一項；它本身是純函式。經 `Math::MakeRotation*` 用 libm 三角函數，屬 (b)（determinism.md §4.8） |
| `PreviewTexture`（`UploadFrame`） | include＋src | 四否；對傳入的 renderer 做上傳（`PreviewTexture.cpp:7-32`） | 留（由綁 Main 的 System 呼叫） | 包（併在 Preview 內） | 同上，本身是函式，不持狀態 |
| `EventSounds` | include＋src | 四否；`soundSeconds` 回呼只在呼叫內使用（`EventSounds.cpp:69`） | 留 | 留 | 同。PvP 也用（`WeaponEventSounds.cpp`） |
| `CatalogPlan` | include＋src | 四否（`CatalogPlan.cpp:28-70`） | 留 | 留 | 同 |
| `ReadOnlyAssetCatalog` | include＋src | Q1 否（`Mount`／`Unmount` 不需與他者排序）／Q2 否（值成員）／Q3 否／Q4 否；`Mount` 讀檔 | 包（值狀態成 Resource；`Mount` 由宣告 `Io:file` 的 System 呼叫） | 留 | 有狀態且有 IO，不是純函式，照判法為「包」；內部不改 |
| `UiTextInput`（`ToUiTextInput`） | include＋src | 四否；`clipboard` 回呼只在呼叫內（`UiTextInput.cpp:21`） | 留 | 未查證／留 | 補上證據。PvP Client 也用（`PvpApplication.cpp:283`） |
| `AbsolutePath` | include＋src | Q3 讀行程目前目錄（`AbsolutePath.cpp:18`） | 留（只在 Host／Startup 或 `Io:file` System 呼叫） | 留 | 同；註記它讀 OS 狀態 |
| `FrameworkError` | include | 型別定義 | 留 | — | L.2 未列 |
| json binding（`CatalogText`、`EventSoundsText`） | bindings/json | 四否（nlohmann 解析與序列化） | 留 | 未查證／留 | 補上證據 |

framework 細分分佈：Host 3（WindowShell、HeadlessShell、ImGuiShell）、包 2（OffscreenPreview、ReadOnlyAssetCatalog）、留 8。

### 工具

| 工具 | 元件 | 判定 | 證據與理由（一句） |
|---|---|---|---|
| voxel_map_editor | `EditorClient`（`window/EditorWindow.cpp`） | Host（重寫） | 擁有幀內順序（主表）；ImGui 繪製進 `Gpu:imgui` System |
| | `EditSession` | 包（文件 World 的 Resource） | 值成員、無跨幀參考（`Editing.hpp:115-121`）；Undo、Revision、`TakeChanges` 原樣 |
| | `MapPreview` | 包 | 值成員（`Editing.hpp:213-215`）；`Apply` 以參數收文件 |
| | `PreviewScene` | 包（渲染 Runtime 的 Resource） | 包住 `OffscreenPreview`（`PreviewScene.hpp:55`）與每塊 mesh |
| | `Tools`、`FlyCamera`、`Visibility`、`Orientation`、`Build`、`MapBuild`、`GridDocument`、`MarkersFile`、`BoxImport`、`ChunkedMap`、`VoxelMap` | 留 | 掃描在這些檔只命中 `ShapeCache::grid_`（呼叫內生命期）；公開 API 以參數收資料（`Tools.hpp:61-72`、`FlyCamera.hpp:39-43`）。未逐行讀 |
| | `Editing` 的 `OpenEditor`／`SaveEditor`、`FileAccess` | 留（由 `Io:file` System 呼叫） | 經 `WriteFileAtomically`（時鐘＋亂數，spec L.1） |
| | `MapMeasure`、`PreviewMeasure`、`Synthetic` | 不適用（量測與合成資料的 CLI 模式）；`PreviewMeasure` 為 Host 形狀 | `PreviewMeasure` 自己跑 `PumpEvents` 迴圈（`:205`、`:302`），不經 shell |
| audio_editor | `AudioEditorClient` | Host（重寫） | 主表 |
| | `PreviewPlayer` | 包（綁 Main 的 Resource） | `mixer_` 指向外部 Mixer（`PreviewPlayer.hpp:63`）；`at` 由呼叫者給 |
| | `Edit`、`Conversion`、`WavFile`、`ProjectFile`、`SoundExport`、`Waveform`、`FileAccess` | 留 | 掃描只命中 `Conversion` 的區域 seed 亂數；未逐行讀 |
| model_viewer | `ViewerClient` | Host（重寫） | 主表；`pendingAction_` closure 改資料；牆鐘改 `FrameTime` |
| | `PreviewRenderer` | 包（渲染 Runtime 的 Resource） | 包住 `OffscreenPreview`（`PreviewRenderer.cpp:109`） |
| | `PreviewSounds` | 包（Mixer 為服務、Submitter 為 Resource） | `PreviewSounds.cpp:27`、`:39` |
| | `SoundPlayback`（`EventSoundFiles`） | 包 | 有跨幀狀態，`Update` 的時刻由參數給（`EventSoundFiles.cpp:206`） |
| | `PlaybackClock`、`ModelSession`、`EventHistory` | 包 | 值狀態；`Advance(wallSeconds)` 以參數收時間（`Playback.cpp:8`） |
| | `EventDocument`、`EventFiles`、`ModelContent`、`ModelSummary`、`FileAccess` | 留 | 未逐行讀；掃描無命中 |
| | `PreviewMeasure` | 不適用（量測）；Host 形狀 | 自己跑 `PumpEvents` 迴圈（`PreviewMeasure.cpp:96`） |
| ui_editor | `EditorClient`（`EditorWindow.cpp`） | Host（重寫） | 主表 |
| | `EditorApp`（`EditorApp*.cpp`，約 2400 行） | 重寫（介面） | 存下兄弟物件參考（`EditorApp.hpp:91-92`） |
| | `DocumentSession`、`UndoStack` | 包 | 文件與 Undo 狀態；存檔經 `Io:file` |
| | `AssetPreviewContext` | 包（親和 Main 的 Resource） | 跨幀存 `SDL_Renderer*`（`AssetPreviewContext.cpp:173`）與紋理快取；文字後端全域初始化屬 Engine `text` 列 |
| | `PreviewAdapter` | 包 | `UiRuntime` 成 Resource、`Update` 成 System（`PreviewAdapter.cpp:348`）；繪製經 ImGui |
| | `SdlPathDialogs`、`PathDialog` | 包 | 選擇窗記帳狀態；`platform_` 參考改由 `PlatformEvents` 取得（觀察 5） |
| | `GizmoMath`、`ElementSchema`、`ElementClipboard`、`UiDocumentBridge`、`CommandLine`、`Diagnostic`、`EditorDocumentEdits` | 留 | 掃描只命中函式內 `static const`（不可變）；未逐行讀 |

## 判定分佈

- 主表（8 列）：Host 7（framework/bindings、framework/include、framework/src、四個工具，各以主成分計）、未審 1。留／包／重寫／服務作為主成分各 0。
- 細分：framework Host 3、包 2、留 8；工具 Host 4（各工具的 client）＋量測 harness 2（Host 形狀、不適用）、重寫 1（ui_editor `EditorApp`）、包 15、留 9 組。服務 0（Mixer 屬 Engine `audio` 列，不在本範圍）。

## 本範圍的觀察

1. **宣告的「Q1 為是 → 組織層」與規格 I.3／L.1 的判法不一致**（規格前提的問題，不改規格）。I.3 只把 Q1 當作「留」的排除條件，包或重寫由三問決定；L.1 也把 Q1 為是的 `input`、`asset`、`ui`、`render` 判成包。照宣告字面，`OffscreenPreview`、`PreviewPlayer`、`PreviewAdapter`、`AssetPreviewContext` 等只是每幀被呼叫的元件都會變成組織層。本報告用「排序別人才是組織層」的讀法，兩種結果都列在細分表。**三份部分報告要用同一種讀法**，否則合併後判定分佈不可比；建議主會話先定。
2. **WindowShell／ImGuiShell 在改造後的位置（spec L.2）只部分成立。** (a) `WindowShell` 成為視窗 host 骨架與標準 ScheduleDef：對 Editor host 成立（四個工具都經 `RunImGuiShell` → `RunWindowShell`）。但今天 PvP 不用 `WindowShell`（`PvpApplication.cpp:1383` 直接建 `RuntimeLoop`；framework 只用到 `ToUiTextInput`、`EventSounds`），而 H.4.2 的 Client host 是三個 Runtime 加模擬執行緒，`WindowShell` 單平台、單迴圈、主執行緒的形狀蓋不住。三層原則允許 framework 持有通用結構，但規格不應預設 PvP 會遷到它上面。(b) `ImGuiShell` 不只是「包」：它的 `Update`／`Render`（`ImGuiShell.cpp:408-447`）是寫死的幀順序，正是 H-7 的對象，要重寫成 stage 序；ImGui context 才是「包」理由所說的 `Gpu:imgui` 承載。(c) **這兩項重寫都會改 framework 的 Design Invariants**：`docs/framework/architecture.md` DI 6（`RunWindowShell` 擁有唯一的 `RuntimeLoop`、每次 pump 的事件先於該幀 `Update` 交給 `OnEvent`）與 DI 2 的文字輸入擁有規則。spec K.3 第 7 項列的三處 Delta 沒有包含它，是一個漏列的 Architecture Delta，要使用者同意。
3. **四個工具的 host 骨架重複**（數「各工具都自己寫一遍」的部分）：`Main.cpp` 經 `RunHeadlessShell` 分派 CLI 與視窗 4/4；`RunXxxWindow`（約 10 行：`WindowShellOptions` 加 `RunImGuiShell` 加 factory）4/4；`*Unavailable.cpp` 存根 4/4；`FileAccess` 包 `WriteFileAtomically`／`ReadAllBytes` 4/4（ui_editor 在 `DocumentSession.cpp:94`）；全視窗 ImGui 主窗設定 3/4；未存檔關窗提示（`CloseRequested` 回 false、下一幀問）3/4；選擇窗記帳 3/4，**三種不同實作**；離屏預覽的著色器路徑加驅動設定 2/4；量測模式自己跑 `PumpEvents` 迴圈 2/4；預覽用 Mixer 加 `SdlAudioOutput` 2/4。前四項很薄；帶判斷狀態的是選擇窗記帳與關窗提示。遷移後若各工具各自把它們寫成 System，就是 runtime-composition Replacement Criteria「Host 的協調邏輯在兩個以上 host 重複」的形狀；照 framework DI 5（兩個以上產品以同一方式組合者先進 framework），這兩項是 Editor host 標準 plugin 的候選。
4. **H.4.3 的 Editor host 形狀在預覽方向上有缺口。** 規格寫 `Preview` Runtime 從文件 World 唯讀 Extract → `RenderSubmit` → `Readback`，ImGui System 宣告 `Gpu:imgui`。但 readback 的影像要回到 ImGui 畫成 `ImGui::Image`（`EditorWindow.cpp:603`），而 immediate-mode 的 ImGui 在同一次呼叫裡讀輸入、改文件、畫圖（`:564-625`：Render→Upload→Image→Fly→Pick→Edit）。ImGui System 放在文件 World，影像就是渲染 World 到文件 World 的回流，H-6 禁止渲染 Runtime 寫回來源（含 boundary）；放在渲染 World，編輯就變成渲染 World 寫文件。可行的形狀是 ImGui System 全在文件 World，readback 經 Host 的交換點（像 Client 的 S↔F Mailbox）交過去，**代價是預覽影像比今天晚一幀**（今天同一幀）。選取與編輯用 CPU 射線打文件（`:617`），不讀 readback，所以編輯語意不變。這是行為差異，交工具線與架構師決定；本報告不改規格。
5. **model_viewer 的選擇窗記帳缺陷（`ViewerWindow.cpp:112-113`）與狀態權威。** 它用常數 tag `kContentDialog = 1`（`:43`、`:623`），所以分不出舊對話框遲到的回覆和目前的回覆；其他 tag 的回覆被取出後直接丟棄。audio_editor 用遞增的 `lastTag_`（`AudioEditorWindow.cpp:231`、`:264`），ui_editor 用 `PathDialogRequests`（`SdlPathDialogs.cpp:48-57`、`:62-73`）。三者都從平台取出全部回覆（`TakePathDialogAnswers`）後自己決定哪一個算數，「目前是哪個對話框」是判斷用的狀態。遷移後回覆經 `PlatformEvents`（boundary）進文件 World，記帳要有一個權威擁有者；這是統一的自然時點（也對應 `docs/cross-component-rules.md` 待建的第一條「選擇窗記帳規則」）。修正屬工具的行為變更，照「架構任務只做架構」交產品／工具任務，不在遷移批順手改。
6. **時鐘的來源都在 client，掃描只看到一部分。** model_viewer `steady_clock`（`ViewerWindow.cpp:107`、`:120`）與 `Engine::Time::Now()`（`:426`、`PreviewSounds.cpp:68`），audio_editor `Engine::Time::Now()`（`AudioEditorWindow.cpp:117`、`:331`、`:335`），voxel 用 ImGui 的 `io.DeltaTime`（`EditorWindow.cpp:654`，時鐘在 ImGui 的 SDL3 後端裡）。全部 zone=B；遷移後由 Host 取樣成 `FrameTime`（external）Resource（H.7「多時鐘協調」）。掃描的 clock 樣式不含 `Engine::Time::Now` 與 ImGui 的幀時間，屬 LIM-3 範圍；`EventSoundFiles.cpp:13` 是誤報（只做型別換算）。
7. **掃描漏掉的三類，都影響判定**（LIM-3）：(a) ImGui 全域 context：四個工具的 9 個檔共 814 處 `ImGui::` 呼叫，Q3 掃描 0 命中；(b) 藏在 pimpl 結構裡的跨幀指標（`AssetPreviewContext.cpp:173` 的 `SDL_Renderer*`），成員樣式只認尾底線名稱；(c) 跨幀 closure 只命中型別（hits:113），是否捕獲 `this` 要人讀（`ViewerWindow.cpp:115`、`:175`）。另外掃描依目錄切模組，`framework/include` 與 `framework/src` 是同一組元件的標頭與實作，判定只能在細分表依元件做。
8. **I.3 的四問對 Host 類模組判得出來，但答案幾乎都由 Q1 決定。** 例如 `WindowShell.cpp:38-39` 的成員參考指向 Host 自己擁有的平台與 client，不是 World 資料，Q2 答「有，但不指向 World 資料」。不算規格前提錯誤；只是對 framework 與工具 client 這類元件，Q2～Q4 提供的區分很少，真正決定工作量的是「幀內順序要搬進幾個 stage」與「有沒有回流」（觀察 4），四問沒有問到。
