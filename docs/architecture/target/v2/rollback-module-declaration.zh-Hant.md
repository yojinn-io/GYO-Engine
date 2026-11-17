# 回滾 Module：事前宣告（修訂 1）

> 性質：事前宣告（AGENTS §3 的 Architecture Delta；§2 的基盤 Architecture）。**使用者核准之前不實作、不改任何條文與守衛。**
> 批次代號：`v8-arch-gyo2-rollback-module`。
> **這是「Kernel 與編排層的邊界」拆出來的子份之一**（`v8-arch-gyo2-boundary-split-integrator-first`）。統合是邊界卡的 `kernel-app-boundary-declaration.zh-Hant.md`（分支 `claude/gyo2-kernel-app-boundary-4af68c`，讀的是修訂 15，6ba6124d）。**以統合為準、不改統合定過的事**；本頁需要 Kernel 給的原語、和其他子份的交界、關聯表沒有的新關聯，都回報統合，由統合定（第 7、8 節）。
> 收口時三份子份一起送換模型檢查，由統合那張卡負責；本頁不自己送。

**修訂紀錄**：修訂 0（b55af62e）回報統合六件。修訂 1（2026-11-17）：統合修訂 17（a125c45b）照 L17 判了那六件——原語清單（Ready 時從外面可用）、拿掉 `rollbackActive`、回滾在登記檔依賴時間 Module、「可回滾的 schedule 不參與跨時鐘事件」的檢查落在回滾策略、新增 L22（權威與回滾各自登記）、「在獨占 System 裡能不能用」交使用者；本頁照它改第 1、5、6、7、8、10 節。事件原語的形狀引用游標份修訂 1。

## 0. 依據

**讀的版本**：程式 `origin/gyo2` f00433b0；統合修訂 15（6ba6124d），修訂 1 起照修訂 17（a125c45b）；游標份修訂 1（`claude/gyo2-event-cursor` c55f1253）第 4.8、11 節；HANDOFF 在 `claude/gyo2-time-design`（2416d4c0）；`recovered-from-stale-cards.zh-Hant.md`（同分支）；讀檔卡的宣告修訂 5（`claude/gyo2-restore-entry-a4929d` a3caf87a）第 7、8 節；時間設計 `rebuild-time-design.zh-Hant.md`（修訂 4，同 HANDOFF 分支）第 4、6 節；舊規格 `docs/architecture/plans/kernel/spec.zh-Hant.md` H.5、H.8、H.11（只帶設計思想，不照搬）；`target/kernel.zh-Hant.md` DI-KS 12。權威份在寫修訂 1 時還沒有推出檔案。

**別家的做法**：證據在主 checkout `build/evidence/gyo2-rollback-module/rollback-other-engines.md`（lightyear 736e1695、Unity Netcode 7.2.0 由主會話讀原始碼；GGRS 25bd998、bevy_ggrs b2c42e8 由子 agent〔web-fetch，Opus／medium〕讀 GitHub 的原始碼，raw 檔沒有行號，只到檔名與項目）。統合卡先前查的（Bevy `set_last_changed`、Unity `CopyAndReplaceEntitiesFrom`、flecs snapshot addon、GekkoNet 回呼）在 `build/evidence/gyo2-kernel-app-boundary/rollback-placement-other-engines.md`。

---

## 1. 引用統合的關聯表

以下逐字引用統合第 1 節的「事項」與「狀態」欄（修訂 15）。本頁以這些為前提。

| # | 事項（統合原文） | 狀態（統合原文，摘到和本頁有關的句子） | 本頁怎麼用 |
|---|---|---|---|
| L1 | 分層與歸屬（第 3 節） | 已定（第 2.1 節的決定） | 回滾 Module 是 Module：依賴編排層的登記單位型別與 Kernel；Kernel 不依賴它 |
| L2 | 時鐘與 tick 移出 Kernel，到時間的 Module | 已定（2026-11-17，`v8-arch-gyo2-clock-out-of-kernel`） | 回滾時 tick 怎麼退回，是回滾 Module 和時間 Module 的交界（第 7.2 節） |
| L5 | 登記的生命週期與動態插入 | 已定：Kernel 提供「開 Registrar」「建 Plan」兩步、可重複呼叫……每個 World 建立時宣告准不准動態插入，預設允許、不保證指紋一致 | 重跑時 Plan 必須和第一次跑的相同；Plan 在中間被重建時歷史作廢（第 7.5 節） |
| L6 | Kernel 交出的唯讀登記資訊 | 已定：交出唯讀的表……帶版本號、每次重建 Plan 加一 | 策略在啟動時讀它做檢查；用版本號判斷歷史還能不能用 |
| L10 | Registry 與 Plan 指紋改版、golden 重產 | 已定為後果 | 回滾 Module 的登記不進 Kernel 的指紋（第 5.6 節） |
| L11 | 回滾的機制放哪 | 已定（`v8-arch-gyo2-rollback-module-strategy`）：Kernel 只給原語（讀寫整欄、設 stamp、整批複製／替換之類，具體清單由回滾份回報統合定）；回滾是獨立的 Module……第一版官方只實作 PvP Client 要的「狀態觸發、逐元件歷史、平滑修正」，P2P 的「輸入觸發、存整個狀態」佔位、選了啟動時報錯；伺服器權威策略依賴回滾 Module | 本頁的範圍 |
| L13 | 事件游標 | 游標份修訂 0、1；統合修訂 16、17 判了它回報的介面 | 回滾時事件與游標怎麼辦，是本頁和游標份的交界（第 7.1 節） |
| L17 | 替換條件與歸屬的判斷方式 | 已定（`v8-arch-gyo2-layer-duty-lists`）：兩條核心……每層寫一份可核對的職責清單……判斷時要對照同類型的多家實作……替換條件改成「某層的職責清單要多加一項，或判不出屬於哪一層」就停下來問 | 第 5.1 節的職責清單、第 9 節的替換條件 |
| L20 | Module 之間的依賴 | 已定：依賴只寫在 Module／Plugin 登記檔的「依賴」欄……程式碼裡不另外寫 `Requires`……**回滾依賴時間 Module**（回滾份新關聯：重跑要把 tick 退回）：照 L20 寫在登記檔（修訂 17） | 權威 → 回滾、回滾 → 時間 都寫在登記檔 |
| L22 | 權威的登記與回滾的登記 | 統合 2026-11-17 照 L17 判：兩件是不同的事實（「誰說了算」與「能不能回到過去」），各自的 Module 各自登記，不違反 AGENTS §9；為了 PvP 的作者不寫兩次，伺服器權威策略可以替作者把它的權威元件登記進回滾 Module……權威不讀回滾的內部，只用回滾 Module 的公開登記 | 第 5.6、7.3 節 |
| L21 | 策略的構造（權威與回滾共用） | 已定（`v8-arch-gyo2-strategy-interface-impl`）：interface 是契約、保證不變；Engine 附官方實作，和作者寫的走同一個插口、沒有特權；作者可以從頭實作 interface，也可以繼承官方實作改裝……C++ 的寫法：虛函式的抽象基底類別 | 第 5.3、5.4 節 |

---

## 2. 已定的事

### 2.1 本頁依據的使用者決定（原話以 HANDOFF 為準）

| 代號 | 和本頁的關係 | 原話（摘） |
|---|---|---|
| `v8-arch-gyo2-rollback-module-strategy`（11-17） | 本頁的範圍：獨立 Module、interface＋官方實作、第一版只做狀態觸發、P2P 佔位、DI-KS 12 改寫成「Kernel 提供原語」 | 「本質上如果回滾有策略，就不能單單有官方默認實現，而是相當於一個interface、有官方實現，作者也可以自己複寫。否則作者無法達到自己的目的」「這2件事，照建議。」 |
| `v8-arch-gyo2-strategy-interface-impl`（11-17） | interface 保證不變；官方實作沒有特權；繼承改裝是作者的選擇、官方實作的內部不保證不變 | 「繼承官方實作改裝是選擇，而不是做法，你可以繼承改裝，也可以廢棄掉官方做法，而用自己的實現接上。」 |
| `v8-arch-gyo2-clock-out-of-kernel`（11-17） | tick 在時間 Module | 「剝離出去給其他Module實現最好，和權威一樣，作者可以選，不過能多Module是有依賴關係的，這個依賴需要處理。」 |
| `v8-arch-gyo2-event-trim-by-cursor`（11-16） | Kernel 照游標修剪事件 | 「它讓「不漏讀」成為 Kernel 自己的保證，不必依賴編排層排得對。」 |
| `v8-arch-gyo2-dynamic-insert-per-world`（11-17） | 執行中可能重建 Plan | 見統合 2.1 |
| `v8-arch-gyo2-read-only-registry-view`（11-17） | 唯讀、帶版本號的登記表 | 「可以」 |
| `v8-arch-gyo2-module-deps-in-registers`（11-17） | 依賴寫在登記檔 | 「這個照你建議。」 |
| `v8-arch-gyo2-restore-entry-new-world-default`（11-16） | 讀檔一律在新的 World；讀檔卡給本頁的要求見第 7.4 節 | 「Engine保護的是機制…提供默認支持就是（方案一）」 |
| `v8-arch-gyo2-no-runtime-interception`（11-17） | 伺服器權威改靠 digest 比對與伺服器覆蓋抓錯；Client 端「伺服器覆蓋」就是狀態觸發的回滾 | 「拿掉執行期的攔截，伺服器權威改靠 digest 比對與伺服器覆蓋來抓錯。」 |
| `v8-arch-gyo2-products-pick-modules`（11-17） | 單機可以不選回滾 Module | 「能，要不然單機遊戲也必須上權威了。」 |
| `v8-arch-gyo2-engine-not-rigid`（11-17，通用考量） | 預設方便、不禁止別的做法；很多開源都這樣做就不頭鐵 | 見統合 2.1 |
| `v8-arch-gyo2-layer-duty-lists`（11-17） | 職責清單、對照同類型多家 | 見統合 2.1 |

### 2.2 被改寫或要重看的較早內容

| 較早的內容 | 現在 | 本頁的處置 |
|---|---|---|
| DI-KS 12「回滾所需的狀態全在 World……Rewind／Restore／Replay 只由 Host 在 Ready 呼叫；Restore 不觸發 hook」（11-13 核准；`target/kernel.zh-Hant.md:60`） | 統合 9.1 已寫成「Kernel 提供回滾用的原語（清單由回滾份定），不提供 Rewind／Replay／Restore 與『哪些資料要回滾』的判斷」 | Kernel 這一側照統合；原來的各句歸到回滾 Module 或交使用者重判（第 6 節逐句列） |
| 舊規格 H.5、H-5、H-8～H-11、H.11 取捨 2、3 | 舊設計，只帶思想 | 第 6 節逐條寫「帶什麼、不帶什麼」 |
| 時間設計第 0 節「事件……可回滾的 schedule 不參與」與第 6.3 節第 (2) 條「可回滾的 schedule 不參與……啟動時檢查」（使用者 11-14 的 `-time-plan-two` (5)） | 寫的時候 Kernel 還認得 `rollbackEnabled`；統合已把它移出 Kernel | **統合修訂 17 判：內容不變、只換落點**——檢查由回滾策略讀唯讀登記表（L6；游標份讓 L6 交出「每種事件有哪些讀者」，統合准）在啟動時做（第 7.1 節） |
| 時間設計第 3.1 節「`FixedTime` 不放 tick……tick 要和事件緩衝一起被還原」 | 該節自己已註明「待時間 Module 那一批重新定」 | 本頁只寫回滾對時間 Module 的要求（第 7.2 節） |

---

## 3. 先用遊戲講

**PvP 射擊的 Client（第一版官方實作要支援的）。** 玩家按下前進，Client 不等伺服器，當場預測：第 100 個 tick 角色在 (10, 0)。伺服器的權威狀態晚 6 個 tick 才到，說第 100 個 tick 角色其實被爆炸推到 (9, 1)。回滾 Module 比對自己存的歷史（第 100 個 tick 預測的是 (10, 0)），不一樣——把「可回滾」的那幾欄退回第 100 個 tick、套上伺服器的值，再用存著的本機輸入把第 101～106 個 tick 重跑一遍。重跑期間「送輸入給伺服器」「播腳步聲」這類副作用不能再做一次。重跑完角色的位置跳到新值；如果直接畫，畫面會瞬移，所以先記住重跑前畫面上的位置，接下來幾幀把差距慢慢收掉（平滑修正）。

**單機的時間倒流（作者自己寫策略的例子）。** 解謎遊戲按住倒帶鍵，世界往回退。這不用伺服器：作者寫自己的策略，每個 tick 存整個關卡的狀態，倒帶時一格一格往回放，不重跑。它用的 Kernel 原語和 PvP 一樣（讀整欄、整欄放回、設 stamp），只是觸發與存法不同。這就是使用者說的「否則作者無法達到自己的目的」。

**格鬥遊戲的 P2P（第一版佔位）。** 兩個玩家直連，沒有伺服器。每個 tick 只送輸入；收到對手的輸入和預測不同時，退回那個 tick、存著的整個狀態放回、重跑。第一版選這個策略，啟動時報「這個策略還沒實作」。

**不選回滾的遊戲。** 單機 2D 橫版不選回滾 Module：沒有歷史、沒有「重跑中」的旗標、沒有回滾的啟動檢查。

---

## 4. 別家怎麼做（證據見第 0 節）

| 軸 | lightyear（Bevy 套件） | Unity Netcode for Entities | GGRS＋bevy_ggrs | GekkoNet／GGPO |
|---|---|---|---|---|
| 放在哪 | 網路套件的 `lightyear_prediction` crate | Netcode 套件 | 獨立的 crate＋Bevy 整合 crate | 獨立函式庫 |
| 觸發 | 收到權威狀態（state）或遠端輸入（input），各有 Always／Check／Disabled；深度上限預設 20 | 每幀從最新快照還原、重新預測（等於一律） | 遠端輸入和預測不同 | 同 GGRS |
| 存法 | 逐元件歷史 `PredictionHistory<C>`（元件），只記改過的 | 逐 chunk 複製元件的原始位元組與 change version | 逐型別 `Strategy`（Copy／Clone／Reflect），每幀一份快照，存在 Resource | 遊戲的存讀回呼存整個狀態 |
| 範圍 | 登記過的元件與 Resource；`DisableRollback` 的實體整個略過 | 預測的 ghost | 帶 `Rollback` 標記的實體 | 遊戲自己決定 |
| 還原的寫法 | 經 `Mut` 寫（標 changed）；不在的用 Commands insert／remove（觸發 hook、observer） | 原始位元組寫回 | 同 lightyear；文件明寫「每次還原都會觸發 change detection」 | — |
| 中途生成與刪除 | 刪除先停用不真刪；預先生成的實體依歷史退回 | ghost 生成另有分類系統 | 刪除先停用；快照沒有的活實體刪掉、快照有的已不在就重新生成並改寫參照 | — |
| tick 怎麼退 | `LocalTimeline` Resource 退 k 步、`Time<Fixed>` 換成回滾用的值，跑完換回 | 預測群組自己的 tick | `RollbackFrameCount` Resource 直接設 | 函式庫給 frame |
| 重跑在哪裡發動 | 一個獨占的 System 在 PreUpdate 裡跑 k 次 `FixedMain` | 預測群組在每幀的系統群組裡跑 | PreUpdate 的系統依請求跑 `LoadWorld`／`AdvanceWorld`／`SaveWorld` | 遊戲主迴圈 |
| 事件（訊息） | 沒找到回滾訊息的程式碼 | — | 不存；重跑會重寫；文件叫使用者別在回滾的 schedule 用訊息 | — |
| 副作用 | `Rollback` Resource＋`is_in_rollback` 條件；送複製、更新輸入的系統跳過 | 預測群組外的系統不重跑 | 讀輸入的 schedule 不重跑 | — |
| 回滾後的畫面 | `CorrectionPolicy`（預設指數衰減，可逐實體覆寫） | `GhostPredictionSmoothingSystem` | 沒有內建 | — |
| 使用者怎麼換掉 | 設定值；系統集公開可插 | 設定值 | 逐型別 `Strategy` trait；系統集公開；`InputPredictor` trait | 回呼 |

**歸納**（交叉比較，取能力不取技術債）：
- 沒有一家把回滾放在 ECS 核心（統合卡已查：Bevy、flecs v4、Unity Entities、Unreal Mass）。核心給的只有：讀寫元件與 Resource、設 change stamp（Bevy `set_last_changed`）、原始位元組與版本的讀取（Unity）。
- 兩家 Bevy 套件都**在一個 System 裡**發動重跑（巢狀跑 schedule），不是由主迴圈在 World 閒置時呼叫。GYO 的舊設計（H-9）是後者。
- 兩家 Bevy 套件還原時都**標 changed、觸發 hook**；Unity 寫原始位元組。GYO 舊設計（H-11）是「標目前 stamp、不觸發 hook」。
- **事件不回滾**是 bevy_ggrs 明寫的限制；GYO 舊設計把固定步的事件緩衝放進快照。
- 「策略」在別家多半是設定值加公開的掛點；只有 bevy_ggrs 的逐型別存法是 trait。使用者定的 interface 比它們更開放（整個策略可換），這是使用者的決定，不是別家的做法。

---

## 5. 回滾 Module 的形狀

### 5.1 職責清單（L17）

**回滾 Module 的職責是「把一個 World 的一部分狀態退回過去的某個步，再讓它重新走到現在」。** 清單：
1. 提供 `RollbackStrategy` interface 與官方實作（第 5.3、5.4 節）。
2. 回滾資料的登記：哪些元件、Resource 可回滾，各自怎麼存、怎麼比、要不要平滑（作者經回滾 Module 的登記函式宣告）。
3. 啟動時的檢查（策略自己的，例：P2P 佔位報錯；登記的型別能不能複製）。
4. 提供「重跑中」的旗標 Resource，讓作者的副作用 System 自己加條件跳過。
5. 觸發、存、還原、重跑、修正的流程（由策略實作；Module 提供把策略掛進 schedule 的 System）。

**不在清單上的**（放別層）：tick 與時鐘（時間 Module）；誰是權威、digest（權威 Module）；收送網路資料（網路 Module）；事件的保留與游標（Kernel，游標份）；schedule 與 stage 的名字、何時跑哪張（編排層與作者）；存檔格式（作者）。

### 5.2 依賴方向

```
權威 Module ──→ 回滾 Module ──→ 編排層（登記單位型別）──→ Kernel
                    │                                        ↑
                    ├──→ Kernel（原語、唯讀登記表）───────────┘
                    └──→ 時間 Module（第 7.2 節；L20，統合修訂 17）
```
- 回滾 Module 不依賴權威 Module、網路 Module（單機的時間倒流也用它）。
- 權威 → 回滾、回滾 → 時間都寫在登記檔（L11、L20）。統合不另立「步數讀寫」的 interface（只有回滾一個用的人，AGENTS §1；lightyear 也直接改 `Time<Fixed>`）。所以選回滾的產品一定也選時間 Module。
- Kernel 不依賴回滾 Module，也不認得「回滾」「重跑中」（統合 DI-KS 15）。

### 5.3 `RollbackStrategy` interface（契約，保證不變）

C++ 寫法照 L21：虛函式的抽象基底類別。官方實作與作者的實作走同一個插口（回滾 Module 的 `UseStrategy`）。下面是**職責**的切法；函式名與參數型別在實作批定，但切法一旦核准就是契約。

| 掛點 | 什麼時候被呼叫 | 做什麼 | 對照 |
|---|---|---|---|
| 啟動檢查 | 編排層 build → Kernel 建 Plan → finish 之後（L5 的順序，和權威檢查同一時機） | 讀唯讀登記表（L6）與回滾 Module 的登記，回傳啟動錯誤；P2P 佔位在這裡報錯 | lightyear 的 plugin build；權威策略的檢查 |
| 記錄 | 可回滾那張 schedule 每跑完一步之後 | 存這一步的狀態（存法由策略定） | lightyear `UpdateHistory`；bevy_ggrs `SaveWorld` |
| 判斷要不要回滾 | 每一輪、可回滾的那張 schedule 開始之前 | 讀外面送進來的資料（權威狀態、遠端輸入），決定退回哪一步或不退 | lightyear `Check`；GGRS `check_simulation_consistency` |
| 還原 | 決定要回滾之後 | 把狀態退回目標步；重跑前記下要給修正用的值 | lightyear `Prepare`；bevy_ggrs `LoadWorld` |
| 重跑 | 還原之後 | 立起「重跑中」旗標、退回 tick、跑 k 步、放下旗標 | lightyear `run_rollback`；bevy_ggrs `AdvanceWorld` 迴圈 |
| 修正 | 每一幀（畫面那一側） | 依重跑前後的差，算出這一幀畫面要加的偏移 | lightyear `VisualCorrection` |

- 作者可以只覆寫其中幾個（繼承官方實作改裝），也可以全部自寫（從頭實作 interface）——L21。
- **外面送資料進來的入口**（權威狀態、遠端輸入）不在 interface 裡：送什麼、什麼格式是策略與網路 Module 之間的事。官方的狀態觸發策略定義它自己的入口（第 5.4 節）。這樣 interface 不為某一種觸發來源長出參數。
- **「重跑在哪裡發動」**——在一個獨占 System 裡巢狀跑，或在 World 閒置時由外面（runner）呼叫——是統合第 10 節第 12 件，由統合問使用者；它決定 Kernel 的原語要在什麼情境下可用（第 6 節）。

### 5.4 第一版官方實作：狀態觸發、逐元件歷史、平滑修正

照 lightyear 的形狀（它是 PvP Client 最接近的成熟參考），改成 GYO 的機制：

1. **登記**：`rollback.Component<Position>(options)`、`rollback.Resource<R>(options)`。options：比較函式（預設 `==`）、修正函式（選填；例：位置用線性內插）。型別必須可複製（登記時編譯期檢查；Kernel 的 `kStorable` 只要求可移動，`Storage.hpp:60-63`）。
2. **記錄**：每步之後，對每個登記的欄，只記這一步改過的格（用 stamp 判斷：`changed` 大於上一次記錄時的 stamp）；刪掉的格記「這一步沒有」。歷史是有上限的環，上限＝最大回滾深度（設定值，預設照 lightyear 的 20；PvP 移植時依 64 Hz 與目標延遲另定）。
3. **權威狀態的入口**：網路 Module（或作者）把「第 T 步的權威值」交給回滾 Module 的入口（一個 Resource 或函式，實作批定）。
4. **判斷**：模式三種（照 lightyear）——一律回滾、比對後不同才回滾（預設）、關閉；比對 = 權威值對歷史第 T 步。超過最大深度的回滾請求丟掉並記診斷。
5. **還原**：登記的每一欄，把第 T 步的成員與值放回（第 T 步沒有的格移除、有的放回）；作者標成「不回滾」的實體不動（照 lightyear `DisableRollback`）。配發器若被登記成可回滾的 Resource 就一起退回，重跑時 `Spawn` 拿到同樣的編號（舊 H.5 的思想）。還原觸不觸發 hook、stamp 怎麼標，是第 10 節第 2、3 件。
6. **重跑**：「重跑中」旗標立起 → tick 退回 T（第 7.2 節）→ 跑 k 次可回滾的那張 schedule → 旗標放下。作者的輸入歷史（例 `InputRing`）不登記成可回滾，重跑時照 tick 讀（舊 H.5 的思想：輸入歷史不回滾）。
7. **修正**：還原前記下每個有修正函式的格在畫面上的值；重跑後誤差 = 舊值 − 新值，存在那個實體的修正元件上；每幀依衰減設定（預設指數，照 lightyear `CorrectionPolicy`）縮小；畫面那一側讀「新值＋誤差」。修正元件與它的 System 屬回滾 Module，掛在每幀的哪個 stage 由作者或編排層決定。

### 5.5 P2P 佔位：輸入觸發、存整個狀態

- 類別存在（`P2PInputRollbackStrategy` 之類），啟動檢查一律回傳「這個策略還沒實作」的啟動錯誤（照 `v8-arch-gyo2-authority-strategy-mechanism`「啟動時報」的同一形狀）。
- **原語清單照它預見**（AGENTS §2 基盤：格鬥遊戲的 P2P 是使用者 11-16 點名、只佔位的 Feature）：「存整個狀態」用到的原語（讀整欄、整欄放回）要包含在第 6 節的清單裡，日後實作 P2P 不必再改 Kernel。

### 5.6 選策略、登記、指紋

- 產品選了回滾 Module（L15），在它的 build 裡 `UseStrategy(...)`。沒選策略時怎麼辦是第 10 節第 5 件。
- 回滾 Module 的登記（哪些型別可回滾）存在回滾 Module 自己那裡，不寫進 Kernel（照 `authority-own-registration` 的同一形狀：Kernel 不存標籤）。
- **權威替作者登記**（L22）：登記函式 `rollback.Component<T>(options)` 是回滾 Module 的公開介面；伺服器權威策略可以替作者呼叫它，把權威元件登記成可回滾。回滾 Module 不知道是誰呼叫的（同一個入口、沒有特權，和 L21 同理）；作者自己登記過的型別再被權威登記一次時，照「同一型別登記兩次」的規則處理（實作批定：沿用第一次的 options，或兩次的 options 不同時是啟動錯誤）。
- Kernel 的 Registry 與 Plan 指紋不含回滾的登記（L10）。兩端要不要比回滾的登記，屬權威份的 C 指紋（L12）。

---

## 6. 需要 Kernel 給的原語（回報統合定，本頁不定 Kernel 的介面）

**統合修訂 17 的判定**（照 L17，不問使用者）：下表前五列 Kernel 給，**在 World 是 Ready 時從外面可用**；第六列「在獨占 System 裡也能用」動到 DI-KS 18，**統合交使用者**（統合第 10 節第 12 件，就是本頁原來第 10 節第 1 件「重跑在哪裡發動」）。拿掉 `DiagnosticsBuffer::Entry::rollbackActive`——准。名字、參數、放在哪個型別仍由 Kernel 的實作批定。

起點：Kernel 現在在 World 是 Ready 時已經給外面逐格的讀寫——`Alive`、`Entities`、`Get`、`Keys`、`Stamps`、`GetMut`、`Insert`、`Remove`、`Despawn`、`Resource`、`ResourceMut`、`SetResource`、`RemoveResource`（`World.hpp:118-225`），以及目前的 stamp `Stamp()`（`:267`）。從外面寫不跑 hook（`Despawn` 的註解「no hook runs」，`:181`）。以下是回滾還缺的。

| 原語（描述性名稱） | 為什麼需要 | 現在有沒有 | 別家 | 誰用 |
|---|---|---|---|---|
| **整欄讀出**：依 id 順序走過一欄的（id、added、changed、值） | 記錄只記改過的格；存整個狀態 | 逐格有（`Keys`＋`Get`＋`Stamps`），要走三次、每次查找 | Unity chunk 原始資料與版本 | 官方、P2P、時間倒流 |
| **整欄放回**：把一欄的成員與值一次換成給定的集合 | 還原；P2P 整個狀態 | 只能逐格 `Insert`／`Remove`；每次 `Insert` 是插入排序（`Storage.hpp` `Put`） | Unity `CopyAndReplaceEntitiesFrom`；舊 GYO H.5「整欄替換」 | 官方、P2P、時間倒流 |
| **指定 stamp 寫回**：寫一格（或整欄）時由呼叫者給 `changed`（與 `added`），或明確要求「標目前 stamp」 | 第 10 節第 3 件兩個選項都要它：保留歷史的 stamp，或標目前 stamp | 外面寫入一律標「下一個 stamp」（`WriteStamp()`，`World.hpp:350`），不能指定 | Bevy `set_last_changed`（文件寫「主要給回滾網路策略用」） | 官方、作者自寫 |
| **Resource 的 stamp 與存在一起讀寫** | 可回滾的 Resource（含配發器）退回時連「有沒有」一起退 | `SetResource`／`RemoveResource` 有；stamp 不能指定 | bevy_ggrs `ResourceSnapshotPlugin` | 官方、P2P |
| **在獨占 System 裡也能用上面全部** | 若統合第 10 節第 12 件選「在獨占 System 裡巢狀跑」（統合問使用者） | `ExclusiveWorld` 的閉包隨 L7 能讀寫任何值；結構變更（插入、移除）只經同步點（DI-KS 18）——整欄放回是結構變更 | lightyear、bevy_ggrs 都在 System 裡用 `&mut World` | 官方 |
| **不跑 hook 的結構變更**與**跑 hook 的結構變更**兩條路都在 | 第 10 節第 2 件由策略選；Kernel 兩條都給，才不替作者決定 | 從外面寫＝不跑；System 經 Commands＝同步點跑 | lightyear／bevy_ggrs 跑；Unity 原始位元組不跑 | 官方、作者自寫 |

**看使用者怎麼判才需要的**：第 10 節第 4 件選「事件回滾」時，原語多一項——**取出／放回一種事件的紀錄、寫入序號、讀者游標**（游標份修訂 1 第 11 節第 3 條、統合 L11 修訂 17）。Kernel 仍只有游標一種讀法；要第二種讀法就是 Kernel 職責清單要多一項，照 L17 停下來問（游標份第 4.8 節）。

**不需要 Kernel 新給的**（回滾 Module 自己做得到）：
- 逐型別的複製函式：回滾的登記是有型別的（`rollback.Component<T>`），Module 自己生成；Kernel 不必要求元件可複製。
- 配發器：它是產品登記的 Resource（讀檔卡的宣告第 5.1 節），用上面的 Resource 原語退回。
- 「重跑中」旗標：回滾 Module 的 Resource，Kernel 不認得。

**要從 Kernel 拿掉的**（和統合一起定）：
- `DiagnosticsBuffer::Entry::rollbackActive`（`Storage.hpp:229-233`）：Kernel 不再知道回滾。拿掉，作者要分辨時讀回滾 Module 的「重跑中」旗標（**統合修訂 17 准**）。這也回答讀檔卡要求的「`rollbackActive` 由誰、在哪裡設」（第 7.4 節）。
- `Rollback` 列舉、`rollbackEnabled`、`notInRollback`、`rollbackSafe`：統合 3.2 已列移出，本頁接手它們的思想（第 5.4 節第 6 點用旗標加條件取代 `NotInRollback`；「有副作用的 System 在重跑中要跳過」由作者加條件，回滾 Module 的啟動檢查要不要提醒，是第 10 節第 4 件）。

**舊 DI-KS 12 各句的去處**：

| 舊句 | 去處 |
|---|---|
| (c) System 不得有 Local | 權威份（區移出）；回滾 Module 的說明頁寫「`Local` 不回滾」（bevy_ggrs pitfalls 同） |
| domain tick 只有一個來源且在快照內 | tick 在時間 Module（L2）；退回 tick 是第 7.2 節的交界 |
| 快照含 `rollback = Yes` 的欄與 Resource、配發器、Fixed domain 事件緩衝 | 欄、Resource、配發器：官方實作的登記；事件緩衝：第 7.1 節的交界 |
| 不含 role=Input 的歷史 | 官方實作的說明：輸入歷史不登記成可回滾 |
| 重跑與一次跑完逐位元相同（stamps 除外） | 回滾 Module 的 Design Invariant（第 9 節），守衛在回滾 Module 的測試 |
| 可回滾 schedule 的 ingress 在重跑中不執行 | 作者用「重跑中」旗標加條件；強不強制是第 10 節第 4 件 |
| 只由 Host 在 Ready 呼叫 | 統合第 10 節第 12 件（統合問使用者） |
| Restore 不觸發 hook | 第 10 節第 2 件 |

---

## 7. 和其他份的交界（本頁的要求；誰定見第 8 節）

### 7.1 事件游標（L13）

- **問題**：重跑第 T+1～T+k 步時，第 T 步寫的、要給第 T+1 步讀的事件，讀者的游標第一次跑時已經讀過了；重跑時讀不到 → 重跑的結果和第一次不同。反過來若連游標一起退回，第一次跑時另一張 schedule（例：每幀那一側）讀過的事件會被再讀一次。時間設計第 6.3 節第 (2) 條就是在講這件事，但它的「啟動時檢查」靠 Kernel 認得可回滾的 schedule，現在 Kernel 不認得了。
- **別家**：bevy_ggrs 明寫訊息不回滾、叫使用者別在回滾的 schedule 用訊息；lightyear 沒找到回滾訊息的程式碼；GYO 舊設計把固定步的事件緩衝（t 與 t−1）放進快照。
- **兩種都做得到**（游標份修訂 1 第 4.8 節、第 11 節第 3 條；統合 L11 修訂 17）：
  - 事件不回滾：重跑時重新發出，被回滾那一側自己的讀者也會讀到重複的；處理交給回滾策略與作者（lightyear 給 System 自己判是否在回滾）。Kernel 不變。
  - 事件回滾：原語「取出／放回一種事件的紀錄、寫入序號、讀者游標」；回滾 Module 快照時取出、還原時放回，重跑時被回滾那一側讀到的和第一次相同。Kernel 仍只有游標一種讀法。
  - 選哪一種是使用者判（第 10 節第 4 件）。
- **「可回滾的 schedule 不參與跨時鐘事件」**（`time-plan-two` (5)）：統合修訂 17 判內容不變、只換落點——由回滾策略在啟動檢查讀唯讀登記表（L6 交出「每種事件有哪些讀者」，游標份提、統合准）。官方實作照做：可回滾的 schedule 裡的 System 寫的事件，若有不在這張 schedule 的讀者，或它讀的事件有不在這張 schedule 的寫者，啟動錯誤。這條是使用者 11-14 定的內容，本頁不改它的嚴格度；作者自寫的策略可以不檢查（L21）。

### 7.2 時間 Module（L2）

- 重跑要把 tick 退回 T，跑完回到現在；固定步的累加器（時間設計 `FixedTime`／`TickAccumulator`）**不能**退（時間設計第 4.2 節第 4 點「做回滾、摘要、存檔的產品要把它排除」）。
- 別家：lightyear 直接改 `LocalTimeline` 與 `Time<Fixed>` 兩個 Resource，跑完換回；bevy_ggrs 直接設 `RollbackFrameCount`。兩者都是回滾那一側改時間那一側的 Resource。
- **本頁的要求**（給時間 Module 的批）：tick 存在一個回滾 Module 能寫的地方（Resource）；「跑一步固定步」的那張 schedule 自己推進 tick，或時間 Module 給一個「推進一步」的函式讓重跑迴圈呼叫——兩者擇一，在時間的批定；重跑時 System 讀到的「固定步的時間」要等於第一次跑那一步時的值。
- **依賴**：回滾 Module 在登記檔依賴時間 Module，直接讀寫它的 tick Resource（統合修訂 17，L20；不另立「步數讀寫」的 interface）。

### 7.3 權威 Module（L11、L12、L21、L22）

- 方向已定：權威 → 回滾。回滾 Module 不讀權威的任何東西。
- **登記**（L22，統合修訂 17）：權威與回滾各自登記；伺服器權威策略可以替作者呼叫回滾 Module 的公開登記函式（第 5.6 節）。回滾這一側只要求：權威只用公開登記，不碰回滾的內部。
- **要和權威份對齊的**（收口時）：
  - 伺服器權威策略在 Client 端「選用哪個回滾策略」怎麼表達：本頁的提議是權威策略只要求「產品選了回滾 Module、且選了某個策略」，啟動時讀回滾 Module 公開的「目前策略」檢查。
  - Client 收到伺服器覆蓋（L3 的「伺服器覆蓋」）的那條路，就是第 5.4 節第 3 點的權威狀態入口；網路 Module 寫進來。

### 7.4 讀檔（讀檔卡的宣告第 7 節給本頁的要求）

| 讀檔卡的要求 | 本頁的回答 |
|---|---|
| 讀檔＝新 World，回滾的快照不跨世代 | 歷史跟著 World：若歷史存在 World 裡（第 10 節第 1 件的其中一個選項），新 World 天然是空的；若存在 Module 的記憶體，Module 要以 World 為鍵、World 換掉時清空。兩者都守得住，實作批加測試 |
| 讀檔的路上 hook 照常；舊 Restore 不觸發 hook，共用之前要對齊 | 回滾的還原和讀檔的放回**不共用**：讀檔是作者的 System 用一般的 `Spawn`／`SpawnWithId`；回滾用第 6 節的原語。還原跑不跑 hook 是第 10 節第 2 件，不影響讀檔 |
| 重看 LIM-7（重放的生成撞上只帶 (b) 區元件的活實體） | 區已移出 Kernel，LIM-7 的「(b) 區／(c) 區」要改寫（統合減法批）。回滾這一側：重跑前已把可回滾欄的成員退回第 T 步、配發器也退回，重跑的 `Spawn` 拿到和第一次一樣的編號；會撞到的只有「帶不回滾資料的活實體佔了同一個編號」——照 lightyear 的 `DisableRollback`，本機專用的實體不和可回滾的實體共用編號空間，寫進回滾 Module 的限制（取代 LIM-7 的回滾那一半） |
| 定 `RejectedSpawn` 的 `rollbackActive` 由誰、在哪裡設，總數要不要扣除重放的部分 | **統合修訂 17 定**：Kernel 拿掉診斷紀錄的 `rollbackActive`、`RejectedSpawn` 不加它（Kernel 不認得回滾，DI-KS 15）；總數照實計（重跑的也算）；要分開看的作者讀回滾 Module 的「重跑中」旗標 |

### 7.5 動態插入與唯讀登記表（L5、L6）

- 重跑要用和第一次跑相同的 Plan。World 准動態插入時，兩次之間 Plan 可能被重建。
- 官方實作的做法：記錄時連同登記表的版本號（L6）；判斷要回滾時，目標步記錄的版本號和現在不同 → 不回滾、記診斷（歷史作廢，從現在重新記）。伺服器權威策略要求 World 宣告不准動態插入（L5），PvP 不會碰到。
- 不需要 Kernel 新給東西（版本號已定）。

---

## 8. 回報統合的條目與統合的判定

修訂 0 回報六件；統合修訂 17（a125c45b）照 L17 判（不問使用者的部分）：

| 回報的 | 統合的判定 | 本頁改了哪裡 |
|---|---|---|
| Kernel 原語清單（第 6 節） | 前五列給，Ready 時從外面可用；拿掉 `rollbackActive`——准 | 第 6 節開頭 |
| 新關聯：回滾 → 時間 | 照 L20 在登記檔依賴時間 Module，不另立 interface | 第 5.2、7.2 節；原第 10 節第 7 件刪 |
| 新關聯：回滾 ↔ 游標，「可回滾的不參與」的去處 | 內容不變、落點換到回滾策略讀 L6；事件回滾在游標設計上做得到（原語見第 6 節） | 第 2.2、6、7.1 節 |
| 新關聯：回滾的登記與權威的登記 | 新增 L22：不同的事實、各自登記；權威可替作者登記進回滾 | 第 1、5.6、7.3 節 |
| 重跑在哪裡發動（動 DI-KS 18） | **統合問使用者**（統合第 10 節第 12 件），定了通知本卡 | 原第 10 節第 1 件移到統合 |
| 讀檔卡的 `rollbackActive` | Kernel 不加；總數照實計；作者讀「重跑中」旗標 | 第 7.4 節 |

**還沒對齊的**：權威份的宣告（第 7.3 節「選用哪個回滾策略」的表達、權威替作者登記的呼叫形狀）。

---

## 9. Design Invariants 與 Replacement Criteria（草案；核准後寫進回滾 Module 的頁）

- **R-1 契約不變**：`RollbackStrategy` 的掛點與語意（第 5.3 節）是契約；官方實作的內部不保證不變（L21）。守衛：一個測試用的「從頭實作 interface」的策略，和官方實作跑同一組測試案例。
- **R-2 回滾等價**：從狀態 s 直接跑 k 步，與「跑到現在、退回第 T 步、重跑回現在」，登記為可回滾的欄與 Resource 逐位元相同（stamp 除外）；輸入歷史在兩種跑法中相同。守衛：回滾 Module 的等價測試；突變：漏退配發器、漏退 tick、還原時漏掉被刪的格、重跑中旗標沒立。（舊 H-5 的思想）
- **R-3 不認得權威與網路**：回滾 Module 不 include 權威 Module、網路 Module。守衛：依賴掃描。
- **R-4 沒選就沒有**：產品不選回滾 Module 時，Kernel 與編排層裡沒有任何回滾的狀態或分支。守衛：Kernel 的掃描（統合附錄已列 `rollback` 相關識別字 0 命中）。
- **R-5 歷史不跨世代**：換 World 之後，舊 World 的歷史不能還原到新 World。守衛：測試。
- **RC**：職責清單（第 5.1 節）要多加一項，或判不出某樣東西屬回滾還是別層（L17）；Kernel 為了回滾要多加第 6 節以外的原語；P2P 實作時發現第 6 節的原語不夠。

---

## 10. 要使用者判的（三份一起送檢查之後逐件問；每件都先用遊戲講）

**重跑在哪裡發動**（修訂 0 的第 1 件）已移到統合（統合第 10 節第 12 件），由統合問使用者、定了通知本卡。修訂 0 給它列的材料（供統合參考）：在一個獨占的 System 裡巢狀跑（lightyear `run_rollback`、bevy_ggrs `run_ggrs_schedules`；GYO 已有巢狀跑，K0／DI-KS 16～21；代價：整欄放回是結構變更，動 DI-KS 18）／在 World 閒置時由 runner 呼叫（舊 DI-KS 12、H-9；代價：runner 要懂回滾，System 發現預測錯了只能等這一輪跑完，時間設計第 4.1 節）／兩條都給、由策略選。**回滾 → 時間的依賴**（修訂 0 的第 7 件）統合已定，刪。

1. **歷史存在哪**。例：Client 存了 20 步的位置歷史。
   - World 裡的 Resource（bevy_ggrs）或元件（lightyear）。代價：歷史出現在登記表與除錯工具裡；權威的 digest 要排除它。好處：跟著 World 的世代（第 7.4 節）。
   - 回滾 Module 自己的記憶體（舊 GYO 的 Runtime）。代價：要自己以 World 為鍵、換 World 時清空；除錯工具看不到。
2. **官方實作還原時跑不跑 hook**。例：角色被退回到「還沒撿起武器」的那一步，武器元件被移除；「移除時更新空間索引」的 hook 要不要跑。
   - 跑（lightyear、bevy_ggrs）。代價：hook 裡的副作用會在回滾時發生，作者要自己用「重跑中」旗標判斷。
   - 不跑（舊 H-11、Unity 寫原始位元組）。代價：空間索引這類衍生資料要靠重跑重算，或作者另外補。
   - 不論選哪個，作者自寫的策略可以選另一個（第 6 節兩條路都給）。
3. **官方實作還原時 stamp 怎麼標**。例：渲染那一側用「位置改過了」決定要不要重送給 GPU。
   - 標目前 stamp（舊 H.11 取捨 2、lightyear、bevy_ggrs）。代價：還原碰到的格都被當成改過（bevy_ggrs pitfalls 明寫）。
   - 寫回歷史裡的 stamp（Bevy `set_last_changed` 的用途）。代價：渲染那一側看不到「被還原」這件事，要另外通知。
4. **事件在重跑中**與**副作用的提醒**。例：第 100 步「開槍」事件給第 101 步的「扣彈藥」讀；重跑時要不要讀得到。另一例：「播腳步聲」的 System 沒加「重跑中跳過」的條件。
   - 事件不回滾，回滾 Module 啟動時檢查並報錯（bevy_ggrs 的限制，加上檢查）／只警告不報錯（照 `engine-not-rigid`）。
   - 事件回滾，Kernel 給「取出／放回一種事件的紀錄、寫入序號、讀者游標」的原語（舊 GYO H.5 的思想；游標份已確認做得到，第 7.1 節）。
   - 副作用的條件：回滾 Module 不檢查（lightyear、bevy_ggrs 都不檢查）／啟動時用 effect 宣告提醒（舊 H-8 的思想）。
5. **選了回滾 Module 但沒選策略**。例：產品清單寫了回滾，忘了 `UseStrategy`。
   - 啟動錯誤（和權威一樣「選了就要說清楚」）。
   - 預設用官方實作（lightyear、bevy_ggrs 裝了 plugin 就有預設行為）。

我的建議另外標明，不當成已定：第 1 件傾向 World 裡的 Resource（世代問題自然解掉、照 bevy_ggrs）；第 2、3 件傾向照兩家 Bevy 套件（跑 hook、標目前 stamp），理由是「很多開源都這樣做就不頭鐵」；第 4 件的事件傾向不回滾（兩家 Bevy 套件都不回滾訊息，Kernel 不多原語），副作用傾向不檢查；第 5 件傾向啟動錯誤。重跑在哪裡發動（統合問）的材料裡，修訂 0 傾向「在獨占 System 裡巢狀跑」（兩家 Bevy 套件都這樣做、GYO 已有巢狀跑、時間設計選了「System 驅動固定步」與它同形）。

---

## 11. 範圍的限界

- 這是宣告，沒有寫程式、沒有量測。實作批的估計在統合核准、第 10 節定完之後另寫；統合第 8 節提案把權威、回滾、「拿掉權威規則」放同一條線（主會話還沒確認）。
- GGRS／bevy_ggrs 由子 agent 讀，沒有行號；主會話沒有抽查。GekkoNet 沒有重讀，引用統合卡的偵察。
- Unity Netcode 的還原（`GhostUpdateSystem`）只讀到用途說明，沒有讀到寫回原始位元組的那幾行。
- 游標份的交界照它修訂 1 對齊（第 7.1 節）；權威份到修訂 1 時還沒有檔案，第 7.3 節是本頁單方面的提議，收口時對齊。
- 第 6 節的原語是「回滾需要什麼」，不是 Kernel 的介面設計；名字、參數、放在 `World` 還是別的型別，由統合與 Kernel 的實作批定。
