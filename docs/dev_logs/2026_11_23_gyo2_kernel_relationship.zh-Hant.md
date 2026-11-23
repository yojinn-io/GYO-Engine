# 2026-11-23 Kernel 的關係與父子（`v8-arch-gyo2-kernel-relationship`，子批 KR）

Owner：Kernel（GYO2 主會話開的任務卡，使用者「開 1、2、3」的第 1 張）。分支 `claude/gyo2-kernel-hierarchy-06b6eb`（開向 `gyo2`）。作者 Opus 5.5／high；宣告的敵對式檢查 Opus／high（和作者同一個模型：使用者 2026-11-23 經主會話「現在fable用量不夠了，暫時用Opus」）。

## 為什麼

使用者 2026-11-23 定父子關係照 Bevy 進 Kernel、ECS 機制以 Bevy 為主參考（HANDOFF `v8-arch-gyo2-hierarchy-bevy-and-primary-reference`）；`transform` 的階層傳播（X2）、UI（U）、文字的 span（T）、物理的碰撞體掛剛體都會積在上面。SDL 宣告第 6.5 節列了需求 1～5 與和 GYO 現行 Kernel 契約的衝突點 K1～K9，交給本批。

## 經過與結論

宣告：[kernel-relationship-declaration.zh-Hant.md](../architecture/target/v2/kernel-relationship-declaration.zh-Hant.md)。

| 修訂 | 內容 | 檢查 |
|---|---|---|
| 0（4b5340e2） | 建議 `Children` 依 EntityId 升冪（索引）、維護放套用步驟 | Opus／high（同模型）：blocker 5、should 11、nit 12，全部採用（主 checkout `build/evidence/gyo2-kernel-relationship/declaration-r0-adversarial-opus.md`） |
| 1（9612408c） | Q1 改建議 (A′) 附掛先後（T、U 照 Bevy 靠 `Children` 的先後）；5b 移到 insert 之後、5a 之前；新 Q3 先拆再刪、Q4 連帶刪除先後；已核准條文與判定的改動逐條列出 | — |

使用者 2026-11-23 在本卡動工前核准：「Q1～Q4 照建議，其他一起同意，開始」（f074239c）。

實作第 1 步停過一次（AGENTS §14 冒頭；宣告第 14 節）：Kernel 自己登記兩個元件之後，J-12、J-20、J-32、J-18 共四組守衛的期待值也要改，不在第 12.5 節同意的清單裡。停下回報三個選項。使用者在本卡回「繼續」，卡片讀成照建議的 (a)、在回覆裡寫明讀法後繼續；之後主會話轉來使用者的明確選擇「照建議，KR 選 (a)」（HANDOFF `v8-arch-gyo2-kernel-relationship-golden-extended`），第 12.5 節補列這四處。

## 做了什麼

- `engine/ecs/kernel/include/engine/kernel/Relationship.hpp`（新）：`ChildOf{parent}`；`Children`（固定寬度：指標＋兩個 32 位元的計數，sizeof 在各平台與建置類型相同；只讀走訪；修改只有 Kernel 的 `Detail::RelationshipStore`）；兩者的 `HashValue`；編譯期判斷用的 `RelationshipTraits`；`RejectedRelationship`（原因、路）。
- `Registry.cpp`：`AddKernelRegistrations` 在 `KernelSettings` 之後登記 `ChildOf`、`Children`（owner 是 Kernel，ComponentId 0、1）；`RegistryData::childOf`、`children`。
- 編譯期擋（各自的 `GYO_KERNEL_REJECT_RELATIONSHIP_*` 標記）：`QueryTerm<Write<T>>`、`World::GetMut`、`ExclusiveWorld::Modify`（`ChildOf` 與 `Children`）；`Commands::Insert`／`Remove`、`CommandsFor`、`Spawn`／`SpawnWithId` 的 bundle、`ExclusiveWorld` 的當場插入／拿掉／生成、Host 的 `World::Insert`／`Remove`、`Registrar::InitialValue` 的目標（`Children`）；`DiagnosticsWriter<RejectedRelationship>`。
- `World.cpp` 的套用：每個階段的關係狀態（變空的父、待附掛的子）；despawn 階段深度優先前序展開連帶刪除（Q4）、本階段有 `ChildOf` 拿掉或覆寫紀錄的子不連帶（Q3）、留下的子的 `ChildOf` 同一步拿掉；remove 階段銷毀 `ChildOf` 之後當場從父拿掉；spawn 寫進的 `ChildOf` 到 5b 才檢查；insert 階段寫之前檢查（覆寫被拒時 OnRemove 讀到原本的值）；**第 5b 步**（insert 之後、5a 之前）依 (planPos, recSeq) 附掛、新建 `Children` 格進 OnAdd 與 5a、變空的父看淨差後先 OnRemove 再拿掉。`Restore` 不展開連帶刪除；`RestoreSet` 給了 `Children` 就不維護（`Detail::Relationships` 三種）。
- Host 路：`World::Insert<ChildOf>`／`Remove<ChildOf>`／`Despawn` 也維護、連帶刪除、拒絕不合法的目標，改到 `Children` 時標 World API 的寫入。
- `StructureProblem`（存取驗證打開時）加查 DI-KS 30（`World::RelationshipProblem`）。
- `Executor.cpp`：`RestoreFrom` 收 `targetsGiven`；當場插入的 `ChildOf` 被拒時回 false。`Params.hpp`：`RestoreSet` 記下有沒有給 `Children`。
- 回滾 Module 的整合案：`tests/common/rollback/StrategyTests.cpp`「R-2 with ChildOf and Children…」（`[rb-r2-family]`；ctest `rollback.rollback_strategy`）——`ChildOf`、`Children` 都登記回滾，步裡掛子、換父、拆開又掛回（2 的 `Children` 順序變）、和刪 2 同一個階段把 50 換到 2（Q3：50 留下、沒有父）、連帶刪除 2 的子；回滾到第 4 步重跑後摘要＝直跑。回滾 Module 的程式沒改（主會話 2026-11-23 排定整合案放在本 PR）。
- 測試：`tests/common/kernel/RelationshipTests.cpp`（新，ctest `kernel.relationship`，39 案，標記照宣告 12.1）；`compile_fail/RelationshipSample.cpp`（新，configure 時 `kernel.relationship_compile_fail`，對照組＋20 個變體）；負向樣本的 CMake 函式改成收 sample 與變體清單（`SystemContractSample.cpp` 照舊）。
- 寫死值（第 12.5 節，§14.4）：新增 `kFirstOwnerComponent = 2`（`KernelTestSupport.hpp`），ComponentId 的期待值改成「作者的第 n 個」；golden 換成新值（J-18 `RegistryTests.cpp:41`、J-20 `SchedulerTests.cpp:138-142`、J-54 `HookFlushTests.cpp:935`、`InitialValueTests.cpp:656`、J-32 `WorldTests.cpp:519`）；J-32 hook 紀錄的 ComponentId（`WorldTests.cpp:757`、`:760`、`:836`）。
- 文件：Kernel DI-KS 29～33（新）、DI-KS 9、12、15 補句、RC 26、27（新）——草稿，最終文字在合併前核准定稿；說明頁第 19 節「父子與關係」、第 17 節補讀檔裡的父子；回滾頁 R-2 前提 8；讀檔宣告 C-10 補一句；邊界統合宣告第 3.1 節加「關係」；README 列入。

## 驗證

- 建置：本機 macOS x86_64，`core` preset（RelWithDebInfo）。基準（第 0 步，跟上 gyo2 之後）196／196；完成時 197／197（多 `kernel.relationship`）——見下面「受け入れ判定」。
- kernel：`gyo_kernel_tests` 442 案全過（既有 400、新 42：審查前 39＋審查後 3）；`gyo_kernel_tests_reversed` 的 J-18 過；`gyo_rollback_tests` 56 案全過。
- 負向樣本：`kernel.relationship_compile_fail` 對照組可編譯，20 個變體都以各自的標記失敗（configure 輸出）。

### 突變（D184：新加的＋find 在改動範圍內的）

- 新加 `kr-` 30 條：30／30 killed（`build/evidence/gyo2-kernel-relationship/kr-mutants.log`；第一輪 28／30，兩條處理後重跑 3／3）。
  - `kr-restore-expands` 第一輪是 wrong_failure：`[kr-restore-no-expand]` 裡 `RestoreSet` 給了 `ChildOf`，它對子的 `ChildOf` 的拿掉紀錄已經觸發 Q3 的豁免，那個案例分不出「`Restore` 展開連帶刪除」；實際抓到的是 `[kr-restore-unregistered-childof]`，expect 改指它、`what` 寫明。
  - `kr-host-no-mark` 第一輪存活：`[kr-host]` 的 Host 插入都在第一次 Run 之前，那時沒有邊界快照；案例補一次 Run 之後的 Host 插入。
- 宣告 12.3 列了、沒有立的：`kr-linked-no-visited`（修訂 1 已撤回：等價）；`kr-linked-by-id`（「先展開再依編號」無法用一處替換表達），改立 `kr-linked-sorted`（子依編號、不依 `Children` 的順序），配合 `[kr-linked-order]` 改成附掛先後與編號不同（先掛 B 30、再掛 A 10）。
- 既有突變：find 落在改動範圍內、或被本批改得對不上的 29 條（kernel）＋1 條（rollback）：全部 killed（`related-kernel-mutants.log`、`related-kernel-mutants-rerun.log`、`related-rollback-mutants.log`）。其中 22＋1 條的 find／replace 照新寫法改寫、同一個改變（despawn 那段多一層迴圈、`ApplySync`／`ApplyRestore` 多一個參數、spawn 與關係兩個 lambda 有同樣的幾行）；3 條 J-32 的 expect 把 ComponentId 加 2（§14.4：緩的面沒有）。改寫後第一輪 19／29：6 條 `sr-` 的 find 因 `rejectRelationship` 抄了同樣的行而不唯一、3 條 expect 帶舊 ComponentId、1 條 replace 用到改名的變數，處理後重跑 10／10。
- 靜態掃描：所有突變清單 2205 條，基準上唯一、本分支上不唯一的 find：0（正例 `kr-no-add` 命中 1 次）。
- 審查之後（下一節）新加 3 條（`kr-net-stamp-not-restored`、`kr-immediate-counts-rounds`、`kr-rewired-dropped-early`）、改寫 12 條的 find（含 `sr-no-record`、`sr-records-capped`：寫入改由 `RecordKernelDiagnostics<R>` 做），重跑 64／64、rollback 1／1（`review-fix-kernel-mutants.log`、`review-fix-rollback-mutants.log`）；全清單 2209 條，不唯一的 find：0。
- §14.5 的數量：新加 33 條，本批新增或改寫的條文 9 條（DI-KS 29～33、9、12、15，RC 不計）×5＝45，未超過。

## 審查之後（設計審查與 code review，都是 Opus／high、和實作者同模型）

報告：主 checkout `build/evidence/gyo2-kernel-relationship/design-review-opus.md`、`code-review-opus.md`。L1 三平台在審查前的 999ecc6d 上 success（run 45125895018，`L1 /` 三個 job 都是 success）。

| 條 | 處理 |
|---|---|
| 設計 F1／code N-11：刪父的同一個階段又插入同一個父 | 交使用者（合併前核准頁第 1 節 R-1） |
| 設計 F2：`fromSpawn` 與拒絕分支的死碼、排版殘留 | 改：`Attach` 拿掉 `fromSpawn`，5b 對每一項都查（insert 來的必為真） |
| 設計 F3：小動作抄了 4／2／2 份 | 改：`World::DetachChild`、`AttachChild`、`HostDetach`、`RejectedOf`、`RecordKernelDiagnostics<R>`（`RejectedSpawn` 也用）；兩條深度優先迴圈保持分開 |
| 設計 F4：文件寫實 | 改：DI-KS 29（內部寫死、表延後）、DI-KS 33 與 R-2 前提 8（開發版報 W-1）、說明頁（從 Bevy 搬來的 `add_child` 寫法、淨差）；C15、淨差兩列寫在合併前核准頁第 3 節 |
| 設計 F5（`Relationships` 與 `Derive` 重疊） | 之後 |
| 設計 F9：RC 27 (a) 的已知使用者 | 改 |
| code S-1／設計 F6：當場插入回傳值算到 hook 輪 | 改：`ApplyImmediateRecords` 回報「本筆套用之後、hook 輪之前」的計數；`[kr-immediate-hook-reject]`、突變 `kr-immediate-counts-rounds`。`RejectedSpawn` 那一側同樣的寫法沒動（既有，記為後續） |
| code S-2：淨結果沒變也打 stamp | 改（照已核准的宣告 5.11）：每個被碰到的 `Children` 記下階段開始時的內容與 stamp，結束時相同就還原；`[kr-children-unchanged-net]`、突變 `kr-net-stamp-not-restored` |
| code S-3：`[rb-r2-family]` 驗不到順序 | 改：第 1、2 步掛 51、50，回滾點時 1 的 `Children` 是 `[51, 50]`；最後 2 的 `Children` 是 `[51, 52, 50, 53]` |
| code S-4：交給新主人，舊主人死不死通知不同 | 改：Q3 的子 `ChildOf` 留到它的紀錄（換父是覆寫）；`[kr-handover-while-owner-dies]`、突變 `kr-rewired-dropped-early` |
| code N-1～N-7、N-9、N-10 | 改（N-2 連帶展開只沿 `ChildOf` 指著它的子；N-3 `Restore` 留下的子若這次也要刪就留給它自己的 despawn；N-4 重複檢查改成排序；N-5 `GYO_ASSERT`；N-6 自我 move；N-7 Host 路只在改到時標寫入；N-9 比對訊息；N-10 註解寫 64 位元） |
| code N-8：Host 的 `Trigger` 各輪裡的拒絕 path 標成 `ImmediateChange` | 記下，列舉沒有更貼切的值 |

N-2、N-3 沒有加案例與突變：N-2 只在 `RestoreSet` 給了前後不一致的 `ChildOf`／`Children`（作者違反 R-2 前提 8）時才看得到，照「誤用不加守衛」不補；N-3 要 `Restore` 同時刪父與 id 較大的子、而子的別的元件有讀 `ChildOf` 的 OnRemove hook，留給回滾那一側有需要時。

## 範圍的限界、後續

- 通用的 `Registrar::Relationship<R>`、一對一、指向自己、自訂集合、重排、走訪輔助、換父的通知：只定形（RC 26、27）。
- 讀檔的 `[kr-load]` 用 System 的延後放回代替「生成期間」的路：生成世界是編排層的，Kernel 這一側走同一條套用路。
- 負向編譯樣本「判法以外的骨架三處各一份」（批次表後續小事）：本批只把 Kernel 自己的兩份收成一個函式，world_logging、world_asset 不在範圍。
- `LogWorldLabel`（代碼審計 N2）不在本批（主會話同意另開小批）。
