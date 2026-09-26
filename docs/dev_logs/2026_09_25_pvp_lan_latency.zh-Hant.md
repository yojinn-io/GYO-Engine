# 2026-09-25 — PvP 拖窗失去回應、連線保活與跨視窗延遲

## 使用者回報與調查範圍

上一版的本機 WASD 與滑鼠視角已明顯順暢。使用者同屏開啟兩個 Client 後回報：

1. 拖曳視窗**標題列**以調整位置時，整個桌面與兩個 Client 暫時失去回應；Player1 掉線後恢復。
2. Player1 自己移動時，Player2 視窗中的 Player1 有明顯滯後。

使用者選擇以本機／LAN 低延遲為優先，接受丟包時偶爾小幅校正，並要求記錄原因。
本次不把「自動測試通過」當成整個桌面卡頓已被證明解決。

## 已確認原因

### 1. 傳輸連線存活依賴畫面執行緒

原本 Client worker 只在握手階段送 Hello，Playing 後依賴主執行緒產生 PlayerInput。
Gateway 的 session 期限是 5 秒。當畫面／視窗操作卡住、不再產生輸入時，即使
network worker 仍正常收 Snapshot，Gateway 也會因為收不到 Client 封包而移除玩家。
這是「主執行緒停頓導致掉線」的具體程式漏洞。

修正：Playing 後由 network worker 每秒重送既有、已驗證的 Hello，Gateway 回覆
Welcome。它只維持 Session，不提交移動命令、不刷新 Match 的 15 個缺包 Tick
移動期限，也不新增協定版本。真正失去網路／整個程序停止時仍使用原有超時。
Client failure 與 Gateway session expiry 現在記錄玩家 ID 和原因，不輸出 token。

### 2. 兩段緩衝疊加，造成可見的跨視窗滯後

前次計畫選擇六步中立命令提前量（約 100 ms），加上遠端 100 ms 插值。
六步不只是啟動延遲：兩端開始後都每 Tick 前進一個命令，因此正常情況下差距會維持。
本機鏡頭則在預測狀態間插值、約落後預測尖端一個 Tick。

在理想時序下，另一個視窗相對本機畫面的滯後約為 180–200 ms，再受收包與呈現
排程影響。20 Hz Snapshot 會影響輸入起步的取樣相位，但不能再把固定 50 ms
重複加到穩定狀態的 100 ms 插值緩衝上。

依使用者選擇，命令提前量改為 **三步（50 ms）**，遠端插值改為 **50 ms**。
Authority 60 Hz、Snapshot 20 Hz、批次 30 Hz、未確認上限 12、未來命令上限 32
均不變。不加入遠端外推。代價是遺失快照／抖動超出緩衝時，遠端可能停住再校正；
首批命令遺失也可能讓實際提前量增加，未引入時鐘同步或自動縮減所有積欠。

### 3. 重新同步後重複計算舊時間，讓提前量膨脹

第一次新配置的實際雙 GUI 量測仍得到 **150.788 ms** 中位滯後，超過 150 ms
驗收門檻，因此未降低門檻來掩蓋問題。

`gui/create-movement.csv` 給出具體重現：

| 畫面時間 | frame 間隔 | 權威已完成 | 本機最新命令 | 未確認命令 |
|---|---:|---:|---:|---:|
| 0.442 s | 16.2 ms | 20 | 26 | 6 |
| 0.551 s | 108.5 ms | 26 | 34 | 8 |

這次間隔來自驗收 probe 的場景 readback／儲存；正式遊戲沒有這個 capture 行為。
它仍然重現了一般畫面停頓後的預測器錯誤：

1. 新快照的 ACK 26 已追上本機 tip 26，`Reconcile` 以這個新權威狀態重新同步。
2. `SeedLead` 正確建立三個中立命令 27–29。
3. 同一幀 `Advance(108.5 ms)` 又把**已被新權威狀態涵蓋的舊間隔**計算為五個追趕步驟，增加命令 30–34。
4. 新增的五步會變成持續的額外提前量，而非只影響當下的一幀。

修正限制 fresh seed／reseed 後首次 Advance 只接受最多一個新的固定步驟，以便立即
採樣當前操作，同時避免重演已經由新權威狀態涵蓋的時間。一般 reconciliation 與
正常固定步 catch-up 不變。回歸測試以 108.5／250 ms 等間隔檢查不再產生五步爆量。

### 4. 拖窗時滑鼠鎖定的生命週期缺口

之前「Playing 且 focused」就自動啟用 relative mouse，沒有獨立的自由游標狀態。
拖窗與首次加入交錯時，事件處理仍可能看到舊 Connecting 狀態；隨後 RefreshState
進入遊戲，又會自動要求 relative mouse，缺少避免在視窗互動中重新捕捉的保障。

現在進場保持自由游標，只能透過本視窗、已聚焦、內容範圍內的左鍵點擊或 Tab
啟用捕捉；Tab 可釋放。MOVED／RESIZED／FOCUS_LOST／MINIMIZED 事件立即釋放，
同批事件的後續點擊不能重新捕捉；僅重新聚焦也不捕捉。釋放時，即使 W 還按著，
新移動命令也會中立。已送出的不可變命令仍按原契約完成。

這項修正移除程式可控制的滑鼠鎖定風險，但不等於已證明使用者描述的整個桌面卡頓
只由滑鼠鎖定造成。需要後續在同一桌面環境實際拖曳確認。

## 尚未證實的整機卡頓原因

原試玩日誌只記錄啟動，沒有 frame 耗時或 session expiry 原因，無法倒推完整事件鏈。
核心在 **19:02:18**（本次雙 Client 啟動約 11 秒後）記錄四組 Machine Check／
Unified Memory Controller 錯誤，原文標示 `Corrected error, no action required.`。
這是同時段的系統層證據，**不是記憶體故障或卡頓因果的診斷**。
未發現同時段的 GPU reset／OOM 記錄；當下後續查詢約有 21 GB 可用記憶體、swap 未用，
但它不能代表卡住瞬間的狀態。未修改硬體、核心、驅動或全域顯示設定。

讀碼顯示首次世界 frame 會同步建立兩個新的 GPU pipeline 變體；新增 HUD 字串也會
首次 rasterize／upload。這些是待量測的耗時來源，不能直接命名為根因。第一輪實際
量測：兩個 GUI 圖形初始化約 833／837 ms；首次世界 frame 約 15 ms，正常繪製最大
約 65 ms，未重現數秒的全桌面停頓。未為了猜測而加入 Engine 預熱框架。

產品目前記錄：圖形初始化、首次世界 frame、超過 250 ms 的事件／Update／Render、
滑鼠模式切換時間、frame 間隔與連線失敗。最小化略過呈現時等待 10 ms，避免沒有
VSync 等待時主迴圈空轉。日誌足以讓下一次拖窗停頓有可追蹤的階段資訊。

## 驗證與證據

- 初次量測及失敗門檻：`build/target/_build/test/logs/pvp-lan-followup/`。
- 最終完整驗收：`build/target/_build/test/logs/pvp-lan-final/result.json`，`passed=true`。
- 系統原文：`build/target/_build/test/logs/pvp-entry-kernel.log`。
- C++ 完整建置與 27 個 CTest 通過；其中 PvP 29 個案例、56,705 個 assertions 通過。產品 Go race 通過。
- 網路 probe 新增主執行緒停 6 秒但 worker 正常：Session 存活、玩家按原規則停止、權威 Tick 繼續、恢復後重新同步並能再次移動。
- GUI probe 驗證視窗移動釋放鼠標、仍按 W 時停止新預測、重新聚焦不捕捉、Tab 切換及 ESC 清理。
- `*-presentation.csv` 記錄主機 monotonic 時間與實際提交繪製的位置；`presentation-latency.json` 比較相同位移門檻。
- 此延遲是同主機 post-Render 的位置量測，不是螢幕 scanout／input-to-photon，也不是實體雙機 LAN 的時鐘同步量測。

### 最終雙 GUI 結果

| 項目 | 結果 |
|---|---:|
| 跨視窗位移門檻 | 0.25／0.5／0.75／1.0／1.25 單位，5 組皆有效 |
| 延遲中位數／平均／最大 | 101.183／89.402／101.602 ms |
| 穩定移動幀／有實際畫面位移 | 32／32 |
| 無新 Snapshot 仍有畫面位移 | 21 幀 |
| 最大顯示校正 | 0.11841 世界單位 |
| 全程最大未確認命令數 | 6（上限 12） |
| 112.4 ms 停頓後重新同步 | ACK 27 → tip 31，未確認 4 |
| 107.5 ms 停頓後重新同步 | ACK 306 → tip 310，未確認 4 |
| 首次世界 frame（兩視窗） | 17.27／18.77 ms |
| 正常 Render 最大耗時（兩視窗，排除 capture） | 64.45／64.49 ms |
| 圖形初始化（兩視窗） | 1,030.5／980.9 ms |

150 ms 驗收門檻保持不變。這是一輪有固定腳本、含場景 capture 的同機量測，
不是所有負載與輸入模式的延遲保證。舊六步／100 ms 版本沒有同格式的跨視窗 trace，
因此不把理論減少的約 100 ms 宣稱為舊版到新版的實測差值。

最終真實 socket 測試：基線有 80 份共同權威快照一致。RTT 0／20／40 ms 各組加入
單向最多 10 ms 抖動、5% 隨機丟包、首批與連續兩批輸入遺失，三組皆通過。
每組 301 個輸入封包中丟棄 21 個（隨機 15、首批 2、連續批次 4），476 個 Snapshot
中丟棄 25 個；最大 UDP 封包 144 bytes。也通過 Gateway 替換、Match 失聯後兩個
Client 返回 Lobby、離開重入與非法 IPC 檢查。GPU smoke 1／1、shader 3／3 通過。

### 重跑方式與人工確認

於 repository 根目錄執行：

```bash
cmake --build --preset test --parallel 6
ctest --preset test --parallel 4 --output-junit logs/pvp-lan-final-tests.xml
python3 build/acceptance/object_fps_pvp/run_network.py \
  --match build/target/object_fps_pvp/bin/gyo_object_fps_pvp-match \
  --gateway build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway \
  --probe build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_network_probe \
  --arena build/target/object_fps_pvp/bin/assets/object_fps_pvp/pvp_arena.json \
  --gui-probe build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_gui_probe \
  --arena-root build/target/object_fps_pvp/bin/assets/object_fps_pvp \
  --network-impairments --output build/target/_build/test/logs/pvp-lan-final
```

Gateway 需先於 `apps/object_fps_pvp` 執行 `go build -o ../../build/target/_services/object_fps_pvp/bin/gyo_object_fps_pvp-gateway ./gateway/cmd`。
網路／GUI 驗收需要本機 sockets 與桌面 GPU。輸出路徑為可再生、git 忽略的本機證據；
移植時應重新執行，不能僅依賴這些路徑存在。

人工試玩：第一個視窗 Create + Join，第二個 Refresh → Join Room。進場游標保持
自由；點擊遊戲內容後 WASD／滑鼠操作，Tab 釋放游標，再拖曳標題列。分別確認
平移、轉向、同時操作及拖窗後重新點擊恢復。自動驗收使用合成 SDL 視窗事件，
**不能取代真實桌面 compositor／標題列拖曳的人工確認**；此項仍待使用者試玩。
若再發生整機停頓，保留該次 Client／Gateway 日誌和時間，與核心記錄對照。

## 變更邊界

所有修正、觀測與驗收仍由 object_fps_pvp 所有。沒有改 Engine、公共 Go Gateway、
Arena Data Contract、Authority／Snapshot 頻率或 v2 wire schema。三步提前量與
50 ms 遠端插值是使用者確認後的產品策略調整，不是公共預測框架或責任移轉。

後續 Esc 重入的身分標示、Lobby 名額快取及 HTTP Leave 清理問題，另記錄於
[重入生命週期日誌](2026_09_25_pvp_rejoin_lifecycle.zh-Hant.md)。
