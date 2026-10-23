# black_frame 的真實樣本

owner：object_fps_pvp。`N-gui-black-map-intermittent` 的觀測工具 `build/acceptance/object_fps_pvp/black_frame.py`（只記錄，不是判定）用這些圖測試；計畫見 [black-map-observation](../../../../docs/object_fps_pvp/plans/v7.1/black-map-observation.md)。

都是 2026-10-23 的證據，用最近鄰取樣縮小到寬 320（不混色，像素值都是原圖的值）。原圖在 git 忽略的建置目錄。

| 檔案 | 內容 | 原圖（sha256） |
|---|---|---|
| `screen-world.png` | 螢幕截圖裡 create 那個 GUI 視窗：上方 8 列是標題列，下面是 16:9 的內容（有 HUD） | `pvp-v71-gui-black-map-20261023/A-22-s.png`（`d0196b4e…`）的 x 1～856、內容列 42～523、其上 19 列標題列 |
| `readback-world.png` | 場景讀回（`scene` 貼圖，在 UI 之前，所以沒有 HUD） | `pvp-v71-sdl-isolation-l2-20261023b/runs/gui-probe-A/gui/join-world.bmp`（`5e6a38b6…`） |
| `window-warmup.png` | 暖機幀（黑屏）的真實樣本：O3 正常建置的 timing-gui-short，create 視窗出現後 0.013 秒的截圖 | `pvp-v71-black-map-o3-20261023/timing-normal/watch/w56733-000.bmp`（`f24e3ce1…`） |
| `window-m3-black-with-hud.png` | O3 的突變 M3（世界幀的清除色改為 0、不送地板和牆，HUD 照畫） | `pvp-v71-black-map-o3-20261023/m3/watch/w56748-006.bmp`（`cc79823f…`） |
| `window-lobby.png` | 產品 Client 的大廳視窗，O2 冒煙時以行程內截圖（1 倍）取得：上方 8 列是標題列 | `pvp-v71-black-map-o2-smoke-20261023/product-2/w56590-020.bmp`（`d9629b53…`），標題列 32 列、內容 1280×720 |

