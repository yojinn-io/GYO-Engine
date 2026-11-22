# 時間 Module＋事件（`v8-arch-gyo2-time-events`）

日期：2026-11-19～2026-11-20。分支 `claude/gyo2-time-events-8fe1bf`（從 `origin/gyo2`；第 3 步開始前 rebase 到 #249 之後）。**狀態：已合併（#253，merge 59fd4da9，2026-11-20），L1 三平台通過。**（第 1～7 步完成，合併前核准 2026-11-20；本行由第 4 批 `v8-arch-gyo2-authority-rollback` 的第一個提交更正。）

這一批由任務卡做（GYO2 主會話開出）。事前宣告：[time-events-declaration.zh-Hant.md](../architecture/target/v2/time-events-declaration.zh-Hant.md) 修訂 4；使用者 2026-11-19「照建議，全部核准」（動工前核准，`v8-arch-gyo2-time-events-declaration-approved`）。修訂 0 送 Fable／high 敵對式檢查（報告 `declaration-r0-adversarial-check.md`）。實作者 Opus／high；審查、§14.4 檢證、新突變清單的敵對式檢查都是 Fable／high（`v8-arch-gyo2-batch3-no-xhigh`：本批不用 xhigh）。

用詞：「第 N 步」＝宣告第 12 節的步（E1～E9 的分組）。scenario 的「第 2 段」是 scenario 的段名（ctest `scenario.segment2`）。

## 做了什麼

| 步 | 提交 | 內容 |
|---|---|---|
| 第 1 步 延後的守衛 | a31711df | 第 2 批延後的綁實作守衛 6 條立起來：`naming_guard.py` 的 `ROOTS` 加 `engine/orchestration`、新 `tests/common/orchestration/orchestration_guard.py`（O3、O4、O6、O0 fail closed）、Kernel 的 K9（搬出去的名字不回來）、`gyo_expect_links` 的編排層連結檢查；K1 的 `PRODUCT_ALIASES` 拿掉（和 #249 同一處，rebase 時取 #249 的版本） |
| 第 2 步 時間的型別 | 54c21e6b | `engine/world_time`（`GYO::Engine::WorldTime`）：`RealTime`、`VirtualTime`、`FixedTime`、`ActiveTime`、`Tick`、`TimeControl`、`TimeLimits`、`TickAccumulator`（整數奈秒、沒有 padding 的 `static_assert`）、`AdvanceVirtual`、`StepLength::FromHz`（四捨五入到奈秒一次）。測試先行 `world_time.types` |
| 第 3 步 Message 與游標 | 5f071177、f6b08661、8d633c5c | 保留式事件改名 `Message`（`MessageReader`／`MessageWriter`／`MessageOptions`）；每個讀者一個游標（mount × 訊息型別），寫入序號 64 位元；上限預設 1024、頂滿丟最舊、聚合的 `DroppedMessages`（`TakeDiagnostics`）；修剪在最外層 Run 結束、清空輪之後，巢狀與出錯時不修剪；拿掉 `EventTrim` |
| 第 4 步 拿掉時鐘、骨架、時間 Module | d44ad582、9327f770、23082f7c、ce260c3b、44d83408 | Kernel 拿掉 `Domain`、`CurrentTick`、`DomainTick`、`tickStamps`、D-E12、F-E1、F-9；診斷、`RejectedSpawn`、`WorldFailure` 改帶同步點序號（`World::SyncEpoch()`）；測試的閘門改成外面數的 Resource。編排層宣告預設骨架 `Main`（First、PreUpdate、FixedMainLoop、Update、PostUpdate、Last）與 `FixedUpdate`（五個 stage），`ScheduleSkeleton::Off` 可關。時間 Module `WorldTimeModule`（`AdvanceTime` 在 Main/First、`RunFixedSteps` 在 Main/FixedMainLoop，`ExclusiveWorld<Writes<Tick,FixedTime,ActiveTime>>`）；`modules.csv` 加 `world_time`（預設建進 World） |
| 第 5 步 推送式事件 | b72835fa、36374375、56b18b19、f4b21270 | `Event`、`Observer`、`Trigger`、`On<E>`、元件的 `Add<T>`／`Remove<T>`；Commands 的觸發在同步點最後一步；`ExclusiveWorld::Trigger` 當場；Host 的 `WorldExecutor::Trigger` 當場、用暫存佇列清空、上限是 `hookRoundLimit`；同一觸發點 observer 依名字排序（名字重複＝R-E2），不對作者承諾先後 |
| 第 6 步 scenario、預算、文件 | 353e3cd4、d848c72f、7a03621d、85bf96ba、52e46f54 | scenario 第 2 段 S2-1～S2-8（S2-9＝第 1 段照過）；判定 5 的量測程式 `gyo_world_time_budget`（不進 ctest）；Design Invariants 與架構文件（下述）；`WorldTimeOptions::fixedStep`（false 時不掛 `RunFixedSteps`，一幀上限 `maxFrameDelta`） |
| 第 7 步 審查與突變檢查後的修正 | e46cf182、58980d92、036902ad、e397c449、44a54a43 | M1、S1／S4／S7（下述）；檢出缺口的測試與突變；守衛的陽性對照與 fail closed；清單的合併、退役、expect 收緊；DI 文字 |

指紋版本（golden 在本機 macOS x86_64 重產）：Plan 6→8、Registry 4→7、C 8→11、匯出 `gyo.kernel.plans` 4→6、World 摘要格式 1→2。

## 改的 Design Invariants（21 條）

- `target/kernel.zh-Hant.md`：DI-KS 1、8、9、10（改寫成 Message、Event 兩段，含「範圍的限界」）、11、12、14、15、25；RC 19～22（游標宣告第 9 節四條）。
- `target/v2/orchestration.zh-Hant.md`：O-1、O-3、O-4、O-6、O-11。
- `target/v2/world-time.zh-Hant.md`（新頁）：型別 1～3、WT-1～WT-6、RC (a)～(e)。
- `target/determinism.md` DI 2、`target/world.zh-Hant.md` DI 6 與最後一條 RC：宣告第 10 節沒列，是 DI-KS 8、10 的直接後果，合併前核准一併看。

## 驗證與經過

- 每一步：`ctest --preset test -L "cpu|shader"` 全過（第 3 步 116 項 → 第 7 步 123 項），三支守衛 `--root .` clean，相關突變（D184）全殺。各步的 log 在證據目錄的 `step1`～`step7`。
- 第 7 步之後的獨立驗證（本卡，44a54a43）：建置 0 warning、`ctest -L "cpu|shader"` 123/123、三支守衛 clean（`step7/verify-build.log`、`verify-ctest.log`）。
- 測試先行：第 2 步嚴格先行。第 4 步的時間 Module、第 5 步的 observer 是先寫實作，再把實作換成空殼跑出 Red（第 4 步 16 失敗、第 5 步 20 案 14 失敗），換回後 Green，不是嚴格的測試先行。第 6 步 C、第 7 步的修正先 Red 再 Green。
- 判定 2：scenario 第 2 段 S2-1～S2-8 第一次就全過（251 個斷言，沒有改 Engine）；S2-8 的檢查內容換了（下面「交合併前核准」第 2 項）。
- 判定 4：九個樣式與 `PRODUCT_ALIASES` 零命中；`\bEventOptions\b` 命中 4 處（下面第 1 項）。正例 `MessageOptions` 命中，掃描有效。
- 判定 5（本機 i9-9980HK、macOS x86_64、Release，10,000 幀）：時間 Module 每幀額外成本 mean／p99：0 步 0.63／0.73 µs、1 步 2.54／9.72 µs、5 步 9.98／32.0 µs，門檻 156 µs。訊息緩衝 100 種 × 1024 筆：4,100,800 B，計算值的 1.0012 倍（門檻 2）。`accessVerification` 開著時另有快照，合計約計算值的 2.001 倍；判定只算緩衝，這一點交合併前核准看（第 3 項）。
- 第 7 步審查（Fable／high，`step7/review-and-14-4-fable.md`）：必改 3、建議改 7、小問題 12；沒有發現 Architecture Drift 與未定義行為。§14.4 G1～G8 八組的描述都屬實；G4、G5 的 tickStamps、G6 的 domain 與 tick 沒有接手者，宣告已寫明。
  - M1：`roundObserverRecorders_` 在 Run 的清空輪與當場改的清空輪只加不清（100 次 Run 長到 100），上限訊息也不列 observer。改成每一輪和 hook 的記錄一起清，三處 `HookRoundLimit` 訊息都列最後一輪的 hook 與 observer；當場改的訊息從 "recorded by 'X'" 變成 "recorded by hook 'X'"（只緊不緩）。
  - S1＋S4：存檔的餘數 ≥ 這次的步長時原本斷言中止。改成留 `餘數 % 新步長`，多的整步記進 `VirtualTime.discarded`；替換時帶過去的 `FixedTime` 也換成這次的步長。
  - S7：倍率 -0.0 寫成 +0.0（摘要照位元組雜湊）。
  - S2、S5 寫進 DI；S3、S6 寫成 DI-KS 10 的限界（下述）；`SkeletonTests.cpp` 的恆真 `CHECK(rig->ticks == 8)` 刪掉（commit 寫了理由）；`world.zh-Hant.md` §2「作者自己記下 tick 開始時的 stamp」現在做不到，改成只說用 `Changed<…>`。
- 第 7 步新突變清單的敵對式檢查（Fable／high，`step7/mutants-adversarial-fable.md`）：檢出缺口 42（高 14）、缺突變的決定 40（高 13）、守衛缺陽性對照與 fail closed。處理：高的全部補、中低的補 DI 有寫的；不補的逐項理由在 `step7/fix-report.md`。
- WT-3 的放回時點：放回在準備時機做；給獨占 System 的多載只能放在這一幀 `RunFixedSteps` 之後、下一幀 `AdvanceTime` 之前，夾在兩者之間是 Programmer Error。多載保留（已核准的 WT-3 准許獨占 System 放回）。

## 突變

對 `origin/gyo2` 的淨數：

| 清單 | 新增 | 退役 |
|---|---|---|
| Kernel `docs/architecture/plans/kernel/scripts/mutations.json`（655 → 737） | 94 | 12 |
| 編排層 `docs/architecture/target/v2/scripts/orchestration_mutations.json`（199 → 227） | 30 | 2 |
| `world_time` `docs/architecture/target/v2/scripts/world_time_mutations.json`（新） | 92 | 0 |
| 合計 | **216** | **14** |

- 上限（`tests/common/mutation/README.md`）：改的 DI 21 條 × 5＝105，超過 111。理由：
  - 三條觸發路（Commands、`ExclusiveWorld`、Host）各有上限來源、錯誤處理、stamp 的決定點（第 5 步 obs- 31）。
  - `world_time` 是新 Module，型別、取樣三種、固定步、放回與帶過去各自有決定點（92 個，第 7 步補了敵對式檢查的「高」與 DI 寫明的「中低」）。
  - 第 7 步新增 77（kernel 37、world_time 28、orchestration 12）：敵對式檢查的高 14＋13 全部、守衛 9、M1／S1／S4／S7 共 11，其餘是 DI 寫明的中低。
- 退役 14：G1～G6 的緩的面裡的突變（`rs-j37-event-domain-unchecked`、`rs-j37-nonclock-allowed`、`rs-j24-de12-removed`、`or-gi-domain-unchecked`、`rs-adv-domain-tick-is-zone-b`、`rs-j12-tick-one-mount`、`we-adv-tickstamp-never-recorded`、`ns-nested-tick-not-advanced`、`or-failure-no-domain`），以及修剪步與「依 tick 讀」消失的 5 個（`rs-adv-event-trim-unknown`、`rs-b3-trim-not-pinned`、`hf-rounds-follow-trim`、`we-j36-no-same-tick`、`we-j36-swap-compare`）。批內加了又退的 2 個：`msg-end-trim-dropped`（併進 `we-adv-event-trim-never`）、`wt-not-driven-dropped`（等於兩個既有突變之和）。
- 每個新突變的 `what` 寫了「它抓、別的抓不到的錯」：第 7 步新增的 77 個、第 2～6 步的 obs- 31、`world_time` 第 6 步 C 的 7 個先寫了（`step5/obs-mutants-reasons.txt`）；其餘 96 個在合併前核准後補上（使用者 2026-11-20 定的第 4 項，Opus/high 子 agent 寫、卡片檢查）。補的時候沒有找到可以退的：子 agent 原本退了 `msg-trim-on-err`（理由：`hf-trim-before-rounds` 打到同一個斷言），卡片放回——兩者是不同的改動（第 7 步敵對式檢查 2.5 已判），`msg-trim-on-err` 獨有的錯（Err 的路徑本身修剪）只有 `[ns-throw-event-written]` 抓得到；照 README「合わせ方」把 J-56 那個案例併進它的 `cases` 與 `expect`（兩個都失敗才算），單獨跑 1/1 killed（`step7/merge-approval-msg-trim-on-err.log`）。
- `guards`（合併前核准新增的欄位，`tests/common/mutation/README.md` 第 9 條）：216 個新突變全部加上，42 種條號，逐一對過文件裡的寫法。勉強對應的 4 處：`sc-s2-real-time-carried` 只掛宣告 3.5 與判定 2（「不帶過去」不在 DI 裡）；`obs-unregistered-silent`、`obs-trigger-no-own-stamp` 掛 kernel DI-KS 10（條文沒明寫）；`wt-rewind-past-tick-allowed`、`wt-schedule-name-length-unchecked`、`wt-add-no-limit-assert` 守的斷言條文在 world-time 第 2 節，掛在 WT-3／WT-1／型別 1。舊的突變碰到時再補（使用者 2026-11-20）。
- scenario 第 2 段：候選都和既有單元突變是同一個改動、只是目標執行檔不同（`cases` 不能合併），照 README 第 1 條不新立；只加 `sc-s2-real-time-carried`（沒有既有突變改時間 Module 的帶過去）。
- 完整跑（判定 3）：在 44a54a43 的副本上跑（2026-11-20 06:02～09:25 JST），**1056/1056 全殺**，沒有存活、wrong_failure、逾時：Kernel 737/737、編排層 227/227、`world_time` 92/92。52e46f54 那一次跑到 Kernel 469/701、全殺時停掉（第 7 步改了清單，結果不再代表最終狀態；log 保留在 `step7/full-52e46f54-partial/`）。

## 範圍的限界

- 合併前核准時 L1 三平台還沒跑（之後在 PR #253 上通過）；golden 與預算只在本機 macOS x86_64 量過。`gyo_world_time_budget` 在 MSVC 只在 L1 編譯時驗證。
- 訊息緩衝頂滿後，每寫一筆用 `erase(begin)` 搬整個緩衝；每丟一筆掃全部游標、重組讀者名單。`DroppedMessages` 每次 Run 多一筆，交換點做出來之前沒人取走；判定 5 的記憶體量測不含這些紀錄。改法等量到成本再做。
- observer 的區規則只有 `CheckZoneAccess`（突變 `obs-zone-access-unchecked`）與 S-7 有守衛；S-9 與 Z6 沒有，區規則在第 4 批拿掉。
- G4（誰都能寫 `Tick`）在第 4 批權威接手之前沒有守衛；G5 的 tickStamps、G6 的 domain 與 tick 沒有接手者。
- 沒改的過期文字（不在本批清單內，交圖書管理員）：`target/README` K.3 對照表第 2、11、15 列、`target/runtime-composition.zh-Hant.md:33`、`target/system-contract.zh-Hant.md:57`、`target/README` DI-KS 15 摘要。

## 合併前核准（使用者 2026-11-20，side chat「btw: 一件件來」轉來；HANDOFF 由主會話記）

1. 判定 4 的 `\bEventOptions\b`（推送式事件的選項型別 `EventOptions` 命中 4 處）：「照建議」——拿掉這個樣式，名字讓給推送式事件（照 Bevy 0.17 Message＝緩衝、Event＝推送）；舊 API 清掉這件事由同一行其他樣式守。宣告判定 4 已改（038021f5）。
2. S2-8：「照建議」——測試照改（AGENTS §14.4 1～5：`step7/review-and-14-4-fable.md` A.1 M3）；宣告第 3.3、3.5 節與第 7 節 S2-8 改成和 WT-2 一致：讀檔後不帶過去的真實時間，第一幀 `delta` 依取樣方式（038021f5；第 3.5 節是卡片一併改的，同一句矛盾）。
3. 判定 5 的記憶體：「照建議」——只算預設狀態（1.0012 倍，成立）；宣告第 9.2 節補「預算不含 `accessVerification` 的快照；開著時約 2 倍，屬除錯模式的已知代價」（038021f5）。界定判定範圍，不是事後放寬。
4. 突變 216 > 105：「照建議，而且我想加一個寫這個突變的理由，否則後面退役都退不掉。」「對的。」——合併前補完 `what`；新規則：每個突變加 `guards`（AGENTS §14.5 一行、README 第 7～9 條，23e6da62；舊的碰到時再補）。結果見上面「突變」段。
5. DI 21 條：「核准」，照現文。G4、G5 照「範圍的限界」交後續批次。
6. 實作和宣告的差異 8 處：「核准。」照現文（observer 依名字排序、`TimeConfig`、游標清除、手動每幀取樣第一幀就套用、`fixedStep`／`fixedStepConditions`、`modules.csv` 寫全名、`NotDriven` 不扣回 `elapsed`、WT-3）。附帶更正：Bevy 不是沒有上限，是用 `Time<Virtual>::max_delta`＝250 ms 截時間（64 Hz 下間接等於一幀最多 16 步，`bevy_time` `virt.rs:86`、`fixed.rs:243`）；GYO 有固定步時直接限步數（5）。使用者：「有一樣的問題，但是解法不一樣。」

## 證據

主 checkout `build/evidence/gyo2-time-events/`（這台 Mac，git 不追蹤）：
- `recon-a-kernel-facts.md`、`recon-b-orchestration-facts.md`、`recon-c-reference-engines.md`、`declaration-r0-adversarial-check.md`：設計段。
- `step1-*`、`step2-*`、`step3/`～`step6/`：各步的建置、ctest、守衛、Red／Green、突變 log 與回報（`report.md`、`report-a/b/c.md`）、golden 新舊值（`golden-old-new.txt`）、預算（`step6/budget-time.txt`、`budget-memory.txt`）。
- `step7/`：審查與 §14.4 檢證（`review-and-14-4-fable.md`）、新突變清單的敵對式檢查（`mutants-adversarial-fable.md`）、修正的回報與 log（`fix-*`）、完整跑（`full-*`）。
