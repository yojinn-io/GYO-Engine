# black_frame 的真實樣本

owner：object_fps_pvp。`N-gui-black-map-intermittent` 的觀測工具 `build/acceptance/object_fps_pvp/black_frame.py`（只記錄，不是判定）用這兩張圖測試；計畫見 [black-map-observation](../../../../docs/object_fps_pvp/plans/v7.1/black-map-observation.md)。

兩張都是 2026-10-23 的證據，用最近鄰取樣縮小到寬 320（不混色，像素值都是原圖的值）。原圖在 git 忽略的建置目錄。

| 檔案 | 內容 | 原圖（sha256） |
|---|---|---|
| `screen-world.png` | 螢幕截圖裡 create 那個 GUI 視窗：上方 8 列是標題列，下面是 16:9 的內容（有 HUD） | `pvp-v71-gui-black-map-20261023/A-22-s.png`（`d0196b4e…`）的 x 1～856、內容列 42～523、其上 19 列標題列 |
| `readback-world.png` | 場景讀回（`scene` 貼圖，在 UI 之前，所以沒有 HUD） | `pvp-v71-sdl-isolation-l2-20261023b/runs/gui-probe-A/gui/join-world.bmp`（`5e6a38b6…`） |

暖機幀（黑屏）的真實樣本要等觀測工具 O3 的跑次才會有，屆時加進本目錄。
