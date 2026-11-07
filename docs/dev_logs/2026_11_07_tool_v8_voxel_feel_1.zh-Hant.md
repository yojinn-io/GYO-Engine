# 工具 v8：ve-feel-1（手感第一批）

日期：2026-11-07。分支 `claude/v8-tool-voxel-editor-measure`。下段。實作：主會話（Opus 5.5）；審查：Opus／high（唯讀）。
事前宣告：[voxel-editor-feel-1.md](../tools/plans/v8/voxel-editor-feel-1.md)，你核准（「承認、PR は後で」）。

**狀態：** E1～E4、L3 成立；E5 的本機部分成立（`cpu|shader` 138/138），L1 待 PR。

## 做了什麼

- **F1 縮放**：
  - 核心的 `Visibility.hpp` 加了 `ChunkBounds`、`FrameDistance`、`ZoomLimits`、`ClampZoom`、`DrawDistanceFor`、`ChooseChunks`（上限 `MaxDrawnChunks` = 2,000）。
  - 縮放的範圍：下限 2 格；上限是框住整張地圖的距離，跟著地圖大小變。
  - 拉遠時，繪製距離取「面板值」與「相機距離 × 1.5」的較大者。
  - 畫的區塊超過 2,000 時，從近的取，面板提示。
  - 面板加了對數刻度的縮放滑桿與鎖定。地圖範圍依文件的 revision 快取。拖曳中不修正縮放。
- **F2 面板**：四個可收合的組——檔案／工具與模式／方塊與朝向／檢視與相機。朝向移到方塊旁邊；功能與快捷鍵不變。
- **F3 朝向箭頭**：放置的位置畫「上」（黃）與「正面 +Z」（青）。
  - Add 用面板的朝向。
  - Paint 用那一格原本的朝向（見「和宣告的差異」）。
- **F4 軸箭頭**：預覽左下角的 X（紅）／Y（綠）／Z（藍），照 Engine 的 view 矩陣旋轉。
- **F5 Line**：按下、拖曳、放開成為一步；Esc 取消。核心加了 `RedrawStroke`；拿掉「點兩下」。
- **量測工具**：輸出實際用的繪製距離與 `capped`。

## 驗證

- **規則測試**：
  - E1：縮放範圍，含空圖、單格、多區塊與負座標、km²、4 格下限。
  - E2：繪製距離、上限、同距離時依序號。
  - E3：Line 只留最後一條、一步 undo、Esc 放回。
- **突變**：`vf1-*` 8 個，**8/8 killed**。另外 `ve-cull-distance-x-only` 的錨點換到搬過去的那一行（意圖不變），killed。
- **本機**：`cpu|shader` 138/138；GPU 的 pick smoke 通過。
- **E4**（Chrome 關閉，最遠的縮放＝量測的預設相機，600 幀丟前 60；件數都一致：600／600／0）：

| 情境 | 結果 | 預算 | 判定 |
|---|---|---|---|
| 舊版 `a19458cc` building（對照） | p95 7.89 ms | — | 環境正常（P1 當時 7.48 ms） |
| building | p95 **8.30 ms**，畫 767，未達上限；實際繪製距離 493 格 | ≤ 16.7 ms | 成立 |
| km2-ground | p95 **14.99 ms**，畫 **2,000**（達上限）；實際繪製距離 3,818 格 | ≤ 16.7 ms、≤ 2,000 | 成立 |

## 審查（Opus／high，唯讀）

- **沒有阻擋項目**，也沒有違反 DI。
- **處理**：
  1. Paint 的箭頭會顯示用不到的朝向。這是宣告本身的錯誤，交你決定；你選「セルの今の向き」，已照做。
  2. 量測工具的前提變了，輸出卻看不出來：已加「實際繪製距離」與 `capped`，並在本 dev_log 註明（見下）。
  3. 測試的檢出力不足：已加上 `MaxDrawnChunks`、同距離的切點、多區塊與負座標的範圍、`FrameDistance` 的下限，並加了兩個突變。
  4. 每幀重算地圖範圍：改成依 revision 快取。
  5. 拖曳中修正縮放會讓射線跳動：拖曳中不修正。
  6. 軸箭頭加了 Z 旋轉；`RedrawStroke` 註明前提；拿掉多餘的變數。

## L3

**成立**（2026-11-07）。你在 building 地圖的副本上試用後說：「嗯，很好基本功能都有了」。副本與 log 在主 checkout `build/evidence/tool-v8-voxel-map/editor-next/feel-1/l3/`。

## 和宣告的差異

1. **F3 的 Paint**：宣告寫「Add 與 Paint 都畫面板的朝向」。但 Paint 會保留格子原本的朝向，所以照你的選擇改成畫那一格的朝向。
2. **量測的前提**：`measure-preview` 的繪製距離改成和編輯器一樣（跟著相機距離擴大，上限 2,000）。所以 ve-cull 的 CB1～CB3 和之後的量測前提不同；CB 的結果不重算，照記。

## 證據

主 checkout `build/evidence/tool-v8-voxel-map/editor-next/feel-1/`：`protocol.txt`、`summary.txt`、各 CSV。
