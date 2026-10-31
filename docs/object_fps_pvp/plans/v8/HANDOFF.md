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

- **進行中**：hitbox-line。[hitbox-plan](hitbox-plan.md) 第 2 版（以 Valorant 為基底：Match 跑動畫、動畫狀態由 Match 決定並上 snapshot、三部位綁骨頭）已核准（`v8-product-hitbox-plan-v2-approve`）。第 2 節轉來的參考資料已逐項查證並標上 ✓／△／✗ 與來源（`813b7615`）：「Valorant 動畫刻意簡單」改成它實際的兩招（每 4 幀算一次＋內插、買裝備階段不算）；「CS 送動畫週期給 Client」查不到出處，`v8-product-anim-state-on-wire` 的理由改成我們自己的量測。下一批 `v8-product-anim-bottlenecks` 已完成（[dev_log](../../../dev_logs/2026_10_31_pvp_v8_anim_bottlenecks.zh-Hant.md)）：linux-x64 上 B1 0.44 ms、B2 0.23 s／93 MB 都通過；B3 各平台可重現但平台間取樣不一致，golden 照 `v8-product-hitbox-golden-policy` 的規則。途中修了上一批 `hitbox_survey` 的暫時物件缺陷（Linux、Windows 的 L1 失敗）。`pv9-hitbox-declare`（範圍含 rewind-line，`pv9-scope-hitbox-and-rewind`）：[protocol-v9](../../protocol-v9.zh-Hant.md) 定稿（`pv9-final-approve`，[dev_log](../../../dev_logs/2026_10_31_pvp_v8_pv9_hitbox_declare.zh-Hant.md)）。`pv9-rename` 完成（[dev_log](../../../dev_logs/2026_10_31_pvp_v8_pv9_rename.zh-Hant.md)，兩樹比對 0 差分）。`v8-product-hit-rules-permille` 完成（[dev_log](../../../dev_logs/2026_10_31_pvp_v8_hit_rules_permille.zh-Hant.md)）：血量與傷害改成千分之一點、版本值 9、權威 digest version 3；兩樹的換算比對 scale 1、3 都是 0 差分，golden 37 行重產。`v8-product-hit-rules-leg-wire` 完成（[dev_log](../../../dev_logs/2026_10_31_pvp_v8_hit_rules_leg_wire.zh-Hant.md)）：wire 與兩端的檢查接受腿，Match 仍只產生 Body；兩樹比對 0 差分、突變 24／24、Sonnet 審查 1～5（4 部分成立，已記錄）。`v8-product-anim-authority` 進行中：[事前宣告](v8-product-anim-authority-declaration.md)已核准（`v8-product-anim-authority-declaration-approve`），分成 A1 domain／A2 match／A3 wire／A4 client。A1 開工前曾停下：搬過去的 CPU 載入器會讀材質貼圖（`CharacterPresentationDefinition.cpp:147-152`），Match 會依賴 SDL image；使用者改選 `v8-product-anim-rules-source-texture-free` (a-1)：Match 只載入模型與 clip，不讀材質，「clip 長度→一圈距離」只寫一次並加對齊測試。A1 完成（[dev_log](../../../dev_logs/2026_10_31_pvp_v8_anim_authority_domain.zh-Hant.md)）：E1 試行 1 不成立（舊 Client 的浮點邊界）→ 判定修正 `v8-product-anim-authority-e1-jump` 核准 → 試行 2、3 通過；xhigh 審查找到 C1～C3、S1、S2，皆先紅後綠修正；突變 17／17。`v8-product-anim-phase-definition` (a)：protocol-v9 §4.3 的相位文字已對齊實作。A2 完成（[dev_log](../../../dev_logs/2026_10_31_pvp_v8_anim_authority_match.zh-Hant.md)）：Match 每 Tick 推進並上 snapshot、寫入前 Assert、digest version 4；E3 兩樹比對 0 差分、golden 37 行重產、突變 11／11。A3 完成（[dev_log](../../../dev_logs/2026_10_31_pvp_v8_anim_authority_wire.zh-Hant.md)）：proto 加 `PlayerAnimation`、IpcHost 編碼、Gateway 與 Client 依 §4.5 檢查、大小實測 884／1022／842 B、突變 22／22。worker 曾偶發失敗一次、原因分不出來（記在 dev_log，L1 時注意）。A4 完成（[dev_log](../../../dev_logs/2026_10_31_pvp_v8_anim_authority_client.zh-Hant.md)）：Client 照 snapshot 播放；§14.4 表與 1～5 經 Sonnet 審查修正後核准（`v8-product-anim-authority-a4-approve`）；xhigh 找到的 F1（holding 凍住跳躍）先紅後綠修正；突變 16／16。**`v8-product-anim-authority` 四個子批全部完成。** 下一批 `v8-product-hitbox-data`：[事前宣告](v8-product-hitbox-data-declaration.md)寫好（D1 形狀／D2 共用姿勢／D3 animation_digest，估 12～16 h，比計畫的 6～10 h 大，原因寫在宣告），等使用者核准 3 項決定。未守住：PvpApplication 的取樣接線、Submit 的初始姿勢捷徑（只有 GUI 驗收會發現）。第 1 版 B-flat 與三輪 survey 的門檻不再適用；survey 工具保留（半徑、Client 與 Match 的比對）。
- **完成**：`v8-docs-plan-sync`；`pv8-design` 第 1、2 輪；`v8-product-crouch` 的不碰 wire 部分（`fd950ea3`、`347fa9ba`）；`pv8-contract-round2`（`044584fd`）；`pv8-rename`（版本值仍是 7）；`pv8-seq-ceiling`；`pv8-seq-narrow`；`pv8-obs-bytes`（`a7455eb9`，Delta 已同意：`pv8-obs-bytes-delta-approve`）；[協議壓力調研](protocol-pressure-survey.md)；[多通道評估](multi-channel-evaluation.md)。protocol-v8 已定稿。
- **最優先**：`audio-line`（`v8-engine-audio-first`）在另一個會話 `v8-audio-line` 進行，從 `v8-engine-audio-plan` 開始（自己的 worktree，分支切自 `claude/pvp-v8-pv8-line`）。它的紀錄放在 Engine 的計畫夾，v8 的文件由本會話同步。
- **並行**：
  - audio-line（會話 `v8-audio-line`）：`v8-engine-audio-plan` 完成（`6ad0d8de`，分支在該會話的 worktree，未推送），6 項決定已在該會話核准；`v8-engine-audio-core` 等你指定。
  - 工具線（會話 `v8-tool-line`）：`v8-tool-plan` 完成（`54dac0c3`，計畫在 `docs/tools/plans/v8/`，未推送）；它的 HANDOFF 列了 18 項等你決定。
- **等你決定**：
  - 不屬於玩法的 4 項放在 v8，還是另開維護線：驗收工具的三個問題（v7.1 D231、D233）、Match 連線品質窗口失敗時沒有寫出原因（v7.1 未結事項）、睡眠後重連（N-sleep-reconnect）、I-3 與 outage-resync P2 的兩個已知例外。
  - `v8-product-sfx` 前：音效資產的來源（v7 D38）。
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
| `pv8-seq-narrow-declaration-approve` | （2026-10-30）核准 [pv8-seq-narrow 的事前宣告](pv8-seq-narrow-declaration.md) N1～N6；`pv8-seq-narrow-test-changes` (a) T1～T5 照表改，實作後突變與另一位審查者核對，再交 1～5 核准後 commit；`pv8-seq-narrow-merged-streams` (a) `GatewayDropReport.merged_streams` 保留 u64 並補進 §2a 的保留清單 | 「照建議核准」 |
| `pv8-seq-narrow-test-changes-approve` | （2026-10-30）核准 pv8-seq-narrow 的 T1～T5 判定變更（§14.4 的 1～5；放寬面只有 T1、T3、T4 不再檢查 u32 上限以上的值，由 protoreflect 守衛與 `wire::Counter` 的 Assert 守住），並接受 N4 編碼 Assert 改用 `wire::Counter` 的單元測試加掃描守衛 | 「核准並 commit」 |
| `v8-product-crouch-wire-declaration-approve` | （2026-10-30）核准 [v8-product-crouch wire 接入的事前宣告](v8-product-crouch-wire-declaration.md) W1～W8、U1～U2；`v8-product-crouch-key` (a) 按住 Left Ctrl；`v8-product-crouch-rules-file` (a) `assets/object_fps_pvp/movement_rules.json`＋`--movement-rules`；`v8-product-crouch-scenarios` (a) 對照表＋只加新路徑的情境；`v8-product-crouch-run-quad-mixed` (a) 保留，只改註解 | 「照建議核准」 |
| `v8-product-hit-rules-v2-values` | （2026-10-30）部位倍率照 object_fps_v2 的實作（`assets/object_fps_v2/characters/enemies/*.enemy.json` 的 `hurt_regions`）：頭 2.0；軀幹與骨盆 1.0；上臂、前臂、大腿、小腿（左右共 8）0.75；11 段沿骨骼的膠囊 | 「部位倍率的數值，參考object_fps_v2裡面有完整的實現。」 |
| `v8-product-hit-rules-limb-wire` | （2026-10-30）四肢一層要上 wire：加 `HitPart` 的新值（例如 `HIT_PART_LIMB`）。pv8 合併後的 wire 變更，使用者同意；版本處理（pv8 的延續或升 pv9）在 hitbox-line 的事前宣告決定 | 「改，我說過會有hitbox的。」 |
| `v8-product-hit-rules-permille` | （2026-10-30）傷害用千分位計算，不取整。血量與傷害以千分之一點為單位（2026-10-30 使用者確認：「對」）（100 HP＝100000），倍率是千分位整數（2000、1000、750），25 × 0.75＝18750 整除；畫面顯示時除以 1000。這改變 `maximum_hp`、`shot_damage`、`hp` 等欄位的單位語意，算 Data Contract 變更 | 「用千分位算。」 |
| `v8-product-hitbox-plan-ultracode` | （2026-10-31）hitbox-line 先開規劃批 `v8-product-hitbox-plan`，用 ultracode（偵察 2×Sonnet 5.5、案 3×Opus 5.5、審 1×Opus 5.5、對抗 1×Sonnet 5.5）。起因：Match 沒有模型與動畫，誰來算骨骼姿勢是架構問題（做法 (A) 取樣完整動畫／(B) 固定姿勢的骨骼膠囊／(C) 部位層級的簡化形狀） | 「用 ultracode」 |
| `v8-product-hitbox-plan-approve` | （2026-10-31）核准 [hitbox-plan](hitbox-plan.md) 的 13 項全部照建議：`v8-product-hitbox-model` (a) B-flat；`v8-product-hitbox-plan-scope` (b) rewind 拆成另一條 PR 線（部位判定線約 45～70 h、回溯線約 23～35 h，範圍超過原估 1.5 倍經使用者重新核准）；`pv9-hitbox-version` (a) 升 pv9、⑤ 的附錄 B 改名 pv10；`v8-product-hitbox-view-invariant` (a) 改寫 Design Invariants 第 8 列、守衛分兩段核准；`v8-product-hitbox-rewind-r` (a) 由 lag-l2 的分位數決定（上限 15 Tick）；`v8-product-hitbox-rewind-cross-life` (a) 判 Miss；`v8-product-hitbox-view-lag-base` J；`v8-product-hit-rules-multiplier` HitPart 為鍵、CombatRules 常數；`v8-product-hit-rules-damage-count` (a)；`v8-product-hit-rules-hud` ceil；`v8-product-hitbox-format-owner` (a)；`v8-product-hitbox-determinism-premise` (a)；`v8-product-hitbox-shape-bound` (a) | 「照建議核准」 |
| `v8-product-hitbox-replacement-thresholds` | （2026-10-31，survey 開工前、看結果之前）hitbox 的 Replacement Criteria 門檻：(1) 四肢最大偏差——走、跑、跳、瞄準、射擊、換彈、受擊各 clip 的任一取樣時刻，四肢各段端點（身體座標）離固定姿勢同一端點的距離，大於該段半徑的 2 倍；(2) 輪廓覆蓋率——站姿正面投影上，11 段聯集的面積除以現在單顆膠囊（半徑 0.25、高 1.8）投影的面積，低於 70%。任一成立就停下，交使用者在 A′ 與 A 之間選。這是重選做法的開關，不是 PR 的受入判定 | 「照提案」 |
| `v8-product-hitbox-revisit-premise` | （2026-10-31）survey 觸發門檻 (1) 之後，先不選 A′ 或 A，重新檢視前提：主會話提出 (c)「只用權威狀態決定的形狀表」（瞄準、動作時刻、跳躍階段查表；步伐造成的偏差 (c1) 接受或 (c2) 移動時用包絡形狀），等使用者選 | 「重新檢視前提」 |
| `v8-product-hit-rules-three-parts` | （2026-10-31）從需求出發：沒有塔科夫級的肢體傷害，部位只有頭、軀幹、腿三個。倍率頭 2.0、軀幹（含骨盆）1.0、腿 0.75；手臂不設判定（射到伸出去的手會穿過）。取代 `v8-product-hit-rules-v2-values` 的「11 段，四肢 0.75」；`v8-product-hit-rules-limb-wire` 的新值改名為 `HIT_PART_LEG` 一類；兩條腿合成一個區域，移動時用包絡形狀，不需要步伐相位 | 「我想了一下，首先，從需求出發，我們沒有塔科夫那種級別的肢體傷害，現在核心是頭部，軀幹，腿部，3個點」；「照提案」 |
| `v8-product-hitbox-three-part-thresholds` | （2026-10-31，量測之前）三部位的重選門檻：(1) 頭球中心（固定姿勢下相對 Head 骨的位置）在各動作下的最大漂移大於頭球半徑；(2) 腿部包絡（站、蹲各自涵蓋移動時兩腿所有樣本的形狀）在正面投影上的面積，大於實際兩腿掃過面積的 1.5 倍。任一成立就停下重選做法。軀幹的門檻另決（主會話提的「大於該段半徑」是在第一次 survey 已顯示跑步時軀幹 0.200 m、骨盆 0.182 m 之後才提的，照現有數字已成立，違反「看結果之前宣告」，交使用者決定） | 「照提案」 |
| `v8-product-hitbox-three-part-survey-method` | （2026-10-31，擴充 survey 之前）三部位的量法：(a) 頭球中心＝固定姿勢下頭部頂點重心，換成相對 Head 骨的位置，各動作下用 Head 骨帶著走，量對固定姿勢中心的距離，門檻見 `v8-product-hitbox-three-part-thresholds` (1)；(b) 腿部包絡＝身體座標的軸對齊盒，涵蓋「移動時」（站姿：walk、jog；蹲姿：crouch_move）兩腿四段的所有樣本端點加半徑；實際掃過面積＝同一組樣本的四段在正面投影上的聯集，門檻 (2)；跳躍的腿另列（樣本落在包絡外的比例），只報告；(c) 軀幹與骨盆分「靜止」與「移動中」兩組形狀，移動中的形狀取移動樣本的平均端點；剩下的偏差只報告、不設門檻，寫進 hitbox 文件的已知限制 | 「照建議」 |
| `v8-product-hitbox-lookup-survey-method` | （2026-10-31，量測之前）頭與軀幹改用權威狀態查表後的殘差量法：(a) 跳躍與動作（jump_start、jump_loop、jump_land、shoot、reload、hit）每個 clip 存 8 個等距時刻的形狀，查表在相鄰兩格之間線性內插，以 survey 的 60 個樣本量殘差；(b) 瞄準存權重 −1、0、+1 三張表、線性內插，以 0.1 為間隔量 −1～+1 的殘差；(c) 移動中（walk、jog、crouch_move）頭與軀幹用移動樣本的平均形狀（不分相位）。門檻維持 `v8-product-hitbox-three-part-thresholds` (1)：頭的殘差 > 頭球半徑 0.184 m 就停下；軀幹只報告 | 「先量查表後的偏差」 |
| `v8-product-hitbox-valorant-base` | （2026-10-31）hitbox 改照「先抄成熟方案、依需求改裝、碰到瓶頸才量」：以 Valorant 為基底（伺服器跑由權威狀態決定的動畫，hitbox 綁骨頭，Client 照伺服器的動畫狀態播放），取代第 1 版 B-flat（`v8-product-hitbox-model` (a)）。[hitbox-plan](hitbox-plan.md) 第 2 版待核准 | 「不要自己先想方案，先抄，看看別人如何做的……參考成熟解決方案，然後在根據我們自己的需求進行改裝，碰到瓶頸在測量」；「以 Valorant 為基底」 |
| `v8-product-hitbox-plan-v2-approve` | （2026-10-31）核准 [hitbox-plan](hitbox-plan.md) 第 2 版，照建議：`v8-product-anim-state-on-wire` 要上 snapshot；`v8-product-hitbox-golden-policy` 先量 B3 再定；`v8-product-anim-bottlenecks-first` 先量 B1～B3。開工前先查證第 2 節轉來的參考資料（CS、Valorant、Overwatch、PUBG／Fortnite），避免資訊誤差 | 「照建議核准，但是你先去調查一下，我轉給你的那些方案。否則信息會出誤差。」 |
| `v8-product-anim-bottlenecks-declaration-approve` | （2026-10-31）核准 [事前宣告](v8-product-anim-bottlenecks-declaration.md)，照建議：`v8-product-anim-bottlenecks-thresholds` (a) B1 12 次取樣 ≤ 1.67 ms、B2 載入 ≤ 2 s 且記憶體增量 ≤ 200 MB；`v8-product-anim-bottlenecks-worst-case` (a) 12 次／Tick；`v8-product-anim-bottlenecks-ci-run` (a) 推送分支後手動 dispatch `cross-platform.yml`，不開 PR | 「照建議核准」 |
| `pv9-scope-hitbox-and-rewind` | （2026-10-31）`pv9-hitbox-declare` 把 hitbox-line 與 rewind-line 的 wire 需求一次推完，pv9 不在回溯時再升一次。起因：pv8 設計時假設 pv7 的 `HitPart` 夠用、核對延到 hitbox 批（protocol-v8「不在 pv8」第 6 項），合併後才發現要加腿 | 「一次推完」 |
| `pv9-final-approve` | （2026-10-31）[protocol-v9](../../protocol-v9.zh-Hant.md) 第 9 節 6 項全部照建議：`pv9-animation-digest` (a) Ready 18／Welcome 20 加 `animation_digest`，Client 不合就報錯；`pv9-upper-body-in-hitbox` (a) 上半身動作進判定姿勢，改寫 `Combat.hpp:134-137`；`pv9-sprint-animation` (a) `PlayerAnimation.sprint_weight_permille`＝8，② 之前恆為 0；`pv9-rewind-client-view` (a) 接受漏收時的姿勢差，`ShotRequest` 不加欄位；`pv9-single-merge` (a) 整條 hitbox-line 一個 PR，做完 hitbox-shot 才合併；`pv9-gateway-check-failure` (a) 維持 runtime 失效的慣例，Match 寫入前 Assert。protocol-v9 定稿 | 「照建議核准」 |
| `v8-product-hit-rules-permille-declaration-approve` | （2026-10-31）核准 [事前宣告](v8-product-hit-rules-permille-declaration.md)，3 項照建議：`v8-product-permille-max-shot-damage` (a) `maximum_shot_damage` 現在就是 50000，部位倍率（2000／1000／750）一起進 `CombatRules`；`v8-product-permille-digest-v3` (a) DigestVersion 3 加 `decision.hitPart` 與受擊紀錄四欄；`v8-product-permille-compare-tool` (a) 擴充 `compare_authority_trees.py`（`--scaled`、`--added`） | 「照建議核准」 |
| `v8-product-hit-rules-leg-wire-declaration-approve` | （2026-10-31）核准 [事前宣告](v8-product-hit-rules-leg-wire-declaration.md)，照建議：寬鬆面只限值 3（commit 前先做突變與 Sonnet 5.5 的審查）；`v8-product-leg-wire-worker-probe` (a) 在 worker probe 加腿的情況 | 「照建議核准」 |
| `v8-product-anim-authority-declaration-approve` | （2026-10-31）核准 [事前宣告](v8-product-anim-authority-declaration.md)，照建議：`v8-product-anim-rules-source` (a) CPU 載入器搬到新程式庫 `player_content`，Match 與 app_support 共用；`v8-product-anim-backward` (a) 用 `std::sin`／`std::cos`；`v8-product-anim-epoch-rule` (a) 換 epoch 整個重設；`v8-product-anim-xhigh-review` (a) A1 狀態機與 A4 內插各一次 xhigh 審查；`v8-product-anim-presentation-invariants` 附錄 A 照草稿 | 「照建議核准」 |
| `v8-product-anim-rules-source-texture-free` | （2026-10-31）`v8-product-anim-rules-source` (a) 的前提不成立（CPU 載入器會讀貼圖，需要 SDL image），改採 (a-1)：`player_content` 有兩條載入路徑，Client 照舊完整載入、Match 只讀角色 FBX 與動畫集；規則的推導只寫一次，Client 與 Match 的規則以對齊測試確認相同。估時多約 2～3 h | 「照建議 (a-1)」 |
| `v8-product-anim-authority-e1-jump` | （2026-10-31）E1 試行 1 不成立之後，核准跳躍判定的放寬：只容許舊 Client 的 double 比較在剛好整數 Tick 跨度時漏判的那一個 Tick（§14.4 的 1～5、判定函式突變 4／4、Sonnet 審查），重跑時延遲 Tick 必須恰好是試行 1 的 19 個 | 「核准並重跑」 |
| `v8-product-anim-phase-definition` | （2026-10-31）protocol-v9 §4.3 的相位定義與宣告的表改成實作：每 Tick Δq16＝round(有號位移 ÷ 一圈距離 × 65536)，一圈距離用這個 Tick 速度的精確比例，累加 mod 2^32 | 「照建議 (a)」 |
| `v8-product-anim-authority-a4-approve` | （2026-10-31）核准 A4 的 §14.4 逐項表與寬鬆面 1～5（T2、T3、T5、T6、T7、T8、T9、P1、P2；先實作不 commit → 突變 16／16 → Sonnet 與 xhigh 審查 → 修正後核准） | 「核准並 commit」 |

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
- 2026-10-30：`pv8-seq-narrow` 完成。84 欄縮窄、刪 `match_id`、預留號碼；N1 0 差分；實測 724／798／674／402／470／398 B；突變 12／12＋proto 手動 3／3；審查者 5 點成立；CTest 100／100。
- 2026-10-30：`v8-product-crouch` 的 wire 接入完成，pv8-line 的 A 層做完。W1 38 個情境 0 差分；W2 衝刺的 digest 相同；實測 748／822／698／450 B；突變 23／23；CTest 100／100。突變工具的前置檢查 worker 失敗 1 次未切分，之後 10 次重複都通過。
- 2026-10-30：合併 origin/master（`e31dfdf4`，tool-line 的 #107、#108），本機建置與 CTest 104／104 通過；推送 `claude/pvp-v8-pv8-line` 並開 PR #110。
- 2026-10-30：PR #110 的 L1 四平台全部通過（run `41046572657`），狀態 CLEAN。
- 2026-10-30：PR #110 合併（`6b22d82c`），pv8-line 收尾。
- 2026-10-30：CONNECTION POOR 的調查（audio-line 開的 connection-poor 會話）：成因是 Match 停頓超過約 100 ms → Backlog 重設 → 品質窗口判失敗；和音效無關，master 上重現。使用者決定不改、交給 B6；README 的 B6 列與 network-architecture §7 的「P2 的第二個例外」補上品質窗口。調查報告：[dev_log](../../../dev_logs/2026_10_30_pvp_v8_connection_poor.zh-Hant.md)（2026-10-31 從會話筆記搬進 repo；證據在使用者 Intel Mac 主 checkout 的 `build/evidence/connection-poor-20261030/`，git 管理外，AGENTS §15）。
- 2026-10-31：同步 master `d8cca12a`（#111、#112、#113）。`v8-product-hitbox-plan` 開始：ultracode run `wf_c70a45ea-cdc`（偵察中途停下，加入使用者轉來的「其他遊戲怎麼處理 hitbox」參考後以 `wf_7b0fc672-291` 重跑）。
- 2026-10-31：`v8-product-hitbox-plan` 的 ultracode 完成（`wf_7b0fc672-291`，7／7）：評分最小契約 25、決定性 23、還原 v2 14；基底 B-flat（固定姿勢的 11 段骨骼膠囊，Match 不讀模型）。對抗式檢查 must 3、should 9、note 4 全部接受並寫進 [hitbox-plan](hitbox-plan.md)。總估 68～105 h，超過原本的 1.5 倍，13 項等使用者決定。
- 2026-10-31：`v8-product-hitbox-survey` 完成量測（約 2 h）。量法修正 1 次（手腳頂點排除，半徑變小、門檻只會變嚴）。門檻 (1) 成立，停下交使用者決定做法。
- 2026-10-31：survey 第 2 輪（三部位）。頭的漂移門檻成立（落地 0.519、起跳 0.383、瞄準 0.29～0.32、跑步 0.252 m），腿部包絡未成立。停下交使用者決定。
- 2026-10-31：survey 第 3 輪（查表）。頭的殘差只剩起跳 0.247 m 超過門檻（8 格的表內插誤差）；跑步 0.176 m 貼近門檻。停下交使用者決定。
- 2026-10-31：使用者指出方法跑偏（自己設計再一輪輪量），改照成熟方案：以 Valorant 為基底重寫 hitbox-plan（第 2 版），部位判定線估約 66～102 h（推論）。

## 未結事項

- 無。
