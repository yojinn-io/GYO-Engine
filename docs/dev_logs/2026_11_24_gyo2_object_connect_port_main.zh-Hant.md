# 2026-11-24 Object_Connect 移植實作第 1 段：主程式、輸入、資產（`v8-product-object-connect-port`）

Owner：產品（Object_Connect）。GYO2 主會話開的任務卡（使用者「開 Object_Connect 移植實作 卡」）。分支 `claude/gyo2-object-connect-port-d5884a`（開向 `gyo2`）。作者 Opus 5.5／high。
根據：移植盤點（`games/object_connect/docs/porting-inventory.zh-Hant.md`，#302、#304）；W（#308）、T（#297）、I（#301）已合併。取捨寫在盤點第 8 節。

## 做了什麼

1. **主程式** `games/object_connect/src/main.cpp`：預設 Module＋`sdl_window`；1280×720、不可改大小（G6）；asset 的內容根在執行檔旁；`--frames N`、`--verbose`。`project.json` 不變（元件表已經沒有元件，Plugin 由 `gyo_product_units` 帶）。
2. **`logic` 加兩件純邏輯**：`ui/MenuLayout`（原 `GameUiRenderer` 的版面、命中、兩種翻頁）、`input/PointerWrap`（原 `InputSystem` 的穿邊）。
3. **`runtime` 函式庫**：`GameData`（`levels.csv`、`nodes.csv` 的 loader，用 `ReferencePath` 宣告地圖與貼圖）；`RuntimeWorld`（`LoadGameData` 在生成期間載入並 `Hold()`、`ReadPlayerInput`、`PresentMenus`）。
4. **`world` 的小改**：`BuildWorld(setup, WorldOptions)`（產品給 `AfterGenerating()`）、公開 `HasNextPuzzle`。
5. **內容**：`assets/object_connect/content.json`、`asset_catalog.json`（關卡 22、圖 10、字型 1、音訊 4）。
6. **測試**：`object_connect.logic` 多兩套；新 `object_connect.runtime`（真的讀 assets、注入 `input`／`window` 的 Message）；新 `object_connect.main`（SDL dummy driver 跑 30 幀）。

## 驗證

- 本機（Intel Mac）：`object_connect.*` 4/4 過；全量 `ctest -L cpu` 結果見 PR。
- 實機：真的 Cocoa 視窗 `--frames 180 --verbose` 開窗、取得焦點、載入、進主選單、自己關窗，結束碼 0。
- **沒驗**：滑鼠與鍵盤的實際操作、穿邊、W 留給實機的三項（執行中改「能不能改大小」、`ResizeWindow`、游標）。這個會話沒有桌面操作的工具；遊戲本身也還沒用到後兩項。交使用者照 PR 的步驟跑。

## 已知限制

- 視窗貼著螢幕邊時穿不過那個邊（盤點第 8 節）；R2a 那段改 `Locked`＋遊戲游標。
- 系統游標還不隱藏（遊戲游標要 R2a 才畫得出來）。
