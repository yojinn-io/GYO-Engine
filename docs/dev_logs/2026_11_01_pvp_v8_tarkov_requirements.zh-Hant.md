# PvP v8：v8-product-tarkov-requirements（迷你塔科夫的實體需求問卷）

日期：2026-11-01。分支 `claude/gifted-khorana-d38ac5`（worktree `gifted-khorana-d38ac5`，切自 master `8a175c0a`，途中合併 `d4a79c4a`）。檔位 medium、下段，主會話 Fable 5.1 直接做，沒有子 agent。估時 2～3 h，實際約 1.5 h（含開工前的行號核對與規則更新的同步）。

**狀態：** 完成。問卷全照典型值（`v8-product-entity-req-typical-values`）；第 7 節是主會話的建議，待使用者確認。

## 做了什麼

- 開工前核對架構卡的行號（記在 [v8 HANDOFF](../object_fps_pvp/plans/v8/HANDOFF.md) 2026-11-01 的規劃紀錄）：pv9 已隨 #128 合併，⑤ 的協議是 pv10；`Participant` 多了動畫欄位，Participant 外另有按 PlayerId 的 `shapes_`、`actionsCombat_`；`PlayerId` wire 為 uint32 且由 Gateway 配發；權威 digest 與兩樹比對的位置；rewind 尚未開工。
- 寫 [實體需求問卷](../object_fps_pvp/plans/v8/entity-requirements.md)：照 [pv8 需求問卷](../object_fps_pvp/plans/v8/pv8-requirements.md)的格式，只問玩法。pv8 已問過的 9 組題沿用不重問；新問 6 節 30 題（實體種類與數量、身分、人物屬性、PvE 規模與 AI 行為、PvP／PvE 組合、誰看得到什麼），每節寫「這題決定什麼」。
- 使用者「全照典型值」。填入後寫「結論」節，作為 `v8-product-entity-contract` 的輸入：
  - id 位寬 uint32 夠（一個 Match 程序生命期約 3,400,000 個實體，餘裕約 1,260 倍；uint16 不夠）。
  - 同時存在約 400 個實體，元件用按 EntityId 排序的連續陣列就夠，不需要 sparse set 或 archetype。
  - 種類是標籤：玩家、AI、Boss 共用身體、戰鬥、動畫元件；只有玩家有「連線」這組輸入來源的狀態，AI 的輸入來源是 brain。
  - 命中結果只要 `hit_entity_id`，pv8 預留的 `HIT_NPC` 不需要。
  - 三種複寫通道的內容與過濾距離；決定性只要求 Match 自己一致，AI 亂數要有種子進 digest。
- 第 7 節沒有典型值，主會話建議：7-1 第一個非玩家實體是 AI 小怪（對「輸入來源與元件分離」最完整的檢驗）；7-2 不拿掉任何項目。
- 計畫：v8 README 加 `entity-line`（樹、各批明細、PR 線）；同日 PR #130 的 navigator 規則生效，三批補上上段／下段與各角色的模型，使用者確認（`v8-product-entity-model-navigator`）。

## 檢驗

- 文件批，沒有程式變更，沒有建置或測試。
- 問卷的典型值來源逐題標了 pv8 題號或附錄 C；結論的數字都能從問卷的典型值算回去（1-3 × 7 天 × 24 局 ≈ 1-4）。
- 證據：無另存的檔案；本 dev_log 與問卷就是全部產出。

## 和其他會話的對齊

- [Task]v8 規劃會話：確認沒有別的會話在改 `PvpMatch.hpp`／`.cpp`；split 排在 rewind 之前無異議；提醒 README 與 HANDOFF 會和 hit-presentation 的 PR 文字衝突。
- [Job][架構師]會話：卡上的模型規則以 navigator.md 為準，已照改。

## 下一批

`v8-product-entity-contract`（上段，ultracode 先問使用者）。開工時先讀 EnTT／flecs 的儲存設計並寫下為何這麼做、能用多少、要改裝多少。
