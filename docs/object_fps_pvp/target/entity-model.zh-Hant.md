# PvP 的實體資料模型（目標架構）

> Status: target, not implemented

更新：2026-11-13（`v8-arch-a6-revision`，架構師會話建檔；產品條文由 `v8-product-match-sim` 起的產品遷移批寫入）。Owner：`object_fps_pvp`。

這是 legacy [`../entity-model.zh-Hant.md`](../entity-model.zh-Hant.md) 的後繼。放在產品自己的 `target/` 資料夾，是因為產品的欄位不進 Engine 層的設計文件：`docs/architecture/target/world.zh-Hant.md` 只寫通用規則（AGENTS §6、§7）。兩套文件並存期間的規則照 [`docs/architecture/target/README.md`](../../architecture/target/README.md)：現有程式（`PvpMatch`、`MatchRuntimeHost`…）受 legacy 檔的 `Design Invariants` 約束；遷到 Kernel 上的新程式受本檔約束。產品遷移完成時 legacy 檔刪除，本檔搬回 `docs/object_fps_pvp/entity-model.zh-Hant.md`。

本檔只寫已經定的事。還沒定的寫「由哪一批的事前宣告定」，不寫成條文。

## 1. 欄位分類：拆欄的依據

Kernel 的寫入政策是「一個欄在一個 World 裡至多一個寫者，以 System 計」（`target/world.zh-Hant.md` §3；spec K.3 第 9 項）。今天 `bodies_` 欄存整份 `PlayerState`，由三段程式寫（生死邊界、移動、擊殺）；拆成 System 之後就是三個寫者，啟動檢查不會過。使用者的決定是**拆欄**，不併 System、不改成多寫者。拆法照 legacy 檔 §3.2 `bodies_` 列已經做好的四組分類：

| 組 | 今天 `PlayerState` 裡的欄位 | 今天誰寫 | 進權威 digest | 目標 |
|---|---|---|---|---|
| 身體 | position、yaw、pitch、verticalVelocity、grounded、crouched、sprinting | 移動（`StepMovement`）；重生時由生死邊界重設整組身體欄位（不只 position） | 是 | 一個欄、一個寫者。重生時的整組重設怎麼交給那個寫者（事件或中間欄），由 `v8-product-match-sim` 的事前宣告定 |
| 生命 | lifeGeneration、lifeState、lifeStateTick、respawnTick | 生死邊界（`ResolveLifeBoundaries`）與擊殺（`Kill`） | 是 | **已定**（遷移計畫 §7 U6）：`LifeState` 只有一個寫者，排在每個 tick 的開頭（移動之前）；擊殺不直接寫它：動作裁決發 `DeathEvent`，`LifeState` 的寫者讀事件後寫；同一個 tick 的快照與 digest 組裝排在動作裁決之後讀（閘門 G1 的謂詞）。不設中間狀態欄。今天的時序（死亡在 T=35 進快照、重生在 R=215，R−T=180）與 golden 不變，由 match-sim 在新路徑上實測 |
| 輸入游標 | lastResolvedCommand、movementEpoch、contiguousPendingCommands | 移動、Tick、`ResetMovementEpoch` | 是（digest 的 `player` 列裡與身體欄位穿插） | 一個欄。digest 的 `player` 列欄序不變（E-9）：編碼器從兩個欄重組出原來的順序 |
| host 診斷 | movementSlack*、connectionQualityFailures | Match host：`movementSlack*` 由 `TrackSlack` 寫，`connectionQualityFailures` 由 `JudgeConnectionQuality` 寫；兩者今天都寫進發布用的快照複本，不寫 `bodies_`（Match 自己的 `Snapshot()` 裡恆為 0） | `connectionQualityFailures` 進；`movementSlack*` 不進 | 一個欄。`connectionQualityFailures` 的唯一寫者是 (c) 的連線品質判定 System（遷移計畫 §5 第 18 列） |

其他四個欄的分類照 legacy 檔 §3.2 的表。其中 `combats_`（生死邊界重設血量與彈匣、動作裁決與擊殺）與 `connections_`（移動、移動世代重設、host 的重新對時請求）今天也各有兩段以上的程式寫，拆成 System 後同樣會撞上「至多一個寫者」；`animations_`、`actionLedgers_` 各一段。它們在 Kernel 上的拆法、欄名、區與寫入政策由 `v8-product-pvp-plan-dryrun` 宣告並以啟動檢查驗證（閘門 G1），本檔不先定。

實體的組成（`target/runtime-composition.zh-Hant.md` §3a）：Player 實體用 Gateway 配的玩家段 id，帶上表四個欄與戰鬥、動畫；Peer 實體（每個收件者的複寫帳本與可見性）是另一個實體，不掛在 Player 上。

## 2. legacy 條文的目標措辭

legacy 檔的 `Design Invariants`（E-1～E-12）與 Replacement Criteria 在 `v8-arch-a6-revision` 逐條重標（`docs/architecture/plans/kernel/a6-revision-report.zh-Hant.md`）。結果：**沒有一條被取代**；四條是「相容（措辭更新）」——性質繼續約束 PvP，只有機制換了；其餘不變。

| 條 | legacy 的寫法裡要換的部分 | 目標措辭 |
|---|---|---|
| E-3 | 「Reset 歸回 First」「不需要 generation」 | Match 段單調、不回收、用盡報錯不回繞——這是 PvP 配發器（產品登記的 Resource）的策略。世代切換＝Host 重建 World，World 內沒有歸零的操作。鍵的格式含 generation 位元，PvP 恆為 0，所以 id 的值與今天逐位元相同。Client 沒有配 id 的程式路徑；本地預測用（自己的 id, ActionId）當暫時鍵。 |
| E-7 | 「刪實體只走 `PvpMatch::EraseEntity`」 | 刪實體只走 World 的 `Despawn`：同步點把那個實體從所有欄同時刪掉，沒有孤兒元件。World 以外按實體 id 存的東西（輸入來源、快取、歷史）仍由產品負責在同一次刪除裡清掉；做法（OnRemove hook 或同一個 System）由 match-sim 的宣告定。快照列的刪除與發布前的檢查由 match-host 的宣告定。 |
| E-12 | 「記入待辦，在固定階段套用」「`GYO_ASSERT(!inTick_)`」 | Tick 內不改任何欄的成員；生成與刪除只進延後佇列，在同步點依固定階段、每階段依 EntityId 升冪套用——這一半現在由 Kernel 保證（`docs/architecture/target/kernel.zh-Hant.md` DI-KS 9），不再是產品自己的守衛。新 id 依裁決順序配發——這一半仍是產品的：配發器是 (c) 的 Resource，配發順序是執行計畫的函數。 |
| RC-2 | 「引入 generation 位元」 | 需要回收 id（一個世代內實際用盡，或需要跨世代保留 id）→ E-3 不成立，停下交使用者。處置不再是改鍵格式（格式已含 generation），而是改配發策略；代價是兩端配發歷史不同時，數值順序不再是契約鍵的順序，PvP 要自己排序，digest 是否編碼 generation 也要一起定。 |

不變的條文——E-1、E-2、E-4、E-5、E-6、E-8、E-9、E-10、E-11，RC-1、RC-3、RC-4、RC-4b、RC-5——遷移期間唯一的文字在 legacy 檔，本檔不重抄；搬回時原樣併入。其中 E-5（欄依 id 升冪）的機制部分由 Kernel 守（spec J-1、J-2），產品仍守「成員關係恰等於種類表」；守衛怎麼改寫列在遷移計畫 §5.2 第 18 列，由 match-sim 的事前宣告逐一判。

## Design Invariants

上表 E-3、E-7、E-12 的目標措辭是遷移後新程式的條文，條號沿用 E-n。正式：使用者 2026-11-13 核准（kernel HANDOFF `v8-arch-target-di-approved`）。本節的變更是 Architecture Delta（AGENTS §3），要使用者同意。守衛（G1、G2、G3、G7、G11、G13）對應的條文由 `v8-product-match-sim` 與 `v8-product-match-host` 寫入本節（遷移計畫 §5.2 第 18 列）；寫入前本節只有上表三條。

### Replacement Criteria

RC-2 的目標措辭見上表；其餘沿用 legacy 檔，搬回時併入。
