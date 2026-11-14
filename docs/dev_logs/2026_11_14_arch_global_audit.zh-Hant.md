# Kernel 研究線：v8-arch-global-audit（全局計畫審計）

日期：2026-11-14。Owner：架構（架構師會話）。分支 `claude/arch-rederive-detect`。報告：[global-audit.zh-Hant.md](../architecture/plans/kernel/global-audit.zh-Hant.md)。

- 執行：審計 A、B、C 用 Fable／high；基本前提的對抗用 Opus／xhigh；統合用主會話 Opus；統合稿的敵對檢查用 Fable／high，一次。這是使用者 2026-11-14 指定的編制。
- 性質：只讀審計；沒有改程式。

## 起因

使用者在 hook 重審時指出，spec 的取捨沿用了已撤回的 entity-store 規則（「舊系統架構影響了新系統架構的設計」）。之後又提出：「GYO 現在就是 PvP 的設計下到 Engine 層了」。卡 07 的 entity-store 重審和 PvP 滲入檢查回來後，使用者下了這些指示：
- 停下卡 01～06 與卡 08，匯總後封存。
- 做一次全局計畫審計，分出舊架構污染、PvP 污染與合理，並附一次架構基本前提的對抗。

## 做了什麼

| 項 | 位置 | 內容 |
|---|---|---|
| 封存匯總 | 證據 `card-archive-2026-11-14/SUMMARY.md` | 七張卡的分支、產出與未決。卡 01、05 的本機 commit 推到 origin，沒有開 PR。卡 08 的未 commit 改動存成 patch |
| 三份審計與一份對抗 | 證據 `global-audit-2026-11-14/` | A：計畫與決定的系譜。B：目標條文的系譜，追到 commit。C：實作的回歸，對照 #213、#218 和卡 08。前提對抗：攻擊 Kernel 的基本前提 |
| 統合 v1 與敵對檢查 | 同上 | v1 被 Fable 推翻：分類法有反向偏差；「四份一致」「判官全是舊行為」不實；只給一條路 |
| 統合 v2 | 報告本體 | 加入「拿掉舊理由後結論還贏不贏」的判定與 O-結／O-理／P-範／S／? 分類；停點改成小、中、大三案附估時 |

## 驗證與經緯

1. 第一次開的三個 Opus 審計被使用者停掉。使用者的意思是「審計用 Fable 跑」，架構師誤讀成「統合用 Fable」。照使用者的編制重開。
2. Fable 對 v1 的抽查：13 項屬實，6 項不實或誇大。不實的項目都已在 v2 改正。
3. 使用者看過 v2 第 5 節後，定了四件事，見 kernel HANDOFF 決定行：目標是通用引擎、實質是 GYO2 但放在 GYO repo、單機要支援且不增加複雜度、停點選「中」。
4. 同日使用者提出新分支重建，架構師同意並補三點（帶過去清單由使用者逐項核准、第一個搬的產品不是 PvP、master 凍結）。使用者決定走新分支（`v8-arch-rebuild-branch`），並要求先開分支、不在 master 上做。長期分支 `gyo2` 從 master 08566260 切出；本批的文件以 PR 進 `gyo2`。「中」的宣告草稿改寫成 [rebuild 事前宣告](../architecture/plans/kernel/rebuild-declaration.zh-Hant.md)。

## 證據

主 checkout `build/evidence/global-audit-2026-11-14/`（git 不追蹤），內含：
- 四份輸入
- 統合 v1、v2
- Fable 敵對檢查

另見 `build/evidence/card-archive-2026-11-14/`、`build/evidence/pvp-leak-check/`、`build/evidence/kernel-hook-window-review/`、`build/evidence/kernel-allotter-a1/`。
