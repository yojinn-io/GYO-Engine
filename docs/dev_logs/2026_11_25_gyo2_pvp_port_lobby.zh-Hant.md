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
- 突變：`pvp-lobby-filter-off`、`pvp-lobby-any-address`、`pvp-lobby-quit-ignored`、`pvp-session-end-ignored` 4/4 killed（審查後 5/5，見下）。
- **沒驗**：真的視窗裡的大廳（IME、點擊、畫面）、大廳 → Client → 回大廳時視窗的關開。交使用者試玩。

## 設計審查（Opus／high，唯讀；作者同模型，Fable 暫停中）

blocker 0、should 4、nit 5。紀錄：`build/evidence/v8-product-pvp-port/segment5-design-review.zh-Hant.md`（本機）。

- S1（沒讀 `UiRootState::failure`、素材失敗沒 log）→ 新 System `WatchDocument`（`Main`／`Last`）：文件顯示了就記 `shown`，沒顯示時記原因（素材 Failed 經 `AssetView::Failure`，或 world_ui 的 failure）；程式記 log，`--frames` 跑大廳而文件沒顯示就回 1。
- S2（`quit` 沒人讀、突變碰巧被殺）→ 拿掉 `LobbyOutcome::quit`（「終了」和關窗結果一樣）；突變改成拿掉 quit 分支的關窗。
- S3（`--frames` 的結束碼悄悄改了）→ `--frames` 時內容不符照舊回 1。
- N1（經 `shared_ptr` 繞過 System 的存取宣告）→ `LobbyOutcome` 改成大廳 World 的 Resource，`ResMut` 寫、Run 之後讀。
- N3（沒驗渲染 World 有畫）→ 大廳測試讀渲染 World 的 `RenderCommands`（2D pass 有 draw），突變 `pvp-lobby-ui-not-drawn`。
- 不改、記下：S4（程式層的「大廳 → Client → 回大廳」與同行程重建 Product 沒有自動驗證：交實機）、N2（Client 的 Product 建不起來時直接結束，不回大廳：產品的決定）、N4（`UiRootState` 手動插入可能多餘，沒確認 InitialValue 的語意，照 ui 測試的寫法留著）、N5（重連沿用同一個 userId，Match 若還留著舊連線會有兩個同 Owner 的玩家：net 宣告 P15 歸產品，很少發生，試玩看）。
- 交使用者：長期要不要「大廳與對局同一個視窗」（要改 Engine：Client 的連線位址改成每個 client World 準備時給，或 `ReplaceWorld` 允許換描述；公開介面）。

修正後：`ctest -L "cpu|gpu"` 253/253，第 5 段突變 5/5 killed（加 `pvp-lobby-ui-not-drawn`）。

## 已知的代價

- 大廳與 Client 是兩個 Product，各開一個視窗；換的時候視窗關掉再開（連線位址在 Product 建好時定，D-8）。
