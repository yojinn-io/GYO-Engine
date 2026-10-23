# PvP v7.1 black-map-observation O4：L2 全部案例與產品 Client 的觀測跑次

日期：2026-10-23。**完成**（O4）。
計畫見 [black-map-observation](../object_fps_pvp/plans/v7.1/black-map-observation.md) 第 2b 節；前一批見 [O3](2026_10_23_pvp_v71_black_map_o3.zh-Hant.md)。

## 做了什麼

- 用 sdl-isolation L2 的 10 個案例（原命令，路徑換成本 worktree `ea04478` 的建置），各跑 2 次，共 20 次；接著產品 Client 連續啟動 5 次（每次 12 秒，`--gateway 127.0.0.1:9`，停在大廳）。
- 每次都由觀測外掛包住，逾時沿用 L2 的值。runner 自己的結果不算數。
- 預測在 O3 之後、看 O4 結果之前寫進計畫第 2a 節；彙總腳本也在結果出來之前寫好。

## 驗證與經過

- 預測 11 項中 10 項符合：
  - 暖機幀只出現在 timing-gui-short 與 bots2-timing-gui-short，每次從視窗出現起約 2.5～2.7 秒；
  - action-short、player-short、gui-bots、bots2-action60、bots2-player60、gui-probe、rapid-edge-60、rapid-edge-144 與產品 Client 一張都沒有。
- **不符**：timing-gui-short 第 1 次的兩個視窗都沒有暖機幀。
  - 那一次視窗出現在螢幕上的時刻是命令開始後 5.46 秒，其他 timing 類跑次約 1.4～1.5 秒；視窗出現時已經是大廳或世界。
  - 所以暖機幀露不露出，取決於視窗是否在停頓期間就顯示。O3、O4 的 timing 類 6 次中 5 次露出。
  - 這一次為什麼晚約 4 秒才顯示，沒有查。目前的觀測分不出是視窗建立得晚，還是顯示得晚。
- rapid-edge-144 兩次各有 1 張 `all_black`／`missing`：都是視窗消失前約 0.5 秒的最後一張，推論是視窗拆除途中，未驗證。
- 截圖每張中位數 61～69 ms、最長 194 ms；沒有其他程序的視窗出現。
- timing 類跑次 runner 自己判 F，原因和 O3 相同（`combat-gui-evidence` 的 HUD 不符），與黑屏無關。

## 證據

- `build/target/_build/test/logs/pvp-v71-black-map-o4-20261023/`（`o4_drive.py`、`o4_summary.py`、`progress.jsonl`、`o4-summary.json`、`runs/<案例>-<次>/watch/`；git 忽略）。
