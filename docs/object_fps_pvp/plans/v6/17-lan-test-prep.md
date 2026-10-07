# 第 17 批：聯機測試準備（地圖與日誌）

狀態：進行中（2026-10-08 開始，分支 `claude/pvp-v6-batch17` 自 `acbebc9`，第 14a 批之後）。使用者決定見 [HANDOFF](HANDOFF.md) 的 D25。
起因：使用者 2026-10-08 要和朋友做 LAN 聯機測試：Match 與 Gateway 在 Mac 上，Client 是 3 台 Windows。之後發行 v1.1.0（含本批）。

## 目標與範圍

做：

- **地圖**：目前的地圖（`pvp_arena.json`，`pvp_training_v1`）保留為測試與驗收用的地圖。新增發行用的地圖 `pvp_corners.json`（`pvp_corners_v1`）：牆與柱子相同，4 個出生點分散在四角 (3,3)、(17,17)、(17,3)、(3,17)，都朝向中心。
- **Client 依 Match 選地圖**：Client 安裝 `arenas.json` 列出的全部地圖；加入時依 Match 宣布的 arena id 與版本選出對應的一張，再比對內容 digest。沒有對應的地圖時沿用 `arena_identity_mismatch`，訊息列出已安裝的地圖。用哪張地圖由啟動 Match 的人以 `--arena` 決定。
- **日誌**：Client、Gateway、Match 各加 `--log`（Gateway 為 `-log`）。每行有牆鐘時間（毫秒，含時區）與單調時鐘；啟動時記錄自己執行檔的 SHA-256、OS 與設定。Client 未指定時寫到執行檔旁的 `logs/`。內容見下。
- **聯機測試手冊**：Mac 的 LAN IP 與 `-advertise-ip`、防火牆、啟動命令、要上傳的檔案。

不做：

- 建房時在 Client 選地圖：需要多房間／換地圖與協議變更，列入 [v8](../v8/README.md)。本批的多地圖安裝是它的前提。
- 不改 wire（pv6）；不改驗收工具與權威測試（它們繼續用 `pvp_training_v1`）。
- 不改公共層的建置設定：版本識別用執行檔的 SHA-256，不把 commit 寫進建置。

## 日誌內容

| 角色 | 事件 | 週期摘要 |
|---|---|---|
| Client | 啟動（SHA-256、OS、GPU 驅動、已安裝的地圖）、建房／加入／離開、Welcome（玩家 id、地圖）、錯誤與被移出的原因、`CONNECTION POOR` 的開關 | 每秒：FPS 與最長幀、最新 Snapshot 的年齡、待確認命令、送出的動作與拒絕數 |
| Gateway | 啟動（SHA-256、位址）、加入／Hello（client 位址）／離開／逐出／過期 | 每 10 秒：每位玩家的收送封包、限流、最後收到的時間 |
| Match | 啟動（SHA-256、地圖）、加入、離開、死亡、重生、逐出 | 每 10 秒：Tick、玩家數、主機晚醒 |

不同機器的時鐘沒有對齊；分析時以各機器的單調時鐘為主，牆鐘只作大致對照。

## 驗收點

L1：CTest、`go test -race`、CI 四平台；權威兩樹比對（測試地圖不變，預期 35／35）；新地圖的內容測試（出生點數、四角、彼此可放置、主持條件）；Client 多地圖選擇的測試（選中、未安裝、內容不同）；突變。
L2（開發量測，不計次）：quad 以兩張地圖各跑一輪；以日誌選項實跑三個角色，檢查日誌內容。完整驗收放在聯機測試之後（使用者決定）。
L3：使用者的聯機測試（Windows 的 L4 證據）。

## 建議檔位

high；Client 的地圖選擇與身分比對局部 xhigh。

## Architecture Delta

1. 需求：LAN 聯機測試需要分散的出生點，同時保留驗收用的地圖（使用者 2026-10-08）。
2. 問題：Client 只能載入一張固定的地圖，產品內容與驗收地圖綁在一起。
3. 邊界：Client 的內容由「一張地圖」變成「已安裝的地圖清單」（`arenas.json`）；身分比對由一個變成多個。wire 與 Match、Gateway 不變。
4. 影響：只有 `object_fps_pvp`（Client 內容與 `ClientConnection`、日誌）。
5. 依賴方向不變。
6. Ownership：地圖清單由產品內容擁有；用哪張地圖仍由 Match 決定。
7. 更小的變更不可行：直接替換 `pvp_arena.json` 會讓驗收與測試的幾何一起改變。
