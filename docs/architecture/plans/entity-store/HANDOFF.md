# 實體容器（entity store）：交接

更新：2026-11-05。**狀態：batch 2 核准（2026-11-05，§7 全照建議）；batch 3a `v8-arch-entity-store-impl` 待驗收事前宣告的承認。** 分支 `claude/v8-arch-entity-store`（自 master `318b4e66` 切出）。
本文件是持續記錄器：每批開始、里程碑、停止時，與工作在同一變更中更新。停下或回報前，先更新「現況」。

## 代號對照

| 代號 | 白話 |
|---|---|
| `v8-arch-entity-store-survey` | 參照調查：Bevy ECS 與 EnTT 逐機制判定 |
| `v8-arch-entity-store-delta` | Architecture Delta 與 Engine 契約文件 |
| `v8-arch-entity-store-impl` | Engine 模組實作與 pvp 換上 |

## 決策

- 2026-11-01（使用者）：ECS 只採資料佈局，不採通用執行期，留在產品（PR #140、entity-model.zh-Hant.md）。
- 2026-11-05（使用者，經圖書管理員會話）：ECS 放進 Engine 共通模組（案 (b)）；核准「先做 Bevy／EnTT 參照調查表作為 Design Invariants 根據」的進行方式。
- 2026-11-05（使用者，本會話）：核准 batch 1 的構成：實作者 Opus 5.5／high；敵對檢查者 Fable 5.1／xhigh（xhigh 的理由：要否定的三點涉及決定性與協議）；參照程式以 shallow clone pin 到 release tag（Bevy v0.20.1 `a0577b5c`、EnTT v3.16.0 `b4e58bdd`），放在會話的 scratchpad（不保存），文件以 tag＋路徑＋行號的 permalink 作為耐久的出處。
- 架構師的暫定立場（**未採納**，batch 2 核准 DI 時由使用者決定）：Engine 只收「容器」（Entity＝index＋generation、SoA column、id 升冪的決定性迭代、延後結構變更的 Commands、change tick）；system／scheduler／world／resources／events／app／plugin 不收；id 分段政策與種類意義留在產品。
- 2026-11-05（使用者，本會話）：「全照建議核准」delta §7 的 10 項：容器版；batch 3 拆 3a（欄，現在做）／3b（`DeferredChanges`，與第一個非玩家批同時）；`engine/entity_store` → `GYO::EntityStore`；待辦套用生成先、刪除後、各依 id 升冪；同點重複刪除合併；函數名沿用 pvp 現名；E-12 以消費端 Assert 守（凍結區間＝Tick 扣掉 Apply），並同意產品 E-5 守衛欄與 E-12 措辭的改寫；`architecture.md` 3a 合併時用 3a 版、3b 補完整版；batch 3 驗收內容照第 9 項、於開始時正式宣告；契約 DI 1～10 與 RC 1～15 生效。

## 現況

- 2026-11-05：收到任務卡；偵察：worktree 乾淨、參照檔案齊全；`entity-model.zh-Hant.md` §1 已有 EnTT／flecs 的調查（約四成可直接用、約六成要改裝），survey 以它為起點只補 Bevy 與逐機制深挖，flecs 不在範圍。
- 2026-11-05：使用者說「開始」。切分支，clone 參照程式，啟動實作者（Opus 5.5／high／general-purpose）。估時：實作者 60～90 分、檢查者 30～45 分、修正 30 分、文件 30 分，共 2.5～3.5 h；超過 1.5 倍停下回報。
- 2026-11-05：實作者交回 survey.zh-Hant.md（284 行、111 個 permalink，約 55 分）。三點判定：a 成立、b 部分成立、c 不成立；與假說矛盾 4 處（generation 對 E-3、change tick 對 entity-model §1.4、Commands 的結構性守護在 C++ 不成立、「index」暗示 Engine 配 id）。原始報告存主 checkout `build/evidence/entity-store/survey-implementer-report-20261105.md`。接著啟動敵對檢查者（Fable 5.1／xhigh／xhigh-reviewer，唯讀）。
- 2026-11-05：敵對檢查者（Fable 5.1／xhigh）交回 must 2／should 7／note 8（約 40 分；抽查 permalink 約 70／111）。兩個 must 都是「判定對、理由錯」：(1) 把 Rust 借用檢查與階段紀律混為一談——Bevy 的 Commands 守存取衝突，exclusive system 與 `spawn_at`／`despawn_no_free` 可隨時改結構，E-12 在 C++ 可用型別分離表達；(2) Bevy `spawn_at` 與 EnTT `generate(hint)` 不是「容器不配 id」的先例，兩者仍以容器為 id 權威，根據只有 E-2。3a 確認、3b／3c 收窄（3c 由「不成立」改「不採（第一版）」，理由換成 C++ 無 `DerefMut` 攔截、§1.4 已核准不採、無觀測到的消費者）。報告存主 checkout `build/evidence/entity-store/survey-adversarial-check-20261105.md`。
- 2026-11-05：實作者（同一子 agent）處置全部 17 項（約 30 分），無反對；survey 284 → 321 行，新增 §6a「敵對檢查的處置」。主會話核對：判定與理由已改寫、無日語與簡體字殘留、參照定義 120 個全部對應。dev_log `docs/dev_logs/2026_11_05_arch_v8_entity_store_survey.zh-Hant.md`。**batch 1 完成，停下等使用者核准**；總用時約 2.5 h（估 2.5～3.5 h）。
- 2026-11-05：架構師會話轉達使用者的語言規則更正（`~/.claude/CLAUDE.md` 例外節，已核對）：本卡片會話全程用正體中文回覆；文件、commit、PR 語言不變。
- 2026-11-05：使用者說「核准，進 batch 2」。batch 2 `v8-arch-entity-store-delta` 開始：實作者 Opus 5.5／high（general-purpose），審查者 Opus 5.5／high；不寫程式。產出 `docs/architecture/entity-store.md`（英文契約）與 `docs/architecture/plans/entity-store/delta.zh-Hant.md`（AGENTS §3 七點、搬移或新建的決定、E-1～E-12／RC-1～RC-5 的歸屬對應表、給圖書管理員的 architecture.md:129 修改稿）。估時 2.5～3 h。
- 2026-11-05：batch 2 實作者交回 `docs/architecture/entity-store.md`（213 行，DI 1～10、RC 1～15）與 `delta.zh-Hant.md`（171 行），約 10 分（根據大多已在 survey 與 entity-model）。建議：新建 Engine 模組 `GYO::EntityStore`＋pvp 改裝（不直接搬檔，因現行標頭含產品知識）、容器版、batch 3 拆 3a（欄，現在做）／3b（`DeferredChanges`，與第一個非玩家批同時）。報告存主 checkout `build/evidence/entity-store/delta-implementer-report-20261105.md`。
- 2026-11-05：審查者（Opus 5.5／high，唯讀）交回 must 1／should 12／note 11（約 35 分），總評「修正後可交核准、不需重做；設計方向無反對根據」。must：`EntityOf`「所有者多載優先」以同名多載會 ambiguous、匿名命名空間的測試在 Engine 內 ADL 找不到 → 契約要改成所有者提供的 hook。should 主要：Engine 文件不得引產品路徑（可刪除性）；「觀測」壓力只有「容器內容已與產品無關」、其餘是使用者決定與預測；33 行應為 30 行；E-5／E-12 的產品 DI 變更要列入並取使用者同意；architecture.md 稿與 3a 分割衝突；缺 batch 3 事前宣告。報告存 `build/evidence/entity-store/delta-review-20261105.md`。修正交回同一實作者。
- 2026-11-05：實作者處置審查全部項目（約 6 分；M1、S1～S12、N1～N9，N10／N11 不需改）：鍵的讀法改為 `EntityKey<T>` trait（所有者特化優先，預設讀 `id`，不靠 ADL）；契約移除所有產品路徑（grep 0 件）；判斷事項全部移到 delta §7（10 項）；新增 delta §10「審查的處置」。實作者自行在契約定了「callback 擲出時清空待辦」（N6），可改列為決定。主會話核對：DI 10／RC 15、§7 10 項、無假名與簡體。dev_log `docs/dev_logs/2026_11_05_arch_v8_entity_store_delta.zh-Hant.md`。**batch 2 完成，停下等使用者決定**；總用時約 1.5 h（估 2.5～3 h）。
- 待辦（核准後）：通知圖書管理員會話本線分支與 delta §5 的 architecture.md 修改稿（用哪一版依決定 8）；開 PR（batch 1～2 文件）；batch 3 開始前事前宣告驗收判定（delta §7 第 9 項）。
- 2026-11-05：使用者「全照建議核准」（見「決策」）。契約與 delta 的狀態改為已核准；推送分支、開 PR [#156](https://github.com/yojinn-io/GYO-Engine/pull/156)（草稿；batch 1～2 文件，3a 的程式與圖書管理員的 architecture.md 修改進同一 PR）；已通知圖書管理員會話（請它先讀 delta §5 與契約，等 3a 程式進分支後再改 architecture.md，用「3a 版」）。下一步：提出 batch 3a 的驗收事前宣告，取得承認後開始。
- 2026-11-05：寫 batch 3a 的[驗收事前宣告](impl-3a-declaration.zh-Hant.md)（7 項判定、範圍、模型／檔位、估時 9～13 h），停下等使用者承認；檢證者 Fable 5.1 也在此一併請示。

## 未結事項

- `docs/architecture.md:129` 的 ECS 字樣：在 batch 2 的 PR 內由圖書管理員會話修改；本線不改。
- batch 3 的換上時機（與第一個非玩家實體的批同時，或先做）：batch 2 決定。
