# 交接：架構師 → 圖書管理員（2026-11-12）

這不是健檢報告，是架構師在 kernel 改造期間累積、屬於文件整理的事項清單，交給圖書管理員會話排進下一次整理。
資料夾結構與放置由圖書管理員決定；架構內容的判斷不在這裡（看起來像架構漂移的發現，回報架構師會話，記在 `docs/architecture/plans/kernel/HANDOFF.md` 的「下一步」或本檔末的「回報架構師」節）。

## 0. 先避開在飛的檔

PR #185（`claude/arch-remove-object-fps`，待 L1 與使用者合併）改了這些現行文件：三份根 README、`docs/architecture.md`、`creating_apps.md`、`tool_projects.md`、`releasing.*`、`rendering_architecture.*`、`ui_toolchain.md`、`architecture/animation-events.md`、`object_fps_pvp/assets.zh-Hant.md`（新附錄 A）、三份 `plans/*/inactive_products.md`、`tools/plans/v8/{README,model-viewer,assets}.md`、kernel 計畫文件、新 dev_log。
**從 #185 合併後的 master 開分支**，否則會衝突。

## 1. 文件健檢逾期

上一份報告基準 `5ee499a`（2026-10-21），到 `56b8dda3` 已有 1304 個 first-parent commit。照 `README.md` 的手動步驟做一份新報告（`2026_11_XX_documentation.zh-Hant.md`），再分批修。

## 2. `-remove-object-fps` 留下的（PR #185 的審查 N4、N6）

- 「Object_FPS」「ObjectFPS」字樣（審查 N6；指 v1 現況的要改，指產品族或 pvp 的不改）：`docs/architecture.md:136`（「understand Object_FPS asset names」）、`:293`（「The current Object_FPS runtime layout includes …」）；`docs/creating_apps.md:87`（「Object_FPS currently needs only `builtin`」）；`docs/rendering_architecture.*:55、66、129、248`（圖與表以 Object_FPS 當遊戲端範例、「Object_FPS 目前…」；`:211～227` 是 2026-09-16 的歷史證據，標明是歷史，不改）；`docs/ui_toolchain.md:203`（「Object_FPS stores Exposure and Gamma in its app runtime client」）、`:223`（「does not link ObjectFPS」）。不改的例：`docs/object_fps_pvp/log-contract` 的 `[ObjectFPS/PvP …]` 日誌前綴、`network-architecture:164、177` 的 ObjectFPS Adapter。
- `docs/tools/plans/v8/ui-navigation.md:40`：「不碰 `object_fps_v2` … v2 有同樣的問題，要做另排一批」已失效（產品已移除），加「已移除」註記即可，是計畫文件。
- `docs/architecture/3d-assets.md` 已刪、授權記錄搬到 `docs/object_fps_pvp/assets.zh-Hant.md` 附錄 A：全 repo 的入鏈由審查者查過現行文件，`plans/` 與 `dev_logs/` 的歷史連結不改；請在健檢的斷鏈項再掃一次確認。
- 三份 `inactive_products.md` 已在檔頭加一行「產品已移除」；內容是歷史，不改。

## 3. 跨元件規則登記頁（使用者 2026-11-07 核准；未建）

- 位置與語言：`docs/cross-component-rules.md`，英文；從 `docs/architecture.md`「Repository ownership」節與 README 文件一覽連結。
- 每列：規則一句話｜唯一歸屬（owner＋文件節）｜守衛（測試／突變／審查）｜已知實作或引用處。
- 頁首用法：寫「某個 Engine／framework 契約的直接後果」類規則前先查；查到引用，查不到登記並寫歸屬；歸屬有爭議交架構師。
- 第一條：選擇窗記帳規則（每次請求新 tag、只認最新、Chosen／Cancelled 結束、Failed 不結束、可放棄）。歸屬 framework `Framework::PathDialogRequests<Purpose>`（`v8-tool-path-dialog-rule-handover`）；之前的現況是 platform-sdl DI 10＋ui_editor／audio_editor／model_viewer 各一份。
- 工具線 2026-11-11 轉交的查證資料（建頁時照原始碼再查）：合規實作 ui_editor 的 `PathDialogRequests`、audio_editor 的 `ApplyDialogAnswers`；已知不合規 `tools/model_viewer/src/ViewerWindow.cpp`（`:43` `kContentDialog = 1` 固定 tag、`:112` 讀回答、`:621-627` 送請求；讀碼結論、未實機重現），處置併入 `v8-tool-path-dialog-rule-handover`、隨功能線暫停；工具線紀錄在 `docs/tools/plans/v8/HANDOFF.md`「已知缺陷（改造期間不修）」。
- 注意：framework 已決定退役→`plugins/`（kernel HANDOFF `v8-arch-plugins-layer`，audit §3.1）。登記頁的「歸屬」寫目標歸屬時標明「目標（遷移批）」，現況另寫一欄，不要把兩者混在同一格。

## 4. 新舊架構文件的分隔（kernel HANDOFF `v8-arch-docs-next-folder`）

使用者 2026-11-11 決定：現況架構文件留 `docs/architecture/`，目標架構放 `docs/architecture/target/`，不混放。請在健檢時核對：
- `docs/architecture/target/README.md` 的部件一覽與 `docs/architecture/plans/kernel/spec.zh-Hant.md` §L 是否同步（spec 是來源，target 是部件文件）。
- 現況文件裡已被 -spec-v03 標上「目標見 target/…」的段落（`architecture.md`、`entity-store.md`、`determinism.md` §4.7 系、`object_fps_pvp/entity-model.zh-Hant.md` §5.2、`error-handling.md`、framework 的廢止標示、`mechanism.zh-Hant.md` §3.3）措辭是否一致、連結是否通。
- README 三語的文件一覽有沒有列 `target/` 與 `plans/kernel/`。

## 5. dev_log 與停線記錄的缺漏

- kernel 線各批的 dev_log：`2026_11_11_arch_kernel_spike`、`2026_11_11_arch_kernel_audit`、`2026_11_12_arch_kernel_spec_v03`、`2026_11_12_arch_kernel_spike_pageguard`、`2026_11_12_arch_remove_object_fps`；`-spec` 批（v0.1，PR #178 前後）請確認有沒有獨立 dev_log，沒有就記為缺漏回報架構師補（dev_log 由做那批的會話寫）。
- 2026-11-11 停法 (a) 之後，產品線、工具線、voxel 線的 HANDOFF 停線記錄各自「下次變更時補」（kernel HANDOFF 現況 2026-11-11 條）。健檢時查三線 HANDOFF 開頭有沒有補；沒有就列缺漏。

## 6. 用字

- 日語文件與回覆不用中文詞義：ミューテーションテスト、パッケージング、アセット、共通層、ローカル等（PR #121 的規則）。
- 繁中檔不混簡體與日文字形、避免大陸用語（README 第 6 條）。
- 本批新寫的 `docs/object_fps_pvp/assets.zh-Hant.md` 附錄 A 是從英文翻的，順手核一次用字。

## 回報架構師（圖書管理員填）

（看起來像架構內容不一致、文件說法互相矛盾而不是過時的，寫在這裡，架構師會話來讀。）
