# 2026-11-25 PvP 移植實作第 5 段：大廳（`v8-product-pvp-port`）

Owner：產品（Object_FPS_PVP）。GYO2 主會話開的任務卡（PvP 移植實作）。作者 Opus 5.5／high。分支疊在第 3 段（#327）上，並 merge 了 origin/gyo2（U 卡 #322、#326）。
根據：事前宣告第 1 節（第 5 段，2026-11-24 主會話加）、第 2 節「PV12 大廳」；`docs/architecture/target/v2/ui.zh-Hant.md` 第 9 節。

## 做了什麼

1. **大廳**（`include/object_fps_pvp/client/LobbyWorld.hpp`、`src/client/LobbyWorld.cpp`）：自己的 Product，`Game`（`world_asset`、`camera`、`input`、`window`、`window_camera`、`world_ui`、`world_render`、`sdl_window`）＋`Render`（`render_world`、`camera_render`、`ui_render`、`sdl_gpu`）。照 ui 頁第 9 節：登記 ui 與字型的 loader；一個實體掛 `UiRoot`（文件 `object_fps_pvp.ui.lobby`、canvas `lobby`、相機指定）與 `UiBindings`（`address`、`status`、`hint`）；`UiRoot::filter` 對位址欄用遊戲的 `FilterAddressText`（全形、句號轉 ASCII）；`LobbyActions`（`Update`）分派 `pvp.submit_address`（有效就記下位址、關窗；無效就寫狀態、文字留在欄裡）與 `pvp.quit`。
2. **文件**：`assets/object_fps_pvp/ui/pvp_lobby.json`（gyo.ui v2，日語；master 的房間按鈕不帶，PV18 不做），catalog 加 `object_fps_pvp.ui.lobby`（型別 `ui`）。
3. **連線結束**：Client 的 World 讀 net 的 `PeerDisconnected`，記下理由（被拒、無回應、逾時、Match 關閉）並關窗；`StatusOf` 多 `ended`、`endReason`。
4. **程式**（`src/client_main.cpp`）：大廳 → 用送出的位址建 Client → 連線結束時帶理由回到大廳；關窗或「終了」結束。`--connect` 第一次跳過大廳；`--frames N` 只跑一個 Product。`ParseMatchAddress` 給大廳與 `--connect` 共用。
5. **CMake**：產品的 Module 加 `world_ui`、`ui_render`；client 函式庫連 `GYO::Ui`、`GYO::OS`。

## 驗證

- 本機（Intel Mac）：`ctest -L "cpu|gpu"` 253/253；`object_fps_pvp.*` 6 項（新 `object_fps_pvp.main_connect`：`--connect` 的 Client 開窗畫 30 幀；`object_fps_pvp.main` 現在是大廳開窗畫 30 幀）。
- `object_fps_pvp.client` 加 3 案：大廳的文件與字型載入、畫出來，形不對的位址寫狀態不關窗，全形數字與「。」送出後成為 `192.168.0.10:40000` 並關窗；「終了」；Match 沒有座位時 Client 記下「Match に断られました（the match is full）」並關窗。
- 突變：`pvp-lobby-filter-off`、`pvp-lobby-any-address`、`pvp-lobby-quit-ignored`、`pvp-session-end-ignored` 4/4 killed。
- **沒驗**：真的視窗裡的大廳（IME、點擊、畫面）、大廳 → Client → 回大廳時視窗的關開。交使用者試玩。

## 已知的代價

- 大廳與 Client 是兩個 Product，各開一個視窗；換的時候視窗關掉再開（連線位址在 Product 建好時定，D-8）。
