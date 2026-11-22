# threads、io 重審：報告與事前宣告（`v8-arch-gyo2-threads-io-review`）

更新：2026-11-22（修訂 0）。Owner：架構（GYO2 主會話開的任務卡 `local_a184932c`）。**狀態：草稿，待敵對式檢查，之後交使用者核准動工。**

批次表第 6 組「threads、io 重審」（`v8-arch-gyo2-threads-io-audio-review-start`）。R0（`v8-arch-rebuild-r0-approved`）把 `engine/core/threads`、`engine/core/io` 照 master 帶過來、標「帶過去並重審」，重審點在 [R0 清單](rebuild-r0-carry-list.zh-Hant.md)附錄 A.1。

**用詞**：照日誌、trace 的宣告，即設計段／實作段、動工前核准／合併前核准、第 N 步。標「**卡片照 L17 判**」的，是這張卡照命名規範、別家原始碼與已定的原則自己判的，**不是使用者的話**。只有第 7 節是交使用者定的。

---

## 0. 結論（一段）

- **threads**：`RoleThread` 這個機制（一條長命、具名、自己有 Waiter、合作式停止的執行緒）**留下、改名**：`role` 是 v7 PvP 的 sim／net／render 角色，名字指的是產品的概念（N5、N6）。新名字 `DedicatedThread`。日誌的寫檔執行緒今天就在用它；runner 的執行緒（O-13）之後也用它。`ThreadPriority` 留下。**asset 要的工作執行緒池（X-1、X-4、X-6）踩中 threads 的 Replacement Criteria 1**，要不要現在在 threads 加一個薄的 `WorkerPool`，交使用者（第 7 節 Q1）。
- **io**：留下 asset、工具、存檔會用到的部分（`IFileSystem`、原生後端、`file://` 對應、`Vfs` 的讀取疊掛、整檔讀、原子寫、使用者檔案的輔助）；**拿掉 master 與 gyo2 都沒有使用者、也沒有點名 Feature 的部分**（檔案監看、目錄列舉、複製、能力表、五個 Stream 類、非原子的整檔寫、`Vfs` 的寫入路由、`UriScheme` 的 `Asset`／`Http`／`Https`）；補 asset 5.12 要的**多執行緒同時讀的契約**（新的 DI 6）。拿掉之後 io 不再依賴 `GYO::Math`。
- 其他卡在用、或宣告裡寫了的介面（`ReadAllBytes`、`MountPoint`、`Vfs` 的讀取疊掛）都不變，不需要經主會話協調改介面；只有 Q1 會影響 asset 卡的 world_asset 怎麼拿預設的執行器，結論出來後經主會話轉給 asset 卡。

---

## 1. 讀過的資料

| 資料 | 用在哪裡 |
|---|---|
| [批次表](rebuild-batch-plan.zh-Hant.md) 第 6 組（主會話分支 `claude/gyo2-time-design` 的版本，那一列尚未併進 `gyo2`） | 範圍、依賴 |
| [R0 清單](rebuild-r0-carry-list.zh-Hant.md)附錄 A.1 threads、io 兩列 | 重審點：role 的命名、`PriorityHint`、和 runner 是否相合；Vfs／MountTable／IFileWatcher 是不是資產模型要的詞彙；另立 target |
| HANDOFF `v8-arch-rebuild-r0-approved`、`v8-arch-gyo2-plan-gaps-2026-11-21`（主會話分支） | 帶過來的經過、補列的經過 |
| [編排層](orchestration.zh-Hant.md) O-3 runner、O-12 交換點、O-13 多個 World 的執行緒 | runner 擁有迴圈與執行緒的安排；World 整個交出；入口的形在 PvP Client 移植那批 |
| [交換點宣告](exchange-declaration.zh-Hant.md) E-5、E-15、E-17、第 5.6 節對照表（X-1、X-4 列，threads 列） | 「執行緒池由產品或 threads 的 Module 建」；「threads 重審照它重寫 `RoleThread` 的用途」；「多執行緒同時讀檔的契約不在本頁定」 |
| [讀檔宣告](restore-entry-declaration.zh-Hant.md)、HANDOFF `v8-arch-gyo2-save-version-into-restore`、`-save-version-module` | 存檔格式是作者的；存檔版本 Module 只比版本標記、不讀內容 → 對 io 沒有直接要求 |
| asset 宣告（`claude/gyo2-asset-585196`，55a14825 動工前核准）5.6（X-1～X-6）、5.12（對 io 的要求）、5.14 | 第 4、5 節的需求來源 |
| [threads.md](../../threads.md)、[io.md](../../io.md)（master 08566260 的條文，帶過來未改） | 現行的 Contract、DI 1～9（threads）、DI 1～5（io）、Replacement Criteria |
| master 的 `engine/threads`、`engine/io` 與它們的使用者 | 只看當時為什麼這樣做、誰在用（第 2 節） |
| 參考引擎（`~/Code/GitHome`） | 第 6 節 |

證據：主 checkout `build/evidence/gyo2-threads-io-review/`（git 不追蹤）的 `inventory.txt`（每個符號在 master 與 gyo2 的使用者）與產生它的 `inventory.py`。走查用 `git grep -E` 經 Python 跑，終了碼 0／1 以外視為失敗；陽性對照：`ReadAllBytes` 命中 master 的 5 個工具檔、`Vfs` 命中 gyo2 的 `VfsTests.cpp`（第一次用 `\bVfs\b` 時 `\b` 在 git 的 ERE 不成立、被吞成零，改寫後才命中——照 §14.2 記下）。

---

## 2. 現況（事實）

### 2.1 threads

- `engine/core/threads`：`RoleThread`、`RoleContext`、`RoleThreadOptions`、`ThreadPriority`、`ThreadError`（`RoleThread.hpp` 103 行、`.cpp` 118 行）。連結 `GYO::Time` 公開、`GYO::OS` 私有。
- **gyo2 上的使用者**：日誌的 `JsonLinesFileSink` 的寫檔執行緒（`engine/modules/logging/src/JsonLinesFileSink.cpp:102,250`，用 `Notify`／`RequestStop`／`Wait`）；`tests/common/os/ThreadTests.cpp` 在 role 的執行緒上讀回優先權。編排層的 runner、其他 Module 都沒有用。
- 系譜：TT-1（v7 執行緒分離），PvP 的 sim／net 角色各一條長命執行緒。

### 2.2 io

| 部分 | master 的使用者（io 之外） | gyo2 的使用者（io 之外） | 測試 |
|---|---|---|---|
| `IFileSystem`、`NativeFileSystem`、`NativeFileUri` | 9 處（工具 5、PvP、asset、imgui shell） | 無 | `NativeFileSystemTests` |
| `Uri`、`file://` 對應、`ParseUriLoose` | asset 的路徑解析 | 無 | `UriTests`（DI 3 的守衛） |
| `ReadAllBytes`、`ReadAllText` | 7 處、1 處（asset catalog） | 無；asset 宣告 5.12 要用 | 有 |
| `WriteFileAtomically`、`ProbeFileStamp`、`IsNativePathWithin` | 4、2、2 處（工具） | 無 | `UserFilesTests` |
| `Vfs`、`MountTable`、`MountPoint` | **無**（只出現在 CMake 清單） | 無；asset 宣告 5.12 要把內容根目錄掛在一個 scheme 下 | `VfsTests`（讀的疊掛）、`UriTests`（掛載不出根，DI 5） |
| `IFileWatcher`、`CreateWatcher` | 無 | 無；asset 5.12「留給熱重載，本批不用」 | 只有「回 NotSupported」 |
| `DirectoryIterator`、`Iterate`、`List`、`Copy` | 無 | 無 | 只有「回 NotSupported」 |
| `FileSystemCapabilities` | 無 | 無 | 只有「每個欄位照實」 |
| `BufferedStream`、`MemoryStream`、`SpanStream`、`StreamReader`、`StreamWriter` | 無 | 無 | Buffered、Reader 各 2 案；Memory 被測試當假資料 |
| `WriteAllBytes`、`WriteAllText` | 無 | 無 | 無 |
| `Vfs` 的寫入路由（寫的 `Open`、`CreateDirectories`、`Remove`、`Move`；`readOnly`、`preferWrite`） | 無 | 無 | 無 |
| `UriScheme::Asset`／`Http`／`Https` | 無 | 無 | `Asset` 有 1 案 |

- 檔案：`.hpp` 26 個、`.cpp` 17 個，其中 7 個 `.cpp` 是只有 namespace 的空殼（實作在標頭）。
- 連結：`GYO::Base` 公開；`GYO::Math`（只有 Stream 類用 `Math::Min`／`Max`）、`GYO::OS` 私有。R0 清單寫的「另立 target」已在 R0b 做完（`GYO::IO`），io.md 仍寫「compiled into GYO::Engine」。
- 突變：io 的在 `docs/architecture/plans/os-subsystem/scripts/mutations.json`（`NativeFileSystem.cpp` 13、`Uri.cpp` 20、`WriteFileAtomically.cpp` 3、`MountTable.hpp` 1、`Vfs.cpp` 1）與 `framework-rework/scripts/io_user_files_mutations.json`（15）；threads 的在 `time-threads-trace` 2 個、`os-subsystem` 3 個。
- 測試執行檔叫 `engine_tests`（舊名；N14 應為 `gyo_io_tests`）。

---

## 3. threads 逐項判定

| # | 項目 | 判定 | 理由 |
|---|---|---|---|
| T1 | `RoleThread` 這個機制 | **留下** | 該不該存在：日誌今天就要一條長命執行緒（寫檔不卡住呼叫者）；runner 要把整個 World 交給另一條執行緒（O-13，Bevy 的 pipelined 渲染就是一條具名的長命執行緒，`bevy_render/src/pipelined_rendering.rs:181-182`）；Unreal 的 `FRunnableThread`、Godot 的 `Thread`〔知〕都是同一個東西。和產品類別無關 |
| T2 | 名字 `RoleThread`、`RoleContext`、`RoleThreadOptions` | **改名**：`DedicatedThread`、`DedicatedThreadContext`、`DedicatedThreadOptions`；檔名跟著改（N16）。**卡片照 L17 判** | N5：名字是概念的標籤，`role` 指的是 v7 PvP 的 sim／net／render 角色（R0 附錄已指出）。「dedicated thread」是和「執行緒池」對照的通用詞：一條執行緒只做一件事、由擁有者建與收。不叫 `Thread`：`engine/os` 已有 `OS::Thread` namespace 與 `engine/os/Thread.hpp`，同名會混；之後若加池（Q1），`DedicatedThread` 與 `WorkerPool` 正好對照 |
| T3 | `ThreadPriority`（三級提示）與 `OS::Thread::PriorityHint` | **留下**，名字不改 | 三級提示是通用的：Godot `Thread.Priority` 是 LOW／NORMAL／HIGH 三級〔知〕，SDL 有 LOW／NORMAL／HIGH／TIME_CRITICAL〔知〕。兩個列舉值對值相等是有意的：threads 的使用者拿不到 os 的標頭（DI 9） |
| T4 | 自己的 Waiter、合作式停止、`WaitBackend()` | **留下** | 日誌的迴圈用 `Wait`／`Notify`；精確的等待歸 time（os-wait），不是 PvP 專屬 |
| T5 | runner 的執行緒（②③） | **不改程式**；threads.md 寫明用法 | O-3：runner 擁有迴圈與執行緒的安排；O-13：World 整個交出、入口的形在 PvP Client 移植那批。`DedicatedThread` 已經足夠當 runner 的執行緒：runner 建、runner 收（DI 3、4），body 跑交出的 World。交換點宣告第 5.6 節 threads 列要求「threads 重審照它重寫 `RoleThread` 的用途」→ 只改文件的「用途」與使用者，不加 API |
| T6 | 工作執行緒池（asset X-1、X-4、X-6；交換點 E-5） | **交使用者（Q1）** | threads.md Replacement Criteria 1：「使用者要池、排程器、工作佇列 → 另立元件，不長大 `RoleThread`」。asset 的讀檔解碼要在 World 執行緒外跑（F1、F2：23.7 MB 的 fbx），現在條件成立。新元件是 Architecture Delta |

---

## 4. io 逐項判定

判準（卡片照 L17 判）：「先問該不該存在」。master 與 gyo2 都沒有使用者、也沒有點名 Feature 的，拿掉，等 Feature 出現再加（AGENTS §1、§2 應用的部分；Replacement Criteria 會寫明加回時的位置）。有點名需求的（asset 5.12、工具搬回、存檔）留下。

| # | 項目 | 判定 | 理由 |
|---|---|---|---|
| I1 | `IFileSystem`＋`NativeFileSystem`＋`NativeFileUri` | **留下**，介面縮到下面 I3 之後剩的操作 | asset 讀內容、工具讀寫使用者檔案、作者讀寫存檔的唯一出口（「一個公開出口，各實作注入」：後端以 `IFileSystem` 注入）。Bevy 的 `AssetReader` 也是「一個 trait，各後端實作」（`bevy_asset/src/io/mod.rs:196`） |
| I2 | `Uri`、`file://` 對應、`ParseUri`、`FileUriFromNativePath`、`NativePathFromFileUri`、`ParseUriLoose`、`Path`／`PathUtils` | **留下** | DI 2～4 與產品類別無關；`MountTable` 用 `ParseUriLoose` 與 `ContainsTraversal`。`PathUtils` 裡 I1～I9 之後在 io 內沒有呼叫者、也沒有測試的函式，在第 2 步拿掉，名單記進 dev_log（卡片照 L17 判） |
| I3 | `IFileWatcher`＋`CreateWatcher`；`DirectoryIterator`＋`Iterate`；`List`＋`DirectoryEntry`＋`ListOptions`；`Copy`；遞迴刪除的 `RemoveOptions`；`FileSystemCapabilities`＋`Capabilities()` | **拿掉** | master、gyo2 都沒有使用者，原生後端全部回 `NotSupported`；熱重載、檔案瀏覽沒有點名的 Feature（asset 5.12：「本批不用」）。能力表只為了事先查「哪些不支援」，不支援的操作拿掉之後就不需要；後端不支援的仍回 `NotSupported`。加回的條件寫進 Replacement Criteria |
| I4 | `IStream`、`FileOpenMode`、`Seek` | **留下** | `Open` 回的就是 `IStream`；整檔讀寫建在它上面 |
| I5 | `BufferedStream`、`MemoryStream`、`SpanStream`、`StreamReader`、`StreamWriter` | **拿掉** | 都沒有使用者。asset 的 loader 吃整檔位元組、串流沒有 Feature（asset 決定 8）；存檔的格式是作者的。測試裡當假資料的 `MemoryStream` 改成測試自己的小假物件。拿掉後 io 不再依賴 `GYO::Math` |
| I6 | `ReadAllBytes`（含 `Vfs&` 版）、`ReadAllText`、`WriteFileAtomically`、`ProbeFileStamp`、`IsNativePathWithin` | **留下** | asset 5.12 要帶上限的整檔讀；catalog 是文字；工具與存檔要原子寫；工具搬回時用使用者檔案的輔助（master 4 個工具在用） |
| I7 | `WriteAllBytes`、`WriteAllText`（非原子的整檔寫） | **拿掉** | 沒有使用者；「原子寫只有一個」（io.md），存檔與工具都該用原子寫 |
| I8 | `Vfs`、`MountTable`、`MountPoint`（讀取疊掛、scheme→根目錄、掛載不出根） | **留下讀取，拿掉寫入路由**（寫的 `Open`、`CreateDirectories`、`Remove`、`Move` 經 `Vfs`；`readOnly`、`preferWrite` 欄位）。卡片照 L17 判 | 讀：asset 宣告（已核准）5.12 把內容根目錄掛在 io 的一個 scheme 下，DI 5（掛載不出根）由 `MountTable::Resolve` 守；留著不需要改 asset 卡的介面。寫：沒有使用者、沒有測試，而且唯一的原子寫 `WriteFileAtomically` 吃的是 `IFileSystem&`、不是 `Vfs`，經 `Vfs` 寫根本接不上原子寫。寫入一律直接對 `IFileSystem` 做。Bevy 的 `AssetSource` 也是讀寫分開、寫是可選的（`source.rs:390`）|
| I9 | `UriScheme::Asset`、`Http`、`Https` | **拿掉**，只留 `None`、`File`、`Unknown`（其他 scheme 的名字照舊放在 `schemeText`） | N5：core 裡的 `Asset` 指的是 asset 的概念；`Http`／`Https` 沒有 Feature。`asset://` 照舊解析，只是 `scheme == Unknown`、`schemeText == "asset"` |
| I10 | 多執行緒同時讀的契約（asset 5.12、交換點 E-17） | **補**：新 DI 6（第 8.2 節） | 照 Bevy：`AssetReader: Send + Sync`（`io/mod.rs:196`，同一個讀取者給多條執行緒同時用）；`AssetSource` 在 Plugin `build` 時建好、之後不變。GYO：`IFileSystem` 的實作接受任何執行緒的同時呼叫（原生後端不持狀態）；`Open` 回的 stream 同一時間只給一條執行緒；`Vfs` 的掛載表在分享出去之前設好，`Mount`／`Unmount`／`Clear` 不和任何其他呼叫同時發生，讀取（`Open`、`Exists`、`Stat`）可以同時。不加鎖、不加守衛（誤用交審查） |
| I11 | 位置與 target | **不動**：`engine/core/io`、`GYO::IO`，不是 Module | asset 私有依賴 io；core 不登記（`v8-arch-gyo2-core-not-registered`）。io.md 的「compiled into GYO::Engine」改成現況 |
| I12 | 測試執行檔 `engine_tests` | **改名** `gyo_io_tests`（N14）；突變的 `target` 跟著改。卡片照 L17 判 | 舊名是 master 的 `GYO::Engine` 時代留下的 |
| I13 | 空殼 `.cpp`（`Vfs.cpp`、`MountTable.cpp`、`DirectoryIterator.cpp`、四個 helper） | **拿掉**（實作在標頭）；`os-g4-io-opens` 的目標檔從 `Vfs.cpp` 改成 io 裡留下的 `.cpp`（例：`src/path/Uri.cpp`） | 空檔沒有用；突變照 §14.5 改目標、不另立 |

---

## 5. 和其他卡的關係

| 卡 | 它用到 io／threads 的什麼 | 本批之後 |
|---|---|---|
| asset（`gyo2-asset`，已核准、未實作） | 5.12：`ReadAllBytes`、掛載點、疊掛（本批不靠）、`IFileWatcher`（本批不用）；5.6：X-1 執行器、X-4、X-6 | 用到的全留；`IFileWatcher` 拿掉與 asset 5.12「本批不用」一致，熱重載那批再加。**Q1 的結論**決定 world_asset 有沒有 Engine 給的預設池 → 經主會話轉 asset 卡 |
| 讀檔＋存檔版本（`gyo2-restore-impl`） | 程式沒有用 io。宣告修訂 12 草案（7970a9d6，第 501 行）寫作者讀寫存檔用三個既有的：「存檔目錄（`UserFiles.hpp`）」、`WriteFileAtomically`、`ReadAllBytes`，並請本卡要拿掉或改時先對齊（2026-11-22 經主會話轉來） | 三個都**留下、不改**（I6）。**更正**：`UserFiles.hpp` 只有 `ProbeFileStamp`（讀整檔算戳記，判斷檔案有沒有被改過），io、os 都**沒有**「存檔目錄」（每個使用者的資料目錄，例：macOS 的 Application Support、Windows 的 `%APPDATA%`、Linux 的 `$XDG_DATA_HOME`）的 API。別家：Godot `user://`、Unity `Application.persistentDataPath`、Unreal `FPaths::ProjectSavedDir`〔知〕；Bevy 沒有內建（用外部 crate）。這是 OS 層（adaptor）的事，不在本批；經主會話轉讀檔卡更正描述，要不要補由主會話排 |
| 網路 Module（`gyo2-net-module`） | 分支沒有用 io、threads | 無影響；之後若要收包執行緒，用 `DedicatedThread` |
| audio 重審（`gyo2-audio-review`） | audio 不用 threads、io | 無影響 |
| 日誌（無在跑的卡） | `RoleThread` | 第 1 步跟著改名（行為不變） |

兩邊誰後合併，誰用 merge 跟上 `gyo2`。

---

## 6. 和別家的對照

| | 長命的具名執行緒 | 工作池 | 檔案抽象 | 多執行緒讀 |
|---|---|---|---|---|
| Bevy〔源〕 | 渲染執行緒用 `std::thread::Builder::new().name("Render thread")`（`pipelined_rendering.rs:181`），不經 bevy_tasks | `bevy_tasks` 三個全域池 `ComputeTaskPool`、`AsyncComputeTaskPool`、`IoTaskPool`（`usages.rs`）；`TaskPoolPlugin` 照核心數比例分（io、async_compute 各 25%，`task_pool_plugin.rs:121-133`）；執行緒名 `<name> (<i>)`（`task_pool.rs:39`） | `AssetReader` trait、`FileAssetReader` 只有一個 `root_path`（`io/file/mod.rs:40`）；具名來源 `AssetSourceId`；沒有疊掛；放在 bevy_asset 裡，沒有通用 VFS | `AssetReader: Send + Sync` |
| Godot〔知〕 | `Thread`，三級優先權 | `WorkerThreadPool` 一個全域池 | `FileAccess`／`DirAccess`，`res://`（可疊 PCK）、`user://` | — |
| Unreal〔知〕 | `FRunnable`＋`FRunnableThread::Create(name, priority)` | `FQueuedThreadPool`、TaskGraph | `IPlatformFile` 一層包一層（pak 疊在原生上） | — |
| Unity〔文〕 | 引擎內部 | Job System 的工作執行緒由引擎管（`JobsUtility.JobWorkerCount`） | `System.IO`；`AsyncReadManager` | — |
| flecs〔源〕 | — | `ecs_set_threads`、`ecs_set_task_threads`（`addons/pipeline.h:231-245`），給 System 用 | — | — |
| master（GYO 舊） | `RoleThread`（TT-1） | 無；asset 在主執行緒每幀輪詢 | `Vfs`＋`IFileSystem`，`Vfs` 沒有使用者 | 沒寫 |
| **本批** | `DedicatedThread`（改名） | Q1 | `IFileSystem`＋讀取的 `Vfs`，寫直接對 `IFileSystem` | DI 6，照 Bevy |

**和別家不同的地方**（只列有取捨的）：
- GYO 沒有全域池（Bevy、Godot 都是全域單例）：GYO 的 World 彼此平等、Product 可以有多個，全域狀態和 O-15 的「Product 一份的共用處」衝突。所以池若要有，是一個物件、由某人建、放進共用處或選項（Q1）。
- `Vfs` 只讀：Godot 的 `user://` 可寫；GYO 的寫一律直接對 `IFileSystem`（I8 的理由：唯一的原子寫吃 `IFileSystem&`）。

---

## 7. 交使用者的

### Q1 工作執行緒池要不要現在放進 threads

- **遊戲的例子**：玩家進關卡，asset 要在背景讀一個 23.7 MB 的模型、解碼，World 照常跑幀。誰提供「背景的執行緒」？
- **前提**（誰定的）：asset 的執行器介面由 asset 自己定、asset 不依賴 threads（asset X-1，使用者核准 55a14825）；執行緒的數量與歸屬由 runner 或產品定（X-4）；交換點宣告寫了兩條路：產品經 `ProductDescription::options` 給，或「之後的 threads Module」建進共用處（交換點 5.6 節 X-1、X-4 列，使用者 2026-11-22 合併前核准）；threads 的 Replacement Criteria 1（池要另立元件）。
- **選項**：
  - **(a) 在 `engine/core/threads` 加一個薄的 `WorkerPool`**：建立時給執行緒數（≥1）與名字（執行緒名 `<name> (<i>)`，照 Bevy）；`Submit(工作)` 排進先進先出的佇列，停了之後回 false、不排；`Stop(Drain)` 跑完排著的再收、`Stop(Discard)` 丟掉排著的、等跑到一半的做完再收（asset X-6 的「停下或等完」）；解構等於 `Stop(Discard)`。不做：future、優先權、偷工作、scope、全域單例。**誰建 Product 的那一個**由 asset 卡定（例：world_asset 在產品沒給執行器時自己建一個，包成 asset 的執行器介面）。第二個要池的使用者出現時（例：網路），改成一個 Module 統一建進共用處——這一條寫進 Replacement Criteria。
  - **(b) (a)＋現在就加一個 Module**，在 `Configure` 建 Product 一份的池放進共用處，asset 等使用者 `Find`。照 Bevy 的 `TaskPoolPlugin`（預設帶、比例分配）。
  - **(c) Engine 不提供池**：產品或測試自己寫執行器放進選項（asset X-1 允許）；world_asset 沒有預設。
- **建議 (a)**。理由：池是機制，放在 threads 一個出口，避免 asset（以及之後的網路）各寫一個（使用者的原則「機制不分散在各個實作裡」）；(c) 讓每個產品都要自己寫池，和「預設方便」（`engine-not-rigid`）相反。(b) 現在只有 asset 一個使用者，Module 是為單一用途加的一層（AGENTS §1），等第二個使用者再加不會推翻 (a) 的介面。
- **代價**：(a) 加一個新元件（約 150 行＋測試、突變），threads 的 Design Invariants 多三條（第 8.1 節）。(a)、(b) 都不影響 asset 已核准的介面（執行器是 asset 的話）。
- **交給主會話轉 asset 卡的**：選 (a) 時，「產品沒給執行器時 world_asset 建預設池」這一句要不要寫進 asset 的實作，由 asset 卡判。

### 不交使用者、卡片照 L17 判的（列出來讓你知道）

`RoleThread` 改名 `DedicatedThread`（T2）；io 拿掉的十項（I3、I5、I7、I8 的寫入、I9、I13）；測試執行檔改名（I12）；多執行緒讀的契約照 Bevy（I10）。理由都在第 3、4 節；這些都是拿掉沒有使用者的東西或照命名規範改名，沒有推翻你之前的決定。Design Invariants 的變更（第 8 節）屬 Architecture Delta，隨本宣告一起核准。

---

## 8. Design Invariants 與 Replacement Criteria 的變更

兩份文件照 gyo2 現況重寫（owner 路徑、target、使用者），開頭的「master 08566260 的條文」註記拿掉。

### 8.1 threads.md

- DI 1～8：`RoleThread` 改成 `DedicatedThread`，內容不變。DI 9：連結不變（`GYO::Time`、`Threads::Threads`、私有 `GYO::OS`）。
- 「What it is」改寫：使用者是日誌的寫檔執行緒與 runner 的執行緒（O-13）；runner 建、runner 收，World 不綁執行緒。
- 選 Q1 (a) 時加：
  - **DI 10 停了之後不收工作**：`Stop` 之後的 `Submit` 回 false、工作不跑也不留。
  - **DI 11 停止會收完執行緒**：`Stop` 返回時沒有工作在跑；`Drain` 跑完返回前排著的，`Discard` 丟掉它們。
  - **DI 12 池不是全域的**：沒有單例，誰建誰擁有。
- Replacement Criteria 1 改寫：「需要排程器、工作之間的依賴、優先權或 scope → 另立元件或換掉 `WorkerPool`，不長大 `DedicatedThread`」；(a) 時加「第二個使用者要池時，改由一個 Module 統一建進 Product 的共用處」。

### 8.2 io.md

- DI 1～5 不變（DI 5 仍由 `MountTable::Resolve` 守）。
- **新 DI 6 多執行緒讀**：`IFileSystem` 的實作接受任何執行緒的同時呼叫；`Open` 回的 stream 同一時間只給一條執行緒；`Vfs` 的掛載表只在沒有其他呼叫時改，讀取可以同時。守衛：行為測試（第 10 節判定 5）；不立突變（理由見第 10 節）。
- 刪掉 master 的 `IAssetSource`、asset resolver 的段落（gyo2 沒有）；原生後端的描述去掉已拿掉的操作。
- Replacement Criteria 1（`IAssetSource` 與 `IFileSystem` 二選一）改寫成：「要列目錄、監看檔案、複製、或第二個非原生來源（pack、memory）時，在 `IFileSystem` 上加那一個操作、並同時加它的測試；不要把能力表加回來代替」。RC 2～4 不變。

---

## 9. Architecture Delta（AGENTS §3 七項）

1. **為什麼要變**：R0 標「帶過去並重審」；asset 5.6、5.12 與交換點 E-15、E-17 交了需求；`role` 是產品的概念；io 有大量沒有使用者的介面（堆在 core 的公開介面上，之後任何人都可能開始依賴）。
2. **現在的問題**：第 2 節。
3. **變的邊界**：threads 的公開型別名；io 的公開介面縮小、`Vfs` 只讀、DI 6 新增；Q1 (a) 時 threads 多一個元件。
4. **影響的 Module／Product／Tool**：threads、io、日誌（改名）、os 的測試與註解；asset 卡（只有 Q1）；`gyo2` 上沒有產品。
5. **依賴方向**：io 少一條（`GYO::Math`）；其餘不變。Q1 (a) 時 threads 不多依賴。
6. **Ownership**：不變（threads、io 都是 Engine core）。
7. **為什麼不能更小**：只改名不縮 io，沒有使用者的介面會繼續被當成 Contract 帶到產品搬回；只縮 io 不寫 DI 6，asset 的工作執行緒同時讀檔沒有根據。

---

## 10. 實作段（使用者說「開始」後）

### 10.1 步驟、模型與檔位

上段（觸及 Design Invariants 與公開介面）。

| 步 | 做什麼 | 實作者 | 審查／檢查 |
|---|---|---|---|
| 1 | `RoleThread` → `DedicatedThread`（程式、測試、日誌、os 的測試與註解、命名規範 N15／N16 的例子、os 邊界的允許清單）；threads 的 3 個突變改目標檔 | Opus 5.5／high（本卡） | — |
| 2 | io 的拿掉（I3、I5、I7、I8 寫入、I9、I13）、`GYO::Math` 拿掉、測試執行檔改名；退役打在被拿掉的程式上的突變、改目標的照 §14.5 | Opus 5.5／high | — |
| 3 | DI 6 的行為測試；io.md、threads.md 重寫 | Opus 5.5／high | — |
| 4 | （Q1 (a)）`WorkerPool`、測試、突變 | Opus 5.5／high | — |
| 5 | dev_log；合併前的唯讀設計審查；開 PR（base `gyo2`） | Opus 5.5／medium | 審查：Fable 5.2／high（和實作者不同模型） |

### 10.2 判定（結果出來之前宣告）

1. 本機（macOS arm64）`ctest -L 'cpu|shader'` 全過；PR 的 L1 三平台（linux-x64、windows-x64、macos-arm64）通過。
2. **相關突變全殺**：改目標的 threads 3 個（`tt1-stop-without-notify`、`tt2-role-backend-fixed`、`th-x8-role-priority-lost`）；io 留下的檔上受影響的突變（打在 `NativeFileSystem.cpp`、`Uri.cpp`、`MountTable.hpp`、`WriteFileAtomically.cpp`、`UserFiles.cpp` 且 `find` 落在改動範圍或殺它的測試被改的）；`os-g4-io-opens`（改目標後）；io 連結檢查的突變（新的，見下）；Q1 (a) 的新突變。照 D184，只跑這些；PR 合併前背景完整跑一次 os-subsystem、framework-rework 兩份清單。
3. 走查：`RoleThread`、`RoleContext` 在 `engine/`、`tests/` 零命中（`docs/dev_logs`、`docs/architecture/plans` 是歷史，不改）；拿掉的符號（I3、I5、I7、I9 的型別名與函式名）在 `engine/`、`tests/` 零命中。照 §14.2：Python 跑 `git grep`、分辨終了碼，先在改動前的樹上命中一次。
4. io 的連結檢查改成 `gyo_expect_links(gyo_io GYO::Base GYO::OS)`、`GYO::OS` 不得公開。這是**只締緊**（少一個允許的連結），照 §14.4 事後報告 1～5；加一個突變「`gyo_io` 加回 `GYO::Math`」並確認被殺。
5. DI 6 的行為測試：8 條執行緒經同一個 `Vfs`（掛在原生後端上）各讀 200 次 3 個不同內容的檔，位元組全對；再加 8 條直接對同一個 `NativeFileSystem` 讀。本機連跑 20 次不失敗。**不立突變**：把共享狀態塞進後端的突變只在真的競爭時才出錯，結果不穩定（§14.1 的偶發），不能當「予期的失敗」。
6. Q1 (a)：`WorkerPool` 的測試涵蓋 DI 10～12 與執行緒名；每條 DI 一個突變（停了仍收、`Drain` 不跑排著的、`Discard` 不等跑到一半的），預期的失敗寫成測試名。
7. 測試判定的變更（照 §14.4，一般測試寫理由）：
   - `UriTests`「schemes other than file keep their parse」：`scheme == Asset` 改成 `scheme == Unknown` 且 `schemeText == "asset"`。守的性質（其他 scheme 照舊解析）不變；多檢查 `schemeText`，沒有緩的面。`UriTests` 是 DI 3 的守衛檔，但這一案不屬 DI 3（DI 3 是 `file://`）。
   - 刪掉的案例：`BufferedStreamTests`、`StreamReaderTests` 全部（被測的類拿掉）；`NativeFileSystemTests`「NativeFileSystem says what it supports」裡 `Copy`、`List`、`Iterate`、`CreateWatcher`、遞迴 `Remove` 回 NotSupported 與能力表照實的判定（被測的操作拿掉）；同一案裡開檔模式組合回 NotSupported 的判定（讀＋建立、讀寫、覆寫既有檔等）**留下**，`Open` 還在。拿掉的判定守的性質隨元件一起消失，不移到別處。測試裡轉發到原生後端的假 `IFileSystem`（`NativeFileSystemTests.cpp:449-458`）跟著介面縮小。`VfsTests` 的假資料從 `MemoryStream` 換成測試內的小假物件，判定不變。
   - io 的 DI 1～5 守衛不變（`UriTests` 的掛載案例、os 邊界守衛 R1～R4 照舊涵蓋 `engine/core/io`；拿掉檔案不縮小目錄 glob 的涵蓋）。

### 10.3 停止條件

照 AGENTS §14 冒頭。另外：若第 2 步發現 asset、讀檔、網路的分支已開始用到要拿掉的介面，停下、經主會話協調。

---

## 11. 不做的事、範圍的限界

- 不加 runner 的執行緒入口（O-13 的形在 PvP Client 移植那批）。
- 不做 asset 的執行器介面與預設池的接線（asset 卡）；不做熱重載、目錄瀏覽、pack 後端。
- 不改日誌的行為；不改 os 的 `Thread`、`Files`。
- 不加「存檔目錄」（每個使用者的資料目錄）的 API：屬 OS 層，第 5 節讀檔卡那一列。
- 三份突變清單的路徑還指向 master 計畫資料夾（HANDOFF 待辦「小批清理」#269）：本批只改清單裡的目標檔與目標執行檔，不搬清單。
- io 在 macOS x64、Windows UNC（`gyo_io_unc_probe`）的行為照舊；本機只有 macOS arm64，Windows 靠 L1。

## 12. 估時

設計段：本頁（約 3 h，已用）。實作段：Q1 (a) 時 8～12 h；(b) 再加 3～4 h；(c) 5～7 h。

## 13. 處理對照（敵對式檢查後填）

（待填）
