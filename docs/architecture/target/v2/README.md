# docs/architecture/target/v2：GYO2 線的文件

`gyo2` 分支上重建通用引擎（`v8-arch-rebuild`）的文件都放這裡（使用者 2026-11-14，`v8-arch-gyo2-docs-target-v2`）。
2026-11-14 以前 Kernel 研究線（改造舊架構）的計畫、規格與紀錄在 [`../../plans/kernel/`](../../plans/kernel/README.md)，是參考，不是這條線的現況。

**先讀**：[HANDOFF.md](HANDOFF.md)（現況、使用者的決定、經過、下一步）。

## 給作者看的

- [system-contract-page.zh-Hant.md](system-contract-page.zh-Hant.md)：System 契約的說明頁——怎麼寫一個 System、錯誤怎麼回報、要注意的地方、生成的元件與寫者。寫的是 `claude/gyo2-mechanism` 上能跑的程式。
- [diagrams/mechanism-runtime.svg](diagrams/mechanism-runtime.svg)：圖一，一個 Product 的機制全貌（11 月 11 日的圖，只換名字）。
- [diagrams/control-plane-revised.svg](diagrams/control-plane-revised.svg)：圖二，基礎的 System 契約（11 月 11 日的圖，換名字並調整）。

## 用詞與命名

- [rebuild-vocabulary.zh-Hant.md](rebuild-vocabulary.zh-Hant.md)：同詞異義表（v2 與使用者的回覆；v3 待寫）。
- [rebuild-naming.zh-Hant.md](rebuild-naming.zh-Hant.md)：命名（N17～N24；採 11 月 11 日命名規範的第 1 到 4 條）。

## 設計與判斷

- [rebuild-concept.zh-Hant.md](rebuild-concept.zh-Hant.md)：概念層的重構稿。
- [rebuild-judgment-rules.zh-Hant.md](rebuild-judgment-rules.zh-Hant.md)：舊設計去留的判斷機制。
- [rebuild-entity-order.zh-Hant.md](rebuild-entity-order.zh-Hant.md)：Entity 的順序。
- [rebuild-diagram-review.zh-Hant.md](rebuild-diagram-review.zh-Hant.md)：11 月 11 日三張圖的重新評審（用詞、思路、資料的流向）。
- [rebuild-1111-decisions-status.zh-Hant.md](rebuild-1111-decisions-status.zh-Hant.md)：11 月 11 日那批機制的決定在 gyo2 上的現況。
- [rebuild-time-design.zh-Hant.md](rebuild-time-design.zh-Hant.md)：時間、固定步與跨時鐘事件的提案 v1（**沒有通過檢查**，不是待決的提案；v2 待寫）。

## 計畫與批次的紀錄

- [global-audit.zh-Hant.md](global-audit.zh-Hant.md)：全局計畫審計（2026-11-14）——這條線的起點。
- [rebuild-declaration.zh-Hant.md](rebuild-declaration.zh-Hant.md)：`v8-arch-rebuild` 的事前宣告（階段 R0～R6）。
- [rebuild-r0-carry-list.zh-Hant.md](rebuild-r0-carry-list.zh-Hant.md)：R0 帶過去清單（已核准）。
- [rebuild-r0b-report.zh-Hant.md](rebuild-r0b-report.zh-Hant.md)：R0b 的報告（刪除舊實現；第 3 節是 R0b-3 的判定變更）。

## dev_log

- [2026_11_14_arch_global_audit.zh-Hant.md](../../../dev_logs/2026_11_14_arch_global_audit.zh-Hant.md)：全局計畫審計。
- `2026_11_14_gyo2_mechanism_skeleton.zh-Hant.md`：機制的骨架（在分支 `claude/gyo2-mechanism`，PR #225 合併後出現在 `docs/dev_logs/`）。

## 沒有搬過來的

- 圖三 `networking-systems.svg` 沒有改過，留在 [`../../plans/kernel/diagrams/`](../../plans/kernel/diagrams/)；網路的圖等機制完成後再處理（`v8-arch-gyo2-network-diagram-after-mechanism`）。
- 規格（`spec.zh-Hant.md`）、mechanism、blueprint、networking、spike-report 等 11 月 11 日到 13 日的文件留在 `plans/kernel/`，本資料夾的文件引用它們時連到那裡。
