# 2026-11-22 事件游標的重評（`v8-arch-gyo2-event-cursor-reeval`）

Owner：架構（GYO2 主會話開的任務卡）。分支 `claude/elastic-jones-5af7fa`（開向 `gyo2`）。作者 Opus 5.5；宣告的敵對式檢查與合併前的設計審查 Fable 5.2／high（唯讀）。

## 為什麼

生成世界的設計審查指出：被 run 條件擋住的 `Message` 讀者會一直拉住保留。狀態機修訂 8 的五家對照發現別家都沒有這種行為。使用者 2026-11-22 在狀態機卡選 (A＋)：狀態機那一批照 (A)（不改契約、說明頁寫明）做，另外現在就開這張卡重評（HANDOFF `v8-arch-gyo2-state-machine-r8-crosscheck`）。

## 經過與結論

宣告：[event-cursor-reeval-declaration.zh-Hant.md](../architecture/target/v2/event-cursor-reeval-declaration.zh-Hant.md)（修訂 3，使用者核准）。

| 修訂 | 建議 | 檢查 |
|---|---|---|
| 0（338e40a0） | (R1) 被條件跳過一律算讀過＋(R2) 讀者出生就拉住 | Fable／high：blocker 3、should 7、nit 7。(R1) 推翻的是時間方案二設計稿明寫的賣點（帶條件的讀者不再漏讀，`rebuild-time-design.zh-Hant.md:421`、`:432`），而且自己多加了「條件判否＝關閉」的前提（節奏型條件會靜默漏讀）；(R2) 讓讀取畫面期間的輸入在第一個固定步一次讀到 |
| 1、2（d87698a8、f957143b） | (C) run 條件在定義時宣告「判否時放開讀者」，`InState`／`WhileGenerating`／`AfterGenerating` 宣告；維持「跑過一次才拉住」 | 窄複查 Fable／high：blocker 1、should 4、nit 6（求值 short-circuit 讓 set 上的宣告輪不到等），修訂 2 補四條附屬規則 |
| 3（f7aeaa6f） | (A)＋說明頁，不改 Kernel；加 RC 24（草案） | — |

修訂 2 之後，使用者問「在不違反GYO的規範前提下，有沒有解決方案？」，再要卡片「先確認這個問題的前提。是Engine需要解決的，還是作者需要解決的？」。卡片的查證：作者不改 Engine 就處理得了（讀者不帶狀態條件、每幀讀了不要的就丟；要跨過狀態轉換的放 Resource）；成熟引擎都交給作者（Bevy 核心要讀者每幀讀、只有狀態 Plugin 層事後補清除工具；lightyear 範例的接收器不帶狀態條件；flecs、Unity 看狀態）；坑是 GYO 的預設（被跳過要補讀）造成的，所以 Engine 要講清楚；(C) 等於 Engine 替作者決定遊戲規則。使用者：「同意，改成 (A)＋說明頁」，之後「核准，加 RC 24，開始」。兩次都照四樣轉給主會話。

## 做了什麼（只改文件）

- `docs/architecture/target/v2/system-contract-page.zh-Hant.md`：新第 15 節「帶條件或不常跑的讀者」（三個後果、「會不會拉住、補讀」表、寫法、三種暫停寫法的差別、為什麼 Engine 不替作者處理）；第 5 節「訊息」與第 14 節連過去。
- `docs/architecture/target/kernel.zh-Hant.md`：RC 24（草案）「帶條件的讀者要補償」。DI-KS 10 不變。
- `docs/architecture/target/v2/event-cursor-declaration.zh-Hant.md`：頂端註記（重評、維持不改）與兩處事實更正（lightyear 行號漂移；Bevy 在固定步暫停時無上限累積）。內文不改寫。
- `docs/cross-component-rules.md`：CCR-7（owner：Kernel 的 DI-KS 10，作者的寫法在 System 契約說明頁第 15 節）。
- `docs/architecture/target/v2/README.md`：宣告與本 dev_log 列入。

沒改：Kernel 的程式、契約、測試、突變；日誌 Module 的 LG-1；狀態機用的介面。

## 檢驗

- 程式沒有改，沒有建置與測試；突變不跑（沒有 find／expect 受影響）。
- `git grep -n "MessageReader" -- engine/modules engine/orchestration`：只命中 `world_logging`（`Forward`，不帶條件）與 `rollback`（讀讀者名單），沒有 Engine 的 Module 把讀者放在長期跳過的條件下。這個命令以 `Forward.hpp` 當正例命中。
- 合併前的設計審查：見下節。

## 合併前的設計審查

（審查後補）

## 證據

這台 Mac 的主 checkout `build/evidence/gyo2-event-cursor-reeval/`（git 不追蹤）：`source-verification.md`（本卡重讀的原始碼行）、`declaration-check-r0-fable.md`、`declaration-check-r1-fable.md`（兩次檢查）。五家對照在 `build/evidence/gyo2-state-machine-r8/crosscheck-message-retention.md`。

## 留給之後

- 狀態機那一批的說明頁可以連到 System 契約說明頁第 15 節（已請主會話轉告）。
- 作者反覆寫補償時照 RC 24 重評；候選 (C)、(E) 的全文在 f957143b。
