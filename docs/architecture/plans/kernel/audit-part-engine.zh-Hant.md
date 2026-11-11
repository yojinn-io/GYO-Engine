# 部分報告：`v8-arch-kernel-audit` 範圍 ①　`engine/*`

更新：2026-11-11。Owner：架構（架構師會話的子代理，審查者）。**性質：三份並行部分報告之一，主會話合併並逐列核對後才進 `audit.zh-Hant.md`；本檔不是結論。**
判法、產出、判定與範囲の限界照事前宣告 `docs/architecture/plans/kernel/audit-declaration.zh-Hant.md`（分支 `claude/arch-kernel-mechanism`）。只讀程式碼，沒有改程式、沒有改契約。

## 範圍與件数の突き合わせ

- 範圍：`engine/` 下每個子目錄一列（範圍名 `engine`，`--roots engine`）。
- 審查的程式碼：master `1cff54137993d463894fe2e0adcd9b9e8341f73a`（本 worktree）。
- **件数の突き合わせ：一致**。掃描的模組數 22（`audit_summary.csv` 22 列）＝報告的列數 22＋未審 0。`engine/config` 沒有原始檔（0 檔 0 行），仍列一列，判「不適用」。
- 行數合計 30,490（`audit_summary.csv` 的 `lines` 欄，含 `.h`／`.inl`；`engine/io` 的 4,379 比 `wc` 的 .cpp／.hpp 多 8 行即此故）。

## 證據位置

- 掃描（共用腳本，未修改）：`python3 /Users/karasu/Code/Source/GYO-Engine-arch/docs/architecture/plans/kernel/scripts/audit_scan.py --repo <worktree> --out <下行> --roots engine`。
- CSV：開發機（macOS，使用者的 Mac）主 checkout 的 `build/evidence/kernel-audit/engine/`（git 不追蹤）。
  - `audit_hits.csv`（279 筆命中＋表頭；SHA-256 `0bfa0b80…02129dc`）、`audit_summary.csv`（SHA-256 `03f757ac…431586`）。
  - 本報告的「CSV n」＝`audit_hits.csv` 的第 n 行（表頭是第 1 行）。腳本對路徑排序、不用雜湊，主 checkout 同一 commit 重跑應得同樣行號。
- 補充掃描（腳本不認得的形狀；LIM-3 的實例，見觀察 2）：同目錄 `supplementary/`
  - `namespace_vars.txt`（`nsvars.py`：命名空間範圍的非 const 變數，含匿名命名空間）
  - `clock_alias_reads.txt`（經別名讀時鐘：`Clock::now()`、`Time::Now()`、`Time::NowNs()`、`SDL_GetTicksNS`）
  - `global_context_and_refs.txt`（第三方全域 context、`std::function`、成員裡的 `shared_ptr` 與介面指標）
  - 主會話對齊後追加（〈掃描漏的三類〉）：`third_party_global_calls.txt`、`pimpl_ptrs.py`＋`pimpl_raw_pointers.txt`、`monotonic_clock_includers.txt`

## 讀了哪些檔

- 必讀：事前宣告；規格 v0.2 §G（含 G.4）、§H.7、§I.3、§L；mechanism §1～§2；`target/` 全部（README、determinism、error-handling、limits、world、system-contract、scheduler、runtime-composition）；`docs/architecture/determinism.md` §2、§4。
- 程式（逐檔讀或讀到判定所需的段落）：`base/{Log,Assert}.hpp`；`entity_store/EntityColumns.hpp`；`net/GyopDatagram.hpp`；`collision/StaticAabbGrid.{hpp,cpp}`；`runtime/` 全部 7 檔；`time/{MonotonicClock,LateWakeStats,Waiter}.hpp`、`Waiter.cpp`；`threads/RoleThread.hpp`；`trace/TraceSink.{hpp,cpp}`；`os/` 公開標頭、`Shutdown.cpp`、`posix/Console.cpp`；`io/WriteFileAtomically.cpp`、`Vfs.hpp`、`MountTable.hpp`、`MountPoint.hpp`、`NativeFileSystem.cpp`（watcher 段）；`asset/AssetManager.{hpp,cpp}`、`TypeId.hpp`、`AssetWatcher.cpp`；`input/SdlInput.{hpp,cpp}`（段落）、`SdlInputInjection.hpp`、`InputActionMap.{hpp,cpp}`、`PhysicalInputFrame.hpp`；`platform/SdlPlatform.{hpp,cpp}`（全域與 dialog 段）、`SdlLogBridge.cpp`；`render/Renderer.hpp`、`Renderer.cpp`（成員）、`SdlGpuRenderDevice.cpp`（成員與執行緒檢查）、`SdlRenderer.cpp`（全域）、`shaders/pipeline/CMakeLists.txt`；`ui/UiRuntime.{hpp,cpp}`（Impl）、`UiRenderer.{hpp,cpp}`（Impl）；`text/ITextRasterizer.hpp`、`SdlTtfTextRasterizer.cpp`；`model/Animation.hpp`、`UfbxModelLoader.cpp`（全域掃描）；`audio/Mixer.hpp`、`SdlAudioOutput.cpp`（回呼段）；`config/*.csv`。
- 使用點（判 Q2 與「誰在用」）：`apps/object_fps_pvp/src/Pvp/PvpApplication.cpp:95-115`（asset 的組合方式）；`grep` 了 `IRuntimePort`、`RuntimeLoop`、`FixedTickRuntime`、`AnimationInstance`、`SdlRenderer`、`Base/Sha256` 的使用者。

## 判法的讀法（主會話 2026-11-11 對齊；最終讀法）

主會話（架構師）對齊後的讀法，以 spec I.3 為準；本報告的「判定」欄全部照它寫，已寫好的列逐列回頭核對過：

- **Q1 為是（有生命週期或更新迴圈）只排除「留」**。是「包」還是「重寫」由 Q2～Q4 決定：Q2～Q4 皆否 → 包（狀態變 Resource、每幀入口變 System、生命週期交 Host 或 plugin，內部不改）；任一為是 → 偏重寫那一層介面，或**先拆掉那些狀態再包**（表中寫「包（先拆…）」）。
- **組織層（Host／重寫骨架）**＝模組自己**排序其他元件**：寫死的呼叫序、擁有迴圈或執行緒並決定誰先誰後（對應 spec H.7）。只是「每幀被呼叫一次的 Update」不算組織層。
- **留**＝純函式且三否定皆滿足。
- 另外兩個判定照 spec §L 的定義，不受上面的讀法改變：**服務**＝經 I.1 邊界接入、不持有 World 的外部系統（自有執行緒，只經佇列交換）；**Host**＝成為 Host 的零件。本範圍把 os、time、threads 判 Host，理由是它們只持有 OS 資源（訊號處理、計時器、執行緒），只由 Host 使用、System 不呼叫，不是因為它們排序其他元件。

本報告初稿用的是另一種讀法（P／A 兩型：被動入口照 Q2～Q4 判，主動或持有 OS 資源算組織層）。兩種讀法結果不同的列，下方〈讀法差異〉列出兩個結果並標最終；表中 Q1 欄的「（P）」「（A）」是初稿留下的分型，只供對照。

## 逐模組表

| 模組 | 行數 | Q1 生命週期／更新迴圈 | Q2 跨 tick 指標或參考 | Q3 全域可變狀態（含時鐘、亂數） | Q4 自起執行緒碰 World 資料 | zone 初判 | 判定 | 理由 | 工時粗估 | 受 LIM 影響 |
|---|---|---|---|---|---|---|---|---|---|---|
| `engine/asset` | 3333 | **是（P）**：`AssetManager::BeginFrame`／`Update`（`AssetManager.hpp:59`、`:62`；CSV 2、3、15、16）；`AssetWatcher::Poll`（`AssetWatcher.cpp:50`；CSV 12、19）由 `Update` 呼叫（`AssetManager.cpp:26`、`:237`；CSV 17）。無執行緒；「async」是 `Update` 內同步讀檔的假非同步（`AssetManager.cpp:226-234`） | **是**：`AssetManager` 存五個參考與兩個指標到組合根擁有的協作者（`AssetManager.hpp:157-163`；CSV 4～10）；`AssetPipeline` 存 `IAssetSource&`、`LoaderRegistry&`（`AssetPipeline.hpp:23-24`；CSV 13、14）。產品端把它們並排放在同一個 `Impl`（`PvpApplication.cpp:96-104`；`tools/ui_editor/src/AssetPreviewContext.cpp:94-98` 同形） | **是（僅熱重載）**：`AssetWatcher::NowNs` 讀 `system_clock`（`AssetWatcher.cpp:149`；CSV 18、20），熱重載預設關（`AssetManager.hpp` Options `enableHotReload=false`）。`TypeId` 的函式內 `static const int kToken`（`TypeId.hpp:37`）不可變，但以位址當型別 id、`operator<` 依位址（`TypeId.hpp:32`）→ 跨行程不同序（W-10 的形狀；B.6 風險 5） | 否：掃描無命中（Q4 欄 0）＋人工確認無執行緒 | B（邊界外；只搬位元組，內容身分的計算在 `model` loader 與產品） | **包**（先收攏參考） | 更新是被動的，狀態可成一個 Resource；但五個協作者今天由組合根各自持有，要先收進同一個 Resource（例如以 `AssetManager` 擁有它們），參考才變成 Resource 內部的；`Update` 內同步 IO 要宣告 `Io:file` 副作用。loader 留 | 8～20 h | LIM-3（經存下參考讀寫攔不到；收攏前的參考形狀正是 L2） |
| `engine/audio` | 1619 | **是（A）**：`Mixer::Create`、`SdlAudioOutput::Open`（CSV 35、56、21、31）；`SDL_InitSubSystem(SDL_INIT_AUDIO)`（`SdlAudioOutput.cpp:112`）、`SDL_OpenAudioDeviceStream` 帶回呼（`:122`）；大量 `std::atomic`（CSV 23～30、37～52、59～70） | 否（World 資料）：`Submitter` 持有 `shared_ptr<MixerState>` 與 `SubmitterSlot*`（`Mixer.hpp:159-160`；CSV 34），都指向 mixer 自己的狀態 | **是**：`Submitter` 綁第一個呼叫它的執行緒（`Mixer.cpp:68`、`:237-238`；CSV 40、53、54）；音訊回呼讀 `Time::Now()`（`SdlAudioOutput.cpp:78`，掃描未命中，見補充 `clock_alias_reads.txt`）；`Play` 以 Engine 時間戳排程（`Mixer.hpp` `Stop(…, Time::TimePoint at)`，`:131`；CSV 33） | 否（World 資料）：執行緒由 SDL 起（回呼 `SdlAudioOutput.cpp:76`），只碰 `MixerState`；CSV 41、53、54 是 `thread::id` 型別命中，不是起執行緒 | B（determinism.md §4.5：邊界外；整數混音天生 (c)） | **服務**；`Submitter` 是帶執行緒親和的 Resource | 有自己的執行緒與 OS 裝置，經 SPSC 佇列收請求，符合 I.1 外部系統；整數混音契約不變 | 2～6 h | LIM-2 不影響（不同執行緒＝外部服務） |
| `engine/base` | 670 | 否：掃描無 Q1 lifecycle／update 命中；CSV 71、74 是 handler 的 atomic，不是迴圈 | 否：掃描無命中＋人工確認（全為值型別與函式） | **是（例外）**：`logHandler`（`Log.hpp:43`；CSV 74、75）、`assertionHandler`、`thread_local reportingAssertion`（`Assert.hpp:49-50`；CSV 71～73）；只寫不讀回、不影響結果 | 否：掃描無命中＋人工確認 | C（`Sha256` 被內容身分 `PlayerAnimationDigest.cpp` 使用；`StrictJson` 解析契約資料）；handler 不適用 | **留**（附例外） | `Result`／`Error`／`StrictJson`／`Sha256`／`Fnv1a` 是純工具；log 與 assert 的全域只寫不讀，照 spec I-5 列例外。target error-handling DI 2（`Base::Result` 保持純值）不受影響 | 0～2 h（把例外寫進 DI） | 無 |
| `engine/collision` | 962 | 否：CSV 76～79 的 `Initialize` 是 `StaticAabbGrid::Build` 內部的私有建構步驟（`StaticAabbGrid.hpp:124`、`StaticAabbGrid.cpp:64-76`），建好後不變；無更新入口 | 否：`StaticAabbGrid` 以 `std::vector` 擁有複本（`StaticAabbGrid.hpp:141-147`），掃描無命中 | 否：掃描無命中＋`namespace_vars.txt` 無本模組列 | 否：掃描無命中 | C（determinism.md §2 預測核心、§4.4） | **留** | 純查詢；`StaticAabbGrid` 跨 tick 保存時放 Resource（不可變）；collision DI 1 不受影響 | 0～2 h | 無 |
| `engine/config` | 0 | 不適用：無原始檔（`projects.csv`、`tools.csv`） | 不適用 | 不適用 | 不適用 | 不適用 | **不適用** | 產品／工具登記資料，不是程式；不過 `projects.csv` 列著 `object_fps`、`object_fps_v2`，K.3 第 8 項的移除批要一併刪列（§7 完整刪除） | 0 h（移除批 0.1 h） | 無 |
| `engine/entity_store` | 155 | 否：掃描無命中＋人工確認（只有 `FindEntity`／`InsertEntity`／`EraseEntity` 函式模板，`EntityColumns.hpp:52-91`） | 否：模組不存指標；`FindEntity` 回傳的指標由呼叫者持有，文件規定下一次插入／刪除即失效（`EntityColumns.hpp:23-25`） | 否：掃描無命中＋人工確認 | 否：掃描無命中 | C（determinism.md §4.7） | **留**（演算法）＋成為 World 欄的實作 | 正是 W-1、W-2 的實作；spec L.1 列的 Delta（回傳位置的查找、批次合併、DI 4 延伸）屬 World 批，不是本模組的改造 | 0 h（模組本身）；Delta 計入 World 批 | 無 |
| `engine/input` | 1156 | **是（A）**：`SdlInput::BeginFrame`／`HandleEvent`／`EndFrame`（`SdlInput.hpp:35-39`；CSV 80、81、84、85），`HandleEvent` 在 `SdlPlatform::PumpEvents` 的 observer 內、主執行緒呼叫 | **是（Host 物件，非 World 資料）**：`SdlPlatform* platform_`（`SdlInput.hpp:70`、`SdlInputInjection.hpp:29`；CSV 82、83） | **是**：讀 SDL 全域狀態 `SDL_GetMouseState`（`SdlInput.cpp:484`）、`SDL_TextInputActive`（`:363`）、`SDL_GetWindowFlags`（`:287`）；`PhysicalInputFrame.timestamp` 是時間點型別（`PhysicalInputFrame.hpp:112`；CSV 86、87），不是讀時鐘。`InputActionMap` 三否定（`InputActionMap.cpp:75-90` 只疊代 vector） | 否：掃描無命中 | B（邊界外；指令由產品量化後才進 (c)） | **包（先拆：SDL 全域查詢與 `platform_` 移到 Host 的 pump）**＋**留**（`InputActionMap`）。初稿讀法：Host | `SdlInput` 不排序其他元件，只是被 pump 的 observer 每幀呼叫，所以不是組織層；但 Q2、Q3 為是。拆法：滑鼠位置、文字輸入是否開啟、視窗焦點由 Host 在 pump 時取樣，與事件一起寫進 boundary Resource（spec O2、P1）；`BeginFrame`／`HandleEvent`／`EndFrame` 的狀態機就只讀那份資料，可成 Resource＋System。`SetRelativeMouseMode`／`SetTextInput` 是對視窗的操作，改成 System 發出、Host 執行的請求。`InputActionMap` 是純映射 | 4～10 h | LIM-3（Host 碰過的資料再被 System 讀，屬 L6 形狀） |
| `engine/io` | 4379 | **部分（P）**：串流 `Open`／`Close` 持有檔案控制代碼（RAII，逐次呼叫；CSV 88～125）；`IFileWatcher::Poll`（`IFileWatcher.hpp:69`；CSV 91）只有介面，唯一實作回 `Unsupported`（`NativeFileSystem.cpp:427-429`）；無更新迴圈 | 否（World 資料）：`StreamReader`／`StreamWriter` 存 `IStream& s_`（CSV 109、110），是短命包裝；`MountPoint` 以 `shared_ptr<IFileSystem>` 擁有後端（`MountPoint.hpp:30`），在 `Vfs` 內部 | **是**：`WriteFileAtomically` 讀 `steady_clock` 並用 `std::random_device` 產暫存檔名（`WriteFileAtomically.cpp:15-16`；CSV 116～119）；其餘掃描無命中 | 否：掃描無命中 | B（邊界外；只搬位元組） | **包**（檔案服務作為 `Io:file` 副作用）＋**留**（路徑、Uri、記憶體串流） | 時鐘與亂數只決定暫存檔名、不回流到任何結果，所以「先拆狀態」不需要，但它不能被 (c) 檔 include（J-17）；檔案操作是副作用，只能由宣告 `Io:file` 的 (b) System 或 Host 呼叫 | 2～6 h | LIM-3（全域狀態 L5 不解除） |
| `engine/map_markers` | 457 | 否：掃描無命中＋人工確認（解析與展開函式） | 否：掃描無命中 | 否：掃描無命中＋`namespace_vars.txt` 無列 | 否：掃描無命中 | C（determinism.md §2 契約資料、§4.6） | **留** | 契約資料的純展開 | 0 h | 無 |
| `engine/math` | 1451 | 否：掃描無命中＋人工確認（值型別與函式） | 否：掃描無命中 | 否：掃描無命中＋`namespace_vars.txt` 只有一個 `concept`（非變數） | 否：掃描無命中 | C（呼叫處在邊界內；determinism.md §4.8，E1 前 `sin`/`cos`/`acos` 依平台 libm） | **留** | 純函式；決定性的缺口（E1）是 determinism 線的事，不是 Kernel 改造 | 0 h | 無 |
| `engine/model` | 1174 | **是（P）**：`AnimationInstance::Advance(deltaSeconds)`（`Animation.hpp:81`、`Animation.cpp:213`；CSV 126、127）由呼叫者給時間；其餘（`SamplePose`、`BlendPoses`、`SkinMesh`、ufbx loader）是函式 | **是（共享、不可變）**：`AnimationInstance::model_` 是 `shared_ptr<const ModelAsset>`（`Animation.hpp:88`）；不構成寫入繞路，但作為元件時是跨 tick 參考 | 否：掃描無命中；`UfbxModelLoader.cpp` 無命名空間範圍變數（`namespace_vars.txt` 無列）；ufbx 自身的全域狀態未查證（第三方列） | 否：掃描無命中 | 取樣 B／DET-1 中間地帶（determinism.md §4.2）；**ufbx loader C**（內容身分，§4.9） | **留**（取樣、loader）＋**包**（`AnimationInstance`，或隨 object_fps_v2 移除後刪） | 取樣是純函式；`AnimationInstance` 的使用者只有 `apps/object_fps_v2`（`EnemySystem.cpp`）與 `tests/common/model/ModelTests.cpp`，K.3 第 8 項移除後產品端無人用 | 1～4 h | 無 |
| `engine/net` | 144 | 否：掃描無命中＋人工確認（`EncodeGyopDatagram`／`DecodeGyopDatagram`） | 否：`GyopDatagram.payload` 是指向輸入位元組的 span（`GyopDatagram.hpp:59-63`），由呼叫者持有 | 否：掃描無命中 | 否：掃描無命中 | B（determinism.md §2 Outside：wire codec） | **留** | 純編解碼 | 0 h | 無 |
| `engine/os` | 2355 | **是（A）**：`FileReader`／`FileWriter::Open`／`Close`（`Files.hpp:41`、`:86`、`:98`；CSV 129～131、137～139、144～146）；`InstallShutdownRequests` 裝訊號處理（`Console.hpp:76`）；mutex（CSV 136、143） | 否：掃描無命中＋人工確認（值型別與 RAII 控制代碼） | **是**：`Shutdown.cpp:11` 的 `constinit std::atomic requested`（CSV 134）；`posix/Console.cpp:24-25`、`windows/Console.cpp:54-55` 的 `installing`／`installed`（掃描只認出 mutex，`installed` 見 `namespace_vars.txt`）；`clock_gettime`、`system_clock`（CSV 140～142、147、148）；`getenv`（`posix/Files.cpp:412-419`） | 否：掃描無 thread-start；`Thread.cpp` 只設定目前執行緒的名稱與優先度 | 不適用 | **Host**（adaptor） | OS 層原意是 adaptor；檔案部分經 `io` 的 `Io:file` 副作用被間接使用，System 不直接呼叫；訊號→`Interrupted` 的處置是 H.7「Runtime Error 與 Interrupted 的處置」 | 0～4 h | 無 |
| `engine/platform` | 1285 | **是（A）**：`SdlPlatform::Create`、`PumpEvents`（`SdlPlatform.hpp:225`、`:239`；CSV 151、152、166、167）；`SetLiveFrameHandler`（`SdlPlatform.hpp:253`）；`SDL_InitSubSystem(SDL_INIT_VIDEO)`（`SdlPlatform.cpp:211`） | **是（回呼）**：`EventObserver`、`LiveFrameHandler` 是 `std::function`（`SdlPlatform.hpp:221-222`；CSV 149、150），live frame handler 跨幀保存並捕獲呼叫者狀態 | **是**：`thread_local` ×3（`SdlPlatform.cpp:61`、`:73`、`:75`；CSV 157～159）＋`SdlLogBridge.cpp:24`（CSV 154）；永不銷毀的 dialog 登記表（`SdlPlatform.cpp:112-115`；CSV 163）；`registeredWatches`（`:78`）、`SdlLogBridge.cpp:20-21` 的 `state`／`bridged`；`SDL_GetTicksNS`、`Time::Now()`（`:299`、`:660-661`；CSV 171） | 否（World 資料）：SDL 的 dialog 回呼在 SDL 的執行緒寫 mailbox（`SdlPlatform.cpp:121-160`），不是本模組起的、也不碰 World | B | **Host** | pump、live frame、視窗與 dialog 是 H.7 列的 Host 職責；全域登記表與 `thread_local` 無法變成 Resource，所以不是「包」；平台 DI 7（observer 與 live frame 內不做 operation）由 Host 的 pump 處理器守 | 4～10 h（`PlatformEvents` boundary） | LIM-3（host 碰過同欄 L6） |
| `engine/render` | 4300 | **是（A）**：`Renderer::Initialize`／`Reset`／`Render`（`Renderer.hpp:24`；CSV 183、194）；`SdlGpuRenderDevice::Create`／`Shutdown`（CSV 175、178～182）；裝置綁建立它的執行緒（`SdlGpuRenderDevice.cpp:122`、`:147`、`:261`；CSV 177）。`shaders/pipeline/src/main.cpp` 是建置期執行檔 `gyo_shader_tool`（`shaders/pipeline/CMakeLists.txt:20`），CSV 188～193 屬工具不屬執行期 | **是（Host／裝置物件）**：`Renderer::Impl` 存 `IRenderDevice*`、`const ShaderLibrary*`（`Renderer.cpp:134-135`）；裝置存 `SdlPlatform*`（`SdlGpuRenderDevice.cpp:117`）；`ModelResource` 存 `IRenderDevice*`（`ModelRenderer.cpp:22`，人工補看 pimpl 時找到）；`ModelInstance` 持 `shared_ptr<ModelResource>`（`ModelRenderer.hpp:79`）。其餘 pimpl 指標是自己擁有的 SDL 控制代碼（`supplementary/pimpl_raw_pointers.txt`） | **是**：命名空間範圍的 atomic 計數 `nextGeneration`（`SdlRenderer.cpp:27`；CSV 173）、`nextFrameToken`（`SdlGpuRenderDevice.cpp:20`；CSV 176）；`Time::NowNs()` 量 swapchain 等待（`SdlGpuRenderDevice.cpp:1006-1016`，掃描未命中） | 否：掃描只有 `thread::id` 型別命中（CSV 177）；無起執行緒 | B（邊界外） | **包（先收攏：裝置、`ShaderLibrary`、`ModelResource` 的裝置指標收進渲染 World 的同一個 Resource）**，親和 `GraphicsContextOwner`。初稿讀法：A 型應算組織層，但初稿已判包 | 不排序其他元件（`Render(queue)` 只處理交給它的佇列），不是組織層；Q2 的指標都指向渲染側的物件，收進同一個 Resource 後變成內部的；Q3 的全域計數只產生跨實例唯一的 handle 值、留在 (b)，列為審查項不拆；`RenderQueue` 是 extract 的目標 | 8～24 h | LIM-2 不影響（一 World 一執行緒） |
| `engine/runtime` | 291 | **是（A）**：`RuntimeLoop::Run` 迴圈呼叫 `IRuntimeClient::ProcessEvents`／`Update`／`Render`（`RuntimeLoop.cpp:27-58`、`IRuntimeClient.hpp:12-14`；CSV 196、197、199、200）；`RunLiveFrame`（`RuntimeLoop.hpp:31-37`）。`FixedTickRuntime::Advance` 以回呼驅動 tick（`FixedTickRuntime.hpp:44-66`；CSV 195） | **是**：`RuntimeLoop::client_` 跨幀指向 client（`RuntimeLoop.hpp:45`；CSV 198）；`IRuntimePort::Query()` 回借用的參考，「runtime 前進前要複製」（`IRuntimePort.hpp:10-11`、`:17`） | **是**：`RuntimeLoop` 讀 `Clock::now()`（`RuntimeLoop.cpp:34`、`:51`、`:67`；`Clock`＝`steady_clock`，掃描未命中）；`FixedTickRuntime` 以 double 累加並傳出 `deltaSeconds`（`:19`、`:48-56`，G-6）。無全域變數 | 否：掃描無命中 | 不適用（Host 的時序；G-6 定為 (b)） | **重寫**（`RuntimeLoop`／`IRuntimeClient`）＋**Host**（`FixedTickRuntime`）＋**刪除候選**（`IRuntimePort`） | 三段硬編碼的調度由 Host＋ScheduleDef 取代，live frame 留成 Host 職責（H.7）；`FixedTickRuntime` 主動驅動 tick，是 Host 的時鐘零件，步長改整數 `stepNs`（G-6）；`IRuntimePort` 只有 object_fps／v2 與 `tests/common/core/runtime/RuntimePortTests.cpp` 在用，借用視圖的形狀與 Link（複製進佇列）相反 | 8～20 h（含 FixedTickRuntime 2～4 h） | LIM-1（Host 決定 stage 順序） |
| `engine/text` | 285 | **是（A）**：`SdlTtfTextRasterizer::Create` 呼叫 `TTF_Init`、解構呼叫 `TTF_Quit`（`SdlTtfTextRasterizer.cpp:55-70`；CSV 201、202） | 否：掃描無命中；`ITextRasterizer::Rasterize` 只在呼叫期間借字型位元組（`ITextRasterizer.hpp` 註解） | **是（第三方全域 context）**：SDL_ttf 的 `TTF_Init` 引用計數；`SDL_GetError`（`:45`） | 否：掃描無命中 | B | **包**（後端：Resource，生命週期交 Host）＋**留**（`ITextRasterizer` 介面與型別） | 唯一實作的後端有全域初始化（I.3 第 1、3 條不滿足），不能當葉子；Q3 是第三方的全域 context，拆不掉，契約只能靠審查（LIM-3 的 L5）；介面本身無狀態，`Rasterize` 逐次呼叫，沒有可重寫的介面層，所以判包、附審查項。初稿讀法：A 型應算組織層，但初稿已判包 | 1～3 h | 無 |
| `engine/threads` | 221 | **是（A）**：`RoleThread::Start`（`RoleThread.hpp:73`、`RoleThread.cpp:41-66`；CSV 207、208、219～221）、`RequestStop`／`Join`；擁有 `std::thread`（`RoleThread.hpp:100`；CSV 216） | 否（World 資料）：`RoleContext::state_` 指向自己的 State（`RoleThread.hpp:58`；CSV 204）；`Body` 是 `std::function`（`:69`；CSV 206），內容由 owner 給 | 否：掃描無 Q3 命中＋`namespace_vars.txt` 無列 | 起執行緒＝是（CSV 215、223、224）；**碰 World 資料＝否**（本模組只跑 owner 給的 body） | 不適用 | **Host**（runner 的落點） | DI 4「the owner orders roles」（`RoleThread.hpp:60-61`；spec H.7 引 `:62`，實際是 `:60-61`）正是 H.7 的「執行緒的建立與角色綁定」 | 0～2 h | LIM-2（一 World 一執行緒，RoleThread 是落點） |
| `engine/time` | 408 | **是（A）**：`Waiter::Create`（`Waiter.hpp:68`；CSV 239、248）持有 OS 計時器（kqueue／waitable timer）；mutex、condition_variable、atomic（CSV 242～247、249） | 否：CSV 249 是 `WaitUntil` 內部的 RAII 守衛（`Waiter.cpp:174-177`），不跨呼叫 | **是（時鐘）**：`MonotonicClock`＝`steady_clock`、`Now()`、`NowNs()`（`MonotonicClock.hpp:18-26`；CSV 237、238）；`Waiter.cpp:184`、`:187` 讀 `Now()`；`LateWakeStats` 是值型別、無全域 | 否：掃描無命中 | 不適用（時鐘樣本經 Host 進 `HostClockSample`，那是 C 的 boundary 輸入，但模組本身不分區） | **Host** | 時鐘與等待是 Host 職責；(c) 檔不得 include（J-17）。`Time::Now()` 這個別名是全 engine 讀時鐘的主要入口，掃描沒認出（觀察 2） | 0～2 h | 無 |
| `engine/trace` | 362 | **是（A）**：`TraceSink::Open` 起 writer 角色執行緒（`TraceSink.cpp:125-150`；CSV 262～266）、`Close`（CSV 253、272）；mutex／condvar（CSV 256～258） | 否（World 資料）：`EndLine` 是 `Close` 的參數，不存（`TraceSink.hpp:102-103`；CSV 252） | **是（只在 `FlushNow`）**：`steady_clock` 算逾時（`TraceSink.cpp:185`、`:190`；CSV 268、270）；無全域變數 | 起執行緒＝是（CSV 266）；碰 World 資料＝否（只寫已序列化的字串佇列，`TraceSink.cpp:160-178`） | B | **服務**（`Diagnostics` 副作用的接收端） | System 只推已序列化的列；寫檔執行緒不讀 World | 2～6 h | 無 |
| `engine/ui` | 4471 | **是（P）**：`UiRuntime::Initialize`／`Update`（`UiRuntime.hpp:77`、`:86`；CSV 276、277、279、280）；`UiRenderer::Initialize`（`UiRenderer.hpp:36`；CSV 274、278） | **是**：`UiRenderer::Impl` 跨幀存 `IRenderDevice*`、`ITextRasterizer*`、**`Asset::AssetManager*`**（`UiRenderer.cpp:147-149`）；`UiRuntime::Impl` 存 `UiTextFilter`（`std::function`，`UiRuntime.cpp:859`；CSV 275）與指向自己文件的 `const UiCanvas*`（`:853`） | 否：掃描無命中＋`namespace_vars.txt` 無變數列；`unordered_map` 只作查找（`UiRenderer.cpp:151-153`、`UiValidation.cpp:71-72`；`Reset` 疊代它釋放 GPU 資源 `:158-160`，順序不影響結果） | 否：掃描無命中 | B（邊界外；版面有字面位元 golden，但不在邊界內） | **包**（`UiRuntime`：狀態→Resource、`Update`→System）＋**局部重寫**（`UiRenderer` 的取得方式）＋**留**（文件、編解碼、驗證、版面） | `UiRenderer` 把 `AssetManager`（將是 Resource）與裝置的指標存過幀；runtime-composition §2 把 Client 的 Frame World 與 Render World 分開後，這是跨 World 的指標，要改成每次由句柄傳入或在 extract 解析字型與圖 | 8～18 h | LIM-3（經存下參考讀 L2） |
| `engine/voxel_mesh` | 1012 | 否：掃描無命中＋人工確認（文件解析與網格展開函式） | 否：掃描無命中 | 否：掃描無命中＋`namespace_vars.txt` 無列 | 否：掃描無命中 | C（determinism.md §2 契約資料、§4.6） | **留** | 契約資料的純展開，字面位元 golden 守著 | 0 h | 無 |

### 判定分佈（主判定）

| 判定 | 數 | 模組 |
|---|---|---|
| 留 | 8 | base、collision、entity_store、map_markers、math、model、net、voxel_mesh |
| 包 | 6 | asset（先收攏）、input（先拆）、io、render（先收攏）、text、ui |
| Host | 4 | os、platform、threads、time |
| 服務 | 2 | audio、trace |
| 重寫 | 1 | runtime |
| 不適用 | 1 | config |
| 合計 | 22 | ＝掃描模組數 22，未審 0 |

分項（主判定以外的部分）：input 的 `InputActionMap` 留；io 的路徑／Uri／記憶體串流留；model 的 `AnimationInstance` 包或刪；runtime 的 `FixedTickRuntime` Host、`IRuntimePort` 刪除候選；text 的介面留；ui 的 `UiRenderer` 局部重寫、文件與版面留。

「無結論」：0。

### 讀法差異（初稿 P／A 讀法 → 最終讀法）

| 模組 | 初稿讀法的結果 | 最終讀法的結果 | 差異的原因 |
|---|---|---|---|
| input | Host（pump 的零件） | **包（先拆）** | `SdlInput` 不排序其他元件；Q2、Q3 的狀態可移到 Host 的 pump |
| render | 照 A 型應為組織層（初稿未照做，寫了包） | **包（先收攏）** | 不排序其他元件；初稿表內不一致，最終讀法下一致 |
| text | 照 A 型應為組織層（初稿未照做，寫了包） | **包**（附審查項） | 同上 |
| runtime（`FixedTickRuntime`） | Host（A 型） | **Host** | 最終讀法下仍是組織層：它決定這一幀跑幾次 FixedUpdate，正是 H.7 的「多時鐘協調」；只套四問會得「包」（Q2～Q4 皆否），但 spec G-6 把累加器明定為 Host 的邏輯、不進 World，所以不判包 |
| os、time、threads | Host（A 型） | **Host** | 不排序其他元件，但只持有 OS 資源、只由 Host 使用（spec §L 的 Host 定義） |
| platform | Host | **Host** | `PumpEvents` 在 OS modal loop 裡回呼 live frame handler 跑 Update／Render（`SdlPlatform.hpp:239-253`），即排序其他元件 |
| asset、ui、model、io | 照 P 型判 | 同 | 初稿已照 Q2～Q4 判 |

工時合計（粗估，不是承諾）：約 52～150 h，集中在 render、asset、runtime、ui、platform、input。

## 掃描漏的三類，人工補看（主會話 2026-11-11 指示；LIM-3 範圍，命中只靠閱讀）

審查者②回報 `audit_scan.py` 漏三類，加上時鐘樣式 `Engine::Time::Now`。本範圍逐類補看，結果放 `supplementary/`，下表是對判定的影響。

| 類別 | 怎麼看 | 結果 | 影響的列 |
|---|---|---|---|
| 第三方全域 context（`ImGui::`、SDL 全域） | `grep` SDL 全域查詢與設定、`TTF_*`、`IMG_*`、`ImGui::`，依模組計數（`third_party_global_calls.txt`） | `engine/` 內**沒有** `ImGui::`。SDL 全域狀態：input 讀 `SDL_GetMouseState`、`SDL_TextInputActive` ×6、`SDL_GetWindowFlags`、`SDL_GetHint` 等；audio `SDL_InitSubSystem`／`SDL_SetHint`；platform `SDL_InitSubSystem`、`SDL_SetHint`、`SDL_SetLogOutputFunction`、`SDL_AddEventWatch`；text `TTF_Init`／`TTF_Quit`；asset、render 只有 `SDL_GetError`／`SDL_ClearError`（SDL 的執行緒區域錯誤字串，不影響結果） | input、audio、platform、text 已在表中；asset、render 的 `SDL_GetError` 不改判定 |
| pimpl 結構裡的跨幀裸指標 | `pimpl_ptrs.py`：`struct Impl`／`*State`／`*Slot` 第一層的裸指標成員（`pimpl_raw_pointers.txt`，26 筆） | 指向**別的元件**的：`Renderer.cpp:134-135`（裝置、`ShaderLibrary`）、`ModelRenderer.cpp:22`（裝置，**表中初稿漏列，已補**）、`SdlGpuRenderDevice.cpp:117`（`SdlPlatform`）、`UiRenderer.cpp:147-149`（裝置、文字、`AssetManager`）。`UiRuntime.cpp:853` 指向自己文件內的 canvas。其餘 19 筆是自己擁有的 SDL 控制代碼（stream、window、GPU 資源） | render（Q2 已補 `ModelRenderer.cpp:22`）、ui（已在表中） |
| lambda／`std::function` 捕獲 `this` 並跨幀保存 | `grep` `[this`、`[=`、`[&`、`[x = …get()]` 與存起來的回呼（`SetTextFilter`、`SetLiveFrameHandler`、`AddEventWatch`、`RoleThread::Start`） | engine 內的 `[this]` 兩處都是當下用完、不保存：`UiRenderer.cpp:448`（`std::visit`）、`UiRuntime.cpp:1125`（`find_if`）。**跨幀保存的**：`TraceSink.cpp:145` 把自己的 `Impl*` 交給 writer 執行緒（到 `Close` 為止）；`RoleThread.cpp:53` 捕獲自己的 State；`SdlPlatform.cpp:256`、`:620` 把自己的 `Impl*`／node 交給 SDL 的 event watch；`SdlLogBridge.cpp:78` 交 `&state`。這些都指向模組自己的狀態，不指向 World。產品側：`PvpApplication.cpp:1389` 的 `SetLiveFrameHandler([this, &loop]…)` 存在 platform 裡跨幀；`PvpApplication.cpp:997` 的 `SetTextFilter` 是無捕獲 lambda | platform（Q2 已寫 live frame handler 跨幀捕獲）、ui（`UiTextFilter` 目前唯一的使用者無捕獲，不改判定；若產品改成捕獲狀態，就違反 system-contract §3 的無捕獲原則，要經 Resource 傳入） |
| 時鐘樣式 `Engine::Time::Now` | 已在初稿的 `clock_alias_reads.txt`（樣式 `Now()`／`NowNs()`／`Clock::now()`／`SDL_GetTicks`）；另列 include `engine/time/MonotonicClock.hpp` 的檔（`monotonic_clock_includers.txt`，8 檔：audio ×2、platform、render、runtime、threads、time ×2） | 讀時鐘的地方與表中所寫一致：runtime、platform、render、audio、time、asset（`system_clock`）、trace、io、os | 無新增 |

## 本範圍的觀察

1. **判法前提的不一致（I.3／宣告）已由主會話對齊**：宣告的「Q1 為是 → 組織層」與「包＝更新變 System」對有被動更新入口的模組給相反答案；spec I.3 沒有前一句。主會話 2026-11-11 定案（上方〈判法的讀法〉）後，本範圍逐列重核：判定改變的只有 input（Host → 包（先拆）），render 與 text 的初稿判定在新讀法下才前後一致（〈讀法差異〉）。事前宣告該句的文字仍是舊的，修不修交主會話（本批不改宣告與規格）。
2. **掃描腳本漏的形狀（LIM-3 的實例；影響 spec J-17 的掃描清單）**：除了下面兩類，審查者②回報的三類見上方〈掃描漏的三類，人工補看〉：
   - 經別名讀時鐘：`Clock::now()`（`RuntimeLoop.cpp:34`、`:51`、`:67`）、`Time::Now()`（`SdlPlatform.cpp:299`、`:660`；`SdlAudioOutput.cpp:78`；`Waiter.cpp:184`）、`Time::NowNs()`（`SdlGpuRenderDevice.cpp:1006-1016`）。`audit_scan.py` 的 clock 樣式只認 `steady_clock` 等原名，所以只在 `MonotonicClock.hpp` 命中一次。J-17 若照同一樣式掃 (c) 檔，會漏掉 include `engine/time/MonotonicClock.hpp` 後呼叫 `Time::Now()` 的檔。建議 J-17 掃 include 而不是函式名。
   - 匿名命名空間裡的非 const 變數：`SdlLogBridge.cpp:20` `BridgeState state`、`posix/Console.cpp:25`／`windows/Console.cpp:55` `bool installed`、`SdlRenderer.cpp:27`、`SdlGpuRenderDevice.cpp:20`、`Shutdown.cpp:11`。`static-mutable` 樣式要求 `static` 關鍵字，所以全部未列為 Q3；本範圍以 `supplementary/namespace_vars.txt` 補上。spec I.3 第 3 條說「照 entity-store 守衛列 5、6、8」，那份守衛是否有同樣的盲點未查證。
3. **與 spec §L.1 初判不同之處與原因**：
   - `input`：初判「後端包」→ 本報告「後端包，但要**先拆**」（初稿曾判 Host，照主會話的讀法改回）。`SdlInput` 持 `SdlPlatform*`、讀 SDL 全域視窗與滑鼠狀態（`SdlInput.cpp:287`、`:363`、`:484`），Q2、Q3 為是，初判「包」少了先拆這一步：那些查詢要移到 Host 的 pump，取樣結果與事件一起進 boundary Resource，狀態機才能包。
   - `platform`：初判「包（Host 擁有）」→「Host」。永不銷毀的 dialog 登記表（`SdlPlatform.cpp:112-115`）與三個 `thread_local` 無法成為 Resource，「包」的定義不成立；實質（Host 擁有）相同。
   - `runtime` 的 `FixedTickRuntime`：初判「留（Host 的時鐘工具）」→「Host」。`Advance` 以回呼驅動 tick，是 A 型的更新迴圈（`FixedTickRuntime.hpp:44-56`）；用語差異大於實質。`IRuntimePort`：初判「-audit 定」→ **刪除候選**：K.3 第 8 項移除 object_fps／v2 後只剩它自己的測試；借用視圖（`Query()` 回參考，前進前要複製）與 Link（訊息複製進佇列）形狀相反，不能當 Link 的前身。刪不刪交使用者（可併入 `v8-arch-remove-object-fps` 做 §7 的清理）。
   - `ui`：初判「包」→ 加「`UiRenderer` 局部重寫」。`UiRenderer` 跨幀存 `AssetManager*`（`UiRenderer.cpp:149`），在 Frame／Render World 分開、渲染只經 Extract 以 const 讀來源 World（runtime-composition §2、DI 1）後是跨 World 的指標。`AssetManager` 該屬 Frame World 還是 Render World（貼圖上傳要裝置），target 文件沒有寫，改造批要先定。
   - `asset`：初判「包」→ 同意，但 Q2 為是：五個協作者由組合根各自持有、`AssetManager` 存參考（`AssetManager.hpp:157-163`），包之前要先收進同一個 Resource。另外「async」是 `Update` 內同步讀檔（`AssetManager.cpp:226-234`），即在 System 內做阻塞 IO。
   - `render`：初判的「裝置綁建立它的執行緒（未複核）」已確認（`SdlGpuRenderDevice.cpp:122`、`:147`、`:261`）；另有初判未列的全域 atomic 計數（`SdlRenderer.cpp:27`、`SdlGpuRenderDevice.cpp:20`）與 pimpl 裡的跨元件指標（`Renderer.cpp:134-135`、`ModelRenderer.cpp:22`、`SdlGpuRenderDevice.cpp:117`），判定從「包」改為「包（先收攏）」。
   - `model`：初判「動畫推進包」→ `AnimationInstance` 產品端只有 object_fps_v2 在用，移除後可刪；初判寫「取樣是 (b)」，但 ufbx loader 是內容身分的 (c) 路徑（determinism.md §4.9），同一模組兩個區。
4. **決定性區表（determinism.md §4）的回對候選**（交主會話的 (e) 節，本範圍不改契約）：§4 沒有列 `engine/base`，但 `Sha256` 是內容身分 `AnimationContentDigest`（`PlayerAnimationDigest.cpp`）的計算、`StrictJson` 是契約資料的解析入口，兩者都在邊界內；§4.2 只講 `GYO::Model` 的取樣 (b)，ufbx loader 的 (c) 寫在 §4.9 的產品段落而 §4.2 沒交叉引用；`FixedTickRuntime` 的 double 累加（G-6）與 `engine/time` 作為 Host judgement 的時鐘來源（§2 的 injected clock）在 §4 沒有列。
5. **spec H.7「今天在哪裡」的 engine 部分已逐一確認**：`FixedTickRuntime`（`FixedTickRuntime.hpp:44`）；`RoleThread.hpp`（spec 寫 `:62`，DI 4 的句子實際在 `:60-61`）；`RuntimeLoop.hpp:31-37`（`RunLiveFrame`）；pump 另見 `SdlPlatform.hpp:239`、`:253`；訊號→`Interrupted` 的來源是 `os` 的 `InstallShutdownRequests`（`Console.hpp:76`）與 `RequestedShutdown`（`Shutdown.cpp:22-26`）。spec G.4 的三條違反都在產品（`PvpMatch`、`MatchRuntimeHost`、`MovementDiag`），不在本範圍。
6. **engine 內的工具與產品資料**：`engine/render/shaders/pipeline/` 是建置期執行檔 `gyo_shader_tool`，不是執行期程式碼，CSV 188～193 的命中（含 `steady_clock` nonce）不影響 render 的判定；`engine/config/projects.csv` 列有產品名（含要移除的 object_fps、object_fps_v2）。兩者都不是 Kernel 審查的對象，只記錄，是否屬「Engine 不承擔工具職能」交 owner 判斷。
7. **組織層之外，engine 本身大多是留或包**：重寫只有 `runtime` 一個模組（291 行，硬編碼的三段調度）；Host 四個（os、platform、threads、time）都是今天已經由 Host 使用的 adaptor；與 mechanism §1、spec L.5 的預測一致。成本主要在「包」的六個模組怎麼把今天由組合根手接的參考（asset、ui、render 的裝置與 platform 指標）收進 Resource 或改成每次傳入，以及 input 的 SDL 查詢移到 Host 的 pump。
