# framework 整改：v8-framework-rework-plan（定計畫）

日期：2026-11-01。分支 `claude/elated-antonelli-4dbcac`（切自 master `ebfe3225`）。檔位：規劃用 ultracode，文件 medium；主會話引擎由 Fable 5.1 換成 Opus 5.5（使用者切換）。估 2～3 h（文件部分）；規劃的 workflow 約 39 分鐘（6 個子 Agent）。

**狀態：** 完成。使用者核准 PLAN 第 1 版與 D-1～D-9（全部採建議）。程式未改。

相關文件：[README](../architecture/plans/framework-rework/README.md)、[PLAN](../architecture/plans/framework-rework/PLAN.md)、[HANDOFF](../architecture/plans/framework-rework/HANDOFF.md)。

## 做了什麼

- 架構會話交下的職責說明與修正方向第 2 版，寫進計畫資料夾的 README（使用者原則原話、目標分層、交接、不做）。原稿另存為證據。
- ultracode 規劃：核對現況 1（medium）→ 獨立提案 3（最小 Delta、風險優先、使用者優先）→ 審查合成 1 → Sonnet 對抗式檢查 1。引擎除對抗式檢查外都繼承主會話。
- 寫 PLAN：7 批（加上 D-6 的小批共 8 批），合計估 57～78 h，不含工具線。PR A 只做加法，工具切換後 PR B 刪舊件。
- 使用者核准後，把 D-1～D-9 記進 HANDOFF。

## 驗證與經緯

- 核對現況：職責說明的 16 項事實中，12 項成立，4 項有偏移或需要補充（行號、`build/cmake/` 路徑、`FileService` 與 io.md DI 2、DI 3 不合、`ReadOnlyAssetCatalog::Mount` 不經 ContentManifest）。
- 合成評分（40 分滿分）：最小 Delta 32、風險優先 34、使用者優先 31。骨架取風險優先，介面取最小 Delta，外殼原則取使用者優先。
- 對抗式檢查：blocker 1、major 8、minor 5，全部在 PLAN 第 7 節處理。主會話自己核對了其中三項（`ReadOnlyAssetCatalog` 用 `AbsolutePath`、core preset 不建 framework、pvp 設 `imeComposition`），也核對了「CI 沒有 sanitizer」。
- 和職責說明不同的結論：刪舊件移到最後一批；T1 不能早於外殼批；FileService 不照搬；cases.json 不必補案例；批次由 4 批改成 7 批。理由與證據在 PLAN 第 1 節。
- 過程中的修正：README 初稿把方案原稿寫成「在會話暫存區」，使用者指出違反 AGENTS §15，改成存到 build/evidence 並寫明位置。

## 證據

使用者的 Mac，主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/framework-rework/`（git 不追蹤）：

- `2026_11_01_framework-fix-direction-v2.md`：架構會話的修正方向第 2 版（SHA-256 `cdef6a07…a17e374`）。那次審查的原始輸出未保存。
- `2026_11_01_plan-workflow-output.json`：規劃 workflow 的最終回傳值。
- `2026_11_01_plan-workflow-journal.jsonl`：逐 Agent 回傳值，三個提案原文只在這裡。
