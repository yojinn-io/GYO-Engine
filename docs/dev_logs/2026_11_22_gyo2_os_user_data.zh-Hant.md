# 2026-11-22 GYO2：core/os 加「這個使用者的資料目錄」

批次代號：`v8-arch-gyo2-os-user-data`。分支 `claude/gifted-mirzakhani-4b20a5`（基於 `gyo2` 8e894776）。任務卡，主會話「[GYO2][不良少年]新架構重構」開的。實作者 Opus 5.5／high。事前宣告：[os-user-data-declaration.zh-Hant.md](../architecture/target/v2/os-user-data-declaration.zh-Hant.md)（修訂 1，使用者 2026-11-22「Q1 (b)、Q2 (a)，開始」）。

## 起因

讀檔卡（宣告 19.6）發現 Engine 沒有「每個使用者的資料目錄」的 API，玩家存檔要放哪裡只能由各產品自己判斷平台。threads／io 重審卡問了使用者，使用者 2026-11-22 選 Q3 (a)：補在 os 層，另開小批。

## 做了什麼

- 公開出口只有一個：`OS::Files::UserDataDirectory(organization, application)`（`engine/core/os/include/engine/os/Files.hpp`，放在同類的 `FontDirectories` 旁）。回傳 `<base>/<organization>/<application>`（organization 是空字串時不帶這一層），目錄不存在就建。
- 組合只寫一次：`src/sys/UserData.cpp` 檢查名字、拼路徑、建目錄，三個平台都編譯它。
- 各平台注入：`Detail::Platform` 加步驟 `UserDataBase()`。
  - macOS：`$HOME/Library/Application Support`（照 Godot，不用 Foundation，不加連結）。
  - Windows：`SHGetFolderPathW(CSIDL_APPDATA | CSIDL_FLAG_CREATE)`。它是 `FOLDERID_RoamingAppData` 的包裝，在已經連結的 shell32 裡。這是 Windows 的 `Platform` 第一個真正的步驟。
  - Linux：`$XDG_DATA_HOME` 是絕對路徑時用它，否則 `$HOME/.local/share`。
- 使用者的決定：
  - Q1 (b)：每個名字只能是一個路徑段。空的 application、`.`、`..`、含 `/` 或 `\` 的名字回 `InvalidPath`，在建任何目錄之前就拒絕。
  - Q2 (a)：這次新建的每一層在 POSIX 上是 0700。為此 os 內部加了 `Detail::CreateDirectories(path, NewDirectoryAccess)`；公開的 `CreateDirectories` 仍是 0777 扣掉 umask。
- 取不到 base 時回 `IOFailed`（POSIX 沒有絕對路徑的 `HOME`），不退到目前的工作目錄。
- `HOME`、base 結尾的 `/` 會去掉（`Detail::WithoutTrailingSlashes`）。
- 字型目錄改成呼叫同一組小函式（macOS `Home()`；Linux `XdgDataHome()`、`Home()`），判斷方式和結果不變。這一點由新的測試確認（見下）。
- os.md：Components 表的 Files 列、Contracts 的 Files 加一段、Guard 節 shell32 的用途。Design Invariants 節沒有改。
- CMake 的註解：shell32 的用途補上 `SHGetFolderPathW`（`engine/core/os/CMakeLists.txt`、`tests/common/os/CMakeLists.txt`）。不加連結。

## 驗證

| 項目 | 結果 | 證據 |
|---|---|---|
| macOS（arm64，本機）`gyo_os_tests` | 73 個案例、1062 個斷言全部通過 | 本機執行（未存 log） |
| macOS `ctest -R "^os\.\|gyo_os"`（含 `os.boundary_guard` 和它的自我測試） | 7／7 通過 | 本機執行（未存 log） |
| Linux（Docker `gyo-linux-run:1`，gcc-14）`gyo_os_tests` | 74 個案例、1075 個斷言全部通過（多一個只在 Linux 跑的 XDG 案例） | 主 checkout `build/evidence/gyo2-os-paths/linux-run.log`、`mutants-linux-rerun.log` 開頭 |
| 新突變 `ou-x1`～`ou-x20` | 20／20 killed（macOS 13、Linux 7） | `build/evidence/gyo2-os-paths/mutants-darwin-ou.log`、`mutants-linux-rerun.log` |
| 既有突變中 find 落在改動附近、或函式搬了位置的（D184） | `os-w4`、`os-w5`、`os-x6`：3／3 killed | `build/evidence/gyo2-os-paths/mutants-darwin-existing.log` |
| `os-g18-windows-extra-link`（find 和改動的行相鄰） | 只在 Windows 跑，本機沒有執行。find 那一行（`target_link_libraries(gyo_os PRIVATE shell32)`）沒有改動 | — |
| CI L1 三個平台 | 開 PR 後補記 | — |

證據放在這台 Mac 的主 checkout：`/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-os-paths/`（不進 repo）。

## 經過

- 事前宣告的修訂 0 交 Fable 5.2／high 做敵對式檢查（必修 3、應修 6、輕微 6）。主要改動：
  - 組合從「POSIX、Windows 各寫一份」改成只寫一次，三個平台注入 base。這樣也剛好就是卡片寫的「三個平台各自在 `src/sys/<平台>` 注入」。
  - 名字的檢查、建目錄的權限改成交使用者選。
- 實作時發現 `HOME` 結尾帶 `/` 會在 base 中間留下 `//`（不只是結尾），所以 macOS、Linux 從 `HOME` 拼 base 時先去掉結尾的 `/`。字型目錄沿用舊的拼法，結果不變。
- 寫 os.md 時一度改到 Design Invariants 節（Invariant 3 括號內 shell32 的用途），和宣告「不改這一節」不符，當場撤回。
- 突變清單交 Fable 5.2／high 做敵對式檢查（必修 1、應修 6、輕微 6），全部照改：
  - 必修：XDG 案例沒有切換工作目錄，`ou-x12` 會在 repo 根目錄建出 `relative-data/`。所有 POSIX 案例改用 `ScopedCurrentPath`（RAII，`REQUIRE` 中斷時也還原）。
  - 新增突變：organization 的檢查（`ou-x16`）、macOS 和 Linux 的相對 `HOME`（`ou-x17`、`ou-x18`）、Linux 的 `.local/share`（`ou-x19`）、錯誤碼 `IOFailed`（`ou-x20`）。
  - 測試：
    - 新增字型目錄的 POSIX 案例，確認小函式化之後結果不變。
    - 權限案例固定 umask 022，迴圈前先確認結果在已存在的那一層之下。
    - 結尾 `/` 改用 `//`，`while` 改成 `if` 也會被抓到。
    - 「沒有 HOME」的兩個結果改名 `withoutHome`、`relativeHome`，突變可以分得出是哪一個失敗。
    - Windows 的名字案例改用測試專用的名字，解構子裡不再用 `REQUIRE`。
  - expect：`ou-x5` 對 Windows 的分支也匹配；`ou-x6` 和 `ou-x13`、`ou-x10` 和 `ou-x12` 加上能區分的 `values:` 片段；`ou-x15` 的 `guards` 改成 `"os 9"`。
- Linux 第一次跑，`ou-x10`、`ou-x12` 是 wrong_failure。在 Docker 手動套上 `ou-x12` 重現，發現 `CAPTURE` 一個 `const char*` 時 doctest 印的是指標位址（`logged: ignored := 0x55c9…`），不是字串。迴圈變數改成 `std::string` 後重跑，7／7 killed。斷言與 expect 都沒改。
- 第一次在 macOS 跑突變時，zsh 沒有拆開沒加引號的 `$IDS`，`ou-` 的突變沒被選到。用 `${=IDS}` 重跑。

## 突變的數量（mutation README 第 8 條）

這一批不改 Design Invariants，所以第 8 條的上限（改的條文數 × 5）是 0。加了 20 個，全部守本宣告的契約（`guards` 寫 `os-user-data-declaration C<n>`；`ou-x15` 另守 `os 9`）。照第 1、3 條逐一對照：

- 每一個都是獨立的決定，或是不同檔案裡的同一種決定。例如結尾的 `/` 有三處：共用的組合、macOS 的 `HOME`、Linux 的 `HOME`。相對的 `HOME` 有兩處：macOS、Linux。
- 每一個的 `what` 都寫了只有它捕捉的錯。
- 宣告預計 11 個。多出的 9 個都來自兩次敵對式檢查指出的缺口，以及實作時多出的位置（結尾的 `/` 要分平台去掉）。

合併前核准時交使用者看。

## 沒做的與後續

- Windows 沒有突變（本機沒有 Windows）。Windows 的 base 只靠 CI L1 的測試 1：能抓到資料夾選錯、`\` 沒換成 `/`；抓不到 `CSIDL_FLAG_CREATE` 被拿掉、失敗分支、`WideToUtf8` 的錯誤。
- HANDOFF 與批次表（`rebuild-batch-plan.zh-Hant.md` 第 64 列，在 `claude/gyo2-time-design`）寫的 `OS::Paths` 由主會話改。
- 網路卡（#275）也改 `engine/core/os` 與 os.md。兩邊誰後合併，誰就 merge 跟上 gyo2，兩邊都留。
