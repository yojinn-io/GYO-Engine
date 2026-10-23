# PvP v7.1 proto-rename-v7、version-errors：協議改名為 v7，版本 6→7，版本不符時說清楚

日期：2026-10-16。**完成**（proto-rename-v7 `d5104a0`、version-errors `046a0fd`、契約條文 D99 `16216ff`；兩批的四平台 L1 都通過）。
補寫：2026-10-23 由圖書管理員依既有紀錄補寫。
後續：隨 pv7-line 的 PR [#78](https://github.com/yojinn-io/GYO-Engine/pull/78) 合併（`0fabb52`）。
契約見 [protocol-v7](../object_fps_pvp/protocol-v7.zh-Hant.md) §1；計畫見 [v7.1 計畫](../object_fps_pvp/plans/v7.1/README.md)的「pv7-line 各批」；交接見 [v7.1 交接](../object_fps_pvp/plans/v7.1/HANDOFF.md)的規劃紀錄與決策 D97～D99、D101。

## 做了什麼

- 改名（proto-rename-v7）：`client_v6`／`runtime_v6` 改為 `client_v7`／`runtime_v7`，包括 proto 檔名與 package、Go 套件、CMake target、C++ 的 include 與 namespace，照 v6 的先例；版本值仍是 6，所以行為完全不變。
- 版本升級（version-errors，盤點的 P1「版本與資訊分離」）：Client、Gateway、Match、驗收工具四處一起改成 7，由測試核對四者一致。
- 版本不符時說清楚：
  - HTTP：Gateway 的拒絕 body 帶自己的版本並寫一行 log；Client 看 body 的 `error`（不看狀態碼，避免把 `room_full` 誤判），產生「Client 7、Gateway 幾版」的錯誤。
  - IPC：版本不符、解析失敗、訊息為空、未知欄位分開報告；Gateway 讀到其他版本的 Ready 時，關閉前先送一則只含版本的告知；Match 對「連上之後沒收到任何框架就斷」寫一行提示。
  - Client：其他版本的 GYOP 照舊靜默丟棄，另外計數。
- 凍結 pv7 版分析器（54 個檔，清單另存 sha256）：從這批起，凍結的 pv6 工具讀不了新跑次。`run_quad` 的混版測試改寫成 pv7 版，`older-arena` 退役（pv7 不改 arena）。

## 驗證與經過

- 改名：先確認重產的 v6 綁定與提交版逐位元一致，新舊的差只有名稱。全量 CTest 70／70；權威 digest 與 v7 最終版（`6d3f287`）同機兩樹比較，scale 1、3 各 35 情境 0 差異。實際約 1 h（估 3 h）。四平台 L1 run 38771828429 通過，同時開了 draft PR #78（D98）。
- version-errors：全量 CTest 70／70；突變 14／14 killed（實作 7 個、審查後再加 7 個）；digest 0 差異。混版測試對 v7 的 pv6 執行檔 3／3 通過；改寫前 older-arena 失敗的那次保留。
- pv6 語料核對（16-L2 的 43 項）：舊分析器 43／43 重現原結果；新舊的差只在事前列出的版本比對項。
- 審查：會話的 ultracode 開關打開後，事前沒問就跑了審查 workflow（3 位審查者＋1 次對抗式檢查，約 25 分）；之後使用者決定照計畫的檔位，不讓開關覆蓋（D101）。19 項發現中確認 17 項、推翻 2 項，例如 HTTP join 改成先讀版本、Gateway 先單獨讀版本欄位，都已修正。
- 動到定稿契約文字的 2 項先保留原文、交使用者決定；D99 照建議加上告知範圍的附註與 log 的 `remote=<addr>`，wire 不變。
- 中途的失誤：一個突變第一次判成 wrong_failure（doctest 只印出運算式），改期望後 killed；IPC 測試改成正常關閉，避免連線重設造成偶發失敗。
- 短跑次（只記錄）全部通過；有 11 個數值落在 16-L2 的範圍外，v7 與 pv7 交錯各跑 4 輪後兩邊都偶有替代命令，判斷為主機擾動、不是回歸。
- 注意：Gateway 執行檔不在 ALL 裡，跑驗收前要單獨建；這次建之前它還是 10-11 的舊版。
- 實際約 1.3 h（估 12 h）；四平台 L1 run 38784720725 通過。Architecture Delta：Build Graph 的 target 改名（依賴不變），其餘只在產品內。

## 證據

`build/target/_build/test/logs/pvp-v71-proto-rename-20261016/`、`pvp-v71-version-errors-20261016/`（含 `frozen-pv7.sha256`、`pv6-corpus/`、`short/`、`after-review/`）（git 忽略）。
