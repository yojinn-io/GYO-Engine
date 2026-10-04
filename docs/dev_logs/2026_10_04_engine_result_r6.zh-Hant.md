# Engine Assert／Result 統一 R6：收尾

日期：2026-10-04。Owner：Engine（`GYO::Base`），連帶文件、共通測試與計畫紀錄。
狀態：完成，PR [#36](https://github.com/yojinn-io/GYO-Engine/pull/36) 合併為 `a15c836`（分支 `claude/result-unification-r6`，基準 `f72bfbd`）。
計畫與證據見 [Assert／Result 統一](../architecture/plans/result-unification/README.md) 與 [HANDOFF](../architecture/plans/result-unification/HANDOFF.md#r6-收尾)，本文只記經過。

## 經過

1. R5（#35）L1 四列通過、windows-x64 警告不變後合併，從 master 建立 R6 分支。
2. 全範圍稽核（Apps 以外）：R0 基線中要移除的寫法全部歸零；engine 剩下的 throw 都只在模組內部使用，並在邊界轉成 Result。
3. 未啟用產品：在 scratch 中分別建置 `eeebc1e` 與 `f72bfbd`，逐 TU 做 syntax-only 比較。本計劃新增 82 筆錯誤，分布在 20 個 TU（其中 1 個是新增失敗），分成 3 類原因，寫成 `inactive_products.md`。
4. 整理 Apps（pvp）剩下的項目，作為之後順手修改時的參考。
5. `error-handling.md` 定稿；寫入 Architecture Report。
6. 最終審查（1 個唯讀 agent）：修正 smoke 測試的錯誤輸出、AssetCatalog 丟掉 resolver detail 的問題、規則 2／4／5 的措辭、`inactive_products.md` 的計數，以及紀錄上的缺漏；PLAN 開頭加入「實作與本文件的差異」彙整。
7. 本機驗收：core 23／23、test 50／50；依賴圖與 R0 相比只多出計劃列出的邊；29 檔 syntax-only 全部通過；沒有第一方警告。
8. PR #36 的 L1 四列與 CI gate 通過，windows-x64 的警告與 R5 逐項相同；合併為 `a15c836`，本計劃完成。

## 留給下一步

- 範圍外、留待日後：Apps 剩餘項目、未啟用產品的遷移。
