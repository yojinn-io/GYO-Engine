# PvP v7.1 outage-resync：斷線恢復＝重新對時

日期：2026-10-23 起。分支 `claude/pvp-v71-outage-resync`（從 master `cdb9b07`）。

**狀態：進行中**。計畫已核准（D172）。`outage-characterize` 完成，等關卡 G1 的決定。

相關文件：

- 計畫：[outage-resync.md](../object_fps_pvp/plans/v7.1/outage-resync.md)。
- 決策 D171 與設計原則：v7.1 記錄器分支 `claude/pvp-v71-after-re-evaluate` 的 HANDOFF。

## 批次

| 批次 | 檔位 | 做了什麼 | 驗收 |
|---|---|---|---|
| `plan-commit` | medium | 提交計畫草案第 2 版與本紀錄 | 只改文件 |
| `outage-characterize`（`c200992` 等） | high | 用測試固定中斷與晚到的現狀：Host 側接上產品的 `LocalPlayerPrediction`，注入雙向、只上行、只下行、部分送達的中斷；Client 側 3 個；稀疏替代 1 個；語料唯讀掃描。產品程式不改 | cpu 測試 293／293；突變 4／4 killed；L1 run 39944311104；報告：[outage-characterize-report.md](../object_fps_pvp/plans/v7.1/outage-characterize-report.md) |

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
