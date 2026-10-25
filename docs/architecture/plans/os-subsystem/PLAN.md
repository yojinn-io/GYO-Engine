# OS 子系統：批次計畫

更新：2026-10-25。**狀態：第 1 版已核准**（D46）；開放決策 1、14、15 已決定。
file:line 以 master `23fab75` 為準，每批開始時重新核對。本文件依 D12 匿名：只寫「提出需求的消費端」（以下稱消費端），程式路徑與證據路徑當作資料保留。

## 1. 目標與範圍

目標（D40）：Engine 的其他模組、產品與工具都不必考慮 OS。所有 OS 差異收攏到新的 Engine 子系統 `engine/os`。消費端在 Windows 發現的長路徑問題，只是 Files 部件第一個觀測到的壓力。

範圍：

- 做：
  - 新子系統 `engine/os`（獨立 target `GYO::Os`），以及它的 Design Invariants 與守衛。
  - engine/io 的原生 `IFileSystem` 後端與路徑契約（UTF-8、file:// 對應）。
  - 把 Engine、產品、工具現有的 OS 分支逐批搬進 engine/os：檔案、程序事實、主控台與結束要求、執行緒、等待；socket 錯誤分類是條件式。
- 不做：
  - 視窗、輸入、剪貼簿、GPU、SDL 時鐘（engine/platform/sdl 照舊）。
  - socket 本身（asio，只在消費端）。
  - 建置層的 OS 條件（CMake、CI、Python、登錄資料）。
  - Go 的轉送程序。
  - 新的 OS 後端或調參（例如 Linux 的精準等待），這些要另有 Feature。
- 分工（D45）：
  - 本計畫的批次只交付結構與契約。
  - 長路徑的行為修正與驗收屬於消費端的發行任務，在 A1～A3 完成後由它進行（第 8 節）。

## 2. 架構樹（目標狀態）

各節點標出負責的批次代號（與第 7 節、README 的進度表一致），以及現在的來源位置（← 表示從那裡搬來）。

```text
GYO-Engine：OS 差異的歸屬（目標狀態）
│
├── engine/base                          最底層共用型別（Result、Error、CauseDetail）
│
├── engine/os  ［新，GYO::Os］           OS 差異的唯一 owner；只依賴 engine/base 與標準庫
│   ├── OsError                          os-skeleton｜錯誤碼從 1 開始，detail 帶原生碼（errno=／win32=）
│   ├── Files：讀取原語                  os-files-read｜UTF-8 轉原生、開檔讀取、Stat、Exists、MakeAbsolute、PathToUtf8／PathFromUtf8
│   │   └── Windows 長路徑               消費端 P1｜\\?\ 前綴只在這裡、呼叫 OS 前產生
│   ├── Files：寫入原語                  os-files-write｜寫檔、建目錄、原子取代、路徑元件比較
│   │                                    ← LogFile.cpp、Arena.cpp、MatchShotTrace.hpp、MatchIngressTrace.hpp、AssetWatcher.cpp
│   │                                    ← （os-tool-files）tools/ui_editor 的 FileService.cpp、DocumentSession.cpp
│   ├── System：系統字型目錄             os-tool-files｜← ImGuiSdlRendererHost.cpp:39-111
│   ├── Process                          os-process-info｜執行檔路徑、程序與執行緒 CPU 時間、本地時間
│   │                                    ← LogFile.cpp:22-84、NetworkStatistics.cpp:11-80、main.cpp:65-75
│   ├── Console                          os-console-shutdown｜UTF-8 命令列、主控台 UTF-8 輸出、結束要求
│   │                                    ← match_main.cpp:10-81、tools/ui_editor 的 Main.cpp:71、CommandLine.cpp:46
│   ├── Thread                           os-threads-backend｜執行緒命名、優先度提示 ← RoleThread.cpp:11-80
│   ├── Wait                             os-wait-backend｜喚醒原語（kqueue／waitable timer／cv）← Waiter.cpp:11-160
│   └── Net（條件式）                    os-net-errors｜socket 錯誤碼的中立分類 ← ClientConnection.cpp:885-891
│
├── engine/io  （編在 GYO::Engine 內）   中立的檔案抽象；不含任何 OS 程式碼
│   ├── IFileSystem／Vfs／MountTable／stream／helpers   既有，不變
│   ├── 路徑契約                         io-contracts｜UTF-8、file:// 與原生絕對路徑的單一對應、ParseUri 修正
│   └── NativeFileSystem                 os-files-read｜file:// 後端，唯讀起步，所有 OS 呼叫交給 os::Files
│
├── engine/asset                         消費端 P3：讀檔改經 engine/io（NativeFileAssetSource 變成薄轉接）
├── engine/time      Waiter（公開 API 不變）       → 呼叫 os::Wait
├── engine/threads   RoleThread（公開 API 不變）   → 呼叫 os::Thread
├── engine/trace     TraceSink（位元組與順序不變） → 呼叫 os::Files（開放決策 2）
├── engine/platform/sdl（不動）          視窗、輸入、剪貼簿、SDL 時鐘、BaseDirectory；PathFromUtf8 移到 engine/os
│
└── 範圍外：asio（消費端的 socket）、third_party、建置層（CMake、CI、Python）、Go 轉送程序、
            shader pipeline 主機工具（開放決策 12）
```

依賴方向（A → B 表示 A 依賴 B；全部往下，沒有循環）：

```text
產品／工具 ──┬──→ engine/asset ──→ engine/io ──→ engine/os ──→ engine/base
             ├──→ engine/trace ──→ engine/threads ──→ engine/time ──→ engine/os
             ├──→ engine/platform/sdl ──→ engine/os（PathFromUtf8）、SDL
             └──→ engine/os（程序事實、主控台）
```

engine/os 不依賴 engine/io、Time、Threads、Trace、SDL、asio、GYO::Engine，也不依賴任何產品或工具（R5 與連結守衛）。檔案路徑上的方向是 asset → io → os → base（D41）。

## 3. 部件

### 3.1 OsError（os-skeleton）

- `enum class OsErrorCode : std::uint16_t { NotFound = 1, AccessDenied, InvalidPath, InvalidEncoding, TooLarge, IoFailed, Unsupported };`，`using OsError = Base::Error<OsErrorCode>;`，`static_assert(Base::CodedError<OsError>)`。
- detail 的固定格式：`errno=<n>` 或 `win32=<n>`；和路徑有關的錯誤再加 `; path_bytes=<n>`。message 原樣保留（error-handling.md 規則 5）。
- 上層（例如 IoError）用 `Base::CauseDetail(osError, ...)` 組 detail，不自創格式（對抗式檢查發現 `Error.hpp:57-70` 的格式與草案不同，已修正）。

### 3.2 Files：讀取原語（os-files-read）

- 責任：engine/os 中唯一直接碰原生檔案系統的地方。
  - 把 UTF-8 路徑轉成原生形式：Windows 用 `MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS)` 轉 UTF-16，POSIX 原樣使用。
  - 開檔讀取、讀取、查大小、Stat、Exists、絕對化（Windows 的磁碟機相對路徑要用 `GetFullPathNameW`）。
  - 失敗回傳帶原生錯誤碼的 OsError。
  - os-files-read 只處理一般長度的路徑；Windows 長路徑是消費端的 P1，寫在同一個 Windows 原語內部。
- API 草稿：

  ```cpp
  namespace Engine::Os::Files {
  enum class FileKind : std::uint8_t { None = 0, Regular, Directory, Other };
  struct FileStatus final { FileKind kind; std::uint64_t sizeBytes; std::chrono::nanoseconds modifiedSinceUnixEpoch; };
  class ReadFile final {            // move-only; native handle kept opaque (pimpl), no HANDLE or fd in the header
  public:
      [[nodiscard]] static Base::Result<ReadFile, OsError> Open(std::string_view utf8Path);
      [[nodiscard]] Base::Result<std::size_t, OsError> Read(std::span<std::byte> into);
      [[nodiscard]] Base::Result<std::uint64_t, OsError> Size() const;
      [[nodiscard]] Base::Result<void, OsError> Seek(std::uint64_t offset);
  };
  [[nodiscard]] Base::Result<FileStatus, OsError> Stat(std::string_view utf8Path);   // missing -> NotFound
  [[nodiscard]] Base::Result<bool, OsError> Exists(std::string_view utf8Path);
  [[nodiscard]] Base::Result<std::string, OsError> MakeAbsolute(std::string_view utf8Path); // generic '/' form, UTF-8
  [[nodiscard]] bool IsAbsolutePath(std::string_view utf8Path) noexcept; // per platform, pure string rule
  // D47: Result, not plain values -- invalid input is InvalidEncoding on every platform, never an exception
  [[nodiscard]] Base::Result<std::string, OsError> PathToUtf8(const std::filesystem::path&);
  [[nodiscard]] Base::Result<std::filesystem::path, OsError> PathFromUtf8(std::string_view);
  [[nodiscard]] bool DefaultCaseSensitivePaths() noexcept; // platform default only; volumes may differ
  }
  ```

  實作時的調整（2026-10-25，os-files-read 的 xhigh 審查之後）：
  - `PathToUtf8`／`PathFromUtf8` 改成回傳 Result（D47）。原因：MSVC 的 `std::filesystem::path` 遇到不合法的 UTF-8 或未配對 surrogate 會丟例外，違反 Invariant 5、6。
  - 新增 `IsAbsolutePath`。原因：engine/io 的原生後端不能自己判斷 OS，又必須拒絕「在這個平台不是絕對路徑」的 file URI 路徑，例如 Windows 上的 `/x`。
  - `ReadFile::Open` 只接受一般檔案，`Stat`／`Exists` 跟隨 symbolic link。詳見 os.md 的 Contracts。

- 不含：
  - Uri、IFileSystem、IStream、Vfs、mount、watcher（留在 engine/io，engine/os 不認識它們）。
  - 路徑字串的正規化規則（PathUtils）。
  - 從記憶體解碼（SDL_image、SDL_ttf、ufbx）。
  - 寫入原語（os-files-write）。

### 3.3 engine/io 的 NativeFileSystem（os-files-read）

- 責任：實作 `IFileSystem` 的 file:// 後端，所有 OS 呼叫交給 `Os::Files`，本身不含 OS 標頭、OS 巨集或 fstream。這實現了 `IFileSystem.hpp:26-28` 預告、由「platform 側」提供的原生實作；本計畫確定「platform 側」就是 engine/os 加上這個後端。
- 唯讀起步：
  - `Open` 只接受讀取模式，回傳包住 `Os::Files::ReadFile` 的 IStream；另外提供 `Exists`、`Stat`、`ToNativePathString`（呼叫 `NativePathFromFileUri`）。
  - `Capabilities()` 明確把寫入類欄位設成 false：canOpenWrite、canList、canIterate、canCreateDirectories、canRemove、**canRemoveRecursive**、canMove、canCopy、supportsWatch。`FileSystemCapabilities.hpp` 的預設值多半是 true，不能沿用。supportsToNativePath 設成 true。caseSensitivePaths 取自 `Os::Files::DefaultCaseSensitivePaths()`，io.md 註明實際值依磁碟區而異。
  - 其他方法回傳 `IoErrorCode::NotSupported`，等到有消費端需要時才實作。
- OsError 轉成 IoError 時，用 CauseDetail 保留原生錯誤碼。

### 3.4 engine/io 的路徑契約（io-contracts）

寫進新增的 `docs/architecture/io.md`（英文，含 `Design Invariants`），用測試守住：

1. Engine 字串裡的路徑一律是 UTF-8（D43）：Uri、Path、resolver 根目錄、IAssetSource 的路徑、engine/os 的 API。
2. file:// Uri 與原生絕對路徑只有一種對應，是純字串規則，三個平台結果相同（D44）：
   - `file:///Users/x` ↔ `/Users/x`（POSIX 根）；
   - `file:///C:/x` ↔ `C:/x`（磁碟機代號是第一個路徑段，RFC 8089 的形式）；
   - `file://server/share/x` ↔ `//server/share/x`（UNC）；是否在這一輪支援 UNC 根目錄，見開放決策 14。
   - `file://localhost/...` 等同空 authority。
   - `file://C:/x`（磁碟機代號放在 authority）回傳 InvalidPath。
   - 路徑位元組直接是 UTF-8，不做百分比編碼。
   - file:// 只表示絕對路徑；`..` 仍然拒絕。
   - POSIX 上字面為 `/C:/x` 的路徑無法表示，這個限制寫進 io.md。
3. 新規則只套用在 file://。`ParseUriLoose` 對其他 scheme 保留現行語意（例如 `res:///a` 變成相對路徑是刻意的），並用測試釘住。
4. 長路徑前綴（`\\?\`、`\\?\UNC\`）不出現在 engine/io 的字串裡。`PathUtils` 的 `NormalizeSlashes` 會把它改寫成 `//?/`（`PathUtils.cpp:72、257、320`），所以前綴只能在 engine/os 內產生。

要修的位置：

- `Uri.cpp:118-121`：ParseUri 剝掉開頭的 `/`。
- `Uri.cpp:159-179`：ParseUriLoose。
- `Uri.cpp:43-46`：ToString 在 authority 為空、path 為 `C:/x` 時輸出新規則拒絕的 `file://C:/x`，往返會壞。
- `MountTable.hpp:30、54-60`：經 ToString 再 ParseUriLoose 的往返。
- `DirectoryEntry.hpp:14`、`MountPoint.hpp:17、26` 的範例註解。

新增純字串函式 `FileUriFromNativePath`、`NativePathFromFileUri`。

### 3.5 Process（os-process-info）

- 責任：目前程序的 OS 事實。
  - 執行檔映像的路徑（UTF-8）。
  - 程序 CPU 時間、目前執行緒的 CPU 時間。
  - 把 `system_clock` 時間點轉成本地日曆欄位與 UTC 偏移：吸收 `localtime_s`／`localtime_r` 與 `%z` 的平台差異。格式化留給呼叫端。
- 查詢每次都向 OS 讀，不快取。
- 來源：`LogFile.cpp:22-30、38-46、57-84`、`main.cpp:65-75`（localtime 的同一知識寫了第二次）、`NetworkStatistics.cpp:11-38、58-80`。
- 「執行檔在哪裡」與 platform/sdl 的 `BaseDirectory()` 是不同的事實，見開放決策 8。

### 3.6 Console（os-console-shutdown）

- 責任：
  - 不經 SDL 入口的程式取得 UTF-8 argv（Windows 用 `GetCommandLineW`、`CommandLineToArgvW`）。
  - 主控台的 UTF-8 輸出：`main.cpp:137` 等處把 UTF-8 寫到 Windows 主控台，但沒有設定輸出 code page。這是對抗式檢查補上的站點。
  - 結束要求：整個程序只安裝一次。POSIX 用 `sigaction` 處理 SIGINT／SIGTERM，handler 只寫 lock-free atomic；Windows 用 `SetConsoleCtrlHandler`，範圍見開放決策 9。
- 結束要求旗標是 engine/os 唯一的程序層級狀態，由它擁有（State Authority）。收到結束要求後做什麼是呼叫端的政策。
- 來源：`match_main.cpp:10、22-23、35、50-54、81`，`tools/ui_editor/src/Main.cpp:71`、`CommandLine.cpp:46`。

### 3.7 Thread（os-threads-backend）

- 責任：在目前執行緒上套用名稱與優先度提示，回傳是否套用。`RoleThread` 的 Start 順序、`ThreadPriority` 公開型別、停止與 join 都留在 engine/threads。
- API：`SetCurrentName(std::string_view utf8Name)`、`SetCurrentPriority(PriorityHint)`，回傳 bool。
- 內容逐行搬自 `RoleThread.cpp:43-80`，含 `#else` 後備。
- `:53` 逐 byte 擴寬成 UTF-16 是已知缺陷：搬移時原樣保留，另外排批修正（行為變更）。

### 3.8 Wait（os-wait-backend）

- 責任：只提供 OS 的喚醒原語：Create、Signal、Block（可選的相對逾時）、回報機制種類。
  - 內容逐行搬自 `Waiter.cpp:11-30、54-121、129-160`：macOS 的 kqueue（EVFILT_USER＋EVFILT_TIMER，NOTE_NSECONDS|NOTE_CRITICAL）；Windows 的 auto-reset event 加高解析度 waitable timer，含兩層退回；其他平台的 condition_variable。這個 `#else` 是通用後備，不是 Linux 專屬。
- 留在 engine/time 的是 Time 的語意與權威狀態：latch、單一等待者、deadline 迴圈、醒後重讀時間基準、LateSince、WaitBackend 列舉與字串。
- cv 後備必須在 mutex 內讀 Time 傳入的 latch 參照，不保存副本；漏掉就會 lost wakeup。
- Block 保留現行的例外規格：`Waiter.cpp:116-121` 不是 noexcept，搬移時不改。

### 3.9 Net（os-net-errors，條件式）

- 只做「`std::error_code` 對應到哪個中立分類」。engine/os 不依賴 asio，不碰 native handle。
- 「哪些分類算暫時性」是消費端的政策（`ClientConnection.hpp:89-91`），留在消費端。
- 這段原始碼本來就沒有 OS 巨集，守衛偵測不到。要不要做，見開放決策 7。

## 4. 邊界

- **engine/io**：
  - 檔案抽象的 owner，位在 engine/os 之上。保留 IFileSystem、Vfs、MountTable、Uri、Path、stream、helpers，新增 NativeFileSystem 後端。
  - 原生路徑在 engine/io 裡一律是 UTF-8 的通用形式：分隔符是 `/`，例如 `C:/a`、`/a`。轉成反斜線、UTF-16、`\\?\` 都是 engine/os 的事。
  - engine/io 目前沒有自己的 target，編在 GYO::Engine 裡（`engine/CMakeLists.txt:13-45`），所以邊是 GYO::Engine → GYO::Os。
- **engine/asset**：
  - `IAssetSource` 的註解（「filesystem / pak / zip / memory などの抽象」）和 `IFileSystem` 的責任重複（D42）。
  - 消費端的 P3 讓 asset 的讀檔改經 engine/io；`NativeFileAssetSource` 保留類別名與 API，當薄轉接，讓產品與工具的建構點（`PvpApplication.cpp:91`、兩個 `ObjectFpsApplication.cpp:40`、`AssetPreviewContext.cpp:93`）不必改。
  - IAssetSource 的去留，見開放決策 4。
- **engine/platform/sdl**：
  - 視窗、輸入、剪貼簿、GPU、事件時間戳、SDL_main 入口、SDL 時鐘域照舊，Design Invariants 1-9 不動。engine/os 不連 SDL、不呼叫 SDL_Init。
  - `PathFromUtf8`（`SdlPlatform.cpp:225-230`）只用 std，在 os-path-utf8-sweep 移到 engine/os，呼叫端同一批切換，不留轉發。這是 platform-sdl 公開 API 的 Ownership 變更。
  - 不連 SDL 的程序需要執行檔路徑，這碰到 platform-sdl.md 的 Replacement Criteria 1。解法是用 engine/os，不是另做非 SDL 的 platform 後端；這點寫進 Delta。
- **asio**：
  - 只在消費端（`apps/object_fps_pvp/CMakeLists.txt:8-9、37、40`）；Engine 沒有 socket 層。
  - asio 的 INTERFACE 定義（`third_party/asio/CMakeLists.txt:10-15`）會把 windows.h 間接帶進產品，文字守衛抓不到，產品端的 NOMINMAX 暫時不能移除。這是已知盲點。
- **engine/time、engine/threads、engine/trace**：
  - 依賴順序是 Base ← Os ← Time ← Threads ← Trace。
  - Waiter 與 RoleThread 的公開 API、WaitBackend 字串（消費端日誌 `wait_backend=`，`NetworkStatistics.cpp:106`）、name_applied 與 priority_applied 的結果都不變。
  - 三個設定期 exactly 守衛要改期望值，依 AGENTS §14.4 事先核准：`tests/common/time/CMakeLists.txt:31-32`；Trace 接上 Os 時的 `tests/common/trace/CMakeLists.txt:11` 與 trace.md Invariant 7。
  - trace.md Invariants 1-5 與 Windows 文字模式的 CRLF（`TraceSinkTests.cpp:32-38`、`DiagnosticFileTests.cpp:29`）在任何搬移中都不改。
  - `FileOpenMode.hpp:23` 已有的 Text 旗標（交給 OS 做換行轉換）是換行語意的既有契約，開放決策 2 以它為起點。
- **third_party**：
  - engine/os 只用 OS SDK 與 std，不引入第三方函式庫；third_party 不掃也不改。
  - 會用路徑開檔的第三方 API（ImGui 的 `AddFontFromFileTTF`、SDL 的 `SDL_SaveBMP`、`SDL_IOFromFile`）由第三方吸收 OS 差異，在 os.md 列為 R4 不管的範圍；呼叫端要用 `Os::Files::PathToUtf8` 交 UTF-8。SDL 的 Windows 檔案 IO 不加 `\\?\`，所以長路徑仍受限〔推論〕。
- **建置層**：
  - CMake 的 WIN32／APPLE／UNIX、MSVC 旗標、CI workflow、Python 驗收腳本、`engine/config` 的平台欄都在 os.md 列為範圍外。
  - 例外：非 os 的 target 不可從 CMake 依 OS 定義識別用的巨集（例如 `$<$<PLATFORM_ID:Windows>:GYO_WIN>`），否則可以繞過 R2。由設定期守衛檢查。
  - repo 沒有 Windows application manifest（longPathAware、activeCodePage=UTF-8）。LongPathsEnabled 是使用者機器的登錄值，消費端控制不了，所以長路徑只能靠 engine/os 的 `\\?\` 加寬字元 API。

## 5. Design Invariants 與 Replacement Criteria（草案）

以下是 os-skeleton 寫進 `docs/architecture/os.md` 的內容草案（屆時以英文撰寫，見出し用 `Design Invariants`）。依 AGENTS §3，新增這一節需要使用者同意。io-contracts 寫 io.md 的節（第 3.4 節）時同樣需要同意。

### Design Invariants

1. **OS 知識只在 engine/os。**
   - 範圍：engine/os/src 以外的 C/C++ 原始碼（engine、apps、tools、build/acceptance、tests；tests/common/os 除外）。
   - 這些原始碼不 include OS 標頭（R1），不在前處理指令裡判斷 OS 巨集（R2），也不呼叫列管的 OS 或 CRT 分裂 API（R3）。
   - R2 的巨集包含 SDL_PLATFORM_*、__MINGW32__、__CYGWIN__，以及 `__has_include(<OS 標頭>)`。
   - 編譯器巨集（_MSC_VER、__clang__、__GNUC__）與架構巨集（_M_X64、__aarch64__）不算 OS 判斷。
   - 例外只能寫在 owner 目錄下只縮不增的 allowlist。
2. **原生檔案存取只經 engine/os**（R4）。
   - 範圍：engine（engine/os 與建置期主機工具除外，engine/io 包含在內）、apps、tools。
   - 這些程式碼不直接使用 fstream 系列、fopen、_wfopen，也不使用 `std::filesystem` 的任何會碰檔案系統的操作；只允許 path 型別與純字串操作。
   - 檔案經 engine/io 的抽象；程序事實直接呼叫 engine/os。
   - tests 與 build/acceptance 不受 R4 限制，它們只寫開發者控制的暫存路徑。
3. **engine/os 是 OS 差異的最底層**（R5 加上連結守衛）。
   - GYO::Os 只連結 GYO::Base 與 OS 系統函式庫；cv 後端搬入後加 Threads::Threads。
   - engine/os 只 include engine/base、標準庫與 OS 標頭。
   - API 不出現 Uri、IFileSystem、IStream 這類上層型別。
4. **公開標頭不外洩 OS。**
   - engine/os/include 只 include 標準庫與 engine/base。
   - 持有原生 handle 的類別用 pimpl 或不透明儲存，不出現 HANDLE 或 fd 型別。
   - include 它的程式不需要 NOMINMAX。
5. **中立型別與 UTF-8。**
   - 路徑是 UTF-8 的 `std::string_view`（輸出是 UTF-8 `std::string`），資料是 byte span，時間是 `std::chrono`。
   - 不合法的 UTF-8 回傳 InvalidEncoding。
   - Windows 上一律轉成 UTF-16、呼叫寬字元 API，絕不走 ACP 窄字元 API。
   - 長路徑形式只在 engine/os 內、呼叫 OS 前產生，不回傳，也不寫進錯誤訊息。
6. **失敗是 Runtime Error。**
   - 回傳 `Base::Result<_, OsError>`，detail 依第 3.1 節的格式保留原生錯誤碼。
   - 例外不穿過公開邊界，也不穿過 OS 回呼（signal handler、console ctrl handler）。
   - error-handling.md「Exception at the platform boundary」的範圍，在 os-console-shutdown 批延伸到 engine/os（需同意）。
7. **不帶產品知識、不含政策。**
   - 不出現產品或工具的名稱、檔名、schema、日誌欄位。
   - 哪些 socket 錯誤算暫時性、收到結束要求後做什麼、時間戳怎麼格式化，都由呼叫端決定。
8. **狀態的權威**（AGENTS §3）。
   - engine/os 只擁有自己的 OS 資源與結束要求旗標。
   - 查詢每次都向 OS 讀，不快取來做判斷。
   - Waiter 的 latch 屬於 Time，engine/os 只在 cv 後備的 mutex 內讀它的參照。
9. **搬移不改可觀察行為。**
   - 檔案位元組（Windows 的 CRLF、append 語意）、WaitBackend 字串、名稱截斷規則、優先度對應、逾時換算（Windows 100 ns 進位、毫秒退回、kqueue NOTE_NSECONDS）、例外規格都維持不變。
   - 唯一允許的差異是該批事先宣告的變更（標 ★）。
10. **每個能力在每個 OS 都有明確結果。**
    - 不需要 OS 呼叫就能滿足的要求，視為已套用。例如 Normal 優先度：現行 `RoleThread.cpp:74、78` 直接回傳 true。
    - 需要 OS 呼叫卻沒有做時，回報未套用或 Unsupported，不可以無聲回報成功。
    - 各 OS 做不到同一行為時，差異寫在型別或文件裡。

### Replacement Criteria

1. 有消費端需要 engine/os 交出原生 handle（socket、HANDLE、fd）才能完成工作，或必須知道自己在哪個 OS 才能正確使用中立函式：停止補參數，提出原生 handle 層或重新設計該部件。
2. 新增或修改 engine/os 的任何部件，都得重跑 Wait 的計時 L2 才能放心：提出把 Wait 拆成獨立部件。
3. engine/os 為了實作某個能力，需要依賴 engine/io、Time、Threads、Trace、SDL、asio 或 GYO::Engine：分層畫錯了，停下來交使用者決定，不加這條邊。
4. 有消費端需要非同步或事件式檔案 I/O、目錄監看或大型目錄列舉，`Os::Files` 的窄原語必須長成另一個 IFileSystem：停下，重新劃分 engine/os 與 engine/io 的分工。
5. 為了維持呼叫端的位元組（例如 CRLF），engine/io 或 engine/os 必須維護 OS 專屬的 stream 包裝：提出把換行語意當成 Data Contract 決定。
6. allowlist 連續兩批沒有縮小反而變大，或 tests 以外需要 3 條以上永久條目，或誤報需要反覆調整規則：重寫規則或縮小 Invariant 1 的範圍，不再加例外。
7. 同一能力在同一 OS 上要跨兩個以上 engine/os 檔案分支、各 OS 檔案大半重複，或要加入程序與檔案模型和 POSIX、Win32 都不同的平台：提出改成後端介面加選擇表。

## 6. 守衛

- **位置與 owner**：
  - `tests/common/os/os_boundary_guard.py`，只用 Python 標準庫，由 Engine 擁有。
  - 註冊為 CTest `os.boundary_guard`、`os.boundary_guard_selftest`（unittest，合成 fixture 在 `tests/common/os/fixtures/guard_tree/`）。
  - 另有 `OsHeaderHygieneTests`（doctest）。
  - 守衛只掃文字，四平台 L1 都跑，結果應該和平台無關。
- **掃描範圍**：
  - 頂層根目錄 engine、apps、tools、build/acceptance、tests，副檔名 .c .cc .cpp .cxx .h .hh .hpp .hxx .inl .mm。
  - 排除 engine/os/src、tests/common/os、third_party、build/target。
  - 每個存在的頂層根目錄至少要掃到一個檔，避免路徑錯了卻空跑通過。build/acceptance 底下有些子目錄沒有 C/C++ 檔（common、ui_editor），所以根目錄以頂層計算，不逐一展開子目錄。
  - 不寫任何產品名稱，刪掉任何產品都不必改守衛（AGENTS §7、§9）。
- **分期**：
  - os-skeleton 先只掃 Engine 擁有的 engine 與 tests/common，配 Engine 的 allowlist。
  - os-guard-wide（發行之後）才擴到 apps、tools、tests/<產品>、build/acceptance，配各 owner 的 allowlist。這樣 hotfix 的關鍵路徑最短。
- **比對方式**（沿用 `tests/object_fps_pvp/sdl_symbol_guard.py` 的 strip_comments_and_strings；Engine 腳本自備一份，不 import 產品的檔案）：
  - R1 與 R5 對**原始的 #include 行**比對。清過字串的文字會把引號內的路徑清掉，R5 會殺不到突變。
  - R2 只在清過的 #if／#ifdef／#ifndef／#elif／defined(...)／__has_include 裡比對。
  - R3、R4 在清過的程式碼比對，列管清單集中在一處。
  - 每個發現印一行 `file:line: R<n> <match>`，有發現時結束碼 1。
  - engine/os/include 另做公開標頭掃描：只要 include OS 標頭就失敗。
- **allowlist**（只縮不增的 ratchet，比照 `HeaderHygieneTests.cpp:75-104` 的 G2）：
  - 檔名一律是 `os_boundary_allowlist.txt`，放在 owner 目錄，每條只能列該目錄樹下的檔案，越界就失敗。
  - 格式是 `path rule count # reason`。實際數量大於 count、小於 count、或列出的檔案不存在，都失敗。
  - 基準用空 allowlist 實跑產生，和盤點對照；不一致就記錄到 HANDOFF，不放寬樣式。
  - 目標狀態：tests 以外全空，tests 只留理由明確的永久條目（例如 `TraceSinkTests.cpp:255` 的 /dev/full、`EntryTests.cpp:10` 的 sys/wait.h）。
- **標頭衛生**：
  - `OsHeaderHygieneTests` 只檢查 Windows 的識別巨集（_WINDOWS_、_INC_WINDOWS、_WINSOCK2API_、min／max）。
  - POSIX 這一側靠 engine/os/include 的文字掃描。原因：在 macOS 的 libc++ 與 Linux 的 libstdc++，只要 include `<string>`、`<filesystem>` 就會定義 `_PTHREAD_H`，用巨集檢查的話，沒有違規也會失敗（對抗式檢查在本機實測）。
- **突變**：
  - 目錄在本夾的 `scripts/mutations.json`。
  - os-skeleton 先擴充 `time-threads-trace/scripts/run_mutations.py`，加上「command」型檢查，直接執行守衛腳本。現行的 check() 只能建置 target 並執行同名執行檔（`run_mutations.py:48-55`），跑不了 Python 守衛。
  - 文字層的突變要在四平台都能編譯，例如放在 `#if 0` 區塊內的 #include 與 API 呼叫：R1 看原始行、R3 不看 #if 0，都抓得到。
  - 預定的突變：os-g1（R1）、os-g2（R2）、os-g3（R3）、os-g4（在 engine/io 插入 fstream，R4，守住 D41）、os-g5（R5）、os-g6（allowlist count 加 1）、os-g7（公開標頭）、os-g8（gyo_os 連 GYO::Time，設定期失敗）、os-g9（從巨集清單刪掉 __linux__，selftest 失敗）。
  - 只有預期的檢查失敗、而且輸出符合 expect 正則，才算 killed。依消費端 D184 的作法，每批只重跑相關突變，PR 線合併前在背景完整跑一次。

## 7. 批次

檔位依 agents/effort-gearbox.md。ultracode 與高於主對話的檔位，在批次開始時說明並徵求同意。估時是實作者的工時，不含等待 CI 與使用者。

### 7.0 os-plan-docs：寫計畫交核准（medium，3～5 h）

- 做：本夾的 README、PLAN、HANDOFF 與 dev_log。
- 不做：任何程式；os.md 與 io.md 的本文（屬於 os-skeleton、io-contracts）。
- 通過的定義：
  - 使用者核准 PLAN。
  - 回答開放決策 1（剩餘部件的時程）、14（UNC）、15（PR 的形態）。
  - 同意第 9 節的 A1～A3 Delta：新 target GYO::Os、os.md 與 io.md 的 Design Invariants 節。
- 結果：2026-10-25 全部達成（D46）。

### 7.1 os-skeleton（A1）：建立 engine/os 與守衛（high，6～9 h）

- 白話：建立 engine/os 目錄與 GYO::Os，寫下它的設計約束，並加上掃描守衛，把 Engine 現有的 OS 站點凍結在只能縮小的清單裡。
- 依賴：os-plan-docs。
- 做：
  - `engine/os/CMakeLists.txt`（gyo_os、GYO::Os；在 `engine/CMakeLists.txt` 的 math 之後、time 之前加 `add_subdirectory(os)`）。
  - `OsError.hpp`。
  - `docs/architecture/os.md`（Design Invariants 1-10、Replacement Criteria 1-7）。
  - 守衛 R1～R5（只掃 engine 與 tests/common）、公開標頭掃描、selftest 與 fixtures、`gyo_expect_links(gyo_os GYO::Base)`（DEFER）、Engine 的 allowlist。
  - 突變 runner 的 command 型擴充；突變 os-g1～os-g9。
- 不做：任何執行期行為變更、搬任何站點、產品與工具的 allowlist（os-guard-wide）。
- 驗收：
  - L1（四平台）：守衛、selftest 通過。
  - 空 allowlist 的輸出和盤點的 Engine 檔案集合一致，不一致就記錄。
  - 突變全部 killed，而且只靠預期訊息判定。
  - fixture 證明刪掉合成產品不必改共通檔。
  - L2、L3：無。
- 停止條件：守衛的誤報需要放寬樣式才能通過時，停下回報（Replacement Criteria 6）。
- 支援的 Feature：消費端的長路徑修正（P1）必須寫在 engine/os 內並遵守它的約束；之後每批搬移，都以 allowlist 縮小作為客觀的完成判定。

### 7.2 io-contracts（A2）：engine/io 的路徑契約（high；字串規則局部 xhigh；契約審查 ultracode，1 審查者＋1 對抗檢查，先問；6～8 h）

- 白話：定下 engine/io 的路徑契約，包括路徑字串一律 UTF-8、file:// 與原生路徑只有一種對應並修好 ParseUri、IO 錯誤保留作業系統的錯誤碼。
- 依賴：os-plan-docs。和 os-skeleton 可以並行：它是純字串，不需要 A1 的產物。
- 做：
  - `docs/architecture/io.md`。
  - 第 3.4 節列出的全部修正：ParseUri、ParseUriLoose 只對 file:// 套新規則、ToString 的往返、MountTable 的往返、範例註解。
  - `FileUriFromNativePath`、`NativePathFromFileUri`。
  - IoError detail 依 CauseDetail；UNC 依開放決策 14。
  - 依 §14.3 先寫失敗的測試：`file:///Users/x` 必須解析成 `/Users/x`。舊碼得到 `Users/x`，所以是 Red。
- 不做：原生後端；asset 接線與產品呼叫端（P3、P4）。
- 驗收：
  - L1（四平台，純字串，三平台結果相同）：UriTests 涵蓋以下情況，先在舊碼確認 Red，修正後 Green：
    - POSIX 根、`C:/`、`c:/`、localhost、空 authority；
    - `file://C:/` 被拒、`..` 被拒；
    - 非 ASCII 的 UTF-8 原樣保留；
    - ToString 往返、MountTable 往返、Loose 對其他 scheme 的既有語意。
  - 既有 VfsTests 與 AssetPathResolverTests 不改判定也通過；需要改判定時，依 §14.4 報告。
  - 突變：拿掉開頭斜線的保留、磁碟機判斷只認大寫、ToString 回到舊形式，三個都必須被殺。
- 停止條件：AssetPathResolver 的 catalog 路徑解析受影響，需要改既有判定時，停下回報。
- 支援的 Feature：消費端的 P3 把 resolver 產生的原生絕對路徑轉成 file:// Uri 時，需要這個對應；之後工具的 VFS 也能 mount 到原生目錄。

### 7.3 os-files-read（A3）：唯讀檔案原語與原生後端（high；UTF-8↔UTF-16 與 Windows 錯誤對應局部 xhigh；公開介面由 1 位 xhigh 審查 agent 檢查，開始時徵求同意；8～12 h）

- 白話：在 engine/os 寫好三個平台的唯讀檔案原語（UTF-8 路徑轉原生路徑），並在 engine/io 加上呼叫它們的唯讀原生檔案系統。
- 依賴：os-skeleton、io-contracts。
- 做：
  - 第 3.2、3.3 節。Windows 用 CreateFileW／ReadFile，POSIX 用 open／read／fstat。
  - GYO::Engine → PRIVATE GYO::Os。
  - Windows 上加一個只記錄的測試：超過 260 字元的路徑目前回報什麼原生錯誤碼。讓消費端的 P1 一開始就有現成的 Red，P2 也有格式可以比對。
- 不做：長路徑（P1）、asset 接線（P3）、寫入、列舉、platform/sdl 的 PathFromUtf8 移除。
- 驗收：
  - L1（四平台）：
    - FilesTests 用暫存目錄讀寫比對。
    - 含 ACP 以外字元的路徑可讀。批次開始時要先確認 CI runner 的 ACP，選它表示不了的字元，否則測試沒有檢出力。
    - 不合法的 UTF-8 回傳 InvalidEncoding；不存在回傳 NotFound 且帶原生碼。
    - NativeFileSystemTests 經 file:// Uri 讀取、Stat、Exists；寫入類方法回傳 NotSupported；Capabilities 的寫入欄位為 false。
    - 守衛顯示 engine/io 沒有任何 R1～R4 命中。
  - 突變：Windows 改用 CP_ACP 轉換（windows-x64 上必須被非 ACP 測試殺死）、NotFound 對應錯誤、Capabilities 回到預設值，各一個。
- 停止條件：IStream 包裝的 seek 與讀取語意和 BufferedStreamTests、StreamReaderTests 的期待不一致時，停下回報。
- 支援的 Feature：消費端 P1 的長路徑寫在這個原語內，P3 的 asset 讀檔經這個後端；之後產品與工具的檔案存取不再直接用 fstream。

### 7.4 消費端的長路徑修正（交接點，不屬於本計畫）

見第 8 節。A1～A3 完成後，由消費端的發行任務進行。

### 7.5 os-guard-wide（A1b）：守衛擴到產品與工具（medium；規則調整 high；4～6 h）

- 白話：把守衛的掃描範圍擴到產品、工具、產品測試與驗收程式，替各 owner 建立只能縮小的例外清單。
- 依賴：os-skeleton。排在消費端發行之後。
- 做：
  - 各 owner 的 allowlist 基準（產品 owner 的內容變更）。
  - `OsHeaderHygieneTests`。
  - 設定期守衛：非 os target 不可依 OS 定義識別巨集。
  - 其餘突變。
- 驗收：L1（四平台）全綠；基準和盤點對照，並記錄差異。

### 7.6 os-process-info：程序事實（high，6～9 h）

- 白話：把執行檔路徑、程序與執行緒的 CPU 時間、本地時間搬進 engine/os，消費端的統計與日誌時間戳不再寫 OS 判斷。
- 依賴：os-guard-wide。
- 做：
  - 第 3.5 節的 API。
  - 改 `LogFile.cpp`、`NetworkStatistics.cpp`、`main.cpp` 的對應段落。消費端照原本的字串格式自己組時間戳。
  - 開始前先在 Windows 確認 UCRT `%z` 的現行輸出；新實作會改變格式時，當成 Data Contract 變更宣告。
- 驗收：
  - L1：日誌時間戳在固定 time_point 與固定 TZ 下，逐 byte 等於舊實作；執行檔路徑指向測試執行檔本身；CPU 時間不會減少；allowlist 縮小。
  - L3（只記錄）：Windows 與 Linux 的 exe sha、log 檔名、時間戳和前一版相同。

### 7.7 os-console-shutdown：命令列、主控台輸出與結束要求（xhigh；★ Windows 行為變更；8～12 h）

- 白話：把結束訊號、命令列與主控台輸出搬進 engine/os。Linux 與 macOS 行為不變；Windows 的伺服器端程序也能優雅結束；沒有 SDL 入口的程式也能拿到 UTF-8 命令列，主控台也能正確顯示 UTF-8。
- 依賴：os-process-info。
- 做：
  - 第 3.6 節。
  - error-handling.md 平台邊界節延伸到 engine/os（需同意）。
  - 連結守衛明列 shell32。
- 驗收：
  - L1：POSIX 子程序送 SIGINT／SIGTERM，斷言 Requested 與 Reason。
  - Windows 對新程序群組送 CTRL_BREAK；能否在 CI 做，批次開始時確認，不能就記錄缺口。
  - CommandLineUtf8 以非 ACP 參數測試。
  - 突變：拿掉 SIGTERM 註冊、拿掉 CTRL_CLOSE 分支。
  - L3（事前宣告，只記錄）：Windows 實機用 Ctrl+C、Ctrl+Break、關閉主控台結束伺服器端程序，log 最後一行都是 end 紀錄。

### 7.8 os-files-write：寫入原語與消費端的寫檔（high，10～14 h）

- 白話：擴充寫檔、建目錄、原子取代等原語，讓消費端剩下的直接開檔都改走 engine/io。
- 依賴：os-files-read、消費端的長路徑修正。
- 做：
  - WriteFile（truncate／append）、CreateDirectories、ReplaceFile、SamePathComponent；NativeFileSystem 開放對應方法與 Capabilities。
  - 改 `Arena.cpp:64、127`、`LogFile.cpp:87、129`、`MatchShotTrace.hpp:286`、`MatchIngressTrace.hpp:255`、`AssetWatcher.cpp:178、186`。
  - 換行：寫出的位元組必須和現在的文字模式 ofstream 相同（Windows 為 CRLF），做法依開放決策 2。
  - 依消費端 D184 重新定位約 29 個消費端突變。
- 驗收：
  - L1：既有測試不改判定；Windows 長路徑的 arena 與 log 案例 Red→Green。
  - L3：Windows 深路徑（P 約 235）下寫得出 log、讀得到 arenas.json。

### 7.9 os-tool-files：編輯器的檔案服務與系統字型目錄（high，6～9 h）

- 白話：把 UI 編輯器的原子存檔、路徑比較與系統字型目錄改用 engine/os，編輯器不再自己判斷 OS。
- 依賴：os-files-write。
- 做：
  - `tools/ui_editor` 的 FileService.cpp（:13-92 的 MoveFileExW 與 towlower 比較、:29 的 weakly_canonical、:111-135 的 exists／is_regular_file／file_size／last_write_time）。
  - DocumentSession.cpp:229。
  - ImGuiSdlRendererHost.cpp:39-111 的系統字型目錄，改由 engine/os 提供 FontDirectories；字型檔名的選擇留在工具。
  - 和延後的 AtomicWriteFile（輸入與呈現計畫 IP-5）協調，只留一份實作。
- 驗收：L1：工具既有測試不改判定；ReplaceFile 能覆蓋既有檔，失敗時原檔不變。

### 7.10 os-trace-writers：trace 寫檔（high，3～5 h）

- 白話：讓 trace 寫檔也走 engine/os，寫出的位元組（含 Windows 的 CRLF）、喚醒與順序都不變。
- 依賴：os-files-write；依開放決策 2。
- 做：改 `TraceSink.cpp:102`；修訂 trace.md Invariant 7 與 `tests/common/trace/CMakeLists.txt:11`，依 §14.4 先寫 1～5 點並取得同意；更新 tt2-* 突變。
- 驗收：L1：TraceSinkTests 不改判定；Windows 長路徑 trace 案例 Red→Green；tt2 突變照舊被殺。

### 7.11 os-path-utf8-sweep：剩下的窄字串路徑（high，8～12 h）

- 白話：把產品與工具剩下的窄字串路徑轉換都改成 UTF-8，並把 PathFromUtf8 從 SDL 模組移到 engine/os，讓非 ASCII 資料夾在各處都能用。
- 依賴：os-files-write、os-tool-files、os-console-shutdown。
- 做：
  - 移除 platform/sdl 的 PathFromUtf8，呼叫端改用 `Os::Files::PathFromUtf8`。
  - 用 rg 列出剩下的 `.string()` 與 `path(const char*)`，由審查者逐一對照。對抗式檢查補上的站點包括：`match_main.cpp:67`、`MovementTraceWriter.hpp:81`、`Arena.cpp:150`、`LogFile.cpp:132`、`TraceSink.cpp:104、110`、`EditorApp.cpp:603、722、2096`、`DocumentSession.cpp:233-234`、`build/acceptance` 的 SDL_SaveBMP 呼叫端（VisualProbe.cpp:130、MuzzleProbe.cpp:49、object_fps_v2/main.cpp:81）、`tests/common/ui/UiRendererTests.cpp:133-186`。
  - 同步 inactive_products.md。
- 驗收：L1：含 ACP 以外字元的根目錄跑 asset 載入、log 與 trace 寫出、編輯器匯出；Windows 上先在舊樹確認 Red。

### 7.12 time-threads-invariants：先替 time 與 threads 寫約束（medium，3～4 h）

- 白話：在搬動計時核心之前，先替 time 與 threads 寫下可以逐條檢查的設計約束。
- 依賴：os-plan-docs。可以和任何批並行，不改程式。
- 做：
  - `docs/architecture/time.md` 與 `threads.md`（英文）。內容取自 `Waiter.hpp:53-65`、`RoleThread.hpp:31-95`、time-threads-trace 的 TT-1 與 D29。
  - 每一條對應到現有守衛；對不到的標「無守衛」。
- 通過的定義：使用者同意這兩個 Design Invariants 節。

### 7.13 os-wake-record：計時量測基準（medium，3～5 h，另加各平台實機約 2 h）

- 白話：把晚醒量測程式合回 master，並在四種平台重新量一次基準，給計時搬動做前後對照。
- 依賴：os-plan-docs。可以並行。
- 做：
  - 從本機分支 `claude/tt1-l2-wake-record`（`a8f9535`）合入 `tests/common/time/WakeRecord.cpp`；衝突時照 TT-1 L2 的定義重建。
  - 補 notify→wake 段落。
  - 量基準：macOS arm64、macOS Intel 實機、Windows x64 的 CLI 與視窗、Linux x64。其中 Windows 上不初始化 SDL 的程序是第一次量。
  - 和消費端規劃中的同類量測協調，只做一次。
- 驗收：L2（事前宣告並核准、只記錄）同 TT-1 L2。證據放 `build/target/_build/test/logs/os-wake-baseline-<日期>/`。

### 7.14 os-threads-backend：執行緒命名與優先度（high；Start 順序局部 xhigh；4～6 h，另加 L2 約 2～3 h）

- 白話：把執行緒命名與優先度的作業系統呼叫搬進 engine/os；RoleThread 的介面與啟動順序不變，每個平台的結果和搬動前相同。
- 依賴：os-skeleton、time-threads-invariants、os-wake-record。
- 做：
  - 第 3.7 節。
  - gyo_threads → PRIVATE GYO::Os；`tests/common/time/CMakeLists.txt:32` 的期望值依 §14.4 先核准。
- 驗收：
  - L1：RoleThreadTests 不改判定；新增讀回測試確認名稱真的套用。
  - L2（事前宣告、只記錄，附撤回條件）：A＝搬動前的 master，B＝搬動後。各平台啟動消費端，每個 role started 行的 name_applied、priority_applied、wait_backend 在 A 和 B 必須逐字相同；任一項不同就撤回整批。

### 7.15 os-wait-backend：等待原語（xhigh；整批由 1 位 xhigh 審查 agent 檢查，開始時徵求同意；8～12 h，另加 L2 約 4～6 h）

- 白話：最後才把等待原語的作業系統後端搬進 engine/os；Waiter 的語意與公開介面不改，並用事先宣告的交錯前後對照證明計時沒有變差。
- 依賴：os-threads-backend。
- 做：
  - 第 3.8 節；gyo_time 的期望值依 §14.4 先核准。
  - 6 個 Waiter 突變重新定位。
  - 另加「cv 後備在 mutex 外檢查 latch」的突變，必須被 notify 競爭測試殺死；殺不到就補測試。
- 驗收：
  - L1：WaiterTests、RoleThreadTests、TraceSinkTests 不改判定；新增 WakeSignalTests。
  - L2：事前宣告，判定數值在看結果之前經使用者核准並凍結（開放決策 13，§14.1）。順序 ABABABAB；平台 macOS arm64、macOS Intel 實機、Windows x64 的 CLI 與視窗、Linux x64。
  - 不通過時：先回報使用者，不在同一批調整；撤回方式是 revert，Waiter.cpp 回到 allowlist。
  - L3：消費端 LAN 場次的 `wait_backend=` 值與搬移前相同。

### 7.16 os-net-errors：socket 錯誤分類（high，3～5 h，條件式）

- 白話：（依開放決策 7）把 socket 錯誤碼的分類搬進 engine/os，消費端只保留「哪些算暫時性」的政策，分類結果和現在完全相同。
- 依賴：os-skeleton。
- 做：
  - `Os::Net::Classify(std::error_code)`。
  - `ClientConnection.cpp:885-891` 改成先 Classify、再依政策判斷。
  - 同一變更更新消費端的文字守衛 `tests/object_fps_pvp/test_diagnostic_isolation.py:107`：它靜態檢查呼叫點的文字，不改會失敗。
- 驗收：L1：等價表測試對所有 std::errc 與 asio::error 值，舊實作與新的「分類加政策」逐一相同；Windows 的 Red 要先在 windows-x64 跑出來。

### 7.17 os-close：收尾（medium，3～5 h）

- 白話：收尾，包括確認 tests 以外的例外清單已清空、同步所有文件，並在合併前完整跑一次相關突變。
- 依賴：全部。
- 做：
  - `AssetPathResolverTests.cpp:29` 改用中立的絕對路徑。
  - 同步 os.md、io.md、time.md、threads.md、trace.md、platform-sdl.md、error-handling.md。
  - engine/io 註解的「platform 側」改指 engine/os。
  - 評估移除非 os target 的 NOMINMAX。
- 驗收：L1 全量；tests 以外沒有 allowlist 條目；背景完整跑一次相關突變。

### 估時合計

- 關鍵路徑（消費端的長路徑修正之前）：os-skeleton 與 io-contracts 並行，接 os-files-read，約 20～29 h 工時。
- 整條線：約 100～150 h 工時。依開放決策 1 的建議，A1～A3 以外都排在消費端發行之後。

## 8. 與消費端發行任務的交接（P1～P5）

- Owner：消費端的發行任務（消費端 D193 的長路徑批；分工見 D45 與消費端 D197），不屬於本計畫。
- 開始條件：A1～A3（os-skeleton、io-contracts、os-files-read）完成，本計畫通知分支與 commit。

| 代號 | 內容 | 位置 |
|---|---|---|
| P1 | Windows 長路徑真正讀得到：絕對化、正規化、`/` 轉 `\`、加 `\\?\`、已有前綴時不重複加；遵守 os.md Invariant 5。UNC 依開放決策 14。位置與注意事項見表後 | engine/os 的 Windows 檔案原語（`engine/os/src/windows/Files.cpp` 的 `OsCallPath`） |
| P2 | 開檔失敗時記錄 OS 錯誤碼與路徑長度，取自 OsError 的 detail 格式（第 3.1 節） | asset 的錯誤路徑 |
| P3 | asset 的讀檔改經 engine/io 的 IFileSystem：`NativeFileAssetSource.cpp:21`、`AssetCatalog.cpp:30`、`ContentManifest.cpp:93`。NativeFileAssetSource 保留類別名與 API；IAssetSource 不刪也不改 | engine/asset |
| P4 | 和 P3 放在**同一個變更**：resolver 根目錄與 catalog 路徑改成 UTF-8，位置是 `PvpApplication.cpp:812`、兩個 `ObjectFpsApplication.cpp:71`、`ReadOnlyAssetCatalog.cpp:46、55`、`ContentManifest.cpp:104、110`。這是 Engine 契約的遷移，所以同一變更改完所有消費端與工具 | 產品、工具、engine/asset |
| P5 | 驗收，見下 | 消費端 |

P1 的位置與注意事項（os-files-read 的 xhigh 審查提出，架構已預留）：

- Windows 原語裡，「UTF-8 轉 UTF-16」（`ToWide`）和「交給檔案 API 的形式」（`OsCallPath`）已經分成兩個函式。長路徑前綴只加在 `OsCallPath`。
- `MakeAbsolute` 用的是 `ToWide`，不經過 `OsCallPath`，所以它的結果永遠不會帶前綴（Invariant 5）。
- 呼叫端自己傳入的 `//?/C:/x` 形式，engine/io 會拒絕（io.md Invariant 4）；engine/os 直接收到時，P1 要決定怎麼處理，並在 os.md 寫明。
- `\\?\` 會關掉 Win32 的路徑正規化（結尾的點與空白、保留的裝置名）。這是 Invariant 9 要求事先宣告（★）的行為變更，P1 要在開始時宣告。
- os-files-read 已放好給 P1 的 Red：`FilesTests.cpp` 的「missing path longer than 260 characters」只記錄目前的錯誤碼。P1 還要再加一個「存在的長路徑檔案讀得到」的 Red→Green。

P5 的驗收由發行任務宣告，本計畫只列交接條件：

- Windows 單元測試用超過 300 字元的路徑，先在未修正的樹確認 Red，再確認 Green。
- windows-x64 L1 的深路徑套件情境：P 取在 145 到約 219 之間（例如 200），讀完 bundle 的所有 .dxil。
- Windows 非 ACP 的根目錄下，ContentManifest::Load 仍然成功：舊碼能過，新碼也必須能過。
- Windows 實機在事故深度 P=156 與 P 約 210 各啟動一次，對照事故 log 第 2 行。
- 守衛 allowlist 中 asset 的條目刪除；macOS 與 Linux 的既有測試不改判定。

P1～P5 完成後的下一個失敗點：產品自己開的 arenas.json（約 P≥219）、log 的 create_directories 與 TraceSink（約 P≥229），都屬於本計畫後續的 os-files-write、os-trace-writers。最後是執行檔本身（約 P≥238），沒有 longPathAware 時 Windows 連程式都啟動不了〔推論〕。

## 9. Architecture Delta（AGENTS §3 的七點）

1. **Feature**：
   - 直接的 Feature 是消費端的 Windows 畫面程序：發行包解壓到深資料夾時，shader 路徑 271 字元超過 MAX_PATH 260 而開不了（事故 log 第 2 行；同一個 exe sha 搬淺兩層後成功）。行為修正由消費端的 P1～P5 交付，本計畫提供它需要的結構與契約。
   - 使用者明定的目標是整理 OS 子系統（D40）：下一版起，Engine 的其他模組、產品與工具都不必考慮 OS；伺服器端在各 OS 能優雅結束、診斷資訊一致；非 ASCII 資料夾也能用。
2. **現在的問題**：
   - 原生檔案存取沒有 owner。IFileSystem 預告的原生實作從沒寫過；asset 另有職責重複的 IAssetSource；asset 與 trace 各自直接開檔，都沒處理長路徑。
   - engine/io 不含任何 OS 程式碼，代表它從未被使用、也沒有對 OS 做相容（D41）。
   - 產品與工具自己判斷 OS，而且同一件知識寫了兩次：localtime（`main.cpp:67`、`LogFile.cpp:38`）、原子取代（`FileService.cpp:72` 與 IP-5 延後的 AtomicWriteFile）、執行檔位置（`LogFile.cpp:57` 與 platform/sdl 的 BaseDirectory）。
   - 路徑編碼沒有契約（Windows 上是 ACP）。
   - ParseUri 把 POSIX 絕對路徑變成相對路徑。
   - time 與 threads 的 OS 分支沒有 Design Invariants 可以對照。
3. **改變的 Architecture Boundary**：
   - 新增 Engine 子系統 engine/os（獨立 target GYO::Os），位在 GYO::Base 之上、engine/io、Time、Threads、Trace 之下，是 OS 差異的唯一 owner。
   - 新的 Design Invariants：os.md、io.md。engine/io 的「platform 側」確定為 engine/os 加上 io 內的 NativeFileSystem。
   - Data Contract 變更：Engine 的路徑字串定為 UTF-8；file:// 的解析規則。
4. **影響的 Module、Product、Tool**：
   - Engine：engine/os（新）、engine/io、engine/asset（P3）、engine/trace、engine/time、engine/threads、engine/platform/sdl（PathFromUtf8）、engine/CMakeLists.txt。
   - 消費端：路徑呼叫端與後續的 OS 分支。
   - 未啟用的 apps/object_fps、apps/object_fps_v2：路徑呼叫端。
   - 工具：tools/ui_editor、tools/object_fps_preview。
   - 不受影響：shader pipeline 主機工具（開放決策 12）、Go 轉送程序、Python 驗收腳本。
5. **Dependency Direction**：
   - 新增的邊全部往下：GYO::Os → GYO::Base；GYO::Engine → GYO::Os；之後是 GYO::Time、GYO::Threads、GYO::PlatformSDL → GYO::Os；依開放決策 2，才有 GYO::Trace → GYO::Os；產品與工具 → GYO::Os。
   - GYO::Os 不依賴任何上層，不產生循環。
   - 產品與工具原本直接依賴 OS 標頭，改成依賴 Engine 的中立 API，Application → Engine 的方向更純粹。
6. **Ownership**：
   - engine/os 成為 OS 差異的唯一 owner，也是結束要求旗標的權威。
   - engine/io 擁有檔案抽象、Uri 與路徑契約；Time 擁有 Waiter 語意與 WaitBackend 字串；Threads 擁有 RoleThread 生命週期；Trace 擁有寫檔順序與位元組；產品擁有政策（暫時性錯誤、結束收尾、日誌格式）；PlatformSDL 擁有 BaseDirectory。
   - PathFromUtf8 的 Ownership 從 platform/sdl 移到 engine/os。
7. **為什麼不能更小**：
   - 只修 NativeFileAssetSource（消費端 D193）或在 asset 內寫 helper，會把 Windows 長路徑與 UTF-16 轉換的知識留在 asset。Trace 依 Invariant 7 不能連 GYO::Engine，產品與工具也碰不到 asset 的內部 helper，只能各寫一份。同一知識重複出現已經是觀察到的壓力，不是預測。
   - 原生實作只放進 engine/io 也不夠：io 編在 GYO::Engine 裡，Time、Threads、Trace 連不到。所以 engine/os 必須是獨立的底層 target。
   - 另一方面，已經壓到最小的可撤回單位：架構批次只交付結構與契約；原生後端唯讀起步；不改換行與 append 語意；計時核心排在最後，先補約束與基準，L2 不通過就 revert；allowlist 保留現狀，不會一次大改。

需要使用者同意的項目（依各批的時點）：

- os.md、io.md、time.md、threads.md 的 Design Invariants 節。
- 新 target GYO::Os（建置圖變更）。
- time、threads、trace 的 exactly-links 守衛期望值（§14.4）。
- trace.md Invariant 7（開放決策 2）。
- error-handling.md 平台邊界節的延伸。
- platform-sdl 公開 API 移除 PathFromUtf8。

## 10. 風險

- **消費端時程**：A1～A3 是 P1 的前提，A1～A3 延誤就會拖住 hotfix。緩解：A2 與 A1 並行；產品的 allowlist 移到發行後；開放決策 1、14、15 在核准時答覆。
- **`\\?\` 的副作用**：它會停用 Win32 的路徑正規化，相對路徑必須先絕對化並正規化；MSVC 與 CreateFileW 對它的行為只是推論。緩解：P1 用 windows-x64 的 Red→Green 證明，Red 沒出現就停下回報（§14.3）；A3 先放只記錄的 >260 測試。
- **UTF-8 契約只改一半會退步**：今天在日文或中文 Windows 上能用的非 ASCII 資料夾，可能反而壞掉。緩解：P3 與 P4 同一變更，清單含對抗式檢查補上的兩處；其餘由 os-path-utf8-sweep 逐一對照。
- **ParseUriLoose 被 AssetPathResolver 使用**：修 file:// 規則可能影響 catalog 路徑解析。緩解：新規則只套 file://；既有測試不改判定也要通過。
- **Capabilities 預設值幾乎都是 true**：Vfs 可能誤以為後端可寫。緩解：用測試與突變鎖住唯讀的 Capabilities。
- **計時核心退步**：可能出現 lost wakeup，Windows 100 ns 進位與毫秒退回、kqueue NOTE_CRITICAL 也可能被改掉。緩解：排在最後、先寫約束與基準、xhigh 審查、競爭突變、事先宣告的 ABAB L2、不通過就 revert。
- **日誌與 trace 的 Data Contract 被無意改變**：CRLF、append、`%z`、`wait_backend` 字串。緩解：Invariant 9、逐 byte 測試、既有測試不改判定。
- **突變目錄大量變成 stale**：Engine 端約 14 個、消費端約 63 個。緩解：每批同一變更重新定位，依消費端 D184 只重跑相關的。
- **守衛的盲點**：asio 經 INTERFACE 帶進的 windows.h、沒有巨集的隱含 OS 語意（文字模式換行、reuse_address、TransientSocketError）都抓不到。緩解：os.md 記為已知盲點，靠審查。
- **engine/os 變成雜物箱**：緩解：Invariant 7、Replacement Criteria、每批都要標出支援的 Feature。
- **Windows 的結論多半是推論**：緩解：每個 Windows 行為都先在 windows-x64 L1 確認 Red；做不到就依 §14.3 記錄並停下回報。macos-x64（CI 上的 Rosetta）的偶發問題只記錄，不當成阻擋條件。

## 11. 開放決策

1、14、15 已在核准時決定（D46）；其餘在對應批次開始前回答。

1. **剩餘部件與消費端發行的先後**。**已決定（D46）：(a)。**
   - (a) A1～A3 在發行前完成，其他全部排在發行之後，順序是 os-guard-wide → os-process-info → os-console-shutdown → os-files-write → os-tool-files → os-trace-writers → os-path-utf8-sweep，計時的兩批最後；time-threads-invariants 與 os-wake-record 不改程式，可以並行。
   - (b) os-process-info 也排進發行前。
   - 取捨：(a) 符合「先完成穩定版本再重構」，hotfix 範圍最小；代價是發行期間產品仍有 OS 分支，但守衛會凍結它們，不會再增加。(b) 產品的 OS 分支提早消失，但多改和日誌契約有關的程式。
   - 證據：D45；`LogFile.cpp:22-84`；`NetworkStatistics.cpp:11-80`。
   - 建議：(a)。
2. **trace 與產品的寫檔怎麼接到檔案抽象**。
   - (a) GYO::Trace 直接 PRIVATE 連 GYO::Os，呼叫寫入原語；trace.md Invariant 7 改成「exactly GYO::Threads and GYO::Os」。
   - (b) 把 engine/io 拆成獨立 target GYO::IO，Trace 經 IFileSystem 寫檔。
   - (c) Trace 不接，維持 ofstream。
   - 取捨：(a) 邊最少，Trace 不必認識 Uri，但 Trace 繞過 io 直接呼叫 os。(b) 最符合 asset → io → os 的形狀，但 io 要從 GYO::Engine 拆出，append 與文字模式也要先定語意。(c) 零成本，但 Windows 上 P≥234 仍會失敗，而且留下一條無限期的 Engine 例外。
   - 證據：trace.md:52；`tests/common/trace/CMakeLists.txt:11`；`engine/CMakeLists.txt:13-45`；`FileOpenMode.hpp:23`。
   - 建議：(a)。
3. （併入第 9 節，不再是選項）engine/os 是獨立 target GYO::Os。原因：time、threads、trace 不連 GYO::Engine，只有獨立 target 才能讓它們使用 engine/os，這是 D41 的必然結果。需要的是同意，不是選擇。
4. **IAssetSource 的去留**。
   - (a) 保留，當 asset 的窄介面，NativeFileAssetSource 是包在 IFileSystem 上的薄轉接。
   - (b) 改寫成「從 IFileSystem 加上 resolver 根目錄讀 bytes」的單一實作類別，移除介面。
   - (c) 移除 IAssetSource，loader 與 ShaderLibrary 直接收 IFileSystem 與 Uri。
   - 取捨：(a) 改動為零，但檔案來源的抽象仍有兩份，pak 或 memory 會被要求實作兩次。(b) 去掉重複，呼叫端只改型別名。(c) 抽象最少，但 IFileSystem 的介面很大，所有 asset 測試的 fake 都要改寫。
   - 建議：先 (a)。等第二個非原生來源出現時，再在 (b) 與 (c) 之間決定；觸發條件寫進 io.md 的 Replacement Criteria。
5. （刪除）Wait 與 Threads 是否永久例外：D41 已定「Waiter、RoleThread 改呼叫 engine/os」，所以只剩順序。L2 不通過時，依 §14.1 先回報、revert，再由使用者決定下一步。
6. **WaitBackend 列舉放在哪裡**。
   - (a) 留在 Time 的公開介面，engine/os 另有 WaitMechanism，Waiter 做 1:1 映射。
   - (b) 移到 engine/os。
   - 取捨：(a) 日誌字串契約仍由 Time 擁有，產品不必改，只是多一個映射。(b) 只有一份定義，但 OS 機制名稱直接變成公開契約。
   - 建議：(a)。
7. **socket 錯誤分類要不要搬**。
   - (a) 先不搬，等 Windows 實機確認 WSAECONNRESET 真的出現。
   - (b) 搬分類，政策留在消費端。
   - (c) 搬分類，並加上 Windows 的 SIO_UDP_CONNRESET。
   - 取捨：(c) 要碰 socket handle，觸發 Replacement Criteria 1。
   - 建議：(a)；看到實機證據後，再在 (b) 與 (c) 之間決定。
8. **「執行檔在哪裡」以誰為權威**。
   - (a) 兩個不同的事實各自為權威：Os::Process 給執行檔映像的路徑，PlatformSDL::BaseDirectory 給 SDL 的資源基準目錄（macOS app bundle 時是 Resources），文件寫明兩者的差異。
   - (b) BaseDirectory 改成呼叫 engine/os：會改變 macOS app bundle 的語意。
   - 建議：(a)。
9. **Windows 上哪些主控台事件算結束要求**。
   - (a) CTRL_C、CTRL_BREAK、CTRL_CLOSE 都算；CTRL_CLOSE 時，收尾的上限低於 OS 的約 5 秒。
   - (b) 只算 CTRL_C 與 CTRL_BREAK。
   - 建議：(a)。收尾上限在 os-console-shutdown 開始時寫定。
10. **系統字型目錄的時點**。D41 已定工具不再判斷 OS，只剩時點。
    - (a) 在 os-tool-files 搬。
    - (b) 延到 os-close 之前。
    - 建議：(a)。
11. **engine/os 內部依 OS 選擇程式碼的方式**。
    - (a) 每個 OS 一個翻譯單元，由 CMake 依 OS 選檔。
    - (b) 同一檔內用 #if。
    - (c) 新部件用 (a)；Wait 與 Thread 的搬移批先用 (b)，diff 最小，方便審查與 L2 對照，穩定後再評估。
    - 建議：(c)。
12. **shader pipeline 主機工具**（獨立的 CMake project GyoShaderTools）。
    - (a) 守衛只套用 R1～R3，凍結即可，不連 GYO::Os。
    - (b) 把 engine/os 一起編入。
    - (c) 完全排除。
    - 取捨：(a) 的 Windows 長路徑與 ACP 問題（`main.cpp:40、56、68`）仍然存在。
    - 建議：(a)。等開發者在 Windows 深路徑上建置失敗時，再以那個 Feature 評估 (b)。
13. **os-wait-backend 的 L2 判定數值**：在看結果之前，由使用者核准並凍結。
    - (a) A、B 的 Backend() 字串相同；B 的晚醒 P99 各對中位數 ≤ A 的中位數 ×1.25＋0.05 ms，P50 ≤ A＋0.02 ms；notify→wake 用同一規則；零早醒；每種情境 4 對交錯。Linux 的 Waiter 本身就是 cv，所以 Linux 只比較 A/B 的晚醒分布。
    - (b) 更嚴：P99 ≤ A×1.1＋0.02 ms，每種情境 8 對。
    - (c) 只記錄，不設判定。
    - 數值的來源在採用前要先做對抗式檢查。
    - 證據：time-threads-trace/HANDOFF.md:97-108。
    - 建議：(a)。
14. **UNC 根目錄要不要在這一輪支援**。**已決定（D46）：(a)，這一輪暫不支援。**
    - (a) io.md 定義 UNC 的 Uri 對應，但宣告「UNC 根目錄暫不支援」，P1 不寫 `\\?\UNC\` 分支。
    - (b) io-contracts 一併修 `NormalizeSlashes`（`PathUtils.cpp:247-268`）與 Path 正規化的 collapse（`PathUtils.cpp:79`），保留開頭的雙斜線，讓 UNC 全線可用。
    - 取捨：(a) 沒有消費端證據時不擴大範圍（AGENTS §1），但把遊戲裝在網路磁碟上的人會失敗。(b) 完整，但會改到 resolver 的既有行為：`AssetPathResolver.cpp:70` 預設會把 `//server/share` 壓成 `/server/share`。
    - 建議：(a)。
15. **PR 的形態**：慣例是「每個 Engine 子系統與它的第一個消費端放在同一個 PR」。**已決定（D46）：(a)，A1～A3 併進消費端的發行線 PR。**
    - (a) A1～A3 在本計畫的分支完成；消費端的發行任務把這個分支併進它的發行線 PR，再加上 P1～P5。兩個任務，一個 PR。
    - (b) A1～A3 單獨開 PR，先合併。
    - 取捨：(a) 符合慣例與「功能線為單位、PR 盡量少」，代價是發行線 PR 延誤時 engine/os 也跟著延後。(b) engine/os 先進 master，但合併時沒有任何正式程式碼使用它。
    - 建議：(a)。

## 12. 尚未確認之處（推論清單）

以下都沒在 Windows 上實際跑過，各批開始時先在 windows-x64 確認 Red：

- MSVC 的 `std::ifstream(std::string)` 與 `path::string()` 走 ACP。
- CreateFileW 加 `\\?\` 能繞過 MAX_PATH。
- Windows 在 P≥238 時無法啟動執行檔。
- SDL 的 Windows 檔案 IO 不處理長路徑。
- MSVC CRT 的 append 在多程序之間不原子。
- UCRT 的 `%z` 輸出。
- `RoleThread.cpp:53` 對非 ASCII 名稱擴寬錯誤。
- Explorer 內建解壓會跳過長路徑的檔案。
