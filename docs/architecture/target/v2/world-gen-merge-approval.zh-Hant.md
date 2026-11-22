# 合併前核准：生成世界＋預設的實體配發器（`v8-arch-gyo2-world-gen`）

更新：2026-11-22。**狀態：合併前核准（使用者 2026-11-22，原話「照建議，全部核准」）。合併要使用者另外說「合併」。**PR [yojinn-io/GYO-Engine#272](https://github.com/yojinn-io/GYO-Engine/pull/272)。宣告：[world-gen-declaration.zh-Hant.md](world-gen-declaration.zh-Hant.md) 修訂 2（2026-11-22 動工前核准）。dev_log：[2026_11_22_gyo2_world_gen.zh-Hant.md](../../../dev_logs/2026_11_22_gyo2_world_gen.zh-Hant.md)。

> **核准**（使用者，2026-11-22 約 20:17 JST，本卡的對話）：原話「照建議，全部核准」。核准的版本是本頁 f6eaec1f：第 1 節的守衛搬家（四組，含檢證者補出、已用案例收掉的第二個緩的面）、第 2 節和宣告的偏差（D-9 留一份另加一份、測試替身的位置）、第 3 節 Design Invariants 的最終文字（O-14 等、DI-KS 4／15 (7)／27、EI-1～EI-3）。未結事項 C19 照第 4.1 節，和 PR #270 誰後合併誰補。

**這一頁請你核准三件**：第 1 節 Design Invariants 守衛的搬家（AGENTS §14.4：DI 的守衛不論有沒有緩的面都要你核准）；第 2 節和宣告的偏差；第 3 節 Design Invariants 的最終文字。設計審查的結論在第 4 節，CI 的結果在第 5 節。

## 1. Design Invariants 守衛的搬家（§14.4 的 1～5）

### 1.1 Kernel J-8 前半與兩個突變搬到 `entity_ids`

`RangeAllotter` 自己的性質（單調、用完回 `nullopt`、不回繞、`Owns`、沒有 `Reset`）原本寫在 Kernel 的 J-8 案例前半，由突變 `we-j8-allotter-wraps`、`we-j8-allotter-reset` 守。型別搬到 `entity_ids` 之後，這些斷言與兩個突變跟著搬到 `tests/common/entity_ids/EntityIdsTests.cpp`「allotter: monotonic, exhaustion reported, never wraps; no Reset」與 `entity_ids_mutations.json`。Kernel 的 J-8 留下機制的半（用完時 Kernel 回 `AllotterExhausted`、範圍內的 `SpawnWithId` 被丟並記 `IdInAllotterRange`、沒有配發器就不能 `Spawn`），改用測試替身。

1. **守的性質**：預設的配發策略單調、不回繞、不能歸零（Kernel DI-KS 4 的〔併自 world DI 2〕，現在是 entity_ids 的 EI-2）。
2. **緩的面**（兩個；檢證者補出第二個）：
   - Kernel 的測試不再檢查任何配發器型別自己的行為——Kernel 已經不附配發器型別，所以 Kernel 那一側沒有東西可檢查。若有人在 Kernel 裡再放一個配發器型別，Kernel 的測試不會替它檢查單調與不回繞。
   - Kernel 與編排層的機制測試改用測試自己的範圍配發器（`tests/common/support/TestRangeAllotter.hpp`），不再用出貨的 `RangeAllotter` 跑。「用完時 Kernel 報 `AllotterExhausted`」與「範圍內的 `SpawnWithId` 被丟」原本沒有用真型別的案例；**已補一案**：`entity_ids.module`「with the default allotter, an id in its range is dropped and a used-up range is reported」（Product＋Module 的 World），所以這一個緩的面收掉了。替身目前和 `RangeAllotter` 逐行同邏輯，但沒有東西保證以後一致；它守的是 Kernel 的機制，不是預設策略。
3. **締的面**：沒有。
4. **性質由誰繼續守**：`entity_ids.allotter` 的同一組斷言（逐字搬過去，15 個判定）；`HashTests.cpp` 的 `static_assert(Hashable<RangeAllotter>)` 改成對替身，真型別可雜湊由 `entity_ids.allotter` 的 padding 案（`Summary` 成功）間接守；`we-j8-allotter-wraps`、`we-j8-allotter-reset` 改成改 `engine/modules/entity_ids/include/engine/entity_ids/RangeAllotter.hpp`，由那個案例在預期的訊息下抓到（killed）。
5. **根據與證據**：宣告第 10 節第 1 列、5.10；commit 5260f06a；突變記錄 主 checkout `build/evidence/gyo2-world-gen/impl/mut-ei.log`（第 1 步，5/5）與 `mut-review.log`（設計審查改完之後，6/6；和目前的程式相符的是這一份）。

### 1.2 `ks-hash-allotter-as-bytes` 搬到 `entity_ids`

1. **守的性質**：預設配發器的雜湊函式逐欄、padding 不進 World 摘要（DI-KS 27 對這個型別的部分）。
2. **緩的面**：沒有——雜湊函式跟著型別搬；突變改的是同一個函式（只換了檔案與 namespace），由 `entity_ids.allotter`「equal allotters give one World summary」抓。
3. **締的面**：沒有。
4. **性質由誰繼續守**：上述案例與突變（killed）；Kernel 這一側「摘要對 Resource 呼叫逐型別雜湊」由 Kernel 的 D-9 案例與 `ks-hash-summary-resource-bytes` 守（見 1.3）。
5. **根據與證據**：宣告第 10 節第 2 列；主 checkout `build/evidence/gyo2-world-gen/impl/mut-ei.log`（第 1 步）與 `mut-review.log`（目前的程式，killed）。另：DI-KS 27 的條文原本寫「Kernel 自己的型別（配發器、…）在 Kernel 裡寫好」，已改字（第 3 節）。

### 1.3 Kernel 的 D-9 padding 案例：留一份、另加一份（和宣告不同）

宣告第 10 節第 3 列寫「搬到 entity_ids」。實作時發現 Kernel 的突變 `ks-hash-summary-resource-bytes`（World 摘要改成照 Resource 的原始位元組雜湊）只靠這個案例抓；整個搬走後它存活。所以改成：**Kernel 留一份**（用測試替身配發器，守 Kernel 的摘要路徑），`entity_ids` 另一份（守 `RangeAllotter` 自己的雜湊函式）。

1. **守的性質**：相等的值不論 padding 裡是什麼，World 摘要相同（D-9、DI-KS 27）。
2. **緩的面**：沒有（Kernel 的案例還在，判定同一句）。
3. **締的面**：`entity_ids` 多一份同樣的檢查，對象是搬過去的型別。
4. —（沒有緩的面）。
5. **根據與證據**：dev_log「驗證與經過」；主 checkout `build/evidence/gyo2-world-gen/impl/mut-d184-kernel.log`（第一次，22/23，`ks-hash-summary-resource-bytes` survived）；留一份之後在目前的程式上重跑：`mut-head-kernel.log`，`ks-hash-summary-resource-bytes` killed。

### 1.4 不屬判定變更、只報告的

- 本批改了程式而 find 對不上的 5 個既有突變（`g8-frame-list-reversed`、`g8-frame-error-ignored`、`or-skeleton-not-declared`、`or-skeleton-off-ignored`、`or-skeleton-after-units`）：照新程式重寫 find／replace，改的性質、案例、預期訊息都不變。連同 find 落在改動範圍的 `or-runframe-no-gameinit`、`sc-lower-bound-not-moved`、`or-guard-real-o4-module-include`、`or-guard-real-o3-plan-read`，在目前的程式上重跑：`mut-head-orchestration.log` 9/9 killed；`rb-r3-links-more`：`mut-head-rollback.log` 1/1 killed。
- `sc-lower-bound-not-moved`（scenario 第一段的受け入れ判定）：`file` 改指 `entity_ids` 的標頭，find、expect 不變；killed。
- Kernel 測試的 `RangeAllotter` 換成測試自己的範圍配發器（見 1.1 的第二個緩的面）。靠配發器行為的 Kernel 突變——`what` 或 find 提到 allotter、或 cases 含 J-8——共 25 個（第一次選的 23 個漏了 `sr-no-record`、`sr-no-count`），在目前的程式上重跑：`mut-head-kernel.log` 25/25 killed。

## 1.5 §14.4 的檢證

檢證者 Fable 5.2／high，唯讀，對象 7fad36c6；報告在主 checkout `build/evidence/gyo2-world-gen/impl/verification-14-4-fable.md`。結論：沒有被丟掉而沒交代的斷言（J-8 前半 15 個判定逐字搬到 entity_ids；其餘 Kernel 測試檔除型別名外沒有差異）；搬過去的三個突變沒有變弱。指出的是：1.1 少寫一個緩的面（已補案例收掉）、`HashTests.cpp` 的 `static_assert` 沒交代、幾處突變的記錄不全或是舊程式的（已在目前的程式上重跑並存檔）、DI-KS 27 的條文沒跟上（已改字，第 3 節）、替身的位置和宣告不同（第 2 節）。上面 1.1～1.4 已照它改。

## 2. 和宣告的偏差

（判定 2 的 `wg-skeleton-off-unregistered` 一度沒有立；設計審查 A4 指出清單已有同型的先例，已補立，不再是偏差。）

| 宣告 | 實作 | 理由 |
|---|---|---|
| 第 10 節第 3 列 D-9 搬到 entity_ids | Kernel 留一份、entity_ids 另加一份 | 1.3 |
| 5.10：測試替身放 `tests/common/kernel/WorldTestSupport.hpp` | 放 `tests/common/support/TestRangeAllotter.hpp` | 編排層的測試也要用，放共用處只有一份；宣告第 14 節第 4 件「改替身」的突變位置跟著是這個檔 |
| 判定 1 C2 的「遊戲初始化的計數仍是 1」 | 用「`probe generating` 只出現一次」數（遊戲初始化的 schedule 裡一個 System 記一行） | 同一個性質；測試用的遊戲沒有獨立的計數 Resource |

## 3. Design Invariants 的最終文字（請你核准）

已照宣告第 7 節寫進各頁，標「最終文字在合併前核准定稿」：
- [orchestration.zh-Hant.md](orchestration.zh-Hant.md)：O-14（新）、O-2、O-3、O-4、O-8 的補句、RC-O14～RC-O19。
- [kernel.zh-Hant.md](../kernel.zh-Hant.md)：DI-KS 4 的補句（Kernel 不附配發器型別；J-8 的分工）、DI-KS 15 第 (7) 項改字、DI-KS 27 改字（Kernel 自己的型別不再含配發器；檢證者指出）。
- [entity-ids.zh-Hant.md](entity-ids.zh-Hant.md)：EI-1～EI-3、RC-EI1～RC-EI4。

## 4. 設計審查（批次表〈每批的設計審查〉）

Fable 5.2／high，唯讀，對象 cdfc304b；報告在主 checkout `build/evidence/gyo2-world-gen/impl/design-review-fable.md`。

**結論**：兩個核心形狀都成立——生成的狀態只有一個來源（`WorldGeneration`，啟動與替換共用建 World 時的一行，`Product::Generating` 讀它）；配發器是「Kernel 一個公開出口（`EntityAllotter` concept）＋Module 注入實現」。編排層沒有認得不該認得的東西，Kernel 與時間 Module 沒有被改。新加的突變各守一個獨立的決定，沒有替誤用加的。沒有需要推翻設計、要你定的設計問題。

**建議改 6 條，全部已改**（細節在 dev_log「設計審查」）：
- 配發器的標頭改經 Kernel 取 `EntityId`，不再繞過連結。
- Kernel 的註解不再點名上層 Module。
- O-14 只留編排層保證的事，作者的做法改指說明頁第 14 節。
- `wg-skeleton-off-unregistered` 照先例補立。
- 交換點合併時「進／出」的位置標在程式註解上，C19 列為未結事項。
- 讀檔範例不再抄寫預設上限。

**記下、交之後的批的**（不在本批改）：
- 作者寫 `WorldGeneration` 就能結束或重新進入生成，所以不必加 `Product::Regenerate`。
- 預設都帶 `entity_ids` 與回滾時，配發器沒有登記成可回滾；回滾可以向 Kernel 問哪個 Resource 是配發器。
- 預設範圍佔滿 32 位元，網路複製的外來編號也要先動範圍。
- 「條件擋住的讀者拉住保留」是事件游標契約的問題，狀態機那一批提。
- 私有常數 `kSkeletonOwner` 的名字已過時，交主會話判。

## 4.1 未結事項

- **C19**（交換函式的進出包住生成中的一輪）：和 PR #270（交換點）誰後合併誰補。合併時進、出放進 `RunRound`：「進」在前處理之後、「出」在判定之後（程式裡已用註解標出位置）。若最省事地寫成「進；`RunRound`；出」，「進」裡的 `Hold()` 會被前處理清掉，現有測試抓不到，只有 C19 抓得到。

## 5. CI

- 7fad36c6（設計審查的修正之後）：L1 三平台 linux-x64、windows-x64、macos-arm64 通過，CI gate 通過（run 45012890881）。
- 91d471df（§14.4 檢證的修正：補一個 `entity_ids` 案例、文件）：L1 執行中，結果出來後補在這裡。
- 本機 macOS Intel：`ctest --preset test -L "cpu|shader"` 167/167（91d471df）。
