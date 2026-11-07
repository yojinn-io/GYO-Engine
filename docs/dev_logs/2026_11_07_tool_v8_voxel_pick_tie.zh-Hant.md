# 工具 v8：ve-pick-tie（拾取在編輯平面那一層的平手修正）

日期：2026-11-07。分支 `claude/v8-tool-voxel-editor-measure`。實作：主會話（Opus 5.5／high）。
事前宣告：[voxel-editor-pick-tie.md](../tools/plans/v8/voxel-editor-pick-tie.md)，使用者選「今、小さな修正バッチで」。

**狀態：** PT1～PT3 成立；L1 三平台通過（PR #170，2026-11-07，run 42377813964）。

## 經過

1. **發現**：`ve-feel-survey` 在 km2-ground 用近距離相機（`--orbit-distance 330`）確認時，40 輪有 4 輪失敗。
   - 都是 undo 失敗：那幾輪按下與拖曳都沒有改到任何格。
   - 固定出現在兩個畫面位置。
   - 依 AGENTS §14 停下回報。
2. **診斷**：使用者選「原因を先に調べる」。
   - 量測工具把失敗的階段寫進 CSV；在 stderr 印出按下時的拾取結果與射線。
   - 結果：`Pick` 回傳「平面上的格」（onPlane），但那一格（例如 (1004, 0, 1006)）已填了地面。
   - 用印出的射線重算兩個 t：
     - DDA 累加的進入點 t：337.2832555517757；
     - 直接算的平面 t：337.2832555517746。
   - DDA 那個大約 1e-12，所以 `entered > planeT` 讓迴圈提前結束。
3. **Red**：`VoxelMapToolsTests.cpp` 加「ve-pick-tie: a slanted ray entering a filled cell of the plane's layer picks the cell」，用量到的兩條射線。修正前失敗（onPlane、法線、落點三個斷言，兩條射線都失敗）。
4. **修正**：`Editing.cpp` 的 `Pick` 中，DDA 走到「射線和平面相交的那一格」時，不拿 t 比大小，有填就回傳它。
   - 只放寬這一格。之後的格照舊由平面勝出，所以既有的「二樓在一樓上方時平面勝出」不變。
5. **Green**：測試不改，修正後通過；核心測試 61 案例全部通過。

## 驗證

- **突變**：
  - 新的 `ve-pick-tie-no-exemption`（拿掉例外）：killed。
  - 既有的 `vw1-snap-volume-first`（刪掉這一行）：它的 `find` 改成新的那一行，意圖不變，killed。
  - 合計 2/2。
- **本機**：`cpu|shader` 138/138；GPU 的 pick smoke 通過。
- **PT3**：km2-ground 近距離 40 輪的確認，失敗 0。之後的 300 輪量測也是失敗 0（結果在 [feel-survey](../voxel_map_editor/feel-survey.zh-Hant.md)）。

## 判定變更

- **沒有放寬或收緊既有的判定**。
- 只把既有突變 `vw1-snap-volume-first` 的錨點換成修正後的那一行：它刪掉的仍是「平面勝出」的那一行，期待的測試與訊息不變。

## 證據

主 checkout `build/evidence/tool-v8-voxel-map/editor-next/feel/`：
- `km2-near-check*.csv`／`.err`：診斷；
- `km2-near-after-fix-check.*`：PT3。
