# 2026-11-25 遊戲流程框架第 5 段：設置（`v8-product-pvp-game-flow`）

Owner：產品（Object_FPS_PVP）。GYO2 主會話開的任務卡（遊戲流程框架）。作者 Opus 5.5／high。分支疊在第 4 段（#351）上。
根據：[事前宣告](../../games/object_fps_pvp/docs/game-flow-declaration.zh-Hant.md)第 3.6 節與第 4 節第 5 段；使用者 `v8-arch-gyo2-settings-resolution-fullscreen`、`v8-arch-gyo2-window-mode-vsync-start`。

## 做了什麼

1. **設定的資料**（`include/object_fps_pvp/client/Settings.hpp`）：`ClientSettings{resolution, window, vsync}`；解析度只有四種（1280×720、1600×900、1920×1080、2560×1440）；檔案 `{"version":1,"resolution":[w,h],"windowMode":"windowed"|"borderless","vsync":bool}`（JSON Module）；讀不到或不是設定檔時用預設值並說明原因。
2. **主菜單的「設定」**（`pvp_menu.json` 的 `settings` canvas，同一個場景換 canvas）：解像度（按一下換下一種）、画面（ウィンドウ／全画面（枠なし））、VSync（オン／オフ）、適用、戻る。適用：尺寸不同時送 window 的 `ResizeWindow`，存檔，顯示結果。在用的設定是 Frame World 的 Resource，`CarryOver` 帶過場景。
3. **程式**：啟動時讀 `<使用者資料目錄>/GYO/object_fps_pvp/settings.json`（一台機器一份，不分 profile），視窗用它的尺寸開；讀不到就記一行警告、用預設值。
4. **視窗模式與 VSync**：選擇與存檔都做了；套用要等「視窗模式＋VSync」卡合併（它的介面：window 的 `SetWindowMode` Message 與 `WindowSettings::mode`，render 的 `Presentation`（Render World，Extract 從來源抄））。那張卡合併後，本卡接上：啟動時的 `WindowSettings::mode`、適用時送 `SetWindowMode`、Render World 的 Extract 寫 `Presentation`。

## 驗證

- 本機（Intel Mac）`ctest -L "cpu|gpu"` 256/256、`-L "go|network"` 5/5。
- client：編碼往返、不提供的尺寸與別的版本讀不進、下一種尺寸繞回、壞檔回預設並說明；主菜單 → 設定 → 尺寸換兩次（1920×1080）、全画面、VSync オフ → 適用：送出一次 `ResizeWindow{1920,1080}`、存檔、讀回相同；換場景後設定還在。
- 突變：`pvp-settings-no-resize`、`pvp-settings-not-saved`、`pvp-settings-not-carried`、`pvp-settings-any-size`，加 `pvp-menu-multi-ignored`：5/5 killed。

## 沒驗

- 真的視窗換尺寸、比例（UI 的 fit 與相機）：交使用者試玩。視窗模式與 VSync 的實際套用：等那張卡。
- 程式啟動時讀設定（`client_main.cpp`）：沒有自動測試，實機看。
