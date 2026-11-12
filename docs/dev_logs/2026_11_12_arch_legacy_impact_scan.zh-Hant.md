# 架構 v8：`v8-arch-legacy-impact-scan`（舊架構文件對新架構的影響掃描）

日期：2026-11-12。Owner：架構（架構師會話開出的任務卡；子會話執行）。分支 `claude/arch-legacy-impact-scan`（自 origin/master `c3ce8e9f` 開、rebase 到 `53ead000`）。模型／檔位：主體 Fable 5.1／high；對抗複核 Opus／high（只讀）。

## 做了什麼

- 依 `v8-arch-docs-legacy`（kernel HANDOFF 決定表，PR #191 分支）的規則「現有程式受 legacy 的 DI 約束、新程式（`engine/kernel` 起）受 target 約束」，掃描卡片點名的 36 檔（legacy 九檔、現況文件、工具設計文件、操作類、AGENTS.md 引舊結構的條文），每檔找四類東西：Design Invariants／Replacement Criteria 的三選一判定（相容／衝突／被取代）、守衛測試引用的規則會不會誤判新程式、未加註的誤導句、target／spec 對 legacy 的反向依賴；另一節列操作類文件會過時的流程。
- 產出報告 `docs/architecture/plans/kernel/legacy-impact.zh-Hant.md`；kernel README 文件清單與 HANDOFF 現況各加一行。零修改：沒有改任何舊文件、target、spec、測試。

## 結果（件数の突き合わせ：一致）

- 掃 37 檔（另 1 個點名對象不存在：`docs/tools/*.md` 頂層；`docs/cross-component-rules.md` 在掃描期間由 PR #190 建立、補掃）。
- Design Invariants 198 條：相容 177／衝突 2／被取代 19；Replacement Criteria 134 條：相容 116／衝突 1／被取代 17；無 DI 節的檔另判 15 條契約條文（相容 8／衝突 1／被取代 6）。
- 衝突 4 條（＋規則層候選 1）：error-handling.md L124「no error chains」vs `KernelError` 鏈；preview.md DI 4 readback vs 資料只往前流；field-session.md DI 3 vs 執行期驗證開關；os.md RC 9（D78）vs 頁保護進 engine/os（條件：採用第三層時）；AGENTS §9 vs plugin 清單在 main（候選）。卡片列的衝突候選 threads DI 4、determinism DI 1～3 不成立；entity-store DI 1、4 被取代。
- 守衛 21 個：會誤判新程式 1 個（`neutral_names_guard.py` 的 WORDS 含 spawn／zone／extract，條件：被套到 kernel）；`os_boundary_guard` 會正確攔住放進 kernel 的 OS 碼（決定頁保護的位置）；framework 退役的影響檔 4 組。
- 誤導句 33 條（4 條已被 PR #190／#191 處理、1 條部分）；反向依賴 91 處（行號引用 13 處；spec 引 entity-model 的「第 139 行」等在 PR #191 後已實際失效；target/determinism 刻意不重述 legacy 的 §1／§2）。
- 對抗複核（Opus 5.5／high，約 8 分鐘）：抽 10 條（全部衝突 4＋候選 1＋5 條），一致 8／不一致 2（field-session DI 3 複核判相容附誤導風險；entity-model E-3 複核判相容）；原判定不改、並列（報告 §8）。複核另指出基準過時（master 在掃描期間前進，已 rebase 到 `53ead000` 並加行號偏移表）、§6 漏列 15 處（已補）。

## 驗證與經緯

- 守衛掃描用 `git grep -nP`，先以正例（`entity_store_guard.py:1` 引用 entity-store.md）命中、退出碼 0；寬掃 476 命中、窄掃約 200；反向引用掃描 110 命中、逐筆歸類。
- 判定門檻在開始前宣告並給使用者看過（覆蓋 100%、DI 100% 有三選一、守衛 100% 有誤判判定、對抗抽 ≥8 含全部衝突、零修改 4 檔、單檔 ≤1.5 h）；parent 會話在本卡的會話列表找不到，門檻未經 parent 核定。
- 用時約 4 小時（估 4；含 rebase 與併入複核）；沒有單一文件超過 1.5 小時。
- 範囲の限界：判定是文件推論，沒有實跑 kernel 程式或在 kernel 假目錄上實跑守衛；network-architecture 的 §3、§4、§8～§9 只掃標題；AGENTS.md 只掃引舊結構的條文。

## 證據

主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/legacy-impact/`（git 管理外）：`guard-scan.txt`、`guard-scan-narrow.txt`、`reverse-scan.txt`、各 `.stderr`、`guard-scope.txt`、`adversary-report.md`。
