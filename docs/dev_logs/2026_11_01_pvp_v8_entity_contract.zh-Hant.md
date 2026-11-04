# PvP v8：v8-product-entity-contract（實體資料模型的契約）

日期：2026-11-01。分支 `claude/gifted-khorana-d38ac5`（worktree `gifted-khorana-d38ac5`）。檔位 ultracode（契約）、上段；主會話 Fable 5.1。估 6～10 h，實際約 3.5 h（workflow 約 50 分、134 萬 token；整理與寫文件約 2.5 h）。

**狀態：** 完成。[entity-model.zh-Hant.md](../object_fps_pvp/entity-model.zh-Hant.md) 定稿，8 個決定與 Design Invariants 節經使用者核准（`v8-product-entity-contract-approve`，原話「照建議核准」）。

## 做了什麼

- ultracode workflow（run `wf_f9f52a0b-209`，7／7 回傳，0 錯誤）：
  - 偵察 2×Opus 5.5 high：(a) EnTT／flecs 的儲存設計（sparse set、archetype、id 的 generation 位元、entity range、`make_alive`、迭代順序與 swap-and-pop）；(b) Match 內所有按 PlayerId 的狀態、`player_id` 從 Gateway 配發到 digest 的流向、Client 消費 snapshot 的假設、單機 EnemySystem 的先例。
  - 提案 3×Opus 5.5 high：最小變更、決定性優先、協議優先。
  - 統合 Fable 5.1 high：評分 25／24／23，以協議優先為基底接枝另兩案；寫 EnTT／flecs 評估、7 個待決、Delta 七點。
  - 敵對檢查 Opus 5.5 xhigh：must 3、should 12、note 7，判定「修正後可核准」；抽查約 30 處行號。
- 主會話把統合案與 22 項發現併成 `entity-model.zh-Hant.md`（435 行）：§1 成熟方案對照（為何、能用約四成、要改裝約六成、起點理由的修正）；§2～§5 四件事；`Design Invariants` E-1～E-12 與 Replacement Criteria RC-1～RC-5；§7 守衛 G1～G12；§8 pv10 附錄 C 改寫建議 8 項；§9 split 的步驗序與驗收；§10 與 rewind 的接法；§11 Delta 七點；§12 決定記錄；處置表。
- 敵對檢查改掉的要緊事：split 的 base 要是缺陷批 commit；缺陷批重生 golden 依 §14.4 有緩める面、要使用者核准；新增 E-12（Tick 內不改欄的成員，生成與刪除記待辦在固定階段套用）；連線相關的公開 API 一律查 `connections_`；沒有 Connection 的射手不量 reference age；G2、G6、G8 的觀測手段改寫；RC-4 限判定函數、另立 RC-4b；決定 1 加 Match 段上限 2^28−1 的選項；決定 5 拆成兩軸。
- 主會話另核對：`MatchRuntimeHost.cpp:756-760` 的逐出只 `erase_if` players 與 combat；`adapter.go:132` 檢查 `len(in.Animation) != len(in.Players)`；`RequireValidAnimation` 只在 `PvpMatch::Snapshot()` 內；runner 的 json 有 `records` 欄位（`AuthorityDigest.cpp:938`）。缺陷屬實。
- 計畫：README 的 entity-line 加 `v8-product-evict-animation-orphan`（排在 split 之前）；HANDOFF 記決定。

## 檢驗

- 文件批，沒有程式變更。
- 證據：agent 的原始輸出（兩份偵察、三案、統合、敵對檢查、workflow 的 json 與腳本）存在主 checkout `build/evidence/v8-product-entity-contract/`（git 管理外，Intel Mac）。
- 行號以 `1c598a83` 為準，敵對檢查抽查後有偏移的已改寫；定稿前的逐一重核留到 split 批開工（文件 §13 已寫）。
- 未實測：容量 40＋211N B、排序 vector 的成本、varint 位元組的節省；都標「推論」。

## 發現（不在本批修）

- 實證缺陷：host 逐出玩家時 snapshot 的 animation 沒刪，Gateway 會 `runtimeFailed` 讓所有人斷線。排成 `v8-product-evict-animation-orphan`。真正 Gateway 上的失效未重現；偵察的 dump 在 scratchpad，不算證據，缺陷批要在本工作樹重跑。
- pv9 的 `CombatState` 全公開（含 hp、彈藥）與需求 3-3「數值不公開」衝突；pv10 要拆成公開 CombatPose 與私人 Vitals。
- Match 沒有任何亂數種子來源；AI 批要定，否則 E-10 無法落地。
- `MaxPlayers=4` 與需求 ≤8 的落差，pv10 要一併重寫三方檢查。

## 下一批

`v8-product-evict-animation-orphan`（high，3～5 h），再 `v8-product-match-entity-split`（事前宣告照 entity-model §9.2）。
