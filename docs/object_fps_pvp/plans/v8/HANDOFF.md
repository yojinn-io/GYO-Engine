# PvP v8 交接

更新：2026-10-30。Owner：`object_fps_pvp`。**狀態：`pv8-contract-round2`（`044584fd`）與 `pv8-obs-bytes`（`a7455eb9`）完成；等你同意 obs-bytes 的 Architecture Delta 並核准 protocol-v8 定稿，之後接 pv8-line 的 A 層。**
本文件是 v8 的記錄器：每批開始、里程碑、停止時，和工作在同一個變更中更新。人看的計畫在 [README](README.md)。

## 代號對照

### 代號規則（`v8-docs-id-scheme`）

批次與決策都用「`<版本>-<領域>-<主題>`」，看代號就知道是什麼事，和 git commit message 的 scope 類似。只用 ASCII 小寫與連字號。全 repo 的規則與舊號的讀法寫在 [docs/architecture.md](../../../architecture.md) 的「Plan records and identifiers」。

| 前綴 | 意思 | 例子 |
|---|---|---|
| `v8-product-` | 產品（`object_fps_pvp`）的任務或決定 | `v8-product-crouch` |
| `v8-engine-` | Engine 的任務或決定 | `v8-engine-audio` |
| `v8-tool-` | 工具（editor 等）的任務或決定 | `v8-tool-ui-editor-widgets` |
| `v8-docs-` | 文件的任務或決定 | `v8-docs-plan-sync` |
| `pv8-<主題>-` | 協議 pv8 的任務或決定 | `pv8-design`、`pv8-action-…`、`pv8-item-…` |

- 舊的流水號 D243～D254 在 2026-10-30 換成代號。v7.1 HANDOFF 也有 D243～D245（worker-wake-phase），和這裡無關。
- dev_log 與 commit 訊息是歷史紀錄，裡面的 D243～D254 不改寫，照下表讀。
- pv8-design 文件裡的 D-1～D-8 是那份設計的待決事項編號，照原樣保留。

| 舊號 | 代號 | 舊號 | 代號 |
|---|---|---|---|
| D243 | `v8-product-scope` | D249 | `pv8-design-air-calibration-draft` |
| D244 | `v8-product-order` | D250 | `pv8-design-air-calibration` |
| D245 | `v8-product-flow-after-tools` | D251 | `v8-product-ads-cancel` |
| D246 | `pv8-scope-crouch-sprint` | D252 | `v8-product-crouch-pose` |
| D247 | `pv8-design-ultracode` | D253 | `v8-product-crouch-jump-blocked` |
| D248 | `pv8-design-first-picks` | D254 | `pv8-design-remaining-picks` |

批次的舊名：`v8-plan-sync`→`v8-docs-plan-sync`；`au1-plan`／`au1`→`v8-engine-audio-plan`／`v8-engine-audio`；`crouch`→`v8-product-crouch`；`sprint`→`v8-product-sprint`；`b6-plan`／`b6`→`v8-product-lead-withdraw-plan`／`v8-product-lead-withdraw`（撤回多加的領先，v7.1 INVENTORY 的 B6）；`a4`→`v8-product-input-delay`（延遲位移，A4）；`proto-rename-v8`→`pv8-rename`；其他加上前綴。

### 其他代號

| 代號 | 意思 | 出處 |
|---|---|---|
| ①～⑤ | 五組：基礎功能、體驗優化、手感時序、工具、流程項目 | README「樹狀總覽」 |
| audio-line、pv8-line、hitbox-line | PR 線 | README「PR 線」 |
| pvN／vN | 網路協議版本／遊戲版本，分開編號（`pv8-design-remaining-picks`） | v6 交接 D20 |

## 現況（每次停下來時更新）

- **進行中**：pv8-line 的 A 層（`pv8-final-approve-round2`）。`pv8-rename` 完成；`pv8-seq-ceiling` 完成（既有測試的判定變更已核准，`pv8-seq-ceiling-wrap-test-approve`）。下一批是 `pv8-seq-narrow`：[事前宣告](pv8-seq-narrow-declaration.md) 草稿已寫好，等你核准（含 `pv8-seq-narrow-test-changes`、`pv8-seq-narrow-merged-streams` 兩項）。
- **完成**：`v8-docs-plan-sync`；`pv8-design` 第 1、2 輪；`v8-product-crouch` 的不碰 wire 部分（`fd950ea3`、`347fa9ba`）；`pv8-contract-round2`（`044584fd`）；`pv8-rename`（版本值仍是 7）；`pv8-seq-ceiling`；`pv8-obs-bytes`（`a7455eb9`，Delta 已同意：`pv8-obs-bytes-delta-approve`）；[協議壓力調研](protocol-pressure-survey.md)；[多通道評估](multi-channel-evaluation.md)。protocol-v8 已定稿。
- **最優先**：`audio-line`（`v8-engine-audio-first`）在另一個會話 `v8-audio-line` 進行，從 `v8-engine-audio-plan` 開始（自己的 worktree，分支切自 `claude/pvp-v8-pv8-line`）。它的紀錄放在 Engine 的計畫夾，v8 的文件由本會話同步。
- **並行**：
  - audio-line（會話 `v8-audio-line`）：`v8-engine-audio-plan` 完成（`6ad0d8de`，分支在該會話的 worktree，未推送），6 項決定已在該會話核准；`v8-engine-audio-core` 等你指定。
  - 工具線（會話 `v8-tool-line`）：`v8-tool-plan` 完成（`54dac0c3`，計畫在 `docs/tools/plans/v8/`，未推送）；它的 HANDOFF 列了 18 項等你決定。
- **等你決定**：
  - 不屬於玩法的 4 項放在 v8，還是另開維護線：驗收工具的三個問題（v7.1 D231、D233）、Match 連線品質窗口失敗時沒有寫出原因（v7.1 未結事項）、睡眠後重連（N-sleep-reconnect）、I-3 與 outage-resync P2 的兩個已知例外。
  - `v8-product-sfx` 前：音效資產的來源（v7 D38）。
  - `v8-product-hit-rules` 前：部位倍率。
- **發現（不在本產品的範圍，只報告）**：Engine 計畫 [輸入與呈現 HANDOFF](../../../architecture/plans/input-and-present/HANDOFF.md) 開頭寫「IP-3 進行中（ip3b）」，但 IP-3／IP-4 已隨 PR #85 在 2026-10-23 合併（merge commit `054b08f`，見 [v7.1 交接](../v7.1/HANDOFF.md) 2026-10-22～23 的規劃紀錄）。開頭已過時，交給該計畫或圖書管理員更新。

## 閱讀入口

- 計畫：[README](README.md)
- 前一版：[v7.1 交接](../v7.1/HANDOFF.md)、[v7.1 穩定基線](../v7.1/STABLE_BASELINE.md)
- 規則：AGENTS.md、[變速箱規則](../../../../agents/effort-gearbox.md)

## 決策紀錄

| 代號 | 決定 | 使用者的原話 |
|---|---|---|
| `v8-product-scope` | （2026-10-29）v8 的核心是補完遊戲功能與工具。塔科夫方向不在 v8：只收集玩法需求，協議升級之後另開 | 「v8 留在 pv7、打磨現有的 4 人對戰；塔科夫只寫需求，協議升級之後另開。」之後：「v8的核心是完善整個遊戲流程，還有工具，連audio都沒有，遊戲連蹲下還沒做，談什麼「塔科夫」？」 |
| `v8-product-order` | （2026-10-29）順序：① 基礎功能（audio、蹲下、部位 hitbox）→ ② 體驗優化（ADS、衝刺）→ ③ 手感時序（v7.1 交過來的）→ ④ 工具（UI editor、Voxel Map Editor、Assets Editor） | 「優先基礎功能（audio，蹲下，部位hitbox ），在來遊戲體驗優化（ADS、衝刺），再來手感時序（v7.1 交過來）、最後工具（UI editor，Voxel Map Editor、Assets Editor）」 |
| `v8-product-flow-after-tools` | （2026-10-29）對局規則、出生分散與保護、暫停選單、畫面設定、預熱幀、角色外觀排在工具之後（⑤）。①② 完成才像基礎 FPS，③ 是優化，④ 是 Engine 的穩定 | 「這些排在工具後面。」「第一第二完畢了，才比較像一個基礎fps，第三，是優化，第四才是Engine 穩定。」 |
| `pv8-scope-crouch-sprint` | （2026-10-29）pv8 照建議：蹲下、衝刺、ADS 的欄位在 `pv8-design` 一次設計，隨蹲下的 PR 合併，衝刺與 ADS 先填預設值。對局規則不進 pv8，到 ⑤ 再升協議。計畫的檔位旁加上引擎（模型），照變速箱規則的「エンジン（モデル）」節 | 「照建議開，記得檔位上加個agent模型（opus啥）。變速箱規則升級了。」 |
| `pv8-design-ultracode` | （2026-10-29）v8 開始：第一批 `pv8-design`，照編制跑 ultracode：2 位唯讀偵察（Sonnet 5.5，medium）、3 個獨立方案（Opus 5.5）、1 位審查（Opus 5.5）、1 次對抗式檢查（Sonnet 5.5），共 7 個 agent | 「可以開始了v8進程」；選「pv8-design (Recommended)」「照編制跑 (Recommended)」 |
| `pv8-design-first-picks` | （2026-10-29）`pv8-design` 的 D-1、D-3、D-4 照建議：D-1 (a) `aiming` 放 `CombatState`=15，ADS 的移速只看 `aim_held`＋PlayerState，換彈不影響移速；D-3 (a) 空中可以蹲、站起走同一個檢查，蹲著可以跳、跳高不變；D-4 (a) 蹲下參數與倍率放 Arena 內容 v2（② 調倍率會改 arena_digest，已安裝的內容要重新安裝）。D-2、D-5～D-8 還沒決定 | 「嗯這3個照建議就可以了。」（指 D-1、D-3、D-4） |
| `pv8-design-air-calibration-draft` | （2026-10-29）取代 pv8-design-first-picks 的 D-3、D-4（D-1 維持 (a)）。D-3：起跳後不能蹲，蹲著不能跳。D-4：做參數校準，進地圖時確認。主會話的解讀（待使用者確認）：D-3＝姿態只在 grounded 時切換，空中維持離地時的姿態；蹲著時 `jump_requested` 不生效（不會先站起再跳）。D-4＝沿用 `jump_height`／`gravity` 的先例：蹲下參數與倍率由 Match 經 Ready→Welcome 下發，Client 進地圖時檢查範圍並採用，同一 session 內改變就明確報錯；來源放 Match 端的移動規則資料，不進 arena_digest，調整時 Client 不必重新安裝內容。Welcome／Ready 因此新增欄位 | 「這樣把D-1照建議，D-3 起跳不能蹲，蹲下不能跳。D-4，可以做一個參數校準，在進地圖是確認。」 |
| `pv8-design-air-calibration` | （2026-10-29）確認 pv8-design-air-calibration-draft 的解讀，D-1 (a)、D-3、D-4 定案。D-3：姿態只在著地時切換，空中（含走下平台）維持離地時的姿態；蹲著按跳無效。D-4：蹲下參數與倍率由 Match 在進地圖時經 Ready→Welcome 下發校準，不進 arena_digest（不做 Arena v2）。pv8-design 改寫為第 2 版 | 「D-3、D-4 照你的解讀：姿態只在著地時切換，空中維持離地時的姿態，蹲著按跳無效；蹲下參數與倍率由 Match 在進地圖時下發校準，不進 arena_digest。改寫 pv8-design。」 |
| `v8-product-ads-cancel` | （2026-10-29）取消 ADS（`ads` 批、`aim_held`、`aiming`、`aim_speed_scale` 刪除；② 只剩 `v8-product-sprint`；pv8-design 的 D-1 不再適用）。蹲跳規則取代 pv8-design-air-calibration 的 D-3：蹲下時移速變慢、體型變矮；空中可以蹲（站起走同一個頭頂檢查）；蹲著按跳直接轉為跳躍（站起並跳）。主會話的補充：頭上被擋站不起來時不跳、維持蹲姿；空中蹲下時腳不動、頭往下縮，不會跳得更高（要腳往上收另外決定）。pv8-design 改寫為第 3 版 | 「OK，我想了一下，取消ADS。蹲跳動作【蹲：蹲下時移速變慢、體型變矮。空中蹲：可以。蹲著跳：直接轉為跳躍】」 |
| `v8-product-crouch-pose` | （2026-10-29）補充 v8-product-ads-cancel：「體型變矮」就是蹲姿，不改變原有模型。呈現用現有角色的蹲姿動畫（`Crouch_Idle_Loop`、`Crouch_Fwd_Loop`）；模型縮放照舊用站立高度，蹲姿高度只用在碰撞、受擊膠囊與眼高 | 「注意，這個就是蹲姿，不用改變原有模型。」（回應「身體變矮」） |
| `v8-product-crouch-jump-blocked` | （2026-10-29）確認 v8-product-ads-cancel 的兩點補充：蹲著按跳但頭上被擋站不起來時，不跳、維持蹲姿；空中蹲下時腳的位置不動（不往上收，蹲跳不會跳得更高） | 「對」（回應「頭上被擋時，蹲著按跳就不跳；空中蹲下時腳不往上收」） |
| `pv8-design-remaining-picks` | （2026-10-30）`pv8-design` 的其餘決定：D-2 照建議（先實測；超過 1200 B 時把 `sprinting` 移出 wire，不改 §8）；D-5 (a) 不加 `movement_rules_revision`（遊戲還沒上線）；D-6 (a) 衝刺沒有體力；D-7 遊戲版本與協議版本分開、可以不一致。主會話的解讀（待確認）：proto 檔名跟協議版本，這次 wire 改變使協議升為 pv8，和遊戲 v8 同號只是巧合，所以照 (a) 改名為 `client_v8`／`runtime_v8`；D-8 同意 pv8 的 Architecture Delta。另：使用者要求在最後給一份調研報告，主題是哪些遊戲行為會佔用協議（參考主流射擊遊戲、鬥陣特工風格的戰鬥系統、塔科夫的地圖／背包／裝備／簡單武器） | 「D-2照建議，D-5 沒關係，遊戲沒上線。D-6，照建議。D-7，現在遊戲版本和協議版本是分開的v8是遊戲版本，pv7是協議版本，可以不一致。D-8同意。最後我發現，現在壓力來到了協議層，你最後給我一份調研報告，是什麼遊戲行為會佔用這些，參考主流的射擊遊戲，還有一個就是「鬥陣特工」風格的戰鬥系統、「塔科夫」則是地圖、背包、裝備、簡單的武器（沒有改裝）。」 |
| `pv8-design-final-approve` | （2026-10-30）核准 protocol-v8 定稿；`pv8-design-remaining-picks` 的 D-7 解讀正確：檔名跟協議版本，這次升為 pv8。之後被 `pv8-scope-reopen` 重開 | 「核准 protocol-v8 定稿，D-7 的解讀正確（檔名跟協議版本，這次升為 pv8）。接著開始 crouch。」 |
| `pv8-scope-reopen` | （2026-10-30）重開 pv8，一起評估這些協議壓力點：流水號（協議的序號欄位）、多通道、action 分離、撿物品、背包、Boss 血量廣播、技能釋放、任務、房間規則。取代 `v8-product-scope` 的「塔科夫方向的協議升級不在 v8」與 `pv8-scope-crouch-sprint` 的「pv8 只放 ①② 的欄位」；protocol-v8 退回草稿。依 AGENTS §2，先寫需求再定契約 | 「流水號、多通道、action分離、檢物品、背包、boss血量廣播、技能釋放、這些都是壓力點，請放到pv8裡面一起評估。任務、房間規則。一起評估」；「重開 pv8」 |
| `v8-docs-id-scheme` | （2026-10-30）批次與決策的代號改成「版本-領域-主題」：`v8-product-…`、`v8-docs-…`、`pv8-action-…`、`pv8-item-…` 等。舊流水號 D243～D254 照對照表換掉（解決和 v7.1 的撞號）。規則寫進 docs/architecture.md，舊的 dev_log 不改寫 | 「任務撞號的問題用版本號，v8-product-xxx：這個是產品任務，v8-docs-xxx：這個是文檔任務，其實就合git commit message 有些類似。pv8-action-xxx、pv8-item-xxx 同理，這是處理協議的。從編號我就能看出目前是什麼問題。（這樣比之前f8，d9 還要白話文翻譯才能看得懂）」；「這個號碼變更寫到架構文檔中。否則以前的dev_logs 會亂」 |
| `pv8-req-typical-values` | （2026-10-30）[pv8 需求問卷](pv8-requirements.md) 先用典型值，之後可以逐題改 | 「需求問卷先用典型值。」 |
| `v8-product-crouch-nowire-first` | （2026-10-30）`v8-product-crouch` 先做不碰 wire 的部分（C++ 的姿態、碰撞高度、站起判定、替補、SeedLead、重生、呈現）；proto、版本值、Ready／Welcome 的參數等新的 pv8 定稿再接 | 「crouch 不碰 wire 的部分先做」 |
| `pv8-design-round2-ultracode` | （2026-10-30）新範圍的 pv8 設計照編制跑 ultracode，啟動前給使用者看 prompt | 「新的 pv8 設計照編制跑 ultracode，啟動前給我看 prompt。」 |
| `v8-engine-audio-first` | （2026-10-30）audio 最優先：`audio-line`（`v8-engine-audio-plan` → `v8-engine-audio` → `v8-product-sfx`）排在 crouch 與 pv8 第 2 輪之前。在哪裡做（本會話先暫停 crouch，或另開會話並行）等使用者決定 | 「對了，audio排在哪裡？這個是最優先的。」 |
| `v8-engine-audio-session` | （2026-10-30）audio-line 另開一個會話做（自己的 worktree），從 `v8-engine-audio-plan` 開始；本會話繼續 crouch 與 pv8 第 2 輪的 prompt | 「audio-line 另開一個會話做（自己的 worktree），從 v8-engine-audio-plan 開始；本會話繼續 crouch 與 pv8 第 2 輪的 prompt。」 |
| `v8-tool-line-session` | （2026-10-30）另開一條並行的工具線（④），先做規劃 `v8-tool-plan` | 「在給我一個並行的工具線。」 |
| `v8-product-crouch-declaration-approve` | （2026-10-30）核准 [v8-product-crouch 的事前宣告](v8-product-crouch-declaration.md) J1～J4，接受「先實作、用突變證明 Red」的偏差；跑 J1 | 「核准 v8-product-crouch 的事前宣告（接受用突變證明 Red），跑 J1。」 |
| `pv8-round2-picks` | （2026-10-30）[pv8-round2](pv8-round2.md) 的 6 項全部照建議：`pv8-version-split` (a) A 層現在合併、B 層在 ⑤ 時升 pv9；`pv8-req-v8-scope` (a) v8 只做 ⑤ 的對局規則，其他只留形狀；`pv8-seq-cpp-types` (b) C++ 維持 u64，用 `WireCounterMax` 加編碼 Assert；`pv8-reserve-numbers` (a) 預留 B／C 的號碼；`pv8-obs-bytes-now` (a) 現在做觀測；`pv8-hello-doc-mismatch` (a) 以程式的 250 ms 為準 | 「pv8 第 2 輪的 6 項全部照建議。」 |
| `pv8-next-contract-and-obs` | （2026-10-30）先做 `pv8-contract-round2`（把第 2 輪寫進 protocol-v8）與 `pv8-obs-bytes`（實際位元組與超車比例的觀測），之後再接 pv8-line 的 A 層 | 「先做 pv8-contract-round2（把第 2 輪寫進 protocol-v8）和 pv8-obs-bytes（實際位元組與超車比例的觀測），之後再接 pv8-line 的 A 層。」 |
| `pv8-obs-bytes-delta-approve` | （2026-10-30）同意 `pv8-obs-bytes` 的 Architecture Delta：Gateway 新增診斷狀態 `sizeStats`，歸在 `classDiagnostic`（只寫進日誌、不參與判定） | 「都同意」（對「同意 obs-bytes 的 Delta 並核准 protocol-v8 定稿嗎？」） |
| `pv8-final-approve-round2` | （2026-10-30）核准 [protocol-v8](../../protocol-v8.zh-Hant.md) 定稿：第 1 輪（蹲下、衝刺、參數校準）＋第 2 輪 A 層（縮窄、刪 `match_id`、預留號碼）；附錄 B／C 是設計，不是 pv8 條文。接著開始 pv8-line 的 A 層（`pv8-rename` 起） | 「都同意」 |
| `pv8-seq-ceiling-declaration-approve` | （2026-10-30）核准 [pv8-seq-ceiling 的事前宣告](pv8-seq-ceiling-declaration.md) K1～K3；`pv8-seq-ceiling-test-seam` (a) 判定抽成純函式、測函式邊界，各處用突變確認呼叫；`pv8-seq-ceiling-table-fix` (a) 在 protocol-v8 §2a 的表補上 `ContiguousPending` 與 `Tick` 兩處 | 「照建議核准」 |
| `pv8-seq-ceiling-wrap-test-a` | （2026-10-30）既有測試 `PvP sequence and epoch boundaries never wrap into an old command namespace` 照 (a) 處理：邊界改成 `WireCounterMax`，加「超過上限即用盡」的情境；先做突變與另一位審查者的核對，再把 §14.4 的 1～5 交給使用者核准 | 「照 (a) 改測試」 |
| `pv8-seq-ceiling-wrap-test-approve` | （2026-10-30）核准既有測試 never-wrap 的判定變更（§14.4 的 1～5：放寬面是不再要求 u32 上限到 u64 上限之間照常前進；收緊面是 u32 上限處停、超過上限即用盡；審查者 5 點成立） | 「核准並 commit」 |

## 規劃紀錄（記錄器）

- 2026-10-29：`v8-docs-plan-sync` 開始（檔位 medium，引擎 Opus 5.5，估 2 h）。分支 `claude/pvp-v8-plan-sync`（從 origin/master `62fd7ebf`）。規劃前在主會話盤點 origin/master 的程式碼：`MovementCommand` 只有前後、左右、視角、跳；`CombatRules` 只有一把武器的參數；`HitPart` 已有 Body／Head；沒有計分、對局狀態、音效；Esc 呼叫 `connection.Leave()`；工具只有 `ui_editor` 與綁在已停用 `object_fps` 的 `object_fps_preview`。
- 2026-10-29：`v8-docs-plan-sync` 完成。README 改寫為分批計畫（五組、各批檔位與引擎、估時、PR 線、不在 v8 的項目）；2026-10-22 版的候選與規劃輸入保留在 README 後半。沒有另建 INVENTORY：候選的去處寫在 README 的樹與「不在 v8 的項目」。dev_log：`docs/dev_logs/2026_10_29_pvp_v8_plan_sync.zh-Hant.md`。
- 2026-10-29：`pv8-design` 開始（pv8-design-ultracode）。主會話先做少量偵察：`client_v7.proto` 的 `MovementCommand`（6 欄）、`PlayerState`（至 19）、`WorldSnapshot`、`CombatState`；`protocol-v7.zh-Hant.md` §0 的原則（一次 wire 變更、每個欄位寫出它支撐的功能）。workflow 的 agent 只讀不寫，產出由主會話整理成 `pv8-design.md` 與 `protocol-v8.zh-Hant.md` 草案，停在核准關卡。
- 2026-10-29：`pv8-design` 的 workflow（run `wf_f6c34055-22e`）完成，7／7 個 agent 回傳，約 16 分。評分：最小加法 26、風險優先 24、功能優先 19；以最小加法為基底合成。對抗式檢查：must 4、should 8、note 4。主會話核對 must 3：`ResetMovementEpoch` 清空 `lastActualCommand`（`PvpMatch.cpp:433`），對抗方的推論不成立，但由此找到反方向的缺口（epoch 輪替後替補會把蹲著的玩家拉起來），補進草案。agent 的原始輸出留在會話的工作檔，不放進 repo（同 IP-3／IP-4 的做法）。所有大小都是手算，沒有建置或執行測試。
- 2026-10-30：型別最大實測。暫時的 Go 測試（`gateway/adapter/zz_size_probe_test.go`，量完刪除）以 pv7 生成碼量測，pv8 的新欄位以 protowire 附加：pv7 契約最大 1073 B（和既有測試一致）、型別最大 1151 B；pv8 契約最大 1101 B、型別最大 1175 B；PlayerInput 12 命令 477→525 B。protocol-v7 §8 的 1155 B 原定義無法核對（沒有測試）。Gateway 以列舉常數重組 snapshot（`adapter.go:88-106`），wire 上不會有負的 enum。寫 `protocol-v8.zh-Hant.md` 定稿候選。協議壓力調研交給 1 位背景子 agent（general-purpose，引擎與 effort 繼承，只讀、只回傳文字）。
- 2026-10-30：協議壓力調研完成，寫成 [protocol-pressure-survey.md](protocol-pressure-survey.md)。子 agent 約 4.5 分、32 次工具呼叫。主會話換上實測數字，補第 5 節（和現有元件的對照）與第 6 節（會影響的決定）；其他遊戲的敘述沒有逐一打開來源核對，已在報告中標明。分支沒有推送。
- 2026-10-30：使用者核准 protocol-v8 定稿、D-7 解讀正確，指示開始 crouch。開工前把 origin/master（PR #102～#105）合進本分支（`b648df1a`，無衝突），發現 v7.1 HANDOFF 也用了 v8-product-scope、v8-product-order、v8-product-flow-after-tools，撞號，停下問使用者。使用者另要求評估多通道：實測接近實際值的 4 人 snapshot 約 406 B（1 小時）／439 B（24 小時），帳面的 1101／1175 B 來自 uint64 上限；寫成 [multi-channel-evaluation.md](multi-channel-evaluation.md)。暫時的測試量完刪除。crouch 尚未開始。
- 2026-10-30：決策與批次的代號改成「版本-領域-主題」（`v8-docs-id-scheme`），v8 的計畫文件全部換掉，對照表在「代號對照」；dev_log 照規則不改寫。pv8 重開（`pv8-scope-reopen`）。
- 2026-10-30：`v8-product-crouch` 不碰 wire 的部分實作。`StanceRules` 不放進 `Arena`（`ArenaContentDigest` 綁住全部成員），當參數傳給 `StepMovement`／`QueryShot`，舊簽名保留並用預設值。建置成功、`object_fps_pvp.cpu` 通過、新測試 7 個（61 個斷言）通過。突變 9 個：8 個 killed；`reset-substitution-stands` 存活，查明 Match 在新 epoch 的第 1 個命令前不推進該玩家（`PvpMatch.cpp:473-476`），先前「epoch 輪替後替補會把蹲著的玩家拉起來」的判斷錯誤，該規則刪除並更正 pv8-design、protocol-v8 §5 與事前宣告。偏差：事前宣告寫先寫失敗的測試，實際先實作，Red 改用突變證明。commit 1：`fd950ea3`。遠端蹲姿動畫延到 J1 之後。
- 2026-10-30：pv8 第 2 輪 ultracode 完成（run `wf_0b9d1bf5-4aa`）。結論與分批寫成 [pv8-round2.md](pv8-round2.md)；對抗式檢查 must 4 個，主會話把第 1 個（u32 截斷）降為 should，理由寫在文件。

- 2026-10-30：J1（`v8-product-crouch-declaration-approve` 之後）。base＝`de1c679b`（`../GYO-Engine-v8-base`），branch＝`fd950ea3`（之後的 commit 只改文件），同一台 Intel Mac，`compare_authority_trees.py` 各跑 1 次：scale 1 與 scale 3 都是「36 scenarios, 0 differing」。宣告寫「約 35 個」，實際 36 個，判定不受影響。J1 通過。

- 2026-10-30：`v8-product-crouch` commit 2（`347fa9ba`）。權威 digest v2：`crouched`、`sprinting` 只在 true 時符號化，站姿情境的位元組與既有 golden 行不變（J2 的做法；格式定義的變更由主會話決定，已向使用者說明）。新增 golden 情境 `move-crouch/contract`、`move-crouch-slab/slab`，以 dump 確認行為（蹲跳、slab 下放開與按跳都無效、走出後站起）。J2：`authority_digest` 第 1 次失敗，原因是只重建了最佳化版，`-O0` 執行檔是舊的（環境，非實作）；全部重建後通過，失敗記錄在此（§14.1）。J4：`test` preset 93 個測試中 `object_fps_pvp.mutation_tool` 失敗，原因是突變 `v71a1-rules-ignored` 的 find 對準舊的 `PvpMatch` 建構子；更新 find／replace（語意相同），單獨跑確認 killed，之後通過。其餘 92 個通過。D184 的相關突變（find 在改動檔案中的 54 個）：54／54 killed。dev_log：`docs/dev_logs/2026_10_30_pvp_v8_crouch_nowire.zh-Hant.md`、`docs/dev_logs/2026_10_30_pvp_v8_pv8_design.zh-Hant.md`。

- 2026-10-30：`pv8-contract-round2` 完成（`044584fd`），對抗式檢查 1 次（Sonnet 5.5）的 must 2 個、should 若干已納入；`MatchStatus` 的號碼衝突由主會話更正。`pv8-obs-bytes` 完成（`a7455eb9`）：Gateway 的資料包大小分布、Client 依種類拆開的 stale 計數；`test` preset 93／93 通過；相關突變 3／3 killed。dev_log 兩份。實際數字待下一次 L2／LAN 場次。
- 2026-10-30：`pv8-rename` 完成。重產只差名稱；CTest 99／99；兩樹 digest（base `33127ad6`）scale 1、3 都是 38 個情境 0 差分；相關突變 0 個。計畫的引擎是 Sonnet 5.5，實際由主會話直接做（機械式替換）。
- 2026-10-30：`pv8-seq-ceiling` 完成。K1 38 個情境 × scale 1、3 0 差分；K3 第 1 次因既有測試釘著 uint64 邊界而停止（宣告前提錯誤），照 (a) 改測試並經 §14.4 核准；突變 16／16；CTest 100／100。

## 未結事項

- 無。
