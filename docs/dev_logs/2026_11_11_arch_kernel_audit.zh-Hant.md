# 2026-11-11 架構：`v8-arch-kernel-audit`（現有模組對規格 v0.2 的審查）

Owner：架構（架構師會話）。計畫：`docs/architecture/plans/kernel/`（README 批次表、audit-declaration、audit）。

## 做了什麼

- 主會話先寫共用的決定性掃描腳本 `plans/kernel/scripts/audit_scan.py`，全範圍一跑：44 模組、645 命中（`build/evidence/kernel-audit/scan-full/`）。
- 三個 Opus／high 審查者並行（engine／framework-tools／product-thirdparty），各在獨立 worktree 寫部分報告（`audit-part-*.zh-Hant.md`）；第三份另寫宣告的四節（Body 拆欄與 golden、Host 職責位置、G.4 確認、第三方重核）。
- 第一份回來即發現事前宣告的判法「Q1 為是→組織層」比 spec I.3 粗；主會話對齊為 I.3 的讀法並通知另兩份重核（engine 改一列、product-thirdparty 改六列）。這是判法對齊，不是看結果後放寬。
- 主會話逐列讀三份後合併成 `audit.zh-Hant.md`，做 (e) 決定性區表回對（漏 3、標錯候選 1）。
- 對抗複核 Fable／high 只讀抽 5 模組：4/5 一致；不一致的 engine/input 後端重判為 Host 並寫明原因；複核者指出判法五處模糊，定義補進 audit §0。

## 驗證與經緯

- 事前宣告五條判定：覆蓋、證據、對抗複核（4/5）、掃描可重跑且件数一致（44＝22＋8＋14；645＝279＋180＋186）、不改程式——全部成立。
- 件数の突き合わせ：一致。
- 審查者估 3 h／1.5～2 h／3 h，實際 10 min＋重核 15 min／1.5 h／2.9 h；複核 50 min；都在上限內。

## 結論（摘要；詳細在 audit.zh-Hant.md）

- 工時形狀約 415～850 h（不含 Kernel 本體），重寫集中在組織層；Engine 大多留或包，與 spec L.5 預測一致。
- 要使用者決定：framework DI 2／6 的 Delta；Editor 預覽回流（晚一幀）；IRuntimePort 與 AnimationInstance 刪除；AssetManager 的 World 歸屬；初始成員走 Startup；決定性區表三漏一錯。
- 規格要改七項（audit §6），最重要的是 (c) 區在現行 S-8／Z4 下不能輸出診斷，與 G.4 第 3 條衝突。
- 無結論交遷移批：Body 拆欄後 Life 欄的兩個寫者與同 tick 讀後寫。

## 證據

主 checkout `build/evidence/kernel-audit/`（git 不追蹤；使用者的 Mac）：`scan-full/`、`engine/`（含 `supplementary/`）、`framework-tools/`、`product-thirdparty/`（含 `supplement-app-root/`、`thirdparty_manual_grep.txt`、`h7_include_scan.txt`）。三個審查者的 worktree 分支（`worktree-agent-*`）保留部分報告的原始 commit，報告檔已複製進 `claude/arch-kernel-audit`。
