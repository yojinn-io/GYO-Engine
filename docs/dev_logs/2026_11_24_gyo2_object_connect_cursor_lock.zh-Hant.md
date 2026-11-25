# 2026-11-24 Object_Connect：拖線時鎖住游標（`v8-product-object-connect-port`）

Owner：產品（Object_Connect）。移植實作卡的後續（使用者「處理掉那個游標問題」）。分支 `claude/gyo2-object-connect-cursor-lock-d5884a`（開向 `gyo2`）。作者 Opus 5.5／high。取捨寫在盤點第 12 節。

## 做了什麼

1. wrapEdges 關卡拿著血管時請 `CursorGrab::Locked`，指標改用相對移動量追蹤，螢幕邊擋不住；穿邊直接移動遊戲自己的指標；放手解鎖並把系統游標放回遊戲游標的位置。解決第 1 段起的已知限制（視窗貼著螢幕邊時推不過那個邊）。
2. `--verbose` 印出鎖定與解除。
3. 另一個獨立 commit：fast_float 的註解改成「產品自己的依賴」「獨立的 target」（使用者暫定 (a)，HANDOFF `v8-arch-gyo2-oc-fast-float-a`；只改註解）。

## 設計審查（Opus／high，作者同為 Opus；Fable 暫停）與後續

blocker 0、should 1、nit 6。
- should：鎖住時把 `MouseMotion` 直接當邏輯像素用，但契約是後端的單位；Windows 相對模式是 raw input 的滑鼠計數，拖線時指標會快好幾倍。遊戲換算不了 → 主會話照新 AGENTS 判 (1)：Engine 給產品選項（#329，`v8-arch-gyo2-relative-mouse-system-scale`）。本 PR 疊在 #329 上，產品設 `LogicalPixels`。使用者補充：不准用平台條件編譯或執行期判斷平台（#329 沒有）。
- nit 處理：鎖住時關掉 `PointerWrap` 的「warp 沒被套用」防護（鎖住時 warp 當場生效，那個防護只會吃掉真的快速甩動）；補「鎖住時失焦，同一幀解鎖並放回游標」的測試。手動突變 2 個（lock 不看焦點、防護在鎖住時照開）都被抓到。
- nit 記錄（盤點第 12 節）：鎖住那一幀前的移動在 Windows 上少算一幀；SDL 拒絕相對模式時遊戲看不到；鎖住時改視窗大小不重新對齊。
- 更正：原本的已知限制比先前寫的窄（盤點第 8 節）——貼著螢幕邊時，往邊移動到達的那一幀照樣能穿邊；穿不過的只有「停在邊上再往外推」。

## 驗證

- 本機（Intel Mac）：`object_connect.*` 4/4；新測試：鎖定的請求、穿邊、放手後的解鎖與 warp、OS 回報的位置卡在螢幕邊時照樣穿邊。手動突變 3 個（不鎖、解鎖時不放回游標、鎖住時不用相對量）全被抓到。
- 實機：使用者在這台 Mac 試過：「這個我已經試過了。沒多大問題。」
