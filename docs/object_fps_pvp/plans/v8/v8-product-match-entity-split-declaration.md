# v8-product-match-entity-split 的事前宣告

更新：2026-11-03。Owner：`object_fps_pvp`。**狀態：已同意（`v8-product-match-entity-split-declaration-approve`，2026-11-03，檔位照建議）。** 契約：[entity-model](../../entity-model.zh-Hant.md) §3（元件）、§5（決定性）、§9（步驗序與驗收）、Design Invariants E-5、E-7、E-8、E-9；決定 4 (a)（每元件一欄的 vector）。檔位 high、digest 與迭代順序局部 xhigh；上段（Architecture 判斷、DI 守衛測試）；實作者主會話 Fable 5.1、審查者 `xhigh-reviewer` 指定 Opus 5.5 xhigh、§14.4 檢證者 Opus 5.5。估 14～20 h（超過 30 h 停下回報）。

## 這批做什麼

Match 內部的 Ownership 重整：`PvpMatch::Participant`（`PvpMatch.hpp:116-137`，一個 `std::map<PlayerId, Participant>`）拆成五個依 EntityId 升冪的欄——實體元件 `bodies_`、`combats_`、`animations_`，輸入來源 `actionLedgers_`、`connections_`；Participant 外的 `shapes_`、`actionsCombat_` 歸位成 Tick 內派生暫存 `tickShapes_`、`actionStartCombats_`。刪實體只走 `PvpMatch::EraseEntity`。**不改 wire、不改行為、不改公開介面、不改 digest。**

不在這批：段界常數與 Match 段配發器（第一個非玩家批）、`EntityKind` 種類表（第一個非玩家批）、PlayerState 拆欄（第一個 AI 批）、pv10、PoseHistory（rewind 批）。這是 AGENTS §3 的 Architecture Delta (B)，七點在 entity-model §11。

## base

兩樹比對與 diff 的 base＝`edbd827f`（`v8-product-evict-animation-orphan` 的最後一個 commit，含重生後的 golden；entity-model 決定 3 (a)）。base 的 runner 另建一棵樹（-O0 與最佳化兩種），`--list` 應為 42 個情境、37 個 golden。

## 步驗序（entity-model §9.1；每步 build、跑 `authority_digest` CTest、再跑兩樹 compare；任一步不是 0 差分就停下照 §14 回報）

| 步 | 內容 | 預期 |
|---|---|---|
| S0 | 本宣告取得同意；建 base 樹的 runner；證據目錄 `build/evidence/v8-product-match-entity-split/` | — |
| S1 | `EntityId.hpp`（`EntityId`、`PlayerId` 別名、`NoEntity`）、`EntityColumns.hpp`（`FindEntity`、`InsertEntity`、`EraseEntity` 三個自由函數）與它們的單元測試 | 行為沒用到；不跑 compare，記錄「未跑」 |
| S2 | `Participant` 的 retiredActionThrough、actions → `ActionLedger`；其餘輸入來源與診斷 → `Connection`；兩欄升冪 vector；相關私有函式改簽名 | 0 差分 |
| S3（局部 xhigh） | `players_` → `bodies_`、`combats_`、`animations_`（`AnimationTrack{value, before}`）；16 處使用逐一改成走欄或 `FindEntity`；連線相關的公開 API 一律查 `connections_`；Leave → `PvpMatch::EraseEntity`。G1、G2 先寫先跑 | 0 差分 |
| S4 | `actionsCombat_` → `actionStartCombats_`（整欄複本）、`shapes_` → `tickShapes_`（升冪 vector）；Reset 清掉五欄與快取 | 0 差分，特別看 combat 與 hitbox 情境 |
| S5 | `Snapshot()` 依 `bodies_` 順序以 `FindEntity` 組三個 vector；加 `RequireEntityOrder`（三個 vector 的 id 序列嚴格升冪且相同）；更新 entity-model「實作現況」 | `--scale 1` 與 `--scale 10` 都 0 差分；golden 不變 |
| S6 | 突變 M1～M5（只跑本批）；`xhigh-reviewer`（Opus 5.5 xhigh）審查 S3、S5 的迭代順序與 digest 路徑 | 5／5 killed；審查的發現先紅後綠 |
| S7 | dev_log、README 一覽、HANDOFF；同一變更提交 | — |

## 守衛與突變

| 守衛 | 做法 | 突變（應被殺） |
|---|---|---|
| G1 `EntityColumnsStayOrdered` | 3 個 id 依降冪 Join（3, 2, 1），每 Tick 檢查 Snapshot 三個 vector 的 id 序列＝{1,2,3}；Leave(1)→{2,3}；再 Join(1)→{1,2,3}；120 Tick。既有 golden 殺不掉 append 突變，所以必須是單元測試；digest 情境數維持 42 | M1 `InsertEntity`→push_back；M2 `EraseEntity`→swap-and-pop；M4 Snapshot 反向走欄 |
| G2 `EraseEntityRemovesEveryColumn` | Join 兩人、開槍讓 `tickShapes_`、`actionStartCombats_` 有值，Leave(1)；friend 存取器 `ColumnIdsForTest()` 斷言各欄與快取都不含 1；再 Join(1)＋跑 Tick 靠 `InsertEntity` 的重複 Assert；公開 API 回 nullopt／false | M3 `EraseEntity` 漏刪 `animations_`；漏刪 `connections_` |
| G7 `TieBreakByEntityId` | `QueryShot` 等距兩目標正反順序都選小 id；`ResolveActions` 同 A 的動作依 id 先後；同時重生 id 小的先放 | 平手鍵改成插入順序 |
| `RequireEntityOrder` | GYO_ASSERT，37 個 golden 情境每 Tick 經過 | — |
| M5 | `actionStartCombats_` 改在動作之後才複製 | 被 hitbox 的 golden 情境與 compare 殺 |

## 驗收（結果出來前宣告；結果出來後不改）

報告開頭一行：`件數の突き合わせ：一致（base 與 branch 情境數 42＝42；每個情境的 record 數相等，由兩樹 --run --ticks 輸出的 json 各情境 records 欄位逐一比對）／不一致（…）／確認できない（…）`。

1. `compare_authority_trees.py --base <edbd827f 的 runner> --branch <S6 的 runner>`，`--scale 1` 與 `--scale 10` 各一次，exit 0，42 個情境全部 0 差分；不得用 `--scaled` 或 `--added`。
2. `authority_digest` CTest 通過；`git diff edbd827f -- tests/object_fps_pvp/fixtures/authority_golden.txt` 為空。
3. `DigestVersion = 4`、`DigestFields` 不變；dump 標籤不變：兩樹對全部 42 個情境 `--dump` 再 diff 為空。
4. `git diff --stat edbd827f -- apps/object_fps_pvp/protocol apps/object_fps_pvp/gateway engine services` 為空；Client 專用原始碼（ClientConnection、ClientSimulation、LocalPlayerPrediction、SnapshotTimeline、PvpApplication、PlayerPresentation）為空；`MatchRuntimeHost`、`IpcHost` 只允許因簽名改動而跟著改的行；`AuthorityDigest.cpp` 不改。
5. G1、G2、G7 Green；M1～M5 全部被殺。
6. 產品單元測試全部通過；L1 的 cpu|shader 全部通過（PR 開了之後看）。
7. 範圍的限界：效能不量（RC-1 的門檻之後有實體再量）；network 標籤只在本機；split 的 0 差分只有 2 人情境，3 個以上實體的順序由 G1 的單元測試守。
8. 公開介面（`PvpMatch.hpp:82-111`）簽名不變；私有函式的簽名改變不算。

## 時間與停止條件

- 估 14～20 h；超過 30 h（1.5 倍）停下回報。
- 任一步不是 0 差分、G1／G2 寫出來殺不掉對應突變、或發現現有程式依賴 `std::map` 的參照穩定性而 vector 做不到（E-12 的情況提前出現），停下照 AGENTS §14 回報，不自行放寬。
