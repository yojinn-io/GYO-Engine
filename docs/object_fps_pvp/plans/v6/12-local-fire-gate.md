# 第 12 批：本機射擊冷卻閘

狀態：未開始。對應 v6 [交接](HANDOFF.md)第4項。
先讀 [進度](README.md)、[交接](HANDOFF.md)、[基線](BASELINE.md) 與 [v6 契約](../../protocol-v6.zh-Hant.md)。

執行規則沿用 v5／本計畫 README：只做指定範圍、先凍結再量測、失敗跑次保留、長測另外授權。

## 目標與範圍

v5 第04批開發實跑：間隔剛好 12 Tick 的點擊，在幀抖動下會在本機被擋。
權威的 10 Tick 冷卻本身正確（`PvpMatch.cpp:296-312`）。

本機有兩道閘：

- Tick 閘：`PvpApplication.cpp:445` 以最新 Snapshot 的 Tick 比對 `nextAllowedShotTick`；該 Tick 約比權威晚 2 Tick。
- 牆鐘閘：`localCooldownUntil`（`:442` 判斷、`:455` 設定為送出後 10 Tick 的牆鐘時間）。

做（依 D11⑤）：

- 以 Client 已有的相位追蹤（`Movement.hpp:36-52` 的 `MovementPhase*`）估計權威 Tick，補償落差並保留安全邊際。
- 兩道閘採一致的判定基準，不得一道放行、另一道擋下。
- 分開記錄「本機擋下」與「權威拒絕」。
- 「冷卻點擊不排隊」的設計不變。

不做：

- 不改權威的 10 Tick 冷卻、不改 Match。
- 不改 wire。
- 不改其他動作（換彈、移動）的送出規則。

## 交付

- 兩道閘的補償實作與安全邊際常數（寫明理由與來源）。
- 本機擋下與權威拒絕的分開計數（Client 診斷與驗收報告）。
- L1 測試、L2 證據、L3 回報。
- dev_log：`docs/dev_logs/YYYY_MM_DD_pvp_v6_batch12.zh-Hant.md`。

## 驗收點

L1（自動）：

- 用[第02批](02-acceptance-tools.md)（02b）的計時器晚醒分布合成幀抖動，分別記錄：
  間隔 12 Tick 的點擊被本機擋下的比例、權威冷卻拒絕率。
- 乾淨跑次的權威冷卻拒絕必須為 0。
- 兩道閘一致：同一組輸入下，兩道閘的放行結果相同。
- 突變檢查：補償多 1 Tick（或去掉安全邊際）時，權威冷卻拒絕必須 >0，測試必須失敗。

L2（實機）：

- 動作短測 30／60／144 FPS，在本批 base commit 上以凍結工具量 before／after。
- 報告分列本機擋下與權威拒絕；不得以本機擋下掩蓋合法操作失敗。

L3（人工）：使用者確認連點手感（12 Tick 左右的節奏不再被吃掉）。

## 量測與權威規則

- 量測基線世代：在本批的 base commit 上，以凍結工具量 before／after 比較。
  [第04批](04-measurement-baseline.md)（B0）只作歷史參照；v5 STABLE_BASELINE 的數字只用來對門檻。
  IP-2 已合併時，before 必須在 IP-2 之後的 base 上重量。
- 先凍結來源／產物／分析器再量測；開發與乾淨量測不同時進行。
- 權威不變：本批不碰權威路徑。正式證明是同機兩樹（base／branch）digest 比對（[第03批](03-authority-digest.md)）；
  CI 只做自洽與不依賴 libm 的 golden 子集。不修改共通 workflow。
- 若本批在[第09批](09-protocol-v6.md)之後執行：候選期原則上不改 wire（D11①）。

## 平台

| 平台 | 本批 |
|---|---|
| macOS Intel／Metal 實機 | 預定執行（L2 動作短測、L3） |
| CI 四平台（windows-x64、linux-x64、macos-arm64、macos-x64）L1 | 預定執行 |
| Linux lavapipe GPU | 未執行：pvp 沒有宣告 GPU 檢查（`checks.json` 的 `gpu` 為 false）；CI toolchain 列的共通 `render.*` 照常執行，但不作為本批驗收依據 |
| Windows D3D12 實機 | 未執行：沒有實機（D11⑩） |
| Linux 實機 | 未執行：沒有實機（D11⑩） |
| macOS arm64 實機 | 未執行：沒有實機（D11⑩）。計時器晚醒分布依平台不同，L1 的合成抖動只用已量到的分布 |

## 建議檔位

high；Tick 估計與安全邊際局部 xhigh。
Tick 估計屬時序邏輯，補償過頭會靜默增加權威拒絕；其餘部分有 L1 與 L3 做外部驗證。
xhigh 若高於主對話檔位，本批開始時說明並徵求同意（D8）。

## 依賴與合併順序

依賴：

- [第04批](04-measurement-baseline.md)（B0；02b 的計時器分布）。

合併順序（建議）：

- 合併順序只是建議，用來減少衝突與方便歸因，不是依賴；後合併的一方 rebase。
- `PvpApplication.cpp` 的建議合併順序：05→06→**12**→11→13。比本批先合併的是[第05批](05-first-person-arms-after-death.md)與
  [第06批](06-input-migration.md)；順序不同時由後合併者 rebase，不因此等待。

## Architecture Delta

無。只改產品 Client 內部的操作閘；不新增 Dependency Edge，不改 wire、Match 或權威規則。

## 完成條件與停止條件

完成：

- L1 全過（含突變；乾淨跑次權威冷卻拒絕＝0）；L2 before／after 報告分列兩種計數；L3 使用者確認。
- 更新 [進度](README.md)、[交接](HANDOFF.md)、dev_log 後停止，不自動開始下一批。
- 一批一個 PR；commit 與 PR 用日語。

停止：

- 找不到同時滿足「權威拒絕＝0」與「改善本機擋下」的安全邊際：停下，帶量測結果請使用者決定。
- 需要改權威冷卻、Match 或 wire：停下回報並重新規劃。
- L2 失敗：保留跑次，有限定位後停下回報。
