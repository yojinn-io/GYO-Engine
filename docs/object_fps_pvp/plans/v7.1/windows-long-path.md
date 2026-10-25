# windows-long-path：發行包解壓到深資料夾時開不了 shader（2026-10-25：D193 → L4（D195）→ 子決策定案（D196）→ 分工更正（D197）→ OS 子系統的 PLAN 核准（D198）：A1～A3 由 OS 子系統任務做，併進發行線 PR；P1～P5 由本會話在 A1～A3 完成後實作）

發行線的第一批（INVENTORY `N-windows-max-path`）。唯讀調查，還沒有改任何程式。

## 現象與成因

- 2026-10-16 的全 Windows LAN 臨時測試：Client 開不了 shader，路徑 271 字元，超過 Windows 的 MAX_PATH（260）。證據：主 checkout 的 `build/target/_build/test/logs/pvp-v71-windows-lan-20261016/logs/clientA/logs/client-20261016-104011.log:2`。
- 長度的來源：
  - 發行包內的資產路徑本身就長：`gyo-object_fps_pvp/bin/assets/object_fps_pvp/shaders/builtin/objects/<64 字元雜湊>.dxil`；
  - 壓縮檔解開後多兩層同名資料夾（`gyo-package-object_fps_pvp-windows-x64/gyo-object_fps_pvp-windows-x64/`）；
  - 玩家把它放在 `C:\Users\<名字>\Downloads\<下載工具產生的資料夾>\` 底下。
- CI 的套件檢查解在短路徑，所以沒抓到。repo 裡沒有 `longPathAware`，也沒有 `\\?\` 前綴。
- 資源載入之後一律用 ID 存取，不碰路徑。路徑只出現在載入那一步：`AssetPathResolver` 把 ID 換成發行包裡的檔案路徑，`NativeFileAssetSource::ReadAll`（`engine/asset/loading`，`IAssetSource` 的原生檔案實作）向 OS 開檔。所有資產（shader、arena、字型）都經過它，260 字元的限制就卡在這一次開檔。

## 選項

| 代號 | 做法 | 優點 | 代價 |
|---|---|---|---|
| w1 | Windows 執行檔的 manifest 宣告 `longPathAware` | 改動小 | 只有在系統開了 `LongPathsEnabled`（登錄檔，消費者版 Windows 預設關閉）時才有效，單獨用解決不了 |
| w2 | 只改原生檔案來源 `NativeFileAssetSource` 在 Windows 上開檔的那一處，把絕對路徑加上 `\\?\` 前綴（ID 存取、resolver、render 都不動） | 不需要系統設定；所有資產一次解決；作業系統的開檔限制留在唯一碰 OS 的元件 | `engine/asset` 的一個元件的改動；只修資產，日誌、trace 等其他寫檔路徑另外看 |
| w5 | 產品自己實作一個 `IAssetSource`，在裡面處理長路徑；Engine 不改 | Engine 一行都不動 | Windows 的開檔限制是作業系統的行為，不是遊戲知識；每個在 Windows 讀檔的產品都要各自重做 |
| w3 | shader 檔名的雜湊縮短（例如 64 → 16 字元） | 路徑短約 48 字元 | 改 shader 建置與 manifest 的內容契約；更深的資料夾仍會超過 |
| w4 | 發行包少一層資料夾 | 路徑短約 40 字元 | 改打包流程；同樣只是延後超過的深度 |

## 建議與驗收

- **建議 w2**：不靠系統設定、也不只是延後問題；修正落在唯一向 OS 開檔的元件。可以和 w4 一起做，但不是必要。（2026-10-25 依使用者的提問改正：原本寫成「Engine 的改動」，範圍說得太大。）
- **驗收**：windows-x64 的 L1 套件檢查加一個深路徑情境（解到總長超過 300 字元的資料夾，Client 的煙霧測試能載入 shader）；Engine 的單元測試（Windows 上超過 260 字元的路徑讀得到）。
- **估時**：2～4 h（D159 給的範圍內）。

## 決定（D193）

使用者在分叉的會話裡確認，經規劃會話轉達：路徑問題只在 Engine 的 Asset 載入模組（`engine/asset`）內處理，不動 Engine 其他模組、ID 存取與產品（不另做 `IAssetSource`，w5 不採用）。做法是 w2，範圍：

1. 只改 `loading/NativeFileAssetSource` 向 OS 開檔的那一處。resolver 不動；resolver、錯誤訊息、manifest 裡的路徑都維持一般形式。
2. 只在 Windows、而且路徑過長時：先轉成絕對路徑並正規化（反斜線，不含 `.` 或 `..`），加上 `\\?\` 前綴，以寬字元開檔。短路徑的行為不變，其他平台不變。
3. 開工前先掃 Asset 模組，確認所有資產（字型、圖片等載入器）都經過 `IAssetSource`；有繞過的改成經過它，不在各載入器重複處理。

驗收：Asset 模組的單元測試（Windows 上 300 字元以上的路徑讀得到，一般路徑不變）；windows-x64 的 L1 套件檢查加上「解壓到深資料夾」的情境（Client 的煙霧測試能載入 shader）。估時 2～4 h。

## 改為 L4（D195，取代 D193 的範圍）

OS 統一子系統的任務（另開的會話「規劃 Engine 的 OS 統一子系統」）做了 os-longpath-decision，使用者採 L4：「v7.1 的 windows-long-path 採 L4：為 engine/io 的 IFileSystem 寫只讀的原生實作，asset 的讀檔改走它。」

內容（轉達時的理解，子決策定案後再更新）：
- `engine/io` 的 `IFileSystem` 目前沒有原生實作（`engine/io/src/fs/*.cpp` 是空殼，`Vfs` 只有測試在用）。L4 寫一個只讀的原生實作：`Open`（只讀）、`Exists`、`Stat`，其他回 NotSupported（介面的 Capabilities 允許部分實作）。Windows 的長路徑在這個實作內部處理（絕對化、`\\?\` 前綴、寬字元）。
- asset 的讀檔改走它：`NativeFileAssetSource`（類別名與 API 不變，產品與 ui_editor 不用改）、`ContentManifest.cpp:93`、`AssetCatalog.cpp:30`。產品自己的 `Arena.cpp` 與 `AssetWatcher` 的 stat 不在範圍內。
- v7.1 不開 `engine/os`（L3 不採用）；L4 的原生實作之後成為 OS 子系統 Files 部件的起點。

判斷用的證據（由 `build/target/object_fps_pvp` 的實際檔案配置算出；P＝路徑到 `bin\` 為止的字元數，事故時 P＝156）：

| 檔案 | 失敗時的 P |
|---|---|
| shader `.dxil`（`bin\` 之後 115 字元） | ≥145 |
| 其他資產 | ≥164 |
| `content.json`／`asset_catalog.json` | ≥約 220 |
| 產品的場地檔 | 約 222 |
| log | ≥229 |
| movement trace | ≥234 |
| 執行檔本身 | ≥238（推論：之後 Windows 本身就開不了） |

同一台機器、同一個執行檔往上搬兩層就正常（clientA 的 `client-20261016-104436.log`）。

另外兩個發現：
1. 現在的錯誤訊息分不出「路徑太長」與「檔案不存在」（`ifstream` 沒有 OS 錯誤碼）。Explorer 內建解壓若跳過長路徑的檔案，改程式也沒用（推論，沒在測試機確認）。建議 L4 的驗收加上：開檔失敗時記錄 OS 錯誤碼與路徑長度。
2. Windows 上路徑以 ANSI code page 的窄字串傳遞（`path::string()`），code page 以外的字元不論長短都會失敗（推論，依 MSVC 語意）。v7.1 是否改成 UTF-8 是待定的子決策。

待使用者決定的子決策（在那個會話確認）：原生實作放在 `engine/io` 的哪裡（建議 `engine/io/src/fs/native/`，OS 程式只在這個資料夾）；v7.1 的路徑編碼（建議維持系統 code page）；`file://` Uri 與原生絕對路徑的對應契約（建議 v7.1 由轉接層直接建 Uri，不改 `ParseUri`，不一致處記給 os-plan）。估時暫定 6～9 h。

## 子決策定案：由 OS 子系統任務以 engine/os 的第一片實作（D196）

使用者在 OS 子系統的會話定案（經該會話轉達）：
- 實作位置：「你要先開engine/os，然後把io下嗎相關系統的實作移動過去。」`engine/os` 在 v7.1 發行前建立；`IFileSystem` 的原生實作放在 `engine/os`，`engine/io` 只留中立的抽象；asset 的讀檔（`NativeFileAssetSource`、`ContentManifest`、`AssetCatalog`）改走 `IFileSystem`。
- 路徑編碼：「現在改成 UTF-8」。同一片內改；產生 resolver 根目錄的 5 個呼叫端一起改：產品 `object_fps_pvp` 的 `PvpApplication.cpp:812`、`object_fps` 與 `object_fps_v2` 的 `ObjectFpsApplication.cpp:71`、ui_editor 的 `ReadOnlyAssetCatalog.cpp:46`、Engine 的 `ContentManifest.cpp:104`。
- `file://` 契約：「現在就修 ParseUri」（三平台的解析測試）。
- 另外：「你的目標不是修路徑，你的目標是整理OS的子系統。」

對 v7.1 的影響：
- 這一片就是 windows-long-path 的修正，由 OS 子系統任務在它的 PLAN 經使用者核准後實作；D193、D195 的實作不再由 v7.1 進行。
- 第一片的檔位、驗收與估時寫在 OS 子系統的 PLAN；預定納入：Windows 上 300 字元以上的路徑讀得到、開檔失敗時記錄 OS 錯誤碼與路徑長度、windows-x64 的 L1 套件檢查加深路徑情境。
- `PvpApplication.cpp` 會有一行變更，和 v7.1 的分支（PR #95）碰到同一個檔案，合併順序之後協調。
- `lan15-windows` 照舊等這一片完成。

## 分工更正（D197）

使用者：「你來做！！！，那邊只修架構，你等那邊修好了，就做。產品任務和架構分開。」

- OS 子系統任務：只做架構（建立 `engine/os`、把系統相關的實作從 `engine/io` 移過去、照 D196 的契約整理），完成後通知本會話。
- v7.1（本會話）：windows-long-path 的修正與驗收（Windows 上的長路徑能讀、開檔失敗時記錄 OS 錯誤碼與路徑長度、asset 的讀檔接上、產品端的呼叫端、windows-x64 的 L1 深路徑情境），在架構完成後實作。兩邊的確切界線如下（OS 子系統的會話提出，寫進它的 PLAN，使用者核准 PLAN 時一併確認；代號暫定，以 PLAN 為準）。

分層（使用者確認：「asset → io → os 的原生實作」「這個方向沒有問題。」）：asset → io → os → base。`engine/os` 是最底層，只依賴 `engine/base` 與標準庫，API 只用中立型別（UTF-8 路徑、位元組、`std::chrono`，錯誤帶 OS 錯誤碼），不認識 Uri 或 `IFileSystem`；`engine/io` 保留中立抽象，另加唯讀的原生 `IFileSystem` 後端，透過 `engine/os` 碰 OS。使用者：「那原生系統相關的實作放到 engine/os 下，io 則調用 engine/os。正常來說，io 一定會碰到操作系統問題的，如果沒碰到說明就是沒有使用和沒有對OS進行兼容，這個也是目前要修的地方之一。」

劃線原則：架構決定「OS 差異放在哪、對外承諾什麼」（新子系統、依賴方向、介面與路徑契約、Design Invariants 與守衛測試）；修正讓這次事故的行為在那個位置上成立（長路徑、錯誤記錄、接線、驗收）。長路徑程式碼的位置由架構決定，寫它屬於修正。另外更正：`engine/io` 目前沒有任何 OS 相關的實作可搬（`engine/io/src/fs/*.cpp` 只有空的 namespace），原生實作是在 `engine/os` 新寫。

| 代號 | 歸屬 | 內容 |
|---|---|---|
| A1 | 架構（OS 子系統任務） | 建立 `engine/os`：目錄與建置登錄；Design Invariants 文件；守衛測試（OS 標頭與巨集只允許在 `engine/os`，既有位置列入允許清單後逐批縮小） |
| A2 | 架構 | `engine/io` 的契約：Engine 內的路徑字串一律 UTF-8；`ParseUri` 的 `file://` 規則修正與三平台解析測試；`IoError` 帶 OS 錯誤碼的方式 |
| A3 | 架構 | `engine/os` 的唯讀檔案原語（三平台一般路徑都讀得到，含 UTF-8 轉原生字串，Windows 轉 UTF-16）；`engine/io` 的唯讀原生 `IFileSystem` 後端（`Open` 讀、`Exists`、`Stat`，其他 NotSupported），沒有 OS 標頭，呼叫上述原語 |
| P1 | 修正（v7.1，本會話） | Windows 長路徑真正讀得到：寫在 `engine/os` 的 Windows 檔案原語裡（絕對化、`\\?\` 前綴），遵守 `engine/os` 的 Design Invariants；`engine/io` 的後端不必知道長路徑 |
| P2 | 修正 | 開檔失敗時記錄 OS 錯誤碼與路徑長度（用 A2 定的 `IoError` 方式） |
| P3 | 修正 | asset 的讀檔接上 `engine/io` 的 `IFileSystem`（由它的原生後端再呼叫 `engine/os`）：`NativeFileAssetSource`（類別名與 API 不變，當薄轉接層）、`ContentManifest.cpp:93`、`AssetCatalog.cpp:30`；`IAssetSource` 不刪不改（去留由 PLAN 決定） |
| P4 | 修正 | 5 個 resolver 根目錄呼叫端改用 UTF-8，和 P3 同一個變更（P3 之後改由 Files 以 UTF-8 解讀，呼叫端不改會讓現在能用的非 ASCII 路徑壞掉）。只有 `PvpApplication.cpp:812` 屬於 object_fps_pvp；`object_fps`、`object_fps_v2` 的 `ObjectFpsApplication.cpp:71`、ui_editor 的 `ReadOnlyAssetCatalog.cpp:46` 屬於其他 owner，`ContentManifest.cpp:104` 在 Engine。這是 Engine 契約的遷移，同一變更改完所有消費端並在說明裡寫明 |
| P5 | 修正 | 驗收：Engine 單元測試（Windows 上 300 字元以上的路徑讀得到、一般路徑不變）、windows-x64 的 L1 深路徑情境、lan15-windows |

時程：A1～A3 在 OS 子系統的 PLAN 經使用者核准後實作，完成時那個會話通知本會話分支與 commit；PLAN 裡的其他部件（Wait、Threads、Process、NetErrors、`IAssetSource` 的去留）v7.1 不必等。

## OS 子系統的 PLAN 核准後（D198）

OS 子系統的 PLAN 核准（Engine D46；計畫文件 `docs/architecture/plans/os-subsystem/`，交接在 PLAN 第 8 節）。使用者：「核准 os-subsystem 的 PLAN。開放決策 1、14、15 照建議：(1) A1～A3 在發行前，其他全部排在發行後；(14) UNC 根目錄這一輪暫不支援；(15) A1～A3 併進消費端的發行線 PR。同意 A1～A3 的 Delta（新 target GYO::Os、os.md 與 io.md 的 Design Invariants 節）。」

對本批的影響：
- **PR 的形態**：A1～A3（os-skeleton、io-contracts、os-files-read）在 OS 子系統的分支完成，併進 v7.1 的發行線 PR，再加上 P1～P5：兩個任務、一個 PR。
- **UNC**：這一輪不支援 UNC 根目錄（io.md 會寫明），P1 不寫 `\\?\UNC\` 的分支。
- **P4 多兩處**：`ContentManifest.cpp:110`（`AppendFromFile((assetRoot / path).string())`）與 `ReadOnlyAssetCatalog.cpp:55`（`LoadFromFile(catalogPath.string())`），共 7 處。漏掉的話，catalog 路徑會以系統 code page 的字串交給 UTF-8 契約，造成退步。
- **P5 驗收條件**（本會話宣告；以下是 PLAN 列的交接條件）：
  - Windows 上超過 300 字元的路徑：先在未修正的樹確認 Red，再確認 Green；
  - windows-x64 L1 的深路徑套件情境：P 取在 145 到約 219 之間（例如 200），讀完所有 `.dxil`；
  - Windows 非 code page 字元的根目錄下，`ContentManifest::Load` 仍然成功；
  - 實機在 P＝156 與 P 約 210 各啟動一次；
  - 守衛 allowlist 中 asset 的條目刪除。
- P1～P5 之後的下一個失敗點是產品自己開的 `arenas.json`（約 P≥219），屬於發行之後的 os-files-write。其他部件（Process、主控台與結束要求、寫檔、trace、計時等）都排在 v7.1 發行之後。
- A1～A3 估計 20～29 h（A1 與 A2 可以並行），等使用者說開始；完成時那個會話通知分支與 commit。
- **合併的安排**（暫定，兩邊都記下）：A1～A3 用分支 `claude/trusting-joliot-f13b78`（從 master `23fab75` 切出，不帶 PR #95；開工前若 master 有新提交，先同步並告知新的基準）。PR #95 合併後，本會話從 master 切 `claude/pvp-v71-release`；A1～A3 完成後用 `git merge --no-ff` 併進發行線（不 rebase、不 squash），再做 P1～P5；整個 PR 用 `gh pr merge --merge` 合進 master。P4 那一行與 PR #95 的衝突由本會話在發行線處理。
