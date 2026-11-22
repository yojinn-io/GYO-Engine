# 2026-11-22 GYO2：第 5 批 生成世界（重做）＋預設的實體配發器

批次代號：`v8-arch-gyo2-world-gen`。分支 `claude/gyo2-world-gen-77331d`（從 `gyo2` 45f13757；中途 merge #267、#268、#269）。任務卡 `gyo2-world-gen`，主會話「[GYO2][不良少年]新架構重構」開的卡。宣告：[world-gen-declaration.zh-Hant.md](../architecture/target/v2/world-gen-declaration.zh-Hant.md) 修訂 2（使用者 2026-11-22 動工前核准「照建議，全部核准」；同日「開始」）。

## 做了什麼

### 第 1 步：`entity_ids` Module、`RangeAllotter` 搬出 Kernel（5260f06a）

- `engine/modules/entity_ids`：`RangeAllotter`（連同逐欄雜湊）與 `EntityIdsModule`。每個選了它的 World 登記 `RangeAllotter` 為配發器，準備函式在沒有值時放 `EntityIdsOptions` 的範圍（預設 `[1, 2^32−1]`）。登記檔一列 `default=yes`、`depends` 空。
- Kernel 的 `Storage.hpp` 拿掉 `RangeAllotter`（Kernel 不附任何配發器型別，DI-KS 4）。
- 測試：Kernel 與編排層的測試改用共用的替身 `tests/common/support/TestRangeAllotter.hpp`（測試自己的範圍配發器，`GYO::Test`；宣告 5.10 寫放 `WorldTestSupport.hpp`，編排層也要用所以放共用處）；回滾與 scenario 改用 `EntityIds::RangeAllotter`。`RangeAllotter` 自己的性質（Kernel J-8 的前半、D-9 的 padding 案）搬到 `tests/common/entity_ids/`；Kernel 的 J-8 留機制的半（用完回報、範圍內的 `SpawnWithId` 被丟）。新案例 C13～C16（`entity_ids.module`、`entity_ids.default`：C16 用 `gyo_product_units` 由真的 `config/modules.csv` 產生登記單位表）。
- 突變：`we-j8-allotter-wraps`、`we-j8-allotter-reset`、`ks-hash-allotter-as-bytes` 從 Kernel 清單搬到新清單 `docs/architecture/target/v2/scripts/entity_ids_mutations.json`（id 不變，補 `guards`）；新增 `ei-prepare-overwrites`、`ei-not-default`；`sc-lower-bound-not-moved` 的 `file` 改指 Module 的標頭。

### 第 2 步：生成中（c361eb88）

- `engine/orchestration/include/engine/orchestration/WorldGeneration.hpp`、`src/WorldGeneration.cpp`：`WorldGeneration`（`Generating()`、`Hold()` 寫成 `held = generating`）、逐欄雜湊、名字固定的 run 條件 `WhileGenerating()`／`AfterGenerating()`。
- `Product`：每個 World 在任何登記單位的 `Build` 之前登記 `WorldGeneration`（和預設骨架同一次 `Register`，owner `orchestration`）；`MakeExecutor` 建好 World 時放 `{true, false}`（在準備函式之前，啟動與替換共用）；`RunFrame` 改成逐 World 呼叫私有的 `RunRound`（前處理 → 每幀清單 → 判定）；`RunSchedule` 不碰生成的狀態；`Product::Generating(name)` 讀 Resource。
- 案例：`orchestration.O-14`（`WorldGenTests.cpp`：C1～C3、C5～C12、C17、C18）；`world_time.generation`（`WorldGenTimeTests.cpp`：C4 兩種不 tick 的寫法）。
- 突變 `wg-` 6 個（清單 `orchestration_mutations.json`）。宣告列的 `wg-skeleton-off-unregistered` 當時沒有立，理由寫「Kernel 的斷言中止程式，抓不到識別得出的失敗」；設計審查指出清單已有先例（`g8-frame-error-ignored`：Kernel 斷言文字＋限定案例），照先例補立（見「設計審查」），和宣告不再有偏差。

### merge #269（fb92b179）

#269 把測試補助的 namespace 改成 `GYO::Test::<單位>`、`gyo_expect_links` 移到 `tests/common/support/ExpectLinks.cmake`。衝突兩處（`SkeletonTests.cpp`、`InitialValueTests.cpp` 的 `using` 行），取 #269 的寫法再加回替身的 `using`；另有 #269 帶回的 `RangeAllotter` 字樣一併換成替身。

### 第 3 步：scenario 第 6 段

`tests/common/scenario/ScenarioSegment6Tests.cpp`：走路的小遊戲用 `entity_ids` 與 `world_time` 兩個 Module。S6-1 新遊戲在準備函式暫停時間，地圖分 3 幀生成（讀取畫面的計數 3、`Tick` 0、遊戲初始化 1 次）→ S6-2 解除暫停後走 5 步、計分 50 → S6-3 存檔 → S6-4 換 World 讀檔（`extra` 放存檔與配發器下界），重新生成地圖 3 幀、第 3 幀照存檔編號放回主角 → S6-5 再走 5 步，位置與分數接得上。第 6 段不另立突變：「替換後重跑遊戲初始化」已有 `sc-replace-reruns-gameinit`，「生成期間 tick 前進」已由 `wg-ends-while-held` 抓（§14.5）。

### 第 4 步：守衛與文件

- EI-3 的連結檢查（`gyo_expect_links(gyo_entity_ids GYO::Base GYO::Kernel GYO::Orchestration)`），scenario 第 6 段通過之後才立（AGENTS §3）；突變 `ei-links-more`。
- 文件：[orchestration.zh-Hant.md](../architecture/target/v2/orchestration.zh-Hant.md) O-14 新增、O-2／O-3／O-4／O-8 補句、RC-O14～RC-O19；[kernel.zh-Hant.md](../architecture/target/kernel.zh-Hant.md) DI-KS 4、DI-KS 15 第 (7) 項改字；新頁 [entity-ids.zh-Hant.md](../architecture/target/v2/entity-ids.zh-Hant.md)；[system-contract-page](../architecture/target/v2/system-contract-page.zh-Hant.md) 第 13 節補句、第 14 節新增；[world-time.zh-Hant.md](../architecture/target/v2/world-time.zh-Hant.md) 交界表一列；[架構 README](../architecture/README.md) Module 清單。都標「最終文字在合併前核准定稿」。

## 驗證與經過

- 本機 macOS（Intel，RelWithDebInfo，preset `test`）：第 1 步後 `ctest -L "cpu|shader"` 164/164；merge #269 之後 166/166。第 3、4 步與 D-9 的修正之後 167/167（多出的一個是 `scenario.segment6`）。
- 第一次基準建置和我同時改 `Storage.hpp` 撞在一起而失敗（建置中途讀到改了一半的標頭），不算數；之後每一步改完才建置。
- 突變（只跑相關的，D184）：
  - `entity_ids` 清單 6/6 killed（含 `ei-links-more`）。第一次跑 `ks-hash-allotter-as-bytes` 存活、`ei-not-default` 建置失敗，原因都在測試：新案例的 World 已有 Module 放的值，`SetResource` 是賦值、padding 不一定跟著複製（改成先 `RemoveResource` 再放）；預設帶的測試多 include 了 `RangeAllotter.hpp`（拿掉）。改後 killed。
  - `wg-` 6/6 killed。第一次跑 `wg-held-not-cleared`、`wg-generating-not-cleared` 逾時：C3 寫成「跑到生成結束」的無限迴圈，突變讓生成永不結束時卡住；改成最多 5 幀並 `REQUIRE` 結束。
  - find 被本批改到而重寫的既有突變 5 個（`g8-frame-list-reversed`、`g8-frame-error-ignored`、`or-skeleton-not-declared`、`or-skeleton-off-ignored`、`or-skeleton-after-units`），連同 find 落在改動範圍的 `or-runframe-no-gameinit`、`sc-lower-bound-not-moved`、`or-guard-real-o4-module-include`、`or-guard-real-o3-plan-read`、`rb-r3-links-more`：10/10 killed。
  - Kernel 清單裡靠配發器行為的 23 個（`what` 或 find 提到 allotter、或 cases 是 J-8）：第一次 22/23，`ks-hash-summary-resource-bytes` 存活。它守 Kernel 自己的性質（World 摘要對 Resource 呼叫逐型別雜湊，DI-KS 27），原本由 Kernel 的 D-9 padding 案例抓；第 1 步把那個案例整個搬去 `entity_ids`，Kernel 就沒有案例抓它了（搬過頭）。改成 **D-9 案例在 Kernel 留一份**（用替身配發器，守 Kernel 的摘要路徑），`entity_ids` 那份守 `RangeAllotter` 自己的雜湊函式；之後 killed，23/23。和宣告第 10 節第 3 列（「D-9 搬到 entity_ids」）的差異：留一份、另加一份，不是搬走，沒有緩的面。
- 加的突變（§14.5 第 8 點）：`ei-prepare-overwrites`、`ei-not-default`、`ei-links-more`、`wg-` 6 個，共 9 個；每個都寫了它獨抓的誤り與 `guards`。搬家 3 個（id 不變）。
- 證據：本機的突變與 ctest 記錄在主 checkout `build/evidence/gyo2-world-gen/impl/`（git 不追蹤，這台 macOS Intel 機器）；別家對照與宣告的兩次檢查報告在主 checkout `build/evidence/gyo2-world-gen/`。

## 設計審查（Fable 5.2／high，唯讀，HEAD cdfc304b）

報告：主 checkout `build/evidence/gyo2-world-gen/impl/design-review-fable.md`。總評：生成的狀態只有一個來源；配發器是「Kernel 一個公開出口＋Module 注入實現」；沒有需要推翻設計的問題。建議改 6 條，全部照改：

| # | 指出 | 處理 |
|---|---|---|
| A1 | `RangeAllotter.hpp`（與測試替身）直接 include `entity_store`，靠 Kernel 的 PUBLIC link 傳遞，EI-3 的連結守衛看不到 | 改用 `Kernel::EntityId`（include Kernel 的公開標頭），EI-3 寫明 |
| A2 | Kernel 的註解點名上層 Module；concept 的說明用「範圍、不回繞」描述所有配發器 | 註解改成中立 |
| A3 | O-14 寫進私有函式名、一行實作、作者的做法 | O-14 只留編排層保證的事，作者的做法指說明頁第 14 節；EI-3 守衛欄改成已立 |
| A4 | `wg-skeleton-off-unregistered` 不立的理由站不住 | 照先例立起來（cases 限 C11，expect 是 Kernel `ResourceIdOf` 的斷言文字）；見下面的突變結果 |
| A5 | 和 PR #270 合併時，最省事的解衝突會把「進」放到前處理之前，只有 C19 抓得到 | `RunRound` 裡加註解標出進／出的位置；未結事項列 C19 |
| A6 | 讀檔移下界的範例把預設上限抄成字面值 | scenario 第 6 段與說明頁改成讀現有值的 `last` |

改後：受影響的突變（`wg-skeleton-off-unregistered`、`wg-held-not-cleared`、`wg-ends-while-held`、`entity_ids` 清單 6 個）9/9 killed（主 checkout `build/evidence/gyo2-world-gen/impl/mut-review.log`）；`ctest -L "cpu|shader"` 167/167。

另順手改的（記下的那一段）：B1 測試替身的註解不再宣稱「和 `RangeAllotter` 同語意」；B6 `RunRound` 的回傳型別照 N13 寫 `Base::Result`。其餘記下的不改，交之後的批（主會話轉）：B3 作者寫 `WorldGeneration` 就能結束或重新進入生成，`Product::Regenerate` 不必做（狀態機那一批問「World 內換關卡」時的答案）；B6 `RunRound` 的 entry 寫死 `"RunFrame"`（交換點的入口共用時加參數）、`kSkeletonOwner`／`own0` 名字已過時（交主會話判）；B7 預設都帶 `entity_ids` 與 `rollback` 時配發器沒有登記成可回滾，回滾可向 Kernel 問哪個 Resource 是配發器；B8 預設範圍佔滿 32 位元 index，網路複製的外來編號也要先動範圍；B9「條件擋住的讀者拉住保留」是事件游標契約的問題，狀態機那一批提；B10 只用 `RunSchedule` 推的 runner 推不動生成；B11 C16 以「全預設 Module 不給設定就能建 World」為前提；B12 換自訂配發器要把其他單位全列出來；B13 `Hold()` 要 `ResMut`，並行時會串行化。

## §14.4 的檢證（Fable 5.2／high，唯讀，對象 7fad36c6）

報告：主 checkout `build/evidence/gyo2-world-gen/impl/verification-14-4-fable.md`。沒有被丟掉而沒交代的斷言、搬過去的突變沒有變弱。指出並處理的：
- 緩的面少寫一個：Kernel 與編排層的機制測試改用替身後，「用完時 Kernel 報 `AllotterExhausted`」「範圍內的 `SpawnWithId` 被丟」沒有用真 `RangeAllotter` 的案例。補 `entity_ids.module`「with the default allotter, an id in its range is dropped and a used-up range is reported」。
- 突變的記錄不全或是舊程式的（`ks-hash-summary-resource-bytes` 修好後的重跑沒存、編排層 9 個在 7fad36c6 之前跑、`rb-r3-links-more` 沒存）；Kernel 的選法照所寫的條件應是 25 個（漏 `sr-no-record`、`sr-no-count`）。在目前的程式上重跑並存檔：主 checkout `build/evidence/gyo2-world-gen/impl/mut-head-kernel.log` 25/25、`mut-head-orchestration.log` 9/9、`mut-head-rollback.log` 1/1 killed。
- DI-KS 27 的條文仍寫「Kernel 自己的型別（配發器、…）」：改字，列進合併前核准。
- 測試替身的位置和宣告 5.10 不同、`HashTests.cpp` 的 `static_assert` 換成替身沒交代：寫進合併前核准頁。

## 合併前（未完成）

- CI L1 三平台（PR 上）。
- C19（交換函式的進出包住生成中的一輪）：和 PR #270 誰後合併誰補；合併時進、出放進 `RunRound`，「進」在前處理之後、「出」在判定之後（設計審查 A5）。
- §14.4：第 10 節的 DI 守衛搬家（J-8 兩個突變、`ks-hash-allotter-as-bytes`、D-9 的 padding 案）請使用者核准；檢證者 Fable／high。
- 合併前核准：第 7 節的最終文字。
