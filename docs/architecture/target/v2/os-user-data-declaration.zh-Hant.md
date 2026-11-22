# 事前宣告：core/os 加「這個使用者的資料目錄」（`v8-arch-gyo2-os-user-data`）

更新：2026-11-22（修訂 1）。Owner：架構（GYO2 主會話開的任務卡）。
**狀態：動工前核准（2026-11-22，使用者：「Q1 (b)、Q2 (a)，開始」）。Q1、Q2 都照建議，記錄在 9.1。**

經過：

- 修訂 0：第一份草稿。
- 修訂 1：照 Fable 5.2／high 的敵對式檢查（唯讀；必修 3、應修 6、輕微 6）修訂，對照表在第 10 節。主要改動：
  - 組合（檢查→base→拼→建）只寫一次，三個平台只提供 base（S1）。
  - 名字的檢查和建目錄的權限改成交使用者選（M1、M2）。
  - 補驗收的停止條件（M3）、缺的突變（S3）、測試一律改向暫存目錄（S4）、契約沒定義的情況（S5）。

## 1. 這一批做什麼

### 1.1 用遊戲講

玩家按「存檔」，遊戲要把檔案寫到這台電腦上「這個玩家、這個遊戲」的資料夾：

- macOS：`~/Library/Application Support/<公司>/<遊戲>`
- Windows：`%APPDATA%\<公司>\<遊戲>`
- Linux：`$XDG_DATA_HOME/<公司>/<遊戲>`，沒設時用 `~/.local/share/<公司>/<遊戲>`

現在 Engine（io、os）沒有這個 API，每個產品得自己判斷平台、讀環境變數、拼路徑。這正是 os 層存在的理由：平台差異只在 os 裡寫一次。

### 1.2 根據

| 來源 | 內容 |
|---|---|
| 使用者 2026-11-22，threads／io 重審卡 Q3 | 選 (a)「補在 os 層，另開小批」（`threads-io-review-declaration.zh-Hant.md` Q3；HANDOFF `v8-arch-gyo2-threads-io-review-approved`）。**這是這一批的實際依據（使用者的 owner 決定）** |
| 讀檔卡宣告 19.6（`restore-entry-declaration.zh-Hant.md`，`claude/gyo2-restore-impl-7e6bc6`） | 點名的 Feature：玩家存檔（單機遊戲）。作者用 `WriteFileAtomically`、`ReadAllBytes` 讀寫存檔，缺的是「放在哪個目錄」。目前還沒有產品呼叫它 |
| os.md Replacement Criterion 9（D78） | 「使用者的 owner 決定」可以帶進元件，批次照它自己的宣告驗收。所以不必改這條 |

### 1.3 改成什麼

`engine/core/os/include/engine/os/Files.hpp` 加一個函式，放在 `FontDirectories` 旁邊：

```cpp
// This user's data directory for one application, created when missing:
// <base>/<organization>/<application>, or <base>/<application> when
// organization is empty. base is ~/Library/Application Support on macOS,
// the Roaming AppData known folder (%APPDATA%) on Windows, and
// $XDG_DATA_HOME (ignored unless absolute) or ~/.local/share elsewhere.
// Answers an absolute UTF-8 path in '/' form without a trailing separator.
[[nodiscard]] Base::Result<std::string, OSError> UserDataDirectory(
    std::string_view organization, std::string_view application);
```

- **一個公開出口**：只有 `OS::Files::UserDataDirectory`。
- **組合只寫一次**：檢查名字、拼路徑、建目錄寫在新的 `src/sys/UserData.cpp`。三個平台都編譯它，也是 `src/sys` 底下唯一三個平台共用的來源。
- **各平台注入**：平台之間只差在 base 從哪裡來，所以 `Detail::Platform`（`src/sys/Platform.hpp`）加一個步驟 `UserDataBase()`，`src/sys/{macos,linux,windows}/Platform.cpp` 各自實作。Windows 的 `Platform` 第一次有真正的步驟，該檔與 `Platform.hpp` 開頭說「Windows 的步驟都是空的」的註解要一起改。

### 1.4 不做什麼

- 不加 `OS::Paths` 這個新 namespace（理由在 3.3）。
- 不做存檔的讀寫、檔名、版本：那是作者的事（讀檔卡 19.6、SV-1）。
- 不做設定檔目錄、快取目錄、Documents 等其他使用者目錄：目前沒有點名的 Feature 需要它們。
- 不從 Engine 讀產品名稱：名字由呼叫者傳進來（os.md Invariant 7）。
- 不改 io：io 的 native backend 已能讀寫這個目錄底下的檔案。

## 2. 成熟方案的對照

| | SDL 3 `SDL_GetPrefPath` | Godot `user://` | Unity `persistentDataPath` | Unreal `ProjectSavedDir` |
|---|---|---|---|---|
| 出處 | 原始碼（本機 `GYO-Engine-v8-plan/build/target/_build/test/_deps/sdl3-src/src/filesystem/{cocoa,windows,unix}`） | 原始碼（GitHub master：`platform/*/os_*`、`drivers/unix/os_unix.cpp`、`core/os/os.cpp`） | 官方文件（6000.6）的摘要 | 〔未查證〕官方 API 頁由 JavaScript 產生，沒讀到內容 |
| macOS base | `NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory)`（Foundation） | `$HOME` + `Library/Application Support`，不用 Apple API | Player：`~/Library/Application Support/unity.<公司>.<產品>`（Editor 的目錄已存在時用 `<公司>/<產品>`） | 〔未查證〕 |
| Windows base | `SHGetFolderPathW(CSIDL_APPDATA \| CSIDL_FLAG_CREATE)`（Roaming）；超過 `MAX_PATH` 回錯 | 環境變數 `APPDATA`（Roaming） | `SHGetKnownFolderPath(FOLDERID_LocalAppDataLow)`（LocalLow） | 〔未查證〕一般說法是 `%LOCALAPPDATA%\<專案>\Saved` |
| Linux base | `$XDG_DATA_HOME`，否則 `$HOME/.local/share`；結尾的 `/` 去掉一個 | 同左；`XDG_DATA_HOME` 是相對路徑時忽略並警告 | `$XDG_CONFIG_HOME/unity3d/<公司>/<產品>` | 〔未查證〕 |
| 名字參數 | `org`（可空）、`app`，不檢查 | 專案設定的名字（處理成安全的資料夾名），預設 `godot/app_userdata/<名>` | 公司名、產品名（Player 設定） | 專案名 |
| 不存在時 | 建，`mkdir` 0700（每一層） | 啟動時建（`make_dir_recursive`） | 文件沒寫 | 〔未查證〕 |
| 失敗 | Linux、Windows 回 NULL＋`SDL_GetError`；macOS 忽略 `mkdir` 失敗 | 只記日誌，不回報給呼叫者；沒有 `HOME` 時退到 `"."` | 文件沒寫 | 〔未查證〕 |
| 回傳的形式 | 結尾帶分隔符（Windows 是 `\`） | `/` 形式 | | |

另外，XDG Base Directory 規格〔憑記憶，未在本機查證〕寫：寫檔時目的目錄不存在，應以 0700 建立；相對路徑的 `$XDG_*` 要忽略。

共同點：四家都內建；都由引擎把平台 base 和名字拼起來；查得到的兩家（SDL、Godot）都會建目錄。這一批照共同點做。

## 3. 契約（每一條的來源）

| # | 條文 | 照誰 |
|---|---|---|
| C1 | 兩個參數 `organization`（可空）、`application` | SDL（Unity 也是公司＋產品）。只要一個名字時，`organization` 傳空字串 |
| C2 | macOS 的 base 是 `$HOME/Library/Application Support` | Godot；os 現有的 `FontDirectoryCandidates` 也用 `$HOME/Library/Fonts`。不同於 SDL 的 Foundation：用 Foundation 要多連結一個 framework，就得改 os.md Invariant 3。sandbox 裡的 `HOME` 本來就指向容器，結果相同 |
| C3 | Windows 的 base 是 `SHGetFolderPathW(CSIDL_APPDATA \| CSIDL_FLAG_CREATE)` | SDL；卡片寫的「Known Folder」。微軟文件說 Vista 起它只是包 `SHGetKnownFolderPath`，`CSIDL_APPDATA` 就是 `FOLDERID_RoamingAppData`。退掉的選項見 3.3 |
| C4 | Linux（和其他非 Windows、非 Apple 的平台）的 base：`$XDG_DATA_HOME` 是非空的絕對路徑時用它，否則用 `$HOME/.local/share` | XDG 規格、Godot；os 現有的 `FontDirectoryCandidates` 已經這樣判斷 |
| C5 | 名字的檢查：見 **Q1** | — |
| C6 | 目錄不存在時建立，包括不存在的上層目錄；已經存在也算成功。權限見 **Q2** | SDL、Godot 都會建 |
| C7 | 回傳絕對路徑、`/` 形式、結尾不帶分隔符：base 結尾的 `/`（Windows 的 `\` 先換成 `/`）全部去掉再拼，所以 base 是 `/` 時結果是 `/<organization>/<application>` | os 的路徑契約（`ExecutablePath`、`MakeAbsolute` 都是 `/` 形式）；Godot 也換成 `/`；去掉結尾的 `/` 照 SDL |
| C8 | 取不到 base 時回 `IOFailed`：POSIX 沒有 `HOME`，或 `HOME` 不是絕對路徑（沒有 native error，不寫 detail）；Windows 的 `SHGetFolderPathW` 失敗（HRESULT 屬 Win32 facility 時 detail 是 `win32=<n>`，否則不寫）。base 不是 UTF-8 回 `InvalidEncoding`。建目錄的錯誤照 `CreateDirectories` 原樣傳出。呼叫者不依碼分支 | `ExecutablePath` 的先例（os.md：OS 失敗是 `IOFailed`、「no caller branches on the code」；macOS 取不到時不寫 detail）。不同於 Godot 退到 `"."`：存檔會默默寫到目前的工作目錄（Invariant 10：不默默回報成功） |
| C9 | 每次呼叫都重新讀 OS 和環境變數，不快取 | os.md Invariant 8 |
| C10 | 沒定義、交給 OS 的情況：Windows 上名字結尾是 `.` 或空格（例 `Acme Inc.`）時，回傳的字串保留它，但磁碟上的名字被 Win32 去掉了。之後再經 os 開檔也會做同樣的正規化，所以照樣打得開。`:`、`CON` 等保留名由 `CreateDirectories` 回錯。符號連結不解析 | 只寫明，不加檢查（不為防誤用加守衛） |

### 3.1 不交使用者、卡片自己判的

- 名字 `UserDataDirectory`：和同組的 `FontDirectories` 寫法一致；master 的 IP-5 計畫也用這個名字。
- 放在 `OS::Files`：見 3.3。
- 參數名 `organization`、`application`：照 SDL 的意思寫成全字。
- C4（忽略相對路徑）、C8（取不到時回錯，不退到 `"."`）：規格、Godot、os 既有的寫法與 Invariant 10 都一致，不交使用者。
- 「HOME」「XDG data home」的判斷，各平台的 `Platform.cpp` 裡各寫一個小函式，字型目錄和使用者資料目錄都呼叫它：Linux 的 `DataHome()`、macOS 的 `Home()`。判斷方式和現在的字型目錄一樣，字型目錄的結果不變。

### 3.2 ★ 宣告的行為差異

沒有。這是新函式；字型目錄只是把同樣的判斷抽成小函式，行為不變。

### 3.3 系譜與退掉的選項（AGENTS §4）

- **系譜**：master 的 IP-5 計畫原本想用 SDL 的 `SDL_GetPrefPath` 做 `UserDataDirectory`，後來延後，一直沒做（`origin/master` 的 `docs/architecture/plans/input-and-present/HANDOFF.md:37`）。gyo2 還沒有 SDL，而且 os 是 OS 差異唯一的位置（os.md Invariant 1），所以這次在 os 裡自己寫。組合的步驟照 SDL；macOS 和 Linux 的 base 照 os 現有的 `FontDirectoryCandidates`；Windows 的 base 照 SDL。
- **退掉的選項**：
  - **新開 `OS::Paths` namespace 和標頭**（Q3 原文與 HANDOFF 用來舉例的名字）：同類的 `FontDirectories` 已經在 `OS::Files` 裡。另開一組，同一類的東西會分在兩個地方；`src/common/Paths.hpp` 的名字也已經被內部的路徑檢查用掉了。
  - **只回 base，拼名字和建目錄交給呼叫者**：每個產品都會重寫一遍拼路徑和建目錄。四家都由引擎拼好。
  - **不建目錄、只回路徑**：查得到的兩家都會建；呼叫者拿到路徑後幾乎一定要先建目錄才能寫。
  - **組合在 POSIX 和 Windows 各寫一份**（修訂 0 的做法）：機制寫了兩遍，Windows 那份沒有突變能檢查（S1）。
  - **Windows 讀環境變數 `APPDATA`**：Godot 這樣做，os 的字型目錄也這樣讀 `LOCALAPPDATA`。好處是測試可以改向，不碰真的 `%APPDATA%`。退掉的理由：環境變數只是登入時從 Known Folder 抄來的值，可能被清掉或改掉（例如服務、某些啟動器）；卡片也指定 Known Folder。代價是 Windows 的測試要寫進真的 `%APPDATA%`（用測試專用的名字，結束時刪掉）。
  - **直接呼叫 `SHGetKnownFolderPath`**：結果和 `SHGetFolderPathW` 相同，但要用 ole32 的 `CoTaskMemFree` 釋放記憶體，得多連結一個程式庫，等於要改 Invariant 3。`SHGetFolderPathW` 在已經連結的 shell32 裡。
- **需要它的 Feature 與 Product**：玩家存檔（單機遊戲）。

## 4. 檔案

| 檔案 | 改動 |
|---|---|
| `engine/core/os/include/engine/os/Files.hpp` | 加 `UserDataDirectory` |
| `engine/core/os/src/sys/Platform.hpp` | 加步驟 `UserDataBase()`；改開頭的註解 |
| `engine/core/os/src/sys/UserData.cpp`（新） | 組合：檢查、拼、建 |
| `engine/core/os/src/sys/{macos,linux}/Platform.cpp` | 實作 `UserDataBase()`；字型目錄改呼叫同一個小函式 |
| `engine/core/os/src/sys/windows/Platform.cpp` | 實作 `UserDataBase()`（`SHGetFolderPathW`）；改開頭的註解 |
| （Q2 選 (a) 時）`src/sys/Files.cpp`、`src/sys/windows/Files.cpp` | 建目錄加內部的權限參數（公開的 `CreateDirectories` 不變） |
| `engine/core/os/CMakeLists.txt` | 加 `src/sys/UserData.cpp`；shell32 的註解補上 `SHGetFolderPathW` |
| `tests/common/os/CMakeLists.txt` | 連結檢查的 shell32 註解補上 `SHGetFolderPathW` |
| `tests/common/os/FileChangeTests.cpp` | 測試（第 5 節） |
| `docs/architecture/plans/os-subsystem/scripts/mutations.json` | 突變（5.2） |
| `docs/architecture/os.md` | Components 表的 Files 列；Contracts 的 Files 加一段（C1～C10）；Guard 節 shell32 的用途（第 284 行附近）。**不改 Design Invariants 節** |
| `docs/dev_logs/2026_11_22_gyo2_os_user_data.zh-Hant.md` | dev_log |

不改准許清單：R1～R3 不掃 `engine/core/os/src`。`shlobj.h` 只在 `src/sys/windows`，R6 不會誤中 `CSIDL_APPDATA`。

HANDOFF 和批次表（`rebuild-batch-plan.zh-Hant.md` 第 64 列，在 `claude/gyo2-time-design` 分支）寫的是 `OS::Paths`，由主會話改，本卡不改，改在回報裡轉告。

## 5. 測試與突變

### 5.1 測試（`gyo_os_tests`，CI L1 三個平台）

**POSIX 的每一個案例**（包括名字被拒絕的案例）都先把 `HOME`、`XDG_DATA_HOME` 指到測試自己的暫存目錄，結束時還原並刪掉暫存目錄，不碰開發者真正的目錄（S4）。Windows 的 base 沒辦法改向，用測試專用的名字建在真的 `%APPDATA%` 下，結束時刪掉；名字被拒絕的案例本來就不建目錄。

1. 回傳 base＋`/<organization>/<application>`：絕對路徑、`/` 形式、結尾沒有分隔符，而且目錄存在。base 的期望值：
   - macOS：`<HOME>/Library/Application Support`
   - Linux：`<HOME>/.local/share`
   - Windows：測試自己呼叫 `SHGetFolderPathW`，把結果換成 `/`
2. `organization` 是空字串時，回 base＋`/<application>`。
3. 再呼叫一次（目錄已經存在）也成功，回同一個路徑。
4. Linux：`XDG_DATA_HOME` 是絕對路徑時用它；是相對路徑或空字串時改用 `HOME`。在同一個行程裡改環境變數再呼叫，結果跟著變（C9）。
5. `HOME`（Linux 是 `XDG_DATA_HOME`）結尾帶 `/` 時，結果沒有 `//`。
6. POSIX：沒有 `HOME` 時回 `IOFailed`（Linux 同時也沒有 `XDG_DATA_HOME`），而且不在目前的工作目錄建任何東西。
7. 名字照 Q1 的結果拒絕，回 `InvalidPath`，不建任何目錄。
8. （Q2 選 (a) 時，POSIX）新建的每一層權限都是 0700；已經存在的上層目錄權限不變。

### 5.2 突變（`docs/architecture/plans/os-subsystem/scripts/mutations.json`，`guards` 寫 `"os-user-data-declaration C<n>"`）

每個獨立的決定一個（mutation README 第 3 條）：

| id | 改法 | 只有它捕捉的錯 | 平台 |
|---|---|---|---|
| ou-x1 | 不建目錄 | C6 | 不限（`src/sys/UserData.cpp`；本機跑 darwin） |
| ou-x2 | 不檢查 `.`、`..` | C5 的一個分支 | 不限 |
| ou-x3 | （Q1 選 (b) 時）不檢查分隔符 | C5 的另一個分支 | 不限 |
| ou-x4 | `organization` 是空字串時仍多拼一個 `/` | C1、C7 | 不限 |
| ou-x5 | 不去掉 base 結尾的 `/` | C7 | 不限 |
| ou-x6 | 沒有 `HOME` 時回空字串（結果變成相對路徑），不回錯 | C8 | darwin |
| ou-x7 | 第一次的 base 存進 `static`，之後沿用 | C9 | linux |
| ou-x8 | macOS 的 base 少了 `Library/` | C2 | darwin |
| ou-x9 | Linux 接受相對的 `XDG_DATA_HOME` | C4 | linux |
| ou-x10 | Linux 優先用 `HOME` | C4 | linux |
| ou-x11 | （Q2 選 (a) 時）建目錄用預設權限 | C6 | darwin |

- darwin 和「不限」的在本機 macOS 跑；linux 的用既有的 `linux-run.Dockerfile` 在本機 Docker 跑。
- Windows 沒有突變：本機沒有 Windows，Windows 的 base 只靠 CI L1 的測試 1。組合的部分三個平台共用，由 ou-x1～x5 涵蓋。
- 依 D184，只跑這些，再加上 find 落在這次改動範圍的既有突變。
- 數量：這一批不改 Design Invariants，所以 mutation README 第 8 條的上限（改的條文數 × 5）是 0；這些突變守的都是本宣告的契約，合併前核准時說明。
- 突變清單在跑之前交不同模型（Fable／high）做敵對式檢查（AGENTS §14.2）。

## 6. 驗收（看結果之前宣告）

- CI L1 三個平台（linux-x64、windows-x64、macos-arm64）全部通過。
- 本機 macOS 的 `gyo_os_tests` 和 `os.boundary_guard` 通過。
- 5.2 的突變全部以預期的失敗 killed。
- **停止條件（M3）**：任何一個突變沒有執行（例如 Docker 跑不起來），驗收就不成立。這時停下回報，不改用別的方法代替。
- 不需要 L2、L3。
- 證據放主 checkout 的 `build/evidence/gyo2-os-paths/`（卡片指定的位置）。

## 7. Architecture Delta（AGENTS §3）

1. 依據：使用者的 owner 決定（Q3 (a)）；Feature 是玩家存檔（讀檔卡 19.6）。
2. 問題：沒有這個 API，每個產品都得自己分辨平台，OS 知識會漏出 os（Invariant 1）。
3. 邊界：os 的公開介面多一個函式；`Detail::Platform` 多一個步驟，Windows 的 `Platform` 不再是空的。
4. 影響：只有 `engine/core/os`、`tests/common/os`、os.md。目前沒有呼叫者（讀檔卡 19.6 指路到這裡）。
5. 依賴方向：不變。不加連結（shell32 本來就連結），不加 include 的方向。
6. Ownership：「使用者資料目錄的位置」歸 os；目錄裡放什麼、檔名、格式歸作者。
7. 為什麼不能更小：見 3.3 退掉的選項。

Design Invariants 都不改，逐條對照：

- Invariant 1、2：OS 知識只在 os 裡。
- Invariant 3：不加連結。
- Invariant 4：標頭只多 `std::string_view`。
- Invariant 5：UTF-8、`/` 形式、只用寬字元 API。
- Invariant 7：名字由呼叫者傳入。
- Invariant 8：不快取。
- Invariant 10：三個平台都有明確的結果，取不到 base 時回錯。

## 8. 批次、檔位與模型

上段（公開介面）。

| 角色 | 模型／檔位 |
|---|---|
| 實作者（本卡） | Opus 5.5／high |
| 突變清單的敵對式檢查 | Fable／high（唯讀） |
| 合併前的設計審查 | Fable／high（唯讀） |

估時：實作與測試約 2 小時，突變（含 Docker）與文件約 2 小時。

## 9. 要你決定的

### Q1 名字的檢查（C5）

- **遊戲的例子**：作者寫 `UserDataDirectory("Acme", "../Other")`，結果會跑到別的遊戲的目錄；寫 `UserDataDirectory("Acme/Games", "Shooter")`，就多建一層。
- **別家**：SDL 完全不檢查。Godot 會把專案名處理成安全的資料夾名；自訂目錄名的處理〔憑記憶〕是允許分隔符、只去掉 `..`。所以兩個選項都和其中一家不同。
- **選項**：
  - **(a)** 只拒絕空的 `application`，以及含 `.`、`..` 這兩個路徑段的名字；`/` 允許，可以建出多層。
  - **(b)** 每個名字只能是一個路徑段：再加上拒絕 `/`、`\`。
- NUL 與不是 UTF-8 的名字，兩個選項都交給 `CreateDirectories` 本來就有的檢查（`InvalidPath`、`InvalidEncoding`），不另寫。
- **建議 (b)**：參數叫「公司」「應用」，一個名字對一層，契約最簡單。要多層的作者可以自己在回傳的目錄底下再建。(a) 要逐段檢查 `..`，條文和檢查都比較多。

### Q2 新建目錄的權限（C6，只影響 macOS、Linux）

- **遊戲的例子**：同一台機器有別的使用者帳號。存檔目錄是 0755（多數系統的 umask 022）時，別人讀得到你的存檔；0700 時只有你自己讀得到。
- **別家**：SDL 每一層都用 0700，XDG 規格〔憑記憶〕也說用 0700；Godot 用預設權限。Windows 不受影響：`%APPDATA%` 底下的目錄繼承使用者自己的權限。
- **選項**：
  - **(a) 0700**：只套在這次新建的層，已經存在的不動。os 的建目錄要加一個內部的權限參數，公開的 `CreateDirectories` 不變。多一個突變（ou-x11）、一個測試（5.1 第 8 項）。
  - **(b) 預設權限**：照現有的 `CreateDirectories`（0777 扣掉 umask），不改它。
- **建議 (a)**：SDL 和規格一致，存檔是玩家自己的資料；代價是 os 內部多一個參數。

### 告知（不用決定）

- **C2**：macOS 照 Godot 用 `$HOME`，不照 SDL 用 Foundation，所以不必改 Invariant 3。
- **C3**：Windows 照 Q3 原文與 SDL、Godot 用 Roaming（`%APPDATA%`）。Unity 用 LocalLow，Unreal〔未查證〕據說用 Local。
- **名字**：Q3 原文與 HANDOFF 舉例寫 `OS::Paths`，這裡改放 `OS::Files::UserDataDirectory`，理由在 3.3。HANDOFF 和批次表由主會話改。

動工的指令：例如「Q1 (b)、Q2 (a)，開始」。

### 9.1 使用者的決定（照四樣記，轉主會話寫進 HANDOFF）

| | Q1 名字的檢查 | Q2 新建目錄的權限 |
|---|---|---|
| 原話 | 「Q1 (b)、Q2 (a)，開始」（2026-11-22） | 同左 |
| 比過的選項 | (a) 只拒空的應用名與 `.`、`..`，允許 `/` 建多層／(b) 每個名字一層，也拒 `/`、`\` | (a) 新建的層用 0700／(b) 照現有 `CreateDirectories` 的預設權限 |
| 理由（宣告的建議） | 參數是「公司」「應用」，一個名字對一層，契約最簡單；(a) 要逐段檢查 `..` | SDL 與 XDG 規格都是 0700，存檔是玩家自己的資料；代價是 os 內部多一個權限參數 |
| 出處 | 本宣告第 9 節；本卡對話 | 同左 |

所以 C5＝每個名字一個路徑段：`application` 空字串、任一名字是 `.`、`..` 或含 `/`、`\`，回 `InvalidPath`。C6＝這次新建的每一層用 0700（POSIX），已經存在的不動；ou-x3、ou-x11 與 5.1 第 8 項都要做。

## 10. 修訂 1 的對照（敵對式檢查，Fable 5.2／high）

| 意見 | 處理 | 位置 |
|---|---|---|
| M1：C5 的依據寫錯（`CheckPath` 不是拒分隔符的先例；兩家都和現案不同）；NUL／UTF-8 的檢查多餘 | 改成 Q1 兩個選項；NUL／UTF-8 交給 `CreateDirectories` | 第 3 節 C5、第 9 節 Q1 |
| M2：沒寫 XDG 規格與 SDL 都是 0700 | 改成 Q2 | 第 9 節 Q2 |
| M3：「未執行」時驗收成不成立沒寫 | 寫明停止條件 | 第 6 節 |
| S1：組合寫兩遍、Windows 那份沒有突變 | 組合只寫一次，三個平台注入 base | 1.3、3.3、第 4 節 |
| S2：沒列「讀環境變數 `APPDATA`」 | 列進退掉的選項，寫明理由與代價 | 3.3 |
| S3：C8、C9、C2 沒有突變；README 第 8 條 | 加 ou-x6～x8；寫明數量的說明 | 5.2 |
| S4：突變時會寫到真實目錄 | POSIX 每一個案例都改向暫存目錄 | 5.1 |
| S5：結尾的點與空格、結尾的 `/`、兩種錯誤碼 | 加 C10；C7 去掉結尾的 `/`；C8 統一成 `IOFailed` | 第 3 節 |
| S6：和 HANDOFF、批次表的 `OS::Paths` 不同 | 第 9 節告知；第 4 節寫明由主會話改 | 第 4、9 節 |
| 輕微：C4、C8 卡片可以自己判 | 移到 3.1 | 3.1 |
| 輕微：1.2「三者有兩者」 | 照實寫：依據是使用者的 owner 決定 | 1.2 |
| 輕微：對照表的 SDL 細節 | 已改（cocoa 忽略 `mkdir` 失敗、Windows 的 `MAX_PATH`）。Godot 的 macOS 照網頁查到的原始碼寫，沒有採用「先看 `XDG_DATA_HOME`」（檢查者是憑記憶） | 第 2 節 |
| 輕微：shell32 的註解過期 | 列進第 4 節 | 第 4 節 |
| 輕微：只抽 Linux，macOS 不一致 | macOS 也抽 `Home()` | 3.1 |
| 輕微：ou-x2、x3 不限平台 | 已改 | 5.2 |
| 輕微：證據目錄和批次代號不一致 | 保留卡片指定的 `gyo2-os-paths` | 第 6 節 |

## 11. 實作與宣告不同的地方（動工後，合併前核准時一起看）

| 宣告 | 實作 | 理由 |
|---|---|---|
| 5.2 預計 11 個突變 | 20 個（`ou-x1`～`ou-x20`），全部 killed | 突變清單的敵對式檢查補了 5 個（organization 的檢查、macOS 和 Linux 的相對 `HOME`、Linux 的 `.local/share`、錯誤碼）；結尾的 `/` 要在三處去掉（共用的組合、macOS、Linux 的 `HOME`），各一個。mutation README 第 8 條的說明在 dev_log |
| C7「base 結尾的 `/` 去掉」 | 另外在 macOS、Linux 從 `HOME` 拼 base 時，先去掉 `HOME` 結尾的 `/` | `HOME=/x/` 會在 base 中間留下 `//`，不只是結尾。字型目錄沿用舊的拼法，結果不變 |
| 3.1「Linux 的 `DataHome()`」 | Linux 是 `DataHome()`（只看 `XDG_DATA_HOME`）和 `Home()` 兩個小函式 | 字型目錄和使用者資料目錄從 `HOME` 拼的路徑不同，共用的是「是不是絕對路徑」的判斷。一度寫成 `XdgDataHome`，違反 N17 的縮寫規則（不准一大一小），設計審查指出後改回 |
| C10「Windows 名字結尾的 `.`、空格……之後再經 os 開檔也會做同樣的正規化，照樣打得開」 | os.md 只寫「交給 OS、不定義」 | 設計審查指出：Win32 只在路徑的最後一段去掉結尾空格，中間段的行為不同，「照樣打得開」沒有測試也沒有在 Windows 驗證過。照 C10 的原意（不加檢查）只寫交給 OS |
| 5.1 測試 1～8 | 另加字型目錄的 POSIX 案例 | 確認字型目錄小函式化之後結果不變（敵對式檢查指出既有的字型測試擋不住） |

其餘照宣告。dev_log：`docs/dev_logs/2026_11_22_gyo2_os_user_data.zh-Hant.md`。

### 11.1 合併前，使用者的決定（照四樣記，轉主會話寫進 HANDOFF）

| | 突變的去留 |
|---|---|
| 原話 | 「照建議，三個都保留」（2026-11-22） |
| 比過的選項 | 設計審查者（Fable／high）建議退掉 `ou-x20`（守錯誤碼 `IOFailed`，os.md 寫呼叫者不依碼分支）、`ou-x7`、`ou-x8`（守 `HOME` 結尾 `/` 造成路徑中間的 `//`）／卡片建議三個都保留 |
| 理由（卡片的建議） | `ou-x7`、`ou-x8` 守實作時刻意加的行為，有測試支撐；`ou-x20` 是 `CHECK(withoutHome.error().code == IOFailed)` 唯一的突變（mutation README「尋ねるもの」）。20 個全部留下；第 8 條的說明在 dev_log |
| 出處 | 本卡對話；dev_log `2026_11_22_gyo2_os_user_data.zh-Hant.md` |
