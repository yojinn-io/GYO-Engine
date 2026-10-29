# PvP v8 交接

更新：2026-10-30。Owner：`object_fps_pvp`。**狀態：`pv8-design` 完成定稿候選 [protocol-v8](../../protocol-v8.zh-Hant.md)，等你核准；[協議壓力調研](protocol-pressure-survey.md) 完成。**
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
| sprint | 衝刺（ads 已取消，D251） | 同上 |
| analyzer-105、phase-golden、correct-assert、b6-plan、b6、a4 | v7.1 交過來的手感時序（INVENTORY 的 N-analyzer-literal-105、B10、N-correct-invariant-assert、B6、A4） | [v7.1 INVENTORY](../v7.1/INVENTORY.md) |
| ui-editor-widgets、voxel-map-editor、assets-editor | 工具 | README |
| match-rules、spawn-layout、pause-menu、display-settings、warmup-hidden、player-look | 流程項目 | README |
| pvN／vN | 網路協議版本／遊戲版本 | v6 交接 D20 |

## 現況（每次停下來時更新）

- **進行中**：`pv8-design`（D247）。D-1～D-8 處理完（D250～D254）；型別最大已實測（4 人 snapshot 1175 B，餘裕 25 B）；定稿候選 [protocol-v8](../../protocol-v8.zh-Hant.md) 寫好，等你核准。分支 `claude/pvp-v8-pv8-line`，本計畫的文件隨 pv8-line 的 PR 合併。
- **完成**：`v8-plan-sync`。
- **之後**：`au1-plan`（audio-line，可並行）。
- **等你決定**：
  - **決策編號撞號**：本文件的 D243～D254 和 v7.1 HANDOFF 的 D243～D245（worker-wake-phase，PR #105，2026-10-30 合進 master）同號、意思不同。處理方式（改前綴 `V8-D`、跳號、或只加註）等你決定；決定前，新的決定先不編號。
  - [多通道評估](multi-channel-evaluation.md) 的 M-1～M-3 → 被下一項取代。
  - **pv8 範圍擴大的評估**（2026-10-30 使用者：「流水號、多通道、action分離、檢物品、背包、boss血量廣播、技能釋放、這些都是壓力點，請放到pv8裡面一起評估。任務、房間規則。一起評估」）。這會重開剛定稿的 protocol-v8，並和 D243（塔科夫方向不在 v8）、D246（pv8 只放 ①② 的欄位）衝突，要先確認範圍與 crouch 怎麼處理，見主會話的提案。
  - **已核准、待編號**：protocol-v8 定稿；D254 的 D-7 解讀正確（檔名跟協議版本，這次升為 pv8）。使用者：「核准 protocol-v8 定稿，D-7 的解讀正確（檔名跟協議版本，這次升為 pv8）。接著開始 crouch。」
  - 讀 [協議壓力調研](protocol-pressure-survey.md)（只是規劃輸入，沒有要你決定的事；第 6 節列了它會影響的決定）。
  - 不屬於玩法的 4 項放在 v8，還是另開維護線：驗收工具的三個問題（v7.1 D231、D233）、Match 連線品質窗口失敗時沒有寫出原因（v7.1 未結事項）、睡眠後重連（N-sleep-reconnect）、I-3 與 outage-resync P2 的兩個已知例外。
  - `product-sfx` 前：音效資產的來源（v7 D38）。
  - `hit-rules` 前：部位倍率。
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
| D247 | （2026-10-29）v8 開始：第一批 `pv8-design`，照編制跑 ultracode：2 位唯讀偵察（Sonnet 5.5，medium）、3 個獨立方案（Opus 5.5）、1 位審查（Opus 5.5）、1 次對抗式檢查（Sonnet 5.5），共 7 個 agent | 「可以開始了v8進程」；選「pv8-design (Recommended)」「照編制跑 (Recommended)」 |
| D248 | （2026-10-29）`pv8-design` 的 D-1、D-3、D-4 照建議：D-1 (a) `aiming` 放 `CombatState`=15，ADS 的移速只看 `aim_held`＋PlayerState，換彈不影響移速；D-3 (a) 空中可以蹲、站起走同一個檢查，蹲著可以跳、跳高不變；D-4 (a) 蹲下參數與倍率放 Arena 內容 v2（② 調倍率會改 arena_digest，已安裝的內容要重新安裝）。D-2、D-5～D-8 還沒決定 | 「嗯這3個照建議就可以了。」（指 D-1、D-3、D-4） |
| D249 | （2026-10-29）取代 D248 的 D-3、D-4（D-1 維持 (a)）。D-3：起跳後不能蹲，蹲著不能跳。D-4：做參數校準，進地圖時確認。主會話的解讀（待使用者確認）：D-3＝姿態只在 grounded 時切換，空中維持離地時的姿態；蹲著時 `jump_requested` 不生效（不會先站起再跳）。D-4＝沿用 `jump_height`／`gravity` 的先例：蹲下參數與倍率由 Match 經 Ready→Welcome 下發，Client 進地圖時檢查範圍並採用，同一 session 內改變就明確報錯；來源放 Match 端的移動規則資料，不進 arena_digest，調整時 Client 不必重新安裝內容。Welcome／Ready 因此新增欄位 | 「這樣把D-1照建議，D-3 起跳不能蹲，蹲下不能跳。D-4，可以做一個參數校準，在進地圖是確認。」 |
| D250 | （2026-10-29）確認 D249 的解讀，D-1 (a)、D-3、D-4 定案。D-3：姿態只在著地時切換，空中（含走下平台）維持離地時的姿態；蹲著按跳無效。D-4：蹲下參數與倍率由 Match 在進地圖時經 Ready→Welcome 下發校準，不進 arena_digest（不做 Arena v2）。pv8-design 改寫為第 2 版 | 「D-3、D-4 照你的解讀：姿態只在著地時切換，空中維持離地時的姿態，蹲著按跳無效；蹲下參數與倍率由 Match 在進地圖時下發校準，不進 arena_digest。改寫 pv8-design。」 |
| D251 | （2026-10-29）取消 ADS（`ads` 批、`aim_held`、`aiming`、`aim_speed_scale` 刪除；② 只剩 `sprint`；pv8-design 的 D-1 不再適用）。蹲跳規則取代 D250 的 D-3：蹲下時移速變慢、體型變矮；空中可以蹲（站起走同一個頭頂檢查）；蹲著按跳直接轉為跳躍（站起並跳）。主會話的補充：頭上被擋站不起來時不跳、維持蹲姿；空中蹲下時腳不動、頭往下縮，不會跳得更高（要腳往上收另外決定）。pv8-design 改寫為第 3 版 | 「OK，我想了一下，取消ADS。蹲跳動作【蹲：蹲下時移速變慢、體型變矮。空中蹲：可以。蹲著跳：直接轉為跳躍】」 |
| D252 | （2026-10-29）補充 D251：「體型變矮」就是蹲姿，不改變原有模型。呈現用現有角色的蹲姿動畫（`Crouch_Idle_Loop`、`Crouch_Fwd_Loop`）；模型縮放照舊用站立高度，蹲姿高度只用在碰撞、受擊膠囊與眼高 | 「注意，這個就是蹲姿，不用改變原有模型。」（回應「身體變矮」） |
| D253 | （2026-10-29）確認 D251 的兩點補充：蹲著按跳但頭上被擋站不起來時，不跳、維持蹲姿；空中蹲下時腳的位置不動（不往上收，蹲跳不會跳得更高） | 「對」（回應「頭上被擋時，蹲著按跳就不跳；空中蹲下時腳不往上收」） |
| D254 | （2026-10-30）`pv8-design` 的其餘決定：D-2 照建議（先實測；超過 1200 B 時把 `sprinting` 移出 wire，不改 §8）；D-5 (a) 不加 `movement_rules_revision`（遊戲還沒上線）；D-6 (a) 衝刺沒有體力；D-7 遊戲版本與協議版本分開、可以不一致。主會話的解讀（待確認）：proto 檔名跟協議版本，這次 wire 改變使協議升為 pv8，和遊戲 v8 同號只是巧合，所以照 (a) 改名為 `client_v8`／`runtime_v8`；D-8 同意 pv8 的 Architecture Delta。另：使用者要求在最後給一份調研報告，主題是哪些遊戲行為會佔用協議（參考主流射擊遊戲、鬥陣特工風格的戰鬥系統、塔科夫的地圖／背包／裝備／簡單武器） | 「D-2照建議，D-5 沒關係，遊戲沒上線。D-6，照建議。D-7，現在遊戲版本和協議版本是分開的v8是遊戲版本，pv7是協議版本，可以不一致。D-8同意。最後我發現，現在壓力來到了協議層，你最後給我一份調研報告，是什麼遊戲行為會佔用這些，參考主流的射擊遊戲，還有一個就是「鬥陣特工」風格的戰鬥系統、「塔科夫」則是地圖、背包、裝備、簡單的武器（沒有改裝）。」 |

## 規劃紀錄（記錄器）

- 2026-10-29：`v8-plan-sync` 開始（檔位 medium，引擎 Opus 5.5，估 2 h）。分支 `claude/pvp-v8-plan-sync`（從 origin/master `62fd7ebf`）。規劃前在主會話盤點 origin/master 的程式碼：`MovementCommand` 只有前後、左右、視角、跳；`CombatRules` 只有一把武器的參數；`HitPart` 已有 Body／Head；沒有計分、對局狀態、音效；Esc 呼叫 `connection.Leave()`；工具只有 `ui_editor` 與綁在已停用 `object_fps` 的 `object_fps_preview`。
- 2026-10-29：`v8-plan-sync` 完成。README 改寫為分批計畫（五組、各批檔位與引擎、估時、PR 線、不在 v8 的項目）；2026-10-22 版的候選與規劃輸入保留在 README 後半。沒有另建 INVENTORY：候選的去處寫在 README 的樹與「不在 v8 的項目」。dev_log：`docs/dev_logs/2026_10_29_pvp_v8_plan_sync.zh-Hant.md`。
- 2026-10-29：`pv8-design` 開始（D247）。主會話先做少量偵察：`client_v7.proto` 的 `MovementCommand`（6 欄）、`PlayerState`（至 19）、`WorldSnapshot`、`CombatState`；`protocol-v7.zh-Hant.md` §0 的原則（一次 wire 變更、每個欄位寫出它支撐的功能）。workflow 的 agent 只讀不寫，產出由主會話整理成 `pv8-design.md` 與 `protocol-v8.zh-Hant.md` 草案，停在核准關卡。
- 2026-10-29：`pv8-design` 的 workflow（run `wf_f6c34055-22e`）完成，7／7 個 agent 回傳，約 16 分。評分：最小加法 26、風險優先 24、功能優先 19；以最小加法為基底合成。對抗式檢查：must 4、should 8、note 4。主會話核對 must 3：`ResetMovementEpoch` 清空 `lastActualCommand`（`PvpMatch.cpp:433`），對抗方的推論不成立，但由此找到反方向的缺口（epoch 輪替後替補會把蹲著的玩家拉起來），補進草案。agent 的原始輸出留在會話的工作檔，不放進 repo（同 IP-3／IP-4 的做法）。所有大小都是手算，沒有建置或執行測試。
- 2026-10-30：型別最大實測。暫時的 Go 測試（`gateway/adapter/zz_size_probe_test.go`，量完刪除）以 pv7 生成碼量測，pv8 的新欄位以 protowire 附加：pv7 契約最大 1073 B（和既有測試一致）、型別最大 1151 B；pv8 契約最大 1101 B、型別最大 1175 B；PlayerInput 12 命令 477→525 B。protocol-v7 §8 的 1155 B 原定義無法核對（沒有測試）。Gateway 以列舉常數重組 snapshot（`adapter.go:88-106`），wire 上不會有負的 enum。寫 `protocol-v8.zh-Hant.md` 定稿候選。協議壓力調研交給 1 位背景子 agent（general-purpose，引擎與 effort 繼承，只讀、只回傳文字）。
- 2026-10-30：協議壓力調研完成，寫成 [protocol-pressure-survey.md](protocol-pressure-survey.md)。子 agent 約 4.5 分、32 次工具呼叫。主會話換上實測數字，補第 5 節（和現有元件的對照）與第 6 節（會影響的決定）；其他遊戲的敘述沒有逐一打開來源核對，已在報告中標明。分支沒有推送。
- 2026-10-30：使用者核准 protocol-v8 定稿、D-7 解讀正確，指示開始 crouch。開工前把 origin/master（PR #102～#105）合進本分支（`b648df1a`，無衝突），發現 v7.1 HANDOFF 也用了 D243～D245，撞號，停下問使用者。使用者另要求評估多通道：實測接近實際值的 4 人 snapshot 約 406 B（1 小時）／439 B（24 小時），帳面的 1101／1175 B 來自 uint64 上限；寫成 [multi-channel-evaluation.md](multi-channel-evaluation.md)。暫時的測試量完刪除。crouch 尚未開始。

## 未結事項

- 無。
