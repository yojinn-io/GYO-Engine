# 第 09 批：FireGate 常數的重推與 C2

狀態：**完成（2026-10-14）**：S1、S2（09a）、S2b（09b）、S4-lite（`6587030`）、S5（`793c4f5`）、S6（宣告 D79）、S7（C2 **通過**）、S8；S3（M0）移到 v7.1（D75），C2 沒有觸發補做。先前的狀態：**S1、S2（09a）、S2b（09b）完成；S3（M0）移到 v7.1（D75），只在 C2 觸發時補做；S4 改成 S4-lite（縮小版凍結），範圍已照建議核准、實作進行中**（2026-10-14，D75）。先前的狀態：S1 完成（只記錄）；第 09a 批的實作等 D48 的證據（2026-10-10；決定 1～11 照建議，D47：「決定 1～11 照建議，開始第 09a 批，檔位照建議」）。PR 線 P2（PR #73）。依賴第 08b 批。
檔位：規劃 ultracode（D45；使用者 2026-10-10「開始第 09 批，開 ultracode，檔位照建議」）。之後各步的檔位見「步驟」。

## 起因與範圍

- README「本機射擊閘的兩個常數」：`FireGateGuardSeconds`（v6：2 ms）與 `FireGatePendingSpreadTicks`（v6：7 Tick）是由 v6 的時序推導的，任務 1～3 改變了那些時序。做法：先量兩條通道再推導，更新合成時間線（`tests/object_fps_pvp/FireGateTests.cpp`（v7.1 已退役：D90 採 A1，`a1-shot-step` 以 `ShotGate` 取代，D137）），commit 並凍結；再以同一份事前宣告比較 v6 最終 tree 與 v7 tree（C2），加上 12 Tick 連點。
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

更正／補充（2026-10-14，D75）：M0 移到 v7.1，v7 不做 M0（只在 C2 的 clean 案例出現權威 Cooldown 拒絕時補做）。v7 的凍結是 S4-lite（縮小版凍結）：值仍是 2 ms／7，依據是本節（08b L2 after）與 09-M0-plan 在 `b205615` 上的離線推估（clean 的 ys 最大約 0.81 ms，〔推論／離線〕），沒有 M0 的實測確認。

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

### 縮小版凍結（D75，S4-lite）

2026-10-14 執行（範圍照 D75 ①～⑤，使用者核准）。證據：`build/target/_build/test/logs/pvp-v7-09-s4lite-20261014/`。

- **值**：`FireGateGuardSeconds`＝2 ms（定義上等於 `MovementPhaseDeadbandSeconds`）、`FireGatePendingSpreadTicks`＝7 Tick，和 v6 相同，不變。
- **依據**：
  - 本節的推導（08b L2 after clean）：guard 的實際需求上界約 1.0 ms，對 2 ms 的餘裕約 1.0 ms；spread 在 y_shot ≥ −5 ms 時、δ＋ε_c ≤ 約 43 ms 時成立；43 ms 是界線，不是餘裕（2＋5＋66.7＋43＝7 step）。clean 中保持路徑 0／612，δ、ε_c 沒有量到，δ 的喚醒來源是 hypothesis（上面 spread 節）。
  - 09-M0-plan 的離線推估〔離線推估，不是 M0 的實測〕：`b205615`（09ab-L2 after 的建置）13 次 clean、20170 個命令，max(ys) 的 p99 分箱口徑約 0.81 ms、逐命令口徑 0.651 ms（`pvp-v7-09-m0-plan-20261014/revise/lw_percmd.txt`、`derivation/preestimate_b205615.txt`）。和 08b 的結論一致，沒有變大。
  - 沒有的：M0 的實測確認（臨界窗射擊的 R−earliest、窗口外的 yw）。所以這是「值不變、適用範圍與殘餘寫明」的凍結，不是完整重推。
- **程式**（只改註解與測試，產品行為不變）：
  - `FireGate.hpp`：guard 與 spread 的推導改用 v7 的各項（命令由模擬角色在步邊界醒來後交給 worker 送出；Gateway 先寫輸入、後寫動作；spread 的上界 ceil((guard＋晚到＋保持)/step)）；寫明 guard 本身的殘餘：P90 落在死區邊緣（設計最壞值約 3.1 ms，上面 guard 節）；列出不大於 v6 值的常數都蓋不住的「同路／領先＝2」例外（延遲已消失後的 late slew、撤回後殘餘、延遲下降、D44 領先跳升、主機與 Match 停頓，長的 Match 停頓可能超過 2 Tick；送出被釘住已由 09a 處理）；spread 的「晚到 ≤5 ms」標明是假設、沒有量；指向本文件與 v7.1 的候選 A1、B1、B6。
  - `FireGateTests.cpp`：新增「The fire gate constants keep their frozen values」，以字面值釘住 `FireGateGuardSeconds == 0.002`、`MovementPhaseDeadbandSeconds == 0.002`、`FireGatePendingSpreadTicks == 7`。先前 spread 只由「holds a pending shot」的算術（102＋7＋10）間接釘住，guard 只由「follows the newest command less the lead」的 6.0／6.1 ms 邊界間接釘住；`guard==死區` 是恆等式，擋不住沒有重推的死區變更，所以死區也用字面值釘住。
  - 同檔的最小來源更新：`Pacing` 註解原本引用 v6 第 10 批 L2，改成 08b L2 after clean 的量（612 發中 0 發在前一個間隔內有上行 Actions；a（probe 送出→relay 第一次上行，只有 Client 段；Gateway→Match 段沒有量）P50 0.37、最大 1.68 ms；`Measured` 模型的參數仍是 v6 的）；主機模型的「batch 07」標明是 v6 第 07 批，並註明 v7 的改寫移到 v7.1。
- **突變**：新增 batch `v7-09` 的 5 個突變（`FireGate.hpp`）。4 個值突變（guard −0.5 ms、guard ＋2 ms、spread 6、spread 10）的預期訊息是上面的字面值釘住測試，但它們同時也被既有測試殺掉（guard −0.5 ms 與 ＋2 ms：「follows the newest command less the lead」的 6.0／6.1 ms 邊界；spread 6／10：「holds a pending shot」的 102＋7＋10），所以證明不了釘住測試多擋了什麼。第 5 個 `v7-09-guard-plus-50us`（guard＋0.05 ms，落在 6.0／6.1 ms 邊界的 0.1 ms 間隔之內）只有字面值釘住測試會殺掉。連同既有的 `12-no-guard`、`12-no-pending-spread`（確認沒有 stale：搜尋文字各找到 1 次）共 7 個，`run_mutations.py --only … --allow-dirty` 的結果 **7／7 killed**（`fix/mutations.log`、`fix/mutations.json`）。另做臨時確認：套用 `v7-09-guard-plus-50us` 時，`gyo_object_fps_pvp_tests` 除了字面值釘住測試以外的 248 個 test case 全部通過，只有釘住測試失敗（`mutations-fix.sh`）。突變後重建，執行檔和突變前只差 LC_UUID（第一次執行，`after-mutations/`）。
- **權威 digest**：只改註解與測試。修改前（`4dcbbcf`）與修改後在同一個 build 目錄（`build/target/_build/test`）建置，object_fps_pvp 的 7 個 probe（含 GUI probe，連結 `PvpApplication` 與 `FireGate`）、digest runner 2 個、其他測試執行檔 4 個與 `gyo_object_fps_pvp_tests`，共 14 個以 `strip -S -x` 後逐位元組比對：前 13 個的差異全部在 LC_UUID（0～16 bytes，macOS 連結器的 debug map 時間戳），LC_UUID 以外 0 bytes；第 14 個 `gyo_object_fps_pvp_tests`（本批改的測試）不同。test preset 不建遊戲本體，所以以同一套原始碼的 probe 代替。審查修正後（`fix/`）再比一次時，同一個 checkout 有第 S5 批並行修改 `build/acceptance/object_fps_pvp/`，`gyo_object_fps_pvp_gui_probe` 因此不同；其餘 12 個（含 digest runner 2 個）仍只差 LC_UUID。`object_fps_pvp.authority_digest` 通過；`ctest --preset test` 69／69、`object_fps_pvp.*` 37／37 通過。
- **「常數涵蓋不了的失敗」第 2 項**：依 D74 U3／U8 加了補充（原文保留），見該節。
- **沒有做的（移到 v7.1，[v7.1](../v7.1/README.md)「從 v7 移入的項目」第 10 項）**：M0 影子量測；「合成時間線」1～10 項的改寫（主機模型、命令與動作分開走、DelayDrop／LeadJump／TokenLock 情境等）；v7-09 突變由對應情境殺掉（現在由字面值釘住測試與既有測試殺掉）。
- **M0 的條件式補做**：C2 的 clean 案例（12 Tick 連點、冷卻邊緣連點、30 FPS 矩陣）出現權威 Cooldown 拒絕時，停下回報，再補 M0（用 M0 宣告草案 v2 與 M0 probe，先完成中止的 xhigh 審查；見步驟 S3 的更正）。「重推條件」第 10 項同時成立。

## 常數涵蓋不了的失敗

1. **輸入送出被釘住**（決定 1）：
   - 新命令要等 1 個 token（`ClientConnection.cpp:604`），補充率 60／秒等於命令的產生率（`:597-599`，`Movement.hpp:22-28`）。一次停頓用掉容量之後，等待就一直維持（程式推論，三份提案獨立得到）。動作不經 token，D36 讓它立即送出（`:821-822`），於是「同路」前提失效，earliest 估晚 1 Tick。
   - 實證：08b after gateway-250ms r6 有 3 發 R−E＝−1（Tick 800 的 s′ 7.52 ms、890 是 8.49、1040 是 7.36；這 3 發的 w 是 8.1～10.9 ms），同一跑次玩家 1 被釘住 11 秒。這 3 發被接受，因為 headless probe 不經閘門、也不在冷卻邊緣。
   - 頻率（每位玩家每秒 first sent−generated 的中位數 >2 ms）：現行 worker 的 clean 2／54 跑次，持續 ≥3 秒的 1／54（08-2 after clean-30 r10：玩家 1 連續 8 秒、玩家 2 連續 3 秒，等待 11～16 ms）；08b after 故障 5／18。
   - **不是 v7 特有**：06 after gateway-250ms-r5 也有 1 發 s′ 8.265 ms 早 1 Tick（w 11.2 ms），v6final 有相同的 token 規則（`../GYO-Engine-v6final` 的 `ClientConnection.cpp:549-556`、`Movement.hpp:28`）；v6 是否也會被釘住未驗證。舊 worker 的等待中位數本來就約 0.7～1.0 ms，2 ms 門檻對它會誤判，只記錄。
   - 用 guard 涵蓋需要約 1 step，比 v6 大，觸發停止條件。被釘住期間輸入也晚 11～16 ms，會污染 C2 的移動指標。
2. **領先跳升**（D44 的推論機制）：佇列變 4 時，earliest 最多估晚 2 Tick（`FireGate.cpp:22` 固定減 2）。任何不觸發停止條件的常數都蓋不住。
   - 補充（2026-10-14，D75 S4-lite；依 D74 U3／U8）：本項的範圍比「佇列變 4」大。改寫為：**Match 端的領先超過 2 step＋4 ms＋guard 時，earliest 可能估晚 1～2 Tick**，冷卻邊緣的點擊被閘門放行、再被權威以 Cooldown 拒絕。觸發包括：
     - **late slew 與撤回後殘餘**（D74 U3）：
       - late slew：觸發 late 修正的延遲在 slew 期間已消失時，已往前修的部分讓命令與射擊早到，最多約一次修正量（≤2 step）；閘門忽略負的 phaseShift（`FireGate.cpp:18-19`），earliest 可能估晚 1～2 Tick〔推論／離線：grid100 late_slew 命令 ys 中位數約 24 ms、最大約 34 ms，`pvp-v7-09-m0-plan-20261014/derivation/preestimate_b205615.txt`；M0 預測 Pm-slew〕。延遲仍在時方向安全，只會多擋。
       - 撤回後殘餘：從撤回開始（含撤回的 slew）到下一次視窗決定（約 4 秒），相位偏早約 3.3 ms，超過 guard 的 2 ms（[09b](09b-phase-tracking.md) 1-7 節第 2 點），earliest 可能估晚 1 Tick。
       - 兩者記為已知殘餘；修法候選是 v7.1 的 B1（重新擷取的樣本邊界 S／M；S＝只收 slew 結束之後產生的命令）。
     - **延遲下降**（Wdrop，D74 U8）：相位在延遲較大的期間決定；單程延遲下降 Δ 之後，命令比閘門假設的早 Δ 到達，直到下一次視窗決定（240 個樣本；約 4 秒是修正剛清空視窗之後的上界，穩定狀態下約 24 個樣本、約 0.4 秒就會反應）。期間 m∈(0, Δ−guard] 的射擊 earliest 晚 1 Tick。不需要 late 修正，`lead_jumps.py` 偵測不到（它量 gen→exec），network20 的那一種領先不到 1 Tick（`queued` 仍是 2）。〔已驗證，`b205615` 09ab-L2 after〕network20／40 共 12 跑次，12／12 都有 Wdrop；Wdrop 內命令的 ys 中位數在 network20 是 5.4～9.3 ms、network40 是 14.7～17.7 ms。每跑次 Wdrop 內只有 1 發射擊（玩家 A 在 4.20 s 那一發，是 gameplay 計畫刻意的換彈中射擊，權威以 Reloading 拒絕，不是 Cooldown）。〔推論：重播的 E，headless probe 不記錄 E〕這 12 發中有 4 發 R−E＝−1，12 發都符合 m 帶的預測。Δ−guard 在 network20 約 3.4～7.3 ms、network40 約 12.7～15.7 ms（`pvp-v7-09-m0-plan-20261014/revise/wdrop.txt`、`revise/lead_windows.txt`、`adversarial/net_window.txt`）。要蓋住需要 guard ≥ Δ（5～18 ms；ys 最大 17.71 ms），比 v6 大，觸發停止條件，所以 guard 的適用範圍寫成「決定之後單程延遲沒有下降超過約 guard」。代價〔推論：(Δ−guard)/step〕：每次延遲下降後至多約 4 秒內，冷卻邊緣的點擊在 network20 約 20～45%、network40 約 75～95% 會被權威拒絕；實際網路的頻率未知。記為已知殘餘；對應 v7.1 的 B6（B6 記的是問題本身：`FireGate.cpp` 的 earliest 寫死減 2，領先改變時估晚；修法在 v7.1 決定）。
     - **D44 領先跳升**：原文（佇列變 4，最多估晚 2 Tick）。
     - **送出被釘住的解除**：已由第 09a 批處理（第 1 項）。
     - 主機與 Match 停頓見第 3 項。
   - C2 的 clean 預期不受影響〔推論：同一建置 13 次 clean，在 acq 與已知停頓以外 0 段，`revise/lead_windows.txt`；clean 的 late 修正與延遲下降都是 0〕；C2 的 network 與回歸案例本來就只記錄。v7.1 的 A1（射擊併入輸入命令）若採用，這一類連同兩個常數一起消失。
3. **主機停頓與 Match Tick 停頓**：2 ms 以上的小停頓就足以讓 y 超過 guard；新 Match 的跑次中 9／162 有 >5 ms 的 Tick 停頓，同一跑次都有 Client 模擬角色晚醒 >5 ms（可能是主機層級的停頓，是否同一時刻未驗證）。08b after 0／36。屬殘餘。

## D44

- 結論：D44 **不需要**在推導或凍結前調查，維持第 16 批。理由：常數蓋不住領先跳升（是前提被打破，不是常數大小的誤差），先查 D44 不會得到不同的值；推導用的 08b L2 72 次遲到修正／領先跳升 0 次；唯一有完整 trace 的 D44 中，被重設的玩家沒有被釘住，兩者同源沒有證據（共同觸發只是 hypothesis）。
- 但相位追蹤的既有缺陷（HANDOFF「C1 之後要決定的事」：settling 期間累積的 late 樣本會在 settle 完成時立刻觸發第二次 late 修正）和 D44 推論鏈中的 late 修正屬於同一類（有關聯是 hypothesis），而且它屬於重推條件 1。若要在第 09 批處理，必須排在 M0 與凍結之前（決定 5）。
- 要把 D44 提前的條件（任一成立就停下問使用者）：M0、C2 或回歸的 clean 跑次出現領先跳升，而且同一跑次有權威 Cooldown 拒絕；決定 1 選了修正，而修正確認會改變遲到修正或 backlog 的行為；依 D44 原本的規則，開發跑次中的比例升高，或舊版也出現。（補充，2026-10-14，D75：M0 移到 v7.1，v7 只剩 C2 與回歸；補做 M0 時本條照樣適用）
- 記錄：凍結的 `lead_jumps.py`（sha256 `14cd5b5c…`），新舊分開累計；被釘住的秒數與停頓並列記錄。

## probe 相位

- 建議（決定 6）：C2 的 gameplay 矩陣、25 案回歸、「P2 頭 30 FPS 只記錄」都不隨機化（headless probe 不經閘門；受相位影響的 b、a＋b 等本來就不跨建置解讀；改動 v6 的矩陣 probe 會偏離 C1 的量測來源）。
- 只有 M0（headless）在每個連射段的起點加固定 seed 的 U(0, 1 step) 偏移，讓 s 覆蓋臨界窗。錨定的格點讓 s 集中：08b after clean 的 s′ 落在 [0, 1.70]（63 發）與 [5.33, 17]（549 發），s 範圍 <4 ms 的「跑次×玩家」是 24／72。
  - 更正／補充（2026-10-14，D75）：09-M0-plan 發現每段起點偏移在 30／60 FPS 無效，草案 v2 改為每發忙等控制 s（D74 U4）。M0 移到 v7.1，本項只在補做 M0 時適用。
- GUI 連點：GUI probe 已有 `--fps`（`gui_main.cpp:90`），`action_short.hpp:140` 關掉 vsync、`:615` 用 `SDL_DelayNS` 以相對時間控制節拍，幀相位會自然漂移，種子偏移沒有作用。v6final 也一樣。

## 合成時間線（凍結時一併改寫，`FireGateTests.cpp:174-283`）

更正／補充（2026-10-14，D75）：下面 1～10 項的完整改寫移到 v7.1（[v7.1](../v7.1/README.md)「從 v7 移入的項目」第 10 項）。v7 的 S4-lite 只做第 8 項的字面值釘住與突變確認，以及必要的最小更新（例如 `:174-179` 對 v6 第 10 批的引用改成 v7 來源）；第 9 項的註解照 S4-lite 的範圍寫。

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

補充（2026-10-14，D75）：下列條件在 v7（S4-lite 之後）仍然有效；但完整的重推（M0 型量測與合成時間線 1～10 項的改寫）在 v7.1 做。v7 期間若有條件成立，先停下回報，再決定在 v7 補做 M0 或併入 v7.1。

1. 相位追蹤改變（S2b 本身在凍結之前，不算重推）：`MovementPhaseTarget`／`Deadband`／`FirstDeadband`／`Percentile`／`WindowSamples`／`LateSamples`／`MaximumCorrection`、`InitialCommandLead`、樣本年齡的定義（`Movement.hpp:36`、`:45-59`；`LocalPlayerPrediction.cpp:97-98`、`:120-151`、`:316-321`）。HANDOFF「C1 之後要決定的事」的兩項若處理，也算在這裡。
2. 輸入送出規則改變：`InputSendRate`、`InputSendBurst`、token bucket（`ClientConnection.cpp:593-628`、`:672-682`），包含決定 1 的修正。
3. 動作通道改變：`ActionSendRate`、Client 共用期限、D36、`CoarseWakeLead`；link 的節拍、獨立重送與計時器；snapshot 轉送頻率、Match IPC 或 Tick 排程、結果路徑。
4. 第 11 批 pv7：心跳對時或時間回聲若共用輸入 token、`nextActionSendAt` 或 link 的 `nextSend`，或改變 link 寫出迴圈的喚醒來源。只是新增獨立通道時記錄確認即可。補充（2026-10-14，D73）：第 11 批與 pv7 移到 v7.1，本條在 v7.1 觸發（v7 不改 wire，v7 期間不會成立）。
5. D44 的調查或修正改到 1～3 的任一項，或證實送出被釘住與 D44 同源。
6. 第 15 批 Windows／LAN：在 ≥15 分鐘的場次量 P90 範圍、窗口外的尾巴與射擊比命令快的量。尾巴 >0.5 ms、P90 偏離死區中心 >1.5 ms，或 Client 段射擊比命令快 >0.3 ms，就重推 guard；Client worker 加 Gateway 計時器的晚醒 P99.9 >約 43 ms 就重推 spread。Windows 的計時器解析度會改變被釘住的頻率。 補充（2026-10-14，D76）：第 15 批移到 v7.1，本條在 v7.1 的 LAN 場次觸發。
7. 第 13 批 FPS 上限：低於 30 FPS 時，「裁決在 pending 上界生效前回來」的論證要重新確認。 補充（2026-10-14，D76）：第 13 批移到 v8，本條在 v8 觸發。
8. `cooldownTicks`、`AuthorityTickRate`、`SnapshotIntervalTicks` 改變。
9. 第 10 批／TT-2 的日誌寫出進入熱路徑，使射擊路徑的 P99 增加 >1 ms。 補充（2026-10-14，D76）：第 10 批與 TT-2 移到 v7.1，本條在 v7.1 觸發。
10. 任何 clean 量測（M0、C2、回歸、第 16 批）出現：窗口外的 R<earliest；R>earliest＋7；權威 Cooldown 拒絕（這同時是停止條件）。
11. D49 第二階段（丟棄回報給 Match）實作後，重跑 M0，並用 D48 第③步的方式加跑 Client→Gateway 短停頓的注入（2026-10-10 使用者追加）。理由：現在 Client→Gateway 的遲到被 Gateway 過濾（`gateway/server.go:420-423`），幾乎不產生 late 修正；丟棄回報讓 Match 看見這些遲到之後，late 修正與 W_post 的分布都可能改變，D48 第③步的結論也只在現在的過濾下成立。補充（2026-10-14，D73）：D49 第二階段（`GatewayDropReport`）移到 v7.1，本條在 v7.1 觸發；08c 已提前的部分（已解析命令照常轉送）在凍結之前發生，不觸發本條。

## 步驟（決定確認後執行）

- **S1**（只在決定 1 選 A 或 B 時）：先跑「P2 頭 30 FPS，只記錄」（P1b 頭 `805bd11` 對 P2 頭 `a954aa3`，clean-30×6 交錯）。必須在任何產品變更之前。`../GYO-Engine-p1b-c1` 已移除，P1b 要從零重建，能否重現 C1 的雜湊未驗證。宣告 high＋1 次 xhigh 對抗式檢查；執行 medium。
- **S2 = 第 09a 批**（只在選 A 或 B 時）：送出被釘住的處理。high；計時、期限與 token 的局部 xhigh。
  - A：修正 token bucket 停頓後回不來的問題（產品內的 `ClientConnection`）。恢復步數 N 在實作前由補充率推出並寫進宣告（例如故障結束後 ≤3 步），不由實作自己的單元測試訂出。先核對 Gateway 的 session 封包率上限（每秒 120，`session.go:117-125`），回歸中記錄 `rate_limited_packets`。
  - B：`ShotTiming` 帶入最新命令的送出等待並加進 margin（worker→模擬角色→主執行緒的資料流，小的 Architecture Delta）。
  - 驗收：CTest 全過、權威 digest 不變、突變 killed、開發跑次記錄被釘住的秒數。
  - （補充，2026-10-14，09ab-doc）：已完成，採 A 的比例折讓修正版（`InputSendDrainRate`＝80，D66、D67），見 [09a](09a-input-send-pinning.md)「09a-impl」。和 09b 合併做一次 L2（09ab-L2）：被釘住的玩家·秒在 17 個案例全部 0。
- **S2b = 第 09b 批（暫稱）：相位追蹤的既有缺陷**（D47⑤ 的更正）：settling 期間收進來的舊樣本留在下一個視窗（修正後第一個視窗的 P90 實際約 P93）、settling 期間累積的 late 樣本在 settle 完成時立刻觸發第二次 late 修正，以及延遲的位移（v7 的「產生→執行」約 38 ms，v6 是 21～25 ms）。這會改相位常數或相位追蹤的定義，屬於重推條件 1，所以排在 M0 與凍結之前。可能與 D44 的 late 修正有關（hypothesis）；若修正確認會改變遲到修正或 backlog 的行為，依 D44 的提前條件停下問使用者。規劃與檔位在開始時決定。
  - （補充，2026-10-14，09ab-doc）：已完成（B2：樣本換算＋late 修正後重新擷取一次，D66），見 [09b](09b-phase-tracking.md)「09b-impl」與「09ab-L2 的結果」。合併 L2 依宣告 Jab5 不成立（δ＞70.6 ms 的串接），D71 記為已知限制並繼續；已知限制彙整在 09b 文件 1-7 節。
- **S3 = M0 影子量測**（常數變更前、S2 與 S2b 之後的頭）。宣告 high＋1 次 xhigh 對抗式檢查；執行 medium。
  - 更正（2026-10-14，D75）：**移到 v7.1，v7 只做條件式補做**。C2 的 clean 案例（12 Tick 連點、冷卻邊緣連點、30 FPS 矩陣）出現權威 Cooldown 拒絕時，停下回報，再補 M0：用已有的宣告草案 v2（`build/target/_build/test/logs/pvp-v7-09-m0-plan-20261014/draft-v2.md`）與 M0 probe（`../GYO-Engine-m0`，本機 commit `5ef8a89`，分支 `m0-probe-5ef8a89`，不推送），先完成中止的 xhigh 審查；D74 的 U1～U9 只在補做時適用。以下原文保留。
  - probe-only 修補：headless probe 在送出時記錄 `presented.shotTiming` 與閘門算出的 earliest（`gameplay_action.hpp:142` 的 `PresentAt` 回傳 `ClientPresented.shotTiming`，可行），寫進另一個檔案，避免凍結分析器拒絕；每段起點加 seed 偏移。
  - 跑次：clean-30／60／144 與 gateway-250ms、host-ipc-250ms、downstream-250ms 各 ≥6 次，兩種主機狀態都要有（約 30～40 分鐘）。
  - 窗口的事先定義：停頓＝resolved 時刻的 Tick 間隔 ≥ step＋1.5 ms，窗口涵蓋其後 0～3 Tick；被釘住＝每秒送出等待中位數 >2 ms 的玩家秒；領先跳升＝`lead_jumps.py`。
  - （補充，2026-10-14，09ab-doc）M0 規劃要處理的待決事項見 [09b](09b-phase-tracking.md)「09ab-L2 的事前宣告」第 12 節，其中新增的一條：B2 撤回後約 4 秒內相位誤差約 3.3 ms，超過 guard 的 2 ms（`FireGate.hpp:24-27`）。
  - 門檻（宣告時寫明根據）：窗口外 clean 射擊 R<earliest＝0 發；窗口外 yw＋L_w 的最大值 ≤ guard−0.5 ms；clean 的 R−earliest ≤7；臨界窗 s∈(6, 8] ms 至少約 100 發，不足就標「未驗證」；落在前一次上行 Actions 之後 I 以內的射擊另列最少發數。
- **S4 凍結**：`FireGate.hpp` 的註解（值不變）、`FireGateTests` 依上節改寫、v7-09 突變、本文件的推導節。high；時間線模型與確定性案例的局部 xhigh；1 次對抗式檢查。驗收：全量 CTest 與權威 digest、突變全部 killed；commit 並推送，CI 綠燈。
  - 更正（2026-10-14，D75）：改成 **S4-lite（縮小版凍結；範圍 2026-10-14 照建議核准）**：①值不變：`FireGateGuardSeconds`＝2 ms、`FireGatePendingSpreadTicks`＝7；依據是「常數的推導」（08b L2 after）與 09-M0-plan 的離線推估（`b205615` clean ys 最大約 0.81 ms，〔推論／離線〕）。②`FireGate.hpp` 註解：v7 的各項與「同路」前提的例外（送出被釘住已由 09a 處理；領先超過 2 step＋4 ms：late slew、撤回後殘餘、延遲下降 Wdrop、D44 領先跳升；主機與 Match 停頓），指向本文件；註明 v7.1 會重新檢討（A1、B1、B6）。③`FireGateTests`：釘住字面值（`CHECK(FireGateGuardSeconds==0.002)`、`CHECK(MovementPhaseDeadbandSeconds==0.002)`、spread 7）；「合成時間線」1～10 項的完整改寫移到 v7.1，v7 只做必要的最小更新（例如 `:174-179` 對 v6 第 10 批的引用改成 v7 來源）。④突變：值的變更由字面值釘住殺掉；既有 `12-no-guard`、`12-no-pending-spread` 確認沒有 stale。⑤本文件推導節標「縮小版凍結（D75）」，殘餘清單依 D74 U3／U8 改寫「常數涵蓋不了的失敗」第 2 項。檔位：high（實作）、1 次 xhigh 對抗式檢查（局部）；估 1.5～2.5 h。驗收照原文（全量 CTest 與權威 digest、突變 killed、CI 綠燈）。
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
  - GUI 取不到 earliest（`PvpApplication` 只在內部持有 `presented.shotTiming`，公開的觀察沒有 phaseShift 與 phaseDecided）；要取得就得改產品，違反 v6 逐位元組一致的前提。所以 C2 逐筆記錄點擊時刻、本機擋下、送出的 ID、前一發是否已有裁決、裁決；earliest 只在 M0 量。（補充，2026-10-14，D75：M0 移到 v7.1。C2 的判定本來就不參照 M0 的 earliest（GUI 取不到），所以判定規則不變；clean 的 (i)(ii) 或 30 FPS 矩陣出現權威 Cooldown 拒絕時，除了照原本的停止條件停下，也是補做 M0 的觸發。）
  - 判定（沿用 v6 第 12 批）：after 在 clean 的 (i)(ii) 中權威 Cooldown 拒絕＝0；**明文排除** action short 既有的刻意注入的那 1 次拒絕（v6 dev_log:69）。窗口內的拒絕仍然停下，但定位程序事先宣告。（補充，2026-10-14，D75：C2 的 clean 案例——12 Tick 連點、冷卻邊緣連點、30 FPS 矩陣——出現權威 Cooldown 拒絕時，停下回報，再補 M0；見步驟 S3 的更正。）本機擋下只記錄；L1 的參考值引用凍結後的模型版本（commit）。
- **25 案回歸**：同一個凍結頭，結果不進入 C2 的判定。relay 的「結果任一秒 ≤31」由 judge 套用；Gateway 的兩個最短間隔以字面值比較 ≥33.3，每跑次至少 1 個有值的視窗（第 08b 批判定 2、5）。`backpressure_probe.py` 6 案與 `run_network.py` 各 1 次；非 respawn 的重設照 08b 的 D44 規則。只記錄：D44 領先跳升、被釘住秒數、停頓、c 的分布。
- **「P2 頭 30 FPS 只記錄」**：P1b 頭 `805bd11`（從零重建）對 P2 頭 `a954aa3`，clean-30 各 6 輪交錯，依狀態分層。決定 1 選 A 或 B 時必須在修正之前跑（S1）；選 C 時併入 M0 的 session。（補充，2026-10-14，D75：選的是 A，S1 已完成；M0 移到 v7.1。）

## S1 的事前宣告（2026-10-10 使用者核准）

下面是證據目錄 `build/target/_build/test/logs/pvp-v7-batch09-s1-20261010/declaration.md`（SHA-256 `9e0869a0…`）的全文；核准時以那個檔案的雜湊寫進 `declaration.sha256`。準備：P1b `805bd11` 與 P2 `a954aa3` 在 detached worktree 從零建置（單元測試 197／197、206／206），xhigh 對抗式檢查要求的修正 A～J 已套用，並由另一位 high 核對（沒有缺漏）。核對另提 5 點非阻擋的觀察（例如 `result.json` 沒有 `timer_baseline` 時，可改用 `action-client.json` 的 `timer` 算主機狀態），沒有改。

**請特別看兩點**：
1. **P1b 沒有重現 C1 的雜湊**：執行檔內嵌了工作樹的絕對路徑、目的檔的時刻（Mach-O 的 N_OSO）與由此衍生的 UUID，所以任何重建都不會得到 C1 的雜湊。同源對照（同一產品程式在兩個位置建置）反組譯正規化後 0 行不同。「程式內容相同」仍是 hypothesis，需要你核准才開跑。
2. **S1 只記錄**：除了「12 次跑次完成、沒有觸發停止條件」，沒有通過或不通過的判定；結果不當作任何門檻的來源。

#### S1 的事前宣告：P2 頭 30 FPS，只記錄（草案；依計畫「步驟」S1，xhigh 對抗式檢查已完成，修正 A～J 已套用；使用者核准時，本檔 `declaration.md` 的 SHA-256 寫進證據目錄的 `declaration.sha256`）

- **目的**：在任何產品變更（第 09a 批）之前，於同一個 session 交錯量測 P1b 頭與 P2 頭的 clean-30，單獨顯示 P2 線對 30 FPS 的影響（HANDOFF 第 09 批：「P2 頭另跑一次 30 FPS，只記錄，單獨顯示任務 3 的影響」）。
  - 兩棵 tree 的差異是整條 P2 線：P2-log、第 06 批（Gateway 結果 30 Hz）、第 07 批（Match Tick 與 IPC）、第 08 批（Client 網路 worker 改用 asio，即任務 3）、第 08b 批（事件驅動轉送）。本次量測無法在這條線內部再分開歸因。
  - **只記錄**：除了「12 次跑次全部完成、沒有觸發停止條件」以外，沒有任何通過或不通過的判定。結果不進入 C2 的判定，也不當作 M0 或常數推導的門檻來源（D45：分布不向其他建置借用）。
- **對象與產物**（雜湊寫在 `artifacts.sha256`，開跑前與結束後各核對一次；不一致就停下）：
  - p1b＝`805bd11`（C1 的 after，PR #71 的頭）；p2＝`a954aa3`（第 08b 批的產品程式；`git diff --stat a954aa3..HEAD -- apps build engine tests` 為空）。
  - 兩棵都在 detached worktree（`../GYO-Engine-p1b-s1`、`../GYO-Engine-p2-s1`）從零建置。`build.sh` 由 C1 的 `build.sh` 複製，只改註解、輸出目錄與 worktree 清單（`build.sh.diff`）。
  - p1b：Match `aca1578b…`、Gateway `a1e68523…`（內嵌 `vcs.revision=805bd11`、`vcs.modified=false`）、probe `a1c4503e…`。
  - p2：Match `1d753abc…`、Gateway `5ba59cbb…`（內嵌 `vcs.revision=a954aa3`、`vcs.modified=false`）、probe `6e62bed7…`。
  - arena 兩棵都是 `0026013c…`。
  - **P1b 沒有重現 C1 的雜湊**：C1 after 是 Match `5884b869…`、Gateway `0fd81f75…`、probe `a5a13c26…`，本次三者都不同，只有 arena 相同。
    - 已知的差異有兩類：
      - ①執行檔內嵌絕對的工作樹路徑 `GYO-Engine-p1b-s1`。原始位元組出現次數 Match 172、probe 188、Gateway 11；`strings -a` 為 5／11／11 行。
      - ②Mach-O 的 debug map（N_OSO，P1b 的 Match 有 154 筆）在 n_value 記錄每個目的檔的修改時刻（第一筆 `0x6ac92312`＝2026-10-09T17:23:30Z），LC_UUID 由內容衍生。
      - 所以即使在同長度的路徑重建，Match 與 probe 也不會得到 C1 的雜湊；把路徑字串原地換回 `GYO-Engine-p1b-c1` 也不相等（`adversarial/path_substitution.txt`）。
    - 同源的對照（`adversarial/binary_compare.txt`）：
      - 比對兩組：本次 p2 的 Match 對主 checkout 的 Match（`c9cc6617…`，即第 08b 批 L2 的 match-after）；本次 p2 的 probe 對主 checkout 的 probe（`d45a1f0b…`，第 08b 批 L2 用的 probe）。
      - `nm -U` 符號集合相同（10210／11790），`__text` 大小相同。
      - `otool -tV` 反組譯只正規化 RIP 位移、分支與呼叫目標、位址註解（保留立即值）後，0 行不同。
      - `__cstring` 只差 0x40／0x36 bytes。
    - 建置條件：
      - C1 的 `build-p1b-c1.log` 與本次 `build-p1b-s1.log` 統一路徑、去掉 ninja 計數與秒數後，只差 configure／generate 的秒數與 1 行 ninja 狀態。
      - 11 個依賴來源都是乾淨的固定 tag（2026-10-03）；Go 1.27.1 於 2026-08-28 安裝。兩者都早於 C1。
    - 「程式內容相同，差異只在路徑、目的檔時刻與由此衍生的 UUID」是 hypothesis（未驗證：C1 的執行檔已移除，無法逐位元組比對）。以上是支持它的證據，沒有反證。
    - 依「本批另加的停止條件」2 的精神，這一點**請使用者核准**後才開跑。
  - 每棵 tree 用自己的 runner、Match、Gateway、probe 與 arena。
    - `805bd11..a954aa3` 中 `build/acceptance` 只改 7 個檔。其中 runner 與 probe 端只有 `gameplay_action.hpp`：多記一個 `received_ns`，不改送出與判定。另外 `judge.py` 從 p2 tree 匯入 `network_statistics.py`（只讀 Match 統計行）。
    - `run_gameplay.py`、`gameplay_evidence.py`、`command_evidence.py`、`command_evidence_v7.py`、`start_phase_evidence.py` 兩棵逐位元組相同。
- **分析器與腳本**（`analyzers.sha256`，開跑前與結束時由 `run.py` 核對）：
  - v6 最終清單的凍結子集：39 個 Python 檔，在兩棵 tree 都與 C1 的 `analyzers.sha256` 一致。
    - 原本有 40 個；`run_network.py` 在 `a954aa3` 改過（`--movement-trace`），S1 不使用，所以排除。
  - 分析器 v7：`command_evidence_v7.py` `394d5e25…` 與它匯入的 `command_evidence.py` `5fa9779b…`，兩棵相同。
  - 唯讀匯入的檔案：
    - C1 的 `summarize.py` `bbf51ddc…`：用它的 `substitutes`、`fisher_two_sided`、`STRUCTURAL`、`PROBE_FAILURES`。
    - 參考包 `analyze.py` `68ee027a…`：用它的 `host_late_p99`。
    - `pvp-v7-08-eval-20261009/segments.py` `6a3fa237…`。
    - `pvp-v7-08b-plan-20261009/d44-adversarial/lead_jumps.py` `14cd5b5c…`。
    - p2 worktree 的 `network_statistics.py` `b37baf63…`。
  - 本批腳本：`run.py`、`judge.py`、`sleeper.py`（C1 版 `98105dbf…`）、`build.sh`、`prep_hashes.py`。
    - 對抗式檢查改動了 `run.py`（`bd41e62b…`）與 `judge.py`（`51b7f294…`），已重跑 `prep_hashes.py`，以新的雜湊為準。
- **主機**：
  - 閒置：session 期間不做開發，開 caffeinate，前景沒有其他使用者程式。暫停 PR #73 的 auto-fix。
  - 閒置閘門（沿用 C1 第 2 次 session 的附註）：第 1 回之前連續 3 次 5 秒 sleeper，每次最大值都 <10 ms。不成立就每 30 秒重試，最多 30 分鐘，仍不成立就停下。
  - 每次跑次前後各跑一次 5 秒 sleeper，並記錄 CPU 最高的 5 個程序（C1 的做法；第 08b 批 L2 是每回 6 案的前後）。
  - 分層的依據是 probe 的 TimerBaseline（連線前 3 秒，`result.json` 的 `timer_baseline`）：
    - 主分類：`late_p99_ms` ≥6.0 為 8 ms 狀態（C1 `05-30fps-comparison.md:41`，與第 14b 批相同）。
    - 另列：`interval_p99_ms` ≥22（第 08b 批 L2 `judge.py:59`）的分類與不一致的跑次。兩者是同一次量測的兩個欄位，所以一致率不能當作分類正確的根據（本計畫 C2 的主機狀態一項）。
  - TimerBaseline 的 P99 是 nearest-rank，約 180 個樣本中的第 2 大（`timer_baseline.hpp:87-90`）；sleeper 的 P99 是約 300 個樣本中的第 4 大（`sleeper.py` 的 `int(q*(n-1))`）。TimerBaseline 量測時，同一 tree 的 Match 與 Gateway 已在執行（`action_probe.py:528`、`:533`、`:554`），沒有 session 流量，但不是沒有其他程序。兩棵 tree 的背景負載不同是否影響分類是 hypothesis（未驗證）。
  - 轉換：前後兩次 sleeper 各以 P99 ≥6.0 ms 判狀態，兩者不同就算「轉換」。只列出，不進入分層。TimerBaseline 只量連線前 3 秒，會漏掉跑次中途的轉換。
  - 交叉確認：p1b 的舊 Match 用參考抽取器的 `host_late_p99`（`start_ns` 後 1～15 秒）。p2 的 Match 晚醒不反映主機狀態（08b after 兩種狀態下 `tick_late` 都 ≤446 µs），只列出它的 `host_late_p99` 與 `tick_late_max_us`。
  - 兩棵 tree 8 ms 狀態頻率的雙側 Fisher 精確檢定：只記錄。S1 沒有判定，所以不設停止條件；C2 的 p<0.05 停止條件不套用到 S1。主機狀態算不出來的跑次不進入檢定。
- **跑次**：
  - 只跑遊戲矩陣的 clean-30（`run_gameplay.py --case clean-30`），每棵 tree 6 輪。
  - 第 k 回：k 為奇數時先 p1b 後 p2，偶數時先 p2 後 p1b。
  - 不隨機化（D47⑥）；不補跑（S1 只記錄，本宣告新訂，計畫未規定補跑）。
  - 目錄：`runs/clean-30-r<k>-<tree>/`，由 `run_gameplay.py` 建立，`run.py` 不得預先建立。另有 `progress.jsonl`、`judgement.json`，結束或停止時寫 `files.sha256`。
  - 每次跑次結束、做完後 sleeper 之後，用該 tree 自己的分析器 v7 在案例目錄寫出 `command-evidence-v7.json`。
  - 預計約 10 分鐘（12 次，每次約 20 秒加上兩次 sleeper 與分析器 v7），另加閒置閘門的時間。
- **每次跑次的分類**（沿用 C1）：
  - E：runner 逾時、沒有判定行、`ready.json` 或 `result.json` 缺漏。
  - A（與 C1 `summarize.py:96-109` 相同）：
    - `result.json` 的錯誤含 `summarize.STRUCTURAL` 的字樣；
    - `result.json` 沒有 `movement`，而錯誤不是以 `summarize.PROBE_FAILURES` 開頭（分析器本身拋出例外）；
    - 分析器 v7 的結束碼不是 0／1，或本跑次沒有寫出 `command-evidence-v7.json`。Python 未捕捉的例外結束碼也是 1，只看結束碼抓不到。
  - F：其他不通過，包括 `ready.json` 之後的 probe 失敗（`PROBE_FAILURES`）。沒有 `movement` 時替代列為「未知」；主機狀態算不出來的跑次只列在 tree×全部。
  - P：通過。
  - `run.py` 的判斷順序：
    1. 錯誤含 STRUCTURAL 字樣 → A。
    2. 沒有 movement：錯誤是 PROBE_FAILURES → F（不再看 v7）；否則 → A。
    3. v7 結束碼不是 0／1，或 `case_dir/command-evidence-v7.json` 不存在 → A。
    4. 其餘依 passed 分為 P／F。
- **停止條件**（立即停下，保留全部證據、不重跑，交使用者決定）：
  1. 分類 E 或 A。
  2. 任一棵 tree 出現權威 Cooldown 拒絕（`rejection==3`，`Combat.hpp:51-53`）。這是 P2 的停止條件；gameplay 計畫中沒有預期的 Cooldown（`gameplay-plan.json` 的 `expected_rejection` 沒有 3）。
     - 同一跑次若又有領先跳升，同時符合本計畫 D44 一節「要把 D44 提前的條件」。
  3. 產物或分析器的雜湊不符。
  4. 任一 worktree 的 HEAD 改變或追蹤檔有變更（每次跑次前核對）。
  5. 閒置閘門 30 分鐘內不成立。
  - `run.py` 本身的其他例外（sleeper 輸出無法解析、分析器 v7 逾時、`OSError` 等）也照停止處理：在 `progress.jsonl` 記為停止，照常寫 `files.sha256`。
  - 主 checkout 的 HEAD 在開始與結束時記錄。S1 結束前，第 09a 批的產品變更不得 commit（計畫：S1「必須在任何產品變更之前」）。
- **本批另加的停止條件在 S1 的對應**（內容請使用者確認）：
  1. 不適用（M0）。
  2. 依精神適用：P1b 的雜湊需使用者核准。
  3. 適用（E／A）。
  4. 只記錄；是否套用由使用者決定（見未決 5）。
  5. D44 提前條件：
     - Cooldown 與領先跳升出現在同一跑次時，已由停止條件 2 停下。
     - S1 結束後，若 p1b（舊版）出現 LifeRespawn 以外的 epoch 重設或領先跳升，報告中明列「D44 提前條件：舊版也出現」，並停下問使用者。
  6. 主 checkout 的 HEAD 在開始與結束時記錄。期間若出現非文件的 commit，該 session 依條件 6 作廢（S1 的產物在 worktree，不受影響，但仍照條件處理）。
  7. 不適用。
- **只記錄的指標**（每次跑次各一份，再依 tree×主機狀態與 tree×全部狀態彙總；不足 3 輪的格只列出。缺少輸入檔的跑次，例如 probe 失敗只留下 `client-failure.json`、沒有 `action-client.json`，需要該檔的項目記為 null，彙總時跳過並列出「未知」的輪數）：
  1. 通過與否：凍結分析器（`result.json` 的 `passed`／`errors`）與分析器 v7 的結果並列。C1 對 after 改用分析器 v7 判定的規則，在這裡不構成判定。
  2. 替代（Held＋Neutral）：用 C1 的 `summarize.substitutes`。
     - 範圍：`start_ns` 後［1, 15）秒，依解析時刻。
     - 拆成兩類：Match 停頓造成的（`snapshot_produced` 間隔 ≥40 ms 的那個 Tick 起 0～3 Tick，C1 `05-30fps-comparison.md:48`）與其他。
     - 列出筆數、來源、秒數與玩家。比率以兩種方式列出，以 `Fraction` 精確計算：排除 Match 停頓（Σ其他 ÷ Σ`remaining_originals`）與不排除（Σ（其他＋Match 停頓）÷ Σ`remaining_originals`）。C1 排除 Match 停頓的前提是兩棵 tree 的 Match 相同（`05-30fps-comparison.md:48`）；S1 的 p1b 是第 07 批之前的 Match，前提不成立，處理方式與 C2 計畫的更正相同。
  3. Actual 比率＝`movement.actual` ÷ `remaining_originals`。
  4. 停頓重設：`start_phase` 各 epoch 的 `stall_reseeds` 合計（凍結的 `start_phase_evidence.py` `e719dab6…`）。另列 LifeRespawn 以外的 epoch 重設。
  5. 移動延遲：`actual_p50_ms`／`actual_p95_ms`（產生→執行）。兩棵 tree 的「產生」都是模擬角色的步邊界，定義相同。
  6. a（probe 送出→relay 第一次上行）與 c（裁決 Tick 的 `snapshot_produced`→relay 第一次下行 kind 7）：
     - 用 `segments.run_actions`，只取合法裁決且五個時刻（送出、relay 上行、Tick、relay 下行、probe 取得）齊全的（`segments.py` 的 `run_actions`）。
     - 列出 P50／P95／最大，以及 c 換算成整 Tick 的分布。
     - p1b 的 Gateway 結果走 30 Hz 的 ticker，但寫出後重新錨定，多數間隔是兩個 ticker（平均約 15～18 包／秒，`06-gateway-results-30hz.md:8`、`:14`；第 06 批修正），c 的差異是預期中的。
     - b、d、`legal_match_p95_ms` 不跨 tree 解讀（第 08b 批 L2 宣告的只記錄項）；`legal_match_p95_ms`／`legal_client_p95_ms` 只列出。
  7. 被釘住的秒數：每位玩家、以該玩家第一筆 generated 起算的每一秒，非 seed 命令「第一次 sent−generated」的中位數 >2 ms 就算一秒；排除最後 1 秒（`pvp-v7-09-plan-20261010/adversarial/pinned_count.py:2-3`）。
     - 列出秒數、最長連續秒數，以及每位玩家整段的等待 P50／P95／最大。
     - 只對 p2（新 worker）有意義；p1b 照列，附等待中位數供判讀。
  8. D44 領先跳升：在每個案例目錄執行凍結的 `lead_jumps.py`（sha256 `14cd5b5c81ef73f9dee4d9d8593fdc6518dd183f8bcc8df14bf0098230ae3a07`），新舊分開累計。
  9. Tick 間隔：相鄰 authority Tick 第一筆 `resolved` 的時刻差 ≥ step＋1.5 ms（M0 的停頓定義，預先記錄）。只對 p2 的 Match 有意義：舊 Match 的 Tick 晚醒使大多數間隔都超過，試算時 C1 第 2 次的 P1b 12 輪有 1324 個。
  10. 主機：見「主機」一節，另列模擬角色晚醒的最大值（`action-client.json` 的 `simulation_wakes`）。
- **報告**：
  - 依 tree×狀態並列：輪數、通過（凍結／v7）、替代合計（排除與不排除 Match 停頓兩種比率）與每輪的筆數與秒數（Match 停頓另列）、Actual、P50／P95、停頓重設、a／c、被釘住秒數、領先跳升、Tick 間隔、Cooldown、轉換次數，以及各項「未知」的輪數。
  - 另列 TimerBaseline 兩種分類的不一致與 Fisher p。
  - 不寫「比較好」或「比較差」的結論；只以記錄的方式描述 P2 線的影響，並註明差異涵蓋第 06～08b 批與 P2-log。
- **門檻的來源（D45）**：S1 沒有判定門檻，只有以下分類與記錄用的界線。
  - TimerBaseline 主狀態：`late_p99_ms` ≥6.0 ms。來源 C1 `05:41`、第 14b 批；指標是 probe 連線前 3 秒的絕對期限晚醒 P99；不混入其他流量（在連線之前量）。
  - TimerBaseline 次狀態：`interval_p99_ms` ≥22 ms。來源第 08b 批 L2 `judge.py:59`；同一次量測。
  - sleeper 狀態：P99 ≥6.0 ms。`sleeper.py` 也用絕對的 1/60 秒期限；第 08b 批 L2 每回 sleeper P99 在 4 ms 狀態為 3.87～4.18 ms，在 8 ms 狀態為 6.42～8.24 ms（本次以該 session 的 `progress.jsonl` 計算）。
  - 閒置閘門：最大 <10 ms。來源 C1 第 2 次 session（使用者核准的附註）；建置與流量：獨立的 sleeper 程序。
  - Match 停頓：≥40 ms。來源 C1 `05:48`；指標是 Match 的 `snapshot_produced`。C1 以 v6 與 P1b 的 Match 在 clean-30／60 定義；套用到 p2 的 Match 是跨建置使用，只作記錄界線。
  - 被釘住：2 ms。來源本計畫 `09-firegate-c2.md:57`、`:113`；本次試算時第 08b 批 L2 after clean-30 的等待 P50 每位玩家 0.082～0.142 ms。建置與流量：第 08 批之後的 worker；非 seed 的移動命令，不含動作；計畫證據 `pvp-v7-09-plan-20261010/c2risk/send-wait-*.jsonl`。
  - Tick 間隔：step＋1.5 ms。來源本計畫 S3（`09-firegate-c2.md:113`）；指標是 Match 的 `resolved` 時刻。為新 Match 定義；舊 Match 上無效（試算 1324／1691）。
  - 領先跳升：20 ms。來源 `lead_jumps.py` 文件字串；指標是 gen→exec 中位數。建置與流量：第 08b 批規劃的 d44-adversarial，第 08 批 L2 的 trace。
- **證據**：`build/target/_build/test/logs/pvp-v7-batch09-s1-20261010/`。內容：
  - `declaration.md`（本宣告）、`run.py`、`judge.py`、`sleeper.py`、`build.sh`、`prep_hashes.py`、`artifacts.sha256`、`analyzers.sha256`、`prep.json`、`commands.txt`、`sha256.txt`。
  - `adversarial/`：xhigh 對抗式檢查的腳本與輸出（`path_substitution.txt`、`binary_compare.txt` 等）。
  - `dryrun/`：用既有證據（第 08b 批 L2、C1 第 2 次）對 judge 試算的結果；缺 `action-client.json` 的合成 F 跑次試算（`make_synthetic_f.py`、`judgement-synthetic-f.json`）；`run.py` 分類與例外處理的模擬（`mock_run_d.py`、`mock_main_e.py` 與輸出）。

## C2 的事前宣告（2026-10-14 核准，D79；S6）

下面是證據目錄 `build/target/_build/test/logs/pvp-v7-09-c2-prep-20261014/declaration.md`（SHA-256 `eb4caa6d432897ef952f14d657858af0935f16be9e972c05a49082072932e19b`）的全文；它和同目錄的 `declaration-draft.md` 逐位元組相同，標題的「未核准」是核准前的字樣，核准紀錄見 HANDOFF 的 D78、D79。xhigh 的 D45 檢查見同目錄 `xhigh-d45-check.md`。

### 第 09 批 C2 的事前宣告（草案 v2，S6；D45 的 xhigh 檢查已套用，未核准）

核准時，本檔原樣複製為 prep 目錄的 `declaration.md`，它的 SHA-256 寫進「執行步驟」的 `DECLARATION` 字面值。`run_c2.py` 在開跑時核對這個字面值，並寫進開始紀錄；`judge_c2.py` 再核對一次。四個字面值（`--frozen-sha`、`--artifacts-sha`、`--declaration-sha`、`DRIVER_SHA`）一律抄本檔，不從手邊的檔案重算。

#### 白話摘要

- **要做什麼**：在同一台機器、同一個 session 裡，交錯跑 v6 最終版（before）與 v7 凍結頭 `d685d1e`（after），回答兩個問題：
  1. **30 FPS 的移動**（C2-A）：v7 的 clean-30 是否和 clean-60 一樣穩，而且不比 v6 差。用的是 C1 的那一套規則，逐條重新宣告。
  2. **連點會不會被伺服器以「冷卻中」拒絕**（C2-B）：用 SDL 注入的點擊，走產品自己的射擊閘（FireGate），在 30／60／144 FPS 連點。after 的「12 Tick 連點」與「每一幀都點」中，伺服器的 Cooldown 拒絕必須是 0。
  3. 另外跑 after 的回歸（25 案、背壓 6 案、網路探針）。回歸有自己的通過與否，不進入 C2 的判定。
- **怎麼判**：全部交給事先凍結的 `run_c2.py`（執行）與 `judge_c2.py`（判定）；不重跑、不補做。乾淨案例出現 Cooldown 拒絕就立刻停下，這同時是 D75 的 M0 補做觸發。
- **你要配合的事**：
  - 正式量測約 **1.7～1.9 小時**（含閒置閘門與判定；細目見「跑次」），期間不要用這台機器。
  - 其中連點約 1 小時會在桌面上開兩個遊戲視窗，**請不要碰滑鼠、觸控板與鍵盤**：
    - 移動滑鼠：記成「受干擾」（D 類），那一跑不算；累計 3 次就停下。
    - **按下滑鼠按鍵**：點擊紀錄會和產品計數對不上，記成 A 類，**立刻停下**。D 類只抓得到視角的變化。
  - 量測期間暫停 PR #73 的 auto-fix；開跑時與結束時，PR 分支在遠端只能比 `d685d1e` 多出文件的 commit，否則開不了跑，或判定不通過。
- **需要你決定的**：D78 已核准 U1～U8。這次的 D45 檢查之後，**U5 改寫**，並**新增 U9**（D44「舊版也出現」其實在 C1 的資料就已成立）。兩項都在最後一節，各附建議。

#### 目的

- 依計畫（`09-firegate-c2.md`「C2 的計畫」、D31、D47 ⑦⑧⑨、D75、D77、D78），以同一份事前宣告比較 v6 最終 tree 與 v7 凍結頭：
  - C2-A：30 FPS 對比矩陣（D31；C1 的判定逐條重新宣告）。
  - C2-B：連點（決定 7）：
    - (i) 12 Tick 連點：判定；
    - (ii) 冷卻邊緣連點：after 判定，before 只記錄；
    - (iii) 10／11／13 Tick：只記錄；
    - (iv) Gateway 暫停 250 ms 後 6 秒的連點：非 clean，只記錄。
  - 回歸：同一個凍結頭的 25 案、`backpressure_probe.py` 6 案、`run_network.py` 1 次。
- 前面批次要求 C2 寫明的事，本宣告的對應：
  - 被釘住的秒數與 late 修正數（`09b-phase-tracking.md:1363`、`:1696`）：見「只記錄」。
  - after 包含資訊流修正，以及 late 修正／D44 重設增加時怎麼歸因（`08c-drop-report.md:760`、`:777`）：見「對象與產物」的差異清單與歸因。
- 「P2 頭 30 FPS 只記錄」已在 S1 完成，不在本次。

#### 對象與產物

雜湊寫在 `artifacts.sha256`（SHA-256 `d7083fd9af23618e0e6e1ba56becb621d4ebb8cc5387eca432ba806cc7bec262`，82 行），`run_c2.py` 在每個項目之前核對；不一致就停下。

- **before＝`../GYO-Engine-v6final`**（`fee92ff`，產品程式＝`c7d6dd3`＝tag `object_fps_pvp-v1.1.0`；唯讀）。
  - 重算的雜湊與 C1、第 03a 批一致：Match `81166294…`、Gateway `b45d4334…`、action probe `3a99d3ff…`、arena `0026013c…`（`prep.json` 的 `before_vs_c1`、`before_vs_03a`）。
  - 矩陣用 v6final 自己的 runner、Match、Gateway、probe、arena。
- **before 的連點＝v6final 的 Match、Gateway、arena、部署資產＋`../GYO-Engine-v6c2` 的 GUI probe 與 rapid runner**（`1be1edd`，D77 (a)）。
  - GUI probe `400884dd…`，與 S5 `v6c2-patched.sha256` 一致。GUI probe 經 rpath 載入的 SDL（v6c2 的 `_deps/sdl3-build/libSDL3.0.dylib`，`012d6ed1…`）也列入。
  - GUI probe 內的產品程式和 v6final 不是逐位元組相同（內嵌 worktree 路徑）。依 D77，以 S5 的正規化比對作為同一程式的證據：C++ 反組譯 0 差異，Gateway 只差 build ID 與 pclntab。本批停止條件 2 依此解讀。
  - `run_action_short.py` 在 v6c2 與 after 逐位元組相同（`3a4e39a2…`）；兩棵的 `RunActionRapid` 也沒有差異（D45 檢查已核對）。
- **after＝`../GYO-Engine-c2-after`**（detached worktree，`d685d1e`，從零建置；`build.sh`，`build.sh.diff` 是和 S1 版的差異）。
  - `d685d1e` 的產品程式等於 `b205615`（09ab-L2 after、D72 矩陣的建置），只差註解：S4-lite 的 `FireGate.hpp` 與 09ab-doc 的註解（`git diff --stat b205615 d685d1e -- apps engine services`：4 檔、只有註解）。`6587030`（S4-lite）之後只有 S5 的 `build/acceptance` 與文件。
  - 建置：`build.sh` 18:18～18:40 從零建置，結束碼 0（`build-c2-after.log`、`prep.txt`）；之後 `git status --porcelain --untracked-files=no` 為空。
  - 產物：
    - Match `657ef798…`、Gateway `5aa8907a…`（內嵌 `vcs.revision=d685d1e`、`vcs.modified=false`）、Client app `fdc088cb…`；
    - action probe `951109f9…`、GUI probe `587dd64f…`（與它載入的 `libSDL3.0.dylib` `43a7d227…`）、timing probe `dae05222…`、network probe `c979b605…`；
    - digest runner `9bccd492…`／`44b51562…`（-O0）；
    - arena `0026013c…`（兩棵相同）與部署資產 31 檔。
- **after 與 before 的差異**（像 S1 那樣列出；本次量測無法在這條線內部再分開歸因）：
  - P1a／P1b：任務 1、2（Client 三角色分離、命令在固定步邊界產生；第 01～05 批，C1）；
  - P2-log；第 06 批（Gateway 結果 30 Hz）；第 07 批（Match 的 Tick 與 IPC）；第 08 批（Client 網路 worker 改用 asio）；第 08b 批（事件驅動轉送）；
  - 第 08c 批（丟棄回報與**資訊流修正**：已解析的命令照常轉送）；第 09a 批（送出被釘住的修正，`InputSendDrainRate`＝80）；第 09b 批（相位追蹤 B2）；S4-lite（只改註解與測試）。
  - **歸因**：08c 的風險 1 預測資訊流修正會讓 late 修正變多、D44 重設可能增加（`08c-drop-report.md:777`）。所以 after 的 late 修正或 D44 重設比 before 多時，只記錄為「整條 v7 線的差異」，不歸給單一批次；v6 本身也有 late 修正（U9）。
- **權威 digest**（`digest.sh`，`digest/`；**全部通過**）：
  - after 的 CTest `object_fps_pvp.authority_digest`：1／1 通過；
  - before 用 v6final 自己的檢查腳本、golden 與兩個 runner，輸出寫在本目錄：30 個 golden 情境 0 不符，-O0 比對 30 個零角度情境通過；
  - 跨 tree：`compare_authority_trees.py` 的 base＝v6final runner、branch＝after runner，scale 1 與 3：都是 35 情境、0 不同。
- 開跑前兩棵 tree 的權威 digest 都要通過（計畫「建置」）。

#### 分析器與腳本

清單 `analyzers.sha256`（SHA-256 `680d74614db3027a65288684bccc691fe60c57887a43c886b7ffb7638b4b9280`；142 行），`run_c2.py` 在每個項目之前核對全部。

- 三棵 tree 的 `build/acceptance/object_fps_pvp/*.py` 全部列入，傳遞匯入不必逐一追。
  - 和 C1 的凍結清單（40 檔）比較：before 全部相同；v6c2 只有 `run_action_short.py`、`test_run_action_short.py` 不同（S5）；after 另有 `run_network.py`（`a954aa3` 起的 `--movement-trace`）不同（`prep.json` 的 `c1_frozen_subset`）。
  - 所以矩陣（`run_gameplay.py` `0a2bc490…` 及其匯入）兩棵相同；連點的 runner 兩棵相同；`run_network.py` 只用 after 的。
  - `start_phase_evidence.py`（late 修正數的來源）兩棵都是 `e719dab6…`。
- 分析器 v7：after 的 `command_evidence_v7.py`（`394d5e25…`）與 `command_evidence.py`（`5fa9779b…`）。**兩棵 tree 的矩陣跑次都用 after 的這一份**（C1 的做法；before 只作診斷）。
- 唯讀匯入（`judge_c2.py` 載入時自行核對這 6 個雜湊）：
  - C1 的 `summarize.py`（`bbf51ddc…`）：`substitutes`、`fisher_two_sided`、`STRUCTURAL`、`PROBE_FAILURES`、`REFERENCE_P50` 與常數；
  - 參考包的 `analyze.py`（`68ee027a…`）：`host_late_p99`；
  - `segments.py`（`6a3fa237…`）、`lead_jumps.py`（`14cd5b5c…`）；
  - after 的 `network_statistics.py`（`b37baf63…`）與 `action_probe.py` 的 `maximum_window`（`59002b43…`）。
- 本批腳本：
  - `run_c2.py`（`31bec643…`）；
  - `judge_c2.py`（`5d2ed0f3…`；內含 `DRIVER_SHA`＝`31bec6430c82510930557713a7763894c555c68e6e53c92daff9c0343308ed4f`）；
  - `sleeper.py`（S1／C1 版 `98105dbf…`）、`build.sh`、`prep_hashes.py`、`digest.sh`。
  - `judge_c2.py` 同時提供 `run_c2.py` 用的排程、每個項目的分類、停止判斷與 C1 的補跑規則，所以執行與判定用同一份程式。
  - `pinned`、`tick_gaps`、`lead_jumps` 的計算照抄 S1 的 `judge.py`：S1 的判定檔把匯入固定在 `../GYO-Engine-p2-s1`，本批不碰那個 worktree。
- D61 的事後規則的基準：`b205615` 的 D72 矩陣（`pvp-v7-09ab-matrix-20261014/matrix/`）逐案的「重設原因×故障窗內／外」，以字面值寫在 `judge_c2.D61_BASELINE`。7 個 1000 ms 案例有重設，全部在窗內：starvation 1～3 次，upstream-1000ms 另有 backlog 2 次。

#### 主機

- **閒置**：
  - session 期間不做開發，`caffeinate -dims`，前景沒有其他使用者程式。暫停 PR #73 的 auto-fix。
  - **非文件變更**（本批停止條件 6）。主 checkout：開跑時、每個項目之前、結束時，`git diff --name-only d685d1e` 只能出現文件（`docs/` 或 `*.md`）。這一條同時涵蓋 commit 與未 commit 的變更，所以核准後、開跑前的非文件 commit 也抓得到。
  - PR 分支在遠端（auto-fix 會推到這裡）：開跑時與結束時以 `git ls-remote origin refs/heads/claude/pvp-v7-p2` 取頭。這個頭必須已經在本機，而且和 `d685d1e` 的差異只有文件。開跑時不成立就停下；結束時的值寫進結束紀錄，由 `judge_c2.py` 檢查。
  - 本宣告裡的「非文件變更」都是這個意思。
- **閒置閘門**（C1 第 2 次 session 的附註）：第 1 個項目之前連續 3 次 5 秒 sleeper，每次最大值都 <10 ms；不成立就每 30 秒重試，最多 30 分鐘。
- **每個項目前後**各跑一次 5 秒 sleeper，並記錄 CPU 最高的 5 個程序。
- **C2-A 的分層**（宣告為偏離 C1，決定 8）：
  - 主分類：probe 的 TimerBaseline（`result.json` 的 `timer_baseline.late_p99_ms`；連線前 3 秒，`sleep_until` 絕對期限）≥6.0 ms 為 8 ms 狀態。兩棵 tree 的 action probe 量法逐字相同（D45 檢查已核對）。
  - **這違反 probe 自己的標註**：標註是 `use: interpretation only; never a verdict, threshold or denominator`，而分層等於把它當分母的劃分。這一點由決定 8 核准。
  - 背景負載的影響有部分證據：S1 的 `late_p50` 在 p1b 是 2.33～2.86 ms、p2 是 1.70～2.17 ms，6／6 分得開；`late_p99` 在 4 ms 狀態分不開。所以兩棵 tree 的分類可能有系統差，Fisher 的停止偏保守。
  - 另列：`interval_p99_ms` ≥22 的分類與不一致的跑次。兩者是同一次量測的兩個欄位，一致率不能當作分類正確的根據。
  - 轉換：前後兩次 sleeper 各以 P99 ≥6.0 ms 判狀態，兩者不同就算轉換；只列出。
  - 交叉確認：before 的舊 Match 用參考抽取器的 `host_late_p99`（`start_ns` 後 1～15 秒）。after 的 Match 晚醒不反映主機狀態，只列 `tick_late_max_us`。
  - 基本輪數之後，兩棵 tree 的 8 ms 狀態頻率（clean-30＋clean-60）做雙側 Fisher 精確檢定，**p<0.05 就停下**（本批停止條件 4）。
  - 主機狀態算不出來就停下（C1）：P／F 跑次沒有 TimerBaseline，或 before 的跑次算不出舊 Match 的 `host_late_p99`。
- **C2-B 的主機狀態只記錄**：
  - GUI probe 的計時器以幀起點為準（`SDL_DelayNS`，`schedule: frame_relative`），晚醒的定義和 action probe 不同（S5 開發跑次約 1.1 ms），6.0 ms 的門檻不適用。
  - 所以連點只列前後 sleeper 的狀態與轉換（sleeper P99 ≥6.0；來源是 08b L2 的雙峰分布）。C2-B 的判定（Cooldown＝0）不分層；結論附上判定跑次的 sleeper 狀態組成。

#### 跑次

- 順序：A → B → R（D78 U1）。`--parts` 可以只跑其中一部分。第 k 回：k 為奇數時先 before 後 after，偶數時先 after 後 before（C1）。不隨機化（D47 ⑥）。
- **A：30 FPS 矩陣**（`run_gameplay.py --case`）：
  - 基本：第 1～6 回，每回 clean-30、clean-60（k≤3 時再加 clean-144）。每棵 tree clean-30／60 各 6 輪、clean-144 3 輪，共 30 項。
  - 補跑（C1 的規則，狀態改用 TimerBaseline）：基本輪數之後先做停止判斷；沒有停下，就看每一種有樣本的狀態。clean-30／60 × before／after 這 4 格只要有一格少於 3 輪，就再跑一整回（只含 clean-30、clean-60）。直到每格都 ≥3 輪，或每棵 tree 每個案例都到 12 輪。補跑最多 24 項；用 C1 的資料模擬時就補滿了。
  - 每項的後 sleeper 之後，用 after 的分析器 v7 寫出 `command-evidence-v7.json`。
- **B：連點**（`run_action_short.py --case rapid-<情境>-<FPS>`，`--gpu-driver metal`）：
  - 第 1～3 回 × FPS（30、60、144）× 情境（interval12、edge、interval10、interval11、interval13、pause）× 兩棵 tree，共 108 項。
  - 每次跑次：
    - interval 情境固定 4 段×11 發＝**44 發送出**；
    - edge 是點擊時間累計 5 秒，60 FPS 約 285 次點擊、31 發送出；
    - pause 是 Gateway SIGSTOP 250 ms、放開後每幀點 6 秒。
  - 判定格的樣本以**送出的發數**計（本機擋下的點擊不可能被權威拒絕）。3 輪合計：interval12 132 發、edge 約 93 發，都 ≥40（決定 7）。
  - 12 Tick 的定義照 D77 (b)：從段起點每 12 Tick 排程，在第一個到期的幀點擊。
    - 實際間隔重算：60 FPS 的 v7 是 11.26～12.59 Tick、v6 是 11.29～12.63 Tick（D77 與 S5 報告寫的 11.2～12.4 有誤），都大於冷卻 10。
    - 30 FPS 的間隔約 10～14 Tick，會碰到冷卻邊緣。
- **R：after 的回歸**（27 項）：
  - 25 案逐案執行（`run_gameplay.py --case`，順序照 `--list-only`）；
  - `backpressure_probe.py`（timing probe，6 案一次）；
  - `run_network.py`（network probe，1 次）。
- 目錄與檔案：
  - `runs/<案例>-r<回>-<tree>/`（A、B）與 `runs/<案例>-after/`（R），由各 runner 建立，`run_c2.py` 不預先建立。
  - runner 的工作目錄是 `runs/`，不是 tree 的根目錄，讓唯讀的 v6final 不會被寫入；子程序設 `PYTHONDONTWRITEBYTECODE`。
  - 另有 `progress.jsonl`（每個事件一行）；結束或停止時寫 `files.sha256`。
- **不補跑、不重跑**。使用者在 session 目錄放 `STOP-REQUEST` 檔，會在下一個項目之前停下。
- **時間**（`run_c2.py --list-only` 的估算；每項含兩次 sleeper 約 32 秒，以 C1 第 2 次的 54 項 28 分鐘與 S5 開發跑次的 14～20 秒推算）：

  | 部分 | 項數 | 估計 |
  |---|---|---|
  | 閒置閘門 | — | ≥0.3 分 |
  | A 基本 | 30 | 16 分 |
  | A 補跑 | 0～24 | 0～13 分 |
  | B 連點 | 108 | 約 58 分（桌面會開視窗） |
  | R 回歸 | 27 | 約 18 分 |
  | 判定 | — | 約 5 分 |
  | 合計 | 165～189 項 | **約 1.7～1.9 小時** |

#### 分類

- **A、R 的遊戲案例**（沿用 S1／C1；`judge_c2.classify_gameplay`）：
  - E：runner 逾時（900 秒）、沒有判定行、`ready.json` 或 `result.json` 缺漏。
  - A：
    - 錯誤含 `summarize.STRUCTURAL` 的字樣；
    - 沒有 `movement`，而錯誤不是以 `PROBE_FAILURES` 開頭；
    - （只有 A 部分）分析器 v7 的結束碼不是 0／1，或本跑次沒有寫出 `command-evidence-v7.json`。
  - F：其他不通過（含 `ready.json` 之後的 probe 失敗）。P：通過。
- **B 的連點**（新訂；`judge_c2.classify_rapid`）：
  - E：runner 逾時（300 秒）；`result.json` 缺漏；runner 在產生紀錄之前就出錯（服務啟動失敗、GUI 期限逾時等）。
  - D（受干擾）：任一角色的報告 `disturbed`（排程外的滑鼠移動）。不是產品結果，不算入輪數，不重跑。
  - A（紀錄不完整）：
    - 逐筆點擊紀錄缺漏，或和產品計數不一致（外部的滑鼠按鍵會走到這裡）；
    - 送出的點擊沒有裁決（`missing`）；
    - `attribution_mismatches` 或 `decision_order_violations` 不為 0；
    - 逐筆歸屬的 Cooldown 數**大於**產品計數；
    - Cooldown 計數取不到（create 或 join 的計數缺漏）。
  - F：其他未完成（GUI 結束碼不是 0 等）。P：完成。
  - `ambiguous`（同一幀收到多個裁決、無法逐筆歸屬）只記錄：判定用的是產品計數器，它是精確的。
- **R 的背壓與網路**：
  - 背壓：E＝逾時，或 `backpressure-results.json` 不是 6 案；F＝任一案不通過。
  - 網路：P＝`result.json` 的 `passed` 而且結束碼 0；其餘 F（逾時是 E）。
- `judge_c2.py` 從原始檔案重算每一項的分類，和 `progress.jsonl` 不一致就算完整性問題。

#### 停止條件

立即停下，保留全部證據、不重跑，交使用者決定。由 `run_c2.py` 在每個項目之後判斷（`judge_c2.item_stop`），`judge_c2.py` 事後重播。

1. 分類 E 或 A（任何部分、任何 tree）。
2. **權威 Cooldown 拒絕**（`rejection==3`，`Combat.hpp:51-53`）：
   - A 的矩陣，任一棵 tree（S1 的做法；gameplay 計畫沒有預期的 Cooldown）。after 的同時記為 **M0 補做觸發**（D75）。
     - 說明：矩陣是 headless、不經閘門，這裡的 Cooldown 表示時序前提被打破。照 D75 也觸發 M0。
   - B 的 after、clean 情境（interval12、edge、interval10／11／13）。interval12、edge 記為 M0 補做觸發（D75）；interval10／11／13 是否觸發 M0 由使用者決定（D78 U3）。
     - clean 情境的 P／F 跑次若 Cooldown 數是「未知」，也停下。
   - R 的 clean-30／60／144（重推條件 10 所說「回歸」的 clean 量測）。是否觸發 M0 由使用者決定。
   - Cooldown 的數法：
     - A、R：`actions.jsonl` 的裁決。
     - B：create 的產品計數 `authority_cooldown_rejections` 與逐筆歸屬數取大者，加上 join 的計數，再減去刻意注入（create 報告的 `deliberate_cooldown_injections`、`cooldown_injection_action_id`）。
       - join 的計數只算 join 自己的裁決（`PvpApplication.cpp:377-381`）。
       - 連點模式沒有注入，所以減去的是 0；v6 dev_log:69 的那 1 次只在 action short，本次不跑。
       - 逐筆歸屬數大於產品計數時是 A 類（上面），所以「取大者」實際上等於產品計數。
3. 主機狀態算不出來（見「主機」）。
4. A 的基本輪數之後，以及 A 結束時（C1）：
   - Fisher p<0.05；
   - before 無法重現 v6 參考範圍：C1 的 (a) before clean-60 有 ≥2 輪失敗，(b) before 的 P50 中位數落在參考包範圍 ±3 ms 之外。狀態用 before 自己的舊 Match `host_late_p99`，和參考包的分類相同；
   - 判定第 3 條不成立。
5. **回歸的 D61 即時規則**：非 respawn 的重設出現在故障窗 (start, release＋1.5 s] 之外；沒有故障窗的案例出現就停（25 案一律適用，見「判定」）。
6. B 的受干擾跑次累計 3 次（機器沒有保持無人操作）。
7. 雜湊與 worktree：
   - 產物或分析器的雜湊不符（含清單本身對宣告字面值的雜湊）；
   - 任一 worktree 的 HEAD 改變，或追蹤檔有變更；
   - 非文件變更（主 checkout 或遠端 PR 分支，見「主機」）；
   - `declaration.md` 的雜湊不符。
8. 閒置閘門 30 分鐘內不成立。
9. `STOP-REQUEST` 檔；`run_c2.py` 自己的其他例外。

- **本批另加的停止條件的對應**：
  - 1 不適用（M0 移到 v7.1），改為上面 2 的 M0 觸發；
  - 2 依 D77 的正規化比對解讀；
  - 3＝上面 1；4＝上面 4；
  - 5（D44 提前）見「只記錄」與 U9；
  - 6＝上面 7；
  - 7 依換檔規則。
- **Cooldown 的定位程序**（事先宣告；不判定。GUI 取不到 earliest，D75）。對每一筆拒絕列出：
  - 點擊的時刻與幀、`resolved_tick`；
  - 前一發的 ID、`resolved_tick` 與裁決，兩者的 Tick 差；
  - 點擊當下的 `snapshot_tick`、`next_allowed_shot_tick`、本機冷卻剩餘、前一發裁決是否已到；
  - 前後 0.25 秒內的最長幀；Gateway 暫停的時段（pause）；Match 統計行的 `tick_late_max_us`；前後 sleeper。
  - **盲點**：連點的 Match 沒有開 `--movement-trace`，沒有 resolved 時刻的 Tick 間隔；統計行的 `tick_late_max_us` 會漏掉 Tick 間隔（計畫 `:37`）。所以 B 的定位抓不到 Match 停頓。
  - A、R 的拒絕另列同一跑次的領先跳升、被釘住秒數、late 修正與 Tick 間隔。

#### 判定

##### C2-A（C1 的規則逐條重新宣告）

| C1 的條目 | C2 | 理由 |
|---|---|---|
| v6＝本批 before 同一狀態的格；參考包只列出 | 沿用 | |
| 一條只用它引用的格；每格 ≥3 輪才判定，否則「未驗證」；N＝被判定那一格的輪數 | 沿用 | |
| 主機狀態＝Match 的 `host_late_p99` | **改寫**：TimerBaseline `late_p99_ms` ≥6.0 | 新 Match 的晚醒不反映主機狀態（計畫）；違反 probe 的標註，由決定 8 核准 |
| 每輪通過：before＝凍結分析器；after＝凍結分析器扣掉 `GAP_ERROR`，再加分析器 v7 | 沿用 | after 仍是分執行緒的 v7 |
| 對照的前提：v6 clean-30 替代比率 ≥0.15%，或 ≥2 輪各 ≥5 筆（不含 Match 停頓） | 沿用 | before 的 Match 與 C1 相同 |
| 1. v7 c30 通過率 ≥ v7 c60 − 1／N，且 ≥ v6 c30 | 沿用 | |
| 2 前半. v7 c30 比率 ≤ 2×v7 c60 比率＋0.05% | 沿用（排除 Match 停頓）；不排除的結果另列 | 同一棵 tree 的 Match 相同，C1 排除的前提成立 |
| 2 後半. （有對照時）v7 c30 ≤ v6 c30 ÷3＋0.05% | **改寫**：排除與不排除各算一次，兩者都成立才成立；結果不同時判定為「使用者決定」 | 兩棵 tree 的 Match 不同，C1 排除的前提不成立（計畫、D78 U2） |
| 3. v7 c60 不比 v6 差（通過率 −1／N、比率 ≤2×＋0.05%）；只因延遲門檻不成立時標第 04 批的已知位移、停下 | 沿用；比率部分同上改寫 | |
| 4. v7 c30、c60 不分狀態：停頓重設 0、LifeRespawn 以外的 epoch 重設 0 | 沿用（沒有 `movement` 的 F 跑次也判不成立，和 C1 一樣偏保守） | |
| 5. clean-144 只記錄；移動延遲只記錄 | 沿用 | |
| 6. 第 03a 批的數字只列出 | **改為** C1 第 2 次的數字只列出 | |
| 結論：至少一個狀態判定到有對照的第 2 條後半、所有判定的條目成立、第 4 條成立 → 通過；否則「未證明」並列出缺少的狀態 | 沿用；另加「使用者決定」（上面兩種情形） | |
| 補跑規則 | 沿用，狀態改用 TimerBaseline | |
| 停止條件「before 無法重現 v6 參考範圍」 | 沿用，狀態用 before 的 Match `host_late_p99` | 參考範圍是用這個分類定的 |
| Fisher p<0.05 | 沿用，狀態改用 TimerBaseline | |

- 替代、比率、門檻都以 `fractions.Fraction` 從原始計數精確計算（C1）。

##### C2-B

- **判定**（沿用 v6 第 12 批）：after 的 (i) interval12 與 (ii) edge，在 30／60／144 FPS 共 6 格，每一格的權威 Cooldown 拒絕都是 0（扣掉刻意注入；本模式沒有注入）。
  - 每格要有 ≥3 輪 P，P 的**送出**合計 ≥40，而且沒有 Cooldown 數「未知」的跑次；否則該格「未驗證」，C2-B 的結論是「未驗證」，並列出缺少的格。
  - 任一格出現 Cooldown 拒絕就不通過（session 已依停止條件 2 停下）。
  - 結論附上判定跑次的 sleeper 狀態組成。
- 本機擋下只記錄。L1 的參考值引用凍結後的模型版本：`FireGateTests.cpp` 在 `6587030`（S4-lite）。
- before 的全部情境、after 的 (iii)(iv)：只記錄。

##### 回歸（不進入 C2 的判定；另列「回歸通過」）

- 25 案每案都要成立：
  - `run_gameplay.py` 的判定通過（凍結分析器，不扣 `GAP_ERROR`，和 08b 一樣）；
  - relay 的「結果任一秒」每個 session ≤31（`action_probe.maximum_window`，第 08b 批判定 3）；`relay.json` 缺漏或沒有玩家也算問題；
  - Gateway 的 `results_min_interval_ms` 與 `link_action_min_interval_ms`，以印出的字面值在每個有值的視窗 ≥33.3，每跑次至少 1 個有值的視窗（第 08b 批判定 2、5）。
- **D44 的重設規則＝D61**（08c L2 已核准的規則；D45 檢查後改寫，見 U5）：
  - 即時規則（25 案一律適用，是停止條件 5）：非 respawn 的重設（backlog、starvation 等）出現在故障窗 (start, release＋1.5 s] 之外就停。沒有故障窗的案例出現就停。08b 的「故障案例出現 backlog 就停」依 D61 拿掉。
  - 事後規則：以同一產品程式的 `b205615`（D72 矩陣）為基準，逐案比較「重設原因×窗內／外」的次數。出現基準沒有的原因或位置，或次數比基準多，就立旗標交使用者判斷。每案只有 1 個跑次，所以照 D61 的「1 個跑次的交使用者判斷」。
- `backpressure_probe.py` 6 案全部通過；`run_network.py` 通過。

##### 整體

- `judge_c2.py` 的結束碼 0 只在以下全部成立時：
  - session 完整、沒有停止、完整性沒有問題、沒有應停未停、沒有給使用者的旗標；
  - C2-A、C2-B、回歸都通過（只跑部分時只看該部分）。
- 其他情形結束碼 1，原因寫在 `judgement.json`。
- 完整性的檢查項目：
  - 開始紀錄要有 `DRIVER_SHA`，以及兩個清單與宣告的字面值；兩個清單與 `declaration.md` 的雜湊等於字面值；`judge_c2.py` 本身在清單中；
  - 開始與結束紀錄的主 checkout 只有文件變更；遠端 PR 分支只有文件變更；
  - `files.sha256` 與檔案一致，`runs/` 沒有清單外的檔案；
  - 項目的順序等於宣告的排程（A 的補跑回數，由紀錄重播 C1 的規則決定）；
  - 每一項的分類可由原始檔案重算。

#### 只記錄

每項各一份，再依 tree×狀態（A）、tree×情境×FPS（B）彙總；不足 3 輪的格只列出；缺少輸入的項目記為 null，彙總時列出「未知」的輪數。

- **A**：
  - 凍結分析器與分析器 v7 的通過並列；
  - 替代（Match 停頓另列；兩種比率）、Actual、P50／P95、停頓重設、非 respawn 重設；
  - a／c 與 c 的整 Tick 分布（`segments.run_actions`）；
  - 被釘住的秒數（2 ms，`pinned_count.py` 的定義）；
  - **late 修正數**：`start_phase.*.epochs[*].late_corrections`（凍結的 `start_phase_evidence.py`，兩棵同為 `e719dab6`）。兩棵 tree 分開累計（新舊分開，D44）。本次用這個欄位，不用 D63④ 的 D48 偵測器增量；
  - D44 領先跳升（凍結的 `lead_jumps.py`，新舊分開累計）；
  - Tick 間隔（resolved 時刻 ≥ step＋1.5 ms）；
  - `legal_match_p95_ms`／`legal_client_p95_ms`（受 probe 幀相位影響，不跨建置解讀）；
  - 模擬角色晚醒；主機的各項。
- **B**：
  - 點擊、送出、本機擋下、都不是、ambiguous；前一發裁決是否已到；
  - 送出→裁決（P50／P95／最大）；送出點擊的間隔（Tick）；接受的射擊之間 `resolved_tick` 的最小差；
  - 最長幀、GUI 計時器、sleeper；Gateway 暫停（pause）。
  - **被釘住秒數與 late 修正數在 B 量不到**：GUI probe 沒有命令紀錄（`clients-commands.jsonl`）。
- **R**：上面 A 的記錄項目（領先跳升、被釘住秒數、late 修正數、Tick 間隔、c 的分布、非 respawn 重設）。
- **給使用者的旗標**（不停下，報告時列出）：
  - D44 提前條件：同一跑次有領先跳升與 Cooldown（A、R）；
  - D61 的事後規則（R）。
- **只記錄、不立旗標**：before 的矩陣跑次的領先跳升、late 修正、LifeRespawn 以外的 epoch 重設（`d44_old_tree_record_only`；理由見 U9）；before 的連點出現 Cooldown 拒絕（D78 U4）。

#### 預測

可被推翻；不參與判定。

- P1（A）：after 的 clean-30 與 clean-60 收斂：12／12 通過，Client 替代 ≤2 筆／輪（C1 第 2 次：v7 c30 2／20125、c60 0）。
- P2（A）：機器夠安靜時主要落在 4 ms 狀態，8 ms 狀態大概又是「未驗證」（C1 第 2 次 51／54、S1 12／12 在 4 ms 狀態）。
- P3（A）：before 的 clean-30 替代比率 ≥0.15%，對照重現（C1：4 ms 狀態 0.570%）；C2-A 的結論是「通過（4 ms 狀態）」。
- P4（A）：
  - 移動「產生→執行」的 P50：after 約 37.8 ms、before 21～25 ms（第 04 批的已知位移）。
  - c：after 全部落在第 0 個 Tick，before 分散在 0～5（S1）。
  - before 的 late 修正：C1 的 v6 27 次中有 4 次、共 6 個（U9）。
- P5（B）：after 的 (i)(ii) 在三種 FPS 都是 Cooldown 0（S5 開發跑次：60 FPS 送出 44 與 31 發，0）。edge 在 60 FPS 約 254 次本機擋下。(i) 的送出間隔：60 FPS 11.26～12.59 Tick；30 FPS 約 10～14 Tick（範圍限定 FPS）。
- P6（B）：送出→裁決的中位數 after 約 17.6 ms、before 約 53～69 ms（S5）。
- P7（B）：
  - (iii) interval10 會有本機擋下（幀量化讓部分點擊早於 10 Tick）；after 的 Cooldown 仍是 0。
  - (iv) 非 clean，Cooldown 可能不為 0。09a 修好的是送出被釘住，但 GUI 沒有命令紀錄，無法直接確認。
- P8（R）：
  - 25／25 通過；Gateway 兩個最短間隔都是 33.4；relay 每秒 ≤30。
  - 1000 ms 故障案例的重設全部在故障窗內，分布和 `b205615` 相同（D61 的兩條規則都不觸發）。

#### 門檻的來源（D45）

每個門檻的來源建置、指標，以及為什麼適用。「跨建置」表示把在別的建置上定出來的值，用在本次的建置。

| 門檻 | 值 | 來源 | 指標 | 為什麼適用／限制 |
|---|---|---|---|---|
| TimerBaseline 主狀態 | `late_p99_ms` ≥6.0 | C1 `05:41`、第 14b 批（v6 probe） | action probe 連線前 3 秒，`sleep_until` 絕對期限晚醒的 P99（約 180 樣本中的第 2 大） | 兩棵 tree 的量法逐字相同。**違反 probe 的標註**（never a threshold or denominator），由決定 8 核准。背景負載有部分影響：S1 的 `late_p50` 6／6 分得開，`late_p99` 在 4 ms 狀態分不開；Fisher 的停止偏保守 |
| TimerBaseline 次狀態 | `interval_p99_ms` ≥22 | 第 08b 批 L2 `judge.py:59` | 同一次量測 | 只列出不一致 |
| sleeper 狀態 | P99 ≥6.0 | S1 宣告；第 08b 批 L2 的 sleeper 雙峰（4 ms 狀態 3.87～4.18、8 ms 狀態 6.42～8.24 ms） | 獨立程序的 1/60 秒絕對期限 | A：只判轉換；B：附在結論的狀態組成 |
| 閒置閘門 | 最大 <10 ms | C1 第 2 次 session（使用者核准） | 同上 | 與建置無關 |
| Match 停頓 | 間隔 ≥40 ms，0～3 Tick | C1 `05:48`（v6 與 P1b 的 Match） | Match 的 `snapshot_produced` | before 的 Match 與 C1 相同；after 的 Match（第 07 批之後）是跨建置使用，所以第 2 後半、第 3 條兩種方式都算 |
| C1 的比率常數 | 0.05%、0.15%、≥5 筆×≥2 輪、≥3 輪、≤12 輪、−1／N、2×、÷3 | C1 宣告（`05:61-66`） | Client 替代÷原始命令 | 規則的形狀，不是分布；沿用 C1 的理由（0.05% 約每輪 0.84 筆的地板） |
| before 重現 v6 | P50 中位數在參考包 ±3 ms；clean-60 失敗 ≥2 輪 | 參考包 `pvp-v6-30fps-reference-20261007`（v6 最終建置）、C1 | 移動 P50；狀態＝Match `host_late_p99` | 同一建置（before＝v6final）、同一個分類法；算不出 `host_late_p99` 就停下（C1） |
| Fisher | p<0.05 | C1 `05:43`、計畫 | TimerBaseline 的 8 ms 頻率 | 檢定兩棵 tree 的主機狀態是否偏移 |
| Cooldown | 0 | P2 停止條件、v6 第 12 批 | 權威的拒絕代碼 3 | 不是分布；出現任何一次，就表示閘門放行了權威拒絕的射擊 |
| 連點的輪數與發數 | ≥3 輪、送出 ≥40 發 | 計畫決定 7（「點擊」改讀為「送出」：本機擋下不可能被權威拒絕） | P 跑次 | 檢定力很低：每格約 90 發送出、0 次拒絕時，95% 上限約 3.3%／發；只能說「沒有看到」（v6 第 12 批 L2 也只是一致性確認） |
| 受干擾的停止 | 3 次 | 本宣告新訂〔提議〕，D78 U7 核准 | `disturbed`（只抓視角變化） | 1～2 次可能是意外碰到；3 次表示桌面沒有保持無人狀態。滑鼠按鍵是 A 類 |
| relay 結果 | 任一秒 ≤31 | 第 08b 批判定 3 | relay 下行 kind 7 的接收時刻 | 產品的上限是 30＝floor((1 s−1 ns)／I)＋1（08b `:270-279`），31 是加 1 的時間壓縮餘裕（08b `:38`）。乾跑 `b205615` 的最大值是 30 |
| Gateway 最短間隔 | 印出值 ≥33.3 | 第 08b 批判定 2、5；I＝1/30 秒（`action_delivery.go:16`） | Gateway 統計行（`%.1f`） | 產品的設計值；乾跑 25 案都是 33.4 |
| D44 重設的即時規則 | 故障窗 (start, release＋1.5 s] 之外，或沒有故障窗時出現 | D61（08c L2 宣告；從 08b 放寬） | Match 的 reset 事件 | 25 案一律適用，沒有豁免。乾跑 `b205615`：upstream-1000ms 的 backlog 在 +1.547 s、所有 starvation 在 +0.53～1.98 s，全在窗內 |
| D44 重設的事後規則 | 基準沒有的原因／位置，或次數比基準多 | D61；基準＝`b205615` 的 D72 矩陣 | 同上 | 基準和 after 的產品程式相同（只差註解），不是跨建置。每案 1 個跑次，所以只立旗標 |
| 被釘住 | 每秒中位數 >2 ms | 計畫 `:57`、`:113`（第 08b 批之後的 worker） | 非 seed 移動命令的 sent−generated | 只記錄；before（v6 worker）照列，等待中位數本來就約 1.4～2.0 ms，會誤判 |
| late 修正 | 次數 | `start_phase_evidence.py`（兩棵同為 `e719dab6`） | Client 的相位追蹤紀錄 | 只記錄；兩棵 tree 分開累計 |
| Tick 間隔 | step＋1.5 ms | M0 的停頓定義（計畫 S3） | Match `resolved` | 只記錄；舊 Match 上無效（S1 試算 1324 個） |
| 領先跳升 | gen→exec 中位數 +20 ms | `lead_jumps.py`（第 08 批 L2 的 trace，v7） | Client 與 Match 的命令紀錄 | 用在 v6 的 trace 是跨建置（v6 含意圖年齡），before 只記錄（U9） |
| 逾時 | 遊戲 900 s、連點 300 s、背壓 1200 s、網路 900 s | S1（900 s）；連點＝runner 自己的 100 s 期限＋啟動 | — | 超過就是 E |

#### 證據

`build/target/_build/test/logs/pvp-v7-09-c2-prep-20261014/`（準備）：

- `declaration-draft.md`（本檔）、`run_c2.py`、`judge_c2.py`、`sleeper.py`、`build.sh`（`build.sh.diff`）、`prep_hashes.py`、`digest.sh`、`artifacts.sha256`、`analyzers.sha256`、`prep.json`、`build-c2-after.log`、`prep.txt`、`digest/`、`xhigh-d45-check.md`、`commands.txt`、`sha256.txt`。
- `dryrun/`：`make_mock.py`、`driver_mock.py`、`fake_runner.py`、`tamper.py`、`run_tampers.sh`、`check_tampers.py`、`mutate.py`、`compare_s1.py` 與輸出（見「乾跑」）。

正式 session：`build/target/_build/test/logs/pvp-v7-09-c2-<日期>/`（`progress.jsonl`、`runs/`、`files.sha256`、`judgement.json`）。

#### 乾跑（準備時做的，不是證據的一部分）

腳本與輸出在 `dryrun/`（不凍結）。模擬 session 需要 prep 目錄的 `declaration.md`，準備時暫時用本草案的複本，用完移到 `dryrun/mock-declaration.md`（雜湊在 `dryrun/mock-declaration.sha256`）；正式的 `declaration.md` 在核准時才建立。

- **C2-A 對 C1 第 2 次**（`make_mock.py c1`，`c1-as-a.judgement.json`）：
  - 結論「通過」（4 ms 狀態，條件 1、2a、2b、3、4 成立；8 ms 狀態未驗證），與 C1 的判定相同。
  - 差別只在分層：TimerBaseline 把 after 的 clean-30 一輪分到 8 ms 狀態（C1 用 Match 分類是 4 ms），所以 after c30 的 4 ms 格是 11 輪（C1 12 輪）。
  - 兩種比率：after c30 排除 0.011%、不排除 0.033%；v6 c30 0.570%／0.635%。Fisher（基本輪數）p＝1.0。
  - late 修正：before 6（4 跑次）、after 0（U9）。
- **記錄項目對 S1**（`make_mock.py s1`、`compare_s1.py`、`compare_s1.txt`）：12 跑次×12 項，**144 相同、0 不同**。12 項是：通過、替代、Match 停頓替代、停頓重設、Cooldown、c 的整 Tick 分布、a、被釘住秒數、領先跳升、Tick 間隔、狀態、Match `host_late_p99`。
- **C2-B 對 S5 開發跑次**（`make_mock.py s5`）：
  - 4 跑次都是 P、Cooldown 0。
  - 送出→裁決中位數 after 17.6、before 52.9／69.4 ms，與 S5 報告相同。
  - (i) 的送出間隔：after 11.26～12.59、before 11.29～12.63 Tick。
  - 只有 1 輪、只有 60 FPS，所以結論是「未驗證」，規則正確地這樣報。
- **回歸對 09ab 矩陣（`b205615`）＋第 08b 批的背壓與網路開發跑次**（`make_mock.py reg`，`reg-as-r.table.txt`）：
  - 25 案 P；Gateway 兩個最短間隔全部 33.4；relay 每秒最大 30。
  - D61 的即時規則 0 次、事後規則 0 次（基準就是同一份資料）；回歸通過。
- **執行器的模擬**（`driver_mock.py`）：runner 換成連結既有證據的 `fake_runner.py`，sleeper 換成常數；雜湊、worktree、主 checkout、遠端 PR 分支（真的 `git ls-remote`）、宣告雜湊與分類照真的做。
  - 正常：189 項（A 基本 30＋補跑到第 12 回 24＋B 108＋R 27；C1 的資料有 8 ms 狀態，所以補跑到上限），結束碼 0。
  - 用它做的 `mock-full` 經 `judge_c2.py`（真的字面值）：**結束碼 0**，完整性 0 問題，C2-A、C2-B、回歸都通過，旗標 0。
  - 停止的 5 種都會停下並寫出 `files.sha256`：
    - after 的 edge-30 注入 1 次 Cooldown：第 4 項停下，記 M0 觸發；
    - 第 3 項沒有判定行：E 停下；
    - `STOP-REQUEST`：第 5 項後停下；
    - after 的 HEAD 不符：開始前停下；
    - 主 checkout 在第 4 項執行中出現非文件變更：第 5 項之前停下。
  - 停下的 session 經 `judge_c2.py` 報告 `stopped` 與 M0 旗標。
- **負向測試**（`tamper.py`、`run_tampers.sh`、`check_tampers.py`、`neg/results.txt`）：**29 種竄改全部被拒，而且理由正確**。
  - 完整性：未完成、停止紀錄、排程順序、`DRIVER_SHA`、`files.sha256` 過期、清單外的檔案、分類不一致、少跑補跑回、三個字面值（兩個清單、宣告）、主 checkout 的非文件變更、遠端 PR 分支。
  - 停止與判定：矩陣 Cooldown、連點 Cooldown、join 的 Cooldown、逐筆歸屬多於計數（A 類）、受干擾 3 次、停頓重設、Gateway 間隔、relay 每秒 32 以上、D61 即時規則（無故障窗的重設）、D61 事後規則（只立旗標、不停）、Fisher、before 重現（clean-60 失敗 2 輪）、沒有 TimerBaseline、回歸 clean 的 Cooldown、逐筆紀錄不一致（A 類）、送出不足 40 發（點擊夠多）。
  - 正向對照 2 種：刻意注入的 Cooldown 被排除（結束碼 0）；(iv) pause 的 Cooldown 只記錄（結束碼 0）。
- **突變測試**（`mutate.py`、`neg/mutations.txt`；D45 檢查 SHOULD 11）：10 個突變中 8 個被殺。
  - 被殺：relay 31→99、刪 D61 的故障窗規則、刪 join 計數、刪 before 重現的檢查、刪 TimerBaseline 缺漏的停止、刪回歸 clean 的 Cooldown 停止、刪主 checkout 的兩處檢查（同時刪）、送出 40→點擊 40。
  - 存活 2 個，原因都已確認：
    - `max(counter, attributed)`→只用 counter：等價突變。逐筆歸屬多於計數已經是 A 類（竄改 `attributed-over` 被拒），所以 max 不會改變結果。
    - 只刪主 checkout 的逐項檢查：開始與結束紀錄的檢查仍然擋下；兩處都刪就被殺。
- `run_c2.py --list-only`（`list-only.txt`）與 `--preflight`（真的字面值；遠端 PR 分支 `51cf1e4`，只有文件）通過。

#### 和計畫不同的地方

1. **B 的主機狀態不分層**：GUI probe 的計時器以幀起點為準，TimerBaseline 的 6.0 ms 不適用；連點只記錄 sleeper。C2-B 的判定本來就不分層。
2. **25 案逐案執行**：一次跑完整矩陣時，`run_gameplay.py` 在第一個失敗就停（`if not result['passed']:break`），後面的案例會沒有紀錄。逐案執行也讓每一案之前都能核對雜湊與 worktree。
3. **第 2 後半與第 3 條的比率兩種方式都要成立**：計畫只說「兩種方式報告」，沒有說用哪一種判定（D78 U2）。
4. **回歸的 D44 重設規則改用 D61**（D45 檢查後改寫；取代 D78 U5）：
   - 計畫寫的是「照 08b 的 D44 規則」。但 08c L2 已核准 D61，把 08b 的「故障案例出現 backlog 就停」拿掉，保留故障窗外的規則。
   - 本宣告 25 案一律用 D61 的即時規則，再加上以 `b205615` 為基準的事後規則。
   - 前一版草案把 1000 ms 案例的兩條規則都豁免。那是看了乾跑結果才定的（循環），也沒有設計文件可引，已撤回。
5. **刻意注入的排除是空的**：計畫要明文排除 action short 的那 1 次（v6 dev_log:69）。本次只跑連點模式，它不注入；排除照樣寫在 judge（以 action ID 排除），結果是 0。action short 的預設矩陣不跑（計畫的 B 只有 (i)～(iv)）。
6. **after 的 (iii) 出現 Cooldown 也停下**（D78 U3）。
7. **before 的連點全部只記錄**（D78 U4）。
8. **before 重現的停止判斷用舊 Match 的分類**：主分類改成 TimerBaseline，但參考範圍是用 Match `host_late_p99` 定的。
9. **分析器 v7 兩棵 tree 都用 after 的那一份**（C1 的做法）；v6final 沒有這個檔。
10. **runner 的工作目錄改為 `runs/`**（C1、S1 是 tree 的根目錄），讓唯讀的 v6final 不被寫入。`git status` 以 `--no-optional-locks` 執行，不更新 v6final 的 index。
11. **新增 D 類（受干擾）與累計 3 次的停止**（D78 U7）。
12. **連點的樣本以送出的發數計**（計畫寫「點擊」）。
13. **非文件變更的檢查**：計畫寫「有任何非文件的變更，該 session 作廢」。本宣告把它具體化成：主 checkout 對 `d685d1e` 的差異，與遠端 PR 分支（見「主機」）。
14. **late 修正數用 `start_phase` 的欄位**，不用 D63④ 的 D48 偵測器增量（兩棵 tree 都有、而且凍結）。
15. **12 Tick 的定義**照 D77 (b)；(ii) 的「5 秒」是點擊時間的累計（S5）。實際間隔的數字已更正。
16. **earliest 不量**（GUI 取不到；M0 在 v7.1），所以拒絕的定位沒有 R−E；B 的定位也看不到 Match 停頓（Match 沒開 `--movement-trace`）。

#### 需要使用者決定的地方

D78（2026-10-14）已核准 U1～U8；以下保留原文的要點。D45 檢查之後有兩項要再請你看：**U5 改寫**與**新增的 U9**。

- U1 一次跑完，順序 A → B → R（D78 核准）。
- U2 Match 停頓替代的比率，排除與不排除兩種都成立才算；不一致交使用者（D78 核准）。
- U3 after 的 (iii) 出現 Cooldown 也停下，是否補 M0 由你決定（D78 核准）。
- U4 before 連點的 Cooldown 只記錄（D78 核准）。
- **U5 改寫（D45 檢查，依 D61）：回歸的 D44 重設規則**
  - 原核准（D78）：08b 的規則只用在 250 ms 故障與無故障案例，1000 ms 只記錄。
  - 建議改為：25 案一律用 D61 的即時規則（故障窗 (start, release＋1.5 s] 之外的非 respawn 重設就停；沒有故障窗的案例出現就停；不再有「故障案例出現 backlog 就停」）。另加 D61 的事後規則：以 `b205615`（D72）為基準，逐案比較「原因×窗內／外」，出現新的原因、位置或次數增加就立旗標交你判斷。
  - 機制：
    - 原建議的 1000 ms 豁免連故障窗外的重設也不查，而且是看了乾跑結果才定的（循環）。
    - network0／20／40（約 4 秒窗）、drain-stall（1.3 秒）、cross-life（4.5 秒）既不是 250 ms 故障，也不是無故障，原規則沒有涵蓋。
    - D61 是 08c L2 已核准的規則，和回歸的對象同一條線。
  - 證據：
    - `b205615` 的 1000 ms 重設全部在窗內：upstream-1000ms 的 backlog 在 +1.547 s，starvation 在 +0.53～1.98 s。所以改用 D61 之後，乾跑照樣通過（`dryrun/reg-as-r.judgement.json`）。
    - `xhigh-d45-check.md` MUST-FIX 1；`08c-drop-report.md:586`、`:655`、`:671`；HANDOFF D61。
- U6 v6 的領先跳升只立旗標，留給第 16 批（D78 核准）。**事實更正**見 U9；本宣告改為只記錄，不立旗標。
- U7 受干擾的跑次記 D 類，不重跑，累計 3 次停下（D78 核准）。另外請注意：按下滑鼠按鍵會變成 A 類，立刻停下。
- U8 relay 門檻維持 ≤31（D78 核准）。理由更正：產品的上限是 30＝floor((1 s−1 ns)／I)＋1（08b `:270-279`），31 留 1 給 relay 端的時間壓縮。
- **U9（新增；事實更正）：D44「舊版也出現」在 C1 的資料就已成立**
  - 事實：
    - S1 的 D44 提前條件寫著「舊版（v6）也出現 → 停下問使用者」。
    - D45 檢查發現，C1 第 2 次（2026-10-09）的 v6 跑次就有真的 late 修正：clean-30 的 r2、r3、r4 與 clean-60 的 r8，共 6 次。
    - 其中 clean-60 r8 那次領先跳升（玩家 2、epoch 1，gen→exec 22.6→43.4 ms），同一位玩家的 `late_corrections`＝1。
    - 所以「舊版也出現」在 C2 之前就已成立。前一版草案（U6）只把它當成 `lead_jumps.py` 跨建置的可能誤報，這個判斷不對。
  - 建議：記錄這個事實；D44 的整體回歸維持在第 16 批，修法維持 v7.1 的候選 A3；**不停下 C2**。C2 對 before 的領先跳升、late 修正與非 respawn 重設只記錄、不立旗標，after 照常記錄並和 before 並列。
  - 機制：
    - D44 提前條件的用意，是確認 D44 不是 v7 才有的退步。v6 也出現，表示它是既有行為（late 修正機制在 v6 就有）。這會改變 D44 的「性質」判斷，不會改變 C2 要回答的兩個問題（30 FPS 的收斂、閘門是否被權威拒絕）。
    - C2 的停止條件仍然涵蓋真正危險的組合：同一跑次有領先跳升與 Cooldown（旗標），以及 Cooldown 本身（停下）。
  - 另一個選擇：照 S1 的字面停下，先調查 D44（把第 16 批的 D44 部分提前），之後再跑 C2。代價是 v7 收尾延後；而且以目前的證據，調查的結果不會改變 C2 的判定規則。
  - 證據：`dryrun/c1-as-a.judgement.json` 的 `d44_old_tree_record_only` 與 `late_corrections_by_part_tree`（before 6、after 0）；`xhigh-d45-check.md`「事實更正」。

#### 執行步驟（核准後）

```
### 字面值一律抄宣告，不從檔案重算
P=build/target/_build/test/logs/pvp-v7-09-c2-prep-20261014
cp $P/declaration-draft.md $P/declaration.md          # 核准的版本；它的 sha256 就是 DECLARATION
FROZEN=680d74614db3027a65288684bccc691fe60c57887a43c886b7ffb7638b4b9280
ARTIFACTS=d7083fd9af23618e0e6e1ba56becb621d4ebb8cc5387eca432ba806cc7bec262
DECLARATION=<核准時 declaration.md 的 sha256>
python3 -I -B $P/run_c2.py --preflight --frozen-sha $FROZEN --artifacts-sha $ARTIFACTS --declaration-sha $DECLARATION
caffeinate -dims python3 -I -B $P/run_c2.py --output build/target/_build/test/logs/pvp-v7-09-c2-<日期> --frozen-sha $FROZEN --artifacts-sha $ARTIFACTS --declaration-sha $DECLARATION
python3 -I -B $P/judge_c2.py --session build/target/_build/test/logs/pvp-v7-09-c2-<日期> --frozen-sha $FROZEN --artifacts-sha $ARTIFACTS --declaration-sha $DECLARATION
```


## C2 結果（2026-10-14 19:39～21:03，commit H `a6127cc`；**依宣告 `passed`＝true**）

- session：165 項全部完成，沒有停止、沒有完整性問題、沒有使用者旗標；主 checkout 與遠端 PR 分支在 session 期間只有文件（開始與結束都核對）。PR #73 的 auto-fix 在 session 期間暫停，結束後恢復。受干擾（D 類）0 次。
- **C2-A（30 FPS 對比矩陣）：通過。** 主機：兩棵 tree 的 24 次全部在 8 ms 狀態（TimerBaseline），Fisher p＝1.0，sleeper 沒有轉換；4 ms 狀態沒有樣本，標「未驗證」。補跑 0 項。

  | 8 ms 狀態 | before（v6final `fee92ff`） | after（`d685d1e`） |
  |---|---|---|
  | clean-30 通過 | 1／6 | 6／6 |
  | clean-30 替代（Held＋Neutral，排除 Match 停頓） | 109／10045（1.09%） | 0／10062 |
  | clean-60 通過、替代 | 6／6、0／10053 | 6／6、0／10061 |
  | clean-144 通過、替代 | 3／3、0／5028 | 3／3、0／5031 |

  Match 停頓造成的替代兩棵 tree 都是 0，所以排除與不排除兩種比率相同（D78 U2）。v6 的 clean-30 在 8 ms 狀態失敗是 C1 已知的現象；v7 在同一狀態 6／6 通過，替代 0。
- **C2-B（GUI 連點，經過真正的 FireGate）：通過。** after 的判定格（12 Tick 與冷卻邊緣，30／60／144 FPS，各 3 輪）18 次全部 P，**權威 Cooldown 拒絕 0**（12 Tick 每格送出 132 發；冷卻邊緣送出 92～96 發，點擊 432～1954 次、本機擋下 340～1858 次）。刻意注入的排除 0 件。判定跑次的 sleeper 狀態：8 ms 13、4 ms 5。
  - 只記錄：after 的 10／11／13 Tick 與暫停後連點 Cooldown 也都是 0。before 有 3 次 Cooldown（interval10-60、interval10-144、pause-144 各 1，D78 U4 只記錄）。
  - 送出→裁決的中位數：after 30 FPS 約 34 ms、60 FPS 約 18 ms、144 FPS 約 15 ms；before 各 FPS 約 52～72 ms（含 1 次 102 ms）。
  - 檢定力：每格 0／92～132 發，95% 上限約每發 2.3～3.3%；這是「沒有看到」，不是比例的確認（宣告「判定」）。
- **回歸（25 案＋backpressure 6 案＋network）：通過**，D61 規則下即時與事後的問題都是 0。
- **只記錄**：late 修正 A／before 2 次（clean-30-r4）、A／after 0、R／after 0；D44 舊版的領先跳升 0、非 respawn 重設 0（D79 U9）。
- **M0 的條件式補做（D75）：沒有觸發**（C2 的 clean 案例沒有權威 Cooldown 拒絕）。
- 證據：`build/target/_build/test/logs/pvp-v7-09-c2-20261014/`（`progress.jsonl`、`judgement.json`、`files.sha256`、`runs/`）；執行與判定的 log 在同層的 `pvp-v7-09-c2-20261014.run.log`、`.judge.log`。

## S1 結果（2026-10-10，只記錄）

- 宣告 2026-10-10 使用者核准（「S1 宣告核准，開始跑，檔位照建議」）；`declaration.sha256`＝`9e0869a0…`。開跑前 preflight 通過；量測期間暫停 PR #73 的 auto-fix、背景的 D48 分析也先停下，跑完後恢復。
- 12 次全部完成，全部是 P（凍結分析器與分析器 v7 都通過），沒有觸發停止條件。主 checkout 的 HEAD 開始與結束相同（`fcda6e0`），兩個 worktree 沒有變動。
- 主機：12 次全部是 4 ms 狀態（TimerBaseline），沒有 8 ms 的樣本，所以兩種狀態的比較這次沒有資料。
- 只記錄的數字（每棵 tree 6 輪，clean-30）：

  | | P1b 頭 `805bd11` | P2 頭 `a954aa3` |
  |---|---|---|
  | `legal_client_p95_ms`（每輪） | 100.0～133.3 ms（3～4 幀） | 35.5～37.8 ms（約 1 幀） |
  | c 落在第幾個 Tick（0／1／2／3／4／5） | 18／68／48／34／26／10 | 204／0／0／0／0／0 |
  | 移動「產生→執行」每位玩家 P50 的中位數 | 37.9 ms | 37.8 ms |
  | Client 替代（Held＋Neutral） | 0／10062 | 0／10062 |
  | 被釘住的玩家秒 | 1（1 個跑次） | 0 |
  | 領先跳升、非 respawn 重設、Cooldown 拒絕 | 0 | 0 |

- 讀法：
  - Client 看到的動作延遲（產生→probe 取得裁決的 P95）從 3～4 幀變成約 1 幀，這是 P2 線整體（第 06、07、08、08b 批）的效果。這次量測無法在 P2 線內部再分開歸因。
  - c（裁決 Tick→結果到 relay）在 P2 頭全部落在 1 ms 內，在 P1b 頭分散在 0～5 個 Tick：舊的兩條 30 Hz 節拍加上「每隔一次 ticker」的缺陷。
  - 移動的「產生→執行」兩邊相同（約 37.8 ms），符合第 09b 批的分解（領先 2 步＋目標 4 ms，與網路路徑無關）。
  - `legal_match_p95_ms`（P1b 7.1～21.6、P2 13.2～15.2 ms）受 probe 幀相位影響，不跨建置解讀（第 08b 批）。
- 證據：`build/target/_build/test/logs/pvp-v7-batch09-s1-20261010/`（`progress.jsonl`、`judgement.json`、`judge.stdout.txt`、`files.sha256`、`runs/`）。

## 本批另加的停止條件

1. M0 的任何一條門檻不成立：推導不成立，停下。（更正，2026-10-14，D75：M0 移到 v7.1，本條在 v7 不適用；只在條件式補做 M0 時適用。v7 的對應停止條件是 C2 的 clean 案例出現權威 Cooldown 拒絕。）
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
4. **M0 影子量測**：凍結前在常數變更前的頭做（建議要做；README 允許）。門檻與窗口照「步驟」S3。（更正，2026-10-14，D75：M0 移到 v7.1，只在 C2 的 clean 案例出現權威 Cooldown 拒絕時補做；v7 的凍結改成 S4-lite。）
5. **（2026-10-10 使用者更正：在 P2 處理，排在 09a 之後、M0 與凍結之前，暫稱第 09b 批）相位追蹤的既有缺陷**（HANDOFF「C1 之後要決定的事」：settling 期間的舊樣本與 late 樣本、延遲位移約 38 對 21～25 ms）：要不要在第 09 批處理。屬重推條件 1，若處理必須排在 M0 與凍結之前，否則凍結後馬上要重推；可能與 D44 的 late 修正有關（hypothesis）。建議：本批不處理，維持「另立批次」，並在重推條件寫明；但若你希望先處理，就排在 09a 之後、M0 之前。
6. **probe 相位**：(A)【建議】矩陣、回歸、30 FPS 記錄都不隨機化；只有 M0 的每段起點用 seed 偏移；GUI 連點本來就會漂移。(B) 矩陣也隨機化：會失去與 C1、06～08b 的可比性。（補充，2026-10-14，D75：M0 移到 v7.1，「M0 的每段起點用 seed 偏移」只在補做時適用，且草案 v2 已改為每發忙等控制 s。）
7. **C2 連點的內容**：(A)【建議】(i) 12 Tick（判定）、(ii) 冷卻邊緣連點（after 判定）、(iii)(iv) 只記錄；earliest 只在 M0 量。(B) 只做 12 Tick（與 v6 第 12 批相同）：對 1 Tick 的下界錯誤沒有檢定力，不建議。（補充，2026-10-14，D75：M0 移到 v7.1；C2 的判定不參照 earliest，不受影響。）
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
