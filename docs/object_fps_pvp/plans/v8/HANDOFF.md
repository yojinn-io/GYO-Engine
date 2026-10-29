# PvP v8 交接

更新：2026-10-29。Owner：`object_fps_pvp`。**狀態：`v8-plan-sync` 完成，實作未開始。**
本文件是 v8 的記錄器：每批開始、里程碑、停止時，和工作在同一個變更中更新。人看的計畫在 [README](README.md)。

## 代號對照

批次代號像函數名，下表是它的註釋；樹狀總覽與各批明細見 README。

| 代號 | 意思 | 出處 |
|---|---|---|
| D243～ | 使用者在 v8 的決定（接續 v7.1 的 D86～D242） | 本文件「決策紀錄」 |
| ①～⑤ | 五組：基礎功能、體驗優化、手感時序、工具、流程項目 | README「樹狀總覽」 |
| v8-plan-sync | 寫本計畫與本文件 | 同上 |
| audio-line（au1-plan、au1、product-sfx） | Engine 音效＋產品音效 | 同上 |
| pv8-line（pv8-design、crouch） | pv8 契約與蹲下，三角色同一個 PR | 同上 |
| hitbox-line（hitbox-data、hitbox-rewind、hit-rules） | 部位 hitbox | 同上 |
| sprint、ads | 衝刺、瞄準 | 同上 |
| analyzer-105、phase-golden、correct-assert、b6-plan、b6、a4 | v7.1 交過來的手感時序（INVENTORY 的 N-analyzer-literal-105、B10、N-correct-invariant-assert、B6、A4） | [v7.1 INVENTORY](../v7.1/INVENTORY.md) |
| ui-editor-widgets、voxel-map-editor、assets-editor | 工具 | README |
| match-rules、spawn-layout、pause-menu、display-settings、warmup-hidden、player-look | 流程項目 | README |
| pvN／vN | 網路協議版本／遊戲版本 | v6 交接 D20 |

## 現況（每次停下來時更新）

- **完成**：`v8-plan-sync`（分支 `claude/pvp-v8-plan-sync`）。
- **下一批**：① 的 `au1-plan` 或 `pv8-design`。兩條 PR 線可以並行，先後由使用者決定。
- **等你決定**：
  - 不屬於玩法的 4 項放在 v8，還是另開維護線：驗收工具的三個問題（v7.1 D231、D233）、Match 連線品質窗口失敗時沒有寫出原因（v7.1 未結事項）、睡眠後重連（N-sleep-reconnect）、I-3 與 outage-resync P2 的兩個已知例外。
  - `product-sfx` 前：音效資產的來源（v7 D38）。
  - `hit-rules` 前：部位倍率。`ads` 前：ADS 是否影響命中。
- **發現（不在本產品的範圍，只報告）**：Engine 計畫 [輸入與呈現 HANDOFF](../../../architecture/plans/input-and-present/HANDOFF.md) 開頭寫「IP-3 進行中（ip3b）」，但 IP-3／IP-4 已隨 PR #85 在 2026-10-23 合併（merge commit `054b08f`，見 [v7.1 交接](../v7.1/HANDOFF.md) 2026-10-22～23 的規劃紀錄）。開頭已過時，交給該計畫或圖書管理員更新。

## 閱讀入口

- 計畫：[README](README.md)
- 前一版：[v7.1 交接](../v7.1/HANDOFF.md)、[v7.1 穩定基線](../v7.1/STABLE_BASELINE.md)
- 規則：AGENTS.md、[變速箱規則](../../../../agents/effort-gearbox.md)

## 決策紀錄

| 編號 | 決定 | 使用者的原話 |
|---|---|---|
| D243 | （2026-10-29）v8 的核心是補完遊戲功能與工具。塔科夫方向不在 v8：只收集玩法需求，協議升級之後另開 | 「v8 留在 pv7、打磨現有的 4 人對戰；塔科夫只寫需求，協議升級之後另開。」之後：「v8的核心是完善整個遊戲流程，還有工具，連audio都沒有，遊戲連蹲下還沒做，談什麼「塔科夫」？」 |
| D244 | （2026-10-29）順序：① 基礎功能（audio、蹲下、部位 hitbox）→ ② 體驗優化（ADS、衝刺）→ ③ 手感時序（v7.1 交過來的）→ ④ 工具（UI editor、Voxel Map Editor、Assets Editor） | 「優先基礎功能（audio，蹲下，部位hitbox ），在來遊戲體驗優化（ADS、衝刺），再來手感時序（v7.1 交過來）、最後工具（UI editor，Voxel Map Editor、Assets Editor）」 |
| D245 | （2026-10-29）對局規則、出生分散與保護、暫停選單、畫面設定、預熱幀、角色外觀排在工具之後（⑤）。①② 完成才像基礎 FPS，③ 是優化，④ 是 Engine 的穩定 | 「這些排在工具後面。」「第一第二完畢了，才比較像一個基礎fps，第三，是優化，第四才是Engine 穩定。」 |
| D246 | （2026-10-29）pv8 照建議：蹲下、衝刺、ADS 的欄位在 `pv8-design` 一次設計，隨蹲下的 PR 合併，衝刺與 ADS 先填預設值。對局規則不進 pv8，到 ⑤ 再升協議。計畫的檔位旁加上引擎（模型），照變速箱規則的「エンジン（モデル）」節 | 「照建議開，記得檔位上加個agent模型（opus啥）。變速箱規則升級了。」 |

## 規劃紀錄（記錄器）

- 2026-10-29：`v8-plan-sync` 開始（檔位 medium，引擎 Opus 5.5，估 2 h）。分支 `claude/pvp-v8-plan-sync`（從 origin/master `62fd7ebf`）。規劃前在主會話盤點 origin/master 的程式碼：`MovementCommand` 只有前後、左右、視角、跳；`CombatRules` 只有一把武器的參數；`HitPart` 已有 Body／Head；沒有計分、對局狀態、音效；Esc 呼叫 `connection.Leave()`；工具只有 `ui_editor` 與綁在已停用 `object_fps` 的 `object_fps_preview`。
- 2026-10-29：`v8-plan-sync` 完成。README 改寫為分批計畫（五組、各批檔位與引擎、估時、PR 線、不在 v8 的項目）；2026-10-22 版的候選與規劃輸入保留在 README 後半。沒有另建 INVENTORY：候選的去處寫在 README 的樹與「不在 v8 的項目」。dev_log：`docs/dev_logs/2026_10_29_pvp_v8_plan_sync.zh-Hant.md`。

## 未結事項

- 無。
