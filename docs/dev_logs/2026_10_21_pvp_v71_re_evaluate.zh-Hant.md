# PvP v7.1 re-evaluate：用 pv7 之後的數據判斷相位追蹤與 D44 的項目還要不要繼續

日期：2026-10-21。**完成**（開始 `f083867`、第 1 版 `bda85e5`、第 2 版 `33cfcd5`、D154 `1786af1`、補量結果 `64f9ba6`、結案 `51adff9`；結案或取消 10 項、交給 v8 3 項、有條件 1 項）。
補寫：2026-10-23 由圖書管理員依既有紀錄補寫。
後續：隨 PR [#82](https://github.com/yojinn-io/GYO-Engine/pull/82)（after-pv7 線）合併（`5ee499a`）。
判斷清單見 [re-evaluate](../object_fps_pvp/plans/v7.1/re-evaluate.md)；交接見 [v7.1 交接](../object_fps_pvp/plans/v7.1/HANDOFF.md)的決策 D154、D155。

## 做了什麼

- 範圍：INVENTORY 處置為 re-evaluate 的 12 項，加上從 `d44-regression-pv7` 衍生的 3 項（門檻 105 重推、快速正向回收、分析器寫死的 105）。
- 做法：分三組只讀評估（相位追蹤與延遲位移、Host 與 wire、A5），讀程式與現有證據，主要證據是 D44 回歸第 3 次 session 的 197 項。不跑新的 L2；分析腳本留在會話的 scratchpad，沒有進 repo。
- 判斷用樹狀總覽列出「不需要繼續／還需要／有條件才需要」，每個不需要繼續的項目都附重開條件。

## 驗證與經過

- **第 1 版方向錯**：寫了修法、工時與順序。使用者指出「re-evaluate 就是評估，這些case還是否需要繼續的，而不是評估要如何做的」，第 2 版改成只留「需不需要」與根據。
- D154：使用者同意第 2 版，但 B6、B7、A5 先補量再定；其他可證明的直接處理（結案 N-respawn-reject-volume、N-conflict-window-permanent-loss、B9 等，取消 B8），A4、N-analyzer-literal-105 交給 v8。
- 補量：沿用 v7 09R-map 的 9 格（δ 45／60／80 ms × 持續 100／150／300 ms），每格 2 次，量什麼在跑之前寫進文件；只記錄，不設門檻。
- 補量第 1 次 18／18 全部是 runner error，原因是 observability-tools 的 OT-4 引入的缺陷（`d5a4cec` 修正，見[觀測工具的開發日誌](2026_10_21_pvp_v71_observability.zh-Hant.md)）；失敗的跑次另存、不計入，修正後 18／18 完成。
- 結果：分布和 v7 的 09ab L2 相同。A5 的 300 ms 型在 pv7 仍然 12／12 被 backlog 重設，約 0.2～0.6 秒恢復，沒有 held。
- **第 1 版對 B6 的判斷低估了**：第 1 版說殘餘只有「約 4 秒、+3 ms」。補量顯示 100 ms 的格確實如此，但 150 ms 的格部分撤回後，沒有被重設的 4 個單位留下 +22～+27 ms、約 4.2 秒的延遲。原因是第 1 版沒有區分 09b 1-7 的第 1 點與第 3 點。
- D155：使用者聽過 B6 的機制說明（相位追蹤自己的時序問題，不是事故的副作用）後，擔心沿用 09b 的方案達不到最好的效果，決定 B6 交給 v8 重新規劃；B7 取消；A5 取消（照設計保留，連同門檻 105 重推、快速正向回收）。
- 限制：每格只有 4 個單位、單一主機、固定起點，推不出真實網路下的頻率。

## 證據

`build/target/_build/test/logs/pvp-v71-d44-l2-20261021c/`（主要的評估證據）、`pvp-v71-reeval-map-20261021/`（`runs/`、`analysis.json`、`analyze.py`；第 1 次的 runner error 在 `runs-attempt1-runner-error/`）（git 忽略）。
