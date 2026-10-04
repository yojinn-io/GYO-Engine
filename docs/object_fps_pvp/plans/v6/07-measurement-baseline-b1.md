# 第 07 批：量測基線 B1（IP-2 之後）

狀態：未開始。依賴 Engine IP-2 合併。
先讀 [進度](README.md)、[交接](HANDOFF.md)、[基線](BASELINE.md)、[第 04 批](04-measurement-baseline.md)，
以及 Engine 的 [input-and-present 計畫](../../../architecture/plans/input-and-present/PLAN.md)（IP-2）。

## 目標與範圍

IP-2 改變呈現的取得方式，幀節奏與 Skipped 頻率會跟著變。第 04 批的 B0 是在阻塞取得下量的，
IP-2 之後的差異無法再對 B0 歸因。本批用**凍結的工具**在 IP-2 合併後的來源重取基線 B1。

量測基線世代：

- B0＝第 04 批（v5 產品×新工具）。
- B1＝本批（IP-2 合併後的來源）。
- 之後各批一律在自己的 base commit 上以凍結工具量 before／after；B0、B1 只作歷史參照。
- v5 STABLE_BASELINE 的數字只用來對門檻。

做：

- 固定來源 commit：IP-2 合併後的 master commit；記錄它包含哪些 pvp 與 Engine 批次。
- 在獨立 worktree 量測；凍結來源／產物／分析器，記錄指紋；分析器指紋與 B0 不同時說明原因。
- 機器閒置；跑次、長度與事件分母事前宣告。
- 項目集合與 B0 相同：動作短測 30／60／144、人物短測、雙 GUI 整合短測、25 案矩陣一次。
- 每輪另列 Skipped 幀數與比例，與 B0 並列。
- B1 與 B0 的差異逐項列出並分類（幀節奏、Skipped、延遲、其他）；對門檻只用 v5 STABLE_BASELINE。

不做：

- 無程式變更（產品、Engine、驗收工具都不改）。
- 不跑長測（另外授權）。
- 不判定 IP-2 是否完成；IP-2 的 Engine 端驗收屬 Engine 計畫。
- 第3項的拖動／縮放重現不在本批重做；那是 IP-2 的 L2。

## 交付

- [基線](BASELINE.md) 新增 B1 一節：來源 commit、包含的批次、工具與產物指紋、平台指紋、各項結果、與 B0 的差異表。
- 原始資料保留在證據目錄，路徑寫入 BASELINE。
- 本批 dev_log（`docs/dev_logs/YYYY_MM_DD_pvp_v6_batch07.zh-Hant.md`），README／HANDOFF 更新。
- 消費端結果以文字摘要寫回 Engine 的 [input-and-present HANDOFF](../../../architecture/plans/input-and-present/HANDOFF.md)。

## 驗收點

- L1 自動：不適用（無程式變更）。權威不變的證明由來源內各批自行負責，本批不另做。
- L2 實機：macOS 上依事前宣告完成全部項目；結果、指紋與原始資料齊全；未測、受干擾、缺檔不算通過。
- L3 人工：不適用（本批不含人工操作）。

## 平台

| 平台 | 本批 |
|---|---|
| macOS Intel／Metal 實機 | 預定執行（全部量測） |
| CI 四平台（windows-x64、linux-x64、macos-arm64、macos-x64）L1 | 未執行（無程式變更，沒有新的 L1） |
| Linux lavapipe GPU（CI 唯一跑 GPU 測試處） | 未執行：pvp 沒有宣告 GPU 檢查（checks.json 的 gpu 為 false）；CI toolchain 列的共通 render.* 照常執行，但不作為本批驗收依據 |
| Windows D3D12 實機 | 未執行（v6 實機只有 macOS Intel，D11⑩）；B1 不代表 Windows 的幀節奏 |
| Linux 實機 | 未執行（同上） |
| macOS arm64 實機 | 未執行（同上） |

## 建議檔位

建議 medium。內容是照事前宣告執行量測並記錄；分析器已凍結，不需要設計判斷。
沒有 CI 這類外部驗證，因此不降到 low。依 D8，開始時說明檔位。

## 依賴

- IP-2 合併（經 IP-2 間接依賴 FF-2）。
- 第 02 批的凍結分析器與第 04 批的 B0（作為對照）。

## Architecture Delta

無。本批只量測，不改程式、Build、Data Contract 或 Ownership。

## 執行規則（沿用 v5）

執行規則沿用 v5／本計畫 README：只做指定範圍、先凍結再量測、失敗跑次保留、長測另外授權。

- 只做使用者指定的本批；一批一個 PR，commit 與 PR 用日語。
- 開始、里程碑、停止時更新 README／HANDOFF／dev_log。
- 先凍結來源／產物／分析器再量測；開發與乾淨量測不同時進行。
- 長測另外授權。

## 完成與停止

完成條件：

- 事前宣告的項目全部有結果，B1 一節與差異表寫入 BASELINE。
- 更新 README／HANDOFF／dev_log 與 Engine HANDOFF 的消費端摘要後停止，不自動開始下一批。

停止條件：

- 分析器需要修改才能處理常見的 Skipped：停下回報。依 D13，IP-2 的 L2 必須在合併前完成；
  若 IP-2 的 L2 階段就需要修改分析器，處理方式是 IP-2 停下、先在 IP-2 的 base commit 上（仍是阻塞取得）
  做驗收工具修正批，再回到 IP-2 跑 L2，不會走到本批。本條只適用於 **IP-2 的 L2 已通過，但重取 B1 時才暴露問題**：
  本批停下，工具修正批由使用者另行指定，不在本批修改工具；修正後以新凍結的工具重取 B1，並在 Engine HANDOFF 記錄這件事。
- 結果超出 v5 門檻：保留全部跑次，有限定位後停下回報；不重跑到通過為止。
- 失敗、受干擾或缺檔的跑次保留，不覆寫。
