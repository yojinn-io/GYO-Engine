# 第 03 批：權威 digest 閘門

狀態：未開始。先讀 [進度](README.md)、[交接](HANDOFF.md)、[基線](BASELINE.md) 與 [第01批](01-plan-and-baseline.md)。
依賴：第01批（與第02批平行）。

本批建立「權威不變」的量測工具：逐 Tick 權威狀態 digest，以及同機兩樹比對。
後續第04、09、10批與 Engine 的 FF-8、FF-9 都用它證明權威結果有沒有改變。
**不改產品邏輯、不改 wire、不改權威結果。**

## 背景

- Math 計畫 B6a 已用同樣方法證明權威不變：5 個 arena、62 個情境、規模 1 與 10，-O2／-O0 一致；
  但 harness 只在 scratch，不在 repo（見 [Math 交接](../../../architecture/plans/math-foundation/HANDOFF.md) B6a「全模擬 digest」）。
- 權威路徑每一步移動都呼叫 `std::sin`／`std::cos`（`PlanarMovement.cpp:17-18`、`ShotQuery.cpp:26-28`），
  結果依賴各平台 libm，不能做跨平台逐位元 golden。

## 目標與範圍

### 做

1. **情境 runner**（`tests/object_fps_pvp`）：把 B6a 的情境設計收進 repo。
   - 涵蓋：移動、跳躍、貼牆滑行、射擊命中與擦邊（含牆遮擋）、死亡、重生、prediction＋虛擬線路。
   - 情境清單、arena、規模在開始時事前宣告；規模 10 的完整 trace（B6a 為 3.86 GB）不進 repo、不進 CI。
2. **digest 輸出**：逐 Tick 權威狀態 digest；不一致時報告第一個分歧的 Tick 與欄位。
   - digest 包含的欄位明列成清單並加版本。第09批要算「不含新欄位」的 digest，第10批要宣告會變的情境集合，都依這份清單。
3. **兩樹比對腳本**（正式證明）：同一台機器建置 base 與 branch 兩棵樹，逐情境比對 digest。
   - 腳本放在產品 owner 的 `tests/object_fps_pvp` 或 `build/acceptance/object_fps_pvp`，不放 `docs/`。
4. **CI 自洽**（不是正式證明）：
   - -O0／-O2 比較：在 `tests/object_fps_pvp` 用產品自有的第二個編譯 target（以 -O0 重新編譯 domain 來源），或只在本機執行。二擇一，開始時寫明理由。
   - golden 只放不依賴 libm 的子集：yaw＝pitch＝0、軸向牆。範圍寫進測試與本文件。
   - 依賴 `sinf`／`cosf` 的情境只做自洽與 characterization，不放 golden。
5. **記錄 v5 樹的 digest**：在 `05042fa`（目前產品仍為 v5）上跑一次，結果寫進 [基線](BASELINE.md)。

### 不做

- 不修改共通 workflow（`.github/workflows/*`）；刪除 pvp 時不得需要改它（AGENTS §7）。
- 不做每平台各一份的 golden（等於依 OS 分支）。
- 不改產品邏輯、Collision、Math。
- 不為比對而公開產品或 Engine 的內部函式。

## 開始時確認

- FF-8 以本批證明「標頭改由 Engine 編解碼後行為不變」。若情境不經過 `Wire.hpp` 的編解碼，本批只能證明權威行為，
  不能證明位元組；位元組由 FF-8 的 golden 向量負責。開始時寫明本批情境是否經過 wire 編解碼。

## 交付

- 情境 runner、digest 欄位清單、兩樹比對腳本、CI 自洽測試（含 golden 子集）。
- `05042fa` 的 digest 紀錄（[基線](BASELINE.md)）。
- 本批 dev_log：`docs/dev_logs/YYYY_MM_DD_pvp_v6_batch03.zh-Hant.md`。

## 驗收點

- **L1（自動）**：
  - `ctest -L pvp` 通過；CI 四平台通過。
  - base 與 branch 相同時，兩樹比對全部一致。
  - 靈敏度：對 Movement 注入 1 ULP 擾動能被抓到，且指出正確的 Tick；其他刻意改動（事前宣告種類與數量）的捕捉數達到宣告。
  - golden 子集在四平台都一致；子集範圍與測試內的宣告相符。
- **L2（實機）**：無 GUI。兩樹比對在 macOS Intel 本機執行並記錄。
- **L3（人工）**：無。

## 平台

| 平台 | 本批 | 說明 |
|---|---|---|
| macOS Intel／Metal 實機 | 預定執行 | 兩樹比對、`05042fa` digest、-O0／-O2 本機比較；不涉及 Metal |
| CI 四平台 L1（windows-x64、linux-x64、macos-arm64、macos-x64） | 預定執行 | 自洽與 golden 子集；macos-x64 經 Rosetta 2 |
| Linux lavapipe GPU（CI 唯一跑 GPU 測試處） | 未執行 | pvp 沒有宣告 GPU 檢查（`checks.json` 的 `gpu` 為 false）；CI toolchain 列的共通 `render.*` 照常執行，但不作為本批驗收依據 |
| Windows D3D12 實機 | 未執行 | 依 D11⑩；權威正式證明只在同機兩樹 |
| Linux 實機 | 未執行 | 同上 |
| macOS arm64 實機 | 未執行 | 同上 |

## 建議檔位

- 主體 high：要讀懂 Match 的 Tick 順序與狀態，有 CI 與靈敏度測試做外部驗證。
- golden 策略與情境選擇局部升 xhigh：策略錯了，之後所有「權威不變」的證明都會失效，而且不會報錯。處理完降回 high。
- 檔位高於主對話時，開始時說明並徵求同意（D8）。

## Architecture Delta

無。新增的 runner、fixture 與腳本都屬 `object_fps_pvp` owner，只在選取該 owner 時組入；不改共通 workflow、不新增依賴邊。

## 執行規則

- 每次只做使用者指定的批次；一批一個 PR，commit 與 PR 用日語。
- 開始、里程碑、停止時更新 [進度](README.md)、[交接](HANDOFF.md) 與 dev_log，然後停止，不自動開始下一批。
- 合併順序：本批與第02批都合併後，第04批才固定來源 commit。
- 執行規則沿用 v5／本計畫 README：只做指定範圍、先凍結再量測、失敗跑次保留、長測另外授權。

## 完成條件

- L1 全部通過；靈敏度結果與事前宣告一致。
- golden 子集範圍、digest 欄位清單、-O0／-O2 的執行方式都寫進本批 dev_log。
- `05042fa` 的 digest 已記錄。

## 停止條件

- 同一樹在同機重跑 digest 不一致（非決定性）：停下，有限定位後回報，不放寬比對。
- -O0 與 -O2 不一致：停下回報；這可能是產品既有問題，本批不修。
- 要達成 CI 自洽必須修改共通 workflow：停下，改為只在本機執行並回報。
- 靈敏度未達宣告：停下回報，不事後降低宣告。
