# PvP v7.1 pv7-checkpoint：A1 之前的整體回歸，v7 基準對現在 232 項 0 停止

日期：2026-10-18～19。**完成**（probe 與連點 runner 的影子紀錄 `7a5e0ea`、宣告第 3 版 `f2c8cfa`、session 結果 `e1b2c40`、結案 `cc222ba`；L2 232／232、0 停止，判定依 D135 以重新標記後的診斷為準）。
補寫：2026-10-23 由圖書管理員依既有紀錄補寫。
後續：隨 PR [#78](https://github.com/yojinn-io/GYO-Engine/pull/78)（pv7-line）合併（`0fabb52`，2026-10-20）。
宣告與紀錄見 [pv7-checkpoint 的 L2 事前宣告](../object_fps_pvp/plans/v7.1/pv7-checkpoint-l2.md)；交接見 [v7.1 交接](../object_fps_pvp/plans/v7.1/HANDOFF.md)的決策 D127～D135。

## 做了什麼

- 目的有兩個（D129）：A1（`a1-shot-step`）這個行為改變之前，把 v7 穩定基準到 pv7-line 目前為止的產品整段比一次；並產出影子資料，給 A1 決定冷卻保留上限 K 與 Client 的 S 取法。D44 事件回歸不在範圍（屬於之後的 `d44-regression-pv7`）。
- 比較方法（D130）：同一台機器、同一個 session 交錯跑兩棵樹。B＝v7 基準 `6d3f287`（pv6），用 16-L2 的凍結工具判；A＝現在（最後一個改程式的 commit `7a5e0ea`，在 `12ebe2b` 乾淨重建），用凍結的 pv7 工具判。兩棵樹的 probe 相位可能不同，所以只比和 probe 相位無關的量（移動「產生→Actual」、Actual 比例、拒絕原因分布）；合法射擊 P95 只記錄（D132）。
- 影子資料的程式（D131，`7a5e0ea`）：probe 照 Client 的規則填 E、S 與 view tick，連點 runner 加 `--movement-trace` 並重新凍結。改之前同一個 clean-60 的 43 筆全部走 epoch 路徑（E＝S＝0），改之後 on_time 41、life 2。
- 宣告把影子紀錄的對接寫成判定（D127）：對接鍵在 (player, E, life, S) 之外加上由 trace 推出的 session 邊界；每筆 on_time 的紀錄都要對到 resolved 事件，對不到時先找產品行為的解釋（重設、取消、trace 先結束），找不到才算工具無效（D132）。
- 宣告第 1 版做了門檻來源的對抗式檢查（1 位 xhigh）：MUST-FIX 4、SHOULD-FIX 9、NOTE 9，第 2 版逐項修正；第 3 版補上工具、開發跑次與乾跑，使用者核准後立即跑（D133）。

## 驗證與經過

- 乾跑：逐項 89 列、突變 11 個全殺、driver 8 列、模擬 session 7 列、第 5 節合成數字 11 列，全部如預期。開發跑次找到並修正 1 個工具錯誤（`reset` 事件記的是新的 epoch，H3 原本用舊的 E 去找）。B 的語料檢查：183 個 v7 跑次重判，182 個和當時相同，唯一不同符合 D83 的 HUD 規則。
- L2 session（2026-10-19 00:02～02:24，141.9 分，估 148.8 分）：232／232 完成、0 停止、完整性問題 0。受干擾 2 項，其中 1 項是 agent 在 session 中呼叫工具讓 Claude App 佔用 CPU 造成，之後不再呼叫。
- **凍結判定 `passed`＝false**：C1、C2 是 None。原因是判定工具 `judge_cp.py` 的標籤錯誤（`schedule()` 讓兩棵樹共用同一個 `extra`），乾跑沒有抓到，因為合成測試沒有經過 `schedule()`。凍結檔不改，另寫診斷包住 `schedule()` 重新標記：C1、C2、C3 全部成立，「A 變慢」的方向最大 0.282 ms（下限 1 ms）；gateway-1000ms 的 Actual 比例 B 0.966、A 0.987（K1 修正的效果）。
- 使用者決定（D135）：以重新標記後的診斷為判定、結案；標籤錯誤記為工具缺陷，原本的 `judgement.json` 照樣保留。C6 在 `socket-path-1000ms` 的旗標只記錄、不追查。
- 預測 P1～P4 都沒有被推翻：B 的 gateway-1000ms 三次都在恢復後 starvation、A 三次都沒有；A 的 clean 退回率為 0。
- 影子資料（交給 A1）：late 的晚到量 d 從 0 到 28 Tick 共 98 筆；有解釋的未對上 3 筆，都在 `upstream-1000ms`（backlog 重設）。使用者據此定 K＝min(2, cooldownTicks)、S＝N（D136，見 A1 的 dev log）。盲區：「退回後下一發壓在冷卻邊緣」只出現在 clean 的連點，0 次只反映測試設計。
- 只記錄：合法射擊 P95 依 v7 D46⑦ 不能跨建置比較（例：clean-60 的 Client 端 B 17.5～19.4 ms、A 19.3～33.8 ms）；`duplicate-reorder-conflict` 的 A 三次都有 conflict 拒收，是情境本身的設計。
- 這套工具之後改成兩棵 pv7 樹、修掉標籤錯誤，沿用為 A1 的 L2 工具。

## 證據

`build/target/_build/test/logs/pvp-v71-pv7-checkpoint-20261018/`（重新凍結的清單）、`pvp-v71-pv7-checkpoint-l2-prep-20261018/`（工具、開發跑次、乾跑）、`pvp-v71-pv7-checkpoint-l2-20261019/`（`judgement.json`、`judgement-relabelled.json`）（git 忽略）。
