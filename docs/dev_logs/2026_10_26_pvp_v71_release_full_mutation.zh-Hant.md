# PvP v7.1 發行線：全量突變目錄（D185，第一次）

日期：2026-10-25～26。發行線 `claude/pvp-v71-release`（PR #96）。

**狀態：完成；只有 1 條屬於「測試失去檢出力」，已修好（擋發版的條件解除）**。

## 怎麼跑

- 在獨立的 worktree（會話的 scratchpad，固定在發行線 `d8873fd`）以 `caffeinate -i` 背景執行，不擋其他工作。2026-10-25T10:01Z 開始，15:17Z 結束（約 5 小時 16 分）。
- 目錄：Engine 的 time-threads-trace、input-and-present、os-subsystem（`docs/architecture/plans/time-threads-trace/scripts/run_mutations.py --catalog …`），產品的 `tests/object_fps_pvp/mutations.json`（`tests/object_fps_pvp/run_mutations.py`）。
- 跑的期間，同一台 Mac 上也在建置與跑 quick-close 的驗收（load average 到 32）。

## 結果

| 目錄 | 數量 | killed | 其他 |
|---|---|---|---|
| time-threads-trace | 14 | 14 | |
| input-and-present | 83 | 83 | |
| os-subsystem | 41 | 36 | 5 個只在 Windows 跑（跳過，D201 在 Windows 機器上跑） |
| 產品 | 654 | 643 | 10 wrong_failure、1 survived |

基準：`ctest` 93 個中 `render.sdl_gpu_mesh_smoke`（gpu 標籤）失敗；產品突變的檢查都不用它。

## 非 killed 的分類（D185）

| 突變 | 第一次 | 分類 | 處理 |
|---|---|---|---|
| `v7-flow-ledger-remove-silent`、`v7-flow-ledger-reset-close-uncounted`、`v7-flow-trace-rejection-cap-off-by-one`、`v7-flow-trace-suppressed-uncounted`、`v7-flow-trace-rejection-dropped-uncounted`、`v7-flow-trace-name-keeps-commands-suffix`、`v7-flow-trace-name-suffix-accepted` | wrong_failure（`object_fps_pvp.cpu` 60 s 逾時） | 環境（負載） | 負載降下後單獨重跑：7／7 killed。不改目錄 |
| `v7-08-submit-no-wake` | wrong_failure（「overdue catch-up packets」） | 環境（負載） | 單獨重跑 killed。不改目錄 |
| `v71ip-match-takes-another-life` | wrong_failure（IPC 測試在同一個案例中止） | 期望訊息過時 | silent-branch-asserts 的 S1（`MatchRuntimeHost.cpp` 的 `GYO_ASSERT(merged.commands.empty() && merged.stagedSequences.empty())`）先擋下。expect 加上這條 assert（舊訊息保留），`ed63c24` |
| `ci-liveness-reads-time-echo` | wrong_failure（「an outage did not write its six connection lines」） | 期望訊息過時 | rebind-on-new-port 加的「中斷的 6 行」檢查先抓到；兩次結果相同。expect 加上這條（舊訊息保留），`ed63c24` |
| `csc-r5-uncounted` | survived | **測試失去檢出力（擋發版）** | 見下節 |

## csc-r5-uncounted：測試失去檢出力

- **現象**：拿掉 r5（窗口的 epoch 或 life 已被取代）的計數，worker 驗收仍通過。單獨重跑 10 次：10／10 survived（第一次重跑的 wrong_failure 是 r7 assert 偶發，見下）。
- **原因**：`RequireDroppedCounts` 要求第一行 worker 統計的 `stale_input_windows≥2`，`CheckDroppedDatagrams` 送 r1、r5 各 1 個。client-path-tests 的 C17-3（`CheckHelloCadence`）之後在同一個統計區間多送了 1 個 r1 窗口：沒有突變時記到 3，拿掉 r5 的計數後還是 2，所以檢查通過。D185 目的 (1) 說的情況：一條線改了共用測試，削弱了別的突變。
- **修正**：`CheckDroppedDatagrams` 送 3 個 r5 窗口（兩個其他 epoch、一個其他 life），門檻改成 ≥4。拿掉 r5 的計數最多剩 2（本檢查的 r1 與 C17-3 的 r1），必定失敗。沒有突變時記到 5，連跑 5 次通過。
- **判定的變更（AGENTS.md §14.4，只收緊）**：
  1. 守的性質：被丟掉的已取代窗口（r1 與 r5）都計數。
  2. 放寬的面：沒有。舊判定抓到的突變（`csc-r1-uncounted`，以及期望這條訊息的其他突變）在新判定下仍被殺。
  3. 收緊的面：門檻 2→4，輸入多 2 個 r5 窗口；`csc-r5-uncounted` 從存活變成被殺。
  4. （無放寬面）
  5. 證據：worker 為檢查的 47 個突變（被改動的測試檔殺死的全部，D184）47／47 killed。
- 經過中觀察到：`csc-r5-uncounted` 的第一次單獨重跑，worker 被 r7 的 `GYO_ASSERT(found!=input.commands.end() && *found==existing)` 中止。這個突變不改控制流程；沒有突變的 worker 連跑 20 次沒有出現 r7，1 次失敗是另一個已知的偶發（「resend catch-up burst」）。r7 只出現一次，原因不明，記錄下來，沒有修正（AGENTS.md §14.3：無法重現，不報告為已修正）。

## 證據

- 本機輸出（git 忽略，會話的 scratchpad）：`mut-full-results/`（全量的 log、`product-report.json`）、`rerun11.json`（11 條重跑）、`r5-1.json`～`r5-10.json`、`worker47.json`。
- 目錄的變更：`ed63c24`（2 條 expect）、本變更（worker 的檢查）。
