# 架構漂移健檢（2026-10-27）

基準：master `37d533d`（PR #95 合併後）。上一份報告的基準是 `c8ee213`（2026-10-04），這段期間 first-parent 合併了 57 個 PR（#38～#95）。
判斷標準與步驟見 [README](README.md)。這是第一次照新增的步驟 5～7 做：OS 相依、只有介面的抽象、執行期的證據。

做法：6 位掃描員依 README 的步驟分工，1 位審查者到程式碼裡逐項嘗試推翻。89 項發現中，確認 67 項、修正 22 項、推翻 0 項；審查者另外補了 1 項漏掉的。
本報告以 master `37d533d` 為準。OS 統一子系統的分支（`claude/trusting-joliot-f13b78`，最新 `c9087d0`，未合併）已經處理的部分，註明「分支已處理，未合併」。
除了註明已立案的項目，其餘都尚未立案。

## 結論

- **Engine 仍然沒有被產品入侵。**
  - `engine/`（不含 `engine/config`）與 `services/` 沒有產品名稱、玩法常數或產品協議常數。
  - 依賴圖沒有循環，Engine 的 target 不會連到任何產品或工具。
  - 新模組 net、time、threads、trace 的依賴邊與計畫宣告的一致。
  - 共通層沒有依產品名稱分支。
- **上次列出的 Engine 缺口大多已收進 Engine。**
  - 已收進的有：輸入層、Collision 合法性檢查、文字輸入與剪貼簿、GYOP 標頭的 C++ 編解碼、Color 有限性檢查，以及日誌出口。
  - `object_fps_pvp` 的 C++ 已經沒有任何直接的 SDL 呼叫。
- **這段期間最大的問題是 OS 差異沒有歸屬。**
  - Engine 沒有指定負責 OS 差異的模組。
  - 檔案抽象只有介面，這點上次就存在，但上一份報告漏判。
  - 程序服務只在 SDL 後端。
  - Windows 長路徑已經在實機發生。
  - 這些都已交給 OS 統一子系統任務（2026-10-25 開立），計畫與大部分修正在未合併的分支上。
- **新出現的歸屬問題在 Gateway。** Engine 擁有的 `services/gyo_gateway` 在 v7.1 由產品的批次修改，設計約束只寫在產品文件裡。
- 上一份報告有三處不準確，在「更正」一節說明。

## 這段期間的結構變化

- **新的 Engine 模組**：
  - net（#45，GYOP 標頭編解碼）；
  - time、threads（#71）；
  - trace（#82）。
- **既有模組的擴充**：`engine/platform/sdl` 大幅擴充，多了 Native 層、入口、日誌橋、輸入注入（#85）；`engine/base` 多了 `Fnv1a.hpp`、`Log.hpp`。
- **新的最上層目錄**：`agents/`（#87）。只放變速箱規則，由 AGENTS.md 匯入，PR 中已標為 Architecture Delta。
- **Engine 計畫**：
  - foundation-followups：FF-1～9 完成；
  - input-and-present：IP-1～4 完成，IP-5 未開始；
  - time-threads-trace：TT-1、TT-2 完成；
  - OS 統一子系統：進行中，在分支上。
- **產品的穩定版本**：v6（#66，之後標為缺陷版）、v7（#74）、v7.1（#83）。
- **新的 `Design Invariants` 節**：`docs/architecture/input.md`、`platform-sdl.md`、`trace.md`。
- **公開介面的重大變更**：
  - include 根目錄統一（#52）；
  - 完整鍵盤與有序事件（#50）；
  - SDL 隔離（#85）；
  - Collision 改用 double 並公開 `IsValid`（#58）；
  - `RuntimeLoop` 改用 Engine 的時間基準（#71）。

## 一、應收進 Engine 的功能

| 優先 | 功能 | 證據 | 建議的最小做法 |
|---|---|---|---|
| 中 | **不依賴 SDL 的程序服務：執行檔位置、UTF-8 命令列與路徑轉換** | `BaseDirectory`、`PathFromUtf8`、UTF-8 入口都只在 `GYO::PlatformSDL`（`SdlPlatform.hpp:246-255`、`EntryPoint.hpp`）。<br>沒有 SDL 的 Match 自己用 `GetModuleFileNameW`、`/proc/self/exe`、`_NSGetExecutablePath` 找執行檔（`LogFile.cpp:56-84`，`match_main.cpp:75-76` 使用），命令列直接把窄字串當路徑（`match_main.cpp:36-55`）。<br>ui_editor 用一般的 `main`，也把窄字串當路徑（`Main.cpp:71-76`、`CommandLine.cpp:46`）。<br>`platform-sdl.md` 把 Process services 歸 Engine；它的 Replacement Criteria 第 1 條「需要第二個非 SDL 後端」，可視為已經觸發。 | 有兩個獨立消費端（Match、ui_editor），Engine 也有缺口，符合收進條件。<br>**已立案**：OS 統一子系統（程序資訊與 UTF-8 路徑兩批，分支已處理，未合併）。<br>立案範圍沒提到的一點：沒有 SDL 的程式要不要也有最外層的例外處理（`RunEntry` 的對應物），目前由 Match 自己處理（`match_main.cpp:160`）。 |
| 中 | **原子寫檔與「路徑是否在根目錄內」的判斷** | ui_editor 用 `MoveFileExW` 自己做原子取代、只在 Windows 用 `towlower` 比較路徑（`FileService.cpp:36-52`、`:69-92`、`:193-212`）。<br>Engine 自己的 shader 建置工具另做一份原子寫檔，機制不同（`engine/render/shaders/pipeline/src/main.cpp:64-71`）。<br>Engine 的 `WriteAllBytes` 只能經 `IFileSystem` 寫，而 `IFileSystem` 沒有原生實作（見第二節）。 | 兩個獨立消費端，Engine 有缺口。<br>**已立案**：OS 統一子系統（分支上有原子寫檔與原生檔案系統，未合併）。 |
| 低 | CPU 時間、本地時間、停止要求 | 程序與執行緒 CPU 時間（`NetworkStatistics.cpp:58-81`）。<br>`localtime_s`／`localtime_r` 在同一產品裡寫了兩次（`main.cpp:65-76`、`LogFile.cpp:33-53`）。<br>無視窗的 Match 用 `std::signal` 接停止要求（`match_main.cpp:23-25`、`:94`）；Windows 的主控台事件（Ctrl+C、關閉視窗）不是 signal，沒有處理。 | Engine 有缺口，但只有一個消費端。是否一起收進程序服務，由 OS 統一子系統任務決定（分支已有對應實作）。 |

### 判斷過、目前不建議收進 Engine 的

- **socket 錯誤分類**：在 `ClientConnection.cpp:897-910` 與 `IpcHost.cpp:113-115`。
  - 不建議的理由：Engine 刻意不依賴 socket（`docs/architecture.md:133`），產品用的是 asio，沒有繞過 Engine 的抽象；連線專案也只有一個。
  - Windows 的 `WSAECONNRESET` 行為，在 lan15-windows 之前沒有實機確認。
- **輸入法組字事件**：Engine 的文字輸入只送出確定後的文字，沒有轉送組字事件。
  - 只觀察到一次輸入時卡住，原因是推論（`input-and-present/HANDOFF.md:331`），也還沒有消費端要求。
  - v8 做輸入框元件時再確認（`v8/README.md:61`）。
- **命令列解析**：三處各自手寫，但沒有繞過 Engine 的抽象。真正的缺口是 UTF-8 命令列，已併入上表第一項。
- **可指定分箱的時間直方圖**：pvp 把晚醒用的 `LateWakeStats` 拿來量鎖等待與排隊時間，只有一個消費端。契約面的風險見第二節。
- **真正非同步的資產載入**：`AssetManager` 的 Async 是在呼叫端執行緒分批輪詢。目前只有 v8 的預估壓力，沒有觀測（AGENTS §2），所以不先動。
- **視窗縮小時的節流**：由產品自己暫停 10 ms（`PvpApplication.cpp:1211-1216`）。依 Engine 的既定方針，這是消費端的工作；而且產品用的是 Engine 的 Waiter。

## 二、Engine 這一側的問題

### OS 差異與檔案

| 嚴重度 | 項目 | 說明 |
|---|---|---|
| 高 | **Windows 長路徑：資產用一般路徑字串開檔** | 2026-10-16 的 Windows LAN 臨時測試中，路徑 271 字元，Client 開不了 shader（`docs/object_fps_pvp/plans/v7.1/windows-long-path.md:7`）。<br>master 上 `NativeFileAssetSource.cpp:21`、`ContentManifest.cpp:93`、`AssetCatalog.cpp:30` 仍用 `std::ifstream` 加窄字串開檔。<br>失敗門檻不只 shader：manifest 約 220 字元、log 229、trace 234（同文件 `:50-60`）。錯誤訊息也分不出「路徑太長」和「檔案不存在」。<br>CI 的套件檢查解壓在短路徑，所以沒有抓到。<br>**已立案**：v7.1 的 windows-long-path（D193→D198）與 OS 統一子系統。分支的修正只涵蓋讀資產，log、trace、場地檔要等寫檔那幾批一起合併，否則只是把失敗點往後推。<br>建議把「深路徑的套件情境」放進 windows-x64 的 L1，當作回歸守衛。 |
| 中 | **Engine 沒有負責 OS 差異的模組** | 等待在 Time（`Waiter.cpp:11,54,129`：kqueue、waitable timer、condition_variable），執行緒在 Threads（`RoleThread.cpp:11,45,64`）。程序與路徑服務在 PlatformSDL，而且綁在 SDL 上；shader 建置工具也經 SDL 呼叫 OS。<br>Windows 標頭的前置定義也重複了（`Waiter.cpp:17-22`、`RoleThread.cpp:15-20`）。<br>目前每種差異大致只在一處處理，但沒有歸屬規則可以檢查，新模組也能直接加入 OS 程式碼。Time 與 Threads 的公開標頭沒有 OS 內容，這點是好的。<br>**已立案**：OS 統一子系統。分支上已有 OS 邊界守衛與允許清單，合併後才算數。 |
| 中 | **檔案抽象只有介面**（上一份報告漏判） | `IFileSystem.hpp:26-29` 寫明原生實作由 platform 側提供，但從來沒有寫。全 repo 唯一的實作是測試替身（`tests/common/core/io/VfsTests.cpp:20`）。<br>Engine 以外只有 `AssetPathResolver.cpp` 用到 `engine/io` 的路徑工具。<br>結果是 asset、trace、產品、工具都直接用 `std::fstream` 開檔，長路徑和路徑編碼的問題也因此散在各處。<br>這段期間 `engine/io` 沒有任何 commit，所以 10-04 時就是這個狀態。<br>`Vfs`、`MountTable` 是寫在標頭裡的實作，本身完整，但除了測試沒有消費端；Result 統一和 io 契約修正仍為它們花了工夫。<br>**已立案**：原生後端（OS 統一子系統，分支）。<br>**未立案**：`Vfs`、`MountTable` 沒有消費端這件事。建議比照 Math，記錄「第一個消費端出現前不擴充」。 |
| 中 | **把路徑字串交給 OS 的編碼做法不一致** | UTF-8 的有：`SdlPlatform.cpp:225-228`（`char8_t`）、`SceneCaptureFile.cpp:25`（`u8string`）。<br>窄字串的有：shader 工具把 `temp.string()` 交給要求 UTF-8 的 `SDL_RenamePath`（`main.cpp:64-69`）；asset 把 JSON（UTF-8）的路徑轉成窄字串開檔（`ContentManifest.cpp:103,109` → `AssetCatalog.cpp:30`）。<br>在 Windows 上，窄字串會照 ANSI 字碼頁解讀。這一項沒有實機事故，是依 MSVC 語意推論。<br>**已立案**：OS 統一子系統（UTF-8 路徑一批，分支）。 |
| 中 | **Windows 無 SDL 程序與 Linux 的等待精度沒有量測** | 只要初始化 SDL，就會調高程序的計時器解析度；不初始化 SDL 的 Match 沒有這一點（`time-threads-trace/HANDOFF.md:59、94`）。<br>唯一的實機數據：主機沒開 Client 時，晚醒 ≥2 ms 佔 1.67%、最大 14.7 ms；開著時 0（`v7.1/HANDOFF.md:294`）。<br>Linux 只有 condition_variable 後備（`Waiter.cpp:154-155`），從未在實機量過；在 macOS 上它比 Waiter 慢約 30 倍（`time-threads-trace/HANDOFF.md:86`）。<br>**已立案**：lan15-windows（`v7.1/INVENTORY.md:130`）、OS 統一子系統的晚醒紀錄（量 Linux）。<br>建議 lan15-windows 分「主機有／沒有 Client」兩段量，並讀日誌裡的等待後端。 |
| 低 | Windows 絕對路徑的判斷規則有三份，彼此不同 | `PathUtils.cpp:20-35` 只要 `C:` 就算絕對；同檔 `:275-292` 要 `C:\` 或 `C:/`；`ShaderLibrary.cpp:39-47` 拒絕任何 `:`。<br>OS 分支沒有改這三份規則，所以**未立案**。 |
| 低 | 時間基準跨系統睡眠時的行為沒寫進契約 | `MonotonicClock.hpp:11-14` 只寫了 macOS。產品已用自己的政策處理睡眠（D182、斷線恢復），但下一個寫逾時的消費端只能自己再踩一次。<br>建議在 time 的 Design Invariants 補一句：跨睡眠的行為依平台而定，呼叫端不得依賴。 |
| 記錄 | Trace 在 Windows 寫出 CRLF，契約沒寫 | `TraceSink.cpp:102` 用文字模式開檔，`trace.md:26` 只寫「只加換行」；這件事只寫在測試裡（`TraceSinkTests.cpp:32-38`）。<br>分支的 `trace.md` 已寫明，未合併。 |

### Gateway 的歸屬

| 嚴重度 | 項目 | 說明 |
|---|---|---|
| 中 | **Engine 擁有的 Gateway：設計約束寫在產品文件，修改由產品的批次發起** | `docs/architecture/` 沒有 Gateway 的文件。限速與改綁的設計意圖只寫在產品文件（`docs/object_fps_pvp/network-architecture.zh-Hant.md:897、908-909、923`）。<br>`services/gyo_gateway/README.md:8-11` 有記錄 token bucket，但沒有 Design Invariants、Replacement Criteria，也沒有改綁的條件。<br>v7.1 在 session 加入新的狀態（`lastVerified`、`schedule`）與改綁判斷（`session.go:26-41`，2026-10-23），晚於 Replacement Criteria 規則生效（10-16）。這段期間的三次修改都由產品批次發起。<br>依 AGENTS §7 刪除 pvp 後，Engine 的 Gateway 會失去所有約束紀錄。<br>建議在 `docs/architecture/` 為 Gateway 寫 Design Invariants 與 Replacement Criteria，不點名產品，產品文件改為引用它。新增這一節屬於 Architecture Delta，需要使用者同意。**未立案**。 |
| 中 | **限速 120 包／秒寫死，產品的送出預算只剩 4 包餘裕** | `session.go:42-53` 是套件常數，同檔的改綁規則卻由呼叫端傳入（`:71-77`），兩者設計不一致。<br>產品的送出預算是對著這個上限設計的：82＋30＋4＝116（`Movement.hpp:40-43`）。<br>120 不是照產品流量訂的：10-04 時就是固定窗口 `packets > 120`，D121 只改了演算法。實際的壓力在另一邊：任何需要更高上限的產品，包括 v8 多武器後的 pvp，都得修改 Engine。<br>建議擇一：改成像改綁一樣由呼叫端傳入；或明確宣告 120 是 Engine 的傳輸契約上限，並把理由寫成不依賴產品的形式。這是公開介面的變更，屬於 Architecture Delta。**未立案**。 |
| 低 | Gateway↔產品之間的傳輸契約，兩端歸屬不同 | Go 端的 TCP frame 與序號比較屬於 Engine（`framing.go:13、52、54-78`）；C++ 端在產品（`Wire.hpp:13-17、49-58`、`IpcHost.cpp:582-585`、`ClientConnection.cpp:533`）。<br>FF-8 當時明文把 TCP frame 排除在 Engine 之外（使用者 D4）。兩端沒有共用的測試向量，改了一邊，另一邊的測試不會失敗。<br>注意這是兩條不同的路徑：TCP frame 在 Gateway↔Match，序號比較在 Client↔Gateway 的 UDP。<br>**待決定歸屬**：(a) 收進 `engine/net`，與 Go 共用測試向量；或 (b) 把 Go 端的 framing 也移出 Engine。 |
| 低 | Engine 的 Gateway 程式碼註解引用產品計畫的代號 | `session.go:48`、`session_test.go:96、104` 出現 v7.1、K1、D121，以及「開槍的玩家」這種情境。刪除 pvp 之後，這些引用就失去出處（AGENTS §7、§12）。<br>建議處理上一項時一起改寫，改成不具名的機制說明。 |

### 只有介面或沒有消費端的抽象

這些都不影響行為，但會吸收維護成本。建議一律記錄為「第一個消費端出現前不擴充」，去留由使用者決定。

| 抽象 | 現況 |
|---|---|
| `Vfs`、`MountTable` | 見上面「檔案抽象只有介面」。 |
| `IFileWatcher` 與 `AssetWatcher` | 同一件事有兩套：`IFileWatcher` 沒有任何實作；`AssetWatcher` 自己用 `std::filesystem` 輪詢（`AssetWatcher.cpp:4、9、186`）。熱重載預設關閉，啟用中的產品與工具都沒用。 |
| `IRuntimePort` | 啟用中的產品已經沒有實作者，只剩未啟用產品與測試。`docs/architecture.md:82` 仍把它寫成 runtime 的邊界。建議和未啟用產品的去留一起決定。 |
| `InputActionMap` | 只剩未啟用產品與測試在用；pvp 直接讀 `PhysicalInputFrame` 的鍵，屬於正當用法。這段期間 Engine 仍為它做了雜湊統一與合併協調。`input.md` 沒有提到它。 |
| `CatalogFormat.hpp` | 宣告自己是 catalog 鍵名的唯一來源，但沒有任何檔案 include 它；`CatalogParser.cpp:19-34` 自己寫死同一組鍵名和版本，違反 AGENTS §9「同一個事實只在一處維護」。這是 Engine 內部的小修。 |
| `SoundLoader`、`BinaryLoader` | 編進 `GYO::Engine`，但沒有註冊者，也沒有測試。`SoundLoader` 已隨 Audio 排到 v8；`BinaryLoader` 沒有任何計畫提到。 |
| Math 公開介面的一部分（延續） | `Sphere`、`Triangle`、`Matrix3`、`Vec4`、`Vec2i`／`Vec3i` 仍然只有測試在用；Quaternion 的 `Inverse` 已有 pvp 使用。這段期間 Math 沒有任何改動，「不再擴充」有被遵守。 |

### 其他

| 嚴重度 | 項目 | 說明 |
|---|---|---|
| 中 | **對獨立消費端的破壞性變更**（延續，範圍擴大） | 公開 API 仍以沒有過渡期的方式變更（include 根目錄、SDL 隔離、Collision 改 double 等）。<br>未啟用產品的遷移清單只有逐批的 grep 盤點，例如 FF-7 的 94 行、36 檔；沒有做過完整的編譯量測，結論表也過時了（`foundation-followups/inactive_products.md:10-16`）。<br>Engine 仍沒有棄用規則。preview 已停用（`tools.csv:3`）。<br>**待決定**：與 10-04 相同。 |
| 中 | 產品登錄資料仍放在 `engine/config`（延續） | 這段期間只改過內容（停用 preview），位置沒變。維持候選（D11②）。 |
| 低 | Time、Threads、Net 在 master 上沒有 Design Invariants | master 上只有 input、platform-sdl、trace 三份。三個模組都在規則生效（10-16）前建立，建立時不算違規；但 RoleThread 在規則生效後（10-20）又擴充了。<br>Time、Threads 的 Design Invariants 已在 OS 分支寫好並經使用者同意（time 11 條、threads 9 條），未合併。<br>Net **未立案**。建議至少寫一條：與 Go 的 framing 位元組一致，由 `tests/common/net` 的向量守護。 |
| 低 | 產品的文件放在 Engine 的文件資料夾（`docs/architecture/`） | `docs/architecture/3d-assets.md` 是 `object_fps`（v1）的資產文件，標題就是「Object_FPS 3D assets」。<br>它被 `docs/architecture.md:234` 當成 `AssetManager` 的資料契約引用，三份根 README 的第 73 行也連到它。<br>刪除 `object_fps` 時，必須同時修改 Engine 的文件與根 README（AGENTS §7）。同類但較輕的還有：`math.md:5` 在 Engine 的契約文件裡點名 `object_fps_pvp`。**未立案**。 |
| 低 | 同一個建置事實維護在兩處（延續） | MSVC 的 `/Zc:preprocessor` 仍由 `GyoBuild.cmake:43-44` 全域設定，shader 工具又設一次（`engine/render/shaders/pipeline/CMakeLists.txt:27-29`）。<br>更正上次的建議：shader 工具不連結 `GYO::Base`，只把 include 路徑指向它，所以光改成 `GYO::Base` 的 INTERFACE 選項無法消除第二處。要另找單一來源。 |
| 低 | Metal 取得 drawable 時停頓，仍回報 Presented | 最長停頓約 399 ms（`input-and-present/HANDOFF.md:401`），消費端分不出「真的呈現」和「卡在取得」。Windows 上的對應行為沒有實測。<br>**已立案**：IP-2 的未結事項、v8 的 Render 執行緒拆分候選。 |
| 記錄 | `LateWakeStats` 的分箱算不算對外契約 | 產品把它用在鎖等待與排隊時間（`NetworkStatistics.hpp:36-69`）。若 Engine 為晚醒用途調整分箱，這些指標的意義會在沒有訊號的情況下改變。<br>分支的 `time.md` 已把「分箱固定」寫成 Design Invariant 並加守衛，未合併。 |
| 記錄 | 行為變更（延續） | Math 的 `Clamp` 在所有建置都 assert；全域 `-ffp-contract=off`。維持原狀。 |
| 記錄 | 命名與歸屬（延續） | `MeshLayer::ViewModel` 的名稱帶 FPS 色彩，維持原狀。<br>`services/gyo_gateway` 由 Engine 擁有、放在根目錄的 `services/`；它的責任在這段期間擴大，見上面 Gateway 那兩項。 |

## 更正上一份報告

報告不回頭改寫，所以更正寫在這裡。

1. **漏判**：`engine/io` 的檔案抽象只有介面、沒有原生實作，10-04 時已經存在（`engine/io` 在這段期間沒有任何 commit），但報告沒有提到。它正是判斷標準第 2 條第 4 點（契約歸 Engine，但只實作了一部分）的典型樣子。新的步驟 6 就是為了抓這種情況。
2. **結論不正確**：10-04 的結論說「自寫的 `Explain(error)` 現在改由 `Base::Describe` 提供，啟用中的消費端已完成遷移」。但 `c8ee213` 的 `PvpApplication.cpp:55` 已經有同一個 `Explain`。
   - 現況：pvp 有 17 處 `Explain`、7 處 `ExplainUi`，使用 `Describe` 的有 6 處。
   - `error-handling.md` 的規則 7 要求只用一個格式化函式。
3. **說法不正確**：10-04 說「pvp 的 `ExplainUi` 和 ui_editor 的 `UiDocumentBridge.cpp` 都還是自己拼接 `jsonPointer`」。
   - ui_editor 的部分不對：當時它已經用 `Describe`，並把 `jsonPointer` 當作位置欄位。
   - pvp 的部分屬實：`PvpApplication.cpp:69-71` 的 `ExplainUi` 會丟掉 code 與 detail。

## 上一份報告項目的現況

| 項目（10-04） | 當時 | 現況 | 證據 |
|---|---|---|---|
| 輸入層：完整按鍵集與視窗事件 | 高 | **已解決** | IP-1（#50）、IP-2（#53）、IP-3／IP-4（#85）。pvp 的 SDL 直接呼叫已歸零 |
| 公開 Collision 的合法性檢查 | 中 | **已解決** | FF-9（#58）公開 `IsValid`。Arena 改用它；「牆不得為空」以產品內容規則保留，並寫明理由 |
| 文字輸入與剪貼簿 | 中 | **已解決** | IP-3（#85）。編輯行為（Backspace、Ctrl+V）仍由產品處理，符合當時建議 |
| `AssetManager` 沒有「載入並取得型別化 payload」的入口 | 中 | 仍在，降為低 | pvp 的兩份 `LoadShared` 只是組合公開 API，沒有繞過任何抽象，缺口很弱。維持候選（D11②） |
| `Base::Describe` 不輸出 `UiError` 的位置 | 中低 | 仍在（pvp） | `Error.hpp:72-85` 沒有 `UiError` 的 overload。ui_editor 的部分見「更正」 |
| Logging | 中低 | **大部分解決** | Engine 有日誌出口（TT-2，#82）與 SDL 日誌橋（#85）。pvp 的 Client 走 `Base::Log`，Match 的日誌檔由 Engine 的 `TraceSink` 組成。ui_editor 仍有 6 處直接用 `SDL_Log`，它本來就明示連結 SDL |
| GYOP 傳輸框架的 C++ 版本 | 等時機 | **已解決** | FF-8（#45）的 `engine/net`，版本號與訊息種類留在產品。TCP frame 另見第二節 |
| 網路驗收的流程控制 | 等時機 | 仍在 | 沒有第二個需要網路驗收的專案，條件未成立 |
| 判斷過、不建議收進 Engine 的 | — | 判斷不變 | pvp 的 `Csv` 已隨未編譯檔刪除（#55） |
| 對獨立消費端的破壞性變更 | 中 | 仍在，範圍擴大 | 見第二節 |
| Collision 語意改變，消費端沒有訊號 | 中 | 已改變 | 驗證手段已公開（FF-9），遷移方法寫進清單；未啟用產品尚未遷移 |
| 產品登錄資料放在 `engine/config` | 中 | 仍在 | 見第二節 |
| 同一個建置事實維護在兩處 | 低 | 仍在 | 建議已更正，見第二節 |
| Engine 內部的 `IsFinite` 重複 | 低 | **已解決** | FF-4（#47）。Ui 那份依 D11⑦ 有明文理由保留 |
| Math 公開介面有一部分沒有消費者 | 低 | 仍在，未擴充 | 見第二節 |
| 行為變更、命名與歸屬 | 記錄 | 維持 | Gateway 的責任擴大，見第二節 |
| 附錄 A：計畫用的工具放在 `docs/` | — | 仍在，壓力增加 | 見附錄 A |
| 附錄 A：pvp 有 29 個未編譯的 `.cpp` | — | **已解決** | 專案負責人選擇刪除（#55） |
| 附錄 A：pvp 的測試 include 驗收目錄 | — | 仍可接受 | 同一個 owner |

## 附錄 A：流程與專案內部（不屬於健檢核心）

- **計畫用的工具放在 `docs/` 底下，而且已經跨計畫重用。**
  - 突變執行器放在 `docs/architecture/plans/time-threads-trace/scripts/`，input-and-present 計畫也呼叫它，還為它加了功能。
  - 計畫結案或搬移時，這支工具會一起失去歸屬。依 AGENTS §5，重用已經發生，比 10-04 更接近該移到 `build/` 或 `tests/common` 的時點。
- **新的最上層目錄 `agents/` 沒有列進根 README 的目錄表。**
  - 它由使用者指定，也已在 PR 中說明，本身沒有問題；但目錄表是讀者認識最上層概念的入口。
  - CI 的範圍判斷也只把 `docs/` 當成文件（`build/ci/common/ci_scope.py:21`），所以只改 `agents/` 的推送會重跑 L1。
- **repo 沒有 `.gitattributes`。** Windows checkout 時的換行轉換，讓產品驗收、golden 比對、突變工具各自繞路，例如 `run_client_delay.py:394` 先把 CRLF 換成 LF 再算雜湊。
- **Engine 的並行測試有三次在 CI 較慢的 runner 上失敗。**
  - 原因都是缺少啟動握手，三次都歸類為測試本身的問題並已個別修正（`input-and-present/HANDOFF.md:373-378、408-410`，PR #91、#95）。
  - 同一種「靠執行緒啟動時序碰運氣」的斷言，在 `tests/common` 其他並行測試中可能還有（推論），沒有整體盤點。
- **Engine 的 Windows 專屬路徑，突變只能在 Windows 實機驗證。** 開發主機是 macOS，CI 不跑突變（例如 `PathFromUtf8`、長路徑）。
- **Client 網路執行緒把 asio 計時器和 Waiter 接在一起用。**
  - Engine 的精準等待不能同時等 socket，所以最後 2 ms 內不處理收包（`ClientConnection.cpp:829-845`）。
  - Engine 刻意不依賴 socket，這屬於產品內部的取捨。
- **ui_editor 依平台寫死系統字型目錄**（`ImGuiSdlRendererHost.cpp:39-65、74-110`）。已在 OS 分支收進 Engine，未合併。

## 附錄 B：已確認無問題

- `engine/`（不含 `engine/config`）與 `services/` 沒有產品名稱、玩法常數或產品協議常數。
- 依賴圖沒有循環，Engine 的 target 到不了任何產品或工具 target。依賴圖用 `cmake --graphviz` 在 macOS 產生。Go 這一側的方向也正確：產品以 replace 依賴 `services/`，沒有反向依賴。
- 新模組的依賴邊與計畫一致：
  - `gyo_net → gyo_base`；
  - `gyo_time → gyo_base`；
  - `gyo_threads → gyo_time`；
  - `gyo_trace → gyo_threads`，不到 SDL，也不到 `GYO::Engine`，與 `trace.md` 的 Design Invariant 一致。
- 共通的建置、CI、測試與 workflow 沒有依產品名稱分支。10-04 時唯一的命中（`test_workflow_gates.py` 的負向檢查），現在改由登錄資料組出。
- Gateway 的 session 只持有傳輸層的狀態，沒有遊戲狀態或協議內容。產品需要時讀取 session 的值，沒有另存副本，符合 AGENTS §3 的 State Authority。
- Trace、SDL 平台層、輸入幀已經有 Design Invariants 與 Replacement Criteria。
- Engine 的公開標頭、產品驗收的 C++、Go 服務與產品 CMake，都沒有 OS 分支。
- pvp 的 C++ 沒有直接的 SDL 呼叫。ui_editor 剩下的 SDL 呼叫都透過 Engine 允許的 Native 介面接 ImGui，屬於明示的直接使用。
- pvp 的執行緒、等待、時間基準與 trace 寫檔都用 Engine 的元件，沒有自己再做一套。
- macOS 計時器合併造成的晚醒已由 Engine 的 Waiter 解決（TT-1，#71）；Linux 上 Engine 時間基準與 SDL 時鐘的速率差，已在平台邊界以成對取樣處理（IP-3）。

## 附錄 C：這次的涵蓋範圍與限制

- 依賴圖只在 macOS 主機上 configure，只有 Windows 或 Linux 才有的條件 target 沒有涵蓋。
- 玩法用詞用固定的詞表搜尋，組合字可能漏掉。
- 幾個大檔只用 grep 搜尋，沒有逐行讀：`IpcHost.cpp`、`MatchRuntimeHost.cpp`、ui_editor 的 `EditorApp.cpp` 等。
- 未啟用產品沒有建置，破損總量是依遷移清單推論。
- OS 分支只用來判斷「是否已立案、是否已處理」，現況一律以 master 為準。
