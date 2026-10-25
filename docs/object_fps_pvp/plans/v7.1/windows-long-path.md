# windows-long-path：發行包解壓到深資料夾時開不了 shader（2026-10-25 使用者決定 w2，D193；實作等使用者說開始）

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
