# v8-product-pvp-plan-dryrun 事前宣告

更新：2026-11-13。Owner：`object_fps_pvp`（內容歸產品；執行者是架構師會話開的任務卡 01）。批次代號 `v8-product-pvp-plan-dryrun`（PvP 能不能表達成 Kernel 的規則；閘門 G1）。
來源：遷移計畫 [migration-plan.zh-Hant.md](../../../architecture/plans/kernel/migration-plan.zh-Hant.md) §3.6（本批）、§2.1 G1、§3 共通、§7 U6／U11／U14、§8 RC-2／RC-4。出處另見 spec [spec.zh-Hant.md](../../../architecture/plans/kernel/spec.zh-Hant.md) B.1、G.1、H.2b、H.3、H.4.1、H.4.2、H.5、L.3、L.3.1；產品目標文件 [entity-model（target）](../../target/entity-model.zh-Hant.md) §1；legacy [entity-model](../../entity-model.zh-Hant.md) §3.2；Kernel 已實作的 Registry／Plan（`engine/kernel/`，dev_log [registry-schedule](../../../dev_logs/2026_11_13_arch_kernel_registry_schedule.zh-Hant.md)）。
**狀態：草稿，等使用者核准。** 判定與數字在看到任何結果之前寫下，核准後不放寬（AGENTS §14.1）。核准前不寫程式。
分支 `claude/pvp-plan-dryrun`，base origin/master `e1518759`（#212、#213、#214 都在內）。

## 0. 為什麼（Feature → Contract → Architecture）

- **Feature**：跨產品能力「Match 與 Client 預測共用同一份模擬核心」（Product：object_fps_pvp 的 Match 與 Client；玩家可見：預測不因兩端順序不同而回彈）；另一個名指 Feature：Body 拆欄後「獨占寫者」成立（使用者 K.3 第 9 項）。
- **Contract**：PvP 的欄、Resource、事件與 System 的**宣告**（zone、writePolicy、role、rollback、掛點、順序）；Match 的 Loading＋FixedUpdate、ClientSim 的 Loading＋FixedUpdate 的 ScheduleDef（§3 Q2）；Life 的寫者形狀照 U6：`LifeState`＋`DeathEvent`，不設中間狀態欄。只寫描述，System 函式體是空的、回 Ok。
- **Architecture**：不變。產品測試目錄多一份登記與一支測試執行檔；Contract 撐不住時只回報（例：某 digest 欄必須 Shared 才能登記＝撐不住，LIM-4）。新的依賴邊只有「產品測試 → `GYO::Kernel`」（Application → Engine，方向合規）。

## 1. 事前分析（看結果前寫下；不是結果）

讀 `engine/kernel/src/Fingerprint.cpp` 的 C 指紋（格式 2）定義與 spec H.4.1／H.4.2 後，**謂詞 (iv)「Match 與 ClientSim 的 cFingerprint 相同」照 spec 字面登記時，預期不成立**。C 指紋對「跑 zone C System 的每個 schedule」雜湊 `rollbackEnabled`、stage 序列、`eventTrim` 與只留 (c) System 的 Plan（review B6 加入），而 spec 字面的兩端在這三處都不同：

| # | 差異 | 出處 | 為什麼會進 C 指紋 |
|---|---|---|---|
| P1 | Match 的 FixedUpdate 不是 `rollbackEnabled`；ClientSim 是 | spec H.4.1「clock、Serial」對 H.4.2「clock、`rollbackEnabled`、Serial」 | `out.Flag(rollbackEnabled)` |
| P2 | Match 的 FixedUpdate 有 `Judge` stage；ClientSim 沒有 | spec H.4.1 stage 序 Ingress→Simulate→Judge→Egress；H.4.2 只列 Ingress、Simulate、Egress | `out.Strings(stages)`，且 stage 結束的同步點在化簡後的 Plan 裡 |
| P3 | Match 有只在 Match 的 zone C System：連線品質判定（host 判斷＝(c)，target/determinism.md §2 Host judgement 列；spec L.3 `MatchRuntimeHost` 列「`:450-451` 是 host 判斷（(c)）」）與快照組裝（digest 編碼它的結果＝(c)，determinism.md §2 規則 1） | 同左 | zone C System 逐一雜湊 |

spec H.3 寫「Client 多出的 Rollback 與 PredictionHistory 在 FixedUpdate 之外（zone=B）」，預設 Match 沒有比 Client 多的 (c)；但 host 判斷與快照組裝是 Match 才有的 (c)。這是 spec H.3／H-4 與 C 指紋格式 2 之間的衝突，**不是**本批能在登記裡解的事（改 C 指紋的範圍是 Kernel 的 Contract；把 host 判斷改成 (b) 違反 determinism.md §2）。怎麼處理交 §3 Q1。

另：遷移計畫 §3.6 Contract 寫「ClientSim 的 FixedUpdate」一套 ScheduleDef，spec H.4.2 與 H.2b 第 7、8 點寫 ClientSim 也有 `Loading`（ready 時兩端 (c) 狀態可比對）。Match 的 Loading 有 zone C System，ClientSim 少了 Loading，(iv) 也必然不同。交 §3 Q2。

## 2. 範圍（逐檔）

| 檔 | 動作 | 內容 |
|---|---|---|
| `docs/object_fps_pvp/plans/v8/v8-product-pvp-plan-dryrun-declaration.md` | 新增 | 本檔 |
| `tests/object_fps_pvp/KernelPlanDryrunTests.cpp` | 新增 | §4 的登記（替身型別、空函式體 System、三個 plugin）＋四個謂詞的檢查＋把 `ExportPlans()` 印到標準輸出 |
| `tests/object_fps_pvp/CMakeLists.txt` | 改 | 新執行檔 `gyo_object_fps_pvp_plan_dryrun`（只連 `GYO::Kernel` 與 doctest；不連 Match／Client 的程式庫）＋ctest `object_fps_pvp.plan_dryrun`（標籤 `cpu;pvp`） |
| `tests/object_fps_pvp/mutations.json` | 改 | 新 check `plan_dryrun`＋本批突變（§6） |
| `docs/dev_logs/2026_11_1x_pvp_plan_dryrun.zh-Hant.md` | 新增 | 完成或停止時寫（AGENTS §15） |
| `docs/object_fps_pvp/plans/v8/README.md` | 改 | dev_log 一覽加一列 |
| `docs/object_fps_pvp/plans/v8/HANDOFF.md` | 改 | 一行 |
| `docs/architecture/plans/kernel/HANDOFF.md` | 改 | 一行 |
| `docs/architecture/plans/kernel/migration-plan.zh-Hant.md` | 改 | 只在 §7 U14 列末加一句 N-11 的查證結論（任務卡「結論寫進宣告與 U14」）；不改其他字 |

**不碰**：`engine/`（含 `engine/kernel/`）、`apps/`、其他 `tests/`、`docs/architecture/target/`、spec。產品程式不改一行（§3.6「被取代的 legacy 條文：無」）。

## 3. 決定點（請使用者選；每點附機制、取捨、證據）

**Q1 謂詞 (iv) 的事前預測不成立時，本批怎麼走**
- (a) **推薦**：照 spec 字面登記（P1～P3 都照原樣），四個謂詞**全部算完**再依停止條件停、回報。機制：一次登記得到 (i)(ii)(iii) 的真實結果與 (iv) 的實際差異清單（C 指紋各段的差），架構師一次拿到全部材料去解 H.3／C 指紋的衝突；之後改形狀再跑是下一輪。代價：G1 這一輪不過；測試若照實判會在 CI 紅，所以停止時不開 PR、不合併，只把工作樹與匯出存證（§7）。
- (b) 現在就停在宣告，交架構師先解衝突（例：C 指紋只算共用 plugin 的部分；Client 加空 `Judge` stage 並讓 Match 也 `rollbackEnabled`；或其他），解完重寫本宣告。代價：(i)(ii)(iii) 的結果延後；架構師要在沒有 finish 輸出的情況下判。
- (c) 不推薦：在宣告裡先改形狀讓 (iv) 可能成立（Match 宣告 `rollbackEnabled`、Client 加空 stage、host 判斷改 (b)）。這是為了湊綠改 Contract，最後一項還違反 determinism.md §2。

**Q2 ClientSim 的 schedule**
- (a) **推薦**：照 spec H.4.2／H.2b 第 7、8 點，ClientSim 有 `Loading`（stage 序與 Match 相同）與 `FixedUpdate`。計畫 §3.6 的「ClientSim 的 FixedUpdate」一句讀作摘要。
- (b) 照計畫 §3.6 字面只登記 FixedUpdate：(iv) 必然不同（Match Loading 的 (c) System 進 C 指紋），結論沒有資訊量。

**Q3 欄與 System 的拆法（§4 的表）**：照 §4 登記；`combats_`、`connections_` 的拆法與 Body 重生交接是本批定的描述形狀（entity-model target §1 交給本批），match-sim 可以在它的宣告改（那時有函式體與 golden）。請核准表本身。

**Q4 Peer 實體**：計畫 §3.6 Contract 提到「Player／Peer 分開」。**推薦**不登記 Peer：它的欄（複寫帳本、可見性）是 zone B、只在 Match，四個謂詞都不讀它；它的 id 由誰配、帳本形狀尚未定義（pv10、match-host），登記等於替 match-host 發明設計。列為範囲の限界。若選登記，以 zone B 欄＋一個 Egress 的 (b) System 最小登記，id 來源標「假設」。

## 4. 登記內容（Q3 的表；照 spec H.4.1／H.4.2，名稱照 target/naming 第 2 條的詞性）

型別是**替身**：測試檔內的空 struct，名字照目標欄名；真實欄型別在 match-sim 定（§8 限界）。三個 plugin：`PvpSimulationPlugin`（兩端共用的 (c) 資料與 System）、`PvpMatchHostPlugin`、`PvpClientSimHostPlugin`。Match 的 Registry＝共用＋Match host；ClientSim＝共用＋ClientSim host。

### 4.1 欄（Player 實體；全部由共用 plugin 登記，Exclusive）

| 欄 | 來源（今天） | zone | role | rollback | 唯一寫者 |
|---|---|---|---|---|---|
| `Body` | `bodies_` 身體組 | C | State | Yes | `StepMovement` |
| `LifeState` | `bodies_` 生命組 | C | State | Yes | `ApplyLife`（U6） |
| `InputCursor` | `bodies_` 輸入游標組 | C | State | Yes | `StepMovement` |
| `HostDiagnostics` | `bodies_` host 診斷組（`movementSlack*`、`connectionQualityFailures`） | C | State | No | `JudgeConnectionQuality`（只在 Match；`TrackSlack` 併入它，entity-model target §1「一個欄」） |
| `Connection` | `connections_`（命令佇列、替代計數、backlog、重設排程、quality） | C | State | Yes | `StepMovement`（host 的重新對時請求改經 `InputRing`，`RequestMovementResync` 不再直寫） |
| `Combat` | `combats_`（整份一欄，不拆） | C | State | Yes | `ResolveActions`（重生時的血量彈匣重設與換彈完成，改由它讀 `RespawnEvent`／tick 做；今天在 `ResolveLifeBoundaries`） |
| `Animation` | `animations_` | C | State | Yes | `AdvanceAnimation` |
| `ActionLedger` | `actionLedgers_` | C | State | Yes | `ResolveActions`（提交與 ACK 退役改經 `InputRing` 由它消費） |

### 4.2 Resource 與事件

| 名 | 種類 | zone | role／rollback | 寫者 |
|---|---|---|---|---|
| `InputRing` | Resource | C | Input／No | `StageInputs`（ingress） |
| `MembershipRequests` | Resource | C | Input／No | `StageMembershipRequests`（ingress） |
| `MembershipCursor` | Resource | C | State／Yes | `ApplyMembership` |
| `HostClockSample` | Resource（boundary，Host 在 Ready 寫） | C | Input／No | 無 System（Z2 例外） |
| `ActionRefusals` | Resource（U7 的 per-connection Egress） | C | State／Yes | `ResolveActions` |
| `AssetStaging`、`AssetRequestList` | Resource | C | Input／No | `StageAssets`（ingress，只掛 Loading） |
| `AssetCursor`、`ArenaAssets`、`AnimationAssets` | Resource | C | State／No | `ApplyStagedAssets` |
| `ArenaCollision` | Resource | C | State／No | `InstallArena` |
| `LoadingStageDone<S>`（資產、地圖、場景、可互動物件、敵人、玩家，各一） | Resource | C | State／No | 各 stage 的 `Finish…Stage` |
| `LoadingState` | Resource | C | State／No | `FinishPlayersStage` |
| `SnapshotOut` | Resource（Match host plugin） | C | State／No | `AssembleSnapshot` |
| `LinkInputInbound`、`LinkMembershipInbound`、`AssetInbox`、`LinkOutbound` | Resource，external | B | — | ingress 消費（清空）／`SendLinkOutbound`（Match）、`SendInputs`（ClientSim）；ClientSim 另有 `IntentInbound`、`PresentationOutbound` |
| `DeathEvent` | 事件，domain Fixed | C | — | `ResolveActions`（U6） |
| `RespawnEvent` | 事件，domain Fixed | C | — | `ApplyLife`（Body、InputCursor、Combat 的重生重設交給各自的寫者；entity-model target §1 交 match-sim 定的「事件或中間欄」，本批先登記事件形，不設中間欄，與 U6 一致） |

### 4.3 schedule

| Registry | schedule | 選項 |
|---|---|---|
| 兩端 | `Loading` | domain Loading、clock、Serial；stages：Ingress → 資產 → 地圖 → 場景 → 可互動物件 → 敵人 → 玩家（H.2b 第 2 點）；ingress stage＝Ingress |
| Match | `FixedUpdate` | domain Fixed、clock、Serial、**不** rollbackEnabled；stages：Ingress → Simulate → Judge → Egress；runner `gyo-match-sim`（H.4.1） |
| ClientSim | `FixedUpdate` | domain Fixed、clock、`rollbackEnabled`、Serial；stages：Ingress → Simulate → Egress；runner `gyo-client-sim`（H.4.2） |

Startup 不登記：PvP 沒有 Startup System（H.2b 第 1 點：Startup 只放登記與固定 Resource）。Loading 的 runner 與同 World 的 FixedUpdate 相同（D-E13）。runner 由各 host plugin 的 Finish 安裝。

### 4.4 System

| System | plugin | zone | 掛點 | 讀 | 寫 |
|---|---|---|---|---|---|
| `StageMembershipRequests` | 各 host | B, ingress | Loading.Ingress、FixedUpdate.Ingress | `LinkMembershipInbound`、`MembershipCursor`、CurrentTick | `LinkMembershipInbound`（清空）、`MembershipRequests` |
| `StageInputs` | 各 host | B, ingress | FixedUpdate.Ingress | `LinkInputInbound`（Match）／`IntentInbound`（ClientSim）、CurrentTick | 收件（清空）、`InputRing` |
| `StageAssets` | 各 host | B, ingress | Loading.Ingress | `AssetInbox` | `AssetInbox`（清空）、`AssetStaging`、`AssetRequestList` |
| `ApplyStagedAssets` | 共用 | C | Loading.資產 | `AssetStaging`、`AssetRequestList` | `AssetCursor`、`ArenaAssets`、`AnimationAssets` |
| `FinishAssetsStage` | 共用 | C | Loading.資產，after `ApplyStagedAssets` | `AssetCursor`、`AssetRequestList` | `LoadingStageDone<資產>` |
| `InstallArena` | 共用 | C | Loading.地圖，run 條件讀 `LoadingStageDone<資產>` | `ArenaAssets` | `ArenaCollision` |
| `FinishMapStage` | 共用 | C | Loading.地圖，after `InstallArena`，同條件 | `ArenaCollision` | `LoadingStageDone<地圖>` |
| `FinishSceneStage`、`FinishInteractablesStage`、`FinishEnemiesStage` | 共用 | C | 各自 stage，run 條件讀前一 stage 的旗標 | 前一旗標 | 自己的旗標 |
| `ApplyMembership` | 共用 | C | Loading.玩家（條件讀 `LoadingStageDone<敵人>`）、FixedUpdate.Simulate | `MembershipRequests`、CurrentTick | `MembershipCursor`；`SpawnWithId<Body, LifeState, InputCursor, HostDiagnostics, Connection, Combat, Animation, ActionLedger>`；`Despawn` |
| `FinishPlayersStage` | 共用 | C | Loading.玩家，after `ApplyMembership` | `LoadingStageDone<敵人>` | `LoadingStageDone<玩家>`、`LoadingState` |
| `ApplyLife` | 共用 | C | FixedUpdate.Simulate | `DeathEvent`、`Body`、`ArenaCollision`、CurrentTick | `LifeState`、`RespawnEvent` |
| `StepMovement` | 共用 | C | FixedUpdate.Simulate | `InputRing`、`LifeState`、`RespawnEvent`、`ArenaCollision`、CurrentTick | `Body`、`InputCursor`、`Connection`；`DiagnosticsWriter<MovementTraceRecord>`（G.4 第 3 條改句柄） |
| `ResolveActions` | 共用 | C | FixedUpdate.Simulate | `InputRing`、`Body`、`InputCursor`、`LifeState`、`Animation`、`RespawnEvent`、`ArenaCollision`、`AnimationAssets`、`HostClockSample`、CurrentTick | `Combat`、`ActionLedger`、`ActionRefusals`、`DeathEvent` |
| `AdvanceAnimation` | 共用 | C | FixedUpdate.Simulate | `Body`、`LifeState`、`DeathEvent`、`AnimationAssets`、CurrentTick | `Animation` |
| `JudgeConnectionQuality` | Match host | C | FixedUpdate.Judge | `Connection`、`InputRing`、`HostClockSample`、CurrentTick | `HostDiagnostics` |
| `AssembleSnapshot` | Match host | C | FixedUpdate.Judge | `Body`、`LifeState`、`InputCursor`、`HostDiagnostics`、`Connection`、`Combat`、`Animation`、`ActionLedger`、`DeathEvent`、CurrentTick | `SnapshotOut` |
| `SendLinkOutbound` | Match host | B | FixedUpdate.Egress | `SnapshotOut`、`ActionRefusals`、CurrentTick | `LinkOutbound` |
| `SendInputs` | ClientSim host | B | FixedUpdate.Egress，`NotInRollback` | `InputRing` | `LinkOutbound` |
| `PublishPresentation` | ClientSim host | B | FixedUpdate.Egress，`NotInRollback` | `Body`、`LifeState`、`Combat`、`Animation`、`DeathEvent` | `PresentationOutbound` |

ClientSim 的 ingress System 全部帶 `NotInRollback`（D-E15）。快照與 digest：digest 今天由測試從快照算（`AuthorityDigest.cpp`），所以謂詞 (iii) 的「snapshot／digest 組裝」在 Registry 裡對應 `AssembleSnapshot`（Match）；ClientSim 沒有權威快照，對應 `PublishPresentation`（呈現讀死亡）。

**順序邊的規則（看結果前定）**：before／after 只在兩種情況加——(1) spec 明文（`Finish…Stage` after 該 stage 的其他 System，H.2b 第 3 點）；(2) finish 回報 D-E8 的配對。(2) 的方向一律照今天 `PvpMatch::Tick` 與 spec H.4.1 的序：成員 → 生死 → 移動 → 動作裁決 → 動畫；Judge stage 內連線品質 → 快照組裝。不為了降低非資料流比例而增刪邊；邊的增減只回應 D-E8，最後的邊清單照實寫進 dev_log。

## 5. 判定與門檻（G1 的四個謂詞照計畫 §2.1／§3.6 原文；全部由程式從 `FrozenRegistry`（`Plans()`、`Systems()` 的存取集合，與 `ExportPlans()` 印出的是同一份資料）機械檢查）

兩個 Registry（Match、ClientSim）各跑一次 finish。

| # | 判定 | 門檻 |
|---|---|---|
| (i) | finish 成立（零啟動錯誤）且 D-E2＝0 | 兩個 Registry 的 `Finish()` 都 Ok；每個 Exclusive 欄與 Exclusive Resource 的 System 寫者（含延後寫與 bundle）≤1；§4.1 的八個欄與 §4.2 的 C Resource 全部是 Exclusive（**不靠改 Shared**；測試逐欄查 `ColumnInfo.writePolicy`） |
| (ii) | 為消 D-E8 加的 before／after 邊中，非資料流邊 ≤50% | 每個 Registry 的每個 Plan（有邊的）：`NonDataflowEdges() * 2 <= declaredEdges.size()`（字面定義，registry-schedule 的 `dataflow` 欄）；擴充定義 `NonDataflowEdgesExtended()` 只印不判 |
| (iii) | Life 形狀的 Plan 謂詞（U6） | 兩個 Registry 的 FixedUpdate Plan 上：`LifeState` 的寫者恰一個（`ApplyLife`），它的 step 在 `StepMovement` 之前、與 `StepMovement` 同在 Simulate；`DeathEvent` 的寫者集合恰等於 {`ResolveActions`}；Match 的 `AssembleSnapshot` 讀 `LifeState` 與 `DeathEvent`，step 在 `ResolveActions` 之後（ClientSim 的 `PublishPresentation` 同此檢查）；D-E19＝0（由 (i) 的 finish Ok 涵蓋，另查每個 System 每個 schedule 至多一個掛點） |
| (iv) | Match 與 ClientSim 的 cFingerprint 相同（H-4） | `CFingerprint()` 兩者 `==`；不同時印出差異來源（兩端的 C 欄、C Resource、C 事件、C System 名單與每個含 C 的 schedule 的 rollback／stages／化簡 Plan） |
| N-11 | 查證（不是門） | 見 §9；結論寫進本檔與 U14 |
| 範圍 | 只動 §2 的檔 | 完成前以 Python 比對 `git diff --name-only origin/master...HEAD` 對 §2（先以一個 §2 外的路徑當正例命中） |

**T=35／R=215 的數值不在本批判**（函式體為空量不到），留 match-sim 在新重放 host 上實測（計畫 §3.6）。

## 6. 件数の突き合わせ・守衛與突變的去處

- **件数の突き合わせ**（本批不是量測批，但照 §3 共通在報告第一行寫）：§4.4 宣告的 System 數＝兩個 Registry 的 `Systems()` 數（各自的 plugin 合計）＝各 Plan 的 System step 數合計（每個掛點一次）；§4.1 的欄數＝`Components()` 數；最後登記的邊數＝各 Plan `declaredEdges` 合計＝dev_log 的邊清單列數。任何一項不一致＝「不一致」，不確定受理判定。
- **守衛與突變的去處（四選一）**：碰到的既有守衛＝**無**（產品程式不改）。**新增**＝`object_fps_pvp.plan_dryrun`（G1 的受理判定測試）。之後的去處：match-sim 以真實 plugin 的登記取代替身時**改寫**（判定不變）。
- **本批突變**（`tests/object_fps_pvp/mutations.json`，check `plan_dryrun`；每條的預期失敗是測試裡該謂詞專用的訊息片段，照 AGENTS §14.2，Build 失敗不算 killed）：

| id | 改什麼（`KernelPlanDryrunTests.cpp` 的登記） | 預期失敗（訊息片段） |
|---|---|---|
| `pd-life-second-writer` | `ResolveActions` 加寫 `LifeState` | `G1(i) finish` |
| `pd-life-shared` | `LifeState` 改 Shared | `G1(i) shared` |
| `pd-combat-second-writer` | `ApplyLife` 加寫 `Combat` | `G1(i) finish` |
| `pd-life-after-movement` | `ApplyLife` 改 after `StepMovement` | `G1(iii) life before movement` |
| `pd-death-extra-writer` | `AdvanceAnimation` 加寫 `DeathEvent` | `G1(iii) death writers` |
| `pd-snapshot-drops-death` | `AssembleSnapshot` 不讀 `DeathEvent` | `G1(iii) snapshot reads` |
| `pd-snapshot-before-actions` | `AssembleSnapshot` 搬到 Simulate、before `ResolveActions` | `G1(iii) snapshot after actions` |
| `pd-life-two-mounts` | `ApplyLife` 在 FixedUpdate 掛兩次 | `G1(i) finish` |
| `pd-nondataflow-edges` | 加三條無資料往來的 before 邊（使比例過半） | `G1(ii) non-dataflow` |
| `pd-client-extra-c` | ClientSim 多登記一個 zone C System | `G1(iv) cFingerprint` |

  依 Q1 (a)，若 (iv) 照實不成立，`pd-client-extra-c` 在原程式就已經「失敗」，無法判 killed；屆時記為「判定不能」，不算 killed，等形狀解決後重跑。
- **敵對檢查**：審查與突變一覽的敵對檢查合併一次，Fable／high 只讀（與作者 Opus 不同、不是小模型；navigator〈敵対的な検査〉、計畫 §3.6）。報告寫檢查者模型、找到的處數、加入後的檢出數／總數。
- §14.4 驗證者：不出場（不改既有 golden、守衛判定或 DI 守衛）。

## 7. 停止條件

- **任一謂詞不成立就停**，不改謂詞、不改區、不改 writePolicy 湊綠：
  - (iii) 不成立＝U6 重開，交使用者（計畫 §3.6）。
  - (ii) 非資料流邊 >50%，或某 digest 欄必須 Shared 才過 (i)＝停，重劃區或重拆欄（K.2 第 3、10 條，§8 RC-4），交架構師與使用者。
  - (iv) 不成立（§1 預測）＝停，回報 C 指紋各段的差，交架構師（spec H.3／H-4 與 C 指紋格式 2 的衝突）。
- 停止時：不開 PR、不合併；測試與登記留在分支的 WIP commit（不 push 到 master），`ExportPlans()` 的輸出與 finish 報告存主 checkout `build/evidence/pvp-plan-dryrun/`；dev_log 照樣寫（停止也是結果）。
- §4 的表在 finish 後若要改（D-E8 的邊以外的任何改動，例如改寫者、改區、加欄）＝先停、報告、取得核准再改。
- AGENTS §14 冒頭三條；單一問題累計 >2 h；總時數 >24 h（估時上限 16 h ×1.5）。

## 8. 範囲の限界

- 共通三條：**LIM-2**（v1 只有 Serial）、**LIM-3**（只驗描述，不驗函式體；函式體經全域或捕獲讀到的東西不在描述裡＝L5）、**LIM-6**（本批不 `Run`，存取驗證開關不起作用，「無違規」只指 finish 的靜態檢查）。
- dryrun 成立只表示「規則表達得出」，不表示遷移後行為相同（那是 match-sim 的 golden 37）；Life 的 T／R 數值留 match-sim。
- 替身型別：欄的大小與對齊是空 struct 的，Registry 指紋與 C 指紋不等於遷移後的值；(iv) 比的是同一份替身，形狀的比較有效、數值無意義。
- `RespawnEvent`、`Combat` 的單一寫者、`TrackSlack` 併入 `JudgeConnectionQuality`：是本批為了能登記而定的描述形狀，行為等價由 match-sim 以 golden 證；match-sim 可以改，改的時候在它的宣告寫。
- Peer 不登記（Q4 推薦時）。
- U14：v1 以 Welcome 隱含 ready，Match 的 Loading「玩家」stage 不生成初始成員；本批登記了 `ApplyMembership` 的 Loading 掛點，只證明描述成立，不證明那條路在真實 host 上被跑到（kernel HANDOFF〈未結事項〉）。
- 命名：`DeathEvent`（使用者 U6 定的名）與本批的 `RespawnEvent` 都不是過去式，與 target/naming DI 2「Event 是過去式」不一致；照 U6 的名登記，記給架構師。
- 只在 Intel Mac 本機跑 finish；三平台由 L1 的 ctest 給（若有 PR）。

## 9. N-11 查證（Client 能否從 Welcome 推出 Loading 的請求清單）

看結果前的查證，結論先寫：**v1 的 PvP 範圍內可以推出，但順序與內容是產品規則、不在 wire 上**。
- 證據：Client 啟動時讀本機 `maps/arenas.json` 安裝所有 arena（`PvpApplication.cpp:963`），Welcome 到達時以 (`arena_id`, `arena_version`) 選一個並以 `arena_digest` 比內容（`ClientConnection.cpp:292-307`、`:569-571`）；動畫內容是本機固定內容，只以 `animation_digest` 比對（`:310-315`）；移動與戰鬥規則的數值直接在 Welcome 裡（`client_v9.proto` 的 `combat_rules`、`jump_height`、`gravity`、crouch／sprint 欄），不是檔案。
- 所以 Client 的請求清單＝{(`arena_id`, `arena_version`) 對應的 arena、本機固定的動畫內容}，兩端若用同一條產品規則排序就同源（H.2b 第 8 點的「兩端序號同源」）。缺口：Welcome 沒有「場景、可互動物件、敵人」的清單；PvP v1 沒有這些內容，所以現在不缺，有了就要 manifest（pv10-requirements）。這一句寫進 U14 列末。

## 10. 模型／檔位、估時

- 實作：本卡會話 Opus 5.5／high（計畫 §3.6）。審查兼突變一覽敵對檢查：Fable／high 一次（只讀）。驗證者：不出場。
- 估時：**8～12 h**（宣告 2、登記與謂詞檢查 3～5、突變與審查處置 2～3、文件 1）；計畫的 7～14＋守衛去處 1～2 h（推定）。停止線見 §7。

## 11. 被取代的 legacy 條文

無（只登記，不改產品）。
