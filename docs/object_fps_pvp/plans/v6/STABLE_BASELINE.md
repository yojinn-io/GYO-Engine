# PvP v6 穩定基線

升格日期：2026-10-08。Owner：`object_fps_pvp`。
依使用者決定（D26）以**縮減範圍**完成第 14b 批後，**v6 升格為穩定基線**。使用者的理由：「核心問題已經暴露出來了，在核心問題沒有解決之前，再漂亮的測試數據都是沒有多大意義的。」核心問題是 2026-10-08 LAN 聯機測試看到的單執行緒主迴圈與以畫面幀為節拍的時序，由 v7 處理。
範圍為 **macOS 26.7.1、Intel x86_64、SDL 視訊驅動 cocoa、Metal、120 Hz 顯示器上限、同機三角色**，加上 LAN 聯機測試（Windows Client 2 台，D3D12）作為真實網路的佐證。
這是已驗證範圍的開發基線。GUI 長時間運行、1808 秒長測、兩台機器的時鐘漂移、Linux 與 macOS arm64 實機都沒有驗證（見「跳過與未執行」）。
v5 的[穩定基線](../v5/STABLE_BASELINE.md)原樣保留。

## 已知問題（2026-10-10 追加）

- **Gateway 悄悄丟掉已解析的命令，Match 的遲到量測被截斷。** 本基線（master `c7d6dd3`、量測來源 `fee92ff`、tag `object_fps_pvp-v1.1.0`（`9a6fa8e`））同時帶著 Gateway 的過濾（`391ca00`）與 Match 的遲到量測（`a4ccaa5`）：Gateway 用權威狀態的延遲副本 `lastResolved` 丟掉序號不大於它的命令，Match 因此幾乎量不到 Client 端的網路遲到，Client 的 late 修正在截斷的樣本上校準。
- 權威狀態與裁決沒有錯（被過濾的都是 Match 不會執行的命令）；損失的是資訊。本基線的驗收數字都是在這個盲區下量到的。
- 經過、影響與處置見 [v7 事故紀錄](../v7/Incident/2026-10-10-gateway-silent-drop.md)。修正在 v7 的 P2（丟棄回報計畫，D53）。要不要重新發佈本基線，修好之後再決定。

## 固定契約與版本

Client／Gateway／Match 共同使用 pv6，詳細政策以 [v6 契約](../../protocol-v6.zh-Hant.md)為準。v6 相對 v5 的重點：

- wire（第 09 批，唯一的 wire 變更）：受擊時點欄位、arena 內容 digest 比對、GYOP 標頭的編解碼收進 Engine。
- 權威判定（第 10 批，唯一的權威變更）：Collision 的 float／double 統一；權威 golden 凍結，之後各批兩樹比對都是 35／35 相同。
- 房間上限 4 人（第 16 批）：出生點 2～64、Match 只主持出生點數不少於上限的 arena。
- 呈現與手感：本機射擊冷卻閘（第 12 批）、遠端上半身俯仰瞄準（第 11 批）、受擊反應與方向指示（第 13 批）。
- 發行地圖 `pvp_corners_v1`（四角出生），Client 依 Match 宣布的 id 與版本選擇已安裝的地圖；三個角色的日誌（第 17 批，D25）。測試與驗收用訓練地圖 `pvp_training_v1`。

## 來源與產物

| 項目 | 值 |
|---|---|
| master | `c7d6dd3`（PR #65）；量測來源 `fee92ff` ＝ `c7d6dd3` 加文件，產品程式相同 |
| 建置 | detached worktree 從零建置（test preset）；相依套件原始碼取自主工作目錄已下載的同版本（`deps-sources.txt`，第一次建置卡在網路下載） |
| Client SHA-256 | `950096c3bb756b2de4071d5c088c819c1e9ab894cc38ef863b79ea88ed3907a9` |
| Match SHA-256 | `7e1feeac89c48a04a48fd8a6a3eeed3ca1d8c53c2890780d6dfa27907167e715` |
| Gateway SHA-256 | `3766a1506e457562db80160bde15fdc91c08016bec81842a9389586dec1a8fc6` |
| 訓練地圖 `pvp_arena.json` | `0026013c731a7d7317ce6be4f84f897b91fa1b807c9621d60fb608570f1775a5` |
| probe、發行地圖、`arenas.json`、`asset_catalog.json` | 完整值見證據目錄的 `artifacts.sha256`（清單本身 `07fdbacc…`） |
| 驗收工具 | 61 檔，清單 `tools.sha256`（`0d2ffcfb…`）；和 14a 只差 `worker_main.cpp`（第 17 批的多地圖自測，短測不使用）；Python 分析器與第 07a 批相同 |

## 驗收結果

第 14b 批（縮減範圍）。事前宣告 `declare.txt`（SHA-256 `038b1de7…`），使用者核准。主機狀態依每輪 probe 的計時器晚醒 P99（≥6 ms 算 8 ms 狀態）。

| 項目 | 結果 | 指標 |
|---|---|---|
| CTest 全標籤 | 通過 | 63／63 |
| 權威兩樹比對（對第 16 批的 base `7886848`） | 通過 | 35 個情境 0 不同 |
| Match 不連結 SDL／Renderer | 通過 | `otool -L` 只有系統庫 |
| 產品移除 | 通過 | 刪除 262 檔與登錄列後建置與 CTest 通過；D12 連結 0；殘留提及與 14a 相同（程式與登錄 12、文件 265，都是已知的說明性提及） |
| 25 案真網路矩陣（逐案） | 通過 | 判定的 24 案 24／24；23 案在 8 ms 狀態 |
| 　clean-30 | 記錄（不判定） | 這一輪失敗：Actual 1549／1658（93.4%）、7 次停頓重設，8 ms 狀態（D21 的設計範圍邊界，v7 處理） |
| action short 30／60／144 FPS＋capture | 通過 | 4／4 |
| player short 30／60／144 FPS＋capture | 3 案通過，player144 無效 | player144 只到 120 FPS（顯示器上限），依 D11③ 不計次 |
| 雙 GUI 整合短測（移動＋戰鬥，16 秒） | 通過 | 可見 P50／P95 37.1／38.3 ms；Actual P50／P95 29.0／36.8 ms、100%；視窗 clean |
| 長測短模式（2 循環，60 Hz） | 通過 | 87／87 動作、每循環判定簽名 1、2 次死亡與重生、Actual P50／P95 25.8／28.6 ms |
| 4 人 quad clean-60 ×5 | 通過 | 5／5；Actual 100%、P50 31.5～36.2 ms、P95 36.6～39.0 ms；每輪第 5 人 `room_full`、12 次死亡與重生；沒有拒絕；IPC 合併 0；2 輪 4 ms、3 輪 8 ms 狀態 |
| 4 人 quad clean-30 ×5 | 記錄（不判定） | 功能全部通過；Actual 99.8%、99.4%、99.3%、100%、100% |
| 4 人 1 GUI＋3 bot | 通過 | GUI 60.0 FPS、全程 4 人、共 24 次死亡與重生、Actual P50／P95 25.5／29.1 ms |
| 4 人 2 GUI＋2 bot：action60、player60、GUI timing short | 通過 | 3／3；GUI timing 可見 P50／P95 36.7／38.7 ms、Actual P50／P95 25.6／35.9 ms、視窗 clean |
| LAN 聯機測試（第 17 批，Windows Client 2 台） | 通過 | 約 14.5 分鐘無斷線；Match 收到實際命令 99.46%／99.74%；擊殺 19／19 與 Match 一致；重生全在四角。詳見[驗收狀態](ACCEPTANCE_STATUS.md) |
| L3 原生操作清單 1～10（第 14a 批） | 通過 | 使用者；第 17 批之後沒有重做 L3，LAN 測試由使用者實際遊玩 |

## 跳過與未執行

使用者決定跳過（D26，標「跳過（使用者決定）」，不算通過）：

- GUI 三輪（各 120 秒、200 事件、戰鬥）。
- Headless 長測 60 Hz 與 144 Hz（各 113 循環／1808 秒）。
- 完整驗收的 4 人一輪（短測的 1 GUI＋3 bot 已執行）。
- Client／Host 時鐘漂移（兩台實體機器）。LAN 測試是實機佐證，但沒有量測漂移本身（各機器的時鐘沒有對齊，v7 任務 4）。

沒有實機而未執行：Linux、macOS arm64（D11⑩）。Windows 只有 LAN 測試的 Client 實機，沒有跑驗收工具。CI 在四平台跑 L1。

## 已知限制（交給 v7 與 v8）

- 單執行緒主迴圈：畫面或事件阻塞時命令停止產生（LAN 測試的切換視窗、v6 L3 切換工作區後斷線）。v7 任務 1。
- 命令跟著畫面幀產生：30 FPS 的相位餘裕不足（D21）；clean-30 在 v6 不判定。v7 任務 2。
- 網路路徑的短休眠輪詢與主機晚醒：8 ms 狀態下延遲 P95 較高。v7 任務 3 與 Time 子系統。
- 日誌缺口：拒絕原因、Match 結束紀錄、每位玩家的 Held 統計等。v7 任務 8。
- 遠端角色外觀相同、受擊方向只指向最後一位攻擊者、建房時選地圖。v8。

## 驗收過程中保留的紀錄

- 第一次建置在 configure 階段卡在相依套件下載 37 分鐘（`build-attempt1-network.log`），第二次在 configure 階段因使用者暫停而中止（`build-attempt2-paused.log`）；第三次完成。三次都在任何量測之前。
- 原本的宣告（含 GUI 三輪與兩組長測，`declare-superseded-ded713d7.txt`）在執行前由使用者縮減，保留。
- 矩陣 clean-30 失敗與 player144 無效，都依宣告只記錄。

## 證據索引

證據都在 `build/target/_build/test/logs/`（git 忽略，只在本機）：

- `pvp-v6-batch14b-20261008/`：宣告（含被取代的一份）、`artifacts.sha256`、`tools.sha256`、`deps-sources.txt`、建置與 CTest、`authority/`、`removal/`、`match-links.txt`、S1～S9 的原始資料（`1-matrix/` 到 `9c-bots-gui/`）、`progress.txt`。
- `pvp-v6-lan-20261008/`：LAN 聯機測試三個角色的日誌與 trace（`artifacts.sha256`）。
- `pvp-v6-batch14a-20261007/`：第 14a 批（第 17 批之前的來源），含 L3 前的短測。
- `pvp-v6-30fps-reference-20261007/`：v7 對比用的 30 FPS 失敗案例基準包。
