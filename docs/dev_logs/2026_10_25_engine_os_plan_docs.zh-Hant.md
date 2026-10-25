# Engine os-plan-docs：OS 子系統的計畫

日期：2026-10-25。**計畫已核准**（D46：開放決策 1、14、15 照建議，同意 A1～A3 的 Delta）；程式尚未動。
計畫見 [README](../architecture/plans/os-subsystem/README.md)、[PLAN](../architecture/plans/os-subsystem/PLAN.md)；紀錄見 [交接](../architecture/plans/os-subsystem/HANDOFF.md)的「os-plan-docs」。

## 做了什麼

- 回答提出需求的消費端的阻塞問題：Windows 長路徑怎麼修。
  - 先量出各檔案從多深開始失敗：shader 在 P≥145，manifest 約 220，log 229，執行檔本身約 238。
  - 結論：真正的瓶頸只在資產讀取；log、trace 要再深 65～80 字元才會失敗。
- 使用者定下方向（D40～D45）：
  - 目標是整理 OS 子系統，不是修路徑。
  - 分層 asset → io → os → base。
  - asset 改經 engine/io。
  - 路徑改 UTF-8、修 ParseUri。
  - 架構與產品修正分開。
- 規劃整個子系統：
  - 新增 `engine/os`（GYO::Os）。部件有 OsError、Files、System、Process、Console、Thread、Wait、Net（條件式）。
  - engine/io 新增原生後端與路徑契約。
  - 守衛用只縮不增的 allowlist 凍結現有 OS 站點。
  - 共 17 批：關鍵路徑是 os-skeleton 與 io-contracts 並行，接 os-files-read，約 20～29 h；整條線約 100～150 h。
- 消費端的長路徑修正（P1～P5）列為交接點，由消費端的發行任務在 A1～A3 完成後做。

## 驗證與經緯

- 盤點與設計用 ultracode（workflow `wf_014e00ee-e72`，9 個 agent）：
  - 盤點：4 路，共 150 個站點。
  - 設計：3 個角度各一案。
  - 審查：1 位，評分 7／8／6。
  - 對抗檢查：1 位，19 項檢查、20 個遺漏站點。
- 使用者在審查途中陸續定下 D40～D45，工作流程兩度停下，帶著新決定續跑審查與對抗檢查；盤點與設計沿用快取。
- 對抗檢查發現的錯誤都在 PLAN 修正，清單見 HANDOFF 的批次紀錄。其中兩處照原案做一定會失敗：
  - 標頭衛生的 `_PTHREAD_H` 檢查：在 macOS、Linux 上，沒有違規也會失敗。
  - 突變 runner：跑不了 Python 守衛。
- 驗證：PLAN 引用的 file:line 都以 master `23fab75` 核對過。這一批沒有程式變更，所以沒有 L1。

## 證據

- 事故 log：主 checkout 的 `build/target/_build/test/logs/pvp-v71-windows-lan-20261016/logs/clientA/logs/client-20261016-104011.log` 第 2 行（git 忽略）。
- workflow 的完整輸出在本機會話暫存，未進 repo。
