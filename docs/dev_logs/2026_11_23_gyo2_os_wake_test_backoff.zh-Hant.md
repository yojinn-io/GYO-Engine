# 2026-11-23 「loses no signal」測試的退避：調查與撤回（`v8-arch-gyo2-os-wake-test-backoff`）

件數の突き合わせ：一致。各次量測送出與留下的行數都相同：

- Mac：48／48、6／6、12／12。
- Linux：208／208、104／104、160／160。
- 上限中斷只有 Mac 參考裡改前的 4 次，照宣告記為 timeout。

Owner：OS 層的測試（`tests/common/os`）。GYO2 主會話開的任務卡（PR #289 的 (e)）。分支 `claude/friendly-borg-8cbc08`（開向 `gyo2`，PR #293）。作者 Opus 5.5。
宣告：[`os-wake-test-backoff-declaration.zh-Hant.md`](../architecture/target/v2/os-wake-test-backoff-declaration.zh-Hant.md)（修訂 4，撤回）。

## 結論

**退避的修改撤回。** 這一批最後只拿掉 #289 在「loses no signal」案例裡加的進度 `Note`。測試檔和 #289 之前（`2212637b`）完全相同，退避（4096）與判定都沒有改。

撤回的理由（使用者的決定）：要解決的「機器忙時拖到幾十秒」，在 CI 上基本沒有（L1 三個平台一直通過，唯一慢過的一次 35.8 s 仍在 60 s 內），在 Linux 上沒有（驗收中現狀版本 `yes` 24 最長 1.21 s、`yes` 32 最長 0.95 s，套量測 patch 的版本），只出現在這台 Intel Mac 本機滿載時。

## 做了什麼

- `tests/common/os/WakeSignalTests.cpp`：拿掉 `Note()` 的呼叫、`note` lambda、`blockedRounds`、`slowRounds`、`longestRound`、`caseStart`、`NoteEvery`、`#include "CaseTiming.hpp"`。
- `tests/common/os/CaseTiming.hpp`：說明改成 `Note` 已拿掉、暫時沒有呼叫者，和 listener 一起由主會話拿掉。
- 驗證（Mac）：
  - `gyo_os_tests` 80 案、`gyo_os_wake_fallback_tests` 9 案通過。
  - ow-x1、ow-x19 用 `run_mutations.py` 重跑 3 輪，6/6 抓到（殺它們的測試改了，照 `mutation-rerun-scope` 只跑相關的）。
- 突變：沒有加、沒有拿掉。find 落在改動檔案的突變：掃過全部 26 份突變目錄，0 個；正例 `engine/core/os/src/sys/linux/WakeSignal.cpp` 命中 10 個。

## 經過

1. 探索：
   - 在 Linux 實機（Ryzen 7 6800H，16 執行緒，Ubuntu 24.04）比了五種退避；這台 Mac 沒加負載，只比了突變。
   - 宣告修訂 0 建議自旋 65536 次（B）。
2. 敵對式檢查（Fable 5.2／high）：must 5。
   - 修訂 0 用的檢出力指標不對，宣告的量測手段也讀不到數字。
   - 修訂 1 改用「突變下第一次遺失的比率」。
3. 使用者「開始」。
4. 驗收：
   - Mac 閒置 ow-x19 的比 0.40，不成立 → 停。
   - 使用者「用linux的標準來設計。」（修訂 2）。
5. 拿掉 Note 後重跑：
   - Linux 閒置兩格的比率只有改後的 0.37、0.15 倍 → 停。
   - 使用者「選 1」，檢出力改看「2 萬回合內抓到的比例」（修訂 3）。
6. 使用者「現在fable用量不夠了，暫時用Opus」（主會話轉來），之後的檢查改 Opus 5.5／high，和作者同一個模型。
7. 驗證者（Opus 5.5／high，唯讀）：
   - 第 4 節「緩める面」不成立：閒置時每回合的比率下降、長尾變重、Mac 每輪下降、只量了套 patch 的版本。
   - 三處數字錯誤（已改正）。
8. 補量真實二進位：最終版閒置 ow-x19 19/20（base 20/20）→ 停（§14，可能的回歸）。
9. 使用者問「你在解決什麼問題？」「這個問題在CI上有嗎？」「那這個問題在linux上有嗎？」，然後決定「那就撤回，只拿掉 Note，記錄下來就好」（修訂 4）。

本卡的偏差：

- 原本要解決的是「拖到幾十秒」，後來偏到「證明每回合掃到縫的機率完全沒變」。這個量本身極不穩定，判定因此改了兩次、停了三次。
- 沒有先查「這個問題在 CI、Linux 上有沒有」，就把它當成要用 §14.4 完整驗收對付的問題。
- 第 13 節停下時，本卡把時間盒子列為停止的理由，把等待時間也算了進去。使用者指出後已撤回，真正的理由只有 §14 的兩條。

## 留下的紀錄

| 項目 | 內容 | 證據 |
|---|---|---|
| 65536 次自旋對本機逾時的效果（參考，沒有事前宣告） | Mac 環境負載（`load1` 33～49）下，套量測 patch 的改前、改後交錯各 6 次：改前 4 次碰到 40 s 上限，改後 0.07～1.36 s | `accept/m-t-ambient.txt`；宣告第 14.5 節 |
| 65536 次自旋的代價 | 閒置時每回合的檢出力可能變弱：Linux 比率 0.49、0.23 倍；真實二進位閒置 ow-x19 19/20 | 宣告第 12、13 節 |
| Linux 負載下現狀的檢出力 | 兩條執行緒在同一顆 CPU 輪流跑；`yes` 24 下 ow-x19 只抓到 8/20（真實二進位） | `accept/linux/p-x.txt` |
| 掃描 | 遺失只落在 `round % 64` 的頭 0～3 格 | 宣告第 2.4 節 |
| 檢出力對小改動敏感 | 量測 patch 不同，第一次遺失的比率差好幾倍 | 宣告第 2.1、11、12 節 |
| 條文守不到的一處 | Invariant 8 寫明後備實作的 pending「cleared under its mutex」，但把 `pending = false` 移到鎖外，任何案例都抓不到（照文件的呼叫方式行為相同，只剩 data race）。交 review，不加突變 | 宣告第 13.1 節 |
| 單核 | Linux 綁 1 顆 CPU：任何退避都測不到這個競爭，都能前進 | 宣告第 2.5 節 |

## 檢驗

- Mac：見上。
- L1：PR #293 推上去後再看，結果記在 PR。

## 範圍の限界

- 60 s 的逾時本身沒有重現。
- Windows、macos-arm64 沒有在本機量。

## 證據

主 checkout 的 `build/evidence/gyo2-os-wake-test-backoff/`（git 不追蹤，這台 macOS Intel 機器）：

- 探索：`run_variants.py`、`mutant_variants.py`、`summarize_mutants.py`、`experiment-backoff-variants*.diff`、`explore-*.txt`、`mac-mutants-v2.txt`、`spin_cost.cpp`、`spin-cost-mac.txt`、`linux/`。
- 審查：`declaration-review-fable.md`。驗證者的報告沒有另存檔案，要點在宣告第 13.1 節。
- 驗收：`measure.diff`、`measure-final.diff`、`accept/`（`build_bins.py`、`accept.py`、`judge_mutants.py`、`src/`、Mac 的輸出、`linux/`）。
- 撤回後的突變：`final-revert-mutants.log`。

Linux 實機上另有副本：`~/Workspace/Remote/gyo2-os-wake-test-backoff/`（只是副本，不是判定依據）。

花的時間：牆鐘約 11:00～14:15。其中等待（使用者的回覆、Linux 的背景量測合計 1 小時以上、審查與驗證 agent）占大半。
