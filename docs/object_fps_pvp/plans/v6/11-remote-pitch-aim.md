# 第 11 批：遠端上半身俯仰瞄準

狀態：未開始。對應 v6 [交接](HANDOFF.md)第1項。
先讀 [進度](README.md)、[交接](HANDOFF.md)、[基線](BASELINE.md) 與 [v6 契約](../../protocol-v6.zh-Hant.md)。

執行規則沿用 v5／本計畫 README：只做指定範圍、先凍結再量測、失敗跑次保留、長測另外授權。

## 目標與範圍

v5 第04批 L3：A 玩家抬頭看天時，B 玩家畫面中的 A 沒有任何變化。
這是 v5 契約「上半身：明確產品骨骼遮罩組合持槍／瞄準」的缺口。
`pitch` 已在 Snapshot 與時間線取樣中（`SnapshotTimeline.hpp:120` 內插），**不需改 wire**。

做：

- `PlayerPresentationFrame` 加 `pitch`。目前只有 `yaw`（`PlayerPresentation.hpp:18-21`）；
  填值在 `PvpApplication.cpp:599-605`，改填時間線內插後的 `pitch`。
- `players/animations.animset.json` 加 `Armature|Pistol_Aim_Up`／`Pistol_Aim_Neutral`／`Pistol_Aim_Down`
  （`UAL1_Standard.fbx`，各 0.167 秒）；`PlayerPresentation.cpp:85-102` 的 clip 清單同步。
- 依 `pitch` 在三個姿勢間混合，作為上半身基底，再疊射擊／換彈。
  沿用既有上半身遮罩（建立 `:139-151`、組合 `:448-457`）。
- 映射參數（夾限範圍、對應方式）放 `players/presentation.json`，不寫進 C++。
- 死亡時忽略 `pitch`。
- 呈現仍是同區間、同生命權威資料的純函數：重送、ACK、重複 Snapshot、時間線 hold 都不改變結果。

不做：

- 不改 wire、不改 Match。
- 不改第一人稱（相機與 ViewModel）。
- 不擴充 Engine 動畫框架；遮罩與混合留在產品。
- 不做受擊反應（→[第13批](13-hit-reaction.md)）。

## 開始時的提案（D11⑥）

呈現細節在本批開始時提案，使用者確認後才實作。預設提案：

- `pitch` 先夾限，再在範圍內線性對應 Down／Neutral／Up。
- 符號：正 `pitch` 朝下（`ShotQuery.hpp:17`，與相機一致），對應方向須在測試中鎖住。
- 射擊與換彈時保留俯仰（基底仍是俯仰姿勢，再疊動作）。
- 死亡時忽略 `pitch`，只播 Death01。

使用者未確認前不寫實作。

## 交付

- `PlayerPresentationFrame.pitch`、俯仰基底混合、animset 三個 clip、`presentation.json` 映射參數。
- L1 測試（`tests/object_fps_pvp/PlayerPresentationTests.cpp` 或同 owner 的新檔）。
- L2 證據目錄與報告；L3 回報。
- dev_log：`docs/dev_logs/YYYY_MM_DD_pvp_v6_batch11.zh-Hant.md`。

## 驗收點

L1（自動）：

- 呈現純函數測試：`pitch` 極值與夾限、正負號方向、射擊／換彈疊加時保留俯仰、死亡時忽略、同區間同輸入得同結果。
- 突變檢查：把映射方向反轉或忽略 `pitch` 時，測試必須失敗。
- 權威 digest（[第03批](03-authority-digest.md)）不變：本批不碰權威路徑，只作附帶檢查。

L2（實機）：

- 人物短測 30／60／144 FPS。達成 FPS 未達名義×0.85 判 invalid（D11③）：不計次，報告列達成 FPS 與計時器分布，
  同跑次延遲門檻照判；有效輪不足時該項標「未驗證」，不算通過。
- Metal capture：三個 `pitch`（上、中、下）各一張。

L3（人工）：使用者用雙 GUI 抬頭、低頭，確認對方畫面中的上半身跟著變化；射擊與換彈時仍保留俯仰。

## 量測與權威規則

- 量測基線世代：在本批的 base commit 上，以凍結工具量 before／after 比較。
  [第04批](04-measurement-baseline.md)（B0）只作歷史參照；v5 STABLE_BASELINE 的數字只用來對門檻。
  IP-2 已合併時，before 必須在 IP-2 之後的 base 上重量，不跨世代比較。
- 先凍結來源／產物／分析器再量測；開發與乾淨量測不同時進行。
- 權威不變的正式證明：同機兩樹（base／branch）digest 比對。CI 只做自洽與不依賴 libm 的 golden 子集。不修改共通 workflow。
- 若本批在[第09批](09-protocol-v6.md)之後執行：候選期原則上不改 wire（D11①）。

## 平台

| 平台 | 本批 |
|---|---|
| macOS Intel／Metal 實機 | 預定執行（L2 人物短測、Metal capture、L3） |
| CI 四平台（windows-x64、linux-x64、macos-arm64、macos-x64）L1 | 預定執行 |
| Linux lavapipe GPU | 未執行：pvp 沒有宣告 GPU 檢查（`checks.json` 的 `gpu` 為 false）；CI toolchain 列的共通 `render.*` 照常執行，但不作為本批驗收依據 |
| Windows D3D12 實機 | 未執行：沒有實機（D11⑩） |
| Linux 實機 | 未執行：沒有實機（D11⑩） |
| macOS arm64 實機 | 未執行：沒有實機（D11⑩） |

## 建議檔位

high。產品內的跨檔案實作，需先讀懂既有遮罩組合與時間線語意；有 L1 與 L3 做外部驗證。

## 依賴與合併順序

依賴：

- [第04批](04-measurement-baseline.md)（B0 與 02 的 FPS 判定規則）。
- [第13批](13-hit-reaction.md)依賴本批（優先順序的最底層是俯仰基底）。

合併順序（建議）：

- 合併順序只是建議，用來減少衝突與方便歸因，不是依賴；後合併的一方 rebase。
- `PvpApplication.cpp` 的建議合併順序：05→06→12→**11**→13。比本批先合併的是[第05批](05-first-person-arms-after-death.md)、
  [第06批](06-input-migration.md)、[第12批](12-local-fire-gate.md)；順序不同時由後合併者 rebase，不因此等待。

## Architecture Delta

無。動畫遮罩與混合屬產品呈現與產品資產（沿用 v5 Delta 第5點）；不新增 Dependency Edge，不改 Engine 介面或 wire。

## 完成條件與停止條件

完成：

- D11⑥ 提案經使用者確認並照做；L1 全過（含突變）；L2 有效輪通過或依 D11③ 標示；L3 使用者確認。
- 更新 [進度](README.md)、[交接](HANDOFF.md)、dev_log 後停止，不自動開始下一批。
- 一批一個 PR；commit 與 PR 用日語。

停止：

- 提案未經使用者確認：只停在提案，不實作。
- 需要改 wire、Match、第一人稱或 Engine 動畫框架才能達成：停下回報並重新規劃。
- L2 出現與本批無關的回歸：保留失敗跑次，有限定位後停下回報，不擴大範圍。
