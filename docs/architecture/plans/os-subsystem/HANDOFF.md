# OS 子系統：交接

更新：2026-10-25。**狀態：計畫已核准**（D46）；A1～A3（os-skeleton、io-contracts、os-files-read）本機 L1 完成，os-files-read 的 xhigh 審查已修正（D47）；四平台 L1 待推送。2026-10-25 由架構任務在規劃批次中建立。
本文件是持續記錄器：每批開始、里程碑、停止時，與工作在同一變更中更新。停下或回報前，先更新「現況」。依 D12 匿名。

## 現況

- 2026-10-25：使用者核准 PLAN 第 1 版（D46），說「開始 os-skeleton 與 io-contracts」。
- os-skeleton：GYO::Os、OsError、os.md、守衛（Engine 範圍）、突變 runner 擴充完成，本機 core preset 的 L1 全過、突變 11／11 killed。四平台 L1 要等分支推送（見未結事項）。
- io-contracts：file:// 對應、ParseUri／ParseUriLoose／ToString 修正、io.md、MountTable 不越出根目錄完成；Red→Green、契約審查（ultracode）的發現已修正、本機 L1 全過、突變 11／11 killed。
- os-files-read：使用者說「先不推送，繼續做 os-files-read，A1～A3 都完成後再一起跑 L1」。engine/os 的唯讀檔案原語（POSIX、Windows）與 engine/io 的 NativeFileSystem 完成，本機 L1 全過，突變在本機 7／7 killed，另有 2 個 Windows 專用的突變要到 Windows 才能跑。
- os-files-read 的 xhigh 審查（使用者同意：審查公開介面與 Windows 實作，修完後再推送）：沒有 blocker。1 個 major，即 `PathToUtf8`／`PathFromUtf8` 在 Windows 會丟例外 → 使用者決定改成回傳 Result（D47）。其餘 minor、nit 已修，測試缺口已補。本機突變 34／34 killed（2 個 Windows 專用的跳過）。
- 下一步：推送本分支，手動觸發 `cross-platform.yml` 跑四平台 L1。Windows 的程式碼第一次在 CI 編譯。
- 消費端的發行任務已收到分工、交接內容與 D46（A1～A3 併進它的發行線 PR），等 A1～A3 完成（L1 通過後通知分支與 commit）。

## 閱讀入口

1. [進度與執行規則](README.md)。
2. [批次計畫](PLAN.md)：架構樹、部件、邊界、Design Invariants 草案、守衛、批次、交接、Delta、開放決策。
3. 本文件：代號對照、決策、消費端的壓力摘要、研究事實、批次紀錄、未結事項。
4. 相關 Engine 計畫：[時間、執行緒與 Trace](../time-threads-trace/README.md)（Waiter、RoleThread、TraceSink 的既有約定與 L2 基準）、[輸入與呈現](../input-and-present/README.md)（platform-sdl 的 Process services、延後的 AtomicWriteFile）、[Result 統一](../result-unification/README.md)（Vfs 的 characterization）。

## 批次代號對照

| 代號 | 白話 |
|---|---|
| os-plan-docs | 寫這份計畫，交使用者核准 |
| os-skeleton（A1） | 建立 engine/os、GYO::Os、OsError、os.md 與 Engine 範圍的守衛 |
| io-contracts（A2） | 定下 engine/io 的路徑契約（UTF-8、file:// 對應、錯誤碼）並修好 ParseUri |
| os-files-read（A3） | engine/os 的唯讀檔案原語，加上 engine/io 呼叫它們的唯讀原生檔案系統 |
| P1～P5 | 消費端的長路徑修正與驗收（不屬於本計畫） |
| os-guard-wide（A1b） | 守衛擴到產品、工具、產品測試與驗收程式 |
| os-process-info | 執行檔路徑、CPU 時間、本地時間搬進 engine/os |
| os-console-shutdown | 命令列、主控台輸出、結束要求搬進 engine/os |
| os-files-write | 寫入原語，消費端剩下的寫檔改走 engine/io |
| os-tool-files | 編輯器的原子存檔、路徑比較、系統字型目錄改用 engine/os |
| os-trace-writers | trace 寫檔改走 engine/os，位元組不變 |
| os-path-utf8-sweep | 剩下的窄字串路徑改 UTF-8，PathFromUtf8 移到 engine/os |
| time-threads-invariants | 先替 time 與 threads 寫 Design Invariants |
| os-wake-record | 晚醒量測程式合回 master，量四平台基準 |
| os-threads-backend | 執行緒命名與優先度的 OS 呼叫搬進 engine/os |
| os-wait-backend | 等待原語的 OS 後端搬進 engine/os，ABAB L2 |
| os-net-errors | socket 錯誤碼的中立分類（條件式） |
| os-close | 收尾：allowlist 清空、文件同步、完整突變 |

## 決策紀錄

Engine 側的決策號，在本計畫建立前最大是 D39（time-threads-trace 的 xhigh 審查），所以本計畫從 D40 開始。消費端的決策號寫成「消費端 Dnnn」。

| 日期 | 決策 | 來源 |
|---|---|---|
| 2026-10-25 | D40：目標是整理 OS 子系統，不是修路徑；長路徑只是 Files 部件第一個觀測到的壓力 | 使用者：「你的目標不是修路徑，你的目標是整理OS的子系統。」 |
| 2026-10-25 | D41：分層是 asset → io → os → base。<br>engine/os 只依賴 engine/base 與標準庫，API 只用中立型別，不認識 Uri 與 IFileSystem。<br>engine/io 保留中立抽象，新增不含 OS 標頭、呼叫 engine/os 的原生 IFileSystem 後端。<br>Waiter、RoleThread、產品與工具照同一模式改呼叫 engine/os。<br>engine/io 目前沒有可搬出的 OS 程式，原生後端與檔案原語是新程式 | 使用者：「那原生系統相關的實作放到 engine/os 下，io 則調用 engine/os。正常來說，io 一定會碰到操作系統問題的，如果沒碰到說明就是沒有使用和沒有對OS進行兼容，這個也是目前要修的地方之一。」；確認「asset → io → os 的原生實作」時：「這個方向沒有問題。」 |
| 2026-10-25 | D42：asset 的讀檔改經 engine/io 的 IFileSystem（IAssetSource 與 IFileSystem 的職責重複）；NativeFileAssetSource 暫時保留類別名與 API，當薄轉接；IAssetSource 的去留是開放決策 4 | 使用者選擇「為 engine/io 的 IFileSystem 寫只讀的原生實作，asset 的讀檔改走它」；之後：「我還以為asset會把io的實作利用起來，原來是自己做的呀。」 |
| 2026-10-25 | D43：Engine 字串裡的路徑現在就改成 UTF-8；產生 resolver 根目錄的呼叫端，和 asset 改走 IFileSystem 放在同一個變更 | 使用者對「路徑編碼要怎麼處理？」選：「現在改成 UTF-8」 |
| 2026-10-25 | D44：現在就修 ParseUri 的 file:// 規則；file:// 與原生絕對路徑只有一種對應，三平台都有解析測試 | 使用者對「file:// 的 Uri 和原生絕對路徑的對應怎麼定？」選：「現在就修 ParseUri」 |
| 2026-10-25 | D45：產品任務和架構分開。<br>本計畫只交付結構與契約。<br>長路徑的行為修正與驗收（P1～P5）由消費端的發行任務在 A1～A3 完成後進行；P1 寫在 engine/os 的 Windows 檔案原語內，遵守它的 Design Invariants | 使用者在消費端的發行任務會話說：「你來做！！！，那邊只修架構，你等那邊修好了，就做。產品任務和架構分開。」（經該會話轉達，消費端 D197）；界線由本計畫依架構需要劃定，在核准 PLAN 時一併確認 |
| 2026-10-25 | D46：核准 PLAN 第 1 版。<br>開放決策 1：A1～A3 在消費端發行前完成，其他全部排在發行之後。<br>開放決策 14：UNC 根目錄這一輪暫不支援，io.md 寫明限制，P1 不寫 `\\?\UNC\` 分支。<br>開放決策 15：A1～A3 併進消費端的發行線 PR。<br>同意 A1～A3 的 Delta（新 target GYO::Os、os.md 與 io.md 的 Design Invariants 節） | 使用者：「核准 os-subsystem 的 PLAN。開放決策 1、14、15 照建議：(1) A1～A3 在發行前，其他全部排在發行後；(14) UNC 根目錄這一輪暫不支援；(15) A1～A3 併進消費端的發行線 PR。同意 A1～A3 的 Delta（新 target GYO::Os、os.md 與 io.md 的 Design Invariants 節）。」 |
| 2026-10-25 | D47：`Os::Files::PathToUtf8`／`PathFromUtf8` 改成回傳 `Base::Result<…, OsError>`，不合法的輸入一律回 InvalidEncoding，兩平台一致、不丟例外。修訂 PLAN 第 3.2 節的簽名。起因：xhigh 審查發現 MSVC 的 `std::filesystem::path` 遇到不合法的 UTF-8 或未配對 surrogate 會丟例外，違反 os.md Invariant 5、6 | 使用者對「PathToUtf8／PathFromUtf8 遇到不合法的輸入時要怎麼處理？」選：「改成回傳 Result」 |

## 消費端的壓力摘要

提出需求的消費端是一個線上遊戲，有繪製畫面的程序、權威模擬的伺服器端程序，以及 Go 的轉送程序。以下是與 Engine 有關的事實；未標「實機」的是靜態分析。

- **事故（實機）**：
  - 2026-10-16，Windows 機器把發行包解壓到 `C:\Users\<名字>\Downloads\<下載工具產生的資料夾>\<兩層同名資料夾>\` 底下，畫面程序開不了 shader，路徑 271 字元。
  - 證據：主 checkout 的 `build/target/_build/test/logs/pvp-v71-windows-lan-20261016/logs/clientA/logs/client-20261016-104011.log` 第 2 行。
  - 同一台機器、同一個 exe sha（`55c5c3ac…`）搬淺兩層後就正常（同目錄的 `client-20261016-104436.log`）。
  - 錯誤訊息分不出「路徑太長」與「檔案不存在」，因為 ifstream 不給 OS 錯誤碼。
- **深度門檻**：P 是路徑到 `bin\` 為止的字元數，以「P＋bin 之後的長度 ≥ 260」視為失敗。長度由主 checkout 的 `build/target/object_fps_pvp` 實際檔案配置算出。

  | 檔案 | 開檔者 | bin 之後的長度 | 開始失敗的 P |
  |---|---|---|---|
  | shader `.dxil` | `NativeFileAssetSource.cpp:21` | 115 | 145（事故 P＝156） |
  | 其他資產 | 同上 | ≤96 | 164 |
  | `content.json`、`asset_catalog.json` | `ContentManifest.cpp:93`、`AssetCatalog.cpp:30` | 34～40 | 約 220 |
  | `arenas.json`、場地檔 | `Arena.cpp:64、127` | 33～38 | 約 222 |
  | log | `TraceSink.cpp:102`（經 `LogFile.cpp:129`） | 31 | 229 |
  | movement trace | `TraceSink.cpp:102` | 26 | 234 |
  | 執行檔本身 | Windows 啟動 | 22 | 約 238〔推論：之後 Windows 本身就開不了〕 |

- **產品與工具的 OS 分支**：
  - `LogFile.cpp:22-30、38-46、57-84`：執行檔路徑、localtime。
  - `main.cpp:65-75`：localtime，同一知識的第二份。
  - `NetworkStatistics.cpp:11-38、58-80`：程序與執行緒的 CPU 時間。
  - `match_main.cpp:10、22-23、81`：std::signal。
  - `tools/ui_editor/src/FileService.cpp:13-92`：MoveFileExW、不分大小寫的路徑比較。
  - `ImGuiSdlRendererHost.cpp:39-111`：系統字型目錄。
- **錯誤分類**：`ClientConnection.cpp:885-891` 的 TransientSocketError 沒有 OS 巨集，WSAECONNRESET 只出現在測試註解裡（`ConnectionLivenessTests.cpp:103`）；暫時性的判斷是消費端政策（消費端 D172 D-7）。

## 研究事實

- **engine/io**：
  - `engine/io/src/fs/*.cpp` 都只有空的 namespace，engine/io 裡 grep 不到任何 OS 巨集、OS 標頭或 fstream。
  - 全 repo 唯一的 IFileSystem 實作是測試的 ScriptedFileSystem（`tests/common/core/io/VfsTests.cpp:20`）；asset 只用 engine/io 的路徑工具（`AssetPathResolver.cpp:3-4`）。
  - engine/io 編在 GYO::Engine 裡（`engine/CMakeLists.txt:13-45`），Time、Threads、Trace 連不到它。
- **ParseUri 與路徑工具**：
  - `ParseUri` 對沒有 scheme 的輸入拒絕絕對路徑（`Uri.cpp:87-101`）。
  - 對 file:// 會把 `file:///Users/x` 變成 `Users/x`（`Uri.cpp:118-121`）。
  - `DirectoryEntry.hpp:14` 的範例 `file://C:/...` 會把 `C:` 當成 authority。
  - `Uri::ToString` 在 authority 為空時輸出 `file://C:/x`（`Uri.cpp:43-46`）。
  - `NormalizeSlashes` 預設會把開頭的 `//` 壓成 `/`（`PathUtils.cpp:247-268`、`AssetPathResolver.hpp:22`），也會把 `\\?\` 改寫成 `//?/`。
- **路徑編碼**：
  - `options.assetsRoot` 由 `.string()` 產生：`PvpApplication.cpp:812`、兩個 `ObjectFpsApplication.cpp:71`、`ReadOnlyAssetCatalog.cpp:46`、`ContentManifest.cpp:104`。catalog 路徑另有 `ContentManifest.cpp:110` 與 `ReadOnlyAssetCatalog.cpp:55`。
  - 在 MSVC 上這些是 ACP 窄字串，`std::ifstream(std::string)` 也以 ACP 解讀〔推論，依 MSVC STL 語意〕。
  - repo 裡沒有 Windows application manifest（longPathAware、activeCodePage）。
- **既有守衛的限制**：
  - `time-threads-trace/scripts/run_mutations.py:48-55` 的 check() 只能建置 target 並執行同名執行檔，跑不了 Python 守衛。
  - 在 macOS 的 libc++ 上，只 include `<string>`、`<filesystem>` 就會定義 `_PTHREAD_H`（對抗式檢查在本機實測）；Linux 的 libstdc++ 經 `<bits/gthr-posix.h>` 同樣會定義。
- **計時基準**：
  - TT-1 L2 閒置時 CLI 的 Waiter P99 0.10 ms、cv 4.06 ms；視窗的 Waiter P99 0.133 ms（time-threads-trace/HANDOFF.md:97-108）。
  - 量測程式 `WakeRecord.cpp` 只在本機分支 `claude/tt1-l2-wake-record`（`a8f9535`）。
  - Windows 上不初始化 SDL 的程序，計時精度從未量過。

## 批次紀錄

### os-plan-docs

- 2026-10-25：開始。先回答消費端的阻塞問題「長路徑怎麼修」（os-longpath-decision），使用者在 D41～D45 定下方向，接著規劃整個子系統。
- 2026-10-25：盤點與設計（ultracode，workflow `wf_014e00ee-e72`，9 個 agent）。
  - 盤點：4 路並行（Engine、產品與工具、測試與建置、既有邊界文件），共 150 個站點。
  - 設計：3 個角度各寫一案（風險優先、Ownership 先行、消費端優先）。
  - 審查：1 位審查者評分 7／8／6，以「Ownership 先行」案為底，移植另外兩案的優點。
  - 對抗檢查：1 位對照程式碼找到 19 項檢查、20 個遺漏站點。
- 2026-10-25：審查進行到一半時，使用者陸續定下 D40～D45，工作流程兩度停下，帶著新決定續跑審查與對抗檢查；盤點與設計沿用快取。
- 2026-10-25：寫 PLAN 時修正了對抗檢查找到的問題：
  - 標頭衛生只檢查 Windows 巨集，原因是 `_PTHREAD_H` 在 macOS 與 Linux 一定會被定義。
  - 突變 runner 加 command 型檢查；文字突變改成放在 `#if 0` 內，四平台都能編譯。
  - io-contracts 補上 ToString、MountTable 的往返，Loose 只對 file:// 套新規則；UNC 列為開放決策 14。
  - OsError 的 detail 改用 CauseDetail 的格式。
  - Capabilities 補上 canRemoveRecursive 與 ToNativePathString。
  - 刪掉和 D41 衝突的「永久例外」選項。
  - Invariant 10 對照現行行為重寫；Block 保留現行例外規格。
  - io-contracts 與 os-skeleton 改成並行；產品的 allowlist 移到發行後的 os-guard-wide。
  - os-files-write 拆出 os-tool-files；補上主控台 UTF-8 輸出與 sweep 漏掉的站點。
  - PLAN 依 D12 匿名。
- 證據：workflow 的完整輸出在本機會話暫存，未進 repo；PLAN 引用的 file:line 都以 master `23fab75` 核對過，批次開始時重新核對。
- 2026-10-25：使用者核准 PLAN 第 1 版，開放決策 1、14、15 照建議（D46）。本批完成。

### os-skeleton

- 2026-10-25：開始（high），基準 master `23fab75`（開工前確認 master 沒有新提交）。
- 實作：
  - `engine/os`：`gyo_os`／`GYO::Os`（只連 GYO::Base），`OsError`（`OsErrorCode` 從 1 開始；detail 的固定格式 `errno=<n>`／`win32=<n>`，與路徑有關時加 `; path_bytes=<n>`，由 `NativeErrorDetail`、`MakeOsError` 產生）。`engine/CMakeLists.txt` 在 math 之後、time 之前加入。
  - `docs/architecture/os.md`（英文）：Contracts、Guard、已知盲點、Design Invariants 1-10、Replacement Criteria 1-7，內容依 PLAN 第 5 節。
  - 守衛 `tests/common/os/os_boundary_guard.py`：R1～R5，掃 engine 與 tests/common；engine/os/src 只套 R5，engine/os/include 全套。CTest `os.boundary_guard`、`os.boundary_guard_selftest`（24 項，臨時目錄裡的合成樹，不讀 repo 檔案）、`gyo_os_tests`；DEFER 的 `gyo_expect_links(gyo_os GYO::Base)`。
  - `tests/common/os/os_boundary_allowlist.txt`：Engine 的基準。
  - 突變 runner（`time-threads-trace/scripts/run_mutations.py`）加上 command 型檢查（`"command"` 清單，`{python}` 代表執行中的直譯器），既有的 target 型不變。
  - 本計畫的突變目錄 `scripts/mutations.json`。
- 基準：空 allowlist 實跑共 58 個發現、12 個檔案。和盤點一致：
  - R1／R2：Engine 的 `Waiter.cpp`、`RoleThread.cpp`，共通測試的 WaiterTests、RoleThreadTests、TraceSinkTests、EntryTests、AssetPathResolverTests。
  - R3：`Waiter.cpp`、`RoleThread.cpp`。
  - R4：asset 4 個（NativeFileAssetSource、AssetCatalog、ContentManifest、AssetWatcher）與 `TraceSink.cpp`。
  - 計數已寫進 allowlist。
- L1（本機 macOS Intel，core preset）：`ctest` 30／30 通過。
- 突變（PLAN 第 6 節的 os-g1～os-g9，另加 os-g10 R4 的 std::filesystem 操作、os-e1 detail 格式）：11／11 killed，只靠預期訊息判定。
- 未啟用產品的遷移清單加「OS」節（foundation-followups/inactive_products.md）。
- 證據：本機執行輸出（git 忽略）；突變與守衛可用 PLAN 第 6 節的指令重跑。

### io-contracts

- 2026-10-25：開始。檔位 high；字串規則局部 xhigh；契約審查 ultracode（1 審查者＋1 對抗檢查）。PLAN 標「先問」，依使用者先前的指示（標先問的 ultracode 照計畫直接開）沒有再問。
- Red（§14.3）：先寫 `tests/common/core/io/UriTests.cpp`，在舊碼上跑，12 個案例中 11 個失敗。例如 `file:///Users/x` 得到 `Users/x`、`file://C:/x` 被接受、`#` 被拆成 fragment。剩下 1 個案例釘住非 file scheme 的既有行為。
- 實作：
  - `Uri.cpp`：file:// 由獨立分支處理，非 file scheme 的程式碼不變。
  - `ToString` 印出正規形式。
  - `ParseUriLoose` 對 file:// 套同一規則。
  - 新增 `FileUriFromNativePath`、`NativePathFromFileUri`。
  - `DirectoryEntry.hpp` 的範例改成 `file:///C:/...`。
  - `docs/architecture/io.md`（英文）：路徑契約、行為變更、Design Invariants 1-5、Replacement Criteria 1-4。
- 契約審查（workflow `wf_ab6de80a-f62`）：兩位都判「先修再提交」。審查者 13 項（5 major），對抗檢查者 9 項（7 major），重疊很多。這一批修正的：
  - ParseUriLoose 對被拒的 file URI 退回舊規則，經 MountTable 往返後，UNC 與 drive-authority 變成有效路徑 → 被拒的 file URI 在 Loose 裡不帶 scheme，永遠沒有原生路徑。
  - `file:////server/share`（四斜線 UNC）被當成本機路徑 → NotSupported。
  - `/C:/x` 與 `file:///C:\x` 往返後指向另一個檔案 → 先把反斜線轉成 `/` 再判斷磁碟機；「看起來像磁碟機的 POSIX 路徑」一律 InvalidPath；加上不動點測試。
  - MountTable 請求裡的 `..` 會在新的正規化中被解析掉，越出 mount 根目錄 → `Resolve` 遇到 `..` 段回 PathEscapesRoot。這是本批新加的正規化造成的退步，所以在本批修；審查者建議先和使用者確認範圍，修法與理由已向使用者回報。
  - native 的 `C:`、`\x`（相對於目前磁碟機）、裝置與 NT 前綴（`\\.\`、`\??\`）→ InvalidPath。
  - `NativePathFromFileUri` 對手工建立的 Uri 重新驗證：authority、query、fragment 不可有；路徑必須正規化後與自身相同；NUL 不可有。
  - Loose 的 file scheme 判斷不分大小寫。
  - 磁碟機代號只認 ASCII（不受 locale 影響）。
  - io.md 裡「正式的 resolver 都不允許 scheme」與事實不符：shader bundle 的 resolver 用預設值允許 scheme → 改正敘述，並在 AssetPathResolverTests 加一個新判定。
  - io.md 的 Invariant 2、4 改寫成可以檢查的形式。
- 不在本批修的既有問題：ParseUriLoose 對未知 scheme 不設定 schemeText，所以 `pak://...` 經 MountTable 往返後會失去 scheme（與 HEAD 相同）。屬於 io.md 的 Replacement Criteria 1、4，等有 pack 後端時處理。
- 行為變更（寫在 io.md）：
  - 允許 scheme 的 resolver 現在把 `file:///x`、`file://localhost/x` 當成絕對路徑而拒絕。出貨的 catalog 與 manifest 都沒有 file:// 條目。
  - `MountTable::Resolve` 拒絕 `..` 段。
  - 既有的 VfsTests 與 AssetPathResolverTests 不改判定也通過。
- L1（本機 macOS Intel，core preset）：
  - engine_tests 103／103 案例。UriTests 共 23 個案例；AssetPathResolverTests 新增 1 個。
  - ctest 30／30；守衛通過。
- 突變：io-m1～io-m11，11／11 killed。包含 PLAN 要求的三個：開頭斜線、只認大寫、ToString 舊形式。
- 證據：本機執行輸出（git 忽略）；審查的探測程式在本機會話暫存。

### os-files-read

- 2026-10-25：開始（high；UTF-8↔UTF-16 與 Windows 錯誤對應局部 xhigh）。公開介面的 xhigh 審查（1 位）依 PLAN 要徵求同意，實作完成後再問。
- 實作：
  - `engine/os/include/engine/os/Files.hpp`：`ReadFile`（Open、Read、Size、Seek、Tell；pimpl，標頭不出現 handle 或 fd）、`Stat`、`Exists`、`MakeAbsolute`、`PathToUtf8`／`PathFromUtf8`、`DefaultCaseSensitivePaths`。
  - `src/common/Paths.cpp`：UTF-8 驗證（RFC 3629：不接受 overlong、surrogate、超過 U+10FFFF）與路徑引數檢查（空字串、NUL、不合法 UTF-8）。這些在呼叫 OS 前就拒絕，detail 只有 `path_bytes=<n>`。
  - `src/posix/Files.cpp`：open／read／fstat／stat／lseek／getcwd，EINTR 重試，O_CLOEXEC。
  - `src/windows/Files.cpp`：`MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS)` 轉 UTF-16，`CreateFileW`／`::ReadFile`／`GetFileSizeEx`／`SetFilePointerEx`／`GetFileAttributesExW`／`GetFullPathNameW`。Win32 錯誤碼對應到 OsErrorCode。
  - 兩邊都把「開啟目錄」報成 InvalidPath。
  - CMake 依 WIN32 選檔（開放決策 11 的 (c)）。
  - `engine/io` 的 `NativeFileSystem`：唯讀；Capabilities 每個欄位都明確設定；OsError 用 CauseDetail 轉成 IoError；讀取用的 IStream 可 seek。
  - `GYO::Engine` → PRIVATE `GYO::Os`。engine/io 的公開標頭不出現 engine/os 的型別。
- 守衛的調整：engine/os 內以相對路徑 include（例如 `../common/Paths.hpp`）時，依實際指向的檔案判斷是否留在 engine/os 或 engine/base 內。selftest 新增 1 項，共 25 項。
- 突變 runner 的調整：加上 `platforms` 欄位。只在某些平台編譯的程式碼，其突變在其他平台標為 skipped，不列入計數。
- 測試：
  - `tests/common/os/FilesTests.cpp` 11 個案例：
    - 讀取、seek、tell；
    - ACP 以外字元（日文、正體中文）的路徑；
    - 不存在時回 NotFound，detail 帶原生碼與 path_bytes；
    - 目錄回 InvalidPath；
    - 不合法引數與 UTF-8；
    - Stat、Exists、MakeAbsolute、UTF-8 互轉、大小寫預設值；
    - 只記錄：超過 260 字元的路徑。macOS 上是 `NotFound errno=2; path_bytes=418`。
  - `tests/common/core/io/NativeFileSystemTests.cpp` 7 個案例，包含 VFS mount 經 NativeFileSystem 一路讀到 engine/os 的端到端案例。
- L1（本機 macOS Intel，core preset）：gyo_os_tests 15／15；engine_tests 全過；ctest 全過；守衛通過（engine/io 沒有任何 R1～R4 命中）。
- 突變：本機 7／7 killed（os-f1～os-f3、io-n1～io-n4）。os-f4（Windows 改用 CP_ACP）、os-f5（Windows 的 NotFound 對應）只在 Windows 跑，本機標為 skipped，見未結事項。
- Windows 的程式碼在本機編譯不到，第一次編譯會是四平台 L1 的 windows-x64。
- 證據：本機執行輸出（git 忽略）。
- 2026-10-25：xhigh 審查（1 位，使用者同意涵蓋公開介面與 Windows 實作）。審查者逐行以 MSVC 的方式讀 Windows 實作：
  - 名稱查找（`::ReadFile`）、巨集衝突、narrowing、警告設定（repo 沒有 /WX）、UTF-16 轉換的緩衝、`GetFullPathNameW` 迴圈，都沒有問題。
  - 沒有 blocker。
  - 1 個 major：`PathToUtf8`／`PathFromUtf8` 在 Windows 會丟 `std::system_error` → D47，改成回傳 Result。Windows 的 `PathToUtf8` 直接由 `path.native()` 以 `WC_ERR_INVALID_CHARS` 轉換。
  - 這一批同時修正的 minor 與 nit：
    - Windows 的 `Stat`／`Exists` 原本不跟隨 symbolic link → reparse point 改開目標並描述它。
    - FILETIME 為 0 與超出範圍時的有號溢位 → 0 與飽和；POSIX 的 tv_sec 同樣處理。
    - `Seek(INT64_MIN)` 的 `-offset` 是未定義行為 → 改成 `offset < -base`。
    - moved-from 的 `ReadFile` → `GYO_ASSERT`，並在標頭寫明。
    - `GetLastError`／`errno` 在組字串之前就先讀出。
    - `WideCharToMultiByte` 改用 `WC_ERR_INVALID_CHARS`，未配對 surrogate 不再被默默換成 U+FFFD。
    - seek 超過檔尾 → 和其他唯讀串流一樣回 SeekFailed（io.md）。
    - `MakeAbsolute` 兩平台的差異 → 寫進標頭與 os.md。
    - 在這個平台不是絕對路徑的 file URI 路徑 → 新增 `IsAbsolutePath`，由 NativeFileSystem 拒絕。
    - 層自己拒絕的引數不再捏造原生碼 → detail 只有 path_bytes。
    - 空 span 的 Read 不呼叫 OS。
    - FIFO 與裝置 → POSIX 用 O_NONBLOCK 開啟，只接受一般檔案；Windows 用 `GetFileType` 判斷。
    - 關閉後的串流與無效 mode 的錯誤碼。
  - Windows 原語裡，「UTF-16 轉換」（`ToWide`）和「交給檔案 API 的形式」（`OsCallPath`）分成兩個函式，給 P1 加長路徑前綴的位置（PLAN 第 8 節）。
  - 補上的測試缺口：
    - 3、4 位元組的 overlong；
    - 父路徑是檔案；
    - `MakeAbsolute` 的非 ASCII；
    - `Seek(UINT64_MAX)`；
    - move 與 moved-from；
    - 沒有 UTF-8 形式的 path；
    - io 層的錯誤碼對應；
    - mtime 對照寫入前後的時鐘；
    - 目錄的 sizeBytes；
    - FIFO 與 NUL；
    - symbolic link；
    - `Seek(INT64_MIN)` 與超過檔尾；
    - Windows 的 `GetACP` 記錄；
    - 平台絕對路徑。
  - 測試數：gyo_os_tests 19 個案例、NativeFileSystemTests 9 個。
  - 突變：os-f4 改成兩個方向都換成 ACP。新增 os-f6～os-f8、io-n5、io-n6。整份目錄在本機 34／34 killed，Windows 專用的 2 個跳過。
  - 本機 L1：ctest 30／30，守衛通過。
- 2026-10-25：推送本分支，手動觸發四平台 L1：[run 40172686040](https://github.com/yojinn-io/GYO-Engine/actions/runs/40172686040)，`04beea4`。
  - 結果：失敗。macos-x64、macos-arm64、linux-x64 的 L1 與 quick acceptance 都成功；windows-x64 的 L1 與兩個 quick acceptance 失敗。
  - 原因：只有 `tests/common/os/FilesTests.cpp` 編譯失敗（`error C2872: 'ReadFile': ambiguous symbol`）。這個測試為了 `GetACP` include 了 windows.h，又寫了 `using namespace Engine::Os::Files;`，類別 `ReadFile` 因此和 Win32 的 `::ReadFile` 撞名。
  - engine/os 的 `src/windows/Files.cpp` 與 NativeFileSystem 第一次在 MSVC 上編譯，都成功了。
  - 這正是審查 finding 15 預警的陷阱。產品的翻譯單元會經 asio 間接看到 windows.h，所以只在測試裡加命名空間不夠：趁還沒有任何使用者，把類別改名為 `FileReader`。os.md 的 Contracts 加上命名規則：不重用 Win32 函式名，也不用 windows.h 的 A／W 巨集名。突變目錄的 find 與 expect 字串同步改名。
  - 改名後本機：ctest 30／30，守衛通過，相關突變 12／12 killed。

## 未結事項

- ~~使用者核准 PLAN，回答開放決策 1、14、15，並同意 A1～A3 的 Delta。~~ 2026-10-25 完成（D46）。
- A1～A3 併進消費端的發行線 PR（D46）。消費端的發行任務 2026-10-25 回覆的做法（暫定，開 PR 時再和使用者確認，消費端 D198）：
  - A1～A3 的分支從 master 切，不帶消費端其他線的改動。本計畫的分支 `claude/trusting-joliot-f13b78` 是從 master `23fab75` 切出的。
  - 消費端的發行線在它目前那條線的 PR 合併後，從 master 切出 `claude/pvp-v71-release`。
  - A1～A3 完成後，由消費端以 `git merge --no-ff` 併進發行線（不 rebase、不 squash，保留 commit 與訊息），再在上面做 P1～P5；整個 PR 照 repo 規則以 `gh pr merge --merge` 合進 master。
  - A1～A3 若在發行線開好之前完成，就等它開好再併。P4 在產品檔案的那一行，以及和消費端其他線的衝突，由消費端在發行線處理。
  - 完成時通知消費端：分支名稱與最後的 commit。
- ~~os-skeleton 開始時：用空 allowlist 實跑守衛，產生 Engine 的基準，對照盤點。~~ 2026-10-25 完成，和盤點一致。
- A1～A3 的四平台 L1：使用者決定 A1～A3 都完成、審查修完後再推送（2026-10-25）。D46 決定 A1～A3 併進消費端的發行線 PR，所以先推送本分支，再手動觸發 `cross-platform.yml`。
- ~~os-files-read 的公開介面 xhigh 審查（1 位）：等使用者同意。~~ 2026-10-25 完成，修正見「os-files-read」。
- Windows 專用的突變 os-f4、os-f5 要在 Windows 機器上執行（CI 不跑突變）。在那之前，Windows 端的檢出力只有 L1 的測試本身。
- os-files-read 的非 ACP 測試：對「改用 ACP 轉換」這類突變，只要 runner 的 ACP 不是 UTF-8（65001）就有檢出力。測試同時含日文、正體中文與 é，在 1252 與 932 上都有效。新增的 Windows 專用案例「record the process ANSI code page」會印出 `GetACP()`，並檢查它不是 65001；第一次 windows-x64 L1 從它的輸出確認。
- ~~os-files-write 開始前：先定好命名規則（審查的 finding 15）。~~ 2026-10-25 寫進 os.md 的 Contracts（Names）。第一次 windows-x64 L1 實際撞到之後，`ReadFile` 改名為 `FileReader`。os-files-write 的寫入類名稱照這條規則取。
- io-contracts 起的既有契約問題（審查的 finding 10）已在 os-files-read 處理：NativeFileSystem 用 `Os::Files::IsAbsolutePath` 拒絕「在這個平台不是絕對路徑」的 file URI 路徑。
- ~~io-contracts 開始時：契約審查用 ultracode（1 審查者＋1 對抗檢查），先問。~~ 2026-10-25 完成。
- 既有問題（不屬於本計畫的批次）：ParseUriLoose 對未知 scheme 不設定 schemeText，`pak://` 之類的 root 經 MountTable 往返會失去 scheme。等有 pack 後端時，依 io.md 的 Replacement Criteria 處理。
- ~~os-files-read 開始時：確認 Windows CI runner 的 ACP，選它表示不了的字元做測試。~~ 改成選一般 Western code page 表示不了的日文與正體中文，確認放到第一次 windows-x64 L1（見上）。
- A1～A3 完成後：通知消費端的發行任務（分支與 commit）。
