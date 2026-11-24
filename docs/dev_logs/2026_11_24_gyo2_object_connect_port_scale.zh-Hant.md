# 2026-11-24 Object_Connect 移植實作第 4 段：固定解析度的縮放（`v8-product-object-connect-port`）

Owner：產品（Object_Connect）。GYO2 主會話開的任務卡（使用者「開 Object_Connect 移植實作 卡」；使用者在本卡告知「R2b已經合併」）。分支 `claude/gyo2-object-connect-scale-d5884a`（開向 `gyo2`，疊在第 3 段 #318 上）。作者 Opus 5.5／high。
取捨寫在盤點第 11 節。

## 做了什麼

1. 選 R2b 卡給的 (A)：維持單一 World 自己組 `Pass2D`（動態頂點第 3 段已用 R2a 做好）。
2. `runtime/PlayfieldFit`：camera 的 `Compute`＋`Letterbox{1280,720}` 算出遊戲畫面在視窗裡的矩形；`DrawFrame` 設 `Pass2D::viewport`（黑邊），`ReadPlayerInput` 以它換算滑鼠、穿邊與 warp。
3. 跟上 R2b：`RenderCommands::passes` 變成 `variant<Pass2D, Pass3D>`，測試讀 pass 的地方改用 `std::get`。

## 驗證

- 本機（Intel Mac）：`object_connect.*` 4/4；新測試：`FitPlayfield` 的五種視窗、1600×720 視窗裡的點擊與畫面的 viewport、黑邊裡的穿邊與 warp。手動突變 3 個（不扣視區的偏移、不設 viewport、warp 不加偏移）全被抓到（第一次的測試沒抓到前兩個之一，補強後才抓到）。
- 實機：真實視窗 `--frames 120 --verbose`（Metal、coreaudio）exit 0（有幀顯示、沒有畫壞的）。
