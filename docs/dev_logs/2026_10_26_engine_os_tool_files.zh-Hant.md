# Engine os-tool-files：UI 編輯器的檔案服務與字型目錄改走 engine/io、engine/os

日期：2026-10-26。**本機完成**：xhigh 審查已修正，事後發現的差異由使用者定為 D57；四平台 L1 待推送後執行。
計畫見 [os-tool-files](../architecture/plans/os-subsystem/PLAN.md)（第 7.9 節）；紀錄見 [交接](../architecture/plans/os-subsystem/HANDOFF.md)的「os-tool-files」；開工前的決定是 D56，審查後的決定是 D57。

## 做了什麼

- engine/os：
  - `RenameReplacing`：一步改名並取代已存在的檔案。POSIX 用 `rename`，Windows 用 `MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH)`，和編輯器原本的旗標相同。PLAN 原名 ReplaceFile 是 Win32 的函式名，照 os.md 的 Names 改名。
  - `RemoveFile`：只刪檔。
  - `ResolvedPath`：等同 `std::filesystem::weakly_canonical`，存在的前段解開 symlink，其餘照原樣再依字面消去點段。POSIX 用 `realpath`，Windows 用 `GetFinalPathNameByHandleW`。
  - `SamePathComponent`：Windows 用 `CompareStringOrdinal` 忽略大小寫，其他平台逐位元組比較。
  - `FontDirectories`：系統目錄在前，使用者目錄在後，只列出存在的目錄。
- engine/io：NativeFileSystem 開放 `Move`（取代已存在的目標）與 `Remove`（只刪檔，遞迴仍是 NotSupported），`Capabilities` 跟著改。新增 `Helpers::WriteFileAtomically`，是唯一的原子寫入實作，IP-5 之後沿用。
- 工具（UI 編輯器）：
  - FileService 的讀、寫、Stat、Exists、建目錄、原子寫入改經 engine/io；絕對化、路徑解析、路徑成分比較改呼叫 engine/os。
  - DocumentSession 的存在檢查改用新的 `PathExists`；錯誤訊息裡的路徑改成 UTF-8（原本用 `.string()`，在 Windows 是 ANSI）。
  - 字型改由 `FontDirectories` 提供目錄，配上不分 OS 的檔名清單；路徑以 UTF-8 交給 ImGui。
  - 工具的 allowlist（6 條）清空並刪除。

## 驗證與經緯

- 本機 core preset 33／33（審查修正後 engine_tests 122／122、gyo_os_tests 53／53，Windows 專用的案例只在 Windows 編進去）；產品組態的編輯器測試 6／6。
- `ResolvedPath` 的守衛測試直接拿 `weakly_canonical` 的結果當參照，涵蓋 symlink 與缺少的尾段。macOS 的暫存目錄 `/var` 是連到 `/private/var` 的 symlink，所以這個測試在 macOS 上本身就有檢出力。
- 新增「經 symlink 進入 asset 根目錄的匯出被拒絕」：只比字面的突變（ui-x1）會讓它失敗。
- 突變：Engine 端 27／27 killed，工具端 2／2 killed；Windows／Linux 專用的 19 個跳過（Windows 的到 lan15 跑，D206）。
  - os-x4 第一次跑是 timeout，原因在我寫的測試：doctest 的 `CAPTURE` 回報失敗時會再算一次運算式，而 `Utf8()` 裡面有 REQUIRE，在回報器裡又觸發斷言，造成鎖死。改成先存成變數後 killed。實作沒有錯。
- xhigh 審查（1 位，唯讀）：沒有 blocker。
  - 1 個 major：Windows 的 `ResolvedPath` 往回找到磁碟根目錄時接錯路徑。已修，改成和 MSVC 的 `weakly_canonical` 一樣，不解析根目錄。
  - 修了 7 個 minor 裡可修的部分：錯誤集合對齊現行 STL；POSIX 先 `stat` 再 `realpath`；刪目錄的錯誤碼統一；移除 64 MiB 上限；註解不提工具名稱。
  - 補了測試缺口：記錄呼叫的 `IFileSystem` 檢查原子寫入的步驟；根目錄、`subst`、UNC 的路徑解析；字型目錄的順序。
  - 修正後突變：Engine 端 32／32、工具端 2／2 killed。
- D57：審查發現、事前沒有宣告的差異，由使用者決定接受：POSIX 的空路徑絕對化與讀目錄／FIFO、Windows 不合法名稱的錯誤時點、錯誤訊息文字、字型的邊界情況。沒有磁碟代號的磁碟區維持回錯誤，記為已知差異。
- 已知的缺口：
  - 原子性本身（讀者只會看到舊檔或完整的新檔）靠 `rename` 的語意，測試看不到中間狀態；暫存檔的清理與「失敗時目標不變」有測試。
  - symlink 案例在 Windows 上建不了 symlink 時只寫訊息後返回。
  - Windows 的實作在本機只能閱讀，審查是推論；要靠四平台 L1 的 windows-x64（含 `subst` 與 UNC 的參照比對）。
  - 讀超過 64 MiB 的檔案沒有測試。
