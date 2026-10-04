# 第 04 批：量測基線 B0 與缺陷重現

狀態：進行中（2026-10-05 開始）。先讀 [進度](README.md)、[交接](HANDOFF.md)、[基線](BASELINE.md)、[第02批](02-acceptance-tools.md) 與 [第03批](03-authority-digest.md)。
依賴：第02批、第03批都已合併。

本批在固定的來源上，用第02批凍結的工具量出基線 B0，並重現 HANDOFF 第3、6項。
**無程式變更、不跑長測、不修任何缺陷。**

## 量測基線世代

- **B0＝本批**：v5 產品×第02批的新工具。
- IP-2 合併後，由[第07批](07-measurement-baseline-b1.md)用凍結工具重取 B1。
- 之後各批一律在自己的 base commit 上以凍結工具量 before／after 比較；本批只作歷史參照。
- v5 [穩定基線](../v5/STABLE_BASELINE.md)的數字只用來對門檻，不作回歸比較。

## 目標與範圍

### 做

1. **固定來源**：取第02、03批都合併後的 master commit，在獨立 worktree 量測。
   - 記錄 commit 與它包含哪些批次，包括已合併的 Engine 批次（IP-*、FF-*）。若其中有改變執行期行為的 Engine 批次，事前寫明。
   - 用第03批的 runner 記錄這個 commit 的權威 digest，與 `05042fa` 比較並記錄結果。
2. **開跑前**：凍結第02批的工具與產物（核對雜湊）、機器閒置、事前宣告跑次與分母。
3. **短測**（各一次，依事前宣告）：
   - 動作短測 30／60／144 FPS。
   - 人物短測。FPS 未達名義×0.85 依 D11③ 判 invalid：不計次，列出達成 FPS 與計時器分布，同跑次延遲門檻照判。
   - 雙 GUI 整合短測。
   - 25 案矩陣一次。
4. **第3項重現（L3，使用者在場）**：使用者拖動標題列、縮放視窗。
   - 記錄 `render_ms`、`frame_gap_ms`、Held 比例、連線品質窗口。
   - 產品只在第一個世界幀、render ≥250 ms 或 delta ≥0.25 秒時記 render log（`PvpApplication.cpp:938-942`）；長幀之外的分布只能由 probe 取得，報告註明。
   - 已知（v5 manual-2）：停頓全在 `render_ms` 內，例如 `render_ms=1199.0` 的幀之後，下一幀 `frame_gap_ms=1202.1`；兩個 client 合計 12 筆 `render_ms` ≥250 ms（362.8–1199.5 ms），全部 `presented=1`。
5. **第6項重現（L3，使用者在場）**：使用者依原操作重現死亡後第一人稱手臂仍可見。
   - 用第02批移入的逐幀 `submitted_meshes` 與 Presented／Skipped 欄位，加截圖。
   - 注意：產品在 `!Alive()` 時把 `submitted_meshes` 設為 0（`PvpApplication.cpp:516`），欄位為 0 不代表畫面上沒有手臂；以截圖為準。
6. **寫入 Engine 計畫**：第3項的重現摘要以文字寫進 [input-and-present 交接](../../../architecture/plans/input-and-present/HANDOFF.md)。
   - 只寫事實與數字；pvp 寫成「提出需求的消費端」，以文字摘要寫入（D1），不附連結（D12）。
   - 依 D12，Engine 文件不寫產品名、不連結 `docs/object_fps_pvp`；程式檔案的 file:line 與證據路徑可作為資料保留。
   - 這份重現是 IP-2 的消費端條件。

### 不做

- 不改程式，包括驗收工具；工具有問題就停下。
- 不跑長測、完整 GUI 三輪。
- 不修第3、6項；修正在 IP-2 與[第05批](05-first-person-arms-after-death.md)。

## 交付

- [基線](BASELINE.md)新增「量測基線 B0」節：來源 commit、包含的批次、工具雜湊、平台指紋（含計時器分布）、各短測結果、權威 digest。
- 第3、6項的可重跑重現步驟與結果；第6項若未重現，列出已排除的假設。
- input-and-present 交接的第3項文字摘要。
- 證據放 `build/target/_build/test/logs/pvp-v6-batch04-*`（git 忽略）。
- 本批 dev_log：`docs/dev_logs/YYYY_MM_DD_pvp_v6_batch04.zh-Hant.md`。

## 驗收點

- **L1（自動）**：凍結工具的雜湊與第02批記錄一致；權威 digest 的比較結果已記錄。
- **L2（實機）**：宣告的跑次全部完成；每輪通過，或失敗跑次已保留並完成有限定位；指紋含計時器分布。
- **L3（人工）**：第3、6項由使用者在場操作；重現步驟寫成可重跑的形式。

## 平台

| 平台 | 本批 | 說明 |
|---|---|---|
| macOS Intel／Metal 實機 | 預定執行 | 全部短測、矩陣、第3與第6項重現 |
| CI 四平台 L1（windows-x64、linux-x64、macos-arm64、macos-x64） | 未執行 | 本批無程式變更；文件 PR 觸發的 CI 不算本批量測 |
| Linux lavapipe GPU（CI 唯一跑 GPU 測試處） | 未執行 | pvp 沒有宣告 GPU 檢查（`checks.json` 的 `gpu` 為 false）；CI toolchain 列的共通 `render.*` 照常執行，但不作為本批驗收依據 |
| Windows D3D12 實機 | 未執行 | 依 D11⑩。第3項在 Windows 根因可能不同（拖動標題列會進入 Win32 modal 迴圈），本批的重現不代表 Windows |
| Linux 實機 | 未執行 | 依 D11⑩ |
| macOS arm64 實機 | 未執行 | 依 D11⑩ |

## 建議檔位

medium：執行並記錄結果，失敗時停下回報，不在本批修改。後續批次以本批的基線與重現為依據，所以不降到 low。

## Architecture Delta

無。不改程式，只新增量測紀錄與文件。

## 執行規則

- 每次只做使用者指定的批次；一批一個 PR，commit 與 PR 用日語。
- 開始、里程碑、停止時更新 [進度](README.md)、[交接](HANDOFF.md) 與 dev_log，然後停止，不自動開始下一批。
- 先凍結來源／產物／分析器再量測；開發與乾淨量測不同時進行。
- 失敗跑次保留，不覆寫、不自動重跑；長測另外授權。
- 執行規則沿用 v5／本計畫 README：只做指定範圍、先凍結再量測、失敗跑次保留、長測另外授權。

## 完成條件

- 宣告的跑次完成；B0 已寫入基線。
- 第3、6項的重現結果與步驟已記錄；第3項摘要已寫進 input-and-present 交接。

## 停止條件

- 凍結工具的雜湊不符，或分析器需要修改才能處理結果：停下回報，不在本批改工具。
- 跑次失敗：保留，有限定位後停下回報。
- 第3項未重現：記錄嘗試過的操作，停下回報；IP-2 的消費端條件因此不成立，由使用者決定下一步。
- 第6項未重現：列出已排除的假設，停下回報；第05批不執行。
- 權威 digest 與 `05042fa` 不同，且不能由已記錄的合併批次解釋：停下回報。
