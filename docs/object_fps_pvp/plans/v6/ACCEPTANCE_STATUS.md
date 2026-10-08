# PvP v6 驗收狀態與回報表

更新：2026-10-08。Owner：`object_fps_pvp`。**v6 已升格為穩定基線（[STABLE_BASELINE](STABLE_BASELINE.md)）**：14a 完成；LAN 聯機測試完成；14b 依使用者決定（D26）縮減為在最終來源上重跑一次短測，全部通過；GUI 三輪、兩組長測與時鐘漂移「跳過（使用者決定）」。
操作入口：[手動驗收指南](MANUAL_ACCEPTANCE.md)。契約：[v6 契約（pv6）](../../protocol-v6.zh-Hant.md)。
v5 的[驗收狀態](../v5/ACCEPTANCE_STATUS.md)與[穩定基線](../v5/STABLE_BASELINE.md)原樣保留，不繼承其完成勾選。

狀態只用：`通過`、`失敗`、`受干擾`、`缺資料`、`未執行`、`記錄（不判定）`。欄位未填不視為通過。一個平台通過不代表其他平台。Windows、Linux 與 macOS arm64 沒有實機（D11⑩），標「未執行」；CI 在四平台跑 L1 的建置與測試。

環境：macOS（Darwin 25.6.0）、Intel x86_64、SDL 視訊驅動 cocoa、Metal、同機三角色、SDL 注入輸入（L3 為人工）。
來源：`f41c22a`（master `5972451` 加文件），detached worktree 從零建置。證據都在 `build/target/_build/test/logs/pvp-v6-batch14a-20261007/`（git 忽略，只在本機）：事前宣告 `declare.txt`（SHA-256 `889313d1…`，使用者核准）、`artifacts.sha256`、`tools.sha256`（驗收工具 61 檔，與第 16 批的清單 `b8eabc7c…` 相同）。

主機狀態：每輪 probe 的計時器晚醒 P99；≥6 ms 算 8 ms 狀態，否則算 4 ms 狀態。

## 14a：短測與驗收交付

| 項目 | macOS | Windows | Linux | 指標與證據 |
|---|---|---|---|---|
| CTest 全標籤 | 通過 | 未執行 | 未執行 | 63／63（量測樹）；CI 另在四平台跑 L1 |
| 權威 digest 閘門（第 03 批） | 通過 | 未執行 | 未執行 | 同機兩樹比對（對第 16 批的 base `7886848`）35／35 相同；golden 不變；`authority/` |
| 25 案真網路玩法矩陣（逐案，D24） | 通過 | 未執行 | 未執行 | 判定的 24 案 24／24；9 案在 8 ms 狀態；`1-matrix/` |
| 　clean-30 | 記錄（不判定） | 未執行 | 未執行 | 這一輪通過：Actual 1672／1676（99.8%），P50／P95 22.0／28.1 ms，4 ms 狀態。30 FPS 的相位餘裕缺口仍是 D21 的設計範圍邊界（第 10、16 批曾失敗），由 v7 處理 |
| action short 30／60／144 FPS＋capture | 通過 | 未執行 | 未執行 | 4／4；`2-action/` |
| player short 30／60／144 FPS＋capture | 通過 | 未執行 | 未執行 | 4／4（144 FPS 這次有效）；同距相位跨幀率一致；`3-player/` |
| 雙 GUI 整合短測（移動＋戰鬥，16 秒） | 通過 | 未執行 | 未執行 | 可見 P50／P95 35.4／37.2 ms（20／20）、Actual P50／P95 29.9／37.3 ms、戰鬥通過、視窗 clean；`4-gui-short/` |
| 長測短模式（2 循環，60 Hz） | 通過 | 未執行 | 未執行 | 87／87 動作、每循環判定簽名 1、2 次死亡／重生、Actual P50／P95 34.6／39.1 ms；`5-soak-short/` |
| 4 人 quad clean-60 ×5 | 通過 | 未執行 | 未執行 | 5／5；Actual 100%、P50 26.3～30.6 ms、P95 32.3～35.6 ms；每輪第 5 人 `room_full`、12 次重生全部在空的出生點、沒有被拒絕的動作；全部在 8 ms 狀態、IPC 合併 0；`6-quad-clean-60/` |
| 4 人 quad clean-30 ×5 | 記錄（不判定） | 未執行 | 未執行 | 功能全部通過；Actual 100%、100%、96.5%、97.7%、100%（D21 邊界）；`7-quad-clean-30/` |
| 4 人 1 GUI＋3 bot | 通過 | 未執行 | 未執行 | GUI 60 FPS、全程 4 人、共 24 次死亡；`8-quad-gui-bots/` |
| 4 人 2 GUI＋2 bot：action60、player60、GUI timing short | 通過 | 未執行 | 未執行 | 3／3；GUI timing 可見 P50／P95 36.8／39.5 ms、視窗 clean；`9a-`、`9b-`、`9c-` |
| 版本混用（容量 2 對 4） | 通過 | 未執行 | 未執行 | 第 16 批 L2（`pvp-v6-batch16-l2-20261007/12-quad-mixed/`）：舊 Client、舊 Match、舊 Gateway、舊 arena 都明確失敗；之後產品程式未變 |
| Match 不連結 SDL／Renderer | 通過 | 未執行 | 未執行 | `otool -L` 只有系統庫；`match-links.txt` |
| 產品移除 | 通過 | 未執行 | 未執行 | scratch worktree 刪除 254 檔與 `projects.csv` 的登錄列後，建置與 CTest 35／35 通過；沒有空目錄；D12 連結 0。發現：根 README（三語）與 `docs/releasing.*` 以本產品為建置範例，刪除後說明會過時（只報告）；`removal/` |
| L3 原生操作清單 1～10 | 通過 | 未執行 | 未執行 | 2026-10-08 使用者：「全部正常」（`f41c22a` 的量測樹建置，2 Client＋2 bot）。觀察（使用者決定不判失敗、交給 v7）：切換 macOS 工作區（Spaces）去看其他畫面時仍在連續射擊，回來後看到斷線訊息；原因未查明，見 v7 README |
| 第 3 項（視窗拖動／縮放的呈現阻塞） | 通過 | 未執行 | 未執行 | macOS Intel／Metal 已驗證（L3 第 6 項）；Windows／Linux 未執行，根因可能不同（Windows 拖動標題列會進入 Win32 modal 迴圈）。按下縮放角到開始拖動之間的停頓依 D19 交給 v7 |

## LAN 聯機測試（第 17 批的 L3，Windows 實機）

2026-10-08，使用者與朋友，依 [LAN 聯機測試手冊](LAN_TEST.md)。證據：`build/target/_build/test/logs/pvp-v6-lan-20261008/`（git 忽略，只在本機；`artifacts.sha256`）。觀察與日誌缺口的分析見 [v7 任務清單](../v7/README.md)的「v6 LAN 聯機測試的觀察」。

- 構成：Mac（Intel）跑 Match＋Gateway，Match 從主工作目錄的 master `c7d6dd3` 建置（SHA-256 `c19c9733…`），Gateway 在本機 `go build`（`ce4eb45d…`）；Client 是 2 台 Windows，snapshot `object_fps_pvp-snapshot-20261007-c7d6dd3`（兩台 SHA-256 相同，`e6654cd6…`）。同一個區域網路，Wi-Fi。原計畫 3 台 Client，日誌只有 2 台。
- 正式對局：10:55:53～11:10:31（約 14.5 分鐘），2 人，發行地圖 `pvp_corners_v1`。

| 項目 | Client A（Windows，D3D12，60 FPS） | Client B（Windows，D3D12，165 FPS） | 指標與證據 |
|---|---|---|---|
| 版本與地圖 | 通過 | 通過 | 兩台都依 Match 自動選中 `pvp_corners_v1` |
| 連線與斷線 | 通過 | 通過 | 全程沒有斷線、逐出或 Gateway 限流；`CONNECTION POOR` 失敗窗口最多 2／3（A）、1／3（B） |
| 移動命令 | 通過 | 通過 | Match 收到實際命令的比例 99.46%／99.74%；Held 連續最多 8 Tick |
| Snapshot | 通過 | 通過 | 每秒 60；年齡 P50／P95 8.7／18.6 ms、8.1／19.2 ms |
| 戰鬥 | 通過 | 通過 | 動作 382／338，接受 373／338，拒絕 9／0（原因沒有記錄，見 v7 任務 8）；命中 76／76；擊殺 19／19，與 Match 的死亡紀錄一致 |
| 四角重生 | 通過 | 通過 | 38 次重生都在四角：(17,17) 11、(3,17) 11、(3,3) 10、(17,3) 6 |
| 切換視窗 | 通過 | 通過 | A 切換 2 次、B 切換 6 次，沒有斷線；A 有一次事件處理阻塞 267 ms，Match 有 3 Tick 收不到命令（v7 任務 1） |
| 三個角色的日誌 | 通過 | 通過 | 啟動 SHA-256、階段、地圖、每秒摘要、Match 事件、Gateway 事件與統計都有；缺口列在 v7 任務 8 |

- 開測前的失敗都是環境因素：Mac 的網段改變，Gateway 以新的 `-advertise-ip` 重啟；一台 Client 第一次啟動沒有加 `--gateway`，連到預設的 127.0.0.1 而逾時。

## 14b：最終來源上的短測（縮減範圍，D26）

第 17 批改了 Client 與 Match，14a 的指紋不再成立，所以在升格用的來源上重跑一次 14a 的短測。來源 `fee92ff`（master `c7d6dd3` 加文件），detached worktree 從零建置；事前宣告 `declare.txt`（SHA-256 `038b1de7…`，使用者核准）。證據：`build/target/_build/test/logs/pvp-v6-batch14b-20261008/`（git 忽略，只在本機）。數字見 [STABLE_BASELINE](STABLE_BASELINE.md)。

| 項目 | macOS | Windows | Linux | 指標與證據 |
|---|---|---|---|---|
| CTest 全標籤 | 通過 | 未執行 | 未執行 | 63／63 |
| 權威兩樹比對（對 `7886848`） | 通過 | 未執行 | 未執行 | 35／35 相同；`authority/` |
| Match 不連結 SDL／Renderer | 通過 | 未執行 | 未執行 | `match-links.txt` |
| 產品移除 | 通過 | 未執行 | 未執行 | 刪除 262 檔與登錄列後建置與 CTest 通過；D12 連結 0；`removal/` |
| 25 案矩陣（逐案） | 通過 | 未執行 | 未執行 | 判定的 24 案 24／24；23 案在 8 ms 狀態；`1-matrix/` |
| 　clean-30 | 記錄（不判定） | 未執行 | 未執行 | 這一輪失敗：Actual 93.4%、7 次停頓重設，8 ms 狀態（D21） |
| action short | 通過 | 未執行 | 未執行 | 4／4；`2-action/` |
| player short | 通過（player144 無效） | 未執行 | 未執行 | 3 案通過；player144 只到 120 FPS，D11③ 不計次；`3-player/` |
| 雙 GUI 整合短測 | 通過 | 未執行 | 未執行 | 可見 P50／P95 37.1／38.3 ms、視窗 clean；`4-gui-short/` |
| 長測短模式 | 通過 | 未執行 | 未執行 | 87／87 動作；`5-soak-short/` |
| 4 人 quad clean-60 ×5 | 通過 | 未執行 | 未執行 | 5／5、Actual 100%、IPC 合併 0；`6-quad-clean-60/` |
| 4 人 quad clean-30 ×5 | 記錄（不判定） | 未執行 | 未執行 | 功能全部通過；Actual 99.3%～100%；`7-quad-clean-30/` |
| 4 人 1 GUI＋3 bot | 通過 | 未執行 | 未執行 | GUI 60 FPS、全程 4 人；`8-quad-gui-bots/` |
| 4 人 2 GUI＋2 bot（三項） | 通過 | 未執行 | 未執行 | 3／3；`9a-`、`9b-`、`9c-` |

## 完整驗收（14b 原範圍）

| 項目 | macOS | Windows | Linux | 指標與證據 |
|---|---|---|---|---|
| GUI 三輪（各 120 秒、200 事件、戰鬥） | 跳過（使用者決定） | 未執行 | 未執行 | D26 |
| Headless 長測 60 Hz（113 循環／1808 秒） | 跳過（使用者決定） | 未執行 | 未執行 | D26 |
| Headless 長測 144 Hz（113 循環／1808 秒） | 跳過（使用者決定） | 未執行 | 未執行 | D26 |
| 4 人 1 GUI＋3 bot 一輪（D23⑤） | 跳過（使用者決定） | 未執行 | 未執行 | 短測的 1 GUI＋3 bot 已通過 |
| Client／Host 時鐘漂移（兩台實體機器） | 跳過（使用者決定） | 跳過（使用者決定） | 未執行 | LAN 測試是實機佐證，但沒有量測漂移（v7 任務 4） |

## 回報欄位

每個跑次回報以下欄位；請把本節複製到新檔填寫。

```text
跑次／日期：
平台（OS／架構／GPU／GPU 驅動／SDL 視訊驅動／顯示器更新率）：
同機或 LAN：
來源 HEAD 與工作樹狀態：
Match／Gateway／Client／probe SHA（artifacts.json 或 run-manifest.json）：
事前宣告（總輪數、補跑規則）：
完整輸出目錄：
已知干擾（休眠、切換視窗、其他大型工作）：
主機狀態（計時器晚醒 P99）：

GUI 每輪：狀態｜可見 P50／P95｜配對數／事件數｜移動送出 P95／Actual P50／P95／Actual 比例｜
  動作送出／裁決／接受｜死亡／重生｜裁決抵達 P95｜射擊回饋 P95｜視窗狀態｜非預期重設｜退出碼
長測每組：狀態｜循環數／秒數｜動作宣告／送達｜每循環判定簽名數｜死亡／重生｜
  移動 Actual P50／P95／比例｜漂移窗口全數通過？｜合法動作 Match／Client P95｜慢幀｜Gateway 限流｜退出碼
4 人每輪：狀態｜第 5 人結果｜動作／裁決｜拒絕類別｜死亡／重生（空出生點？）｜IPC 合併｜主機狀態
L3 清單：每項 通過／失敗／無法判定＋備註
```

## 升格條件（14b）

2026-10-08 使用者決定（D26）：跳過長測與完整 GUI 三輪，14b 只在最終來源上重跑一次短測，通過即升格。原本的條件保留如下，第 1 項依 D26 改為「短測全部通過」：

1. 14b 的 GUI 三輪、兩組長測與 4 人一輪在 macOS 全部 `通過`，依事前宣告執行；受干擾的輪依 fix/06 補跑規則處理，原輪保留。
2. 短測項目的指紋與完整驗收是同一個來源與產物；期間有程式變更時，受影響的項目重跑。
3. 未執行的平台與項目在本表明示，不寫成通過；基線範圍明寫「macOS Intel／Metal、同機」；clean-30 依 D21 列為設計範圍邊界。
