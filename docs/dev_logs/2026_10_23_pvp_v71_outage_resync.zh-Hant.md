# PvP v7.1 outage-resync：斷線恢復＝重新對時

日期：2026-10-23 起。分支 `claude/pvp-v71-outage-resync`（從 master `cdb9b07`）。

**狀態：規劃中**（草案第 2 版，等使用者核准）。

相關文件：

- 計畫：[outage-resync.md](../object_fps_pvp/plans/v7.1/outage-resync.md)。
- 決策 D171 與設計原則：v7.1 記錄器分支 `claude/pvp-v71-after-re-evaluate` 的 HANDOFF。

## 批次

| 批次 | 檔位 | 做了什麼 | 驗收 |
|---|---|---|---|
| `plan-commit` | medium | 提交計畫草案第 2 版與本紀錄 | 只改文件 |

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
