# 2026-11-24 Object_Connect：拖線時鎖住游標（`v8-product-object-connect-port`）

Owner：產品（Object_Connect）。移植實作卡的後續（使用者「處理掉那個游標問題」）。分支 `claude/gyo2-object-connect-cursor-lock-d5884a`（開向 `gyo2`）。作者 Opus 5.5／high。取捨寫在盤點第 12 節。

## 做了什麼

1. wrapEdges 關卡拿著血管時請 `CursorGrab::Locked`，指標改用相對移動量追蹤，螢幕邊擋不住；穿邊直接移動遊戲自己的指標；放手解鎖並把系統游標放回遊戲游標的位置。解決第 1 段起的已知限制（視窗貼著螢幕邊時推不過那個邊）。
2. `--verbose` 印出鎖定與解除。
3. 另一個獨立 commit：fast_float 的註解改成「產品自己的依賴」「獨立的 target」（使用者暫定 (a)，HANDOFF `v8-arch-gyo2-oc-fast-float-a`；只改註解）。

## 驗證

- 本機（Intel Mac）：`object_connect.*` 4/4；新測試：鎖定的請求、穿邊、放手後的解鎖與 warp、OS 回報的位置卡在螢幕邊時照樣穿邊。手動突變 3 個（不鎖、解鎖時不放回游標、鎖住時不用相對量）全被抓到。
- 實機：使用者在這台 Mac 試過：「這個我已經試過了。沒多大問題。」
