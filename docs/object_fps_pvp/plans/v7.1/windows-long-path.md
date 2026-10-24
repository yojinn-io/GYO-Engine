# windows-long-path：發行包解壓到深資料夾時開不了 shader（關卡，2026-10-25，等使用者決定）

發行線的第一批（INVENTORY `N-windows-max-path`）。唯讀調查，還沒有改任何程式。

## 現象與成因

- 2026-10-16 的全 Windows LAN 臨時測試：Client 開不了 shader，路徑 271 字元，超過 Windows 的 MAX_PATH（260）。證據：主 checkout 的 `build/target/_build/test/logs/pvp-v71-windows-lan-20261016/logs/clientA/logs/client-20261016-104011.log:2`。
- 長度的來源：
  - 發行包內的資產路徑本身就長：`gyo-object_fps_pvp/bin/assets/object_fps_pvp/shaders/builtin/objects/<64 字元雜湊>.dxil`；
  - 壓縮檔解開後多兩層同名資料夾（`gyo-package-object_fps_pvp-windows-x64/gyo-object_fps_pvp-windows-x64/`）；
  - 玩家把它放在 `C:\Users\<名字>\Downloads\<下載工具產生的資料夾>\` 底下。
- CI 的套件檢查解在短路徑，所以沒抓到。repo 裡沒有 `longPathAware`，也沒有 `\\?\` 前綴。
- 讀檔的是 Engine 的 `NativeFileAssetSource::ReadAll`（`engine/asset`），所有資產（shader、arena、字型）都經過它。

## 選項

| 代號 | 做法 | 優點 | 代價 |
|---|---|---|---|
| w1 | Windows 執行檔的 manifest 宣告 `longPathAware` | 改動小 | 只有在系統開了 `LongPathsEnabled`（登錄檔，消費者版 Windows 預設關閉）時才有效，單獨用解決不了 |
| w2 | Engine 的 `NativeFileAssetSource` 在 Windows 上把絕對路徑轉成 `\\?\` 延伸長度路徑再開檔 | 不需要系統設定；所有資產一次解決 | Engine 的改動（資產 IO），要照 Engine 的 Design Invariants 檢查；只修資產，日誌、trace 等其他寫檔路徑另外看 |
| w3 | shader 檔名的雜湊縮短（例如 64 → 16 字元） | 路徑短約 48 字元 | 改 shader 建置與 manifest 的內容契約；更深的資料夾仍會超過 |
| w4 | 發行包少一層資料夾 | 路徑短約 40 字元 | 改打包流程；同樣只是延後超過的深度 |

## 建議與驗收

- **建議 w2**：唯一不靠系統設定、也不只是延後問題的做法。可以和 w4 一起做，但不是必要。
- **驗收**：windows-x64 的 L1 套件檢查加一個深路徑情境（解到總長超過 300 字元的資料夾，Client 的煙霧測試能載入 shader）；Engine 的單元測試（Windows 上超過 260 字元的路徑讀得到）。
- **估時**：2～4 h（D159 給的範圍內）。
