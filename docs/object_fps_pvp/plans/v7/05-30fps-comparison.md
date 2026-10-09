# 第 05 批：C1——30 FPS 修正前後對比（任務 1、2）與視窗操作

狀態：**完成**（2026-10-09）。C1 通過（第 2 次 session，4 ms 狀態判定且有對照；8 ms 狀態未驗證）；使用者操作的 L2、L3 通過。PR 線 P1b。依賴第 04 批與 TT-1；C1 只在 PR #71 的 CI 全綠之後執行。
起因：2026-10-07 使用者指示保存 v6 的 30 FPS 失敗案例，在 v7 分執行緒完成後再跑一次比較，預期 clean-30 與 clean-60 的結果相近（v7 README「v6 的 30 FPS 失敗案例與各幀率的對照」）。定義見 D31。

## 事前宣告草案（未核准；2026-10-09 具體化，D39 審查後修訂，待使用者核准）

核准時，本節連同核准日期另存為 `build/target/_build/test/logs/pvp-v7-batch05-<開跑日>/declare.txt`，記下 SHA-256，之後不改。D39 的審查（1 位評審、1 次對抗式檢查）與處理見本節最後。

### 對象與凍結

- before：v6 最終量測來源 `fee92ff`（產品程式＝`c7d6dd3`＝tag `object_fps_pvp-v1.1.0`）。沿用第 03a 批的從零建置（`../GYO-Engine-v6final`）。開跑前重算產物雜湊寫進本批的 `artifacts.sha256`，必須和 03a `artifacts.sha256` 中 before 的部分逐項一致；不一致就停下。
- after：`805bd11`（PR #71 的頭，CI 全綠）。在 detached worktree `../GYO-Engine-p1b-c1` 從零建置：把 03a 的 `build.sh` 複製到本批目錄，只改輸出目錄（`E`）與 worktree 清單，差異保留；產物雜湊寫進 `artifacts.sha256`。
  - 與 before 的差異是任務 1、2（P1a、P1b），仍是 pv6。Gateway 與 Match 的原始碼到 `805bd11` 都沒有差異；權威 digest 在 `aa9a342` 對 v6 最終 tree 35／35 相同，之後只改了 Client、probe 與測試。網路 worker 的輪詢與送出節拍不變。
  - 量測期間暫停 PR #71 的 auto-fix。C1 的結論只適用於量測的 commit；之後 PR 有任何非文件變更，本次 session 作廢，所有格在新的宣告下重跑（和 before 交錯），不做部分重跑。
- 分析器與腳本（開跑前寫進 `analyzers.sha256`）：
  - 凍結子集：v6 最終清單 `0d2ffcfb…`（`pvp-v6-batch14b-20261008/tools.sha256`）中的 40 個 Python 檔，兩棵 tree 逐位元組相同。
  - 分析器 v7：`command_evidence_v7.py`（`805bd11` 的版本，SHA-256 `394d5e25…`；ID 在 `347d375` 的規則修改前後相同，所以以雜湊識別），以及它匯入的 `command_evidence.py`。
  - 共用抽取器：參考包 `pvp-v6-30fps-reference-20261007/analyze.py`（SHA-256 `68ee027a…`），以 `all_runs.py` 的方式載入它的函式，不用它四捨五入後的輸出。
  - 本批的腳本：`run.py`（輪次順序與補跑）、`sleeper.py`（03a 的版本）、`summarize.py`（每輪的分類、主機狀態、通過與否、各項計數、停止條件、判定）。三者開跑前寫好，先在 03a 的資料上試算一次，之後凍結；所有比率與門檻以 `fractions.Fraction` 從原始計數精確計算。

### 跑次

- 案例：遊戲矩陣的 clean-30、clean-60、clean-144（`run_gameplay.py --case`）。每棵 tree 用自己的 runner、Match、Gateway 與 probe。
- 第 k 回：依序跑 clean-30、clean-60（k ≤ 3 時再加 clean-144）；每個案例內，k 為奇數時先 before 再 after，偶數時先 after 再 before。
- 基本輪數：第 1～6 回（clean-30、clean-60 每棵 tree 6 輪；clean-144 每棵 tree 3 輪，只作對照）。
- 補跑：基本輪數之後先跑 `summarize.py`（主機狀態與停止條件），沒有停下才補跑。對每一種主機狀態：clean-30／clean-60 × before／after 這 4 格中只要有一格有樣本、而且有一格少於 3 輪，就繼續跑完整的回（只含 clean-30、clean-60，回的編號接續）。直到每個有樣本的狀態 4 格都 ≥3 輪，或每棵 tree 每個案例都達 12 輪為止。
- 機器閒置：期間不做開發，caffeinate 開啟，沒有其他使用者程式在前景。每輪前後各跑 5 秒 sleeper。分析器 v7 在每輪的後 sleeper 之後執行。
- 檔案：`runs/<案例>-r<回>-<tree>/`（由 `run_gameplay.py` 建立；`run.py` 不得預先建立，第 03a 批第 1 次就是壞在這裡），另有 `progress.jsonl`、`summary.json`、`judgement.json`，結束時寫 `files.sha256`。分析器 v7 把 `command-evidence-v7.json` 寫進案例目錄。
- 預計時間：基本 30 輪約 40 分鐘；補跑最多再 48 輪。

### 每輪的分類

- E（runner／環境錯誤）：`ready.json` 沒有出現，包括服務啟動失敗、probe 沒有加入兩個 session、HTTP 啟動逾時、`run.py` 的例外或子程序逾時。停下回報；經使用者同意後才依原宣告重跑該輪。
- A（分析器結構上拒絕）：凍結分析器或分析器 v7 因 schema、協議、未知事件種類而拒絕 trace，或分析器本身拋出例外。這就是停止條件「凍結分析器拒絕 after 的 trace」；該輪結束後停下。
- F（失敗的輪）：其他所有不通過的情況，包括 `ready.json` 出現之後 probe 才失敗（例如「Client simulation stopped」）、Match 沒有正常寫完 trace。照常保留，算入通過率，繼續下一輪。沒有 `movement` 時，替代筆數列為「未知」，不進入替代比率。主機狀態以 `ready.json` 的 `start_ns` 計算；連狀態都算不出來就停下回報。

### 主機狀態

- 主分類：共用抽取器的 `host_late_p99`（`start_ns` 之後 1～15 秒，Match `snapshot_produced` 對擬合 60 Hz 格點的晚醒 P99），不四捨五入；≥6.0 ms 為 8 ms 狀態，其餘為 4 ms 狀態。
- 同時記錄：probe 的 TimerBaseline（`timer.late_p99_ms` ≥6.0 ms 視為 8 ms，和第 14b 批相同），以及每輪前後的 sleeper；報告兩種分類的一致率，作為 C2 之後的分類依據。
- **與 D31 的偏離（請核准）**：D31 要求「每輪前後量沒有 Client 的 Match 空轉晚醒」。但空的 Match 不寫 `snapshot_produced`（每位玩家一筆），Match 的 tick 晚醒要到 P2-log 才有紀錄，所以本批以 sleeper 近似（和第 03a 批相同）。嚴格的檢查在 P2-log 之後做（第 09 批的 C2）。
- 偏移的停止條件：clean-30 與 clean-60 合併計算，以基本輪數中兩棵 tree 的 8 ms 狀態輪數做雙側 Fisher 精確檢定，p < 0.05 就停下報告。sleeper 只記錄，不作停止條件（它的分布是雙峰的，用中位數比較等於比較多數狀態）。

### 每輪的指標與通過與否

- 指標（兩棵 tree 用同一套程式）：
  - 替代（Held＋Neutral）的筆數、時段與 Tick（1～15 秒窗，依解析時刻）。Match 停頓造成的替代另列：Match 的 `snapshot_produced` 間隔 ≥40 ms 的那個 Tick 起 0～3 Tick 內解析的替代。Match 在兩棵 tree 相同，這類替代與 Client 無關，不進入下面的替代比率（只列出）。
  - 停頓重設：`result.json` 的 `start_phase.<玩家>.epochs[*].stall_reseeds` 合計（凍結的 `start_phase_evidence.py`，讀 Client trace）。共用抽取器不計算這一項，改用這個兩棵 tree 相同的凍結程式。
  - Actual 比率、產生→執行的延遲 P50／P95、LifeRespawn 以外的 epoch 重設、最大幀、runtime gap（後兩項的來源在兩棵 tree 不同：v6 是畫面幀，v7 是 probe 的主迴圈與模擬角色，只列出、不跨樹比較）。
  - after 另記：模擬步的晚醒（`action-client.json` 的 `simulation_wakes`）、分析器 v7 的結果與晚醒的 seed 夾住。before 也跑分析器 v7，只作診斷。
- 替代比率：Σ（非 Match 停頓的替代筆數）÷ Σ `movement.remaining_originals`（同一窗內 Client 產生的原始命令數，不含 seeded neutral 與 lifecycle 取消；分子依解析時刻，分母依產生時刻，照凍結的定義）。
- 每輪通過與否：
  - before：該 tree 的 `run_gameplay.py` 判定（凍結分析器）。
  - after：凍結分析器的錯誤中，只有「Client runtime gap/dropped time is not a matched normal LifeRespawn or session-start seed clamp」屬於分執行緒後語意改變的規則（每幀的 runtime_gap、seed 夾住豁免），這一條改以分析器 v7 判定；其餘照凍結分析器。所以：凍結分析器沒有其他錯誤、而且分析器 v7 通過，才算通過。凍結分析器對 after 的原始判定照列。
  - 兩棵 tree 共同照凍結規則判定、但意義在 v7 改變的項目（列出，不改）：「Client full-run frame reached100ms」與最大幀在 v7 只量 probe 的主迴圈（不再停住命令產生），仍作為兩棵 tree 共同的主機異常指標；延遲門檻（P50 ≤50、P95 ≤66.7 ms）在 v7 量的是「步邊界→執行」，不含意圖的年齡（≤1 幀）。分析器 v7 的 gap 規則只看 `[start_ns, end_ns)`，凍結規則看整段 trace，這是輕微的放寬。60 Hz ±2 步與 STALL_RULE 在遊戲矩陣中不產生判定，照列。

### 判定

- v6＝本批 before 在同一狀態的格；參考包與第 03a 批只列出，不替代本批的樣本。
- 一條只用到它引用的格（案例×tree×狀態）；引用的每一格都 ≥3 輪才判定這一條，否則這一條在該狀態標「未驗證」。N 為被判定的那一格的輪數。
- 對照的前提（每個狀態）：v6 clean-30 的替代比率 ≥0.15%，或至少 2 輪各有 ≥5 筆替代（都不含 Match 停頓）。不成立時，第 2 條後半標「未驗證（對照未重現）」。

1. v7 clean-30 的通過率 ≥ v7 clean-60 的通過率 − 1／N，而且 ≥ v6 clean-30 的通過率。
2. v7 clean-30 的替代比率 ≤ 2 × v7 clean-60 的替代比率 ＋ 0.05%；而且（有對照時）≤ v6 clean-30 的替代比率 ÷ 3 ＋ 0.05%。0.05% 約為每輪 0.84 筆，用來在參照值為 0 時門檻不為 0。
3. v7 clean-60 不比 v6 差：通過率 ≥ v6 clean-60 的通過率 − 1／N，而且替代比率 ≤ 2 × v6 clean-60 的替代比率 ＋ 0.05%。第 3 條若只因凍結的延遲門檻不成立，標為第 04 批已知的位移，停下由使用者決定。
4. v7 的 clean-30、clean-60：不分狀態，所有輪的停頓重設為 0，而且沒有 LifeRespawn 以外的 epoch 重設。
5. clean-144 只記錄。移動延遲只記錄跨樹的比較，不用來說明「30 與 60 相近」：v7 的「產生→執行」不含意圖的年齡（30 FPS 時最多 33 ms）。
6. 第 03a 批的數字只列出，不歸因（不同 session，每案 3 輪，幾乎都在 4 ms 狀態）。

- **C1 的結論**：至少有一個狀態判定了「有對照」的第 2 條後半，所有判定的條目都成立，而且第 4 條成立，才算通過。任一判定的條目不成立就不通過（第 3 條不成立即停止條件「v7 clean-60 的退步超出宣告」）。沒有任何狀態判定到有對照的第 2 條後半時，結論是「未證明」，並列出缺少的狀態。
- 停止條件「before 無法重現 v6 參考範圍」：基本輪數之後，對 before 有 ≥3 輪的每個案例×狀態，若 (a) before clean-60 有 ≥2 輪失敗（參考為 15／15 與 6／6），或 (b) before 的 P50 中位數落在參考包 `all-runs.jsonl` 同案例同狀態 P50 的［最小, 最大］±3 ms 之外（clean-30：4 ms 19.9～28.6、8 ms 26.0～28.2；clean-60：4 ms 22.7～35.9、8 ms 22.5～33.8 ms），就停下報告。
- 報告表：每個案例×tree×狀態列出輪數、通過、替代合計與每輪範圍與時段（Match 停頓另列）、Actual、P50、P95、停頓重設、sleeper、TimerBaseline；after 另列 `simulation_wakes`。
- 解讀時的註記：after 的模擬角色用 TT-1 的 kqueue Waiter（第 04 批的開發跑次中晚醒最大約 0.2 ms），probe 的 `sleep_until` TimerBaseline P99 約 8 ms；8 ms 狀態下的改善包含計時精度，C1 分不開。

### 使用者操作的 L2 與 L3（另一個 session，使用者在場）

- L2：after 建置的 Match（`--movement-trace`）、Gateway、兩個 Client（`--movement-trace`、`--log`）；依 v6 手動驗收第 6 項重現 IP-2 的標題列拖動與角落縮放，各至少 3 次、每次至少 2 秒。
  - 判定（以加入之後的整段 session 計，比只看拖動期間更嚴）：每位玩家 Match 的連續替代（Held 或 Neutral，不含 Match 停頓造成的）≤3 Tick，停頓重設 0，LifeRespawn 以外的 epoch 重設 0。
  - 停止條件「模擬晚醒 ≥100 ms」的來源：Client trace 中模擬角色發出的 `runtime_gap`（`frame_seconds` ≥0.1）。
- L3：連續射擊中切換 Spaces、縮小、遮住視窗；保留 Client、Gateway、Match 的日誌與 movement trace。判定不斷線。
  - 原計畫的「30 FPS 的手感」：產品 Client 沒有指定 FPS 的選項，本批不做；依 D41 在第 05b 批（產品加 `--fps` 限幀選項）之後做。

### D39 審查的處理（2026-10-09）

- 評審（必修 6、建議 13）與對抗式檢查（12 項）各自獨立進行，指出的主要問題相同：缺少停頓重設的直接計數（v7 的 runtime_gap 只在部分 seed 時發出，偵測偏向 v7）；Match 本身的偶發停頓（62 輪中 2 次）會讓替代條件誤判；v6 在 4 ms 狀態不一定重現 30 FPS 的問題，「比 v6 好」可能自動成立；沒有整體結論；v7 中途崩潰會被當成可重跑的環境錯誤；判定與彙總腳本沒有凍結、浮點比較會誤判；停止條件不明或容易誤停。
- 處理：以上全部改寫進本節（停頓重設的直接計數、Match 停頓的分類、對照的前提、整體結論、E／A／F 分類、腳本凍結與精確計算、Fisher 檢定、before 重現的量化條件、輪次與補跑的精確規則、替代＝Held＋Neutral、延遲的定義、L2／L3 的可執行性）。
- 需要使用者特別確認的點：與 D31 的偏離（sleeper 近似）、Match 停頓替代的排除、對照的前提與「未證明」的結論、量測期間暫停 auto-fix、L3 不做 30 FPS 的手感。

## 驗收點

- L2：照核准的宣告執行；失敗的跑次保留，先有限定位，不默默重跑。
- L3：使用者的清單通過；重現 Spaces 斷線算失敗，並保留日誌。

## 建議檔位

medium（執行與記錄）；宣告草案以 high 撰寫。依 D39，宣告草案建議由 1 位評審加 1 次對抗式檢查，批次開始時徵求同意。

## 停止條件

各條的量化定義在事前宣告中：

- v7 clean-60 的退步超出宣告（判定第 3 條不成立；只因延遲門檻不成立時也停下，由使用者決定）。
- before 無法重現 v6 參考範圍（表示環境改變；(a) 失敗輪數或 (b) P50 範圍）。
- 主機狀態頻率在兩樹間明顯偏移（Fisher 精確檢定 p < 0.05）。
- 分類 E 的輪（runner／環境錯誤）、分類 A 的輪（分析器結構上拒絕 after 的 trace），或主機狀態算不出來。
- Spaces 斷線重現，或視窗被遮住時模擬晚醒 ≥100 ms：保留日誌後停下，提出 macOS 程序活動宣告（App Nap 對策）作為新的 Architecture Delta，交給使用者決定。

## 平台表

| 平台 | 本批 |
|---|---|
| macOS Intel（Metal、120 Hz） | L2、L3 |
| Windows（D3D12） | 第 15 批的 LAN 場次 |
| Linux、macOS arm64 | 未驗證 |

## 結果

### C1 第 1 次 session（2026-10-09 10:27～10:42，停止）

- 宣告 `declare.txt`（SHA-256 `f4c4396f…`），session `build/target/_build/test/logs/pvp-v7-batch05-20261009/`。預檢通過（before 的產物與 03a 一致；40 個凍結分析器在兩棵 tree 與清單一致；分析器 v7 `394d5e25…`、參考抽取器 `68ee027a…`）。
- 基本 30 輪跑完（分類 E、A 都沒有），`summarize.py --stage base` 觸發停止條件「before 無法重現 v6 參考範圍」，依宣告停下，沒有補跑也沒有重跑：
  - before clean-30 的 8 ms 狀態：P50 中位數 22.8 ms，在參考範圍［26.0, 28.2］±3 ms 之外。
  - before clean-60 的 8 ms 狀態：3 輪全部失敗（參考為 15／15 與 6／6）。
- 有限定位（不判定）：
  - 這次的「8 ms 狀態」不是參考包的 8 ms 狀態：`host_late_p99` 在 10～47 ms（參考的 8 ms 狀態約 6～8 ms），失敗輪大多帶著「Clean Match runtime gap/dropped time」或「Client full-run frame reached100ms」，兩棵 tree 都有。
  - 每輪前後的 sleeper 在兩棵 tree 都間歇出現 20～75 ms 的最大值（參考 session 約 4 ms）；停下時的 15 分鐘負載平均約 6，前景以外的 Chrome renderer 約 50% CPU。判斷為機器沒有處於宣告要求的閒置狀態（之前的從零建置與 app 重啟剛結束、瀏覽器在背景運作）。
  - TimerBaseline 與主分類的一致率 18／30。
- 只記錄、不作判定的觀察：在 4 ms 狀態，after clean-30 的 4 輪 Client 替代都是 0（1 輪因「Client full-run frame reached100ms」失敗），before clean-30 只有 1 輪落在 4 ms 狀態（替代 28、Actual 未達 99%）；after clean-60 第 1 輪在 4 ms 狀態因延遲門檻失敗（P50 45.3、P95 69.4 ms，Match 停頓替代 6）。
- 部分判定（基本輪數時點，不構成結論）：結論「未證明」；4 ms 狀態的第 2a、3 條成立，其餘未驗證；第 4 條（v7 的停頓重設與 epoch 重設）成立。
- 證據：`progress.jsonl`（SHA-256 `3560d459…`）、`summary.json`（`1e2b470f…`）、`judgement.json`（`0c8f5878…`）、`runs/`、`session.log`、`summarize-base.txt`。

### C1 第 2 次 session（2026-10-09 10:49～11:17，**通過**）

- 同一份宣告，加上使用者核准的閒置檢查（附註；`declare.txt` SHA-256 `62111cd5…`）。開跑前關掉 Chrome、ChatGPT／Codex；閒置檢查一次通過（3 次 sleeper 最大約 4 ms）。session `build/target/_build/test/logs/pvp-v7-batch05-20261009-s2/`。
- 基本 30 輪後沒有停止條件；8 ms 狀態只有 before 有樣本，依規則整回補跑到第 12 回（clean-30、clean-60 每棵 tree 12 輪）。全部 54 輪沒有 E、A 類。
- 主機狀態：54 輪中 51 輪在 4 ms 狀態；8 ms 狀態只有 before 的 3 輪（各案例 1 輪）。TimerBaseline 與主分類的一致率 53／54。

| 狀態 | 案例 | tree | 輪數 | 通過 | Client 替代（合計／原始命令） | 比率 | 每輪 | Match 停頓替代 | P50 中位數 | P95 中位數 |
|---|---|---|---|---|---|---|---|---|---|---|
| 4 ms | clean-30 | v6 | 11 | 9 | 105／18435 | 0.570% | 14、9、43、3、0、2、0、3、21、7、3 | 12 | 21.4 | 27.8 |
| 4 ms | clean-30 | v7 | 12 | 12 | 2／20125 | 0.010% | 只有 1 輪 2 筆，其餘 0 | 6 | 38.1 | 39.3 |
| 4 ms | clean-60 | v6 | 11 | 11 | 8／18442 | 0.043% | 0～4 | 3 | 24.9 | 27.9 |
| 4 ms | clean-60 | v7 | 12 | 12 | 0／20122 | 0% | 全部 0 | 0 | 37.9 | 39.0 |
| 8 ms | clean-30 | v6 | 1 | 0 | 30／1677 | 1.789% | 30 | 0 | 25.6 | 38.5 |
| 8 ms | clean-60 | v6 | 1 | 1 | 0／1675 | 0% | 0 | 0 | 24.6 | 38.9 |

- 判定（4 ms 狀態，有對照：v6 clean-30 的比率 0.570% ≥0.15%）：第 1 條成立（v7 clean-30 12／12 ≥ v7 clean-60 12／12 − 1／12，而且 ≥ v6 clean-30 9／11）；第 2 條前半成立（0.010% ≤ 2×0%＋0.05%）、後半成立（0.010% ≤ 0.570%÷3＋0.05%）；第 3 條成立（v7 clean-60 12／12 ≥ 11／11 − 1／12；0% ≤ 2×0.043%＋0.05%）。8 ms 狀態：after 沒有樣本，各條「未驗證」。第 4 條成立：v7 的 clean-30、clean-60 共 24 輪，停頓重設 0、LifeRespawn 以外的 epoch 重設 0。
- **結論：通過**（宣告的規則：至少一個狀態判定到有對照的第 2 條後半，所有判定的條目成立，第 4 條成立）。8 ms 狀態未驗證：這個 session 的機器夠安靜，after 沒有落到 8 ms 狀態（第 1 次 session 的「8 ms 狀態」是非閒置的主機，不能使用）。
- 只記錄：
  - v6 clean-30 的 3 個失敗輪都是「Clean original movement Actual below99%」（30 FPS 的已知問題）；v7 沒有失敗輪。
  - 移動延遲（產生→執行）：v7 的 P50 約 38 ms，在 30 與 60 FPS 都一樣；v6 是 21～25 ms。這是第 04 批已知的位移（v7 的「產生」是步邊界、不含意圖的年齡），C1 之後與相位追蹤一起決定。
  - after 的模擬步晚醒：每輪的 P99 上界 ≤0.25 ms；全 session 最大 65.9 ms 的晚醒出現 1 次（沒有停頓重設）。
- 證據：`progress.jsonl`（SHA-256 `a7ea5920…`）、`summary.json`（`e3302d94…`）、`judgement.json`（`2717b3fb…`）、`idle-gate.jsonl`（`d70fdc11…`）、`runs/`、`session.log`、`summarize-*.txt`。

### 使用者操作的 L2 與 L3（2026-10-09 11:37～11:41，**通過**）

- after 建置（`805bd11`）的 Match（`--movement-trace`）、Gateway、兩個 Client（`--movement-trace`、`--log`）。使用者建房、加入，依宣告做標題列拖動與角落縮放，再於連續射擊中切換 Spaces、縮小、遮住視窗。目錄 `build/target/_build/test/logs/pvp-v7-batch05-l2l3-20261009-1137/`，判定腳本 `judge.py`。
- 期間兩個 Client 共出現 11 次 OS 的 modal loop（「slow event processing」1.1～4.0 秒，其中多次只有 live frame 在更新畫面）；兩位玩家各有死亡與重生（LifeRespawn 1 次與 6 次）。
- L2（每位玩家加入後的整段 session，約 140 秒）：Match 的替代（Held 或 Neutral）**0 筆**，最長連續 0 Tick（門檻 ≤3）；Match 停頓（間隔 ≥40 ms）1 次，沒有造成替代；停頓重設 0；LifeRespawn 以外的 epoch 重設 0。**通過**。v6 在同樣的 modal loop 中命令會停住（任務 1 的起因）。
- L3：兩個 Client 都沒有斷線（`connection failed` 0）；模擬角色的晚醒沒有 ≥100 ms 的（Client trace 的 `runtime_gap` 0 筆），停止條件沒有觸發。**通過**。App Nap 造成斷線的疑慮在這次沒有重現。
- 「30 FPS 的手感」依宣告沒有做（產品 Client 沒有指定 FPS 的選項）；依 D41 在第 05b 批做。

