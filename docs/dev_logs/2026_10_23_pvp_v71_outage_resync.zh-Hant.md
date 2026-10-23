# PvP v7.1 outage-resync：斷線恢復＝重新對時

日期：2026-10-23 起。分支 `claude/pvp-v71-outage-resync`（從 master `cdb9b07`）。

**狀態：進行中**。計畫已核准（D172），`gate-characterize` 已決定（D173）。`resync-contract` 的條文核准（`gate-contract`，D174）。`drop-counters` 進行中。

相關文件：

- 計畫：[outage-resync.md](../object_fps_pvp/plans/v7.1/outage-resync.md)。
- 決策 D171 與設計原則：v7.1 記錄器分支 `claude/pvp-v71-after-re-evaluate` 的 HANDOFF。

## 批次

| 批次 | 檔位 | 做了什麼 | 驗收 |
|---|---|---|---|
| `plan-commit` | medium | 提交計畫草案第 2 版與本紀錄 | 只改文件 |
| `outage-characterize`（`c200992` 等） | high | 用測試固定中斷與晚到的現狀：Host 側接上產品的 `LocalPlayerPrediction`，注入雙向、只上行、只下行、部分送達的中斷；Client 側 3 個；稀疏替代 1 個；語料唯讀掃描。產品程式不改 | cpu 測試 293／293；突變 4／4 killed；L1 run 39944311104 四平台成功；報告：[outage-characterize-report.md](../object_fps_pvp/plans/v7.1/outage-characterize-report.md) |
| `resync-contract`（`02f4b6d`、本次） | high，審查 xhigh | network-architecture §4「連線中斷與恢復」、§7 兩節 Design Invariants；protocol-v7 §2；log-contract §1.4；ingress README | 只改文件；xhigh 對抗式審查 1 次（必須修訂 → 已處理）；`gate-contract` 等使用者核准 |

## 規劃

- **D171**：C13 選 (b)，1～10 秒晚到的大負值用斷線重連（重新對時）處理，新開 PR 線，規劃用 ultracode。
- **ultracode**：
  - 參與者：2 位偵察、3 份計畫、1 位評審、1 次對抗式審查。
  - 評審以「風險優先」版為底（58／70）。
- **對抗式審查的結論是「必須修訂」**，主要有三點：
  - 草案漏讀使用者補充的設計原則：身分接回被縮成「同一 endpoint」，並移出範圍。
  - 部分送達、只有上行中斷、冷卻期內這三種情況，大負值現在就會進入 Client 的相位追蹤，草案卻只記錄。
  - `slack_clamped` 追加到 ingress 行，會觸發 log-contract 的 Replacement Criteria。
- **抽查**：主會話對照程式碼，確認了上述三點，以及 endpoint 屬於允許的 session 軟狀態（`network-architecture.zh-Hant.md:137`）。
- **第 2 版的處置**：
  - 把狀態與規則寫成表（計畫第 3 節）；
  - Match 端加上「晚到過久就換 epoch」；
  - 身分接回列為首要決策點（D-2）；
  - L2 要讀的行改成新的 A 級行族。
  - 對照表見計畫第 11 節。

## outage-characterize

### 結果摘要

- **雙向中斷**：大負值目前都被 Client 擋掉，有兩層保護：`settledAfter_`，以及 SeedLead 清空命令環。
- **缺口**：冷卻期內、只有上行、部分送達。
  - 部分送達（延遲 0.5～1 s）時，Client 做了 3～6 次 Late 修正，接著觸發 Backlog 換 epoch。
  - 延遲 3 s 的樣本夾在 −1 s，Client 因為序號超出 64 格環而無聲丟棄。
- **新發現**：
  - 只有上行中斷之後，中斷期間累積的 12 命令領先量要等 Backlog 清掉，約 0.5～1 s。
  - 稀疏替代不論多晚都產生 −1 s 樣本。S3 兩個方向都不成立：
    - 稠密替代時 slack_late < late_first（64 筆上限）；
    - 稀疏且超過 600 序號時 slack_late > late_first。
- **B_fuse 實測**：冷卻期外 32 Tick，只有下行 42 Tick，冷卻期內距上一次換 epoch 61 Tick。
- **語料**：
  - clean 跑次的 snapshot 最大間隔 0.119 s（約 2,700 條串流）；
  - `late_us` 最大 221,470 µs，沒有一筆 ≥250 ms。

### 經過

- **第一版的模擬 Client 是自己寫的簡化模型**。為了讓證據來自產品程式（§14.2），改用產品的 `LocalPlayerPrediction`。兩版在 72 個情境的 Host 結果完全一致。
- **Client 的 Late 修正一度量成 0**。原因是 Client 的 Observation 在換 epoch 時歸零，而部分送達的跑次中途有 Backlog 換 epoch。改成從移動紀錄讀 Late 決定後，量到 3～6 次。這是量測方法的問題，不是產品的缺陷。
- **突變 `oc-client-samples-before-seed` 第一次是 wrong_failure**：拿掉 `settledAfter_` 的過濾後，outage 測試仍然通過，因為 SeedLead 同時清空了命令環。殺掉它的是既有的 2 個測試，所以把期待訊息改成那 2 個測試，並在目錄的說明裡寫明。這不是放寬判定：新突變第一次登記，期待訊息依實際的守衛修正。
- **證據夾**：`build/target/_build/test/logs/pvp-v71-outage-characterize-20261023/`。

## resync-contract

- **決定**：`gate-characterize` 的決定（D173）：T_int 0.5 s、N_o 20 Tick、D-15 (a)。
- **第一版條文**（`02f4b6d`）把計畫第 3 節寫成 §4 的狀態與規則、§7 的不變式。
- **xhigh 對抗式審查**：判定「必須修訂」（Critical 2、Major 8、Minor 12）。主會話對照程式，確認兩個 Critical 成立：
  - **C1**：`SeedLead` 會清空未確認的命令再重新產生（`LocalPlayerPrediction.cpp:72`）。在權威還沒追上 tip 時強制呼叫它，會用不同內容改寫還在途中的序號。這是計畫第 3.1 節第 3 步的設計錯誤：特性測試的 rig 每幀固定 1 Tick，沒有「Match 停頓、只追趕 5 步」的情境，所以沒有量到。
  - **C2**：「只壓掉觸發的那一筆樣本」、slack 下限、S3 等式三者互相矛盾。只有上行、暫存 0.6 s、冷卻期內的情境，一次送達的 34 個晚到裡約一半超過 20 Tick。
- **修訂**：全部處理，和計畫不同的地方寫在計畫第 13 節。最大的兩項：
  - 恢復時權威 < tip 就只重設相位的擷取，不呼叫 SeedLead；
  - N_o 改用時間判斷（值不變），晚到過久的第一次到達一律不產生樣本，計 `late_beyond_no`，S3 改成 `late_first == slack_late + late_beyond_no`。
- **新增的測試要求**（之後的批次）：
  - Match 停頓超過追趕上限時的恢復情境；
  - 冷卻期內暫存 0.6 s 的 slack 下限與 S3；
  - 以跳號製造靜默的改綁攻擊測試；
  - T_int／T_max 兩端相同的 fixture。
- **第二輪 xhigh 複核**（使用者要求）：判定「必須修訂」。
  - 上一輪的 C2、M1、M3、M6、M8 已解決，其餘部分解決。
  - 新的 Critical：只有 Hello 帶 token（`client_v7.proto:8`），Input 與 Actions 只在 `Admit` 比對 endpoint（`session.go:97-105`）。所以「通過 token 驗證的最後時刻」在正常遊玩中，每個 1 s 的 Hello 週期裡約有一半時間會被判成靜默，可以被改綁。主會話對照程式，確認成立。
  - 修正：改成 token 比對或 endpoint 比對都刷新。
  - Major：S3 在 `retention-600` 之前兩個方向都不保證；epoch 相同、權威 < tip 時，領先量留到 Backlog，列為 P2 的第二個例外。
  - Minor 8 項：見計畫第 13 節第 8 點。
- **第三輪 xhigh 複核**（使用者要求）：判定「可送核准」。沒有 Critical 或 Major，Minor 7 項在送 `gate-contract` 之前全部處理。審查者建議先修的是 m1：照字面，從其他位址送來、帶同一個 session id 的封包也會觸發 Gateway 的時鐘視窗重開，可以被外部用來灌爆 A 級日誌。
