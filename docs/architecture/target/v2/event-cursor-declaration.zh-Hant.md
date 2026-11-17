# 事件游標：事前宣告（修訂 4）

> **核准**：使用者 2026-11-17 在主會話核准修訂 4（4b94e3ea），原話「核准游標」（HANDOFF `v8-arch-gyo2-event-cursor-declaration-approved`）。核准本宣告不等於 AGENTS §14.4 的判定變更核准：守衛判定的變更仍在實作批逐案列出、逐條請使用者核准。核准時，四份一起的最後一次換模型檢查還在跑；檢查若要改本頁，由統合那張卡通知，改完交使用者再看。

> 性質：事前宣告（AGENTS §3 的 Architecture Delta；§2 的基盤 Architecture）。**使用者核准之前不實作、不改任何條文與守衛。核准本宣告不等於 AGENTS §14.4 的判定變更核准**：每一個守衛判定的變更，在實作那一批的事前宣告逐案列出、逐條核准。
> 批次代號：`v8-arch-gyo2-event-cursor`。
> 這一份是「Kernel 與編排層的邊界」拆成的子份裡，Kernel 事件那一份（`v8-arch-gyo2-boundary-split-integrator-first`）。**以統合為準**：統合 `kernel-app-boundary-declaration.zh-Hant.md`（分支 `claude/gyo2-kernel-app-boundary-4af68c`，本頁對齊修訂 28，16c84630）定過的事，本頁不改。

**修訂紀錄**：修訂 0（facc0846）寫出八件要使用者判的題目，並回報七處介面與六條新關聯。修訂 1（c55f1253）照統合修訂 16 記下那七處的判定。修訂 2（96fae7dc）、修訂 3（bafc4050）照統合修訂 19、20 的交叉檢查補 PvP Client 的註記與回滾的題目。**修訂 4（2026-11-17，最後一版）**：使用者定「事件照 Bevy 分兩種」（`v8-arch-gyo2-events-two-kinds-like-bevy`），保留式改名 `Message`，推送式（`Event`、`Observer`）併入本頁；八件題目由統合照 L17 判（統合第 15.1 節），第 4 節改成判定的紀錄。選項的比較表留在 git 歷史（修訂 3，bafc4050，第 4 節）。之後四份一起做一次換模型檢查，再請使用者核准（統合那張卡負責）。

---

## 0. 引用統合的關聯表

統合第 1 節的關聯表裡，本頁開頭要引用的七條（「狀態」欄照統合修訂 28 摘錄）：

| # | 事項 | 狀態（統合修訂 28） | 本頁怎麼用 |
|---|---|---|---|
| L1 | 分層與歸屬（統合第 3 節） | 已定 | 兩種事件都留在 Kernel（照 `bevy_ecs`，`events-two-kinds-like-bevy`）；跨 World、經網路的轉送不在 Kernel |
| L2 | 時鐘與 tick 移出 Kernel，到時間的 Module（`Domain`、`ScheduleOptions::clock`、Kernel 推 tick、`CurrentTick`、`AccessKind::DomainTick`、tickStamps、D-E12、F-E1） | **已定**（`v8-arch-gyo2-clock-out-of-kernel`） | 訊息的順序鍵與上限的單位不用 tick；訊息不屬於任何時鐘，F-9 與 F-E1 一起拿掉 |
| L8 | 不靠時鐘的計數（`RejectedSpawn`、診斷紀錄帶的 tick） | 改用 Kernel 自己的計數（stamp 或同步點序號）。**統合照 L17 判**（修訂 19）：選哪一個是實作的細節、不問使用者，由實作批定 | 丟棄記錄要標時間時，用 L8 定的那個計數 |
| L10 | Registry 與 Plan 指紋改版、golden 重產 | 已定為後果；跨端比較的指紋不用 Kernel 的編號（權威份）；Registry 與 Plan 指紋只在同一端比，照舊用 Kernel 的編號 | 拿掉 Plan 的 `EventTrim` 步與 `ScheduleOptions::eventTrim`；observer 的登記進 Registry 指紋；golden 重產併入「時間 Module＋事件游標＋拿掉時鐘」那一條線（統合第 8 節的第二次） |
| L11 | 回滾的機制放哪 | **已定**（`v8-arch-gyo2-rollback-module-strategy`）：Kernel 只給原語；回滾是獨立的 Module，有策略。事件進不進快照由統合 15.1 判：不進 | `Message` 不回滾、Kernel 不加事件的原語；推送式不保留、重跑時重新觸發 |
| L13 | 事件游標 | 待游標份；**修訂 26**：事件照 Bevy 分兩種，游標份的內容只適用於保留式（改名）；推送式另立（名字、時機、和 hook 的關係、observer 的存取）。兩者的細節由統合 15.1 照 L17 判 | 本頁的範圍：保留式的游標與修剪，加上推送式（統合要求一併寫進本頁，因為本頁是 Kernel 事件的那一份） |
| L17 | 替換條件與歸屬的判斷方式 | **已定**（`v8-arch-gyo2-layer-duty-lists`）：兩條核心＋每層的職責清單；agent 自己判；判斷時對照同類型多家實作 | 統合 DI-KS 15 第 (2) 項「事件」涵蓋兩種，不算 Kernel 職責清單多一項；第 9 節的 RC 用「清單多一項」 |

---

## 1. 依據

**讀的版本**：程式 `origin/gyo2` f00433b0（2026-11-17 沒有新 commit）；統合修訂 28（16c84630）的第 2.1、15.1、15.6 節與第 8 節；HANDOFF 與時間設計 `origin/claude/gyo2-time-design` 2416d4c0。

**證據**（主 checkout）：
- `build/evidence/gyo2-kernel-app-boundary/section15-cross-engine.md`：統合第 15 節的七家對照（Opus／high，讀本地原始碼：bevy 6a911653f、flecs 09790b5f7、com.unity.entities 4630ff5、com.unity.netcode c4cabf4、MassSample ca98258、lightyear 736e1695、bevy_replicon d80c849）。本頁的系譜欄照它寫。
- `build/evidence/gyo2-event-cursor/reference-engines-events.md`：本卡修訂 0 讀的出處。

**使用者的決定**（原話以 HANDOFF 與統合 2.1 為準）：

| 代號 | 和本頁有關的內容 |
|---|---|
| `v8-arch-gyo2-events-two-kinds-like-bevy`（11-17，side chat；代號待主會話定） | 事件照 Bevy 分兩種：保留式改名，對應 Bevy 的 `Message`；增加推送式，對應 Bevy 的 `Event`＋`Observer`；兩種都在 Kernel。原話：「OK，照Bevy的來，1）改名。2）增加一個 推送式。位置參照Bevy來放。」比過：只留保留式／只有推送式（flecs）／兩種都給（Bevy）← 選 |
| `v8-arch-gyo2-event-trim-by-cursor`（11-16） | Kernel 丟掉所有讀者都讀過的、或超過上限的紀錄，丟了有記錄，不需要知道時鐘。理由（使用者的）：「它讓『不漏讀』成為 Kernel 自己的保證，不必依賴編排層排得對。」 |
| `v8-arch-gyo2-time-plan-two`（11-14）第 5、8 項 | (5)「事件：每個讀者一個游標；留到讀過或碰到上限，丟了有記錄；可回滾的 schedule 不參與」。(8)「經網路的事件：這一輪定形狀，做法照 bevy_replicon 那一型（作者照常寫讀、中間送、帶 tick），PvP 移植時做」 |
| `v8-arch-gyo2-cross-clock-events-open`（11-14）＋`-three-clarifications` (3) | 跨時鐘的事件放開；「不光要考慮單機，也要兼容網絡部分」（使用者說明「這是要求」） |
| `v8-arch-gyo2-kernel-is-ecs-no-restrictions`（11-16） | Kernel 是 ECS，不疊加限制 |
| `v8-arch-gyo2-same-input-same-result`（11-15） | 同樣的輸入、同樣的結果（單執行緒）；「這一幀固定步跑幾步」算輸入 |
| `v8-arch-gyo2-dynamic-insert-per-world`（11-17） | World 在 Ready 時可以再登記或拿掉 System、下一次執行前重建 Plan（統合 L5） |

**統合照 L17 判的**（統合 15.1；使用者 11-17 說「什麼都要我來定，乾脆代碼我也自己寫好了。名字你有命名規範。例子你有源碼。」之後，名字照命名規範、細節照別家原始碼由統合判）：本頁第 3、4 節標「統合照 L17 判」的各條。**這些不是使用者的話。**

**時間設計 6.3 (2)**：主會話的設計稿（使用者在 `time-plan-two` 定下的只有 (5) 那一句）。設計稿寫的「可回滾那一側自己的讀者維持依位置讀」，本頁不採（Kernel 只有游標一種讀法）；「走 Ingress」隨區移到權威 Module（統合 3.2）。

---

## 2. 先用遊戲講

**射擊遊戲，一個 World，固定步不回滾（單機、PvP 的 Match）。**

**保留式（`Message`）**：每幀的「讀手把」寫一筆「按下了開火」。中間一個獨占的 System（時間 Module 的）依真實時間跑固定步零到數次；固定步裡的「生成子彈」讀它。每幀的「播槍口火光」也讀它。固定步裡的「子彈落地」寫「命中了」，每幀的「播命中音效」讀。

- 這一幀固定步跑 0 次：「生成子彈」沒跑，它的游標停著，訊息留著，下一幀讀到。
- 這一幀固定步跑 3 次：「生成子彈」第一次讀到上次以來的全部，後兩次沒有新的。三筆「命中了」，「播命中音效」下次一次讀到三筆。
- 「播槍口火光」暫停時被 run 條件跳過：游標停著，恢復後一次讀到（上限以內）。
- 暫停很久：碰到上限，最舊的丟掉；這次 Run 結束時記一筆「丟了幾筆、哪段序號、誰沒讀到」。
- 「遊戲結束畫面」的讀者從沒跑過：它不拉住保留；第一次跑時從還留著的最舊一筆讀起。

**推送式（`Event`＋`Observer`）**：「角色身上加了『著火』元件」時，一個 observer 當場生成火焰粒子；作者自訂的「門被打開了」事件，由開門的獨占 System 觸發，observer 當場播音效。一般 System 觸發的（經 `Commands`），等到同步點才跑 observer。推送式不保留：當下沒有 observer，就沒有人收到。

Kernel 不知道「固定步」「每幀」「暫停」是什麼；保留式只看「每種訊息有哪些讀者、每個讀者讀到第幾筆」。

**PvP Client**：模擬是可回滾的固定步。使用者 11-14「可回滾的 schedule 不參與」照舊，由回滾策略在啟動時檢查兩個方向（統合 15.1）。所以「每幀寫、固定步讀」「固定步寫、每幀讀」在 PvP Client 的模擬裡都不走保留式訊息：輸入照回滾策略自己的輸入歷史（統合 15.1：lightyear、Unity 都把要重播的輸入放在按 tick 索引的環）；命中音效照現行 PvP 的做法，由畫面那一側從快照偵測（時間設計 6.4）。

---

## 3. 已定的事

「誰定」欄：**使用者**＝HANDOFF 或統合 2.1 的使用者決定；**由決定推出**＝本頁從使用者決定直接推出；**統合照 L17 判**＝統合 15.1 的判定，不是使用者的話。

### 3.1 兩種事件與名字

| 事 | 定了什麼 | 誰定 | 系譜 |
|---|---|---|---|
| 分兩種、都在 Kernel | 保留式＋推送式，位置照 `bevy_ecs` | 使用者（`events-two-kinds-like-bevy`） | Bevy 兩種都在 `bevy_ecs`；flecs 只有推送式；Unity Entities 核心沒有推送式（section15-cross-engine.md 第 1、2 條） |
| 保留式的名字 | `Message`、`MessageReader`、`MessageWriter`、`MessageOptions`（取代 `Event*`） | 統合照 L17 判（命名規範） | Bevy `Message`、`MessageReader`、`MessageWriter` |
| 推送式的名字 | `Event`、`Observer`、觸發 `Trigger`、observer 的參數 `On<E>`；事件型別名用過去式（naming DI 2） | 統合照 L17 判 | `Observer` 三家共通；觸發 Bevy 叫 trigger、flecs 叫 emit；`On<E>` 照 Bevy |

### 3.2 保留式（`Message`）

| 事 | 定了什麼 | 誰定 | 系譜 |
|---|---|---|---|
| 每個讀者一個游標，照游標修剪 | 一筆紀錄在「所有拉住保留的讀者都讀過」或「被上限擠掉」時丟掉；丟了有記錄；不靠時鐘 | 使用者（`event-trim-by-cursor`、`time-plan-two` (5)） | — |
| 不執行的讀者 | 被 run 條件跳過、或所在 schedule 這一次沒跑，游標不動，下次補讀（上限以內） | 由決定推出（「讀者在上限內不漏讀」由 Kernel 保證，L13） | — |
| 游標放哪 | 放在 World，以（掛載點、訊息種類）為鍵 | 統合照 L17 判 | **GYO 從使用者決定推出，不是照別家**：照游標修剪，Kernel 必須讀得到全部游標。別家：Bevy 放每個 System 的 `Local`（`message_reader.rs:38-41`），因為它的保留固定兩次 update、不看游標（`messages.rs:69-77`）；flecs 沒有保留式、建議自己做 queue（`ObserversManual.md:105-112`）；lightyear 的接收器只能取走一次、沒有游標（`receive.rs:61-64`）；replicon 沿用 Bevy 的 `Messages`。代價：停跑的讀者拉住保留，靠上限擋 |
| 誰算一個讀者 | System 掛載、run 條件、hook、observer 都是讀者，各有游標（同一個 System 掛兩處是兩個讀者）；推游標不算寫入 | 統合照 L17 判（observer 那一項由本頁補：observer 是 System，見 3.3） | Bevy `on_message` 條件自帶游標、讀了就消耗（`schedule/condition.rs:1148-1154`）；讀者與讀者可以並行（`message_reader.rs:30-33`）；Bevy 的 observer 也是 System、可以帶 `MessageReader`（`observer/mod.rs:30`） |
| 從沒跑過的讀者、新讀者 | 跑過一次才拉住保留；第一次跑從還留著的最舊一筆讀起。L5 動態插入的讀者同樣 | 統合照 L17 判 | Bevy 新游標讀到緩衝裡還留著的（`messages.rs:175-178`）；Bevy 另有 `get_cursor_current` 只讀之後的（`:180-186`），GYO 不給 |
| 被拿掉的讀者 | L5 拿掉 System 時，重建 Plan 的同一步拿掉它的游標；只被它拉住的紀錄可以修剪 | 由決定推出（L5；統合已記為 L13×L5） | — |
| 上限 | 每種訊息留幾筆，預設 1024，登記時可改（沒有量測根據，是預設值）；滿了丟最舊；不給「滿了算錯誤」 | 統合照 L17 判 | 別家分歧：Unity Netcode 輸入環 64 筆、滿了覆蓋最舊（`ICommandData.cs:169`、`:263-266`）；lightyear 待放緩衝 4096 筆、滿了拒收新的並報錯（`receive.rs:243-254`，為了防不可信的網路灌入擠掉別人的訊息）；Bevy 沒有筆數上限、超過兩次 update 靜默丟（`messages.rs:36-37`、`:79`）；replicon 沒有上限（`message_queue.rs:15-30`）。GYO 的訊息在行程內，照 Unity 丟最舊 |
| 丟棄記錄 | 每次 Run 按訊息種類聚合一筆：丟了幾筆、被丟的序號範圍、沒讀到的讀者；World 記、runner 在交換點取走；不逐筆記明細（免得讀者停住時明細本身無限長）。丟棄是結果、不是錯誤，不中斷 World | 統合照 L17 判；「丟了有記錄」是使用者的 | **只有 GYO 留一份可取走的記錄**（使用者「丟了有記錄」）：lightyear 按種類聚合一行 `warn!`（`receive.rs:451`）、Unity 過期 RPC 只警告（`RpcCommandRequest.cs:100`）、Bevy 靜默。聚合的形狀照 lightyear；「World 記、runner 取走」照 GYO 的 `RejectedSpawn`（`spawn-reject-details-unbounded-runner-takes`、`-is-result-not-error`） |
| 順序鍵 | 每種訊息一個單調遞增的寫入序號（64 位元，不回捲）；讀者依序號讀 | 統合照 L17 判 | 多家一致：Bevy `MessageId.id`（`message/mod.rs:109-123`）、lightyear（remote_tick, next_sequence）（`receive.rs:105-116`）、flecs 自訂事件依發出順序（`ObserversManual.md:2714`）。同一種訊息的寫者彼此衝突、被 Plan 排成先後（`SystemTraits.hpp:119-121` 一個 System 不能同時讀寫同一種），所以序號是決定性的 |
| 修剪時機 | 每次最外層 Run 結束時（hook 清空輪之後）修剪讀過的；寫入時碰到上限當場丟最舊；巢狀跑不修剪；不在 Plan 裡 | 統合照 L17 判 | 多家一致（每個外層幀一次）：Bevy 每幀在 First 輪替（`sub_app.rs:91-98`），為了怕固定步還沒讀就丟，另加閘門（`bevy_time/lib.rs:102-106`、`message/update.rs:14-50`）——照游標修剪正好解掉這個問題；lightyear 每幀在 Last 清（`receive.rs:55`）。不在 Plan 裡照 GYO 的 hook 清空輪（`hook-flush-not-in-plan-bump-versions`） |
| 時間標記 | 紀錄只帶序號；要 tick 的訊息自己在內容裡帶（寫的 System 讀時間 Module 的 Resource） | 統合照 L17 判 | 多家一致（核心不帶 tick、網路層自己加）：Bevy 紀錄只有 id＋內容（`message/mod.rs:104-107`）；replicon 的 tick 在自己的佇列（`server_message.rs:481-513`）；lightyear 在接收紀錄的 metadata（`receive.rs:66-75`）；Unity `ICommandData` 把 Tick 放內容（`ICommandData.cs:65-72`） |
| `World::EventPayloads` | 改名為 `World::MessagePayloads`，照舊是 runner 與測試用的唯讀檢視，列出還留著的紀錄、依序號；不推任何游標、不算讀者 | 由決定推出（改名隨 3.1） | — |
| 驗證與摘要 | 推游標不算寫訊息；存取驗證的快照把游標表與訊息內容分開；World 摘要含游標與寫入序號（摘要涵蓋「World 裡的資料」），摘要改版併入 L10 | 統合照 L17 判（修訂 16） | — |

### 3.3 推送式（`Event`＋`Observer`）

| 事 | 定了什麼 | 誰定 | 系譜 |
|---|---|---|---|
| 觸發的時機 | 兩條：`ExclusiveWorld` 與 World 外面的呼叫者（統合稱 Host）觸發，observer 當場跑；一般 System 經 `Commands` 觸發，延到同步點跑 | 統合照 L17 判 | flecs「操作延後時 observer 跟著延到同步點」（`ObserversManual.md:2598`）；Bevy `World::trigger` 當場（`event/mod.rs:14-16`）、`Commands::trigger` 延後（`system/commands/mod.rs:1160`） |
| 同一點內 observer 的先後 | 決定性（同樣的登記得到同樣的順序），但不對作者承諾是登記順序 | 統合照 L17 判（修訂 28 更正） | flecs 明寫先後 undefined、不要依賴（`ObserversManual.md:2702-2703`）；Bevy 寫順序任意（`observer/distributed_storage.rs:153-155`）。同一點內多次觸發依觸發的先後送達：flecs 自訂事件依發出順序（`:2711-2714`） |
| 和 hook 的關係 | hook 留著、不併入：一個元件每種一個 hook；新增時 hook 在 observer 之前、移除時 hook 在 observer 之後（GYO 現行 OnRemove 的先後在實作批對照） | 統合照 L17 判 | flecs（hook 一定在 observer 之前、移除時在之後，`ObserversManual.md:128-142`）；Bevy（`world/entity_access/mod.rs:1344-1345`）；Mass 只有 observer processor（MassSample `MSPhysicsInitProcessors.cpp:244-245`、`:360-361`）；Unity Entities 核心沒有推送式 |
| 元件的新增與移除觸發 observer | 放進這次的契約，和自訂事件同一批實作（Bevy 的 `On<Add<T>>` 那一型） | 統合照 L17 判（先前「先不做」撤回） | 三家都有：Bevy `On<Add<T>>`、flecs OnAdd／OnRemove／OnSet、Mass `ObservedOperations` |
| observer 是什麼 | observer 是 System：存取照簽名推出、照存取驗證；在觸發點跑、不進 Plan 的步驟（同 hook） | 統合照 L17 判 | Bevy 的 observer 是 System（`observer/mod.rs:30`）；GYO 的 hook 已是帶參數的 System 形（`Params.hpp:984-998` 的 `HookCtx`） |
| 保留與回滾 | 推送式不保留；回滾重跑時重新觸發，observer 讀回滾 Module 的「重跑中」自己判斷 | 統合照 L17 判 | lightyear 用 `Rollback` Resource 判斷重跑中（`core/timeline.rs:332-350`）；Unity `IsFirstTimeFullyPredictingTick`（`NetworkTime.cs:39-44`） |
| 歸屬 | 統合 DI-KS 15 第 (2) 項「事件」涵蓋兩種，不算 Kernel 職責清單多一項 | 統合照 L17 判 | — |

**本頁照 L17 補的細節**（Kernel 事件的職責內，沒有推翻使用者的決定；收口檢查時請檢查者一併看）：

- observer 自己用 `Commands` 記的延後變更（含再觸發的事件），照 hook 的規則：進下一窗，最後一個同步點之後由 hook 清空輪一輪一輪套用、有上限（F-12）。理由：和 hook 同一條路，就不會在同一個同步點裡無上限地連鎖（`v8-arch-gyo2-hook-queue-flush-at-run-end` 的理由）。確切的佇列（和 hook 共用還是分開）在實作批對照 Bevy、flecs 定。
- `ExclusiveWorld` 當場觸發時，observer 在獨占 System 的呼叫裡跑，等同巢狀的一次 System 呼叫：照巢狀執行的規則（DI-KS 16～21：句柄、stamp、出錯記一次）。observer 裡不能再巢狀跑 schedule（同 hook，X-13）。
- observer 的登記進 Registry 指紋（L10），交進唯讀登記表（L6：觸發、元件或事件型別、存取、登記者）。

### 3.4 跨時鐘、跨 World、網路與回滾

| 事 | 定了什麼 | 誰定 | 系譜 |
|---|---|---|---|
| 同一 World、不同時鐘 | 時鐘不在 Kernel；訊息不屬於任何時鐘，任何 schedule 的 System 都能讀寫任何訊息；游標保證不漏讀（上限內）；F-9、F-E1、J-37、`MessageOptions` 裡的 domain 拿掉 | 使用者（L2、`cross-clock-events-open`）＋由決定推出 | — |
| 兩個 World 之間、經網路 | 照 replicon 的形狀：轉送是網路（或交換資料的）Module 的一般讀者與寫者，tick 由那個 Module 自己加；Kernel 不加外部游標；做法延到網路 Module 的宣告（統合 L23） | 使用者（`time-plan-two` (8) 的方向）＋統合照 L17 判 | replicon 寫進 Bevy 的 `Messages`（`server_message.rs:493`），tick 在自己的佇列（`:481-513`） |
| 回滾 | `Message` 不回滾，Kernel 不加訊息的原語；「可回滾的 schedule 不參與」照舊，由回滾策略在啟動時檢查兩個方向（讀 L6 的讀者名單）；推送式不保留、重跑時重新觸發 | 使用者（`time-plan-two` (5)）＋統合照 L17 判 | lightyear 訊息不回滾（`core/timeline.rs:332-350`、`manager.rs:403-408`）；Unity RPC 不回滾。兩家都把要重播的輸入放在另一個按 tick 索引的環（lightyear `input_buffer.rs:25-39`、Unity `ICommandData` 64 筆） |
| Kernel 只有一種讀法 | 保留式只有游標；不做時間設計 6.3 (2) 的「依位置讀」 | 由決定推出（照游標修剪） | — |

---

## 4. 修訂 3 第 4 節八件題目的處理

修訂 3 的八件都由統合照 L17 判（統合 15.1，2026-11-17），**不是使用者的話**。選項的比較表在 git 歷史（bafc4050）。

| 修訂 3 的題目 | 修訂 3 的建議 | 統合的判定 | 和建議的差別 |
|---|---|---|---|
| 4.1 游標放哪、誰算一個讀者 | 放 World；run 條件與 hook 也是讀者 | 同建議 | 無；observer 隨「是 System」也是讀者（本頁補） |
| 4.2 上限的單位、預設、滿了 | 每種事件 1024 筆；只丟最舊 | 同建議 | 無 |
| 4.3 不執行的讀者、新讀者 | 跑過一次才拉住；從最舊一筆讀起 | 同建議 | 無 |
| 4.4 丟棄的記錄 | World 記一筆明細、runner 取走 | 改成每次 Run 按種類聚合一筆（筆數、序號範圍、沒讀到的讀者），不逐筆 | 照 lightyear 的聚合；免得明細本身無限長 |
| 4.5 順序鍵 | 每種一個寫入序號 | 同建議 | 無 |
| 4.6 時間標記 | 只帶序號、tick 放內容 | 同建議 | 無 |
| 4.7 修剪時機 | 最外層 Run 結束＋寫入時碰上限 | 同建議 | 無 |
| 4.8 跨 World、網路、回滾 | 轉送用一般讀寫；回滾兩種結果各寫一段 | 轉送照建議、延到網路 Module；回滾判「不進」，Kernel 不加訊息的原語 | 修訂 3 的「放回哪些讀者的游標」不再需要 |

---

## 5. 介面變更

### 5.1 保留式（L13 的事件 API，改名）

| 介面 | 現在 | 改成 |
|---|---|---|
| `EventReader<E>` → `MessageReader<M>` | 依 Plan 位置與 tick 挑（`Params.hpp:417-438`）；建構時讀 `DomainTick`、`EventDomain`（`:413-414`） | `Read()` 回「序號比這個讀者的游標大的全部紀錄」，依序號；這次呼叫結束時游標推到讀到的最後一筆（同一次呼叫裡多次 `Read()` 回同一批）；不讀時鐘 |
| `EventWriter<E>` → `MessageWriter<M>` | 紀錄 `{tick, seq, writerPos, payload}`（`Params.hpp:458-459`） | 紀錄 `{seq, payload}`；寫入時碰到上限丟最舊、累計到這次 Run 的丟棄記錄 |
| `Detail::EventBuffer<E>` | 帶 tick、照 tick 修剪（`detail/Storage.hpp:170-210`） | 改名；只帶序號；修剪照「拉住保留的讀者游標的最小值」與上限 |
| `World::EventPayloads<E>()` → `World::MessagePayloads<M>()` | `World.hpp:222-232` | 改名；依序號；不推游標 |
| `EventOptions` → `MessageOptions` | `name`、`zone`、`domain`（`Declarations.hpp:99-105`） | `name` 留；`domain` 拿掉（L2）；`zone` 拿掉（統合 3.2）；加上限（預設 1024）——統合修訂 16 准 |
| Plan 的 `StepKind::EventTrim`、`ScheduleOptions::eventTrim`、D-E1 的 eventTrim 檢查 | `Plan.hpp:36`、`Scheduler.cpp:218-226`、`:263-270`、`Executor.cpp:417-420`、`Declarations.hpp:148-150`、`Registry.cpp:924` | 拿掉——統合修訂 16 准 |
| 存取種類 `Access::EventRead`／`EventWrite` | `Access.hpp:88-95` | 改名 `MessageRead`／`MessageWrite`；衝突規則照舊 |
| 丟棄記錄的取走 | 無 | World 上一個取走函式（照 `RejectedSpawn` 的形）——統合修訂 16 准 |
| 唯讀登記表（L6） | 有事件的名字 | 加每種訊息的上限與讀者名單——統合修訂 16 准 |

### 5.2 推送式（新增；統合要求寫進本頁）

| 介面 | 內容 |
|---|---|
| 事件型別的登記 | `Registrar` 登記一種 `Event`（型別名用過去式） |
| `Observer` 的登記 | 登記一個 observer System：要觀察的事件（自訂事件，或某個元件的 Add／Remove），參數 `On<E>` 取得事件內容與目標（照 Bevy `On<E>`）；其餘參數照一般 System 推存取 |
| 觸發 `Trigger` | `ExclusiveWorld` 與 World 外面的呼叫者：當場；`Commands`：延到同步點 |
| 存取與驗證 | observer 的存取照簽名推出、照存取驗證；`On<E>` 唯讀 |
| 唯讀登記表（L6）與指紋（L10） | 交出 observer（觸發、型別、存取、登記者）；observer 的登記進 Registry 指紋 |

hook（`OnAdd`／`OnRemove`、`HookCtx`）照舊，不改名、不併入。

### 5.3 不在本頁的

World 摘要的格式改版（含游標與寫入序號）併入 L10；`KernelSettings` 不放預設上限（L9）。這兩處統合修訂 16 已判。

---

## 6. Architecture Delta（AGENTS §3）

統合第 4 節已寫這一批的七項；本頁只寫差分。

1. **Feature**：第 2 節的射擊例子（固定步讀每幀寫的指令、每幀讀固定步的結果、暫停時的讀者不漏讀；元件新增時當場生成粒子、開門當場播音效）；`cross-clock-events-open` 的「放開」；`time-plan-two` (8) 的經網路事件的形狀；`events-two-kinds-like-bevy` 的兩種事件。
2. **現在的問題**：讀取依 Plan 位置與 tick；跨時鐘是啟動錯誤；被 run 條件跳過的讀者漏讀；修剪是 Plan 裡要排對的一步；沒有推送式，作者要「當場反應」只能靠 hook（每個元件每種一個）。
3. **變化的邊界**：Kernel 的事件 API（第 5.1 節，改名＋游標）；新的推送式介面（第 5.2 節）；Plan 少一種步驟。
4. **影響**：`engine/kernel`（`Params.hpp`、`Access.hpp`、`SystemTraits.hpp`、`detail/Storage.hpp`、`World.*`、`Scheduler.cpp`、`Executor.cpp`、`Registry.*`、`Fingerprint.cpp`、`Declarations.hpp`）；`tests/common/kernel` 的事件案例（J-36、J-37、J-54 與 J-56 的修剪案、`HookFlushTests.cpp`、`WorldTests.cpp`、`SignatureTests.cpp`、`SkeletonTests.cpp` 的讀寫者）；`naming_guard.py`、`kernel_symbols.py` 的名字清單；時間 Module（不必排修剪）；回滾 Module（檢查「可回滾的不參與」、提供「重跑中」）。
5. **Dependency Direction**：不變。轉送訊息的 Module 依賴時間 Module（拿 tick）照 L20 寫在登記檔。
6. **Ownership**：「事件留多久」從「時鐘＋Plan 裡的修剪步」改成「Kernel 照游標與上限」；「可回滾的不參與」從 Kernel 的啟動檢查（時間設計提案）改成回滾策略的事。
7. **為什麼不能更小**：只把 tick 換成別的計數、保留「依位置讀」：沒有時鐘就沒有「一 tick」界定兩格緩衝，而且使用者已定照游標修剪。推送式不做：使用者已定兩種都給。

---

## 7. 基盤 Architecture 的根據（AGENTS §2）

### 7.1 為什麼算基盤

事件 API 在每個讀寫事件的 System 裡；讀取規則與名字一改，所有讀寫事件的遊戲邏輯都受影響。上面已積或排著要積的：時間 Module、狀態機、回滾 Module、網路的轉送、音效。

### 7.2 予見的根據 → 要求

| 根據（出處） | 要求 |
|---|---|
| 射擊遊戲：每幀寫指令、固定步 0～N 步讀（`same-input-same-result` 的例子） | 讀者在任何次數的執行之間不漏讀（上限內）；同一筆對同一讀者恰好一次 |
| 暫停時固定步不跑（`time-plan-two` (6)；狀態機的「暫停時不跑遊戲邏輯」） | 長期不跑的讀者有上限、丟了有記錄 |
| 指令集（使用者 11-17，L5） | 動態加上與拿掉的讀者有定義 |
| 經網路的事件（`time-plan-two` (8)，PvP 移植時） | 一般讀者就能轉送；tick 不在 Kernel 的紀錄裡 |
| 音效時刻（2026-10-30，PvP） | 單機與 Match：訊息內容自己帶 tick；PvP Client：畫面那一側從快照偵測（第 2 節末） |
| 兩種事件（`events-two-kinds-like-bevy`） | 推送式當場或同步點觸發、hook 與 observer 先後固定、observer 照存取驗證 |
| 參考實作：七家（section15-cross-engine.md） | 「怎麼做」：見第 3 節的系譜欄 |

### 7.3 預算

- **記憶體**：每種訊息 ≤ 上限 × 紀錄大小；紀錄＝內容＋8 位元組序號。預設 1024 筆：內容 32 位元組的訊息約 40 KB；一個 World 100 種訊息全部頂滿約 4 MB。游標表：每個（讀者、訊息種類）8 位元組。丟棄記錄：每次 Run 每種訊息至多一筆，大小和讀者數成正比。**這些是算的，沒有量**；實作批在一個 host 上量實際的峰值，與這裡比。
- **時間**：讀取從「掃全部紀錄、挑兩個 tick」變成「從游標往後掃」；修剪每次最外層 Run 一次。推送式的成本隨 observer 數，本批不設執行時間的判定；實作批若要設，事前宣告。

---

## 8. 守衛與測試的處置（草案；每案在實作批逐案判斷、逐條請使用者核准）

| 守衛 | 守的性質 | 處置 | 放寬的面 | 收緊的面 | 之後由誰守 |
|---|---|---|---|---|---|
| J-36（F-7、F-8） | 讀者依位置與 tick 讀；每筆對每個執行了的讀者恰好一次；隔兩 tick 讀不到；順序 (tick, writerPos, seq)；hook 事件的 writerPos | 改（改名＋游標） | 「隔兩 tick 讀不到」不再成立：沒讀過的讀者之後仍讀得到（上限內）；順序改成寫入序號（Serial 下兩者同序，巢狀跑時舊的鍵沒有定義） | 被 run 條件跳過、schedule 沒跑的讀者下次補讀；「每筆對每個讀者恰好一次」擴大到全部跑過的讀者（上限內）；碰到上限有聚合的丟棄記錄 | Kernel |
| J-37（F-9、F-E1） | 事件跨 domain、同 domain 的非 clock schedule 讀寫是啟動錯誤 | 拿掉（統合 7.2 已列） | 任何 schedule 都能讀寫 | 無 | 游標保證不漏讀 |
| J-54 的「清舊事件不在預設位置」「輪裡出錯時清舊事件不做」 | 修剪排在清空輪之前、出錯時不修剪 | 改：修剪改在清空輪之後；「出錯時不修剪」留 | 無 | 無 | Kernel |
| J-56 的「內層 clock 修剪」 | 巢狀跑的內層修剪 | 改：巢狀跑不修剪，只在最外層 Run 結束時 | 內層讀過的紀錄多留到最外層結束 | 無 | Kernel |
| J-24 的 F-E1、D-E1 的 eventTrim 案 | 錯誤代號各一個最小 Registry | 刪這兩案 | 不再報 | 無 | — |
| J-18、J-20 的 golden | Plan 決定性、指紋 | 改：`EventTrim` 步拿掉、observer 進 Registry 指紋，golden 重產（L10） | 舊 golden 不再比對 | 無 | Kernel |
| `SignatureTests.cpp` 的事件案（`:23`、`:54`、`:162`） | 簽名推出的存取、同一 System 不能讀寫同一種 | 改名（判定不變） | 無 | 無 | Kernel |
| `HookFlushTests.cpp:134-145`、`WorldTests.cpp:772-800` 的讀者 | 用 `CurrentTick` 判讀到的時機 | 改：改用寫入序號與讀者的執行次數判 | 逐案 | 逐案 | Kernel |
| `naming_guard.py`、`kernel_symbols.py` | 命名、符號清單 | 改：`Event*` → `Message*`；加 `Event`、`Observer`、`Trigger`、`On` | 無 | 範圍變大 | 同 |
| 新：保留式 | 上限與聚合的丟棄記錄、不執行的讀者補讀、從沒跑過的不拉住、新讀者從最舊讀起、動態拿掉的讀者釋放保留、run 條件與 hook 與 observer 的游標、修剪時機、推游標不算寫入（驗證不誤報） | 新增 | 無 | 全部是新的性質 | Kernel |
| 新：推送式 | 當場與同步點兩條時機；hook 與 observer 的先後（新增時 hook 先、移除時 hook 後）；元件 Add／Remove 觸發；observer 的存取驗證；observer 的延後變更照 hook 規則；同樣的登記得到同樣的 observer 順序；observer 不能巢狀跑 schedule | 新增 | 無 | 全部是新的性質 | Kernel |

**突變**：事件相關的現有突變照 J-36 的改寫逐條判；新性質各配突變，預期的失敗寫明是哪一個案例。DI-KS 10 的新守衛照 AGENTS §14.2 送換模型的敵對式檢查。

---

## 9. Design Invariants 與 Replacement Criteria（草案；核准後在實作批寫進 `target/kernel.zh-Hant.md`）

**DI-KS 10（改寫）**：**事件有兩種，都在 Kernel，都不看時鐘。**
- **保留式（`Message`）**：每個讀者一個游標，在 World，以（掛載點、訊息種類）為鍵；System 掛載、run 條件、hook、observer 都是讀者；讀者在上限內不漏讀，Kernel 自己保證——被 run 條件跳過、所在 schedule 沒跑的讀者游標不動、下次補讀；跑過一次的讀者才拉住保留，第一次跑從還留著的最舊一筆讀起；每種訊息一個寫入序號，讀者依序號讀；一筆紀錄在拉住保留的讀者都讀過時修剪（每次最外層 Run 結束），或被上限擠掉時丟掉並在這次 Run 按種類聚合記一筆；修剪不在 Plan 裡；推游標不算寫入；Kernel 只有游標一種讀法；訊息不回滾。
- **推送式（`Event`＋`Observer`）**：不保留；`ExclusiveWorld` 與 World 外面的觸發當場跑 observer，`Commands` 的觸發延到同步點；同一點內 observer 的先後是決定性的、不承諾是登記順序；元件新增時 hook 在 observer 之前、移除時在之後；observer 是 System，存取照簽名推出、照存取驗證，不進 Plan 的步驟。
- 守衛：J-36 改寫、第 8 節新增的兩組。

**DI-KS 11**：「hook 只有 OnAdd／OnRemove、同步執行、依登記順序」照統合 9.1；補一句「和 observer 的先後見 DI-KS 10」。

**DI-KS 14**：「每個 domain 的事件只在它的 clock schedule 內」那一句拿掉（統合 9.1 已改寫，本頁確認）。

**Replacement Criteria**（加進 Kernel 的 RC，照 L17 不數次數）：
- 為了某一種事件、某一種策略（回滾、網路），事件機制要多一種讀法、多一種保留規則或多一種觸發時機——Kernel 職責清單「事件」那一項要拆成更多項，停下來問。
- 游標或修剪要知道時鐘、schedule 的語意（哪張是固定步、哪張會暫停）才做得對。
- observer 需要進 Plan 的步驟、或要對作者承諾 observer 的先後，才做得出某個 Feature。
- 上限的預設值或丟棄記錄要依產品分支（共通層出現產品專屬的判斷，AGENTS §9）。

---

## 10. 批次、模型、估計

| 批 | 內容 | 依賴 | 實作者／審查者 | 估計 |
|---|---|---|---|---|
| 事件游標的實作（保留式改名＋游標；推送式） | 第 5.1、5.2 節；第 8 節的守衛與突變；golden 重產 | 本宣告與統合核准；Kernel 減法的「結構段」、編排層骨架；**和時間 Module、拿掉時鐘與 `EventTrim` 同一條線**（統合第 8 節；主會話已在 `rebuild-batch-plan.zh-Hant.md` 照它重排，待使用者確認） | 上段（公開介面、Design Invariants）：Opus／high；審查 Fable／high；§14.4 的檢證者 Fable／high；DI-KS 10 守衛的敵對式檢查 Fable／high | 保留式 8～12 h（游標表與修剪 3～4 h、上限與聚合的丟棄記錄 1～2 h、改名與拿掉 `EventTrim`／`eventTrim`／F-9 1～2 h、J-36 改寫與新案例 2～3 h、突變與檢查 1～2 h）；推送式 8～12 h（登記與觸發、兩條時機、和 hook 的先後、元件 Add／Remove 觸發、存取驗證、測試與突變）。這是讀程式量估的 |

推送式在線內排在保留式之後（同一條 PR 線），因為 observer 也是保留式的讀者，游標表要先有。

---

## 11. 關聯（修訂 0 回報的，與修訂 4 新加的）

修訂 0 回報的六條，統合都已處理（修訂 16、19、20、28）：L13×L5（重建 Plan 時同一步更新游標表）、L13×L6（交出上限與讀者名單）、L13×L9（預設上限不放 `KernelSettings`）、L13×摘要與驗證、L13×L11（判定為不進快照、Kernel 不加訊息的原語；「可回滾的不參與」的檢查移到回滾策略，兩個方向）、批次先後（統合第 8 節與主會話的批次表已對齊）。

**修訂 4 新加的**（推送式帶來的；都在 Kernel 事件的職責內，本頁照 L17 寫在第 3.3 節末，收口檢查時請檢查者一併看）：

1. **observer × hook 清空輪（F-12）**：observer 的延後變更走 hook 的規則（下一窗、清空輪、上限）。
2. **observer × 巢狀執行（DI-KS 16～21）**：`ExclusiveWorld` 當場觸發時 observer 等同巢狀的一次 System 呼叫；observer 不能巢狀跑 schedule。
3. **observer × L6、L10**：observer 交進唯讀登記表、進 Registry 指紋。
4. **observer × 回滾**：推送式不保留、重跑時重新觸發；讀「重跑中」要依賴回滾 Module（統合 15.2 已記「重跑中」放哪交回滾份考慮）。

---

## 12. 範圍的限界

- 這是宣告，沒有寫程式、沒有量測；第 7.3 節的記憶體與第 10 節的時間是算的、估的。
- 系譜照 `section15-cross-engine.md`（子 agent 讀本地原始碼）與本卡修訂 0 讀的出處；Mass 只有範例專案、Unreal 引擎本體不在本地；本地沒有 bevy_ggrs。
- 第 3、4 節標「統合照 L17 判」的是統合的判定，不是使用者的話；使用者的原話照 HANDOFF（`claude/gyo2-time-design` 2416d4c0）與統合 2.1 的轉載，沒有重讀原始對話。
- 第 3.3 節末「本頁照 L17 補的細節」三條是本卡的判斷，沒有經統合逐條確認；收口檢查請一併看。
- 本頁沒有送檢查；四份一起送換模型檢查由統合那張卡負責。
