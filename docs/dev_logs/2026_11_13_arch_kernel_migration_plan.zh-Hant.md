# 2026-11-13 架構：`v8-arch-migration-plan`（kernel 改造的遷移批規劃）

Owner：架構（架構師會話）。計畫：`docs/architecture/plans/kernel/`（README 批次樹、HANDOFF、`migration-plan.zh-Hant.md`）。狀態：**規劃稿完成、待使用者核准**；不寫程式。

## 做了什麼

- 2026-11-12 使用者同意用 ultracode 規劃。workflow：提案者 Opus×3／high（風險優先、價值優先、成本優先）→ 統合者 Fable／high → 對抗檢查者 Opus／xhigh。提案三份與統合稿 790 行，第一輪對抗 blocker 4／should 19／nit 16。
- 架構師逐條定處置（HANDOFF `v8-arch-migration-plan-round1`），修訂由子代理套用（第一次誤用 Fable，使用者指出用量後停掉改 Opus／high；教訓記入記憶 `subagent-model-explicit`）。第二輪對抗 Opus／xhigh：blocker 1（未吸收 -zones 核定的門）／should 11／nit 14、第一輪 5 條只改一半 → 處置 `v8-arch-migration-plan-round2`（loader 由 plugin 登記、`AssetInbox` IO 服務歸 plugins-foundation、預算改 `BU-n`、24 處殘留逐條分派）。
- 使用者 2026-11-12 追加橫切規則 `v8-arch-guards-migrate-with-code`（守衛與突變隨程式遷移：四選一去處、以性質判、同批退役、突變重列經不同模型敵對檢查、§0 A10）與決定 `v8-arch-plugin-selection-data`（plugin 選擇是資料，U2 結案）。同輪套入。
- 第三輪對抗（Opus／xhigh，限定範圍）：blocker 0／should 8／nit 12；處置套入後驗收點檢（Opus／high）：A1～A10 成立、數字一致、8 條半改補齊。最終稿 1050 行：34 批＋條件批 1、閘門 G1／G2a／G2b／G3、Delta-1～6、BU-1～BU-8、U1～U14、§5.1（24 處殘留分派）、§5.2（21 個守衛的去處批）、文末處置記錄三節。

## 驗證與經緯

- 受理判定（計畫 §0 A1～A10）：最終輪對抗 0 blocker（A9）；每批有 Feature→Contract→Architecture；§3 七項在計畫層一次；模型／檔位照 navigator／gearbox；BU 引用與 §6 一致；守衛去處與 DI 落點齊。驗收點檢報告 `verify-r3.md`。
- **A9 的文字在看到第二輪結果後由「第二輪 0 blocker」改為「最終輪 0 blocker」**：看結果後改判定文字，已列 `v8-arch-migration-plan-a9` 待使用者核准；理由是原文釘的是輪數不是性質。
- 數字：合計 632～1,299 h（含測試、守衛與突變去處 46～92 h、新批推定）、關鍵路徑 331～692 h、M3 115～247、M4 181～389；多數預算與 Kernel 工時標「推定」，等 `v8-product-pvp-baseline`（已開卡）量出今天的值再核。
- 架構師自己的處置被對抗者修正兩次並接受：S-18（PvP 欄位表不進 target/world）、第二輪 S2-5（撤回「loader 由 host 直接登記」）。

## 證據

- 主 checkout `build/evidence/kernel-migration-plan/`（git 管理外）：`proposal-{risk,value,cost}.zh-Hant.md`、`adversary-report.md`、`adversary-report-r2.md`、`adversary-report-r3.md`、`verify-r3.md`。workflow 逐字稿在 Claude app 的 session 目錄（會消失，不當證據）。
- 分支 `claude/arch-migration-plan`；同分支含決定行 `v8-arch-plugin-selection-data`（target/runtime-composition §4、DI 2 已改）、`v8-arch-guards-migrate-with-code`、`v8-product-kernel-prep-zones-gate`、`v8-arch-migration-plan-round1／round2／a9`。

## 待使用者

核准計畫；核准 A9 文字修正；未決 U1、U3～U14（U2 已定）。核准後第一批：`v8-arch-a6-revision`（架構師）與第 0 段的 kernel-scaffold、h7-scanner（任務卡）。
