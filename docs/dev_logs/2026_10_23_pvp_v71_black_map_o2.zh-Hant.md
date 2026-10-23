# PvP v7.1 black-map-observation O2：截遊戲視窗的觀測外掛

日期：2026-10-23。**完成**（O2）。
計畫見 [black-map-observation](../object_fps_pvp/plans/v7.1/black-map-observation.md)；前一批見 [O1](2026_10_23_pvp_v71_black_map_o1.zh-Hant.md)。

## 做了什麼

- 目的：在 GUI 測試和產品 Client 啟動時截下遊戲視窗，看暖機幀有沒有露出來、露出多久（假說 H1）。只記錄，不修正，也不作為判定。
- 新增 `build/acceptance/object_fps_pvp/black_map_watch.py`（macOS 專用）：
  - 原樣執行指定的命令，依 PID 樹找出命令開的視窗；
  - 視窗出現後前 10 秒每 0.5 秒截一次，之後每 5 秒一次；
  - 跑完用 `black_frame.py` 分類，寫出 `watch.jsonl` 與 `black-map-summary.json`（每個視窗的分類序列、`warmup_like` 的起訖、第一張 `world` 的時間）。
- 預檢：先查螢幕錄製權限（不會跳對話框），再截一張丟棄，接著等 1.5 秒看有沒有新的大視窗。
- 觀測期間其他程序出現的大視窗都記成 `foreign_window`，用來事後判斷有沒有東西搶焦點。
- 支援 `--duration`：時間到就停掉整個行程群組，給不會自己結束的產品 Client 用。

## 驗證與經過

- 冒煙用主 checkout 的產品 Client，跑了 2 次，證據在 `pvp-v71-black-map-o2-smoke-20261023/`。發現四件事，都在工具裡處理了：
  1. 預檢被選單列的螢幕錄影指示器擋下（28×28，owner 是 Window Server）。預檢和 `foreign_window` 改成只看 ≥200×200 的視窗。
  2. 每次啟動 `screencapture` 要 600～1400 ms，遊戲同時出現約 220 ms 的 `slow swapchain acquire`。改成在行程內呼叫 `CGWindowListCreateImage`（`screencapture -l` 用的就是它），1 倍解析度、直接寫成 BMP：
     - 每張中位數 136 ms、最長 198 ms，同樣的負載下沒有再出現 `slow swapchain acquire`；
     - 和 `screencapture` 的 PNG 比對，顏色中位數差 0（較大的差異只出現在文字邊緣，來自 1 倍與 2 倍縮放）；
     - 冒煙時背景建置把 CPU 佔滿（負載約 20），這些時間偏高。
  3. 1 倍 BMP 一張約 4 MB。跑完後 `world`／`lobby`／`no_hud` 每個視窗只保留第一張和最後一張，其他類別全部保留，量測值都在 `watch.jsonl`。
  4. 產品的大廳被分成 `world`：大廳面板的顏色和 HUD 底色相近。O1 加上 `lobby` 分類（面板色比例：大廳 0.47、世界 0.08，門檻 0.25），大廳截圖縮小後加進 fixtures。這個改動在任何觀測跑次之前。
- 冒煙結果：產品 Client 的視窗出現後，0.1～0.3 秒拍到的第一張已經是大廳，2 次都沒有 `warmup_like`。這不算 O4 的觀測。product-1 的 log 在 `graphics ready` 之後有 345 ms、741 ms 的幀間隔，這段時間暖機幀理論上也可能露出來。
- 測試：`test_black_map_watch.py` 17 個，用假的時鐘、行程表、視窗清單與截圖，把主迴圈、分類、精簡、summary 整段跑過。涵蓋 CGImage 四種位元組順序與列尾補齊，不開 GUI。
- 突變（`run_mutations.py`）：`v71-black-map-o2` 16／16 killed；O1 的 13 個重跑，13／13 killed。
- 過程中有一個測試寫法會讓「duration 被忽略」的突變卡住不結束（假命令永不結束），改成假命令在 30 秒自行結束。

## 證據

- `build/target/_build/test/logs/pvp-v71-black-map-o2-smoke-20261023/`（product-1：`screencapture` 版；product-2：行程內截圖版；git 忽略）。
