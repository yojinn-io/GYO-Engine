# 編排層（`engine/orchestration`）

更新：2026-11-20（`v8-arch-gyo2-time-events` 第 6 步：O-1 拿掉 domain 的過渡句、O-4 補預設骨架、O-11 的 `WorldFailure` 欄位與「World 自己的事不經事件」一段；照[事前宣告](time-events-declaration.zh-Hant.md)第 10.2 節，寫法照實作，**最終文字在合併前核准時定稿**）。

**異常分級的草稿**（`v8-arch-gyo2-error-levels`，宣告 `error-levels-declaration.zh-Hant.md` 修訂 6，2026-11-20 動工前核准）：O-1、O-3、O-4、O-9、O-11 標「〔草稿〕」的句子（修訂 7 收窄後）；最終文字在合併前核准定稿。

Owner：架構。程式：`engine/orchestration/`（`GYO::Engine::Orchestration`，target `GYO::Orchestration`）。行為測試：`tests/common/orchestration/`。

由來：[編排層骨架的事前宣告](orchestration-skeleton-declaration.zh-Hant.md)（`v8-arch-gyo2-orchestration-skeleton`，2026-11-18 動工前核准）；分層與職責在[統合宣告](kernel-app-boundary-declaration.zh-Hant.md)第 3 節、第 9.2 節。**本頁的文字在合併前的合併前核准時定稿**（宣告第 12 節）。

編排層在 Kernel 之上、各 Module 之下：它把登記單位（`Module`、`Plugin`）與產品自己的遊戲程式接進每個 World，排遊戲初始化與每幀的流程，替換 World，記自己的警告。Kernel 不認得它（統合 L16、`v8-arch-gyo2-two-layers-like-bevy`）。

## Design Invariants

守衛欄：行為的測試照常是守衛；綁實作方式的掃描守衛延後（`v8-arch-gyo2-guards-after-mechanism-stable`；宣告第 7.4 節），延後的那幾條寫「延後」，這段期間靠本頁與審查守。

<a id="o-1"></a>
**O-1 遊戲初始化。** 一個 `Product` 一份遊戲初始化清單（schedule 名），行程一次、在所有 World 的第一幀與任何 `RunSchedule` 之前跑完、跑在第一個 World（描述的第一個；使用者 `v8-arch-gyo2-game-init-list`）。`RunFrame`、`RunSchedule` 在它還沒跑時先跑它；明確呼叫 `RunGameInit` 只准一次。清單的順序照插入（`Append`、`InsertBefore`、`InsertAfter`；插入當下解析錨點，錨點不在是啟動錯誤 O-E5；同名可以出現兩次）。任何一張回錯誤：停下、回傳、記為已跑（Product 就此結束，O-11）；有 World 壞了時不跑、不記為已跑。〔草稿，D10〕`BuildNewFirst` 替換第一個 World、遊戲初始化在還沒交出去的新 World 上失敗時，不算跑過（新 World 丟掉、舊 World 照跑、回 E2）；下一個入口照本條在舊的第一個 World 上跑。直接呼叫 `WorldExecutor::Run` 是繞過編排層，不先跑遊戲初始化。（「遊戲初始化的 schedule 要屬於一個 domain」的過渡句隨 Kernel 的 D-E12 拿掉，`v8-arch-gyo2-time-events` G2。）守衛：`orchestration.O-1` 各案（J-58 改寫）；scenario 第一段。（取代 Kernel 的 DI-KS 22）

<a id="o-2"></a>
**O-2 登記單位與生命週期。** 產品層一次 `Configure`（登記單位依序，產品自己的最後）；每個 World：用到的登記單位依序 `Build`（Module 先、各自照登記檔的順序；產品自己的 `build` 最後、而且一定經過一次 `Register`）→ Kernel `BuildPlans` → 依序 `Finish`（讀到版本 1 與 Plan）→ 依序 `Check`。Kernel 有啟動錯誤的 World 不跑 `Finish`、`Check` 與清單檢查。出錯的登記單位不用、其餘照做，錯誤一起收齊；有任何錯誤就不建任何 World。替換 World 時對那個 World 重走 `Build`～`Check`（`Configure` 不重跑），登記單位以 World 名為鍵記的資料在 `Build` 時重設。動態插入只走 Kernel 的兩步，不重跑 `Finish`、`Check`、也不通知——它們屬於登記單位自己的生命週期，System 不能重置它（使用者 `v8-arch-gyo2-dynamic-insert-no-lifecycle-rerun`）。World 描述的選項包只轉交、不解讀。守衛：`orchestration.O-2`、`O-5`、`O-E1`～`O-E7` 各案。

<a id="o-3"></a>
**O-3 runner。** 一個 `Product` 至多一個 runner（第二個是 O-E4），由登記單位或產品在 `Configure` 裝；沒有時用只跑一次的 runner（`RunFrame` 一次）。runner 擁有迴圈與執行緒的安排，只呼叫 `Product` 的執行入口（`RunGameInit`、`RunFrame`、`RunSchedule`、`ReplaceWorld`、`InsertSystems`），不排 System 的先後；Product 結束（O-11）之後，關掉程式或回主選單（另建一個 Product）由它決定。〔草稿，target/error-handling「Error levels」〕入口回的錯誤帶 `failure` 就是 E1（這個 Product 有 World 壞了，不能再跑）；不帶就是 E2（這次請求沒做成）。接下來怎麼辦由 runner 決定。〔草稿，退出流程（使用者 2026-11-21「照這個流程改。④ 也要在這一批補一個標準的 main 範例或輔助函式」、「記日誌和處理異常不在同一層」）〕Engine 自己的程式這樣用：預設 runner 跑一幀，失敗時回 `RunnerExit` `Failure` 與原因（E1 是 `WorldFailure` 的完整文字），自己不寫。接下來記不記、結束還是回主選單另建 `Product`，由產品自己的 main 決定；Engine 不給標準 main（使用者 2026-11-21：`RunProduct`、`ExitCode` 刪掉）。產品的 main 怎麼接 runner 的結果，例子在頂層 `examples/orchestration/product_main.cpp`（例子程式，不屬於 Engine，也不是測試；使用者 2026-11-21「加，這tmd是你的職責」）。守衛：`orchestration.O-3` 各案；「不讀 Plan 的步驟、不呼叫 System」的掃描（`orchestration.guard`，`v8-arch-gyo2-time-events` 第 1 步立）。

<a id="o-4"></a>
**O-4 職責。** **編排層的職責是組裝與流程**（統合 L17）。職責清單：登記單位與生命週期（含產品自己的登記）；依賴的讀取與檢查；`Product`（World 群、runner、World 的準備與替換，含帶過去）；遊戲初始化與每幀的清單；生成世界的流程（第 5 條）；交換點、extract 的時機、執行緒的安排（第 5 條）；啟動錯誤的彙整〔草稿，D2：拿掉「與編排層自己的警告」——編排層不寫日誌（O-11），清單少一項〕。（「World 的準備與替換」「帶過去」算原有的項目，不算多一項：使用者 2026-11-18「照建議」。）**編排層給 schedule 的預設骨架**（名字與段落順序；作者可換或關）並經 Kernel 執行它；不認得時間，也不認得任何 Module 掛進去的是什麼（`v8-arch-gyo2-orchestration-gives-schedule-skeleton`；這是「每幀的清單」那一項的預設值，**不算職責清單多一項，使用者 2026-11-19 核准**，`v8-arch-gyo2-time-events-declaration-approved`）。做法：每個 World 在任何登記單位的 `Build` 之前，由編排層（owner `orchestration`）宣告每幀一張 `Main`（stage 依序 `First`、`PreUpdate`、`FixedMainLoop`、`Update`、`PostUpdate`、`Last`）與固定步一張 `FixedUpdate`（stage 依序 `First`、`PreUpdate`、`Update`、`PostUpdate`、`Last`），名字在 `ScheduleSkeleton.hpp` 的 `DefaultSchedules`；每幀的清單沒寫 `Main` 時把它放在最前面，`FixedUpdate` 不進清單（由掛進去的 System 巢狀跑，沒人跑就不跑）；替換 World 時照樣再宣告。作者可以在 `Build` 裡往骨架插 stage；要換掉骨架，在 `WorldDescription::skeleton` 選 `ScheduleSkeleton::Off`（編排層不宣告，作者自己宣告）——開著骨架時作者再宣告 `Main` 是 Kernel 的 R-E2。Module 在登記時把 System 掛進去（O-3、RC-O8）。不帶任何第三方能力；不認得產品、策略的語意、時鐘、回滾。守衛：依賴只有 Kernel 與 Base；`orchestration.O-4` 各案（`SkeletonTests.cpp`，預設骨架）與突變 `or-skeleton-`；include 掃描（`orchestration.guard`）、`naming_guard` 的目錄對應、依賴的連結檢查（`gyo_expect_links(gyo_orchestration GYO::Base GYO::Kernel)`）已立（`v8-arch-gyo2-time-events` 第 1 步）；三條掃描（O3、O4、O6）與 `naming_guard` 的目錄對應各有實際程式碼樹上的陽性對照，O0（掃不到東西不放行）兩處各有突變（第 7 步依敵對式檢查補：`or-guard-real-*`、`or-guard-o0-*`、`or-guard-o4-dropped`、`or-guard-o4-unreadable-silent`）。

<a id="o-5"></a>
**O-5 啟動錯誤一次收齊。** Kernel 的 `StartupReport`（每個 World 一份，原封不動；Kernel 的 `StartupCode` 不擴充）、編排層的 `OrchestrationCode`（O-E1～O-E7）、各登記單位在 `Finish`／`Check` 回報的錯誤（它自己的碼）收成一份 `ProductStartupReport`；準備函式的錯誤（O-E7）在建好的 World 上才會出現，另成一份。執行時的錯誤 `OrchestrationError` 是另一組碼，準備函式回報的碼原樣帶出（存檔版本 Module 的拒絕靠它分辨）。守衛：`orchestration.O-5` 各案。

<a id="o-6"></a>
**O-6 依賴只在登記檔。** Module／Plugin 之間的依賴只寫在 `engine/config/modules.csv`、`plugins.csv`（使用者 `v8-arch-gyo2-module-deps-in-registers`）；`engine/` 的子目錄、引擎內 link 與產品的登記單位表由它產生；編排層建立 `Product` 時檢查（缺、在後、Module 依賴 Plugin 是 O-E3，不重排）。進 World 的 Module 照 `default` 欄預設帶、產品可以關；`default=no` 的與 Plugin 由產品列出。守衛：`cmake.UnitRegistry`（登記檔的讀取與驗證、產品帶哪些單位、產生的表與 link，`cmake -P`）、`orchestration.units`（產生的表建得起 `Product`）、`orchestration.O-E3` 的依賴各案；引擎 CMakeLists 不手寫引擎內 link 的掃描（`orchestration.guard`，`v8-arch-gyo2-time-events` 第 1 步立）。

<a id="o-7"></a>
**O-7 準備時機。** 每個 World 建好之後（啟動與每次替換）、第一次 Run 之前，照「帶過去 → 登記單位的準備 → 描述的準備 → 替換帶的 `extra`」的順序呼叫；準備放預設值時不蓋掉已經在的值；失敗時這個 World 不用。守衛：`orchestration.O-8` 的準備各案；讀檔案例。

<a id="o-8"></a>
**O-8 替換。** `ReplaceWorld` 照產品描述重新登記那個 World（新的 Registry；之前動態插入的一律重置；使用者 `v8-arch-gyo2-restore-reruns-registration`）；先後由作者選（使用者 `v8-arch-gyo2-replace-order-author-choice`）：預設 `BuildNewFirst`（建新的、準備、在新 World 上跑 `beforeSwap`，全部成功才銷毀舊的，任何一步失敗保留舊的），`DropOldFirst`（先銷毀舊的；失敗時位置是空的，Product 就此結束，O-11）。遊戲初始化不重跑；只在兩次 Run 之間；只換健康的 World——中斷的 World 與空的位置不能拿替換來救（O-11）。**編排層不驗證外部資料的內容**（存檔版本、素材、網路快照）：作者在發出 `ReplaceRequest` 之前驗證，選 `DropOldFirst` 也一樣；驗證失敗就回 `Result`、不發替換（使用者 `v8-arch-gyo2-broken-world-ends-product` 第 3 點；存檔版本 Module 與讀檔案例在批次表第 5 條）。存檔的版本轉換不在 Engine（`save-unaware-means-format-only`）；`BuildNewFirst` 的讀檔失敗是安全的。守衛：`orchestration.O-8` 各案、讀檔案例、scenario 第一段。

<a id="o-9"></a>
**O-9 動態插入的失敗。**〔草稿，D2〕Kernel 拒絕（`DynamicInsertRefused`）或建 Plan 失敗時，編排層把錯誤（E2，帶 World 名、owner 與 Kernel 的碼或報告）回給呼叫的人、World 照跑；**編排層不記**（原為「經 `Engine::Base::Log` 記一次 `Warning`」）；`build` 丟例外時先丟掉這一輪再往外傳（照舊）。守衛：`orchestration.O-9` 各案。

<a id="o-10"></a>
**O-10 帶過去。** 任何 Resource 型別都能用 `WorldSetup::CarryOver<T>()` 標記換 World 時帶過去；編排層照標記清單（型別身分＋型別化的搬移函式）泛用地搬，不認得特定型別、不看值的內容（使用者 `v8-arch-gyo2-carry-over-mechanism`）；在所有準備之前；`BuildNewFirst` 失敗時移回舊的 World；`DropOldFirst` 失敗時隨失敗的那次替換丟掉（Product 已結束，O-11；不再有暫存處）。**值裡不能存指向舊 World 的參照（`EntityId`、指標、句柄），這是作者的責任**；不立實作面的守衛，靠本頁與審查。守衛：`orchestration.O-10`、讀檔案例 6、scenario S1-10。

<a id="o-11"></a>
**O-11 一個 World 壞了，Product 就結束。** 任何一個 World 中斷（Kernel 回 Err 或例外離開它的 Run），或它的位置是空的（`DropOldFirst` 失敗），`Product` 就結束（使用者 `v8-arch-gyo2-broken-world-ends-product`；理由：一個 World 是一條鏈，正常不會失敗，失敗了還繼續就是吞錯誤）。`RunGameInit`、`RunFrame`、`RunSchedule`、`ReplaceWorld`、`InsertSystems` 一律先檢查所有 World，有壞的就什麼都不跑、不改，回 `WorldInterrupted` 或 `WorldMissing`（錯誤帶 `failure`，就是 E1），不論指名的是哪個 World——所以不留半幀（正在跑的那一幀，壞在中途時前面的 World 已經跑了，這是那一次的錯誤）。不准拿替換來救壞掉的 World；遊戲正常時的替換（讀檔）不受影響。壞掉的那一刻，編排層與 Kernel 收集 `WorldFailure`（Product 與 World、怎麼壞的：`Interrupted`／`ReplacementFailed`／`ResourceExhausted`、入口、schedule 與那個 World 的同步點序號（`syncEpoch`：跑那張 schedule 之前讀的 `World::SyncEpoch()`；不帶 domain 與 tick——編排層不認得時鐘與遊戲時間，作者要 tick 就放進自己的錯誤，`v8-arch-gyo2-time-events` G6）、Kernel 錯誤碼與錯誤文字（文字的層次帶 System 與步驟）或編排層的報告、發生前最後成功跑的 schedule），不在事後讀壞掉的 World；它掛在回傳的錯誤上（之後每次拒絕都帶同一份）、`Product::Failure()` 讀得到，錯誤回給 runner。〔草稿，P12〕例外（作者程式丟的）離開時照常往外，**拋出前把 failure 記好**（`DropOldFirst` 丟掉舊 World 之後的全部步驟也一樣；記錄用的 `WorldFailure` 在冒險之前先組好，記錄不配置）。**編排層不寫日誌**（入口與預設 runner；記不記由呼叫 runner 的產品 main 決定，見 O-3）：要不要記、怎麼記是日誌機制的事，日誌是獨立的機制、不嵌進功能（同日更正：日誌不寫進編排層——使用者「日誌不要嵌入到功能中呀，日誌是機制」；要不要記、怎麼記是日誌機制的事，日誌機制先不排批次）。在編排層外直接呼叫 `WorldExecutor::Run` 弄中斷的 World，下一個入口照樣看到、記為「outside the Product」。守衛：`orchestration.O-8` 的「ends the Product」「whole frame」「does not mend」「returned, not logged」「outside the Product」「bad_alloc」各案；`[or-ended-failure-tick]`（`syncEpoch` 等於壞掉前讀到的值、文字含 `sync point 1`；突變 `or-failure-no-tick`、`or-failure-text-no-tick`）；讀檔案例 4 (c)(d)；`orchestration.O-1` 的 D4。

**World 自己的事不經事件**（使用者 2026-11-19，`v8-arch-gyo2-world-events-not-via-event`；`-no-separate-eventbus` 的範圍收成「World 裡面的機制」）：通知只在發出它的 World 健康時經 Kernel 的事件發；致命錯誤與 World 自己的壞掉、建立、替換不經事件，結構化錯誤（`WorldFailure`）回給 runner，最後一道把它寫出去的路直接呼叫、不經任何機制。World 健康時的警告（例：`TimeDiscarded`）照常經事件，日誌照常訂閱；Kernel 的診斷紀錄（訊息的丟棄記錄 `DroppedMessages`、生成被拒 `RejectedSpawn`）照事件游標的宣告，World 記、runner 在兩次 Run 之間取走交給日誌（2026-11-20 更正：原本把丟棄記錄列在「經事件」的例子裡，和已核准的游標宣告不一致；使用者 `v8-arch-gyo2-logging-declaration-approved` 第 4 題）——所以日誌 Module 有兩個入口：平常訂閱事件（加上 runner 取走的診斷紀錄），致命時從 runner 拿結構化錯誤（不讓這條例外被讀成「凡是錯誤都不走事件」）。World 的建立與替換由編排層知道，本身不是錯誤；要讓新 World 裡的 System 知道（例：讀檔來的），在它準備好之後往裡面寫 `Message`（讀檔那一批定）。日誌 Module 怎麼接 runner 在日誌那一批定（定了：[日誌的宣告](logging-declaration.zh-Hant.md)第 5.4、5.6 節）。理由（使用者的）：錯誤的範圍與程度不知道，Event 機制本身可能已經壞了，經 Event 發、失敗時連日誌都會丟；後三點（健康的界線、最後一道直接呼叫、建立與替換不是錯誤）是 side chat 的補充，使用者說「補過去」。系譜：Bevy 的錯誤走回呼 `FallbackErrorHandler`、World 的建立與拿掉沒有事件；flecs 的錯誤直接呼叫日誌。守衛：沒有實作面的守衛（日誌那一批之前只是界線），靠本頁與審查。

### Replacement Criteria

符合任一條時，停下來交使用者「修還是重建」（L17 的形式：不數次數，出現就停）。

- **RC-O1**（原 Kernel RC 7「Host 又長回來」，主詞改成編排層）：第一個真實能力接進機制時仍要改編排層的迴圈或 Kernel 本體；或編排層以外的協調邏輯在兩個以上產品中重複。
- **RC-O2**：每加一種產品或 World 的組法都要改編排層的程式。
- **RC-O3**：runner 需要排 System 的先後才做得出某個 Feature。
- **RC-O4**：編排層開始 include 某個第三方或某個 Module 的內部。
- **RC-O5**：編排層的職責清單（O-4）要多加一項，或判不出某樣東西屬於哪一層。
- **RC-O6**：登記檔的欄位不夠用、需要第二份描述同一件事的檔，或 CMake 與編排層讀出的依賴不一致；或「只用函式庫、不要登記單位」的 World 被 O-E3 擋到。
- **RC-O7**：替換 World 的某個用途需要「不重建 World 就換掉內容」。
- **RC-O8**：scenario 的某一段為了通過，必須讓編排層知道某個 Module 或產品的語意。
- **RC-O9**：讀檔（替換 World）重跑登記的時間成為讀取時間的主要部分，或第 5 條的 extract 為了它要改 Kernel。
