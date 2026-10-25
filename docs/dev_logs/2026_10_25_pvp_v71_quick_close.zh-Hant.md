# PvP v7.1 quick-close：關閉 Client 不再卡在 /leave

日期：2026-10-25。分支 `claude/pvp-v71-release`（PR #96）。

**狀態：macOS 驗收通過；Windows 驗收由使用者與 windows-long-path 的實機驗收一起跑（D201）**。計畫與決定見 [quick-close.md](../object_fps_pvp/plans/v7.1/quick-close.md)（D202、D203）。

## 做了什麼

- `ClientConnection.cpp`：網路 worker 結束時（Client 關閉）送的 `/leave` 改用 `ClosingLeaveTimeout`（連線、讀取、寫入各 300 ms，總上限 600 ms）。其他 HTTP 請求維持 2 秒。逾時的話，位子由 Gateway 的 session 到期（T_max＝5 s）釋放。
- worker probe 新增 `--close responding|down|stalled`；`build/acceptance/object_fps_pvp/close_timing.py` 量 `close requested` 到程序結束。
- CTest `object_fps_pvp.close_timing`（每種情境 1 次）當回歸測試；突變 2 個。

## 驗證與經過

- **Red（探索，修正前各 3 次）**：responding 約 3 ms、down 約 2 ms、stalled 2003～2004 ms。核准時的兩條判定（D202）在修正前也會通過：本機關掉 Gateway 時連線立刻被拒。卡住的是 Gateway 收下請求卻不回、或主機不在而連線逾時的情況。
- **macOS 驗收 1（D202 的判定，各 10 次）**：通過；stalled（當時只記錄）303.5～304.1 ms。
- 使用者看過以上結果後加了 stalled ≤1 s 的判定與 CTest（D203，只收緊）。
- **macOS 驗收 2（D203 的判定，各 10 次）**：通過；responding 最大 1.2 ms、down 最大 1.6 ms、stalled 301.5～302.6 ms。
- 突變：`qc-closing-uses-request-timeout`、`qc-closing-timeout-too-long` 2／2 killed（CTest `object_fps_pvp.close_timing`）。
- 本機 `ctest`：全部執行時 `object_fps_pvp.worker` 失敗、`object_fps_pvp.presentation_evidence` 逾時；當時背景在跑發行的全量突變（load average 約 32）。兩者單獨重跑 3 次都通過，判斷為環境（負載），和之前記錄的 worker probe 高負載偶發失敗相同。
- 量的是產品的網路層（`ClientConnection` 的解構），不是整個 Client 程序：延遲只來自這裡，視窗與 GPU 的拆除沒有改。

## 證據

- 本機輸出（git 忽略，會話的 scratchpad）：Red `qc-red.json`、驗收 1 `qc-macos-accept.json`、驗收 2 `qc-macos-accept-d203.json`。重跑：`python3 build/acceptance/object_fps_pvp/close_timing.py --probe build/target/_build/test/acceptance/object_fps_pvp/gyo_object_fps_pvp_worker_probe --runs 10`。

## 還沒做

- Windows 的 10 次（指令在 quick-close.md 的 D203 一節）。
