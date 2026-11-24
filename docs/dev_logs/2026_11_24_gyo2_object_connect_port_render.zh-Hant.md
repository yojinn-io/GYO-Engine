# 2026-11-24 Object_Connect 移植實作第 3 段：畫面（`v8-product-object-connect-port`）

Owner：產品（Object_Connect）。GYO2 主會話開的任務卡（使用者「開 Object_Connect 移植實作 卡」；主會話 2026-11-24 轉告 R2a #315 已合併；使用者在本卡說「繼續」）。分支 `claude/gyo2-object-connect-render-d5884a`（開向 `gyo2`，疊在第 2 段 #316 上）。作者 Opus 5.5／high。
取捨寫在盤點第 10 節。

## 做了什麼

1. `runtime/Drawing`：`Canvas`（組 `Pass2D`，同一張貼圖的連續繪製併成一個 `Draw2D`）、文字（`GlyphAtlas` → 貼圖）、`DrawFrame` System。
2. `runtime/PuzzleDrawing`（原 `PuzzleRenderer::Draw`）、`runtime/UiDrawing`（原 `GameUiRenderer::Draw`）。
3. 遊戲畫自己的游標，同時隱藏系統游標。
4. 地圖 CSV 改成遊戲自己的型別 `MapTable`（宣告它指到的圖）。
5. 主程式帶 `render_world`、`sdl_gpu`；`--hidden`；結束時若有畫壞的幀、跳過的繪製或被拒的貼圖，記一行 Warning。
6. 測試：`object_connect.main` 改 gpu 標籤；`object_connect.runtime` 加畫面資料與 sRGB 換算的檢查。

## 驗證

- 本機（Intel Mac）：`ctest -L "cpu|gpu"` 238/238（`gyo_os_tests`、`os.wake_fallback` 在 load average 30～50 時逾時，單獨重跑都過，與本段無關）。
- 實機：使用者在這台 Mac 上玩過一輪（Metal）：選單、拖線、取消、wrapEdges 關卡的 warp、解開、最終結果、回主選單；使用者確認畫面「可以」。

## 設計審查（Opus／high，作者同為 Opus；Fable 暫停）

blocker 0、should 4、nit 3。處理：

- should：Top 對齊的字比原版低 0.1 倍字高（Engine 預設行高 1.2，原版這個字型是 1.0）→ 行高 1.0。
- should：新測試在圖層順序錯、節點圖沒畫時也會過 → 檢查最後一個繪製是游標、文字在最後一個白色繪製之後、第 1 關畫了心臟與腦的圖；手動突變 2 個（文字先畫、找不到節點圖）都被抓到。
- should：`object_connect.main` 什麼都沒畫也會過 → `--frames` 模式下沒有任何一幀顯示、或有畫壞的情形時結束碼 1。
- should：拿掉 16 ms 下限後，視窗最小化時迴圈會空轉 → 下限 4 ms。
- nit 處理：多行文字逐行對齊；貼圖也檢查高度。
- nit 不處理：貼圖與文字都用 Linear 濾波（HiDPI 拉伸時會雙線性放大，原版是 1:1）；`object_connect.main` 改 gpu 標籤後，L1 只在 Linux 跑啟動（Windows、macOS 只建置）。

## 已知的差別與限制

- 混合在 sRGB 位元組上做（R2a 的規則），半透明覆蓋層比原版稍暗。
- 拖線時不切 `Locked`：視窗貼著螢幕邊時推不過那個邊（盤點第 8 節）。
