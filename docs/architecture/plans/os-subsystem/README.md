# OS 子系統：分批計畫與進度

更新：2026-10-25。Owner：Engine（新增 `engine/os`；`engine/io` 新增原生後端與路徑契約；`engine/time`、`engine/threads`、`engine/trace`、`engine/platform/sdl` 之後改呼叫 `engine/os`）。
**狀態：A1～A3 完成**（2026-10-25，四平台 L1 run 40173730672），消費端的 P1～P5 也完成；D48：其餘批次已開始，os-guard-wide、os-process-info、os-console-shutdown 完成。本文件與 [PLAN](PLAN.md)、[HANDOFF](HANDOFF.md) 於 2026-10-25 建立。

Engine 目前沒有處理 OS 差異的 owner：

- 檔案：engine/io 的 `IFileSystem` 預告由「platform 側」提供原生實作，但從沒寫過；engine/io 不含任何 OS 程式碼，代表它從未被使用，也沒有對 OS 做相容。asset 另有職責重複的 `IAssetSource`，asset 與 trace 各自用 fstream 開檔，都沒處理 Windows 的路徑長度上限與路徑編碼。
- 等待與執行緒：`Waiter.cpp`、`RoleThread.cpp` 各自在同一檔內用 `#if` 分 OS，沒有 Design Invariants 可以對照。
- 產品與工具自己判斷 OS：執行檔路徑、CPU 時間、本地時間、結束訊號、命令列、原子存檔、系統字型目錄；有幾件同一知識寫了兩次。
- 路徑字串沒有編碼契約；`ParseUri` 會把 POSIX 的絕對路徑變成相對路徑。

本計畫新增 `engine/os`，把所有 OS 差異收攏進來，讓 Engine 的其他模組、產品與工具都不必考慮 OS。先讀 [批次計畫](PLAN.md)（架構樹、部件、守衛、批次、Architecture Delta、開放決策）和 [交接](HANDOFF.md)（決策、壓力摘要、研究事實）。

## 來源

- 提出需求的消費端在 Windows 實機發現：發行包解壓到深資料夾時，shader 路徑 271 字元，超過 MAX_PATH 260，開不了。證據：主 checkout 的 `build/target/_build/test/logs/pvp-v71-windows-lan-20261016/logs/clientA/logs/client-20261016-104011.log` 第 2 行。
- 使用者決定不只修路徑，而是整理 OS 子系統（D40）。
- 本夾的文件依 D12 匿名：只寫「提出需求的消費端」，不連結消費端的文件；程式路徑與證據路徑當作資料保留。

## 已定案方針（2026-10-25，使用者）

1. **D40**：目標是整理 OS 子系統；長路徑只是 Files 部件第一個觀測到的壓力。
2. **D41**：分層是 asset → io → os → base。
   - 原生的系統相關實作放在 `engine/os`，engine/io 呼叫 engine/os。engine/os 只依賴 engine/base 與標準庫，API 只用中立型別。
   - engine/io 保留中立抽象，另外新增不含 OS 標頭的原生 `IFileSystem` 後端。
   - Waiter、RoleThread、產品與工具也照同一模式改呼叫 engine/os。
3. **D42**：asset 的讀檔改經 engine/io 的 `IFileSystem`；`NativeFileAssetSource` 暫時保留類別名與 API，當薄轉接；`IAssetSource` 的去留另行決定。
4. **D43**：Engine 字串裡的路徑現在就改成 UTF-8；產生 resolver 根目錄的呼叫端，和 asset 改走 IFileSystem 放在同一個變更。
5. **D44**：現在就修 `ParseUri` 的 file:// 規則，file:// 與原生絕對路徑只有一種對應。
6. **D45**：產品任務和架構分開。
   - 本計畫只交付結構與契約（A1～A3 與之後的部件）。
   - 長路徑的行為修正與驗收（P1～P5）由消費端的發行任務在 A1～A3 完成後進行。
7. **D46**：核准 PLAN 第 1 版。
   - 開放決策 1：A1～A3 在消費端發行前完成，其他全部排在發行之後。
   - 開放決策 14：UNC 根目錄這一輪暫不支援；io.md 定義 UNC 的 Uri 對應並寫明這個限制，P1 不寫 `\\?\UNC\` 分支。
   - 開放決策 15：A1～A3 併進消費端的發行線 PR（兩個任務，一個 PR）。
   - 同意 A1～A3 的 Delta：新 target `GYO::Os`、os.md 與 io.md 的 Design Invariants 節。
8. **D47**：`Os::Files::PathToUtf8`／`PathFromUtf8` 回傳 `Base::Result`，不合法的輸入回 InvalidEncoding，不丟例外（修訂 PLAN 第 3.2 節）。
9. **D48**：其餘批次現在開始，不等消費端發行，取代 D46 的開放決策 1。
   - 產品呼叫端由架構批次一起改，行為與日誌格式不變。
   - os-net-errors 只做等價搬移，排在消費端發行線 PR 合併後。
   - 開放決策 2、6、8～12 照建議。
   - 計時核心最後做，L2 在 lan15 之後；判定數值事先核准。
10. **D49**：os-console-shutdown 的四個決定。
    - 檔位：high，三處逐行論證，實作後 1 位 xhigh 唯讀審查。
    - error-handling.md 的平台邊界例外不延伸到 engine/os；改寫 os.md Invariant 6 的預告。
    - 連結守衛允許 Windows 上 `gyo_os` 連 shell32；定案前做突變檢查與獨立審查（AGENTS §14.4），結果交使用者看。
    - 關閉主控台時最多等 4 秒；主控台的輸出 code page 在正常結束時還原。
11. **D50**：xhigh 審查後的兩個決定。
    - POSIX 上不是 UTF-8 的命令列參數照現狀拒絕，記為事前宣告的差異。
    - shell32 連結守衛定案；`gyo_expect_links` 丟掉假值名字的既有漏洞另開後續。
12. **D51**：`os.console_close` 逾時又沒收到要求時判為「關閉事件沒送到」（Skipped）；關閉主控台的路徑在 lan15 的 Windows 實機驗證。
13. **D52**：os-files-write 的四個決定。
    - 檔位：high，Windows 寫入語意逐行論證，實作後 1 位 xhigh 審查。
    - 換行：Windows 維持 CRLF（`FileOpenMode::Text`，engine/os 轉換）；換行的 Data Contract 決定延到消費端發行後。
    - `ReplaceFile`、`SamePathComponent` 移到 os-tool-files；Windows 長路徑 log 的驗收移到 os-trace-writers。
    - 消費端突變目錄的重新定位由本計畫做，先通知消費端。
14. **D53**：os-files-write 的路徑差異（產品改走 engine/io 的 file:// 規則後的 5 項）補進事前宣告；程式不改。
15. **D54**：新開 io-native-paths，讓 file:// 規則對齊 OS（`..` 照原樣交給 OS、支援 UNC、POSIX 的 `//` 不拒絕、非 UTF-8 目前目錄照 D43）；取消 D46、修改 D44 與 io.md Invariant 3。估時 12～19 h；high＋局部 xhigh＋ultracode 契約審查；判定的定案在突變檢查與獨立審查之後。
16. **D55**：io-native-paths 的定案（突變檢查與 ultracode 契約審查之後）。
    - UriTests 與 AssetPathResolverTests 的判定改變照 AGENTS §14.4 的表定案，包含只有主機的 UNC 照樣對應、由 OS 決定，以及 POSIX 上 `file://host/x` 讀本機的 `/host/x`。
    - io.md 的 Invariant 3 與 Replacement Criteria 3 照草稿定案。
    - `allowAbsolutePath` 的字面站點（點段消去、UNC 的 `//` 收合）記為未結，之後處理。
17. **D56**：os-tool-files 開工前的四個決定。
    - 檔位：high；Windows 的 `RenameReplacing` 與 `ResolvedPath` 逐行論證，實作後 1 位 xhigh 唯讀審查。估時 9～13 h。
    - 新 API：engine/os 的 `RenameReplacing`（原名 ReplaceFile 撞 Win32 名）、`RemoveFile`、`SamePathComponent`、`ResolvedPath`、`FontDirectories`；NativeFileSystem 的 `Move`（取代既有）、`Remove`（只刪檔）。
    - 原子寫入做成 engine/io 的 helper（唯一實作），工具是第一個消費端，IP-5 之後沿用。
    - ★ 事前宣告：Windows 的路徑成分比較改用 `CompareStringOrdinal`；macOS 維持區分大小寫（已知差異）；字型改用不分 OS 的候選清單，在每個字型目錄找。
18. **D57**：os-tool-files 在 xhigh 審查後發現、事前沒有宣告的行為差異。
    - 接受並補宣告：POSIX 的 `AbsolutePath("")` 回 `""`；POSIX 讀目錄、FIFO 回 ReadFailed；Windows 名稱不合法的路徑在 probe 時就回錯誤；錯誤訊息的文字；字型選擇的邊界情況（Linux 的同目錄他平台字型、日文合併；Windows 不讀 `WINDIR`）。
    - 沒有磁碟代號的磁碟區，`ResolvedPath` 維持回錯誤（MSVC 回 `\\?\GLOBALROOT\...`），記為已知差異。
19. **D58**：os-trace-writers 開工前的四個決定。
    - 檔位：high＋1 位 xhigh 審查。
    - trace.md Invariant 7（「exactly GYO::Threads and GYO::Os」）與 os.md Invariant 2（engine/trace 直接經 engine/os 寫檔）的新條文：同意開工，定案前做突變檢查與獨立審查（AGENTS §14.4）。
    - ★ 事前宣告：Windows 的附加改成只能附加的 handle；一批之內在 64 KiB 或批尾分段寫出；錯誤 detail 的路徑是 UTF-8；Windows 長路徑可寫。
    - Windows 長路徑案例的 Red：先推只含新測試的 commit 跑一次 L1（推送前照常先問）。
20. **D59**：本計畫的 commit 不另開 PR，統一放進 #96（發行線）管理；合併照 A1～A3 的做法由消費端進行。
21. **D60**：os-trace-writers 的定案。trace.md Invariant 7（exactly GYO::Threads and GYO::Os，Os 為 PRIVATE 並有設定期檢查）與 os.md Invariant 2（engine/trace 直接經 engine/os 寫檔）照草稿定案；審查發現的差異（非 UTF-8 路徑改為 OpenFailed、錯誤 detail 的文字、`flushEachLine` 的長行一次寫出）接受並補宣告。
22. **D61**：time-threads-invariants 的定案。time.md 與 threads.md 的 Design Invariants（含 Replacement Criteria）照草稿同意；這一批補幾個便宜的守衛（連結檢查的兩個設定期突變、後端名稱的測試、Linux 名稱截斷的檢查），其餘無守衛的項目記為未結。
23. **D62**（消費端 D208、D209 的連帶）：lan15 延期到本計畫做到 os-net-errors 之前為止。那之前的 4 批（os-path-utf8-sweep、os-wake-record、os-threads-backend、os-wait-backend）先做完，然後在 Windows 機器上一次做完 Windows 專用突變、`-s` 確認、實機驗收與計時的 L2，再跑 lan15；os-net-errors 與 os-close 排在 #96 合併後。
24. **D63**：os-path-utf8-sweep 開工前的四個決定。檔位 high＋1 位 xhigh 審查；新增 `Os::Files::PathForMessage`（訊息與 log 用的 UTF-8，保留平台的分隔字元，永不失敗）；拿掉公開的 `Sdl::PathFromUtf8`（`BaseDirectory` 內部照舊），停用中的產品只記進 inactive_products.md，主機 shader 工具的 12 處也一起改；Windows 的 Red 先推測試跑一次 L1。估時 10～15 h。
25. **D64**：Windows 程序自己把 ANSI code page 定為 UTF-8（manifest 的 `activeCodePage`），不由執行環境決定；engine/os 的寬字元 API 維持主線，manifest 是第二道防線。使用者的話：「程序不應該由環境參數來決定。」細化成批次草案 os-utf8-codepage（PLAN 第 3.10、7.18、9.2 節），條文與做法的開放決策 16～23 未決；D211 之下目前只改計畫。

## 架構樹（目標狀態，詳見 PLAN 第 2 節）

```text
engine/base
engine/os  ［新，GYO::Os：OS 差異的唯一 owner］
├── OsError                    os-skeleton
├── Files（讀取／寫入）         os-files-read／os-files-write；Windows 長路徑＝消費端 P1
├── System（系統字型目錄）     os-tool-files
├── Process                    os-process-info
├── Console（命令列、輸出、結束要求）  os-console-shutdown
├── Thread                     os-threads-backend
├── Wait                       os-wait-backend
└── Net（條件式）              os-net-errors
engine/io（中立抽象＋NativeFileSystem 後端，不含 OS 程式碼）   io-contracts、os-files-read
engine/asset → io；engine/time、threads、trace → os；engine/platform/sdl 不動（PathFromUtf8 移到 os）
```

## 進度

| 批次 | 建議檔位 | 狀態 | 交付邊界 |
|---|---|---|---|
| os-plan-docs 計畫 | medium | 完成（2026-10-25 核准，D46；[dev_log](../../../dev_logs/2026_10_25_engine_os_plan_docs.zh-Hant.md)） | 本夾三份文件與 dev_log |
| os-skeleton（A1） | high | 完成（2026-10-25；四平台 L1 run 40173730672；[dev_log](../../../dev_logs/2026_10_25_engine_os_skeleton.zh-Hant.md)） | engine/os、GYO::Os、OsError、os.md、守衛（Engine 範圍）、突變 runner 擴充 |
| io-contracts（A2） | high；字串規則局部 xhigh；契約審查 ultracode（先問） | 完成（2026-10-25；四平台 L1 run 40173730672；[dev_log](../../../dev_logs/2026_10_25_engine_os_io_contracts.zh-Hant.md)） | io.md、UTF-8 路徑、file:// 對應、ParseUri／ToString／MountTable 修正、IoError detail |
| os-files-read（A3） | high；UTF-8↔UTF-16 與錯誤對應局部 xhigh；公開介面 1 位 xhigh 審查（先問） | 完成（2026-10-25；xhigh 審查已修正，D47；四平台 L1 run 40173730672；[dev_log](../../../dev_logs/2026_10_25_engine_os_files_read.zh-Hant.md)） | engine/os 的唯讀檔案原語、engine/io 的唯讀 NativeFileSystem |
| （消費端 P1～P5） | 由消費端決定 | 完成（消費端的發行線，2026-10-25） | 長路徑行為、錯誤記錄、asset 接線、UTF-8 呼叫端、驗收（不屬於本計畫） |
| os-guard-wide（A1b） | medium；規則調整 high | 完成（2026-10-25；四平台 L1 run 40194493434；[dev_log](../../../dev_logs/2026_10_25_engine_os_guard_wide.zh-Hant.md)） | 守衛擴到產品、工具、產品測試、驗收程式 |
| os-process-info | high | 完成（2026-10-25；四平台 L1 run 40194493434；[dev_log](../../../dev_logs/2026_10_25_engine_os_process_info.zh-Hant.md)） | 執行檔路徑、CPU 時間、本地時間 |
| os-console-shutdown | xhigh（D49：high＋三處論證＋1 位 xhigh 審查） | 完成（2026-10-26；四平台 L1 run 40205158822，關閉主控台在 CI 為 Skipped、留給 lan15；[dev_log](../../../dev_logs/2026_10_25_engine_os_console_shutdown.zh-Hant.md)） | UTF-8 命令列、主控台輸出、結束要求（★ Windows 行為變更） |
| os-files-write | high＋1 位 xhigh 審查（D52） | 完成（2026-10-26；四平台 L1 run 40213916104；[dev_log](../../../dev_logs/2026_10_26_engine_os_files_write.zh-Hant.md)） | 寫入原語、消費端的 arena 讀取與 trace 寫檔（ReplaceFile 等移到 os-tool-files，D52） |
| io-native-paths（D54） | high；Uri 規則與 Windows UNC 局部 xhigh；ultracode 契約審查 | 完成（2026-10-26；判定 D55；四平台 L1 run 40295007179；[dev_log](../../../dev_logs/2026_10_26_engine_os_io_native_paths.zh-Hant.md)） | file:// 對齊 OS：`..` 交給 OS、UNC、POSIX 的 `//`、非 UTF-8 目前目錄的契約 |
| os-tool-files | high＋1 位 xhigh 審查（D56、D57） | 完成（2026-10-26；四平台 L1 run 40324361964；[dev_log](../../../dev_logs/2026_10_26_engine_os_tool_files.zh-Hant.md)） | 編輯器的原子存檔、路徑比較、系統字型目錄 |
| os-trace-writers | high＋1 位 xhigh 審查（D58、D60） | 完成（2026-10-26；Red：run 40331764169；Green：四平台 L1 run 40339587478；[dev_log](../../../dev_logs/2026_10_26_engine_os_trace_writers.zh-Hant.md)） | TraceSink 經 engine/os（開放決策 2） |
| os-path-utf8-sweep | high＋1 位 xhigh 審查（D63） | 完成（2026-10-27；Red：run 40431320545；Green：四平台 L1 run 40433158383；[dev_log](../../../dev_logs/2026_10_27_engine_os_path_utf8_sweep.zh-Hant.md)） | 剩下的窄字串路徑、PathFromUtf8 移到 engine/os |
| time-threads-invariants | medium（D61） | 完成（2026-10-27；只改文件與測試；[dev_log](../../../dev_logs/2026_10_27_engine_os_time_threads_invariants.zh-Hant.md)） | time.md、threads.md 的 Design Invariants |
| os-wake-record | medium | 未開始 | 晚醒量測程式合回、四平台基準 |
| os-threads-backend | high；Start 順序局部 xhigh | 未開始 | RoleThread 的 OS 呼叫搬進 engine/os |
| os-wait-backend | xhigh；1 位 xhigh 審查（先問） | 未開始 | Waiter 的 OS 後端搬進 engine/os，ABAB L2 |
| os-net-errors | high | 條件式 | socket 錯誤碼的中立分類（開放決策 7） |
| os-close | medium | 未開始 | allowlist 清空、文件同步、完整突變 |
| os-utf8-codepage（提案，D64） | high＋1 位 xhigh 審查（先問） | 計畫草案，未核准（順序見開放決策 16） | 每個 Windows 執行檔以 manifest 把 code page 定為 UTF-8；R6 守衛；ACP 測試與突變重新設計 |

```text
os-plan-docs ─┬─→ os-skeleton ─┐
              ├─→ io-contracts ┴─→ os-files-read ─→［消費端 P1～P5］─→ os-files-write ─→ os-tool-files ─┐
              │                                                         ├─→ os-trace-writers           │
              │   os-skeleton ─→ os-guard-wide ─→ os-process-info ─→ os-console-shutdown ──────────────┴─→ os-path-utf8-sweep
              ├─→ time-threads-invariants ─┐
              └─→ os-wake-record ──────────┴─→ os-threads-backend ─→ os-wait-backend
                                                                     （全部完成）─→ os-close
```

- 關鍵路徑（消費端的修正之前）：os-skeleton 與 io-contracts 並行，接 os-files-read，約 20～29 h 工時。
- 整條線：約 100～150 h。
- 依 D48：其餘批次現在開始（取代 D46 的「發行後」）；A1～A3 已併進消費端的發行線 PR。

## 尚待回答的開放決策

PLAN 第 11 節的 1、14、15 已由 D46 決定。其餘（2、4、6～13）在對應批次開始前回答。16～26（2026-10-27 加入）：os-utf8-codepage 的順序、套用方式、owner、等價突變、Proton／Wine、longPathAware、1903 以前的 Windows、MinGW 的嵌入，以及 D201 找到的測試缺漏、強制結束時的 0 B log、`os.console_close` 在 Windows Terminal 下的 Skipped。

## 執行規則

- 每次只執行使用者指定的批次；commit 與 PR 用日語。
- 每批開始、里程碑、停止時，更新本表、[HANDOFF](HANDOFF.md) 與 dev_log（`docs/dev_logs/YYYY_MM_DD_engine_os_<批次>.zh-Hant.md`），然後停止。
- 主對話檔位由使用者決定；ultracode 與高於主對話的檔位，在批次開始時說明並徵求同意（D8）。
- Engine 的驗收只驗 Engine 的目標，不以特定產品的存在為前提（AGENTS §7）；消費端的檢查屬於消費端的批次。
- 計時相關的 L1 不設比結構條件更嚴的時間上界；精度只在 L2 量。
- 每批同步更新未啟用產品的遷移清單（[foundation-followups/inactive_products.md](../foundation-followups/inactive_products.md)，加「OS」節）。
- 每批只重跑相關突變，PR 線合併前在背景完整跑一次（消費端 D184 的作法）。
- 出現非預期回歸、範圍擴大，或需要新的第三方依賴、SDL 邊、往上的依賴邊時，停下回報並重新規劃。
