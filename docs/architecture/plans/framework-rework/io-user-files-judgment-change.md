# v8-engine-io-user-files：契約、行為變更與條文（已核准）

日期：2026-11-01。依 AGENTS.md §3（Engine 公開介面的變更，Architecture Delta AD2）與 §14.3（缺陷的處理）提出。
**狀態：2026-11-01 使用者核准（「照建議核准」）。** 第 1 節全部採建議：包含判定用修正版 (a)；framework 的 `IsPathWithin` 不在這批修（收尾批刪除，T2 後 ui_editor 用修正版）；AD2；第 3 節條文已寫入 `docs/architecture/io.md`。io.md 的契約是 Path contract 底下的列表，所以「User files」寫成一個列表項目（文字不變）；同時把原子寫那一項的「the parent directory must exist」補成「除非呼叫者要求建立」，指向這一項，避免兩處矛盾。

## 1. 要使用者決定的事

1. **路徑包含判定要用修正版，還是照舊版移植**（xhigh 審查的 F2～F4）。
   - 機制：framework 的 `IsPathWithin` 有三個缺陷，照實移植會一起帶進 Engine 的公開 API：
     - F2：根目錄以分隔符結尾時，底下什麼都判成不在裡面。ui_editor 只給 `--asset-catalog` 時根目錄是 `.`，正好觸發，「不得匯出到資產根目錄」的守衛（`tools/ui_editor/src/DocumentSession.cpp:207-208`）因此失效。
     - F3：根目錄本身是連結，或寫法的大小寫和磁碟不同（macOS 的預設磁碟、Windows 的 8.3 短名）時，同樣全部判成不在裡面。
     - F4：POSIX 上先詞法消去 `..`，再解析連結，和 OS 實際開檔的位置不一致，兩個方向都會錯。
   - (a)（建議，已實作）修正版：候選路徑的父目錄交 OS 解析（連結先於 `..`）、最後一段照寫；根目錄整條解析；結尾分隔符代表它前面的目錄。對錯以「OS 實際把檔案放在哪裡」為準（測試用標準函式庫的 `weakly_canonical` 當獨立對照），先 Red 後 Green。代價：和 framework 舊版在上述情況答案不同，工具線 T2 換過來時，這些情況的行為會跟著改（對照測試逐條列出）。
   - (b) 照舊版移植：Engine 的 API 和 framework 完全一致，但缺陷進入 Engine 的契約，ui_editor 的守衛在 T2 之後仍然失效。
2. **framework 現有的 `IsPathWithin` 要不要現在就修**（ui_editor 目前就受 F2 影響）。
   - 建議不修：它在收尾批刪除，工具線 T2 讓 ui_editor 改用 engine/io 後就是修正版。代價：在 T2 合併前，ui_editor 只給 `--asset-catalog` 時仍可匯出到資產根目錄。
   - 另一案：在這批一併修 framework（行為變更、多一組測試），屬工具使用中的元件，要另外核准。
3. **AD2**：engine/io 新增使用者路徑的輔助（第 2 節），以及 engine_tests 直接連結 `GYO::Os`。
4. `docs/architecture/io.md` 的條文（第 3 節）。Design Invariants 不變。

## 2. 介面與行為

- `Helpers::ProbeFileStamp(fs, uri, readOptions)`（新標頭 `helpers/UserFiles.hpp`）：不存在時成功、`exists == false`；不是一般檔案時從狀態判斷、不開檔就回 InvalidPath（開具名管道或裝置有副作用）；超過 `readOptions.maxBytes` 時失敗，所以呼叫者要「不限大小」得明說。舊 framework 版讀整檔沒有上限。
- `Helpers::WriteFileAtomically(fs, uri, data, {createParentDirectories = true})`：先建立缺少的上層目錄（已存在就不動；磁碟根目錄下的檔案直接寫），再照原本的原子寫。舊 framework 版一律建立父目錄；父目錄是一般檔案時，舊版在建目錄就失敗，新版在開暫存檔時失敗，錯誤碼與訊息不同。
- `FS::IsNativePathWithin(candidate, root)`（放在 `NativeFileUri` 旁）：見第 1 節 (a)。介面是 UTF-8 字串（io.md DI 2）：非 UTF-8 或帶 Win32 命名空間前綴（`//?/`、`//./`）回 InvalidPath，和 file:// 對應一致。它只判斷包含，不改任何路徑，也不是沙箱：放在根目錄內、指向外面的連結算在裡面。OS 無法解析（沒有權限）時退回詞法比較。
- 沒有新增：ReadTextFile（用 `ReadAllBytes`，呼叫者給 maxBytes）、PathExists（用 `IFileSystem::Exists`）、AbsolutePath（D-2：留在 framework 核心）。

## 3. io.md 的條文（英文，照原文件語言）

**Contracts 新增**

> ### User files (v8-engine-io-user-files)
>
> Helpers for native paths a person names, in UTF-8, through `IFileSystem`; the asset and mount paths do not use them. `Helpers::ProbeFileStamp` reports a missing file as `exists == false`, refuses anything that is not a regular file from its status before opening it (InvalidPath), and reads the file whole for its hash, up to the caller's `ReadAllOptions::maxBytes`. `Helpers::WriteFileAtomically` with `createParentDirectories` creates the missing directories above the file first; a file right under a volume's root needs none. `FS::IsNativePathWithin` decides whether a file written at a native path lands in a root: the candidate's parent is resolved by the OS (`Os::Files::MakeAbsolute`, then `Os::Files::ResolvedPath`, links before '..') and its last name kept as written, since an atomic save renames over that name; the root is resolved whole; a trailing separator names the directory before it; names are compared with `Os::Files::SamePathComponent`; a path the OS cannot resolve is compared with '.' and '..' removed as written. It changes no path and is not a sandbox: a link named inside the root counts as inside. A namespace prefix is InvalidPath, as in the file mapping.

**Replacement Criteria 3（句尾補一句）**

> `FS::IsNativePathWithin` is such a resolving helper; it only decides containment and changes no path.

## 4. 驗證

件數の突き合わせ：一致（宣告 15・執行 14・killed 14、Windows 限定 1 條在 macOS 記為 skipped、stale 0）。經過見 HANDOFF。

- engine/io 的使用者檔案測試 13 條通過；`os.boundary_guard` 通過（engine/io 沒有 OS 程式碼，`std::filesystem` 只用詞法操作）。
- Red→Green：修正前 12 條中 6 條失敗（`red_before_fix.txt`），修正後全過（`green_after_fix.txt`）。
- 新舊對照：包含判定 20 個情況都和 OS 的實際行為一致，和 framework 不同的 4 個逐條列出（`parity_after_fix.txt`）；檔案戳記 3 個情況和 framework 一致。
- 突變的經過：第一次 6/8（兩條存活：缺 `..` 接連結的測試；多餘的一般檔案檢查是等價突變），補測試後第二次 7/8（等價突變仍存活，刪掉多餘檢查並移除該條），第三次 7/7；xhigh 審查後語意修正，第四次 13/14（結尾分隔符那條只在 OS 解析失敗時有作用），補測試後 1/1。F8 補回一般檔案檢查後，它在 macOS 上又是等價的（engine/os 也擋），所以不設突變，理由見 HANDOFF。
- xhigh 審查：F1（我新引入的回歸）已修；F2～F4 交第 1 節決定；F5、F7、F8 已修；F6（解析失敗時退回）寫進契約；F9（訊息差異）寫進第 2 節。
- 範圍的限界：UNC 與長路徑沒有在本機測（計畫原列在驗收中；UNC 依 D46 本輪不支援），Windows 的大小寫與 8.3 短名只靠 CI 與推論；權限退回那條在 Windows 與超級使用者下無法觸發。
- 全量回歸：`build/target/_build/test` 全量建置成功，CTest 115/115 通過（50.3 s，`-j 6`）。
- 證據：使用者 Mac，主 checkout `build/evidence/framework-rework/v8-engine-io-user-files/`。
