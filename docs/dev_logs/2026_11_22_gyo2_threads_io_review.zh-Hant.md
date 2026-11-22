# 2026-11-22 threads、io 重審（`v8-arch-gyo2-threads-io-review`）

Owner：架構（GYO2 主會話開的任務卡 `local_a184932c`）。分支 `claude/gyo2-threads-io-review-da738d`（base `gyo2`）。實作者 Opus 5.5／high；敵對式檢查 Fable 5.2／high（宣告一次、`WorkerPool` 的突變清單一次，都唯讀）。事前宣告：[threads-io-review-declaration.zh-Hant.md](../architecture/target/v2/threads-io-review-declaration.zh-Hant.md)（修訂 1，動工前核准的版本 74d38577）。

## 為什麼

- R0（`v8-arch-rebuild-r0-approved`）把 `engine/core/threads`、`engine/core/io` 照 master 帶過來，標「帶過去並重審」，之後沒排進批次表（2026-11-21 補列，`v8-arch-gyo2-plan-gaps-2026-11-21`）。
- 使用者 2026-11-22：「threads／io 重審、audio 重審 也可以開始了。」
- 需求的來源：asset 宣告 5.6（X-1～X-6）、5.12；交換點宣告 E-15、E-17；讀檔宣告 19.6；網路宣告修訂 2 第 7 節。

## 使用者的決定（照四樣，已轉主會話記 HANDOFF）

原話：「Q1 選 a、Q2 選 a、Q3 選 a，開始」（本卡的對話，2026-11-22）。比過的選項與理由在宣告第 7 節；使用者沒有另外說理由，採用卡片的建議。

1. **Q1 工作執行緒池**：(a) `engine/core/threads` 加 `WorkerPool`，產品自己建、經選項交出，Engine 不預設開執行緒。比過：(b) 加預設開著的 Module（Bevy `TaskPoolPlugin`）／(c) 不提供。推翻 rebuild-concept C8 的「worker pool 第一版不做」，同一批改寫 C8。
2. **Q2**：(a) 拿掉 `Vfs`／`MountTable`／`MountPoint`（連同 `Vfs&` 版的讀檔、`ParseUriLoose`），刪 io DI 5。比過：(b) 留讀取、修 `ParseUriLoose` 的缺陷。
3. **Q3 存檔目錄**：(a) 補在 os 層，另開一個小批（本批不做）。比過：(b) 不補。

## 做了什麼

1. **io 拿掉**（宣告 I2b、I3、I5、I7、I8、I9、I13）：
   - 標頭 14 個、原始檔 12 個（含 7 個空殼）、測試檔 3 個（`BufferedStreamTests`、`StreamReaderTests`、`VfsTests`）。
   - `IFileSystem` 縮成 `Name`、`Open`、`Exists`、`Stat`、`CreateDirectories`、`Remove(uri)`、`Move`、`ToNativePathString`。
   - `UriScheme` 只剩 `None`、`File`、`Unknown`（其他 scheme 的名字放 `schemeText`）；`ParseUriLoose` 拿掉。
   - `PathUtils` 公開的只剩 `Normalize`、`Parent`、`RemoveDotSegments`；`Normalize` 內部用的三個判定改成檔內函式；`Path` 只剩 `FromNormalized`、`Parse`、`Str`、`Empty`、`Parent`、`==`。宣告 I2b 列的名單之外，`Join`、`Filename`、`Extension` 在拿掉 `Path` 的成員後也沒有呼叫者，照同一條判準一起拿掉。
   - io 不再連結 `GYO::Math`（只有被拿掉的 Stream 類用到）。
2. **io 補與改**：`IOErrorCode::TooLarge`，`ReadAllBytes`／`ReadAllTextUtf8`／`ProbeFileStamp` 超過上限回它（I6b；兩條路徑都經 `TooLargeErr()`）；`IFileSystem` 與原生後端的註解寫明 DI 6。
3. **測試執行檔** `engine_tests` → `gyo_io_tests`（N14）；io 突變的 `target` 跟著改。
4. **threads**：`WorkerPool`（`WorkerPool.hpp`、`WorkerPool.cpp`，約 150 行；工作者是 `RoleThread`）與 `WorkerPoolTests.cpp`（7 案）。`RoleThread` 不改名（同詞異義表與 N2，宣告 T2）。
5. **文件**：io.md、threads.md 照 gyo2 現況重寫（DI 5 刪除、DI 6 新增、threads DI 10～12、兩邊的 RC）；os.md 的 Files 一節寫明檔案呼叫沒有共享的可變狀態（io DI 6 的依據）；rebuild-concept C8 改寫。

## 驗證與經過

- **本機**（macOS arm64，preset `test`）：`ctest -L 'cpu|shader'` 169/169 通過（`WorkerPool` 加進去之前跑過一次；最後一次見下方「最後的確認」）。`gyo_threads_tests` 的 `WorkerPool*` 連跑 30 次、`gyo_io_tests` 連跑 20 次，都沒有失敗。
- **突變**（照 D184 只跑相關的）：
  - os-subsystem 清單的 io 突變與 `os-g4-io-opens`：33/33 killed，1 個只在 Windows 跑、本機略過。
  - framework-rework 的 io_user_files：14/14 killed，1 個略過。
  - orchestration 的 `cr-io-links-more`、`cr-io-os-public`：2/2 killed。
  - `wp-*` 五個：5/5 killed（處理敵對式檢查之後重跑）。
  - 測試執行檔改名後 io 突變的 target 全部改了，所以這兩份 io 清單整份重跑，而不是只挑 find 在改動範圍裡的。
- **走查**（判定 3）：`removed_scan.py` 在核准版 74d38577 命中 333 處（陽性對照），在 d3bb8f09 命中 0 處。第一次在 1ea0a7e8 命中 1 處（`PathUtils.cpp` 一行註解提到 `NormalizeSlashes`），已改。
- **`WorkerPool` 的突變清單敵對式檢查**（Fable 5.2／high，§14.2）：必修 1、應修 5、輕微 6。
  - 必修：停止的兩個案沒確認第一個工作已開始，正確實作也可能失敗。改成工作先回報「開始了」再停止。
  - 應修：補「Discard 不丟」與先進先出的案和突變；Drain 改成在停止的一方記下返回時已跑完的件數；`WaitUntilRefused` 加上限；解構案只守「等跑到一半的工作」，「丟掉排著的」寫明沒有守衛。
  - 輕微裡的 priority 沒傳下去、`Start` 中途失敗的回收：沒有讀回或注入的入口，寫進 threads.md 的 Contracts，標明沒有守衛、靠審查。「不得從兩條執行緒同時 `Stop`」寫進條文。「`wp-discard-returns-early` 在 stopper 被搶占時可能存活」是極小的競態，記在這裡、不另處理。
  - 宣告 10.2-6 寫「DI 10 一個、DI 11 兩個、DI 12 沒有」。實際是 DI 10 一個、DI 11 三個（多了 `wp-discard-keeps-queued`，README 第 3 條：Drain 與 Discard 是兩個獨立的決定），另加 Contracts 的先進先出一個（`wp-last-in-first-out`）。都是只締緊。
- **順路發現、已轉主會話**：orchestration 清單約第 3526 行有一份 `or-exch-round-after-failed-in` 的舊副本黏在下一個物件裡。主會話查證後指出現行版另外存在、也有執行過（`build/evidence/gyo2-exchange/c19-mutants.log`），只是殘留文字；由下一個改那份清單的批刪除。本卡最初回報「從沒執行過」是誤判，已向主會話更正。本卡改那份清單時用文字取代，沒有動到那一段。

## 測試判定的變更（§14.4）

- **io 連結檢查**（守 O-6）：期待從 {Base, Math, OS} 改成 {Base, OS}。1～5 在宣告第 10.2 節第 4 項，隨宣告核准。`cr-io-links-more` 改寫 find／expect；`cr-io-math-public` 改名 `cr-io-os-public`，改成「`GYO::OS` 外露」。
- **一般測試**：
  - `UriTests`「schemes other than file keep their parse」：`Asset` 改成 `Unknown` 加上 `schemeText == "asset"`，沒有緩的面。
  - 刪掉 loose parser、掛載的案例；其中「`file` scheme 不分大小寫」原本只在 loose 的案裡測到，改用 `ParseUri` 另立一案留下。
  - `NativeFileSystemTests`「says what it supports」：拿掉 `Copy`／`List`／`Iterate`／`CreateWatcher`／遞迴 `Remove` 與能力表的判定，開檔模式的判定留下。
  - `ProbeFileStamp` 超過上限的案：多檢查錯誤碼是 `TooLarge`（締緊）。
  - 新增：`ReadAllBytes` 的 `TooLarge` 案（兩條路徑）；DI 6 的煙霧測試（不當判定）。
  - 拿掉的判定守的性質隨元件一起消失，不移到別處。

## 突變的增減（§14.5、README 第 8 條）

- **退役 5 個**（守的程式被拿掉）：`io-m6-loose-fallback`、`io-m7-mount-escape`、`io-n1-caps`、`io-x3-remove-recursive-accepted`、`io-x5-move-not-advertised`。
- **改寫或改目標**：`os-g4-io-opens`（目標檔從被拿掉的 `Vfs.cpp` 改到 `NativeFileSystem.cpp`）；`io-x4`、`io-x7`（`Remove` 拿掉 options）；`cr-io-*` 兩個；io 突變的 target 改成 `gyo_io_tests`（約 50 個，只動欄位）。
- **新增 6 個**，每個各自捕一個別的突變捕不到的誤り，`guards` 都已寫明：
  - `io-y1-too-large-as-read-failed`：I6b。
  - `wp-submit-after-stop`：threads 10。
  - `wp-drain-drops-queued`、`wp-discard-keeps-queued`、`wp-discard-returns-early`：threads 11。
  - `wp-last-in-first-out`：先進先出。
- 這批改了 4 條 Design Invariants（threads 10、11、12，io 6），上限是 20 個，6 個在範圍內。

## 合併前的設計審查（Fable 5.2／high，唯讀）

設計上沒有必修。程序上的必修 1 件：本節之後的「最後的確認」原本是佔位。應修 5、輕微 9，處理如下：

- **應修**：
  - `WaitUntilRefused` 的上限從次數改成時間（10 秒）並 yield。原因是 CI 的 `ctest --parallel` 下，stopper 可能排不到。
  - threads.md 寫明工作者用 `RoleThread` 的例外與代價：body 看的是池、不是 `RoleContext`；每條執行緒多一個不用的 Waiter。RC 加一條「拆出執行緒啟動」。`Start` 中途丟例外的情況寫進註解。
  - os.md 第 30～31 行還寫 io 持有 `Vfs` 與 streams，已改。
  - io.md、threads.md 引用別份文件的節號時，補上檔名與分支。
  - dev_log 的案數改正（7 案），證據位置寫明是哪一台機器。
- **輕微，有處理的**：
  - `StopMode` 收進 `WorkerPool::` 底下。
  - threads.md 補兩句：Linux 名字的截斷、丟掉的工作在呼叫 `Stop` 的執行緒上解構。
  - io DI 6 的標題改成「call」，涵蓋所有呼叫。
  - io.md、`Uri.cpp`、`FileInfo.hpp` 裡 master 時代的殘留註解。
- **輕微，不在本批**：同一判準下沒人用、但宣告沒列的 `IOFwd.hpp`、`IFileSystem::Name()`、`FileInfo` 的 ctime／atime／permissions、`NormalizeOptions` 的旗標，列進後續。`ThreadCount()`、`ThreadName()`、`priority` 超出宣告列的介面：`ThreadName` 給測試用，`priority` 照 `RoleThread` 帶下去，留下。
- 處理之後：`wp-*` 重跑 5/5 killed，`gyo_threads_tests` 連跑 30 次都過。

## 最後的確認

- 本機（合併 gyo2 #273、#274 之後）：全量建置通過；`ctest -L 'cpu|shader'` 169/169。
- L1 三平台、四份清單的完整突變重跑：PR 上跑，結果補在這裡。

## 後續（不在本批）

- **存檔目錄**（Q3 (a)）：os 層另開一個小批，由主會話排。
- **asset 卡**（已經主會話轉告）：asset 宣告 5.12 引的 `MountPoint.hpp`、`Vfs.hpp` 行號失效；可以改用 `ReadAllBytes`（有 `TooLarge` 了）；產品沒給執行器時怎麼辦由 asset 卡定（AS-4 不允許在 World 執行緒上當場跑）。
- `OS::Files::DefaultCaseSensitivePaths` 在 io 拿掉能力表之後沒有 Engine 內的使用者（os 的測試仍在用），去留在 os 那邊看。
- 突變清單的路徑還指向 master 的計畫資料夾（HANDOFF 待辦「小批清理」#269）。
- io 裡同一判準下沒人用、宣告沒列的：`IOFwd.hpp`（沒有任何 include）、`IFileSystem::Name()`、`FileInfo` 的 ctime／atime／permissions、`NormalizeOptions` 的旗標（只用預設值）。下次動 io 時照「先問該不該存在」判。

## 證據

作者的 Mac（macOS arm64）上主 checkout 的 `build/evidence/gyo2-threads-io-review/`（git 不追蹤）：
- `inventory.py`、`inventory.txt`：設計段的使用者盤點。
- `os-files-shared-state.txt`：os 檔案呼叫的共享狀態掃描。
- `removed_scan.py`、`removed_scan.txt`：判定 3 的走查。
- `mut-*.log`：突變的執行紀錄。
