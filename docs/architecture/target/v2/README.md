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
- [rebuild-time-design.zh-Hant.md](rebuild-time-design.zh-Hant.md)：時間、固定步、runner、跨時鐘事件的設計 v2（修訂 4）。主要的決定已定（第 0 節，使用者 2026-11-14）；第 2 節以後的選項表是決定之前寫的，留作紀錄。
- [kernel-nested-schedule-declaration.zh-Hant.md](kernel-nested-schedule-declaration.zh-Hant.md)：事前宣告（K0）：獨占 World 的 System 可以跑一張 schedule。四件事使用者已定（2026-11-15）；整份宣告與第 10 節的細節待核准。
- [time-types-declaration.zh-Hant.md](time-types-declaration.zh-Hant.md)：事前宣告（T1）：累加器與時間的型別。草稿，待核准。
- [nested-queue-spike-synthesis.zh-Hant.md](nested-queue-spike-synthesis.zh-Hant.md)：巢狀時延後的變更怎麼放——三個探索（兩個試做、一個掃描）的統合。草稿，待對抗檢查與使用者判。
- [stamp-order-spike-synthesis.zh-Hant.md](stamp-order-spike-synthesis.zh-Hant.md)：外層刪除、裡面用同一個編號生成——「用 stamp 分先後」三個探索的統合。草稿，待對抗檢查與使用者判。

## 計畫與批次的紀錄

- [global-audit.zh-Hant.md](global-audit.zh-Hant.md)：全局計畫審計（2026-11-14）——這條線的起點。
- [rebuild-declaration.zh-Hant.md](rebuild-declaration.zh-Hant.md)：`v8-arch-rebuild` 的事前宣告（階段 R0～R6）。
- [rebuild-r0-carry-list.zh-Hant.md](rebuild-r0-carry-list.zh-Hant.md)：R0 帶過去清單（已核准）。
- [rebuild-r0b-report.zh-Hant.md](rebuild-r0b-report.zh-Hant.md)：R0b 的報告（刪除舊實現；第 3 節是 R0b-3 的判定變更）。
- [hook-flush-declaration.zh-Hant.md](hook-flush-declaration.zh-Hant.md)：`v8-arch-gyo2-hook-flush` 的事前宣告（hook 的紀錄在 schedule 跑完時套用；修訂 3，使用者 2026-11-15 核准）。
- [spawn-reject-declaration.zh-Hant.md](spawn-reject-declaration.zh-Hant.md)：`v8-arch-gyo2-spawn-reject` 的事前宣告（同步點丟掉不能生成的那一筆；修訂 2，使用者 2026-11-15 核准）。

## dev_log

- [2026_11_14_arch_global_audit.zh-Hant.md](../../../dev_logs/2026_11_14_arch_global_audit.zh-Hant.md)：全局計畫審計。
- [2026_11_14_gyo2_mechanism_skeleton.zh-Hant.md](../../../dev_logs/2026_11_14_gyo2_mechanism_skeleton.zh-Hant.md)：機制的骨架跑通。
- [2026_11_14_gyo2_r0b3_ci.zh-Hant.md](../../../dev_logs/2026_11_14_gyo2_r0b3_ci.zh-Hant.md)：R0b-3（守衛與 CI 政策的調整），PR #225 併入。
- [2026_11_14_gyo2_initial_value.zh-Hant.md](../../../dev_logs/2026_11_14_gyo2_initial_value.zh-Hant.md)：漏斗工具 `Registrar::InitialValue`。
- [2026_11_14_gyo2_strict_order.zh-Hant.md](../../../dev_logs/2026_11_14_gyo2_strict_order.zh-Hant.md)：歧義的嚴格模式 `Registrar::RequireExplicitOrder`。
- [2026_11_15_gyo2_funnel_check.zh-Hant.md](../../../dev_logs/2026_11_15_gyo2_funnel_check.zh-Hant.md)：漏斗工具的對抗檢查與區規則，PR #229 併入（標記為需要產品測試）。
- [2026_11_15_gyo2_hook_flush.zh-Hant.md](../../../dev_logs/2026_11_15_gyo2_hook_flush.zh-Hant.md)：hook 的紀錄在 schedule 跑完時套用（清空輪、兩層上限、指紋升版）。
- [2026_11_15_gyo2_spawn_reject.zh-Hant.md](../../../dev_logs/2026_11_15_gyo2_spawn_reject.zh-Hant.md)：同步點丟掉不能生成的那一筆（三條斷言改成丟掉、留明細與總數、LIM-7、指紋升版）。
- [2026_11_16_gyo2_nested_schedule.zh-Hant.md](../../../dev_logs/2026_11_16_gyo2_nested_schedule.zh-Hant.md)：K0，獨占 World 的 System 可以跑一張 schedule（巢狀執行、`ExclusiveWorld<…>` 的閉包與帶寫入的宣告、開局由 Kernel 跑、指紋升版）。
- [2026_11_16_gyo2_module_plugin_definition.zh-Hant.md](../../../dev_logs/2026_11_16_gyo2_module_plugin_definition.zh-Hant.md)：Module／Plugin 的定義改成「誰維護、介面是誰的」，AGENTS §6 與 v2 文件對齊。

## 沒有搬過來的

- 圖三 `networking-systems.svg` 沒有改過，留在 [`../../plans/kernel/diagrams/`](../../plans/kernel/diagrams/)；網路的圖等機制完成後再處理（`v8-arch-gyo2-network-diagram-after-mechanism`）。
- 規格（`spec.zh-Hant.md`）、mechanism、blueprint、networking、spike-report 等 11 月 11 日到 13 日的文件留在 `plans/kernel/`，本資料夾的文件引用它們時連到那裡。
- [recovered-from-stale-cards.zh-Hant.md](recovered-from-stale-cards.zh-Hant.md)：從前提已變的任務卡（狀態機、重新評估 Loading）回收的資源，以及哪些舊設計不要帶進來（2026-11-17）。
