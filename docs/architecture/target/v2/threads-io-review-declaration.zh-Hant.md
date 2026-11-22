# threads、io 重審：報告與事前宣告（`v8-arch-gyo2-threads-io-review`）

更新：2026-11-22（修訂 1：Fable 5.2／high 敵對式檢查的必修 5、應修 4、輕微 6 全部處理，對照在第 13 節）。Owner：架構（GYO2 主會話開的任務卡 `local_a184932c`）。**狀態：待使用者核准動工（交使用者的是第 7 節 Q1～Q3 與第 8 節的 Design Invariants 變更）。**

批次表第 6 組「threads、io 重審」（`v8-arch-gyo2-threads-io-audio-review-start`）。R0（`v8-arch-rebuild-r0-approved`）把 `engine/core/threads`、`engine/core/io` 照 master 帶過來、標「帶過去並重審」，重審點在 [R0 清單](rebuild-r0-carry-list.zh-Hant.md)附錄 A.1。

**用詞**：照日誌、trace 的宣告，即設計段／實作段、動工前核准／合併前核准、第 N 步。標「**卡片照 L17 判**」的，是這張卡照命名規範、別家原始碼與已定的原則自己判的，**不是使用者的話**。交使用者定的只在第 7 節與第 8 節。

---

## 0. 結論

- **threads**：`RoleThread`（一條長命、具名、有自己的 Waiter、合作式停止的執行緒）**照舊留下，名字也不改**。日誌的寫檔執行緒今天在用它；runner 的執行緒（O-13）之後也可以用它。同詞異義表已經把 Role 定成通用的「執行緒角色」，N2 也規定帶過來的詞保留原名。**asset 要的工作執行緒池**踩中 threads 的 Replacement Criteria 1，但這會推翻 rebuild-concept C8 的「worker pool 第一版不做」，所以交使用者（**Q1**）。
- **io**：
  - **留下**：asset、工具、存檔用得到的部分，包括 `IFileSystem`、原生後端、`file://` 對應、整檔讀、原子寫、使用者檔案的輔助。
  - **拿掉**：master 與 gyo2 都沒有使用者、也沒有點名 Feature 的部分，包括檔案監看、目錄列舉、複製、能力表、五個 Stream 類、非原子的整檔寫、`UriScheme` 的 `Asset`／`Http`／`Https`、沒人呼叫的路徑函式。
  - **`Vfs`／`MountTable`／`MountPoint`**：asset 的實作與宣告都已經不用掛載。拿掉它們會連帶刪掉 DI 5，所以交使用者（**Q2**）。
  - **補上**：整檔讀超過上限時，回一個能和其他讀取失敗分開的錯誤碼。asset 今天因為分不出「太大」，自己另寫了一個讀迴圈。
  - **寫明**：多執行緒同時讀的契約（新 DI 6），依據是現有程式沒有共享狀態，不立守衛。
  - 拿掉之後，io 不再依賴 `GYO::Math`。
- **存檔目錄**（每個使用者的資料目錄）：讀檔卡請本卡問使用者要不要補（**Q3**）。
- 其他卡在用的介面不變：讀檔卡用到的 `WriteFileAtomically`、`ReadAllBytes`，asset 用到的 `NativeFileSystem`、`NativeFileUri`、`IStream`。Q1、Q2 與「太大」的錯誤碼會影響 asset 卡，結論出來後經主會話轉過去。

---

## 1. 讀過的資料

| 資料 | 用在哪裡 |
|---|---|
| [批次表](rebuild-batch-plan.zh-Hant.md) 第 6 組（主會話分支 `claude/gyo2-time-design` 的版本，那一列尚未併進 `gyo2`） | 範圍、依賴 |
| [R0 清單](rebuild-r0-carry-list.zh-Hant.md)附錄 A.1 threads、io 兩列 | 重審點：role 的命名、`PriorityHint`、和 runner 是否相合；Vfs／MountTable／IFileWatcher 是不是資產模型要的詞彙；另立 target |
| HANDOFF `v8-arch-rebuild-r0-approved`、`v8-arch-gyo2-plan-gaps-2026-11-21`（主會話分支） | 帶過來與補列的經過 |
| [同詞異義表](rebuild-vocabulary.zh-Hant.md) Role 列（第 141、297 行）、[命名規範](rebuild-naming.zh-Hant.md) N2、N5、N13～N17 | T2 |
| [rebuild-concept](rebuild-concept.zh-Hant.md) C8（第 35 行）、[圖的檢查](rebuild-diagram-review.zh-Hant.md)第 67 行 | Executor 的三種落點：主執行緒、專用執行緒（Dedicated Thread）、worker pool；「worker pool 沒有 Feature 需要，第一版不做」 |
| [編排層](orchestration.zh-Hant.md) O-3、O-12、O-13、O-15 | runner 擁有迴圈與執行緒的安排；World 整個交出；入口的形在 PvP Client 移植那批 |
| [交換點宣告](exchange-declaration.zh-Hant.md) E-5、E-15、E-17、第 5.6 節對照表 | 「執行緒池由產品或 threads 的 Module 建」；「threads 重審照它重寫 `RoleThread` 的用途」；「多執行緒同時讀檔的契約不在本頁定」 |
| asset 宣告（`claude/gyo2-asset-585196`，55a14825 動工前核准）5.3、5.6（X-1～X-6）、5.12、5.14、第 20 節 S8 與 N4（第 1096、1104 行）；asset 工作樹未提交的 `engine/modules/asset/src/ReadWholeFile.cpp` | 第 4、5 節 |
| 讀檔宣告修訂 12 草案（`claude/gyo2-restore-impl-7e6bc6`）19.6（約第 560 行） | 存檔目錄的請求（Q3） |
| 網路 Module 宣告（`claude/gyo2-net-module-da73cd`）第 353～357 行 | 「threads／io 不必提供」「不依賴 `core/io` 的 stream」 |
| [threads.md](../../threads.md)、[io.md](../../io.md)（master 08566260 的條文） | 現行 Contract、DI、RC |
| 突變清單：`plans/os-subsystem/scripts/mutations.json`、`plans/time-threads-trace/scripts/mutations.json`、`plans/framework-rework/scripts/io_user_files_mutations.json`、`target/v2/scripts/orchestration_mutations.json` | 第 10.3 節 |
| master 的 `engine/threads`、`engine/io` 與它們的使用者 | 只看當時為什麼這樣做 |
| 參考引擎（`~/Code/GitHome`） | 第 6 節 |

證據放在主 checkout 的 `build/evidence/gyo2-threads-io-review/`（git 不追蹤）：
- `inventory.txt`：每個符號在 master 與 gyo2 有哪些使用者。
- `inventory.py`：產生 `inventory.txt` 的腳本。
- 走查方法：用 Python 跑 `git grep -E`，終了碼 0、1 以外都視為失敗。
- 陽性對照：`ReadAllBytes` 命中 master 的 5 個工具檔；`Vfs` 命中 gyo2 的 `VfsTests.cpp`。
- 照 §14.2 記下一次失誤：第一次用 `\bVfs\b` 時，`\b` 在 git 的 ERE 不成立，結果被吞成零命中；改寫正規式後才命中。
- 檢查者對 origin/master 3b411e21 另外重跑過，「零使用者」的結論相同。

---

## 2. 現況（事實）

### 2.1 threads

- `engine/core/threads` 有 `RoleThread`、`RoleContext`、`RoleThreadOptions`、`ThreadPriority`、`ThreadError`（`RoleThread.hpp` 103 行、`.cpp` 118 行）。連結：`GYO::Time` 公開，`GYO::OS` 私有。
- gyo2 上的使用者：
  - 日誌 `JsonLinesFileSink` 的寫檔執行緒（`engine/modules/logging/src/JsonLinesFileSink.cpp:102,250`）。
  - `tests/common/os/ThreadTests.cpp`。
  - 編排層的 runner 和其他 Module 都沒有用。
- 系譜：TT-1（v7 執行緒分離）。後來同詞異義表把 Role 定成通用的「一條有名字、有固定職責的執行緒」。

### 2.2 io

| 部分 | master 的使用者（io 之外） | gyo2 的使用者（io 之外） | 測試 |
|---|---|---|---|
| `IFileSystem`、`NativeFileSystem`、`NativeFileUri` | 9 處（工具 5、PvP、asset、imgui shell） | asset 工作樹（未提交）的 `ReadWholeFile.cpp` | `NativeFileSystemTests` |
| `IStream`、`FileOpenMode` | 經上面 | 同上 | 有 |
| `Uri`、`file://` 對應 | asset 的路徑解析 | 經 `NativeFileUri` | `UriTests`（DI 3 的守衛） |
| `ParseUriLoose` | asset 的路徑解析 | 只有 `MountTable` | `UriTests` |
| `ReadAllBytes`、`ReadAllText` | 7 處、1 處（asset catalog） | 無（asset 因為分不出「太大」而自己寫讀迴圈）；讀檔宣告寫作者讀存檔用 `ReadAllBytes` | 有 |
| `WriteFileAtomically`、`ProbeFileStamp`、`IsNativePathWithin` | 4、2、2 處（工具） | 讀檔宣告寫作者寫存檔用 `WriteFileAtomically` | `UserFilesTests` 等 |
| `Vfs`、`MountTable`、`MountPoint` | **無**（只出現在 CMake 清單） | 無；asset 宣告第 20 節 S8：「不再有來源名…疊掛載不再需要」 | `VfsTests`（讀的疊掛）、`UriTests`（掛載不出根，DI 5） |
| `IFileWatcher`、`DirectoryIterator`、`List`、`Iterate`、`Copy`、遞迴 `Remove`、`FileSystemCapabilities` | 無 | 無 | 只有「回 NotSupported」與「能力表照實」 |
| `BufferedStream`、`MemoryStream`、`SpanStream`、`StreamReader`、`StreamWriter` | 無 | 無（網路宣告：不依賴 io 的 stream） | Buffered、Reader 各 2 案；Memory 在測試裡當假資料 |
| `WriteAllBytes`、`WriteAllText` | 無 | 無 | 無 |
| `UriScheme::Asset`／`Http`／`Https` | 無 | 無 | `Asset` 有 1 案 |

- 檔案：`.hpp` 30 個、`.cpp` 17 個；其中 7 個 `.cpp` 是只有 namespace 的空殼，實作在標頭裡。
- **缺陷**（檢查者用探針確認，本卡讀碼確認）：`ParseUriLoose` 對未知的 scheme 不保留 `schemeText`（`Uri.cpp:326-336`），所以 `ToString` 會印成 `://…`。`MountTable::Candidates` 要求 scheme 不是空的，結果是**今天只有 `asset`、`http`、`https` 這三個 scheme 掛得上**；`content://` 這類 scheme 候選數是 0。
- 連結：`GYO::Base` 公開；`GYO::Math`（只有 Stream 類用到 `Math::Min`／`Max`）與 `GYO::OS` 私有。R0 清單說的「另立 target」已在 R0b 完成（`GYO::IO`）；io.md 還寫著「compiled into GYO::Engine」。連結的檢查在 `tests/common/core/CMakeLists.txt:37`（`ExpectLinks.cmake` 要求完全相等），守它的突變是 `cr-io-links-more`、`cr-io-math-public`（`orchestration_mutations.json`，守 O-6）。
- 測試執行檔叫 `engine_tests`，這是舊名；依 N14 應叫 `gyo_io_tests`。

---

## 3. threads 逐項判定

| # | 項目 | 判定 | 理由 |
|---|---|---|---|
| T1 | `RoleThread` 這個機制 | **留下** | 日誌今天就要一條長命執行緒。O-13 的 runner 要把整個 World 交給另一條執行緒；Bevy 的 pipelined 渲染就是一條具名的長命執行緒（`bevy_render/src/pipelined_rendering.rs:181-182`），Unreal 的 `FRunnableThread`、Godot 的 `Thread`〔知〕也是。它就是 C8 說的「專用執行緒」落點，和產品類別無關 |
| T2 | 名字 `RoleThread`、`RoleContext`、`RoleThreadOptions` | **不改**（卡片照 L17 判；修訂 0 原本提議改名，現撤回） | 同詞異義表已把 Role 定成通用的「執行緒角色」，例子就是 `RoleThread`（第 141 行）；新概念遇到這個詞要讓開（第 297 行）；N2 也規定帶過來的詞保留原名。所以 R0 擔心的「role 出自 v7」已由詞表處理，改名反而推翻詞表 |
| T3 | `ThreadPriority`（三級提示）與 `OS::Thread::PriorityHint` | **留下** | Godot 有 LOW／NORMAL／HIGH 三級〔知〕，SDL 有 LOW／NORMAL／HIGH／TIME_CRITICAL〔知〕。兩個列舉的值一一對應是刻意的：threads 的使用者拿不到 os 的標頭（DI 9） |
| T4 | 自己的 Waiter、合作式停止、`WaitBackend()` | **留下** | 日誌的迴圈用 `Wait`／`Notify`；精確等待歸 time 管 |
| T5 | runner 的執行緒 | **不改程式**；threads.md 寫明「**可用於** runner 的執行緒」 | O-3、O-13：runner 建執行緒、也由 runner 收；入口的形在 PvP Client 移植那批定，現在沒有實作。交換點宣告第 5.6 節要求「照它重寫用途」，所以只改文件 |
| T6 | 工作執行緒池 | **交使用者（Q1）** | 踩中 threads.md 的 RC 1，但牴觸 C8 的「第一版不做」 |

---

## 4. io 逐項判定

**基盤與判準。**
- io 的公開介面是 Engine 的公開 Interface，依 AGENTS §2 屬**基盤**。它上面會疊 asset、工具、作者的存檔程式、日後的 pack 後端。所以留下的部分要寫出「根據 → 要求」（4.1）。
- 拿掉的判準（卡片照 L17 判）：master 與 gyo2 都沒有使用者、也沒有點名的 Feature。拿掉之後，日後要在哪裡、怎麼加回，寫進 Replacement Criteria（第 8.2 節）。

### 4.1 根據 → 要求（留下的部分）

| 根據（出處） | Feature／Product | 對 io 的要求 | 對應 |
|---|---|---|---|
| asset 宣告 R1、5.6、5.12（核准 55a14825） | 任何有內容的遊戲（①②③）：關卡進場讀模型、貼圖、catalog | 從 World 以外的執行緒讀整檔、帶位元組上限、能分出「太大」；多條執行緒同時讀 | `NativeFileSystem`、`ReadAllBytes`、I6b、DI 6 |
| 讀檔宣告修訂 12 草案 19.6 | 玩家存檔（①②③） | 原子寫（壞檔是最嚴重的 Bug）；整檔讀 | `WriteFileAtomically`、`ReadAllBytes` |
| master 的 4 個工具（R3 搬回） | 工具存讀使用者檔案 | 原生路徑 ↔ file URI；原子寫；變更偵測；「寫在根目錄內」的判定 | `NativeFileUri`、`IsNativePathWithin`、`ProbeFileStamp` |
| io DI 1～4（與產品無關的 Contract） | 跨平台的檔案路徑 | UTF-8、單一 `file://` 對應、不加長路徑前綴 | `Uri` 的 file 部分 |

### 4.2 逐項

| # | 項目 | 判定 | 理由 |
|---|---|---|---|
| I1 | `IFileSystem`＋`NativeFileSystem`＋`NativeFileUri` | **留下**，介面縮成 I3 拿掉之後剩下的操作 | 檔案存取的唯一出口：後端以 `IFileSystem` 注入。Bevy 的 `AssetReader` 也是一個 trait、由各後端實作（`bevy_asset/src/io/mod.rs:196`） |
| I2 | `Uri` 的 file 部分（`ParseUri`、`FileUriFromNativePath`、`NativePathFromFileUri`）、`Path`、`PathUtils` 裡有呼叫者的函式 | **留下** | 4.1 最後一列 |
| I2b | `PathUtils` 中沒有呼叫者的公開函式：`ReplaceExtension`、`NormalizeSlashes`、`IsAbsolutePathLike`、`StripSchemeLoose`、`JoinRootAndRelative`；`Path` 中沒有呼叫者的成員：`Filename`、`Extension`、`Join`、`operator/`、`IsAbsoluteLike`、`HasTraversal`、`HasNullByte`。Q2 選拿掉時，再加上只有 `MountTable` 在用的 `ContainsTraversal` | **拿掉**（卡片照 L17 判）。`PathUtils.cpp` 內部還在用的（例如 `IsSlash`）改成檔內函式 | 走查結果記在證據裡 |
| I3 | `IFileWatcher`＋`CreateWatcher`；`DirectoryIterator`＋`Iterate`；`List`＋`DirectoryEntry`＋`ListOptions`；`Copy`；`RemoveOptions`（遞迴刪除）；`FileSystemCapabilities`＋`Capabilities()` | **拿掉** | master、gyo2 都沒有使用者，原生後端全部回 `NotSupported`。熱重載、檔案瀏覽都沒有點名的 Feature（asset 5.12 寫「本批不用」）。能力表只用來事先查「哪些不支援」；這些操作拿掉之後，後端不支援的操作仍回 `NotSupported` |
| I4 | `IStream`、`FileOpenMode`、`Seek` | **留下** | `Open` 回的就是 `IStream`；asset 的讀迴圈也用它 |
| I5 | `BufferedStream`、`MemoryStream`、`SpanStream`、`StreamReader`、`StreamWriter` | **拿掉** | 沒有使用者。asset 的 loader 吃整檔位元組、不需要串流（asset 決定 8）；網路自己用 `std::span` 讀寫、不依賴 io 的 stream（網路宣告第 356 行）。測試裡當假資料的 `MemoryStream` 改成測試自己的小假物件 |
| I6 | `ReadAllBytes`、`ReadAllText`、`WriteFileAtomically`、`ProbeFileStamp`、`IsNativePathWithin` | **留下** | 4.1 |
| I6b | `ReadAllBytes` 超過 `maxBytes` 時的錯誤碼 | **改**：`IOErrorCode` 加 `TooLarge`，超過上限回它，不再回 `ReadFailed`。`ProbeFileStamp` 的「和 `ReadAllBytes` 一樣失敗」跟著改。卡片照 L17 判 | asset 為了分出「太大」，在 `ReadWholeFile.cpp` 自己寫了一份讀迴圈，同一個機制分散成兩份。補上之後 asset 可以直接用 `ReadAllBytes`；要不要改用由 asset 卡判，經主會話轉告。gyo2 上現在沒有人依賴 `ReadFailed` 這個舊行為 |
| I7 | `WriteAllBytes`、`WriteAllText`（非原子的整檔寫） | **拿掉** | 沒有使用者；存檔與工具都該用原子寫（io.md：「原子寫只有一個」） |
| I8 | `Vfs`、`MountTable`、`MountPoint`，以及 `ReadAllBytes`／`OpenRead`／`OpenWriteTruncate` 的 `Vfs&` 版本、`ParseUriLoose` | **交使用者（Q2）**，建議拿掉 | 會刪掉 DI 5 |
| I9 | `UriScheme::Asset`、`Http`、`Https` | **拿掉**，只留 `None`、`File`、`Unknown`；其他 scheme 的名字放在 `schemeText`（`ParseUri` 今天就是這樣做） | N5：core 裡出現 `Asset`，指的是 asset 的概念；`Http`／`Https` 沒有 Feature。Q2 選留下時，要同時修 2.2 的缺陷（見 Q2） |
| I10 | 多執行緒同時讀的契約 | **寫明**：新 DI 6（第 8.2 節），**不立守衛**，只放煙霧測試 | 照 Bevy：`AssetReader: Send + Sync`（`io/mod.rs:196`）。現況相符：`NativeFileSystem` 沒有成員；os 的 `Files` 沒有共享的可變狀態（本卡讀碼與檢查者掃描；os.md 沒有承諾這點，所以 DI 6 也寫進 os.md 的 Files 一節）。不加鎖，誤用交審查 |
| I11 | 位置與 target | **不動**：`engine/core/io`、`GYO::IO`，不是 Module | io.md 的「compiled into GYO::Engine」改成現況 |
| I12 | 測試執行檔 `engine_tests` | **改名** `gyo_io_tests`（N14）；io 突變的 `target` 跟著改（約 56 個，只動欄位）。卡片照 L17 判 | 舊名是 master 的 `GYO::Engine` 時代留下的 |
| I13 | 7 個空殼 `.cpp` | **拿掉**。`os-g4-io-opens` 的目標從 `Vfs.cpp` 改成 `NativeFileSystem.cpp`（`find` 是同一行 namespace，不用改） | 照 §14.5 改目標，不另立新突變 |

---

## 5. 和其他卡的關係

| 卡 | 它用到 io／threads 的什麼 | 本批之後 |
|---|---|---|
| asset（`gyo2-asset`，實作中，工作樹未提交） | `NativeFileUri`、`NativeFileSystem::Open`、`IStream`；自寫讀迴圈；不用 `Vfs`、`MountPoint`、`ReadAllBytes`；5.6 的 X-1 執行器 | 用到的介面都留下。經主會話轉告三件事：Q1 的結論；Q2 的結論（asset 5.12 引的 `MountPoint.hpp`、`Vfs.hpp` 行號會失效）；I6b 補了 `TooLarge`，可以改用 `ReadAllBytes` |
| 讀檔＋存檔版本（`gyo2-restore-impl`） | 宣告 19.6：作者讀寫存檔用 `WriteFileAtomically`、`ReadAllBytes`；存檔目錄由本卡問使用者 | 兩個 API 留下（已回覆，主會話轉告）；存檔目錄是 Q3 |
| 網路 Module（`gyo2-net-module`） | 不用 threads、io（宣告第 353～357 行） | 無影響；它提到的 `SpanStream` 會被拿掉，要用時照 datagram 的需要重新設計 |
| audio 重審（`gyo2-audio-review`） | 不用 | 無影響 |
| 日誌（目前沒有在跑的卡） | `RoleThread` | 不改名，無影響 |

兩邊誰後合併，誰就用 merge 跟上 `gyo2`。

---

## 6. 和別家的對照

| | 長命的具名執行緒 | 工作池 | 檔案抽象 | 多執行緒讀 |
|---|---|---|---|---|
| Bevy〔源〕 | 渲染執行緒用 `std::thread::Builder::new().name("Render thread")`（`pipelined_rendering.rs:181`） | `bevy_tasks` 有三個全域池（`usages.rs`）；`TaskPoolPlugin` 依核心數比例分配（io、async_compute 各 25%，`task_pool_plugin.rs:121-133`）；執行緒名是 `<name> (<i>)`（`task_pool.rs:39`） | `AssetReader` trait；`FileAssetReader` 只有一個 `root_path`（`io/file/mod.rs:40`）；具名來源 `AssetSourceId`；沒有疊掛；放在 bevy_asset 裡，沒有通用的 VFS | `AssetReader: Send + Sync` |
| Godot〔知〕 | `Thread`，三級優先權 | `WorkerThreadPool`，一個全域池 | `FileAccess`、`res://`（可疊 PCK）、`user://` | — |
| Unreal〔知〕 | `FRunnableThread::Create(name, priority)` | `FQueuedThreadPool`、TaskGraph | `IPlatformFile` 一層包一層 | — |
| Unity〔文〕 | 引擎內部 | Job System 的工作執行緒由引擎管 | `System.IO`、`persistentDataPath` | — |
| flecs〔源〕 | — | `ecs_set_threads`、`ecs_set_task_threads`（`addons/pipeline.h:231-245`） | — | — |
| master（GYO 舊） | `RoleThread` | 無 | `Vfs` 加 `IFileSystem`；`Vfs` 沒有使用者 | 沒寫 |
| **本批** | `RoleThread`（不改） | Q1 | `IFileSystem` 加原生後端；`Vfs` 看 Q2 | DI 6，照 Bevy |

**和別家不同、有取捨的地方**：
- GYO 不做全域池；Bevy、Godot 都是全域單例。原因是 GYO 一個行程裡可以有多個 Product，全域狀態和 O-15 的「Product 一份的共用處」衝突。所以若要有池，它是一個物件，由某人建、放進選項或共用處（Q1）。
- Q2 若選拿掉，GYO 和 Bevy 一樣沒有通用 VFS，但比 Godot 少了 `res://`／`user://` 的疊掛。

---

## 7. 交使用者的

### Q1 工作執行緒池要不要放進 threads（推翻 C8 的「第一版不做」）

- **遊戲的例子**：玩家進關卡，asset 要在背景讀一個 23.7 MB 的模型並解碼，World 照常跑幀。這條「背景的執行緒」由誰提供？
- **前提**：
  - asset 的執行器介面由 asset 自己定，asset 不依賴 threads（asset X-1，已核准）。
  - 執行緒的數量與歸屬由 runner 或產品定（X-4）。
  - 交換點宣告核准了兩條路：產品經 `ProductDescription::options` 給，或「之後的 threads Module」建進共用處（交換點 5.6 節 X-1、X-4 列）。
  - rebuild-concept C8：「worker pool 沒有 Feature 需要，第一版不做」——那時還沒有 asset 的需求。
  - asset 的 AS-4：讀檔與解碼在 World 執行緒之外（已核准）。
  - world_asset 預設是開著的（asset 宣告第 241 行）。
  - 網路宣告第 355 行：「本線不需要 threads 的新原語」。
- **選項**：
  - **(a) threads 提供 `WorkerPool` 類，由產品建**。
    - 介面：建立時給執行緒數（至少 1）與名字，執行緒名照 Bevy 取 `<name> (<i>)`；`Submit(工作)` 先進先出，停了之後回 false、不排入；`Stop(Drain)` 跑完排著的工作再收；`Stop(Discard)` 丟掉排著的、等跑到一半的做完再收；解構等於 `Stop(Discard)`。
    - 不做：future、優先權、偷工作、scope、全域單例。
    - 產品要背景執行緒時，自己建一個，經 `options` 交給 world_asset（交換點核准的第一條路）。
    - **產品沒給時 world_asset 怎麼辦，是 asset 卡和你的事**：不能在 World 執行緒上當場跑（違反 AS-4），所以只剩「啟動時報錯」或「world_asset 自己建」；後者是交換點沒核准的第三條路。
  - **(b) (a)＋一個預設開著的 Module**：在 `Configure` 建 Product 一份的池、放進共用處，執行緒數依核心比例分配（照 Bevy `TaskPoolPlugin`）。代價：每個 Product 預設都會開執行緒，包括測試、工具、無頭程式。
  - **(c) Engine 不提供池（C8 照舊）**：產品或測試自己寫執行器放進選項，Engine 沒有預設。
- **建議 (a)**：
  - 池是機制。放在 threads 一個出口，就不會由每個要背景的產品各寫一份（使用者原則：「機制不分散在各個實作裡」）。
  - 不預設開執行緒，單機與工具不多付成本（「單機不增複雜度」）。
  - 執行緒數由產品定，符合 X-4。
  - 不選 (b)：現在只有 asset 一個使用者，預設的 Module 是為單一用途加的一層。
  - 不選 (c)：每個產品都要自己寫池。
- **代價**：新元件約 150 行，外加測試與突變；threads 的 DI 多三條（8.1）；推翻 C8 的一句話（rebuild-concept C8 同一批改寫）。
- **留給 asset 卡的**：「產品沒給執行器」時是報錯還是別的。

### Q2 拿掉 `Vfs`／`MountTable`／`MountPoint`（連帶刪掉 io DI 5）

- **遊戲的例子**：做模組（mod）的遊戲，想讓 mod 的資料夾疊在原版內容上，同名的檔案以 mod 為準。這就是疊掛載。
- **前提**：
  - master 沒有使用者。
  - asset 卡的修訂 3 改成「一個內容根目錄、catalog 寫相對路徑」，宣告第 20 節 S8 寫「疊掛載不再需要」，實作也不用它。
  - R0 附錄的重審點正好是「Vfs、MountTable 是不是資產模型要的詞彙」。
  - DI 5「掛載不出根」只有 `MountTable` 一個守的地方。
- **選項**：
  - **(a) 拿掉**：連同 `ReadAllBytes`／`OpenRead` 的 `Vfs&` 版、`ParseUriLoose`、只有它們在用的路徑函式；刪掉 DI 5、突變 `io-m7-mount-escape`、`io-m6-loose-fallback`、`VfsTests` 與 `UriTests` 的掛載和 loose 案例。「內容根目錄 + 相對路徑不出根」的檢查由 asset 自己做（asset 5.3 的 `ContentPath`，它已經在做）。
  - **(b) 留讀取**：照舊留著，拿掉寫入路由與 `Vfs::Open` 的 mode 參數（改成只讀），並照 §14.3 修 2.2 的缺陷：先立一個 Red「`content://` 掛得上」，再讓 `ParseUriLoose` 保留 `schemeText`。
- **建議 (a)**：
  - 沒有使用者，Feature 也明說不需要；Bevy 的 asset 也沒有通用 VFS。
  - 留著要先修一個缺陷、再改寫入，等於替沒有人用的東西加工。
  - mod 疊掛是點名之後才做的 Feature；到時在 asset 的根目錄設定裡加，或另立元件（寫進 RC）。
- **代價**：DI 5 刪除（Design Invariants 節的刪除要你同意）；日後做 mod 疊掛要重新設計。

### Q3 存檔目錄（每個使用者的資料目錄）要不要補

- **遊戲的例子**：玩家按存檔，檔案該放哪裡？macOS 放 `~/Library/Application Support/<遊戲>`、Windows 放 `%APPDATA%\<遊戲>`、Linux 放 `$XDG_DATA_HOME/<遊戲>`。
- **前提**：
  - io、os 現在都沒有這個 API（`UserFiles.hpp` 只有 `ProbeFileStamp`）。
  - 讀檔宣告 19.6 請本卡問你。
  - os 的原意是 adaptor，OS 的需求一律落在 OS 層（使用者 2026-10-28）。
- **選項**：
  - **(a) 補在 os 層**：例如 `OS::Paths` 加「這個使用者的資料目錄」，另開一個小批，不在本批。
  - **(b) 不補**：作者自己決定存檔路徑。
- **別家**：Godot `user://`、Unity `Application.persistentDataPath`、Unreal `FPaths::ProjectSavedDir` 都內建〔知〕；Bevy 沒有，用外部 crate。
- **建議 (a)**：三家都內建，算「預設方便」；放在 os 層是因為它本身就是平台的差異。排在哪一批由主會話排。

### 不交使用者、卡片照 L17 判的

以下都是拿掉沒有使用者的東西、補一個錯誤碼，或照命名規範改名，沒有推翻你之前的決定：

- io 拿掉的項目：I2b、I3、I5、I7、I9、I13。
- I6b：補 `TooLarge` 錯誤碼。
- I12：測試執行檔改名 `gyo_io_tests`。
- I10：多執行緒讀的契約照 Bevy 寫。
- T2：`RoleThread` 不改名。

---

## 8. Design Invariants 與 Replacement Criteria 的變更（Architecture Delta，隨本宣告核准）

兩份文件照 gyo2 的現況重寫（owner 路徑、target、使用者），拿掉開頭「master 08566260 的條文」那段註記。

### 8.1 threads.md

- DI 1～9 不變。
- 「What it is」改寫：使用者是日誌的寫檔執行緒；可用於 runner 的執行緒（O-13），由 runner 建、由 runner 收，World 不綁執行緒。它是 C8 的「專用執行緒」落點。
- Q1 選 (a) 時加三條：
  - **DI 10 停了之後不收工作**：`Stop` 之後的 `Submit` 回 false，工作不跑也不留。
  - **DI 11 停止會收完執行緒**：`Stop` 返回時沒有工作在跑；`Drain` 在返回前跑完排著的工作，`Discard` 丟掉它們。
  - **DI 12 池不是全域的**：沒有單例，誰建誰擁有。由 API 的形狀保證，沒有守衛。
- RC 1 改寫：需要排程器、工作之間的依賴、優先權或 scope 時，另立元件或換掉 `WorkerPool`，不把 `RoleThread` 長大。(a) 時再加：第二個使用者要池時，提議改成一個 Module 統一建進 Product 的共用處。

### 8.2 io.md

- DI 1～4 不變。
- **DI 5**：Q2 選 (a) 時**刪除**；選 (b) 時不變。
- **新 DI 6 多執行緒讀**：
  - `IFileSystem` 的實作接受任何執行緒同時呼叫。
  - `Open` 回的 stream 同一時間只給一條執行緒用。
  - 依據：原生後端沒有成員；os 的 `Files` 沒有共享的可變狀態（os.md 的 Files 一節同步寫明）。
  - 守衛：**無**（交審查）；另有一個煙霧測試，不當判定。
- 拿掉 master 的 `IAssetSource`、asset resolver 那幾段（gyo2 沒有）；原生後端的描述拿掉已刪的操作；整檔讀超過上限回 `TooLarge`。
- **RC 1 改寫**：
  - 要列目錄、監看檔案、複製，或要第二個非原生來源（pack、memory）時，在 `IFileSystem` 上加那一個操作、同時加它的測試，不把能力表加回來代替。
  - Q2 選 (a) 時再加：要疊掛（例如 mod）時另立元件；不要在 `IFileSystem` 後端裡做優先順序。
- RC 2～4 不變。Q2 選 (a) 時，RC 4（非 file 的 scheme）改寫成「要非 file 的 URI 時另立型別」。

---

## 9. Architecture Delta（AGENTS §3 七項）

1. **為什麼要變**：
   - R0 標了「帶過去並重審」。
   - asset 5.6、5.12、交換點 E-15、E-17、讀檔 19.6 交了需求。
   - io 有大量沒有使用者的公開介面，留在基盤上，搬產品時會被當成 Contract 開始依賴。
2. **現在的問題**：見第 2 節，包括 `ParseUriLoose` 的缺陷，以及 asset 因錯誤碼分不清而複製了一份讀迴圈。
3. **變的邊界**：
   - io 的公開介面縮小。
   - 錯誤碼多一個。
   - DI 6 新增，DI 5 依 Q2 決定。
   - Q1 選 (a) 時，threads 多一個元件。
4. **影響的 Module／Product／Tool**：
   - io、threads、os（只改文件）、rebuild-concept C8（只有 Q1 選 (a) 時）。
   - asset 卡：Q1、Q2、I6b。
   - `gyo2` 上沒有產品。
5. **依賴方向**：io 少一條 `GYO::Math`；其餘不變。
6. **Ownership**：不變。
7. **為什麼不能更小**：
   - 不縮 io，沒有使用者的介面會繼續被當成 Contract。
   - 不補 `TooLarge`，asset 那份重複的讀迴圈會留下來。
   - 不寫 DI 6，asset 的工作執行緒同時讀檔就沒有根據。

---

## 10. 實作段（使用者說「開始」之後）

### 10.1 步驟、模型與檔位

這批屬於上段：觸及 Design Invariants 與公開介面。

| 步 | 做什麼 | 實作者 | 審查／檢查 |
|---|---|---|---|
| 1 | io 拿掉：I2b、I3、I5、I7、I9、I13，以及 Q2 (a) 的部分；拿掉 `GYO::Math`；測試執行檔改名；突變照 10.3 處理 | Opus 5.5／high（本卡） | — |
| 2 | I6b `TooLarge`；DI 6 的煙霧測試；io.md、threads.md、os.md 的 Files 一節、rebuild-concept C8（Q1 (a) 時）的文件 | Opus 5.5／high | — |
| 3 | （Q1 (a)）`WorkerPool`、測試、突變；**突變清單交別的模型做一次敵對式檢查**（§14.2） | Opus 5.5／high | Fable 5.2／high |
| 4 | dev_log；合併前的唯讀設計審查；開 PR（base `gyo2`） | Opus 5.5／medium | Fable 5.2／high |

### 10.2 判定（在看到結果之前宣告）

1. **測試**：本機（macOS arm64）`ctest -L 'cpu|shader'` 全過；PR 的 L1 三平台（linux-x64、windows-x64、macos-arm64）通過。
2. **相關突變全殺**（照 D184 只跑這些）：
   - io 留下的檔上受影響的突變，也就是 10.3 的「改寫」「改目標」兩欄，加上 `find` 落在改動範圍內、或殺它的測試被改過的那些。
   - `cr-io-links-more`、`cr-io-math-public`（改寫後）。
   - `os-g4-io-opens`（改目標後）。
   - I6b 的新突變：超過上限時回 `ReadFailed`，預期的失敗是 `ReadAllBytes` 新案例的 `TooLarge` 判定。
   - Q1 (a) 的新突變。
   - PR 合併前，在背景完整跑一次 os-subsystem、framework-rework、orchestration、time-threads-trace 四份清單。
3. **走查零命中**：拿掉的型別名與函式名（I2b、I3、I5、I7、I9；Q2 (a) 時再加 `Vfs`、`MountTable`、`MountPoint`、`ParseUriLoose`）在 `engine/`、`tests/`、`tools/`、`.github/`、`cmake/` 都不再出現。照 §14.2 用 Python 跑 `git grep`、分辨終了碼，改動之前先在現在的樹上命中一次。
4. **io 連結檢查的判定變更**（守 O-6 的守衛，照 §14.4 寫出 1～5，隨本宣告核准）：
   1. 守的性質：io 只連結它需要的單位，而且 os、math 不外露。
   2. 緩的面：期待集合從 {Base, Math, OS} 改成 {Base, OS}，「連了 {Base, OS}、少了 Math」從失敗變成通過。
   3. 締的面：「連了 Math」從通過變成失敗。
   4. 性質仍由同一個檢查守住：期待集合等於 io 實際要的連結，任何多連或少連都會失敗。
   5. 根據：Math 只有被拿掉的 Stream 類在用（第 2.2 節）。
   - 突變處理：`cr-io-links-more` 的 `find`／`expect` 改成新的連結集合（這是同一個錯誤，不另立新突變，§14.5）；`cr-io-math-public` 改成「`GYO::OS` 外露」，預期的失敗是「gyo_io exposes GYO::OS」。
5. **DI 6 的煙霧測試**：8 條執行緒對同一個 `NativeFileSystem` 各讀 200 次、3 個不同內容的檔，位元組全部正確。這個測試**不當守衛也不當判定**：沒有突變能讓它確定失敗；把共享狀態塞進後端，只在真的競爭時才會出錯，結果不穩定（§14.1）。
6. **Q1 (a) 的 `WorkerPool`**：
   - 測試涵蓋 DI 10、DI 11 與執行緒名。
   - 突變：DI 10 一個（停了之後仍收工作）；DI 11 兩個（`Drain` 不跑排著的工作、`Discard` 不等跑到一半的工作）；DI 12 沒有（由 API 的形狀保證）。
   - 預期的失敗寫成測試名。
   - 突變清單由別的模型做一次敵對式檢查。
7. **測試判定的變更**（照 §14.4；這些都是一般測試，寫明理由即可）：
   - `UriTests`「schemes other than file keep their parse」：`scheme == Asset` 改成 `scheme == Unknown` 加上 `schemeText == "asset"`。守的性質不變，還多檢查了 `schemeText`，沒有緩的面。這個案例不屬於 DI 3（DI 3 是 `file://`）。
   - 刪掉的判定，守的性質都隨元件消失，不移到別處：
     - `BufferedStreamTests`、`StreamReaderTests` 全部。
     - `NativeFileSystemTests`「says what it supports」裡 `Copy`、`List`、`Iterate`、`CreateWatcher`、遞迴 `Remove` 回 NotSupported 的判定，以及能力表的判定。
     - Q2 (a) 時，`VfsTests` 全部，以及 `UriTests` 裡掛載與 loose parser 的案例。
   - 留下的判定：
     - 同一案裡開檔模式組合回 NotSupported 的判定（讀＋建立、讀寫、覆寫既有檔等）留下。
     - 轉發到原生後端的測試用假 `IFileSystem`（`NativeFileSystemTests.cpp:449-458`）跟著介面縮小。
   - `ProbeFileStamp` 超過上限的案例：期待從 `ReadFailed` 改成 `TooLarge`，這是締緊。
   - io 的 DI 1～4 守衛不變；os 邊界守衛 R1～R4 以目錄涵蓋 `engine/core/io`，拿掉檔案不會縮小它的範圍。

### 10.3 突變的處理（照 §14.5）

| 處理 | 突變 |
|---|---|
| 退役（守的程式被拿掉） | `io-n1-caps`、`io-x5-move-not-advertised`、`io-x3-remove-recursive-accepted`；Q2 (a) 時再加 `io-m7-mount-escape`、`io-m6-loose-fallback` |
| 改寫 | `cr-io-links-more`、`cr-io-math-public`（10.2-4）；`io-n2-write-mode`、`io-w2-write-mode-loose`、`io-w3-windows-text-ignored` 只有在 `find` 觸及被拿掉的程式時才改 |
| 改目標 | `os-g4-io-opens` 改到 `NativeFileSystem.cpp`；io 的突變的 `target` 從 `engine_tests` 改成 `gyo_io_tests` |
| 新增 | I6b 一個；Q1 (a) 三個 |
| threads | 不改名，所以 threads 的突變（含 `os-g6-stale`）不動 |

### 10.4 停止條件

照 AGENTS §14 冒頭。另外，若發現 asset、讀檔、網路的分支已經開始用到要拿掉的介面，停下來，經主會話協調。

---

## 11. 不做的事、範圍的限界

- 不加 runner 的執行緒入口（O-13）；不做 asset 執行器的接線。
- 不做熱重載、目錄瀏覽、pack 後端、mod 疊掛。
- 不改日誌，不改 os 的程式（只改 os.md 的文件）；存檔目錄若補，在 os 那批（Q3）。
- 三份突變清單的路徑還指向 master 的計畫資料夾（HANDOFF 待辦「小批清理」#269）：本批只改清單內的欄位，不搬清單。
- 本機只有 macOS arm64；Windows 的 UNC 探針（`gyo_io_unc_probe`）靠 L1 跑。

## 12. 估時

- 設計段：約 4 h（已用）。
- 實作段（Q1 (a)、Q2 (a)）：8～11 h。
- Q2 改選 (b)：加 2 h（修缺陷、Red→Green）。
- Q1 改選 (b)：加 3～4 h；改選 (c)：減 3 h。

## 13. 處理對照（Fable 5.2／high 敵對式檢查）

| 意見 | 處理 | 位置 |
|---|---|---|
| B1 必修：`asset://` 照舊解析對 `ParseUriLoose` 不成立，非 asset 的 scheme 掛不上 | 讀碼確認是既有缺陷。Q2 (a) 時隨 `ParseUriLoose` 一起拿掉；選 (b) 時照 §14.3 先立 Red 再修 | 2.2、I9、Q2 |
| B2 必修：留下 `Vfs` 讀取沒有實際使用者，決勝理由是費用；asset 因錯誤碼分不清自寫讀迴圈 | 改成 Q2，建議拿掉；補 I6b `TooLarge` | I6b、I8、Q2 |
| B3 必修：Q1 選項不完整，(a) 的「world_asset 自建」是交換點沒核准的第三條路；網路沒有要池 | (a) 改成「產品建、經選項給」；寫明 AS-4 讓「當場跑」不成立、world_asset 預設開著的後果、C8 被推翻；拿掉「之後的網路」 | Q1 |
| B4 必修：漏了讀檔卡交來的存檔目錄 | 列成 Q3 | Q3、第 5 節 |
| B5 必修：漏 orchestration 清單的 `cr-io-*`；新加的 Math 突變與 `cr-io-links-more` 重複；「只締緊」不對；漏 `os-g6-stale`；退役的沒點名 | 10.2-4 照 §14.4 寫 1～5 隨宣告核准；不另立新突變，改寫 `cr-io-*`；10.3 點名；不改名所以 `os-g6-stale` 不動 | 2.2、10.2、10.3 |
| S1 應修：命名沒對過同詞異義表與 C8 | 撤回改名（T2） | T2、第 0 節 |
| S2 應修：DI 6 的守衛被高估；Vfs 只讀應拿掉 mode | DI 6 改成無守衛＋煙霧測試，寫明依據並同步 os.md；Vfs 的 mode 寫進 Q2 (b) | I10、8.2、10.2-5、Q2 |
| S3 應修：沒照基盤處理；PathUtils 名單要先列；RC | 加 4.1 根據 → 要求；I2b 列名單；RC 寫加回的位置 | 第 4 節、8.2 |
| S4 應修：WorkerPool 突變的別模型檢查與數量 | 10.1 第 3 步加檢查；10.2-6 寫實際數量 | 10.1、10.2-6 |
| 輕微：.hpp 數、inventory 的 master、Vfs 的其他操作、走查範圍、`os-g4` 的 find、T5 的「可用於」 | 30 個；記下 origin/master 也已重跑；Vfs 的處理併入 Q2；走查範圍加 tools、.github、cmake；`os-g4` 改指 `NativeFileSystem.cpp`；T5 改寫 | 2.2、1、Q2、10.2-3、I13、T5 |

沒能查證的：Godot、Unreal、Unity 標〔知〕〔文〕的說法；Windows 檔案呼叫的執行緒安全只做了靜態掃描。
