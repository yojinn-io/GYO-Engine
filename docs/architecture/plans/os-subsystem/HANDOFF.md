# OS 子系統：交接

更新：2026-10-25。**狀態：計畫已核准**（D46）；os-skeleton 本機 L1 完成（四平台 L1 待推送）；io-contracts 進行中。2026-10-25 由架構任務在規劃批次中建立。
本文件是持續記錄器：每批開始、里程碑、停止時，與工作在同一變更中更新。停下或回報前，先更新「現況」。依 D12 匿名。

## 現況

- 2026-10-25：使用者核准 PLAN 第 1 版（D46），說「開始 os-skeleton 與 io-contracts」。
- os-skeleton：GYO::Os、OsError、os.md、守衛（Engine 範圍）、突變 runner 擴充完成，本機 core preset 的 L1 全過、突變 11／11 killed。四平台 L1 要等分支推送（見未結事項）。
- io-contracts：進行中。
- 消費端的發行任務已收到分工、交接內容與 D46（A1～A3 併進它的發行線 PR），等 A1～A3 完成。

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

## 未結事項

- ~~使用者核准 PLAN，回答開放決策 1、14、15，並同意 A1～A3 的 Delta。~~ 2026-10-25 完成（D46）。
- A1～A3 併進消費端的發行線 PR（D46）。消費端的發行任務 2026-10-25 回覆的做法（暫定，開 PR 時再和使用者確認，消費端 D198）：
  - A1～A3 的分支從 master 切，不帶消費端其他線的改動。本計畫的分支 `claude/trusting-joliot-f13b78` 是從 master `23fab75` 切出的。
  - 消費端的發行線在它目前那條線的 PR 合併後，從 master 切出 `claude/pvp-v71-release`。
  - A1～A3 完成後，由消費端以 `git merge --no-ff` 併進發行線（不 rebase、不 squash，保留 commit 與訊息），再在上面做 P1～P5；整個 PR 照 repo 規則以 `gh pr merge --merge` 合進 master。
  - A1～A3 若在發行線開好之前完成，就等它開好再併。P4 在產品檔案的那一行，以及和消費端其他線的衝突，由消費端在發行線處理。
  - 完成時通知消費端：分支名稱與最後的 commit。
- ~~os-skeleton 開始時：用空 allowlist 實跑守衛，產生 Engine 的基準，對照盤點。~~ 2026-10-25 完成，和盤點一致。
- os-skeleton 的四平台 L1：分支還沒推送。D46 決定 A1～A3 併進消費端的發行線 PR，所以要先推送本分支，再手動觸發 L1（推送前徵求使用者同意）。
- io-contracts 開始時：契約審查用 ultracode（1 審查者＋1 對抗檢查），先問。
- os-files-read 開始時：確認 Windows CI runner 的 ACP，選它表示不了的字元做測試。
- A1～A3 完成後：通知消費端的發行任務（分支與 commit）。
