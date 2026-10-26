# OS 子系統：交接

更新：2026-10-26。**狀態：A1～A3 完成**；D48：其餘批次已開始（os-guard-wide、os-process-info、os-console-shutdown 完成；os-files-write 完成（L1 run 40213916104）；io-native-paths 完成（L1 run 40295007179）；os-tool-files 完成（L1 run 40324361964）），本分支建立在消費端的發行線之上。2026-10-25 由架構任務在規劃批次中建立。
本文件是持續記錄器：每批開始、里程碑、停止時，與工作在同一變更中更新。停下或回報前，先更新「現況」。依 D12 匿名。

## 現況

- 2026-10-25：使用者核准 PLAN 第 1 版（D46），說「開始 os-skeleton 與 io-contracts」。
- os-skeleton：GYO::Os、OsError、os.md、守衛（Engine 範圍）、突變 runner 擴充完成，本機 core preset 的 L1 全過、突變 11／11 killed。四平台 L1 要等分支推送（見未結事項）。
- io-contracts：file:// 對應、ParseUri／ParseUriLoose／ToString 修正、io.md、MountTable 不越出根目錄完成；Red→Green、契約審查（ultracode）的發現已修正、本機 L1 全過、突變 11／11 killed。
- os-files-read：使用者說「先不推送，繼續做 os-files-read，A1～A3 都完成後再一起跑 L1」。engine/os 的唯讀檔案原語（POSIX、Windows）與 engine/io 的 NativeFileSystem 完成，本機 L1 全過，突變在本機 7／7 killed，另有 2 個 Windows 專用的突變要到 Windows 才能跑。
- os-files-read 的 xhigh 審查（使用者同意：審查公開介面與 Windows 實作，修完後再推送）：沒有 blocker。1 個 major，即 `PathToUtf8`／`PathFromUtf8` 在 Windows 會丟例外 → 使用者決定改成回傳 Result（D47）。其餘 minor、nit 已修，測試缺口已補。本機突變 34／34 killed（2 個 Windows 專用的跳過）。
- 四平台 L1：第一次 windows-x64 失敗（測試檔裡 `ReadFile` 撞名）→ 類別改名為 `FileReader`。第二次全部通過：[run 40173730672](https://github.com/yojinn-io/GYO-Engine/actions/runs/40173730672)，`96fd2fc`。
- 下一步：A1～A3 完成，已通知消費端的發行任務（分支與 commit）。本計畫接下來的批次，依 D46 排在消費端發行之後。
- 2026-10-25：（之後由 D48 取代）使用者一度考慮把剩下的 13 批也提前到發行前，之後確認維持 D46：「這13批還是照原計劃放到v7.1發佈後吧，工程量有些大，現在先讓v7.1做完他的工作。完成後我回來讓你開始」。本計畫在消費端發行之後、使用者說開始時再繼續，從 os-guard-wide 開始。
- 2026-10-25：D48，其餘批次現在開始。本分支併入消費端的發行線（`c4f9430`；含 PR #95、A1～A3、P1～P5），合併後基準 ctest 30／30、守衛通過。順序：先做不碰發行線檔案的 os-guard-wide、os-process-info、time-threads-invariants、os-wake-record，os-net-errors 排在發行線 PR 合併後，計時核心最後。
- 消費端的發行任務已收到分工、交接內容與 D46（A1～A3 併進它的發行線 PR），等 A1～A3 完成（L1 通過後通知分支與 commit）。
- 2026-10-25：os-guard-wide 本機完成（本機 commit，未推送）。守衛擴到 apps、tools、build/acceptance、tests，產品、工具與驗收程式各有自己的 allowlist；設定期檢查與標頭衛生測試加入。本機 ctest 31／31、守衛通過、相關突變 16／16 killed（Windows 專用 1 個跳過）。四平台 L1 等推送（要使用者同意）。下一批是 os-process-info。
- 2026-10-25：使用者說「先不推送，繼續做 os-process-info，做完再和 os-guard-wide 一起跑四平台 L1」。os-process-info 本機完成（本機 commit，未推送）：engine/os 新增 Process，消費端的執行檔路徑、CPU 時間、本地時間改呼叫它。本機 ctest 31／31，產品組態的相關測試 12／12，突變 7／7 killed（Windows 專用 1 個跳過）。下一步：推送並跑四平台 L1（要使用者同意），之後是 os-console-shutdown。
- 2026-10-25：使用者說「推送本分支，手動觸發四平台 L1，一起驗 os-guard-wide 和 os-process-info」。第一次 run 40192938131 只有 macos-arm64 的 toolchain 列失敗（本批新加的 CPU 測試前提錯，見「os-process-info」），修正 `a9219fa` 後第二次 [run 40194493434](https://github.com/yojinn-io/GYO-Engine/actions/runs/40194493434) 全部成功。兩批完成。下一批是 os-console-shutdown（xhigh，★ Windows 行為變更）。
- 2026-10-26：io-native-paths（D54）完成，判定已定案（D55）。四平台 L1 [run 40295007179](https://github.com/yojinn-io/GYO-Engine/actions/runs/40295007179) 全部成功，Windows 的 UNC 探針兩項 Passed。Windows／Linux 專用的突變待對應平台。
- 2026-10-26：os-tool-files 完成（D56、D57），四平台 L1 [run 40324361964](https://github.com/yojinn-io/GYO-Engine/actions/runs/40324361964) 全部成功。Windows 專用的突變與 `-s` 確認留給 lan15。
- 2026-10-26：os-trace-writers 開始（D58），基準 `ca434952`，本機進行中。
- 2026-10-26：D59，本計畫的 commit 統一放進 #96。已請消費端把本分支併到 `ca434952`（不含 Red 測試 `a75e4028`），等它回覆發行線上的 commit。

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
| 2026-10-25 | D48：OS 子系統其餘的批次現在開始，不等消費端發行；取代 D46 的開放決策 1。<br>預設決定：<br>產品與工具的呼叫端由架構批次一起改（可觀察行為不變，日誌格式逐 byte 不變）；<br>開放決策 7 採 (b)，os-net-errors 只做等價搬移，排在消費端的發行線 PR 合併後；<br>開放決策 2、6、8、9、10、11、12 照 PLAN 第 11 節的建議；<br>計時核心（os-threads-backend、os-wait-backend）排在最後，L2 實機量測在消費端的 lan15 之後安排，判定數值（開放決策 13）在看結果之前先交使用者核准並凍結。<br>分支併入消費端的發行線，動到發行線 PR 差異裡的檔案前，先通知消費端 | 使用者在消費端的發行任務會話說：「我覺得，現在開始  OS 統一子系統的工作比較好。」（消費端 D205）；回到本會話確認時，對「要我現在開始 OS 子系統剩下的批次嗎？」選：「開始，照預設決定」 |
| 2026-10-25 | D49：os-console-shutdown 開工前的四個決定。<br>檔位：主會話照現在的設定（high），signal handler、主控台 handler、結束旗標三處逐行論證，實作後 1 位 xhigh 唯讀審查者看這三處。<br>error-handling.md 的「平台邊界的例外」不延伸到 engine/os：handler 只寫 lock-free atomic、宣告 noexcept；改寫 os.md Invariant 6 的預告（Design Invariants 節的變更）。<br>連結守衛允許 Windows 上 `gyo_os` 連 shell32（`CommandLineToArgvW`）：放寬面只有這一個名字；定案前做突變檢查，並請 1 位審查者用實際資料核對 AGENTS §14.4 的 1～5，結果交使用者看。<br>開放決策 9 的收尾上限：關閉主控台時 handler 最多等 4 秒（OS 約 5 秒強制結束）；主控台的輸出 code page 在正常結束時還原 | 使用者：「開始 os-console-shutdown。先說明檔位、★ Windows 行為變更的內容，以及需要我同意的地方（error-handling.md 的延伸、連結守衛加 shell32）。」；對四個問題選：「high＋三處論證＋xhigh 審查」「不延伸，改寫 Invariant 6」「同意，先做突變與獨立審查」「等 4 秒、正常結束還原」 |
| 2026-10-25 | D50：xhigh 審查後的兩個決定。<br>POSIX 上不是 UTF-8 的命令列參數照現狀拒絕（`CommandLineUtf8` 守住「輸出一定是 UTF-8」），記為 Invariant 9 的事前宣告差異。<br>shell32 連結守衛定案（AGENTS §14.4 的 1～5 依審查者的實測修正：收緊面其實有一個，即 Windows 上「正好只有 GYO::Base」改成失敗）；審查者發現 `gyo_expect_links` 會丟掉名字是 CMake 假值的項目，這是既有漏洞，另開後續 | 使用者對「POSIX 上不是 UTF-8 的命令列參數要怎麼處理？」選「照現狀拒絕，宣告為差異」；對「shell32 連結守衛的變更要定案嗎？」選「定案，假值漏洞另開後續」 |
| 2026-10-26 | D51：`os.console_close` 在關閉模式下，探測程式活到逾時又沒收到要求，就判為「關閉事件沒送到」並回 77（CTest 顯示 Skipped）。依據：主控台真的關閉時，handler 會記錄要求，或 OS 最晚在 handler 返回時結束程序，所以活到逾時只可能是事件沒送到。關閉主控台的路徑改在 lan15 的 Windows 實機驗證（L3 與 Windows 專用突變 os-c4）。這是這個測試第 2 次改判定方法 | 使用者：「照 (b)：關閉模式下，活到逾時又沒收到要求就判為「關閉事件沒送到」並回 77（skip）；推送後再跑一次 L1。關閉路徑在 lan15 實機驗證。」 |
| 2026-10-26 | D52：os-files-write 開工前的四個決定。<br>檔位：high，Windows 寫入語意（截斷／附加、共用模式、換行、長路徑）逐行論證，實作後 1 位 xhigh 唯讀審查。<br>換行（os.md Replacement Criteria 5 觸發，交使用者決定）：Windows 維持 CRLF，呼叫端用 engine/io 既有的 `FileOpenMode::Text`，engine/os 的 Windows 寫入把 `\n` 換成 `\r\n`；把換行當成 Data Contract 的決定延到消費端發行之後。<br>範圍：`ReplaceFile`、`SamePathComponent` 移到 os-tool-files（消費端是編輯器）；「Windows 長路徑的 log」驗收移到 os-trace-writers（log 檔的寫入走 TraceSink）；這批驗 arena 的長路徑讀取。<br>消費端突變目錄裡因本批過期的約 32 條，由本計畫重新定位（只對應到新程式碼，不改意圖；expect 要變時逐條寫理由），動之前先通知消費端，以它最新的目錄為基礎 | 使用者對四個問題選：「high＋1 位 xhigh 審查」「維持 CRLF，由 Text 模式轉換」「照建議移到後面的批次」「這裡做，先通知 v7.1」 |
| 2026-10-26 | D53：xhigh 審查發現的路徑差異（產品與 AssetWatcher 改走 engine/io 後，依 io.md 的 file:// 規則：POSIX 的 `..` 依字面消去、`//` 開頭被當成 UNC、Windows 的 UNC 拒絕、目前目錄不是 UTF-8 時相對路徑失敗、AssetWatcher 的字串在 Windows 上改以 UTF-8 解讀）補進 os-files-write 的事前宣告差異，程式不改，並告知消費端。之後另開一批「io 原生路徑契約」讓 file:// 規則對齊 OS（D54 起） | 使用者對選項先回答「[No preference]」，之後明確決定：「os-files-write 照「補宣告」收尾：把審查發現的 5 項路徑差異寫進本批的事前宣告（Invariant 9），程式不改。告知 v7.1。合併 ed63c24 後提交。推送前照常先問我。」 |
| 2026-10-26 | D54：新開一批 io-native-paths（io 原生路徑契約），讓 engine/io 的 file:// 規則對齊 OS。<br>範圍：原生路徑轉成 file:// 時不再依字面消去 `..`（開檔照 OS，字面正規化只用在比較與顯示；改 io.md Invariant 3 與 D44）；取消 D46，支援 UNC（`\\server\share` ↔ `file://server/share`，Windows 長路徑用 `\\?\UNC\`）；POSIX 以 `//` 開頭的路徑不再當成 UNC 拒絕；非 UTF-8 的目前目錄照 D43 維持並寫成契約；測試含 POSIX 的 symlink 加 `..`、Windows 的 UNC 往返與長路徑（`\\localhost\C$`，CI 做不到就 Skipped）、重寫 UriTests 的不動點測試。<br>設計：原生 `//X/Y` 與 `file://X/Y` 的對應在各平台相同（engine/io 不含 OS 程式碼）；主機為 localhost 的 UNC 寫成 `file:////localhost/...`。<br>估時 12～19 h。檔位：high，Uri 字串規則與 Windows UNC 長路徑局部 xhigh，契約審查用 ultracode（1 審查者＋1 對抗檢查），兼做 AGENTS §14.4 的獨立核對。<br>§14.4：開工前列出的放寬面與收緊面已得同意；新判定寫好後跑突變檢查與獨立審查，結果與 Invariant 3 的新條文再交使用者定案 | 使用者：「接著另開一批「io 原生路徑契約」，讓 engine/io 的 file:// 規則對齊作業系統。路徑這個東西早改早好，否則又得面對一堆的個性化定製。」（範圍見左欄）；對三個問題選：「同意開工，定案前做突變與審查」「high＋局部 xhigh＋ultracode 契約審查」「現在推送並跑 L1」 |
| 2026-10-26 | D55：io-native-paths 的定案。<br>UriTests 與 AssetPathResolverTests 的判定改變照 AGENTS §14.4 的表定案（突變與 ultracode 契約審查之後；審查的 F1、F2 修正後，開工前表上的 L4 撤回、回到舊判定）。只有主機的 UNC 照樣對應、由 OS 決定；POSIX 上 `file://host/x` 讀本機的 `/host/x`。<br>io.md Invariant 3 與 Replacement Criteria 3 照草稿定案。<br>`allowAbsolutePath` 的字面站點記為未結 | 使用者對三個問題選：「照此定案」「照草稿定案」「記為未結，之後處理」 |
| 2026-10-26 | D56：os-tool-files 開工前的四個決定。<br>檔位：high，Windows 的 `RenameReplacing` 與 `ResolvedPath` 逐行論證，實作後 1 位 xhigh 唯讀審查；估時 9～13 h（原 6～9 h：多了 `ResolvedPath` 與 engine/io 的原子寫入）。<br>新 API：engine/os 的 `RenameReplacing`（PLAN 原名 ReplaceFile 是 Win32 函式名，os.md 的 Names 不允許）、`RemoveFile`、`SamePathComponent`、`ResolvedPath`（取代 `weakly_canonical`，io.md Replacement Criteria 3 的解析 helper）、`FontDirectories`；NativeFileSystem 開放 `Move`（取代既有）與 `Remove`（只刪檔）。<br>原子寫入做成 engine/io 的 helper，是唯一的實作；工具是第一個消費端，IP-5 之後沿用（輸入與呈現計畫的 HANDOFF 記一筆）。<br>★ 事前宣告（os.md Invariant 9）：Windows 的路徑成分比較改用 `CompareStringOrdinal`（OS 的大寫表；原為受 locale 影響的 `towlower`）；macOS 維持逐位元組比較，記為已知差異；字型改用不分 OS 的候選清單，在 `FontDirectories` 的每個目錄找（系統目錄優先，多找使用者字型目錄），只有現在選中的字型不存在時結果才會不同 | 使用者對四個問題選：「high＋1 位 xhigh 審查」「全部同意」「engine/io，唯一實作」「照建議」 |
| 2026-10-26 | D57：os-tool-files 的 xhigh 審查發現、事前沒有宣告的行為差異。<br>接受並補宣告（os.md Invariant 9）：POSIX 的 `AbsolutePath("")` 回 `""`（原為目前目錄加 `/`；Windows 原本就是 `""`）；POSIX 讀目錄、FIFO 回 ReadFailed（原為讀成 `""`、FIFO 會卡住；Windows 原本就失敗）；Windows 名稱不合法的路徑在 probe 時就回 ProbeFailed（原為之後回 WriteFailed）；錯誤訊息的文字（原因改放 detail）；字型選擇的邊界情況（Linux 同目錄裡他平台的字型會先選、會做日文合併；Windows 改用 `GetSystemWindowsDirectoryW`，不讀 `WINDIR`）。<br>沒有磁碟代號的磁碟區：`ResolvedPath` 維持回錯誤，不照 MSVC 回 `\\?\GLOBALROOT\...`（Invariant 5），記為已知差異；編輯器在那種情況退回字面比較 | 使用者對兩個問題選：「全部接受並補宣告」「維持回錯誤，記為已知差異」 |
| 2026-10-26 | D58：os-trace-writers 開工前的四個決定。<br>檔位：high＋1 位 xhigh 審查（使用者選的，不是建議的「只用 high」）。<br>trace.md Invariant 7 改成「exactly GYO::Threads and GYO::Os」、os.md Invariant 2 補上「engine/trace 位在 GYO::Engine 之下，直接經 engine/os 寫檔（開放決策 2）」：同意開工，定案前做突變檢查與獨立審查，再交使用者定案（AGENTS §14.4，比照 D54）。<br>★ 事前宣告（os.md Invariant 9）：Windows 的附加改用只能附加的 handle（對其他附加者原子；原為 CRT 每次寫前 seek 到檔尾）；一批之內在 64 KiB 或批尾分段寫出（位元組不變）；錯誤 detail 的路徑是 UTF-8；Windows 長路徑可寫。<br>Windows 長路徑 trace 案例的 Red：先推只含新測試的 commit 跑一次 L1（推送前照常先問），再推修正 | 使用者對四個問題選：「high＋1 位 xhigh 審查」「同意開工，定案前做突變與審查」「照建議」「先推測試跑一次 L1」 |
| 2026-10-26 | D59：本計畫的 commit 不另開 PR，統一放進 #96（消費端的發行線）管理。使用者先說「你開一個PR吧。現在掛著一堆的commit」，開了草稿 #97（base 是發行線）後改成這個決定；#97 已關閉。合併照 A1～A3 的做法，由消費端以 `git merge --no-ff` 併進發行線：先到 `ca434952`（os-tool-files 完成），os-trace-writers 的 Red 測試 `a75e4028` 等修正後再併 | 使用者：「那算了，統一給#96管理好了。」 |
| 2026-10-26 | D60：os-trace-writers 的定案。<br>trace.md Invariant 7（`GYO::Trace` links exactly `GYO::Threads` and `GYO::Os`，Os 為 PRIVATE，連結檢查另驗 Os 不出現在 INTERFACE）與 os.md Invariant 2（engine/trace 在 GYO::Engine 之下，直接經 engine/os 寫檔）照草稿定案；AGENTS §14.4 的 1～5 經獨立核對（1、2、4 點不完整的部分已補檢查或記為未結）。<br>xhigh 審查發現、事前沒有宣告的差異接受並補宣告（trace.md）：非 UTF-8 的 trace 路徑改為 OpenFailed；錯誤 detail 的文字（UTF-8 路徑加 engine/os 原因）；`flushEachLine` 的長行一次寫出 | 使用者對兩個問題選：「照草稿定案」「全部接受並補宣告」 |

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
- 2026-10-25：第二次四平台 L1：[run 40173730672](https://github.com/yojinn-io/GYO-Engine/actions/runs/40173730672)，`96fd2fc`。
  - 全部成功：windows-x64、macos-x64、macos-arm64、linux-x64 的 L1，quick acceptance（toolchain、消費端）與 CI gate。
  - windows-x64 的 CTest 87／87，其中 gyo_os_tests、os.boundary_guard、os.boundary_guard_selftest、engine_tests 都通過。
  - 只記錄的資料沒有取得：CTest 只在失敗時才印出 doctest 的 MESSAGE，所以 Windows runner 的 `GetACP()` 實際值，以及超過 260 字元路徑的原生錯誤碼，都不在記錄裡。
  - 能確定的是 `CHECK(GetACP() != CP_UTF8)` 通過了，所以非 ACP 測試對「改用 ACP 轉換」這類缺陷有檢出力。
  - 要看實際值，可以在 Windows 上直接執行 `gyo_os_tests -s`，P1 開始時也可以順便看。
- 本批完成（A1～A3 都完成）。

### os-guard-wide

- 2026-10-25：開始（medium；規則調整 high），基準是本分支 `1f28598`（併入消費端發行線 `c4f9430` 之後）。依 D48 不等消費端發行。消費端已收到通知：本批動到守衛腳本、自我測試、`tests/common/os/CMakeLists.txt`、突變目錄（只追加）、os.md 與新的 allowlist 檔。
- 實作：
  - 守衛的根目錄改成 engine、apps、tools、build/acceptance、tests；R4 的 runtime 根目錄改成 engine、apps、tools。不寫產品名稱。其餘規則不變。
  - 各 owner 的 allowlist：`apps/object_fps_pvp`（15 條）、`tools/ui_editor`（6 條）、`build/acceptance/object_fps_pvp`（1 條，誤報，見下）。每條寫明理由與移除它的批次。
  - `OsHeaderHygieneTests`（CTest `os.header_hygiene`，獨立執行檔 `gyo_os_header_tests`）：在 include doctest 之前，記錄 engine/os 公開標頭之後是否定義了 `_WINDOWS_`、`_INC_WINDOWS`、`_WINSOCKAPI_`、`_WINSOCK2API_`、`min`、`max`。另有一個案例檢查 engine/os/include 的每個標頭都被這個測試 include。
  - 設定期檢查（DEFER，和 `gyo_expect_links` 同樣在整個專案設定完才跑）：走遍掃描根目錄下的目錄與 target（engine/os、tests/common/os、third_party 除外），`COMPILE_DEFINITIONS`、`INTERFACE_COMPILE_DEFINITIONS` 與 `COMPILE_OPTIONS` 的 `-D`／`/D` 裡，名稱含 OS 字或以 `$<PLATFORM_ID:...>` 為條件的定義會讓設定失敗。只看名稱不看值，因為值裡的路徑可能含任何字。
  - 自我測試新增 5 項（共 30 項）：產品與工具的 R4、產品測試與驗收程式的 R1／R2、根目錄存在但沒有原始檔、根目錄不寫產品名、產品測試與驗收程式的 allowlist 範圍。原本傳入 `WIDE_ROOTS` 的 5 處改用預設值（等價）。
- 基準（空 allowlist 實跑，產品與工具共 63 個發現、13 個檔案）和盤點對照：
  - 一致：`LogFile.cpp`、`NetworkStatistics.cpp`、`main.cpp`（localtime）、`match_main.cpp`（std::signal）、`Arena.cpp`、`MatchShotTrace.hpp`、`MatchIngressTrace.hpp`、`FileService.cpp`、`DocumentSession.cpp`、`ImGuiSdlRendererHost.cpp`。
  - 盤點沒有、守衛有：`TracePaths.hpp:42` 的 `std::filesystem::exists`（找空的 trace 檔名），歸 os-files-write。
  - 盤點有、守衛沒有：`tools/ui_editor/src/Main.cpp:71`、`CommandLine.cpp:46`、`match_main.cpp` 的命令列。它們把窄字串 argv 直接當路徑，沒有 OS 標頭、巨集或 API，文字規則抓不到；寫進 os.md 的已知盲點，由 os-console-shutdown 處理。
  - 誤報：`build/acceptance/object_fps_pvp/network_main.cpp:216` 的 `std::ofstream signal(...)`，變數名剛好叫 `signal`，被 R3 的「裸 `signal(`」抓到。依 PLAN 第 6 節「不放寬樣式」，列進該目錄的 allowlist 並寫明是誤報，不改規則。
    - 經過：一開始把裸 `signal(` 從 R3 拿掉（理由：直接呼叫需要 `<csignal>`，R1 會抓）。之後發現這是守衛判定的放寬（已 allowlist 的 `<csignal>` 檔，或經 asio 間接拿到 `<signal.h>` 的檔，新增裸呼叫就看不到了），依 AGENTS §14.4 要先徵求同意，所以在提交前改回原樣式。
  - 未啟用產品（object_fps、object_fps_v2 的 apps、tests、build/acceptance）沒有發現；os-skeleton 時 inactive_products.md 預告它們會有 allowlist，已更正。
- 設定期檢查的基準：現有的定義只有 `NOMINMAX`、`GYO_*` 與 `PVP_*` 的路徑與修訂號，都不識別 OS。third_party 的 asio 在 Windows 有 `_WIN32_WINNT`、`WIN32_LEAN_AND_MEAN`（INTERFACE），屬於 third_party，不檢查，寫進已知盲點。
- 探索性確認：在產品組態（`GYO_APPS=object_fps_pvp`、`GYO_TOOLS=ui_editor`、`BUILD_TESTING=ON`）下設定成功、沒有誤報；暫時在產品的 CMake 加 `$<$<PLATFORM_ID:Darwin>:PVP_PROBE=1>`，設定失敗並指出該 target，之後還原。這個確認不進突變目錄，因為目錄只改 Engine 的檔案，刪掉產品也不受影響。
- L1（本機 macOS Intel，core preset）：ctest 31／31（新增 `os.header_hygiene`），守衛通過。產品組態只做了設定，沒有建置。
- 突變：新增 os-g11～os-g17。重跑守衛相關的 os-g1～os-g17，16／16 killed，os-g16（公開標頭 include windows.h，Windows 專用）跳過。
  - os-g14 第一次是 wrong_failure：檢查正確，但 CMake 把長訊息折行，期望的正則對不上。改成印相對路徑，正則的空白改用 `\s+`。
- 證據：本機執行輸出（git 忽略）；重跑方式見 [dev_log](../../../dev_logs/2026_10_25_engine_os_guard_wide.zh-Hant.md)。

### os-process-info

- 2026-10-25：開始（high），基準是本分支 `7a673c0`。改到的消費端檔案（`LogFile.hpp`、`LogFile.cpp`、`NetworkStatistics.cpp`、`main.cpp`、產品的 `CMakeLists.txt`、`tests/object_fps_pvp/CMakeLists.txt`、`DiagnosticFileTests.cpp`）都不在消費端發行線 PR 的差異裡；日誌格式不變，所以依 D48 不必事先通知，完成後告知。
- 實作：
  - `engine/os/include/engine/os/Process.hpp`：`ExecutablePath`、`ProcessCpuTime`、`CurrentThreadCpuTime`、`ToLocalTime`（`LocalTime`：年月日時分秒與 UTC 偏移秒數）。每次都向 OS 讀。
  - `src/posix/Process.cpp`、`src/windows/Process.cpp`：照舊實作的 OS 呼叫（GetModuleFileNameW、/proc/self/exe、_NSGetExecutablePath＋realpath、GetProcessTimes／GetThreadTimes、getrusage、clock_gettime、localtime_r／localtime_s）。Windows 會去掉 `\\?\` 前綴（Invariant 5）。
  - `src/common/Calendar.cpp`：由 localtime 的欄位算 UTC 偏移，不另用 `_get_timezone` 之類的 CRT API，三平台同一套算法。
  - 錯誤碼：ExecutablePath 的 OS 失敗是 IoFailed（原生碼在 detail；沒有呼叫端依錯誤碼分支），Windows 路徑過長是 TooLarge，沒有 UTF-8 形式是 InvalidEncoding；CPU 與本地時間的失敗是 Unsupported。沒有動 `Files.cpp`（它在發行線 PR 的差異裡），所以錯誤碼沒有共用 Files 的對應表。
  - 消費端：`LogTimestamp`、`RunningExecutablePath`、`ProcessCpuSeconds`、`CurrentThreadCpuSeconds` 保留名稱與簽名，改成呼叫 engine/os 的薄轉接；`ClientConnection.cpp`（在發行線 PR 裡）和 `match_main.cpp` 都不用改。新增純函式 `FormatLogTimestamp`、`LogFileName`、`LocalWallClock`，`main.cpp` 的預設 log 檔名改用它們。
  - 連結：`match_domain` 多了 PUBLIC `GYO::Os`（標頭用到 `LocalTime`）；重新編譯 domain 原始碼的 `authority_digest_o0` 也加上 `GYO::Os`。
- 位元組對照（PLAN 第 7.6 節的驗收）：
  - Engine 測試（`tests/common/os/ProcessTests.cpp`）在 5 個固定 TZ（UTC0、JST-9、IST-5:30、NST3:30NDT、PST8PDT）×6 個固定時間點下，比對 `ToLocalTime` 的欄位與偏移、以及呼叫端照 `%04d`／`%02d` 與「±hhmm」組出來的字串，和 C runtime 的 `localtime_*`＋`strftime`（`%Y-%m-%dT%H:%M:%S`、`%Y%m%d-%H%M%S`、`%z`）逐 byte 相同。在 Windows 上這就是 PLAN 要求的「確認 UCRT `%z` 的現行輸出」；結果要等 windows-x64 的 L1。
  - 消費端測試：`FormatLogTimestamp`／`LogFileName` 對固定欄位給出固定的字串。
  - 執行檔路徑：Engine 測試把舊的各平台查法寫成參照，比對結果相同，且指向測試執行檔本身。
  - CPU 時間：不會減少、呼叫執行緒做事後會增加、程序的不小於呼叫執行緒的、新執行緒的只算它自己。
- 事前宣告的差異（Invariant 9；都不改日誌格式）：
  1. CPU 秒數改由整數奈秒換算成 double。舊碼對 user、kernel 各自換算再相加，最後一位元可能不同；日誌印到小數 3 位，而且量測本身不可重現。
  2. 執行檔路徑在 Windows 經 UTF-8 轉回，再用 `make_preferred` 換回 `\`，對 GetModuleFileNameW 的一般結果逐字相同。不同的只有：帶 `\\?\` 前綴的模組路徑（前綴被去掉）、含未配對 surrogate 的路徑（現在是錯誤）、POSIX 上不是 UTF-8 的路徑（現在是 InvalidEncoding，D43）。
  3. 找不到執行檔時的例外訊息改成 `Cannot locate the executable (<OsError>)`。
  4. 本地時間轉不了時（目前時刻不會發生），印出舊的零值 `std::tm` 的欄位、偏移 +0000；舊碼在 Windows 會印本地偏移。
- 檢查與經過：
  - Red：先寫 ProcessTests，在沒有實作時連結失敗。
  - `object_fps_pvp.cpu` 第一次逾時（60 秒）。原因是環境：本機的產品組態沒有指定建置類型（未最佳化），直接執行要 3 分 17 秒；preset 用 RelWithDebInfo。改成 RelWithDebInfo 重建後 23 秒通過。
  - 背景的整體建置會 clone 並建置 DXC（shader 主機工具），和本批無關，中途停掉；Client 與 Match 的入口檔用 compile_commands 做語法檢查。
- L1（本機 macOS Intel）：
  - core preset：ctest 31／31（gyo_os_tests 新增 6 個案例），守衛通過。
  - 產品組態（RelWithDebInfo）：gyo_os_tests、os.*、object_fps_pvp 的 cpu、sdl_symbol_guard、diagnostic_isolation、log_contract、mutation_tool、network_statistics、steady_clock、authority_digest，12／12。
  - `main.cpp`、`match_main.cpp` 只做了語法檢查；Client 與 Match 執行檔沒有連結（要等 DXC 等 shader 主機工具）。
- allowlist：`apps/object_fps_pvp` 從 15 條縮到 7 條（main.cpp、NetworkStatistics.cpp 的全部，LogFile.cpp 的 R1～R3，LogFile.cpp 的 R4 從 4 降到 2）。
- 突變：新增 os-p1～os-p6；本機 7／7 killed（含重跑的 os-g7、os-g17），os-p5（Windows 的 GetThreadTimes）跳過。消費端格式化的探索性突變 3 個（零偏移的符號、月份不補零、檔名少了秒）都被消費端測試抓到；它們改的是產品的檔案，不進 Engine 的突變目錄。
- 證據：本機執行輸出（git 忽略）；重跑方式見 [dev_log](../../../dev_logs/2026_10_25_engine_os_process_info.zh-Hant.md)。
- 2026-10-25：消費端確認：分析腳本讀的 CPU 值只到小數 3 位，驗收腳本與 log-contract 不讀「Cannot locate the executable」與執行檔路徑字串，所以宣告的差異 1～3 沒有讀取端；這些檔案在發行線上也沒有要動。
- 2026-10-25：推送 `9026820`，手動觸發四平台 L1（os-guard-wide 與 os-process-info 一起驗）：[run 40192938131](https://github.com/yojinn-io/GYO-Engine/actions/runs/40192938131)。
  - 結果：失敗。四個平台的 L1 job、消費端的 quick acceptance 都成功；只有 `Quick acceptance / toolchain / macos-arm64` 失敗（CI validation 與 CI gate 是彙總，跟著失敗）。
  - 失敗的是本批新加的 `Os process CPU time covers the calling thread's`：`CHECK( 197516000ns >= 197516916ns )`。
  - windows-x64：gyo_os_tests（含 5 個時區 × 6 個時間點的 `%z` 比對、3000 年以後的拒絕）、os.header_hygiene、os.boundary_guard 都通過。UCRT 的 `%z` 和新實作逐 byte 相同：PLAN 第 7.6 節要求的確認完成，沒有 Data Contract 變更。
- 失敗原因的切分（AGENTS §14.1：實作、測試、環境）：測試的前提錯，實作不變。
  - 本機重現：100 次失敗 3 次。
  - 第 1 次修正：以為差距只來自 `getrusage` 的微秒截斷（user、system 各截斷，最多不到 2 µs），加 2 µs 的容許值。本機 1000 次仍失敗 64 次，差距是 250～350 µs，所以這個解釋不對。
  - 量到的事實：macOS 上 `getrusage` 的程序時間，對正在執行的那一段比 `clock_gettime(CLOCK_THREAD_CPUTIME_ID)` 晚更新，兩個不同來源的時鐘在同一瞬間不必一致。「程序 ≥ 執行緒」不是結構性質。
  - 第 2 次修正：拿掉跨時鐘的比較，改成在 engine/os 讀數前後，用舊碼讀的同一個時鐘各讀一次（`getrusage`／`GetProcessTimes`、`clock_gettime`／`GetThreadTimes`），engine/os 的值要落在兩次之間。這是同一個單調時鐘的確定性質，根據來自 OS 而不是推測（§14.2）。本機 1000 次 0 失敗；os-p1～os-p6 重跑 5／5 killed（os-p4 改由這個案例殺），Windows 專用 os-p5 跳過。
  - 改的是本批新加、還沒有通過過任何驗收的測試，不是受入判定或 Design Invariants 的守衛（§14.4 的 1～5 不適用）；理由記在這裡與 commit。
- 2026-10-25：推送 `a9219fa`，第二次四平台 L1：[run 40194493434](https://github.com/yojinn-io/GYO-Engine/actions/runs/40194493434)。全部成功：四平台的 L1、quick acceptance（toolchain、消費端）、CI validation、CI gate。本批完成（os-guard-wide 同時完成）。

### os-console-shutdown

- 2026-10-25：開始。使用者先要求說明檔位、★ Windows 行為變更與需要同意的地方，回答四個問題後定為 D49（high＋三處逐行論證＋1 位 xhigh 審查；平台邊界例外不延伸、改寫 Invariant 6；連結守衛允許 Windows 的 shell32，定案前做突變與獨立審查；關閉主控台最多等 4 秒、正常結束還原 code page）。基準是本分支 `83add69`。
- 協調：`tools/ui_editor/CMakeLists.txt` 與突變 runner `time-threads-trace/scripts/run_mutations.py` 在消費端發行線 PR 的差異裡。動 CMake 前已通知消費端（加一行 `gyo_ui_editor PRIVATE GYO::Os`）；runner 的擴充是向下相容的，完成時一起告知。
- 實作：
  - `engine/os/include/engine/os/Console.hpp`：`CommandLineUtf8`、`Utf8ConsoleOutput`、`ShutdownReason`、`InstallShutdownRequests`、`RequestedShutdown`。
  - `src/common/Shutdown.cpp`：結束要求旗標（lock-free 的 `std::atomic<std::uint8_t>`，第一個要求的理由贏），engine/os 唯一的程序狀態。
  - `src/posix/Console.cpp`：`sigaction` 處理 SIGINT、SIGTERM，`SA_RESTART`、不重設 handler、只擋住處理中的訊號，和舊的 `std::signal` 相同；argv 照原樣交出，不是 UTF-8 的參數回 InvalidEncoding。
  - `src/windows/Console.cpp`：`SetConsoleCtrlHandler` 處理 Ctrl+C、Ctrl+Break、關閉主控台（最多等 4 秒）；登出與關機照 OS 的預設。`GetCommandLineW`＋`CommandLineToArgvW` 轉 UTF-8（`src/windows/Wide.cpp`，不改分隔字元）。主控台的輸出 code page 設成 UTF-8，解構時還原。
  - `gyo_os` 在 Windows 上 PRIVATE 連 shell32；守衛 `gyo_expect_os_links` 在 Windows 上要求正好是 GYO::Base＋shell32（D49）。
  - 呼叫端：Match（`match_main.cpp`）改用 `InstallShutdownRequests`／`RequestedShutdown`、`CommandLineUtf8`＋`PathFromUtf8`、`Utf8ConsoleOutput`；end 紀錄仍是 `reason=signal`，start 與 end 兩行的字串不變。ui_editor 的 `Main.cpp`、`CommandLine.cpp` 改用 UTF-8 命令列與主控台輸出，`gyo_ui_editor` 多連 GYO::Os。
  - 產品的 allowlist 從 7 條縮到 5 條（`match_main.cpp` 的 R1、R3）。
- 測試：
  - `tests/common/os/ConsoleTests.cpp`（gyo_os_tests）：命令列、非 UTF-8 參數、主控台輸出的套用與還原、沒有要求時為空、理由的名稱。
  - `ConsoleProbe.cpp`＋`console_check.py`（CTest `os.console_arguments`、`os.console_shutdown`，Windows 另有 `os.console_close`）：真的子程序。
    - 參數：日文、é、韓文、emoji、空白、引號、反斜線、tab、空字串，九個參數回來是相同的 UTF-8。外部參照是 Python 的 subprocess 依 Microsoft 規則組出的命令列。
    - 結束要求：POSIX 送「同一個訊號兩次，再送另一個」，程序不會被結束、理由是第一個；Windows 對新程序群組送 Ctrl+Break 三次。
    - 關閉主控台（只有 Windows）：探測程式對自己的主控台視窗送 WM_CLOSE。runner 沒有主控台視窗、或送不出主控台事件時回 77，CTest 顯示為 skipped，缺口看得到。
  - ui_editor 的核心測試補上 UTF-8 路徑參數與非 UTF-8 參數。
  - 實機（本機 macOS）：真的 Match 收到 SIGTERM、SIGINT（各兩次）都優雅結束，exit 0，log 最後一行是 `end version=1 reason=signal`；UTF-8 的 `--arena` 路徑原樣出現在錯誤訊息裡。
  - 消費端提醒有兩個讀取端依賴 POSIX 的 SIGTERM（`tests/object_fps_pvp/ingress_cross_check.py:80、174`，`build/acceptance/object_fps_pvp/run_timing.py:389`）。本機產品組態跑 `object_fps_pvp.ingress_cross`（SIGTERM 結束 Match，讀最後一行的 `reason=signal exit_code=0`）通過，已回覆消費端。
- 突變：新增 os-c1～os-c9、os-g18。runner 擴充：command 型可以同時指定 target（先建置），`{exe:NAME}` 代表建好的執行檔。
  - 本機：os-c1（拿掉 SIGTERM 註冊）、os-c2（handler 重設）、os-c3（後來的要求蓋掉理由）killed，連同重跑的 os-g7、os-g8、os-g17。
  - os-c2 第一次存活：檢查送的第二個訊號和第一個不同，`SA_RESETHAND` 只重設處理過的訊號。改成再送一次第一個訊號後，os-c3 又存活（第三個訊號和第一個相同，最後一個贏也是同一個理由）。最後定為「第一個、第一個、另一個」，三個都 killed。
  - Windows 專用：os-c4（拿掉關閉主控台的分支）、os-c5（拿掉 Ctrl+Break）、os-c6（命令列改用 ANSI code page）、os-g18（多連 ws2_32）要在 Windows 上跑。
- L1（本機 macOS Intel）：core 33／33；產品組態的 os、object_fps_pvp 相關與 ui_editor 共 20／20。Windows 的程式碼在本機編不到，第一次編譯是四平台 L1 的 windows-x64。
- 事前宣告的差異（Invariant 9；★ 是 Windows 行為變更）：
  1. ★ Windows 的 Match：Ctrl+Break 與關閉主控台視窗會優雅結束並寫 end 紀錄（以前直接被終止）。第一次 Ctrl+C 本來就會優雅結束，不變；之後再按 Ctrl+C 或 Ctrl+Break 會直接結束程序，和以前第二次 Ctrl+C 相同（收尾卡住時的出口）。驗收腳本的 `terminate()`（TerminateProcess）不經過 handler，不變。
  2. ★ Windows 的 Match 與 ui_editor：命令列是 UTF-8，ANSI code page 以外的字不再變成 `?`。切分改由 `CommandLineToArgvW` 做，引號內連續 `""` 之類的少見寫法可能和 C runtime 的 argv 切法不同；Python 的 subprocess 組出的命令列兩者切法相同。
  3. ★ Windows 的 Match 與 ui_editor：主控台的輸出 code page 在執行期間是 UTF-8，正常結束時還原；重導到檔案或 pipe 的輸出不受影響。
  4. POSIX：不是 UTF-8 的參數現在會被拒絕（Match 與 ui_editor 回 2），以前照位元組使用。範圍包含 `--listen` 這類非路徑參數（D50，使用者同意）。
  5. 結束要求的 handler 裝不起來時，Match 在 stderr 印一行；以前 `std::signal` 的失敗被忽略。
- xhigh 審查（1 位，D49）：
  - 三處重點沒有 blocker、major。POSIX 的 `sigaction` 旗標在 macOS 實測和 `std::signal` 相同（`0x2`）；glibc 依原始碼判斷相同。狀態權威沒有問題；start、end 兩行的字串不變。
  - minor：Windows 上第二次 Ctrl+C 的行為變了，沒有宣告（舊的 UCRT 在呼叫 handler 前重設成預設，所以第二次會直接結束）→ 改回：要求之後的 Ctrl+C、Ctrl+Break 照預設處理。
  - minor：POSIX 拒絕非 UTF-8 參數是 Invariant 9 的變更 → D50。
  - minor：os.md 有四處沒同步（元件狀態、守衛的連結說明、已解決的盲點、Invariant 6）→ 已改，並寫進 Console 的 Contracts。
  - nit：安裝失敗一半時沒有還原、並行呼叫時第二個會先拿到成功 → 改成加鎖、全部成功或全部還原。
  - 測試缺口：拿掉 `SA_RESTART` 的突變存活 → 探測程式讀回旗標，和同一個 C library 上 `std::signal` 設的旗標比對（os-c7）。Windows 的 Ctrl+C 沒有 L1 → 探測程式在自己新開的主控台送 Ctrl+C（`os.console_ctrl_c`，os-c8、os-c9）。
  - 沒有處理、記成後續：拿掉 install-once 的突變存活（無害）；`Wide.cpp` 與 `Files.cpp` 的 UTF-16→UTF-8 重複，等發行線 PR 合併後再合一；Windows 11 預設終端是 Windows Terminal 時，`os.console_close` 可能逾時而不是 skip（ConPTY 的假視窗可能不理 WM_CLOSE），要在 lan15 的實機上確認；runner 的 code page 已是 65001 或沒有主控台時，還原的路徑不會被走到。
  - §14.4 的獨立核對（shell32）：1 確認；2 確認但有但書（不限可見性；MSVC 預設的標準函式庫本來就有 shell32.lib，實際的連結命令列沒變）；3 原本寫「沒有收緊面」不成立（Windows 上只有 GYO::Base 改成失敗，無害）；4 確認（os-g8 在 macOS 被殺，os-g18 用 `WIN32=1` 模擬失敗），另有既有的假值漏洞；5 部分（os.md 的 Guard 說明要補，已改）。使用者定案（D50）。
- 修正後的檢查：`os.console_shutdown` 通過，macOS 上讀回的旗標是 `reference_flags=0x2`、`sigint_flags=0x2`、`sigterm_flags=0x2`。突變重跑：os-c1、os-c2、os-c3、os-c7、os-g8 killed；Windows 專用的 os-c4、os-c5、os-c6、os-c8、os-c9、os-g18 跳過。
- 2026-10-25：使用者說「推送本分支，手動觸發四平台 L1，驗 os-console-shutdown」。推送 `4ebfd7b`，[run 40202449713](https://github.com/yojinn-io/GYO-Engine/actions/runs/40202449713)。
  - 結果：失敗。macOS 兩列、Linux 的 L1 與 quick acceptance 都成功；windows-x64 的 L1 與 toolchain 列只有 `os.console_close` 失敗（CI validation、CI gate 跟著失敗）。
  - 通過的：windows-x64 第一次編譯 `Console.cpp`、`Wide.cpp`，連 shell32 時設定期檢查通過；`os.console_arguments`、`os.console_shutdown`（Ctrl+Break：一次是要求、第二次直接結束）、`os.console_ctrl_c`（自己的主控台，一次是要求、第二次直接結束）都 Passed。linux-x64 的 `os.console_shutdown` 通過，glibc 上 handler 的 `sa_flags` 和 `std::signal` 相同（審查時只依原始碼判斷，現在實測確認）。
  - `os.console_close` 的輸出：`['ready', 'timeout']`，以 4 結束，花了 10 秒。
  - 原因的切分（AGENTS §14.1）：測試手段在這個環境不成立，不是實作。主控台真的關閉時，OS 最晚在 handler 返回時（4 秒）結束程序，探測程式不可能活到 10 秒的逾時；所以 WM_CLOSE 沒有讓主控台關閉，handler 沒收到事件。審查者預告過 Windows Terminal（ConPTY）會這樣；CI 的 runner 也一樣。
  - 修正（測試，第 1 次）：探測程式送出 WM_CLOSE 3 秒後若沒有要求、視窗也還在，就印「the console window did not close on WM_CLOSE」並回 5，檢查回 77（CTest 顯示為 Skipped）。主控台真的關了卻沒收到要求時，程序會被 OS 結束，檢查仍然失敗，所以 os-c4 在手段成立的機器上照樣抓得到。CI 上的缺口：關閉主控台的路徑在 CI 沒有自動驗證，留給 lan15 實機（L3）與 Windows 上的突變。
- 2026-10-25：推送 `86f30b0`，第二次四平台 L1：[run 40203783441](https://github.com/yojinn-io/GYO-Engine/actions/runs/40203783441)。
  - 結果：失敗，和第一次相同：只有 windows-x64 的 L1 與 toolchain 列的 `os.console_close` 失敗，輸出仍是 `['ready', 'timeout']`、結束碼 4、10 秒。其餘（含 Windows 的 `os.console_arguments`、`os.console_shutdown`、`os.console_ctrl_c`）都通過。
  - 新加的「3 秒後視窗還在就回 5」沒有觸發，所以 3 秒時 `IsWindow` 對那個 handle 回 false：視窗不在了（或那個 handle 本來就不是這個程序看得到的視窗），而主控台的關閉事件沒有送到探測程式，程序也沒有被 OS 結束。
  - 能確定的：活到 10 秒、沒有要求的探測程式，只可能是關閉事件沒有送到（送到的話，handler 會記錄要求，或 OS 會在 handler 返回時結束程序）。實作的關閉分支在 CI 上仍然沒有被驗證到。
  - 依先前對使用者說的「再失敗就停下回報」與 AGENTS §14（同一問題改了判定方法仍沒有結論時停下回報），在這裡停下，交使用者決定下一步。這個測試至今改過 1 次判定方法。
- 2026-10-26：使用者選 (b)（D51）。探測程式在關閉模式下逾時就印「the close event never came」並回 5，檢查回 77；拿掉第 1 次加的 `IsWindow` 判斷（它包含在新規則裡）。
- 2026-10-26：推送 `c507c59`，第三次四平台 L1：[run 40205158822](https://github.com/yojinn-io/GYO-Engine/actions/runs/40205158822)。全部成功：四平台的 L1、quick acceptance（toolchain、消費端）、CI validation、CI gate。windows-x64 的 `os.console_close` 是 Skipped（CTest 的「did not run」也列出），`os.console_arguments`、`os.console_shutdown`、`os.console_ctrl_c` 是 Passed。本批完成；關閉主控台的路徑留給 lan15 的實機。
- 已知、留給 os-path-utf8-sweep：Match 的 `arena_file=` 與 ui_editor 的 `valid:` 仍用 `path::string()` 印路徑。在 Windows 上，ANSI code page 以外的路徑會在印出時丟例外。以前這種路徑在 argv 就變成 `?`，同樣開不了，所以沒有可用的情況變壞。

### os-files-write

- 2026-10-26：開始。使用者先要求說明檔位、要動的消費端檔案與需要同意的地方，回答四個問題後定為 D52（high＋1 位 xhigh 審查；Windows 維持 CRLF，用 `FileOpenMode::Text`；`ReplaceFile`、`SamePathComponent` 移到 os-tool-files，Windows 長路徑 log 的驗收移到 os-trace-writers；消費端突變由本計畫重新定位）。基準是本分支 `bbef352`。開工前通知消費端（PR #96 差異裡的 engine/os Files、NativeFileSystem、消費端突變目錄）。
- 實作：
  - engine/os：`FileWriter`（`WriteMode::Truncate`／`Append`，`LineEndings::AsWritten`／`Native`）、`CreateDirectories`。POSIX 在 `posix/Files.cpp`，Windows 在 `windows/Files.cpp`（沿用 P1 的 `OsCallPath`，共用模式 `FILE_SHARE_READ | FILE_SHARE_WRITE`，附加用 append-only handle）。
  - engine/io：`NativeFileSystem` 開放寫入（`Write | CreateIfMissing` 加 `Truncate` 或 `Append`，`Text` 換成平台的換行）與 `CreateDirectories`；`Capabilities()` 的 canOpenWrite、canCreateDirectories 改成 true。新增 `FS::NativeFileUri`（原生 UTF-8 路徑到 file URI，相對路徑先絕對化）。
  - engine/asset：`AssetWatcher` 的 exists／last_write_time 改經 `NativeFileSystem::Stat`；Engine 的 allowlist 拿掉它的 2 條。
  - 消費端：新增產品內的 `NativeFiles.hpp`／`.cpp`（`ReadNativeFile`、`NativePathExists`、`CreateNativeDirectories`、`NativeTextFile`），`Arena.cpp`、`LogFile.cpp`（FileSha256 的讀取、log 目錄的建立）、`MatchShotTrace.hpp`、`MatchIngressTrace.hpp`、`TracePaths.hpp` 改用它。錯誤訊息的文字不變。產品的 allowlist 清空，檔案刪除。
  - 產品測試的目錄新增 `os_boundary_allowlist.txt`（1 條永久：深路徑 arena 測試在 Windows 上替自己的建立與清理加長路徑前綴）。
- 事前宣告的差異（Invariant 9）：
  1. `AssetChange::writeTimeNs` 改成檔案系統自己的時間（Unix epoch 起的奈秒）；以前經每個程序取樣一次的時鐘差換算，數值會差到微秒等級。
  2. 建不了 log 目錄時的例外訊息改成 `Cannot create directory: <path> (<IoError>)`；以前是 `std::filesystem::filesystem_error` 的訊息。
  3. trace 檔不再經 C++ 的檔案緩衝：位元組在 flush、緩衝滿與 Close 時寫到 OS，寫出的內容相同。
  4. （D53，xhigh 審查後補宣告）這些站點的原生路徑先轉成 file URI，所以照 io.md 的規則（D44、D46）解析：
     - POSIX 上的 `..` 依字面消去，不先解析 symlink（`link/../x` 指向 link 所在目錄下的 x，以前是 link 目標的上一層）；
     - POSIX 上以 `//` 開頭的路徑被當成 UNC 而拒絕（以前 OS 當成 `/`）；
     - Windows 的 UNC 路徑一律拒絕，目前目錄在 UNC 上時的相對路徑也是；
     - POSIX 上目前目錄不是 UTF-8 時，相對路徑失敗；
     - `AssetWatcher::Watch` 收到的字串在 Windows 上以 UTF-8 解讀（以前是 ANSI code page；目前只有測試呼叫）。
     Client 的資產讀取從 P3 起已是如此；這次第一次波及 Linux 上的 Match 與 AssetWatcher。
- 已知的缺口：產品漏掉 `Text` 旗標的突變只在 Windows 上會有差，而消費端的突變 runner 沒有 platforms 欄位，所以沒有放進它的目錄；由 Engine 端的 io-w3（Windows）與產品測試「PvP shot and ingress trace files keep the bytes the text-mode stream wrote」在 windows-x64 的 L1 上守住。
- 突變（Engine）：新增 os-w1～os-w6、io-w1～io-w4；本機 os-w1、os-w4、os-w5、io-w1、io-w2、io-w4 killed，Windows 專用的 os-w2、os-w3、os-w6、io-w3 跳過。A3 的 io-n1-caps、io-n2-write-mode 因唯讀後端的程式碼改掉而過期，照原意圖重新定位（io-n1 改守仍為 false 的 canList；io-n2 改守「讀寫同時開」的拒絕），重跑 killed。
- xhigh 審查（1 位，D52）：沒有 blocker、major。
  - Windows 寫入（截斷、共用模式、附加、換行含 `\r\n` 變 `\r\r\n`、迴圈、目錄判定、長路徑）與 POSIX 寫入、engine/io 的寫入規則都沒有缺陷；審查者在本機另寫程式實測 POSIX 的部分。
  - minor：上面第 4 項的路徑差異沒有宣告 → D53 補宣告。
  - minor：Windows 的 `CreateDirectories` 會拿 `\\?`、`\\server` 這種根的一部分去建目錄，而且前綴 `C:` 檢查的是那個磁碟的目前目錄 → 改成先算出路徑的根（磁碟、UNC、長路徑形式、裝置），從根之後才逐層建立（os-w10）。根的規則另用 Python 版逐例核對 11 個情況。
  - nit：`NativeTextFile` 在成功 Close 之後 `operator bool` 是 false，和 std::ofstream 不同 → 改成成功 Close 後仍為 true，第二次 Close 失敗。
  - nit：新的錯誤訊息用 `path::string()`，Windows 上碰到 ANSI code page 以外的字會丟例外 → 改用 UTF-8。
  - nit：測試插在 D191 的註解與它的測試之間 → 移開。os.md 把 Windows 的附加寫成「和 C++ 串流相同」，其實 append-only handle 對其他寫入者是原子的、CRT 是每次寫前 seek → 改寫描述，並註明 os-trace-writers 搬移附加的呼叫端時要宣告這個差異。
  - 補上的測試（審查列的缺口）：第二個寫入者能同時開、Windows 上持有中的檔案不能刪（os-w9）；兩個附加寫入者交錯寫入（os-w7、os-w8）；開目錄只接受 InvalidPath；超過 16 KiB 緩衝的寫入、未 Close 就解構時仍寫出、Linux 的 `/dev/full` 寫入失敗會被回報（osfw-buffer-overflow-dropped、osfw-destructor-no-flush）；深路徑下寫 trace；新的建目錄錯誤訊息。
  - 沒有處理、記成後續：Capabilities 改成可寫之後，若有人把 NativeFileSystem 掛進 Vfs，寫入會導到它（目前沒有）；`/dev/full` 的檢查在 macOS 上跳過（消費端 runner 沒有 platforms 欄位，所以沒有為 sync 失敗加突變）。
- 突變（消費端）：
  - 本批改動的產品檔（Arena.cpp、MatchShotTrace.hpp、MatchIngressTrace.hpp、LogFile.cpp、TracePaths.hpp）裡共有 32 條消費端突變。實際檢查 find 字串，**沒有任何一條過期**（它們指的是邏輯，不在檔案存取的那幾行），所以 D52 預定的「重新定位」不需要改任何既有條目。
  - 新增 2 條：`osfw-arena-size-unchecked`、`osfw-trace-open-failure-ignored`。後者一開始寫成拿掉 `setstate`，是等價突變（`operator bool` 本身也看 `Good()`），改成讓 `operator bool` 一律回 true。
  - 依消費端 D184，用本機的產品組態（RelWithDebInfo）跑這 34 條：34／34 killed。消費端 runner 拒絕有未提交改動的工作樹，第一次沒有跑；改用 `--allow-dirty` 重跑，跑完確認工作樹只剩預期的改動。
  - 協調：消費端在 `ed63c24` 改了目錄裡 2 條的 expect，提交本批前合併發行線；`csc-r5-uncounted` 由消費端另外處理。
- 2026-10-26：推送 `31a5849`，四平台 L1：[run 40213916104](https://github.com/yojinn-io/GYO-Engine/actions/runs/40213916104)。全部成功：四平台的 L1（含 Windows 上 FileWriterTests 與 C++ 檔案串流的位元組比對、深路徑）、quick acceptance（toolchain、消費端，含 trace 檔的 CRLF 比對）、CI validation、CI gate。本批完成。

### io-native-paths

- 2026-10-26：開始（D54）。基準是本分支 `31a5849`。開工前已預先通知消費端（Client 的資產讀取 P3 會受影響）。
- 契約的設計（實作時定下的細節，定案前交使用者看）：
  - 原生路徑與 file URI 的對應仍是各平台相同的純字串規則；路徑照原樣保留，只把 `\\` 換成 `/`。
  - URI：authority 空或 localhost 是本機；本機時路徑以一個 `/` 開頭是本機路徑，以兩個以上開頭（`file:////host`、`file://///host`，RFC 8089 附錄 E）是 UNC；其他 authority 是 UNC 主機。
  - 原生：一個 `/` 開頭是本機路徑，兩個是 UNC（`//host`，沒有主機才拒絕），三個以上照 POSIX 視為根目錄 `/`。`\\x`、`C:`、`/C:/x`、命名空間前綴、NUL 照舊拒絕。
  - UNC 在 Uri 內部存成「authority 空、路徑以 `//host` 開頭」；正規形式印成 `file://host/...`，主機為 localhost 時印成 `file:////localhost/...`。手工把主機放在 authority 的 Uri 也對應到 `//host` + 路徑。
  - 和開工前的 §14.4 表不同的一點：缺 share 的 UNC（`file://server`、`//server`）不改成 InvalidPath，而是照樣對應，由 OS 決定（再放寬一點；定案時列出）。
- Red：先改寫 UriTests（23 個案例），在舊實作上 5 個案例失敗、31 個斷言失敗，第 13 個案例對錯誤的 Result 呼叫 `.value()` 而中止。
- Green：`Uri.cpp` 改寫（`CanonicalFilePath`、`FilePathFromRest`、`ToString`、`FileUriFromNativePath`、`NativePathFromFileUri`），UriTests 23／23；本機 core 33／33（資產、NativeFileSystem 的既有測試照過）。
- 2026-10-26：使用者去睡，關了 caffeinate，說「你跑完就休眠好了」。停在這裡，本機 commit，未推送。
- 2026-10-26：使用者說「來繼續」。動 PR #96 差異裡的檔案前，再通知消費端一次（列出 6 處；p1 的兩條 find 照原樣保留）。
  - engine/os：Windows `OsCallPath` 對 UNC 長路徑改用 `\\?\UNC\`（只有呼叫端的 `\\?\`、`\\.\` 照原樣傳）；POSIX `MakeAbsolute` 在目前目錄不是 UTF-8 時回 InvalidEncoding。os.md、Files.hpp 同步。
  - AssetPathResolver：根目錄照原樣保留（只把 `\` 換成 `/`），目錄內的相對路徑照舊字面清理並檢查不越出根目錄。連帶的判定變化：`allowEscapeAssetsRoot` 的結果保留 `..`（`assets/game/../shared/a.ppm`，原為 `assets/shared/a.ppm`）；根目錄自己的 `..` 越過開頭（`../assets`）不再是 PathEscapesRoot。`allowAbsolutePath` 的字面清理不在 D54 範圍，沒動（沒有產品設它），定案時列為剩下的字面站點。
  - 測試：NativeFileSystem 的 symlink 加 `..`（以 OS 自己開檔的結果為準，各平台同一判定）、Resolver 的根目錄案例、POSIX 非 UTF-8 目前目錄（macOS 建不出這種目錄，案例明寫後返回；Linux 與 Windows 實跑）、Windows 呼叫端 `\\?\` 長路徑照原樣、UriTests 加手工 Uri 的非正規形式（`///a`、`C:`）。Windows UNC 探針 `gyo_io_unc_probe`（localhost 與 127.0.0.1 兩項，`\\<host>\C$` 不通時回 77＝Skipped）。
  - Red：新案例在舊實作上 3 個失敗（symlink 讀到 "lexical"、根目錄的 `..` 被消掉、escape 的結果）。Green：本機 core 33／33。
  - 突變：相關 35／35 killed（15 個 Windows／Linux 專用的跳過，Windows 的留到 lan15，D206）。重新定位 io-m1、m3、m5、m8、m9、m10、m11、os-f7；io-m4、io-m13 只改 expect（被同一個案例殺掉，訊息不同）；新增 io-m12～m16、asset-r1、asset-r2、os-f8（Linux）、os-f9～f11（Windows）。
  - 本機 WIP commit `c965b01`。接著：ultracode 契約審查（1 審查者＋1 對抗檢查，審查者用 D54 核准的局部 xhigh）與產品組態（wide）的內容相關測試。
  - 產品組態（RelWithDebInfo）的內容相關測試 8／8 通過（engine_tests、object_fps_pvp.cpu／asset_catalog／presentation_cpu、編輯器 3 項、守衛）。
  - 契約審查（ultracode，2 位，約 49 分鐘）：審查者 15 項、對抗檢查 8 項；對抗檢查推翻 F9，其餘維持。兩位都用舊碼（`git show 31a5849`）與新碼實跑真實輸入，輸出在本機 scratchpad（git 忽略）。
    - 2 個 major（F1、F2）：兩個以上的斜線開頭時，「看起來像磁碟機的 POSIX 路徑」的拒絕被繞過（`///C:/x`），磁碟機形狀的 UNC 主機（`//C:/x`）印出的正規形式 `file://C:/x` 解析不回來（不動點破掉）。
    - 修正：磁碟機外觀的檢查移到斜線收合之後，照「原樣」與「去掉點段後」兩種形式檢查（字面檢查只用來拒絕）；命名空間前綴的檢查也移到收合之後；UNC 主機含 `?` 或 `:` 一律拒絕（各種寫法一致，也解決 A4 的 `//??/` 不一致）。結果 `file:///./C:/x` 回到舊判定（InvalidPath），開工前表上的 L4 消失；`/C:/../x` 從「接受為 `/x`」變成拒絕（新的收緊面）。
    - 測試：UriTests 加「產生的各種寫法」性質測試（762 個原生寫法、310 個 URI 寫法，都要往返並保持不動點）；它在修正途中又抓到 `/C:/../x` 與 `///??/` 兩個同類缺陷，一併修正。補 F7 的未守住檢查（手工 Uri 的主機驗證、authority 的反斜線）、F12（`.value()` 前先 REQUIRE，回歸不再讓整個執行檔中止）、F13（symlink 用 UTF-8 路徑建）、F14（`IsUncPath` 的 `\\?`、`\\.`）。
    - 文件：io.md 的 F3、F8、F10、F15、A1、A2、A5；Uri.hpp 的註解（F11、A7）。
    - §14.4 表的更正：T2 錯（舊碼本來就拒絕 `///a`、`C:`，判定不變）；漏列的面：錯誤碼的改變（NotSupported→InvalidPath）、Resolver 的 catalog file URI（原本在根目錄下當相對路徑，現在拒絕）、loose parser 給 file scheme、根目錄不再 squash、POSIX 上 `file://host/x` 讀本機檔、靠字面規則才讀得到的路徑現在失敗（A1）。
    - 不在本批修的：`allowAbsolutePath` 仍會把 catalog 路徑的 `//` 收合、點段字面消去（F6、A8；沒有產品設它）；`normalizeSeparators=false` 時 `..\x` 繞過包含檢查（A6，既有，不在 D54）。
  - 產品組態的內容相關測試在修正後重跑 8／8；本機 core 33／33；受影響的突變 32／32 killed（io-m5 的 expect 改成新的殺法：命名空間檢查關掉時，`///??/` 收合成 `/??/` 而解析不回來）。
  - 2026-10-26：定案關卡（D55）。使用者對三個問題選：「照此定案」「照草稿定案」「記為未結，之後處理」。下一步：整理成一個 commit、通知消費端、問過使用者後推送並跑四平台 L1。
  - 2026-10-26：兩個 WIP commit 併成 `88408b2`，通知消費端（結果與 Windows 專用突變、探針）。使用者說「推送本分支，手動觸發四平台 L1，驗 io-native-paths」。推送 `88408b2`，四平台 L1：[run 40295007179](https://github.com/yojinn-io/GYO-Engine/actions/runs/40295007179)。全部成功：四平台的 L1、quick acceptance（toolchain、消費端）、CI validation、CI gate。windows-x64 上 `io.unc_localhost`、`io.unc_127.0.0.1` 都是 Passed（`\\localhost\C$` 連得到，短路徑與超過 300 字元的 UNC 路徑都實跑）；`os.console_close` 照 D51 為 Skipped。linux-x64 的 gyo_os_tests 通過（含非 UTF-8 目前目錄的案例）。本批完成。
- 下一步（依 PLAN 第 7.8b 節與 D54）：
  1. engine/os：Windows 的 UNC 長路徑改用 `\\?\UNC\`，以 `\\localhost\C$` 測試（CI 做不到就 Skipped）；POSIX `MakeAbsolute` 在目前目錄不是 UTF-8 時回 InvalidEncoding（D43 的明確契約）。
  2. 呼叫端：AssetPathResolver 的根目錄照原樣保留；POSIX 的 symlink 加 `..` 測試（NativeFileSystem）；重新定位相關突變（io-m1、io-m8、io-m10 等）。
  3. 突變檢查與 ultracode 契約審查（1 審查者＋1 對抗檢查，兼做 §14.4 的獨立核對），然後把判定與 io.md Invariant 3 的新條文交使用者定案。
  4. io.md 的路徑契約與 Invariant 3、os.md 的長路徑說明、dev_log。
- 2026-10-26：消費端回覆（開工時要注意的交集）：
  - D53：lan15 與 L2 的腳本傳給產品的 `--log`、trace 路徑都是絕對路徑，沒有「symlink/..」也沒有 `//` 開頭；`content_deep_path.py` 的 `\\\\?\\` 只用在 Python 自己複製檔案。消費端會請使用者在 lan15 期間把發行包解壓到本機，不從網路共用執行。
  - io-native-paths 和發行線的交集：
    1. P1 的 `OsCallPath`：改成 `\\\\?\\UNC\\` 時一起更新 os.md「Long paths on Windows」；消費端的 Windows 專用突變 `p1-no-long-path-form`（find：`return std::wstring(L"\\\\\\\\?\\\\") + full.value();`）與 `p1-long-path-from-too-high`（find：`constexpr std::size_t kLongPathFrom = MAX_PATH - 12;`）的那兩行若有變動，要重新定位。
    2. `NativeFileAssetSourceTests.cpp` 的相對路徑案例讀 `./sub/../payload.txt`，`sub` 存在，照 OS 解析結果相同；開工後跑 engine_tests 確認（本機 core 33／33 已通過）。
    3. `ContentManifest` 的 `IsRelativeContentPath` 自己拒絕 `..`，是內容契約，不改。
  - 分支會話「規劃 Engine 的 OS 統一子系統」轉達同一個決定，估時寫 6～10 h；本會話開工前的估計是 12～19 h（使用者核准 D54 時看的是後者）。

### os-tool-files

- 2026-10-26：開始。使用者先要求說明檔位、要動的工具檔案與需要同意的地方，回答四個問題後定為 D56。基準是本分支 `2b6afc5`。開工前通知消費端（PR #96 差異裡的 engine/os Files、NativeFileSystem、`tools/ui_editor/CMakeLists.txt`）。
- 盤點（工具的 allowlist 6 條）：FileService.cpp（R2 3、R3 2、R4 12）、DocumentSession.cpp（R4 1）、ImGuiSdlRendererHost.cpp（R2 3、R4 2）。另外 DocumentSession.cpp:233-234 的 `.string()`（原列在 os-path-utf8-sweep）在改寫的區塊裡，這批一起改成 UTF-8。
- 實作（本機，未提交）：
  - engine/os：`RenameReplacing`、`RemoveFile`、`ResolvedPath`、`SamePathComponent`、`FontDirectories`（POSIX 與 Windows；Windows 只能靠審查與 L1）。
  - engine/io：NativeFileSystem 的 `Move`、`Remove`（遞迴仍 NotSupported），Capabilities 改；新增 `Helpers::WriteFileAtomically`。
  - 工具：FileService 改走 engine/io（讀、寫、Stat、Exists、建目錄、原子寫入）與 engine/os（絕對化、解析、比較）；新增 `PathExists`（DocumentSession 用）；`FileStamp.writeTime` 改成奈秒；字型改用 `FontDirectories` 與不分 OS 的候選清單。工具的 allowlist 清空並刪除，守衛通過。
  - 測試：`tests/common/os/FileChangeTests.cpp`（新，`ResolvedPath` 以 `weakly_canonical` 為參照）；NativeFileSystemTests 加 Move／Remove 與原子寫入；「says what it supports」的判定隨 D56 改（Move、Remove 不再是 NotSupported，`canMove`、`canRemove` 為 true）；EditorCoreTests 加「經 symlink 進入 asset 根目錄的匯出被拒絕」。
  - 本機：core 33／33（engine_tests 121／121、gyo_os_tests 52／52），產品組態的編輯器測試 6／6。
- 突變（第一輪）：Engine 端 27／27、工具端 2／2 killed。os-x4 第一次 timeout：我寫的測試裡 `CAPTURE(Utf8(input))` 在回報失敗時會再算一次運算式，而 `Utf8` 裡有 REQUIRE，在 doctest 的回報器裡又觸發斷言，造成鎖死。改成先存成變數後 killed。實作沒有錯。
- xhigh 審查（1 位，唯讀，約 28 分鐘）：沒有 blocker；1 個 major、7 個 minor、2 個 nit，另有 6 個測試缺口。審查者抓了 MSVC STL 的原始碼當參照（本機 scratchpad，git 忽略）。
  - major：Windows `ResolvedPath` 往回找到磁碟根目錄時，接回的路徑少了分隔字元（`subst S: C:\work` 下的 `S:/newdir/x.json` 會變成 `C:/worknewdir/x.json`）；而且 MSVC 的 `weakly_canonical` 本來就不解析根目錄。修正：往回找到根目錄時，回傳照字面處理的路徑。
  - 已修：缺少元件的錯誤集合對齊現行 STL（加上 `ERROR_BAD_PATHNAME`、`ERROR_DIRECTORY`、`ERROR_NETNAME_DELETED`）；POSIX 改成先 `stat` 再 `realpath`（macOS 上 `file.txt/` 的結果與 libc++ 一致）；`RemoveFile` 遇到目錄時，各平台都回 InvalidPath；Windows `SamePathComponent` 遇到不合法的 UTF-8，在呼叫 OS 前就拒絕；`ReadTextFile` 不再有 64 MiB 上限（照舊讀完整個檔）；engine/os、engine/io 的註解不再提到工具名稱（Invariant 7）；os.md 的 Components 表。
  - 補上的測試：原子寫入的記錄用 `IFileSystem`（寫入對象不是目標本身、有 Move、寫入失敗時刪掉暫存檔）；只有根目錄存在的路徑；Windows 用 `DefineDosDeviceW` 做的磁碟代號，以及 `\\localhost\C$` 的 UNC；字型目錄的順序；刪目錄的錯誤碼。
  - 修正後重跑：本機 core 33／33、編輯器 6／6；突變 Engine 端 32／32、工具端 2／2 killed（Windows／Linux 專用的 20 個跳過）。
  - 交使用者決定（事前沒有宣告的差異）：POSIX 的 `AbsolutePath("")`、POSIX 讀目錄與 FIFO、Windows 名稱不合法的路徑、錯誤訊息的文字、字型選擇的邊界情況（Linux）與 WINDIR、沒有磁碟代號的磁碟區（GLOBALROOT）。
  - 2026-10-26：使用者定為 D57（a～e 接受並補宣告；GLOBALROOT 維持回錯誤，記為已知差異）。下一步：整理成一個 commit、通知消費端、問過使用者後推送並跑四平台 L1。
  - 2026-10-26：WIP 併成 `c8f3e966`，通知消費端。使用者說「推送本分支，手動觸發四平台 L1，驗 os-tool-files」。推送 `c8f3e966`，四平台 L1：[run 40324361964](https://github.com/yojinn-io/GYO-Engine/actions/runs/40324361964)。全部成功：四平台的 L1、quick acceptance（toolchain、消費端）、CI validation、CI gate。windows-x64 上 gyo_os_tests、engine_tests、編輯器 6 項、`io.unc_*` 都通過。Windows 專用的 `subst`（`DefineDosDeviceW`）與 UNC 的 `ResolvedPath` 案例、以及 symlink 案例，CI 的輸出看不出是實跑還是寫訊息後返回（doctest 的 MESSAGE 只在失敗時印出）；要在 lan15 用 `-s` 跑一次確認。本批完成。

### os-trace-writers

- 2026-10-26：開始。使用者先要求說明檔位、要動的檔案與需要同意的地方（含 trace.md Invariant 7 與 §14.4），回答四個問題後定為 D58。基準是本分支 `ca434952`。
- 盤點：`TraceSink.cpp:102` 的 `std::ofstream`（文字模式；附加或清空；一批結束時 flush，`flushEachLine` 時每行 flush）；`tests/common/trace/CMakeLists.txt:11` 的連結檢查（exactly GYO::Threads）；Engine allowlist 的 `TraceSink.cpp R4 1`；tt2 突變裡 `tt2-final-batch-unwritten` 的 find 會過期；`os-g1-header` 的 find 是 include 行，不受影響。消費端呼叫者：`LogFile.cpp`（附加、每行一寫）、`MovementTraceWriter.hpp`（清空）。
- Red 用的測試：TraceSinkTests 加「超過 300 字元的路徑」（目錄在 Win32 的目錄長度限制內，檔名讓路徑超過 300；讀回與刪除經 engine/os）。macOS 上舊實作也通過（沒有 260 的限制），Red 只能在 Windows 看到；先單獨 commit，推送跑 L1。
- 2026-10-26：推送 Red 用的 `a75e4028`，四平台 L1：[run 40331764169](https://github.com/yojinn-io/GYO-Engine/actions/runs/40331764169)。**Red 如預期**：只有 windows-x64 失敗，而且只有新案例「longer than 300 characters」在開檔時失敗（`TraceSinkTests.cpp(51): REQUIRE( sink.has_value() )`；gyo_trace_tests 以外 95 項中 94 項通過）；macOS、Linux 全部通過。等待期間在本機實作（未推送）。
- 實作（本機，未提交）：TraceSink 的 `std::ofstream` 換成 `Os::Files::FileWriter`（`LineEndings::Native`，附加或清空），一批的行在 64 KiB 或批尾寫出、`flushEachLine` 每行寫出、寫入失敗後不再寫；錯誤 detail 的路徑改成 UTF-8；`gyo_trace` PRIVATE 連 `GYO::Os`；連結檢查改成 exactly GYO::Threads and GYO::Os；Engine allowlist 刪掉 `TraceSink.cpp R4 1`；trace.md Invariant 7 與 os.md Invariant 2 的新條文（草稿，待定案）。
- 本機：core 33／33，gyo_trace_tests 12／12；產品組態的相關測試 8／9，`object_fps_pvp.worker` 第一次失敗（「action worker emitted overdue catch-up packets」，`worker_main.cpp:613` 要求 action 封包間隔 ≥30 ms），之後單獨重跑 3／3 通過。依 §14.1 判為環境：那次和另外 8 項測試、產品組態的建置並行（5 分鐘平均負載 9.17），而且 worker 探針不經 TraceSink 與 LogFile。
- 突變：`tt2-final-batch-unwritten` 重新定位（意圖不變）；`os-g6-stale` 原本指向刪掉的 TraceSink 條目，改指 Waiter 的條目（意圖不變）；新增 tt-x1～tt-x6（連結檢查兩個方向、Windows 換行、附加、Linux 寫入失敗、批尾寫出）。
- xhigh 審查（1 位，唯讀，約 17 分鐘；在 `git archive` 的副本上實跑）：沒有 blocker、major。Invariant 1～6 守住；POSIX 上新舊位元組逐位元組相同（含 `\r`、`\n`、`\r\n`、空行、NUL）；寫入失敗後的 Good／FlushNow 與舊版相同（以 RLIMIT_FSIZE 實測）。
  - 已修：trace.md 的圖畫錯邊（看起來像 Os → Time）；Invariant 7 的「privately」原本沒有守衛（`gyo_expect_links` 只比集合），在連結檢查加上「GYO::Os 不得出現在 INTERFACE」；一行很長之後緩衝不再一直占記憶體；長路徑測試的目錄縮短（原本 TEMP 超過約 60 字元時，在 Windows 會超過 248），並且在每條離開路徑都清掉檔案；`OpenSink` 失敗時印出錯誤；trace.md 的守衛列表補上 tt-x*；補 `<atomic>`。
  - 補的測試：沒有附加時會清空既有檔案；開檔失敗的 detail 以 UTF-8 路徑開頭。新增突變 tt-x7（Os 以 PUBLIC 連結）、tt-x8（清空變附加）、tt-x9（detail 少了路徑）。
  - 仍未守住（記為缺口）：失敗後不再寫、`flushEachLine` 每行一寫、64 KiB 門檻、Close 結果被忽略；這些在測試裡看不到。
  - 修正後：core 33／33，gyo_trace_tests 13／13；產品組態的相關測試逐一串行 9／9（含 worker）；突變 tt2 6／6、tt-x 與 os-g6 共 8／8 killed（tt-x3 Windows、tt-x5 Linux 跳過）。
  - §14.4 的核對：1、2、4 點不完整，3、5 點成立。1、2：檢查只看直接連結，原本 PUBLIC／INTERFACE 的 Os 也會通過，已加檢查補上。4：閉包的末端是 GYO::Base，而 Base 沒有連結檢查，這是新舊判定都有的既有缺口（D50 的假值漏洞也還在）。
  - 交使用者決定（事前沒有宣告的差異）：Linux 上不是 UTF-8 的 trace 路徑改成 OpenFailed；錯誤 detail 的文字（多了 engine/os 的原因，Windows 是 `/`，非 UTF-8 時 detail 沒有路徑）；`flushEachLine` 遇到長行也只寫一次（原本超過串流緩衝的行會拆成多次）。
  - 2026-10-26：使用者定為 D60（條文照草稿定案；差異 a～c 接受並補宣告）。下一步：整理成 commit、通知消費端、問過使用者後推送並跑四平台 L1（Windows 的 Green）。

## 未結事項

- ~~使用者核准 PLAN，回答開放決策 1、14、15，並同意 A1～A3 的 Delta。~~ 2026-10-25 完成（D46）。
- A1～A3 併進消費端的發行線 PR（D46）。消費端的發行任務 2026-10-25 回覆的做法（暫定，開 PR 時再和使用者確認，消費端 D198）：
  - A1～A3 的分支從 master 切，不帶消費端其他線的改動。本計畫的分支 `claude/trusting-joliot-f13b78` 是從 master `23fab75` 切出的。
  - 消費端的發行線在它目前那條線的 PR 合併後，從 master 切出 `claude/pvp-v71-release`。
  - A1～A3 完成後，由消費端以 `git merge --no-ff` 併進發行線（不 rebase、不 squash，保留 commit 與訊息），再在上面做 P1～P5；整個 PR 照 repo 規則以 `gh pr merge --merge` 合進 master。
  - A1～A3 若在發行線開好之前完成，就等它開好再併。P4 在產品檔案的那一行，以及和消費端其他線的衝突，由消費端在發行線處理。
  - 完成時通知消費端：分支名稱與最後的 commit。
- ~~os-skeleton 開始時：用空 allowlist 實跑守衛，產生 Engine 的基準，對照盤點。~~ 2026-10-25 完成，和盤點一致。
- ~~A1～A3 的四平台 L1~~ 2026-10-25 通過（run 40173730672）。
- ~~os-files-read 的公開介面 xhigh 審查（1 位）：等使用者同意。~~ 2026-10-25 完成，修正見「os-files-read」。
- Windows 專用的突變 os-f4、os-f5、os-g16、os-p5（以及消費端的 p1、p4）要在 Windows 機器上執行（CI 不跑突變）。
- 2026-10-26：消費端在發行線上跑的全量突變（消費端 D185）：Engine 的目錄 133／133 killed（含本計畫的突變，平台不符的照 platforms 跳過）。產品 654 條裡，8 條歸因於負載、2 條期望訊息過時（`ed63c24` 已改）、1 條是檢出力問題（`csc-r5-uncounted`，`f4ccd5c` 已修，worker 檢查的 47 條 47／47 killed）。兩個 commit 都已併進本分支（`92f6b11`）。Windows 專用突變照消費端 D206 在 lan15 實機跑。在那之前，Windows 端的檢出力只有 L1 的測試本身。
- os-files-read 的非 ACP 測試：對「改用 ACP 轉換」這類突變，只要 runner 的 ACP 不是 UTF-8（65001）就有檢出力。測試同時含日文、正體中文與 é，在 1252 與 932 上都有效。新增的 Windows 專用案例「record the process ANSI code page」會印出 `GetACP()`，並檢查它不是 65001；第一次 windows-x64 L1 從它的輸出確認。
- ~~os-files-write 開始前：先定好命名規則（審查的 finding 15）。~~ 2026-10-25 寫進 os.md 的 Contracts（Names）。第一次 windows-x64 L1 實際撞到之後，`ReadFile` 改名為 `FileReader`。os-files-write 的寫入類名稱照這條規則取。
- io-contracts 起的既有契約問題（審查的 finding 10）已在 os-files-read 處理：NativeFileSystem 用 `Os::Files::IsAbsolutePath` 拒絕「在這個平台不是絕對路徑」的 file URI 路徑。
- ~~io-contracts 開始時：契約審查用 ultracode（1 審查者＋1 對抗檢查），先問。~~ 2026-10-25 完成。
- 既有問題（不屬於本計畫的批次）：ParseUriLoose 對未知 scheme 不設定 schemeText，`pak://` 之類的 root 經 MountTable 往返會失去 scheme。等有 pack 後端時，依 io.md 的 Replacement Criteria 處理。
- ~~os-files-read 開始時：確認 Windows CI runner 的 ACP，選它表示不了的字元做測試。~~ 改成選一般 Western code page 表示不了的日文與正體中文，確認放到第一次 windows-x64 L1（見上）。
- ~~A1～A3 完成後：通知消費端的發行任務（分支與 commit）。~~ 2026-10-25 完成。
- ~~os-guard-wide 與 os-process-info 的四平台 L1~~ 2026-10-25 [run 40194493434](https://github.com/yojinn-io/GYO-Engine/actions/runs/40194493434) 全部成功。
- 後續（D50）：`tests/common/time/CMakeLists.txt` 的 `gyo_expect_links` 以 `if(entry AND …)` 過濾，會把名字是 CMake 假值的連結項目（N、NO、OFF、FALSE、IGNORE、0、*-NOTFOUND）丟掉，例如 `shell32 OFF` 也會通過。修正只會收緊，依 AGENTS §14.4 由 time-threads-trace 的 owner 修並事後報告。
- ~~os-console-shutdown 的四平台 L1~~ 2026-10-26 [run 40205158822](https://github.com/yojinn-io/GYO-Engine/actions/runs/40205158822) 全部成功（`os.console_close` Skipped）。
- lan15 的 Windows 實機（D51）：用 Ctrl+C、Ctrl+Break、關閉主控台視窗結束 Match，log 最後一行都是 end 紀錄（PLAN 第 7.7 節的 L3）；在實機上跑 `os.console_close`（應為 Passed）與 Windows 專用突變 os-c4、os-c5、os-c6、os-c8、os-c9、os-g18（以及 os-f4、os-f5、os-g16、os-p5）。Windows 11 的預設終端是 Windows Terminal 時，`os.console_close` 可能也是 Skipped，那就用傳統主控台（conhost）視窗跑。~~windows-x64 上的 `%z` 比對~~ 2026-10-25 run 40192938131 通過，已通知消費端的發行任務。~~CPU 測試修正後重跑 L1~~ 2026-10-25 [run 40194493434](https://github.com/yojinn-io/GYO-Engine/actions/runs/40194493434) 全部成功。
- 後續（D55）：AssetPathResolver 的 `allowAbsolutePath` 仍把 catalog 絕對路徑的點段依字面消去，並把 UNC 的開頭 `//` 收合成 `/`（Windows 上會變成目前磁碟機的根目錄）。沒有產品設它；有消費端需要，或做到 asset 解析的批次時，改成照原樣保留（測試「allow absolute paths」的期待值要依 AGENTS §14.4 改）。
- 既有問題（審查的 A6，不在 D54）：`normalizeSeparators=false` 時，catalog 路徑 `..\x` 繞過 Resolver 的包含檢查，在 Windows 上越出根目錄。沒有消費端關掉這個選項。
- io-native-paths 的 Windows 專用突變 os-f9～os-f11 與 Linux 專用的 os-f8 要在對應平台執行（D206 的 lan15；Linux 機器）。
- 已知差異（D57）：Windows 上沒有磁碟代號的磁碟區（WinFsp、ImDisk 之類），`Os::Files::ResolvedPath` 回錯誤，MSVC 的 `weakly_canonical` 則回 `\\?\GLOBALROOT\...`；編輯器的匯出檢查在那種情況退回字面比較，刻意經 junction 的匯出可能繞過。真的遇到時，再決定是否放寬 os.md Invariant 5 的文字。
- os-tool-files 的 Windows 專用突變 os-x7～os-x11 要在 Windows 上執行（lan15，D206）；Linux 上 os-x1～os-x5、os-x12 只在 macOS 跑過。lan15 上另外用 `gyo_os_tests -s -tc="*resolve*"` 與 `gyo_ui_editor_core_tests` 確認 `subst`、UNC、symlink 的案例是實跑（不是寫訊息後返回）。
- 既有缺口（D60 的 §14.4 核對）：`GYO::Base` 沒有設定期的連結檢查，所以 Trace（經 Os、Time）與其他模組的連結閉包的末端沒有守住；`target_link_libraries(gyo_base INTERFACE SDL3)` 也能通過設定。和 D50 的 `gyo_expect_links` 假值漏洞一起處理。
- os-trace-writers 沒有守住的性質（測試看不到）：失敗後不再寫、`flushEachLine` 每行一寫、64 KiB 門檻、Close 的結果；Windows 專用 tt-x3 與 Linux 專用 tt-x5 待對應平台。
- R3 的裸 `signal(` 會抓到名為 `signal` 的變數或宣告。目前只有 1 個誤報，列在驗收程式的 allowlist。若要縮小樣式（例如排除宣告），是守衛判定的變更，依 AGENTS §14.4 先交使用者決定。誤報反覆出現時，照 os.md 的 Replacement Criteria 6 重寫規則。
