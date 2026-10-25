# windows-long-path：發行包解壓到深資料夾時開不了 shader（2026-10-25：D193 → L4（D195）→ 子決策定案（D196）→ 分工更正（D197）：OS 子系統任務只做架構，windows-long-path 由 v7.1 在架構完成後實作）

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
- v7.1（本會話）：windows-long-path 的修正與驗收（Windows 上的長路徑能讀、開檔失敗時記錄 OS 錯誤碼與路徑長度、asset 的讀檔接上、產品端的呼叫端、windows-x64 的 L1 深路徑情境），在架構完成後實作。兩邊的確切界線和那個會話確認後記在這裡。
