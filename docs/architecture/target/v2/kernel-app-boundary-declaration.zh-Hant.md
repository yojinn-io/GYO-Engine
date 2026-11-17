# Kernel 與編排層的邊界：事前宣告（統合；修訂 33）

> 性質：事前宣告（AGENTS §3 的 Architecture Delta；§2 的基盤 Architecture）。**使用者核准之前不實作、不改任何條文與守衛。核准本宣告不等於 AGENTS §14.4 的判定變更核准**：每一個守衛判定的變更，在實作那一批的事前宣告逐案列出、逐條核准。
> 批次代號：`v8-arch-gyo2-kernel-app-boundary`。推導與四家（Bevy、flecs、Unity Entities、Unreal Mass）的對照在[設計稿](kernel-app-boundary-draft.zh-Hant.md)。

**這份是三份宣告的統合**（使用者 2026-11-17，`v8-arch-gyo2-boundary-split-integrator-first`）：

> 「其實我想拆成3份，但是我怕有關聯的內容你會誤判。有這個可能性嗎」「就是併發3份然後有一個統合對麼。」

- 統合（本頁）先立：層與歸屬、**跨份的關聯表（第 1 節）**、會改 Kernel 公開介面的項目。
- 權威 Module、事件游標、回滾 Module（2026-11-17 加入）的宣告由主會話另開卡，**以本頁為準、不改本頁定過的事**；發現新的關聯回報本卡。
- 收口：三份一起送換模型檢查（本卡負責），專找「每份都說是別份負責」的縫，缺口補回本頁，再請使用者逐件定。

**修訂紀錄**：修訂 0、1 各送 Fable／high 敵對式檢查（16 條／12 必改；15 條／9 必改；證據 `declaration-r0-adversarial-check.md`、`declaration-r1-adversarial-check.md`）。修訂 1 在 git 歷史（d169e1f7）。修訂 2 改寫成統合，修訂 1 檢查的處理見第 12 節。修訂 3（2026-11-17）：L2 時鐘定案、新增 L20 Module 之間的依賴。修訂 4（同日）：L20 定案。修訂 5（同日）：L3 定案（拿掉執行期攔截）。修訂 6（同日）：L4 定案；7.3 補有型別／無型別的範圍；記通用考量 `engine-not-rigid`。修訂 7（同日）：L5 定案（生命週期、動態插入、每個 World 的宣告）。修訂 8（同日）：L6 定案（唯讀登記表、版本號）。修訂 9（同日）：L7 定案（`Writes<…>` 選填）。修訂 10（同日）：L9 定案（`KernelSettings` 准讀准寫、下一次 Run 生效）。修訂 11（同日）：L11 定案（回滾獨立 Module、策略）；新增 L21（策略的構造）；子卡加回滾份。修訂 12（同日）：L16 定案。修訂 13（同日）：Loading 卡已歸檔，「交給 Loading 卡」改成「生成世界之後另開一份宣告，放編排層」。修訂 14（同日）：L17 定案（兩條核心、每層的職責清單、agent 自己判、對照同類型多家）；第 10 節的待判事項全部定完。修訂 15（同日）：L15 定案（產品可以只選部分 Module）。修訂 16（同日）：收游標份修訂 0 回報的七處介面與六條新關聯（L5、L6、L11、L13）；批次先後與主會話的批次表對齊的提案（第 8 節）。修訂 17（同日）：收回滾份修訂 0 回報的六件（L11 原語、L20 回滾→時間、新增 L22）；重跑在哪裡發動交使用者。修訂 18（同日）：收權威份修訂 0 回報的 N1～N8（L6、L10 補充；新增 L23～L28）；N5 交使用者。修訂 19（同日）：四份一起送 Fable／high 交叉檢查（15 條／10 必改；證據 `four-docs-adversarial-check.md`），處理見第 13 節；新增 L29、L30；要問使用者的題目合併去重、排好順序（第 14 節）。修訂 20（同日）：四份修正後送 Fable／high 第二輪檢查（17 條新的／6 必改；上一輪 15 條：已解決 6、部分 8、子份解決統合沒跟上 1；證據 `four-docs-adversarial-check-r2.md`），統合的部分本頁已改，處理見第 13 節末；第 14 節重排。修訂 21（同日）：收權威份修訂 2（4a4f177b）——L26 改成作者用名字標記、權威與狀態機互不依賴；權威 6.15 排進第 14 節第 7 題；權威 4.4 的代為登記時機列進第 3 題的材料。修訂 22（同日）：使用者在 side chat 定第 14 節第 4 題——伺服器權威策略不數寫者（`v8-arch-gyo2-authority-no-writer-count`）；L28 不用定；第 14 節去掉因此不用問的題；權威份照這個決定出新修訂、列出連帶影響，之後四份再做一次換模型的交叉檢查（使用者同意的做法）。修訂 23（同日）：收權威份修訂 3（9efe5e1a）——`WritePolicy` 改成拿掉；權威 6.16（判定資料的讀寫規則）排進第 14 節第 1 題；豁免寫者的登記隨 6.16。修訂 24（同日）：使用者在 side chat 定第 14 節第 2 題——不替回滾開 DI-KS 18 的例外，改建機制：System 登記時宣告能不能當場改成員（`v8-arch-gyo2-system-declares-immediate-structural-change`，代號待主會話定）；第三輪檢查（21 條／9 必改）未處理，收口照停止回報等使用者選路。修訂 25（同日）：使用者選「先問會收斂的題、一題一題來」，並要求先問底層機制；第一題定了：可以當場改成員的宣告沿用 `ExclusiveWorld`（`v8-arch-gyo2-immediate-change-via-exclusive-world`，代號待主會話定）。修訂 26（同日）：使用者在 side chat 定事件照 Bevy 分兩種——現在的保留式改名（對應 Bevy `Message`）、增加推送式（對應 Bevy `Event`＋`Observer`），位置照 Bevy（`bevy_ecs` → Kernel）（`v8-arch-gyo2-events-two-kinds-like-bevy`，代號待主會話定）；Kernel 職責清單因此多一項，由這個決定涵蓋（修訂 32 註：15.1 判定寫進 DI-KS 15 第 (2) 項「事件」之內，不另列一項）。修訂 27（同日）：使用者「什麼都要我來定，乾脆代碼我也自己寫好了。名字你有命名規範。例子你有源碼。」——第 14 節的題目重篩，能照 L17 判的由統合判（第 15 節），只剩兩件交使用者。修訂 28（同日）：第 15 節逐條對照本地七家原始碼（證據 `section15-cross-engine.md`）後更正系譜與四處判定；判定資料的讀寫規則照使用者的原則判為兩半都拿掉（15.3），B1～B3 的改寫進確認清單。修訂 29（同日）：確認清單經使用者核准；照第三輪檢查（證據 `four-docs-adversarial-check-r3.md`）改統合自己的部分——寫者規則與讀寫規則拿掉之後，3.2、7.2、9.1、L 表的殘句全部跟上。修訂 30（同日）：收回滾份修訂 4（1f83041f）回報的兩件，統合照 L17 判——拿掉「不跑 hook 的結構變更」原語、留「指定 stamp 寫回」；「重跑中」放時間 Module，列為時間宣告的要求。修訂 31（同日）：收權威份修訂 4（b16db5ba、2440b336）回報的四件：「(c) 區的時間只來自 tick」不再由啟動檢查守，改寫使用者 L2 的一句，補進確認清單第 11 件（待使用者）；時間設計 DI 2 失去守衛、`OneEndOnly` 的網路需求、讀檔卡的通知，統合照 L17 判。修訂 32（同日）：最後一輪（第四輪）檢查 18 條／10 必改（證據 `four-docs-adversarial-check-r4.md`），統合的部分本頁已改、設計的缺口照 L17 判（第 16 節）；確認清單補第 12、13 件，與第 11 件一起待使用者。修訂 33（同日）：收讀檔卡（修訂 10，aaea3dbd）經主會話轉來的請求——建 World 後、第一次 Run 前設配發器範圍的時機，統合照 L17 判（Kernel 給設定、編排層骨架給時機）。

## 0. 依據

**讀的版本**：程式 gyo2 cb6bccca（`origin/gyo2` 到 2026-11-17 沒有新 commit）；HANDOFF `claude/gyo2-time-design` 25515a80 以後；Module／Plugin 定義 PR #240（`claude/gyo2-module-plugin-definition` b0ce94ed，**未合併**）；Bevy 6a911653f。證據：主 checkout `build/evidence/gyo2-kernel-app-boundary/`。

**新舊衝突的規則**（使用者：「以最新的為準，如果拿不準的去檢查一下定案的理由和時間」）：較新的為準；第 2.2 節逐條寫出較新的有沒有回應較舊的理由。

**文件的位置**：「v2 之前的 target 文件先不動，後面統一處理」（`v8-arch-gyo2-pre-v2-target-docs-later`）。新條文（編排層）放 `target/v2/`；Kernel 的 Design Invariants 仍在 `target/kernel.zh-Hant.md`（K0 於 11-16 更新過），改寫在實作批做；`runtime-composition`、`world`、`scheduler`、`system-contract`、`limits` 只列「會被影響」，不改。

---

## 1. 關聯表（三份的交界；兩張子卡開頭引用這張表）

**修訂 32 的說明**：本表各列的「狀態」寫的是當時的情形；凡寫「待使用者」「待某份」「第 14 節」的，以第 15、16 節與第 2.1 節的決定為準。

欄位：**誰定**＝哪一份的宣告做決定；**誰等它**＝哪一份要以它為前提；**動到誰的介面**；**狀態**。

| # | 事項 | 誰定 | 誰等它 | 動到誰的介面 | 狀態 |
|---|---|---|---|---|---|
| L1 | 分層與歸屬（第 3 節） | 統合 | 權威、游標、回滾、生成世界、時間、狀態機 | Kernel、編排層 | 已定（第 2.1 節的決定） |
| L2 | **時鐘與 tick 移出 Kernel，到時間的 Module**（`Domain`、`ScheduleOptions::clock`、Kernel 推 tick、`CurrentTick`、`AccessKind::DomainTick`、tickStamps、D-E12、F-E1） | 統合 | 游標（順序鍵、上限的單位不能用 tick）、權威（「tick 只有一個來源」由伺服器權威策略檢查）、時間 T1／T3、L8、L20 | Kernel 公開介面、時間 Module | **已定**（2026-11-17，`v8-arch-gyo2-clock-out-of-kernel`；第 10 節第 1 件） |
| L3 | **執行期的攔截**（第 7.3 節的六條） | 統合 | 權威（改靠 digest 比對與伺服器覆蓋抓錯，內容在 L12） | Kernel 公開介面（拿掉） | **已定**（2026-11-17，`v8-arch-gyo2-no-runtime-interception`）：拿掉執行期的攔截；伺服器權威改靠 digest 比對與伺服器覆蓋來抓錯（使用者的原話只到這裡）。「Kernel 不加觀察點」是卡片的說法、不在原話裡，和 11-15 的「誰實際寫的」排錯工具相關，見 L30 |
| L4 | **生成世界時寫進去的初始值算不算寫者** | 統合 | 權威（寫者規則只數「修改」）、生成世界（不受綁） | 權威 | **已定**（2026-11-17，`v8-arch-gyo2-init-values-like-bevy`）：參照 Bevy——初始值在建立實體的當下給（`Spawn` 帶的值、`InitialValue`），生成世界就是一般流程裡的 schedule，不另設「寫入不算」的階段；伺服器權威只數修改既有值的寫者（修訂 29：伺服器權威已不數寫者，這一句只剩歷史意義）。「對已存在的實體插入元件」算建立還是修改，由權威份定。**L28 已不用定**（不數寫者） |
| L5 | **登記的生命週期與動態插入** | 統合 | 編排層骨架、權威（檢查時機；PvP 宣告不允許）、指令集（未來） | Kernel 公開介面（`Registry`、`WorldExecutor`） | **已定**（2026-11-17，`v8-arch-gyo2-dynamic-insert-per-world`）：Kernel 提供「開 Registrar」「建 Plan」兩步、可重複呼叫；編排層 build → 建 Plan → finish → 權威檢查；World 在 Ready 時可再登記或拿掉 System、下一次執行前重建受影響的 Plan（「凍結」只代表這一次執行的 Plan 已排好）；**每個 World 建立時宣告准不准動態插入**，預設允許、不保證指紋一致；不允許時插入被拒絕：回傳執行時錯誤、記警告、不退出、World 照跑；伺服器權威策略在啟動時要求它的 World 宣告不允許。這一批只定契約與介面，Plan 的重建先薄。**游標的接點**（游標份新關聯 1）：重建 Plan 時同一步更新游標表（新讀者從哪讀起、被拿掉的讀者釋放保留，行為由游標份定） |
| L6 | Kernel 交出的唯讀登記資訊（第 3.3 節） | 統合 | 權威、除錯工具、編輯器、指令集 | `FrozenRegistry` | **已定**（2026-11-17，`v8-arch-gyo2-read-only-registry-view`）：交出唯讀的表（補上 hooks、`InitialValue`、schedule 選項與 stage、順序圖），帶版本號、每次重建 Plan 加一；交出去的參照只在當前版本有效（或交複本）；修改一律走 `Registrar`（L5 的關卡）；**另交出每種事件的上限與讀者名單**（游標份新關聯 2）；**另交出每個 World 建立時的宣告**（准不准動態插入等，權威份 N4：策略要在啟動時核對） |
| L7 | `ExclusiveWorld` 的存取與 `Writes<…>` | 統合 | 權威（數寫者）、狀態機、時間（驅動固定步的 System）、指令集 | `Params.hpp`、驗證（`World.cpp`） | **已定**（2026-11-17，`v8-arch-gyo2-exclusive-writes-optional`）：隨 L3，`ExclusiveWorld` 能用閉包讀寫任何元件與 Resource 的值（不再分區、不再回 Err）；`Writes<…>` 留作**選填**的標註，Kernel 不解讀，伺服器權威策略可以拿它數寫者（修訂 22：伺服器權威已定不數寫者，`Writes<…>` 與 `Shared` 標記的用途要由權威份重看，標註本身照 L7 留）。留下的 Kernel 機制：閉包與回傳值不指回 World（DI-KS 21）、結構變更只經同步點（DI-KS 18）、唯一參數（S-10）。指令的插入照 L5 的宣告 |
| L8 | 不靠時鐘的計數（`RejectedSpawn`、診斷紀錄帶的 tick） | 統合 | 游標（事件順序鍵也不能用 tick） | Kernel | 隨 L2：改用 Kernel 自己的計數（stamp 或同步點序號；「Kernel 讀時間 Module 的 Resource」違反第 3 節的依賴方向）。用哪一個由實作批定。**統合 2026-11-17 照 L17 判**（修訂 19）：計數是 Kernel 自己的（World 的儲存與同步點，DI-KS 15 的清單內），選 stamp 或同步點序號是實作的細節、不是層的邊界，不問使用者 |
| L9 | D-E18（System 不准宣告 `Res`／`ResMut<KernelSettings>`） | 統合 | — | Kernel | **已定**（2026-11-17，`v8-arch-gyo2-kernel-settings-read-write`）：拿掉 D-E18；System 准讀准寫 `KernelSettings`，**下一次最外層的 Run 才生效**（Kernel 每次最外層 Run 開始時重讀，取代「第一次 Run 時快取」）；`KernelSettings` 只放開發與調校的設定（驗證、出錯時停下、hook 清空輪上限）；L5 的開關不在這裡 |
| L10 | Registry 與 Plan 指紋改版、golden 重產（第 3.4 節） | 統合（改版這件事）；游標（Plan 的 `EventTrim`）、權威（C 指紋） | 三份 | 指紋格式 | 已定為後果；重產次數見第 8 節。**跨端比較的指紋不用 Kernel 的編號**（權威份 N3）：Client 的模擬 World 比 Match 多登記一般資料，同一份判定資料拿到的 `ComponentId` 會不同（`Fingerprint.cpp:340-352` 現在寫的是 Kernel 的 id）；C 指紋與之後的 wire id 用權威份第 6.8 節選的編號（使用者判）。Registry 與 Plan 指紋只在同一端比，照舊用 Kernel 的編號 |
| L11 | **回滾的機制放哪** | 統合（Kernel 的原語）；**回滾份**（回滾 Module 的設計） | 權威（伺服器權威策略選用某個回滾策略）、游標（事件進不進快照）、狀態機 | Kernel（原語）、回滾 Module、權威 Module | **已定**（2026-11-17，`v8-arch-gyo2-rollback-module-strategy`）：Kernel 只給原語（讀寫整欄、設 stamp、整批複製／替換之類，具體清單由回滾份回報統合定）；回滾是**獨立的 Module**，有自己的策略：`RollbackStrategy` interface＋官方實作，作者可以自己寫，也可以繼承官方實作改裝；第一版官方只實作 PvP Client 要的「狀態觸發、逐元件歷史、平滑修正」，P2P 的「輸入觸發、存整個狀態」佔位、選了啟動時報錯；伺服器權威策略依賴回滾 Module（登記檔，L20）。回滾的設計另開第四份宣告。**事件進不進快照**（游標份新關聯 3）：要進的話，原語要有「取出／放回一種事件的紀錄、寫入序號、讀者游標」；Kernel 只有游標一種讀法。使用者 11-14 的「可回滾的 schedule 不參與」（`time-plan-two` (5)）的檢查，從 Kernel 的啟動檢查移到回滾策略讀 L6，落點統合照 L17 判（回滾的規則屬回滾 Module）。**修訂 19 更正：內容不是全部不變**——時間設計 6.3 (2)（主會話的設計稿；使用者在 `time-plan-two` 定下的內容只有 (5)「可回滾的 schedule 不參與」那一句，修訂 20 更正）還寫了「它自己的讀者維持依位置讀」與「每幀那一側要送資料進可回滾的固定步，照規格既有的規則走 Ingress」；前者被游標份 4.8 拿掉（Kernel 只有游標一種讀法），後者的 Ingress 隨區移到權威 Module（3.2）。這兩處要請使用者確認（第 14 節）。另外，PvP Client 的固定步是可回滾的：游標份拿來當 Feature 的「音效時刻（PvP）」「固定步寫命中、每幀讀」若照回滾官方策略的跨 schedule 檢查會被擋；游標份要註明 PvP Client 的模擬不走這條路，或由第 14 節的事件題目一起定。6.3 (2) 的原文兩個方向都擋（「它也不讀別的時鐘的事件」）：每幀寫、固定步讀在 PvP Client 同樣被擋（修訂 20 補）。**回滾份修訂 0（`claude/gyo2-rollback-module-8bac02` b55af62e）回報的原語，統合 2026-11-17 照 L17 判**：Kernel 給——整欄讀出（id、added、changed、值）；整欄放回（成員與值一次換掉）；指定 stamp 寫回（或明確標目前 stamp；系譜 Bevy `set_last_changed`）；Resource 的 stamp 與存在一起讀寫；跑 hook 的結構變更（修訂 30：「不跑 hook」那一條拿掉——官方回滾與 P2P 佔位都跑 hook，沒有點名的 Feature（AGENTS §1）；Bevy 的 hook 也一律會跑。「指定 stamp 寫回」留著：Bevy 核心本來就有 `set_last_changed`（`change_detection/params.rs:1546`），屬 ECS 的能力，作者自寫策略可以用）；**實體存活集合的讀出與放回**（修訂 19 補：存活是獨立的 `entities_`，`World.cpp:89`、`:129-134`，不從欄推出；只放回欄而不放回存活集合，T 之後才生成、現在還活著的實體會讓重跑的 `Spawn` 被丟成 `IdAlive`）。會改成員的原語（整欄放回、存活集合放回）只在 `ExclusiveWorld` 裡可用（修訂 32）；**在獨占 System 裡能不能用：**已定**，沿用 `ExclusiveWorld` 當場改（`immediate-change-via-exclusive-world`）**（第 10 節第 12 件）。Kernel 拿掉 `DiagnosticsBuffer::Entry::rollbackActive`（`detail/Storage.hpp:229-233`）；`RejectedSpawn` 不加 `rollbackActive`（讀檔卡的要求），作者讀時間 Module 的「重跑中」旗標（修訂 32；時間宣告不收時退回由回滾 Module 登記）——Kernel 不認得回滾（DI-KS 15 的職責清單） |
| L12 | 伺服器權威檢查哪些、預設區、C 指紋與 G-5 匯出的內容、digest 比對與伺服器覆蓋、(c) 區獨占套用的新形狀、權威 Module 的名字；`AuthorityStrategy` interface 的形狀 | **權威** | 統合（缺口回報）、狀態機 (c) 區、回滾份 | 權威 Module | 待權威份；回滾的內容移到回滾份（L11） |
| L13 | 事件游標：游標放哪、上限與單位、丟棄記錄、不執行的讀者、順序鍵、跨時鐘事件的網路要求（11-14「也要兼容網絡部分」） | **游標** | 統合（L2、L11）、時間 | Kernel 的事件 API（`EventReader`、`EventWriter`、`World::EventPayloads`、Plan 的 `EventTrim`）——**只限這些；動到別的 Kernel 介面回報統合**。游標份修訂 0（`claude/gyo2-event-cursor` facc0846）回報的七處，統合 2026-11-17 照 L17 判（都在 Kernel 的「事件」職責內或屬既定的層，不問使用者）：(1) `EventOptions` 加上限——准；(2) `ScheduleOptions::eventTrim` 與 D-E1 的 eventTrim 檢查拿掉——准（3.2 補列）；(3) World 上取走丟棄明細的函式（照 `RejectedSpawn` 的形）——准；(4) World 摘要的格式：摘要涵蓋「World 裡的資料」，游標若定在 World（游標份 4.1）就含、寫入序號同；摘要改版併入 L10；權威的 digest 要不要含游標由權威份定；(5) L6 的唯讀登記表交出每種事件的上限與讀者名單——准（L6 的延伸）；(6) 存取驗證：推游標不算寫事件、快照把游標表與事件內容分開——准（驗證的內部）；(7) 預設上限不放 `KernelSettings`——准（L9：`KernelSettings` 只放開發與調校） | 待游標份（8 件交使用者）。**修訂 26**：使用者定事件照 Bevy 分兩種（`events-two-kinds-like-bevy`）：游標份的內容只適用於保留式（改名）；推送式另立（名字、觸發時機、和 hook 的關係、observer 的存取怎麼算進排程與驗證，待統合問使用者） |
| L14 | 生成世界的形狀（每個 World 一次或分批、誰推動、什麼時候算好、讀取畫面） | **生成世界的宣告**（之後另開一份，放編排層；生成世界的宣告已在使用者要求下歸檔，L16 的答案記在 `recovered-from-stale-cards.zh-Hant.md` 第 2 節，分支 `claude/gyo2-time-design`） | 統合（L4、L16） | 編排層 | 等統合；L4 定了之後不再綁 Kernel 減法 |
| L15 | 產品能不能只選部分 Module（「產品選用權威」的前提） | 使用者（在本卡直接回答；定義卡已歸檔） | 權威、回滾、時間 | 建置層、編排層 | **已定**（2026-11-17，`v8-arch-gyo2-products-pick-modules`）：能。原話「能，要不然單機遊戲也必須上權威了。」Kernel、編排層這類別的 Module 都依賴的，由依賴（L20）帶進來，不用另訂「必選」 |
| L16 | 兩層的讀法：Engine（編排層）認得遊戲初始化與生成世界的階段、Kernel 不認得 | 統合 | 生成世界 | — | **已定**（2026-11-17，`v8-arch-gyo2-two-layers-like-bevy`）：照 Bevy——`bevy_app` 認得開局（`Main` 的開局清單，`bevy_app/src/main_schedule.rs:213-305`），`bevy_ecs` 不認得（搜不到 startup）。編排層認得、排流程；Kernel 只跑被叫的 schedule |
| L17 | 替換條件與歸屬的判斷方式（取代「兩次以上」） | 統合 | 權威、游標、回滾（它們的 RC 同理） | — | **已定**（2026-11-17，`v8-arch-gyo2-layer-duty-lists`）：固定兩條核心——「Engine 只提供機制」「Kernel 的職責是 ECS」；每層寫一份可核對的職責清單；不在清單上的預設放別層，**由 agent 自己判、不問使用者**；判斷時要對照**同類型的多家實作**（Kernel 是 ECS，就看 ECS 引擎），找出它們的設計原因再交叉比較，取它們的能力、不取技術債；替換條件改成「某層的職責清單要多加一項，或判不出屬於哪一層」就停下來問 |
| L18 | 狀態機 (b) 區（已核准）的影響：初始進入改由編排層的遊戲初始化保證；`ExclusiveWorld` 隨 L7；`RegisterStateMachine<S, Zone::C>` 的 `Zone` 隨 L12 | 統合列影響；狀態機卡處理 | 狀態機卡 | 狀態機 Module | 列出，不判 |
| L19 | 時間設計 T2（runner、交換函式的登記）、T5（多個 World）改在編排層；T4 改成游標份；T1／T3 的時間 Module 持有時鐘與 tick（L2） | 統合 | 時間設計 | 編排層、時間 Module | 已定（位置） |
| L20 | **Module 之間的依賴**（例：伺服器權威策略要檢查「tick 只有一個來源」，權威 Module 依賴時間 Module） | 統合（編排層讀登記檔、檢查）＋定義卡或建置層的批（登記檔的格式） | 權威、游標、時間、狀態機 | 編排層、建置層；Kernel 不參與 | **已定**（2026-11-17，`v8-arch-gyo2-module-deps-in-registers`）：依賴**只寫在 Module／Plugin 登記檔**的「依賴」欄（11-14 已定每列寫依賴、CMake 照它 link）；編排層從登記檔讀出依賴並在 build 時檢查（O-2）；**程式碼裡不另外寫 `Requires`**；不做策略層級的依賴，權威 Module 整個依賴時間 Module。登記檔給 CMake 與編排層兩邊讀的格式，在編排層骨架批或建置層的批定。**回滾依賴時間 Module**（回滾份新關聯：重跑要把 tick 退回）：照 L20 寫在登記檔，不另立「步數讀寫」的 interface（只有回滾一個用的人，AGENTS §1；lightyear 也直接改 `Time<Fixed>`）——統合照 L17 判 |
| L21 | **策略的構造**（權威與回滾共用）：interface 是契約、保證不變；Engine 附官方實作，和作者寫的走同一個插口、沒有特權；作者可以從頭實作 interface（廢掉官方的），也可以繼承官方實作改裝——後者是作者的選擇，官方實作的內部不保證不變 | 統合 | 權威份、回滾份 | 權威 Module、回滾 Module | **已定**（2026-11-17，`v8-arch-gyo2-strategy-interface-impl`）；C++ 的寫法：虛函式的抽象基底類別（GYO 的 `Plugin` 已是這個寫法，`Registry.hpp:60-75`） |
| L22 | **權威的登記與回滾的登記**：PvP 裡「伺服器權威的元件」與「Client 可回滾的元件」幾乎同一組型別；單機時間倒流只有後者 | 統合（關係）；權威份、回滾份（細節） | 權威、回滾 | 權威 Module、回滾 Module | **統合 2026-11-17 照 L17 判**：兩件是不同的事實（「誰說了算」與「能不能回到過去」），各自的 Module 各自登記，不違反 AGENTS §9；為了 PvP 的作者不寫兩次，伺服器權威策略可以替作者把它的權威元件登記進回滾 Module（和權威 Module 替作者登記進 Kernel 同形，`authority-own-registration`）；權威不讀回滾的內部，只用回滾 Module 的公開登記 |
| L23 | **網路／複製的 Module**（權威份 N1）：伺服器覆蓋的「搬運」、連線時交換 C 指紋、執行中送 digest、wire id 都需要一個呼叫者 | 統合（歸屬）；網路 Module 的宣告（之後，PvP 移植時） | 權威、回滾 | 網路 Module | **統合 2026-11-17 照 L17 判**：歸 Engine 的網路 Module（`v8-arch-gyo2-module-plugin-owner-definition`：網路是 Module，`NetModule`）；權威份只定「規定」（比什麼、何時比、不一致怎麼辦、覆蓋的規則），搬運與交換由網路 Module 做；網路 Module 的設計不在這一輪，權威份把它要的介面列成需求。**修訂 19 補**：網路 Module 把伺服器的值寫進 Client 的模擬 World，在伺服器權威策略看來是「多一個寫者」——網路 Module 要用權威 Module 的「豁免寫者的登記」（L24）把它的寫入 System 登記成不算寫者，列進需求（**修訂 29：讀寫規則拿掉後不需要豁免**，網路 Module 的寫入不受權威檢查）。修訂 31：網路 Module 寫進 Client 的 System 要標 `authority.OneEndOnly`，不進 C 指紋（權威份修訂 4 的 C 指紋收法） |
| L24 | **回滾相關的宣告與檢查歸誰**（權威份 N2）：`rollback` 的 Yes／No／Derived、Z6、Z8、D-E15、D-E16；伺服器權威怎麼接回滾策略；重跑時推 tick 算不算寫者 | 統合 | 權威、回滾、時間 | 權威、回滾 Module | **統合 2026-11-17 照 L17 判**：「哪些資料可回滾」（Yes／No）與 Derived（每 tick 在讀者之前重算）是回滾 Module 的登記與檢查（D-E16 讀 L6 的順序圖）；Z6、Z8、D-E15 這類「判定資料 × 回滾」的交叉規則是伺服器權威策略的檢查，讀回滾 Module 的公開登記（L22）；**作者各自選權威策略與回滾策略，伺服器權威只檢查兩者相容、不指定**（限制只放作者自選處）；回滾的還原與重跑時退 tick 不算寫者（舊規格 B.3「不算寫者：Runtime 的回滾還原」；lightyear 同）。**修訂 19 補（統合照 L17 判）**：(1) 回滾份要接 Derived 與 D-E16（修訂 1 全文沒有）；要不帶的話寫明並交使用者。(2) 權威份要接 Z6、Z8、D-E15（修訂 0 的 5.0 仍寫「不在這份」）。(3) 「不算寫者」要讓策略認得出來（**修訂 29：讀寫規則兩半拿掉，這一項與 L23 的豁免都不需要了**；修訂 23：不數寫者之後，這一項只在權威 6.16 留下「判定資料只由判定的 System 寫」那一半時才需要；拿掉的話 (3) 與 L23 的豁免都不需要**）：權威 Module 提供公開的**豁免寫者的登記**；回滾 Module 公開重跑者與還原者的名單，伺服器權威策略讀名單代為登記豁免（修訂 20 更正：回滾不依賴權威，不能自己登記）；網路 Module 同（L23）；作者自寫的回滾策略由作者在產品裡登記；時間 Module 的 tick 驅動者與 Client 對時改 tick 也要有處理（權威份 5.6、6.11）。(6) 代為登記（回滾的元件、「重跑時不跑」、豁免、只標記）要在建 Plan 之前做：「重跑時不跑」是 Mount 上的執行條件（`Registry.hpp:125-138`），只能在 `AddSystem` 當下給，伺服器權威的 World 又不准動態插入（L5）。權威份 4.4 要寫明這個時機（策略的 interface 加 build 時機，或由權威 Module 的登記入口依已選的策略做、策略要先選定），掛第 14 節第 3 題。(4) 「ingress 在重跑中不執行」不是使用者的題目：回滾 Module 提供「重跑時不跑」的公開登記，伺服器權威策略替作者把它的 ingress 登記進去（L22 同形；回滾不依賴權威）。(5) 可回滾的 schedule 要有登記處：`rollbackEnabled` 已移出 Kernel，回滾 Module 要登記並公開「哪些 schedule 可回滾」（記錄的掛點、跨 schedule 事件的檢查、Z6 都讀它） |
| L25 | **代為登記時的 owner**（權威份 N6） | 統合 | 權威、回滾（L22 也代為登記） | Kernel（3.5 的 owner） | **統合 2026-11-17 照 L17 判**：代為登記的 Module 用作者的 `Registrar`，owner 照舊是作者的登記單位；否則 `InitialValue` 的「只有擁有者能宣告」會誤報 |
| L26 | **狀態機 (c) 區的接法**（權威份 N7）：狀態機 Module 不依賴權威（單機也用） | 統合（關係）；權威份、狀態機 (c) 區的批（細節） | 狀態機 | 權威、狀態機 Module | **統合 2026-11-17 照 L17 判**：同 L22 的形——狀態機公開它的登記，伺服器權威策略可以替作者把狀態機的套用 System 登記成判定的；狀態機不依賴權威。**修訂 21 更新（統合照 L17 判）**：權威份修訂 2 選「權威不依賴狀態機」——依賴是整個 Module（L20），為一個標記把整個狀態機帶進每個選權威的產品不划算；改成作者用名字標記（`authority.MarkResource("名字")`、`MarkSystem("名字")`），權威與狀態機互不依賴，對不到唯讀登記表的名字是啟動錯誤。上面「伺服器權威策略替作者登記」那一句由此取代 |
| L27 | **時間 Module 要公開 tick 的型別與驅動者**（權威份 N8），驅動者標 `ExclusiveWorld<Writes<Tick>>`（L7 的選填標註） | 統合（要求）；時間的宣告（之後） | 權威（「tick 只有一個來源」） | 時間 Module | **統合 2026-11-17 照 L17 判**：列為時間宣告的要求。修訂 29：「tick 只有一個來源」定為權威的專用檢查（只准公開的驅動者與對時寫），所以驅動者與對時要公開；`Writes<Tick>` 是選填標註 |
| L28 | **L4 翻掉了 11-14 的 `v8-arch-gyo2-spawn-writer-funnel`**（權威份 N5）：11-14 使用者否決「生成不算寫者（比照 Despawn）」、選「作者用漏斗、Engine 提供漏斗工具」，理由是「Despawn 是機制，不能當成普通的實作參考」；L4 定「Spawn 帶的值不算寫者」 | 統合 | 權威（P6、5.1、6.3、6.7）、漏斗工具、讀檔卡（HANDOFF `init-values-like-bevy` 的連帶寫「讀檔放回的值不算寫者，已通知讀檔卡」） | 權威 | **待使用者確認**（第 10 節第 13 件）：L4 的理由是 Bevy 與四家的「初始值在建立當下給」，不是 Despawn 的類比，沒有直接回應 11-14 的否決；照「拿不準的去檢查定案的理由和時間」交使用者。**修訂 22：不用定了**——使用者定伺服器權威不數寫者（`authority-no-writer-count`），「生成算不算寫者」不再有東西要數。推出來的、待處理（不是使用者逐條定的）：11-14 的「作者用漏斗、Engine 提供漏斗工具」退掉，漏斗留作作者自選的寫法（事件、效果佇列）；PR #229 已實作的 B1～B3（5a9414c0）在新架構的去留另外看；權威 5.3 規則 1 對生成的「誰能寫」仍要權威份處理 |
| L29 | **回滾策略選在哪一層、哪些 World 可以沒有回滾**（四份交叉檢查第 2 條）：Match 不回滾，但權威 Module 整個依賴回滾 Module，回滾份又建議「沒選策略＝啟動錯誤」、7.3 提議權威要求「選了某個策略」；權威 6.2 問策略選在 World 還是 `Product`，回滾只寫產品的 `UseStrategy` | 統合（合成一題） | 權威（6.2、6.13）、回滾（第 10 節第 5 件） | 權威 Module、回滾 Module、編排層 | **待使用者**（第 14 節）：權威 6.2、6.13 與回滾第 5 件合成一題 |
| L30 | **排錯用的「誰實際寫的」**（`v8-arch-gyo2-who-wrote-debug-tool`，11-15 使用者確認「另外排一批」）與 L3：這個工具要在 Kernel 的寫入關口留記號（只記錄、不擋人），L3 的「Kernel 不加觀察點」是卡片的說法 | 統合 | 除錯工具、L3 | Kernel（寫入關口） | **確認題**（第 14 節第 17 題；修訂 20 更正：11-15 已定保留、另外排一批，L3 的原話沒有否定它）：只問記號留在 Kernel 的寫入關口算不算 Kernel 職責清單多一項 |

**三張子卡的範圍**（一句話；回滾份 2026-11-17 加入）：
- **權威 Module**：照 L12 重新評估伺服器權威策略到底檢查什麼，並設計策略介面、權威 Module 的登記、佔位策略；以 L1～L7、L10、L11 的統合結論為前提。開頭引用 L1、L3、L4、L5、L6、L7、L10、L11、L12、L15、L17、L18、L20、L21。
- **回滾 Module**：照 L11、L21 設計 `RollbackStrategy` interface 與第一版官方實作（狀態觸發、逐元件歷史、平滑修正）、P2P 的佔位，以及需要 Kernel 提供的原語（回報統合定）；以 L1、L2、L5、L6、L10、L11、L13、L20、L21 為前提。開頭引用 L1、L2、L5、L6、L10、L11、L13、L17、L20、L21。
- **事件游標**：照 L13 設計讀者游標與修剪，「讀者在上限內不漏讀」由 Kernel 保證；以 L2、L8、L10、L11 為前提。開頭引用 L1、L2、L8、L10、L11、L13、L17。

---

## 2. 已定的事

### 2.1 這一批依據的決定（使用者）

| 代號 | 內容 | 原話（摘） |
|---|---|---|
| `v8-arch-gyo2-kernel-is-ecs-no-restrictions`（11-16） | Kernel 是 ECS，不疊加限制；先移出，再重新評估設計（範圍：第二、三類「未選」，先研究） | 「Kernel其實就是個ECS，本身就不應該迭代限制在裡面的。」「先移除出來。然後得重新評估設計。」 |
| `v8-arch-gyo2-orchestration-layer-name`（11-16） | 編排層，`engine/orchestration` | 「算了用這個吧，編排也是挺應景的。」 |
| `v8-arch-gyo2-access-verification-stays`（11-16） | 存取驗證留 Kernel（開發版）；DI-KS 2 後半隨權威移出 | 「1）照建議」 |
| `v8-arch-gyo2-event-trim-by-cursor`（11-16） | Kernel 照游標修剪事件，不看時鐘 | 「它讓『不漏讀』成為 Kernel 自己的保證，不必依賴編排層排得對。」 |
| `v8-arch-gyo2-kernel-world-exec-type`（11-16） | Kernel 的型別 `WorldExecutor`；`Product` 搬到編排層 | 「第二個，保留一個型別、另取名。」「照建議」 |
| `v8-arch-gyo2-module-plugin-owner-definition`（11-16） | Module＝自己維護、介面是自己的；Plugin＝第三方官方維護；權威檢查放 Module | 「權威檢查我想放Module，這個是底層能力。」 |
| `v8-arch-gyo2-module-plugin-definition`（11-16，PR #240 未合併） | 定義寫進 AGENTS §6（只放思想）與 v2；兩份登記檔與兩種型別不變、照新定義重分；Module 在 Plugin 前 | 三件未確認（L15 等） |
| `v8-arch-gyo2-authority-own-registration`（11-16） | 權威 Module 自己登記並代為登記進 Kernel；Kernel 不存標籤 | 「4.1 權威 Module 自己登記。」 |
| `v8-arch-gyo2-authority-is-game-design`（11-16） | 權威是遊戲設計的一部分；策略式 Module，作者選策略 | 「這個東西本身就是遊戲邏輯的一部分…」「讀法一」 |
| `v8-arch-gyo2-authority-strategy-mechanism`（11-16） | 策略介面＋檢查；作者可自寫；Engine 附的只實作伺服器權威，其他佔位、選了啟動時報錯 | 「這個核心是要建立程機制而不是寫死。」「啟動時報」 |
| `v8-arch-gyo2-game-init-list`（11-16） | 遊戲初始化（行程一次、主 World、清單可插入）與生成世界（生成世界的宣告）分開 | 「第一個World，內容有作者自己訂，Engine只提供機制」「對的。」 |
| `v8-arch-gyo2-thread-binding-by-runner`（11-16） | runner 決定執行緒；綁執行緒的 Resource 自己斷言；Kernel 的執行緒啟動檢查拿掉 | 「第二個，別人掉過的坑，我沒必要在跳一次。」 |
| `v8-arch-gyo2-no-ambiguity-detection`（11-16） | Kernel 照登記順序平手、不報錯；Kernel 與權威策略都不偵測歧義；伺服器權威連線時比 C 指紋 | 「偵測歧義其實是很麻煩的。我個人覺得沒必要。」「可以。」 |
| `v8-arch-gyo2-pre-v2-target-docs-later`（11-16） | v2 之前的 target 文件先不動 | 「之前就先不管了。後面統一處理」 |
| `v8-arch-gyo2-boundary-split-integrator-first`（11-17） | 拆三份、統合先立 | 見本頁開頭 |
| `v8-arch-gyo2-clock-out-of-kernel`（11-17） | 時鐘與 tick 移出 Kernel，到時間的 Module；作者可選；「tick 只有一個來源」隨權威 | 「照建議，並修改相關文檔。我記得tick的控制之前也是pvp的需求，這個明顯是遊戲需求入侵了Engine設計。剝離出去給其他Module實現最好，和權威一樣，作者可以選，不過能多Module是有依賴關係的，這個依賴需要處理。」 |
| `v8-arch-gyo2-module-deps-in-registers`（11-17） | Module 之間的依賴只寫在登記檔，編排層讀出並檢查；程式碼不寫 `Requires`；權威 Module 整個依賴時間 Module | 「其他家的這個依賴關係如何處理？」→（卡片查四家，建議只寫在登記檔）「這個照你建議。」 |
| `v8-arch-gyo2-no-runtime-interception`（11-17） | 拿掉執行期的攔截（第 7.3 節六條）；伺服器權威改靠 digest 比對與伺服器覆蓋抓錯 | 「別家如何決定的？」→（卡片查四家：觀察點只在結構變更或整格替換、沒有一家讓外部掛鉤拒絕寫入、網路權威靠伺服器覆蓋；看法改成拿掉）「拿掉執行期的攔截，伺服器權威改靠 digest 比對與伺服器覆蓋來抓錯。」 |
| `v8-arch-gyo2-init-values-like-bevy`（11-17） | 生成世界的初始值在建立當下給、不算寫者；不設豁免的階段 | 「這個可以參照Bevy。」 |
| `v8-arch-gyo2-engine-not-rigid`（11-17，通用考量） | 做 Engine 不把規則釘死：預設方便、不禁止別的做法，限制只放在作者自選的地方；先懂別家為何這樣做；很多開源都這樣做就不頭鐵；拿不準交使用者 | 「我們做的是給別人用的東西，如何什麼東西都釘死…我們的限制會把別人的idea卡死。」「如果很多開源項目都這麼幹，就不要頭鐵了…你拿不準的可以交給我，這個是度的問題。」 |
| `v8-arch-gyo2-dynamic-insert-per-world`（11-17） | 執行中可以插入 System；每個 World 宣告准不准（預設准、不保證指紋一致）；不准時拒絕、回傳錯誤、記警告、不退出 | 「那我先問一個問題，動態插入系統有什麼好處？用例子回答。」→「我明白了，這個就類似於指令模式，現在啟動的world沒有系統，但是動態插入可以讓我使用指令，動態加入這個機能，這個應該才是最核心的。未來我自己要做指令集。也是要考慮的。」→「我再問一個問題，GYO能走2條路嗎？我不要求作者一定不能動態插入，而是在World裡聲明，如果World的宣告可以，那就可以動態插入系統，但是，寫明不保證指紋一致性。如果宣告不行，那麼就不能動態插入，插入就拒絕，返回信息（不退出，但是System無法插入，日誌報警告）。」→（卡片複述並列四點建議：預設允許、宣告在建 World 時、拒絕回 Result＋警告、允許時指紋不保證一致）「對的。」 |
| `v8-arch-gyo2-read-only-registry-view`（11-17） | Kernel 交出唯讀的登記表、帶版本號；參照只在當前版本有效；修改只走登記 | 「交出是什麼意思，公開出去可以修改還是說，外部能讀到？」→「和剛才的system插入有關聯嗎？」→（卡片列三處關聯與整體形狀）「可以」 |
| `v8-arch-gyo2-exclusive-writes-optional`（11-17） | `ExclusiveWorld` 能寫任何值（隨 L3）；`Writes<…>` 留作選填標註 | 「現在的GYO呢？」→「這題，你和看下上面的決策，例如System可以動態插入那樣的（如果開啟的話）。」→「我看不懂你在說啥？不是在說World是否能讀寫的問題嗎？我讓你看上面的決策想問是否有關聯。」→（卡片列關聯：這一題大半已由 L3 定，只剩 `Writes<…>` 留不留）「嗯，可以留選填。」 |
| `v8-arch-gyo2-kernel-settings-read-write`（11-17） | System 准讀准寫 `KernelSettings`，下一次 Run 生效；拿掉 D-E18 | 「你的調查，只有半分，3家也是只讀不寫嗎？」→（卡片重查：三家都准寫，差在生效時機）「這個問題是不是涉及上面決策的開關？」→（卡片答 L5 的開關不能放進可寫的設定）「但是L5的開關說的是System的插入。但是這題說的是系統的寫入值。是一樣的嗎？」→（卡片：不一樣，L5 的開關本來就在建 World 時的選項）「這個照建議。」 |
| `v8-arch-gyo2-rollback-module-strategy`（11-17） | Kernel 只給原語；回滾是獨立的 Module、有策略（interface＋官方實作），第一版只實作狀態觸發，P2P 佔位；另開第四份宣告 | 「Kernel 只給原語，回滾放權威 Module 的策略，我在想回滾要不要和權威放一起。你的看法呢」→「我在繼續問問題，這樣可以找到本質性的問題。問題：回滾有策略嗎？」→「不是，我是說本質上如果回滾有策略，就不能單單有官方默認實現，而是相當於一個interface、有官方實現，作者也可以自己複寫。否則作者無法達到自己的目的」→「這2件事，照建議。」 |
| `v8-arch-gyo2-strategy-interface-impl`（11-17） | 權威與回滾的策略都是 interface＋官方實作；作者可自寫或繼承改裝（選擇，不是做法） | 「權威 Module 也是一樣的構造（作者可以複寫，或者繼承改裝），如果是java就是一個interface + imple，但是C++我不知道有沒有類似的方式。」→「繼承官方實作改裝是選擇，而不是做法，你可以繼承改裝，也可以廢棄掉官方做法，而用自己的實現接上。」 |
| `v8-arch-gyo2-two-layers-like-bevy`（11-17） | 編排層認得遊戲初始化與生成世界的階段、Kernel 不認得（照 Bevy 的 `bevy_app`／`bevy_ecs`） | 「用Bevy的方式走，」 |
| `v8-arch-gyo2-layer-duty-lists`（11-17） | 兩條核心＋每層的職責清單；agent 自己判、只在清單邊界上問；判斷要對照同類型多家實作的設計原因 | 「我先想法是固化一個核心，「Engine只提供機制」用這條來判，「Kernel的職責是ECS」那麼和ECS無關的東西就別往裡面加，每層做好自己的職責範圍。最後合作組成Engine。都報告其實不是不行，但是我擔心我會被處理大量判斷之後我的判斷也會出現偏差（已經有好幾次了）」→（卡片複述並提四點）「對，並且也要加入判斷，對照別家實現（不光只是一家，而是同類型的。例如GYO現在是ECS，那麼就要去參考ECS的引擎，盡量找出他們的設計原因，做交叉。我希望獲得的是別人的能力，而不是技術債）」 |
| `v8-arch-gyo2-products-pick-modules`（11-17） | 產品（作者）可以只選部分 Module | 「產品能不能只選部分 Module，能，要不然單機遊戲也必須上權威了。」 |
| `v8-arch-gyo2-authority-no-writer-count`（11-17，side chat） | 伺服器權威策略不數寫者（比過：數、預設單一寫者、`Shared` 才准多寫（11-11 原方向）／數、預設多寫者／不數（replicon、lightyear、Unity Netcode 與四家 ECS）← 選／帶檢查但預設關（卡片不建議）） | 「那就參照別人的權威策略」→「主要我怕自己建立一個不會用到的機制。首先要做System排序和標記，光是這2個就是逼得作者需要100%考慮時序狀態，正常如果一個遊戲物件很多，更本用不到。例如：一個遊戲裡面，有爆炸，有機能，有射擊傷害，還有陷阱。全部都考慮進去，這個不現實。」→「倒不是說推翻自己，而是這個是基礎規則，我怕的是改完之後又牽一髮而動全身」→「對，你說的沒錯，不能建立一個有漏洞的機制，要不然不是用不了還要投入時間維護，要不然又會有一堆的東西疊在這個上面。」（全文六句由 side chat 照四樣轉達，見證據 `inventory.md` 第 5 節） |
| `v8-arch-gyo2-system-declares-immediate-structural-change`（11-17，side chat；代號待主會話定） | 不替回滾開 DI-KS 18 的例外；建機制：System 在登記時宣告「可以當場做成員變更」（例：回滾）或「不能」（普通 System），兩種有不同的判定與限制，選擇權與風險交給作者；回滾的重跑由宣告了可以改成員的特別 System 做（巢狀跑固定步）。比過：在獨占 System 裡巢狀跑（開例外）／World 閒置時由 runner 呼叫／兩條都給／使用者提出的這一種 | 「其實就是一個特別的System來做回滾操作對嗎？」→「這樣，不用開例外，建機制，System在初始化是給一個聲明，可以做成員變更（回滾）和不能做成員變更（普通System），並建立不同的判定和限制，這個選擇權和風險交給作者。你認為如何？」→（side chat 回覆可行並列別家：Bevy `&mut World` 的獨占 System 當場改、普通 System 用 Commands 延後；flecs `immediate`；Unity `EntityManager` 當場、`EntityCommandBuffer` 延後）「轉過去。」 |
| `v8-arch-gyo2-immediate-change-via-exclusive-world`（11-17；代號待主會話定） | 「可以當場改成員」的宣告沿用 `ExclusiveWorld`：簽名裡有它就可以當場改；DI-KS 18 改寫成「普通 System 只經同步點；`ExclusiveWorld` 可以當場改」。比過：沿用 `ExclusiveWorld`（Bevy `&mut World`、flecs `immediate`、Unity `EntityManager` 三家都把「拿到整個 World」當成「可以當場改」）／另外加一個宣告（例 `ExclusiveWorld<Immediate>`；四家都沒有） | 「沿用 ExclusiveWorld」 |
| `v8-arch-gyo2-events-two-kinds-like-bevy`（11-17，side chat；代號待主會話定） | 事件照 Bevy 分兩種：(1) 現在的保留式（`EventReader`／`EventWriter`、游標、保留）改名，對應 Bevy 的 `Message`；(2) 增加推送式，對應 Bevy 的 `Event`＋`Observer`；位置參照 Bevy（兩種都在 `bevy_ecs`，所以在 Kernel）。比過：只留保留式／只有推送式（flecs）／兩種都給（Bevy）← 選 | 「flecs是我喜歡的樣子，但是我不知道，一改又會扯出什麼東西。」→「這2種東西分別是要處理什麼？」→「這2個東西設計哲學都不同。」→「Bevy2種都有嗎？」→「OK，照Bevy的來，1）改名。2）增加一個 推送式。位置參照Bevy來放。」 |

### 2.2 被這一批改寫的較早決定

「回應」欄只寫使用者的話；卡片或側邊會話的說明另外標出。

| 較早的決定（日期） | 當時的內容與理由 | 較新的決定 | 較新的有沒有回應較舊的理由 | 本批的處置 |
|---|---|---|---|---|
| `v8-arch-kernel-exclusive-default`（11-11）、`v8-arch-gyo2-check-history-first` 第 3 點（11-14）、`v8-arch-gyo2-exclusive-default-already-decided`（11-15：「預設是單一寫者」，訂正紀錄） | 預設獨占、Shared 才可多寫；理由欄「權威判定，要可回滾、可比對」 | `kernel-is-ecs-no-restrictions`、`authority-is-game-design`（11-16） | 有：卡片 11-16 查了歷史（理念出自 PvP 的 Gateway、案例全是 PvP），使用者看過後說「權威判定落的太深了…本身就是遊戲邏輯的一部分」 | 寫者規則移出 Kernel，內容由權威份重新評估（L12）；**2026-11-17 使用者定伺服器權威也不數寫者**（`authority-no-writer-count`，原話「倒不是說推翻自己，而是這個是基礎規則，我怕的是改完之後又牽一髮而動全身」，回應的是 11-11 的方向本身） |
| `v8-arch-gyo2-zone-default-b`（11-14：`Zone`、`Role`、`Rollback`、`DigestSet`、`CFingerprint`、zone C 匯出全部留；`Zone` 不填視為 B；`RequireExplicitZones`；「B，先把骨架跑通」） | 讓單機與工具作者不用多寫 | `kernel-is-ecs-no-restrictions`（11-16） | 部分：11-14 的理由（單機不多寫）在移出後仍成立；「全部留在 Kernel」被「先移出」取代 | 移出 Kernel，內容由權威份重新評估 |
| `check-history-first` 第 2 點（11-14：`Domain` 四個值留著） | 「Loading 不是 PVP 特有的，而是遊戲都有。」 | `build-world-phase-is-mechanism`（11-16：「有沒有是作者決定的，而不是Engine」） | **只有 Loading**：使用者更正了 Loading 的理由；整個時鐘在 11-17 另定（`clock-out-of-kernel`） | `Domain` 整個移出 Kernel（L2） |
| `check-history-first` 第 5、6 點（11-14：`DigestSet` 不急；`Zone`／`Role`／`Rollback`／`CFingerprint` 照不照原核准的切） | 「digest 用不用先不急定。」 | `kernel-is-ecs-no-restrictions`（11-16） | 部分：使用者定「先移出」，沒有回答 digest 的去留 | 移出 Kernel；去留由權威份 |
| K.3 第 11 項（11-11：(c) 區的時間只來自 domain tick 與被記錄的 boundary 輸入，例 PvP 的 `HostClockSample`、輸入的到達時刻；`spec.zh-Hant.md:1219`、`:789`）、DI-KS 8、12「domain tick 只有一個來源、任何 System 不得寫、在快照內」 | tick 是 (c) 區（PvP 的權威與回滾）唯一的時間來源 | `clock-out-of-kernel`（11-17） | 有：「我記得tick的控制之前也是pvp的需求，這個明顯是遊戲需求入侵了Engine設計」 | tick 移到時間 Module；「(c) 區的時間只來自 tick」「tick 只有一個來源」歸伺服器權威策略（L12）；DI-KS 8、12 的這部分移出 |
| `v8-arch-gyo2-fixed-step-in-world`、`v8-arch-gyo2-time-plan-two`（11-14：固定步由 World 裡的獨占 System 驅動；`Product` 多個 World） | 「固定步這個我不想限制作者的遊戲邏輯」 | `clock-out-of-kernel`（11-17） | 不衝突：驅動固定步的 System 本來就在時間 Module；改的只是 tick 存在哪裡（Kernel 的 domain tick → 時間 Module 的 Resource） | 照舊；時間設計 T1／T3 照 L2 改寫 tick 的存放 |
| `v8-arch-gyo2-strict-order`（11-14，PR #228 已合併） | `RequireExplicitOrder`；使用者「合併」，五項沒有逐條核准 | `no-ambiguity-detection`（11-16） | 有：「偵測歧義其實是很麻煩的…普通程序無法判斷」 | 拿掉；`StrictOrderTests.cpp` 8 案與 11 條突變退役 |
| DI-KS 5「(c) 歧義是啟動錯誤」「(c) 區對登記順序不敏感」（11-13 核准） | 決定性區不得依登記順序決定結果 | `no-ambiguity-detection`（11-16） | 部分：使用者的原話是「偵測歧義其實是很麻煩的…」與對側邊會話建議的「可以。」；「登記順序相同時平手是決定性的、比指紋抓版本不同」是側邊會話的說明，不是使用者的話 | 平手是 Kernel 的決定性行為；「對登記順序不敏感」不再保證（第 7 節列為放寬的面） |
| K0：`startup-run-by-kernel`、`startup-before-any-schedule`（11-15，#239）；DI-KS 22 | Kernel 排第一、只跑一次、執行中不准跑別的；「系統必須的，就不讓作者碰」 | `game-init-list`（11-16） | 有：「Engine只提供機制」 | 遊戲初始化清單在編排層（可插入）；生成世界交 生成世界的宣告；寫者豁免隨寫者數量拿掉（修訂 29） |
| `v8-arch-gyo2-exclusive-declared-writes`、`shared-means-no-owner`（11-15）；DI-KS 23 | `ExclusiveWorld<Writes<R…>>` 列出單一寫者 Resource、列的算寫者；「要保證誰寫，別開shared」 | `kernel-is-ecs-no-restrictions`（11-16） | 沒有單獨回應 | `Writes<…>` 留作選填標註，「列的算寫者」由權威策略決定、Kernel 不再擋沒列的寫入（L7） |
| `v8-arch-gyo2-spawn-reject-zone-c-blocked-by-zone-b`（11-15） | (c) 區的生成被 (b) 擋住時照樣丟、明細帶記錄者的區（公開欄位 `RejectedSpawn::emitterZone`，`World.hpp:86`）；「之前的權威Match的設計就是為了避免這種情況的」 | `kernel-is-ecs-no-restrictions`（11-16） | 沒有單獨回應 | 丟掉那一筆的行為留 Kernel（DI-KS 9）；「明細帶區」隨區移出，權威份定替代 |
| `v8-arch-gyo2-zone-c-exclusive-apply`（11-15～16） | Kernel 加「(c) 區的獨占套用」 | `kernel-is-ecs-no-restrictions`（11-16） | 沒有直接回應 | 形狀由權威份重看（L12） |
| 狀態機宣告（11-16 核准，`state-machine-declaration-approved`） | Kernel 不改；初始進入在開局之後；(c) 區介面 `RegisterStateMachine<S, Zone::C>` | `game-init-list`（11-16） | 部分：「初始進入在開局之後」可維持或改成插進清單，由狀態機卡另定 | L18 |
| `v8-arch-gyo2-product-type-name`（11-14：Kernel 的型別叫 `Product`） | 「這個不叫 App 了，太模糊 直接用 Product」 | `kernel-world-exec-type`（11-16） | 有：使用者選「保留一個型別、另取名」，`Product` 的名字搬上去 | Kernel 的型別改名 `WorldExecutor` |
| `v8-arch-gyo2-cross-clock-events-open`（11-14） | 跨時鐘事件放開；「不光要考慮單機，也要兼容網絡部分」 | `event-trim-by-cursor`（11-16） | 部分：游標保證不漏讀回應了「放開」；「兼容網絡」沒有回應 | 游標份（L13） |
| DI-KS 3、規格 D-1、D-6（Plan 在 Finish 建一次、永不重建）、R-2（凍結後不能登記）（11-11～13 核准） | 決定性；網路的 wire id 依登記順序，「finish 後登錄不可再變（wire id 的前提）」（`docs/architecture/plans/kernel/networking.zh-Hant.md` 第 5 列） | `dynamic-insert-per-world`（11-17） | 有：使用者點名未來的指令集（「動態插入可以讓我使用指令，動態加入這個機能」），並用「每個 World 宣告」保住 PvP 的需要 | 改：Plan 在兩次執行之間可重建；凍結不再是永久；不允許的 World 照舊不變 |
| `v8-arch-gyo2-module-plugin-registers`（11-14） | 型別 `GYO::Kernel::Module`／`Plugin`、兩份登記檔、Module 在 Plugin 前 | `module-plugin-owner-definition`（11-16）、PR #240 | 有：判準換了，兩份登記檔與型別保留 | 型別搬到編排層（命名空間改） |
| runtime-composition DI 2（11-13：一個 Runtime 一個 runner）、DI 3／3a（11-11、11-13：世界內容只經 Loading；使用者 11-11 的理由「如果現在物件稀少可以算成是Startup，那麼未來東西越來越多呢」）、DI 4（一 World 一執行緒） | — | `game-init-list`、`build-world-phase-is-mechanism`、`thread-binding-by-runner`（11-16） | DI 2：照舊，改成一個 `Product` 一個 runner；DI 3／3a：11-11 的理由由 生成世界的宣告的「可分批」回應；DI 4：「別人掉過的坑」 | 文件不改（統一處理）；DI 3 交 生成世界的宣告；DI 4 改成存取時斷言 |

---

## 3. 分層與歸屬

```
產品（Game／Tool）          main：用哪些 Module／Plugin（產品登記檔）、跑起來
   │
   ├─ 權威 Module、時間、狀態機、audio、網路……（Module）   SDL、ImGui（Plugin）
   │        │ 以登記單位（編排層的 Module／Plugin 型別）接進來
   ▼        ▼
編排層 engine/orchestration   登記單位與生命週期、Product（World 群＋runner）、遊戲初始化清單、
   │                          每幀清單、交換點、extract 的時機、執行緒的安排、啟動錯誤的彙整
   ▼
Kernel engine/kernel          World、ECS 元件／Resource／事件、System 參數與存取推導、schedule、Plan、
                              WorldExecutor、同步點、hook、巢狀跑、存取驗證、登記資訊（FrozenRegistry）
```

依賴方向：箭頭朝下。各 Module 依賴編排層的登記單位型別與 Kernel（和 `bevy_time`、`bevy_state` 依賴 `bevy_app` 同形）；編排層只依賴 Kernel、不依賴任何第三方與任何 Module 的內部；Kernel 不依賴上面任何一層。歸屬照 Module／Plugin 定義（PR #240）。

**系譜**（AGENTS §4）：分層＝`bevy_ecs`／`bevy_app`（`bevy_app/Cargo.toml` 只依賴 `bevy_ecs` 等底層 crate）；權威放 Module＝使用者 11-16（Unity Netcode 是套件、lightyear／replicon 是第三方 crate）；`WorldExecutor` 保留為型別（退掉：併進 `World`，照 Bevy，要改 B-12、DI-KS 12 與「World＝資料」）。

### 3.1 Kernel 留下的

| 留下 | 現在的位置 | 改什麼 |
|---|---|---|
| World、ECS 元件、Resource、事件緩衝 | `World.hpp`、`detail/Storage.hpp` | 事件改游標（L13） |
| System 參數與「存取從簽名推出」 | `Params.hpp`、`SystemTraits.hpp`、`Access.hpp` | `CurrentTick`、`AccessKind::DomainTick` 拿掉（L2）；`ExclusiveWorld` 改成整個 World 的值的讀寫、`Writes<…>` 選填（L7） |
| 登記動作 | `Registrar` | 拿掉 zone、寫入政策、區與回滾的旗標、`DigestSet`、`RequireExplicitZones`、`RequireExplicitOrder`；登記單位的身分由編排層給（3.5） |
| schedule、Plan、Serial 執行、同步點、hook 清空輪 | `Plan.hpp`、`Scheduler.cpp`、`Executor.cpp` | Plan 的 `domain` 拿掉（L2）；`EventTrim` 步隨游標拿掉 |
| `WorldExecutor`（現在的 `Product` 改名） | `Product.hpp` | 拿掉開局的排程（K0） |
| 巢狀跑 schedule | `ExclusiveWorld::RunSchedule`（DI-KS 16～21） | 刪去開局、區、clock 相關的分支 |
| 存取驗證（開發版） | W-5、A.6 | `ExclusiveWorld` 的已宣告集合以「zone B 且 Shared」定義（`World.cpp:154-157`、`:983-987`），隨 L7 改寫 |
| `FrozenRegistry`、Registry 指紋 | `Registry.hpp:504-555`、`Fingerprint.cpp` | 讀取面（3.3）；指紋改版（3.4） |
| `KernelSettings` | `Registry.hpp:389-393` | 拿掉 D-E18；准讀准寫，下一次最外層 Run 生效（L9） |
| `InitialValue`、`SpawnWithId`／配發器、丟掉不能生成的那一筆 | — | 區規則的部分移出；計數待 L8 |
| Extract 參數（唯讀）、`ScheduleOptions::extract`、`ExtractNotNestable`、finish 時用來源 Registry 解 Extract 的 id（`Registry.cpp:343-355`）、D-E14（來源未設或未凍結，`:1060`） | — | 來源 World 與執行時機移編排層；`SetExtractSource` 留作 finish 的輸入 |
| 綁執行緒的 Resource 記來源執行緒、碰錯斷言（**新**，取代 H-2、X-3 的啟動檢查） | `ResourceOptions::affinity` 改義 | 系譜 Bevy `bevy_ecs/src/storage/non_send.rs:74-84` |

**Kernel 保留的啟動錯誤**：R-E2；R-E4（不含 digest set `Registry.cpp:487`）；R-E5；D-E1（不含 Z3 的 ingress stage `:909`；eventTrim `:924` 隨游標）；D-E3（沒有配發器，`:1047`、`:1118`）；D-E4、D-E5、D-E6；D-E7 的機制規則——H-6 Extract 只在 extract schedule（`:1127`、`:1376`）、S-8 的「Diagnostics 只能來自 `DiagnosticsWriter`」（`:1079`）、S-18 hook 不得用 `ExclusiveWorld<Writes<…>>`（`:1133`；修訂 32：改成「hook 的 `ExclusiveWorld<>` 不能當場改成員」，DI-KS 18）；D-E14（`:1060`）；D-E17；D-E19；D-E20。D-E18 拿掉（L9）。D-E12、F-E1 拿掉（L2）。
**Kernel 保留的執行期錯誤**（`KernelErrorCode`）：驗證、巢狀、hook、配發器、結構、Extract 相關的保留；`UntypedWrite`、`ZoneCDespawn`、`ExclusiveWorldWrite` 拿掉（L3）；`UndeclaredResourceWrite` 拿掉（L7）；`StartupNotNestable`、`StartupRunning` 拿掉。

### 3.2 移出 Kernel 的（去處）

| 移出 | 現在 | 去處 |
|---|---|---|
| 登記單位 `Plugin`／`Requires`／`Finish`／`InstallRunner`；R-E1、R-E3、R-E6 | `Registry.hpp:41-75`、`Registry.cpp:646-724`、`:1520` | 編排層（L5）；`Requires()` 拿掉，依賴改從登記檔讀（L20） |
| `Product` 這個名字 | `Product.hpp` | 編排層 |
| 開局的排程（K0）、`ScheduleOptions::startup`（`Declarations.hpp:138`）、D-E21、`StartupNotNestable`、`StartupRunning` | `Executor.cpp:151-170`、`:457-468`；DI-KS 22 | 遊戲初始化清單（編排層）；生成世界交 生成世界的宣告 |
| 開局的寫者豁免（`Registry.cpp:1448-1457`） | DI-KS 6 | **拿掉**（沒有寫者數量，就沒有豁免；修訂 29） |
| `Domain`（含 `Loading`）、`ScheduleOptions::clock`、Kernel 推 tick、`CurrentTick`、`AccessKind::DomainTick`、tickStamps、D-E12、F-E1；`EventOptions::domain` | `Declarations.hpp:56-62`、`Executor.cpp:192-196`、`World.hpp:453-461`、`Params.hpp:645` | 時間 Module（L2）；事件的歸屬改游標（L13） |
| `ScheduleOptions::runner`、`Affinity` 的啟動比對、`kMainRole`；H-2／D-E13、X-3／D-E10、X-E2、X-E3 | DI-KS 14 | runner 安排；Resource 自己斷言（3.1） |
| `Zone`、`external`、`ingressStage`、`DigestSet`、`RequireExplicitZones`、`parallelGuardTest`；D-E2（寫者數量 S-11、digest 禁 Shared S-12）；D-E7 的區規則部分（含 S-6、S-7 `Local` `:1319`、S-8 的「(c) 不得宣告 Io effect」`:1313`、S-9、S-17 `:1335`）；Z1、Z2、Z4、Z7；D-E9（`:1510`）；D-E11 與 X-E1（`:1423`）；Z3；記錄的 `emitterZone`（`detail/Context.hpp:90`）、`RejectedSpawn::emitterZone`（`World.hpp:86`） | `Declarations.hpp`、`Registry.*`、`World.*`、`Executor.cpp` | **拿掉**（修訂 29：`authority-no-writer-count`、讀寫規則兩半拿掉（15.3）、確認清單 `confirm-derived-removals`）；執行期的攔截拿掉（L3）：Z5 的呼叫當下與同步點複核、W-6b 都不再存在 |
| `ingress`（輸入入口的身分）、`Role::Input`（改成權威的 `.input` 標記；輸入歷史由回滾的 `rollback.Input<T>` 持有，第 16 節）；D-E15、Z6、Z8 與 B3（「判定資料 × 回滾」的交叉檢查，L24）；digest 的 R-E4；`CFingerprint`、`ExportZoneCSources`；「tick 只有一個來源」 | `Declarations.hpp`、`Registry.*`、`Fingerprint.cpp` | 權威 Module（內容 L12、15.3）；輸入入口的身分也給回滾的「重跑時不跑」用 |
| `Rollback`（Yes／No／Derived）、`rollbackEnabled`、`notInRollback`、`rollbackSafe`；D-E16；Rewind／Replay（只有註解、沒有實作：`Registry.hpp:136`、`detail/Storage.hpp:231`） | `Declarations.hpp`、`Registry.*`、`Scheduler.cpp:500-525` | 回滾 Module（L11、L24；修訂 20 從權威那一列拆出） |
| `RequireExplicitOrder`、D-E8 | `Registry.hpp:255`、`Scheduler.cpp` | **拿掉**；`TieBreaks` 留 |
| `WritePolicy`（`Shared`／單一寫者） | `Declarations.hpp` | **拿掉**（`authority-no-writer-count`；確認清單第 3 件，使用者核准） |
| `ScheduleOptions::eventTrim`、D-E1 的 eventTrim 檢查（`Registry.cpp:924`）、Plan 的 `StepKind::EventTrim` | `Declarations.hpp`、`Plan.hpp:36`、`Scheduler.cpp:218-226`、`Executor.cpp:417-420` | 拿掉；修剪改游標（L13，游標份 4.7 定時機） |
| `effects`／`Registrar::Effect` | `SystemOptions::effects` | 留 Kernel（同 effect 的兩個 System 是衝突邊） |

### 3.3 Kernel 交出的唯讀登記資訊（L6；已定）

**已定**：唯讀（外部讀得到、不能從這裡改；修改只走 `Registrar`，L5 的「每個 World 宣告准不准插入」是唯一的關卡）；帶版本號，每次重建 Plan 加一，在下一次執行前更新；交出去的參照只在當前版本有效、版本一變就作廢（或交複本）。系譜：Bevy `Schedule::graph()` 回傳 `&ScheduleGraph`（`schedule.rs:677`）、`World::components()`（`world/mod.rs:259`）、`ComponentInfo::hooks()`（`component/info.rs:135`）；Unity `World.Systems` 是 `NoAllocReadOnlyCollection`（`World.cs:142`）；flecs 的 System 是實體、讀改同一份（不採：改動會繞過 L5 的關卡）；版本號的系譜 flecs `rebuild_count`（`pipeline.c:277`）、Bevy `graph.changed`（`schedule.rs:642`）。證據 `read-surface-other-engines.md`。


現在 `FrozenRegistry` 已有：元件、Resource、事件、System（含掛點、存取、`source`、`ownerPlugin`，`Registry.hpp:473-483`）、Plan（含 `DeclaredEdge`）、`TieBreaks`、具名集合、Registry 指紋。缺：hooks（觸發、元件、存取、登記者）、`InitialValue`（Target、Request、宣告者）、schedule 的選項與 stage、Plan 之外的順序圖（D-E16 的 Derived 檢查用到，`Scheduler.cpp:503-521`）、`SystemInfo` 的 effects（`Registry.hpp:473-483` 沒有；（修訂 32：Io 檢查已隨讀寫規則拿掉，effects 留作 Kernel 的衝突邊））、`ExclusiveWorld` 的 `Writes<…>` 標註（L7 之後不進存取集合；修訂 29：不再數寫者，策略只用它看得到獨占 System 寫了什麼）。要交出的只是唯讀資訊，Kernel 不解讀它們的意義。C 指紋（`Fingerprint.cpp:331-406`）讀的正是這些。

### 3.4 指紋（L10）

Registry 指紋現在雜湊登記單位名單（`Fingerprint.cpp:11`）、zone、writePolicy、role、rollback、external、affinity、domain、startup、clock、runner、eventTrim、ingress、effects（`:12-18`、`:149-198`）；Plan 指紋含 domain 與 EventTrim 步（`:203`）。移出之後**格式改版、所有 golden 重產**。C 指紋移權威 Module，用 3.3 的讀取面計算。登記單位名單改由編排層給（3.5）。

### 3.5 登記單位的身分

Kernel 不再認得 `Plugin`，但 `InitialValue` 的「只有擁有者能宣告」（`Registry.cpp:850`）、錯誤訊息的 `OwnerName()`、`ColumnInfo::ownerPlugin` 等需要「是誰登記的」。做法：編排層開 `Registrar` 時給一個 owner（名字與序號）；Kernel 只存、只比相等、只拿來印，不解讀。欄位名 `ownerPlugin` 改成中性的 `owner`（N5）。

---

## 4. Architecture Delta（AGENTS §3 的七項）

1. **需要它的 Feature 或目的**：使用者的 Review 目的（「Kernel是機制，那組裝放在哪裡？」「Kernel其實就是個ECS」）；第 5.4 節的 Feature 表。
2. **現在的問題**：Kernel 混了 `bevy_ecs` 與 `bevy_app` 兩半（`inventory.md` 第 1、2 節）；並疊加了四家都沒有放在核心的規則，單機也要承擔。
3. **變化的邊界**：Kernel／編排層（新）／權威 Module（新）／回滾 Module（新）／時間 Module；Kernel 的公開介面（`Plugin`、`Product`、`Registry`（L5）、`ScheduleOptions`、`ComponentOptions`、`ResourceOptions`、`EventOptions`、`SystemOptions`、`Mount`、`Registrar`、`FrozenRegistry`（L6）、`KernelErrorCode`、`StartupCode`、`AccessKind`、`Plan`、`RejectedSpawn`、`ColumnInfo` 等的 `ownerPlugin`）；Registry 與 Plan 指紋的格式。
4. **影響的 Module／Product／Tool**：`engine/kernel`；新 `engine/orchestration`；新權威 Module、新回滾 Module；`engine/time` 與時間 Module（L2）；狀態機 Module（已核准未實作，L18）；`tests/common/kernel` 的 15 個測試檔 329 個案例與守衛腳本（`kernel_guard.py`、`naming_guard.py`、`kernel_symbols.py`、`store_stamp_guard.py`、`zone_c_export_check.py`、`zone_c_fixture/`、`compile_fail/`）；突變清單（558 個，碰到移出概念的約 176～201 個）。gyo2 上沒有產品與工具。
5. **Dependency Direction**：新增 編排層→Kernel、各 Module→編排層（登記單位型別）、權威→Kernel（讀取面）、回滾→Kernel（原語、讀取面）、權威→回滾（公開登記，L11、L24）；產品→編排層。Kernel 不依賴任何新層。
6. **Ownership**：登記單位的生命週期、runner、遊戲初始化、多個 World、執行緒安排、啟動錯誤的彙整：Kernel → 編排層。寫者數量、區規則、讀寫規則：Kernel → 拿掉（不由機制守，靠 digest 與伺服器覆蓋）。digest、C 指紋、「判定資料 × 回滾」的交叉規則、「tick 只有一個來源」：Kernel → 權威。哪些資料與 schedule 可回滾、Derived：Kernel → 回滾 Module。時鐘與 tick：Kernel → 時間 Module。事件的保留：時鐘 → 游標（仍在 Kernel）。owner 都是架構（架構師會話）。
7. **為什麼不能更小**：比過「留在 Kernel 但預設關」（設計稿第 4 節），使用者選移出；「只改名與文件」不解決單機承擔規則的問題。搬動已合併程式（K0 的開局排程與 J-58；`Plugin`／清單檢查；`StrictOrderTests`）是代價，列在第 8 節。

---

## 5. 基盤 Architecture 的根據（AGENTS §2）

### 5.1 為什麼算基盤

上面已積或排著要積的：狀態機 (b) 區（已核准）、時間 T1～T5、生成世界的宣告、還原入口卡（讀檔）、(c) 區的獨占套用、權威 Module。Kernel 的公開介面與指紋一改，它們都要跟著改。

### 5.2 予見的根據 → 基盤的要求

| 根據（出處） | 要求 |
|---|---|
| 單機不增加複雜度（11-14 `v8-arch-single-player-no-complexity`） | 不選權威時沒有寫者、區、digest 的規則 |
| PvP 的伺服器權威（11-11 起；11-16 只實作這一個策略） | 權威讀得到判定所需的登記資訊（L6）；檢查的時機在 Plan 建好之後（L5）；連線時比 C 指紋 |
| Client 三個 World（11-11）、`Product` 多個 World（11-14） | 多 World 在編排層；Kernel 只提供 `WorldExecutor` |
| 讀取畫面與分批載入（11-11） | 生成世界由編排層推動（生成世界的宣告）；初始值在建立時給（L4） |
| 作者自寫策略、格鬥遊戲的 P2P（11-16，只佔位） | Kernel 不為某個策略增加知識 |
| 狀態機的「進入初始狀態」要讀到設定（11-15 的 Bevy 例子） | 遊戲初始化清單可插入 |
| Bevy 0.10 拿掉整個 World 綁執行緒（soundness #3310） | 執行緒的保證放在需要它的 Resource |
| 未來的指令集（使用者 11-17 點名：「未來我自己要做指令集」） | World 在 Ready 時可再登記 System、重建 Plan；每個 World 宣告准不准 |

### 5.3 預算

本批不碰執行效率。事件游標的記憶體上限是預算項目，在游標份寫。

### 5.4 Feature 與 Product

| Feature | Product | 誰點名 |
|---|---|---|
| 不選權威時沒有限制 | 單機 | 使用者 11-14、11-16 |
| 伺服器權威 | PvP Match／Client | 使用者 11-11、11-16 |
| 多個 World | PvP Client、Editor（文件 World＋渲染 World，11-11） | 使用者 11-11、11-14 |
| 遊戲初始化可插入 | 所有遊戲（狀態機的例子） | 使用者 11-16 |
| 讀取畫面與分批載入 | PvP Client；原話沒有指定其他 Product | 使用者 11-11 |
| 作者自寫策略、P2P 回滾 | 格鬥遊戲（使用者舉例，沒有點名為要做的 Product） | 使用者 11-16 |
| 指令集（執行中用指令加上或拿掉機能） | 原話沒有指定 Product | 使用者 11-17 |

---

## 6. 先用遊戲講

**單機 2D 橫版。** 產品的 `main` 只寫「用這些 Module、跑起來」。編排層照 Module 登記檔把各 Module build，Kernel 凍結、建 Plan，各 Module finish；建出 `Product`（一個 World），在主 World 跑遊戲初始化清單一次（讀按鍵設定；狀態機可以把「進入主選單」插在它後面），然後 runner 每幀跑每幀的清單。「受傷」與「撿補血包」兩個 System 都直接寫主角的血量：Kernel 不攔，照宣告的先後，沒宣告就照登記順序。

**PvP 的 Match。** 產品另選權威 Module、策略選「伺服器權威」。作者寫 `authority.Component<Health>({...})`，權威 Module 記下自己的宣告，再替作者登記進 Kernel。Kernel 凍結、建好 Plan 之後，策略讀 Kernel 交出的登記資訊加上自己的登記做檢查，不過就啟動失敗。檢查哪些由權威份定；執行中怎麼攔由本頁 L3 定。

**PvP 的 Client。** 三個 World 在編排層的 `Product` 裡；runner 決定執行緒、交換點與 extract 的時機。Kernel 只提供 `WorldExecutor`、在它上面跑一張 schedule、讀另一個 World 的唯讀參數。

**格鬥遊戲（P2P 回滾）。** 作者選「P2P 回滾」——第一版只佔位，啟動時報「這個策略還沒實作」。

---

## 7. 守衛與測試的處置

### 7.1 原則

- **留**：判定不變。**搬**：判定一字不改地搬到新的 owner（沒有放寬面；有放寬面的一律標「改」）。**改**：判定改變。**拿掉**：這個性質不再由機制守。**移出**：性質交給權威份重新評估，判定不保證照搬（等同「改」，在權威份逐條核准）。
- 每一列寫：守的性質、處置、放寬的面、收緊的面、之後由誰守。根據與證據：守衛的說明在 `docs/architecture/plans/kernel/spec.zh-Hant.md` 第 1070～1135 行；逐案清單 `build/evidence/gyo2-kernel-app-boundary/test-case-inventory.tsv`（329 案；標籤 zone 165、writer 54、startup 20、clock 48、thread 3、ambiguity 18、rollback 41、lifecycle 10、extract 5、fingerprint 57；都沒碰的 83）。關鍵字掃描會多算（準備資料裡的 `Zone::B`），也可能少算。**每一案的判定變不變，在各實作批逐案判斷、逐條請使用者核准。**
- 突變：558 個，碰到移出概念的約 176～201 個（依關鍵字式不同）。跟著程式搬的突變一起搬；被拿掉的性質的突變退役並寫理由；各批照 D184 只跑相關突變、PR 線合併前完整跑一次。

### 7.2 依守衛

| 守衛（gyo2 現有） | 守的性質 | 處置 | 放寬的面 | 收緊的面 | 之後由誰守 |
|---|---|---|---|---|---|
| J-1～J-5、J-7～J-11、J-13、J-26、J-29、J-31、J-33、J-35、J-53 | 儲存順序、句柄、簽名、編譯失敗、條件、同步點、例外、驗證 | 留（逐案確認準備資料裡的 `Zone::` 等只是移除參數） | 無 | 無 | Kernel |
| J-12 | 簽名推出的存取集合 | 改：`CurrentTick` 拿掉（L2）、`ExclusiveWorld` 改成整個 World 的值的讀寫（L7） | 待定 | 待定 | Kernel |
| J-6 | Running 期間外部讀寫斷言；Ready 時外部寫 (c) 欄斷言（W-6b） | 前半留；後半拿掉（L3） | 第一次跑之後從外面寫 (c) 欄不再斷言 | 無 | Kernel；後半不由機制守（權威靠 digest 與伺服器覆蓋） |
| J-14、J-16、J-17、J-38～J-41、J-47 的 Z1 案 | 區規則、digest 禁 Shared、(c) 來源掃描 | **拿掉**（讀寫規則兩半拿掉、不數寫者；修訂 29）；只有 Z6、Z8、D-E15 的案移權威（另一列） | 選了權威也通過 | 無 | 不由機制守（digest 與伺服器覆蓋） |
| J-15 | 第二個寫者失敗；Startup 不計；Shared 兩寫者通過 | **拿掉**（`authority-no-writer-count`；修訂 29） | 選了權威也通過 | 無 | 不由機制守 |
| J-18、J-20 | id 依登記順序；Plan 決定性；指紋跨平台 golden | 改：golden 重產（L10） | 舊 golden 不再比對 | 無 | Kernel |
| J-19 | 登記單位的重複、requires、凍結後登記、build／finish 順序、型別兩次、R-E4 | 拆：登記單位那半搬編排層（順序照 L5：build → 建 Plan → finish）；「凍結後登記」改成：World 宣告允許時可以、不允許時回傳錯誤並記警告；R-E2／R-E4 留 | 允許的 World 執行中可以登記 | 不允許的 World 從斷言改成回傳錯誤（不退出） | 編排層／Kernel |
| J-21、J-23 | 衝突邊有先後；(b) 平手依 registrationSeq | 改：平手範圍擴大到所有衝突對 | 無 | (c) 也照登記順序 | Kernel |
| J-22 | (c) 歧義在 finish 失敗；`SchedulerTests.cpp:250`、`:265` 守「哪些對算衝突」與 before／after 的效果 | 改：歧義失敗的案拿掉；「哪些對算衝突」「加 before 後的先後」的案留 | 沒有先後的衝突對在 (c) 也通過 | 無 | Kernel（衝突判定）；歧義不由機制守 |
| J-25 | With／Without 不減少衝突（斷言 `ErrorCount(DE8) == 1`，`SchedulerTests.cpp:313-320`） | 改：改用 `TieBreaks` 或衝突邊斷言 | 無（同一個性質換觀察方式） | 無 | Kernel |
| J-27 | (c) Parallel 無守衛 id 失敗；Parallel 未實作 D-E17（`SchedulerTests.cpp:324-350`） | 拆：D-E17 留；(c) 守衛 id 的案拿掉（X-2、DI-KS 7；確認清單第 13 件，待使用者） | 選了權威也不查 (c) Parallel 的守衛 id | 無 | Kernel（D-E17）／不由機制守 |
| J-44 | (c) 登記順序 20 種排列：全部 D-E8，或結果都不變 | 拿掉（有歧義的排列在剩下那支必然失敗） | 「(c) 對登記順序不敏感」不再保證 | 無 | 不由機制守 |
| `StrictOrderTests.cpp`（8 案） | `RequireExplicitOrder` | 拿掉 | (b) 嚴格模式消失 | 無 | 不由機制守 |
| J-24 | 每個啟動錯誤碼一個最小 Registry | 拆：保留的碼留；R-E1／R-E3／R-E6 搬編排層；digest 與 D-E15 的碼移權威；回滾的宣告與 D-E16 的碼移回滾 Module；**刪**：D-E2、D-E8、D-E10、D-E12、D-E13、D-E18、D-E21、X-E2、X-E3、F-E1 | 刪的碼不再報 | 無 | 各新 owner |
| D-E15、Z6、Z8、D-E16 的案（D-E16：`SchedulerTests.cpp:586-593`、`CoverageTests.cpp:112-118`、`:307-313`） | 回滾 × 判定資料的交叉規則；Derived 在讀者之前重算 | 移出：D-E15、Z6、Z8 → 權威的交叉檢查；D-E16 → 回滾 Module（修訂 20 補） | 不選權威時交叉規則全部通過；不選回滾時 Derived 不查 | 無 | 權威／回滾 |
| J-28 | `Role(Main)` 的 System 放錯執行緒 finish 失敗（gyo2 只有這半，`SchedulerTests.cpp:353`） | 拿掉；新增「綁執行緒的 Resource 被別的執行緒碰到就斷言」 | 啟動時不再擋 | 存取時斷言（新） | Kernel（Resource） |
| J-30 | stamp 序列與 tickStamps | 改：stamp 序列留；tickStamps（tick 視角）移時間 Module 或拿掉，由時間的宣告定 | tick 視角的換算不再由 Kernel 保證 | 無 | Kernel／時間 |
| J-32 | 結構變更的決定性 | 留 | 無 | 無 | Kernel |
| J-32b | untyped `Commands` 動 (c)／Exclusive、(b) despawn 帶 (c) 欄回 Err（`ExecutorTests.cpp:296`） | 拿掉（L3） | 這些寫入不再回 Err | 無 | 不由機制守（權威靠 digest 與伺服器覆蓋） |
| J-34 | hook 依登記順序、下一窗生效；(c) hook 衝突 D-E9 | 前半留；D-E9 案拿掉（確認清單第 8 件，已核准） | 任何策略下 hook 衝突都不擋 | 無 | Kernel（前半）／不由機制守 |
| J-36 | 事件讀到的時機、隔兩 tick 讀不到、順序鍵 | 改（游標份） | 待游標份 | 待游標份 | Kernel |
| J-37 | 事件跨 domain 失敗 | 拿掉（在游標批，和 F-E1 一起） | 跨時鐘讀寫通過 | 無 | 游標保證不漏讀 |
| J-45、J-57 的 Resource 與區的案 | `ExclusiveWorld` 的區與寫入限制 | 拿掉（L3、L7）；J-57 的閉包案留 | 獨占 System 寫 (c)／Exclusive、寫沒列出的單一寫者 Resource 都不再回 Err | 無 | 閉包：Kernel；寫入：不由機制守（權威可讀 `Writes<…>` 標註） |
| J-50 | `KernelSettings`：System 宣告 `Res` 是 D-E18；第一次 Run 後外部再寫斷言；第一次 Run 時才讀到 | 改（L9）：D-E18 案拿掉；「第一次 Run 後再寫斷言」拿掉；改成「每次最外層 Run 開始時讀到最新值、Run 中途改的值下一次 Run 才生效」 | System 可以讀寫；第一次 Run 後也能改 | 「下一次 Run 生效」成為守的性質 | Kernel |
| J-52 | Z5 複核 | 拿掉（L3） | 同窗 (b) despawn 碰 (c) insert 不再中斷 | 無 | 不由機制守 |
| J-54、J-55 | hook 清空輪、丟掉不能生成的那一筆 | 留；「上限在 C 指紋」移出；「event trim」隨游標；「startup」隨編排層；「record 的 tick」待 L8；「指紋版本」隨 L10 | 逐案在實作批列 | 逐案 | Kernel |
| J-56 | 巢狀執行 | 留；「獨占 System 自己繞路改成員」兩案拿掉（DI-KS 18 改寫，`immediate-change-via-exclusive-world`）；Z5 七案拿掉（L3）；clock tick 的案移時間 Module、內層 clock 修剪隨游標；extract 案留；單一寫者 Resource 的驗證案拿掉（L7） | 逐案 | 逐案 | Kernel |
| J-58 | Startup 由 Kernel 跑 | **改**：寫成遊戲初始化清單（O-1）搬到編排層；Kernel 的版本在減法批拿掉 | 「Startup 執行中跑別的」「一個 World 兩張」不再是錯誤 | 「整份一次、在所有 World 每幀之前」 | 編排層 |
| 沒有 J 編號：`InitialValueTests`（45）、`SkeletonTests`（9，含「Exclusive 拒第二個寫者」）、Plan export（2） | 初始值、漏斗寫者、骨架 | 初始值的填入留；Z1／Z2／Z3 與漏斗寫者的案、Skeleton 的寫者案拿掉（修訂 29）；Z6、C 指紋、zone C 匯出的案移權威；另有用 D-E2 的 `NestedRunTests.cpp:1379`、`:2066`、`InitialValueTests.cpp:177`、`CoverageTests.cpp:300`、`ZoneRuleTests.cpp:180`、`:219` 同樣拿掉 | 選了權威也通過 | 無 | Kernel／權威／不由機制守 |
| `naming_guard.py`、`kernel_guard.py`、`kernel_symbols.py` | 命名、Kernel 目錄的掃描 | 改：掃描根加 `engine/orchestration` 與權威 Module | 無 | 範圍變大 | 同 |
| `store_stamp_guard.py`；`zone_c_export_check.py`、`zone_c_fixture/` | stamp 寫入；zone C 匯出 | 前者留；後兩者移出 | — | — | Kernel／權威 |

### 7.3 執行期的六條規則（L3 的材料；**已定拿掉**，2026-11-17）

> **修訂 32**：下面「正常業務用有型別的參數…照舊由伺服器權威策略在啟動時檢查」寫在不數寫者、讀寫規則拿掉之前。現況：有型別的寫入啟動時看得到，但只用在 C 指紋、回滾交叉檢查（Z6）與 tick 的專用檢查，**不查誰能寫**；表裡「唯一的寫者」「啟動時檢查（L12）」兩格以此為準。

**範圍**（使用者 2026-11-17 問「假如作者寫的是正常業務呢，比如玩家使用技能、或者道具恢復HP，這應該如何算？」之後補）：拿掉的只是**沒有型別的路**。正常業務用有型別的參數，啟動時就看得出寫哪一欄，照舊由伺服器權威策略在啟動時檢查。

| 寫法 | 啟動時看得出寫哪一欄嗎 | 由什麼守 |
|---|---|---|
| `Write<Health>`、`EventWriter<Healed>`、`CommandsFor<Inserts<…>>`、`Spawn<Bundle<…>>` 等有型別的參數（技能補血、道具回 HP、受傷都是這一種；多來源時走漏斗：各自發事件、唯一的寫者 `ApplyHealth` 套用合併規則，規格 B.3.1） | 看得出 | 伺服器權威策略在啟動時檢查（L12） |
| untyped `Commands`、`ExclusiveWorld` 的閉包、從外面寫（除錯工具、編輯器這類逃生口） | 看不出 | 下表的六條；已定拿掉，改靠 digest 比對與伺服器覆蓋 |

| 規則 | 位置 | 「Kernel 提供中性的檢查掛鉤，權威掛上去」 | 「選了伺服器權威就禁止那幾種參數」 | 「執行期的攔截拿掉」 |
|---|---|---|---|---|
| untyped `Commands` 只寫 zone B 的 Shared 欄（DI-KS 2、J-32b） | `World.cpp`、`Params.hpp` | 蓋得到（記錄當下） | 蓋得到（禁 untyped `Commands`） | 作者自律 |
| `ExclusiveWorld` 不寫 zone C 與 Exclusive（J-45） | `World.cpp:154-157`、`:983-987` | 蓋得到 | 蓋得到，但和狀態機、(c) 區獨占套用衝突 | 作者自律 |
| `ExclusiveWorld` 寫單一寫者 Resource 要列出（DI-KS 23） | `Params.hpp` | 蓋得到 | 同上 | 作者自律 |
| 呼叫當下的 Z5：(b) 的 `Despawn` 碰帶 (c) 欄的實體（`CheckDespawn`） | `World.cpp:355-365` | 蓋得到（呼叫當下） | 蓋不到（`Despawn` 是一般參數） | 作者自律 |
| 同步點的 Z5 複核：同窗 (b) despawn 碰 (c) insert（J-52；每筆記錄帶 `emitterZone`） | `World.cpp:586-629`、`detail/Context.hpp:90` | 蓋不到（發生在套用時）；要另一個「套用前」掛鉤 | 蓋不到 | 作者自律 |
| 第一次跑之後外部寫 (c) 欄（W-6b、J-6 後半） | `World.cpp:196-202` | 蓋得到（外部寫入走同一路徑） | 蓋不到（不是 System 參數） | 作者自律 |

掛鉤的選項和 `authority-own-registration` 的理由「Kernel 完全不動」、第 9.1 節的新 RC「Kernel 又長出限制」相抵——掛鉤本身中性，但它是為一種策略加的執行期分支。

---

## 8. 批次、先後與模型

navigator 的上段（公開介面、Design Invariants），實作者不用小模型。

**和主會話的批次表對齊**（`rebuild-batch-plan.zh-Hant.md`，分支 `claude/gyo2-time-design`；舊版 2416d4c0 寫「Kernel 減法 → 編排層的骨架 → 時間 Module → 其餘可以並行」，游標份修訂 0 第 11 節第 6 條指出和本頁修訂 15 不一致；主會話在 1313746d、5d78a872 照本頁修訂 16 的提案重排）。本頁的提案：照主會話「減法先」的方向，但把 Kernel 減法**拆成兩段**——不需要新家的先拿，要新家的和新家同一條線，守衛與功能（例：事件的修剪）都不留空窗。**主會話已對齊**（`rebuild-batch-plan.zh-Hant.md` 第二段，`claude/gyo2-time-design` 5d78a872；本頁先前引用的 2416d4c0 是舊版），**待使用者確認**（主會話在請使用者核准統合時一起給他確認）。

| 批 | 內容 | 依賴 | 實作者／審查者 | 估計 |
|---|---|---|---|---|
| Kernel 減法・結構段 | 不需要新家的：`Product` 改名 `WorldExecutor`；DI-KS 18 改寫（`ExclusiveWorld` 可以當場改成員；hook 與 observer 不在此列；J-56 兩案退役；當場改後 hook 何時跑、存取驗證快照重取、巢狀時不留舊參照在這一批的宣告定——回滾第一批之前）；登記與建 Plan 兩步、可重複（L5 的 Kernel 半）；唯讀帶版本號的登記表（L6）；`ExclusiveWorld` 整個 World 的值的讀寫、`Writes<…>` 選填（L7）；`KernelSettings` 每次最外層 Run 重讀、拿掉 D-E18（L9）；拿掉執行期攔截（L3）；拿掉歧義錯誤與 `RequireExplicitOrder`（`StrictOrderTests` 退役）；拿掉執行緒的啟動檢查、加綁執行緒 Resource 的斷言；指紋改版、golden 重產（第一次） | 本宣告核准 | Opus／high；Fable／high；§14.4 檢證者 Fable／high | 10～14 h |
| 編排層的骨架 | 新 Module `engine/orchestration`；`Module`／`Plugin` 型別與生命週期、依賴從登記檔讀（L20）、R-E1／R-E3／R-E6 搬出；`Product`；遊戲初始化清單（O-1；J-58 改寫搬來，Kernel 的開局排程同批拿掉）；runner 介面與只跑一次的 runner；每個 World 准不准動態插入（L5 的編排半）；啟動錯誤的彙整（O-5）；**建 World 後、第一次 Run 前給作者一個時機**（讀檔設配發器範圍、放回存檔的實體；讀檔卡修訂 10 的請求，修訂 33），形狀在這一批的宣告定 | 結構段；PR #240 合併 | Opus／high；Fable／high | 10～14 h |
| 時間 Module ＋ 事件游標 ＋ 拿掉時鐘（同一條線） | 時間 Module 持有時鐘與 tick（L2）；事件游標（游標份）；Kernel 拿掉 `Domain`、`clock`、`CurrentTick`、`DomainTick`、tickStamps、`eventTrim` 與 `EventTrim` 步、D-E12、F-E1；golden 重產（第二次） | 骨架（runner）；時間與游標兩份宣告核准 | 各自的宣告 | 各自的宣告 |
| 權威 Module ＋ 回滾 Module ＋ 拿掉權威規則（同一條線） | 權威、回滾照各自的宣告；Kernel 拿掉寫者數量、digest 禁 Shared、區（`Zone`、`Role`、`Rollback`、`ingress` 等）、回滾的宣告（Rewind／Replay 只有註解、沒有實作可拿），交出回滾原語（L11） | 骨架；時間 Module（權威依賴它，L20）；兩份宣告核准 | 各自的宣告 | Kernel 這一半 8～12 h（守衛的搬遷與約 200 個突變的處置）；其餘各自估。**線內的先後**（修訂 19 補）：Kernel 給回滾原語 → 回滾 Module 的公開登記（可回滾的資料、步的 schedule 與巢狀 schedule、輸入歷史、重跑時不跑；回滾份第 11 節的第一批）→ 權威 Module 的骨架（修訂 29：豁免寫者的登記拿掉）（回滾的官方策略，回滾份第 11 節的第二批，排在公開登記之後、和權威骨架互不依賴） → 權威的交叉檢查（Z6、Z8、D-E15）→ 拿掉 Kernel 的權威與回滾規則；golden 重產（第三次：拿掉的 zone、writePolicy、role、rollback、ingress、rollbackSafe 都在 Registry 指紋，`Fingerprint.cpp:11-18`） |
| 之後 | 生成世界（另一份宣告，放編排層）；多個 World（T5）與交換點（原 T2）在編排層；狀態機 (b) 區照它已核准的宣告 | 骨架 | 各自的宣告 | — |

先後的理由：結構段沒有新家要等，先拿掉，編排層骨架要用它的兩步 API；骨架先於各 Module（它們以登記單位接進來）；時鐘的移出和時間 Module、事件游標同一條線，事件的修剪不會有「舊的拿掉、新的還沒有」的空窗；權威規則的移出和權威、回滾 Module 同一條線，守衛不會有空窗；L4 定了之後，生成世界不再綁 Kernel 減法。golden 重產三次（結構段、時鐘那一條線、權威那一條線；修訂 19 更正，原寫兩次）。

**時間 Module 的宣告**（修訂 19 補）：第三、四條線都要經過它，它累積了 L2、L19、L27、回滾份 7.2（重跑退 tick）、游標份 4.6（tick 放在事件內容）的要求，以及時間設計 DI 2（三種時間各一個寫者）隨不數寫者失去守衛、要時間宣告自己定守法（權威份修訂 4 回報），以及回滾份修訂 4 的「重跑中」Resource（放時間 Module；lightyear 放在 core 的 timeline、Unity 放在 `NetworkTime`，兩家都在時間那一側；時間宣告不收時，退回由回滾 Module 登記、恆在）。現在沒有 owner；本頁提議由主會話開一張卡寫它，排在第三條線之前（待主會話）。

**權威份與回滾份的批次草案**：權威份第 10 節（03320e5e）已改成本節的五段，回滾份第 11 節（d8125486）已補（修訂 20 更新）。

**執行期攔截拿掉之後的空窗**（修訂 20 補）：結構段就拿掉執行期的攔截（L3），接手的 digest 比對與伺服器覆蓋要等網路 Module（權威份 5.9：第一版沒有網路，digest 只用在回歸測試）。這段期間 untyped `Commands`、`ExclusiveWorld`、`Despawn`、外部寫入對判定資料的錯誤沒有東西抓；gyo2 上目前沒有產品。

---

## 9. Design Invariants 與 Replacement Criteria（草案；核准後在實作批寫進各 owner 的頁）

### 9.1 Kernel（`target/kernel.zh-Hant.md` 的改寫）

| DI-KS | 處置 | 改寫後的要旨 |
|---|---|---|
| 1 | 改寫 | 描述從簽名推出；`ExclusiveWorld` 的 Contribution 是整個 World 的值的讀寫（L7），`Writes<…>` 是選填的標註、不改變它的存取 |
| 2 | 前半留；後半拿掉（L3） | 驗證開著時，經 World 的未宣告存取與 System 邊界的淨變化記為違規、以 Err 回傳；驗證是開發工具 |
| 3 | 改寫 | 順序只來自 id 與 Plan；Plan 在兩次執行之間可以重建（World 宣告允許動態插入時），同樣的登記得到同樣的 Plan |
| 4 | 留 | — |
| 5 | 改寫 | 衝突與順序分開；沒有順序的衝突對照登記順序平手並列在 `TieBreaks`，不是錯誤；Kernel 不保證結果對登記順序不敏感 |
| 6 | **拿掉**（寫者數量；`authority-no-writer-count`） | — |
| 7 | **拿掉**（(c) 預設 Serial、Parallel 要逐位元守衛，X-2、D-E11、X-E1；確認清單第 13 件，**待使用者**） | — |
| 8 | 逐句去處：Z6、Z8（可回滾的判定資料、輸入不回滾）→ 權威的回滾交叉檢查（L24）；「domain tick 只有一個來源、System 不得寫」→ 權威的專用檢查（15.3）；「(c) 只從 domain tick 取時間」→ 不再由啟動檢查守（確認清單第 11 件，**待使用者**）；其餘區規則拿掉（讀寫規則兩半拿掉） | — |
| 9 | 改寫（修訂 32）；刪「(c) 區」措辭，並刪「獨占的 System 只在它跑的 schedule 的同步點變（DI-KS 18）」——和改寫後的 DI-KS 18 相反 | 一般的 System 執行期間成員不變；`ExclusiveWorld` 可以當場改（DI-KS 18）；其餘照舊 |
| 10 | 改寫（游標份） | 讀者游標內不漏讀，Kernel 自己保證 |
| 11 | 留；「寫入計入獨占」「(c) 的 hook 衝突」拿掉（修訂 29） | hook 只有 OnAdd／OnRemove、同步執行、依登記順序 |
| 12 | 改寫（L11） | Kernel 提供回滾用的原語，清單見 DI-KS 15 第 (13) 項；不提供 Rewind／Replay／Restore 與「哪些資料要回滾」的判斷；回滾在回滾 Module |
| 13 | 留「Extract 參數唯讀、無寫回、只在 extract schedule」；來源 World 與時機移編排層 | — |
| 14 | 改寫 | 保留：Running 期間外部不碰 World（W-6）；同一個 World 同一時間只被一條執行緒跑（`WorldExecutor::RunSchedule` 進入時斷言；系譜 Bevy 的 `&mut World`）；World 的生命期是一個世代，世代切換＝重建 World。移出：一 World 一執行緒角色的啟動檢查（改成綁執行緒的 Resource 自己斷言）；每 domain 一 clock schedule（L2，移時間 Module） |
| 15 | 改寫（取代 15 與 22 的例外） | **Kernel 的職責是 ECS**（L17）。職責清單（修訂 19 逐列對到第 3.1 節）：(1) World 的儲存——實體存活集合、元件欄、Resource，與 World 摘要（`World::Summary`，`World.cpp:1051`）；(2) 事件——保留式（游標，L13；改名，對應 Bevy `Message`）與推送式（observer，對應 Bevy `Event`＋`Observer`；`events-two-kinds-like-bevy` 加的一項）；(3) System 參數與存取推導（含 `effects`、唯讀的 Extract 參數）；(4) schedule、Plan、`WorldExecutor` 與執行（含巢狀跑）；(5) 延後變更與同步點；(6) hook；(7) 生成——配發器（含在 World 是 Ready 時設定配發器的下一個號，讀檔照存檔編號放回前要先訂在存檔記的下一號之後；修訂 33，屬這一項、不另列）、`InitialValue`、丟掉不能生成的那一筆與 `RejectedSpawn` 明細；(8) 存取驗證（開發版）與診斷紀錄；(9) 唯讀的登記表、Registry 與 Plan 指紋；(10) `KernelSettings`；(11) 綁執行緒的 Resource 碰錯時的斷言；(12) 登記——`Registrar`、開 Registrar 與建 Plan 兩步、動態插入的宣告與拒絕、owner 的保存（修訂 20 補，3.1 第 3 列、3.5）；(13) 給上層用的原語，只限下列：整欄讀出、整欄放回、指定 stamp 寫回、Resource 的 stamp 與存在一起讀寫、實體存活集合的讀出與放回、跑 hook 的結構變更路（會改成員的原語只在 `ExclusiveWorld` 裡可用；「不跑 hook」那一條與「Ready 時從外面寫而且跑 hook」都不要了，修訂 30；L11；多一種就是清單多一項）。不在清單上的不放進 Kernel：不內建 schedule 或 stage 名；不認得開局、時鐘、區、權威、回滾策略、產品、執行緒角色；不擁有執行緒、不讀時鐘或亂數。啟動錯誤只有第 3.1 節列的那幾種（守衛：`StartupCode` 的列舉值與清單比對）。判斷某樣東西屬不屬於清單，對照同類型的多家 ECS 實作（Bevy、flecs、Unity Entities、Unreal Mass 等）的位置與設計原因 |
| 16、17、20、21 | 留；刪去開局、區的措辭 | — |
| 18 | 改寫（`immediate-change-via-exclusive-world`） | 一般的 System 執行期間成員不變，結構變更只經同步點；`ExclusiveWorld` 可以當場改成員（回滾的整批放回就在這裡做）；**hook 與 observer 不在此列**——hook 的 `ExclusiveWorld<>` 不能當場改、observer 不准帶 `ExclusiveWorld`（Bevy 明文禁止獨占 System 當 observer，`observer/distributed_storage.rs:472-474`；Bevy 的 hook 只拿 `DeferredWorld`；修訂 32），當場改之後 hook 何時跑、存取驗證的快照重取、巢狀跑時不留指向舊成員的參照，在實作批的宣告定 |
| 19 | 留；Z5 那一句拿掉（L3） | — |
| 22 | 移出 → 編排層 O-1 | — |
| 23 | **拿掉**「列出來的算寫者」（確認清單第 4 件）；`Writes<…>` 留作選填標註（L7） | — |
| 新 | 新增 | **Kernel 的設定下一次 Run 生效**：`KernelSettings` 只放開發與調校的設定，System 與外部都可以讀寫，每次最外層 Run 開始時重讀；會改變 World 規則的開關（例如 L5 的動態插入）放在建 World 時的選項、執行中不能改 |
| 新 | 新增 | **登記表唯讀、帶版本**：Kernel 交出的登記資訊是唯讀的、帶版本號；交出去的參照只在當前版本有效；修改只走 `Registrar` |
| 新 | 新增 | **動態插入照 World 的宣告**：World 建立時宣告准不准在 Ready 時再登記或拿掉 System；不准時，插入回傳執行時錯誤、記警告、不改 World；准時，下一次執行前重建受影響的 Plan，指紋跟著重算、不保證兩端一致 |

Kernel 的 RC：RC 4、5（D-8 誤報是排程的事，留 Kernel）、6、13、14、15 留；RC 3（觸發條件是 D-E8）拿掉；RC 10（為繞過 DI-KS 6 改成 Shared）拿掉；RC 11（回滾等價破裂）移回滾 Module 的 RC（修訂 32 更正，不是寫者規則）；RC 12（X-2 的守衛）隨 X-2 拿掉（確認清單第 13 件）；RC 1 的 (c) 條件、RC 9 改寫掉 (c) 與 J-17；RC 7「Host 又長回來」**保留原條件**、改寫主詞（舊的 Host 職責現在在編排層：「第一個真實能力接進機制時仍要改編排層的迴圈或 Kernel 本體；或 H.7 以外的協調邏輯在兩個以上產品中重複」），另**新增** RC「Kernel 又長出限制：Kernel 的職責清單（DI-KS 15）要多加一項，或判不出某樣東西屬不屬 ECS——不數次數，出現就停下來問（L17）」；RC 8 留。

### 9.2 編排層（新頁 `target/v2/orchestration.zh-Hant.md`）

- **O-1 遊戲初始化**：一個 `Product` 一份遊戲初始化清單，行程啟動時跑一次、在所有 World 開始每幀之前跑完、跑在主 World；清單內的先後由作者與 Module 插入；編排層保證「整份一次、在前」，不保證清單內的先後。守衛：J-58 改寫。
- **O-2 登記單位**：依登記檔順序 build → Kernel 建 Plan → 依同順序 finish → 選了的檢查（權威、回滾）（L5；修訂 32 補回滾）；`Product` 的 World 描述帶各 Module 自己解讀的選項（例：權威策略、回滾策略），編排層在建 World 時轉交、不解讀（統合照 L17 判：屬「登記單位與生命週期」，不是 O-4 多一項）；執行中的插入（指令等）照同一條路再走一次；Module 在 Plugin 之前（PR #240）；**依賴只從登記檔讀**（L20），缺依賴或依賴排在後面是啟動錯誤、不重排；名字重複是啟動錯誤；一個 `Product` 至多一個 runner。守衛：J-19 的登記單位那半（requires 的案改成讀登記檔）。
- **O-3 runner**：runner 擁有迴圈、決定哪條執行緒跑哪個 World；只呼叫 `WorldExecutor::RunSchedule` 與交換點，不排 System 的先後。守衛：掃描（`engine/orchestration` 不呼叫 System、不讀 Plan 的步驟）。
- **O-4 職責**：**編排層的職責是組裝與流程**（L17）。職責清單：登記單位與生命週期、依賴的讀取與檢查、`Product`（World 群與 runner）、遊戲初始化與每幀的清單、生成世界的流程（另一份宣告）、交換點、extract 的時機、執行緒的安排、啟動錯誤的彙整。不帶任何第三方能力，不認得產品、權威策略、時鐘、回滾的語意。守衛：依賴掃描（`engine/orchestration` 只 link Kernel 與 base）、`naming_guard`。
- **O-5 啟動錯誤一次收齊**：Kernel 的啟動錯誤與編排層、各 Module 的錯誤彙整成一份回報；Kernel 失敗時，依賴 `FrozenRegistry` 的檢查不跑，回報寫明「因 Kernel 錯誤而未檢查」。錯誤型別：編排層自己的報告型別包住 Kernel 的 `StartupReport` 與各 Module 的錯誤（Kernel 的 `StartupCode` 不擴充）。
- RC：每加一種產品或 World 的組法都要改編排層的程式；runner 需要排 System 的先後才做得出某個 Feature；編排層開始 include 某個第三方或某個 Module 的內部；**職責清單要多加一項，或判不出屬於哪一層**（L17）。

### 9.3 權威 Module、事件游標

條文在各自的宣告。

---

## 10. 要使用者判的（會改 Kernel 公開介面，在統合定）

1. **時鐘與 tick**（L2）：**已定（2026-11-17）——移到時間的 Module**，作者可選；「tick 只有一個來源」歸伺服器權威策略。原話見第 2.1 節。比過的：留在 Kernel、只拿掉流程名字（現在的做法）／移到時間的 Module（建議；Bevy `bevy_time`、Unity 的 World 時間）。理由（使用者的）：「tick的控制之前也是pvp的需求，這個明顯是遊戲需求入侵了Engine設計」。查證：K.3 第 11 項（`spec.zh-Hant.md:1219`）的內容是 (c) 區（PvP 的權威與回滾）的時間來源。
2. **執行期的攔截**（L3）：**已定（2026-11-17）——拿掉**；伺服器權威改靠 digest 比對與伺服器覆蓋來抓錯。比過的：Kernel 提供中性觀察點、策略在觀察點拒絕（卡片原建議；四家的觀察點只到觀察，沒有拒絕的前例，Bevy 明寫每次寫入呼叫 hook 成本太高，`bevy_ecs/src/component/mod.rs:705-712`）／選了伺服器權威就禁那幾種參數（蓋不到三條）／拿掉（四家共同的做法；卡片查完後改建議這個）。理由：卡片的（四家的做法；不讓 Kernel 為策略長出分支）；使用者照說，沒有另外說。證據 `runtime-interception-other-engines.md`。後果：untyped `Commands`、`ExclusiveWorld`、`Despawn`、外部寫入對權威資料的錯誤不會當場被抓，要等 digest 比對或被伺服器覆蓋。
3. **生成世界時寫進去的初始值**（L4）：**已定（2026-11-17）——參照 Bevy**：初始值在建立實體的當下給，不設「寫入不算」的階段；伺服器權威只數修改既有值的寫者。比過的：建立時的值不算寫者（建議；Bevy required components `bevy_ecs/src/world/mod.rs:400-411`、場景在每幀的 `SpawnScene` 生成 `bevy_scene/src/lib.rs:98-104`；flecs prefab `docs/PrefabsManual.md:18-39`；Unity `EntityManager.Instantiate` `EntityManager.cs:3513`）／編排層保證生成期間不跑每幀、權威信任（K0 的延伸，無先例）／權威自己核對編排層的清單（無先例）／另一個 World 建好再搬（Unity 串流載入；要 Kernel 加跨 World 搬移）。理由：使用者「這個可以參照Bevy」。證據 `init-values-other-engines.md`。
4. **登記的生命週期與動態插入**（L5）：**已定（2026-11-17）**——見關聯表 L5 與第 2.1 節。比過的：生命週期——Kernel 兩步、編排層依序呼叫（建議）／照 Bevy 第一次跑才建 Plan；凍結——照舊永遠凍結／預設可改、PvP 鎖住（Bevy `schedule.rs:637-650` 圖有改動才重建；flecs `pipeline.c:269-277`；Unity 執行中可建 System）；開關——使用者提出「每個 World 宣告」（四家都沒有這種開關；Bevy 最接近的是 Finished 之後加 Plugin 就 panic，`bevy_app/src/app.rs:642-652`）。理由（使用者的）：未來的指令集；「我不要求作者一定不能動態插入，而是在World裡聲明」。卡片補的理由（使用者看過）：L3 拿掉執行期攔截後，權威策略擋不到執行中的插入，World 自己的宣告補上這個缺口。
5. **Kernel 交出的唯讀登記資訊**（L6）：**已定（2026-11-17）**——見第 3.3 節。比過的：交出唯讀的表（建議；三家都這樣）／不交（作者自寫的策略與工具看不到；和已定的「連線比 C 指紋」相抵）／讀改同一份（flecs）。理由：卡片的（三家的做法；不交與 C 指紋相抵；修改只走一條路才守得住 L5 的關卡）；使用者「可以」，沒有另外說。
6. **`ExclusiveWorld` 的存取**（L7）：**已定（2026-11-17）**——見關聯表 L7。和前面的關聯：大半由 L3 定（「只能寫一般區 Shared、其他回 Err」是 L3 拿掉的六條之一）；插入 System 照 L5；改登記只走 `Registrar`（L6）。只剩的一問：`Writes<…>` 留作選填（建議）／拿掉（完全照 Bevy）→ 留作選填。系譜：Bevy 獨占系統 `&mut World` 不列寫入（`multi_threaded.rs:173`、`:506`）、flecs `immediate`（`docs/Systems.md:1500-1512`）、Unity `SystemBase` 直接用 `EntityManager`；K0 的 `exclusive-declared-writes`（使用者 11-15「要保證誰寫，別開shared」）。理由：卡片的（不解讀的標註、不擋任何人，PvP 想查誰寫時可用）；使用者「嗯，可以留選填」。證據 `exclusive-access-other-engines.md`。
7. **D-E18**（L9）：**已定（2026-11-17）**——見關聯表 L9。比過的：准讀准寫、下一次 Run 生效（建議；Bevy `FallbackErrorHandler` 是 Resource，「schedule 正在跑時改，要等跑完才生效」`bevy_ecs/src/error/handler.rs:110-117`）／准讀、寫只在兩次執行之間（flecs `ecs_set_threads`「never while running a system or pipeline」`include/flecs/addons/pipeline.h:216-221`）／准讀不准寫（卡片前一版，依據不完整）／照舊不准（現在）。其他：Bevy `ScheduleBuildSettings` 隨時可設（`schedule.rs:556-576`）、`App::set_error_handler` 只能設一次（`bevy_app/src/app.rs:1471-1499`）；Unity `World.SetTime`（`World.cs:365-372`）。理由：卡片的（三家都准寫；開發中用指令打開驗證接上指令集）；使用者「這個照建議」。卡片第一次的調查只查了「讀」，使用者指出；也一度把 L5 的開關和這一題扯在一起，使用者指出不是同一件事。
8. **回滾的機制放哪**（L11）：**已定（2026-11-17）**——見關聯表 L11、L21。比過的：Kernel 只給原語、回滾放權威（卡片第一版）→ 回滾獨立成 Module（卡片第二版；lightyear 把 `lightyear_prediction` 與 `lightyear_replication` 分成兩個 crate，`crates/replication/prediction/Cargo.toml:2`、`:29`，決定性同步也依賴它，`crates/deterministic/deterministic_replication/Cargo.toml:16`、`:25`）／Kernel 提供完整的快照與還原（現在的 DI-KS 12；flecs v3 snapshot 在 addon、v4 拿掉）／放 PvP 產品。回滾有策略：lightyear `RollbackPolicy`（觸發來源 state／input，模式 Always／Check／Disabled，深度上限，`manager.rs:15-63`）與 `CorrectionPolicy`（`correction.rs:1034`）；GGPO、GekkoNet 由遊戲提供存讀回呼。理由（使用者的）：「如果回滾有策略，就不能單單有官方默認實現…否則作者無法達到自己的目的」。證據 `rollback-placement-other-engines.md`。
9. **兩層的讀法**（L16）：**已定（2026-11-17）——照 Bevy**：編排層認得、Kernel 不認得。使用者 11-16 的兩句（「世界生成階段（初始化）Engine不認識，那Engine如何排列流程？」「ECS，是不是不應該有（階段）這個概念」）分別指兩層。系譜：Bevy（`bevy_app` 的 `Main` 開局清單；`bevy_ecs` 零命中）、flecs（pipeline addon 的 `OnStart`；core 只預留 id）、Unity（Entities 沒有 World 級開局）。原話：「用Bevy的方式走，」
10. **替換條件與歸屬的判斷方式**（L17）：**已定（2026-11-17）**——見關聯表 L17。比過的：出現一次就停下報告（卡片原建議）／兩次以上（草案）／一律走 Delta；使用者改從「固定核心、每層職責清單、agent 自己判」來解：「都報告其實不是不行，但是我擔心我會被處理大量判斷之後我的判斷也會出現偏差（已經有好幾次了）」。並要求判斷時對照同類型的多家實作：「不光只是一家，而是同類型的…盡量找出他們的設計原因，做交叉。我希望獲得的是別人的能力，而不是技術債」。系譜：Bevy `bevy_ecs` 可以單獨使用（`bevy_ecs/README.md:13`），範圍就是 ECS。
11. **Module 之間的依賴**（L20）：**已定（2026-11-17）**——依賴只寫在登記檔，編排層讀出並檢查；程式碼不寫 `Requires`；不做策略層級的依賴，權威 Module 整個依賴時間 Module。比過的：只寫登記檔（建議；系譜 O3DE `gem.json`＋`enabled_gems`、Unreal `.uplugin`）／Module 自己引入依賴（flecs `ecs_import`，可重入；代價：登記檔看不出實際用了什麼）／照舊程式碼寫 `Requires`、編排層檢查（登記檔與程式碼各一份，違反 AGENTS §9）。理由：卡片的（合 11-14「登記檔是為了看得到用了什麼」、同一事實一處）；使用者「這個照你建議」，沒有另外說。證據 `module-dependencies-other-engines.md`。
12. **回滾的重跑在哪裡發動**（L11；回滾份第 10 節第 1 件；會動 DI-KS 18）：**已定（2026-11-17，side chat）**——不開例外、建機制：System 登記時宣告能不能當場改成員；回滾的重跑由宣告了的特別 System 巢狀跑固定步。宣告**沿用 `ExclusiveWorld`**（使用者 2026-11-17「沿用 ExclusiveWorld」），DI-KS 18 改寫成「普通 System 只經同步點；`ExclusiveWorld` 可以當場改」。未定、待統合處理（使用者沒有逐條定）：當場改後 hook 何時跑；存取驗證的快照重取；限制（只在序列執行、巢狀跑時不留指向舊成員的參照，近 DI-KS 21）。
13. **L4 是否取代 11-14 的「生成的 bundle 照舊算寫者、作者用漏斗」**（L28）：**不用定**（修訂 22；伺服器權威不數寫者）。

第 12、13 件與三份子宣告、四份交叉檢查新增的題目，合併去重後的順序在第 14 節。

---

## 11. 範圍的限界

- 這是宣告，沒有寫程式、沒有量測；時間估計是讀程式量估的。
- 第 7 節的處置按守衛說明與逐案清單的關鍵字掃描歸類；每案的判定在各實作批逐案判斷。
- 四家的做法來自子 agent 讀原始碼與文件，主會話抽查了一部分；Unity 讀非官方鏡像，Unreal 沒讀到原始碼。
- 已定的事的原話，有一部分是側邊會話與主會話轉達的；本頁照 HANDOFF 與轉達的文字，沒有重讀原始對話。
- PR #240 未合併；O-2 的「Module 在 Plugin 之前」與定義引用它。
- 修訂 18 已和三份子宣告一起送 Fable／high 交叉檢查（2026-11-17；證據 `four-docs-adversarial-check.md`），處理見第 13 節；修訂 19 本身還沒有再送檢查。

### 附錄：減法批之後的掃描（判定之一）

`git grep -n -P "<式子>" -- engine/kernel` 0 命中；每個式子先在 cb6bccca 用已知例命中一次證明有效；用終了碼區分 0 命中（1）與失敗（其他）。macOS 的 git grep 加 `-E` 不認 `\b`（同一式子 0 命中），一律用 `-P`。式子：

- 一定拿掉的：`\b(Zone|WritePolicy|Rollback)::`、`\bRole::Input\b`、`\bRequireExplicit(Zones|Order)\b`、`\bemitterZone\b`、`\brollbackEnabled\b`、`\bnotInRollback\b`、`\brollbackSafe\b`、`\bingress(Stage)?\b`、`\bDigestSet\b`、`\bCFingerprint\b`、`\bExportZoneCSources\b`、`\bparallelGuardTest\b`、`\bstartupRan_\b`、`\bStartup(NotNestable|Running)\b`、`\bInstallRunner\b`、`\bFinishContext\b`、`\bkMainRole\b`、`\bDomain::Loading\b`、`\bownerPlugin\b`、`\bDE(8|10|13|18|21)\b`、`\bXE[23]\b`
- 時鐘（L2 已定）：`\bDomain::`、`\bclock\b`、`\bCurrentTick\b`、`\bDomainTick\b`、`\btickStamps_\b`、`\beventTrim\b|\bEventTrim\b`（後者在游標批）、`\bDE12\b`、`\bFE1\b`
- 執行期攔截（L3 已定）：`\bUntypedWrite\b`、`\bZoneCDespawn\b`、`\bExclusiveWorldWrite\b`、`\bCheckDespawn\b`（`World.cpp:355` 的 Z5 呼叫當下；`Despawn` 本身留，檢查函式拿掉）、`"Z5"`（規則字串，`World.cpp:360`、`:637`）
- `ExclusiveWorld` 的區限制（L7 已定）：`\bUndeclaredResourceWrite\b`
- 另一個判定：`StartupCode` 的列舉值等於第 3.1 節的清單（測試比對）。

---

## 12. 修訂 1 檢查發現的處理

修訂 0 的 16 條見修訂 1 的第 11 節（git 歷史 d169e1f7）；這裡列修訂 1 檢查的 15 條。

| 發現 | 處理 |
|---|---|
| 1 拆三份當成已定在寫 | 使用者 11-17 定了拆三份＋統合先立；本頁改寫成統合，第 1 節關聯表；會改 Kernel 介面的項目拉回本頁（第 10 節） |
| 2 時鐘移出沒有使用者的決定 | 改成 L2、第 10 節第 1 件；2.2 的 `Domain` 列改「只有 Loading」；補 K.3 第 11 項、DI-KS 8／12、`fixed-step-in-world`、`time-plan-two` |
| 3 2.2 漏列 | 補 `zone-default-b`、`product-type-name`、`spawn-reject-zone-c-blocked-by-zone-b`、狀態機宣告、`exclusive-default-already-decided`；runtime-composition 補理由 |
| 4 狀態機「不受影響」 | 改成 L18；第 4 節第 4 項列入 |
| 5 D-E14 與執行期錯誤沒歸屬 | 3.1 補 D-E14 與 `KernelErrorCode` 的去處；DI-KS 15 的守衛改成「列舉值等於清單」 |
| 6 生命週期做不到 | L5、第 10 節第 4 件；O-2 加凍結後的步驟 |
| 7 7.2 歸錯 | J-25、J-27、J-22 拆開；J-58 改標「改」；新增「移出」的定義 |
| 8 批次空窗 | 生成世界與減法、權威同一條線；時間依賴骨架；F-E1、J-37 在游標批；golden 重產兩次計入 |
| 9 掃描只蓋一部分 | 附錄補齊識別字，依 L2、L3、L7 分組；加列舉值比對 |
| 10 3.3、3.4 | 3.3 改正（`source`、`ownerPlugin` 已有）；「不交」與 C 指紋相抵寫進第 10 節第 5 件；3.4 補登記單位名單；3.5 owner 身分 |
| 11 6.3 少一條 | 7.3 補「呼叫當下的 Z5」 |
| 12 DI 與 RC | RC 5 留 Kernel；RC 7 保留原條件、改寫主詞，另立新 RC；RC 12 移權威；DI-KS 14 補「同一時間只被一條執行緒跑」；O-3、O-4 補守衛 |
| 13 第 10 節第 3、5 件的選項 | L7 的第二個選項寫明「哪些要列要另定」；S-18 標依賴 L7；修訂 1 的「計數」那件併入 L2 的代價與 L8 |
| 14 替使用者補理由 | 2.2 的 DI-KS 5、K0 兩列改成只寫使用者的話，側邊會話的說明另標 |
| 15 小錯 | S-7／S-8 更正；H-6 `:1127`；`CurrentTick` 74 處；估計重算 |

---

## 13. 四份交叉檢查的處理（修訂 19）

檢查者 Fable／high；讀的版本：統合 ffdc8014、權威 0b802f5e、游標 c55f1253、回滾 829bc791。證據 `four-docs-adversarial-check.md`（主 checkout `build/evidence/gyo2-kernel-app-boundary/`）。統合的部分本頁已改；子份的部分通知各卡。

| 發現 | 統合（本頁） | 通知子份 |
|---|---|---|
| 1 Derived／D-E16 沒人接；Z6／Z8／D-E15 兩份都說是對方的；回滾第 10 節第 4 件的 ingress 和 L24 相抵 | L24 補 (1)(2)(4) | 回滾：接 Derived、D-E16；第 4 件拿掉 ingress 那句。權威：接 Z6、Z8、D-E15 |
| 2 Match 不回滾卻被迫選回滾策略 | 新增 L29，三題合一（第 14 節） | 權威：6.2、6.13 改指 L29。回滾：第 5 件改指 L29 |
| 3 「不算寫者」的寫入策略認不出來 | L23、L24 (3) | 權威：加豁免寫者的登記。回滾：公開重跑者與還原者 |
| 4 PvP Client 的事件路線被擋；L11「內容不變」只對一半；回滾第 4 件「只警告」和 7.1 相抵 | L11 更正；第 14 節加確認題 | 游標：註明 PvP Client 的模擬不走跨 schedule 的路（或留給第 14 節的事件題）。回滾：第 4 件拆三題 |
| 5 R-2 等價和「事件不回滾」相抵 | — | 回滾：R-2 加前提，或把「可回滾 schedule 內的事件同一步寫讀完」列成選項 |
| 6 可回滾的 schedule 沒有登記處 | L24 (5) | 回滾：5.4 加 schedule 的登記並公開 |
| 7 原語缺實體存活集合；列號對不上；「Ready 時從外面跑 hook」是新機制 | L11 補 | 回滾：第 6 節補存活集合、改正列號 |
| 8 DI-KS 15 對不上 3.1 | 9.1 逐列對齊、原語改列舉 | — |
| 9 3.3 缺 effects 與 `Writes<…>` | 3.3 補 | — |
| 10 L28 未定、權威當前提；讀檔卡沒列 | L28 補讀檔卡 | 權威：P6 標「待 L28」 |
| 11 改寫較早決定卻沒有題目；2.2 漏兩列 | 2.2 補兩列；第 14 節加確認清單題 | 權威：把 D-E9、K.3 第 5 項、Z3 的 stage、(c) 獨占套用的新形狀、`emitterZone`、漏斗的 B1～B3 列成一份確認清單 |
| 12 事件在權威裡沒有位置 | 第 14 節加「digest 含不含事件與游標」 | 權威：4.3 補事件的登記 |
| 13 已登記的型別怎麼「只標記」 | — | 權威：4.3 加只標記的入口（tick、狀態機的值） |
| 14 批次 | 第 8 節補線內先後、時間宣告、golden 三次 | 權威：第 10 節改成統合第 8 節的五段。回滾：補批次草案 |
| 15 小錯 | eventTrim 改 `:924` | 權威：4.5 的第一種核對法違反 O-4，拿掉。回滾：補 L23～L30 |

### 2.2 補的兩列

| 較早的決定（日期） | 當時的內容與理由 | 較新的決定 | 較新的有沒有回應較舊的理由 | 本批的處置 |
|---|---|---|---|---|
| `v8-arch-gyo2-funnel-block-b1-b3-fingerprint-a`（11-14） | 原話「B1–B3 要擋，指紋用方案 A」：擋 B1（ingress 經 Input 的請求造出 (c) 區 State）、B2（(c) 區的 System 造出 Input）、B3（開回滾的 schedule 裡出現不回滾的 (c) 區元件）；宣告一律寫進指紋、格式版本加一。理由：沒有說 | `kernel-is-ecs-no-restrictions`（11-16）、`rollback-module-strategy`（11-17） | 沒有單獨回應 | B1、B2 隨區移權威份；B3 是「判定資料 × 回滾」，照 L24 歸伺服器權威策略的交叉檢查；方案 A 的「宣告進指紋」隨 C 指紋移權威份，Registry 指紋照 3.4 改版。列進第 14 節的確認清單 |
| `v8-arch-gyo2-who-wrote-debug-tool`（11-15） | 原話「執行時記下『這個值實際是誰寫的』，這個能保留嗎？我認為排錯非常有用。」、確認「3. 另外排一批。」；K0 實作時守「每一個記號要對得回唯一的一步」 | `no-runtime-interception`（11-17） | 沒有：L3 的原話只說拿掉攔截、改靠 digest 與覆蓋；「Kernel 不加觀察點」是卡片的說法 | L30，第 14 節第 17 題（確認題） |

### 第二輪檢查的處理（修訂 20）

證據 `four-docs-adversarial-check-r2.md`。讀的版本：統合 1f0e5dac、權威 03320e5e、回滾 d8125486、游標 96fae7dc。

| 發現 | 統合（本頁） | 通知子份 |
|---|---|---|
| 1 統合沒跟上 L24：回滾的宣告與 D-E16 仍送權威；J-46 不存在、Rewind／Replay 沒有實作 | 3.2 拆列；第 4 節第 3～6 項；J-24；7.2 加一列；第 8 節更正 | 回滾：「J-46 搬來」改成三處實際的案、重跑那一段是新寫 |
| 2 「回滾自己登記豁免」和「回滾不依賴權威」相抵 | L24 (3) | 權威 4.8、回滾 5.4：只留「回滾公開名單、伺服器權威策略代為登記」 |
| 3 策略的 interface 沒有登記時的時機 | L24 (6)；第 3 題 | 權威 4.4 二擇一寫明 |
| 4 tick 驅動者過不了規則 1；6.11 依賴 6.4 | L24 (3)；第 14 節重排 | 權威 5.6、6.11、4.8 |
| 5 C 指紋的內容沒有一節列出 | — | 權威 5.8 加「進／不進／待判」表；6.14 第 3 條照表改寫 |
| 6 R-2 前提缺一半 | 第 14 節加一題 | 回滾 R-2 列出前提與誰守 |
| 7 第 3 題的題目 | 第 14 節第 3 題拆四個子題 | 回滾：建議與 Match 的情形對不上 |
| 8 第 14 節的順序與選項 | 重排；第 10 題三個選項；第 15 題改成確認題 | — |
| 9 事件 × 回滾三份寫法不同 | L11 更正（使用者定的只有 (5)；兩個方向都擋） | 游標第 11 節第 3 條；游標、回滾補「每幀寫、固定步讀」；原語寫清放回哪些讀者的游標 |
| 10 巢狀跑的 schedule 算不算可回滾 | — | 回滾 5.4 寫規則，權威 5.14 引用 |
| 11 代為登記進回滾的範圍 | — | 權威 4.3 |
| 12 DI-KS 15 缺登記；新機制無條件列入 | 9.1 加 (12)、原語標條件；L11 | 回滾第 6 節第 6 列標條件 |
| 13 L3 的接手要到網路 Module | 第 8 節寫明空窗 | 權威 4.4：`OnMismatch`、`Digest` 標「形狀待網路 Module」 |
| 14 權威對狀態機、網路的依賴 | — | 權威 4.2、第 9 節、8.1 |
| 15 規則 1 對生成怎麼算 | 第 1 題分「數量」與「誰能寫」 | 權威 5.3 加一句 |
| 16 互讀舊版與殘留舊句 | L4 指向 L28；第 8 節末段更新 | 三份互讀最新版；權威 L3 列、補 L30 |
| 17 規則照查 | — | 回滾第 10 節的引號；權威 6.3、6.5、6.11 用描述名；「N」三義 |

---

## 14. 要問使用者的題目：合併去重、排好順序（修訂 20 重排；**修訂 27 起由第 15 節取代，只留作紀錄**）

來源：統合第 10 節第 12、13 件；權威份第 6 節（15 件，含 6.14 確認清單、6.15）；回滾份第 10 節；游標份第 4 節（8 件）；兩輪交叉檢查新增的。排序的根據是「先定的會改變後面哪些」；同一列的子題一起問。每題問的時候先講遊戲的例子，再列別家的做法與公平的選項，一次一題。

| 順序 | 題目 | 合併自 | 先定它會改變後面的 |
|---|---|---|---|
| 1 | **判定資料的讀寫規則**（權威 6.16；取代原第 1 題，L28 已不用定）：「判定的 System 只讀判定資料」與「判定資料只由判定的 System 寫」兩半都留／只留讀的一半、判定 System 用宣告／都拿掉；生成受不受管併在這一題 | 權威 6.16；統合 13（L28） | 權威 6.4、6.11、4.8（豁免）、5.8 的待判列、6.14 第 6 條；統合 L23、L24 (3) |
| 2 | **回滾的重跑在哪裡發動**：**已定（side chat）——System 宣告能不能當場改成員，回滾由宣告了的特別 System 巢狀跑**；宣告沿用 `ExclusiveWorld`（已定）；hook 時機、快照重取、限制待統合（第 10 節第 12 件） | 統合 12；回滾修訂 0 第 1 件 | DI-KS 18；「Ready 時從外面寫而且跑 hook」要不要；第 13 題；豁免名單 |
| 3 | **策略選在哪一層**，四個子題：權威策略的層；回滾策略的層；哪些 World 可以沒有回滾（Match）；兩邊各自沒選策略怎麼辦（含「被依賴帶進來的 Module 算不算選了」、代為登記的時機要求策略先選定；權威份 4.4 採「登記入口交給已選的策略」（`OnRegister`，不動編排層），另一種「build 之後、建 Plan 之前的通用階段」要動編排層 O-2） | L29；權威 6.2、6.13；回滾第 5 件；第二輪第 3、7 條 | 權威 4.4、4.7；回滾 5.6；編排層 O-5 |
| 4 | **寫者規則數不數**：**已定（2026-11-17，side chat）——不數**（`authority-no-writer-count`） | 權威 6.3 | 第 1 題不用定；第 5 題、第 14 題的 digest 禁 `Shared` 不用問；6.11 待權威份重寫 |
| 5 | ~~對已存在的實體插入元件算建立還是修改~~：**不用問**（不數寫者） | 權威 6.7 | — |
| 6 | **經權威登記的就是判定資料嗎** | 權威 6.5 | 第 7 題 |
| 7 | 6 的後續：System 的區怎麼來 → 「tick 只有一個來源」的檢查形狀（隨前者；tick 驅動者怎麼過規則 1）→ 入口要不要排在讀它的判定 System 之前（用順序圖檢查，取代 Z3 的 stage 檢查）→ C 指紋與 wire id 用的編號 → G-5 匯出留不留 | 權威 6.4、6.11、6.15、6.8、6.12 | — |
| 8 | **游標放哪、誰算一個讀者** | 游標 4.1 | 第 9、10 題；digest 含不含游標 |
| 9 | 不執行的讀者、新讀者從哪讀起 | 游標 4.3 | — |
| 10 | **事件在回滾裡**：進不進快照（進的話放回哪些讀者的游標）／跨 schedule 的檢查多嚴（報錯、只警告、不檢查；改寫 11-14 的「可回滾的 schedule 不參與」；兩個方向）／時間設計 6.3 (2) 的「依位置讀」與「走 Ingress」拿掉的確認 | 回滾第 4 件的事件半；L11 更正；兩輪檢查 | Kernel 原語清單是否多一項；回滾公開登記要不要加事件；權威 5.14 對事件怎麼算；回滾 R-2；游標 4.6 |
| 11 | 紀錄要不要帶時間標記、tick 放在哪（PvP 的音效時刻） | 游標 4.6 | 時間 Module 的宣告 |
| 12 | **回滾等價（R-2）的前提由誰守**：選了伺服器權威時由權威的檢查守；沒選時作者負責／回滾 Module 自己加檢查（新規則） | 第二輪第 6 條 | — |
| 13 | **回滾的歷史存在哪**（World 裡／回滾 Module 自己） | 回滾第 1 件 | digest 要不要排除它 |
| 14 | **digest 比什麼**（含不含事件與游標）；digest 禁 `Shared` 不用問（不數寫者） | 權威 6.9 的「比什麼」、6.6；第一輪第 12 條、第二輪第 8 條 (a) | — |
| 15 | 還原時跑不跑 hook、stamp 怎麼標 | 回滾第 2、3 件 | — |
| 16 | 副作用的提醒（重跑時播腳步聲）：不檢查／啟動時用 effect 提醒 | 回滾第 4 件的副作用半 | — |
| 17 | **「誰實際寫的」排錯工具（確認題）**：11-15 已定保留、另外排一批；只問「記號留在 Kernel 的寫入關口，算不算 Kernel 職責清單（DI-KS 15）多一項」 | L30；第二輪第 8 條 (e) | DI-KS 15 |
| 18 | **確認清單：由既定決定推出而拿掉的規則**（Z3 的 stage 已移到第 7 題） | 權威 6.14 | — |
| 19 | 彼此獨立的：權威 Module 的名字；事件上限（單位、預設、滿了）；丟棄記錄；順序鍵；修剪時機；跨 World 與網路的形狀 | 權威 6.1；游標 4.2、4.4、4.5、4.7、4.8 | — |
| 20 | **批次的先後**（主會話 5d78a872 的五段＋本頁第 8 節的補充）；**時間 Module 的宣告由誰寫**（本頁提議主會話開卡） | 統合第 8 節 | — |

**修訂 26 的說明**：使用者選了「一題一題問、先問底層機制」。底層機制的題目（依序）：可以當場改成員的宣告（已定）→ 保留式事件的游標放哪、誰算讀者 → 推送式事件：名字、觸發時機、和 hook 的關係、observer 的存取 → 保留式的其餘（不執行的讀者、上限、丟棄記錄、順序鍵、修剪時機、時間標記）→ 事件進回滾快照的 Kernel 原語 → 「誰實際寫的」記號的確認題。其餘照下表。

**順序的說明**（修訂 22）：第 4 題定了之後，權威份要出新修訂、四份再做一次交叉檢查；第 2、3 題與游標、回滾的題不受影響，可以先問；第 1、6、7 題用權威份修訂 3（9efe5e1a）的材料。

**不問的**：權威 6.9 的「何時比」「不一致怎麼辦」延到網路 Module 的宣告（L23）；權威 6.10 沒有選項；L8 統合已判；Derived 回滾份已接、不提議不帶。

---

## 15. 統合照 L17 判的（修訂 27）

使用者 2026-11-17 被逐題問設計細節後說：「什麼都要我來定，乾脆代碼我也自己寫好了。名字你有命名規範。例子你有源碼。」所以第 14 節的題目重篩：名字照命名規範、細節照別家原始碼，由統合（或 side chat）照 L17 自己判，**下表都標「卡片照 L17 判，不是使用者的話」**。只把三類交使用者：會推翻使用者之前的決定的、別家做法分歧且真的有取捨的、某層職責清單要多一項的。官方策略的內部選擇（作者可以替換，L21）一律自己判。

### 15.1 底層機制（Kernel）

| 項目 | 判定 | 系譜 |
|---|---|---|
| 推送式事件（side chat 照 L17 判，2026-11-17；同日照四家交叉更正） | 名字：保留式 `Message`、`MessageReader`、`MessageWriter`、`MessageOptions`（取代 `Event*`）；推送式 `Event`、`Observer`、觸發 `Trigger`、observer 參數 `On<E>`；事件型別名用過去式（naming DI 2）。時機兩條：`ExclusiveWorld` 與 Host 觸發當場跑；一般 System 經 `Commands`，延到同步點；同一點內 observer 的先後是決定性的（同樣的登記得到同樣的順序），但不對作者承諾是登記順序（flecs `ObserversManual.md:2702-2703` 明寫先後 undefined、不要依賴；Bevy `observer/distributed_storage.rs:153-155` 寫順序任意；修訂 28 更正）。hook 留著、不併入：一個元件每種一個 hook，新增時 hook 在 observer 之前、移除時在 observer 之後（GYO 現行 OnRemove 的先後在實作批對照）。**生命週期（Add／Remove）觸發 observer 放進這次的契約**（三家都有；先前「先不做」撤回），實作與自訂事件同批。observer 是 System，存取照簽名推出、照存取驗證，在觸發點跑、不進 Plan 的步驟（同 hook）。DI-KS 15 第 (2) 項「事件」涵蓋兩種，不算多一項 | hook 與 observer 分開：flecs `docs/ObserversManual.md:128-142`（hook 一定在 observer 之前、移除時在之後）、Bevy `world/entity_access/mod.rs:1344-1345`；Mass 只有 observer processor（MassSample `MSPhysicsInitProcessors.cpp:244-245`、`:360-361`）；Unity Entities 核心沒有推送式。生命週期觸發 observer：Bevy `On<Add<T>>`、flecs OnAdd／OnRemove／OnSet、Mass `ObservedOperations`。時機：flecs「操作延後時 observer 跟著延到同步點」（`:2598`）、依發出順序送達（`:2711`）；Bevy `World::trigger`（`event/mod.rs:14-16`）／`Commands::trigger`（`system/commands/mod.rs:1160`）。名字：`Observer` 三家共通；觸發 Bevy 叫 trigger、flecs 叫 emit |
| 保留式（`Message`）的游標放哪 | 放在 World，以（掛載點、訊息種類）為鍵 | **GYO 從使用者決定推出來的，不是照別家**：照游標修剪（`event-trim-by-cursor`）要 Kernel 讀得到全部游標。別家：Bevy 放每個 System 的 `Local`（`message_reader.rs:38-41`），因為保留固定兩次 update、不看游標（`messages.rs:69-77`）；flecs 沒有保留式、建議自己做 queue（`ObserversManual.md:105-112`）；lightyear 的接收器只能取走一次、沒有游標（`receive.rs:61-64`）；replicon 沿用 Bevy 的 `Messages`。代價：停跑的讀者拉住保留量，靠上限擋 |
| 誰算一個讀者 | System 掛載、run 條件、observer 是讀者，各有游標；推游標不算寫入；**hook 不能帶 `MessageReader`**（修訂 32：照 Bevy 的 hook 沒有游標與舊規格 F.4 hook 只有訊息的寫者） | Bevy `on_message` 條件自帶游標（`schedule/condition.rs:1148-1154`，註解：不推會一直誤判） |
| 從沒跑過的讀者、新讀者 | 跑過一次才拉住保留；第一次跑從還留著的最舊一筆讀起 | Bevy 新游標讀到緩衝裡還在的（`messages.rs:175-176`） |
| 上限 | 每種訊息留幾筆，預設 1024、登記時可改（沒有量測根據，是預設值）；滿了丟最舊、記一筆；不給「滿了算錯誤」 | 別家分歧：Unity Netcode 輸入環 64 筆、滿了覆蓋最舊（`ICommandData.cs:169`、`:263-266`）；lightyear 待放緩衝 4096 筆、滿了拒收新的並報錯（`receive.rs:243-254`，為了防網路灌入擠掉別人的訊息）；Bevy 沒有筆數上限、超過兩次 update 靜默丟（`messages.rs:36-37`、`:79`）；replicon 沒有上限。GYO 的訊息在行程內，照 Unity 丟最舊；使用者「丟了有記錄」 |
| 丟棄記錄 | 每次 Run 按訊息種類聚合一筆（丟了幾筆、被丟的序號範圍、沒讀到的讀者），World 記、runner 在交換點取走；不逐筆記明細（修訂 28 更正，免得讀者停住時明細本身也無限長） | 別家只寫 log 或靜默：lightyear 按種類聚合一行 `warn!`（`receive.rs:451`）、Unity 過期 RPC 只警告（`RpcCommandRequest.cs:100`）、Bevy 靜默；GYO 多一份可取走的記錄是使用者「丟了有記錄」 |
| 順序鍵 | 每種訊息一個單調遞增的寫入序號 | 多家一致：Bevy `MessageId.id`（`message/mod.rs:109-123`）、lightyear（remote_tick, next_sequence）（`receive.rs:105-116`）、flecs 自訂事件依發出順序（`ObserversManual.md:2714`） |
| 修剪時機 | 每次最外層 Run 結束（hook 清空輪之後）修剪讀過的；寫入時碰到上限當場丟最舊 | 多家一致（每個外層幀一次）：Bevy 每幀在 First 輪替（`sub_app.rs:91-98`），為了怕固定步還沒讀就丟另加閘門（`bevy_time/lib.rs:102-106`、`message/update.rs:14-50`）——照游標修剪正好解掉這個問題；lightyear 每幀在 Last 清（`receive.rs:55`） |
| 時間標記 | 紀錄只帶序號；要 tick 的訊息自己在內容裡帶 | 多家一致（核心不帶 tick、網路層自己加）：Bevy 紀錄只有 id＋內容（`message/mod.rs:104-107`）；replicon 的 tick 在自己的佇列（`server_message.rs:481-513`）；lightyear 在接收紀錄的 metadata（`receive.rs:66-75`）；Unity `ICommandData` 把 Tick 放內容（`ICommandData.cs:65-72`） |
| 事件進回滾快照 | 不進：`Message` 不回滾，Kernel 不加事件的原語；推送式不保留、重跑時重新觸發，observer 讀「重跑中」自己判斷。使用者 `time-plan-two` (5)「可回滾的 schedule 不參與」照舊，由回滾策略在啟動時檢查（兩個方向） | 兩家一致：lightyear 訊息不回滾、用 `Rollback` Resource 判斷重跑中（`core/timeline.rs:332-350`、`manager.rs:403-408`）；Unity RPC 不回滾、副作用用 `IsFirstTimeFullyPredictingTick`（`NetworkTime.cs:39-44`）。兩家都把回滾要重播的輸入放在另一個按 tick 索引的環（lightyear `input_buffer.rs:25-39`、Unity `ICommandData` 64 筆），GYO 的回滾策略也要自己的輸入歷史 |
| 「誰實際寫的」的記號 | 不在這一輪定：使用者 11-15 已定「另外排一批」，記號放不放 Kernel 的寫入關口在那一批的宣告定；要放時是 Kernel 職責清單多一項，那時照 L17 停下問 | — |

### 15.2 編排與 Module 的接法

| 項目 | 判定 | 系譜 |
|---|---|---|
| 策略選在哪一層 | 權威策略與回滾策略都選在 World（建 World 時的選項，在各 Module build 之前），因為登記、Plan、Registry 都是每個 World 一份；沒有回滾策略的 World 就是不回滾、不報錯（Match）；「重跑中」Resource 放時間 Module、恆在（修訂 32；時間宣告不收時退回由回滾 Module 登記）；World 有權威登記卻沒選權威策略是啟動錯誤 | Unity 建 World 時帶 flag 定角色（`ClientServerBootstrap.cs:319`、`:458`）；lightyear、replicon 每個 App（一個 App 一個主 World）。「重跑中」恆在：Unity `NetworkTime` 在兩種 World 都一直在（`NetworkTime.cs:47-50`）。lightyear 把「重跑中」放在 core（`timeline.rs:332-335`），避免別的 plugin 依賴 prediction——GYO 放回滾 Module 時，讀它的要依賴回滾 Module，交回滾份考慮放時間 Module |
| 代為登記的時機 | 策略在建 World 時就選定，權威 Module 的登記入口交給已選的策略（`OnRegister`），一定在建 Plan 之前 | 權威份 4.4 |
| 批次先後、時間 Module 的宣告由誰寫 | 交主會話排（不是使用者的題目） | — |

### 15.3 權威（官方的伺服器權威策略；作者可替換）

| 項目 | 判定 | 系譜 |
|---|---|---|
| Module 的名字 | `Authority`（`engine/authority`） | 使用者與所有決定都用「權威」；replicon 自稱 server-authoritative |
| 經權威登記的就是判定資料 | 是，不另填屬性；「要不要複製」是網路 Module 自己的登記 | **只有 GYO 分成兩層**：lightyear 把預測、雜湊、複製串在同一個 `app.component::<C>()`（`prediction/registry.rs:745`）、Unity 用 `GhostField`、replicon 沒有判定資料。分層的理由：權威可以不靠網路（單機、測試） |
| 「tick 只有一個來源」 | 專用檢查：只准時間 Module 公開的驅動者與對時寫 tick | **只有 GYO 這樣做**：別家只靠可見性或約定——Unity `NetworkTime.ServerTick` 公開（`NetworkTime.cs:73`）、replicon `ServerTick::increment` 是 pub（`server_tick.rs:28-37`）、lightyear 欄位私有但 `apply_delta` 公開（`timeline.rs:215-240`）。保留是因為使用者 11-17 `clock-out-of-kernel` 已把這條歸伺服器權威策略 |
| C 指紋與 wire id 的編號 | 權威登記的順序 | 兩家一致：replicon 登記順序＋協議雜湊偵測兩端順序不同（`lib.rs:545-546`、`shared/protocol.rs:13-19`）、lightyear 登記順序編 u16（`core/utils/src/registry.rs`）；Unity 依 `StableTypeHash` 排序（`GhostCollectionSystem.cs:51-56`），因為它由程式碼生成、跨組件登記，順序控制不了。前提：兩端的 Module build 順序由產品登記檔固定，而且登記順序進 C 指紋（兩端不同就連不上）。不交使用者的理由：這是官方策略的內部（作者可換，L21），錯了會在連線時被指紋擋下、錯得早（修訂 32 補） |
| G-5 的兩份匯出 | 留 | 權威份 5.12 |
| digest 比什麼 | 全部的判定元件與 Resource，作者可排除；不含訊息與游標；**合併用和順序無關的方式**（兩端實體的迭代順序不保證一致）；**每種型別要有雜湊函式**，不直接雜湊原始位元組（padding）（修訂 28 補） | 只有 lightyear 可比：逐元件自願登記雜湊函式（`registry/deterministic.rs:47-75`）、只算帶 `Deterministic` 的實體、只算元件、XOR 合併（`checksum.rs:6-7`、`:313-355`）；Unity Netcode、replicon 沒有狀態 checksum。GYO 把 Resource 也算進去是自己的擴充 |
| 「何時比」「不一致怎麼辦」、跨 World 與網路的形狀 | 延到網路 Module 的宣告 | L23 |
| 判定資料的讀寫規則（權威 6.16） | **兩半都拿掉**（卡片照使用者的原則判，2026-11-17）：不檢查「判定的 System 只讀判定資料」，也不檢查「判定資料只由判定的 System 寫」；連帶拿掉「判定的 System 不用 `Local`、不宣告 Io、不用 `Changed`」與輸入入口的先後檢查（輸入入口的登記留給回滾的「重跑時不跑」）；「System 是不是判定的」不用問，C 指紋要納入哪些 System 的先後由權威份從「寫了判定資料的 System」推、並處理網路寫入者；回滾等價的前提是作者的責任。B1、B2 不擋、B3（回滾的交叉檢查）留——改寫使用者 11-14「B1–B3 要擋」，進確認清單 | 七家都沒有兩半（`authority-open-source.md`、`other-engines-authority.md`）：lightyear 只用作者選的 checksum、Unity Netcode「輸入只能由某群組寫」只寫在文件（`CommandSendSystem.cs:31-34`）、replicon 靠覆蓋。套用的原則：Engine 只提供機制；很多開源都這樣做就不頭鐵；不建立用不到、有漏洞的機制（兩半對 untyped 與閉包都看不到）；不逼作者標記；限制只放作者自選處（作者可自寫策略，L21）；能照原則判的不丟給使用者（使用者問「我有什麼原則？」） |

### 15.4 回滾（官方策略；作者可替換）

| 項目 | 判定 | 系譜 |
|---|---|---|
| 歷史存哪 | World 裡的 Resource | 別家分歧：Unity 每個 World 一個 singleton 持有每個 chunk 的環（`GhostPredictionHistorySystem.cs:219-236`、`PredictionBackupRing.cs:38-47`）——GYO 同形；lightyear 放在每個實體的 `PredictionHistory<C>` 元件（`predicted_history.rs:47-52`） |
| 還原跑不跑 hook、stamp | 跑 hook；標目前 stamp | lightyear 結構還原走延後命令、hook 與 observer 都跑（`rollback.rs:1120-1132`），數值還原標目前 tick（`core/ecs_utils.rs:47-73`）；Unity 沒有 hook，寫入時版本推到目前（`GhostUpdateSystem.cs:1520-1530`）。回滾份可參考的能力：Unity 的「沒變就不還原、不推 stamp」；lightyear 預測期間刪實體改成停用不真刪（`despawn.rs` 開頭），避免 OnRemove 在回滾時產生副作用 |
| 副作用的提醒 | 不檢查；作者讀「重跑中」 | 兩家一致：lightyear `is_in_rollback`（`timeline.rs:332-350`）、Unity `IsFirstTimeFullyPredictingTick`（`NetworkTime.cs:39-44`） |
| 回滾等價的前提由誰守 | 全部是作者的責任（回滾不加新規則；讀寫規則拿掉後權威也不守，修訂 32） | lightyear、Unity Netcode 都不檢查決定性 |

### 15.5 還交使用者的

0. **確認清單第 11～13 件**（15.6）：待使用者。
1. ~~判定資料的讀寫規則~~：照使用者的原則判（15.3），B1～B3 的改寫進下面的確認清單。
2. ~~確認清單~~（**已核准**，15.6）：由已定的決定推出、會推翻使用者較早決定的——權威 6.14（D-E9、K.3 第 5 項、Z3 的 stage、(c) 獨占套用的新形狀、`emitterZone`、漏斗 B1～B3、漏斗不再是必經之路）；`shared-means-no-owner`、`exclusive-declared-writes` 的「列出來的算寫者」、`writer-unit-system`、`despawn-not-writer`（隨不數寫者）。

第 14 節的其他題目由本節取代。

### 15.6 確認清單（修訂 28；**使用者 2026-11-17 核准**：「核准」，`v8-arch-gyo2-confirm-derived-removals`，代號待主會話定）

| # | 較早的決定（使用者的話） | 改成 | 推自 |
|---|---|---|---|
| 1 | `funnel-block-b1-b3-fingerprint-a`（11-14）「B1–B3 要擋」 | B1、B2 不擋；B3（開回滾的 schedule 裡出現不回滾的判定元件）照擋，改由伺服器權威策略的回滾交叉檢查做；「指紋用方案 A」隨 C 指紋移權威 Module | 判定資料的讀寫規則兩半都拿掉（15.3） |
| 2 | `spawn-writer-funnel`（11-14）「選 3 項，讓作者自己做處理，但是 Engine 提供漏斗工具支持。」 | 漏斗不再是 Engine 的必經之路；`InitialValue` 留在 Kernel 當作者自選的寫法 | `authority-no-writer-count` |
| 3 | `shared-means-no-owner`（11-15）「如果作者不想那麼麻煩，可以直接設成shared」「要保證誰寫，別開shared。」 | `Shared` 標記拿掉（沒有寫者數量，就沒有「多寫者」要標） | `authority-no-writer-count` |
| 4 | `exclusive-declared-writes`（11-15）`ExclusiveWorld<Writes<…>>` 列出來的算寫者 | `Writes<…>` 照 L7 留作選填標註，只剩讓策略看得到獨占 System 寫了什麼；不再算進寫者 | `authority-no-writer-count` |
| 5 | 規格 B.3 的寫者單位、「Despawn 不算寫者」等數寫者的細則 | 隨寫者數量一起拿掉 | `authority-no-writer-count` |
| 6 | `zone-c-exclusive-apply`（11-15～16）Kernel 加「(c) 區的獨占套用」、啟動時檢查 | 不加進 Kernel；狀態機的套用 System 由作者用名字標記給權威（L26） | `kernel-is-ecs-no-restrictions`、讀寫規則拿掉 |
| 7 | `spawn-reject-zone-c-blocked-by-zone-b`（11-15）明細帶記錄者的區 `emitterZone` | 「丟掉那一筆」照留；`emitterZone` 拿掉、不給替代（沒有區了） | 區移出 Kernel、讀寫規則拿掉 |
| 8 | 規格的 D-E9（(c) 區 hook 衝突是錯誤） | 拿掉：hook 照登記順序，C 指紋含 hook 的登記順序 | `no-ambiguity-detection` |
| 9 | K.3 第 5 項（(b) 讀 (c) 的順序要寫清楚） | 拿掉：照登記順序平手，不是錯誤 | `no-ambiguity-detection` |
| 10 | Z3（輸入入口要在指定 stage） | 拿掉 | 讀寫規則拿掉 |
| 11 | K.3 第 11 項（11-11）與 `clock-out-of-kernel`（11-17）「(c) 區的時間只來自 tick」歸伺服器權威策略 | 不再由啟動檢查守（原本靠讀的一半），只剩 G-5 的來源掃描與 digest；「tick 只有一個來源」的專用檢查照留 | 讀寫規則拿掉（修訂 31 補；**待使用者確認**，第 1～10 件已核准） |
| 12 | `funnel-one-principle`（11-14）「好，照這個方向改」：寫請求的人在區的檢查裡當作也寫了目標 | 拿掉（沒有區的檢查了） | 讀寫規則拿掉（修訂 32 補；**待使用者確認**） |
| 13 | X-2（DI-KS 7、D-E11、X-E1；11-11 核准的規格）：(c) 預設 Serial，Parallel 要逐位元守衛 | 拿掉；Parallel 未實作仍由 D-E17 擋 | 區移出 Kernel、讀寫規則拿掉（修訂 32 補；**待使用者確認**） |

---

## 16. 第四輪檢查的設計缺口（修訂 32；統合照 L17 判，不是使用者的話）

證據 `four-docs-adversarial-check-r4.md`。

| 缺口 | 判定 | 交給 |
|---|---|---|
| 回滾的還原者與重跑者：權威讀「回滾公開的名單」，回滾已拿掉名單 | 回滾官方的還原、重跑 System 是不帶 `Writes<…>` 標註的 `ExclusiveWorld` System、記錄 System 只寫歷史 Resource，啟動時看不到它們寫判定資料與 tick；權威 C 指紋、Z6、tick 檢查都不需要「扣掉回滾名單」 | 回滾份寫明這一句；權威份刪掉「扣掉回滾名單」 |
| 外部輸入兩份各接一次（權威 `.input`、回滾 `rollback.Input<T>`） | 同一個型別兩種身分：權威的 `.input` 標它是判定的輸入（Z8：不能登記成可回滾的狀態，讀回滾的公開登記）；回滾的 `rollback.Input<T>` 宣告回滾策略替它保留按 tick 索引的歷史。沒有回滾策略的 World（Match），`rollback.Input<T>` 沒有作用，輸入照常是權威的判定輸入、進 digest 與否照權威的 digest 規則 | 兩份各寫一句、互引 |
| observer 沒進權威與回滾的檢查 | 凡寫「System 或 hook」的檢查都含 observer；C 指紋收 observer 的存取，以及 GYO 自己決定性的先後（對作者不承諾，但兩端同樣的登記得到同樣的先後，指紋可以收）；回滾兩個方向的檢查裡，hook 與 observer 算在觸發它的那個 System 所在的一側，從 World 外面觸發的不屬任何一側 | 權威份、回滾份 |
| 「重跑中」在還原之後才立起 | 照 lightyear：決定回滾之後、還原之前立起，重跑完放下（`rollback.rs:427`、`:880`、`:1186`、`:1324`） | 回滾份 5.3、5.5 |
| 外面在 Ready 時觸發 observer，延後變更何時套用 | 照 Bevy `World::trigger`：觸發回來後當場套用 observer 記的延後變更；外面在 Ready 時直接寫（不跑 hook）也不觸發新增移除的 observer | 游標份 3.3 |
| tick 是判定資料又不登記成可回滾，Z6 可能擋到 | Z6 對時間 Module 公開的 tick 例外（由策略直接退回，回滾 R-2 前提 1）；列為時間宣告的要求之一 | 權威份 5.11；第 8 節時間宣告的要求 |
| 時間宣告的要求散在各處 | 第 8 節「時間 Module 的宣告」收齊：持有時鐘與 tick（L2）、公開 tick 型別與驅動者及對時（L27、權威 5.4）、「重跑中」（回滾 5.7）、重跑退 tick 的方式（回滾 7.2）、時間設計 DI 2 的守法、tick 的 Z6 例外；owner 待主會話 | 主會話 |
