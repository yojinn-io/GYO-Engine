# Engine io-native-paths：file:// 規則對齊作業系統

日期：2026-10-26。**完成**：判定由使用者定案（D55）；四平台 L1 通過（[run 40295007179](https://github.com/yojinn-io/GYO-Engine/actions/runs/40295007179)）。
計畫見 [io-native-paths](../architecture/plans/os-subsystem/PLAN.md)（第 7.8b 節）；紀錄見 [交接](../architecture/plans/os-subsystem/HANDOFF.md)的「io-native-paths」；開工前的決定是 D54，定案是 D55。

## 做了什麼

- engine/io 的 file:// 對應（`Uri.cpp`）：
  - 路徑照原樣保留，只把 `\` 換成 `/`；`..`、`.`、重複與結尾的 `/` 交給 OS，開檔時由 OS 照自己的規則解析（POSIX 跟著 symlink；Windows 由 `GetFullPathNameW` 依字面處理）。字面正規化只留給包含檢查與顯示。
  - 支援 UNC（取消 D46）：`file://server/share/x`、`file:////server/share/x`、`file://///server/share/x` 都對應原生的 `//server/share/x`；主機叫 localhost 的 UNC 印成 `file:////localhost/...`。原生路徑一個 `/` 是本機、兩個是 UNC、三個以上是根目錄 `/`。
  - POSIX 以 `//` 開頭的路徑不再當 UNC 拒絕，由 OS 解讀。
  - 仍然拒絕：看起來像磁碟機的 POSIX 路徑（照原樣或去掉點段、多餘斜線後，`/C:/x`、`/./C:/x`、`///C:/x`；字面檢查只用來拒絕），含 `?` 或 `:` 的 UNC 主機（`//C:/x`、`//??/`、`//server:8080`），命名空間前綴（斜線收合後也檢查）。
- engine/os：Windows 的 UNC 長路徑改用 `\\?\UNC\`；POSIX 的 `MakeAbsolute` 在目前目錄不是 UTF-8 時回 InvalidEncoding（D43 寫成契約）。
- engine/asset：`AssetPathResolver` 的根目錄照原樣保留，目錄內的相對路徑照舊在根目錄內字面清理與檢查。
- 文件：io.md 的路徑契約、行為變更節、Invariant 3 與 Replacement Criteria 3（D55 定案）；os.md 的 `MakeAbsolute` 與 Windows 長路徑。

## 驗證與經緯

- Red：UriTests 改寫後，在舊實作上 5 個案例、31 個斷言失敗（第 13 個案例對錯誤的 Result 呼叫 `.value()` 而中止）；之後新增的 symlink、Resolver 根目錄、escape 案例在舊實作上 3 個失敗。
- Green：本機 core preset 33／33。
- symlink 加 `..`：判定是「engine/io 讀到的位元組＝OS 自己開同一路徑讀到的位元組」，各平台同一判定。macOS 上 OS 讀到 symlink 那一側的檔，舊的字面規則讀到另一個檔。建不了 symlink 的 Windows 照 FilesTests 的前例寫訊息後返回。
- 非 UTF-8 目前目錄：macOS 的檔案系統建不出這種名字（EILSEQ），案例寫明後返回；Linux（位元組）與 Windows（不成對的代理字元）實跑。
- Windows UNC：探針 `gyo_io_unc_probe` 經 `\\localhost\C$` 與 `\\127.0.0.1\C$`，在短路徑與超過 300 字元的路徑上做往返、讀、寫、建目錄；分享不通時回 77（CTest 標 Skipped）。本機只做了語法檢查。
- 突變：第一輪相關的 35／35 killed，15 個 Windows／Linux 專用的跳過（Windows 的到 lan15 跑，D206）。審查後的修正再跑一次受影響的 32 條：32／32 killed（1 個 Windows 專用的跳過）。
- 契約審查（ultracode：1 位審查者＋1 位對抗檢查）：兩位都把真實輸入同時餵給舊碼（`git show 31a5849` 編進暫存目錄）與新碼。2 個 major：兩個以上的斜線開頭時，磁碟機外觀的拒絕被繞過（`///C:/x`），以及磁碟機形狀的 UNC 主機讓不動點破掉（`//C:/x` 印成 `file://C:/x`）。修正後加了「產生的各種寫法」性質測試（762 個原生寫法、310 個 URI 寫法）；它在修正途中又抓到同類的 `/C:/../x` 與 `///??/`。其餘的發現（未被測試守住的檢查、`.value()` 讓整個執行檔中止、文件與註解的落差）一併修正；`allowAbsolutePath` 的字面站點與 `normalizeSeparators=false` 的既有缺口記為未結。
- 產品組態（RelWithDebInfo）：內容相關的產品與編輯器測試通過（engine_tests、object_fps_pvp.cpu／asset_catalog／presentation_cpu、編輯器 3 項、守衛）。
- 已知的缺口：symlink 案例在建不了 symlink 的 Windows 上、非 UTF-8 目前目錄案例在 macOS 上，都只寫訊息後返回，不是 CTest 看得到的 Skipped；Windows UNC 探針在本機只做了語法檢查；四平台 L1 的 windows-x64 上 `io.unc_localhost` 與 `io.unc_127.0.0.1` 都是 Passed，所以 CI 連得到 `\\localhost\C$`。這次沒有在 Windows 上重跑舊碼看 Red（舊碼沒有 `\\?\UNC\` 時探針應以「FAIL reads the file」失敗，這點靠 Windows 專用突變 os-f9、os-f10 在 lan15 確認）。

## 定案（D55）

- AGENTS §14.4：UriTests 與 AssetPathResolverTests 的放寬面與收緊面照表定案。放寬面是點段照原樣、UNC（含只有主機的 UNC）、POSIX 的 `//`（因此 POSIX 上 `file://host/x` 讀本機的 `/host/x`）、Resolver 根目錄的 `..` 與 `//`、`allowEscapeAssetsRoot` 保留 `..`；收緊面是 `/C:/../x` 拒絕、catalog 的 `file://server/...` 拒絕、性質測試，以及開檔照 OS 規則後原本靠字面規則才讀得到的路徑失敗。開工前表上的 L4 在審查後撤回。
- io.md 的 Invariant 3 與 Replacement Criteria 3 照草稿定案。
- 未結：`allowAbsolutePath` 的字面站點（點段消去、UNC 的 `//` 收合），之後處理。
