# 第 14 批：整合驗收與升格

狀態：14a 進行中（2026-10-07 開始，分支 `claude/pvp-v6-batch14` 自 master `5972451`；開始時的決定見 HANDOFF 的 D24）；14b 未開始。分 14a（整合短測、移除檢查、驗收交付）與 14b（完整驗收與升格，**需另外授權**）。
先讀 [進度](README.md)、[交接](HANDOFF.md)、[基線](BASELINE.md) 與 [v6 契約](../../protocol-v6.zh-Hant.md)。

門檻依 v6 契約（與 v5 相同，不放寬）。v5 的歷史授權不套用到 v6。

執行規則沿用 v5／本計畫 README：只做指定範圍、先凍結再量測、失敗跑次保留、長測另外授權。

## 14a：整合短測、產品移除檢查與驗收交付

### 目標與範圍

做：

- 開跑前：凍結來源、產物與分析器；事前宣告跑次、事件分母與輸出目錄。
- 25 案矩陣、雙 GUI 整合短測、長測短模式。
- 4 人案例：第 16 批的 quad 案例與 1 GUI＋3 bot（D23⑤）。2 個 Client 的矩陣不變。GUI 短測也可以用 `--bots` 在有 bot 的房間執行（第 16 批的修訂），要不要納入在本批開始時決定。
- 產品移除檢查（AGENTS §7）：在 scratch worktree 刪除 pvp，確認其餘仍成立（見下）。
- `MANUAL_ACCEPTANCE.md`、`ACCEPTANCE_STATUS.md`（v6 目錄內）依平台分欄；未執行的平台標「未執行」。
- 確認兩個 Engine 計畫的狀態：[input-and-present](../../../architecture/plans/input-and-present/HANDOFF.md)、
  [foundation-followups](../../../architecture/plans/foundation-followups/HANDOFF.md)。結案由各自 owner 在各自 HANDOFF 記錄，
  不屬本批；本批只確認狀態並寫進 dev_log。

不做：

- 完整 GUI 三輪與 1808 秒長測（→14b）。
- 程式修正：失敗時保留跑次，有限定位後回報。
- 不重新量測權威決定性：權威 golden 在[第10批](10-collision-authority.md)確認不變並凍結，本批只確認[第03批](03-authority-digest.md)閘門通過。
- 不改 wire（候選期規則 D11①）。

### 產品移除檢查

在 scratch worktree（不動本 worktree）刪除：

- `apps/object_fps_pvp`、`assets/object_fps_pvp`、`tests/object_fps_pvp`、`build/acceptance/object_fps_pvp`、`docs/object_fps_pvp`。
- 登錄列：`engine/config/projects.csv` 的 `object_fps_pvp` 列，以及其他指向 pvp 的登錄資料。

確認：

- Engine、ui_editor 與共通測試的建置與 CTest 通過（v5 先例：`build.ci` 須在 git repo 內）。
- 共通層沒有指向 pvp 的 Reference；沒有 Dummy Target、空目錄或 Compatibility Branch。
- Engine 計畫文件沒有失效連結（決定 D12：Engine 文件不得連到 `docs/object_fps_pvp`）。明確的檢查：
  grep `docs/architecture` 與 `docs/checkup`，不得有指向 `docs/object_fps_pvp` 的連結。
  例外只限歷史 dev_log 與 checkup 報告中已有的歷史提及，須逐條列出（檔案:行與理由）寫進證據目錄與 dev_log。
- Engine 計畫文件刪除後內容仍可理解：引用消費端證據的地方有文字摘要（程式與證據路徑作為資料可保留）。
- `docs/dev_logs/*_pvp_*` 依 D11⑪ 視為歷史紀錄保留，不列入刪除集合。
- 結果與刪除檔數記在證據目錄與 dev_log。

### 交付

- 證據目錄（來源／產物／分析器指紋、事前宣告、原始資料、報告）。
- `MANUAL_ACCEPTANCE.md`、`ACCEPTANCE_STATUS.md`（依平台分欄）。
- 第3項以本批 L3 通過為前提，在狀態表寫：「macOS Intel／Metal 已驗證；Windows／Linux 未執行，根因可能不同（Windows 拖動標題列會進入 Win32 modal 迴圈）」。
  實際是否已驗證，以本批 L3 結果為準；未通過就不寫「已驗證」。
- 兩個 Engine 計畫狀態的確認結果（dev_log；結案紀錄由各自 owner 寫在各自 HANDOFF，含 `inactive_products.md` 遷移清單）。
- dev_log：`docs/dev_logs/YYYY_MM_DD_pvp_v6_batch14a.zh-Hant.md`。

### 驗收點

L1（自動）：

- CTest 全標籤通過；CI 四平台通過。
- 第03批權威 digest 閘門通過（同機兩樹比對為正式證明；CI 只做自洽與不依賴 libm 的 golden 子集）。
- 移除檢查：刪除後的建置與 CTest 通過。

L2（實機）：

- 25 案矩陣：clean-30 以外的 24 案 24／24；clean-30 照跑並記錄（D24①，D21 的設計範圍邊界，由 v7 處理）。各案以 `--case` 分別執行（D24②）。
- 雙 GUI 整合短測通過；FPS invalid 依 D11③（未達名義×0.85 不計次，報告達成 FPS 與計時器分布，
  同跑次延遲門檻照判；有效輪不足標「未驗證」，不算通過）。
- 長測短模式通過。

L3（人工）：使用者跑原生操作清單，涵蓋第1項（俯仰）、第2項（受擊）、第4項（連點）、第6項（死亡後手臂）與視窗拖動／縮放（第3項）。

### 量測規則

- 先凍結來源／產物／分析器再量測；機器閒置，不與編譯同時進行。
- 量測基線世代：在本批 base commit 上以凍結工具量測；[第04批](04-measurement-baseline.md)（B0）與
  [第07批](07-measurement-baseline-b1.md)（B1）只作歷史參照；v5 STABLE_BASELINE 的數字只用來對門檻。
- 失敗、受干擾、缺檔都不是通過；保留首份失敗與全部原始資料，不覆寫、不無限重跑。

### 平台

| 平台 | 14a |
|---|---|
| macOS Intel／Metal 實機 | 預定執行（L2 矩陣、雙 GUI 短測、長測短模式、L3） |
| CI 四平台（windows-x64、linux-x64、macos-arm64、macos-x64）L1 | 預定執行 |
| Linux lavapipe GPU | 未執行：pvp 沒有宣告 GPU 檢查（`checks.json` 的 `gpu` 為 false）；CI toolchain 列的共通 `render.*` 照常執行，但不作為本批驗收依據 |
| Windows D3D12 實機 | 未執行：沒有實機（D11⑩）；第3項在 Windows 的根因可能不同 |
| Linux 實機 | 未執行：沒有實機（D11⑩）；`run_native_window.py`（Linux X11）未授權 |
| macOS arm64 實機 | 未執行：沒有實機（D11⑩） |

### 建議檔位

medium。執行並記錄結果；機器需閒置。

### 依賴

第 02～13 批（含 02a／02b／02c）與第 16 批（房間上限 4 人）已完成，或經使用者決定不執行（例如第04批沒有重現第6項時的第05批）。
Engine 計畫 [IP-1、IP-2](../../../architecture/plans/input-and-present/PLAN.md)、[FF-1…FF-9](../../../architecture/plans/foundation-followups/PLAN.md)
的狀態由本批確認並記錄，不由本批結案。

### Architecture Delta

無。本批只執行驗收與彙整各批已記錄的 Delta，不改程式。移除檢查發現的結合問題只報告，不在本批修復。

### 完成條件與停止條件

完成：

- L1、L2、L3 全部有結果並寫入 `ACCEPTANCE_STATUS.md`；移除檢查通過；兩個 Engine 計畫的狀態已確認並記錄。
- 更新 [進度](README.md)、[交接](HANDOFF.md)、dev_log 後停止。交付狀態是「v6 短測與驗收交付完成」，**不是升格**。
- 一批一個 PR；commit 與 PR 用日語。

停止：

- 任何 L1／L2 失敗：保留跑次，有限定位後停下回報，不在本批修正。
- 移除檢查失敗（共通層需要修改才能刪除 pvp）：停下，以 Architecture Delta 形式回報，不在本批修復。
- 不自動開始 14b。

## 14b：完整驗收與升格（需另外授權）

### 目標與範圍

使用者明確授權後才執行。14a 完成不等於授權。

做：

- 完整 GUI 三輪（沿用 v5 規模：各至少 120 秒／200 個預先登記移動事件，死亡／重生窗口預先排程）。
- Headless 長測 60 Hz 與 144 Hz 各 113 循環（1808 秒），串行執行，顯式長測參數，不自動重跑。
- 4 人：1 GUI＋3 bot 一輪（D23⑤）。長測維持 2 個 Client，以便和過去比較。
- 全部通過才寫 `STABLE_BASELINE.md`（v6 目錄內）並更新契約狀態行。

不做：

- 不放寬門檻；長測中的時間抖動以[第02批](02-acceptance-tools.md)（02b）的計時器基線解讀，只供解讀，不當門檻。
- 不以重跑覆蓋失敗或中斷的跑次。
- 不改程式。

### 交付

- 完整驗收證據目錄、`ACCEPTANCE_STATUS.md` 更新、`STABLE_BASELINE.md`（只在全部通過時）。
- dev_log：`docs/dev_logs/YYYY_MM_DD_pvp_v6_batch14b.zh-Hant.md`。

### 驗收點

- L1：同 14a 的來源與產物指紋未變（變了就回到 14a）。
- L2：GUI 三輪與兩組長測依 v6 契約門檻判定。
- L3：GUI 三輪中的人工操作依手動指南。

### 量測規則

- 凍結來源／產物／分析器，與 14a 相同；機器閒置，約 1.5 小時以上。
- 執行工具的背景時限須大於 1808 秒（v5 的 144 Hz 第一次跑在約 1800 秒被 30 分鐘背景上限中止，判無效）。
- 失敗與中斷的跑次全部保留；補驗前先分析，再決定。

### 平台

| 平台 | 14b |
|---|---|
| macOS Intel／Metal 實機 | 授權後預定執行 |
| CI 四平台 L1 | 預定執行（與 14a 同一來源） |
| Linux lavapipe GPU | 未執行：pvp 沒有宣告 GPU 檢查（`checks.json` 的 `gpu` 為 false）；CI toolchain 列的共通 `render.*` 照常執行，但不作為本批驗收依據 |
| Windows D3D12 實機 | 未執行：沒有實機（D11⑩） |
| Linux 實機 | 未執行：沒有實機（D11⑩） |
| macOS arm64 實機 | 未執行：沒有實機（D11⑩） |

平台範圍若要擴大，依使用者授權另訂。

### 建議檔位

medium。執行與記錄。

### 依賴

14a 完成，並取得使用者授權。

### Architecture Delta

無。只執行驗收與寫入基線文件。

### 完成條件與停止條件

完成：全部通過，寫入 `STABLE_BASELINE.md`；更新 [進度](README.md)、[交接](HANDOFF.md)、dev_log 後停止。

停止：

- 任一項失敗或中斷：不升格；保留全部跑次，有限定位後回報。
- 來源或產物指紋與 14a 不同：停止，回到 14a。
