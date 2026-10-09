# 第 09 批：FireGate 常數的重推與 C2

狀態：**執行中：S1 準備與第 09a 批的設計**（2026-10-10；決定 1～11 照建議，D47：「決定 1～11 照建議，開始第 09a 批，檔位照建議」）。PR 線 P2（PR #73）。依賴第 08b 批。
檔位：規劃 ultracode（D45；使用者 2026-10-10「開始第 09 批，開 ultracode，檔位照建議」）。之後各步的檔位見「步驟」。

## 起因與範圍

- README「本機射擊閘的兩個常數」：`FireGateGuardSeconds`（v6：2 ms）與 `FireGatePendingSpreadTicks`（v6：7 Tick）是由 v6 的時序推導的，任務 1～3 改變了那些時序。做法：先量兩條通道再推導，更新合成時間線（`tests/object_fps_pvp/FireGateTests.cpp`），commit 並凍結；再以同一份事前宣告比較 v6 最終 tree 與 v7 tree（C2），加上 12 Tick 連點。
- HANDOFF 第 09 批：C2 之後跑 25 案回歸；P2 頭另跑一次 30 FPS，只記錄，單獨顯示任務 3 的影響。
- 輸入（使用者 2026-10-09、D46）：D44（是否要在常數推導前查清楚，或在什麼條件下要重推）；計時器鎖定；probe 幀相位（起點要不要隨機化）；已完成批次核對的更正。
- 停止條件（P2）：權威 digest 改變；需要改 wire；常數必須比 v6 大；乾淨跑次出現權威 Cooldown 拒絕。

## 閘門的語意（規劃時逐行核對，產品程式＝08b 的 `a954aa3`）

- 位置：`PvpApplication.cpp:461` 的 `fireGate.Allows(shotTiming)` 不通過就累加本機擋下（`:462`），點擊丟掉、不排隊；通過才 `SubmitAction`，再呼叫 `Submitted`（`:473`）。HUD 的冷卻剩餘用同一個判斷式（`:520`）。headless 的 action probe 不經過閘門（`gameplay_action.hpp:163` 直接 `SubmitAction`）。
- 判斷式（`FireGate.cpp:50-57`）：required＝max(snapshot 的 nextAllowed、已收到裁決的 resolvedTick＋10、未判定射擊的 pendingNextAllowed)；earliest ≥ required 才放行。
- earliest（`FireGate.cpp:11-26`）＝A＋(N−L)−2＋ceil((s−4 ms−guard−max(0, phaseShift))/step)，下限 A＋1。A＝最新已調和 snapshot 的 Tick，L＝它已解析的命令序號，N＝最新的預測命令，s＝距 N 的步邊界經過的時間。
- **guard 守下界（R ≥ earliest）**。前提（`FireGate.hpp:17-26`）：步邊界送出的命令比它執行的 Tick 早「2 步＋4 ms」到達，同一幀的動作走同一條路，不會更早。違反時，冷卻邊緣的點擊被放行、實際早 1 Tick 判定，變成權威 Cooldown 拒絕。guard 太大只會多擋邊緣點擊。
- **spread 守上界（R ≤ earliest＋7）**。`pendingNextAllowed＝earliest＋7＋cooldown`（`FireGate.cpp:59-69`），收到裁決或包含它的 snapshot 就清掉。違反時，下一發在裁決回來前依上界放行，被權威拒絕。太大只會在裁決未到時多擋。

## 常數的推導

**結論：`FireGateGuardSeconds`＝2 ms、`FireGatePendingSpreadTicks`＝7 Tick，兩者都不變。** 只在「命令在發布時立即送出（沒有被釘住）、沒有主機停頓、領先＝2」的範圍內成立；凍結前要先處理「需要決定的事」1、5，並做 M0 影子量測（決定 4）。

### guard

- 需求量（規劃推導，對抗式檢查更正）：點擊相位 s∈(4＋guard, 4＋guard＋step] 的射擊，閘門預測在 T_N−1 判定；它若在 T_N−2 的收件截止前到達，就會早 1 Tick。要求 guard ≥ max(y_shot)，y_shot＝C(T_N−2)−B_N−p_s−4 ms（B_N＝步邊界，p_s＝射擊的路徑）。
  - 評審原本用命令的 `host_accepted` 代替 B_N＋p_s，算出 y。但 v7 的命令由模擬角色在步邊界醒來時產生（`ClientSimulationRole.cpp:125`、`:178-185`），所以 y_shot＝y＋L_w＋w＋(p_c−p_s)：L_w 是角色晚醒，w 是 worker 的送出等待。
  - 射擊比命令快的部分：Client→relay 段由 relay 實測，08b after clean 612 發中射擊最多比命令快 0.115 ms；Gateway→Match 段同一個寫出迴圈先寫輸入、後寫動作（`runtime_link.go:208-232`），射擊不會更快（依程式推論）。
- 實測（08b L2 after clean：clean-30×12、clean-60×6；Client＝probe `d45a1f0b…`，Client 程式與 `a954aa3` 相同；只取被 actual 解析的移動命令，epoch 開始 2 秒後）：
  - y 的最大值 4 ms 狀態 0.448 ms、8 ms 狀態 0.652 ms（評審、三份提案、對抗式檢查都重現）；每跑次 P90 −0.20～+0.51 ms。
  - 加上 w 後最大 0.518／0.737 ms；L_w ≤0.208 ms；射擊快 ≤0.115 ms。實際需求的上界約 1.0 ms，對 2 ms 的餘裕約 1.0 ms。
  - 射擊重播：612 發 R−E 全部為 0。但臨界窗 s′∈(6, 6.65] ms 只有 14 發，檢定力低。
- 設計上的最壞值：死區 2.0＋P90 以上的尾巴 0.44＋w 0.33＋L_w 0.21＋射擊快 0.12 ≈ 3.1 ms，大於 2 ms。P90 會停在死區內任何位置（`LocalPlayerPrediction.cpp:143-147`），同一台機器上就移動約 0.7 ms；首次決定只用 8 個樣本、死區 0.5 ms（`Movement.hpp:53`、`:56`）。
- 尾巴的取樣不足：上面只有 1 個沒有停頓的 session。同一套 Match Tick 迴圈與同一個 P2 Client，在 3 個 session 的 54 次 clean 中，有 3 次 y >2 ms：2.218（約 2 ms 的主機小停頓）、6.294（被釘住加 17 ms 停頓）、11.787（28.6 ms 的 Tick 間隔）。match.log 的 `tick_late` 會漏掉這種 Tick 間隔（08-2 clean-60-r3-before 有 28.0 ms 的間隔，`tick_late_max` 只有 146 µs），偵測要用 resolved 時刻的 Tick 間隔。
- 處理（建議，決定 2）：維持 2 ms。殘餘類別寫明：P90 落在死區邊緣、2 ms 以上的主機小停頓、送出被釘住、領先跳升。v6 第 12 批也維持 2 ms，但那次 L2 只是一致性確認（dev_log 2026_10_07:69-70），所以 v6 是「接受殘餘、留待 v7 確認」，不是「殘餘已確認」。

### spread

- 上界：R−E ≤ ceil((g−y_shot＋D)/step)，D ≤ 2I＋δ＋ε_c（Client 的保持 I、link 的保持 I、link 寫出迴圈的晚醒 δ、Client 計時器的晚醒 ε_c）。在 y_shot ≥ −5 ms 時，只要 δ＋ε_c ≤ 約 43 ms，R−E ≤7 都成立，比評審的公式更有餘裕。
  - Client：新請求、ACK-only、重送共用 `nextActionSendAt`（`ClientConnection.cpp:418-420`）。
  - link：每位玩家「寫出完成＋I」（`action_delivery.go:170-196`、`:413-426`），而且只要有未決請求或 acknowledged>retired 就自己每 I 送一次，相位與 Client 無關，所以兩段保持疊加不能排除。這也是不採用 6 的理由。
  - δ：link 寫出迴圈由輸入、動作、結果喚醒（`runtime_link.go:238-251`），雙方玩家每秒 60 次的輸入會叫醒它（hypothesis）。結果通道的計時器鎖定是在 Gateway→Client 觀察到的，不能直接套用到 link。
- 實測（08b after clean）：送出前 33.3 ms 內有上行 Actions 的射擊 0／612（最小間隔 95.4 ms），保持路徑沒有被量到；submit→worker 收到裁決 P99 15.6、最大 16.1 ms，裁決在上界生效前就回來，所以 clean 中 spread 量不出差別。故障案例有 R−E＝8、9、15（gateway-250ms、上行故障），屬於非 clean。
- 結論：維持 7（決定 3，建議不必另外決定）。

### 兩條通道（README 要求先量）

- 下行結果通道 c：P50 0.36～0.37、P95 0.70 ms（第 08b 批 L2）。
- 上行動作通道：Match 端沒有記錄動作的收件時刻，只量到 Tick 解析度（k＝0）；Client→relay 段由 relay 實測（上面）。

## 常數涵蓋不了的失敗

1. **輸入送出被釘住**（決定 1）：
   - 新命令要等 1 個 token（`ClientConnection.cpp:604`），補充率 60／秒等於命令的產生率（`:597-599`，`Movement.hpp:22-28`）。一次停頓用掉容量之後，等待就一直維持（程式推論，三份提案獨立得到）。動作不經 token，D36 讓它立即送出（`:821-822`），於是「同路」前提失效，earliest 估晚 1 Tick。
   - 實證：08b after gateway-250ms r6 有 3 發 R−E＝−1（Tick 800 的 s′ 7.52 ms、890 是 8.49、1040 是 7.36；這 3 發的 w 是 8.1～10.9 ms），同一跑次玩家 1 被釘住 11 秒。這 3 發被接受，因為 headless probe 不經閘門、也不在冷卻邊緣。
   - 頻率（每位玩家每秒 first sent−generated 的中位數 >2 ms）：現行 worker 的 clean 2／54 跑次，持續 ≥3 秒的 1／54（08-2 after clean-30 r10：玩家 1 連續 8 秒、玩家 2 連續 3 秒，等待 11～16 ms）；08b after 故障 5／18。
   - **不是 v7 特有**：06 after gateway-250ms-r5 也有 1 發 s′ 8.265 ms 早 1 Tick（w 11.2 ms），v6final 有相同的 token 規則（`../GYO-Engine-v6final` 的 `ClientConnection.cpp:549-556`、`Movement.hpp:28`）；v6 是否也會被釘住未驗證。舊 worker 的等待中位數本來就約 0.7～1.0 ms，2 ms 門檻對它會誤判，只記錄。
   - 用 guard 涵蓋需要約 1 step，比 v6 大，觸發停止條件。被釘住期間輸入也晚 11～16 ms，會污染 C2 的移動指標。
2. **領先跳升**（D44 的推論機制）：佇列變 4 時，earliest 最多估晚 2 Tick（`FireGate.cpp:22` 固定減 2）。任何不觸發停止條件的常數都蓋不住。
3. **主機停頓與 Match Tick 停頓**：2 ms 以上的小停頓就足以讓 y 超過 guard；新 Match 的跑次中 9／162 有 >5 ms 的 Tick 停頓，同一跑次都有 Client 模擬角色晚醒 >5 ms（可能是主機層級的停頓，是否同一時刻未驗證）。08b after 0／36。屬殘餘。

## D44

- 結論：D44 **不需要**在推導或凍結前調查，維持第 16 批。理由：常數蓋不住領先跳升（是前提被打破，不是常數大小的誤差），先查 D44 不會得到不同的值；推導用的 08b L2 72 次遲到修正／領先跳升 0 次；唯一有完整 trace 的 D44 中，被重設的玩家沒有被釘住，兩者同源沒有證據（共同觸發只是 hypothesis）。
- 但相位追蹤的既有缺陷（HANDOFF「C1 之後要決定的事」：settling 期間累積的 late 樣本會在 settle 完成時立刻觸發第二次 late 修正）和 D44 推論鏈中的 late 修正屬於同一類（有關聯是 hypothesis），而且它屬於重推條件 1。若要在第 09 批處理，必須排在 M0 與凍結之前（決定 5）。
- 要把 D44 提前的條件（任一成立就停下問使用者）：M0、C2 或回歸的 clean 跑次出現領先跳升，而且同一跑次有權威 Cooldown 拒絕；決定 1 選了修正，而修正確認會改變遲到修正或 backlog 的行為；依 D44 原本的規則，開發跑次中的比例升高，或舊版也出現。
- 記錄：凍結的 `lead_jumps.py`（sha256 `14cd5b5c…`），新舊分開累計；被釘住的秒數與停頓並列記錄。

## probe 相位

- 建議（決定 6）：C2 的 gameplay 矩陣、25 案回歸、「P2 頭 30 FPS 只記錄」都不隨機化（headless probe 不經閘門；受相位影響的 b、a＋b 等本來就不跨建置解讀；改動 v6 的矩陣 probe 會偏離 C1 的量測來源）。
- 只有 M0（headless）在每個連射段的起點加固定 seed 的 U(0, 1 step) 偏移，讓 s 覆蓋臨界窗。錨定的格點讓 s 集中：08b after clean 的 s′ 落在 [0, 1.70]（63 發）與 [5.33, 17]（549 發），s 範圍 <4 ms 的「跑次×玩家」是 24／72。
- GUI 連點：GUI probe 已有 `--fps`（`gui_main.cpp:90`），`action_short.hpp:140` 關掉 vsync、`:615` 用 `SDL_DelayNS` 以相對時間控制節拍，幀相位會自然漂移，種子偏移沒有作用。v6final 也一樣。

## 合成時間線（凍結時一併改寫，`FireGateTests.cpp:174-283`）

1. 主機模型：拿掉舊 Match 的 Tick 晚醒 Bell(3,2)／(7.2,2.4)，改為「收件截止＝k·step＋U(0, 0.3 ms)」；命令路徑加 L_w（≤0.21 ms）與 w。另注入主機小停頓（2～5 ms）與 Match 停頓（28～73 ms）情境，只斷言「本機擋下與權威拒絕分開計數」，拒絕數記為已知殘餘。
2. 相位誤差：保留 U(±死區)（雙機漂移的最壞情況）；固定加入 e＝±2 ms 的邊緣案例加尾巴，預期 lead −1 的比例只記錄；另加單機變體 U(±0.5 ms)（首次決定的死區），斷言 minimumLead ≥0。
3. 命令與動作分開走：命令＝步邊界＋L_w＋w＋τ_c；動作＝點擊時刻＋τ_a；τ_a−τ_c∈[−0.12, 0] ms。新增 TokenLock 情境（w～U(0, step) 持續）：選修正時斷言 minimumLead ≥0；只記錄時以「已知缺陷」為名，釘住 minimumLead ≥−1。
4. LeadJump 情境（佇列 4）：釘住 minimumLead ≥−2 作為已知限制；D44 修正後再改成 ≥0。
5. 動作的保持：Worst＝Client 保持 U(0,I)（含 ACK-only 錨定）＋link 獨立保持 U(0,I)＋link 晚醒；斷言 maximumLead ≤7、拒絕 0。Measured＝08b after clean 的 a（P50 0.37、最大 1.68 ms）。另加上行延遲（故障）情境，不只下行。`:174-179` 引用 v6 第 10 批的註解改成 v7 的來源。
6. 裁決回程：拿掉 30 Hz 的 U(0, 1/30 s)（`:278`），改為事件驅動「判定 Tick＋U(0, 1 ms)＋單程＋下一幀的 Drain」；另加下行延遲 D∈{0, 100, 250 ms}。
7. 點擊情境：保留 10～13 Tick 與「12 Tick 不被吃掉」（`:319`、`:327-328`），門檻以新的節奏重算；新增「冷卻邊緣連點」（每一幀都點，由閘門決定）。12 Tick 情境對 1 Tick 的下界錯誤沒有檢定力（evidence 提案的模型推論）。
8. 釘住凍結值：`CHECK(FireGateGuardSeconds==0.002)` 與 `CHECK(MovementPhaseDeadbandSeconds==0.002)`（字面值；`guard==死區` 是恆等式，擋不住沒有重推的死區變更），spread 的字面值 7 維持。突變（batch `v7-09`）：guard−0.5 ms、spread 6、spread 10、guard＋2 ms，各由對應的情境殺掉；既有 `12-no-guard`、`12-no-pending-spread` 保留並確認沒有 stale。
9. `FireGate.hpp:17-33` 的註解：公式改為 v7 的各項，補上「同路」前提的例外（送出被釘住、領先跳升、主機與 Match 停頓），指向本文件。
10. 依 D45，模型的每個分布都寫明來源建置與指標（08b L2 after 加上 M0），不向其他建置借用。

## 重推條件（凍結之後，任一成立就重推兩個常數，重跑 M0 型的量測、時間線與突變）

1. 相位追蹤改變（S2b 本身在凍結之前，不算重推）：`MovementPhaseTarget`／`Deadband`／`FirstDeadband`／`Percentile`／`WindowSamples`／`LateSamples`／`MaximumCorrection`、`InitialCommandLead`、樣本年齡的定義（`Movement.hpp:36`、`:45-59`；`LocalPlayerPrediction.cpp:97-98`、`:120-151`、`:316-321`）。HANDOFF「C1 之後要決定的事」的兩項若處理，也算在這裡。
2. 輸入送出規則改變：`InputSendRate`、`InputSendBurst`、token bucket（`ClientConnection.cpp:593-628`、`:672-682`），包含決定 1 的修正。
3. 動作通道改變：`ActionSendRate`、Client 共用期限、D36、`CoarseWakeLead`；link 的節拍、獨立重送與計時器；snapshot 轉送頻率、Match IPC 或 Tick 排程、結果路徑。
4. 第 11 批 pv7：心跳對時或時間回聲若共用輸入 token、`nextActionSendAt` 或 link 的 `nextSend`，或改變 link 寫出迴圈的喚醒來源。只是新增獨立通道時記錄確認即可。
5. D44 的調查或修正改到 1～3 的任一項，或證實送出被釘住與 D44 同源。
6. 第 15 批 Windows／LAN：在 ≥15 分鐘的場次量 P90 範圍、窗口外的尾巴與射擊比命令快的量。尾巴 >0.5 ms、P90 偏離死區中心 >1.5 ms，或 Client 段射擊比命令快 >0.3 ms，就重推 guard；Client worker 加 Gateway 計時器的晚醒 P99.9 >約 43 ms 就重推 spread。Windows 的計時器解析度會改變被釘住的頻率。
7. 第 13 批 FPS 上限：低於 30 FPS 時，「裁決在 pending 上界生效前回來」的論證要重新確認。
8. `cooldownTicks`、`AuthorityTickRate`、`SnapshotIntervalTicks` 改變。
9. 第 10 批／TT-2 的日誌寫出進入熱路徑，使射擊路徑的 P99 增加 >1 ms。
10. 任何 clean 量測（M0、C2、回歸、第 16 批）出現：窗口外的 R<earliest；R>earliest＋7；權威 Cooldown 拒絕（這同時是停止條件）。

## 步驟（決定確認後執行）

- **S1**（只在決定 1 選 A 或 B 時）：先跑「P2 頭 30 FPS，只記錄」（P1b 頭 `805bd11` 對 P2 頭 `a954aa3`，clean-30×6 交錯）。必須在任何產品變更之前。`../GYO-Engine-p1b-c1` 已移除，P1b 要從零重建，能否重現 C1 的雜湊未驗證。宣告 high＋1 次 xhigh 對抗式檢查；執行 medium。
- **S2 = 第 09a 批**（只在選 A 或 B 時）：送出被釘住的處理。high；計時、期限與 token 的局部 xhigh。
  - A：修正 token bucket 停頓後回不來的問題（產品內的 `ClientConnection`）。恢復步數 N 在實作前由補充率推出並寫進宣告（例如故障結束後 ≤3 步），不由實作自己的單元測試訂出。先核對 Gateway 的 session 封包率上限（每秒 120，`session.go:117-125`），回歸中記錄 `rate_limited_packets`。
  - B：`ShotTiming` 帶入最新命令的送出等待並加進 margin（worker→模擬角色→主執行緒的資料流，小的 Architecture Delta）。
  - 驗收：CTest 全過、權威 digest 不變、突變 killed、開發跑次記錄被釘住的秒數。
- **S2b = 第 09b 批（暫稱）：相位追蹤的既有缺陷**（D47⑤ 的更正）：settling 期間收進來的舊樣本留在下一個視窗（修正後第一個視窗的 P90 實際約 P93）、settling 期間累積的 late 樣本在 settle 完成時立刻觸發第二次 late 修正，以及延遲的位移（v7 的「產生→執行」約 38 ms，v6 是 21～25 ms）。這會改相位常數或相位追蹤的定義，屬於重推條件 1，所以排在 M0 與凍結之前。可能與 D44 的 late 修正有關（hypothesis）；若修正確認會改變遲到修正或 backlog 的行為，依 D44 的提前條件停下問使用者。規劃與檔位在開始時決定。
- **S3 = M0 影子量測**（常數變更前、S2 與 S2b 之後的頭）。宣告 high＋1 次 xhigh 對抗式檢查；執行 medium。
  - probe-only 修補：headless probe 在送出時記錄 `presented.shotTiming` 與閘門算出的 earliest（`gameplay_action.hpp:142` 的 `PresentAt` 回傳 `ClientPresented.shotTiming`，可行），寫進另一個檔案，避免凍結分析器拒絕；每段起點加 seed 偏移。
  - 跑次：clean-30／60／144 與 gateway-250ms、host-ipc-250ms、downstream-250ms 各 ≥6 次，兩種主機狀態都要有（約 30～40 分鐘）。
  - 窗口的事先定義：停頓＝resolved 時刻的 Tick 間隔 ≥ step＋1.5 ms，窗口涵蓋其後 0～3 Tick；被釘住＝每秒送出等待中位數 >2 ms 的玩家秒；領先跳升＝`lead_jumps.py`。
  - 門檻（宣告時寫明根據）：窗口外 clean 射擊 R<earliest＝0 發；窗口外 yw＋L_w 的最大值 ≤ guard−0.5 ms；clean 的 R−earliest ≤7；臨界窗 s∈(6, 8] ms 至少約 100 發，不足就標「未驗證」；落在前一次上行 Actions 之後 I 以內的射擊另列最少發數。
- **S4 凍結**：`FireGate.hpp` 的註解（值不變）、`FireGateTests` 依上節改寫、v7-09 突變、本文件的推導節。high；時間線模型與確定性案例的局部 xhigh；1 次對抗式檢查。驗收：全量 CTest 與權威 digest、突變全部 killed；commit 並推送，CI 綠燈。
- **S5 SDL 注入連點模式**：`action_short.hpp`／`run_action_short.py` 加 rapid 模式，用 SDL 事件注入按下與放開，走 `PvpApplication` 的正常路徑（經過真正的 FireGate）；v6 的 probe-only 修補放在從 `fee92ff` 另開的 worktree（例如 `../GYO-Engine-v6c2`），產品執行檔要與 v6final 逐位元組一致，`git diff --stat` 只能出現 `build/acceptance`。high。
- **S6 C2 宣告**：high 撰寫＋xhigh 對抗式檢查，使用者核准。
- **S7 C2 執行**，加 25 案回歸、`backpressure_probe.py`、`run_network.py`。medium。
- **S8 收尾**：文件、HANDOFF、README、PR #73。medium。

## C2 的計畫（宣告在 S6 定稿；以下是要點與已知的修正）

- **建置**：before 矩陣沿用 `../GYO-Engine-v6final`（`fee92ff`，產品程式＝`c7d6dd3`＝tag `object_fps_pvp-v1.1.0`），開跑前重算產物雜湊，必須與 C1 一致；before 連點用 S5 的 worktree；after＝凍結後的頭，在 detached worktree 從零建置，Gateway 要 `vcs.modified=false`。開跑前兩棵 tree 的權威 digest 都要通過。量測期間暫停 PR #73 的 auto-fix；有任何非文件的變更，該 session 作廢。
- **A：30 FPS 對比矩陣**（D31）：clean-30、clean-60 每棵 tree ≥6 輪交錯，clean-144 每棵 3 輪對照；補跑規則照 C1。
  - 主機狀態：**宣告為偏離 C1**。C1 的主分類（`05-30fps-comparison.md:40`）是共用抽取器的 `host_late_p99`（Match snapshot 的晚醒），但新 Match 的晚醒不反映主機狀態（08b after 兩種狀態下 `tick_late` 都 ≤446 µs），所以改以 probe TimerBaseline 分層；TimerBaseline 只量連線前 3 秒，會漏掉跑次中的轉換，所以每輪前後各量 5 秒 sleeper 判定「轉換」（只列出、不進入分層判定），before 的舊 Match 另用自己的晚醒交叉確認。兩棵 tree 的 8 ms 狀態頻率做雙側 Fisher 檢定，p<0.05 就停下。（`late_p99_ms` 與 `interval_p99_ms` 是同一次 3 秒量測的兩個欄位，兩者的一致率不能當作分類正確的根據。）
  - 判定：C1 的規則**逐條重新宣告**哪些沿用、哪些改寫。C1 的替代比率排除 Match 停頓，前提是兩棵 tree 的 Match 相同（`05-30fps-comparison.md:48`），C2 不成立，所以 Match 停頓的替代以「排除」與「不排除」兩種方式報告。
  - 指標：通過率；Held＋Neutral 的筆數與時段；Actual 比率；停頓重設；移動延遲 P50／P95 只記錄；v7 另記被釘住的秒數、D44 領先跳升、停頓。v7 的預期：clean-30 與 clean-60 收斂。
  - 分析器：事先列出 sha256（v6 最終清單中 C1 的凍結子集；分執行緒後語意改變的規則只以分析器 v7 判定；共用抽取器 `analyze.py` `68ee027a…`；新增後凍結的連點 judge、送出被釘住偵測、`lead_jumps.py`）。
- **B：連點**（決定 7）：(i) 12 Tick 連點（判定）；(ii) 冷卻邊緣連點，每一幀都點 5 秒（after 判定，before 只記錄）；(iii) 10／11／13 Tick（只記錄）；(iv) Gateway 暫停 250 ms 後 6 秒的連點，誘發送出被釘住（非 clean，只記錄）。射牆、不擊殺；每段 11 發後換彈。每棵 tree、30／60／144 FPS、每情境 ≥40 次點擊、≥3 輪，交錯。
  - GUI 取不到 earliest（`PvpApplication` 只在內部持有 `presented.shotTiming`，公開的觀察沒有 phaseShift 與 phaseDecided）；要取得就得改產品，違反 v6 逐位元組一致的前提。所以 C2 逐筆記錄點擊時刻、本機擋下、送出的 ID、前一發是否已有裁決、裁決；earliest 只在 M0 量。
  - 判定（沿用 v6 第 12 批）：after 在 clean 的 (i)(ii) 中權威 Cooldown 拒絕＝0；**明文排除** action short 既有的刻意注入的那 1 次拒絕（v6 dev_log:69）。窗口內的拒絕仍然停下，但定位程序事先宣告。本機擋下只記錄；L1 的參考值引用凍結後的模型版本（commit）。
- **25 案回歸**：同一個凍結頭，結果不進入 C2 的判定。relay 的「結果任一秒 ≤31」由 judge 套用；Gateway 的兩個最短間隔以字面值比較 ≥33.3，每跑次至少 1 個有值的視窗（第 08b 批判定 2、5）。`backpressure_probe.py` 6 案與 `run_network.py` 各 1 次；非 respawn 的重設照 08b 的 D44 規則。只記錄：D44 領先跳升、被釘住秒數、停頓、c 的分布。
- **「P2 頭 30 FPS 只記錄」**：P1b 頭 `805bd11`（從零重建）對 P2 頭 `a954aa3`，clean-30 各 6 輪交錯，依狀態分層。決定 1 選 A 或 B 時必須在修正之前跑（S1）；選 C 時併入 M0 的 session。

## 本批另加的停止條件

1. M0 的任何一條門檻不成立：推導不成立，停下。
2. 產物雜湊不符：before 與 C1 不一致，或連點 worktree 的產品執行檔與 v6final 不同。
3. 凍結分析器在結構上拒絕 trace，或環境錯誤：保留，不重跑。
4. 兩棵 tree 的主機狀態頻率 Fisher p<0.05。
5. D44 須提前的條件成立。
6. 量測 session 期間出現任何非文件的變更：該 session 作廢。
7. 執行途中範圍擴大（例如修正 token bucket 牽動相位追蹤或 backlog）：依換檔規則停下，重新規劃。

## 需要決定的事（2026-10-10 使用者決定：全部照建議，D47）

1. **（必須先決定，碰到停止條件）輸入送出被釘住**
   - (A)【建議】另開第 09a 批，修正 Client token bucket 停頓後回不來的問題（產品內的 `ClientConnection`），之後在新的頭上做 M0。理由：這是根本原因；同時拿掉被釘住期間多出的 11～16 ms 輸入延遲，那段延遲也會污染 C2 的移動指標；閘門介面不必改。代價：移動的送出時序會改變，y 要重量；這條規則 v6 也有，所以 C2 的 after 會同時含有這個修正，C2 的 before 只能記錄。
   - (B) FireGate 帶入「最新命令的送出等待」並加進 margin。不動移動語意，但要改 worker→模擬角色→主執行緒的資料流（小的 Architecture Delta）。
   - (C) 不改，只記錄。不建議：在這個狀態下「clean 的權威拒絕＝0」沒有保證，C2 的冷卻邊緣連點可能觸發停止條件。
2. **guard 的值**：(A)【建議】維持 2 ms，殘餘類別寫明並列為重推條件。(B) 2.5 ms：比 v6 大，需要你核准；設計上的最壞值約 3.1 ms，2.5 也蓋不住。
3. **spread**：維持 7（建議，不必另外決定）。
4. **M0 影子量測**：凍結前在常數變更前的頭做（建議要做；README 允許）。門檻與窗口照「步驟」S3。
5. **（2026-10-10 使用者更正：在 P2 處理，排在 09a 之後、M0 與凍結之前，暫稱第 09b 批）相位追蹤的既有缺陷**（HANDOFF「C1 之後要決定的事」：settling 期間的舊樣本與 late 樣本、延遲位移約 38 對 21～25 ms）：要不要在第 09 批處理。屬重推條件 1，若處理必須排在 M0 與凍結之前，否則凍結後馬上要重推；可能與 D44 的 late 修正有關（hypothesis）。建議：本批不處理，維持「另立批次」，並在重推條件寫明；但若你希望先處理，就排在 09a 之後、M0 之前。
6. **probe 相位**：(A)【建議】矩陣、回歸、30 FPS 記錄都不隨機化；只有 M0 的每段起點用 seed 偏移；GUI 連點本來就會漂移。(B) 矩陣也隨機化：會失去與 C1、06～08b 的可比性。
7. **C2 連點的內容**：(A)【建議】(i) 12 Tick（判定）、(ii) 冷卻邊緣連點（after 判定）、(iii)(iv) 只記錄；earliest 只在 M0 量。(B) 只做 12 Tick（與 v6 第 12 批相同）：對 1 Tick 的下界錯誤沒有檢定力，不建議。
8. **C2 的主機狀態**：宣告為偏離 C1，以 TimerBaseline 分層，前後 sleeper 判定轉換，舊 Match 用自己的晚醒交叉確認（建議）。
9. **v6 的 tree**：矩陣沿用 `../GYO-Engine-v6final` 並以雜湊核對；連點從 `fee92ff` 另開 worktree 套用 probe-only 修補（建議）。
10. **D44** 維持第 16 批（本規劃的結論），累計時加上被釘住的秒數。
11. **檔位**：主力 high；S2、S4 的局部，以及 S1、S3、S6 的對抗式檢查用 xhigh。

## 規劃的過程與證據

- workflow（ultracode）：方案 3 位（high；證據推導、時間線模型、C2 與風險）、評審 1 位（high）、對抗式檢查 1 位（xhigh）。三份方案與評審的常數都是 2 ms／7，並各自獨立發現「送出被釘住」。
- 對抗式檢查的結論：「結論部分成立，論證有誤」。已併入本草案的修正：
  - guard 的需求量漏了模擬角色晚醒 L_w 與 worker 送出等待 w：實際需求上界約 1.0 ms、餘裕約 1.0 ms（不是 1.35 ms），設計最壞值約 3.1 ms（不是 2.4～2.9 ms）；δpath 由 relay 拆成兩段，Client 段實測射擊最多快 0.115 ms。
  - 尾巴只取自單一 session；同一程式 54 次 clean 中 3 次 y >2 ms；停頓的偵測改用 resolved 時刻的 Tick 間隔；M0 的門檻改為窗口外的 yw＋L_w，並事先定義窗口。
  - 「送出被釘住」不是 v7 特有（06 after 有違反，v6 有相同的 token 規則）。
  - 凍結前要決定相位追蹤的既有缺陷（決定 5）。
  - C2：主機狀態分類的出處是 C1:40（不是 :41），131／132 的一致率是同一次量測兩個欄位的恆等式，所以宣告為偏離 C1；C1 的判定逐條重新宣告；GUI 已有 `--fps` 但取不到 earliest；刻意注入的 Cooldown 拒絕要明文排除。
  - `CHECK(guard==死區)` 是恆等式，改為釘住字面值；spread 的 δ 根據改為 link 寫出迴圈的喚醒來源；M0「修正後」的恢復步數改為實作前推出（避免循環論證）。
  - 事實更正：`../GYO-Engine-p1b-c1` 已移除；3 發 R−E＝−1 的 s′ 與 Tick 的對應順序；24／72（不是 22／72）；新 Match 的 Tick 停頓 9／162（12／162 重現不了）；v6 第 12 批的 L2 只是一致性確認。
- 證據：`build/target/_build/test/logs/pvp-v7-09-plan-20261010/`（`evidence/`、`model/`、`c2risk/`、`judge/`、`adversarial/`，各有 `commands.txt` 與 `sha256.txt`）。
