# 2026-11-24 Object_Connect 移植實作第 4 段：固定解析度的縮放（`v8-product-object-connect-port`）

Owner：產品（Object_Connect）。GYO2 主會話開的任務卡（使用者「開 Object_Connect 移植實作 卡」；使用者在本卡告知「R2b已經合併」）。分支 `claude/gyo2-object-connect-scale-d5884a`（開向 `gyo2`，疊在第 3 段 #318 上）。作者 Opus 5.5／high。
取捨寫在盤點第 11 節。

## 做了什麼

1. 選 R2b 卡給的 (A)：維持單一 World 自己組 `Pass2D`（動態頂點第 3 段已用 R2a 做好）。
2. `runtime/PlayfieldFit`：camera 的 `Compute`＋`Letterbox{1280,720}` 算出遊戲畫面在視窗裡的矩形；`DrawFrame` 設 `Pass2D::viewport`（黑邊），`ReadPlayerInput` 以它換算滑鼠、穿邊與 warp。
3. 跟上 R2b：`RenderCommands::passes` 變成 `variant<Pass2D, Pass3D>`，測試讀 pass 的地方改用 `std::get`。

## 設計審查（Opus／high，作者同為 Opus；Fable 暫停）

blocker 0、should 2、nit 3。處理：

- should：游標在黑邊裡完全看不到（遊戲畫面的 pass 只畫在 viewport 裡，系統游標又藏著）→ 遊戲的游標另起一個涵蓋整個視窗的 pass。
- should：沒有測縮放比例不是 1 時的換算 → 加 3200×1440／2 倍的點擊、viewport、黑邊裡的游標與 warp；加 1600×900（1.25 倍）時游標的位置與大小。手動突變 2 個（warp 加實體像素的偏移、游標不乘倍率）都被抓到（後者第一次沒抓到，補了 1.25 倍的測試才抓到）。
- nit 不處理：黑邊點擊的檢查抓不到「把黑邊裡的位置夾到畫面邊」的錯（邊上沒有東西）；viewport 用的是最後一次 `WindowResized` 的大小，換到另一個縮放比例的螢幕時可能有一幀偏掉（視窗不可縮放，風險低）；`RenderCommands.hpp` 開頭的註解還寫 view 拉滿整個目標（Engine 的，不是本段，轉主會話）。

## 驗證

- 本機（Intel Mac）：`object_connect.*` 4/4；新測試：`FitPlayfield` 的五種視窗、1600×720 視窗裡的點擊與畫面的 viewport、黑邊裡的穿邊與 warp。手動突變 3 個（不扣視區的偏移、不設 viewport、warp 不加偏移）全被抓到（第一次的測試沒抓到前兩個之一，補強後才抓到）。
- 實機：真實視窗 `--frames 120 --verbose`（Metal、coreaudio）exit 0（有幀顯示、沒有畫壞的）。
