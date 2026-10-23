# PvP v7.1 d44-regression-pv7：pv7 上線後的 D44 回歸，290 個 case-run 加長測 0 次

日期：2026-10-21。**完成**（計畫 `5d938dc`、工具與宣告 `d7dc7f3`／`2e5ca49`、DS-1 補遺 `379527f`、結案 `4c56632`；session 第 3 次 197／197 項，0 個 D44 事件，讀法「未再現」）。
補寫：2026-10-23 由圖書管理員依既有紀錄補寫。
後續：隨 PR [#82](https://github.com/yojinn-io/GYO-Engine/pull/82)（after-pv7 線）合併（`5ee499a`）。
計畫與紀錄見 [d44-regression-pv7](../object_fps_pvp/plans/v7.1/d44-regression-pv7.md)；交接見 [v7.1 交接](../object_fps_pvp/plans/v7.1/HANDOFF.md)的決策 D146～D151。

## 做了什麼

- 問題：D44（偶發的 movement epoch 重設）在 v7 第 16 批只是「未再現」。之後 pv7 改了輸入、結果、動作的原樣轉送，加了丟棄回報，A1 改了射擊判定，Gateway 限速換成 token bucket；目前的建置還沒有被任何 D44 型的 L2 量過。
- 計畫（D146）：單樹加第 16 批普查基準。母體照第 16 批（遊戲 95、背壓 120、網路一般 30、impairments 15、60 Hz 長測），新增 v7 沒量過的持續延遲 x1、ramp 各 15 次；Gateway→Match 短停頓只記錄。事件定義（E1～E3）、停止條件、三分法讀法都沿用第 16 批。
- 工具：`run_d44.py`、`judge_d44.py`、乾跑；宣告第 2 版經 1 位 xhigh 做 D45 檢查（門檻來源的對抗式檢查）後由使用者核准（D147）。

## 驗證與經過

- session 第 1 次（D147）停在第 117／197 項 `ramp-r7`：`run_client_delay.py` 自己的注入純度檢查不成立（relay 送出延遲 P99 22.3 ms，上限 10 ms；其他 47 次是 2.6～3.6 ms）。切分為環境，不是實作。照宣告不重跑，前 116 項 0 事件。
- DS-1 補遺（D149、D150）：只有「量到的 relay 送出延遲超過上限」的注入無效改成不計數（I 項），累計 3 次才停；Gateway 限速拒絕等其他注入無效仍是停止。這是放寬停止條件，照 AGENTS.md §14.4 寫了 1～5，由另 1 位審查者用資料核對，抓到 P1（p99 沒量到也會被判成 I），改程式後才核准。
- session 第 2 次（D150）停在第 92 項之後：主 checkout 被切到 `master`（使用者在 Zed 裡 review 時切了分支），凍結工具變成舊版，driver 的檢查照設計攔下。不是本會話造成；前 92 項 0 事件。
- session 第 3 次（D151，12:11～15:03 JST）：不改工具、整個重跑，期間不切主 checkout。197／197 項完成，沒有停止、沒有 I 項、使用者旗標 0。
- 結果：遊戲 95、背壓 120、網路一般 30、impairments 15、x1 15、ramp 15 個 case-run 加 60 Hz 長測，全部 0 個 D44 事件（單側 95% 上限：遊戲 3.1%、背壓 2.5%、網路一般 9.5%、其他各 18.1%）。只用第 3 次讀結論；第 1、2 次保留為紀錄，不合併計數。
- 只記錄：受干擾 9 項都是 0 事件；使用者中途搬動電腦、電源線鬆脫，兩段電池供電和 6 個項目重疊，都是 P、0 事件，事後標記在 `power-intervals.json`（不在判定裡）。x1／ramp 的 backlog 重設發生在注入放出之後，是 A5 的機制（原文一度寫成「注入窗內」，re-evaluate 重算時更正）。
- 讀法：「未再現」，D44 仍不結案。使用者的讀法：D44 是「Match 看不到的參與者」（Gateway 依自己的副本丟命令、不告訴 Match）的徵兆；消除看不到的參與者之後，解釋不了的重設就不再出現。第 08 批那幾次沒辦法逐案證明成因，因為當時沒有丟棄計數。
- 交出去的事：A5 與 D44 的成因交給 `re-evaluate`；Windows／Linux 的 E1-5 交給 `lan15-windows`。

## 證據

`build/target/_build/test/logs/pvp-v71-d44-l2-prep-20261021/`、`pvp-v71-d44-l2-20261021/`、`pvp-v71-d44-l2-20261021b/`、`pvp-v71-d44-l2-20261021c/`（含 `judgement.json`、`judgement.md`、`power-intervals.json`），以及各自的 `.stdout.txt`（git 忽略）。
