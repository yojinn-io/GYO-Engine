# 實體容器（entity store）：交接

更新：2026-11-05。**狀態：batch 1 `v8-arch-entity-store-survey` 完成，等使用者核准；batch 2 未開始。** 分支 `claude/v8-arch-entity-store`（自 master `318b4e66` 切出）。
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

## 現況

- 2026-11-05：收到任務卡；偵察：worktree 乾淨、參照檔案齊全；`entity-model.zh-Hant.md` §1 已有 EnTT／flecs 的調查（約四成可直接用、約六成要改裝），survey 以它為起點只補 Bevy 與逐機制深挖，flecs 不在範圍。
- 2026-11-05：使用者說「開始」。切分支，clone 參照程式，啟動實作者（Opus 5.5／high／general-purpose）。估時：實作者 60～90 分、檢查者 30～45 分、修正 30 分、文件 30 分，共 2.5～3.5 h；超過 1.5 倍停下回報。
- 2026-11-05：實作者交回 survey.zh-Hant.md（284 行、111 個 permalink，約 55 分）。三點判定：a 成立、b 部分成立、c 不成立；與假說矛盾 4 處（generation 對 E-3、change tick 對 entity-model §1.4、Commands 的結構性守護在 C++ 不成立、「index」暗示 Engine 配 id）。原始報告存主 checkout `build/evidence/entity-store/survey-implementer-report-20261105.md`。接著啟動敵對檢查者（Fable 5.1／xhigh／xhigh-reviewer，唯讀）。
- 2026-11-05：敵對檢查者（Fable 5.1／xhigh）交回 must 2／should 7／note 8（約 40 分；抽查 permalink 約 70／111）。兩個 must 都是「判定對、理由錯」：(1) 把 Rust 借用檢查與階段紀律混為一談——Bevy 的 Commands 守存取衝突，exclusive system 與 `spawn_at`／`despawn_no_free` 可隨時改結構，E-12 在 C++ 可用型別分離表達；(2) Bevy `spawn_at` 與 EnTT `generate(hint)` 不是「容器不配 id」的先例，兩者仍以容器為 id 權威，根據只有 E-2。3a 確認、3b／3c 收窄（3c 由「不成立」改「不採（第一版）」，理由換成 C++ 無 `DerefMut` 攔截、§1.4 已核准不採、無觀測到的消費者）。報告存主 checkout `build/evidence/entity-store/survey-adversarial-check-20261105.md`。
- 2026-11-05：實作者（同一子 agent）處置全部 17 項（約 30 分），無反對；survey 284 → 321 行，新增 §6a「敵對檢查的處置」。主會話核對：判定與理由已改寫、無日語與簡體字殘留、參照定義 120 個全部對應。dev_log `docs/dev_logs/2026_11_05_arch_v8_entity_store_survey.zh-Hant.md`。**batch 1 完成，停下等使用者核准**；總用時約 2.5 h（估 2.5～3.5 h）。
- 2026-11-05：架構師會話轉達使用者的語言規則更正（`~/.claude/CLAUDE.md` 例外節，已核對）：本卡片會話全程用正體中文回覆；文件、commit、PR 語言不變。

## 未結事項

- `docs/architecture.md:129` 的 ECS 字樣：在 batch 2 的 PR 內由圖書管理員會話修改；本線不改。
- batch 3 的換上時機（與第一個非玩家實體的批同時，或先做）：batch 2 決定。
