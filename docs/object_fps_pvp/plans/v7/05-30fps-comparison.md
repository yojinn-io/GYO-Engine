# 第 05 批：C1——30 FPS 修正前後對比（任務 1、2）與視窗操作

狀態：**宣告草案待審查與核准**（2026-10-09）。PR 線 P1b。依賴第 04 批與 TT-1；C1 只在 PR #71 的 CI 全綠之後執行。
起因：2026-10-07 使用者指示保存 v6 的 30 FPS 失敗案例，在 v7 分執行緒完成後再跑一次比較，預期 clean-30 與 clean-60 的結果相近（v7 README「v6 的 30 FPS 失敗案例與各幀率的對照」）。定義見 D31。

## 事前宣告草案（未核准；2026-10-09 具體化，待 D39 審查與使用者核准）

宣告核准時，本節連同核准日期另存為 `logs/pvp-v7-batch05-*/declare.txt`，並記下它的 SHA-256；之後不改。

### 對象與建置

- before：v6 最終量測來源 `fee92ff`（產品程式＝`c7d6dd3`＝tag `object_fps_pvp-v1.1.0`）。
  - 沿用第 03a 批的從零建置（`../GYO-Engine-v6final`）。開跑前重算產物雜湊，必須和 03a `artifacts.sha256` 中 before 的部分逐項一致；不一致就停下。
- after：PR #71 在 CI 全綠時的頭，於宣告核准時寫入 commit。之後只允許文件變更；任何非文件的變更都要依本宣告重新評估受影響的項目。
  - 在 detached worktree `../GYO-Engine-p1b-c1`，以第 03a 批的 `build.sh`（同一份 DEPS）從零建置；產物雜湊寫進 `artifacts.sha256`。
  - 與 before 的差異是任務 1、2（P1a、P1b），仍是 pv6。Gateway 的原始碼沒有差異；Match 的行為由權威 digest 證明不變（對 v6 最終 tree 35／35）。網路 worker 的輪詢與送出節拍不變。
- 分析器（開跑前寫進 `analyzers.sha256`）：
  - 凍結子集：v6 最終清單 `0d2ffcfb…`（`pvp-v6-batch14b-20261008/tools.sha256`）中的 Python 檔。兩棵 tree 的這些檔案必須逐位元組相同（`fee92ff..after` 對 `build/acceptance/object_fps_pvp/*.py` 的差異只有新增的 `command_evidence_v7.py` 與其測試）。
  - 分析器 v7：after tree 的 `command_evidence_v7.py`（ID `pvp-v7-commands-1`），以及它匯入的 `command_evidence.py`。
  - 共用抽取器：參考包 `pvp-v6-30fps-reference-20261007/analyze.py`（Held 筆數與時段、Actual、runtime gap、延遲、主機晚醒）。

### 跑次

- 案例：遊戲矩陣的 clean-30、clean-60、clean-144（`run_gameplay.py --case`）。每棵 tree 用自己的 runner、Match、Gateway 與 probe。
- 基本輪數：clean-30、clean-60 每棵 tree 6 輪；clean-144 每棵 tree 3 輪（對照組）。
- 順序：每一回依序跑 clean-30、clean-60（前 3 回再加 clean-144），兩棵 tree 的先後每回交替（第 1 回 before→after，第 2 回 after→before，依此類推）。
- 補跑（主機狀態）：基本輪數跑完後，對 clean-30 與 clean-60 各自檢查兩種狀態。某一狀態中，兩棵 tree 合計至少有 1 輪、但其中一棵少於 3 輪時，就以同樣的交替順序補跑該案例，直到兩棵都有 3 輪，或每棵 tree 該案例達 12 輪為止。
- 機器閒置：期間不做開發，caffeinate 開啟，沒有其他使用者程式在前景。每輪前後各跑 5 秒 sleeper（第 03a 批的 `sleeper.py`）。
- 每一輪都保留。失敗或異常只做有限定位，不重跑。runner 或環境錯誤（啟動前就結束、沒有任何數據）停下回報，經使用者同意後才依原宣告重跑該輪（比照第 03a 批）。

### 主機狀態

- 主分類：共用抽取器的 `host_late_p99`（1～15 秒，Match `snapshot_produced` 對擬合 60 Hz 格點的晚醒 P99）。≥6.0 ms 為 8 ms 狀態，其餘為 4 ms 狀態。
- 同時記錄：probe 的 TimerBaseline（`action-client.json`），以及每輪前後的 sleeper；報告兩種分類的一致率，作為 C2 之後的分類依據。
- D31 的「沒有 Client 的 Match 空轉晚醒」：Match 的 tick 晚醒要到 P2-log 才有紀錄，本批以每輪前後的 sleeper 近似（和第 03a 批相同），並在報告中註明。
- 各 tree 的狀態頻率照列，不假設相同，只在同一狀態內比較。
- 偏移的停止條件：同一案例在基本輪數內，兩棵 tree 的 8 ms 狀態比例相差 50 個百分點以上；或 after 輪前後的 sleeper P99 中位數比 before 輪高 2 ms 以上。符合任一項就停下報告。

### 每輪的判定與指標

- 指標（共用抽取器，兩棵 tree 相同）：Held 筆數與時段（1～15 秒窗）、Actual 比率、移動延遲 P50／P95、runtime gap、最大幀；凍結分析器的 LifeRespawn 以外的 epoch 重設次數；after 另記模擬步的晚醒（`action-client.json` 的 `simulation_wakes`）與分析器 v7 的結果。
- Held 比率＝兩位玩家在 1～15 秒窗內 Held 筆數的合計 ÷ 同一窗內被解析的原始命令數（`movement.remaining_originals`）。
- 每輪通過與否：
  - before：該 tree 的 `run_gameplay.py` 判定（凍結分析器）。
  - after：凍結分析器的錯誤中，只有「Client runtime gap/dropped time is not a matched normal LifeRespawn or session-start seed clamp」屬於分執行緒後語意改變的規則（每幀的 runtime_gap、seed 夾住豁免）；這一條改以分析器 v7 判定，其餘照凍結分析器。也就是：凍結分析器沒有其他錯誤，而且分析器 v7 通過，才算通過。60 Hz ±2 步與 STALL_RULE 在遊戲矩陣中不產生判定（STALL_RULE 只是報告中的文字），照列。
  - 凍結分析器對 after 的原始判定照列，不作判定依據。

### 判定（各主機狀態內；兩棵 tree 在該狀態都至少有 3 輪才判定，否則該格標「未驗證」）

以通過率（通過輪數 ÷ 該狀態的輪數）與 Held 比率（該狀態各輪合計）比較；N 為比較的兩格中較少的輪數。

1. v7 clean-30 的通過率 ≥ v7 clean-60 的通過率 − 1／N。
2. v7 clean-30 的 Held 比率 ≤ 2 × v7 clean-60 的 Held 比率 ＋ 0.05%，而且 ≤ v6 clean-30 的 Held 比率 ÷ 3 ＋ 0.05%（0.05% 約為每輪 1 筆，避免分母為 0 時無法判定）。
3. v7 clean-60 不比 v6 差：通過率 ≥ v6 clean-60 的通過率 − 1／N，而且 Held 比率 ≤ 2 × v6 clean-60 的 Held 比率 ＋ 0.05%。
4. v7 的 clean-30、clean-60：所有輪都沒有 LifeRespawn 以外的 epoch 重設。
5. clean-144 只記錄（對照組）。移動延遲 P50／P95 只記錄：第 04 批已知常數不變時 v7 的「輸入取樣→執行」比 v6 多約 1 幀，C1 之後與相位追蹤一起決定。
6. 第 03a 批的數字一起列出，分開顯示第 03 批與第 04 批的貢獻。

### 使用者操作的 L2 與 L3（另一個 session，使用者在場）

- L2：after 建置的兩個 Client 加 Match（`--movement-trace`），依 v6 手動驗收第 6 項重現 IP-2 的標題列拖動與角落縮放，各至少 3 次、每次至少 2 秒。判定：拖動與縮放期間，每位玩家 Match 的連續替代（Held 或 Neutral）≤3 Tick，LifeRespawn 以外的 epoch 重設 0。
- L3：連續射擊中切換 Spaces、縮小、遮住視窗；保留 Client、Gateway、Match 的日誌與 movement trace。判定不斷線；30 FPS 的手感由使用者判斷。

## 驗收點

- L2：照核准的宣告執行；失敗的跑次保留，先有限定位，不默默重跑。
- L3：使用者的清單通過；重現 Spaces 斷線算失敗，並保留日誌。

## 建議檔位

medium（執行與記錄）；宣告草案以 high 撰寫。依 D39，宣告草案建議由 1 位評審加 1 次對抗式檢查，批次開始時徵求同意。

## 停止條件

- v7 clean-60 的退步超出宣告。
- before 無法重現 v6 參考範圍（表示環境改變）。
- 主機狀態頻率在兩樹間明顯偏移。
- Spaces 斷線重現，或視窗被遮住時模擬晚醒 ≥100 ms：保留日誌後停下，提出 macOS 程序活動宣告（App Nap 對策）作為新的 Architecture Delta，交給使用者決定。
- 凍結分析器拒絕 after 的 trace。

## 平台表

| 平台 | 本批 |
|---|---|
| macOS Intel（Metal、120 Hz） | L2、L3 |
| Windows（D3D12） | 第 15 批的 LAN 場次 |
| Linux、macOS arm64 | 未驗證 |
