# 第 09a 批：輸入送出被釘住（Client token bucket）

狀態：**設計完成（草案），實作前停下：D44 的提前條件**（2026-10-10）。PR 線 P2（PR #73）。排在第 09 批 S1 之後、第 09b 批之前（D47①）。
檔位：設計 high（只讀）＋1 次 xhigh 對抗式檢查；實作 high，計時、期限與 token 的局部 xhigh（D47⑪）。

依使用者的要求（memory「decision points for learning」），每一節寫出機制、為什麼重要、選項與代價、證據在哪裡，以及被推翻的說法。

## 機制：為什麼 token 補不回來

- **規則**（`ClientConnection.cpp`，自 `730984e` 起未改）：
  - 補充：`inputTokens = Min(InputSendBurst, inputTokens + Δt/period)`，period＝1/60 秒（`:595-598`；`Movement.hpp:22`、`:28`：`InputSendRate`＝60、`InputSendBurst`＝2）。
  - 決定：`(fresh && inputTokens>=1) || (now>=nextInputSendAt && inputTokens>=InputSendBurst)`，送出就扣 1（`:601-605`）；重送期限 `nextInputSendAt＝sentAt＋period`（`:628`）。
  - 新命令的喚醒時刻是 `refill(1.0)`（`:672-682`），最後一段由 `CoarseWakeLead`＝2 ms 的 kqueue 等待接手（`:698-718`）。模擬角色每個固定步發布一次（`ClientSimulationRole.cpp:179`）。
- **推導**：令 x 為命令產生當下的 token 數。產生間隔正好是 1/60 秒時，x≥1 就立即送出，下一個命令時又是 x；x<1 就在 token 到 1 時送出、歸 0，下一個命令時還是 x。**補充率等於產生率，所以 bucket 是中性穩定的：沒有任何力量把 x 拉回 2。** 送出的晚醒只改變送出時刻、不改變 token 數，所以格點也不動。
- **什麼會改變 x**（對抗式檢查更正）：只有產生格點移動時（相位修正、停頓後的追趕），以及上限截斷與 x≈2 時的重送競態。發布本身的晚醒會相消。往前修正讓 x 下降，往後修正與視窗合併（折返）讓 x 上升。
- **一旦 x<1**，送出就固定在一組間隔 1/60 秒的格點上，命令也是 60 Hz 產生，兩組格點的相位差 W＝(1−x)/60 秒一直維持，這就是 11～16 ms 的「被釘住」。
- **證據**（`build/target/_build/test/logs/pvp-v7-09a-design-20261010/`）：
  - 所有 clean 跑次的 x 中位數都 <2（最大 1.881；08b clean 1.04～1.88，08-2 clean 0.408～1.876），也就是穩態就停在任意的中性水位（`bucket_census_summary.txt`）。
  - 08-2 clean-30 r10 after 玩家 1：6.6 秒的一次停頓之後 x＝0.330，等待 11.19 ms，送出相位固定在約 3.0 ms，一直到 16 秒（`timeline_08-2_clean30_r10_after_p1.txt`）。relay 的上行每秒 60 個、client→relay 中位數 0.079 ms，所以延遲在 Client 端。
  - 08b gateway-250ms r6 after 玩家 1：100 ms 的停頓與往前修正後，x 依序 1.17→0.775→0.575→0.373→0.183，等待固定在 13.7 ms（`timeline_08b_gw250_r6_after_p1.txt`）；合成模型能逐步重現（`synth.txt` S1）。
- **v6 呢**：規則的行相同（v6final 的 `:549`、`:556`、`:580`）。但 v6 每個畫面幀發布一次：30 FPS 每次發布帶 2 個新命令，bucket 一直頂在上限，結構上不會被釘住（C1 的 v6 clean-30 36 個玩家跑次 0 秒）；60 FPS 很少（2 秒）；144 FPS 發布被量化到 6.9 ms 的幀格，經常被釘住（128 個玩家秒，約 3 ms）。v7 由角色以 60 Hz 發布、與 FPS 無關，所以每個 FPS 都暴露。

## 為什麼重要

1. **FireGate**：命令晚送、射擊不晚，「同路」前提失效，射擊可能早 1 Tick 被判定（第 09 批文件「常數涵蓋不了的失敗」1）。
2. **移動延遲**：被釘住期間輸入多晚 11～16 ms，也會污染 C2 的移動指標。
3. **D44（實作前要先停下的原因）**：被釘住時，權威端的待處理命令從 2 掉到 1（r10 p1 的 7～13 秒，`queued_by_second.txt`），約 3.6～4 秒後相位追蹤往前修正、折返，佇列回到 2。被釘住「吸收」了相位修正與 late 修正多出來的領先。第 09b 批的對抗式檢查把真正的 late 修正重新分類：被釘住的 4 次沒有重設，沒被釘住的 1 次就是 D44。同一跑次的 r6 玩家 2 有同樣的停頓，但沒有往前修正、沒有被釘住，佇列一直是 2（`adversarial/r6_p2_natural_experiment.txt`），是自然對照。修好之後，late 修正會實際移動到達時刻、不再被吸收，可能讓傳輸型 late 修正更常變成 backlog 重設（hypothesis；修正事件沒有記錄在 trace 裡）。這符合第 09 批文件的 D44 提前條件。

## 修法的選項

共同前提：不改 wire、不改 Gateway、不改 FireGate。評估用開環回放（`sim.py`，現行規則的驗證誤差中位數 r6 0.39 ms、r10 1.39 ms）與確定性情境（`synth.txt` S1～S6）。

| 選項 | 內容 | 代價 | 判斷 |
|---|---|---|---|
| A 全域加快補充 | 所有水位都以 R 補充 | x 回到 2，重送競態變常見（回放重送 132→7084，封包 +10.7％）；等於所有流量的契約都改了 | 不建議 |
| B 只在 token<1 時加快 | 同上，但限水位 | 送出後幾乎都 <1，R 一直作用（重送 1353 對 132） | 不建議 |
| **C 等待過的送出打折** | 補充率仍是 60／秒、上限仍是 2；新命令必須等待時，那次送出只扣 60/R 個 token | 穩態與現行相同（回放重送 133 對 132，封包 +10／72 個玩家跑次） | **建議**（見下面的修正版） |
| D 提高容量（4） | 仍是中性，沒有修正 | 會連發，違反「突發兩個封包」的契約 | 不建議 |
| E 只在等待期間加快 | 幾何收斂，沒有有限步數 | 不如 C | 不建議 |
| F 寬免（扣 0） | 惡意發布下速率無上限 | 要另加上限 | 不建議 |

**C 的兩種寫法**（對抗式檢查推翻了設計稿「不受 `worker_main:428` 限制」的理由）：
- bool 版（設計稿）：只要等待過，不論等多久都打折 0.25 個 token。從未滿的 bucket（x≈1.99）開始時，送出 i 與 i−2 的最短間隔只有 12.70 ms（<14 ms），和 A、B 在 R＝80 的最壞值相同（`adversarial/check_rule.txt` B）。原因：只等了 0.17 ms 的送出也拿到整整 0.25 個 token。要用就得改寫 `Movement.hpp`「突發兩個封包」的契約為「任三個送出至少間隔 1/R」，並加一段從未滿 bucket 開始的檢查。
- **與等待時間成比例的折讓【建議】**：記下第一次看到「新命令且 token<1」的時刻，送出時扣 `1 − min(1−60/R, 等待時間/period)`。恢復剖面、恢復步數、Gateway 上界、worker 序列的結果都與 bool 版相同，而且任何水位下三包間隔 ≥16.70 ms（`check_rule.txt` A～E 的 prop:80）。

**R 的選擇**（對抗式檢查更正了「可改 75」）：往前修正的 slew 期間，產生間隔是 1/60÷1.25＝13.33 ms，也就是 75 Hz（`LocalPlayerPrediction.cpp:17`、`:287-298`、`:383-384`）。R＝75 時等待在整段 slew 都不減少，R＝70 反而增加。所以 R 必須 >75；R＝90 時惡意發布的最壞值 92＋31＋2＝125 超過 Gateway 每秒 120 的上限（`session.go:117-125`）。可選範圍 75<R≤85，**建議 R＝80**（最壞 82＋31＋2＝115）。`PhaseSlewFraction` 改變時 R 要重推。

## 恢復步數（實作前推導，避免循環論證）

- 定義：P＝1/60 秒，R＝80，c＝60/R＝0.75，Δ＝1/60−1/80 秒＝4.1667 ms。
- 等待 W0 之後送出扣 c，下一個命令時 x＝2−c−W0/P；W0>Δ 時還要等 W1＝W0−Δ（晚醒會相消）。歸納：W_k＝max(0, W0−k·Δ)。
- **最後一次擾動之後，連續等待的命令最多 N＝ceil(W0/Δ) ≤ R/(R−60)＝4 個**；等待 >2 ms 的最多 4 個（約 67 ms）。N(R)＝ceil(R/(R−60))：R＝80 為 4、85 為 4、90 為 3。
- 擾動的定義（對抗式檢查更正）：自上次擾動起的累積格點位移超出 ±1 ms（相位修正、停頓、追趕；單一間隔的判定會漏掉 <1 ms 的修正，例如首次決定的 0.5 ms 死區），或命令在 bucket≥1 時仍晚送 >1 ms。往前修正的 slew 期間（每步短 3.33 ms，最多約 10 步）整段都算擾動：R＝80 從 x＝1.05 進入時，等待最多約 3.4 ms。
- 殘餘：恢復後 x 停在 1～1.25，之後往前修正超過 (x−1)P 時仍有 1 個命令等待（不超過修正量，可能 >2 ms），列為 FireGate 的殘餘類別，納入 M0 的窗口。
- 獨立模型確認：R＝80／75／70 分別有 4／5／7 個命令等待，與推導一致（`adversarial/check_rule.txt` A）。

## 測試與突變（實作時）

- `worker_main` 新增確定性的實時測試：閒置讓 bucket 滿；連發 3 個讓第 3 個進入等待；之後每步發布 1 個。斷言：w0≥12 ms；k＝1..3 的等待每步減 4.167±1.0 ms；k≥4 時等待 ≤1.5 ms＋max(0, P−發布間隔)（或只在發布間隔偏差 ≤0.5 ms 時檢查）。寬容值的來源改為 worker probe 自己的指標（mock 收到時刻減 `SendInput` 時刻，L1 重複 30 次）；08b L2 的 p99 0.86 ms 只作參考。
  - 位置：要放在 `:429` 之後、用 78 以上的序號，或者重編後續各段的序號（插在 60 FPS 段之前會讓 `:414` 失敗，插在 `:393` 之前會讓 `:403` 逾時）。
  - 總預算：`:261` 的 ≤120 窗口裡其實沒有輸入流量（`:236` 的 Input(67,67) 在 ack 77 之後是空操作），所以「輸入＋動作＋Hello」的總和沒有實跑測試在檢查。改成尚未確認的 Input(78,78)，或在文件寫明總預算只由 `static_assert(InputSendDrainRate+InputSendBurst+(ActionSendRate+1)+2<=120)` 保證。
- `MovementRecoveryTests` 的模型複本：token 區塊是 `Path::V6` 與 `Path::Product` 共用的，打折只加在 `Path::Product`（V6 要繼續代表 v6final 的規則）。`:539`、`:650` 的 ≤61 計的是日曆秒，可能失敗（回放的滑動窗口是 62～64），改寫的界線要先宣告。這個檔的 Held／Actual 期望值若有變化，就是 D44 互動的訊號，停下。
- 突變（batch `v7-09a`；只在預期的失敗訊息出現時算 killed，對抗式檢查更正了每條的 expect）：`charge-off`（打折改回扣 1）→「did not drain」；`charge-every-send`、`waited-sticky` → w0 斷言的訊息；`drain-70` →「did not drain」；`drain-120` → 只在沒有 static_assert 時宣告；另加 `drain-75` 讓「outlived four commands」也有突變涵蓋。
- 文件同步：`Movement.hpp` 的契約註解（「nothing is repaid」）、`08-client-worker-asio.md:20`、`network-architecture.zh-Hant.md:81`、`:222-223`、第 09 批文件的 S2 與 S3。
- Architecture Delta（小）：`Movement.hpp` 新增 1 個常數 `InputSendDrainRate` 並改契約註解；不增加依賴邊，owner 不變，不改 wire 與 Data Contract。另外，這條規則現在有三份拷貝（Network、Arm、測試模型），屬 code smell；抽成純函式是替代案，需要另外核准。

## 修不到的部分

- 主機或 worker 停頓本身（r10 在 6.6 秒：bucket 1.775 仍晚送 21.9 ms）。09a 只消除停頓之後的 token 赤字。
- 每個 epoch 前 2 個命令約 16.7 ms 的等待，原因未查。

## 需要決定的事

1. **（最先決定，與第 09b 批的決定 1 是同一件事）D44 的提前條件**：09a 會消除「被釘住吸收額外領先」，可能讓傳輸型 late 修正更常變成 backlog 重設。請在 [第 09b 批](09b-phase-tracking.md)「需要決定的事」1 的 (a)／(b)／(c) 中選擇；我的傾向是 (a)：照計畫做 09a，在開發跑次與 L2 用凍結的偵測器記錄每次 late 修正後的 30 Tick 合計，比例升高就停下。
2. **修法**：C 的「與等待時間成比例的折讓」（建議），或 bool 版加上契約改寫。
3. **R**：80（建議；範圍 75<R≤85）。
4. **`worker_main` 的總預算**：把 `:236` 改成尚未確認的 Input(78,78)，讓實跑測試同時看到輸入與動作（建議）；或只靠 `static_assert`。
5. **bucket 規則抽成純函式**（三份拷貝的 code smell）：本批不做，列為候選（建議）。

## 設計的過程與證據

- 設計 1 位（high，只讀）＋對抗式檢查 1 位（xhigh）。對抗式檢查確認：機制推導、恢復上界 N＝4（不是循環論證）、Gateway 預算、以及「實作前要先停下問使用者」。推翻或修正的：C 的選擇理由「不受 `worker_main:428` 限制」（只在 bucket 滿時成立）；「可改 R＝75」；突變的 expect；新測試的插入位置；`:261` 其實沒檢查到輸入；`MovementRecoveryTests` 的模型是兩條路徑共用；擾動的定義；同步清單漏了 `network-architecture.zh-Hant.md`。
- 證據：`build/target/_build/test/logs/pvp-v7-09a-design-20261010/`（`timeline_*.txt`、`phase_shifts.txt`、`queued_by_second.txt`、`bucket_census*.txt`、`unlimited_wait.txt`、`sim*.txt`、`synth.txt`；對抗式檢查在 `adversarial/`：`check_rule.py`／`.txt`、`crosscheck_sim_b.*`、`r6_p2_natural_experiment.txt`、`worker_probe_run1.txt`；各有 `commands.txt` 與 `sha256.txt`）。
