# 2026-11-21 os.md、time.md 的舊路徑與 OS 定義守衛的死條目（v8-arch-gyo2-os-docs-paths）

Owner：架構（GYO2 主會話）。分支 `claude/gyo2-os-docs-paths`，從 `gyo2` 963d06e0（#262 合併後）。

## 為什麼

os-wait 卡的後續清單，使用者決定兩件轉主會話（HANDOFF `v8-arch-gyo2-os-wait-followups`）：
1. os.md、time.md 多處還寫 #257 分層前的路徑（`engine/os`、`engine/time` 等）與 gyo2 沒帶回的元件。原話：「轉主會話」。
2. OS 定義守衛的豁免清單 `GYO_OS_DEFINITION_EXEMPT` 寫的是 `engine/os`，實際在 `engine/core/os`。原話：「轉主路徑」（＝轉主會話）。

## 做了什麼

- os.md、time.md：目錄路徑 `engine/{os,io,time,threads,base,math}` 改成 `engine/core/...`（include 路徑 `engine/<unit>/<header>` 不變，這兩份沒有出現）。
- os.md 的依賴圖改成現況（Module → core；core/io → core/os → core/base；core/threads → core/time → core/os 的等待原語）。master 的 `engine/asset`、舊 `engine/trace`、`engine/platform/sdl` 從圖上拿掉。
- os.md 裡指 `engine/platform/sdl` 的三處（`BaseDirectory`、Out of scope、Invariant 6 的範圍）改成「SDL adapter（gyo2 還沒有，隨 SDL 批回來）」；Invariant 2 裡「`engine/trace` 直接經 os 寫檔（D58）」改成註明那是 master 的例外、gyo2 的 trace Module 經 logging 寫、沒有這個例外。
- OS 定義守衛：豁免清單拿掉 `engine/os`。那個條目在 #257 之後已經不對應任何目錄，`engine/core/os` 本來就在檢查範圍內；`gyo_os` 不靠 build 定義辨識 OS（原始碼用編譯器的巨集），所以不補豁免。訊息裡的路徑改成 `engine/core/os`。

## 守衛判定（AGENTS §14.4）

- 判定不變：拿掉的條目不對應任何目錄，改前改後守衛對每個目標的判斷一樣（沒有緩的面、沒有緊的面）。訊息文字改了路徑，既有突變的 expect 不含那段文字。
- 相關突變：os-subsystem 的 `os-g13`、`os-g14`、`os-g19`、`os-g20`、`os-g21` 5/5 killed。

## Design Invariants 的文字

os.md Invariant 2、6 的文字有改（路徑與已不存在的 `engine/trace`、`engine/platform/sdl`）。意思不變的是路徑；拿掉 master 的 trace 例外、把 SDL 的範圍改寫成「SDL adapter」屬於條文改寫，合併前交使用者確認。

## 驗證

- 本機 macOS arm64：重新 configure（OS 定義守衛在 configure 時跑）、全量建置、ctest 157/157。

## os-subsystem 清單裡 17 個失效突變的退役（同一 PR）

- 使用者（os 卡）：「交給主會話，刪除。」比過的選項：留著不動／刪除，使用者選刪除。理由：檔案在 gyo2 重建時已刪，這些突變跑不起來，也守不到任何東西。
- 刪掉的 17 個，`file` 都指向 gyo2 不存在的路徑（逐一確認過不存在）：
  - `engine/asset`：`os-g2-macro`、`os-g10-filesystem-op`、`p2-asset-detail-drops-cause`、`p3-relative-path-not-absolute`、`io-w4-watcher-missing-is-error`、`asset-r1-root-cleaned`、`asset-r2-escape-unchecked`
  - `engine/trace`（master 的舊 trace）：`os-g1-header`、`tt-x1-trace-links-engine`、`tt-x2-trace-without-os`、`tt-x3-windows-trace-as-written`、`tt-x4-trace-append-truncates`、`tt-x5-linux-trace-write-failure-ignored`、`tt-x6-trace-batch-end-unwritten`、`tt-x7-trace-os-public`、`tt-x8-trace-truncate-appends`、`tt-x9-trace-detail-without-path`
- README 第 7 點的檢查：17 個都沒有 `guards`；`git grep` 找它們的 id，只出現在 os-subsystem 計畫的 HANDOFF 與 linux-run（當時的紀錄，不改寫）。清單裡沒有其他 `file` 指向不存在路徑的突變。
- 清單 172 → 155 個，只刪除、其他條目的格式不動。
