# 回滾 Module：事前宣告（修訂 5）

> 性質：事前宣告（AGENTS §3 的 Architecture Delta；§2 的基盤 Architecture）。**使用者核准之前不實作、不改任何條文與守衛。**
> 批次代號：`v8-arch-gyo2-rollback-module`。
> **這是「Kernel 與編排層的邊界」拆出來的子份之一**（`v8-arch-gyo2-boundary-split-integrator-first`）。統合是邊界卡的 `kernel-app-boundary-declaration.zh-Hant.md`（分支 `claude/gyo2-kernel-app-boundary-4af68c`；修訂 4 照 16c84630，以第 15 節與 2.1 的新決定為準）。**以統合為準、不改統合定過的事。**
> **使用者 2026-11-17 核准**（HANDOFF `v8-arch-gyo2-rollback-module-declaration-approved`）。原話：本卡「核准」；主會話確認時「是，回滾那份要核准」。核准的版本是修訂 4 加補正（7dfd7167）。**核准本宣告不等於 AGENTS §14.4 的判定變更核准**：每一個守衛判定的變更（例：D-E16 的案從 Kernel 搬到回滾 Module），在實作那一批的事前宣告逐案列出、逐條核准。核准之後的第四輪換模型檢查（Fable／high，`four-docs-adversarial-check-r4.md`）要回滾份改幾處，照統合 35e8a1f3 第 16 節改成修訂 5；**修訂 5 的改動待使用者再看**（改動清單在修訂紀錄）。核准宣告不等於開始實作：實作照第 11 節的批次，排在統合第 8 節第四條線裡。

**修訂紀錄**：
- 修訂 0（b55af62e）：初版，回報統合六件。
- 修訂 1（829bc791）：照統合修訂 17 的判定（原語、回滾依賴時間、L22）。
- 修訂 2（d8125486）：照統合修訂 19 與第一輪四份交叉檢查（Derived、公開登記、存活集合）。
- 修訂 3（1875ffb1）：照統合修訂 20 與第二輪檢查（R-2 的前提、巢狀 schedule、Match）。
- 修訂 4（2026-11-17）：照統合 16c84630 第 15 節與使用者的新決定——重跑由 `ExclusiveWorld` 的 System 當場整批放回再巢狀跑固定步；事件分 `Message`／`Event`，`Message` 不回滾、Kernel 不加事件原語；伺服器權威不數寫者、讀寫規則兩半都拿掉，所以豁免寫者與「重跑者與還原者名單」拿掉、R-2 的前提全歸作者；官方策略的細部照統合 15.4；第三輪檢查（`four-docs-adversarial-check-r3.md`）第 3、6、17、18、19 條；第 10 節改成已定的紀錄。
- 修訂 5（2026-11-17）：照第四輪檢查與統合 35e8a1f3 第 16 節——(1) 寫明官方的還原、重跑 System 是不帶 `Writes<…>` 的 `ExclusiveWorld` System、記錄 System 只寫歷史 Resource，權威不需要回滾的名單；重跑退 tick 也走這條（5.5、7.2）。(2) `rollback.Input<T>` 與權威的 `.input` 是同一型別的兩種身分；沒有回滾策略的 World 上沒有作用（5.4、5.5、6.3）。(3)「重跑中」改成照 lightyear：決定回滾之後、還原之前立起，重跑完放下（5.3、5.5、R-2 的突變）。(4) 兩個方向的檢查與 R-7：hook 與 observer 算在觸發它的 System 所在的一側，從 World 外面觸發的不屬任何一側；hook 不能帶 `MessageReader`（7.1、R-7、5.5 末）。(5) 收統合移來的 RC 11（第 9 節）。(6) 第 0 節讀權威修訂 4；權威份 5.11 → 5.11；2.1 把讀寫規則拿掉分開列。

## 0. 依據

**讀的版本**：程式 `origin/gyo2` f00433b0；統合 16c84630（第 1、2.1、3、8、9.1、15 節）；權威份修訂 4（`claude/gyo2-authority-module` 2440b336）第 4.3、4.4、5.11、6.16 節；統合 35e8a1f3 第 16 節；游標份修訂 3（`claude/gyo2-event-cursor` bafc4050）；HANDOFF 在 `claude/gyo2-time-design`；讀檔卡的宣告修訂 5（`claude/gyo2-restore-entry-a4929d` a3caf87a）第 7、8 節；時間設計 `rebuild-time-design.zh-Hant.md`（修訂 4）第 4、6 節；舊規格 `docs/architecture/plans/kernel/spec.zh-Hant.md` A.1（Derived）、E 節（hook 的參數）、H.5、H.8、H.11（只帶設計思想，不照搬）；`target/kernel.zh-Hant.md` DI-KS 12。

**別家的做法**：
- 本份的證據：主 checkout `build/evidence/gyo2-rollback-module/rollback-other-engines.md`。lightyear 736e1695、Unity Netcode 7.2.0 由主會話讀原始碼；GGRS 25bd998、bevy_ggrs b2c42e8 由子 agent（web-fetch，Opus／medium）讀 GitHub 的原始碼，raw 檔沒有行號，只到檔名與項目。
- 統合卡的證據（同目錄 `../gyo2-kernel-app-boundary/`）：`rollback-placement-other-engines.md`（Bevy `set_last_changed`、Unity `CopyAndReplaceEntitiesFrom`、flecs snapshot addon、GekkoNet 回呼）、`section15-cross-engine.md`（第 15 節的七家對照）、三輪交叉檢查 `four-docs-adversarial-check*.md`。

---

## 1. 引用統合的關聯表

統合第 1 節的「事項」與「狀態」欄，摘到和本頁有關的句子。統合第 15 節取代的部分另外標出。

| # | 事項 | 狀態（統合原文摘） | 本頁怎麼用 |
|---|---|---|---|
| L1 | 分層與歸屬 | 已定 | 回滾 Module 是 Module：依賴編排層的登記單位型別與 Kernel；Kernel 不依賴它 |
| L2 | 時鐘與 tick 移出 Kernel，到時間的 Module | 已定（`v8-arch-gyo2-clock-out-of-kernel`） | 重跑時退 tick 是回滾和時間 Module 的交界（第 7.2 節） |
| L5 | 登記的生命週期與動態插入 | 已定：Kernel 提供「開 Registrar」「建 Plan」兩步、可重複呼叫……每個 World 建立時宣告准不准動態插入 | 重跑要用同一個 Plan；Plan 在中間被重建時歷史作廢（第 7.5 節） |
| L6 | Kernel 交出的唯讀登記資訊 | 已定：唯讀……帶版本號 | 啟動檢查讀它（Derived 的順序、跨 schedule 的訊息） |
| L10 | Registry 與 Plan 指紋改版 | 已定為後果 | 回滾的登記不進 Kernel 的指紋（第 5.7 節） |
| L11 | 回滾的機制放哪 | 已定（`v8-arch-gyo2-rollback-module-strategy`）：Kernel 只給原語……回滾是獨立的 Module……`RollbackStrategy` interface＋官方實作……P2P 佔位……。L11 欄裡「跑 hook 與不跑 hook 兩條路」「Ready 時從外面寫而且跑 hook」「在獨占 System 裡能不能用交使用者」三句，已由使用者的 `immediate-change-via-exclusive-world` 與統合 15.1、15.4 取代 | 本頁的範圍；第 6 節照新決定 |
| L13 | 事件游標 | 游標份；統合 15.1 判了名字（保留式 `Message`、推送式 `Event`＋`Observer`）與「事件進回滾快照：不進」 | 第 7.1 節 |
| L17 | 替換條件與歸屬的判斷方式 | 已定（`v8-arch-gyo2-layer-duty-lists`）：兩條核心……職責清單……對照同類型的多家實作 | 第 5.1 節的職責清單、第 9 節的替換條件；官方策略的細部由卡片照 L17 判 |
| L20 | Module 之間的依賴 | 已定：依賴只寫在登記檔……**回滾依賴時間 Module**（重跑要把 tick 退回） | 權威 → 回滾、回滾 → 時間都寫在登記檔 |
| L21 | 策略的構造 | 已定（`v8-arch-gyo2-strategy-interface-impl`）：interface 是契約、保證不變；官方實作和作者寫的走同一個插口、沒有特權；作者可以從頭實作，也可以繼承官方實作改裝 | 第 5.3、5.5 節 |
| L22 | 權威的登記與回滾的登記 | 統合照 L17 判：不同的事實、各自登記；伺服器權威策略可以替作者把它的權威元件登記進回滾 Module；權威只用回滾 Module 的公開登記 | 第 5.4 節（重複登記的規則）、第 7.3 節 |
| L23 | 網路／複製的 Module | 統合照 L17 判：歸 Engine 的網路 Module，搬運與交換由它做。L23 欄裡「網路 Module 要用豁免寫者的登記」隨 `authority-no-writer-count` 與 15.3 的讀寫規則拿掉而不需要 | 權威狀態送進回滾的入口由網路 Module 搬（第 5.5 節第 3 點） |
| L24 | 回滾相關的宣告與檢查歸誰 | 統合照 L17 判：可回滾的資料（Yes／No）與 Derived 是回滾 Module 的登記與檢查（D-E16）；Z6、Z8、D-E15 是伺服器權威策略讀回滾公開登記的交叉檢查；作者各自選兩個策略，伺服器權威只檢查相容；(4)「重跑時不跑」的公開登記；(5) 可回滾的 schedule 的登記處；(6) 代為登記要在建 Plan 之前。L24 欄裡 (3)「豁免寫者的登記、回滾公開重跑者與還原者」隨 15.3「判定資料的讀寫規則兩半都拿掉」而不需要 | 第 5.4、7.3 節 |
| L25 | 代為登記時的 owner | 統合照 L17 判：用作者的 `Registrar`，owner 是作者的登記單位 | 第 5.4 節 |
| L26 | 狀態機的接法 | 統合照 L17 判（修訂 21）：權威與狀態機互不依賴，作者用名字標記 | 回滾與狀態機也互不依賴；巢狀 schedule 由作者登記（第 5.4 節） |
| L27 | 時間 Module 公開 tick 的型別與驅動者 | 列為時間宣告的要求 | 重跑時寫的就是這個 tick（第 7.2 節） |
| L28 | L4 與 11-14 的漏斗 | 修訂 22：不用定了（伺服器權威不數寫者） | 不影響本頁 |
| L29 | 回滾策略選在哪一層 | 由統合 15.2 判：選在 World（建 World 時的選項）；沒有回滾策略的 World 就是不回滾、不報錯 | 第 5.7 節 |
| L30 | 「誰實際寫的」排錯工具 | 由統合 15.1 判：不在這一輪定，另外排一批 | 若做，回滾的還原寫入也會留記號；本頁不另要求 |

---

## 2. 已定的事

### 2.1 本頁依據的使用者決定（原話以 HANDOFF 與統合 2.1 為準）

| 代號 | 和本頁的關係 | 原話（摘） |
|---|---|---|
| `v8-arch-gyo2-rollback-module-strategy`（11-17） | 本頁的範圍：獨立 Module、interface＋官方實作、第一版只做狀態觸發、P2P 佔位、DI-KS 12 改寫成「Kernel 提供原語」 | 「本質上如果回滾有策略，就不能單單有官方默認實現，而是相當於一個interface、有官方實現，作者也可以自己複寫。否則作者無法達到自己的目的」 |
| `v8-arch-gyo2-strategy-interface-impl`（11-17） | interface 保證不變；官方實作沒有特權；繼承改裝是作者的選擇 | 「繼承官方實作改裝是選擇，而不是做法，你可以繼承改裝，也可以廢棄掉官方做法，而用自己的實現接上。」 |
| `v8-arch-gyo2-system-declares-immediate-structural-change`（11-17，side chat） | 不替回滾開 DI-KS 18 的例外；System 登記時宣告能不能當場改成員；回滾的重跑由可以改成員的特別 System 做 | 「這樣，不用開例外，建機制，System在初始化是給一個聲明，可以做成員變更（回滾）和不能做成員變更（普通System），並建立不同的判定和限制，這個選擇權和風險交給作者。你認為如何？」 |
| `v8-arch-gyo2-immediate-change-via-exclusive-world`（11-17） | 那個宣告沿用 `ExclusiveWorld`；DI-KS 18 改寫成「普通 System 只經同步點；`ExclusiveWorld` 可以當場改」 | 「沿用 ExclusiveWorld」 |
| `v8-arch-gyo2-events-two-kinds-like-bevy`（11-17，side chat） | 保留式改名 `Message`、新增推送式 `Event`＋`Observer` | 「OK，照Bevy的來」 |
| `v8-arch-gyo2-authority-no-writer-count`（11-17，side chat） | 伺服器權威不數寫者 | 「那就參照別人的權威策略」 |
| `v8-arch-gyo2-authority-rw-rules-removed-by-principle`（11-17；**卡片照使用者的原則判，不是使用者逐字定的**，統合 15.3） | 判定資料的讀寫規則兩半都拿掉；和上一列是兩筆不同的判定 | —（不是使用者的話） |
| `v8-arch-gyo2-confirm-derived-removals`（11-17；代號待主會話定） | 確認清單核准（統合 15.6） | 「核准」 |
| `v8-arch-gyo2-clock-out-of-kernel`（11-17） | tick 在時間 Module | 見統合 2.1 |
| `v8-arch-gyo2-time-plan-two` (5)（11-14） | 「事件：……可回滾的 schedule 不參與」——使用者定的只有這一句；兩個方向、依位置讀、走 Ingress、啟動時檢查是時間設計 6.3 (2) 的提案 | 見 HANDOFF |
| `v8-arch-gyo2-event-trim-by-cursor`、`-dynamic-insert-per-world`、`-read-only-registry-view`、`-module-deps-in-registers`、`-restore-entry-new-world-default`、`-no-runtime-interception`、`-products-pick-modules`、`-engine-not-rigid`、`-layer-duty-lists` | 前提（見統合 2.1） | — |

### 2.2 被改寫或要重看的較早內容

| 較早的內容 | 現在 | 本頁的處置 |
|---|---|---|
| DI-KS 12「回滾所需的狀態全在 World……Rewind／Restore／Replay 只由 Host 在 Ready 呼叫；Restore 不觸發 hook」（`target/kernel.zh-Hant.md:60`） | 統合 9.1：Kernel 提供原語、不提供 Rewind／Replay／Restore；使用者定重跑由 `ExclusiveWorld` 的 System 做 | 各句的去處見第 6.3 節 |
| DI-KS 18「結構變更只經同步點」 | 使用者定改寫成「普通 System 只經同步點；`ExclusiveWorld` 可以當場改」（`immediate-change-via-exclusive-world`）；改寫的文字、hook 時機、快照重取、限制由統合處理 | 本頁只用它（第 6 節） |
| 舊規格 H.5、H-5、H-8～H-11、H.11 取捨 2、3 | 舊設計，只帶思想 | 第 5.5、6.3 節逐條寫帶什麼 |
| 時間設計 6.3 (2) 的提案（兩個方向、依位置讀、走 Ingress） | 「依位置讀」被游標份拿掉；「走 Ingress」隨區移權威；兩個方向的檢查由回滾策略在啟動時做（統合 15.1） | 第 7.1 節 |
| 時間設計第 3.1 節「`FixedTime` 不放 tick……tick 要和事件緩衝一起被還原」 | 該節自己已註明待時間 Module 重新定；事件不進快照（統合 15.1） | 本頁只寫對時間 Module 的要求（第 7.2 節） |

---

## 3. 先用遊戲講

**PvP 射擊的 Client（第一版官方實作要支援的）。** 玩家按下前進，Client 不等伺服器，當場預測：第 100 個 tick 角色在 (10, 0)。伺服器的權威狀態晚 6 個 tick 才到，說第 100 個 tick 角色其實被爆炸推到 (9, 1)。每一幀開頭，回滾 Module 的一個 `ExclusiveWorld` System 比對自己存的歷史（第 100 個 tick 預測的是 (10, 0)），不一樣——當場把「可回滾」的那幾欄與活著的實體退回第 100 個 tick、套上伺服器的值，再用存著的輸入歷史把第 101～106 個 tick 巢狀跑一遍。重跑期間「送輸入給伺服器」「播腳步聲」這類副作用不能再做一次。重跑完角色的位置跳到新值；如果直接畫，畫面會瞬移，所以先記住重跑前畫面上的位置，接下來幾幀把差距慢慢收掉（平滑修正）。

**單機的時間倒流（作者自己寫策略的例子）。** 解謎遊戲按住倒帶鍵，世界往回退。作者寫自己的策略，每個 tick 存整個關卡的狀態，倒帶時一格一格往回放，不重跑。它用的 Kernel 原語和 PvP 一樣（讀整欄、整欄放回、存活集合），只是觸發與存法不同。這就是使用者說的「否則作者無法達到自己的目的」。

**格鬥遊戲的 P2P（第一版佔位）。** 兩個玩家直連，每個 tick 只送輸入；收到對手的輸入和預測不同時，退回那個 tick、整個狀態放回、重跑。第一版選這個策略，啟動時報「這個策略還沒實作」。

**PvP 的 Match、單機 2D 橫版。** Match 不回滾：建 World 時不選回滾策略，這個 World 就不回滾、不報錯。橫版遊戲連回滾 Module 都不選：沒有歷史、沒有回滾的啟動檢查。

---

## 4. 別家怎麼做（證據見第 0 節）

| 軸 | lightyear（Bevy 套件） | Unity Netcode for Entities | GGRS＋bevy_ggrs | GekkoNet／GGPO |
|---|---|---|---|---|
| 放在哪 | 網路套件的 `lightyear_prediction` crate | Netcode 套件 | 獨立的 crate＋Bevy 整合 crate | 獨立函式庫 |
| 觸發 | 收到權威狀態或遠端輸入，各有 Always／Check／Disabled；深度上限預設 20 | 每幀從最新快照還原、重新預測 | 遠端輸入和預測不同 | 同 GGRS |
| 存法、存在哪 | 逐元件歷史 `PredictionHistory<C>`（每個實體的元件），只記改過的 | 每個 World 一個 singleton 持有每個 chunk 的環，複製元件的原始位元組與 change version | 逐型別 `Strategy`（Copy／Clone／Reflect），每幀一份快照，存在 Resource | 遊戲的存讀回呼 |
| 還原的寫法 | 經 `Mut` 寫（標目前 tick）；不在的用延後命令 insert／remove（hook、observer 都跑） | 原始位元組寫回；沒變的不還原、不推版本 | 同 lightyear | — |
| 中途生成與刪除 | 預測期間刪實體改成停用、不真刪 | ghost 生成另有分類系統 | 刪除先停用；快照沒有的活實體刪掉、快照有的已不在就重新生成並改寫參照 | — |
| tick 怎麼退 | `LocalTimeline` 退 k 步、`Time<Fixed>` 換成回滾用的值，跑完換回 | 預測群組自己的 tick | `RollbackFrameCount` Resource 直接設 | 函式庫給 frame |
| 重跑在哪裡發動 | 一個獨占的 System 在 PreUpdate 裡跑 k 次 `FixedMain` | 預測群組在每幀的系統群組裡跑 | PreUpdate 的系統依請求跑存讀與推進 | 遊戲主迴圈 |
| 訊息 | 不回滾 | RPC 不回滾 | 不存；重跑會重寫；文件叫使用者別在回滾的 schedule 用訊息 | — |
| 輸入 | 另一個按 tick 索引的環（`input_buffer.rs`） | `ICommandData` 64 筆的環 | GGRS 給每幀的輸入 | 遊戲提供 |
| 副作用 | `Rollback` Resource＋`is_in_rollback`（在 core 的 timeline） | `NetworkTime.IsFirstTimeFullyPredictingTick` | 讀輸入的 schedule 不重跑 | — |
| 回滾後的畫面 | `CorrectionPolicy`（預設指數衰減） | `GhostPredictionSmoothingSystem` | 沒有內建 | — |
| 使用者怎麼換掉 | 設定值；系統集公開 | 設定值 | 逐型別 `Strategy` trait；系統集公開 | 回呼 |

**歸納**：沒有一家把回滾放在 ECS 核心；核心給的只有讀寫元件與 Resource、change stamp、原始位元組與版本。兩家 Bevy 套件都在一個拿到整個 World 的 System 裡發動重跑——GYO 的新決定（`ExclusiveWorld`）和它們同形。訊息都不回滾；輸入另存在按 tick 索引的環。「重跑中」的旗標兩家都放在時間那一側（lightyear 的 core timeline、Unity 的 `NetworkTime`）。

---

## 5. 回滾 Module 的形狀

### 5.1 職責清單（L17）

**回滾 Module 的職責是「把一個 World 的一部分狀態退回過去的某個步，再讓它重新走到現在」。** 清單：
1. 提供 `RollbackStrategy` interface 與官方實作（第 5.3、5.5 節）。
2. 公開的登記（第 5.4 節）：可回滾的資料、Derived、步的 schedule、巢狀跑的 schedule、重跑時不跑、輸入歷史。登記可以讀（權威的交叉檢查 Z6、Z8、D-E15、B3 讀它）。
3. 啟動時的檢查（策略自己的）：P2P 佔位報錯；Derived 的順序條件（取代 Kernel 的 D-E16）；`time-plan-two` (5) 兩個方向；巢狀 schedule 的所屬存在。
4. 觸發、存、還原、重跑、修正的流程（由策略實作）。

**不在清單上的**（放別層）：tick、時鐘與「重跑中」旗標（時間 Module，第 7.2 節）；誰是權威、digest（權威 Module）；收送網路資料（網路 Module）；訊息的保留與游標（Kernel）；schedule 與 stage 的名字、何時跑哪張（編排層與作者）；存檔格式（作者）；當場改成員的機制本身（Kernel 的 `ExclusiveWorld`）。

### 5.2 依賴方向

```
權威 Module ──→ 回滾 Module ──→ 時間 Module
                    │
                    ├──→ 編排層（登記單位型別）──→ Kernel
                    └──→ Kernel（原語、唯讀登記表、ExclusiveWorld）
```
- 回滾 Module 不依賴權威 Module、網路 Module、狀態機（單機的時間倒流也用它）。
- 權威 → 回滾、回滾 → 時間都寫在登記檔（L11、L20）。作者各自選權威策略與回滾策略，伺服器權威只檢查相容（L24）。
- Kernel 不依賴回滾 Module，也不認得「回滾」「重跑中」（統合 DI-KS 15）。

### 5.3 `RollbackStrategy` interface（契約，保證不變）

C++ 寫法照 L21：虛函式的抽象基底類別。官方實作與作者的實作走同一個插口。函式名與參數型別在實作批定，切法一旦核准就是契約。

| 掛點 | 什麼時候被呼叫 | 做什麼 | 對照 |
|---|---|---|---|
| 啟動檢查 | 編排層 build → Kernel 建 Plan → finish 之後（L5 的順序） | 讀唯讀登記表（L6）與回滾的公開登記，回傳啟動錯誤；P2P 佔位在這裡報錯 | lightyear plugin build |
| 記錄 | 步的 schedule 每跑完一步之後（記錄的 System 由回滾 Module 掛在步的 schedule 最後；stage 由登記時給） | 存這一步的狀態 | lightyear `UpdateHistory`；bevy_ggrs `SaveWorld` |
| 判斷要不要回滾 | 每一輪、步的 schedule 開始之前（回滾的 `ExclusiveWorld` System 裡） | 讀外面送進來的資料（權威狀態、遠端輸入），決定退回哪一步或不退；**決定回滾就立起「重跑中」** | lightyear `Check`；GGRS `check_simulation_consistency` |
| 還原 | 決定要回滾之後（同一個 `ExclusiveWorld` System，當場改；「重跑中」已立起） | 把狀態退回目標步；先記下要給修正用的值 | lightyear `Prepare`；bevy_ggrs `LoadWorld` |
| 重跑 | 還原之後（同一個 System 巢狀跑） | 退回 tick、巢狀跑 k 次步的 schedule；**重跑完放下「重跑中」** | lightyear `run_rollback`；「重跑中」的起落照 lightyear：判斷時插入 `Rollback` Resource（`rollback.rs:427`）、還原（`:880`）、重跑（`:1186`）、結束時拿掉（`:1324`） |
| 修正 | 每一幀（畫面那一側） | 依重跑前後的差，算這一幀畫面要加的偏移 | lightyear `VisualCorrection` |

- 作者可以只覆寫其中幾個（繼承官方實作改裝），也可以全部自寫（L21）。
- **外面送資料進來的入口**（權威狀態、遠端輸入）不在 interface 裡；官方的狀態觸發策略定義它自己的入口（第 5.5 節第 3 點）。這樣 interface 不為某一種觸發來源長出參數。

### 5.4 公開的登記（回滾 Module 的介面，不屬於某個策略）

作者呼叫，或伺服器權威策略在建 Plan 之前代為呼叫（L22、L24 (6)、L25：用作者的 `Registrar`，owner 是作者的登記單位）。

| 登記 | 意思 | 誰讀 | 取代 |
|---|---|---|---|
| **可回滾的資料**：`rollback.Component<T>(options)`、`rollback.Resource<R>(options)` | 登記了就是可回滾（Yes），沒登記就是不回滾（No）。options：比較函式（預設 `==`）、修正函式（選填）。型別要可複製（登記時編譯期檢查；Kernel 的 `kStorable` 只要求可移動，`Storage.hpp:60-63`）。**步的 schedule 上有生成時，配發器要登記成可回滾的 Resource**（否則重跑時 `Spawn` 拿到不同編號） | 策略；權威的 Z6、B3 | 舊 `Rollback::Yes／No` |
| **Derived**：`rollback.Derived<T>()` | 每一步在讀者之前由同一張 schedule 的立即寫者重算；不進歷史、還原時不動。啟動檢查：每張步的 schedule 與巢狀 schedule 裡，這種資料的每個讀者之前，順序圖上要有同一張 schedule 的 `Write<T>` 寫者，違反是啟動錯誤（讀 L6 的順序圖，統合 3.3）。系譜：舊規格 A.1 的 Derived（修訂 S15）；lightyear、bevy_ggrs 沒有對應的概念 | 策略；權威的 Z6 | 舊 `Rollback::Derived`、Kernel 的 D-E16（`Scheduler.cpp:500-525`） |
| **步的 schedule**：`rollback.StepSchedule(name, recordStage)` | 回滾的單位：記錄掛在它每跑完一步之後，重跑時巢狀跑 k 次的就是它 | 策略（記錄、重跑）；跨 schedule 的訊息檢查；權威的 Z6、D-E15 | 舊 `rollbackEnabled` |
| **巢狀跑的 schedule**：`rollback.NestedSchedule(name, stepSchedule)` | 從步的 schedule 裡巢狀跑的 schedule（例：作者的狀態值在兩端同一個 tick 自己換時，套用的 System 在固定步裡跑「進入／離開」那幾張）。重跑時會被跑到，所以算可回滾的一部分；**不記錄、不當重跑的單位**，只進檢查的範圍。要另外登記的理由：Kernel 的巢狀跑目標是執行時才給的名字（`ExclusiveWorld::RunSchedule(std::string_view)`，`Params.hpp:900`），啟動時從登記表推不出來 | 跨 schedule 的訊息檢查、Derived 的檢查；權威份 5.11 | （新） |
| **重跑時不跑**：`rollback.SkipDuringResimulation(...)` | 在 `AddSystem` 當下給那個 System 的 Mount 掛一個 Kernel 的一般執行條件（讀「重跑中」Resource；`Registry.hpp:125-138` 的 `conditions`，只能在 `AddSystem` 當下給，L24 (6)），並記進公開登記。用在輸入的入口、送網路、播音效 | 策略；權威的 D-E15（伺服器權威策略替作者把入口登記進來，L24 (4)） | 舊 `notInRollback` |
| **輸入歷史**：`rollback.Input<T>(capacity)` | 按 tick 索引、有上限的環（World 的 Resource，回滾 Module 持有）；入口 System 在一般的步寫進「目前的 tick」，重跑時策略照重跑的 tick 讀出。輸入歷史本身不回滾。**和權威的 `.input` 是同一個型別的兩種身分**（統合第 16 節）：權威的 `.input` 標它是判定的輸入（Z8 讀本頁的公開登記，輸入不能登記成可回滾的狀態）；`rollback.Input<T>` 宣告回滾策略替它保留按 tick 索引的歷史。**沒有回滾策略的 World（Match）上 `rollback.Input<T>` 沒有作用**：不建環、不改寫入，輸入照常是權威的判定輸入 | 策略；權威份 5.11 的 Z8 | 舊 `role = Input` 的 `InputRing`（統合 15.1：「回滾的策略也要自己的輸入歷史」，lightyear `input_buffer.rs:25-39`、Unity `ICommandData` 64 筆） |

- **誰登記巢狀 schedule**：作者（狀態機不依賴回滾，L26 的同形）；伺服器權威策略要不要在 `OnRegister` 代登記、要不要加交叉檢查，由權威份定。漏登記的後果：那幾張 schedule 不在檢查的範圍裡；回滾 Module 看不到巢狀跑，不能在執行時抓（寫進限制）。
- **重複登記的規則**（和權威份 4.3「作者的為準」統一，第三輪檢查第 19 條）：每個登記入口帶一個來源——作者自己登記，或代為登記（伺服器權威策略呼叫時標成代為）。不論先後，作者自己的登記為準、蓋過代為登記；兩筆都是作者自己的而 options 不同，是啟動錯誤；兩筆都是代為登記而 options 不同，也是啟動錯誤。「代為」不是特權（任何呼叫者都可以標，只代表「可以被作者蓋過」），所以不違反 L21 的「沒有特權」。
- 公開登記在啟動檢查之後不變（准動態插入的 World 另照第 7.5 節）。

### 5.5 第一版官方實作：狀態觸發、逐元件歷史、平滑修正

照 lightyear 的形狀，細部照統合 15.4 與卡片照 L17 的判斷：

1. **歷史存在 World 的 Resource**（統合 15.4；Unity 同形）：每個可回滾的型別一份按 tick 索引的環，上限＝最大回滾深度（設定值，預設照 lightyear 的 20；PvP 移植時依 64 Hz 與目標延遲另定）。新 World 天然沒有舊歷史（第 7.4 節）。
2. **記錄**：每步之後，對每個可回滾的欄，只記這一步改過的格（`changed` 大於上一次記錄時的 stamp）；刪掉的格記「這一步沒有」；存活集合的變化一起記。
3. **權威狀態的入口**：網路 Module（L23）或作者把「第 T 步的權威值」交給回滾 Module 的入口（Resource 或函式，實作批定）。
4. **判斷**：模式三種（照 lightyear）——一律回滾、比對後不同才回滾（預設）、關閉；比對＝權威值對歷史第 T 步。超過最大深度的請求丟掉並記診斷。
5. **還原**（在 `ExclusiveWorld` 的 System 裡當場做；判斷決定回滾時「重跑中」已立起，照 lightyear）：可回滾的每一欄與存活集合放回第 T 步；**沒變的格不寫、不推 stamp**（照 Unity，用登記的比較函式；卡片照 L17 判：少掉一批假的「改過了」，比較函式本來就登記了）；有變的格寫入並**標目前 stamp**；成員有變的**跑 hook**（統合 15.4，照 lightyear；hook 在當場改成員時的時機由統合處理）。Derived 不動。還原不算寫者（舊規格 B.3；伺服器權威也不再數寫者）。
6. **重跑**（同一個 System）：tick 退回 T（第 7.2 節）→ 巢狀跑 k 次步的 schedule（登記「重跑時不跑」的 System 被擋下）、每次照時間 Module 的規則推進 tick、讀輸入歷史的那一格 → 放下「重跑中」。
9. **官方的回滾 System 在啟動時看得到什麼**（統合第 16 節）：還原與重跑是一個**不帶 `Writes<…>` 標註**的 `ExclusiveWorld` System；記錄是只寫歷史 Resource 的一般 System。所以啟動時從登記表看不到它們寫判定資料或 tick——權威的 C 指紋、Z6、「tick 只有一個來源」的檢查都不必扣掉回滾，也不需要回滾公開名單。重跑時退 tick 也在這個 `ExclusiveWorld` System 裡做（第 7.2 節）。作者自寫的策略若給它的 System 加了 `Writes<…>`，權威怎麼看由權威份定。
10. **沒有回滾策略的 World**：記錄、判斷、還原、重跑都不跑；`rollback.Input<T>` 沒有作用（第 5.4 節）；公開登記照樣可讀（權威的交叉檢查在這種 World 不適用，權威份 5.11）。
7. **修正**：還原前記下每個有修正函式的格在畫面上的值；重跑後誤差＝舊值 − 新值，存在那個實體的修正元件上；每幀依衰減設定（預設指數，照 lightyear `CorrectionPolicy`）縮小；畫面讀「新值＋誤差」。修正元件與它的 System 屬回滾 Module，掛在每幀的哪個 stage 由作者決定。
8. **啟動檢查**：Derived 的順序（第 5.4 節）；巢狀 schedule 登記的所屬是步的 schedule；`time-plan-two` (5) 兩個方向（第 7.1 節），違反是啟動錯誤；副作用不檢查（統合 15.4）。

**卡片照 L17 判、不採的**：lightyear「預測期間刪實體改成停用、不真刪」。理由：GYO 的 Kernel 沒有「停用的實體」，要加就是 Kernel 職責清單多一項（L17 要停下問）；lightyear 這樣做是為了避開 OnRemove 在回滾時的副作用，但官方實作本來就選了還原時跑 hook，作者要的副作用改用 observer（下一點）。

**hook 讀不到「重跑中」**：hook 的參數只有 `HookCtx`、讀該實體的元件、`Commands`、訊息寫者、診斷（舊規格 E 節；`Params.hpp:982`），讀不到 Resource，**也不能帶 `MessageReader`**（統合 15.1 修訂 32：照 Bevy 的 hook 沒有游標）。所以 hook 只該維護衍生資料（例：空間索引，還原時重算正好），有副作用的反應（音效、特效）寫成 observer——observer 是 System，讀得到「重跑中」（統合 15.1）。寫進說明頁。

### 5.6 P2P 佔位：輸入觸發、存整個狀態

- 類別存在，啟動檢查一律回傳「這個策略還沒實作」的啟動錯誤（照 `v8-arch-gyo2-authority-strategy-mechanism`「啟動時報」的同一形狀）。
- **原語照它預見**（AGENTS §2：格鬥遊戲的 P2P 是使用者 11-16 點名、只佔位的 Feature）：「存整個狀態」要的原語（讀整欄、整欄放回、存活集合）包含在第 6 節，日後實作 P2P 不必再改 Kernel。

### 5.7 策略選在哪裡、「重跑中」、指紋

- **選在 World**（統合 15.2）：建 World 時的選項，在各 Module build 之前；沒有回滾策略的 World 就是不回滾、不報錯（Match）。產品選回滾 Module 或被權威的依賴帶進來，都一樣（不分開處理）。代為登記在建 Plan 之前做，那時策略已選定（統合 15.2）。
- **「重跑中」Resource 放在時間 Module**（統合 15.2 交本份考慮；卡片照 L17 判）：
  - 理由：讀它的有回滾、伺服器權威代登記的入口、作者的副作用 System 與 observer、日後的音效與網路 Module。放回滾 Module 時，這些都要依賴回滾 Module；lightyear 正是為了避免別人依賴 prediction 而放在 core 的 timeline（`timeline.rs:332-335`），Unity 放在 `NetworkTime`（`NetworkTime.cs:39-50`）。兩家都放在時間那一側。回滾已經依賴時間 Module（L20），寫它不必多一條依賴。
  - 規則：時間 Module 只要被建進某個 World，就在那個 World 登記它，值恆為「不在重跑」；只有回滾策略（官方或作者的）會寫它。沒有時間 Module 的 World（例：渲染 World）沒有它，那種 World 也不會有登記「重跑時不跑」的 System（那個登記要求 Resource 存在，啟動時檢查）。
  - 這是給時間 Module 宣告的要求（和 L27 同一類），時間 Module 的職責清單要多這一項；時間的宣告若判它不屬於時間，退回由回滾 Module 登記（同樣的「建起來就登記、恆在」規則）。回報統合（第 8.2 節）。
- Kernel 的 Registry 與 Plan 指紋不含回滾的登記（L10）；兩端要不要比回滾的登記屬權威的 C 指紋。

---

## 6. Kernel 給的原語（統合判定；本頁不定 Kernel 的介面）

起點：Kernel 現在在 World 是 Ready 時給外面逐格的讀寫——`Alive`、`Entities`、`Get`、`Keys`、`Stamps`、`GetMut`、`Insert`、`Remove`、`Despawn`、`Resource`、`ResourceMut`、`SetResource`、`RemoveResource`（`World.hpp:118-225`）與 `Stamp()`（`:267`）。

### 6.1 要的

| # | 原語 | 為什麼需要 | 現在有沒有 | 別家 |
|---|---|---|---|---|
| 1 | **整欄讀出**：依 id 順序走過一欄的（id、added、changed、值） | 記錄只記改過的格；存整個狀態 | 逐格有（`Keys`＋`Get`＋`Stamps`），要走三次、每次查找 | Unity chunk 原始資料與版本 |
| 2 | **整欄放回**：把一欄的成員與值一次換成給定的集合；沒變的格不動 | 還原；P2P 整個狀態 | 只能逐格；`Insert` 是插入排序 | Unity `CopyAndReplaceEntitiesFrom`；舊 GYO H.5 |
| 3 | **實體存活集合的讀出與放回** | 存活是 World 自己的一份清單 `entities_`（`World.cpp:89`、`:129-134`）；只放回欄時，第 T 步之後生成、現在還活著的實體會讓重跑的 `Spawn` 被丟成 `IdAlive` | 讀有；放回只能逐個 | bevy_ggrs 的實體快照 |
| 4 | **Resource 的存在與值一起讀寫** | 可回滾的 Resource（含配發器）退回時連「有沒有」一起退 | `SetResource`／`RemoveResource` 有 | bevy_ggrs `ResourceSnapshotPlugin` |
| 5 | **指定 stamp 寫回**：寫一格（或整欄）時由呼叫者給 `changed`（與 `added`） | 官方實作不用（它標目前 stamp、沒變就不寫）；作者自寫的策略可能要（例：倒帶時保留歷史的 stamp）。**統合照 L17 判留**（8a864c6c）：Bevy 核心本來就有 `set_last_changed`（`bevy_ecs/src/change_detection/params.rs:1546`），是 ECS 的能力；`engine-not-rigid` 不禁止別的做法。修訂 4 原本建議拿掉，收回 | 外面寫入一律標「下一個 stamp」（`WriteStamp()`，`World.hpp:350`），不能指定 | Bevy `set_last_changed` |
| 6 | **上面全部在 `ExclusiveWorld` 裡可以當場用**，寫入標目前 stamp，成員變化跑 hook | 重跑由 `ExclusiveWorld` 的 System 做（使用者 `immediate-change-via-exclusive-world`） | `ExclusiveWorld` 的閉包隨 L7 能讀寫任何值；當場改成員是 DI-KS 18 改寫後的新能力；hook 時機、快照重取、限制由統合處理 | lightyear、bevy_ggrs 在拿到整個 World 的 System 裡做 |

### 6.2 不要了（修訂 4；統合照 L17 判，8a864c6c）

- **「Ready 時從外面寫而且跑 hook」**：只在「runner 發動」時有人用；使用者定了在 `ExclusiveWorld` 裡做，不需要了。
- **「不跑 hook 的結構變更」那一條路**：統合修訂 17 准過「兩條路都給、由策略選」。現在官方實作定了跑 hook（統合 15.4），P2P 照 bevy_ggrs 也是跑；不跑 hook 的那條沒有點名的 Feature（AGENTS §1：不為將來可能需要而一般化），**統合同意拿掉**（Bevy 的 hook 也一律會跑）。外面在 Ready 時寫本來就不跑 hook（`World.hpp:181`），那是現有行為，不是為回滾加的。
- **事件的原語**（取出／放回訊息紀錄、序號、游標）：統合 15.1 判 `Message` 不回滾、Kernel 不加。

### 6.3 從 Kernel 拿掉的、舊 DI-KS 12 的去處

- `DiagnosticsBuffer::Entry::rollbackActive`（`Storage.hpp:229-233`）：拿掉（統合修訂 17 准）；`RejectedSpawn` 不加它；作者讀「重跑中」。
- `Rollback` 列舉、`rollbackEnabled`、`Mount::notInRollback`（`Registry.hpp:135-137`）、`rollbackSafe`、D-E16（`Scheduler.cpp:500-525`）：移到回滾 Module 的公開登記與檢查（第 5.4 節）。D-E16 的測試案跟著搬：`tests/common/kernel/SchedulerTests.cpp:586-593`、`CoverageTests.cpp:112-118`、`:307-313`（舊規格把它們編為 J-46，gyo2 的測試沒有這個編號）。「重跑第一步讀到重算值」那一案是新寫的（Kernel 沒有 Rewind／Replay 的實作，只有註解 `Registry.hpp:136`、`Storage.hpp:231`）。

| 舊 DI-KS 12 的句子 | 去處 |
|---|---|
| (c) System 不得有 Local | R-2 的前提，作者的責任（第 9 節） |
| domain tick 只有一個來源且在快照內 | tick 在時間 Module（L2）；重跑時由回滾策略退回（第 7.2 節） |
| 快照含 `rollback = Yes` 的欄與 Resource、配發器、Fixed domain 事件緩衝 | 欄、Resource、配發器：可回滾的資料的登記；事件緩衝：不進（統合 15.1） |
| 不含 role=Input 的歷史 | 輸入歷史由回滾 Module 自己存、不回滾（`rollback.Input<T>`，第 5.4 節）；「這是判定的輸入」是權威的 `.input`，同一型別的另一個身分；沒有回滾策略的 World 上 `rollback.Input<T>` 沒有作用 |
| 重跑與一次跑完逐位元相同（stamps 除外） | R-2（作者負責前提） |
| 可回滾 schedule 的 ingress 在重跑中不執行 | 「重跑時不跑」的登記 |
| 只由 Host 在 Ready 呼叫 | 改成由 `ExclusiveWorld` 的 System 做（使用者） |
| Restore 不觸發 hook | 改成跑 hook（統合 15.4） |

---

## 7. 和其他份的交界

### 7.1 事件（L13、統合 15.1）

- **`Message`（保留式）不回滾**；Kernel 不加事件的原語（統合 15.1；lightyear、Unity 兩家一致）。
- **推送式 `Event`** 不保留：重跑時重新觸發；有副作用的 observer 讀「重跑中」自己判斷。
- **`time-plan-two` (5)「可回滾的 schedule 不參與」照舊**，由回滾策略在啟動時檢查，兩個方向（官方實作違反是啟動錯誤）：
  - 步的 schedule 與巢狀 schedule 裡寫的 `Message`，不能有不在這些 schedule 裡的讀者（例：固定步寫「命中」、每幀讀來播音效）；
  - 步的 schedule 與巢狀 schedule 裡讀的 `Message`，不能有不在這些 schedule 裡的寫者（例：每幀寫 UI 指令、固定步讀）。
  - **hook 與 observer 算哪一側**（統合第 16 節）：算在觸發它的那個 System 所在的一側（固定步裡的 System 觸發的 observer 寫的訊息，算固定步那一側）；從 World 外面觸發的（外面在 Ready 時觸發）不屬任何一側，不進這兩個方向的檢查。hook 只會寫訊息、不能讀（不能帶 `MessageReader`）。
  - 讀 L6 交出的「每種訊息有哪些讀者、寫者」（游標份提、統合准）。這種資料要過界時，作者用 Resource 或元件，或從固定步觸發推送式事件、由 observer 讀「重跑中」。作者自寫的策略可以不檢查（L21）。
- 被回滾的那一側自己的讀者在重跑時讀到重寫的訊息；第 T 步寫、第 T+1 步讀的訊息，重跑時游標已推過、讀不到——這是 R-2 的前提之一（第 9 節）。

### 7.2 時間 Module（L2、L20、L27）

- 回滾在登記檔依賴時間 Module，直接寫時間 Module 公開的 tick（L27 的型別）把它退回 T，跑完回到現在；固定步的累加器（時間設計 `FixedTime`／`TickAccumulator`）不能退。
- **給時間 Module 宣告的要求**：
  1. tick 是回滾策略能寫的 Resource（L27 公開型別）。**重跑者用 `ExclusiveWorld` 退 tick，不經 tick 的寫者名單**（統合第 16 節）：時間宣告的「tick 只有一個來源」與權威的 tick 檢查看的是有型別的寫者，回滾的 `ExclusiveWorld` System 不帶 `Writes<…>`，不在那份名單裡、也不必加進去。tick 是判定資料又不登記成可回滾，Z6 對它例外（統合第 16 節、權威份 5.11）。
  2. 重跑時巢狀跑步的 schedule，每一步 tick 怎麼推進（步的 schedule 自己推，或時間 Module 給「推進一步」的函式讓重跑迴圈呼叫），在時間的宣告定；重跑時 System 讀到的固定步時間要等於第一次跑那一步時的值（lightyear `rollback_fixed_time` 的做法）。
  3. **「重跑中」Resource 由時間 Module 登記、恆在**（第 5.7 節）。

### 7.3 權威 Module（L22、L24、L25、統合 15.3）

- 方向：權威 → 回滾。回滾不讀權威的任何東西。
- **交叉檢查**：Z6、Z8、D-E15、B3 是伺服器權威策略讀回滾公開登記的檢查（L24；權威份 5.11）。本頁保證那些登記可讀、穩定（R-6）。
- **沒有豁免寫者、沒有「重跑者與還原者名單」**：伺服器權威不數寫者（`authority-no-writer-count`），讀寫規則兩半都拿掉（`authority-rw-rules-removed-by-principle`，統合 15.3），修訂 2、3 的名單拿掉。官方的回滾 System 不帶 `Writes<…>`，權威啟動時本來就看不到它寫判定資料（第 5.5 節第 9 點）。
- **輸入**：權威的 `.input` 與回滾的 `rollback.Input<T>` 是同一型別的兩種身分（第 5.4 節），兩份互引。
- **R-2 的前提不再由權威守**：讀寫規則拿掉後，權威不檢查判定的 System 讀什麼、用不用 `Local`；前提全部是作者的責任（第 9 節）。權威的交叉檢查只看登記（例：判定寫的資料有沒有登記成可回滾），和前提有部分重疊，但不守讀的那一半。
- **代為登記**：用作者的 `Registrar`、在建 Plan 之前（L24 (6)、L25）；重複登記的規則見第 5.4 節（作者的為準，和權威份 4.3 一致）。
- 權威狀態的入口：Client 收到伺服器覆蓋的那條路，搬運由網路 Module 做（L23）。

### 7.4 讀檔（讀檔卡的宣告第 7 節給本頁的要求）

| 讀檔卡的要求 | 本頁的回答 |
|---|---|
| 讀檔＝新 World，回滾的快照不跨世代 | 歷史存在 World 的 Resource（第 5.5 節第 1 點），新 World 天然沒有舊歷史。R-5 守 |
| 讀檔的路上 hook 照常；舊 Restore 不觸發 hook，共用之前要對齊 | 回滾的還原和讀檔的放回不共用：讀檔是作者的 System 用一般的 `Spawn`／`SpawnWithId`；回滾用第 6 節的原語。兩者都跑 hook，已經一致 |
| 重看 LIM-7（重放的生成撞上只帶 (b) 區元件的活實體） | 區已移出 Kernel，LIM-7 的區的寫法要改寫（統合減法批）。回滾這一側：還原時存活集合與配發器一起退回，重跑的 `Spawn` 拿到和第一次一樣的編號；會撞到的只有「帶不回滾資料的活實體佔了同一個編號」——本機專用的實體不和可回滾的實體共用編號空間，寫進回滾 Module 的限制 |
| `RejectedSpawn` 的 `rollbackActive` 由誰設、總數要不要扣除重放 | 統合修訂 17 定：Kernel 不加；總數照實計；作者讀「重跑中」 |

### 7.5 動態插入與唯讀登記表（L5、L6）

- 重跑要用和第一次跑相同的 Plan。官方實作記錄時連同登記表的版本號（L6）；目標步的版本號和現在不同 → 不回滾、記診斷、歷史作廢重新記。伺服器權威的 World 不准動態插入（L5），PvP 不會碰到。不需要 Kernel 新給東西。

---

## 8. 和統合的往來

### 8.1 已處理的（修訂 0～3）

| 修訂 | 統合的判定或檢查 | 本頁的處理 |
|---|---|---|
| 0 → 1 | 統合修訂 17：原語、回滾依賴時間、事件原語的形狀、L22、`rollbackActive` | 第 1、5.2、6、7 節 |
| 1 → 2 | 統合修訂 19、第一輪交叉檢查：Derived、公開登記、存活集合、列號、批次 | 第 5.4、6、11 節 |
| 2 → 3 | 統合修訂 20、第二輪：豁免只公開名單、D-E16 的實際測試位置、R-2 的前提、巢狀 schedule、Match | 第 5.4、6.3、9 節（名單在修訂 4 又隨 15.3 拿掉） |

### 8.2 修訂 4 的處理與回報

| 來源 | 處理 |
|---|---|
| 使用者：重跑在 `ExclusiveWorld` 的 System 裡、DI-KS 18 改寫 | 第 5.3、5.5、6.1 第 5 列；6.2 拿掉「Ready 時從外面寫而且跑 hook」 |
| 使用者：事件分兩種；統合：`Message` 不回滾、不加原語、(5) 照舊兩個方向、輸入歷史自己存 | 第 5.4（輸入歷史）、6.2、7.1 節 |
| 統合 15.3：讀寫規則兩半拿掉 | 拿掉豁免與名單；R-2 前提全歸作者（第 7.3、9 節） |
| 統合 15.4 與 15.2 的官方細部 | 第 5.5、5.7 節；Unity 的「沒變就不還原」採、lightyear 的「刪除改停用」不採（理由在 5.5） |
| 第三輪第 3 條（R-2 缺「步之間不被別人寫」） | R-2 前提 5 |
| 第三輪第 6 條（巢狀 schedule 一個登記兩種意思、沒人登記） | 分成 `StepSchedule`／`NestedSchedule`；刪掉「狀態機登記」的例子，寫明作者登記（第 5.4 節） |
| 第三輪第 17 條（tick 的字面、配發器） | R-2 前提 1 寫明 tick；配發器要登記成可回滾（第 5.4 節、R-2 前提 6） |
| 第三輪第 18 條（「重跑中」登記進哪些 World） | 第 5.7 節：放時間 Module，建進哪個 World 就登記在哪個 World |
| 第三輪第 19 條（代登記與作者登記的先後） | 第 5.4 節：作者的為準，不論先後 |

**回報統合、統合照 L17 判的**（8a864c6c，不問使用者）：
1. 第 6.2 節的兩項原語：「不跑 hook 的結構變更」拿掉；「指定 stamp 寫回」**留**（Bevy 核心的 `set_last_changed`；`engine-not-rigid`），移回第 6.1 節第 5 列。
2. 「重跑中」Resource 放時間 Module：同意，列進統合第 8 節「時間 Module 的宣告」的要求；時間宣告不收時退回由回滾 Module 登記、恆在。
3. 重複登記「作者的為準」：統合已轉權威份對齊 4.3。

---

## 9. Design Invariants 與 Replacement Criteria（草案；核准後寫進回滾 Module 的頁）

- **R-1 契約不變**：`RollbackStrategy` 的掛點與語意（第 5.3 節）是契約；官方實作的內部不保證不變（L21）。守衛：一個測試用的「從頭實作 interface」的策略，和官方實作跑同一組測試案例。
- **R-2 回滾等價**：從狀態 s 直接跑 k 步，與「跑到現在、退回第 T 步、重跑回現在」，可回滾的欄、Resource、實體存活集合逐位元相同（stamp 除外），**在下列前提下成立**：
  1. 步的 schedule 與巢狀 schedule 上的 System 只讀：可回滾的資料、Derived 的資料、輸入歷史、時間 Module 的 tick 與固定步時間（tick 不登記成可回滾的資料，由回滾策略直接退回，第 7.2 節）。
  2. 那些 System 不用 `Local`。
  3. 那些 System 不讀沒退回的時間（可縮放的時間、時鐘）。
  4. `Message` 在同一步內寫完讀完（`Message` 不回滾）；有副作用的 observer 讀「重跑中」。
  5. 可回滾的資料在兩個步之間，不被步的 schedule 與巢狀 schedule 以外的 System 寫（還原與權威狀態的入口除外）。（第三輪檢查第 3 條）
  6. 步的 schedule 上有生成時，配發器登記成可回滾。
  7. 輸入的入口登記成「重跑時不跑」。
  **誰守**：**作者的責任**（統合 15.4：lightyear、Unity Netcode 都不檢查決定性）。伺服器權威不再守（讀寫規則拿掉，統合 15.3）；它的交叉檢查（Z6、Z8、D-E15、B3）只看登記，和前提 1、6、7 有部分重疊。回滾 Module 不加新的檢查。說明頁寫清這七條（bevy_ggrs 也把 `Local`、訊息寫在 pitfalls）。
  守衛：回滾 Module 的等價測試（測試自己滿足全部前提）；突變：漏退配發器、漏退 tick、漏退存活集合、還原時漏掉被刪的格、「重跑中」沒立、「重跑中」在還原之後才立（還原時跑的 hook 觸發的 observer 讀到「不在重跑」）、重跑完沒放下。（舊 H-5 的思想）
- **R-3 不認得權威、網路、狀態機**：回滾 Module 不 include 它們。守衛：依賴掃描。
- **R-4 沒選就沒有**：產品不選回滾 Module 時，Kernel 與編排層裡沒有任何回滾的狀態或分支；World 沒選回滾策略時，沒有歷史、不記錄。守衛：Kernel 的掃描（統合附錄的 `rollback` 相關識別字 0 命中）；沒有策略的 World 的測試。
- **R-5 歷史不跨世代**：換 World 之後，舊 World 的歷史不能還原到新 World。守衛：測試。
- **R-6 公開的登記可讀、穩定**：可回滾的資料、Derived、步的 schedule、巢狀 schedule、重跑時不跑、輸入歷史，都經公開介面讀得到，啟動檢查之後不變（准動態插入的 World 另照第 7.5 節）。權威的交叉檢查依賴它。守衛：讀取介面的測試。
- **R-7 Derived 的順序條件**（取代 Kernel 的 D-E16）：步的 schedule 與巢狀 schedule 裡，Derived 的每個讀者之前，順序圖上有同一張 schedule 的立即寫者；違反是啟動錯誤。讀者含 observer（hook 讀得到該實體的元件，也算），算在觸發它的 System 所在的一側（統合第 16 節）；從 World 外面觸發的不屬任何一側。守衛：Kernel 現有的三處 D-E16 案搬來（有立即寫者通過、沒寫者或只有延後寫者失敗、不可回滾的 schedule 不檢查）；「重跑第一步讀到重算值」新寫；突變：只看直接邊不看路徑、把延後寫算成立即寫、漏掉 Resource、漏掉巢狀 schedule。
- **RC**：職責清單（第 5.1 節）要多加一項，或判不出某樣東西屬回滾還是別層（L17）；Kernel 為了回滾要多加第 6.1 節以外的原語；P2P 實作時發現第 6.1 節的原語不夠；**回滾等價破裂**（由 Kernel 的 RC 11 移來，統合第 9.1 節修訂 32）：R-2 的等價測試在 L1 平台因回滾 Module 以外的原因（Math、編譯器）失敗——停下，交使用者選「R-2 的『逐位元相同』改寫」或「官方實作支援的範圍縮小」，不在回滾 Module 裡繞過。舊 RC 11 的守衛 J-42 在 gyo2 沒有實作（Kernel 只有註解），現在的守衛是 R-2 的等價測試。

---

## 10. 已定的紀錄（修訂 0～3 列給使用者的題目都定了）

| 題目 | 定了什麼 | 誰定 | 依據 |
|---|---|---|---|
| 重跑在哪裡發動 | `ExclusiveWorld` 的 System 當場整批放回、再巢狀跑固定步；DI-KS 18 改寫，不開例外 | 使用者（`system-declares-immediate-structural-change`、`immediate-change-via-exclusive-world`） | Bevy `&mut World`、flecs `immediate`、Unity `EntityManager`（統合 2.1） |
| 回滾 → 時間的依賴 | 登記檔依賴時間 Module，不另立 interface | 統合照 L17（修訂 17） | AGENTS §1；lightyear 直接改 `Time<Fixed>` |
| 策略選在哪一層、Match、沒選策略 | 選在 World；沒有回滾策略的 World 就是不回滾、不報錯；依賴帶進來的不分開處理 | 統合照 L17（15.2） | Unity 建 World 時帶 flag |
| 事件進不進快照 | `Message` 不回滾、Kernel 不加原語；推送式重跑時重新觸發 | 統合照 L17（15.1），名字照使用者 `events-two-kinds-like-bevy` | lightyear、Unity 兩家一致 |
| 跨 schedule 的檢查多嚴 | `time-plan-two` (5) 照舊，回滾策略在啟動時檢查兩個方向（官方實作報錯） | 使用者的那一句；落點與兩個方向統合照 L17 | 第 7.1 節 |
| 時間設計 6.3 (2) 的「依位置讀」「走 Ingress」 | 拿掉（游標份；區移權威） | 統合照 L17 | 第 2.2 節 |
| R-2 的前提由誰守 | 作者的責任；回滾不加檢查；權威不再守 | 統合照 L17（15.3、15.4） | lightyear、Unity Netcode 都不檢查 |
| 歷史存哪 | World 的 Resource | 統合照 L17（15.4） | Unity 同形 |
| 還原跑不跑 hook、stamp | 跑 hook；標目前 stamp；沒變的不寫（卡片照 L17 加，照 Unity） | 統合照 L17（15.4）；最後一項卡片照 L17 | lightyear；Unity `GhostUpdateSystem.cs:1520-1530` |
| 「指定 stamp 寫回」留不留 | 留（官方不用，作者自寫的策略可用） | 統合照 L17（8a864c6c） | Bevy `set_last_changed` |
| 刪實體改停用 | 不採 | 卡片照 L17 | 第 5.5 節（要 Kernel 多一項職責） |
| 副作用的提醒 | 不檢查；作者讀「重跑中」 | 統合照 L17（15.4） | lightyear `is_in_rollback`、Unity `IsFirstTimeFullyPredictingTick` |
| 「重跑中」放哪 | 時間 Module（統合同意；時間宣告不收時退回回滾 Module） | 卡片照 L17，統合同意（8a864c6c） | lightyear core timeline、Unity `NetworkTime` |
| ingress 在重跑中不執行 | 「重跑時不跑」的公開登記，伺服器權威策略代登記 | 統合照 L17（L24 (4)） | — |

---

## 11. 批次草案（統合第 8 節第四條線「權威 Module ＋ 回滾 Module ＋ 拿掉權威規則」之內）

| 批 | 內容 | 依賴 | 上段／下段 | 實作者／審查者（模型／檔位） | 估計 |
|---|---|---|---|---|---|
| `v8-arch-gyo2-rollback-registrations` | 新 Module（位置照編排層骨架定）；登記檔一列、依賴時間 Module（L20）；公開登記六種（第 5.4 節）與重複登記的規則；`RollbackStrategy` interface（只有形狀）與 P2P 佔位的啟動錯誤；Derived 的檢查（取代 D-E16，三處案搬來）；`time-plan-two` (5) 兩個方向的檢查；R-3、R-4、R-6、R-7 的守衛 | Kernel 減法・結構段（L6 的順序圖、讀寫者清單）；編排層骨架；時間 Module（tick、「重跑中」）；Kernel 給回滾原語與 `ExclusiveWorld` 當場改成員（DI-KS 18 改寫） | 上段（公開介面、Design Invariants） | Opus／high；Fable／high；R-7 守衛的突變一覽另由 Fable／high 敵對檢查（§14.2） | 8～12 h（讀程式量估的） |
| `v8-arch-gyo2-rollback-official-strategy` | 官方實作的記錄、判斷、還原、重跑、修正、輸入歷史（第 5.5 節）；權威狀態的入口（網路 Module 之前先用測試餵）；R-1、R-2、R-5 的守衛 | 上一批 | 上段（R-2 是 Design Invariant 的守衛） | Opus／high；Fable／high；R-2 守衛的突變一覽另由 Fable／high 敵對檢查 | 16～24 h（讀程式量估的；不含效能量測） |

- 線內的先後照統合第 8 節：Kernel 給回滾原語 → 回滾的公開登記 → 權威 Module 的骨架 → 權威的交叉檢查（Z6、Z8、D-E15、B3）→ 拿掉 Kernel 的權威與回滾規則（`Rollback` 列舉、`rollbackEnabled`、`notInRollback`、`rollbackSafe`、D-E16、`rollbackActive`）。D-E16 的守衛在 `rollback-registrations` 接上之後才從 Kernel 拿掉，不會有空窗。
- 效能預算：本宣告沒有寫（PvP Client 的重跑成本＝k 步固定步的時間）。官方實作那一批的事前宣告要寫判定用的預算（AGENTS §2），數字待 PvP 移植的目標延遲與 64 Hz 定。

---

## 12. 範圍的限界

- 這是宣告，沒有寫程式、沒有量測。第 11 節的估計是讀程式量估的；第四條線的批次先後要主會話與使用者確認（統合第 8 節）。
- GGRS／bevy_ggrs 由子 agent 讀，沒有行號；主會話沒有抽查。GekkoNet 沒有重讀，引用統合卡的偵察。Unity Netcode 的還原只讀到用途說明與統合 15.4 引的行，沒有讀完寫回原始位元組的那幾行。
- 巢狀跑的深度：重跑時「回滾的 `ExclusiveWorld` System → 步的 schedule → 狀態值套用的 `ExclusiveWorld` System → 進入／離開的 schedule」是兩層巢狀；K0 的巢狀跑（DI-KS 16～21）准不准兩層、有沒有深度上限，實作批確認。
- `ExclusiveWorld` 當場改成員時 hook 的時機、驗證快照的重取由統合處理；本頁只要求「成員變化跑 hook」。
- 第 6 節的原語是「回滾需要什麼」，不是 Kernel 的介面設計；名字、參數、放在哪個型別，由統合與 Kernel 的實作批定。
