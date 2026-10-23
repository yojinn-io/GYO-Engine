# outage-characterize 特性報告（關卡 G1）

- 計畫：[outage-resync.md](outage-resync.md) 的 `outage-characterize`（D172）。
- 測試：
  - `tests/object_fps_pvp/PvpMatchTests.cpp` 的「PvP outage characterization: …」4 個；
  - `tests/object_fps_pvp/PredictionTests.cpp` 的 3 個。
- 證據夾：`build/target/_build/test/logs/pvp-v71-outage-characterize-20261023/`（git 忽略），內含：
  - `host-outage-matrix.txt`：72 個情境的全表；
  - `host-sparse.txt`；
  - `corpus_scan.py` 與 `scan.jsonl`：語料掃描；
  - `mutants.json`。
- 產品程式不改，只固定現狀。

## 1. 怎麼量的

- **Host 端**：把產品的 `LocalPlayerPrediction` 接到 `MatchRuntimeHost`，以一幀一 Tick 運行。Client 每幀送出整個未確認的窗口（最多 12 個命令）。
- **中斷注入**：在連線上注入四類中斷：
  - 上行丟失（Lose）；
  - 上行暫存後一次送達（Hold）；
  - 上行延遲送達（Delay）；
  - 不讓 Client 看到 snapshot（下行中斷）。
- **時長**：中斷 18、36、60、120、300、600 Tick（0.3～10 s），各在冷卻期外與冷卻期內（中斷前 10 幀剛換過 epoch）各跑一次。
- **部分送達**：每 `period` 個封包只有 1 個準時，其餘延遲 `delay` 幀。
- **讀取來源**：換 epoch 與 Client 的 Late 決定都從移動紀錄（trace）讀取，因為 Client 的 Observation 在換 epoch 時會歸零。
- **語料**：唯讀掃描 `logs/pvp-v71-*`，共 4,418 條 Client snapshot 串流與 7,052 份 `match-ingress.jsonl`。

## 2. 結果

### 2.1 雙向中斷

| 情況 | 換 epoch（距中斷開始） | 發布的最小 slack | Client |
|---|---|---|---|
| 0.3 s（18 Tick） | 無 | −266,672 µs（late_first 10） | stall reseed 1 次，樣本被擋掉，Late 0 |
| 0.6 s 以上，冷卻期外 | 32 Tick，Starvation | 無負值；重送的 12 個命令都是 EpochOld | Late 0 |
| 0.6 s，冷卻期內 | 無 | −566,678 µs | stall reseed，Late 0 |
| 1 s 以上，冷卻期內 | 51 Tick（距上一次換 epoch 61 Tick），Starvation | 無負值 | Late 0 |

**結論**：雙向中斷的大負值目前都被 Client 擋掉，靠的是兩層保護：

- `settledAfter_`；
- SeedLead 清空命令環。

### 2.2 單向中斷

- **只有上行（丟失）**：
  - 32 Tick 時 Starvation 換 epoch；
  - 上行恢復後約 31 Tick，**又來一次 Backlog 換 epoch**：0.6 s → 93、2 s → 151、5 s → 331、10 s → 631。
  - **原因**：Client 看到新 epoch 後重新 seed，但封包仍在丟失，窗口一路長到 12。上行恢復時這 12 個命令全部到達，Match 帶著約 12 Tick 的領先量運行，直到 Backlog 觸發。
  - **這違反 P2**：中斷期間累積的領先量被帶到中斷之後，持續 0.5～1 s。**計畫裡沒有這一項**（D-15）。
- **只有上行（暫存）、冷卻期內 0.6 s**：
  - 不換 epoch；
  - 34 個晚到命令一次到達，只發布 1 個 −566,678 µs 的樣本；
  - Client 沒有做 Late 修正（需要連續 2 個負樣本）。
- **只有下行**：Client 窗口滿 12 後停止產生命令，42 Tick 時 Starvation 換 epoch。0.6 s 以內不換 epoch，Client stall reseed 1 次。

### 2.3 部分送達（審查 C2 的缺口）

| 情況 | 換 epoch | 最小 slack | Client 的 Late 決定 |
|---|---|---|---|
| 每 8 個準時 1 個，延遲 0.5 s，持續 1 s | 68 Tick，Backlog | −466,676 µs | **4 次** |
| 同上，延遲 1 s，持續 5 s | 107、309，Backlog | −966,686 µs | 5 次 |
| 每 8 個準時 1 個，延遲 3 s，持續 5 s | 無 | **−1,000,000（夾限）** | 0（序號超出 64 格環，無聲丟棄） |
| 每 4 個準時 1 個，延遲 1 s，持續 5 s | 無 | −966,686 µs，75 個負樣本 | 0（負樣本不連續） |

- **保險絲不觸發**：命令陸續到達，佇列不空，所以保險絲永遠不會觸發。
- **延遲 0.5～1 s 時**：連續的負樣本讓 Client 做 Late 修正，每次最多提前 2 Tick。領先量越加越多，最後觸發 Backlog 換 epoch。**大負值確實進入了相位追蹤。**
- **延遲 3 s 時**：late_first 187，slack_late 只有 64。64 筆的上限讓 123 個樣本消失，所以 S3（`slack_late == late_first`）現在不成立。

### 2.4 稀疏替代

只替代 1 個序號，之後全部準時，再讓它晚到 100、590、610、700 Tick：

- **發布值**：每一次都以 −1,000,000 µs 發布。SlackTrack 只限制筆數（64 筆），不限制年齡，所以 11.7 s 前的命令照樣產生樣本。
- **帳本**：只追 600 個序號。超過 600 時，同一次到達被歸為 `resolved_untracked`，不是 `late_first`，所以 **slack_late > late_first**，S3 反方向也不成立。
- **Client**：序號遠在 64 格環之外，丟棄。

### 2.5 語料（門檻來源）

| 類別 | 串流數 | snapshot 最大間隔 |
|---|---|---|
| clean、soak、loss、network、round 等沒有注入停頓的跑次 | 約 2,700 | **0.119 s** |
| 注入 250 ms 停頓（gateway、host-ipc、downstream、socket-path） | 568 | 0.283 s |
| 注入 1000 ms 停頓 | 1,066 | 1.028 s |
| 實機 Windows LAN（2026-10-16） | 12 | 4.185 s（1 次，在跑次 96% 處，原因未查） |
| 睡眠測試（heartbeat-sleep） | 2 | 30.05 s |

- **晚到量**：`late_us` 最大 221,470 µs（13.3 Tick），出現在注入 250 ms 停頓的跑次；7,052 份檔案中，沒有任何一筆 ≥250 ms。

## 3. 對計畫前提的影響

**前提成立，不需要停下來重新規劃**：

- **雙向中斷**：已經被擋掉（兩層保護）。
- **真正的缺口**：冷卻期內、只有上行、部分送達這三種情況，與審查 C2 和計畫第 1 節一致。
- **新發現**（計畫沒有）：
  1. **只有上行中斷之後的 Backlog**：中斷期間累積的 12 命令領先量要等 0.5～1 s 後由 Backlog 清掉（D-15）。
  2. **稀疏替代不分年齡都產生 −1 s 樣本**，S3 兩個方向都不成立。M-B′ 的「晚到 > N_o 就換 epoch」會把這類到達改成 Resync；retention-600 會讓超過 600 序號的紀錄關閉，兩份紀錄一致。這屬於計畫內，不另加批次。
- **B_fuse 的實測值**（取代計畫中的推論值）：
  - 冷卻期外：32 Tick（雙向、只有上行）；
  - 只有下行：42 Tick；
  - 冷卻期內：距上一次換 epoch 61 Tick。

## 4. 關卡 G1 要決定的

1. **D-4 T_int（進入中斷）**
   - **選項**：0.5 s／0.75 s／1 s。
   - **證據**：
     - clean 最大 0.119 s，0.5 s 有 4.2 倍餘裕；
     - 注入 250 ms 停頓時最大 0.283 s，0.5 s 不會報中斷；
     - 注入 1 s 停頓會報中斷，這是正確的，玩家確實 1 s 沒有畫面更新。
   - **建議**：**0.5 s**。
2. **D-3 的 N_o（晚到多少就換 epoch）**
   - **選項**：15 Tick（250 ms）／20 Tick（333 ms）／30 Tick（500 ms）。
   - **證據**：
     - 語料最大 13.3 Tick，出現在注入 250 ms 停頓的跑次；
     - 部分送達延遲 0.5 s 時，晚到約 28 Tick，而這正是會引發 Late 修正與 Backlog 的情況。
   - **取捨**：
     - 15：離語料最大值只有 1.7 Tick 的餘裕，注入 250 ms 停頓的 L2 情境可能開始換 epoch；
     - 30：抓不到延遲 0.5 s 的部分送達；
     - 20：有 1.5 倍餘裕，也抓得到 0.5 s。
   - **建議**：**20 Tick**。
3. **D-15（新）只有上行中斷之後的領先量**
   - **(a)** 本線只記錄：既有、有測試的 Backlog 保險絲會在 0.5～1 s 內清掉。L2 記錄清除所需的時間，交給 v8 B6（「延遲結束後撤回多加的領先」，正是這個區域）。
   - **(b)** 本線設計一條規則，在 Match 端修剪新 epoch 第一批命令的多餘部分，或在 Client 端限制尚未確認時的窗口。兩種做法都會碰到序號與 ACK 的語意，要另做設計與 xhigh 審查，估時 +4～6 h。
   - **建議**：**(a)**。理由：
     - 清除已經有保險絲兜底；
     - 修剪多餘的領先量正是 B6 要重新規劃的部分，在本線先做會和 B6 重疊；
     - P2 的這個例外（最長約 1 s）寫進契約的已知項目。
