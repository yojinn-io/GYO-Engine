# framework 整改：v8-framework-engine-access（framework 可用任何 Engine 模組、共通功能先放 framework）

日期：2026-11-01。分支 `claude/framework-rework`。上段（Design Invariants 與其守衛）。実装者：主會話 Opus 5.5／high；§14.4 検証者：Fable 5.1／high（使用者同意）。估 3～5 h，實際約 3.5 h（含因 PR #132 重做的約 1 h）。

**狀態：** 完成（Mac 上的自測、突變、全量 CTest）。Windows、Linux 由 CI 的 L1 驗證。

相關文件：[判定變更](../architecture/plans/framework-rework/engine-access-judgment-change.md)、[HANDOFF](../architecture/plans/framework-rework/HANDOFF.md)。

## 做了什麼

- **起因**：架構會話查證，framework DI 2 的「不連結 `GYO::Input`、不用 SDL_GPU」源自 10-30 的 ui_editor DI 1，沒有記錄理由，而且和 Editor → Engine 的方向相反；已造成 Input→Ui 轉換寫兩份。使用者的規則：Editor 可以連 framework 與 Engine，共通功能優先 framework。
- **條文**（`docs/framework/architecture.md`）：DI 2 改為 framework 不連遊戲、不點名產品、可用任何 Engine 模組（含 input），SDL 只經 Engine 的 SDL 後端目標，`*Native` 只在綁定且要 Engine 契約允許；加非 `*Native` 的 Engine target 不算改 DI 2，但守衛變更仍照 §14.4。DI 5 改為兩個以上產品用的先放 framework（即使 Engine 已有），遊戲自己的組裝仍等第二個遊戲。用語和工具線 T4 的兩份工具 DI 1 對齊。
- **守衛**（`tests/framework/framework_dependency_guard.py`）：核心清單加 `GYO::Input`、`GYO::InputBackendSDL`；保留 #132 的「核心不碰 input 的 `*Native` 標頭」硬規則；SDL_GPU 硬規則保留到例外清單清空（PR B）。
- **經緯**：第一版在 PR #132 之前做，#132（工具線的 IME 修正）改了同一批檔案，使用者決定等合併後以 #132 為起點重做；第一版存成 patch 留在證據資料夾。

## 驗證與經緯

件數の突き合わせ：一致（宣告 24・執行 24・killed 24、stale 0；検証者修正後宣告 2・執行 2・killed 2）。

- **§14.3／Red→Green**：自測把 #132 的「核心連輸入後端失敗」換成「核心連 Input 通過」並新增綁定不得連 `GYO::Input`；改守衛前 47 條中 1 條失敗，改後 47/47。
- **突變**：fw-guard-input-target 因允許 `GYO::Input` 而作廢，換成 fw-guard-unlisted-engine-target；新增 binding-input-target、gpu-added（原 gpu-trade）、gpu-rule-dropped、binding-list-widened、input-native-allowlistable。24/24。第一次呼叫執行器時 zsh 把 id 清單當一個字串，0 條執行，記錄後改用 xargs。
- **探測**：暫時讓有視窗的核心 target 連兩個 input target，configure 的連結檢查與守衛都通過，已還原。
- **Fable 検証者**：0 blocker、2 major、2 minor。漏列的收緊面（例外計數涵蓋 `GYO::InputBackendSDL` 時要求縮小清單）、DI 2 新句會預先免除 §14.4 → 都已更正；SDL_GPU 規則的理由寫錯（它防的是在例外檔案「新增」）→ 守衛說明、自測與突變已改，重跑 2/2。
- **全量 CTest** 123/123。

## 證據

使用者的 Mac，主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/framework-rework/v8-framework-engine-access/`（git 不追蹤，見該資料夾的 README）。検証者的原始輸出在會話暫存區，未保存。
