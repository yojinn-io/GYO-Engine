# 第 05 批：死亡後第一人稱手臂仍在（第6項）

狀態：未開始。依賴第 04 批完成；第 04 批未重現第6項時，本批不執行。
先讀 [進度](README.md)、[交接](HANDOFF.md)、[基線](BASELINE.md) 與 [第 04 批](04-measurement-baseline.md)。

## 目標與範圍

v5 第04批 L3 中使用者回報「自己死亡時，持槍手臂還是在」。本批依第 04 批的重現步驟定位根因，再修正。
**先有重現與失敗的斷言，才改程式；不猜測修正。**

做：

- 依第 04 批的重現紀錄（使用者在場的原操作、02a 移入存續 probe 的逐幀 `submitted_meshes`、截圖）定位根因。
- 已排除：本機人物不經 `players->Submit` 提交（`apps/object_fps_pvp/src/Pvp/PvpApplication.cpp:567` 跳過自己）。
- 現況：`SubmitWeapon` 在 `!Alive()` 時不提交並設 `submittedMeshes`＝0（`PvpApplication.cpp:516`）；
  `Alive()` 讀最新 Snapshot（`PvpApplication.cpp:151-154`）。
- 限制：`weaponFeedback.dead`（`PvpApplication.cpp:341`）與 `:516` 的 `!Alive()` 讀同一份 Snapshot，
  所以 02a 的「Dead 且 `submitted_meshes`>0」不可能出現，這個斷言在修正前也會通過，不能當作本批的失敗斷言
  （第 02、04 批已記錄同一限制）。
- 待查（依證據逐項排除，不並行修改）：
  - Skipped 幀保留上一張畫面（Presented 判定在 `PvpApplication.cpp:943` 之後）。
  - 加入前的 warmup（`PvpApplication.cpp:740-750`）提交的 viewmodel。
  - ViewModel camera／圖層狀態。
  - `Alive()` 讀 Snapshot 的時點與 `RefreshState` 的順序。
  - `lifeBoundaryThisFrame`（`PvpApplication.cpp:278,326`）。
- 修正只在 pvp Client 呈現內。

不做：

- 遠端屍體上的世界手槍（`apps/object_fps_pvp/src/Pvp/PlayerPresentation.cpp:464-468` 掛上武器、`:512` 提交）。
  屬新需求，記入 [交接](HANDOFF.md) 候選，不在本批處理。
- 不改 wire、Match、權威邏輯。
- 不改 Engine。根因在 Engine render 時停下（見停止條件）。
- 不改驗收工具。02a 的欄位與突變沿用；工具若需修改，停下回報。

## 交付

- 根因說明：重現步驟、證據（逐幀欄位、截圖、日誌片段）、排除過的假設與理由。
- pvp Client 呈現的最小修正。
- L1 斷言：找到根因後、開始實作前，事前宣告一個能在修正前失敗的觀測量
  （例如不經過 `Alive()` 的 render queue 層 ViewModel 提交計數），以它斷言本機 Dead 時每一個 Presented 幀都沒有提交手臂；
  可見與否以截圖為準。
- 本批 dev_log（`docs/dev_logs/YYYY_MM_DD_pvp_v6_batch05.zh-Hant.md`），README／HANDOFF 更新。

## 驗收點

- L1 自動：
  - 事前宣告的觀測量在修正前失敗、修正後通過；拿掉修正的突變必須讓它失敗。
  - 不使用 `submitted_meshes`（它經過 `Alive()`，修正前也會是 0）作為失敗斷言。
  - 優先做成不需 GPU 的 CPU 測試。若觀測量只能經 GUI probe 取得，本批可以在 probe 加入這一個事前宣告的欄位（不改既有判定），開始時說明；需要其他驗收工具修改時，依停止條件停下回報。
  - pvp CPU 測試全部通過。
  - 權威不變：正式證明為同機兩樹（base／branch）以 [第 03 批](03-authority-digest.md) 的 runner 比對 digest，
    預期完全相同。CI 只做自洽與不依賴 libm 的 golden 子集。不修改共通 workflow。
- L2 實機：macOS 死亡與重生的截圖；在本批 base commit 與 branch 上以凍結工具量 before／after
  （動作短測、人物短測，跑次事前宣告）。第 04 批的 B0 只作歷史參照；門檻只對 v5 STABLE_BASELINE 的數字。
- L3 人工：使用者重做第 04 批的原操作，確認死亡期間手臂不可見、重生後恢復。

## 平台

| 平台 | 本批 |
|---|---|
| macOS Intel／Metal 實機 | 預定執行（L2 截圖、before／after 短測、L3） |
| CI 四平台（windows-x64、linux-x64、macos-arm64、macos-x64）L1 | 預定執行 |
| Linux lavapipe GPU（CI 唯一跑 GPU 測試處） | 未執行：pvp 沒有宣告 GPU 檢查（checks.json 的 gpu 為 false）；CI toolchain 列的共通 render.* 照常執行，但不作為本批驗收依據 |
| Windows D3D12 實機 | 未執行（v6 實機只有 macOS Intel，D11⑩） |
| Linux 實機 | 未執行（同上） |
| macOS arm64 實機 | 未執行（同上） |

## 建議檔位

建議 high。要先讀懂呈現提交、生命邊界與 Skipped 路徑；有 L1 斷言與 L3 人工確認做外部驗證。
不升 xhigh。依 D8，開始時說明檔位；高於主對話的檔位需使用者同意。

## 依賴與合併順序

- 依賴第 04 批（重現證據、B0）。第 04 批依賴第 02、03 批，因此 02a 的欄位與第 03 批的 digest runner 已存在。
- `PvpApplication.cpp` 的建議合併順序：05→06→12→11→13。這只是建議，用來減少衝突與方便歸因，不是依賴；
  順序不同時由後合併的一方 rebase。本批不執行時，記錄後略過。

## Architecture Delta

無。修正限於 pvp Client 呈現內部，不新增依賴、不改 Ownership 與 Data Contract。
根因若在 Engine，屬 Delta，不在本批處理（見停止條件）。

## 執行規則（沿用 v5）

執行規則沿用 v5／本計畫 README：只做指定範圍、先凍結再量測、失敗跑次保留、長測另外授權。

- 只做使用者指定的本批；一批一個 PR，commit 與 PR 用日語。
- 開始、里程碑、停止時更新 README／HANDOFF／dev_log。
- 先凍結來源／產物／分析器再量測；開發與乾淨量測不同時進行。
- 長測另外授權；本批不跑長測。

## 完成與停止

完成條件：

- 根因有證據；修正前失敗、修正後通過的 L1 斷言與突變都在。
- 同機兩樹 digest 相同；CI 通過；before／after 結果寫入 dev_log。
- L3 由使用者確認。
- 更新 README／HANDOFF／dev_log 後停止，不自動開始下一批。

停止條件：

- 第 04 批未重現：本批不執行，回報使用者。
- 根因在 Engine render（例如 Skipped 時殘留畫面）：停下，帶證據轉到 Engine 的
  [input-and-present](../../../architecture/plans/input-and-present/PLAN.md)（IP-2）重新規劃。
- 修正需要改 wire、Match 或驗收工具：停下回報。
- 修正前無法以任何事前宣告的觀測量重現失敗：停下回報，不提交修正。
- 失敗跑次保留，有限定位後停下；不覆寫、不無限重跑。
