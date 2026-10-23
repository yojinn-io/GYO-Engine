# PvP v7.1 observability-tools：相位決策寫進 trace、增量計數、加寬偵測器、遺失種子

日期：2026-10-21。**完成**（OT-2～OT-4 `cbdc2b9`、併回 after-pv7 線 `f34645c`、OT-1／OT-6 `3346379`、結案 `3ca4e6a`；結案後發現的 OT-4 缺陷由 `d5a4cec` 修正；四平台 L1 #503、#511、#517、#532 通過）。
補寫：2026-10-23 由圖書管理員依既有紀錄補寫。
後續：隨 PR [#82](https://github.com/yojinn-io/GYO-Engine/pull/82)（after-pv7 線）合併（`5ee499a`）。
計畫與紀錄見 [observability-tools](../object_fps_pvp/plans/v7.1/observability-tools.md)；交接見 [v7.1 交接](../object_fps_pvp/plans/v7.1/HANDOFF.md)的決策 D148、D152、D153。

## 做了什麼

- 程式先放在另一個 worktree 的分支：本批改到 `d44-regression-pv7` session 凍結的工具，主 checkout 必須維持只有文件的變更，等 D44 回歸結案後才併回。
- OT-2：`late_count.py`，用兩個來源各算一次 late 修正的增量計數，必須相等；紀錄被丟（`dropped_observations>0`、`lost>0`）或兩來源不等時回報工具失效，不給數字。
- OT-3：`network_resets.py`，掃整場 Match trace 的非 respawn 重設，「每個探針階段 1 次 starvation」之外的都列為候選，等於加寬原本的網路 D44 偵測器。
- OT-4：網路案例的遺失種子改成參數（預設值重現舊常數），種子寫進 evidence。另一半「Gateway→Match 延遲注入改成正式模式」是設計變更，使用者決定維持現狀（OT-0d）。
- OT-1（E1-3，D152）：使用者選 (a)，相位決策直接寫進 `*-commands.jsonl` 的新 kind `phase_decision`，schema 升到 3，凍結讀者、golden、fixture、日誌契約一起改，不留相容層（「目前沒有發佈，不需要背這個技術債」）。這是 Data Contract 的變更。
- OT-6（E1-6）：跑次結束後再量一次計時器基準，只轉出、不進判定。
- OT-0e（D153）：三個 RTT 階段維持同一個種子（只差 RTT 的受控對照）；「跑次之間換基底種子」交給下一個 L2 的宣告。

## 驗證與經過

- OT-5：在 d44 第 1 次 session 的證據上新舊各算一次：late 修正 121＝121、網路候選 0，和舊工具一致。新偵測器沒有真實的陽性樣本，檢出力只由合成樣本支撐。
- OT-1 第一版每個 window 決定都寫，clean-60 實跑 16 秒寫了 1177 筆，其中 1174 筆在 deadband 內，改成只寫首次、late 與啟動修正的 window 決定。突變第一次有 1 個存活：測試只跑 20 幀，到不了 window 決定，是空檢查；改成跑滿 window 後 killed。
- 實跑 loss-shot：trace 的修正次數等於 start_phase 紀錄的次數，兩個獨立來源一致；權威 digest 不動。CTest 77／77。
- 過程中的錯：OT-4 第一次讓 `ActionRelay` 用四個參數呼叫基底類別，延遲 runner 的測試全部失敗，改回三個參數；第一次沒處理 action-frames 結尾的 `evidence_end` 行。
- L1 #517 第一次只有 macos-x64 不通過（`N-movement-recovery-flake` 的第 2 個實例），使用者同意後重跑失敗的 job，通過，結案。
- **結案後發現的缺陷**：OT-4 讓 `run_case` 用 4 個參數建立 relay，但 `run_client_delay.py` 替換進去的 `DelayRelay` 只收 3 個，每一次真實的延遲注入都是 runner error。單元測試替換了整個 `run_case`、L1 也不跑真實注入，OT-4 的實跑又沒有跑 `run_client_delay.py`，所以沒有抓到。`re-evaluate` 補量第 1 次 18／18 失敗才發現。
- 修正照 §14.3：重現、加回歸測試（Red 確認）、修 `DelayRelay`（Green）；突變 `ot4-delay-relay-three-arguments` killed。之後補量 18／18 完成；修正的 L1 #532 四平台通過。

## 證據

本批沒有專用的證據目錄：新舊分析用 `build/target/_build/test/logs/pvp-v71-d44-l2-20261021/`；OT-4 缺陷的失敗跑次在 `pvp-v71-reeval-map-20261021/runs-attempt1-runner-error/`（git 忽略）。
